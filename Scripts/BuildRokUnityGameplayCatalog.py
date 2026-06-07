import json
from collections import defaultdict
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CONFIG_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "ConfigTables"
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
OUTPUT_JSON = MIGRATION_ROOT / "unity_gameplay_catalog.json"
OUTPUT_MD = MIGRATION_ROOT / "unity_gameplay_catalog.md"

SOURCE_TABLES = {
    "building_type": "BuildingTypeConfig",
    "building_menu": "BuildingMenuData",
    "building_count_limit": "BuildingCountLimit",
    "building_level": "BuildingLevelData",
    "building_produce": "BuildingResourcesProduce",
    "troops": "Arms",
    "heroes": "Hero",
    "hero_skills": "HeroSkill",
    "hero_talents": "HeroTalent",
    "main_tasks": "TaskMain",
}


def load_table(name):
    path = CONFIG_ROOT / f"{name}.json"
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def int_field(row, name, default=0):
    value = row.get(name, default)
    if value in ("", None):
        return default
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return default


def float_field(row, name, default=0.0):
    value = row.get(name, default)
    if value in ("", None):
        return default
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def str_field(row, name, default=""):
    value = row.get(name, default)
    if value is None:
        return default
    return str(value)


def int_list_field(row, name):
    value = row.get(name, "")
    if value in ("", None):
        return []
    if isinstance(value, list):
        return [int_field({"v": item}, "v") for item in value if str(item).strip()]
    parts = str(value).replace(",", "|").split("|")
    return [int(part) for part in parts if part.strip() and part.strip().lstrip("-").isdigit()]


def group_by(rows, key_name):
    groups = defaultdict(list)
    for row in rows:
        groups[int_field(row, key_name)].append(row)
    return groups


def index_by(rows, key_name):
    return {int_field(row, key_name): row for row in rows}


def resource_cost(row):
    return {
        "food": int_field(row, "food"),
        "wood": int_field(row, "wood"),
        "stone": int_field(row, "stone"),
        "gold": int_field(row, "coin"),
        "denar": int_field(row, "denar"),
    }


def make_menu_button(row):
    return {
        "id": int_field(row, "ID"),
        "func": str_field(row, "func"),
        "param": int_field(row, "param"),
        "param1": int_field(row, "param1"),
        "show_type": [
            int_field(row, "showType1"),
            int_field(row, "showType2"),
            int_field(row, "showType3"),
        ],
        "show_level_min": int_field(row, "showLevelMin"),
        "show_level_max": int_field(row, "showLevelMax"),
        "icon": str_field(row, "btnIcon"),
    }


def make_level(row):
    return {
        "id": int_field(row, "ID"),
        "type_id": int_field(row, "type"),
        "level": int_field(row, "level"),
        "size": {
            "width": int_field(row, "width"),
            "length": int_field(row, "length"),
        },
        "requirements": [
            {"type": int_field(row, "reqType1"), "level": int_field(row, "reqLevel1")},
            {"type": int_field(row, "reqType2"), "level": int_field(row, "reqLevel2")},
            {"type": int_field(row, "reqType3"), "level": int_field(row, "reqLevel3")},
        ],
        "cost": resource_cost(row),
        "item_cost": {
            "item_type": int_field(row, "itemType1"),
            "item_count": int_field(row, "itemCnt"),
        },
        "build_seconds": int_field(row, "buildingTime"),
        "power": int_field(row, "power"),
        "mystery_store_cd": int_field(row, "mysteryStoreCD"),
    }


def make_production(row):
    return {
        "id": int_field(row, "ID"),
        "type_id": int_field(row, "type"),
        "level": int_field(row, "level"),
        "produce_speed": int_field(row, "produceSpeed"),
        "gather_min": int_field(row, "gatherMin"),
        "gather_max": int_field(row, "gatherMax"),
    }


def make_count_limit(row):
    return {
        "id": int_field(row, "ID"),
        "town_level": int_field(row, "townLevel"),
        "build_type": int_field(row, "buildType"),
        "limit": int_field(row, "buildCountLimit"),
    }


