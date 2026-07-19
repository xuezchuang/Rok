import json
import math
import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import VerifyRokUnityRuntimeGroundScene as ground_verify

TARGET_LEVEL = os.environ.get("ROK_UNITY_RUNTIME_CITY_ALL_BUILDINGS_LEVEL", "/Game/Maps/L_RokUnityRuntimeCityAllBuildings")
PROMOTED_GAMEAPP_LEVEL = "/Game/Maps/L_RokUnityGameAppCity"
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
SUPPORT_MANIFEST = os.path.join(MIGRATION_ROOT, "unity_runtime_city_support_layers.json")
SUPPORT_MANIFEST_SCHEMA = "rok.unity_runtime_city_support_layers.v2"
SUPPORT_OBJ_IMPORT_AXIS_COMPENSATION = (
    "UE 5.8 FbxFactory OBJ inverse-Y import compensation: "
    "OBJ input=(desired UE.x,-desired UE.y,desired UE.z)"
)
GATE_FLAGS_MANIFEST = os.path.join(MIGRATION_ROOT, "unity_runtime_city_gate_flags.json")
GATE_FLAGS_MANIFEST_SCHEMA = "rok.unity_runtime_city_gate_flags.v1"
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
UNITY_CITY_LOD_SCALE_START_DXF = 250.0
UNITY_CITY_LOD_SCALE_END_DXF = 1500.0
UNITY_CITY_LOD_SCALE_SLOPE = 0.0056
UNITY_CITY_LOD_MAX_SCALE = 7.8
SOURCE_SPRITE_ORIENTATION_MODE = "fixed_source_sprite_plane"
SOURCE_SPRITE_ACTOR_CLASS = "/Script/Rok.RokCitySpriteActor"
SOURCE_SPRITE_ORIENTATION_TAG = "UnitySpritePlaneFixedSourceOrientation"
SOURCE_SPRITE_NORMAL_TAG = "UnitySpritePlaneNormal_0_0p707_-0p707"
SOURCE_SPRITE_FORWARD_UE = (-1.0, 0.0, 0.0)
SOURCE_SPRITE_RIGHT_UE = (0.0, math.sqrt(0.5), math.sqrt(0.5))
SOURCE_SPRITE_UP_UE = (0.0, math.sqrt(0.5), -math.sqrt(0.5))
SOURCE_SPRITE_AXIS_DOT_MIN = 0.9999


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


def package_path(object_path):
    return str(object_path).split(".", 1)[0].rstrip("/")


def has_tag(actor, tag):
    return tag in actor_tags(actor)


def safe_tag(value):
    return "".join(char if char.isalnum() or char == "_" else "_" for char in str(value)).strip("_")


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


def unity_city_lod_scale(dxf):
    value = float(dxf)
    if value <= UNITY_CITY_LOD_SCALE_START_DXF:
        curve_value = 1.0
    elif value < UNITY_CITY_LOD_SCALE_END_DXF:
        curve_value = 1.0 + (value - UNITY_CITY_LOD_SCALE_START_DXF) * UNITY_CITY_LOD_SCALE_SLOPE
    elif value < 3000.0:
        curve_value = 8.0
    else:
        curve_value = 0.0
    return max(0.0, min(UNITY_CITY_LOD_MAX_SCALE, curve_value))


