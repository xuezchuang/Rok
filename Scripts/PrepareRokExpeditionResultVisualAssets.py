from __future__ import annotations

import hashlib
import json
import re
import shutil
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "ref/client-unity/Assets/BundleAssets/UI"
PREFAB = UI / "UIPrefabs/ui_prefabs_battle/UIExpeditionFight/UI_IF_ExpeditionFightTask.prefab"
SOURCE_PNG = UI / "UITextureStatic/ui_res_else/ui_result.png"
SOURCE_META = SOURCE_PNG.with_suffix(".png.meta")
SOURCE_SHEET = SOURCE_PNG.with_suffix(".tpsheet")
V1_MANIFEST = ROOT / "Saved/RokUnityMigration/ExpeditionVisualAssets/v1/atlas_manifest.json"
OUTPUT = ROOT / "Saved/RokUnityMigration/ExpeditionVisualAssets/v2"
MANIFEST = OUTPUT / "result_visual_manifest.json"
EXPECTED = {
    "prefab": "0d0d7a94be3aa16da2d5f6595298774eeac0c401bc2478ccbd91b2dcc26e2c3f",
    "png": "a724b9afb3962c4c2a67dbeb152124dcd246cfc342f152dd0c18d721b4339f2b",
    "meta": "808fe4702c51917d64f01b4fd9b91b259a0c207330ad12b385b7bd5fff290fc4",
    "sheet": "b41677ec16cd8bd8f249f3fc5e8e7c2c5217066f4080c2ca27d7692df7f9849a",
    "v1_manifest": "c5baa5b4f373094dea6e1a75c8311769d8484463f49972b5c5d7f1138ed14bb1",
}
SPRITE_GUID = "0186443df1b8cd144b20fc4dd41fc626"
SPRITE_FILE_ID = 2260871125762619715


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def documents(path: Path) -> dict[str, tuple[int, str]]:
    text = path.read_text(encoding="utf-8-sig")
    result: dict[str, tuple[int, str]] = {}
    starts = list(re.finditer(r"(?m)^--- !u!(\d+) &(\d+)\r?\n", text))
    for index, match in enumerate(starts):
        end = starts[index + 1].start() if index + 1 < len(starts) else len(text)
        result[match.group(2)] = (int(match.group(1)), text[match.end():end])
    return result


def field(block: str, name: str) -> str:
    match = re.search(rf"(?m)^  {re.escape(name)}: (.*)$", block)
    return match.group(1).strip() if match else ""


def vector(block: str, name: str) -> dict[str, float] | None:
    value = field(block, name)
    if not value.startswith("{"):
        return None
    return {axis: float(number) for axis, number in re.findall(r"([xyzw]):\s*([-\d.]+)", value)}


def block_for(docs: dict[str, tuple[int, str]], file_id: str, class_id: int | None = None) -> str:
    if file_id not in docs:
        raise ValueError(f"missing Unity fileID {file_id}")
    actual_class, block = docs[file_id]
    if class_id is not None and actual_class != class_id:
        raise ValueError(f"fileID {file_id} has class {actual_class}, expected {class_id}")
    return block


def line_of(text: str, needle: str) -> int:
    for number, line in enumerate(text.splitlines(), 1):
        if needle in line:
            return number
    raise ValueError(f"line marker not found: {needle}")