def make_building_type(row, menus_by_id, levels_by_type, produce_by_type, limits_by_type):
    type_id = int_field(row, "type")
    menu_buttons = [menus_by_id[menu_id] for menu_id in int_list_field(row, "menuButtons") if menu_id in menus_by_id]
    levels = sorted((make_level(item) for item in levels_by_type.get(type_id, [])), key=lambda item: item["level"])
    production = sorted((make_production(item) for item in produce_by_type.get(type_id, [])), key=lambda item: item["level"])
    count_limits = sorted((make_count_limit(item) for item in limits_by_type.get(type_id, [])), key=lambda item: item["town_level"])
    level_numbers = [item["level"] for item in levels]
    total_power = sum(item["power"] for item in levels)

    return {
        "type_id": type_id,
        "name_locale_id": int_field(row, "l_nameId"),
        "group": int_field(row, "group"),
        "is_move": int_field(row, "isMove"),
        "tip_list": int_field(row, "tipList"),
        "desc_locale_id": int_field(row, "l_descId"),
        "short_desc_locale_id": int_field(row, "l_descId2"),
        "size": {
            "width": int_field(row, "width"),
            "length": int_field(row, "length"),
        },
        "menu_buttons": menu_buttons,
        "level_summary": {
            "count": len(levels),
            "min_level": min(level_numbers) if level_numbers else 0,
            "max_level": max(level_numbers) if level_numbers else 0,
            "total_power": total_power,
            "level1": levels[0] if levels else {},
            "level2": levels[1] if len(levels) > 1 else {},
        },
        "count_limits": count_limits,
        "production": production,
        "levels": levels,
    }


def make_troop(row):
    return {
        "id": int_field(row, "ID"),
        "name_locale_id": int_field(row, "l_armsID"),
        "arms_type": int_field(row, "armsType"),
        "type_name_locale_id": int_field(row, "l_typeName"),
        "sub_type": int_field(row, "subType"),
        "level": int_field(row, "armsLv"),
        "study_id": int_field(row, "studyID"),
        "show_asset": str_field(row, "armsShow"),
        "icon": str_field(row, "icon"),
        "model": str_field(row, "armsModel"),
        "age": int_field(row, "age"),
        "stats": {
            "attack": int_field(row, "attack"),
            "defense": int_field(row, "defense"),
            "hp": int_field(row, "hpMax"),
            "speed": float_field(row, "speed"),
            "capacity": int_field(row, "capacity"),
            "power": int_field(row, "militaryCapability"),
        },
        "skill": int_field(row, "armsSkill"),
        "train_cost": {
            "food": int_field(row, "needFood"),
            "wood": int_field(row, "needWood"),
            "stone": int_field(row, "needStone"),
            "gold": int_field(row, "needGlod"),
        },
        "train_seconds": int_field(row, "endTime"),
        "wounded_cost": {
            "food": int_field(row, "woundedFood"),
            "wood": int_field(row, "woundedWood"),
            "stone": int_field(row, "woundedStone"),
            "gold": int_field(row, "woundedGlod"),
        },
        "wounded_seconds": int_field(row, "woundedTime"),
    }


