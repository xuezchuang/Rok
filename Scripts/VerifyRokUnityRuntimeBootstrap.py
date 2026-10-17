import json
from pathlib import Path
from rok_data_paths import DATA_ROOT, label


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
BOOTSTRAP_PATH = DATA_ROOT / "unity_runtime_bootstrap_state.json"
CATALOG_PATH = DATA_ROOT / "unity_gameplay_catalog.json"
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
    hero_runtime = bootstrap.get("hero_runtime", {})
    hero_selection = hero_runtime.get("selection", {})
    starter_heroes = hero_runtime.get("starter_heroes", [])
    starter_stacks = bootstrap.get("troop_runtime", {}).get("starter_stacks", [])
    trainable_troop_ids = bootstrap.get("troop_runtime", {}).get("trainable_troop_ids", [])
    player_runtime = bootstrap.get("player_runtime", {})

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
    if "listDisplay == 1" not in hero_runtime.get("selection_rule", ""):
        failures.append("Starter hero selection rule does not document hidden listDisplay rows")
    if "HeroProxy.cs" not in hero_runtime.get("source", ""):
        failures.append("Starter hero selection source does not identify Unity HeroProxy")

    if not CATALOG_PATH.exists():
        failures.append(f"Missing gameplay catalog: {CATALOG_PATH}")
        catalog = {}
    else:
        catalog = load_json(CATALOG_PATH)
    eligible_heroes = [
        hero
        for hero in catalog.get("heroes", [])
        if int(hero.get("list_display") or 0) != 1
    ]
    expected_starter_heroes = sorted(
        eligible_heroes,
        key=lambda item: (
            -int(item.get("rare") or 0),
            -int(item.get("score") or 0),
            int(item.get("id") or 0),
        ),
    )[:5]
    expected_starter_ids = [int(hero.get("id") or 0) for hero in expected_starter_heroes]
    actual_starter_ids = [int(hero.get("hero_id") or 0) for hero in starter_heroes]
    if actual_starter_ids != expected_starter_ids:
        failures.append(
            f"Starter hero IDs do not match HeroProxy-visible selection: expected {expected_starter_ids}, got {actual_starter_ids}"
        )
    if any(int(hero.get("list_display") or 0) == 1 for hero in starter_heroes):
        failures.append("Starter hero selection reintroduced a hidden listDisplay == 1 row")

    expected_shortfall = max(0, 5 - len(eligible_heroes))
    selection_expectations = {
        "requested_count": 5,
        "eligible_count": len(eligible_heroes),
        "selected_count": len(expected_starter_heroes),
        "shortfall": expected_shortfall,
    }
    if hero_selection != selection_expectations:
        failures.append(
            f"Starter hero selection evidence expected {selection_expectations}, got {hero_selection}"
        )
    if summary.get("starter_hero_eligible_count") != len(eligible_heroes):
        failures.append("Starter hero eligible count does not match gameplay catalog")
    if summary.get("starter_hero_selection_shortfall") != expected_shortfall:
        failures.append("Starter hero selection shortfall does not match gameplay catalog")
    if expected_shortfall > 0:
        failures.append(
            f"Only {len(eligible_heroes)} HeroProxy-visible heroes are available for 5 starter slots"
        )
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

    expected_local_hero_score = sum(int(hero.get("score") or 0) for hero in starter_heroes)
    expected_local_troop_power = sum(int(stack.get("total_power") or 0) for stack in starter_stacks)
    expected_local_preview_score = expected_local_hero_score + expected_local_troop_power
    if summary.get("total_hero_score") != expected_local_hero_score:
        failures.append("Summary total hero score does not match starter hero rows")
    if summary.get("total_troop_power") != expected_local_troop_power:
        failures.append("Summary total troop power does not match starter troop rows")
    if player_runtime.get("state_kind") != "local_preview":
        failures.append("Player runtime state_kind must be local_preview")
    if player_runtime.get("default_head_id") != 1000:
        failures.append("Player runtime default_head_id must be 1000")
    if player_runtime.get("default_frame_id") != 2000:
        failures.append("Player runtime default_frame_id must be 2000")

    unknown_server_fields = {
        "has_role_id": (False, "role_id", ""),
        "has_display_name": (False, "display_name", ""),
        "has_civilization": (False, "civilization_name", ""),
        "has_alliance": (False, "alliance_name", ""),
        "has_kill_count": (False, "kill_count", 0),
        "has_action_points": (False, "action_points_current", 0),
        "has_vip_level": (False, "vip_level", 0),
    }
    for flag_name, (expected_flag, value_name, expected_value) in unknown_server_fields.items():
        if player_runtime.get(flag_name) is not expected_flag:
            failures.append(f"Player runtime {flag_name} must be false for local preview")
        if player_runtime.get(value_name) != expected_value:
            failures.append(f"Player runtime {value_name} must remain unknown/empty")
    if player_runtime.get("civilization_id") != 0:
        failures.append("Player runtime civilization_id must remain unknown (0)")
    if player_runtime.get("action_points_max") != 0:
        failures.append("Player runtime action_points_max must remain unknown (0)")
    if player_runtime.get("local_hero_score") != expected_local_hero_score:
        failures.append("Player runtime local_hero_score does not match starter hero rows")
    if player_runtime.get("local_troop_power") != expected_local_troop_power:
        failures.append("Player runtime local_troop_power does not match starter troop rows")
    if player_runtime.get("local_preview_score") != expected_local_preview_score:
        failures.append("Player runtime local_preview_score is not hero score plus troop power")
    if "not official combatPower" not in player_runtime.get("local_preview_note", ""):
        failures.append("Player runtime preview note does not distinguish official combatPower")

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
        "runtime_bootstrap": str(Path(label(BOOTSTRAP_PATH))),
        "catalog_verify": str(CATALOG_VERIFY_PATH.relative_to(PROJECT_ROOT)),
        "initial_city_verify": str(CITY_VERIFY_PATH.relative_to(PROJECT_ROOT)),
        "summary": summary,
        "starter_hero_ids": actual_starter_ids,
        "starter_hero_selection": hero_selection,
        "player_runtime": player_runtime,
        "expected_local_preview": {
            "local_hero_score": expected_local_hero_score,
            "local_troop_power": expected_local_troop_power,
            "local_preview_score": expected_local_preview_score,
        },
        "failures": failures,
    }
    OUTPUT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))

    if failures:
        raise RuntimeError("Unity runtime bootstrap verification failed")


if __name__ == "__main__":
    main()
