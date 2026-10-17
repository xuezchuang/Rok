import json
from pathlib import Path, PurePosixPath
from rok_data_paths import DATA_ROOT, label


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
CONFIG_ROOT = DATA_ROOT / "ConfigTables"
UNITY_CLIENT_ROOT = PROJECT_ROOT / "ref" / "client-unity"

INIT_BUILDING_PATH = CONFIG_ROOT / "InitBuilding.json"
BUILDING_MODEL_PATH = CONFIG_ROOT / "BuildingModelData.json"
RUNTIME_BUILDINGS_PATH = MIGRATION_ROOT / "unity_runtime_city_buildings_obj_manifest.json"
RUNTIME_BOUNDARY_PATH = MIGRATION_ROOT / "unity_runtime_boundary_obj_manifest.json"
OUTPUT_PATH = DATA_ROOT / "unity_initial_building_visual_resolution.json"

CIV_TYPE = 101
AGE = 1
BOUNDARY_TYPES = {6, 8}
EXPECTED_INITIAL_BUILDINGS = 26
EXPECTED_BOUNDARY_BUILDINGS = 2
EXPECTED_RUNTIME_SPRITE_BUILDINGS = 24
EXPECTED_BOUNDARY_OBJECTS = 10
VALID_BUILDING_RENDER_QUEUES = {2995, 2996}


def load_json(path):
    if not path.exists():
        raise RuntimeError(f"Missing required source: {path}")
    return json.loads(path.read_text(encoding="utf-8-sig"))


def int_field(row, name, default=0):
    value = row.get(name, default)
    if value in (None, ""):
        return default
    return int(value)


def normalized_unity_asset_path(value):
    return str(value or "").replace("\\", "/")


def unity_asset_exists(asset_path):
    asset_path = normalized_unity_asset_path(asset_path)
    if not asset_path.startswith("Assets/"):
        return False
    return (UNITY_CLIENT_ROOT / PurePosixPath(asset_path)).exists()


def source_sprite_name(asset_path):
    asset_path = normalized_unity_asset_path(asset_path)
    return PurePosixPath(asset_path).stem if asset_path else ""


def build_model_lookup(rows):
    exact = {}
    fallback = {}
    for row in rows:
        building_type = int_field(row, "type")
        model_id = str(row.get("modelId", "")).strip()
        if building_type <= 0 or not model_id:
            continue
        fallback.setdefault(building_type, model_id)
        if int_field(row, "civType") == CIV_TYPE and int_field(row, "age") == AGE:
            exact[building_type] = model_id
    for building_type, model_id in fallback.items():
        exact.setdefault(building_type, model_id)
    return exact


def append_mismatch(failures, init_id, field, expected, actual):
    if expected != actual:
        failures.append(
            f"InitBuilding {init_id} {field} mismatch: expected {expected!r}, got {actual!r}"
        )


