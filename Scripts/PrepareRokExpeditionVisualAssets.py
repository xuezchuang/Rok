from __future__ import annotations

import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "ref/client-unity/Assets/BundleAssets/UI"
PREFABS = {
    "map_flag": UI / "UIPrefabs/ui_prefabs_battle/UIExpeditionFight/UI_Item_ExpeditionMapLevel.prefab",
    "task": UI / "UIPrefabs/ui_prefabs_battle/UIExpeditionFight/UI_IF_ExpeditionFightTask.prefab",
    "main": UI / "UIPrefabs/ui_prefabs_battle/UIExpeditionFight/UI_IF_ExpeditionFight.prefab",
}
ATLAS_ROOT = UI / "UITextureStatic"
OUTPUT = ROOT / "Saved/RokUnityMigration/ExpeditionVisualAssets/v1"
REPORT = ROOT / "Saved/RokUnityMigration/BatchReview20261009NightTimeAndExpedition/expedition_visual_assets_prepare.json"
SELECTED_NAMES = {
    "map_flag": {"img_flag1", "img_flag2", "img_flag3", "img_flag4", "img_def", "img_box", "img_atk"},
    "task": {"bg", "img_mask", "img_captains"},
    "main": {"bg", "img_frame", "img_firstBg", "title"},
}
FLAG_RULE_SOURCE = ROOT / (
    "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/"
    "UI_Item_ExpeditionMapLevel_SubView.cs"
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def documents(path: Path) -> dict[str, tuple[int, str]]:
    source = path.read_text(encoding="utf-8-sig")
    result = {}
    for match in re.finditer(r"(?m)^--- !u!(\d+) &(\d+)\r?\n", source):
        following = source.find("--- !u!", match.end())
        result[match.group(2)] = (int(match.group(1)), source[match.end():following if following >= 0 else len(source)])
    return result


def scalar(block: str, field: str) -> str:
    found = re.search(rf"(?m)^  {re.escape(field)}: (.*)$", block)
    return found.group(1).strip() if found else ""


def vector(block: str, field: str) -> dict[str, float] | None:
    value = scalar(block, field)
    if not value.startswith("{"): return None
    return {key: float(number) for key, number in re.findall(r"([xyzw]):\s*([-\d.]+)", value)}


def atlas_index() -> dict[tuple[str, int], dict]:
    result = {}
    for meta in ATLAS_ROOT.rglob("*.png.meta"):
        text = meta.read_text(encoding="utf-8-sig")
        guid = re.search(r"(?m)^guid: ([0-9a-f]{32})$", text)
        if not guid: continue
        png = meta.with_suffix("")
        sheet = png.with_suffix(".tpsheet")
        if not sheet.exists():
            if "spriteMode: 1" not in text or (guid.group(1), 21300000) in result:
                continue
            raw = png.read_bytes()
            if raw[:8] != b"\x89PNG\r\n\x1a\n" or raw[12:16] != b"IHDR":
                continue
            width, height = struct.unpack(">II", raw[16:24])
            pivot_match = re.search(r"spritePivot: \{x: ([-\d.]+), y: ([-\d.]+)\}", text)
            border_match = re.search(r"spriteBorder: \{x: ([-\d.]+), y: ([-\d.]+), z: ([-\d.]+), w: ([-\d.]+)\}", text)
            if not pivot_match or not border_match: continue
            result[(guid.group(1), 21300000)] = {
                "sprite_name": png.stem, "png": png, "meta": meta, "sheet": None,
                "atlas_size": [width, height], "rect_unity_bottom_left": [0, 0, width, height],
                "pivot": [float(pivot_match.group(1)), float(pivot_match.group(2))],
                "border": [float(v) for v in border_match.groups()],
                "standalone": True,
            }
            continue
        sheet_text = sheet.read_text(encoding="utf-8-sig")
        size = re.search(r"(?m)^:size=(\d+)x(\d+)$", sheet_text)
        if not size: continue
        sprites = {}
        for line in sheet_text.splitlines():
            fields = [part.strip() for part in line.split(";")]
            if len(fields) < 11 or not re.fullmatch(r"-?\d+", fields[1]): continue
            sprites[fields[0]] = {
                "rect_unity_bottom_left": [int(part) for part in fields[1:5]],
                "pivot": [float(part) for part in fields[5:7]],
                "border": [int(part) for part in fields[7:11]],
            }
        for match in re.finditer(r"(?m)^      213: (-?\d+)\r?\n    second: ([^\r\n]+)", text):
            file_id, sprite_name = int(match.group(1)), match.group(2).strip()
            if sprite_name not in sprites: continue
            result[(guid.group(1), file_id)] = {
                "sprite_name": sprite_name, "png": png, "meta": meta, "sheet": sheet,
                "atlas_size": [int(size.group(1)), int(size.group(2))],
                **sprites[sprite_name],
            }
    return result


def selected_references(path: Path, names: set[str]) -> list[dict]:
    docs = documents(path)
    game_objects = {
        object_id: (scalar(block, "m_Name"), scalar(block, "m_IsActive"))
        for object_id, (kind, block) in docs.items() if kind == 1
    }
    transforms = {}
    for object_id, (kind, block) in docs.items():
        if kind != 224: continue
        go = re.search(r"m_GameObject: \{fileID: (\d+)\}", block)
        if go:
            transforms[go.group(1)] = {
                "size_delta": vector(block, "m_SizeDelta"),
                "anchor_min": vector(block, "m_AnchorMin"),
                "anchor_max": vector(block, "m_AnchorMax"),
                "anchored_position": vector(block, "m_AnchoredPosition"),
            }
    selected = []
    for object_id, (kind, block) in docs.items():
        if kind != 114: continue
        go = re.search(r"m_GameObject: \{fileID: (\d+)\}", block)
        sprite = re.search(r"m_Sprite: \{fileID: (-?\d+), guid: ([0-9a-f]{32}),", block)
        if not go or not sprite or go.group(1) not in game_objects: continue
        name, active = game_objects[go.group(1)]
        if name not in names: continue
        selected.append({
            "prefab_game_object_id": go.group(1),
            "component_id": object_id,
            "name": name,
            "active_serialized": active == "1",
            "source_guid": sprite.group(2),
            "source_file_id": int(sprite.group(1)),
            "image_type": scalar(block, "m_Type"),
            "image_color_gamma": scalar(block, "m_Color"),
            "rect_transform": transforms.get(go.group(1)),
        })
    return selected


def prepare() -> dict:
    index = atlas_index()
    refs, missing, nodes_without_sprite = [], [], []
    used_atlases: dict[Path, dict] = {}
    for role, prefab in PREFABS.items():
        found = selected_references(prefab, SELECTED_NAMES[role])
        if not found: raise RuntimeError(f"No selected Image refs in {prefab}")
        found_names = {item["name"] for item in found}
        nodes_without_sprite.extend({"prefab_role": role, "node_name": name,
            "reason": "no statically resolved m_Sprite on this prefab node; dynamic/RawImage/nested content not inferred"}
            for name in sorted(SELECTED_NAMES[role] - found_names))
        for item in found:
            key = item["source_guid"], item["source_file_id"]
            item["prefab_role"] = role
            if key not in index:
                missing.append({**item, "reason": "GUID/fileID has no verified tpsheet/meta chain"})
                continue
            atlas = index[key]
            x, y, w, h = atlas["rect_unity_bottom_left"]
            aw, ah = atlas["atlas_size"]
            assert 0 <= x < aw and 0 <= y < ah and 0 < w <= aw - x and 0 < h <= ah - y
            item.update({
                "sprite_name": atlas["sprite_name"],
                "atlas_source": str(atlas["png"].relative_to(ROOT)).replace("\\", "/"),
                "atlas_size": atlas["atlas_size"],
                "rect_unity_bottom_left": atlas["rect_unity_bottom_left"],
                "rect_png_top_left": [x, ah - y - h, w, h],
                "slate_uv": [x / aw, (ah - y - h) / ah, (x + w) / aw, (ah - y) / ah],
                "pivot": atlas["pivot"],
                "border": atlas["border"],
            })
            refs.append(item)
            used_atlases[atlas["png"]] = atlas
    # The source state machine chooses a flag by account unlock/three-star
    # state. All referenced variants are preserved; no default account flag.
    assert {r["name"] for r in refs if r["prefab_role"] == "map_flag"} >= {
        "img_flag1", "img_flag2", "img_flag3", "img_flag4"}
    assert not missing, f"Selected sprite aliases unresolved: {missing}"
    OUTPUT.mkdir(parents=True, exist_ok=True)
    atlases = []
    sources = [*PREFABS.values(), FLAG_RULE_SOURCE]
    for png, atlas in sorted(used_atlases.items(), key=lambda pair: str(pair[0])):
        target = OUTPUT / "atlases" / png.name
        target.parent.mkdir(parents=True, exist_ok=True)
        raw = png.read_bytes()
        if target.exists():
            assert target.read_bytes() == raw, f"Existing prepared atlas differs: {target}"
        else:
            target.write_bytes(raw)
        sources.extend([png, atlas["meta"]])
        if atlas["sheet"]: sources.append(atlas["sheet"])
        atlases.append({
            "source": str(png.relative_to(ROOT)).replace("\\", "/"),
            "prepared": str(target.relative_to(ROOT)).replace("\\", "/"),
            "sha256": hashlib.sha256(raw).hexdigest(),
            "size": atlas["atlas_size"],
        })
    payload = {
        "schema": "rok.expedition.visual_assets.v1",
        "scope": "source-derived sprite preparation only; no UE import or runtime account-state claim",
        "sources_sha256": {str(p.relative_to(ROOT)).replace("\\", "/"): sha(p)
                            for p in sorted(set(sources), key=str)},
        "atlases": atlases,
        "selected_sprite_references": refs,
        "unresolved_selected_references": missing,
        "selected_nodes_without_static_sprite": nodes_without_sprite,
        "flag_state_reference": {
            "normal_unlocked": "img_flag1",
            "normal_locked": "img_flag2",
            "boss_unlocked": "img_flag3",
            "boss_locked": "img_flag4",
            "stars_visible_only_if_unlocked": True,
            "reward_box_visible_only_if_info_exists_unclaimed_three_stars": True,
            "state_authority": "ExpeditionInfo and isUnlock are account data, unavailable in this read-only preview",
        },
        "runtime_state_gap": "flag variants need actual account unlock/star state; preview uses neutral marker",
        "source_visual_gap": "prefab instances, original full GameView layout and battle/formation state not runtime verified",
    }
    target = OUTPUT / "atlas_manifest.json"
    encoded = (json.dumps(payload, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if target.exists():
        if target.read_bytes() != encoded:
            assert sha(target) in {
                "1ad14b8ae41e886dd05a14aad2eda6c6abf50d54ec591625eadb19c66e3dec57",
                "b9d5966260fbbe70d5867ef72bfac1d11bcf1b732e821e4e5e2c10ddeb70c477",
            }, "Existing v1 manifest differs"
            target.write_bytes(encoded)  # Only upgrade this task's first, unsealed preparation.
    else:
        target.write_bytes(encoded)
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    report = {
        "schema": "rok.expedition.visual_assets_prepare.v1",
        "status": "prepared_not_imported",
        "manifest": str(target.relative_to(ROOT)).replace("\\", "/"),
        "manifest_sha256": sha(target),
        "atlas_count": len(atlases),
        "selected_reference_count": len(refs),
        "unresolved_count": len(missing),
        "nodes_without_static_sprite_count": len(nodes_without_sprite),
        "ue_import": "not_run",
        "unity_gameview": "not_run",
    }
    report_bytes = (json.dumps(report, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if REPORT.exists():
        if REPORT.read_bytes() != report_bytes:
            assert sha(REPORT) in {
                "5f41c54d09e0b386cfb99f5215154e0c534bd6ed61cb74037d29638a11ff9728",
                "d1fb92055c9d9e6b046586aaa8b011238fca3d21e09b79ff45426d9dd1ea7ae3",
            }, "Existing preparation report differs"
            REPORT.write_bytes(report_bytes)  # Same bounded first-run upgrade.
    else:
        REPORT.write_bytes(report_bytes)
    return report


if __name__ == "__main__":
    result = prepare()
    print(json.dumps(result, ensure_ascii=False))
