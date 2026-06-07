import json
import math
import os

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
TARGET_LEVEL = os.environ.get("ROK_UNITY_RUNTIME_CITY_ALL_BUILDINGS_LEVEL", "/Game/Maps/L_RokUnityRuntimeCityAllBuildings")
MIGRATION_ROOT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration")
ASSET_SUFFIX = os.environ.get("ROK_RUNTIME_CITY_ALL_BUILDINGS_ASSET_SUFFIX", "")
SCENE_MANIFEST = os.environ.get(
    "ROK_UNITY_RUNTIME_CITY_ALL_BUILDINGS_MANIFEST",
    os.path.join(
        MIGRATION_ROOT,
        "unity_runtime_city_all_buildings_scene_manifest{0}.json".format("_" + ASSET_SUFFIX if ASSET_SUFFIX else ""),
    ),
)
VERIFY_OUTPUT = os.environ.get(
    "ROK_UNITY_RUNTIME_CITY_ALL_BUILDINGS_VERIFY",
    os.path.join(
        MIGRATION_ROOT,
        "unity_runtime_city_all_buildings_verify{0}.json".format("_" + ASSET_SUFFIX if ASSET_SUFFIX else ""),
    ),
)
CITY_TAG = "RokUnityRuntimeCityAllBuildings"
EXPECTED_BUILDING_TYPE_COUNTS = {
    "1": 1,
    "7": 1,
    "9": 1,
    "14": 1,
    "15": 1,
    "29": 1,
    "51": 4,
    "52": 8,
    "53": 6,
}
UE_UNITS_PER_UNITY_UNIT = 500.0
CAMERA_VIEW_CENTER = (0.0, 0.0)
CAMERA_VIEW_DISTANCE = 20.0
CAMERA_VERTICAL_FOV = 7.5
CAMERA_DXF = 150.0
CAMERA_FORWARD_UNITY = (0.0, -0.7, 0.7)
CAMERA_ASPECT = 16.0 / 10.0


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


def tags_with_prefix(actors, prefix):
    return [tag for actor in actors for tag in actor_tags(actor) if tag.startswith(prefix)]


def vector_distance(a, b):
    return ((float(a.x) - float(b[0])) ** 2 + (float(a.y) - float(b[1])) ** 2 + (float(a.z) - float(b[2])) ** 2) ** 0.5


def camera_forward_ue():
    x, y, z = CAMERA_FORWARD_UNITY
    length = math.sqrt(x * x + y * y + z * z)
    if length < 0.0001:
        return unreal.Vector(0.0, 0.70710678, -0.70710678)
    return unreal.Vector(x / length, z / length, y / length)


def unity_planar_to_ue(x, z, y=0.0):
    return unreal.Vector(-x * UE_UNITS_PER_UNITY_UNIT, z * UE_UNITS_PER_UNITY_UNIT, y * UE_UNITS_PER_UNITY_UNIT)


def vertical_to_horizontal_fov(vertical_fov, aspect_ratio):
    return 2.0 * math.degrees(math.atan(math.tan(math.radians(vertical_fov) * 0.5) * aspect_ratio))


def expected_camera_location():
    center = unity_planar_to_ue(CAMERA_VIEW_CENTER[0], CAMERA_VIEW_CENTER[1], 0.0)
    return center - camera_forward_ue() * (CAMERA_VIEW_DISTANCE * UE_UNITS_PER_UNITY_UNIT)