def main():
    init_table = load_json(INIT_BUILDING_PATH)
    model_table = load_json(BUILDING_MODEL_PATH)
    runtime_buildings = load_json(RUNTIME_BUILDINGS_PATH)
    runtime_boundary = load_json(RUNTIME_BOUNDARY_PATH)

    init_rows = sorted(init_table.get("rows", []), key=lambda row: int_field(row, "ID"))
    model_lookup = build_model_lookup(model_table.get("rows", []))
    runtime_records = runtime_buildings.get("records", [])
    boundary_records = runtime_boundary.get("records", [])
    failures = []

    if len(init_rows) != EXPECTED_INITIAL_BUILDINGS:
        failures.append(
            f"Expected {EXPECTED_INITIAL_BUILDINGS} InitBuilding rows, found {len(init_rows)}"
        )
    if int(runtime_buildings.get("initial_building_count", -1)) != EXPECTED_INITIAL_BUILDINGS:
        failures.append(
            "Unity runtime building export did not originate from all 26 InitBuilding rows"
        )
    if int(runtime_buildings.get("skipped_boundary_building_count", -1)) != EXPECTED_BOUNDARY_BUILDINGS:
        failures.append(
            "Unity runtime building export must skip exactly the CityWall and GuardTower InitBuilding rows"
        )
    if runtime_buildings.get("missing_prefabs"):
        failures.append(
            f"Unity runtime building export has missing prefabs: {runtime_buildings.get('missing_prefabs')}"
        )
    if len(runtime_records) != EXPECTED_RUNTIME_SPRITE_BUILDINGS:
        failures.append(
            f"Expected {EXPECTED_RUNTIME_SPRITE_BUILDINGS} runtime SpriteRenderer records, found {len(runtime_records)}"
        )
    if runtime_boundary.get("schema") != "rok.unity.runtime_boundary_obj.v1":
        failures.append(f"Unexpected runtime boundary schema: {runtime_boundary.get('schema')!r}")
    if float(runtime_boundary.get("container_yaw", 999.0)) != -45.0:
        failures.append(
            f"Runtime boundary container yaw must preserve Unity -45 degrees, got {runtime_boundary.get('container_yaw')}"
        )
    if len(boundary_records) != EXPECTED_BOUNDARY_OBJECTS:
        failures.append(
            f"Expected {EXPECTED_BOUNDARY_OBJECTS} Unity boundary mesh records, found {len(boundary_records)}"
        )

    runtime_by_init_id = {}
    for record in runtime_records:
        init_id = int_field(record, "init_id")
        if init_id in runtime_by_init_id:
            failures.append(f"Duplicate Unity runtime SpriteRenderer record for InitBuilding {init_id}")
        runtime_by_init_id[init_id] = record

    records = []
    boundary_init_ids = []
    sprite_init_ids = []
    for row in init_rows:
        init_id = int_field(row, "ID")
        building_type = int_field(row, "type")
        level = int_field(row, "level", 1)
        grid_x = int_field(row, "posX")
        grid_y = int_field(row, "posY")
        model_id = model_lookup.get(building_type, "")
        if not model_id:
            failures.append(f"InitBuilding {init_id} type {building_type} has no BuildingModelData model")

        common = {
            "init_id": init_id,
            "building_type": building_type,
            "level": level,
            "grid_x": grid_x,
            "grid_y": grid_y,
            "model_id": model_id,
        }

        if building_type in BOUNDARY_TYPES:
            boundary_init_ids.append(init_id)
            if init_id in runtime_by_init_id:
                failures.append(
                    f"Boundary InitBuilding {init_id} must be represented by the runtime boundary mesh collection, not a sprite card"
                )
            records.append({
                **common,
                "representation": "runtime_boundary_mesh_collection",
                "resolution": "unity_runtime_boundary_obj_manifest",
                "sprite_name": "",
                "source_manifest": label(RUNTIME_BOUNDARY_PATH),
                "boundary_object_count": len(boundary_records),
                "container_yaw": runtime_boundary.get("container_yaw"),
            })
            continue

        sprite_init_ids.append(init_id)
        source = runtime_by_init_id.get(init_id)
        if source is None:
            failures.append(f"Missing Unity runtime SpriteRenderer record for InitBuilding {init_id}")
            records.append({
                **common,
                "representation": "missing",
                "resolution": "missing_unity_runtime_sprite_renderer",
                "sprite_name": "",
            })
            continue

        append_mismatch(failures, init_id, "building_type", building_type, int_field(source, "building_type"))
        append_mismatch(failures, init_id, "level", level, int_field(source, "level"))
        append_mismatch(failures, init_id, "grid_x", grid_x, int_field(source, "grid_x"))
        append_mismatch(failures, init_id, "grid_y", grid_y, int_field(source, "grid_y"))
        append_mismatch(failures, init_id, "model_id", model_id, str(source.get("model_id", "")))

        sprite_path = normalized_unity_asset_path(source.get("sprite_path"))
        mask_path = normalized_unity_asset_path(source.get("mask_path"))
        prefab_path = normalized_unity_asset_path(source.get("prefab_path"))
        obj_path = Path(str(source.get("obj_path", "")))
        baked_texture_path = Path(str(source.get("baked_texture_path", "")))
        render_queue = int_field(source, "material_render_queue", -1)
        sprite_name = source_sprite_name(sprite_path)

        if not sprite_name or not unity_asset_exists(sprite_path):
            failures.append(f"InitBuilding {init_id} has missing Unity SpriteRenderer sprite asset: {sprite_path!r}")
        if not mask_path or not unity_asset_exists(mask_path):
            failures.append(f"InitBuilding {init_id} has missing Unity MaskSprite asset: {mask_path!r}")
        if not prefab_path or not unity_asset_exists(prefab_path):
            failures.append(f"InitBuilding {init_id} has missing Unity prefab asset: {prefab_path!r}")
        if not obj_path.is_file():
            failures.append(f"InitBuilding {init_id} has missing exported SpriteRenderer OBJ: {obj_path}")
        if not baked_texture_path.is_file():
            failures.append(f"InitBuilding {init_id} has missing baked SpriteRenderer texture: {baked_texture_path}")
        if render_queue not in VALID_BUILDING_RENDER_QUEUES:
            failures.append(
                f"InitBuilding {init_id} render queue must be one of {sorted(VALID_BUILDING_RENDER_QUEUES)}, got {render_queue}"
            )

        records.append({
            **common,
            "representation": "runtime_sprite_renderer_card",
            "resolution": "unity_prefab_sprite_renderer",
            "sprite_name": sprite_name,
            "source_manifest": label(RUNTIME_BUILDINGS_PATH),
            "prefab_path": prefab_path,
            "renderer_name": source.get("renderer_name", ""),
            "renderer_path": source.get("renderer_path", ""),
            "sprite_path": sprite_path,
            "sprite_guid": source.get("sprite_guid", ""),
            "mask_path": mask_path,
            "mask_guid": source.get("mask_guid", ""),
            "material_path": normalized_unity_asset_path(source.get("material_path")),
            "shader_name": source.get("shader_name", ""),
            "material_render_queue": render_queue,
            "sorting_layer": int_field(source, "sorting_layer"),
            "sorting_order": int_field(source, "sorting_order"),
            "obj_path": str(obj_path),
            "baked_texture_path": str(baked_texture_path),
            "initial_state_visual_sprite_name": source.get("visual_sprite_name", ""),
            "initial_state_visual_resolution": source.get("visual_resolution", ""),
        })

    unexpected_runtime_ids = sorted(set(runtime_by_init_id) - set(sprite_init_ids))
    if unexpected_runtime_ids:
        failures.append(f"Unexpected runtime SpriteRenderer InitBuilding ids: {unexpected_runtime_ids}")
    if len(boundary_init_ids) != EXPECTED_BOUNDARY_BUILDINGS:
        failures.append(
            f"Expected {EXPECTED_BOUNDARY_BUILDINGS} boundary InitBuilding rows, found {len(boundary_init_ids)}"
        )

    queue_counts = {}
    for record in records:
        if record.get("representation") != "runtime_sprite_renderer_card":
            continue
        queue = str(record.get("material_render_queue"))
        queue_counts[queue] = queue_counts.get(queue, 0) + 1
    if queue_counts != {"2995": 23, "2996": 1}:
        failures.append(
            f"Unity initial building render queues must be 23x2995 and 1x2996, got {queue_counts}"
        )

    result = {
        "schema": "rok.unity_initial_building_visual_resolution.v1",
        "civ_type": CIV_TYPE,
        "age": AGE,
        "source_files": {
            "init_building": label(INIT_BUILDING_PATH),
            "building_model_data": label(BUILDING_MODEL_PATH),
            "runtime_buildings": label(RUNTIME_BUILDINGS_PATH),
            "runtime_boundary": label(RUNTIME_BOUNDARY_PATH),
        },
        "building_count": len(records),
        "runtime_sprite_building_count": len(sprite_init_ids),
        "runtime_boundary_init_count": len(boundary_init_ids),
        "runtime_boundary_object_count": len(boundary_records),
        "render_queue_counts": queue_counts,
        "records": records,
        "failures": failures,
    }
    MIGRATION_ROOT.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "output": str(OUTPUT_PATH),
        "building_count": len(records),
        "runtime_sprite_building_count": len(sprite_init_ids),
        "runtime_boundary_init_count": len(boundary_init_ids),
        "render_queue_counts": queue_counts,
        "failure_count": len(failures),
        "failures": failures,
    }, ensure_ascii=False, indent=2))
    if failures:
        raise RuntimeError("Unity initial building visual resolution failed")


if __name__ == "__main__":
    main()
