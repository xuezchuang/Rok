import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
CONFIG_ROOT = MIGRATION_ROOT / "ConfigTables"
CATALOG_PATH = MIGRATION_ROOT / "unity_gameplay_catalog.json"
OUTPUT_PATH = MIGRATION_ROOT / "unity_gameplay_catalog_verify.json"

EXPECTED_COUNTS = {
    "building_type_count": 30,
    "building_level_row_count": 487,
    "building_menu_count": 43,
    "building_count_limit_row_count": 700,
    "building_produce_row_count": 100,
    "troop_count": 54,
    "hero_count": 46,
    "hero_skill_count": 261,
    "hero_talent_count": 117,
    "main_task_count": 205,
}

CORE_BUILDING_TYPES = {1, 6, 7, 8, 9, 14, 15, 29}


def load_source_count(table_name):
    path = CONFIG_ROOT / f"{table_name}.json"
    if not path.exists():
        return -1
    data = json.loads(path.read_text(encoding="utf-8"))
    return len(data.get("rows", []))


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
    }
    for table_name, summary_key in source_table_expectations.items():
        source_count = load_source_count(table_name)
        if source_count != summary.get(summary_key):
            failures.append(f"Source count mismatch for {table_name}: source={source_count} catalog={summary.get(summary_key)}")

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

    heroes = catalog.get("heroes", [])
    heroes_with_skills = sum(1 for hero in heroes if len(hero.get("skill_ids", [])) >= 4)
    if heroes_with_skills != 39:
        failures.append(f"Expected 39 heroes with 4+ skills from the current Unity table, got {heroes_with_skills}")
    if any(not hero.get("model") for hero in heroes):
        failures.append("At least one hero row has no model reference")

    main_tasks = catalog.get("main_tasks", [])
    task_by_id = {task.get("id"): task for task in main_tasks}
    first_task = task_by_id.get(200900)
    if not first_task:
        failures.append("Missing expected first main task 200900")
    elif first_task.get("next_id") != 200901:
        failures.append(f"Main task 200900 next_id expected 200901, got {first_task.get('next_id')}")

    if len({task.get("id") for task in main_tasks}) != len(main_tasks):
        failures.append("Duplicate main task IDs")
    if len({hero.get("id") for hero in heroes}) != len(heroes):
        failures.append("Duplicate hero IDs")
    if len({troop.get("id") for troop in troops}) != len(troops):
        failures.append("Duplicate troop IDs")

    result = {
        "catalog": str(CATALOG_PATH.relative_to(PROJECT_ROOT)),
        "summary": summary,
        "core_building_types": sorted(building_type_ids & CORE_BUILDING_TYPES),
        "heroes_with_4plus_skills": heroes_with_skills,
        "troops_with_positive_power": troops_with_positive_power,
        "troops_with_model": troops_with_model,
        "failures": failures,
    }
    OUTPUT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))

    if failures:
        raise RuntimeError("Unity gameplay catalog verification failed")


if __name__ == "__main__":
    main()
