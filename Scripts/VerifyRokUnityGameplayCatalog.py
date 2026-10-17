import csv
import json
import re
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
CONFIG_ROOT = MIGRATION_ROOT / "ConfigTables"
CATALOG_PATH = MIGRATION_ROOT / "unity_gameplay_catalog.json"
OUTPUT_PATH = MIGRATION_ROOT / "unity_gameplay_catalog_verify.json"
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
COMPILED_TASK_CSV_ROOT = (
    PROJECT_ROOT / "ref" / "config-tables" / "编译客户端配置" / "CSV"
)
TASK_SECTION_SPECS = {
    "main_tasks": {
        "table_name": "TaskMain",
        "summary_key": "main_task_count",
        "first_row": {
            "id": 200900,
            "type": 8,
            "param1": 1,
            "param2": 2,
            "require": 1,
            "reward": 200900,
            "next_id": 200901,
            "icon": "task_icon[task_icon_1003]",
        },
    },
    "side_tasks": {
        "table_name": "TaskSide",
        "summary_key": "side_task_count",
        "verify_side_selection_fields": True,
        "first_row": {
            "id": 307200,
            "type": 6,
            "param1": 203,
            "param2": 5,
            "require": 1,
            "reward": 307200,
            "next_id": 0,
            "group1": 7,
            "order": 200,
            "show_level_req": 0,
            "show_building_req": 0,
            "icon": "task_icon[task_icon_1007]",
        },
    },
    "daily_tasks": {
        "table_name": "TaskDaily",
        "summary_key": "daily_task_count",
        "first_row": {
            "id": 401001,
            "type": 10,
            "param1": 1,
            "param2": 0,
            "require": 50,
            "reward": 90001,
            "next_id": 0,
            "icon": "task_icon[task_icon_1009]",
        },
    },
    "chapter_tasks": {
        "table_name": "TaskChapter",
        "summary_key": "chapter_task_count",
        "first_row": {
            "id": 104001,
            "type": 8,
            "param1": 1,
            "param2": 5,
            "require": 1,
            "reward": 114001,
            "next_id": 0,
            "icon": "task_icon[task_icon_1003]",
        },
    },
}

EXPECTED_COUNTS = {
    "building_type_count": 30,
    "building_level_row_count": 487,
    "building_menu_count": 43,
    "building_count_limit_row_count": 700,
    "building_produce_row_count": 100,
    "training_capacity_row_count": 100,
    "hospital_capacity_row_count": 25,
    "troop_count": 54,
    "hero_count": 46,
    "hero_skill_count": 261,
    "hero_talent_count": 117,
    "main_task_count": 205,
    "side_task_count": 2343,
    "daily_task_count": 118,
    "chapter_task_count": 39,
    "vip_level_count": 18,
    "vip_attribute_count": 354,
    "vip_day_point_count": 8,
    "vip_store_count": 33,
    "civilization_count": 7,
}

CORE_BUILDING_TYPES = {1, 6, 7, 8, 9, 14, 15, 29}


def load_source_count(table_name):
    path = CONFIG_ROOT / f"{table_name}.json"
    if not path.exists():
        return -1
    data = json.loads(path.read_text(encoding="utf-8"))
    return len(data.get("rows", []))


def load_source_rows(table_name):
    path = CONFIG_ROOT / f"{table_name}.json"
    if not path.exists():
        return []
    return json.loads(path.read_text(encoding="utf-8")).get("rows", [])


def load_compiled_task_rows(table_name):
    path = COMPILED_TASK_CSV_ROOT / f"{table_name}.csv"
    rows_by_id = {}
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        for row in csv.DictReader(handle):
            task_id = str(row.get("ID", "")).strip()
            if task_id.lstrip("-").isdigit():
                rows_by_id[int(task_id)] = row
    return rows_by_id


