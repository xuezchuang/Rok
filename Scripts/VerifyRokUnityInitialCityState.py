import json
from pathlib import Path, PurePosixPath
from rok_data_paths import DATA_ROOT, label


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
STATE_PATH = DATA_ROOT / "unity_initial_city_state.json"
VISUAL_RESOLUTION_PATH = DATA_ROOT / "unity_initial_building_visual_resolution.json"
SCENE_VERIFY_PATH = MIGRATION_ROOT / "unity_gameapp_city_runtime_verify.json"
OUTPUT_PATH = MIGRATION_ROOT / "unity_initial_city_state_verify.json"

BOUNDARY_TYPES = {6, 8}
EXPECTED_RUNTIME_SPRITE_BUILDINGS = 24


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
    if state.get("visual_resolution_failures"):
        failures.append(f"Visual resolution failures: {state.get('visual_resolution_failures')}")

    seen_init_ids = set()
    render_queue_counts = {}
    runtime_sprite_count = 0
    runtime_boundary_count = 0
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
        if not visual.get("model_id"):
            failures.append(f"Missing BuildingModelData model for init_id {init_id}")
        if item.get("type_id") in BOUNDARY_TYPES:
            runtime_boundary_count += 1
            if visual.get("representation") != "runtime_boundary_mesh_collection":
                failures.append(f"Boundary InitBuilding {init_id} is not resolved through the runtime boundary mesh collection")
            if visual.get("resolution") != "unity_runtime_boundary_obj_manifest":
                failures.append(f"Boundary InitBuilding {init_id} has wrong visual resolution source")
            if visual.get("boundary_object_count") != 10:
                failures.append(f"Boundary InitBuilding {init_id} does not reference all 10 Unity boundary renderers")
        else:
            runtime_sprite_count += 1
            if visual.get("representation") != "runtime_sprite_renderer_card":
                failures.append(f"InitBuilding {init_id} is not resolved from a Unity prefab SpriteRenderer")
            if visual.get("resolution") != "unity_prefab_sprite_renderer":
                failures.append(f"InitBuilding {init_id} has non-authoritative visual resolution {visual.get('resolution')!r}")
            sprite_name = visual.get("sprite_name", "")
            sprite_path = str(visual.get("sprite_path", "")).replace("\\", "/")
            if not sprite_name or not sprite_path:
                failures.append(f"Missing Unity SpriteRenderer sprite for init_id {init_id}")
            elif PurePosixPath(sprite_path).stem != sprite_name:
                failures.append(
                    f"Unity SpriteRenderer sprite name/path mismatch for init_id {init_id}: {sprite_name!r} vs {sprite_path!r}"
                )
            if not visual.get("sprite_guid") or not visual.get("mask_path") or not visual.get("mask_guid"):
                failures.append(f"Missing Unity sprite/mask GUID provenance for init_id {init_id}")
            if not visual.get("prefab_path") or not visual.get("renderer_path"):
                failures.append(f"Missing Unity prefab/renderer provenance for init_id {init_id}")
            render_queue = visual.get("material_render_queue")
            if render_queue not in (2995, 2996):
                failures.append(f"Invalid Unity render queue for init_id {init_id}: {render_queue}")
            else:
                key = str(render_queue)
                render_queue_counts[key] = render_queue_counts.get(key, 0) + 1
            if visual.get("sorting_layer") != 0 or visual.get("sorting_order") != 0:
                failures.append(f"Unexpected Unity SortingLayer/Order for init_id {init_id}")
        level_config = item.get("level_config", {})
        if not item.get("is_decoration") and item.get("type_id") != 29 and level_config.get("power", 0) <= 0:
            failures.append(f"Gameplay building has no power config: init_id {init_id}")

    if runtime_sprite_count != EXPECTED_RUNTIME_SPRITE_BUILDINGS:
        failures.append(f"Expected 24 runtime SpriteRenderer buildings, found {runtime_sprite_count}")
    if runtime_boundary_count != 2:
        failures.append(f"Expected 2 boundary InitBuilding identities, found {runtime_boundary_count}")
    if render_queue_counts != {"2995": 23, "2996": 1}:
        failures.append(f"Expected Unity building queues 23x2995 and 1x2996, found {render_queue_counts}")

    visual_resolution = {}
    if VISUAL_RESOLUTION_PATH.exists():
        visual_resolution = json.loads(VISUAL_RESOLUTION_PATH.read_text(encoding="utf-8-sig"))
        if visual_resolution.get("schema") != "rok.unity_initial_building_visual_resolution.v1":
            failures.append("Wrong initial building visual resolution schema")
        if visual_resolution.get("building_count") != 26:
            failures.append("Initial building visual resolution does not cover all 26 InitBuilding rows")
        if visual_resolution.get("runtime_sprite_building_count") != 24:
            failures.append("Initial building visual resolution does not cover all 24 non-boundary SpriteRenderers")
        if visual_resolution.get("runtime_boundary_init_count") != 2:
            failures.append("Initial building visual resolution does not cover both boundary InitBuilding rows")
        if visual_resolution.get("failures"):
            failures.append(f"Initial building visual resolution failures: {visual_resolution.get('failures')}")
    else:
        failures.append(f"Missing visual resolution evidence: {VISUAL_RESOLUTION_PATH}")

    scene_verify = {}
    if SCENE_VERIFY_PATH.exists():
        scene_verify = json.loads(SCENE_VERIFY_PATH.read_text(encoding="utf-8"))
        if scene_verify.get("map") != state.get("target_level"):
            failures.append("Scene verify target_level does not match state manifest")
        if scene_verify.get("initial_buildings") != EXPECTED_RUNTIME_SPRITE_BUILDINGS:
            failures.append(
                f"Scene has {scene_verify.get('initial_buildings')} runtime SpriteRenderer buildings, expected {EXPECTED_RUNTIME_SPRITE_BUILDINGS}"
            )
        if scene_verify.get("unique_init_id_count") != EXPECTED_RUNTIME_SPRITE_BUILDINGS:
            failures.append("Scene does not have exactly 24 unique non-boundary Unity InitBuilding identities")
        if scene_verify.get("extra_interior_buildings") != 0:
            failures.append("Promoted GameApp city contains hand-placed supplemental interior preview buildings")
        if scene_verify.get("building_sort_priority_mismatch_count") != 0:
            failures.append(
                f"Scene building sort priorities do not match Unity material queues: {scene_verify.get('building_sort_priority_mismatches')}"
            )
        if scene_verify.get("rendered_unresolved_visual_count") != 0:
            failures.append("Scene contains InitBuilding actors without exact Unity prefab SpriteRenderer provenance")
        if scene_verify.get("failures"):
            failures.append(f"Scene verification failures: {scene_verify.get('failures')}")
    else:
        failures.append(f"Missing scene verification evidence: {SCENE_VERIFY_PATH}")

    result = {
        "state_manifest": str(Path(label(STATE_PATH))),
        "visual_resolution": str(Path(label(VISUAL_RESOLUTION_PATH))),
        "scene_verify": str(SCENE_VERIFY_PATH.relative_to(PROJECT_ROOT)),
        "building_count": len(buildings),
        "gameplay_building_count": state.get("gameplay_building_count"),
        "decoration_count": state.get("decoration_count"),
        "total_power": state.get("total_power"),
        "production_per_hour": state.get("production_per_hour"),
        "runtime_sprite_building_count": runtime_sprite_count,
        "runtime_boundary_init_count": runtime_boundary_count,
        "render_queue_counts": render_queue_counts,
        "failures": failures,
    }
    OUTPUT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))

    if failures:
        raise RuntimeError("Unity initial city state verification failed")


if __name__ == "__main__":
    main()
