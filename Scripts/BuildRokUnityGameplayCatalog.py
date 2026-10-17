import csv
import json
import re
from collections import defaultdict
from pathlib import Path
from rok_data_paths import DATA_ROOT, label


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CONFIG_ROOT = DATA_ROOT / "ConfigTables"
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
OUTPUT_JSON = DATA_ROOT / "unity_gameplay_catalog.json"
OUTPUT_MD = MIGRATION_ROOT / "unity_gameplay_catalog.md"
COMPILED_TASK_CSVS = {
    "main_tasks": PROJECT_ROOT / "ref" / "config-tables" / "编译客户端配置" / "CSV" / "TaskMain.csv",
    "side_tasks": PROJECT_ROOT / "ref" / "config-tables" / "编译客户端配置" / "CSV" / "TaskSide.csv",
    "daily_tasks": PROJECT_ROOT / "ref" / "config-tables" / "编译客户端配置" / "CSV" / "TaskDaily.csv",
    "chapter_tasks": PROJECT_ROOT / "ref" / "config-tables" / "编译客户端配置" / "CSV" / "TaskChapter.csv",
}
HERO_UI_ATLAS_ROOT = (
    PROJECT_ROOT
    / "ref"
    / "client-unity"
    / "Assets"
    / "BundleAssets"
    / "UI"
    / "UITextureStatic"
    / "ui_res_hero"
)
HERO_ICON_ATLAS_SHEETS = {
    "hero_head": HERO_UI_ATLAS_ROOT / "hero_head.tpsheet",
    "hero_skill": HERO_UI_ATLAS_ROOT / "hero_skill.tpsheet",
    "hero_skill2": HERO_UI_ATLAS_ROOT / "hero_skill2.tpsheet",
}

SOURCE_TABLES = {
    "building_type": "BuildingTypeConfig",
    "building_menu": "BuildingMenuData",
    "building_count_limit": "BuildingCountLimit",
    "building_level": "BuildingLevelData",
    "building_produce": "BuildingResourcesProduce",
    "building_barracks": "BuildingBarracks",
    "building_stable": "BuildingStable",
    "building_archery": "BuildingArcheryrange",
    "building_siege": "BuildingSiegeWorkshop",
    "building_hospital": "BuildingHospital",
    "civilizations": "Civilization",
    "troops": "Arms",
    "heroes": "Hero",
    "hero_skills": "HeroSkill",
    "hero_talents": "HeroTalent",
    "main_tasks": "TaskMain",
    "side_tasks": "TaskSide",
    "daily_tasks": "TaskDaily",
    "chapter_tasks": "TaskChapter",
    "vip": "Vip",
    "vip_att": "VipAtt",
    "vip_day_point": "VipDayPoint",
    "vip_store": "VipStore",
    "language": "Language",
    "price": "Price",
}

TRAINING_CAPACITY_SOURCES = {
    9: ("building_barracks", "infantryTrainNumber"),
    10: ("building_stable", "cavalryTrainNumber"),
    11: ("building_archery", "bowmenTrainNumber"),
    12: ("building_siege", "siegeCarTrainNumber"),
}


def load_table(name):
    path = CONFIG_ROOT / f"{name}.json"
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def load_compiled_task_rewards(csv_path):
    """Read the compiled reward column without the extractor's #reward alias.

    The normalized ConfigTables JSON currently collapses both `reward` and the
    non-compiled `#reward` authoring column to the same key. The later column
    overwrites the actual ItemPackageShow group, so task rewards must come from
    the authoritative compiled CSV until that generic extractor is redesigned.
    """
    with csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = csv.DictReader(handle)
        rewards = {}
        for row in rows:
            task_id = str(row.get("ID", "")).strip()
            reward = str(row.get("reward", "")).strip()
            if task_id.lstrip("-").isdigit() and reward.lstrip("-").isdigit():
                rewards[int(task_id)] = int(reward)
        return rewards


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


def localized_text(language_by_id, locale_id, language):
    if locale_id <= 0:
        return ""
    return str_field(language_by_id.get(locale_id, {}), language)


def project_relative(path):
    return label(path)