def make_hero(row):
    skill_ids = int_list_field(row, "skill")
    talent_ids = int_list_field(row, "talent")
    return {
        "id": int_field(row, "ID"),
        "name_locale_id": int_field(row, "l_nameID"),
        "civilization": int_field(row, "civilization"),
        "appellation_locale_id": int_field(row, "l_appellationID"),
        "init_star": int_field(row, "initStar"),
        "max_star": int_field(row, "star"),
        "rare": int_field(row, "rare"),
        "desc_locale_id": int_field(row, "l_desID"),
        "score": int_field(row, "score"),
        "get_item": int_field(row, "getItem"),
        "get_item_num": int_field(row, "getItemNum"),
        "exchange": int_field(row, "exchange"),
        "language_locale_id": int_field(row, "l_heroLanguage"),
        "get_limit": int_field(row, "getLimit"),
        "skill_ids": skill_ids,
        "talent_ids": talent_ids,
        "list_display": int_field(row, "listDisplay"),
        "icon": str_field(row, "heroIcon"),
        "model": str_field(row, "heroModel"),
        "action": str_field(row, "heroAction"),
        "scene": str_field(row, "heroScene"),
        "scene_point": str_field(row, "heroScenePoint"),
        "voice_opening": str_field(row, "voiceOpening"),
        "voice_select": str_field(row, "voiceSelect"),
        "voice_move": str_field(row, "voiceMove"),
        "location_points": [
            str_field(row, "heroLocationPoint1"),
            str_field(row, "heroLocationPoint2"),
        ],
        "scale": float_field(row, "heroScale"),
    }


def make_hero_skill(row):
    return {
        "id": int_field(row, "ID"),
        "name_locale_id": int_field(row, "l_nameID"),
        "type": int_field(row, "type"),
        "type_locale_id": int_field(row, "l_typeID"),
        "battle_skill_id": int_field(row, "skillBattleID"),
        "anger": int_field(row, "anger"),
        "open": int_field(row, "open"),
        "open_locale_id": int_field(row, "l_openID"),
        "awaken_enhance": int_field(row, "awakenEnhance"),
        "icon": str_field(row, "icon"),
        "level_up_cost_item": int_field(row, "levelUpCostItem"),
        "message_locale_id": int_field(row, "l_mesID"),
        "level_effects": [
            {"data": str_field(row, "lvEffectDate1"), "locale_id": int_field(row, "l_lvEffect1")},
            {"data": str_field(row, "lvEffectDate2"), "locale_id": int_field(row, "l_lvEffect2")},
            {"data": str_field(row, "lvEffectDate3"), "locale_id": int_field(row, "l_lvEffect3")},
            {"data": str_field(row, "lvEffectDate4"), "locale_id": int_field(row, "l_lvEffect4")},
        ],
        "army_effect": str_field(row, "armyEffect"),
        "sound": str_field(row, "sound"),
    }


def make_hero_talent(row):
    return {
        "id": int_field(row, "ID"),
        "talent_locale_id": int_field(row, "l_talentID"),
        "icon": str_field(row, "icon1"),
        "gain_tree": int_field(row, "gainTree"),
        "mastery_group_id": int_field(row, "masteryGroupID"),
        "tips_locale_id": int_field(row, "l_tipsID"),
        "type": int_field(row, "type"),
    }


def make_main_task(row):
    return {
        "id": int_field(row, "ID"),
        "name_locale_id": int_field(row, "l_nameId"),
        "name": str_field(row, "name"),
        "desc_locale_id": int_field(row, "l_desc"),
        "desc": str_field(row, "desc"),
        "type": int_field(row, "type"),
        "param1": int_field(row, "param1"),
        "param2": int_field(row, "param2"),
        "require": int_field(row, "require"),
        "reward": int_field(row, "reward"),
        "next_id": int_field(row, "nextId"),
        "icon": str_field(row, "iconImg"),
    }