def verify_task_section(catalog, section_name, spec, failures):
    tasks = catalog.get(section_name, [])
    table_name = spec["table_name"]
    summary_key = spec["summary_key"]
    summary_count = catalog.get("summary", {}).get(summary_key)
    source_rows = load_source_rows(table_name)
    compiled_rows = load_compiled_task_rows(table_name)
    task_by_id = {task.get("id"): task for task in tasks}

    if len(tasks) != summary_count:
        failures.append(
            f"{section_name} array count mismatch: rows={len(tasks)} summary={summary_count}"
        )
    if len(source_rows) != summary_count:
        failures.append(
            f"{table_name} source count mismatch: source={len(source_rows)} summary={summary_count}"
        )
    if len(compiled_rows) != summary_count:
        failures.append(
            f"{table_name} compiled count mismatch: compiled={len(compiled_rows)} summary={summary_count}"
        )
    if len(task_by_id) != len(tasks):
        failures.append(f"Duplicate {section_name} IDs")

    if source_rows:
        first_source_id = int_field(source_rows[0], "ID")
        first_task = task_by_id.get(first_source_id)
        if not first_task:
            failures.append(
                f"{section_name} is missing first source row ID {first_source_id}"
            )
        else:
            for field_name, expected_value in spec["first_row"].items():
                if first_task.get(field_name) != expected_value:
                    failures.append(
                        f"{section_name} first source row {first_source_id} {field_name} "
                        f"expected {expected_value!r}, got {first_task.get(field_name)!r}"
                    )

    for task_id, compiled_row in compiled_rows.items():
        task = task_by_id.get(task_id)
        if not task:
            failures.append(f"{section_name} is missing compiled task ID {task_id}")
            continue
        compiled_reward = int_field(compiled_row, "reward")
        if task.get("reward") != compiled_reward:
            failures.append(
                f"{section_name} task {task_id} reward expected compiled value "
                f"{compiled_reward}, got {task.get('reward')}"
            )
        if spec.get("verify_side_selection_fields"):
            for task_field, csv_field in (
                ("group1", "group1"),
                ("order", "order"),
                ("show_level_req", "showLevelReq"),
                ("show_building_req", "showBuildingReq"),
            ):
                compiled_value = int_field(compiled_row, csv_field)
                if task.get(task_field) != compiled_value:
                    failures.append(
                        f"{section_name} task {task_id} {task_field} expected "
                        f"compiled value {compiled_value}, got {task.get(task_field)}"
                    )

    for task in tasks:
        if task.get("name_locale_id", 0) > 0 and not task.get("name"):
            failures.append(
                f"{section_name} task {task.get('id')} has no localized name"
            )
        if task.get("desc_locale_id", 0) > 0 and not task.get("desc"):
            failures.append(
                f"{section_name} task {task.get('id')} has no localized description"
            )

    return set(task_by_id)


def int_field(row, name, default=0):
    value = row.get(name, default)
    if value in ("", None):
        return default
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return default


def str_field(row, name, default=""):
    value = row.get(name, default)
    return default if value is None else str(value)


def int_list_field(row, name):
    value = row.get(name, "")
    if value in ("", None):
        return []
    if isinstance(value, list):
        return [int_field({"value": item}, "value") for item in value if str(item).strip()]
    parts = str(value).replace(",", "|").split("|")
    return [int(part) for part in parts if part.strip() and part.strip().lstrip("-").isdigit()]


def verify_civilizations(catalog, failures):
    summary_count = catalog.get("summary", {}).get("civilization_count")
    source_alias = catalog.get("source_tables", {}).get("civilizations", {})
    expected_source_alias = {
        "table_name": "Civilization",
        "json_path": "Saved/RokUnityMigration/ConfigTables/Civilization.json",
        "row_count": 8,
    }
    for field_name, expected_value in expected_source_alias.items():
        actual_value = source_alias.get(field_name)
        if actual_value != expected_value:
            failures.append(
                f"Civilization source alias {field_name} expected "
                f"{expected_value!r}, got {actual_value!r}"
            )
    source_rows = [
        row
        for row in load_source_rows("Civilization")
        if int_field(row, "ifShow") == 1
    ]
    civilizations = catalog.get("civilizations", [])
    source_by_id = {int_field(row, "ID"): row for row in source_rows}
    catalog_by_id = {item.get("id"): item for item in civilizations}

    if len(source_rows) != 7:
        failures.append(f"Expected 7 visible Civilization source rows, got {len(source_rows)}")
    if len(civilizations) != 7:
        failures.append(f"Expected 7 catalog civilizations, got {len(civilizations)}")
    if len(civilizations) != summary_count:
        failures.append(
            "Civilization array count mismatch: "
            f"rows={len(civilizations)} summary={summary_count}"
        )
    if len(catalog_by_id) != len(civilizations):
        failures.append("Duplicate civilization IDs")

    expected_ids = set(source_by_id)
    actual_ids = set(catalog_by_id)
    if actual_ids != expected_ids:
        failures.append(
            "Civilization IDs do not match visible source rows: "
            f"expected={sorted(expected_ids)} actual={sorted(actual_ids)}"
        )

    for civilization_id, source_row in source_by_id.items():
        item = catalog_by_id.get(civilization_id)
        if item is None:
            continue
        expected_fields = {
            "display_name_zh_cn": str_field(source_row, "civilization"),
            "bonus_name_zh_cn": str_field(source_row, "civilizationAddName"),
            "bonus_description_zh_cn": str_field(source_row, "civilizationAddDes"),
            "add_data": str_field(source_row, "addData"),
            "bonus_values": int_list_field(source_row, "addData"),
            "initial_hero_id": int_field(source_row, "initialHero"),
            "if_lock": int_field(source_row, "ifLock"),
            "flag_key": str_field(source_row, "civilizationFlag"),
            "mark_key": str_field(source_row, "civilizationMark"),
            "city_key": str_field(source_row, "cityShow"),
            "background_key": str_field(source_row, "background"),
            "soldiers_key": str_field(source_row, "featureArmsShow"),
            "rank": int_field(source_row, "rank"),
        }
        for field_name, expected_value in expected_fields.items():
            actual_value = item.get(field_name)
            if actual_value != expected_value:
                failures.append(
                    f"Civilization {civilization_id} {field_name} expected "
                    f"{expected_value!r}, got {actual_value!r}"
                )

    expected_order = [
        int_field(row, "ID")
        for row in sorted(
            source_rows,
            key=lambda row: (int_field(row, "rank"), int_field(row, "ID")),
        )
    ]
    actual_order = [item.get("id") for item in civilizations]
    if actual_order != expected_order:
        failures.append(
            f"Civilization rank order mismatch: expected={expected_order} actual={actual_order}"
        )

    return {
        "source_alias": source_alias,
        "visible_source_count": len(source_rows),
        "catalog_count": len(civilizations),
        "ids": actual_order,
    }


