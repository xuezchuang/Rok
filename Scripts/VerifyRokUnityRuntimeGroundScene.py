import json
import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)


TARGET_LEVEL = os.environ.get("ROK_UNITY_RUNTIME_GROUND_LEVEL", "/Game/Maps/L_RokUnityRuntimeGround")
VERIFY_OUTPUT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_runtime_ground_verify.json")
SCENE_MANIFEST = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_runtime_ground_scene_manifest.json")
CITY_TAG = "RokUnityRuntimeGround"


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


def vector_distance(a, b):
    return ((float(a.x) - float(b[0])) ** 2 + (float(a.y) - float(b[1])) ** 2 + (float(a.z) - float(b[2])) ** 2) ** 0.5


def load_scene_manifest():
    if not os.path.exists(SCENE_MANIFEST):
        return {}
    with open(SCENE_MANIFEST, "r", encoding="utf-8") as handle:
        return json.load(handle)


def tags_with_prefix(actors, prefix):
    return [tag for actor in actors for tag in actor_tags(actor) if tag.startswith(prefix)]


def count_boundary_materials(boundary):
    material_counts = {}
    for tag in tags_with_prefix(boundary, "UnityMaterial_"):
        key = tag[len("UnityMaterial_"):]
        material_counts[key] = material_counts.get(key, 0) + 1
    return material_counts


def verify_manifest_bounds(records, actors, failures, label):
    by_label = {actor_label(actor): actor for actor in actors}
    max_error = 0.0
    for record in records:
        actor_name = record.get("actor", "")
        actor = by_label.get(actor_name)
        if not actor:
            failures.append("manifest {0} actor is missing from map: {1}".format(label, actor_name))
            continue
        expected = record.get("ue_expected_bounds_center")
        if not expected:
            continue
        bounds_origin, _bounds_extent = actor.get_actor_bounds(False)
        distance = vector_distance(bounds_origin, expected)
        max_error = max(max_error, distance)
        if distance > 0.5:
            failures.append("{0} visible bounds center mismatch for {1}: expected ({2:.2f},{3:.2f},{4:.2f}), got ({5:.2f},{6:.2f},{7:.2f})".format(
                label,
                actor_name,
                float(expected[0]),
                float(expected[1]),
                float(expected[2]),
                float(bounds_origin.x),
                float(bounds_origin.y),
                float(bounds_origin.z),
            ))
    return max_error


