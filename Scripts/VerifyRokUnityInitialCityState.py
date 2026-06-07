import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
STATE_PATH = MIGRATION_ROOT / "unity_initial_city_state.json"
SCENE_VERIFY_PATH = MIGRATION_ROOT / "unity_gameapp_city_verify.json"
OUTPUT_PATH = MIGRATION_ROOT / "unity_initial_city_state_verify.json"


def fail(message):
    raise RuntimeError(message)


def main():
    if not STATE_PATH.exists():
        fail(f"Missing state manifest: {STATE_PATH}")
    state = json.loads(STATE_PATH.read_text(encoding="utf-8"))
    buildings = state.get("buildings", [])
    failures = []

    if state.get("schema_version") != 1:
        failures.append("Unsupported schema_version")
    if state.get("target_level") != "/Game/Maps/L_RokUnityGameAppCity":
        failures.append("Wrong target_level")
    if state.get("building_count") != 26 or len(buildings) != 26:
        failures.append(f"Expected 26 InitBuilding rows, got manifest={state.get('building_count')} rows={len(buildings)}")
    if state.get("gameplay_building_count", 0) < 8:
        failures.append("Expected at least 8 non-decoration startup buildings")
    if state.get("missing_level_config"):
        failures.append(f"Missing level configs: {state.get('missing_level_config')}")

    seen_init_ids = set()
    for item in buildings:
        init_id = item.get("init_id", 0)
        if init_id in seen_init_ids:
            failures.append(f"Duplicate init_id {init_id}")
        seen_init_ids.add(init_id)
        if not item.get("actor_tags") or f"UnityInitId_{init_id}" not in item["actor_tags"]:
            failures.append(f"Missing UnityInitId tag for init_id {init_id}")
        if item.get("type_id", 0) <= 0:
            failures.append(f"Invalid type_id for init_id {init_id}")
        if item.get("level", 0) <= 0:
            failures.append(f"Invalid level for init_id {init_id}")
        visual = item.get("visual", {})
        if not visual.get("sprite_name"):
            failures.append(f"Missing visual sprite for init_id {init_id}")
        level_config = item.get("level_config", {})
        if not item.get("is_decoration") and item.get("type_id") != 29 and level_config.get("power", 0) <= 0:
            failures.append(f"Gameplay building has no power config: init_id {init_id}")

    scene_verify = {}
    if SCENE_VERIFY_PATH.exists():
        scene_verify = json.loads(SCENE_VERIFY_PATH.read_text(encoding="utf-8"))
        if scene_verify.get("target_level") != state.get("target_level"):
            failures.append("Scene verify target_level does not match state manifest")
        if scene_verify.get("config_init_buildings") != state.get("building_count"):
            failures.append(
                f"Scene has {scene_verify.get('config_init_buildings')} config buildings, expected {state.get('building_count')}"
            )
        if scene_verify.get("failures"):
            failures.append(f"Scene verification failures: {scene_verify.get('failures')}")
    else:
        failures.append(f"Missing scene verification evidence: {SCENE_VERIFY_PATH}")

    result = {
        "state_manifest": str(STATE_PATH.relative_to(PROJECT_ROOT)),
        "scene_verify": str(SCENE_VERIFY_PATH.relative_to(PROJECT_ROOT)),
        "building_count": len(buildings),
        "gameplay_building_count": state.get("gameplay_building_count"),
        "decoration_count": state.get("decoration_count"),
        "total_power": state.get("total_power"),
        "production_per_hour": state.get("production_per_hour"),
        "failures": failures,
    }
    OUTPUT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))

    if failures:
        raise RuntimeError("Unity initial city state verification failed")


if __name__ == "__main__":
    main()