def project_relative(path):
    return path.relative_to(PROJECT_ROOT).as_posix()


def read_hero_icon_atlas_sprites():
    sprites = {}
    sources = {}
    for atlas_name, path in HERO_ICON_ATLAS_SHEETS.items():
        lines = path.read_text(encoding="utf-8-sig").splitlines()
        size_line = next((line for line in lines if line.startswith(":size=")), "")
        size_match = re.fullmatch(r":size=(\d+)x(\d+)", size_line)
        if not size_match:
            raise RuntimeError(f"Unable to parse atlas size from {path}")
        atlas_size = [int(size_match.group(1)), int(size_match.group(2))]
        sprite_count = 0
        for line in lines:
            if not line or line.startswith(("#", ":")):
                continue
            parts = [part.strip() for part in line.split(";")]
            if len(parts) < 5:
                raise RuntimeError(f"Unexpected sprite row in {path}: {line!r}")
            sprite_name = parts[0]
            # Catalog rects intentionally preserve TexturePacker's Unity
            # bottom-left coordinates. MakeAtlasBrush performs the single
            # conversion to Slate top-left UVs at render time.
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
            "tpsheet_path": project_relative(path),
            "atlas_size": atlas_size,
            "sprite_count": sprite_count,
        }
    return sprites, sources


def expected_icon_fields(icon_reference, atlas_sprites, expected_atlases):
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


def verify_icon_fields(
    item,
    icon_reference,
    atlas_sprites,
    expected_atlases,
    label,
    failures,
):
    expected = expected_icon_fields(icon_reference, atlas_sprites, expected_atlases)
    for field_name, expected_value in expected.items():
        if item.get(field_name) != expected_value:
            failures.append(
                f"{label} {field_name} expected {expected_value}, got {item.get(field_name)}"
            )

    rect = item.get("icon_rect", [])
    atlas_size = item.get("icon_atlas_size", [])
    if item.get("icon_atlas"):
        if len(rect) != 4 or len(atlas_size) != 2:
            failures.append(f"{label} resolved icon metadata has invalid array lengths")
        else:
            x, y, width, height = rect
            atlas_width, atlas_height = atlas_size
            if (
                x < 0
                or y < 0
                or width <= 0
                or height <= 0
                or atlas_width <= 0
                or atlas_height <= 0
                or x + width > atlas_width
                or y + height > atlas_height
            ):
                failures.append(
                    f"{label} icon rect {rect} is outside atlas size {atlas_size}"
                )
    return expected


def verify_localized_fields(item, locale_field, zh_cn_field, en_field, language_by_id, label, failures):
    locale_id = int_field(item, locale_field)
    expected_language = language_by_id.get(locale_id, {})
    expected_zh_cn = str_field(expected_language, "cn")
    expected_en = str_field(expected_language, "en")
    actual_zh_cn = str_field(item, zh_cn_field)
    actual_en = str_field(item, en_field)

    if locale_id <= 0:
        if actual_zh_cn or actual_en:
            failures.append(f"{label} has display text without a positive locale ID")
        return
    if not expected_zh_cn or not expected_en:
        failures.append(f"{label} locale {locale_id} is missing cn/en source text")
        return
    if actual_zh_cn != expected_zh_cn:
        failures.append(f"{label} Chinese display text does not match locale {locale_id}")
    if actual_en != expected_en:
        failures.append(f"{label} English display text does not match locale {locale_id}")