def read_hero_icon_atlas_sprites():
    sprites = {}
    sources = {}
    for atlas_name, sheet_path in HERO_ICON_ATLAS_SHEETS.items():
        lines = sheet_path.read_text(encoding="utf-8-sig").splitlines()
        size_line = next((line for line in lines if line.startswith(":size=")), "")
        size_match = re.fullmatch(r":size=(\d+)x(\d+)", size_line)
        if not size_match:
            raise RuntimeError(f"Unable to parse atlas size from {sheet_path}")
        atlas_size = [int(size_match.group(1)), int(size_match.group(2))]
        sprite_count = 0
        for line in lines:
            if not line or line.startswith(("#", ":")):
                continue
            parts = [part.strip() for part in line.split(";")]
            if len(parts) < 5:
                raise RuntimeError(f"Unexpected sprite row in {sheet_path}: {line!r}")
            sprite_name = parts[0]
            # Preserve TexturePacker's Unity bottom-left Y. MakeAtlasBrush
            # performs the one required conversion when building Slate UVs.
            rect = [int(parts[index]) for index in range(1, 5)]
            key = (atlas_name, sprite_name)
            if key in sprites:
                raise RuntimeError(f"Duplicate hero icon atlas sprite {key}")
            sprites[key] = {
                "rect": rect,
                "atlas_size": atlas_size,
            }
            sprite_count += 1
        sources[atlas_name] = {
            "tpsheet_path": project_relative(sheet_path),
            "atlas_size": atlas_size,
            "sprite_count": sprite_count,
        }
    return sprites, sources


def resolve_hero_icon(icon_reference, atlas_sprites, expected_atlases):
    match = re.fullmatch(r"([^\[]+)\[([^\]]+)\]", icon_reference)
    if not match:
        return {
            "icon_atlas": "",
            "icon_sprite": "",
            "icon_rect": [],
            "icon_atlas_size": [],
        }
    atlas_name, sprite_name = match.groups()
    resolved = (
        atlas_sprites.get((atlas_name, sprite_name))
        if atlas_name in expected_atlases
        else None
    )
    return {
        "icon_atlas": atlas_name if resolved else "",
        "icon_sprite": sprite_name,
        "icon_rect": list(resolved["rect"]) if resolved else [],
        "icon_atlas_size": list(resolved["atlas_size"]) if resolved else [],
    }


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


