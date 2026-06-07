import json
import math
import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokUnityFreshCityScene as fresh
import CreateRokUnityRuntimeBoundaryOnlyScene as boundary
import CreateRokUnityRuntimeGroundScene as ground
import CreateRokRuntimeCityScene as runtime


TARGET_LEVEL = os.environ.get("ROK_UNITY_RUNTIME_CITY_ALL_BUILDINGS_LEVEL", "/Game/Maps/L_RokUnityRuntimeCityAllBuildings")
CITY_TAG = "RokUnityRuntimeCityAllBuildings"
FOLDER_ROOT = "RokUnityRuntimeCityAllBuildings"
ACTOR_PREFIX = "UnityRuntimeCityAllBuildings"
ASSET_SUFFIX = fresh.safe_tag(os.environ.get("ROK_RUNTIME_CITY_ALL_BUILDINGS_ASSET_SUFFIX", ""))
ASSET_FOLDER_ROOT = "RokUnityRuntimeCityAllBuildings{0}".format("_" + ASSET_SUFFIX if ASSET_SUFFIX else "")
MIGRATION_ROOT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration")
UNITY_RUNTIME_CITY_BUILDINGS_OBJ_MANIFEST = os.path.join(
    MIGRATION_ROOT,
    "unity_runtime_city_buildings_obj_manifest.json",
)
SCENE_MANIFEST = os.environ.get(
    "ROK_UNITY_RUNTIME_CITY_ALL_BUILDINGS_MANIFEST",
    os.path.join(
        MIGRATION_ROOT,
        "unity_runtime_city_all_buildings_scene_manifest{0}.json".format("_" + ASSET_SUFFIX if ASSET_SUFFIX else ""),
    ),
)
BUILDING_OBJ_MESH_DESTINATION = "/Game/{0}/RuntimeBuildingObjMeshes".format(ASSET_FOLDER_ROOT)
BUILDING_TEXTURE_DESTINATION = "/Game/{0}/Textures".format(ASSET_FOLDER_ROOT)
BUILDING_MATERIAL_DESTINATION = "/Game/{0}/Materials".format(ASSET_FOLDER_ROOT)
UE_UNITS_PER_UNITY_UNIT = 500.0
CAMERA_VIEW_CENTER = (0.0, 0.0)
CAMERA_VIEW_DISTANCE = 20.0
CAMERA_VERTICAL_FOV = 7.5
CAMERA_DXF = 150.0
CAMERA_FORWARD_UNITY = (0.0, -0.7, 0.7)
CAMERA_ASPECT = 16.0 / 10.0
BUILDING_CARD_MODE = os.environ.get("ROK_RUNTIME_BUILDING_CARD_MODE", "sprite_actor")
CITY_SPRITE_ACTOR_CLASS = "/Script/Rok.RokCitySpriteActor"
EXTRA_INTERIOR_BUILDING_COUNT = max(0, int(os.environ.get("ROK_RUNTIME_EXTRA_INTERIOR_BUILDINGS", "0")))
UNITY_CITY_BUILDING_TEXTURE_ROOT = os.path.join(
    fresh.ROK_UNITY_ASSETS,
    "BundleAssets",
    "Map",
    "Building",
    "Building_use",
    "shared_building",
    "Building_101",
    "city_building_1",
)
EXTRA_INTERIOR_LAYOUT = [
    (17, "build_17_101_1", -10, 8),
    (16, "build_16_101_1", -4, 11),
    (18, "build_18_101_1", 3, 10),
    (30, "build_30_101_1", 9, 8),
    (28, "build_28_101_1", 10, -8),
    (11, "build_11_101_1", 4, -11),
    (2, "build_2_101_1", -6, -13),
    (3, "build_3_101_1", -12, -6),
    (2, "build_2_101_1", 13, 3),
    (3, "build_3_101_1", 12, -2),
    (11, "build_11_101_1", -1, -14),
    (18, "build_18_101_1", 7, -13),
    (16, "build_16_101_1", -13, 2),
    (17, "build_17_101_1", -7, -9),
    (30, "build_30_101_1", 0, 14),
    (28, "build_28_101_1", -14, -11),
    (51, "build_51_101_1_1", -16, 8),
    (51, "build_51_101_1_2", 15, 8),
    (51, "build_51_101_1_3", 15, -10),
    (2, "build_2_101_1", -15, -2),
    (3, "build_3_101_1", 6, 13),
    (11, "build_11_101_1", -9, 13),
    (18, "build_18_101_1", 12, -13),
    (30, "build_30_101_1", -3, -16),
]


def actor_tags(actor):
    return [str(tag) for tag in list(actor.tags)]


