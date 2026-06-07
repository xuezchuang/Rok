import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
BOOTSTRAP_PATH = MIGRATION_ROOT / "unity_runtime_bootstrap_state.json"
CATALOG_VERIFY_PATH = MIGRATION_ROOT / "unity_gameplay_catalog_verify.json"
CITY_VERIFY_PATH = MIGRATION_ROOT / "unity_initial_city_state_verify.json"
OUTPUT_PATH = MIGRATION_ROOT / "unity_runtime_bootstrap_state_verify.json"


def load_json(path):
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def main():
    failures = []
    if not BOOTSTRAP_PATH.exists():
        raise RuntimeError(f"Missing runtime bootstrap: {BOOTSTRAP_PATH}")

    bootstrap = load_json(BOOTSTRAP_PATH)
    summary = bootstrap.get("summary", {})
    active_task = bootstrap.get("main_task_runtime", {}).get("active_task", {})
    starter_heroes = bootstrap.get("hero_runtime", {}).get("starter_heroes", [])
    starter_stacks = bootstrap.get("troop_runtime", {}).get("starter_stacks", [])
    trainable_troop_ids = bootstrap.get("troop_runtime", {}).get("trainable_troop_ids", [])

    if bootstrap.get("schema_version") != 1:
        failures.append("Unsupported schema_version")
    if summary.get("active_task_id") != 200900:
        failures.append(f"Expected active task 200900, got {summary.get('active_task_id')}")
    if active_task.get("progress", {}).get("rule") != "building_level":
        failures.append(f"Expected active task building_level evaluator, got {active_task.get('progress', {}).get('rule')}")
    if summary.get("active_task_current") != 1 or summary.get("active_task_target") != 2:
        failures.append(f"Expected active task progress 1/2, got {summary.get('active_task_current')}/{summary.get('active_task_target')}")
    if summary.get("active_task_complete"):
        failures.append("Active bootstrap task should not be complete before town center upgrade")
    if summary.get("active_task_chain_count", 0) < 6:
        failures.append("Expected at least 6 migrated active task chain rows")
    if summary.get("reference_task_chain_count", 0) < 10:
        failures.append("Expected at least 10 migrated reference task chain rows")
    if len(starter_heroes) != summary.get("starter_hero_count") or len(starter_heroes) < 5:
        failures.append(f"Expected 5 starter heroes, got {len(starter_heroes)}")
    if any(not hero.get("skill_ids") for hero in starter_heroes):
        failures.append("Starter hero without skill ids")
    if len(starter_stacks) != 4 or summary.get("starter_troop_stack_count") != 4:
        failures.append(f"Expected 4 starter troop stacks, got {len(starter_stacks)}")
    if summary.get("total_troop_count") != 200:
        failures.append(f"Expected total_troop_count 200, got {summary.get('total_troop_count')}")
    if summary.get("trainable_troop_count") != len(trainable_troop_ids):
        failures.append("Trainable troop count does not match ID list")
    if summary.get("trainable_troop_count") < 29:
        failures.append(f"Expected at least 29 trainable troops, got {summary.get('trainable_troop_count')}")
    if any(stack.get("power_per_unit", 0) <= 0 for stack in starter_stacks):
        failures.append("Starter troop stack has no power")

    catalog_verify = {}
    if CATALOG_VERIFY_PATH.exists():
        catalog_verify = load_json(CATALOG_VERIFY_PATH)
        if catalog_verify.get("failures"):
            failures.append(f"Catalog verification failures: {catalog_verify.get('failures')}")
    else:
        failures.append(f"Missing catalog verification evidence: {CATALOG_VERIFY_PATH}")

    city_verify = {}
    if CITY_VERIFY_PATH.exists():
        city_verify = load_json(CITY_VERIFY_PATH)
        if city_verify.get("failures"):
            failures.append(f"Initial city verification failures: {city_verify.get('failures')}")
    else:
        failures.append(f"Missing initial city verification evidence: {CITY_VERIFY_PATH}")

    result = {
        "runtime_bootstrap": str(BOOTSTRAP_PATH.relative_to(PROJECT_ROOT)),
        "catalog_verify": str(CATALOG_VERIFY_PATH.relative_to(PROJECT_ROOT)),
        "initial_city_verify": str(CITY_VERIFY_PATH.relative_to(PROJECT_ROOT)),
        "summary": summary,
        "failures": failures,
    }
    OUTPUT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))

    if failures:
        raise RuntimeError("Unity runtime bootstrap verification failed")


if __name__ == "__main__":
    main()
