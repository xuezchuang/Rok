from __future__ import annotations

import hashlib
import json
import re
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "Saved/RokUnityMigration/ExpeditionVisualAssets/v2/result_visual_manifest.json"
EXPECTED_V1_SHA256 = "c5baa5b4f373094dea6e1a75c8311769d8484463f49972b5c5d7f1138ed14bb1"
EXPECTED_SOURCES = {
    "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_battle/UIExpeditionFight/UI_IF_ExpeditionFightTask.prefab": "0d0d7a94be3aa16da2d5f6595298774eeac0c401bc2478ccbd91b2dcc26e2c3f",
    "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_else/ui_result.png": "a724b9afb3962c4c2a67dbeb152124dcd246cfc342f152dd0c18d721b4339f2b",
    "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_else/ui_result.png.meta": "808fe4702c51917d64f01b4fd9b91b259a0c207330ad12b385b7bd5fff290fc4",
    "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_else/ui_result.tpsheet": "b41677ec16cd8bd8f249f3fc5e8e7c2c5217066f4080c2ca27d7692df7f9849a",
    "Saved/RokUnityMigration/ExpeditionVisualAssets/v1/atlas_manifest.json": EXPECTED_V1_SHA256,
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def docs(text: str) -> dict[str, tuple[int, str]]:
    result: dict[str, tuple[int, str]] = {}
    matches = list(re.finditer(r"(?m)^--- !u!(\d+) &(\d+)\r?\n", text))
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
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


def exact_block(source: dict[str, tuple[int, str]], file_id: str, class_id: int) -> str:
    assert file_id in source, f"missing Unity fileID {file_id}"
    actual_class, body = source[file_id]
    assert actual_class == class_id, f"Unity fileID {file_id} class {actual_class}, expected {class_id}"
    return body


def verify() -> None:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert manifest["schema"] == "rok.expedition.result_visual_assets.v2"
    assert manifest["v1_manifest"]["sha256"] == EXPECTED_V1_SHA256
    v1 = ROOT / manifest["v1_manifest"]["path"]
    assert sha(v1) == EXPECTED_V1_SHA256
    assert manifest["sources_sha256"] == EXPECTED_SOURCES
    for relative, expected in EXPECTED_SOURCES.items():
        assert sha(ROOT / relative) == expected, relative

    atlas = manifest["atlas"]
    source_png = ROOT / atlas["source"]
    prepared_png = ROOT / atlas["prepared"]
    source_bytes = source_png.read_bytes()
    prepared_bytes = prepared_png.read_bytes()
    assert source_bytes == prepared_bytes, "prepared PNG differs bytewise from Unity source"
    assert sha(source_png) == atlas["sha256"] == sha(prepared_png) == atlas["prepared_sha256"]
    assert source_bytes[:8] == b"\x89PNG\r\n\x1a\n" and source_bytes[12:16] == b"IHDR"
    size = list(struct.unpack(">II", source_bytes[16:24]))
    assert size == [512, 512] == atlas["size"]
    assert len(atlas["sprites"]) == 1
    sprite = atlas["sprites"][0]
    assert sprite["name"] == "pl_result_1000_1"
    assert sprite["internal_file_id"] == 2260871125762619715
    assert sprite["sprite_id"] == "349b40e6f4a306f10800000000000000"
    assert sprite["rect_unity_bottom_left"] == [1, 131, 454, 138]
    assert sprite["rect_png_top_left"] == [1, 243, 454, 138]
    assert sprite["uv_top_left_origin"] == [1 / 512, 243 / 512, 455 / 512, 381 / 512]

    source_path = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_battle/UIExpeditionFight/UI_IF_ExpeditionFightTask.prefab"
    prefab_text = source_path.read_text(encoding="utf-8-sig")
    source = docs(prefab_text)
    root = exact_block(source, "6227289810283724345", 224)
    center = exact_block(source, "4385161344070135569", 224)
    flag = exact_block(source, "7125139485787537580", 224)
    img = exact_block(source, "3734920066888637523", 224)
    for go_id in ("1958315710224850218", "5873444356603362705", "5988163079402683861", "4219624141377683467"):
        assert field(exact_block(source, go_id, 1), "m_IsActive") == "1"
    assert field(root, "m_Father") == "{fileID: 0}"
    assert vector(root, "m_AnchorMin") == {"x": 0.0, "y": 0.0}
    assert vector(root, "m_AnchorMax") == {"x": 1.0, "y": 1.0}
    assert field(center, "m_Father") == "{fileID: 6227289810283724345}"
    assert field(flag, "m_Father") == "{fileID: 4385161344070135569}"
    assert field(img, "m_Father") == "{fileID: 4385161344070135569}"
    assert vector(img, "m_SizeDelta") == {"x": 908.0, "y": 102.0}
    assert vector(img, "m_AnchoredPosition") == {"x": 0.0, "y": 261.0}

    # Pin exact source transform/GameObject/sprite chains for the two visible border pieces.
    children = manifest["visual_usage"]["visible_children"]
    assert len(children) == 2
    expected_children = {
        "597719906124241574": ("3751180885874877279", -227.0, -1.0),
        "6958296405779485615": ("4137096087478614813", 227.0, 1.0),
    }
    for child in children:
        transform_id = child["transform_file_id"]
        assert transform_id in expected_children
        expected_go, expected_x, expected_scale_x = expected_children[transform_id]
        assert child["game_object_file_id"] == expected_go
        block = exact_block(source, transform_id, 224)
        assert field(block, "m_Father") == "{fileID: 3734920066888637523}"
        assert vector(block, "m_SizeDelta") == {"x": 454.0, "y": 138.0}
        assert child["sprite_guid"] == "0186443df1b8cd144b20fc4dd41fc626"
        assert child["sprite_file_id"] == 2260871125762619715
        assert child["source_image_enabled"] is True and child["active_in_prefab"] is True
        game_object = exact_block(source, child["game_object_file_id"], 1)
        assert field(game_object, "m_Name") == "img" and field(game_object, "m_IsActive") == "1"
        component = exact_block(source, child["image_component_file_id"], 114)
        assert field(component, "m_Enabled") == "1"
        assert re.search(r"(?ms)^  m_Sprite: \{fileID: 2260871125762619715, guid: 0186443df1b8cd144b20fc4dd41fc626,\s*type: 3\}", component)
        assert vector(block, "m_AnchoredPosition") == child["anchored_position"]
        assert vector(block, "m_LocalScale") == child["local_scale"]
        assert vector(block, "m_AnchoredPosition") == {"x": expected_x, "y": 0.0}
        assert vector(block, "m_LocalScale") == {"x": expected_scale_x, "y": 1.0, "z": 1.0}

    panel = exact_block(source, "4005996255782681250", 224)
    panel_image = exact_block(source, "707079810408244484", 114)
    assert field(panel, "m_Father") == "{fileID: 7125139485787537580}"
    assert vector(panel, "m_SizeDelta") == {"x": 676.0, "y": 761.0}
    assert field(panel_image, "m_Enabled") == "1"
    assert manifest["visual_usage"]["full_panel_image"]["parent_transform_file_id"] == "7125139485787537580"
    assert manifest["visual_usage"]["canvas_context"]["canvas_reference_resolution"].startswith("not serialized")
    assert manifest["acceptance"]["ue_import"] == "not_run"
    assert manifest["acceptance"]["runtime_visual_accepted"] is False
    print(f"passed: source PNG byte-identical, 512x512; one sprite rect; two source image components and hierarchy verified; manifest sha256={sha(MANIFEST)}")


if __name__ == "__main__":
    verify()