def load_scene_manifest():
    if not os.path.exists(SCENE_MANIFEST):
        return {}
    with open(SCENE_MANIFEST, "r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def count_boundary_materials(boundary):
    material_counts = {}
    for tag in tags_with_prefix(boundary, "UnityMaterial_"):
        key = tag[len("UnityMaterial_"):]
        material_counts[key] = material_counts.get(key, 0) + 1
    return material_counts


def count_building_types(buildings):
    counts = {}
    for tag in tags_with_prefix(buildings, "UnityBuildingType_"):
        key = tag[len("UnityBuildingType_"):]
        counts[key] = counts.get(key, 0) + 1
    return counts


def count_tags_by_prefix(actors, prefix):
    counts = {}
    for tag in tags_with_prefix(actors, prefix):
        key = tag[len(prefix):]
        counts[key] = counts.get(key, 0) + 1
    return counts


def dot_vector(a, b):
    return float(a.x) * float(b.x) + float(a.y) * float(b.y) + float(a.z) * float(b.z)


def count_actor_centers_in_camera_view(camera, actors):
    component = camera.get_component_by_class(unreal.CameraComponent)
    if not component:
        return []
    horizontal_fov = math.radians(float(component.get_editor_property("field_of_view")))
    aspect_ratio = max(float(component.get_editor_property("aspect_ratio")), 0.01)
    vertical_fov = 2.0 * math.atan(math.tan(horizontal_fov * 0.5) / aspect_ratio)
    camera_location = camera.get_actor_location()
    forward = camera.get_actor_forward_vector()
    right = camera.get_actor_right_vector()
    up = camera.get_actor_up_vector()
    visible = []
    for actor in actors:
        bounds_origin, _bounds_extent = actor.get_actor_bounds(False)
        to_actor = bounds_origin - camera_location
        depth = dot_vector(to_actor, forward)
        if depth <= 0.0:
            continue
        horizontal_angle = math.atan2(dot_vector(to_actor, right), depth)
        vertical_angle = math.atan2(dot_vector(to_actor, up), depth)
        if abs(horizontal_angle) <= horizontal_fov * 0.5 and abs(vertical_angle) <= vertical_fov * 0.5:
            visible.append(actor_label(actor))
    return visible


def material_blend_mode_counts(actors):
    counts = {}
    for actor in actors:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        material = component.get_material(0) if component else None
        try:
            mode = str(material.get_editor_property("blend_mode")) if material else "None"
        except Exception:
            mode = "Unknown"
        counts[mode] = counts.get(mode, 0) + 1
    return counts


def unique_init_ids(buildings):
    return sorted(set(tag[len("UnityInitId_"):] for tag in tags_with_prefix(buildings, "UnityInitId_")))


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

    unreal.EditorLoadingAndSavingUtils.load_map(TARGET_LEVEL)
    manifest = load_scene_manifest()
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    tagged = [actor for actor in actors if has_tag(actor, CITY_TAG)]
    boundary = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeBoundarySource")]
    ground = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeGroundSource")]
    buildings = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeBuildingSource")]
    extra_buildings = [actor for actor in buildings if has_tag(actor, "RokUnityRuntimeInteriorPreview")]
    initial_buildings = [actor for actor in buildings if not has_tag(actor, "RokUnityRuntimeInteriorPreview")]
    roads = [actor for actor in tagged if has_tag(actor, "RokUnityRoad") or "Road" in actor_folder(actor)]
    asset_review = [actor for actor in tagged if has_tag(actor, "RokUnityFreshAssetReview") or has_tag(actor, "UnityBoundaryAssetReview")]
    cameras = [actor for actor in actors if actor.get_class().get_name() == "CameraActor"]
    player_starts = [actor for actor in actors if actor.get_class().get_name() == "PlayerStart"]
    runtime_cameras = [actor for actor in cameras if has_tag(actor, CITY_TAG) and has_tag(actor, "RokUnityRuntimeCameraSource")]
    runtime_player_starts = [actor for actor in player_starts if has_tag(actor, CITY_TAG) and has_tag(actor, "RokUnityRuntimePlayerStart")]

    boundary_tags = [tag for actor in boundary for tag in actor_tags(actor)]
    ground_tags = [tag for actor in ground for tag in actor_tags(actor)]
    building_tags = [tag for actor in buildings for tag in actor_tags(actor)]
    has_wall = any("UnityMesh_CityWall_101_1" in tag for tag in boundary_tags)
    has_gate = any("UnityMesh_CityGate_101_1" in tag for tag in boundary_tags)
    tower_actors = [
        actor for actor in boundary
        if any(tag.startswith("UnityMesh_CityTower_101_1_") for tag in actor_tags(actor))
    ]
    tower_mesh_tags = sorted(set(tag for tag in boundary_tags if tag.startswith("UnityMesh_CityTower_101_1_")))
    has_city_surface_material = any("UnityMaterial_CitySurface_101_1_mat" in tag for tag in ground_tags)
    has_city_translucent_material = any("UnityMaterial_CityTranslucent_101_1_mat" in tag for tag in ground_tags)
    has_city_surface = has_city_surface_material or any("UnityGroundObject_CitySurface_101_1" in tag for tag in ground_tags)
    has_city_translucent = has_city_translucent_material or any("UnityGroundObject_CityTranslucent_101_1" in tag for tag in ground_tags)
    has_ground_baked_texture = any("UnityTextureSource_baked_texture_path" in tag for tag in ground_tags)
    material_counts = count_boundary_materials(boundary)
    building_type_counts = count_building_types(initial_buildings)
    all_building_type_counts = count_building_types(buildings)
    ground_render_queue_counts = count_tags_by_prefix(ground, "UnityRenderQueue_")
    building_render_queue_counts = count_tags_by_prefix(buildings, "UnityRenderQueue_")
    ground_blend_mode_counts = material_blend_mode_counts(ground)
    building_blend_mode_counts = material_blend_mode_counts(buildings)
    init_ids = unique_init_ids(initial_buildings)
    expected_extra_buildings = len(manifest.get("extra_interior_building_actors", []))
    expected_total_buildings = 24 + expected_extra_buildings

    failures = []
    if len(boundary) != 10:
        failures.append("expected exactly 10 runtime boundary actors, found {0}".format(len(boundary)))
    if len(ground) != 2:
        failures.append("expected exactly 2 runtime ground actors, found {0}".format(len(ground)))
    if len(initial_buildings) != 24:
        failures.append("expected exactly 24 initial runtime city building sprite-card actors, found {0}".format(len(initial_buildings)))
    if len(extra_buildings) != expected_extra_buildings:
        failures.append("expected {0} supplemental interior preview building actors, found {1}".format(expected_extra_buildings, len(extra_buildings)))
    if len(tagged) != len(boundary) + len(ground) + len(buildings) + len(runtime_cameras) + len(runtime_player_starts):
        failures.append("runtime all-buildings map should have only boundary, ground, building, camera, and player-start tagged actors: tagged={0}, boundary={1}, ground={2}, buildings={3}, cameras={4}, player_starts={5}".format(
            len(tagged),
            len(boundary),
            len(ground),
            len(buildings),
            len(runtime_cameras),
            len(runtime_player_starts),
        ))
    if roads:
        failures.append("road actors are not allowed in runtime all-buildings map: {0}".format(len(roads)))
    if asset_review:
        failures.append("asset-review actors are not allowed in runtime all-buildings map: {0}".format(len(asset_review)))
    if len(cameras) != 1 or len(runtime_cameras) != 1:
        failures.append("expected one Unity city_default CameraActor, found cameras={0}, tagged_runtime_cameras={1}".format(len(cameras), len(runtime_cameras)))
    if len(player_starts) != 1 or len(runtime_player_starts) != 1:
        failures.append("expected one Unity city_default PlayerStart, found player_starts={0}, tagged_runtime_player_starts={1}".format(len(player_starts), len(runtime_player_starts)))
    if runtime_cameras:
        camera = runtime_cameras[0]
        expected_location = expected_camera_location()
        actual_location = camera.get_actor_location()
        camera_location_error = vector_distance(actual_location, [expected_location.x, expected_location.y, expected_location.z])
        if camera_location_error > 1.0:
            failures.append("Unity city_default camera location mismatch: expected ({0:.2f},{1:.2f},{2:.2f}), got ({3:.2f},{4:.2f},{5:.2f})".format(
                expected_location.x,
                expected_location.y,
                expected_location.z,
                actual_location.x,
                actual_location.y,
                actual_location.z,
            ))
        component = camera.get_component_by_class(unreal.CameraComponent)
        actual_fov = float(component.get_editor_property("field_of_view")) if component else 0.0
        expected_fov = vertical_to_horizontal_fov(CAMERA_VERTICAL_FOV, CAMERA_ASPECT)
        if abs(actual_fov - expected_fov) > 0.05:
            failures.append("Unity city_default camera FOV mismatch: expected horizontal {0:.3f}, got {1:.3f}".format(expected_fov, actual_fov))
        for required_tag in [
            "UnityCamera_WorldCamera",
            "UnityCameraParam_city_default",
            "UnityCameraDxf_{0:g}".format(CAMERA_DXF),
            "UnityCameraDistance_{0:g}".format(CAMERA_VIEW_DISTANCE),
            "UnityCameraVerticalFov_{0:g}".format(CAMERA_VERTICAL_FOV),
        ]:
            if not has_tag(camera, required_tag):
                failures.append("runtime camera missing source tag {0}".format(required_tag))
    if runtime_player_starts:
        player_start = runtime_player_starts[0]
        expected_location = expected_camera_location()
        player_location_error = vector_distance(player_start.get_actor_location(), [expected_location.x, expected_location.y, expected_location.z])
        if player_location_error > 1.0:
            failures.append("Unity city_default PlayerStart location mismatch: expected camera spawn location, error {0:.2f}".format(player_location_error))
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
    for building_type, expected_count in EXPECTED_BUILDING_TYPE_COUNTS.items():
        actual_count = building_type_counts.get(building_type, 0)
        if actual_count != expected_count:
            failures.append("expected {0} building actors with UnityBuildingType_{1}, found {2}".format(expected_count, building_type, actual_count))
    for disallowed_type in ("6", "8"):
        if building_type_counts.get(disallowed_type, 0):
            failures.append("type {0} should be represented by runtime boundary meshes, not ordinary building sprite cards".format(disallowed_type))
    if len(init_ids) != 24:
        failures.append("expected 24 unique UnityInitId tags, found {0}".format(len(init_ids)))
    if not has_city_surface:
        failures.append("missing CitySurface_101_1 runtime ground actor")
    if not has_city_translucent:
        failures.append("missing CityTranslucent_101_1 runtime ground actor")
    if not has_city_surface_material:
        failures.append("missing CitySurface_101_1_mat material tag")
    if not has_city_translucent_material:
        failures.append("missing CityTranslucent_101_1_mat material tag")
    if not has_ground_baked_texture:
        failures.append("runtime ground actors should use Unity shader-baked texture output")
    if ground_render_queue_counts.get("2991", 0) != 1 or ground_render_queue_counts.get("2992", 0) != 1:
        failures.append("ground should preserve Unity render queues 2991 and 2992, found {0}".format(ground_render_queue_counts))
    if building_render_queue_counts.get("2995", 0) + building_render_queue_counts.get("2996", 0) != expected_total_buildings:
        failures.append("building sprite cards should preserve Unity transparent render queues 2995/2996, found {0}".format(building_render_queue_counts))
    if not any("BLEND_TRANSLUCENT" in key and value == 2 for key, value in ground_blend_mode_counts.items()):
        failures.append("ground materials should be translucent to preserve Unity ZWrite Off, found {0}".format(ground_blend_mode_counts))
    if not any("BLEND_TRANSLUCENT" in key and value == expected_total_buildings for key, value in building_blend_mode_counts.items()):
        failures.append("building materials should be translucent to preserve Unity ZWrite Off, found {0}".format(building_blend_mode_counts))
    for actor in ground + buildings:
        if not has_tag(actor, "UnityShaderZWrite_Off"):
            failures.append("actor should preserve Unity ZWrite Off tag: {0}".format(actor_label(actor)))
    missing_card_tags = [
        actor_label(actor)
        for actor in initial_buildings
        if not has_tag(actor, "UnityRepresentation_runtime_sprite_card")
        or not has_tag(actor, "RokUnityRuntimeSpriteRendererBaked")
        or not has_tag(actor, "UnityTextureSource_BakedMaskSprite")
    ]
    if missing_card_tags:
        failures.append("building actors missing runtime sprite-card/baked-mask tags: {0}".format(", ".join(missing_card_tags[:6])))
    missing_extra_tags = [
        actor_label(actor)
        for actor in extra_buildings
        if not has_tag(actor, "UnityRepresentation_runtime_sprite_card")
        or not has_tag(actor, "RokUnityRuntimeInteriorPreview")
        or not has_tag(actor, "UnityTextureSource_UnityBuildingSpriteMask")
    ]
    if missing_extra_tags:
        failures.append("supplemental interior building actors missing source sprite/mask preview tags: {0}".format(", ".join(missing_extra_tags[:6])))

    max_boundary_bounds_error = verify_manifest_bounds(manifest.get("boundary_actors", []), boundary, failures, "boundary")
    max_ground_bounds_error = verify_manifest_bounds(manifest.get("ground_actors", []), ground, failures, "ground")
    max_building_bounds_error = verify_manifest_bounds(manifest.get("building_actors", []), initial_buildings, failures, "building")
    max_extra_building_bounds_error = verify_manifest_bounds(manifest.get("extra_interior_building_actors", []), extra_buildings, failures, "extra_interior_building")
    camera_visible_actor_centers = []
    camera_visible_building_centers = []
    if runtime_cameras:
        camera_visible_actor_centers = count_actor_centers_in_camera_view(runtime_cameras[0], boundary + ground + buildings)
        camera_visible_building_centers = count_actor_centers_in_camera_view(runtime_cameras[0], buildings)
        if len(camera_visible_building_centers) < 10:
            failures.append("Unity city_default camera should see at least 10 building centers, saw {0}".format(len(camera_visible_building_centers)))
        if not any("Init_005_build_1_101_1" in label for label in camera_visible_building_centers):
            failures.append("Unity city_default camera should see town center Init_005_build_1_101_1")

    folders = {}
    for actor in tagged:
        folder = actor_folder(actor)
        folders[folder] = folders.get(folder, 0) + 1

    summary = {
        "map": TARGET_LEVEL,
        "actors": len(actors),
        "tagged": len(tagged),
        "boundary": len(boundary),
        "ground": len(ground),
        "buildings": len(buildings),
        "initial_buildings": len(initial_buildings),
        "extra_interior_buildings": len(extra_buildings),
        "roads": len(roads),
        "asset_review": len(asset_review),
        "cameras": len(cameras),
        "player_starts": len(player_starts),
        "runtime_cameras": len(runtime_cameras),
        "runtime_player_starts": len(runtime_player_starts),
        "expected_camera": {
            "source": "CameraParamLod.csv city_default",
            "dxf": CAMERA_DXF,
            "view_distance": CAMERA_VIEW_DISTANCE,
            "vertical_fov": CAMERA_VERTICAL_FOV,
            "horizontal_fov": vertical_to_horizontal_fov(CAMERA_VERTICAL_FOV, CAMERA_ASPECT),
            "unity_forward": list(CAMERA_FORWARD_UNITY),
            "ue_location": [
                float(expected_camera_location().x),
                float(expected_camera_location().y),
                float(expected_camera_location().z),
            ],
        },
        "has_wall": has_wall,
        "has_gate": has_gate,
        "tower_actor_count": len(tower_actors),
        "tower_unique_mesh_count": len(tower_mesh_tags),
        "has_city_surface": has_city_surface,
        "has_city_translucent": has_city_translucent,
        "has_city_surface_material": has_city_surface_material,
        "has_city_translucent_material": has_city_translucent_material,
        "has_ground_baked_texture": has_ground_baked_texture,
        "material_counts": material_counts,
        "building_type_counts": building_type_counts,
        "all_building_type_counts": all_building_type_counts,
        "ground_render_queue_counts": ground_render_queue_counts,
        "building_render_queue_counts": building_render_queue_counts,
        "ground_blend_mode_counts": ground_blend_mode_counts,
        "building_blend_mode_counts": building_blend_mode_counts,
        "unique_init_id_count": len(init_ids),
        "max_boundary_bounds_center_error": max_boundary_bounds_error,
        "max_ground_bounds_center_error": max_ground_bounds_error,
        "max_building_bounds_center_error": max_building_bounds_error,
        "max_extra_building_bounds_center_error": max_extra_building_bounds_error,
        "camera_visible_actor_center_count": len(camera_visible_actor_centers),
        "camera_visible_building_center_count": len(camera_visible_building_centers),
        "camera_visible_building_centers_sample": camera_visible_building_centers[:24],
        "folders": folders,
        "failures": failures,
    }
    os.makedirs(os.path.dirname(VERIFY_OUTPUT), exist_ok=True)
    with open(VERIFY_OUTPUT, "w", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)

    line = "UnityRuntimeCityAllBuildingsVerify actors={actors}, tagged={tagged}, boundary={boundary}, ground={ground}, buildings={buildings}, cameras={runtime_cameras}, player_starts={runtime_player_starts}, towers={tower_actor_count}, failures={failure_count}".format(
        failure_count=len(failures),
        **summary
    )
    unreal.log(line)
    if failures:
        raise RuntimeError("; ".join(failures))


if __name__ == "__main__":
    verify_scene()