def load_scene_manifest():
    if not os.path.exists(SCENE_MANIFEST):
        raise RuntimeError("Missing runtime city scene manifest: {0}".format(SCENE_MANIFEST))
    with open(SCENE_MANIFEST, "r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def load_support_manifest():
    if not os.path.exists(SUPPORT_MANIFEST):
        raise RuntimeError("Missing Unity runtime city support-layer manifest: {0}".format(SUPPORT_MANIFEST))
    with open(SUPPORT_MANIFEST, "r", encoding="utf-8-sig") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != SUPPORT_MANIFEST_SCHEMA:
        raise RuntimeError("Unexpected Unity runtime city support-layer schema: {0}".format(manifest.get("schema")))
    return manifest


def load_gate_flags_manifest():
    if not os.path.exists(GATE_FLAGS_MANIFEST):
        raise RuntimeError("Missing Unity runtime CityGate flag manifest: {0}".format(GATE_FLAGS_MANIFEST))
    with open(GATE_FLAGS_MANIFEST, "r", encoding="utf-8-sig") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != GATE_FLAGS_MANIFEST_SCHEMA:
        raise RuntimeError("Unexpected Unity runtime CityGate flag schema: {0}".format(manifest.get("schema")))
    return manifest


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


def normalized_dot_vector_tuple(vector, expected):
    vector_length = math.sqrt(dot_vector(vector, vector))
    expected_length = math.sqrt(sum(float(value) * float(value) for value in expected))
    if vector_length < 0.000001 or expected_length < 0.000001:
        return -1.0
    return sum(
        float(actual) * float(target)
        for actual, target in zip((vector.x, vector.y, vector.z), expected)
    ) / (vector_length * expected_length)


def verify_source_sprite_orientations(buildings):
    mismatches = {}
    minimum_axis_dots = {"forward": 1.0, "right": 1.0, "up": 1.0}
    for actor in buildings:
        axis_dots = {
            "forward": normalized_dot_vector_tuple(actor.get_actor_forward_vector(), SOURCE_SPRITE_FORWARD_UE),
            "right": normalized_dot_vector_tuple(actor.get_actor_right_vector(), SOURCE_SPRITE_RIGHT_UE),
            "up": normalized_dot_vector_tuple(actor.get_actor_up_vector(), SOURCE_SPRITE_UP_UE),
        }
        for axis_name, axis_dot in axis_dots.items():
            minimum_axis_dots[axis_name] = min(minimum_axis_dots[axis_name], axis_dot)
        missing_tags = [
            tag for tag in (
                SOURCE_SPRITE_ORIENTATION_TAG,
                SOURCE_SPRITE_NORMAL_TAG,
                "UnityRepresentation_runtime_sprite_actor_card",
                "UnityRuntimeBuildingCardMode_sprite_actor",
                "UnityGridRootActor",
                "UnityCameraLodDynamic",
                "UnitySpriteGeometry_TextureRectPpu",
            )
            if not has_tag(actor, tag)
        ]
        failed_axes = {
            axis_name: round(axis_dot, 8)
            for axis_name, axis_dot in axis_dots.items()
            if axis_dot < SOURCE_SPRITE_AXIS_DOT_MIN
        }
        actual_class = actor.get_class().get_path_name()
        if missing_tags or failed_axes or actual_class != SOURCE_SPRITE_ACTOR_CLASS:
            mismatches[actor_label(actor)] = {
                "class": actual_class if actual_class != SOURCE_SPRITE_ACTOR_CLASS else "",
                "missing_tags": missing_tags,
                "axis_dots": failed_axes,
            }
    return mismatches, minimum_axis_dots


def expected_grid_anchor_ue(record, container_yaw):
    grid_x = int(record.get("grid_x", 0))
    grid_y = int(record.get("grid_y", 0))
    grid_width = max(1, int(record.get("grid_width", 1)))
    grid_length = max(1, int(record.get("grid_length", 1)))
    unity_x = grid_x * 0.1 + grid_width * 0.05 - 0.05
    unity_z = grid_y * 0.1 + grid_length * 0.05 - 0.05
    yaw = math.radians(float(container_yaw))
    rotated_x = math.cos(yaw) * unity_x + math.sin(yaw) * unity_z
    rotated_z = -math.sin(yaw) * unity_x + math.cos(yaw) * unity_z
    return (
        -rotated_x * UE_UNITS_PER_UNITY_UNIT,
        rotated_z * UE_UNITS_PER_UNITY_UNIT,
        0.01 * UE_UNITS_PER_UNITY_UNIT,
    )


def verify_selection_grid_anchors(manifest_records, buildings, container_yaw):
    actors_by_label = {actor_label(actor): actor for actor in buildings}
    mismatches = {}
    max_live_anchor_error = 0.0
    max_manifest_anchor_error = 0.0
    for record in manifest_records:
        label = str(record.get("actor", ""))
        actor = actors_by_label.get(label)
        if not actor:
            mismatches[label or "<missing-label>"] = {"missing_actor": True}
            continue
        expected_anchor = expected_grid_anchor_ue(record, container_yaw)
        live_anchor_error = vector_distance(actor.get_visual_anchor_world_position(), expected_anchor)
        max_live_anchor_error = max(max_live_anchor_error, live_anchor_error)
        manifest_anchor = record.get("ue_selection_anchor")
        if isinstance(manifest_anchor, list) and len(manifest_anchor) == 3:
            manifest_anchor_error = math.sqrt(sum(
                (float(actual) - float(expected)) ** 2
                for actual, expected in zip(manifest_anchor, expected_anchor)
            ))
        else:
            manifest_anchor_error = float("inf")
        if math.isfinite(manifest_anchor_error):
            max_manifest_anchor_error = max(max_manifest_anchor_error, manifest_anchor_error)
        footprint = actor.get_footprint_size()
        expected_footprint = (
            max(1, int(record.get("grid_width", 1))),
            max(1, int(record.get("grid_length", 1))),
        )
        actual_footprint = (int(footprint.x), int(footprint.y))
        if live_anchor_error > 0.01 or manifest_anchor_error > 0.01 or actual_footprint != expected_footprint:
            mismatches[label] = {
                "live_anchor_error": live_anchor_error,
                "manifest_anchor_error": manifest_anchor_error,
                "actual_footprint": actual_footprint,
                "expected_footprint": expected_footprint,
            }
    return mismatches, max_live_anchor_error, max_manifest_anchor_error


def verify_building_sprite_geometry_and_lod(manifest_records, buildings, container_yaw):
    actors_by_label = {actor_label(actor): actor for actor in buildings}
    mismatches = {}
    max_center_error = 0.0
    max_extent_error = 0.0
    expected_initial_lod_scale = unity_city_lod_scale(CAMERA_DXF)
    for record in manifest_records:
        label = str(record.get("actor", ""))
        actor = actors_by_label.get(label)
        if not actor:
            mismatches[label or "<missing-label>"] = {"missing_actor": True}
            continue

        errors = {}
        expected_anchor = expected_grid_anchor_ue(record, container_yaw)
        actor_location_error = vector_distance(actor.get_actor_location(), expected_anchor)
        if actor_location_error > 0.01:
            errors["actor_root_anchor_error"] = actor_location_error

        expected_center = record.get("ue_expected_bounds_center") or []
        expected_extents = record.get("ue_expected_bounds_extents") or []
        bounds_origin = actor.get_visual_center_world_position()
        bounds_extent = actor.get_visual_bounds_extent()
        if len(expected_center) != 3 or len(expected_extents) != 3:
            errors["missing_expected_geometry"] = True
        else:
            center_error = vector_distance(bounds_origin, expected_center)
            extent_error = max(
                abs(float(bounds_extent.x) - float(expected_extents[0])),
                abs(float(bounds_extent.y) - float(expected_extents[1])),
                abs(float(bounds_extent.z) - float(expected_extents[2])),
            )
            max_center_error = max(max_center_error, center_error)
            max_extent_error = max(max_extent_error, extent_error)
            if center_error > 0.5:
                errors["visible_center_error"] = center_error
            if extent_error > 0.5:
                errors["visible_extent_error"] = extent_error

        base_offset = record.get("ue_lod_base_visual_center_offset") or []
        live_base_offset = actor.get_unity_base_visual_center_offset()
        if len(base_offset) != 3:
            errors["missing_base_visual_center_offset"] = True
        else:
            base_offset_error = vector_distance(live_base_offset, base_offset)
            if base_offset_error > 0.01:
                errors["base_visual_center_offset_error"] = base_offset_error

        source_export_scale = float(record.get("ue_lod_source_export_scale") or 0.0)
        initial_dxf = float(record.get("ue_lod_initial_dxf") or 0.0)
        initial_lod_scale = float(record.get("ue_lod_initial_scale") or 0.0)
        live_lod_scale = float(actor.get_unity_lod_scale())
        if source_export_scale <= 0.0:
            errors["invalid_source_export_lod_scale"] = source_export_scale
        if abs(initial_dxf - CAMERA_DXF) > 0.001:
            errors["initial_dxf"] = initial_dxf
        if abs(initial_lod_scale - expected_initial_lod_scale) > 0.0001:
            errors["manifest_initial_lod_scale"] = initial_lod_scale
        if abs(live_lod_scale - expected_initial_lod_scale) > 0.0001:
            errors["live_initial_lod_scale"] = live_lod_scale

        base_size = record.get("ue_lod_base_sprite_size") or []
        if len(base_size) != 2 or min(float(value) for value in base_size) <= 0.0:
            errors["invalid_base_sprite_size"] = base_size

        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component:
            errors["missing_static_mesh_component"] = True
        else:
            component_scale = component.get_editor_property("relative_scale3d")
            if float(component_scale.y) >= 0.0:
                errors["unity_plane_uv_flip_y"] = float(component_scale.y)

        if errors:
            mismatches[label] = errors

    return mismatches, max_center_error, max_extent_error


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


def verify_extra_alpha_materials(actors):
    valid_count = 0
    errors = {}
    for actor in actors:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        material = component.get_material(0) if component else None
        if not isinstance(material, unreal.Material):
            errors[actor_label(actor)] = ["missing generated base material"]
            continue

        expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
        texture_samples = [item for item in expressions if isinstance(item, unreal.MaterialExpressionTextureSample)]
        baked_textures = []
        for sample in texture_samples:
            texture = sample.get_editor_property("texture")
            if texture and "_bakedalpha." in str(texture.get_path_name()).lower():
                baked_textures.append(texture)
        material_errors = []
        if len(texture_samples) != 1 or len(baked_textures) != 1:
            material_errors.append("expected exactly one Unity-alpha-baked RGBA texture sample")
        else:
            baked_texture = baked_textures[0]
            if not bool(baked_texture.get_editor_property("srgb")):
                material_errors.append("baked building color texture should use sRGB color sampling")
            try:
                if not bool(baked_texture.get_editor_property("has_alpha_channel")):
                    material_errors.append("baked building texture is missing alpha")
            except Exception:
                pass
        if material_errors:
            errors["{0}|{1}".format(actor_label(actor), material.get_path_name())] = material_errors
        else:
            valid_count += 1
    return valid_count, errors


def verify_ground_vertex_alpha_material(actors):
    errors = []
    if len(actors) != 1:
        return ["expected exactly one CityTranslucent_101_1 actor"]
    component = actors[0].get_component_by_class(unreal.StaticMeshComponent)
    material = component.get_material(0) if component else None
    if not isinstance(material, unreal.Material):
        return ["CityTranslucent_101_1 is missing its generated base material"]

    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    texture_samples = [item for item in expressions if isinstance(item, unreal.MaterialExpressionTextureSample)]
    vertex_alpha_textures = []
    for sample in texture_samples:
        texture = sample.get_editor_property("texture")
        if texture and "vertexalpha" in str(texture.get_path_name()).lower():
            vertex_alpha_textures.append(texture)
    if len(texture_samples) != 1 or len(vertex_alpha_textures) != 1:
        errors.append("expected exactly one CityTranslucent VertexAlpha texture sample")
    else:
        texture = vertex_alpha_textures[0]
        try:
            if not bool(texture.get_editor_property("has_alpha_channel")):
                errors.append("CityTranslucent VertexAlpha texture is missing imported alpha")
        except Exception:
            pass
    try:
        if material.get_editor_property("blend_mode") != unreal.BlendMode.BLEND_TRANSLUCENT:
            errors.append("CityTranslucent VertexAlpha material must remain translucent")
    except Exception:
        errors.append("could not inspect CityTranslucent material blend mode")
    # This verifier must remain read-only. RecompileMaterial marks an otherwise
    # saved material package dirty; creation scripts already compile and reject
    # material errors before saving the generated assets.
    return errors


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
        if hasattr(actor, "get_visual_center_world_position"):
            bounds_origin = actor.get_visual_center_world_position()
        else:
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


def verify_support_layers(source_manifest, scene_records, actors, failures):
    source_records = source_manifest.get("records", [])
    source_by_id = {record.get("layer_id"): record for record in source_records}
    scene_by_id = {record.get("layer_id"): record for record in scene_records}
    actors_by_label = {actor_label(actor): actor for actor in actors}
    role_counts = {}
    queue_counts = {}
    max_center_error = 0.0
    max_extent_error = 0.0
    priority_mismatches = {}
    distance_offset_mismatches = {}

    if source_manifest.get("failures"):
        failures.append("Unity support-layer source manifest has failures: {0}".format(source_manifest.get("failures")))
    if source_manifest.get("unclassified_active_renderer_count") != 0:
        failures.append("Unity support-layer source manifest has unclassified active Renderers")
    if source_manifest.get("role_counts") != {"road": 1, "authored_shadow": 1}:
        failures.append("Unity support-layer source manifest must contain one road and one authored shadow")
    if any(
        record.get("obj_import_axis_compensation") != SUPPORT_OBJ_IMPORT_AXIS_COMPENSATION
        for record in source_records
    ):
        failures.append("Unity support-layer source OBJ files are missing UE 5.8 inverse-Y import compensation")
    if len(source_records) != 2 or len(scene_records) != 2 or len(actors) != 2:
        failures.append("expected exactly two Unity support layers: source={0}, scene={1}, actors={2}".format(
            len(source_records), len(scene_records), len(actors)
        ))
    if len(source_by_id) != len(source_records):
        failures.append("Unity support-layer source manifest contains duplicate layer IDs")
    if len(scene_by_id) != len(scene_records):
        failures.append("runtime city scene manifest contains duplicate support-layer IDs")

    missing_ids = sorted(set(source_by_id) - set(scene_by_id))
    orphan_ids = sorted(set(scene_by_id) - set(source_by_id))
    if missing_ids:
        failures.append("runtime city is missing Unity support layers: {0}".format(missing_ids))
    if orphan_ids:
        failures.append("runtime city contains orphan support layers: {0}".format(orphan_ids))

    for layer_id, source in source_by_id.items():
        scene_record = scene_by_id.get(layer_id)
        if not scene_record:
            continue
        actor = actors_by_label.get(scene_record.get("actor", ""))
        if not actor:
            failures.append("support-layer actor is missing from map: {0}".format(scene_record.get("actor", layer_id)))
            continue
        role = source.get("role", "")
        role_counts[role] = role_counts.get(role, 0) + 1
        expected_queue = int(source.get("material_render_queue") or -1)
        queue_counts[str(expected_queue)] = queue_counts.get(str(expected_queue), 0) + 1

        for field in (
            "role",
            "source_mode",
            "source_prefab_sha256",
            "material_guid",
            "material_sha256",
            "texture_guid",
            "source_texture_sha256",
            "obj_sha256",
            "obj_import_axis_compensation",
            "source_rgb_policy",
            "emissive_rgb_multiplier",
        ):
            scene_field = {
                "material_guid": "source_material_guid",
                "material_sha256": "source_material_sha256",
                "texture_guid": "source_texture_guid",
                "source_texture_sha256": "source_texture_sha256",
            }.get(field, field)
            if scene_record.get(scene_field) != source.get(field):
                failures.append("support layer {0} source field mismatch: {1}".format(layer_id, field))
        if source.get("manual_layout") or scene_record.get("manual_layout"):
            failures.append("support layer must not use manual layout: {0}".format(layer_id))

        required_tags = (
            "RokUnityRuntimeCitySupportLayer",
            "UnityLayerId_{0}".format(layer_id),
            "UnitySupportRole_{0}".format(role),
            "UnitySourceMode_unity_active_renderer",
            "UnityRendererActiveInHierarchy",
            "UnityRendererEnabled",
            "UnityMesh_BuiltinPlane10209",
            "UnityMaterialGuid_{0}".format(source.get("material_guid")),
            "UnityTextureGuid_{0}".format(source.get("texture_guid")),
            "UnityRenderQueue_{0}".format(expected_queue),
            "UnityAuthoredRGBA",
            "UnitySourceRgbSemanticsReproduced",
            "UnityNoWhiteToAlpha",
            "UnityNoGeneratedBlur",
            "UnityManualLayout_False",
            "UnityBakedOrientationScaleIdentity",
        )
        for tag in required_tags:
            if not has_tag(actor, tag):
                failures.append("support layer {0} is missing source tag {1}".format(layer_id, tag))

        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component:
            failures.append("support layer {0} has no StaticMeshComponent".format(layer_id))
            continue
        actual_priority = int(component.get_editor_property("translucency_sort_priority"))
        if actual_priority != expected_queue:
            priority_mismatches[layer_id] = {"expected": expected_queue, "actual": actual_priority}
        try:
            distance_offset = float(component.get_editor_property("translucency_sort_distance_offset"))
            if abs(distance_offset) > 0.001:
                distance_offset_mismatches[layer_id] = distance_offset
        except Exception:
            pass
        material = component.get_material(0)
        blend_mode = str(material.get_editor_property("blend_mode")) if material else "None"
        if "BLEND_TRANSLUCENT" not in blend_mode:
            failures.append("support layer {0} must use an authored-RGBA translucent material, found {1}".format(
                layer_id, blend_mode
            ))
        if bool(component.get_editor_property("reverse_culling")):
            failures.append("world-baked support layer must not use reverse culling: {0}".format(layer_id))

        scale = actor.get_actor_scale3d()
        rotation = actor.get_actor_rotation()
        if any(abs(float(value) - 1.0) > 0.001 for value in (scale.x, scale.y, scale.z)):
            failures.append("world-baked support layer must keep identity actor scale: {0}".format(layer_id))
        if any(abs(float(value)) > 0.05 for value in (rotation.pitch, rotation.yaw, rotation.roll)):
            failures.append("world-baked support layer must keep identity actor rotation: {0}".format(layer_id))

        bounds_origin, bounds_extent = actor.get_actor_bounds(False)
        expected_center = source.get("ue_bounds_center")
        expected_extent = source.get("ue_bounds_extents")
        center_error = vector_distance(bounds_origin, expected_center)
        extent_error = vector_distance(bounds_extent, expected_extent)
        max_center_error = max(max_center_error, center_error)
        max_extent_error = max(max_extent_error, extent_error)
        if center_error > 0.5:
            failures.append("support layer {0} bounds-center mismatch: {1:.3f}".format(layer_id, center_error))
        if extent_error > 1.0:
            failures.append("support layer {0} bounds-extent mismatch: {1:.3f}".format(layer_id, extent_error))

    if priority_mismatches:
        failures.append("support-layer translucency priorities do not match Unity queues: {0}".format(priority_mismatches))
    if distance_offset_mismatches:
        failures.append("support-layer TranslucencySortDistanceOffset must remain zero: {0}".format(distance_offset_mismatches))

    return {
        "role_counts": role_counts,
        "queue_counts": queue_counts,
        "missing_layer_ids": missing_ids,
        "orphan_layer_ids": orphan_ids,
        "priority_mismatches": priority_mismatches,
        "distance_offset_mismatches": distance_offset_mismatches,
        "max_bounds_center_error": max_center_error,
        "max_bounds_extent_error": max_extent_error,
    }


def verify_gate_flags(source_manifest, scene_records, actors, failures):
    source_records = source_manifest.get("records", [])
    source_by_id = {record.get("flag_id"): record for record in source_records}
    scene_by_id = {record.get("flag_id"): record for record in scene_records}
    actors_by_label = {actor_label(actor): actor for actor in actors}
    missing_ids = sorted(set(source_by_id) - set(scene_by_id))
    orphan_ids = sorted(set(scene_by_id) - set(source_by_id))
    priority_mismatches = {}
    distance_offset_mismatches = {}
    max_center_error = 0.0
    max_extent_error = 0.0
    centers = []

    if source_manifest.get("failures"):
        failures.append("Unity CityGate flag source manifest has failures: {0}".format(source_manifest.get("failures")))
    if int(source_manifest.get("active_drawable_flag_renderer_count") or 0) != 2:
        failures.append("Unity CityGate flag source manifest must contain two drawable active SpriteRenderers")
    closure = source_manifest.get("source_active_renderer_closure", {})
    if closure.get("build8_drawable_mesh_renderers") != 9 or closure.get("build8_drawable_flag_sprite_renderers") != 2 or closure.get("build8_drawable_renderer_total") != 11:
        failures.append("build_8 drawable Renderer closure must remain 9 meshes plus 2 flags")
    if closure.get("source_unresolved_active_mesh_renderers") != 2:
        failures.append("build_8 unresolved active source MeshRenderer evidence drifted")
    if len(source_records) != 2 or len(scene_records) != 2 or len(actors) != 2:
        failures.append("expected exactly two Unity CityGate flags: source={0}, scene={1}, actors={2}".format(
            len(source_records), len(scene_records), len(actors)
        ))
    if len(source_by_id) != len(source_records):
        failures.append("Unity CityGate flag source manifest contains duplicate flag IDs")
    if len(scene_by_id) != len(scene_records):
        failures.append("runtime city scene manifest contains duplicate CityGate flag IDs")
    if missing_ids:
        failures.append("runtime city is missing Unity CityGate flags: {0}".format(missing_ids))
    if orphan_ids:
        failures.append("runtime city contains orphan CityGate flags: {0}".format(orphan_ids))

    field_map = {
        "source_key": "source_key",
        "source_mode": "source_mode",
        "source_prefab_guid": "source_prefab_guid",
        "source_prefab_sha256": "source_prefab_sha256",
        "sprite_renderer_file_id": "sprite_renderer_file_id",
        "mask_sprite_component_file_id": "mask_sprite_component_file_id",
        "sprite_guid": "source_sprite_guid",
        "sprite_sha256": "source_sprite_sha256",
        "mask_guid": "source_mask_guid",
        "mask_sha256": "source_mask_sha256",
        "material_guid": "source_material_guid",
        "material_sha256": "source_material_sha256",
        "shader_guid": "source_shader_guid",
        "shader_sha256": "source_shader_sha256",
        "runtime_material_color_1": "runtime_material_color_1",
        "main_city_sprite_color": "main_city_sprite_color",
        "baked_texture_sha256": "baked_texture_sha256",
        "obj_sha256": "obj_sha256",
        "representation": "representation",
        "obj_import_axis_compensation": "obj_import_axis_compensation",
    }
    for flag_id, source_record in source_by_id.items():
        scene_record = scene_by_id.get(flag_id)
        if not scene_record:
            continue
        actor = actors_by_label.get(scene_record.get("actor", ""))
        if not actor:
            failures.append("CityGate flag actor is missing from map: {0}".format(scene_record.get("actor", flag_id)))
            continue
        for source_field, scene_field in field_map.items():
            if source_record.get(source_field) != scene_record.get(scene_field):
                failures.append("CityGate flag {0} source field mismatch: {1}".format(flag_id, source_field))
        if source_record.get("manual_layout") or scene_record.get("manual_layout"):
            failures.append("CityGate flag must not use manual layout: {0}".format(flag_id))

        required_tags = (
            "RokUnityRuntimeCityGateFlag",
            "RokUnityRuntimeObjBaked",
            "UnityFlagId_{0}".format(safe_tag(flag_id)),
            "UnitySourceKey_{0}".format(safe_tag(source_record.get("source_key"))),
            "UnitySourceMode_unity_active_renderer",
            "UnityRenderer_SpriteRenderer",
            "UnityRendererActiveInHierarchy",
            "UnityRendererEnabled",
            "UnityImportedTightSpriteMesh",
            "UnityObjImportAxisCompensation_FbxFactoryInverseY",
            "UnitySpriteRendererFileId_{0}".format(source_record.get("sprite_renderer_file_id")),
            "UnityMaskSpriteComponentFileId_{0}".format(source_record.get("mask_sprite_component_file_id")),
            "UnitySpriteGuid_{0}".format(source_record.get("sprite_guid")),
            "UnityMaskGuid_{0}".format(source_record.get("mask_guid")),
            "UnityMaterialGuid_{0}".format(source_record.get("material_guid")),
            "UnityShaderGuid_{0}".format(source_record.get("shader_guid")),
            "UnityRuntimeColor_RSBuildGreen_62_125_195",
            "UnityRuntimeMask_MaskSpritePropertyBlock",
            "UnityRuntimeTexture_GammaShaderBakedRGBA",
            "UnityOpacity_ColorA_SaturateMaskR2_CityTransparency",
            "UnityRenderQueue_2995",
            "UnityShaderZWrite_Off",
            "UnityManualLayout_False",
            "UnityBakedOrientationScaleIdentity",
            "UnityRootRotationOverride_Identity",
            "UnityContainerYaw_Neg45",
        )
        for tag in required_tags:
            if not has_tag(actor, tag):
                failures.append("CityGate flag {0} is missing source tag {1}".format(flag_id, tag))

        if actor.get_class().get_name() != "StaticMeshActor":
            failures.append("CityGate flag must be an identity world-baked StaticMeshActor, found {0}".format(
                actor.get_class().get_name()
            ))
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component:
            failures.append("CityGate flag {0} has no StaticMeshComponent".format(flag_id))
            continue
        static_mesh = component.get_editor_property("static_mesh")
        mesh_path = static_mesh.get_path_name() if static_mesh else ""
        if mesh_path.startswith("/Engine/BasicShapes/Plane"):
            failures.append("CityGate flag must use the Unity tight SpriteMesh, not Engine BasicShapes Plane")
        actual_priority = int(component.get_editor_property("translucency_sort_priority"))
        if actual_priority != 2995:
            priority_mismatches[flag_id] = {"expected": 2995, "actual": actual_priority}
        try:
            distance_offset = float(component.get_editor_property("translucency_sort_distance_offset"))
            if abs(distance_offset) > 0.001:
                distance_offset_mismatches[flag_id] = distance_offset
        except Exception:
            pass
        if bool(component.get_editor_property("reverse_culling")):
            failures.append("world-baked CityGate flag must not use reverse culling: {0}".format(flag_id))
        material = component.get_material(0)
        blend_mode = str(material.get_editor_property("blend_mode")) if material else "None"
        if "BLEND_TRANSLUCENT" not in blend_mode:
            failures.append("CityGate flag {0} material must be translucent, found {1}".format(flag_id, blend_mode))
        if material and not bool(material.get_editor_property("two_sided")):
            failures.append("CityGate flag {0} material must preserve Unity Cull Off as two-sided".format(flag_id))
        if material:
            try:
                if bool(material.get_editor_property("disable_depth_test")):
                    failures.append("CityGate flag {0} must keep depth testing enabled".format(flag_id))
            except Exception:
                pass

        scale = actor.get_actor_scale3d()
        rotation = actor.get_actor_rotation()
        location = actor.get_actor_location()
        if any(abs(float(value) - 1.0) > 0.001 for value in (scale.x, scale.y, scale.z)):
            failures.append("world-baked CityGate flag must keep identity actor scale: {0}".format(flag_id))
        if any(abs(float(value)) > 0.05 for value in (rotation.pitch, rotation.yaw, rotation.roll)):
            failures.append("world-baked CityGate flag must keep identity actor rotation: {0}".format(flag_id))
        if math.sqrt(float(location.x) ** 2 + float(location.y) ** 2 + float(location.z) ** 2) > 0.5:
            failures.append("world-baked CityGate flag requires a large actor translation; OBJ import axes are likely wrong: {0} at ({1:.2f},{2:.2f},{3:.2f})".format(
                flag_id,
                float(location.x),
                float(location.y),
                float(location.z),
            ))

        bounds_origin, bounds_extent = actor.get_actor_bounds(False)
        centers.append(bounds_origin)
        expected_center = source_record.get("ue_bounds_center")
        expected_extent = source_record.get("ue_bounds_extents")
        center_error = vector_distance(bounds_origin, expected_center)
        extent_error = vector_distance(bounds_extent, expected_extent)
        max_center_error = max(max_center_error, center_error)
        max_extent_error = max(max_extent_error, extent_error)
        if center_error > 0.5:
            failures.append("CityGate flag {0} bounds-center mismatch: {1:.3f}".format(flag_id, center_error))
        if extent_error > 1.0:
            failures.append("CityGate flag {0} bounds-extent mismatch: {1:.3f}".format(flag_id, extent_error))

    if len(centers) == 2 and (centers[0] - centers[1]).length() < 100.0:
        failures.append("the two Unity CityGate flags overlap instead of preserving distinct source transforms")
    if priority_mismatches:
        failures.append("CityGate flag translucency priorities must both remain 2995: {0}".format(priority_mismatches))
    if distance_offset_mismatches:
        failures.append("CityGate flag TranslucencySortDistanceOffset must remain zero: {0}".format(distance_offset_mismatches))
    return {
        "missing_flag_ids": missing_ids,
        "orphan_flag_ids": orphan_ids,
        "priority_mismatches": priority_mismatches,
        "distance_offset_mismatches": distance_offset_mismatches,
        "max_bounds_center_error": max_center_error,
        "max_bounds_extent_error": max_extent_error,
    }


def verify_building_sort_priorities(records, actors):
    by_label = {actor_label(actor): actor for actor in actors}
    mismatches = {}
    priority_counts = {}
    distance_offset_mismatches = {}
    for record in records:
        actor_name = record.get("actor", "")
        actor = by_label.get(actor_name)
        if not actor:
            mismatches[actor_name or "<missing-label>"] = "actor missing from map"
            continue
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component:
            mismatches[actor_name] = "missing StaticMeshComponent"
            continue
        expected = int(record.get("material_render_queue", -1))
        actual = int(component.get_editor_property("translucency_sort_priority"))
        priority_counts[str(actual)] = priority_counts.get(str(actual), 0) + 1
        if actual != expected or not has_tag(actor, "UnityRenderQueue_{0}".format(expected)):
            mismatches[actor_name] = {
                "expected_material_render_queue": expected,
                "actual_translucency_sort_priority": actual,
                "has_matching_source_tag": has_tag(actor, "UnityRenderQueue_{0}".format(expected)),
            }
        try:
            distance_offset = float(component.get_editor_property("translucency_sort_distance_offset"))
            if abs(distance_offset) > 0.001:
                distance_offset_mismatches[actor_name] = distance_offset
        except Exception:
            pass
    return priority_counts, mismatches, distance_offset_mismatches


def unresolved_visual_records(records):
    unresolved = []
    for record in records:
        missing = []
        if record.get("visual_resolution") != "unity_prefab_sprite_renderer":
            missing.append("visual_resolution")
        for field in ("prefab_path", "renderer_path", "sprite_path", "sprite_guid", "mask_path", "mask_guid"):
            if not record.get(field):
                missing.append(field)
        if record.get("manual_layout"):
            missing.append("manual_layout")
        if missing:
            unresolved.append({"actor": record.get("actor"), "missing_or_invalid": missing})
    return unresolved


def verify_scene():
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Missing map: {0}".format(TARGET_LEVEL))

    current_world = unreal.EditorLevelLibrary.get_editor_world()
    current_level = package_path(current_world.get_path_name()) if current_world else ""
    if current_level != package_path(TARGET_LEVEL):
        unreal.EditorLoadingAndSavingUtils.load_map(TARGET_LEVEL)
    manifest = load_scene_manifest()
    support_source_manifest = load_support_manifest()
    gate_flags_source_manifest = load_gate_flags_manifest()
    ground_source_manifest = ground_verify.load_ground_source_manifest()
    failures = []
    if manifest.get("schema") != "rok.unity_runtime_city_all_buildings_scene.v1":
        failures.append("unexpected or missing runtime city scene manifest schema")
    if package_path(manifest.get("target_level", "")) != package_path(TARGET_LEVEL):
        failures.append("runtime city scene manifest target does not match verifier target")
    if str(manifest.get("asset_suffix", "")) != str(ASSET_SUFFIX):
        failures.append("runtime city scene manifest asset suffix does not match verifier asset suffix")
    if (
        manifest.get("building_card_mode") == "sprite_actor"
        and manifest.get("building_sprite_orientation") != SOURCE_SPRITE_ORIENTATION_MODE
    ):
        failures.append("runtime city scene manifest must require fixed Unity source sprite-plane orientation")
    runtime_rules = manifest.get("unity_runtime_rules", {})
    if float(runtime_rules.get("source_export_runtime_lod_scale") or 0.0) <= 0.0:
        failures.append("runtime city scene manifest must record the Unity source-export LOD scale")
    if abs(float(runtime_rules.get("initial_camera_lod_scale") or 0.0) - unity_city_lod_scale(CAMERA_DXF)) > 0.0001:
        failures.append("runtime city scene manifest initial building LOD must match the city_default dxf")
    if "LevelDetailCastle.m_scale_curve" not in str(runtime_rules.get("runtime_lod_scale", "")):
        failures.append("runtime city scene manifest must use the Unity LevelDetailCastle camera LOD curve")
    if manifest.get("source_initial_building_count") != 26:
        failures.append("runtime city scene manifest must originate from all 26 Unity InitBuilding rows")
    if manifest.get("source_skipped_boundary_building_count") != 2:
        failures.append("runtime city scene manifest must account for 2 boundary InitBuilding rows")
    if manifest.get("source_runtime_sprite_record_count") != 24:
        failures.append("runtime city scene manifest must contain 24 non-boundary SpriteRenderer records")
    if TARGET_LEVEL == PROMOTED_GAMEAPP_LEVEL and manifest.get("manual_layout_actor_count") != 0:
        failures.append("promoted GameApp city must not contain manual-layout preview actors")

    manifest_building_records = manifest.get("building_actors", [])
    manifest_support_records = manifest.get("support_layer_actors", [])
    manifest_gate_flag_records = manifest.get("city_gate_flag_actors", [])
    manifest_extra_records = manifest.get("extra_interior_building_actors", [])
    if len(manifest_building_records) != 24:
        failures.append("runtime city scene manifest must contain exactly 24 initial building actor records")
    if manifest.get("support_layers_spawned") != 2 or len(manifest_support_records) != 2:
        failures.append("runtime city scene manifest must contain exactly 2 Unity support-layer actor records")
    if manifest.get("city_gate_flags_spawned") != 2 or len(manifest_gate_flag_records) != 2:
        failures.append("runtime city scene manifest must contain exactly 2 Unity CityGate flag actor records")
    if os.path.normcase(os.path.abspath(manifest.get("unity_runtime_city_support_manifest", ""))) != os.path.normcase(os.path.abspath(SUPPORT_MANIFEST)):
        failures.append("runtime city scene manifest points at the wrong Unity support-layer source manifest")
    if os.path.normcase(os.path.abspath(manifest.get("unity_runtime_city_gate_flags_manifest", ""))) != os.path.normcase(os.path.abspath(GATE_FLAGS_MANIFEST)):
        failures.append("runtime city scene manifest points at the wrong Unity CityGate flag source manifest")
    if os.path.normcase(os.path.abspath(manifest.get("unity_runtime_ground_obj_manifest", ""))) != os.path.normcase(os.path.abspath(ground_verify.GROUND_SOURCE_MANIFEST)):
        failures.append("runtime city scene manifest points at the wrong Unity ground source manifest")
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    tagged = [actor for actor in actors if has_tag(actor, CITY_TAG)]
    boundary = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeBoundarySource")]
    ground = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeGroundSource")]
    support_layers = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeCitySupportLayer")]
    gate_flags = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeCityGateFlag")]
    buildings = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeBuildingSource")]
    extra_buildings = [actor for actor in buildings if has_tag(actor, "RokUnityRuntimeInteriorPreview")]
    initial_buildings = [actor for actor in buildings if has_tag(actor, "RokUnityConfigInitBuilding")]
    unclassified_buildings = [
        actor for actor in buildings
        if not has_tag(actor, "RokUnityConfigInitBuilding")
        and not has_tag(actor, "RokUnityRuntimeInteriorPreview")
    ]
    overlapping_building_classes = [
        actor for actor in buildings
        if has_tag(actor, "RokUnityConfigInitBuilding")
        and has_tag(actor, "RokUnityRuntimeInteriorPreview")
    ]
    roads = [actor for actor in support_layers if has_tag(actor, "UnitySupportRole_road")]
    authored_shadows = [actor for actor in support_layers if has_tag(actor, "UnitySupportRole_authored_shadow")]
    manual_roads = [
        actor for actor in tagged
        if actor not in support_layers and (has_tag(actor, "RokUnityRoad") or "Road" in actor_folder(actor))
    ]
    forbidden_legacy_visuals = [
        actor for actor in tagged
        if has_tag(actor, "RokUnityCityVisualLayer")
        or any(tag.startswith("UnityBuildingVisualLayer_") for tag in actor_tags(actor))
        or "CityBuildingVisualLayers" in actor_folder(actor)
        or "CityBuildingBodyGroundLayers" in actor_folder(actor)
    ]
    asset_review = [actor for actor in tagged if has_tag(actor, "RokUnityFreshAssetReview") or has_tag(actor, "UnityBoundaryAssetReview")]
    cameras = [actor for actor in actors if actor.get_class().get_name() == "CameraActor"]
    player_starts = [actor for actor in actors if actor.get_class().get_name() == "PlayerStart"]
    runtime_cameras = [actor for actor in cameras if has_tag(actor, CITY_TAG) and has_tag(actor, "RokUnityRuntimeCameraSource")]
    runtime_player_starts = [actor for actor in player_starts if has_tag(actor, CITY_TAG) and has_tag(actor, "RokUnityRuntimePlayerStart")]
    ground_contract = ground_verify.verify_ground_source_contract(
        ground_source_manifest,
        manifest.get("ground_actors", []),
        ground,
        failures,
    )

    boundary_tags = [tag for actor in boundary for tag in actor_tags(actor)]
    ground_tags = [tag for actor in ground for tag in actor_tags(actor)]
    support_tags = [tag for actor in support_layers for tag in actor_tags(actor)]
    gate_flag_tags = [tag for actor in gate_flags for tag in actor_tags(actor)]
    building_tags = [tag for actor in buildings for tag in actor_tags(actor)]
    has_wall = any("UnityMesh_CityWall_101_1" in tag for tag in boundary_tags)
    has_gate = any("UnityMesh_CityGate_101_1" in tag for tag in boundary_tags)
    gate_actors = [actor for actor in boundary if any(tag.startswith("UnityMesh_CityGate_101_1") for tag in actor_tags(actor))]
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
    city_surface_actors = [
        actor for actor in ground if "UnityMesh_CitySurface_101_1" in actor_tags(actor)
    ]
    city_translucent_actors = [
        actor for actor in ground if "UnityMesh_CityTranslucent_101_1" in actor_tags(actor)
    ]
    has_ground_vertex_alpha_texture = any(
        "UnityTextureSource_vertex_alpha_baked_texture_path" in tag for tag in ground_tags
    )
    has_ground_vertex_alpha_evidence = any("UnityGroundVertexAlphaBaked" in tag for tag in ground_tags)
    has_ground_vertex_alpha_mapping = any(
        "UnityGroundVertexAlphaMapping_ByPolygonVertex_IndexToDirect" in tag for tag in ground_tags
    )
    city_surface_sort_priority = None
    city_translucent_sort_priority = None
    if len(city_surface_actors) == 1:
        component = city_surface_actors[0].get_component_by_class(unreal.StaticMeshComponent)
        if component:
            city_surface_sort_priority = int(component.get_editor_property("translucency_sort_priority"))
    if len(city_translucent_actors) == 1:
        component = city_translucent_actors[0].get_component_by_class(unreal.StaticMeshComponent)
        if component:
            city_translucent_sort_priority = int(component.get_editor_property("translucency_sort_priority"))
    material_counts = count_boundary_materials(boundary)
    building_type_counts = count_building_types(initial_buildings)
    all_building_type_counts = count_building_types(buildings)
    ground_render_queue_counts = count_tags_by_prefix(ground, "UnityRenderQueue_")
    support_render_queue_counts = count_tags_by_prefix(support_layers, "UnityRenderQueue_")
    gate_flag_render_queue_counts = count_tags_by_prefix(gate_flags, "UnityRenderQueue_")
    building_render_queue_counts = count_tags_by_prefix(buildings, "UnityRenderQueue_")
    ground_blend_mode_counts = material_blend_mode_counts(ground)
    support_blend_mode_counts = material_blend_mode_counts(support_layers)
    gate_flag_blend_mode_counts = material_blend_mode_counts(gate_flags)
    building_blend_mode_counts = material_blend_mode_counts(buildings)
    extra_alpha_material_count, extra_alpha_material_errors = verify_extra_alpha_materials(extra_buildings)
    ground_vertex_alpha_material_errors = verify_ground_vertex_alpha_material(city_translucent_actors)
    init_ids = unique_init_ids(initial_buildings)
    expected_extra_buildings = len(manifest_extra_records)
    expected_total_buildings = 24 + expected_extra_buildings
    building_sort_priority_counts, building_sort_priority_mismatches, building_sort_distance_offset_mismatches = verify_building_sort_priorities(
        manifest_building_records,
        initial_buildings,
    )
    if manifest.get("building_card_mode") == "sprite_actor":
        source_sprite_orientation_mismatches, source_sprite_minimum_axis_dots = verify_source_sprite_orientations(
            initial_buildings
        )
    else:
        source_sprite_orientation_mismatches = {}
        source_sprite_minimum_axis_dots = {}
    selection_grid_anchor_mismatches, max_live_grid_anchor_error, max_manifest_grid_anchor_error = verify_selection_grid_anchors(
        manifest_building_records,
        initial_buildings,
        manifest.get("unity_runtime_rules", {}).get("city_container_yaw", -45.0),
    )
    building_sprite_geometry_mismatches, max_building_sprite_center_error, max_building_sprite_extent_error = verify_building_sprite_geometry_and_lod(
        manifest_building_records,
        initial_buildings,
        manifest.get("unity_runtime_rules", {}).get("city_container_yaw", -45.0),
    )
    extra_sort_priority_counts, extra_sort_priority_mismatches, extra_sort_distance_offset_mismatches = verify_building_sort_priorities(
        manifest_extra_records,
        extra_buildings,
    )
    rendered_unresolved_visuals = unresolved_visual_records(manifest_building_records)
    support_verify = verify_support_layers(
        support_source_manifest,
        manifest_support_records,
        support_layers,
        failures,
    )
    gate_flags_verify = verify_gate_flags(
        gate_flags_source_manifest,
        manifest_gate_flag_records,
        gate_flags,
        failures,
    )
    try:
        translucent_sort_policy = int(unreal.SystemLibrary.get_console_variable_int_value("r.TranslucentSortPolicy"))
    except Exception:
        translucent_sort_policy = None

    if len(boundary) != 10:
        failures.append("expected exactly 10 runtime boundary actors, found {0}".format(len(boundary)))
    if len(ground) != 2:
        failures.append("expected exactly 2 runtime ground actors, found {0}".format(len(ground)))
    if len(support_layers) != 2:
        failures.append("expected exactly 2 runtime Unity support-layer actors, found {0}".format(len(support_layers)))
    if len(gate_flags) != 2:
        failures.append("expected exactly 2 runtime Unity CityGate flag actors, found {0}".format(len(gate_flags)))
    if len(roads) != 1 or len(authored_shadows) != 1:
        failures.append("expected one active Unity source road and one authored wall shadow, found roads={0}, shadows={1}".format(
            len(roads), len(authored_shadows)
        ))
    if len(initial_buildings) != 24:
        failures.append("expected exactly 24 initial runtime city building sprite-card actors, found {0}".format(len(initial_buildings)))
    if unclassified_buildings:
        failures.append("runtime building actors must be classified as InitBuilding or InteriorPreview: {0}".format(
            [actor_label(actor) for actor in unclassified_buildings]
        ))
    if overlapping_building_classes:
        failures.append("runtime building actors must not be both InitBuilding and InteriorPreview: {0}".format(
            [actor_label(actor) for actor in overlapping_building_classes]
        ))
    if source_sprite_orientation_mismatches:
        failures.append("initial building cards must preserve the fixed Unity source plane orientation: {0}".format(
            source_sprite_orientation_mismatches
        ))
    if selection_grid_anchor_mismatches:
        failures.append("initial building selection anchors/footprints must match Unity ConvertCityObjTileToLocal: {0}".format(
            selection_grid_anchor_mismatches
        ))
    if building_sprite_geometry_mismatches:
        failures.append("initial building sprite geometry/root/dxf LOD must match Unity SpriteRenderer plus LevelDetailCastle: {0}".format(
            building_sprite_geometry_mismatches
        ))
    if len(extra_buildings) != expected_extra_buildings:
        failures.append("expected {0} supplemental interior preview building actors, found {1}".format(expected_extra_buildings, len(extra_buildings)))
    if TARGET_LEVEL == PROMOTED_GAMEAPP_LEVEL and (extra_buildings or expected_extra_buildings):
        failures.append("promoted GameApp city must contain zero RokUnityRuntimeInteriorPreview actors")
    if len(tagged) != len(boundary) + len(ground) + len(support_layers) + len(gate_flags) + len(buildings) + len(runtime_cameras) + len(runtime_player_starts):
        failures.append("runtime all-buildings map should have only boundary, ground, source support-layer, CityGate flag, building, camera, and player-start tagged actors: tagged={0}, boundary={1}, ground={2}, support={3}, flags={4}, buildings={5}, cameras={6}, player_starts={7}".format(
            len(tagged),
            len(boundary),
            len(ground),
            len(support_layers),
            len(gate_flags),
            len(buildings),
            len(runtime_cameras),
            len(runtime_player_starts),
        ))
    if manual_roads:
        failures.append("manual or unmanifested road actors are not allowed: {0}".format(len(manual_roads)))
    if forbidden_legacy_visuals:
        failures.append("legacy heuristic visual-layer actors are not allowed in the promoted source map: {0}".format(
            [actor_label(actor) for actor in forbidden_legacy_visuals]
        ))
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
    for boundary_actor in boundary:
        boundary_location = boundary_actor.get_actor_location()
        boundary_rotation = boundary_actor.get_actor_rotation()
        boundary_scale = boundary_actor.get_actor_scale3d()
        if any(abs(float(value)) > 0.05 for value in (
            boundary_location.x,
            boundary_location.y,
            boundary_location.z,
        )):
            failures.append(
                "world-baked boundary OBJ must keep identity actor location; Renderer.bounds center alignment is forbidden: {0}".format(
                    actor_label(boundary_actor)
                )
            )
        if any(abs(float(value)) > 0.05 for value in (
            boundary_rotation.pitch,
            boundary_rotation.yaw,
            boundary_rotation.roll,
        )):
            failures.append("world-baked boundary OBJ must keep identity actor rotation: {0}".format(actor_label(boundary_actor)))
        if any(abs(float(value) - 1.0) > 0.001 for value in (
            boundary_scale.x,
            boundary_scale.y,
            boundary_scale.z,
        )):
            failures.append("world-baked boundary OBJ must keep identity actor scale: {0}".format(actor_label(boundary_actor)))
        if not has_tag(boundary_actor, "RokUnityRuntimeObjBaked"):
            failures.append("boundary actor is missing world-baked OBJ provenance: {0}".format(actor_label(boundary_actor)))
        if not has_tag(boundary_actor, "UnityBakedOrientationScaleIdentity"):
            failures.append("boundary actor is missing identity orientation/scale evidence: {0}".format(actor_label(boundary_actor)))
        if not has_tag(boundary_actor, "UnityWorldBakedActorIdentity"):
            failures.append("boundary actor is missing identity location evidence: {0}".format(actor_label(boundary_actor)))
        if has_tag(boundary_actor, "UnityRendererBoundsCenterAligned"):
            failures.append("boundary actor still carries the obsolete Renderer.bounds alignment tag: {0}".format(actor_label(boundary_actor)))
    for gate_actor in gate_actors:
        gate_component = gate_actor.get_component_by_class(unreal.StaticMeshComponent)
        gate_scale = gate_actor.get_actor_scale3d()
        gate_rotation = gate_actor.get_actor_rotation()
        if any(abs(float(value) - 1.0) > 0.001 for value in (gate_scale.x, gate_scale.y, gate_scale.z)):
            failures.append("CityGate_101_1 baked OBJ should keep identity actor scale")
        if any(abs(float(value)) > 0.05 for value in (gate_rotation.pitch, gate_rotation.yaw, gate_rotation.roll)):
            failures.append("CityGate_101_1 baked OBJ should keep identity actor rotation")
        if not has_tag(gate_actor, "RokUnityRuntimeObjBaked") or not has_tag(gate_actor, "UnityBakedOrientationScaleIdentity"):
            failures.append("CityGate_101_1 missing baked OBJ identity-orientation source tags")
        if has_tag(gate_actor, "UnityGateLongAxisFacingCorrected") or has_tag(gate_actor, "UnityGateReverseCulling"):
            failures.append("CityGate_101_1 still carries the obsolete extra reflection tags")
        if not gate_component:
            failures.append("CityGate_101_1 missing StaticMeshComponent")
        elif bool(gate_component.get_editor_property("reverse_culling")):
            failures.append("CityGate_101_1 baked OBJ should not enable reverse culling")
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
    if building_sort_priority_counts != {"2995": 23, "2996": 1}:
        failures.append("initial building component priorities must be 23x2995 and 1x2996, found {0}".format(building_sort_priority_counts))
    if building_sort_priority_mismatches:
        failures.append("building component priorities/tags do not match Unity material queues: {0}".format(building_sort_priority_mismatches))
    if building_sort_distance_offset_mismatches:
        failures.append("building TranslucencySortDistanceOffset must remain zero: {0}".format(building_sort_distance_offset_mismatches))
    if extra_sort_priority_mismatches:
        failures.append("supplemental diagnostic building priorities/tags do not match their Unity material queues: {0}".format(extra_sort_priority_mismatches))
    if extra_sort_distance_offset_mismatches:
        failures.append("supplemental diagnostic building TranslucencySortDistanceOffset must remain zero: {0}".format(extra_sort_distance_offset_mismatches))
    if rendered_unresolved_visuals:
        failures.append("rendered buildings lack exact Unity prefab SpriteRenderer provenance: {0}".format(rendered_unresolved_visuals[:6]))
    if translucent_sort_policy not in (None, 0):
        failures.append("r.TranslucentSortPolicy must remain 0 so same-priority cards use perspective distance sorting, found {0}".format(translucent_sort_policy))
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
    if len(city_surface_actors) != 1 or len(city_translucent_actors) != 1:
        failures.append("expected one CitySurface and one CityTranslucent ground actor, found surface={0}, translucent={1}".format(
            len(city_surface_actors), len(city_translucent_actors)
        ))
    if not has_ground_vertex_alpha_texture:
        failures.append("CityTranslucent_101_1 must use the FBX vertex-alpha baked texture")
    if not has_ground_vertex_alpha_evidence or not has_ground_vertex_alpha_mapping:
        failures.append("CityTranslucent_101_1 is missing strict FBX ByPolygonVertex alpha evidence tags")
    if city_translucent_sort_priority != 2991 or city_surface_sort_priority != 2992:
        failures.append("ground component priorities must preserve Unity queues translucent=2991 and surface=2992, found {0}/{1}".format(
            city_translucent_sort_priority, city_surface_sort_priority
        ))
    if ground_vertex_alpha_material_errors:
        failures.append("CityTranslucent VertexAlpha material verification failed: {0}".format(
            ground_vertex_alpha_material_errors
        ))
    if ground_render_queue_counts.get("2991", 0) != 1 or ground_render_queue_counts.get("2992", 0) != 1:
        failures.append("ground should preserve Unity render queues 2991 and 2992, found {0}".format(ground_render_queue_counts))
    if support_render_queue_counts != {"2992": 1, "2993": 1}:
        failures.append("source support layers should preserve Unity queues road=2992 and authored shadow=2993, found {0}".format(
            support_render_queue_counts
        ))
    if gate_flag_render_queue_counts != {"2995": 2}:
        failures.append("the two CityGate flags should preserve Unity render queue 2995, found {0}".format(
            gate_flag_render_queue_counts
        ))
    if building_render_queue_counts.get("2995", 0) + building_render_queue_counts.get("2996", 0) != expected_total_buildings:
        failures.append("building sprite cards should preserve Unity transparent render queues 2995/2996, found {0}".format(building_render_queue_counts))
    if not any("BLEND_TRANSLUCENT" in key and value == 2 for key, value in ground_blend_mode_counts.items()):
        failures.append("ground materials should be translucent to preserve Unity ZWrite Off, found {0}".format(ground_blend_mode_counts))
    if not any("BLEND_TRANSLUCENT" in key and value == 2 for key, value in support_blend_mode_counts.items()):
        failures.append("source support-layer materials should preserve authored RGBA transparency, found {0}".format(
            support_blend_mode_counts
        ))
    if not any("BLEND_TRANSLUCENT" in key and value == 2 for key, value in gate_flag_blend_mode_counts.items()):
        failures.append("CityGate flag materials should preserve Unity transparent rendering, found {0}".format(
            gate_flag_blend_mode_counts
        ))
    if not any("BLEND_TRANSLUCENT" in key and value == expected_total_buildings for key, value in building_blend_mode_counts.items()):
        failures.append("building materials should be translucent to preserve Unity ZWrite Off, found {0}".format(building_blend_mode_counts))
    for actor in ground + gate_flags + buildings:
        if not has_tag(actor, "UnityShaderZWrite_Off"):
            failures.append("actor should preserve Unity ZWrite Off tag: {0}".format(actor_label(actor)))
    missing_card_tags = [
        actor_label(actor)
        for actor in initial_buildings
        if not has_tag(actor, "UnityRepresentation_runtime_sprite_card")
        or not has_tag(actor, "RokUnityRuntimeSpriteRendererBaked")
        or not has_tag(actor, "UnityTextureSource_BakedMaskSprite")
        or not any(tag.startswith("UnitySpriteGuid_") and len(tag) > len("UnitySpriteGuid_") for tag in actor_tags(actor))
        or not any(tag.startswith("UnityMaskGuid_") and len(tag) > len("UnityMaskGuid_") for tag in actor_tags(actor))
    ]
    if missing_card_tags:
        failures.append("building actors missing runtime sprite-card/baked-mask tags: {0}".format(", ".join(missing_card_tags[:6])))
    missing_extra_tags = [
        actor_label(actor)
        for actor in extra_buildings
        if not has_tag(actor, "UnityRepresentation_runtime_sprite_card")
        or not has_tag(actor, "RokUnityRuntimeInteriorPreview")
        or not has_tag(actor, "UnityTextureSource_UnityBuildingSpriteMaskBakedAlpha")
        or not has_tag(actor, "UnityOpacity_ColorA_SaturateMaskR2_CityTransparency")
    ]
    if missing_extra_tags:
        failures.append("supplemental interior building actors missing baked Unity sprite/mask alpha tags: {0}".format(", ".join(missing_extra_tags[:6])))
    if extra_alpha_material_count != len(extra_buildings):
        failures.append("supplemental interior materials should reproduce color.a * saturate(mask.r * 2.0) * _CityTransparency: valid={0}/{1}, errors={2}".format(
            extra_alpha_material_count,
            len(extra_buildings),
            extra_alpha_material_errors,
        ))

    max_boundary_bounds_error = verify_manifest_bounds(manifest.get("boundary_actors", []), boundary, failures, "boundary")
    max_ground_bounds_error = verify_manifest_bounds(manifest.get("ground_actors", []), ground, failures, "ground")
    max_support_bounds_error = verify_manifest_bounds(manifest_support_records, support_layers, failures, "support_layer")
    max_gate_flag_bounds_error = verify_manifest_bounds(manifest_gate_flag_records, gate_flags, failures, "city_gate_flag")
    max_building_bounds_error = verify_manifest_bounds(manifest.get("building_actors", []), initial_buildings, failures, "building")
    max_extra_building_bounds_error = verify_manifest_bounds(manifest.get("extra_interior_building_actors", []), extra_buildings, failures, "extra_interior_building")
    camera_visible_actor_centers = []
    camera_visible_building_centers = []
    if runtime_cameras:
        camera_visible_actor_centers = count_actor_centers_in_camera_view(
            runtime_cameras[0], boundary + ground + support_layers + gate_flags + buildings
        )
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
        "success": not failures,
        "failure_count": len(failures),
        "map": TARGET_LEVEL,
        "actors": len(actors),
        "tagged": len(tagged),
        "boundary": len(boundary),
        "ground": len(ground),
        "support_layers": len(support_layers),
        "support_role_counts": support_verify["role_counts"],
        "support_queue_counts": support_verify["queue_counts"],
        "support_missing_layer_ids": support_verify["missing_layer_ids"],
        "support_orphan_layer_ids": support_verify["orphan_layer_ids"],
        "support_priority_mismatches": support_verify["priority_mismatches"],
        "support_distance_offset_mismatches": support_verify["distance_offset_mismatches"],
        "support_max_bounds_center_error": support_verify["max_bounds_center_error"],
        "support_max_bounds_extent_error": support_verify["max_bounds_extent_error"],
        "city_gate_flags": len(gate_flags),
        "city_gate_flag_missing_ids": gate_flags_verify["missing_flag_ids"],
        "city_gate_flag_orphan_ids": gate_flags_verify["orphan_flag_ids"],
        "city_gate_flag_priority_mismatches": gate_flags_verify["priority_mismatches"],
        "city_gate_flag_distance_offset_mismatches": gate_flags_verify["distance_offset_mismatches"],
        "city_gate_flag_max_bounds_center_error": gate_flags_verify["max_bounds_center_error"],
        "city_gate_flag_max_bounds_extent_error": gate_flags_verify["max_bounds_extent_error"],
        "buildings": len(buildings),
        "initial_buildings": len(initial_buildings),
        "extra_interior_buildings": len(extra_buildings),
        "unclassified_building_count": len(unclassified_buildings),
        "overlapping_building_class_count": len(overlapping_building_classes),
        "manual_layout_actor_count": sum(1 for record in manifest_extra_records if record.get("manual_layout")),
        "rendered_unresolved_visual_count": len(rendered_unresolved_visuals),
        "rendered_unresolved_visuals": rendered_unresolved_visuals,
        "building_sort_priority_counts": building_sort_priority_counts,
        "building_sort_priority_mismatch_count": len(building_sort_priority_mismatches),
        "building_sort_priority_mismatches": building_sort_priority_mismatches,
        "building_sort_distance_offset_mismatches": building_sort_distance_offset_mismatches,
        "source_sprite_orientation_mismatch_count": len(source_sprite_orientation_mismatches),
        "source_sprite_orientation_mismatches": source_sprite_orientation_mismatches,
        "source_sprite_minimum_axis_dots": source_sprite_minimum_axis_dots,
        "selection_grid_anchor_mismatch_count": len(selection_grid_anchor_mismatches),
        "selection_grid_anchor_mismatches": selection_grid_anchor_mismatches,
        "max_live_grid_anchor_error": max_live_grid_anchor_error,
        "max_manifest_grid_anchor_error": max_manifest_grid_anchor_error,
        "building_sprite_geometry_mismatch_count": len(building_sprite_geometry_mismatches),
        "building_sprite_geometry_mismatches": building_sprite_geometry_mismatches,
        "max_building_sprite_center_error": max_building_sprite_center_error,
        "max_building_sprite_extent_error": max_building_sprite_extent_error,
        "extra_sort_priority_counts": extra_sort_priority_counts,
        "extra_sort_priority_mismatches": extra_sort_priority_mismatches,
        "extra_sort_distance_offset_mismatches": extra_sort_distance_offset_mismatches,
        "translucent_sort_policy": translucent_sort_policy,
        "extra_interior_alpha_formula_tag_count": sum(
            1 for actor in extra_buildings
            if has_tag(actor, "UnityOpacity_ColorA_SaturateMaskR2_CityTransparency")
        ),
        "extra_interior_alpha_material_count": extra_alpha_material_count,
        "extra_interior_alpha_material_errors": extra_alpha_material_errors,
        "roads": len(roads),
        "authored_shadows": len(authored_shadows),
        "manual_roads": len(manual_roads),
        "forbidden_legacy_visuals": len(forbidden_legacy_visuals),
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
        "boundary_world_baked_identity_count": sum(
            1 for actor in boundary
            if all(abs(float(value)) <= 0.05 for value in (
                actor.get_actor_location().x,
                actor.get_actor_location().y,
                actor.get_actor_location().z,
                actor.get_actor_rotation().pitch,
                actor.get_actor_rotation().yaw,
                actor.get_actor_rotation().roll,
            ))
            and all(abs(float(value) - 1.0) <= 0.001 for value in (
                actor.get_actor_scale3d().x,
                actor.get_actor_scale3d().y,
                actor.get_actor_scale3d().z,
            ))
            and has_tag(actor, "UnityWorldBakedActorIdentity")
        ),
        "gate_baked_identity_count": sum(
            1 for actor in gate_actors
            if all(abs(float(value) - 1.0) <= 0.001 for value in (
                actor.get_actor_scale3d().x,
                actor.get_actor_scale3d().y,
                actor.get_actor_scale3d().z,
            ))
            and all(abs(float(value)) <= 0.05 for value in (
                actor.get_actor_rotation().pitch,
                actor.get_actor_rotation().yaw,
                actor.get_actor_rotation().roll,
            ))
            and has_tag(actor, "RokUnityRuntimeObjBaked")
            and has_tag(actor, "UnityBakedOrientationScaleIdentity")
        ),
        "tower_actor_count": len(tower_actors),
        "tower_unique_mesh_count": len(tower_mesh_tags),
        "has_city_surface": has_city_surface,
        "has_city_translucent": has_city_translucent,
        "has_city_surface_material": has_city_surface_material,
        "has_city_translucent_material": has_city_translucent_material,
        "has_ground_baked_texture": has_ground_baked_texture,
        "has_ground_vertex_alpha_texture": has_ground_vertex_alpha_texture,
        "has_ground_vertex_alpha_evidence": has_ground_vertex_alpha_evidence,
        "has_ground_vertex_alpha_mapping": has_ground_vertex_alpha_mapping,
        "city_translucent_sort_priority": city_translucent_sort_priority,
        "city_surface_sort_priority": city_surface_sort_priority,
        "ground_vertex_alpha_material_errors": ground_vertex_alpha_material_errors,
        "material_counts": material_counts,
        "building_type_counts": building_type_counts,
        "all_building_type_counts": all_building_type_counts,
        "ground_render_queue_counts": ground_render_queue_counts,
        "support_render_queue_counts": support_render_queue_counts,
        "city_gate_flag_render_queue_counts": gate_flag_render_queue_counts,
        "building_render_queue_counts": building_render_queue_counts,
        "ground_blend_mode_counts": ground_blend_mode_counts,
        "support_blend_mode_counts": support_blend_mode_counts,
        "city_gate_flag_blend_mode_counts": gate_flag_blend_mode_counts,
        "building_blend_mode_counts": building_blend_mode_counts,
        "unique_init_id_count": len(init_ids),
        "max_boundary_bounds_center_error": max_boundary_bounds_error,
        "max_ground_bounds_center_error": max_ground_bounds_error,
        "ground_identity_count": ground_contract["identity_count"],
        "ground_axis_compensated_count": ground_contract["axis_compensated_count"],
        "ground_face_winding_compensated_count": ground_contract["face_winding_compensated_count"],
        "ground_one_sided_material_count": ground_contract["one_sided_material_count"],
        "ground_predicted_ue_up_facing_triangle_count": ground_contract["predicted_ue_up_facing_triangle_count"],
        "ground_predicted_ue_down_facing_triangle_count": ground_contract["predicted_ue_down_facing_triangle_count"],
        "ground_predicted_ue_degenerate_triangle_count": ground_contract["predicted_ue_degenerate_triangle_count"],
        "ground_imported_ue_up_facing_triangle_count": ground_contract["imported_ue_up_facing_triangle_count"],
        "ground_imported_ue_down_facing_triangle_count": ground_contract["imported_ue_down_facing_triangle_count"],
        "ground_imported_ue_degenerate_triangle_count": ground_contract["imported_ue_degenerate_triangle_count"],
        "ground_max_tight_bounds_center_error": ground_contract["max_bounds_center_error"],
        "ground_max_tight_bounds_extent_error": ground_contract["max_bounds_extent_error"],
        "ground_max_axis_probe_error": ground_contract["max_axis_probe_error"],
        "ground_max_obj_input_bounds_error": ground_contract["max_obj_input_bounds_error"],
        "max_support_bounds_center_error": max_support_bounds_error,
        "max_city_gate_flag_bounds_center_error": max_gate_flag_bounds_error,
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

    line = "UnityRuntimeCityAllBuildingsVerify actors={actors}, tagged={tagged}, boundary={boundary}, ground={ground}, support={support_layers}, gate_flags={city_gate_flags}, buildings={buildings}, cameras={runtime_cameras}, player_starts={runtime_player_starts}, towers={tower_actor_count}, failures={failure_count}".format(
        **summary
    )
    unreal.log(line)
    if failures:
        raise RuntimeError("; ".join(failures))


if __name__ == "__main__":
    verify_scene()
