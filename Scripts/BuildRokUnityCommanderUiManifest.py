import hashlib
import json
import re
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
HERO_ROOT = (
    PROJECT_ROOT
    / "ref"
    / "client-unity"
    / "Assets"
    / "BundleAssets"
    / "UI"
    / "UITextureStatic"
    / "ui_res_hero"
)
OUTPUT_PATH = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_commander_ui.json"
OUTPUT_MD_PATH = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_commander_ui.md"

ATLAS_TARGETS = {
    "hero_background.png": ([1024, 512], "/Game/RokPrototype/UI/CommanderAtlases/rok_commander_background"),
    "hero_head.png": ([1024, 2048], "/Game/RokPrototype/UI/CommanderAtlases/rok_commander_head"),
    "ui_hero.png": ([2048, 512], "/Game/RokPrototype/UI/CommanderAtlases/rok_commander_ui"),
    "hero_skill2.png": ([2048, 2048], "/Game/RokPrototype/UI/CommanderAtlases/rok_commander_skill2"),
}

SPRITES = {
    "hero_background.png": ["hero_background_10"],
    "hero_head.png": [
        "hero_head_2007",
        "hero_head_4000",
        "hero_head_1000",
        "hero_head_1001",
        "hero_head_1002",
        "hero_head_1003",
        "hero_head_1005",
        "hero_head_1006",
        "img_com_heroframe4",
        "img_com_heroframe5",
        "img_com_heroframe_up1",
    ],
    "ui_hero.png": [
        "img_hero_rare4",
        "btn_hero_1000",
        "btn_hero_1000_1",
        "icon_hero_1001_1",
        "icon_hero_1001_2",
        "icon_hero_1002",
        "icon_hero_1004",
        "pl_hero_1003",
        "pl_hero_1004",
        "pl_hero_1007_3",
        "pl_hero_1014",
    ],
    "hero_skill2.png": [
        "hero_skill_200701",
        "hero_skill_200702",
        "hero_skill_200703",
        "hero_skill_200704",
        "hero_skill_200705",
    ],
}


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_tpsheet(path):
    result = {}
    pattern = re.compile(r"^([^:#][^;]*);(\d+);(\d+);(\d+);(\d+);")
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        match = pattern.match(line.strip())
        if match:
            result[match.group(1)] = [int(value) for value in match.groups()[1:]]
    return result


def normalized_uv(rect, size):
    x, y, width, height = rect
    atlas_width, atlas_height = size
    top = atlas_height - y - height
    return {
        "min": [x / atlas_width, top / atlas_height],
        "max": [(x + width) / atlas_width, (top + height) / atlas_height],
    }


def main():
    mappings = []
    for atlas_name, sprite_names in SPRITES.items():
        atlas_path = HERO_ROOT / atlas_name
        sheet_path = HERO_ROOT / atlas_name.replace(".png", ".tpsheet")
        size, target = ATLAS_TARGETS[atlas_name]
        entries = parse_tpsheet(sheet_path)
        for sprite_name in sprite_names:
            if sprite_name not in entries:
                raise RuntimeError("Missing commander sprite {0} in {1}".format(sprite_name, sheet_path))
            rect = entries[sprite_name]
            mappings.append(
                {
                    "source_atlas": atlas_path.relative_to(PROJECT_ROOT).as_posix(),
                    "source_tpsheet": sheet_path.relative_to(PROJECT_ROOT).as_posix(),
                    "source_atlas_sha256": sha256_file(atlas_path),
                    "source_tpsheet_sha256": sha256_file(sheet_path),
                    "sprite_name": sprite_name,
                    "rect": rect,
                    "atlas_size": size,
                    "normalized_uv": normalized_uv(rect, size),
                    "ue_atlas_target": target,
                }
            )

    portrait_path = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "Commander" / "hero_2007_idle.png"
    if not portrait_path.exists():
        raise FileNotFoundError("Missing deterministic Hero_2007 Spine render: {0}".format(portrait_path))

    manifest = {
        "version": 1,
        "reference_resolution": [1440, 810],
        "screen_match_mode": "Expand",
        "reference_capture_expanded_resolution": [1440, 900],
        "source_prefab": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_hero/UICaptain/UI_IF_Captain.prefab",
        "source_head_prefab": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_hero/UICaptain/UI_Item_CaptainHead.prefab",
        "source_mediator": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Captain/CaptainMediator.cs",
        "source_hero_table": "ref/config-tables/编译客户端配置/CSV/Hero.csv",
        "selected_hero": {
            "id": 2007,
            "name": "拜巴尔一世",
            "appellation": "天选之豹",
            "level": 1,
            "max_level": 10,
            "stars": 1,
            "max_stars": 6,
            "troop_capacity": 3500,
            "power": 2300,
            "roles": ["骑兵", "攻城", "技能"],
            "skills": [200701, 200702, 200703, 200704, 200705],
            "model": "Hero_2007_SD",
        },
        "portrait": {
            "source_skeleton": "ref/client-unity/Assets/BundleAssets/Troop/spine_hero/Hero_2007_SD/Materials/Hero_2007_SD.asset",
            "source_texture": "ref/client-unity/Assets/BundleAssets/Troop/spine_hero/Hero_2007_SD/Materials/Hero_2007.png",
            "animation": "idle",
            "sample_time_seconds": 0.55,
            "generated_png": portrait_path.relative_to(PROJECT_ROOT).as_posix(),
            "generated_png_sha256": sha256_file(portrait_path),
            "ue_target": "/Game/RokPrototype/UI/CommanderAtlases/rok_commander_hero_2007_idle",
        },
        "layouts": {
            "background": [0.0, 0.0, 1440.0, 900.0],
            "left_roster": [13.0, 119.0, 304.0, 701.0],
            "hero_portrait": [335.0, 117.0, 650.0, 744.0],
            "hero_detail": [975.0, 125.0, 425.0, 650.0],
            "power": [527.0, 844.0, 390.0, 36.0],
        },
        "ue_widget": "Source/Rok/RokUnityFeatureScreenWidget.cpp",
        "ue_hud": "Source/Rok/RokMainHudWidget.cpp",
        "ue_import_script": "Scripts/ImportRokUnityCommanderUi.py",
        "sprites": mappings,
    }
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    OUTPUT_MD_PATH.write_text(
        "# Unity Commander UI\n\n"
        "- Selected hero: 2007 / 拜巴尔一世 / 天选之豹\n"
        "- Source prefab: `UI_IF_Captain.prefab`\n"
        "- Portrait: deterministic `Hero_2007_SD` Spine `idle` render at 0.55 seconds\n"
        "- Sprite mappings: {0}\n".format(len(mappings)),
        encoding="utf-8",
    )
    print("Wrote {0} with {1} sprite mappings".format(OUTPUT_PATH, len(mappings)))


if __name__ == "__main__":
    main()