def verify_hero_catalog(catalog, failures):
    heroes = catalog.get("heroes", [])
    hero_skills = catalog.get("hero_skills", [])
    hero_talents = catalog.get("hero_talents", [])

    source_heroes = {
        int_field(row, "ID"): row
        for row in load_source_rows("Hero")
        if int_field(row, "ID") > 0
    }
    source_skills = {
        int_field(row, "ID"): row
        for row in load_source_rows("HeroSkill")
        if int_field(row, "ID") > 0
    }
    source_talents = {
        int_field(row, "ID"): row
        for row in load_source_rows("HeroTalent")
        if int_field(row, "ID") > 0
    }
    language_by_id = {
        int_field(row, "id"): row
        for row in load_source_rows("Language")
        if int_field(row, "id") > 0
    }
    atlas_sprites, atlas_sources = read_hero_icon_atlas_sprites()
    if catalog.get("hero_icon_atlas_sources") != atlas_sources:
        failures.append("Hero icon atlas source metadata does not match current tpsheets")

    hero_ids = [item.get("id") for item in heroes]
    skill_ids = [item.get("id") for item in hero_skills]
    talent_ids = [item.get("id") for item in hero_talents]
    if len(set(hero_ids)) != len(hero_ids):
        failures.append("Duplicate hero IDs")
    if len(set(skill_ids)) != len(skill_ids):
        failures.append("Duplicate hero skill IDs")
    if len(set(talent_ids)) != len(talent_ids):
        failures.append("Duplicate hero talent IDs")
    if sorted(hero_ids) != sorted(source_heroes):
        failures.append("Hero IDs do not exactly match the normalized Unity Hero source")
    if sorted(skill_ids) != sorted(source_skills):
        failures.append("Hero skill IDs do not exactly match the normalized Unity HeroSkill source")
    if sorted(talent_ids) != sorted(source_talents):
        failures.append("Hero talent IDs do not exactly match the normalized Unity HeroTalent source")

    catalog_skill_by_id = {item.get("id"): item for item in hero_skills}
    catalog_talent_by_id = {item.get("id"): item for item in hero_talents}
    referenced_skill_ids = set()
    referenced_talent_ids = set()
    localized_hero_count = 0
    hero_head_reference_count = 0
    hero_head_resolved_count = 0
    missing_hero_head_sprites = set()

    for hero in heroes:
        hero_id = hero.get("id")
        source = source_heroes.get(hero_id)
        if not source:
            continue

        expected_skill_ids = int_list_field(source, "skill")
        expected_talent_ids = int_list_field(source, "talent")
        actual_skill_ids = hero.get("skill_ids", [])
        actual_talent_ids = hero.get("talent_ids", [])
        if actual_skill_ids != expected_skill_ids:
            failures.append(f"Hero {hero_id} skill ID order does not match Unity Hero.skill")
        if actual_talent_ids != expected_talent_ids:
            failures.append(f"Hero {hero_id} talent ID order does not match Unity Hero.talent")
        if len(set(actual_skill_ids)) != len(actual_skill_ids):
            failures.append(f"Hero {hero_id} has duplicate skill IDs")
        if len(set(actual_talent_ids)) != len(actual_talent_ids):
            failures.append(f"Hero {hero_id} has duplicate talent IDs")

        referenced_skill_ids.update(actual_skill_ids)
        referenced_talent_ids.update(actual_talent_ids)
        missing_skills = sorted(set(actual_skill_ids) - set(catalog_skill_by_id))
        missing_talents = sorted(set(actual_talent_ids) - set(catalog_talent_by_id))
        if missing_skills:
            failures.append(f"Hero {hero_id} references missing skills: {missing_skills}")
        if missing_talents:
            failures.append(f"Hero {hero_id} references missing talents: {missing_talents}")

        scalar_fields = {
            "name_locale_id": int_field(source, "l_nameID"),
            "civilization": int_field(source, "civilization"),
            "appellation_locale_id": int_field(source, "l_appellationID"),
            "init_star": int_field(source, "initStar"),
            "max_star": int_field(source, "star"),
            "rare": int_field(source, "rare"),
            "desc_locale_id": int_field(source, "l_desID"),
            "score": int_field(source, "score"),
            "base_power": int_field(source, "score"),
            "list_display": int_field(source, "listDisplay"),
        }
        for field_name, expected_value in scalar_fields.items():
            if hero.get(field_name) != expected_value:
                failures.append(
                    f"Hero {hero_id} {field_name} expected source value {expected_value}, got {hero.get(field_name)}"
                )

        icon_reference = str_field(source, "heroIcon")
        icon_match = re.fullmatch(r"([^\[]+)\[([^\]]+)\]", icon_reference)
        expected_icon = verify_icon_fields(
            hero,
            icon_reference,
            atlas_sprites,
            ("hero_head",),
            f"Hero {hero_id}",
            failures,
        )
        if icon_match and icon_match.group(1) == "hero_head":
            hero_head_reference_count += 1
            if expected_icon["icon_atlas"] == "hero_head":
                hero_head_resolved_count += 1
            else:
                missing_hero_head_sprites.add(icon_match.group(2))

        verify_localized_fields(
            hero,
            "name_locale_id",
            "display_name_zh_cn",
            "display_name_en",
            language_by_id,
            f"Hero {hero_id} name",
            failures,
        )
        verify_localized_fields(
            hero,
            "appellation_locale_id",
            "display_title_zh_cn",
            "display_title_en",
            language_by_id,
            f"Hero {hero_id} title",
            failures,
        )
        verify_localized_fields(
            hero,
            "desc_locale_id",
            "display_description_zh_cn",
            "display_description_en",
            language_by_id,
            f"Hero {hero_id} description",
            failures,
        )
        if (
            hero.get("display_name_zh_cn")
            and hero.get("display_name_en")
            and hero.get("display_title_zh_cn")
            and hero.get("display_title_en")
        ):
            localized_hero_count += 1

    localized_skill_count = 0
    hero_skill_reference_count = 0
    hero_skill_resolved_count = 0
    missing_hero_skill_sprites = set()
    hero_skill2_reference_count = 0
    hero_skill2_resolved_count = 0
    missing_hero_skill2_sprites = set()
    for skill in hero_skills:
        skill_id = skill.get("id")
        source = source_skills.get(skill_id)
        if not source:
            continue
        locale_fields = (
            ("name_locale_id", "l_nameID", "display_name_zh_cn", "display_name_en", "name"),
            ("type_locale_id", "l_typeID", "display_type_zh_cn", "display_type_en", "type"),
            ("open_locale_id", "l_openID", "display_open_zh_cn", "display_open_en", "open"),
            ("message_locale_id", "l_mesID", "display_message_zh_cn", "display_message_en", "message"),
        )
        for catalog_locale, source_locale, zh_cn_field, en_field, label in locale_fields:
            expected_locale_id = int_field(source, source_locale)
            if skill.get(catalog_locale) != expected_locale_id:
                failures.append(
                    f"Hero skill {skill_id} {catalog_locale} expected {expected_locale_id}, got {skill.get(catalog_locale)}"
                )
            verify_localized_fields(
                skill,
                catalog_locale,
                zh_cn_field,
                en_field,
                language_by_id,
                f"Hero skill {skill_id} {label}",
                failures,
            )
        icon_reference = str_field(source, "icon")
        icon_match = re.fullmatch(r"([^\[]+)\[([^\]]+)\]", icon_reference)
        expected_icon = verify_icon_fields(
            skill,
            icon_reference,
            atlas_sprites,
            ("hero_skill", "hero_skill2"),
            f"Hero skill {skill_id}",
            failures,
        )
        if icon_match:
            icon_atlas = icon_match.group(1)
            icon_sprite = icon_match.group(2)
            if icon_atlas == "hero_skill":
                hero_skill_reference_count += 1
                if expected_icon["icon_atlas"] == "hero_skill":
                    hero_skill_resolved_count += 1
                else:
                    missing_hero_skill_sprites.add(icon_sprite)
            elif icon_atlas == "hero_skill2":
                hero_skill2_reference_count += 1
                if expected_icon["icon_atlas"] == "hero_skill2":
                    hero_skill2_resolved_count += 1
                else:
                    missing_hero_skill2_sprites.add(icon_sprite)
        if skill.get("display_name_zh_cn") and skill.get("display_name_en"):
            localized_skill_count += 1

    localized_talent_count = 0
    for talent in hero_talents:
        talent_id = talent.get("id")
        source = source_talents.get(talent_id)
        if not source:
            continue
        scalar_fields = {
            "talent_locale_id": int_field(source, "l_talentID"),
            "gain_tree": int_field(source, "gainTree"),
            "mastery_group_id": int_field(source, "masteryGroupID"),
            "tips_locale_id": int_field(source, "l_tipsID"),
            "type": int_field(source, "type"),
        }
        for field_name, expected_value in scalar_fields.items():
            if talent.get(field_name) != expected_value:
                failures.append(
                    f"Hero talent {talent_id} {field_name} expected {expected_value}, got {talent.get(field_name)}"
                )
        verify_localized_fields(
            talent,
            "talent_locale_id",
            "display_name_zh_cn",
            "display_name_en",
            language_by_id,
            f"Hero talent {talent_id} name",
            failures,
        )
        verify_localized_fields(
            talent,
            "tips_locale_id",
            "display_tips_zh_cn",
            "display_tips_en",
            language_by_id,
            f"Hero talent {talent_id} tips",
            failures,
        )
        if talent.get("display_name_zh_cn") and talent.get("display_name_en"):
            localized_talent_count += 1

    referenced_localized_skills = sum(
        1
        for skill_id in referenced_skill_ids
        if catalog_skill_by_id.get(skill_id, {}).get("display_name_zh_cn")
        and catalog_skill_by_id.get(skill_id, {}).get("display_name_en")
    )
    referenced_localized_talents = sum(
        1
        for talent_id in referenced_talent_ids
        if catalog_talent_by_id.get(talent_id, {}).get("display_name_zh_cn")
        and catalog_talent_by_id.get(talent_id, {}).get("display_name_en")
    )
    if referenced_localized_skills != len(referenced_skill_ids):
        failures.append(
            f"Only {referenced_localized_skills}/{len(referenced_skill_ids)} referenced hero skills have cn/en names"
        )
    if referenced_localized_talents != len(referenced_talent_ids):
        failures.append(
            f"Only {referenced_localized_talents}/{len(referenced_talent_ids)} referenced hero talents have cn/en names"
        )

    icon_coverage = {
        "hero_head": {
            "reference_count": hero_head_reference_count,
            "resolved_count": hero_head_resolved_count,
            "missing_count": hero_head_reference_count - hero_head_resolved_count,
            "missing_sprites": sorted(missing_hero_head_sprites),
        },
        "hero_skill": {
            "reference_count": hero_skill_reference_count,
            "resolved_count": hero_skill_resolved_count,
            "missing_count": hero_skill_reference_count - hero_skill_resolved_count,
            "missing_sprites": sorted(missing_hero_skill_sprites),
        },
        "hero_skill2": {
            "reference_count": hero_skill2_reference_count,
            "resolved_count": hero_skill2_resolved_count,
            "missing_count": hero_skill2_reference_count - hero_skill2_resolved_count,
            "missing_sprites": sorted(missing_hero_skill2_sprites),
        },
    }
    summary = catalog.get("summary", {})
    summary_expectations = {
        "hero_head_icon_reference_count": hero_head_reference_count,
        "hero_head_icon_resolved_count": hero_head_resolved_count,
        "hero_head_icon_missing_count": hero_head_reference_count - hero_head_resolved_count,
        "hero_skill_icon_reference_count": hero_skill_reference_count,
        "hero_skill_icon_resolved_count": hero_skill_resolved_count,
        "hero_skill_icon_missing_count": hero_skill_reference_count - hero_skill_resolved_count,
        "hero_skill2_icon_reference_count": hero_skill2_reference_count,
        "hero_skill2_icon_resolved_count": hero_skill2_resolved_count,
        "hero_skill2_icon_missing_count": hero_skill2_reference_count - hero_skill2_resolved_count,
    }
    for field_name, expected_value in summary_expectations.items():
        if summary.get(field_name) != expected_value:
            failures.append(
                f"Summary {field_name} expected {expected_value}, got {summary.get(field_name)}"
            )

    return {
        "hero_count": len(heroes),
        "localized_hero_count": localized_hero_count,
        "referenced_skill_count": len(referenced_skill_ids),
        "localized_referenced_skill_count": referenced_localized_skills,
        "localized_skill_row_count": localized_skill_count,
        "referenced_talent_count": len(referenced_talent_ids),
        "localized_referenced_talent_count": referenced_localized_talents,
        "localized_talent_row_count": localized_talent_count,
        "icon_coverage": icon_coverage,
    }


