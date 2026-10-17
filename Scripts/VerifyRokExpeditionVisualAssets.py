from __future__ import annotations

import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "Saved/RokUnityMigration/ExpeditionVisualAssets/v1/atlas_manifest.json"
REPORT = ROOT / "Saved/RokUnityMigration/BatchReview20261009NightTimeAndExpedition/expedition_visual_assets_prepare.json"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify() -> None:
    payload = json.loads(MANIFEST.read_text(encoding="utf-8"))
    report = json.loads(REPORT.read_text(encoding="utf-8"))
    assert payload["schema"] == "rok.expedition.visual_assets.v1"
    assert report["schema"] == "rok.expedition.visual_assets_prepare.v1"
    assert report["status"] == "prepared_not_imported"
    assert report["manifest_sha256"] == digest(MANIFEST)
    assert len(payload["atlases"]) == report["atlas_count"] == 4
    assert len(payload["selected_sprite_references"]) == report["selected_reference_count"] == 11
    assert payload["unresolved_selected_references"] == []
    assert report["nodes_without_static_sprite_count"] == len(
        payload["selected_nodes_without_static_sprite"]) == 4
    assert report["ue_import"] == report["unity_gameview"] == "not_run"
    for relative, expected in payload["sources_sha256"].items():
        assert digest(ROOT / relative) == expected, relative
    atlas_paths = set()
    for atlas in payload["atlases"]:
        source = ROOT / atlas["source"]
        prepared = ROOT / atlas["prepared"]
        raw = source.read_bytes()
        assert raw == prepared.read_bytes(), prepared
        assert hashlib.sha256(raw).hexdigest() == atlas["sha256"]
        assert raw[:8] == b"\x89PNG\r\n\x1a\n" and raw[12:16] == b"IHDR"
        assert list(struct.unpack(">II", raw[16:24])) == atlas["size"]
        atlas_paths.add(atlas["source"])
    flags = set()
    for item in payload["selected_sprite_references"]:
        prefab = ROOT / {
            "map_flag": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_battle/UIExpeditionFight/UI_Item_ExpeditionMapLevel.prefab",
            "task": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_battle/UIExpeditionFight/UI_IF_ExpeditionFightTask.prefab",
            "main": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_battle/UIExpeditionFight/UI_IF_ExpeditionFight.prefab",
        }[item["prefab_role"]]
        source = prefab.read_text(encoding="utf-8-sig")
        marker = f"--- !u!114 &{item['component_id']}\n"
        assert source.count(marker) == 1
        component = source.split(marker, 1)[1].split("--- !u!", 1)[0]
        assert f"m_GameObject: {{fileID: {item['prefab_game_object_id']}}}" in component
        assert re.search(r"m_Sprite: \{fileID: " + str(item["source_file_id"]) +
                         r", guid: " + item["source_guid"] + r",", component)
        assert item["atlas_source"] in atlas_paths
        png = ROOT / item["atlas_source"]
        meta = png.with_suffix(".png.meta")
        meta_text = meta.read_text(encoding="utf-8-sig")
        assert f"guid: {item['source_guid']}" in meta_text
        x, y, w, h = item["rect_unity_bottom_left"]
        aw, ah = item["atlas_size"]
        assert item["rect_png_top_left"] == [x, ah - y - h, w, h]
        uv = [x / aw, (ah - y - h) / ah, (x + w) / aw, (ah - y) / ah]
        assert all(abs(a - b) < 1e-12 for a, b in zip(item["slate_uv"], uv))
        assert 0 <= x < aw and 0 <= y < ah and w > 0 and h > 0
        sheet = png.with_suffix(".tpsheet")
        if sheet.exists():
            pair = re.escape(str(item["source_file_id"]))
            assert re.search(r"(?m)^      213: " + pair + r"\r?\n    second: " +
                             re.escape(item["sprite_name"]) + r"\r?$", meta_text)
            row = next(line.split(";") for line in sheet.read_text(encoding="utf-8-sig").splitlines()
                       if line.startswith(item["sprite_name"] + ";"))
            assert [int(value) for value in row[1:5]] == [x, y, w, h]
            assert [float(value.strip()) for value in row[5:7]] == item["pivot"]
            assert [int(value.strip()) for value in row[7:11]] == item["border"]
        else:
            assert item["source_file_id"] == 21300000 and "spriteMode: 1" in meta_text
            assert [x, y, w, h] == [0, 0, aw, ah]
        if item["prefab_role"] == "map_flag" and item["name"].startswith("img_flag"):
            flags.add(item["name"])
    assert flags == {"img_flag1", "img_flag2", "img_flag3", "img_flag4"}
    assert payload["flag_state_reference"] == {
        "normal_unlocked": "img_flag1", "normal_locked": "img_flag2",
        "boss_unlocked": "img_flag3", "boss_locked": "img_flag4",
        "stars_visible_only_if_unlocked": True,
        "reward_box_visible_only_if_info_exists_unclaimed_three_stars": True,
        "state_authority": "ExpeditionInfo and isUnlock are account data, unavailable in this read-only preview",
    }
    assert payload["runtime_state_gap"].startswith("flag variants need actual account")
    print(f"passed: {len(payload['selected_sprite_references'])} source sprite references, "
          f"{len(payload['atlases'])} byte-identical PNGs, manifest sha256={digest(MANIFEST)}")


if __name__ == "__main__":
    verify()
