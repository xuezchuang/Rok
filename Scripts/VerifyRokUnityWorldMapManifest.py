import csv
import hashlib
import json
import math
from pathlib import Path
from rok_data_paths import resolve

from BuildRokUnityWorldMapManifest import parse_world_environment


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
MANIFEST_PATH = OUTPUT_DIR / "unity_world_map.json"
VERIFY_PATH = OUTPUT_DIR / "unity_world_map_verify.json"

EXPECTED_TILE_IDS = {
    (37, 4): "Tile-pure-grass2",
    (38, 4): "Tile-JuDian-A1",
    (39, 4): "Tile-snow-you_fz",
    (37, 5): "Tile-pure-grass",
    (38, 5): "Tile-pure-grass1",
    (39, 5): "Tile-snow-you",
    (37, 6): "Tile-mountain-H_GK",
    (38, 6): "Tile-mountain-XJ5",
    (39, 6): "Tile-snow-you",
}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def check_source(role, record, failures):
    relative_path = record.get("path", "")
    if not relative_path:
        failures.append(f"Source {role} has no path")
        return
    path = resolve(relative_path)
    if not path.exists():
        failures.append(f"Source {role} is missing: {relative_path}")
        return
    if path.stat().st_size != record.get("size"):
        failures.append(f"Source {role} size changed: {relative_path}")
    if sha256(path) != record.get("sha256"):
        failures.append(f"Source {role} hash changed: {relative_path}")


def nearly_equal(actual, expected, tolerance=0.001):
    try:
        return math.isclose(float(actual), float(expected), abs_tol=tolerance)
    except (TypeError, ValueError):
        return False


def parse_camera_source(path, failures):
    try:
        with path.open(
            "r",
            encoding="utf-8-sig",
            newline="",
        ) as handle:
            rows = []
            for row_number, row in enumerate(
                csv.DictReader(handle),
                start=2,
            ):
                camera_id = (row.get("ID") or "").strip()
                if not camera_id or camera_id == "string":
                    continue
                rows.append(
                    {
                        "id": camera_id,
                        "distance_unity_units": float(row["dist"]),
                        "vertical_fov_degrees": float(row["fov"]),
                        "dxf": float(row["dxf"]),
                        "unity_forward": {
                            "x": float(row["forwardX"]),
                            "y": float(row["forwardY"]),
                            "z": float(row["forwardZ"]),
                        },
                    }
                )
    except (KeyError, OSError, TypeError, ValueError) as error:
        failures.append(f"Could not parse camera source {path}: {error}")
        return []
    return sorted(rows, key=lambda item: item["dxf"])


def camera_records_match(actual, expected):
    if len(actual) != len(expected):
        return False
    scalar_fields = (
        "distance_unity_units",
        "vertical_fov_degrees",
        "dxf",
    )
    for actual_row, expected_row in zip(actual, expected):
        if actual_row.get("id") != expected_row.get("id"):
            return False
        if any(
            not nearly_equal(
                actual_row.get(field),
                expected_row.get(field),
                tolerance=0.0001,
            )
            for field in scalar_fields
        ):
            return False
        actual_forward = actual_row.get("unity_forward", {})
        expected_forward = expected_row.get("unity_forward", {})
        if any(
            not nearly_equal(
                actual_forward.get(axis),
                expected_forward.get(axis),
                tolerance=0.0001,
            )
            for axis in ("x", "y", "z")
        ):
            return False
    return True