def verify_scene():
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Missing map: {0}".format(TARGET_LEVEL))

    manifest = load_scene_manifest()
    unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    tagged = [actor for actor in actors if has_tag(actor, CITY_TAG)]
    boundary = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeBoundarySource")]
    ground = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeGroundSource")]
    buildings = [actor for actor in tagged if has_tag(actor, "RokUnityFreshInitBuilding") or "Building" in actor_folder(actor)]
    roads = [actor for actor in tagged if has_tag(actor, "RokUnityRoad") or "Road" in actor_folder(actor)]
    asset_review = [actor for actor in tagged if has_tag(actor, "RokUnityFreshAssetReview") or has_tag(actor, "UnityBoundaryAssetReview")]
    cameras = [actor for actor in actors if actor.get_class().get_name() == "CameraActor"]
    player_starts = [actor for actor in actors if actor.get_class().get_name() == "PlayerStart"]

    boundary_tags = [tag for actor in boundary for tag in actor_tags(actor)]
    ground_tags = [tag for actor in ground for tag in actor_tags(actor)]
    has_wall = any("UnityMesh_CityWall_101_1" in tag for tag in boundary_tags)
    has_gate = any("UnityMesh_CityGate_101_1" in tag for tag in boundary_tags)
    tower_actors = [
        actor for actor in boundary
        if any(tag.startswith("UnityMesh_CityTower_101_1_") for tag in actor_tags(actor))
    ]
    tower_mesh_tags = sorted(set(tag for tag in boundary_tags if tag.startswith("UnityMesh_CityTower_101_1_")))
    has_city_surface = "UnityMesh_CitySurface_101_1" in ground_tags
    has_city_translucent = "UnityMesh_CityTranslucent_101_1" in ground_tags
    has_city_surface_material = "UnityMaterial_CitySurface_101_1_mat" in ground_tags
    has_city_translucent_material = "UnityMaterial_CityTranslucent_101_1_mat" in ground_tags
    has_baked_texture = any("UnityTextureSource_baked_texture_path" in tag for tag in ground_tags)
    material_counts = count_boundary_materials(boundary)

    failures = []
    if len(boundary) != 10:
        failures.append("expected exactly 10 runtime boundary actors, found {0}".format(len(boundary)))
    if len(ground) != 2:
        failures.append("expected exactly 2 runtime ground actors, found {0}".format(len(ground)))
    if len(tagged) != len(boundary) + len(ground):
        failures.append("runtime ground map should have only boundary and ground tagged actors: tagged={0}, boundary={1}, ground={2}".format(
            len(tagged),
            len(boundary),
            len(ground),
        ))
    if buildings:
        failures.append("building actors are not allowed in runtime ground map: {0}".format(len(buildings)))
    if roads:
        failures.append("road actors are not allowed in runtime ground map: {0}".format(len(roads)))
    if asset_review:
        failures.append("asset-review actors are not allowed in runtime ground map: {0}".format(len(asset_review)))
    if cameras:
        failures.append("camera actors are not allowed in runtime ground map: {0}".format(len(cameras)))
    if player_starts:
        failures.append("player starts are not allowed in runtime ground map: {0}".format(len(player_starts)))
    if not has_wall:
        failures.append("missing CityWall_101_1 runtime actor")
    if not has_gate:
        failures.append("missing CityGate_101_1 runtime actor")
    if len(tower_actors) != 8:
        failures.append("expected 8 Unity runtime tower actors from build_8_101_1, found {0}".format(len(tower_actors)))
    for material_key, expected_count in {"wall": 1, "gate": 1, "tower": 5, "tower_2": 3}.items():
        actual_count = material_counts.get(material_key, 0)
        if actual_count != expected_count:
            failures.append("expected {0} actors with UnityMaterial_{1}, found {2}".format(expected_count, material_key, actual_count))
    if not has_city_surface:
        failures.append("missing CitySurface_101_1 runtime ground actor")
    if not has_city_translucent:
        failures.append("missing CityTranslucent_101_1 runtime ground actor")
    if not has_city_surface_material:
        failures.append("missing CitySurface_101_1_mat material tag")
    if not has_city_translucent_material:
        failures.append("missing CityTranslucent_101_1_mat material tag")
    if not has_baked_texture:
        failures.append("runtime ground actors should use Unity shader-baked texture output")

    max_boundary_bounds_error = verify_manifest_bounds(manifest.get("boundary_actors", []), boundary, failures, "boundary")
    max_ground_bounds_error = verify_manifest_bounds(manifest.get("ground_actors", []), ground, failures, "ground")

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
        "roads": len(roads),
        "asset_review": len(asset_review),
        "cameras": len(cameras),
        "player_starts": len(player_starts),
        "has_wall": has_wall,
        "has_gate": has_gate,
        "tower_actor_count": len(tower_actors),
        "tower_unique_mesh_count": len(tower_mesh_tags),
        "has_city_surface": has_city_surface,
        "has_city_translucent": has_city_translucent,
        "has_city_surface_material": has_city_surface_material,
        "has_city_translucent_material": has_city_translucent_material,
        "has_baked_texture": has_baked_texture,
        "material_counts": material_counts,
        "max_boundary_bounds_center_error": max_boundary_bounds_error,
        "max_ground_bounds_center_error": max_ground_bounds_error,
        "folders": folders,
        "failures": failures,
    }
    os.makedirs(os.path.dirname(VERIFY_OUTPUT), exist_ok=True)
    with open(VERIFY_OUTPUT, "w", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)

    line = "UnityRuntimeGroundVerify actors={actors}, tagged={tagged}, boundary={boundary}, ground={ground}, towers={tower_actor_count}, failures={failure_count}".format(
        failure_count=len(failures),
        **summary
    )
    print(line)
    unreal.log(line)
    if failures:
        raise RuntimeError("; ".join(failures))


if __name__ == "__main__":
    verify_scene()
