from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path

from BuildRokUnityEconomyCatalog import localized_text, read_language_lookup
from rok_data_paths import DATA_ROOT, label

ROOT = Path(__file__).resolve().parents[1]
CSV = ROOT / "ref/config-tables/编译客户端配置/CSV/Expedition.csv"
UI = ROOT / "ref/client-unity/Assets/BundleAssets/UI"
PREFAB = UI / "UIPrefabs/ui_prefabs_battle/UIExpeditionFight/UI_IF_ExpeditionFightTask.prefab"
MAP_PREFAB = UI / "UIPrefabs/ui_prefabs_battle/UIExpeditionFight/UI_IF_ExpeditionFight.prefab"
FLAG_PREFAB = UI / "UIPrefabs/ui_prefabs_battle/UIExpeditionFight/UI_Item_ExpeditionMapLevel.prefab"
CLIPS = [
    UI / "UIAnimation/shared_uianimation/UA_MainIF_Right_On.anim",
    UI / "UIAnimation/shared_uianimation/UA_MainIF_Right_Off.anim",
    UI / "UIAnimation/UA_ExpeditionTask_centerShow.anim",
    UI / "UIAnimation/UA_ExpeditionTask_centerHide.anim",
]
OUTPUT = DATA_ROOT / "expedition_stage_preview.json"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def numbers(value: str) -> list[int]:
    return [int(part) for part in value.split("|") if part]


def build() -> dict:
    language, _ = read_language_lookup()
    with CSV.open("r", encoding="utf-8-sig", newline="") as stream:
        entries = list(csv.DictReader(stream))
    assert len(entries) == 81 and entries[0]["ID"] == "int", "Expedition.csv row contract changed"
    rows = []
    for entry in entries[1:]:
        stage_id = int(entry["ID"])
        star_ids = numbers(entry["starDesc"])
        star_text = [localized_text(language, value, "cn") for value in star_ids]
        star_conditions = [numbers(entry["starCondition2"]), numbers(entry["starCondition3"])]
        assert stage_id == len(rows) + 1 and int(entry["level"]) == stage_id
        assert len(numbers(entry["battleCoordinate"])) == 2
        assert len(star_ids) == 3 and all(star_text)
        assert len(star_conditions[0]) == len(star_conditions[1]) == 2
        display_stars = [star_text[0], star_text[1].format(star_conditions[0][1]),
                         star_text[2].format(star_conditions[1][1])]
        rows.append({
            "id": stage_id,
            "level": int(entry["level"]),
            "front_number": int(entry["frontNumber"]),
            "battle_coordinate": numbers(entry["battleCoordinate"]),
            "type": int(entry["type"]),
            "battle_id": int(entry["battleID"]),
            "time_limit_seconds": int(entry["times"]),
            "troops_number": int(entry["troopsNumber"]),
            "star_description_ids": star_ids,
            "star_descriptions_zh_cn": display_stars,
            "star_description_templates_zh_cn": star_text,
            "star_conditions": star_conditions,
            "first_reward_group": int(entry["firstReward"]),
            "reward_groups": [int(entry[key]) for key in ("reward1", "reward2", "reward3")],
        })
    assert len(rows) == 80 and rows[0]["battle_coordinate"] == [466, 79]
    assert rows[1]["battle_coordinate"] == [601, 101]
    assert rows[0]["star_description_ids"] == [805038, 805039, 805040]
    clips = {path.stem: digest(path) for path in CLIPS}
    # These are source-serialized keys. The C++ view evaluates zero-tangent
    # Hermite segments, with the separate side/center durations retained.
    curves = {
        "side_show_x": [[0, 100], [0.15, -20], [0.25, 0]],
        "side_hide_x": [[0, 0], [0.1, -20], [0.25, 100]],
        "side_show_alpha": [[0, 0], [0.25, 1]],
        "side_hide_alpha": [[0, 1], [0.25, 0]],
        "center_show_scale": [[0, 1.05], [0.16666667, 0.9], [0.33333334, 1], [0.5833333, 1]],
        "center_show_flag_alpha": [[0, 0], [0.33333334, 0], [0.5833333, 1]],
        "center_hide_scale": [[0, 1], [0.33333334, 1.05]],
        "center_hide_alpha": [[0, 1], [0.33333334, 0]],
    }
    expected_fragments = {
        "UA_MainIF_Right_On": ("time: 0.15", "value: -20", "time: 0.25"),
        "UA_MainIF_Right_Off": ("time: 0.1", "value: -20", "time: 0.25"),
        "UA_ExpeditionTask_centerShow": ("time: 0.16666667", "value: 0.9", "time: 0.5833333"),
        "UA_ExpeditionTask_centerHide": ("time: 0.33333334", "value: 1.05", "value: 0"),
    }
    for path in CLIPS:
        source = path.read_text(encoding="utf-8-sig")
        assert all(fragment in source for fragment in expected_fragments[path.stem]), path
    sources = [CSV, ROOT / "ref/config-tables/s_Language.xlsm", PREFAB, MAP_PREFAB, FLAG_PREFAB, *CLIPS]
    return {
        "schema": "rok.expedition_stage_preview.v1",
        "source_stage_count": len(rows),
        "account_state": "unsynced",
        "preview_only": True,
        "source_files_sha256": {label(path): digest(path) for path in sources},
        "flag_prefab": label(FLAG_PREFAB),
        "flag_visual_asset_status": "source_guid_only_ue_texture_not_imported",
        "animation_clips_sha256": clips,
        "animation_zero_tangent_hermite_curves": curves,
        "stages": rows,
    }


if __name__ == "__main__":
    payload = build()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
                      encoding="utf-8", newline="\n")
    print(f"{OUTPUT}: {len(payload['stages'])} source stages, sha256={digest(OUTPUT)}")
