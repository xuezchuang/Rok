import json
import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)


TARGET_LEVEL = os.environ.get("ROK_UNITY_RUNTIME_BOUNDARY_LEVEL", "/Game/Maps/L_RokUnityRuntimeBoundaryOnly")
VERIFY_OUTPUT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_runtime_boundary_only_verify.json")
SCENE_MANIFEST = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_runtime_boundary_only_scene_manifest.json")
CITY_TAG = "RokUnityRuntimeBoundaryOnly"
UE_UNITS_PER_UNITY_UNIT = 500.0


def actor_label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def actor_folder(actor):
    try:
        return str(actor.get_folder_path())
    except Exception:
        return ""


def actor_tags(actor):
    return [str(tag) for tag in list(actor.tags)]


def has_tag(actor, tag):
    return tag in actor_tags(actor)


def expected_ue_from_unity_position(position):
    return (
        -float(position[0]) * UE_UNITS_PER_UNITY_UNIT,
        float(position[2]) * UE_UNITS_PER_UNITY_UNIT,
        float(position[1]) * UE_UNITS_PER_UNITY_UNIT,
    )


def vector_distance(a, b):
    return ((float(a.x) - b[0]) ** 2 + (float(a.y) - b[1]) ** 2 + (float(a.z) - b[2]) ** 2) ** 0.5


def load_scene_manifest():
    if not os.path.exists(SCENE_MANIFEST):
        return {}
    with open(SCENE_MANIFEST, "r", encoding="utf-8") as handle:
        return json.load(handle)