def main() -> None:
    if OUTPUT.exists() or MANIFEST.exists():
        raise FileExistsError(f"Refusing to overwrite existing v2 output: {OUTPUT}")
    for path, key in ((PREFAB, "prefab"), (SOURCE_PNG, "png"), (SOURCE_META, "meta"),
                      (SOURCE_SHEET, "sheet"), (V1_MANIFEST, "v1_manifest")):
        actual = sha(path)
        if actual != EXPECTED[key]:
            raise ValueError(f"source changed: {path.relative_to(ROOT)} sha256={actual}")

    prefab_text = PREFAB.read_text(encoding="utf-8-sig")
    docs = documents(PREFAB)
    # Pin the Animator root and its direct animated container by stable Unity file IDs.
    root_go_id, root_rect_id = "1958315710224850218", "6227289810283724345"
    center_go_id, center_rect_id = "5873444356603362705", "4385161344070135569"
    flag_go_id, flag_rect_id = "5988163079402683861", "7125139485787537580"
    img_go_id, img_rect_id = "4219624141377683467", "3734920066888637523"
    root_transform = block_for(docs, root_rect_id, 224)
    center_transform = block_for(docs, center_rect_id, 224)
    flag_transform = block_for(docs, flag_rect_id, 224)
    img_transform = block_for(docs, img_rect_id, 224)
    for go_id in (root_go_id, center_go_id, flag_go_id, img_go_id):
        if field(block_for(docs, go_id, 1), "m_IsActive") != "1":
            raise ValueError(f"inactive source hierarchy node {go_id}")
    assert field(root_transform, "m_GameObject") == f"{{fileID: {root_go_id}}}"
    assert field(root_transform, "m_Father") == "{fileID: 0}"
    assert field(center_transform, "m_GameObject") == f"{{fileID: {center_go_id}}}"
    assert field(center_transform, "m_Father") == f"{{fileID: {root_rect_id}}}"
    assert field(flag_transform, "m_GameObject") == f"{{fileID: {flag_go_id}}}"
    assert field(flag_transform, "m_Father") == f"{{fileID: {center_rect_id}}}"
    assert vector(flag_transform, "m_SizeDelta") == {"x": 0.0, "y": 0.0}
    assert vector(flag_transform, "m_AnchoredPosition") == {"x": 0.0, "y": 0.0}
    assert vector(flag_transform, "m_LocalScale") == {"x": 1.0, "y": 1.0, "z": 1.0}
    panel_transform = block_for(docs, "4005996255782681250", 224)
    panel_image = block_for(docs, "707079810408244484", 114)
    assert field(panel_transform, "m_Father") == f"{{fileID: {flag_rect_id}}}"
    assert field(panel_transform, "m_SizeDelta") == "{x: 676, y: 761}"
    assert field(panel_transform, "m_AnchoredPosition") == "{x: 0, y: -23}"
    assert field(panel_image, "m_Enabled") == "1"
    assert f"m_Sprite: {{fileID: 21300000, guid: 7a00b675d6e95204baed223833b7813e," in panel_image
    assert field(img_transform, "m_GameObject") == f"{{fileID: {img_go_id}}}"
    assert field(img_transform, "m_Father") == f"{{fileID: {center_rect_id}}}"
    assert field(img_transform, "m_SizeDelta") == "{x: 908, y: 102}"
    assert field(img_transform, "m_AnchoredPosition") == "{x: 0, y: 261}"

    children = []
    for transform_id, expected_go, x, scale_x, root_order in (
        ("597719906124241574", "3751180885874877279", -227.0, -1.0, 1),
        ("6958296405779485615", "4137096087478614813", 227.0, 1.0, 0),
    ):
        transform = block_for(docs, transform_id, 224)
        go_id = re.search(r"\{fileID: (\d+)\}", field(transform, "m_GameObject"))
        if not go_id or go_id.group(1) != expected_go:
            raise ValueError(f"unexpected child GameObject for transform {transform_id}")
        if field(transform, "m_Father") != f"{{fileID: {img_rect_id}}}":
            raise ValueError(f"sprite child {transform_id} is not under animated img container")
        if vector(transform, "m_AnchoredPosition") != {"x": x, "y": 0.0}:
            raise ValueError(f"unexpected child anchored position {transform_id}")
        if vector(transform, "m_SizeDelta") != {"x": 454.0, "y": 138.0}:
            raise ValueError(f"unexpected child size {transform_id}")
        if vector(transform, "m_LocalScale") != {"x": scale_x, "y": 1.0, "z": 1.0}:
            raise ValueError(f"unexpected child mirroring {transform_id}")
        go = block_for(docs, expected_go, 1)
        go_name = field(go, "m_Name")
        if field(go, "m_IsActive") != "1":
            raise ValueError(f"sprite child not active: {expected_go}")
        components = re.findall(r"(?m)^  - component: \{fileID: (\d+)\}$", go)
        images = []
        for component_id in components:
            class_id, component = docs[component_id]
            if class_id != 114 or field(component, "m_GameObject") != f"{{fileID: {expected_go}}}":
                continue
            sprite = re.search(r"(?ms)^  m_Sprite: \{fileID: (-?\d+), guid: ([0-9a-f]{32}),\s*type: (\d+)\}", component)
            if sprite:
                images.append((component_id, sprite.group(1), sprite.group(2), field(component, "m_Enabled")))
        if len(images) != 1:
            raise ValueError(f"expected one Sprite Image component on {expected_go}, got {images}")
        component_id, source_file_id, guid, enabled = images[0]
        if guid != SPRITE_GUID or int(source_file_id) != SPRITE_FILE_ID or enabled != "1":
            raise ValueError(f"unexpected/disabled sprite source on {expected_go}: {images[0]}")
        children.append({
            "game_object_file_id": expected_go, "game_object_name": go_name,
            "transform_file_id": transform_id, "image_component_file_id": component_id,
            "sprite_file_id": int(source_file_id), "sprite_guid": guid,
            "source_image_enabled": True, "active_in_prefab": True,
            "parent_transform_file_id": img_rect_id, "anchored_position": {"x": x, "y": 0},
            "size_delta": {"x": 454, "y": 138}, "local_scale": {"x": scale_x, "y": 1, "z": 1},
            "local_scale_x_negative_is_mirror": scale_x < 0,
            "root_order": root_order,
        })

    meta_text = SOURCE_META.read_text(encoding="utf-8-sig")
    sprite_entries = {}
    for match in re.finditer(r"(?ms)^    - serializedVersion: 2\n      name: ([^\r\n]+)\n      rect:\n        serializedVersion: 2\n        x: (\d+)\n        y: (\d+)\n        width: (\d+)\n        height: (\d+).*?^      spriteID: ([0-9a-f]+)\n      internalID: (-?\d+)", meta_text):
        name, x, y, width, height, sprite_id, internal_id = match.groups()
        if int(internal_id) == SPRITE_FILE_ID:
            sprite_entries[name] = {"name": name, "rect_unity_bottom_left": [int(x), int(y), int(width), int(height)],
                                    "sprite_id": sprite_id, "internal_file_id": int(internal_id)}
    if list(sprite_entries) != ["pl_result_1000_1"]:
        raise ValueError(f"expected pinned source sprite only, got {sprite_entries}")
    image_bytes = SOURCE_PNG.read_bytes()
    if image_bytes[:8] != b"\x89PNG\r\n\x1a\n" or image_bytes[12:16] != b"IHDR":
        raise ValueError("source is not a PNG with IHDR")
    atlas_size = list(struct.unpack(">II", image_bytes[16:24]))
    sheet_text = SOURCE_SHEET.read_text(encoding="utf-8-sig")
    sheet_size = re.search(r"(?m)^:size=(\d+)x(\d+)$", sheet_text)
    sheet_row = next((row.split(";") for row in sheet_text.splitlines() if row.startswith("pl_result_1000_1;")), None)
    sprite = sprite_entries["pl_result_1000_1"]
    if not sheet_size or [int(sheet_size.group(1)), int(sheet_size.group(2))] != atlas_size:
        raise ValueError("TexturePacker sheet size differs from source PNG dimensions")
    if not sheet_row or [int(v) for v in sheet_row[1:5]] != sprite["rect_unity_bottom_left"]:
        raise ValueError("tpsheet sprite rect differs from TextureImporter metadata")
    x, y, width, height = sprite["rect_unity_bottom_left"]
    sprite["rect_png_top_left"] = [x, atlas_size[1] - y - height, width, height]
    sprite["pivot"] = [float(v.strip()) for v in sheet_row[5:7]]
    sprite["border"] = [int(v.strip()) for v in sheet_row[7:11]]
    sprite["uv_top_left_origin"] = [x / atlas_size[0], 1 - (y + height) / atlas_size[1],
                                    (x + width) / atlas_size[0], 1 - y / atlas_size[1]]

    OUTPUT.mkdir(parents=True)
    prepared_png = OUTPUT / "ui_result.png"
    shutil.copyfile(SOURCE_PNG, prepared_png)
    if sha(prepared_png) != sha(SOURCE_PNG):
        raise ValueError("prepared PNG is not byte-identical")
    sources = {
        str(PREFAB.relative_to(ROOT)).replace("\\", "/"): sha(PREFAB),
        str(SOURCE_PNG.relative_to(ROOT)).replace("\\", "/"): sha(SOURCE_PNG),
        str(SOURCE_META.relative_to(ROOT)).replace("\\", "/"): sha(SOURCE_META),
        str(SOURCE_SHEET.relative_to(ROOT)).replace("\\", "/"): sha(SOURCE_SHEET),
        "Saved/RokUnityMigration/ExpeditionVisualAssets/v1/atlas_manifest.json": sha(V1_MANIFEST),
    }
    manifest = {
        "schema": "rok.expedition.result_visual_assets.v2",
        "scope": "Source-only preparation of one byte-identical Unity atlas PNG for UE import; no UE import/runtime acceptance.",
        "v1_manifest": {"path": "Saved/RokUnityMigration/ExpeditionVisualAssets/v1/atlas_manifest.json", "sha256": sha(V1_MANIFEST)},
        "sources_sha256": sources,
        "atlas": {
            "name": "ui_result", "source": str(SOURCE_PNG.relative_to(ROOT)).replace("\\", "/"),
            "prepared": str(prepared_png.relative_to(ROOT)).replace("\\", "/"),
            "sha256": sha(SOURCE_PNG), "prepared_sha256": sha(prepared_png), "size": atlas_size,
            "guid": SPRITE_GUID, "meta": str(SOURCE_META.relative_to(ROOT)).replace("\\", "/"),
            "tpsheet": str(SOURCE_SHEET.relative_to(ROOT)).replace("\\", "/"),
            "sprites": [sprite],
        },
        "visual_usage": {
            "prefab": str(PREFAB.relative_to(ROOT)).replace("\\", "/"),
            "prefab_sha256": sha(PREFAB), "animator_root": "pl_center",
            "canvas_context": {
                "prefab_root": {"name": "UI_IF_ExpeditionFightTask", "game_object_file_id": root_go_id,
                                "transform_file_id": root_rect_id, "active": True,
                                "anchor_min": vector(root_transform, "m_AnchorMin"),
                                "anchor_max": vector(root_transform, "m_AnchorMax"),
                                "anchored_position": vector(root_transform, "m_AnchoredPosition"),
                                "size_delta": vector(root_transform, "m_SizeDelta"),
                                "local_scale": vector(root_transform, "m_LocalScale"),
                                "parent_transform_file_id": None},
                "pl_center": {"game_object_file_id": center_go_id, "transform_file_id": center_rect_id,
                              "active": True, "parent_transform_file_id": root_rect_id,
                              "anchor_min": vector(center_transform, "m_AnchorMin"),
                              "anchor_max": vector(center_transform, "m_AnchorMax"),
                              "anchored_position": vector(center_transform, "m_AnchoredPosition"),
                              "size_delta": vector(center_transform, "m_SizeDelta"),
                              "local_scale": vector(center_transform, "m_LocalScale")},
                "pl_flagMes": {"game_object_file_id": flag_go_id, "transform_file_id": flag_rect_id,
                               "active": True, "parent_transform_file_id": center_rect_id,
                               "anchor_min": vector(flag_transform, "m_AnchorMin"),
                               "anchor_max": vector(flag_transform, "m_AnchorMax"),
                               "anchored_position": vector(flag_transform, "m_AnchoredPosition"),
                               "size_delta": vector(flag_transform, "m_SizeDelta"),
                               "local_scale": vector(flag_transform, "m_LocalScale"),
                               "canvas_group_component_file_id": "6061874809428078552",
                               "canvas_group_alpha": 1.0, "interactable": True, "blocks_raycasts": True},
                "canvas_reference_resolution": "not serialized in this child prefab; provided by outer UI/canvas runtime and must not be inferred as 1440x810 or 390x448 from this file",
                "ancestor_image_enabled": "the full panel image component is enabled; see img_battle_result_task record below",
            },
            "animated_container": {"name": "img", "game_object_file_id": img_go_id, "transform_file_id": img_rect_id,
                                    "size_delta": vector(img_transform, "m_SizeDelta"),
                                    "anchored_position": vector(img_transform, "m_AnchoredPosition"),
                                    "local_scale": vector(img_transform, "m_LocalScale"),
                                    "anchor_min": vector(img_transform, "m_AnchorMin"),
                                    "anchor_max": vector(img_transform, "m_AnchorMax"), "pivot": vector(img_transform, "m_Pivot")},
            "visible_children": children,
            "full_panel_image": {"game_object_file_id": "9195209094396823430", "name": "bg",
                                 "image_component_file_id": "707079810408244484", "image_enabled": True,
                                 "parent_transform_file_id": flag_rect_id,
                                 "anchored_position": {"x": 0, "y": -23}, "size_delta": {"x": 676, "y": 761},
                                 "source_guid": "7a00b675d6e95204baed223833b7813e",
                                 "sprite_file_id": 21300000, "source_png": "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_battle/img_battle_result_task.png"},
            "source_lines": {
                "container": line_of(prefab_text, "--- !u!224 &3734920066888637523"),
                "root": line_of(prefab_text, "--- !u!224 &6227289810283724345"),
                "pl_center": line_of(prefab_text, "--- !u!224 &4385161344070135569"),
                "pl_flagMes": line_of(prefab_text, "--- !u!224 &7125139485787537580"),
                "right_sprite_child": line_of(prefab_text, "--- !u!224 &597719906124241574"),
                "left_sprite_child": line_of(prefab_text, "--- !u!224 &6958296405779485615"),
            },
            "ue_target": "/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_result_atlas",
        },
        "acceptance": {"source_only_prepared": True, "ue_import": "not_run", "unity_gameview": "not_run", "runtime_visual_accepted": False},
    }
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"prepared {MANIFEST.relative_to(ROOT)} sha256={sha(MANIFEST)}; copied PNG sha256={sha(prepared_png)}; sprites=1")


if __name__ == "__main__":
    main()