def set_tags(actor, tags):
    existing = actor_tags(actor)
    for tag in tags:
        if tag not in existing:
            existing.append(tag)
    actor.tags = existing


def load_building_obj_manifest():
    if not os.path.exists(UNITY_RUNTIME_CITY_BUILDINGS_OBJ_MANIFEST):
        raise RuntimeError("Missing Unity runtime city building OBJ manifest: {0}".format(
            UNITY_RUNTIME_CITY_BUILDINGS_OBJ_MANIFEST
        ))
    with open(UNITY_RUNTIME_CITY_BUILDINGS_OBJ_MANIFEST, "r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def import_runtime_building_obj_mesh(record):
    obj_path = record.get("obj_path")
    if not obj_path or not os.path.exists(obj_path):
        raise RuntimeError("Missing Unity runtime city building OBJ: {0}".format(obj_path))

    asset_name = fresh.safe_tag(os.path.splitext(os.path.basename(obj_path))[0])
    package_path = fresh.proto.asset_package_path(BUILDING_OBJ_MESH_DESTINATION, asset_name)
    existing = fresh.proto.load_asset_if_exists(package_path)
    force_reimport = os.environ.get(
        "ROK_FORCE_REIMPORT_BUILDING_MESHES",
        os.environ.get("ROK_FORCE_REIMPORT", "1"),
    ) != "0"
    if isinstance(existing, unreal.StaticMesh) and not force_reimport:
        return existing

    unreal.EditorAssetLibrary.make_directory(BUILDING_OBJ_MESH_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", obj_path)
    task.set_editor_property("destination_path", BUILDING_OBJ_MESH_DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    imported = fresh.proto.load_asset_if_exists(package_path)
    if isinstance(imported, unreal.StaticMesh):
        return imported
    for imported_path in task.get_editor_property("imported_object_paths"):
        candidate = fresh.proto.load_asset_if_exists(imported_path)
        if isinstance(candidate, unreal.StaticMesh):
            return candidate
    raise RuntimeError("Failed to import Unity runtime city building OBJ: {0}".format(obj_path))


def import_building_texture(source_path, asset_name):
    if not source_path or not os.path.exists(source_path):
        raise RuntimeError("Missing Unity runtime city building baked texture: {0}".format(source_path))

    safe_name = fresh.proto.safe_asset_name(asset_name)
    package_path = fresh.proto.asset_package_path(BUILDING_TEXTURE_DESTINATION, safe_name)
    existing = fresh.proto.load_asset_if_exists(package_path)
    force_reimport = os.environ.get(
        "ROK_FORCE_REIMPORT_BUILDING_TEXTURES",
        os.environ.get("ROK_FORCE_REIMPORT_TEXTURES", "1"),
    ) != "0"
    if existing and not force_reimport:
        return existing

    unreal.EditorAssetLibrary.make_directory(BUILDING_TEXTURE_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", BUILDING_TEXTURE_DESTINATION)
    task.set_editor_property("destination_name", safe_name)
    task.set_editor_property("factory", unreal.TextureFactory())
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", False)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = fresh.proto.load_asset_if_exists(package_path)
    if not texture:
        for imported_path in task.get_editor_property("imported_object_paths"):
            texture = fresh.proto.load_asset_if_exists(imported_path)
            if texture:
                break
    if not texture:
        raise RuntimeError("Failed to import Unity runtime city building texture: {0}".format(source_path))

    try:
        texture.set_editor_property("srgb", True)
        texture.set_editor_property("never_stream", True)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    except Exception:
        pass
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def create_building_material(texture, material_name):
    material_path = fresh.proto.asset_package_path(BUILDING_MATERIAL_DESTINATION, material_name)
    existing = fresh.proto.load_asset_if_exists(material_path)
    update_existing = os.environ.get("ROK_UPDATE_EXISTING_MATERIALS", "1") != "0"
    if existing and not update_existing:
        return existing
    if existing:
        material = existing
    else:
        unreal.EditorAssetLibrary.make_directory(BUILDING_MATERIAL_DESTINATION)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            material_name,
            BUILDING_MATERIAL_DESTINATION,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    try:
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    except Exception:
        pass
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    texture_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSample,
        -480,
        0,
    )
    texture_node.set_editor_property("texture", texture)
    unreal.MaterialEditingLibrary.connect_material_property(texture_node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(texture_node, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(texture_node, "A", unreal.MaterialProperty.MP_OPACITY)

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -480,
        220,
    )
    roughness.set_editor_property("r", 0.92)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def create_preview_building_material(color_texture, mask_texture, material_name):
    material_path = fresh.proto.asset_package_path(BUILDING_MATERIAL_DESTINATION, material_name)
    existing = fresh.proto.load_asset_if_exists(material_path)
    update_existing = os.environ.get("ROK_UPDATE_EXISTING_MATERIALS", "1") != "0"
    if existing and not update_existing:
        return existing
    if existing:
        material = existing
    else:
        unreal.EditorAssetLibrary.make_directory(BUILDING_MATERIAL_DESTINATION)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            material_name,
            BUILDING_MATERIAL_DESTINATION,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    try:
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    except Exception:
        pass
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    color_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSample,
        -560,
        -40,
    )
    color_node.set_editor_property("texture", color_texture)
    unreal.MaterialEditingLibrary.connect_material_property(color_node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(color_node, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    if mask_texture:
        mask_node = unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionTextureSample,
            -560,
            180,
        )
        mask_node.set_editor_property("texture", mask_texture)
        unreal.MaterialEditingLibrary.connect_material_property(mask_node, "R", unreal.MaterialProperty.MP_OPACITY)
    else:
        unreal.MaterialEditingLibrary.connect_material_property(color_node, "A", unreal.MaterialProperty.MP_OPACITY)

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -560,
        360,
    )
    roughness.set_editor_property("r", 0.92)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def configure_static_mesh_component(actor, sort_priority):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        return
    for prop, value in [
        ("cast_shadow", False),
        ("receive_decals", False),
        ("use_as_occluder", False),
        ("collision_enabled", unreal.CollisionEnabled.NO_COLLISION),
        ("translucency_sort_priority", sort_priority),
    ]:
        try:
            component.set_editor_property(prop, value)
        except Exception:
            pass


def align_visible_bounds_center(actor, expected_center):
    if not expected_center:
        return
    origin, _extent = actor.get_actor_bounds(False)
    target = unreal.Vector(float(expected_center[0]), float(expected_center[1]), float(expected_center[2]))
    actor.set_actor_location(actor.get_actor_location() + (target - origin), False, False)


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


def camera_facing_rotation(location):
    target = expected_camera_location() - location
    try:
        return unreal.MathLibrary.make_rot_from_zy(target, unreal.Vector(0.0, 0.0, 1.0))
    except Exception:
        pass
    try:
        return unreal.KismetMathLibrary.make_rot_from_zy(target, unreal.Vector(0.0, 0.0, 1.0))
    except Exception:
        pass
    return fresh.proto.make_rotator(45.0, 0.0, 0.0)


def configure_camera_component(component):
    if not component:
        return
    component.set_editor_property("projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
    component.set_editor_property("field_of_view", vertical_to_horizontal_fov(CAMERA_VERTICAL_FOV, CAMERA_ASPECT))
    component.set_editor_property("aspect_ratio", CAMERA_ASPECT)
    component.set_editor_property("constrain_aspect_ratio", True)
    try:
        pp = component.get_editor_property("post_process_settings")
        for prop, value in [
            ("override_auto_exposure_method", True),
            ("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
            ("override_auto_exposure_min_brightness", True),
            ("auto_exposure_min_brightness", 1.0),
            ("override_auto_exposure_max_brightness", True),
            ("auto_exposure_max_brightness", 1.0),
            ("override_auto_exposure_bias", True),
            ("auto_exposure_bias", 0.0),
            ("override_auto_exposure_apply_physical_camera_exposure", True),
            ("auto_exposure_apply_physical_camera_exposure", False),
            ("override_tone_curve_amount", True),
            ("tone_curve_amount", 0.0),
            ("override_blue_correction", True),
            ("blue_correction", 0.0),
            ("override_expand_gamut", True),
            ("expand_gamut", 0.0),
            ("override_bloom_intensity", True),
            ("bloom_intensity", 0.0),
            ("override_vignette_intensity", True),
            ("vignette_intensity", 0.0),
            ("override_motion_blur_amount", True),
            ("motion_blur_amount", 0.0),
        ]:
            try:
                pp.set_editor_property(prop, value)
            except Exception:
                pass
        component.set_editor_property("post_process_settings", pp)
    except Exception as error:
        unreal.log_warning("Skipping runtime city all-buildings camera post-process setup: {0}".format(error))


def spawn_camera_and_player_start():
    location = expected_camera_location()
    forward = camera_forward_ue()
    rotation = fresh.look_rotation_from_forward(forward)

    camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.CameraActor, location, rotation)
    camera.set_actor_label("{0}_Camera_WorldCamera_city_default".format(ACTOR_PREFIX))
    camera.set_folder_path("{0}/Camera".format(FOLDER_ROOT))
    set_tags(camera, [
        CITY_TAG,
        "RokUnityRuntimeCameraSource",
        "UnityCamera_WorldCamera",
        "UnityCameraParam_city_default",
        "UnityCameraDxf_{0:g}".format(CAMERA_DXF),
        "UnityCameraDistance_{0:g}".format(CAMERA_VIEW_DISTANCE),
        "UnityCameraVerticalFov_{0:g}".format(CAMERA_VERTICAL_FOV),
        "UnityCameraForward_0_-0p7_0p7",
    ])
    configure_camera_component(camera.get_component_by_class(unreal.CameraComponent))

    player_start = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart,
        location,
        rotation,
    )
    player_start.set_actor_label("{0}_PlayerStart_city_default".format(ACTOR_PREFIX))
    player_start.set_folder_path("{0}/Camera".format(FOLDER_ROOT))
    set_tags(player_start, [
        CITY_TAG,
        "RokUnityRuntimePlayerStart",
        "UnityCameraParam_city_default",
    ])
    return camera, player_start


def sprite_card_visual_size(record):
    extents = record.get("ue_bounds_extents") or [50.0, 50.0, 50.0]
    width = max(1.0, float(extents[0]) * 2.0)
    height = max(1.0, max(float(extents[1]), float(extents[2])) * 2.0 * math.sqrt(2.0))
    return width, height


def spawn_building_sprite_actor(label, material, record, folder, sort_priority):
    sprite_actor_class = unreal.load_class(None, CITY_SPRITE_ACTOR_CLASS)
    if not sprite_actor_class:
        raise RuntimeError("Missing runtime sprite actor class: {0}".format(CITY_SPRITE_ACTOR_CLASS))
    center = record.get("ue_bounds_center") or [0.0, 0.0, 0.0]
    location = unreal.Vector(float(center[0]), float(center[1]), float(center[2]))
    width, height = sprite_card_visual_size(record)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        sprite_actor_class,
        location,
        camera_facing_rotation(location),
    )
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.configure_sprite(material, width, height, sort_priority)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if component:
        scale = component.get_editor_property("relative_scale3d")
        component.set_editor_property(
            "relative_scale3d",
            unreal.Vector(float(scale.x), -abs(float(scale.y)), float(scale.z)),
        )
    return actor


def actor_record_from_building(actor, record):
    bounds_origin, bounds_extent = actor.get_actor_bounds(False)
    return {
        "actor": actor.get_actor_label(),
        "init_id": record.get("init_id"),
        "building_type": record.get("building_type"),
        "ue_type": record.get("ue_type"),
        "level": record.get("level"),
        "grid_x": record.get("grid_x"),
        "grid_y": record.get("grid_y"),
        "grid_width": record.get("grid_width"),
        "grid_length": record.get("grid_length"),
        "model_id": record.get("model_id"),
        "visual_sprite_name": record.get("visual_sprite_name"),
        "prefab_path": record.get("prefab_path"),
        "renderer_path": record.get("renderer_path"),
        "sprite_path": record.get("sprite_path"),
        "mask_path": record.get("mask_path"),
        "material_path": record.get("material_path"),
        "shader_name": record.get("shader_name"),
        "shader_asset_path": record.get("shader_asset_path"),
        "shader_queue_tag": record.get("shader_queue_tag"),
        "shader_zwrite": record.get("shader_zwrite"),
        "material_render_queue": record.get("material_render_queue"),
        "representation": record.get("representation"),
        "obj_path": record.get("obj_path"),
        "baked_texture_path": record.get("baked_texture_path"),
        "root_local_position": record.get("root_local_position"),
        "root_local_scale": record.get("root_local_scale"),
        "renderer_local_position": record.get("renderer_local_position"),
        "renderer_local_euler": record.get("renderer_local_euler"),
        "renderer_local_scale": record.get("renderer_local_scale"),
        "unity_bounds_center": record.get("unity_bounds_center"),
        "unity_bounds_extents": record.get("unity_bounds_extents"),
        "ue_expected_bounds_center": record.get("ue_bounds_center"),
        "ue_expected_bounds_extents": record.get("ue_bounds_extents"),
        "ue_alignment_mode": "Unity runtime SpriteRenderer geometry baked to OBJ in UE centimeters; visible bounds center aligned to Unity Renderer.bounds.center",
        "ue_location": [
            float(actor.get_actor_location().x),
            float(actor.get_actor_location().y),
            float(actor.get_actor_location().z),
        ],
        "ue_rotation": [
            float(actor.get_actor_rotation().pitch),
            float(actor.get_actor_rotation().yaw),
            float(actor.get_actor_rotation().roll),
        ],
        "ue_scale": [
            float(actor.get_actor_scale3d().x),
            float(actor.get_actor_scale3d().y),
            float(actor.get_actor_scale3d().z),
        ],
        "ue_visible_bounds_origin": [float(bounds_origin.x), float(bounds_origin.y), float(bounds_origin.z)],
        "ue_visible_bounds_extent": [float(bounds_extent.x), float(bounds_extent.y), float(bounds_extent.z)],
    }


def spawn_buildings_from_runtime_obj_manifest(building_manifest):
    records = []
    sorted_records = sorted(
        building_manifest.get("records", []),
        key=lambda item: (float(item.get("ue_bounds_center", [0.0, 0.0, 0.0])[1]), int(item.get("init_id", 0))),
    )
    for index, record in enumerate(sorted_records):
        texture_source = record.get("baked_texture_path")
        texture_asset_name = "T_RokUnityRuntimeBuilding_{0}".format(
            fresh.safe_tag(os.path.splitext(os.path.basename(texture_source))[0])
        )
        texture = import_building_texture(texture_source, texture_asset_name)
        material_name = "M_RokUnityRuntimeBuilding_{0}_{1}_RQ{2}_Z{3}_AlphaCardV2".format(
            fresh.safe_tag(record.get("model_id", "Building")),
            int(record.get("init_id", index)),
            record.get("material_render_queue", "0"),
            fresh.safe_tag(record.get("shader_zwrite", "")),
        )
        material = create_building_material(texture, material_name)
        label = "{0}_Init_{1:03d}_{2}_{3}".format(
            ACTOR_PREFIX,
            int(record.get("init_id", index)),
            fresh.safe_tag(record.get("model_id", "Building")),
            index,
        )
        folder = "{0}/Buildings".format(FOLDER_ROOT)
        if BUILDING_CARD_MODE == "sprite_actor":
            actor = spawn_building_sprite_actor(
                label,
                material,
                record,
                folder,
                int(record.get("material_render_queue") or (index + 20)),
            )
        else:
            mesh = import_runtime_building_obj_mesh(record)
            actor = fresh.proto.spawn_mesh(
                label,
                mesh,
                (0.0, 0.0, 0.0),
                (1.0, 1.0, 1.0),
                material,
                (0.0, 0.0, 0.0),
                folder,
            )
        actor.tags = []
        set_tags(actor, [
            CITY_TAG,
            "RokBuilding",
            "CityBuilding",
            "RokUnityConfigInitBuilding",
            "RokUnityRuntimeBuildingSource",
            "RokUnityRuntimeSpriteRendererBaked",
            "UnityRepresentation_runtime_sprite_card",
            "UnityRepresentation_runtime_sprite_actor_card" if BUILDING_CARD_MODE == "sprite_actor" else "UnityRepresentation_runtime_obj_card",
            "UnityRendererBoundsCenterAligned",
            "UnityInitId_{0}".format(record.get("init_id")),
            "UnityBuildingType_{0}".format(record.get("building_type")),
            "UnityBuildingLevel_{0}".format(record.get("level")),
            "UnityGridX_{0}".format(record.get("grid_x")),
            "UnityGridY_{0}".format(record.get("grid_y")),
            "UnityGridWidth_{0}".format(record.get("grid_width")),
            "UnityGridLength_{0}".format(record.get("grid_length")),
            "UnityModel_{0}".format(fresh.safe_tag(record.get("model_id", ""))),
            "UnitySprite_{0}".format(fresh.safe_tag(record.get("visual_sprite_name", ""))),
            "UnitySourcePrefab_{0}".format(fresh.safe_tag(os.path.splitext(os.path.basename(record.get("prefab_path", "")))[0])),
            "UnityTextureSource_BakedMaskSprite",
            "UnityShader_{0}".format(fresh.safe_tag(record.get("shader_name", ""))),
            "UnityShaderQueue_{0}".format(fresh.safe_tag(record.get("shader_queue_tag", ""))),
            "UnityShaderZWrite_{0}".format(fresh.safe_tag(record.get("shader_zwrite", ""))),
            "UnityRenderQueue_{0}".format(record.get("material_render_queue", "")),
            "UnityRuntimeMaterial_AlphaCardV2",
            "UnityRuntimeBuildingCardMode_{0}".format(fresh.safe_tag(BUILDING_CARD_MODE)),
        ])
        actor.set_folder_path(folder)
        configure_static_mesh_component(actor, int(record.get("material_render_queue") or (index + 20)))
        if BUILDING_CARD_MODE != "sprite_actor":
            align_visible_bounds_center(actor, record.get("ue_bounds_center"))
        records.append(actor_record_from_building(actor, record))
    return len(records), records


def load_building_type_sizes():
    path = os.path.join(MIGRATION_ROOT, "ConfigTables", "BuildingTypeConfig.json")
    if not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8-sig") as handle:
        table = json.load(handle)
    sizes = {}
    for row in table.get("rows", []):
        try:
            building_type = int(row.get("type", 0))
            sizes[building_type] = (
                max(1, int(row.get("width", 6))),
                max(1, int(row.get("length", 6))),
            )
        except (TypeError, ValueError):
            continue
    return sizes


def unity_grid_to_ue_center(grid_x, grid_y, grid_width, grid_length, height):
    unity_x = float(grid_x) * 0.1 + float(grid_width) * 0.05 - 0.05
    unity_z = float(grid_y) * 0.1 + float(grid_length) * 0.05 - 0.05
    yaw = math.radians(-45.0)
    rotated_x = math.cos(yaw) * unity_x + math.sin(yaw) * unity_z
    rotated_z = -math.sin(yaw) * unity_x + math.cos(yaw) * unity_z
    return unreal.Vector(-rotated_x * UE_UNITS_PER_UNITY_UNIT, rotated_z * UE_UNITS_PER_UNITY_UNIT, max(35.0, float(height) * 0.18))


def extra_building_source_paths(model_id):
    color = os.path.join(UNITY_CITY_BUILDING_TEXTURE_ROOT, "{0}.png".format(model_id))
    mask = os.path.join(UNITY_CITY_BUILDING_TEXTURE_ROOT, "{0}_mask.png".format(model_id))
    if not os.path.exists(color):
        return None, None
    return color, mask if os.path.exists(mask) else None


def extra_building_prefab_model_id(model_id):
    if model_id.startswith("build_51_101_1_"):
        return "build_51_101_1"
    return model_id


def spawn_extra_interior_buildings():
    if EXTRA_INTERIOR_BUILDING_COUNT <= 0:
        return 0, []
    type_sizes = load_building_type_sizes()
    records = []
    for index, (building_type, model_id, grid_x, grid_y) in enumerate(EXTRA_INTERIOR_LAYOUT[:EXTRA_INTERIOR_BUILDING_COUNT]):
        color_path, mask_path = extra_building_source_paths(model_id)
        if not color_path:
            unreal.log_warning("Skipping missing Unity interior preview building texture: {0}".format(model_id))
            continue
        color_texture = import_building_texture(
            color_path,
            "T_RokUnityInteriorPreview_{0}".format(fresh.safe_tag(model_id)),
        )
        mask_texture = import_building_texture(
            mask_path,
            "T_RokUnityInteriorPreview_{0}_Mask".format(fresh.safe_tag(model_id)),
        ) if mask_path else None
        material = create_preview_building_material(
            color_texture,
            mask_texture,
            "M_RokUnityInteriorPreview_{0}_{1}_RQ2995".format(fresh.safe_tag(model_id), index + 1),
        )
        tex_width, tex_height = runtime.texture_dimensions(color_texture)
        grid_width, grid_length = type_sizes.get(int(building_type), (6, 6))
        visual_width = max(260.0, float(max(grid_width, grid_length)) * 70.0)
        visual_height = visual_width * float(tex_height) / max(1.0, float(tex_width))
        location = unity_grid_to_ue_center(grid_x, grid_y, grid_width, grid_length, visual_height)
        record = {
            "init_id": None,
            "preview_id": index + 1,
            "building_type": building_type,
            "ue_type": "InteriorPreview",
            "level": 1,
            "grid_x": grid_x,
            "grid_y": grid_y,
            "grid_width": grid_width,
            "grid_length": grid_length,
            "model_id": model_id,
            "visual_sprite_name": model_id,
            "prefab_path": "Assets/BundleAssets/Scene/City_building/city_building_1/{0}.prefab".format(extra_building_prefab_model_id(model_id)),
            "sprite_path": os.path.relpath(color_path, fresh.ROK_UNITY_CLIENT_ROOT).replace("\\", "/"),
            "mask_path": os.path.relpath(mask_path, fresh.ROK_UNITY_CLIENT_ROOT).replace("\\", "/") if mask_path else "",
            "material_path": "Assets/BundleAssets/Scene/City/city_function/map_building.mat",
            "shader_name": "custom/map/map_building_with_mask-5",
            "shader_queue_tag": "Transparent-5",
            "shader_zwrite": "Off",
            "material_render_queue": 2995,
            "representation": "supplemental Unity interior preview sprite card",
            "ue_bounds_center": [float(location.x), float(location.y), float(location.z)],
            "ue_bounds_extents": [visual_width * 0.5, visual_height * 0.35, visual_height * 0.35],
        }
        label = "{0}_ExtraInterior_{1:02d}_{2}".format(ACTOR_PREFIX, index + 1, fresh.safe_tag(model_id))
        actor = spawn_building_sprite_actor(
            label,
            material,
            record,
            "{0}/InteriorPreviewBuildings".format(FOLDER_ROOT),
            2995 + index,
        )
        set_tags(actor, [
            CITY_TAG,
            "RokBuilding",
            "CityBuilding",
            "RokUnityRuntimeBuildingSource",
            "RokUnityRuntimeInteriorPreview",
            "UnityRepresentation_runtime_sprite_card",
            "UnityRepresentation_runtime_sprite_actor_card",
            "UnityRendererBoundsCenterAligned",
            "UnityInteriorPreviewId_{0}".format(index + 1),
            "UnityBuildingType_{0}".format(building_type),
            "UnityBuildingLevel_1",
            "UnityGridX_{0}".format(grid_x),
            "UnityGridY_{0}".format(grid_y),
            "UnityGridWidth_{0}".format(grid_width),
            "UnityGridLength_{0}".format(grid_length),
            "UnityModel_{0}".format(fresh.safe_tag(model_id)),
            "UnitySprite_{0}".format(fresh.safe_tag(model_id)),
            "UnityTextureSource_UnityBuildingSpriteMask",
            "UnityShader_custom_map_map_building_with_mask_5",
            "UnityShaderQueue_Transparent_5",
            "UnityShaderZWrite_Off",
            "UnityRenderQueue_2995",
            "UnityRuntimeMaterial_InteriorPreview",
            "UnityRuntimeBuildingCardMode_{0}".format(fresh.safe_tag(BUILDING_CARD_MODE)),
        ])
        configure_static_mesh_component(actor, 2995 + index)
        records.append(actor_record_from_building(actor, record))
    return len(records), records


def configure_shared_modules_for_all_buildings_map():
    boundary.CITY_TAG = CITY_TAG
    boundary.FOLDER_ROOT = FOLDER_ROOT
    boundary.ACTOR_PREFIX = ACTOR_PREFIX

    ground.CITY_TAG = CITY_TAG
    ground.FOLDER_ROOT = FOLDER_ROOT
    ground.ACTOR_PREFIX = ACTOR_PREFIX


def build_scene():
    fresh.TARGET_LEVEL = TARGET_LEVEL
    fresh.CITY_TAG = CITY_TAG
    fresh.require_sources()

    fresh.create_or_load_target_map()
    fresh.clear_current_level()

    configure_shared_modules_for_all_buildings_map()

    boundary_manifest = boundary.load_obj_manifest()
    boundary_spawned, boundary_records = boundary.spawn_boundary_from_runtime_obj_manifest(boundary_manifest)

    ground_manifest = ground.load_ground_obj_manifest()
    ground_spawned, ground_records = ground.spawn_ground_from_runtime_obj_manifest(ground_manifest)

    building_manifest = load_building_obj_manifest()
    buildings_spawned, building_records = spawn_buildings_from_runtime_obj_manifest(building_manifest)
    extra_buildings_spawned, extra_building_records = spawn_extra_interior_buildings()
    camera, player_start = spawn_camera_and_player_start()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError("Failed to save runtime city all-buildings map: {0}".format(TARGET_LEVEL))
    unreal.EditorAssetLibrary.save_asset(TARGET_LEVEL, only_if_is_dirty=False)

    summary = {
        "schema": "rok.unity_runtime_city_all_buildings_scene.v1",
        "target_level": TARGET_LEVEL,
        "asset_suffix": ASSET_SUFFIX,
        "asset_folder_root": ASSET_FOLDER_ROOT,
        "building_card_mode": BUILDING_CARD_MODE,
        "rok_unity_root": fresh.ROK_UNITY_ROOT,
        "source_files": {
            "GameAppScene": fresh.SOURCE_CHAIN["GameAppScene"],
            "SwitchMainCityCmd": fresh.SOURCE_CHAIN["SwitchMainCityCmd"],
            "CityGlobalMediator": fresh.SOURCE_CHAIN["CityGlobalMediator"],
            "CityBuildingContainerPrefab": fresh.SOURCE_CHAIN["CityBuildingContainerPrefab"],
            "CityWallPrefab": fresh.SOURCE_CHAIN["CityWallPrefab"],
            "GuardTowerPrefab": fresh.SOURCE_CHAIN["GuardTowerPrefab"],
            "UnityBuildingExportScene": building_manifest.get("scene"),
        },
        "unity_runtime_rules": {
            "city_container_parent": "SceneObject/land_root",
            "city_container_yaw": building_manifest.get("container_yaw"),
            "runtime_lod_scale": building_manifest.get("runtime_lod_scale"),
            "ordinary_building_position": "ConvertCityObjTileToLocal: x*0.1 + width*0.05 - 0.05, y=0.01, yGrid*0.1 + length*0.05 - 0.05",
            "boundary_position": "CityWall and GuardTower return Vector3.zero, exported through Unity runtime MeshRenderer geometry instead of grid cards",
            "ordinary_building_visual": "Unity prefab root GridCollideItem plus child SpriteRenderer named sprite; MaskSprite red-channel mask baked into PNG alpha for UE translucent sprite-card material. UE runtime preview uses RokCitySpriteActor camera-facing cards by default because importing SpriteRenderer geometry as OBJ can become edge-on in PIE.",
            "ordinary_building_asset_refresh": "Generated runtime building OBJ meshes, baked PNG textures, and AlphaCardV2 materials are reimported/rebuilt by default so stale UV/material assets cannot hide cards in PIE.",
            "extra_interior_preview": "Optional ROK_RUNTIME_EXTRA_INTERIOR_BUILDINGS spawns source-derived Unity build_*_101_1 sprite/mask cards as RokUnityRuntimeInteriorPreview actors. These add visual density for default preview maps and are not UnityInitId gameplay-state actors.",
            "render_queue_and_depth": "Unity ground/building shaders use ZWrite Off. UE ground/building materials are translucent and use Unity material.renderQueue as translucency sort priority so ground layers draw before city building sprite cards.",
            "runtime_camera": "Unity GameApp WorldCamera is perspective, but main-city entry uses CameraParamLod city_default: distance=20, vertical_fov=7.5, dxf=150, forward=(0,-0.7,0.7). This local diagnostic city is centered at Unity planar (0,0).",
        },
        "unity_runtime_boundary_obj_manifest": boundary.UNITY_RUNTIME_OBJ_MANIFEST,
        "unity_runtime_ground_obj_manifest": ground.UNITY_RUNTIME_GROUND_OBJ_MANIFEST,
        "unity_runtime_city_buildings_obj_manifest": UNITY_RUNTIME_CITY_BUILDINGS_OBJ_MANIFEST,
        "boundary_spawned": boundary_spawned,
        "ground_spawned": ground_spawned,
        "buildings_spawned": buildings_spawned,
        "extra_interior_buildings_requested": EXTRA_INTERIOR_BUILDING_COUNT,
        "extra_interior_buildings_spawned": extra_buildings_spawned,
        "camera_actor": camera.get_actor_label(),
        "player_start_actor": player_start.get_actor_label(),
        "camera": {
            "unity_source": "CameraParamLod.csv city_default",
            "view_center": list(CAMERA_VIEW_CENTER),
            "view_distance": CAMERA_VIEW_DISTANCE,
            "vertical_fov": CAMERA_VERTICAL_FOV,
            "horizontal_fov": vertical_to_horizontal_fov(CAMERA_VERTICAL_FOV, CAMERA_ASPECT),
            "dxf": CAMERA_DXF,
            "unity_forward": list(CAMERA_FORWARD_UNITY),
            "ue_location": [
                float(camera.get_actor_location().x),
                float(camera.get_actor_location().y),
                float(camera.get_actor_location().z),
            ],
            "ue_rotation": [
                float(camera.get_actor_rotation().pitch),
                float(camera.get_actor_rotation().yaw),
                float(camera.get_actor_rotation().roll),
            ],
        },
        "boundary_actors": boundary_records,
        "ground_actors": ground_records,
        "building_actors": building_records,
        "extra_interior_building_actors": extra_building_records,
        "exclusions": {
            "roads": "not spawned",
            "cameras": "one Unity city_default reference camera is spawned for runtime/editor visibility",
            "player_starts": "one Unity city_default player start is spawned for PIE/runtime visibility",
        },
    }
    os.makedirs(os.path.dirname(SCENE_MANIFEST), exist_ok=True)
    with open(SCENE_MANIFEST, "w", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)
    unreal.log("Rok Unity runtime city all-buildings map saved: {0}; boundary={1}, ground={2}, buildings={3}".format(
        TARGET_LEVEL,
        boundary_spawned,
        ground_spawned,
        buildings_spawned + extra_buildings_spawned,
    ))


if __name__ == "__main__":
    build_scene()