def make_building_type(
    row,
    menus_by_id,
    levels_by_type,
    produce_by_type,
    limits_by_type,
    training_capacity_by_type,
    hospital_capacity_by_level,
    language_by_id,
):
    type_id = int_field(row, "type")
    menu_buttons = [menus_by_id[menu_id] for menu_id in int_list_field(row, "menuButtons") if menu_id in menus_by_id]
    levels = sorted((make_level(item) for item in levels_by_type.get(type_id, [])), key=lambda item: item["level"])
    production = sorted((make_production(item) for item in produce_by_type.get(type_id, [])), key=lambda item: item["level"])
    count_limits = sorted((make_count_limit(item) for item in limits_by_type.get(type_id, [])), key=lambda item: item["town_level"])
    level_numbers = [item["level"] for item in levels]
    total_power = sum(item["power"] for item in levels)

    name_locale_id = int_field(row, "l_nameId")
    desc_locale_id = int_field(row, "l_descId")
    short_desc_locale_id = int_field(row, "l_descId2")

    return {
        "type_id": type_id,
        "name_locale_id": name_locale_id,
        "display_name_zh_cn": localized_text(language_by_id, name_locale_id, "cn"),
        "display_name_en": localized_text(language_by_id, name_locale_id, "en"),
        "group": int_field(row, "group"),
        "is_move": int_field(row, "isMove"),
        "tip_list": int_field(row, "tipList"),
        "desc_locale_id": desc_locale_id,
        "display_description_zh_cn": localized_text(language_by_id, desc_locale_id, "cn"),
        "display_description_en": localized_text(language_by_id, desc_locale_id, "en"),
        "short_desc_locale_id": short_desc_locale_id,
        "display_short_description_zh_cn": localized_text(language_by_id, short_desc_locale_id, "cn"),
        "display_short_description_en": localized_text(language_by_id, short_desc_locale_id, "en"),
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
        "training_capacity": training_capacity_by_type.get(type_id, []),
        "hospital_capacity": (
            hospital_capacity_by_level if type_id == 14 else []
        ),
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


def make_hero(row, language_by_id, atlas_sprites):
    name_locale_id = int_field(row, "l_nameID")
    title_locale_id = int_field(row, "l_appellationID")
    description_locale_id = int_field(row, "l_desID")
    skill_ids = int_list_field(row, "skill")
    talent_ids = int_list_field(row, "talent")
    base_power = int_field(row, "score")
    icon_reference = str_field(row, "heroIcon")
    return {
        "id": int_field(row, "ID"),
        "name_locale_id": name_locale_id,
        "display_name_zh_cn": localized_text(language_by_id, name_locale_id, "cn"),
        "display_name_en": localized_text(language_by_id, name_locale_id, "en"),
        "civilization": int_field(row, "civilization"),
        "appellation_locale_id": title_locale_id,
        "display_title_zh_cn": localized_text(language_by_id, title_locale_id, "cn"),
        "display_title_en": localized_text(language_by_id, title_locale_id, "en"),
        "init_star": int_field(row, "initStar"),
        "max_star": int_field(row, "star"),
        "rare": int_field(row, "rare"),
        "desc_locale_id": description_locale_id,
        "display_description_zh_cn": localized_text(language_by_id, description_locale_id, "cn"),
        "display_description_en": localized_text(language_by_id, description_locale_id, "en"),
        # Unity HeroProxy exposes Hero.score as baseScore. Keep the raw source
        # name as well as the presentation-oriented alias for existing readers.
        "score": base_power,
        "base_power": base_power,
        "get_item": int_field(row, "getItem"),
        "get_item_num": int_field(row, "getItemNum"),
        "exchange": int_field(row, "exchange"),
        "language_locale_id": int_field(row, "l_heroLanguage"),
        "get_limit": int_field(row, "getLimit"),
        "skill_ids": skill_ids,
        "talent_ids": talent_ids,
        "list_display": int_field(row, "listDisplay"),
        "icon": icon_reference,
        **resolve_hero_icon(icon_reference, atlas_sprites, ("hero_head",)),
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


def make_hero_skill(row, language_by_id, atlas_sprites):
    name_locale_id = int_field(row, "l_nameID")
    type_locale_id = int_field(row, "l_typeID")
    open_locale_id = int_field(row, "l_openID")
    message_locale_id = int_field(row, "l_mesID")
    icon_reference = str_field(row, "icon")

    def make_level_effect(data_field, locale_field):
        locale_id = int_field(row, locale_field)
        return {
            "data": str_field(row, data_field),
            "locale_id": locale_id,
            "display_text_zh_cn": localized_text(language_by_id, locale_id, "cn"),
            "display_text_en": localized_text(language_by_id, locale_id, "en"),
        }

    return {
        "id": int_field(row, "ID"),
        "name_locale_id": name_locale_id,
        "display_name_zh_cn": localized_text(language_by_id, name_locale_id, "cn"),
        "display_name_en": localized_text(language_by_id, name_locale_id, "en"),
        "type": int_field(row, "type"),
        "type_locale_id": type_locale_id,
        "display_type_zh_cn": localized_text(language_by_id, type_locale_id, "cn"),
        "display_type_en": localized_text(language_by_id, type_locale_id, "en"),
        "battle_skill_id": int_field(row, "skillBattleID"),
        "anger": int_field(row, "anger"),
        "open": int_field(row, "open"),
        "open_locale_id": open_locale_id,
        "display_open_zh_cn": localized_text(language_by_id, open_locale_id, "cn"),
        "display_open_en": localized_text(language_by_id, open_locale_id, "en"),
        "awaken_enhance": int_field(row, "awakenEnhance"),
        "icon": icon_reference,
        **resolve_hero_icon(
            icon_reference,
            atlas_sprites,
            ("hero_skill", "hero_skill2"),
        ),
        "level_up_cost_item": int_field(row, "levelUpCostItem"),
        "message_locale_id": message_locale_id,
        "display_message_zh_cn": localized_text(language_by_id, message_locale_id, "cn"),
        "display_message_en": localized_text(language_by_id, message_locale_id, "en"),
        "level_effects": [
            make_level_effect("lvEffectDate1", "l_lvEffect1"),
            make_level_effect("lvEffectDate2", "l_lvEffect2"),
            make_level_effect("lvEffectDate3", "l_lvEffect3"),
            make_level_effect("lvEffectDate4", "l_lvEffect4"),
        ],
        "army_effect": str_field(row, "armyEffect"),
        "sound": str_field(row, "sound"),
    }


def make_hero_talent(row, language_by_id):
    talent_locale_id = int_field(row, "l_talentID")
    tips_locale_id = int_field(row, "l_tipsID")
    return {
        "id": int_field(row, "ID"),
        "talent_locale_id": talent_locale_id,
        "display_name_zh_cn": localized_text(language_by_id, talent_locale_id, "cn"),
        "display_name_en": localized_text(language_by_id, talent_locale_id, "en"),
        "icon": str_field(row, "icon1"),
        "gain_tree": int_field(row, "gainTree"),
        "mastery_group_id": int_field(row, "masteryGroupID"),
        "tips_locale_id": tips_locale_id,
        "display_tips_zh_cn": localized_text(language_by_id, tips_locale_id, "cn"),
        "display_tips_en": localized_text(language_by_id, tips_locale_id, "en"),
        "type": int_field(row, "type"),
    }


def make_task(row, compiled_task_rewards, language_by_id):
    task_id = int_field(row, "ID")
    if task_id not in compiled_task_rewards:
        raise RuntimeError(f"Missing compiled task reward for task ID {task_id}")
    name_locale_id = int_field(row, "l_nameId")
    desc_locale_id = int_field(row, "l_desc")
    source_name = str_field(row, "name")
    source_desc = str_field(row, "desc")
    return {
        "id": task_id,
        "name_locale_id": name_locale_id,
        "name": source_name or localized_text(language_by_id, name_locale_id, "cn"),
        "desc_locale_id": desc_locale_id,
        "desc": source_desc or localized_text(language_by_id, desc_locale_id, "cn"),
        "type": int_field(row, "type"),
        "param1": int_field(row, "param1"),
        "param2": int_field(row, "param2"),
        "require": int_field(row, "require"),
        "reward": compiled_task_rewards[task_id],
        "next_id": int_field(row, "nextId"),
        # TaskSide columns driving TaskProxy.GetTaskSideList selection:
        # SortId = group1 * 10000 + order, showLevelReq, showBuildingReq.
        # They stay 0 for task tables without these columns.
        "group1": int_field(row, "group1"),
        "order": int_field(row, "order"),
        "show_level_req": int_field(row, "showLevelReq"),
        "show_building_req": int_field(row, "showBuildingReq"),
        "icon": str_field(row, "iconImg"),
    }


def make_vip(row, price_by_id):
    price_id = int_field(row, "price")
    price_usd = price_by_id.get(price_id, 0.0)
    return {
        "id": int_field(row, "ID"),
        "level": int_field(row, "level"),
        "point": int_field(row, "point"),
        "free_box": int_field(row, "freeBox"),
        "special_box": int_field(row, "specialBox"),
        "price": price_id,
        "price_usd": price_usd,
        "price_display": f"${price_usd:.2f}" if price_usd > 0 else "",
        "icon": str_field(row, "icon"),
    }


def make_vip_attribute(row, language_by_id):
    locale_id = int_field(row, "l_attID")
    localized_template = language_by_id.get(locale_id, "")
    add_value = float_field(row, "add")
    is_integer = int_field(row, "integer") == 1
    display_value = add_value if is_integer else add_value / 10.0
    if float(display_value).is_integer():
        display_number = str(int(display_value))
    else:
        display_number = f"{display_value:g}"
    display_parameter = f"{'+' if add_value > 0 else ''}{display_number}{'' if is_integer else '%'}"
    return {
        "id": int_field(row, "ID"),
        "level_group": int_field(row, "levelGroup"),
        "new_sign": int_field(row, "newSign"),
        "attribute": str_field(row, "att"),
        "description": str_field(row, "dec"),
        "locale_id": locale_id,
        "localized_template": localized_template,
        "display_text_zh_cn": localized_template.replace("{0}", display_parameter),
        "add": add_value,
        "integer": 1 if is_integer else 0,
    }


def make_vip_day_point(row):
    return {
        "id": int_field(row, "ID"),
        "day": int_field(row, "day"),
        "point": int_field(row, "point"),
    }


def make_vip_store(row):
    return {
        "id": int_field(row, "ID"),
        "vip_level": int_field(row, "vipLevel"),
        "item_id": int_field(row, "itemID"),
        "name": str_field(row, "name"),
        "currency_type": int_field(row, "type"),
        "price": int_field(row, "price"),
        "number": int_field(row, "number"),
        "discount": int_field(row, "discount"),
    }


def make_civilization(row):
    return {
        "id": int_field(row, "ID"),
        "display_name_zh_cn": str_field(row, "civilization"),
        "bonus_name_zh_cn": str_field(row, "civilizationAddName"),
        "bonus_description_zh_cn": str_field(row, "civilizationAddDes"),
        "add_data": str_field(row, "addData"),
        "bonus_values": int_list_field(row, "addData"),
        "initial_hero_id": int_field(row, "initialHero"),
        "hero_resource_key": str_field(row, "heroRes"),
        "hero_about_offset": float_field(row, "aboutOffset"),
        "if_lock": int_field(row, "ifLock"),
        "flag_key": str_field(row, "civilizationFlag"),
        "mark_key": str_field(row, "civilizationMark"),
        "city_key": str_field(row, "cityShow"),
        "background_key": str_field(row, "background"),
        "soldiers_key": str_field(row, "featureArmsShow"),
        "rank": int_field(row, "rank"),
    }


def main():
    tables = {alias: load_table(table_name) for alias, table_name in SOURCE_TABLES.items()}
    rows = {alias: table["rows"] for alias, table in tables.items()}
    hero_icon_atlas_sprites, hero_icon_atlas_sources = read_hero_icon_atlas_sprites()
    compiled_task_rewards = {
        alias: load_compiled_task_rewards(csv_path)
        for alias, csv_path in COMPILED_TASK_CSVS.items()
    }
    language_by_id = {
        int_field(row, "id"): row
        for row in rows["language"]
        if int_field(row, "id") > 0
    }
    language_zh_cn_by_id = {
        locale_id: str_field(row, "cn")
        for locale_id, row in language_by_id.items()
    }
    price_by_id = {
        int_field(row, "ID"): float_field(row, "price")
        for row in rows["price"]
        if int_field(row, "ID") > 0
    }

    menus_by_id = {menu_id: make_menu_button(row) for menu_id, row in index_by(rows["building_menu"], "ID").items()}
    levels_by_type = group_by(rows["building_level"], "type")
    produce_by_type = group_by(rows["building_produce"], "type")
    limits_by_type = group_by(rows["building_count_limit"], "buildType")
    training_capacity_by_type = {}
    for type_id, (table_alias, capacity_field) in TRAINING_CAPACITY_SOURCES.items():
        training_capacity_by_type[type_id] = [
            {
                "level": int_field(row, "level"),
                "capacity": int_field(row, capacity_field),
            }
            for row in sorted(rows[table_alias], key=lambda item: int_field(item, "level"))
            if int_field(row, "level") > 0
        ]
    hospital_capacity_by_level = [
        {
            "level": int_field(row, "level"),
            "capacity": int_field(row, "armyCnt"),
            "all_hp_buff": int_field(row, "allHpBuff"),
        }
        for row in sorted(
            rows["building_hospital"],
            key=lambda item: int_field(item, "level"),
        )
        if int_field(row, "level") > 0
    ]

    building_types = [
        make_building_type(
            row,
            menus_by_id,
            levels_by_type,
            produce_by_type,
            limits_by_type,
            training_capacity_by_type,
            hospital_capacity_by_level,
            language_by_id,
        )
        for row in sorted(rows["building_type"], key=lambda item: int_field(item, "type"))
    ]
    troops = [make_troop(row) for row in sorted(rows["troops"], key=lambda item: int_field(item, "ID"))]
    heroes = [
        make_hero(row, language_by_id, hero_icon_atlas_sprites)
        for row in sorted(rows["heroes"], key=lambda item: int_field(item, "ID"))
    ]
    hero_skills = [
        make_hero_skill(row, language_by_id, hero_icon_atlas_sprites)
        for row in sorted(rows["hero_skills"], key=lambda item: int_field(item, "ID"))
    ]
    hero_talents = [
        make_hero_talent(row, language_by_id)
        for row in sorted(rows["hero_talents"], key=lambda item: int_field(item, "ID"))
    ]
    task_sections = {
        alias: [
            make_task(row, compiled_task_rewards[alias], language_by_id)
            for row in sorted(rows[alias], key=lambda item: int_field(item, "ID"))
        ]
        for alias in COMPILED_TASK_CSVS
    }
    main_tasks = task_sections["main_tasks"]
    side_tasks = task_sections["side_tasks"]
    daily_tasks = task_sections["daily_tasks"]
    chapter_tasks = task_sections["chapter_tasks"]
    vip_levels = [make_vip(row, price_by_id) for row in sorted(rows["vip"], key=lambda item: int_field(item, "level"))]
    vip_attributes = [
        make_vip_attribute(row, language_zh_cn_by_id)
        for row in sorted(rows["vip_att"], key=lambda item: int_field(item, "ID"))
    ]
    vip_day_points = [make_vip_day_point(row) for row in sorted(rows["vip_day_point"], key=lambda item: int_field(item, "day"))]
    vip_store = [
        make_vip_store(row)
        for row in sorted(rows["vip_store"], key=lambda item: int_field(item, "ID"))
        if int_field(row, "ID") > 0
    ]
    civilizations = [
        make_civilization(row)
        for row in sorted(
            rows["civilizations"],
            key=lambda item: (int_field(item, "rank"), int_field(item, "ID")),
        )
        if int_field(row, "ifShow") == 1
    ]

    troop_family_counts = defaultdict(int)
    for troop in troops:
        troop_family_counts[str(troop["arms_type"])] += 1

    hero_rare_counts = defaultdict(int)
    for hero in heroes:
        hero_rare_counts[str(hero["rare"])] += 1
    referenced_hero_skill_ids = {
        skill_id
        for hero in heroes
        for skill_id in hero["skill_ids"]
    }
    referenced_hero_talent_ids = {
        talent_id
        for hero in heroes
        for talent_id in hero["talent_ids"]
    }
    hero_head_icon_reference_count = sum(
        1 for hero in heroes if hero["icon"].startswith("hero_head[")
    )
    hero_head_icon_resolved_count = sum(
        1 for hero in heroes if hero["icon_atlas"] == "hero_head"
    )
    hero_skill_icon_reference_count = sum(
        1 for skill in hero_skills if skill["icon"].startswith("hero_skill[")
    )
    hero_skill_icon_resolved_count = sum(
        1 for skill in hero_skills if skill["icon_atlas"] == "hero_skill"
    )
    hero_skill2_icon_reference_count = sum(
        1 for skill in hero_skills if skill["icon"].startswith("hero_skill2[")
    )
    hero_skill2_icon_resolved_count = sum(
        1 for skill in hero_skills if skill["icon_atlas"] == "hero_skill2"
    )

    summary = {
        "building_type_count": len(building_types),
        "building_level_row_count": len(rows["building_level"]),
        "building_menu_count": len(rows["building_menu"]),
        "building_count_limit_row_count": len(rows["building_count_limit"]),
        "building_produce_row_count": len(rows["building_produce"]),
        "training_capacity_row_count": sum(
            len(items) for items in training_capacity_by_type.values()
        ),
        "hospital_capacity_row_count": len(hospital_capacity_by_level),
        "troop_count": len(troops),
        "troop_family_counts": dict(sorted(troop_family_counts.items(), key=lambda item: int(item[0]))),
        "hero_count": len(heroes),
        "hero_rare_counts": dict(sorted(hero_rare_counts.items(), key=lambda item: int(item[0]))),
        "hero_skill_count": len(hero_skills),
        "hero_talent_count": len(hero_talents),
        "hero_referenced_skill_count": len(referenced_hero_skill_ids),
        "hero_referenced_talent_count": len(referenced_hero_talent_ids),
        "hero_head_icon_reference_count": hero_head_icon_reference_count,
        "hero_head_icon_resolved_count": hero_head_icon_resolved_count,
        "hero_head_icon_missing_count": (
            hero_head_icon_reference_count - hero_head_icon_resolved_count
        ),
        "hero_skill_icon_reference_count": hero_skill_icon_reference_count,
        "hero_skill_icon_resolved_count": hero_skill_icon_resolved_count,
        "hero_skill_icon_missing_count": (
            hero_skill_icon_reference_count - hero_skill_icon_resolved_count
        ),
        "hero_skill2_icon_reference_count": hero_skill2_icon_reference_count,
        "hero_skill2_icon_resolved_count": hero_skill2_icon_resolved_count,
        "hero_skill2_icon_missing_count": (
            hero_skill2_icon_reference_count - hero_skill2_icon_resolved_count
        ),
        "main_task_count": len(main_tasks),
        "side_task_count": len(side_tasks),
        "daily_task_count": len(daily_tasks),
        "chapter_task_count": len(chapter_tasks),
        "vip_level_count": len(vip_levels),
        "vip_attribute_count": len(vip_attributes),
        "vip_day_point_count": len(vip_day_points),
        "vip_store_count": len(vip_store),
        "civilization_count": len(civilizations),
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
        "hero_icon_atlas_sources": hero_icon_atlas_sources,
        "summary": summary,
        "building_types": building_types,
        "troops": troops,
        "heroes": heroes,
        "hero_skills": hero_skills,
        "hero_talents": hero_talents,
        "main_tasks": main_tasks,
        "side_tasks": side_tasks,
        "daily_tasks": daily_tasks,
        "chapter_tasks": chapter_tasks,
        "vip_levels": vip_levels,
        "vip_attributes": vip_attributes,
        "vip_day_points": vip_day_points,
        "vip_store": vip_store,
        "civilizations": civilizations,
    }

    MIGRATION_ROOT.mkdir(parents=True, exist_ok=True)
    OUTPUT_JSON.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# Unity Gameplay Catalog",
        "",
        f"- Building types: {summary['building_type_count']}",
        f"- Building level rows: {summary['building_level_row_count']}",
        f"- Building menu buttons: {summary['building_menu_count']}",
        f"- Training capacity rows: {summary['training_capacity_row_count']}",
        f"- Hospital capacity rows: {summary['hospital_capacity_row_count']}",
        f"- Troops: {summary['troop_count']}",
        f"- Heroes: {summary['hero_count']}",
        f"- Hero head icons resolved: {summary['hero_head_icon_resolved_count']}/{summary['hero_head_icon_reference_count']}",
        f"- Hero skills: {summary['hero_skill_count']}",
        f"- Hero skill icons resolved: {summary['hero_skill_icon_resolved_count']}/{summary['hero_skill_icon_reference_count']}",
        f"- Hero skill2 icons resolved: {summary['hero_skill2_icon_resolved_count']}/{summary['hero_skill2_icon_reference_count']}",
        f"- Hero talents: {summary['hero_talent_count']}",
        f"- Main tasks: {summary['main_task_count']}",
        f"- Side tasks: {summary['side_task_count']}",
        f"- Daily tasks: {summary['daily_task_count']}",
        f"- Chapter tasks: {summary['chapter_task_count']}",
        f"- VIP levels: {summary['vip_level_count']}",
        f"- VIP attributes: {summary['vip_attribute_count']}",
        f"- VIP day point rows: {summary['vip_day_point_count']}",
        f"- VIP store rows: {summary['vip_store_count']}",
        f"- Civilizations: {summary['civilization_count']}",
        "",
        "## Building Types",
        "",
        "| Type | Group | Size | Levels | Menus | Produce Levels | Train Capacity Levels |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for item in building_types:
        lines.append(
            f"| {item['type_id']} | {item['group']} | {item['size']['width']}x{item['size']['length']} | "
            f"{item['level_summary']['min_level']}-{item['level_summary']['max_level']} ({item['level_summary']['count']}) | "
            f"{len(item['menu_buttons'])} | {len(item['production'])} | {len(item['training_capacity'])} |"
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
        "## Heroes",
        "",
        "| ID | Name | Title | Rare | Stars | Base Power | Skills | Talents |",
        "| --- | --- | --- | --- | --- | --- | --- | --- |",
    ])
    for hero in heroes:
        lines.append(
            f"| {hero['id']} | {hero['display_name_zh_cn']} | {hero['display_title_zh_cn']} | "
            f"{hero['rare']} | {hero['init_star']}/{hero['max_star']} | {hero['base_power']} | "
            f"{len(hero['skill_ids'])} | {len(hero['talent_ids'])} |"
        )

    lines.extend([
        "",
        "## First Main Tasks",
        "",
        "| ID | Name | Type | Param1 | Param2 | Next |",
        "| --- | --- | --- | --- | --- | --- |",
    ])
    for task in main_tasks[:20]:
        lines.append(f"| {task['id']} | {task['name']} | {task['type']} | {task['param1']} | {task['param2']} | {task['next_id']} |")

    for title, tasks in (
        ("First Side Tasks", side_tasks),
        ("First Daily Tasks", daily_tasks),
        ("First Chapter Tasks", chapter_tasks),
    ):
        lines.extend([
            "",
            f"## {title}",
            "",
            "| ID | Name | Type | Param1 | Param2 | Reward |",
            "| --- | --- | --- | --- | --- | --- |",
        ])
        for task in tasks[:20]:
            lines.append(
                f"| {task['id']} | {task['name']} | {task['type']} | "
                f"{task['param1']} | {task['param2']} | {task['reward']} |"
            )
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(json.dumps({
        "output": str(OUTPUT_JSON),
        "summary": summary,
    }, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