def main():
    failures = []
    if not MANIFEST_PATH.exists():
        failures.append(f"Missing generated world-map manifest: {MANIFEST_PATH}")
        manifest = {}
    else:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    if manifest.get("schema") != "rok.unity_world_map.v1":
        failures.append(f"Unexpected schema: {manifest.get('schema')}")

    sources = manifest.get("sources", {})
    for role in {
        "map_brief",
        "tile_templates",
        "addressable_group",
        "addressable_import_settings",
        "map_tile_group",
        "camera_parameters",
        "world_scene",
        "light_manager",
        "day_night_logic",
    }:
        check_source(role, sources.get(role, {}), failures)

    expected_environment = parse_world_environment()
    if manifest.get("environment") != expected_environment:
        failures.append(
            "World environment does not match the source World.unity "
            "RenderSettings, Directional Light, and runtime day/night rules"
        )

    resolution = manifest.get("addressable_resolution", {})
    expected_root = "ref/client-unity/Assets/BundleAssets/land/MapData/"
    for key in ("map_brief_path", "tile_data_path"):
        if not resolution.get(key, "").startswith(expected_root):
            failures.append(
                f"{key} is not the canonical Addressables land/MapData path"
            )
        if "/BundleAssets/Map/Data/" in resolution.get(key, ""):
            failures.append(f"{key} incorrectly uses legacy Map/Data")

    map_record = manifest.get("map", {})
    expected_map_values = {
        "tile_count": 1600,
        "width_tiles": 40,
        "height_tiles": 40,
        "world_width_unity_units": 7200.0,
        "world_height_unity_units": 7200.0,
    }
    for key, expected in expected_map_values.items():
        if map_record.get(key) != expected:
            failures.append(
                f"Unexpected map {key}: {map_record.get(key)} != {expected}"
            )
    if len(map_record.get("tiles", [])) != 1600:
        failures.append("Map does not contain all 1600 authoritative tile records")

    template_summary = manifest.get("tile_template_summary", {})
    expected_template_values = {
        "tile_type_count": 66,
        "plane_record_count": 594,
        "object_record_count": 1909,
    }
    for key, expected in expected_template_values.items():
        if template_summary.get(key) != expected:
            failures.append(
                f"Unexpected tile-template {key}: "
                f"{template_summary.get(key)} != {expected}"
            )
    templates = manifest.get("tile_templates", [])
    if len(templates) != 66:
        failures.append(f"Expected 66 tile templates, found {len(templates)}")
    template_ids = {item.get("tile_id") for item in templates}
    full_scene = manifest.get("full_scene", {})
    expected_ground_prefab_ids = {
        record.get("prefab_id")
        for template in templates
        for record in template.get("planes", [])
    }
    expected_object_prefab_ids = {
        record.get("prefab_id")
        for template in templates
        for record in template.get("objects", [])
    }
    expected_full_prefab_ids = (
        expected_ground_prefab_ids | expected_object_prefab_ids
    )
    if full_scene.get("tile_count") != 1600:
        failures.append("Full-scene inventory does not cover all 1600 tiles")
    if full_scene.get("tile_type_count") != 66:
        failures.append("Full-scene inventory does not cover all 66 templates")
    if full_scene.get("expanded_plane_instance_count") != 14400:
        failures.append(
            "Full-scene expanded plane count is not the authoritative 14400"
        )
    if full_scene.get("expanded_object_instance_count") != 48096:
        failures.append(
            "Full-scene expanded object count is not the authoritative 48096"
        )
    if full_scene.get("expanded_instance_count") != 62496:
        failures.append(
            "Full-scene expanded instance count is not the authoritative 62496"
        )
    if set(full_scene.get("ground_prefab_ids", [])) != (
        expected_ground_prefab_ids
    ):
        failures.append(
            "Full-scene ground prefab inventory differs from tile templates"
        )
    if set(full_scene.get("object_prefab_ids", [])) != (
        expected_object_prefab_ids
    ):
        failures.append(
            "Full-scene object prefab inventory differs from tile templates"
        )
    full_source_prefabs = full_scene.get("source_prefabs", {})
    if set(full_source_prefabs) != expected_full_prefab_ids:
        failures.append(
            "Full-scene resolved prefab inventory differs from tile templates"
        )
    if full_scene.get("source_prefab_count") != len(
        expected_full_prefab_ids
    ):
        failures.append("Full-scene source prefab count is inconsistent")
    full_unresolved = full_scene.get("unresolved_prefab_ids", [])
    if full_unresolved:
        failures.append(
            "Full scene has unresolved Unity prefab ids: "
            + ", ".join(full_unresolved)
        )
    for prefab_id, record in full_source_prefabs.items():
        if record.get("resolution_method") != "addressable_guid":
            failures.append(
                f"Full-scene {prefab_id} was not resolved through "
                "MapTile Addressables"
            )
        source_path = record.get("source_prefab_path", "")
        if not source_path or not (resolve(source_path)).exists():
            failures.append(
                f"Full-scene source prefab is missing for {prefab_id}"
            )
    if full_scene.get("runtime_strategy") != (
        "stream tiles around the camera from reusable source templates; "
        "do not flatten all instances into level actors"
    ):
        failures.append("Full-scene runtime strategy is not streaming-safe")

    contract = manifest.get("coordinate_contract", {})
    if not nearly_equal(contract.get("tile_size_unity_units"), 180.0):
        failures.append("Tile size is not 180 Unity units")
    if not nearly_equal(contract.get("display_to_unity_scale"), 6.0):
        failures.append("Display-to-Unity scale is not 6")
    if not nearly_equal(contract.get("ue_units_per_unity_unit"), 500.0):
        failures.append("UE-to-Unity scale is not 500")

    prototype = manifest.get("prototype_window", {})
    focus = prototype.get("focus", {})
    display = focus.get("display_coordinate", {})
    unity = focus.get("unity_coordinate", {})
    tile_coordinate = focus.get("tile_coordinate", {})
    ue_world = focus.get("ue_world_position", {})
    if display != {"x": 1146, "y": 157}:
        failures.append(f"Unexpected focus display coordinate: {display}")
    if not (
        nearly_equal(unity.get("x"), 6876.0)
        and nearly_equal(unity.get("y"), 942.0)
    ):
        failures.append(f"Unexpected focus Unity coordinate: {unity}")
    if tile_coordinate != {"x": 38, "y": 5}:
        failures.append(f"Unexpected focus tile coordinate: {tile_coordinate}")
    if focus.get("focus_anchor_policy") != "display_bucket_lower_bound":
        failures.append("Focus anchor policy is not display_bucket_lower_bound")
    bucket = focus.get("unity_coordinate_bucket", {})
    if bucket.get("min_inclusive") != {"x": 6876.0, "y": 942.0}:
        failures.append(f"Unexpected focus bucket minimum: {bucket}")
    if bucket.get("max_exclusive") != {"x": 6882.0, "y": 948.0}:
        failures.append(f"Unexpected focus bucket maximum: {bucket}")
    if not (
        nearly_equal(ue_world.get("x"), -3438000.0)
        and nearly_equal(ue_world.get("y"), 471000.0)
        and nearly_equal(ue_world.get("z"), 0.0)
    ):
        failures.append(f"Unexpected focus UE world coordinate: {ue_world}")

    camera = manifest.get("map_tactical_camera", {})
    camera_source_path = (
        resolve(sources.get("camera_parameters", {}).get("path", ""))
    )
    source_camera_keys = parse_camera_source(
        camera_source_path,
        failures,
    )
    manifest_camera_keys = manifest.get("camera_keys", [])
    if not camera_records_match(manifest_camera_keys, source_camera_keys):
        failures.append(
            "Manifest camera keys do not match CameraParamLod.csv"
        )
    tactical_source = next(
        (
            record
            for record in source_camera_keys
            if record.get("id") == "map_tactical"
        ),
        {},
    )
    if not tactical_source:
        failures.append("CameraParamLod.csv has no map_tactical row")
    elif not camera_records_match(
        [
            {
                key: camera.get(key)
                for key in (
                    "id",
                    "distance_unity_units",
                    "vertical_fov_degrees",
                    "dxf",
                    "unity_forward",
                )
            }
        ],
        [tactical_source],
    ):
        failures.append(
            "map_tactical camera does not match CameraParamLod.csv"
        )
    vertical_fov = float(camera.get("vertical_fov_degrees", 0.0))
    expected_horizontal_fov = math.degrees(
        2.0
        * math.atan(
            math.tan(math.radians(vertical_fov) * 0.5) * 1.6
        )
    )
    if not nearly_equal(
        camera.get("horizontal_fov_degrees_at_16_10"),
        expected_horizontal_fov,
        tolerance=0.0001,
    ):
        failures.append(
            "map_tactical 16:10 horizontal FOV is not derived from the "
            "source vertical FOV"
        )
    if camera.get("prototype_lod_bucket") != 1:
        failures.append("Unexpected phase-one map_tactical LOD bucket")

    if prototype.get("tile_count") != 9:
        failures.append(f"Expected 9 prototype tiles, found {prototype.get('tile_count')}")
    selected_tiles = {
        (item.get("x"), item.get("y")): item
        for item in prototype.get("tiles", [])
    }
    for coordinate, expected_tile_id in EXPECTED_TILE_IDS.items():
        tile = selected_tiles.get(coordinate)
        if not tile:
            failures.append(f"Missing prototype tile {coordinate[0]},{coordinate[1]}")
            continue
        if tile.get("tile_id") != expected_tile_id:
            failures.append(
                f"Prototype tile {coordinate[0]},{coordinate[1]} is "
                f"{tile.get('tile_id')}, expected {expected_tile_id}"
            )
        if expected_tile_id not in template_ids:
            failures.append(f"Missing selected tile template {expected_tile_id}")
        expected_instances = tile.get("plane_count", 0) + tile.get("object_count", 0)
        if len(tile.get("instances", [])) != expected_instances:
            failures.append(
                f"Prototype tile {coordinate[0]},{coordinate[1]} instance count "
                f"does not match its template"
            )
        for instance in tile.get("instances", []):
            position = instance.get("unity_world_position", {})
            # Unity's mountain-ridge prefabs deliberately use pivots beyond
            # the owning 180-unit tile. Keep a one-tile source envelope while
            # still rejecting corrupt transforms.
            if not (
                (coordinate[0] - 1) * 180.0 - 0.001
                <= position.get("x", -math.inf)
                <= (coordinate[0] + 2) * 180.0 + 0.001
                and (coordinate[1] - 1) * 180.0 - 0.001
                <= position.get("z", -math.inf)
                <= (coordinate[1] + 2) * 180.0 + 0.001
            ):
                failures.append(
                    f"Instance {instance.get('prefab_id')} escaped the source "
                    f"envelope around tile {coordinate[0]},{coordinate[1]}"
                )

    calculated_plane_count = sum(
        tile.get("plane_count", 0) for tile in selected_tiles.values()
    )
    calculated_object_count = sum(
        tile.get("object_count", 0) for tile in selected_tiles.values()
    )
    calculated_instance_count = calculated_plane_count + calculated_object_count
    if prototype.get("plane_instance_count") != calculated_plane_count:
        failures.append("Prototype plane count summary is inconsistent")
    if prototype.get("object_instance_count") != calculated_object_count:
        failures.append("Prototype object count summary is inconsistent")
    if prototype.get("instance_count") != calculated_instance_count:
        failures.append("Prototype total instance count summary is inconsistent")

    unresolved = prototype.get("unresolved_prefab_ids", [])
    if unresolved:
        failures.append(
            "Selected prototype has unresolved Unity prefab ids: "
            + ", ".join(unresolved)
        )
    source_prefabs = prototype.get("source_prefabs", {})
    for prefab_id, record in source_prefabs.items():
        if record.get("resolution_method") != "addressable_guid":
            failures.append(
                f"{prefab_id} was not resolved through MapTile Addressables"
            )
        if not record.get("address_guid"):
            failures.append(f"{prefab_id} has no MapTile Addressables GUID")
        source_path = record.get("source_prefab_path", "")
        if not source_path or not (resolve(source_path)).exists():
            failures.append(f"Missing resolved source prefab for {prefab_id}")

    for tile in selected_tiles.values():
        for instance in tile.get("instances", []):
            policy = instance.get("unity_runtime_transform_policy")
            if instance.get("category") == "adorning":
                if policy != "position_only":
                    failures.append(
                        f"Grove instance {instance.get('prefab_id')} does not "
                        "use Unity's position-only runtime policy"
                    )
                runtime_scale = instance.get("unity_runtime_scale", {})
                if runtime_scale != {"x": 1.0, "y": 1.0, "z": 1.0}:
                    failures.append(
                        f"Grove instance {instance.get('prefab_id')} has "
                        "non-unit runtime scale"
                    )
            elif policy != "position_rotation_scale":
                failures.append(
                    f"Instance {instance.get('prefab_id')} has invalid "
                    f"runtime transform policy {policy}"
                )

    result = {
        "manifest": str(MANIFEST_PATH.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "failure_count": len(failures),
        "failures": failures,
        "map_size_tiles": [
            map_record.get("width_tiles", 0),
            map_record.get("height_tiles", 0),
        ],
        "tile_type_count": template_summary.get("tile_type_count", 0),
        "prototype_tile_count": prototype.get("tile_count", 0),
        "prototype_plane_instance_count": prototype.get(
            "plane_instance_count",
            0,
        ),
        "prototype_object_instance_count": prototype.get(
            "object_instance_count",
            0,
        ),
        "resolved_prefab_count": len(source_prefabs) - len(unresolved),
        "unresolved_prefab_count": len(unresolved),
        "full_scene_expanded_instance_count": full_scene.get(
            "expanded_instance_count",
            0,
        ),
        "full_scene_ground_prefab_count": len(
            full_scene.get("ground_prefab_ids", [])
        ),
        "full_scene_object_prefab_count": len(
            full_scene.get("object_prefab_ids", [])
        ),
        "full_scene_resolved_prefab_count": (
            len(full_source_prefabs) - len(full_unresolved)
        ),
        "full_scene_unresolved_prefab_count": len(full_unresolved),
        "focus_display_coordinate": display,
        "focus_unity_coordinate": unity,
        "focus_tile_coordinate": tile_coordinate,
    }
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    VERIFY_PATH.write_text(
        json.dumps(result, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    if failures:
        print("Rok Unity world-map verification FAILED")
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)

    print(
        "Rok Unity world-map verification passed: "
        f"map={result['map_size_tiles'][0]}x{result['map_size_tiles'][1]} "
        f"templates={result['tile_type_count']} "
        f"prototype_tiles={result['prototype_tile_count']} "
        f"prototype_planes={result['prototype_plane_instance_count']} "
        f"prototype_objects={result['prototype_object_instance_count']} "
        f"resolved_prefabs={result['resolved_prefab_count']} "
        f"full_instances={result['full_scene_expanded_instance_count']} "
        f"full_prefabs={result['full_scene_resolved_prefab_count']}"
    )


if __name__ == "__main__":
    main()