def main():
    failures = []
    if not CATALOG_PATH.exists():
        raise RuntimeError(f"Missing gameplay catalog: {CATALOG_PATH}")

    catalog = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    summary = catalog.get("summary", {})

    if catalog.get("schema_version") != 1:
        failures.append("Unsupported schema_version")

    for key, expected in EXPECTED_COUNTS.items():
        actual = summary.get(key)
        if actual != expected:
            failures.append(f"Expected {key}={expected}, got {actual}")

    source_table_expectations = {
        "BuildingTypeConfig": "building_type_count",
        "BuildingMenuData": "building_menu_count",
        "BuildingCountLimit": "building_count_limit_row_count",
        "BuildingLevelData": "building_level_row_count",
        "BuildingResourcesProduce": "building_produce_row_count",
        "Arms": "troop_count",
        "Hero": "hero_count",
        "HeroSkill": "hero_skill_count",
        "HeroTalent": "hero_talent_count",
        "TaskMain": "main_task_count",
        "TaskSide": "side_task_count",
        "TaskDaily": "daily_task_count",
        "TaskChapter": "chapter_task_count",
        "Vip": "vip_level_count",
        "VipAtt": "vip_attribute_count",
        "VipDayPoint": "vip_day_point_count",
        "BuildingHospital": "hospital_capacity_row_count",
    }
    for table_name, summary_key in source_table_expectations.items():
        source_count = load_source_count(table_name)
        if source_count != summary.get(summary_key):
            failures.append(f"Source count mismatch for {table_name}: source={source_count} catalog={summary.get(summary_key)}")

    training_capacity_source_count = sum(
        load_source_count(table_name)
        for table_name in (
            "BuildingBarracks",
            "BuildingStable",
            "BuildingArcheryrange",
            "BuildingSiegeWorkshop",
        )
    )
    if training_capacity_source_count != summary.get("training_capacity_row_count"):
        failures.append(
            "Training capacity source count mismatch: "
            f"source={training_capacity_source_count} "
            f"catalog={summary.get('training_capacity_row_count')}"
        )

    building_types = catalog.get("building_types", [])
    building_type_ids = {item.get("type_id") for item in building_types}
    missing_core = sorted(CORE_BUILDING_TYPES - building_type_ids)
    if missing_core:
        failures.append(f"Missing core building types: {missing_core}")

    for item in building_types:
        levels = item.get("levels", [])
        summary_block = item.get("level_summary", {})
        if len(levels) != summary_block.get("count"):
            failures.append(f"Level count mismatch for building type {item.get('type_id')}")
        if levels and summary_block.get("max_level", 0) <= 0:
            failures.append(f"Missing max level for building type {item.get('type_id')}")
        if item.get("type_id") == 1 and summary_block.get("max_level", 0) < 20:
            failures.append("Town center max level is unexpectedly low")

    building_by_type = {item.get("type_id"): item for item in building_types}
    for type_id in (9, 10, 11, 12):
        capacities = building_by_type.get(type_id, {}).get("training_capacity", [])
        capacity_by_level = {
            item.get("level"): item.get("capacity") for item in capacities
        }
        if len(capacities) != 25:
            failures.append(
                f"Building type {type_id} expected 25 training capacity rows, got {len(capacities)}"
            )
        for level, expected_capacity in ((1, 20), (2, 50), (25, 2000)):
            if capacity_by_level.get(level) != expected_capacity:
                failures.append(
                    f"Building type {type_id} level {level} training capacity "
                    f"expected {expected_capacity}, got {capacity_by_level.get(level)}"
                )

    hospital_capacities = building_by_type.get(14, {}).get(
        "hospital_capacity", []
    )
    hospital_capacity_by_level = {
        item.get("level"): item.get("capacity")
        for item in hospital_capacities
    }
    if len(hospital_capacities) != 25:
        failures.append(
            "Hospital building type 14 expected 25 capacity rows, "
            f"got {len(hospital_capacities)}"
        )
    for level, expected_capacity in ((1, 3000), (10, 14000), (25, 75000)):
        if hospital_capacity_by_level.get(level) != expected_capacity:
            failures.append(
                f"Hospital level {level} capacity expected {expected_capacity}, "
                f"got {hospital_capacity_by_level.get(level)}"
            )

    troops = catalog.get("troops", [])
    troop_family_counts = summary.get("troop_family_counts", {})
    if len(troop_family_counts) < 4:
        failures.append(f"Expected at least 4 troop families, got {troop_family_counts}")
    troops_with_positive_power = sum(1 for troop in troops if troop.get("stats", {}).get("power", 0) > 0)
    if troops_with_positive_power < 25:
        failures.append(f"Expected at least 25 troop rows with positive military power, got {troops_with_positive_power}")
    troops_with_model = sum(1 for troop in troops if troop.get("model"))
    if troops_with_model < 25:
        failures.append(f"Expected at least 25 troop rows with model references, got {troops_with_model}")
    if any(not troop.get("icon") for troop in troops):
        failures.append("At least one troop row has no icon reference")
    for arms_type in (1, 2, 3, 4):
        standard_troops = sorted(
            (
                troop
                for troop in troops
                if troop.get("arms_type") == arms_type
                and troop.get("sub_type") == 1
            ),
            key=lambda troop: troop.get("level", 0),
        )
        if [troop.get("level") for troop in standard_troops] != [1, 2, 3, 4, 5]:
            failures.append(
                f"Troop family {arms_type} does not contain exactly standard tiers 1-5"
            )
        elif standard_troops[0].get("study_id") != 0 or any(
            troop.get("study_id", 0) <= 0 for troop in standard_troops[1:]
        ):
            failures.append(
                f"Troop family {arms_type} has invalid Unity study unlock IDs"
            )

    heroes = catalog.get("heroes", [])
    heroes_with_skills = sum(1 for hero in heroes if len(hero.get("skill_ids", [])) >= 4)
    if heroes_with_skills != 39:
        failures.append(f"Expected 39 heroes with 4+ skills from the current Unity table, got {heroes_with_skills}")
    if any(not hero.get("model") for hero in heroes):
        failures.append("At least one hero row has no model reference")
    hero_relationships = verify_hero_catalog(catalog, failures)
    civilization_verification = verify_civilizations(catalog, failures)

    task_section_ids = {}
    all_task_ids = set()
    for section_name, spec in TASK_SECTION_SPECS.items():
        section_ids = verify_task_section(catalog, section_name, spec, failures)
        overlap = sorted(all_task_ids & section_ids)
        if overlap:
            failures.append(
                f"Task IDs overlap across catalog sections before {section_name}: {overlap[:20]}"
            )
        all_task_ids.update(section_ids)
        task_section_ids[section_name] = len(section_ids)

    if len({troop.get("id") for troop in troops}) != len(troops):
        failures.append("Duplicate troop IDs")

    vip_levels = catalog.get("vip_levels", [])
    vip_attributes = catalog.get("vip_attributes", [])
    vip_day_points = catalog.get("vip_day_points", [])
    if [item.get("level") for item in vip_levels] != list(range(18)):
        failures.append("VIP levels must contain the ordered range 0-17")
    if any(item.get("point", 0) <= 0 for item in vip_levels[:-1]):
        failures.append("At least one non-maximum VIP level has no positive point threshold")
    if not vip_levels or vip_levels[-1].get("level") != 17 or vip_levels[-1].get("point") != 0:
        failures.append("VIP17 must use the Unity maximum-level point sentinel 0")
    if any(item.get("level_group", -1) not in range(18) for item in vip_attributes):
        failures.append("At least one VIP attribute has an invalid level group")
    visible_vip_attributes = [item for item in vip_attributes if item.get("locale_id", 0) > 0]
    if len(visible_vip_attributes) != 221:
        failures.append(f"Expected 221 visible VIP attributes, got {len(visible_vip_attributes)}")
    level_zero_attributes = [item for item in visible_vip_attributes if item.get("level_group") == 0]
    if len(level_zero_attributes) != 2:
        failures.append(f"Expected 2 visible VIP0 attributes, got {len(level_zero_attributes)}")
    if any(item.get("locale_id", 0) > 0 and not item.get("localized_template") for item in vip_attributes):
        failures.append("At least one visible VIP attribute has no localized template")
    if any(item.get("locale_id", 0) > 0 and not item.get("display_text_zh_cn") for item in vip_attributes):
        failures.append("At least one visible VIP attribute has no formatted Chinese display text")
    vip_attribute_by_id = {item.get("id"): item for item in vip_attributes}
    expected_vip_attribute_text = {
        10000: "食物产量+3%",
        10047: "竞技场每日免费次数+1",
        10066: "永久解锁第二建造队列",
        10280: "部队治疗资源消耗-10%",
        10316: "所有伤害+5%",
    }
    for attribute_id, expected_text in expected_vip_attribute_text.items():
        actual_text = vip_attribute_by_id.get(attribute_id, {}).get("display_text_zh_cn")
        if actual_text != expected_text:
            failures.append(f"VIP attribute {attribute_id} expected '{expected_text}', got '{actual_text}'")
    if [item.get("day") for item in vip_day_points] != list(range(1, 9)):
        failures.append("VIP day-point rows must contain days 1-8")
    vip_store = catalog.get("vip_store", [])
    if len(vip_store) != 33 or any(item.get("id", 0) <= 0 for item in vip_store):
        failures.append(f"Expected 33 valid VIP store rows, got {len(vip_store)}")
    if any(not item.get("price_display") for item in vip_levels):
        failures.append("At least one VIP level has no configured price display")

    result = {
        "catalog": str(CATALOG_PATH.relative_to(PROJECT_ROOT)),
        "summary": summary,
        "core_building_types": sorted(building_type_ids & CORE_BUILDING_TYPES),
        "heroes_with_4plus_skills": heroes_with_skills,
        "hero_relationships": hero_relationships,
        "civilizations": civilization_verification,
        "troops_with_positive_power": troops_with_positive_power,
        "troops_with_model": troops_with_model,
        "task_section_ids": task_section_ids,
        "failures": failures,
    }
    OUTPUT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))

    if failures:
        raise RuntimeError("Unity gameplay catalog verification failed")


if __name__ == "__main__":
    main()