def verify_scene():
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Missing map: {0}".format(TARGET_LEVEL))

    manifest = load_scene_manifest()
    unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    tagged = [actor for actor in actors if has_tag(actor, CITY_TAG)]
    boundary = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeBoundarySource")]
    ground = [actor for actor in tagged if "Ground" in actor_folder(actor) or has_tag(actor, "RokUnityFreshGround")]
    buildings = [actor for actor in tagged if has_tag(actor, "RokUnityFreshInitBuilding")]
    asset_review = [actor for actor in tagged if has_tag(actor, "RokUnityFreshAssetReview") or has_tag(actor, "UnityBoundaryAssetReview")]
    cameras = [actor for actor in tagged if actor.get_class().get_name() == "CameraActor"]
    player_starts = [actor for actor in tagged if actor.get_class().get_name() == "PlayerStart"]

    boundary_tags = [tag for actor in boundary for tag in actor_tags(actor)]
    has_wall = any("UnityMesh_CityWall_101_1" in tag for tag in boundary_tags)
    has_gate = any("UnityMesh_CityGate_101_1" in tag for tag in boundary_tags)
    tower_actors = [
        actor for actor in boundary
        if any(tag.startswith("UnityMesh_CityTower_101_1_") for tag in actor_tags(actor))
    ]
    tower_mesh_tags = sorted(set(tag for tag in boundary_tags if tag.startswith("UnityMesh_CityTower_101_1_")))
    has_citywall_prefab = any("UnitySourcePrefab_build_6_101_1" in tag for tag in boundary_tags)
    has_guardtower_prefab = any("UnitySourcePrefab_build_8_101_1" in tag for tag in boundary_tags)
    material_counts = {}
    for tag in boundary_tags:
        if tag.startswith("UnityMaterial_"):
            key = tag[len("UnityMaterial_"):]
            material_counts[key] = material_counts.get(key, 0) + 1

    failures = []
    if len(boundary) != 10:
        failures.append("expected exactly 10 runtime boundary actors, found {0}".format(len(boundary)))
    if len(tagged) != len(boundary):
        failures.append("boundary-only map should have no extra tagged actors: tagged={0}, boundary={1}".format(len(tagged), len(boundary)))
    if ground:
        failures.append("ground actors are not allowed in boundary-only map: {0}".format(len(ground)))
    if buildings:
        failures.append("building actors are not allowed in boundary-only map: {0}".format(len(buildings)))
    if asset_review:
        failures.append("asset-review actors are not allowed in boundary-only map: {0}".format(len(asset_review)))
    if cameras:
        failures.append("camera actors are not allowed in boundary-only map: {0}".format(len(cameras)))
    if player_starts:
        failures.append("player starts are not allowed in boundary-only map: {0}".format(len(player_starts)))
    if not has_wall:
        failures.append("missing CityWall_101_1 runtime actor")
    if not has_gate:
        failures.append("missing CityGate_101_1 runtime actor")
    if len(tower_actors) != 8:
        failures.append("expected 8 Unity runtime tower actors from build_8_101_1, found {0}".format(len(tower_actors)))
    expected_material_counts = {
        "wall": 1,
        "gate": 1,
        "tower": 5,
        "tower_2": 3,
    }
    for material_key, expected_count in expected_material_counts.items():
        actual_count = material_counts.get(material_key, 0)
        if actual_count != expected_count:
            failures.append("expected {0} actors with UnityMaterial_{1}, found {2}".format(expected_count, material_key, actual_count))
    if not has_citywall_prefab:
        failures.append("missing source tag for build_6_101_1")
    if not has_guardtower_prefab:
        failures.append("missing source tag for build_8_101_1")

    max_bounds_center_error = 0.0
    boundary_by_label = {actor_label(actor): actor for actor in boundary}
    for record in manifest.get("boundary_actors", []):
        label = record.get("actor", "")
        actor = boundary_by_label.get(label)
        if not actor:
            failures.append("manifest boundary actor is missing from map: {0}".format(label))
            continue
        expected_x, expected_y, expected_z = expected_ue_from_unity_position(
            record.get("unity_bounds_center") or record.get("unity_world_position", [0, 0, 0])
        )
        bounds_origin, _bounds_extent = actor.get_actor_bounds(False)
        tolerance = 0.5
        max_bounds_center_error = max(max_bounds_center_error, vector_distance(bounds_origin, (expected_x, expected_y, expected_z)))
        if (
            abs(float(bounds_origin.x) - expected_x) > tolerance
            or abs(float(bounds_origin.y) - expected_y) > tolerance
            or abs(float(bounds_origin.z) - expected_z) > tolerance
        ):
            failures.append("visible bounds center mismatch for {0}: expected ({1:.2f},{2:.2f},{3:.2f}), got ({4:.2f},{5:.2f},{6:.2f})".format(
                label,
                expected_x,
                expected_y,
                expected_z,
                float(bounds_origin.x),
                float(bounds_origin.y),
                float(bounds_origin.z),
            ))

    folders = {}
    for actor in tagged:
        folder = actor_folder(actor)
        folders[folder] = folders.get(folder, 0) + 1

    summary = {
        "target_level": TARGET_LEVEL,
        "actors": len(actors),
        "tagged": len(tagged),
        "boundary": len(boundary),
        "ground": len(ground),
        "buildings": len(buildings),
        "asset_review": len(asset_review),
        "cameras": len(cameras),
        "player_starts": len(player_starts),
        "has_wall": has_wall,
        "has_gate": has_gate,
        "tower_actor_count": len(tower_actors),
        "tower_unique_mesh_count": len(tower_mesh_tags),
        "material_counts": material_counts,
        "max_bounds_center_error": max_bounds_center_error,
        "has_citywall_prefab": has_citywall_prefab,
        "has_guardtower_prefab": has_guardtower_prefab,
        "folders": folders,
        "failures": failures,
    }
    os.makedirs(os.path.dirname(VERIFY_OUTPUT), exist_ok=True)
    with open(VERIFY_OUTPUT, "w", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)

    line = "UnityRuntimeBoundaryOnlyVerify actors={actors}, tagged={tagged}, boundary={boundary}, towers={tower_actor_count}, unique_tower_meshes={tower_unique_mesh_count}, failures={failure_count}".format(
        failure_count=len(failures),
        **summary
    )
    print(line)
    unreal.log(line)
    if failures:
        raise RuntimeError("; ".join(failures))


if __name__ == "__main__":
    verify_scene()