def main():
    tables = {alias: load_table(table_name) for alias, table_name in SOURCE_TABLES.items()}
    rows = {alias: table["rows"] for alias, table in tables.items()}

    menus_by_id = {menu_id: make_menu_button(row) for menu_id, row in index_by(rows["building_menu"], "ID").items()}
    levels_by_type = group_by(rows["building_level"], "type")
    produce_by_type = group_by(rows["building_produce"], "type")
    limits_by_type = group_by(rows["building_count_limit"], "buildType")

    building_types = [
        make_building_type(row, menus_by_id, levels_by_type, produce_by_type, limits_by_type)
        for row in sorted(rows["building_type"], key=lambda item: int_field(item, "type"))
    ]
    troops = [make_troop(row) for row in sorted(rows["troops"], key=lambda item: int_field(item, "ID"))]
    heroes = [make_hero(row) for row in sorted(rows["heroes"], key=lambda item: int_field(item, "ID"))]
    hero_skills = [make_hero_skill(row) for row in sorted(rows["hero_skills"], key=lambda item: int_field(item, "ID"))]
    hero_talents = [make_hero_talent(row) for row in sorted(rows["hero_talents"], key=lambda item: int_field(item, "ID"))]
    main_tasks = [make_main_task(row) for row in sorted(rows["main_tasks"], key=lambda item: int_field(item, "ID"))]

    troop_family_counts = defaultdict(int)
    for troop in troops:
        troop_family_counts[str(troop["arms_type"])] += 1

    hero_rare_counts = defaultdict(int)
    for hero in heroes:
        hero_rare_counts[str(hero["rare"])] += 1

    summary = {
        "building_type_count": len(building_types),
        "building_level_row_count": len(rows["building_level"]),
        "building_menu_count": len(rows["building_menu"]),
        "building_count_limit_row_count": len(rows["building_count_limit"]),
        "building_produce_row_count": len(rows["building_produce"]),
        "troop_count": len(troops),
        "troop_family_counts": dict(sorted(troop_family_counts.items(), key=lambda item: int(item[0]))),
        "hero_count": len(heroes),
        "hero_rare_counts": dict(sorted(hero_rare_counts.items(), key=lambda item: int(item[0]))),
        "hero_skill_count": len(hero_skills),
        "hero_talent_count": len(hero_talents),
        "main_task_count": len(main_tasks),
    }

    manifest = {
        "schema_version": 1,
        "source_tables": {
            alias: {
                "table_name": table_name,
                "json_path": f"Saved/RokUnityMigration/ConfigTables/{table_name}.json",
                "source_path": tables[alias].get("source_path", ""),
                "row_count": tables[alias].get("row_count", len(rows[alias])),
            }
            for alias, table_name in SOURCE_TABLES.items()
        },
        "summary": summary,
        "building_types": building_types,
        "troops": troops,
        "heroes": heroes,
        "hero_skills": hero_skills,
        "hero_talents": hero_talents,
        "main_tasks": main_tasks,
    }

    MIGRATION_ROOT.mkdir(parents=True, exist_ok=True)
    OUTPUT_JSON.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# Unity Gameplay Catalog",
        "",
        f"- Building types: {summary['building_type_count']}",
        f"- Building level rows: {summary['building_level_row_count']}",
        f"- Building menu buttons: {summary['building_menu_count']}",
        f"- Troops: {summary['troop_count']}",
        f"- Heroes: {summary['hero_count']}",
        f"- Hero skills: {summary['hero_skill_count']}",
        f"- Hero talents: {summary['hero_talent_count']}",
        f"- Main tasks: {summary['main_task_count']}",
        "",
        "## Building Types",
        "",
        "| Type | Group | Size | Levels | Menus | Produce Levels |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for item in building_types:
        lines.append(
            f"| {item['type_id']} | {item['group']} | {item['size']['width']}x{item['size']['length']} | "
            f"{item['level_summary']['min_level']}-{item['level_summary']['max_level']} ({item['level_summary']['count']}) | "
            f"{len(item['menu_buttons'])} | {len(item['production'])} |"
        )

    lines.extend([
        "",
        "## Troop Families",
        "",
        "| ArmsType | Rows |",
        "| --- | --- |",
    ])
    for arms_type, count in summary["troop_family_counts"].items():
        lines.append(f"| {arms_type} | {count} |")

    lines.extend([
        "",
        "## First Main Tasks",
        "",
        "| ID | Name | Type | Param1 | Param2 | Next |",
        "| --- | --- | --- | --- | --- | --- |",
    ])
    for task in main_tasks[:20]:
        lines.append(f"| {task['id']} | {task['name']} | {task['type']} | {task['param1']} | {task['param2']} | {task['next_id']} |")
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(json.dumps({
        "output": str(OUTPUT_JSON),
        "summary": summary,
    }, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
