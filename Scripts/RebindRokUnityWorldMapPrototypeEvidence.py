import hashlib
import json
import math
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
WORLD_MANIFEST_PATH = MIGRATION_DIR / "unity_world_map.json"
ASSET_MANIFEST_PATH = MIGRATION_DIR / "unity_world_map_assets.json"
GENERATION_PATH = MIGRATION_DIR / "unity_world_map_ue_generation.json"
TARGET_LEVEL = "/Game/Maps/L_RokUnityWorldMapPrototype"
PLAYER_CITY_LEVEL = "/Game/Maps/L_RokUnityGameAppCity"
TARGET_LEVEL_FILE = (
    PROJECT_ROOT
    / "Content"
    / "Maps"
    / "L_RokUnityWorldMapPrototype.umap"
)
TREE_UE_MATERIAL_NAMES = [
    "M_WorldSource_map_tree_nomask",
    "M_WorldSource_map_tree_nomask_1",
]
MOUNTAIN_UE_MATERIAL_NAMES = [
    "M_WorldSource_build_mountain_A",
    "M_WorldSource_build_mountain_F",
]
MOUNTAIN_BODY_MESH_DESTINATION = (
    "/Game/RokUnityWorldMapPrototype/Meshes/MountainBodies"
)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_record(path):
    return {
        "path": str(path.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def load_json(path, expected_schema):
    if not path.exists():
        raise RuntimeError(f"Missing required evidence: {path}")
    payload = json.loads(path.read_text(encoding="utf-8"))
    if payload.get("schema") != expected_schema:
        raise RuntimeError(
            f"Unexpected schema in {path.name}: {payload.get('schema')}"
        )
    return payload


def package_file(object_path):
    package_path = str(object_path).split(".", 1)[0]
    if not package_path.startswith("/Game/"):
        raise RuntimeError(f"Unexpected generated asset path: {object_path}")
    return (
        PROJECT_ROOT
        / "Content"
        / (package_path[len("/Game/") :] + ".uasset")
    )


def payload_fingerprint(asset_manifest):
    payload = {
        "ground": [
            {
                "prefab_id": record["prefab_id"],
                "sha256": record["output"]["sha256"],
            }
            for record in asset_manifest["baked_ground_materials"]
        ],
        "objects": [
            {
                "prefab_id": prefab_id,
                "representation": record["representation"],
                "payload_sha256": (
                    record.get("exported_obj", {}).get("sha256")
                    or record.get("source_mesh", {}).get("sha256")
                    or ""
                ),
            }
            for prefab_id, record in sorted(
                asset_manifest["object_sources"].items()
            )
        ],
        "player_city": asset_manifest["player_city"]["baked_texture"][
            "sha256"
        ],
    }
    serialized = json.dumps(
        payload,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
    ).encode("utf-8")
    return {
        "sha256": hashlib.sha256(serialized).hexdigest(),
        "payload": payload,
    }


def verify_semantic_equivalence(world_manifest, asset_manifest):
    camera = world_manifest["map_tactical_camera"]
    expected_camera = {
        "distance_unity_units": 50.3,
        "vertical_fov_degrees": 30.0,
        "dxf": 1509.0,
    }
    for field, expected in expected_camera.items():
        if not math.isclose(
            float(camera[field]),
            expected,
            rel_tol=0.0,
            abs_tol=1.0e-6,
        ):
            raise RuntimeError(
                f"Source-derived camera {field} changed: {camera[field]}"
            )
    semantics = asset_manifest["source_semantics"]
    tree = semantics["tree"]
    city = semantics["city"]
    expected_semantics = {
        "map_tactical_dxf": (
            semantics["map_tactical_dxf"],
            1509.0,
        ),
        "tree_scale": (
            tree["scale_at_map_tactical"],
            1.0352981926,
        ),
        "city_base_max_scale": (
            city["base_max_scale"],
            7.8,
        ),
        "city_age_size": (
            city["age_size"],
            41.0,
        ),
        "city_max_age_size": (
            city["max_age_size"],
            57.0,
        ),
    }
    for role, (observed, expected) in expected_semantics.items():
        if not math.isclose(
            float(observed),
            expected,
            rel_tol=0.0,
            abs_tol=1.0e-9,
        ):
            raise RuntimeError(
                f"Source-derived {role} changed: {observed}"
            )
    return {
        "camera": expected_camera,
        "tree_scale_at_map_tactical": float(
            tree["scale_at_map_tactical"]
        ),
        "city_base_max_scale": float(city["base_max_scale"]),
        "city_age_size": float(city["age_size"]),
        "city_max_age_size": float(city["max_age_size"]),
        "note": (
            "The source parser changed from hard-coded constants to direct "
            "CSV/YAML derivation; the accepted phase-one visual payload "
            "values remain equivalent. The UE verifier must still compare "
            "every saved actor against the current manifests."
        ),
    }


def main():
    world_manifest = load_json(
        WORLD_MANIFEST_PATH,
        "rok.unity_world_map.v1",
    )
    asset_manifest = load_json(
        ASSET_MANIFEST_PATH,
        "rok.unity_world_map_assets.v1",
    )
    generation = load_json(
        GENERATION_PATH,
        "rok.unity_world_map_ue_generation.v1",
    )
    if generation.get("target_level") != TARGET_LEVEL:
        raise RuntimeError(
            f"Generation targets {generation.get('target_level')}, "
            f"expected {TARGET_LEVEL}"
        )
    if not TARGET_LEVEL_FILE.exists():
        raise RuntimeError(f"Missing saved target map: {TARGET_LEVEL_FILE}")

    expected_terrain = world_manifest["prototype_window"][
        "plane_instance_count"
    ]
    object_sources = asset_manifest["object_sources"]
    expected_rendered = sum(
        1
        for tile in world_manifest["prototype_window"]["tiles"]
        for instance in tile["instances"]
        if instance["kind"] == "object"
        and object_sources[instance["prefab_id"]]["representation"]
        != "empty_source_prefab"
    )
    expected_empty = (
        world_manifest["prototype_window"]["object_instance_count"]
        - expected_rendered
    )
    expected_counts = {
        "terrain_actor_count": expected_terrain,
        "rendered_source_object_actor_count": expected_rendered,
        "omitted_empty_source_record_count": expected_empty,
        "gameplay_actor_count": 1,
    }
    for field, expected in expected_counts.items():
        if generation.get(field) != expected:
            raise RuntimeError(
                f"Existing generation {field} is {generation.get(field)}, "
                f"expected {expected}"
            )

    generated_assets = (
        generation.get("mesh_assets", [])
        + generation.get("texture_assets", [])
        + generation.get("material_assets", [])
    )
    missing_assets = [
        str(path)
        for path in map(package_file, generated_assets)
        if not path.exists()
    ]
    if missing_assets:
        raise RuntimeError(
            "Existing UE assets are missing:\n" + "\n".join(missing_assets)
        )

    previous_rebind = generation.get("map_rebind_evidence", {})
    prior_world_sha = previous_rebind.get(
        "prior_source_world_manifest_sha256",
        generation.get("source_world_manifest", {}).get("sha256"),
    )
    prior_asset_sha = previous_rebind.get(
        "prior_source_asset_manifest_sha256",
        generation.get("source_asset_manifest", {}).get("sha256"),
    )
    semantic_equivalence = verify_semantic_equivalence(
        world_manifest,
        asset_manifest,
    )
    fingerprint = payload_fingerprint(asset_manifest)
    mountain_body_imports = {}
    mountain_body_asset_paths = []
    legacy_combined_mountain_paths = set()
    for prefab_id, record in object_sources.items():
        if record.get("representation") != "exact_source_fbx":
            continue
        geometry = record.get("mountain_geometry_contract", {})
        body_asset = geometry.get("ue_body_mesh_asset")
        if not body_asset:
            raise RuntimeError(
                f"{prefab_id} has no split mountain body asset contract"
            )
        body_file = package_file(body_asset)
        if not body_file.exists():
            raise RuntimeError(
                f"Split mountain body asset is missing: {body_file}"
            )
        mountain_body_asset_paths.append(body_asset)
        legacy_name = f"SM_RokWorld_{prefab_id}"
        legacy_combined_mountain_paths.add(
            "/Game/RokUnityWorldMapPrototype/Meshes/"
            f"{legacy_name}.{legacy_name}"
        )
        mountain_body_imports[prefab_id] = {
            "source_fbx": record["source_mesh"],
            "geometry_contract": geometry,
            "selected_body_asset": body_asset,
            "runtime_asset_paths": [body_asset],
        }

    generation["source_world_manifest"] = source_record(
        WORLD_MANIFEST_PATH
    )
    generation["source_asset_manifest"] = source_record(
        ASSET_MANIFEST_PATH
    )
    generation["target_level_package"] = source_record(
        TARGET_LEVEL_FILE
    )
    generation["mesh_assets"] = sorted(
        {
            *[
                path
                for path in generation.get("mesh_assets", [])
                if path not in legacy_combined_mountain_paths
            ],
            *mountain_body_asset_paths,
        }
    )
    previous_city_source = generation.get("player_city_source", {})
    generation["player_city_source"] = {
        **asset_manifest["player_city"],
        "ue_actor_class": previous_city_source.get("ue_actor_class"),
        "ue_mesh_asset": previous_city_source.get("ue_mesh_asset"),
        "ue_texture_asset": previous_city_source.get("ue_texture_asset"),
        "ue_material_asset": previous_city_source.get(
            "ue_material_asset"
        ),
    }
    generation["runtime_collision_policy"] = {
        "owner": "/Script/Rok.RokWorldMapGameMode",
        "nonselectable_tags": [
            "RokWorldMapTerrain",
            "RokWorldMapSourceObject",
        ],
        "selectable_tag": "RokWorldMapSelectable",
        "nonselectable_collision": "NoCollision",
        "selectable_collision": "QueryOnly",
        "selectable_trace_channel": "Visibility",
        "runtime_log_marker": (
            "Rok world-map collision policy applied:"
        ),
    }
    generation["runtime_tree_lod_policy"] = {
        "owner": "/Script/Rok.RokWorldMapCameraPawn",
        "source_actor_tag": "RokWorldMapSourceObject",
        "tree_actor_tag": "RokWorldMapTree",
        "legacy_tree_material_names": TREE_UE_MATERIAL_NAMES,
        "tree_translucency_sort_priority": 6,
        "bake_dxf": asset_manifest["tree_scale_bake_dxf"],
        "bake_scale": asset_manifest["tree_scale_bake_value"],
        "curve_source_sha256": asset_manifest[
            "source_semantics"
        ]["tree"]["source"]["sha256"],
        "evaluation": "source_curve_scale_over_baked_scale",
        "runtime_log_marker": "Rok world-map tree LOD initialized:",
    }
    generation["runtime_player_city_entry_policy"] = {
        "owner": "/Script/Rok.RokWorldMapCameraPawn",
        "travel_owner": "/Script/Rok.RokWorldMapPlayerController",
        "player_city_tag": "RokWorldMapPlayerCity",
        "trigger": (
            "zoom_in_at_or_below_named_dxf_while_player_city_visible"
        ),
        "threshold_camera_key": "city_bound",
        "target_camera_key": "city_default",
        "transition_seconds": 0.5,
        "target_level": PLAYER_CITY_LEVEL,
        "entry_url_option": "RokCityEntryDxf",
        "runtime_log_markers": [
            "Rok world-map player-city entry started:",
            "Rok player-city level travel requested:",
            "Rok city entry camera initialized:",
        ],
    }
    generation["mountain_render_policy"] = {
        "source_actor_tag": "RokWorldMapMountain",
        "source_contract": asset_manifest["mountain_render_contract"],
        "geometry_policy": "split_fbx_exact_body_only",
        "body_mesh_destination": MOUNTAIN_BODY_MESH_DESTINATION,
        "visible_material_slot_count": 1,
        "excluded_runtime_nodes": [
            "edge",
            "collision",
            "collision_da",
        ],
        "ue_material_names": MOUNTAIN_UE_MATERIAL_NAMES,
        "component_cast_shadow": False,
        "component_affects_distance_field_lighting": False,
        "component_affects_dynamic_indirect_lighting": False,
    }
    generation["mountain_body_imports"] = mountain_body_imports
    generation["asset_build_mode"] = (
        "existing_phase1_map_rebound_to_split_mountain_bodies"
    )
    generation["map_rebind_evidence"] = {
        "prior_source_world_manifest_sha256": prior_world_sha,
        "prior_source_asset_manifest_sha256": prior_asset_sha,
        "source_payload_fingerprint": fingerprint["sha256"],
        "source_payload_record": fingerprint["payload"],
        "semantic_equivalence": semantic_equivalence,
        "acceptance_gate": (
            "Scripts/VerifyRokUnityWorldMapPrototype.py must load the saved "
            "map and compare every generated actor, asset binding, runtime "
            "config, and selection collision against the current manifests."
        ),
    }
    GENERATION_PATH.write_text(
        json.dumps(generation, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    print(
        "Rok Unity world-map existing map evidence rebound: "
        f"terrain={expected_terrain} source_objects={expected_rendered} "
        f"payload={fingerprint['sha256']}"
    )


if __name__ == "__main__":
    main()
