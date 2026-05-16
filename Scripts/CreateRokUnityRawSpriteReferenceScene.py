import math
import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokPrototypeScene as proto


TARGET_LEVEL = os.environ.get("ROK_RAW_SPRITE_REFERENCE_LEVEL", "/Game/Maps/L_RokCitySingleBuilding")
SPRITE_ACTOR_CLASS = "/Script/Rok.RokCitySpriteActor"

SOURCE_SPRITE = os.environ.get(
    "ROK_RAW_SPRITE_SOURCE",
    os.path.join(PROJECT_ROOT, "ref", "resources", "Sprite", "Castle_6_5.png"),
)
TEXTURE_DESTINATION = "/Game/RokPrototype/Textures/UnityRawSpriteReference"
MATERIAL_DESTINATION = "/Game/RokPrototype/Materials/UnityRawSpriteReference"

UNITY_TO_UE_WORLD_SCALE = float(os.environ.get("ROK_RAW_SPRITE_WORLD_SCALE", "170.0"))
UNITY_CAMERA_ASPECT_RATIO = float(os.environ.get("ROK_RAW_SPRITE_CAMERA_ASPECT", str(16.0 / 9.0)))
UNITY_CAMERA_VERTICAL_FOV = float(os.environ.get("ROK_RAW_SPRITE_VERTICAL_FOV", "12.0"))

UNITY_SPRITE_POSITION = (
    float(os.environ.get("ROK_RAW_SPRITE_UNITY_X", "-0.104")),
    float(os.environ.get("ROK_RAW_SPRITE_UNITY_Y", "0.334")),
    float(os.environ.get("ROK_RAW_SPRITE_UNITY_Z", "0.079")),
)
UNITY_SPRITE_SCALE = (
    float(os.environ.get("ROK_RAW_SPRITE_SCALE_X", "0.24076737")),
    float(os.environ.get("ROK_RAW_SPRITE_SCALE_Y", "0.24076748")),
    float(os.environ.get("ROK_RAW_SPRITE_SCALE_Z", "0.24076748")),
)
UNITY_SPRITE_SIZE = (
    float(os.environ.get("ROK_RAW_SPRITE_SIZE_X", "8.62")),
    float(os.environ.get("ROK_RAW_SPRITE_SIZE_Y", "8.0")),
)
UNITY_SPRITE_ROTATION_X = float(os.environ.get("ROK_RAW_SPRITE_ROTATION_X", "45.0"))
ENGINE_PLANE_FLIP_Y = os.environ.get("ROK_RAW_SPRITE_ENGINE_PLANE_FLIP_Y", "1") != "0"

UNITY_CAMERA_POSITION = (
    float(os.environ.get("ROK_RAW_CAMERA_X", "0.52")),
    float(os.environ.get("ROK_RAW_CAMERA_Y", "15.82")),
    float(os.environ.get("ROK_RAW_CAMERA_Z", "-16.37")),
)
UNITY_CAMERA_ROTATION_X = float(os.environ.get("ROK_RAW_CAMERA_ROTATION_X", "45.0"))


def unity_position_to_ue(position):
    return unreal.Vector(
        -float(position[0]) * UNITY_TO_UE_WORLD_SCALE,
        float(position[2]) * UNITY_TO_UE_WORLD_SCALE,
        float(position[1]) * UNITY_TO_UE_WORLD_SCALE,
    )


def unity_direction_to_ue(direction):
    vector = unreal.Vector(-float(direction[0]), float(direction[2]), float(direction[1]))
    try:
        return vector.get_safe_normal()
    except Exception:
        length = max(0.0001, math.sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z))
        return unreal.Vector(vector.x / length, vector.y / length, vector.z / length)


def unity_rot_x_basis(rotation_x_degrees):
    radians = math.radians(rotation_x_degrees)
    c = math.cos(radians)
    s = math.sin(radians)
    unity_right = (1.0, 0.0, 0.0)
    unity_up = (0.0, c, s)
    unity_forward = (0.0, -s, c)
    return (
        unity_direction_to_ue(unity_right),
        unity_direction_to_ue(unity_up),
        unity_direction_to_ue(unity_forward),
    )


def make_rot_from_xy(x_axis, y_axis):
    for library_name in ("KismetMathLibrary", "MathLibrary"):
        library = getattr(unreal, library_name, None)
        function = getattr(library, "make_rot_from_xy", None) if library else None
        if function:
            return function(x_axis, y_axis)
    raise RuntimeError("Unreal Python is missing make_rot_from_xy; cannot build Unity sprite basis.")


def make_camera_rot_from_xz(forward_axis, up_axis):
    for library_name in ("KismetMathLibrary", "MathLibrary"):
        library = getattr(unreal, library_name, None)
        function = getattr(library, "make_rot_from_xz", None) if library else None
        if function:
            return function(forward_axis, up_axis)
    return unreal.MathLibrary.find_look_at_rotation(unity_position_to_ue(UNITY_CAMERA_POSITION), unity_position_to_ue(UNITY_SPRITE_POSITION))


def ue_horizontal_fov_from_unity_vertical_fov(vertical_fov, aspect_ratio):
    return math.degrees(2.0 * math.atan(math.tan(math.radians(vertical_fov) * 0.5) * aspect_ratio))


def import_raw_texture():
    if not os.path.exists(SOURCE_SPRITE):
        raise RuntimeError("Missing raw Unity sprite: {0}".format(SOURCE_SPRITE))

    asset_name = "T_UnityRaw_{0}".format(proto.safe_asset_name(os.path.splitext(os.path.basename(SOURCE_SPRITE))[0]))
    package_path = proto.asset_package_path(TEXTURE_DESTINATION, asset_name)
    existing = proto.load_asset_if_exists(package_path)
    if existing and os.environ.get("ROK_RAW_SPRITE_FORCE_REIMPORT", "0") != "1":
        return existing

    unreal.EditorAssetLibrary.make_directory(TEXTURE_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", SOURCE_SPRITE)
    task.set_editor_property("destination_path", TEXTURE_DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("factory", unreal.TextureFactory())
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", False)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = proto.load_asset_if_exists(package_path)
    if not texture:
        raise RuntimeError("Failed to import raw Unity sprite: {0}".format(SOURCE_SPRITE))

    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    try:
        texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    except Exception:
        pass
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def create_raw_sprite_material(texture):
    material_name = "M_UnityRawSpriteRenderer_{0}".format(proto.safe_asset_name(texture.get_name()))
    material_path = proto.asset_package_path(MATERIAL_DESTINATION, material_name)
    existing = proto.load_asset_if_exists(material_path)
    if existing and os.environ.get("ROK_RAW_SPRITE_FORCE_MATERIAL_REBUILD", "0") != "1":
        return existing
    if existing:
        unreal.EditorAssetLibrary.delete_asset(material_path)

    unreal.EditorAssetLibrary.make_directory(MATERIAL_DESTINATION)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        material_name,
        MATERIAL_DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    texture_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -520, -80
    )
    texture_node.set_editor_property("texture", texture)
    unreal.MaterialEditingLibrary.connect_material_property(texture_node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(texture_node, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(texture_node, "A", unreal.MaterialProperty.MP_OPACITY)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def configure_mesh_component(component, material, width, height):
    if not component:
        return
    component.set_material(0, material)
    y_scale = height / 100.0
    if ENGINE_PLANE_FLIP_Y:
        y_scale = -abs(y_scale)
    component.set_editor_property("relative_location", unreal.Vector(0.0, height * 0.5, 0.0))
    component.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 0.0))
    component.set_editor_property("relative_scale3d", unreal.Vector(width / 100.0, y_scale, 1.0))
    for prop, value in [
        ("cast_shadow", False),
        ("receive_decals", False),
        ("use_as_occluder", False),
        ("render_custom_depth", False),
        ("collision_enabled", unreal.CollisionEnabled.NO_COLLISION),
    ]:
        try:
            component.set_editor_property(prop, value)
        except Exception:
            pass


def spawn_raw_sprite_actor(material):
    actor_class = unreal.load_class(None, SPRITE_ACTOR_CLASS)
    if not actor_class:
        raise RuntimeError("Missing existing sprite actor class: {0}. This raw diagnostic does not use ARokBuildingActor.".format(SPRITE_ACTOR_CLASS))

    right_axis, up_axis, forward_axis = unity_rot_x_basis(UNITY_SPRITE_ROTATION_X)
    card_rotation = make_rot_from_xy(right_axis, up_axis)
    sprite_center = unity_position_to_ue(UNITY_SPRITE_POSITION)
    width = UNITY_SPRITE_SIZE[0] * UNITY_SPRITE_SCALE[0] * UNITY_TO_UE_WORLD_SCALE
    height = UNITY_SPRITE_SIZE[1] * UNITY_SPRITE_SCALE[1] * UNITY_TO_UE_WORLD_SCALE
    root_location = sprite_center - up_axis * (height * 0.5)

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, root_location, card_rotation)
    actor.set_actor_label("UnityRawSprite_Building_Castle_02_Castle_6_5")
    actor.set_folder_path("RokUnityRawSpriteReference/SpriteRenderer")
    actor.tags = [
        "RokUnityRawSpriteReference",
        "UnitySpriteRendererRaw",
        "Castle_6_5",
        "NoMask",
        "NoCleanCutout",
    ]

    try:
        actor.configure_sprite(material, width, height, 0)
        actor.configure_placement(unreal.IntPoint(1, 1), unreal.IntPoint(0, 0), unreal.Vector(0.0, height * 0.5, 0.0))
        actor.set_selection_collision_enabled(False)
    except Exception as exc:
        unreal.log_warning("Raw sprite actor configure fallback: {0}".format(exc))

    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    configure_mesh_component(component, material, width, height)

    unreal.log(
        "UnityRawSpriteReference actor: unity_pos=({0:.3f},{1:.3f},{2:.3f}), unity_rot_x={3:.2f}, "
        "unity_size=({4:.2f},{5:.2f}), unity_scale=({6:.6f},{7:.6f},{8:.6f}), "
        "ue_root=({9:.2f},{10:.2f},{11:.2f}), card_offset_local=(0,{12:.2f},0), "
        "card_size=({13:.2f},{14:.2f}), basis_right=({15:.3f},{16:.3f},{17:.3f}), "
        "basis_up=({18:.3f},{19:.3f},{20:.3f}), basis_forward=({21:.3f},{22:.3f},{23:.3f}), plane_flip_y={24}".format(
            UNITY_SPRITE_POSITION[0],
            UNITY_SPRITE_POSITION[1],
            UNITY_SPRITE_POSITION[2],
            UNITY_SPRITE_ROTATION_X,
            UNITY_SPRITE_SIZE[0],
            UNITY_SPRITE_SIZE[1],
            UNITY_SPRITE_SCALE[0],
            UNITY_SPRITE_SCALE[1],
            UNITY_SPRITE_SCALE[2],
            float(root_location.x),
            float(root_location.y),
            float(root_location.z),
            height * 0.5,
            width,
            height,
            float(right_axis.x),
            float(right_axis.y),
            float(right_axis.z),
            float(up_axis.x),
            float(up_axis.y),
            float(up_axis.z),
            float(forward_axis.x),
            float(forward_axis.y),
            float(forward_axis.z),
            ENGINE_PLANE_FLIP_Y,
        )
    )
    return actor


def spawn_unity_reference_camera():
    camera_forward = unity_direction_to_ue((0.0, -math.sin(math.radians(UNITY_CAMERA_ROTATION_X)), math.cos(math.radians(UNITY_CAMERA_ROTATION_X))))
    camera_up = unity_direction_to_ue((0.0, math.cos(math.radians(UNITY_CAMERA_ROTATION_X)), math.sin(math.radians(UNITY_CAMERA_ROTATION_X))))
    camera_location = unity_position_to_ue(UNITY_CAMERA_POSITION)
    camera_rotation = make_camera_rot_from_xz(camera_forward, camera_up)
    horizontal_fov = ue_horizontal_fov_from_unity_vertical_fov(UNITY_CAMERA_VERTICAL_FOV, UNITY_CAMERA_ASPECT_RATIO)

    camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.CameraActor, camera_location, camera_rotation)
    camera.set_actor_label("Camera_UnityRawSpriteReference")
    camera.set_folder_path("RokUnityRawSpriteReference/Camera")
    camera.tags = ["RokUnityRawSpriteReference", "UnityPerspectiveCamera", "VerticalFov12"]
    try:
        camera.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)
    except Exception:
        pass

    component = camera.get_component_by_class(unreal.CameraComponent)
    if component:
        try:
            component.set_editor_property("projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
        except Exception:
            pass
        component.set_editor_property("field_of_view", horizontal_fov)
        component.set_editor_property("aspect_ratio", UNITY_CAMERA_ASPECT_RATIO)
        component.set_editor_property("constrain_aspect_ratio", True)
        try:
            settings = component.get_editor_property("post_process_settings")
            settings.set_editor_property("override_auto_exposure_method", True)
            settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
            settings.set_editor_property("override_auto_exposure_min_brightness", True)
            settings.set_editor_property("auto_exposure_min_brightness", 1.0)
            settings.set_editor_property("override_auto_exposure_max_brightness", True)
            settings.set_editor_property("auto_exposure_max_brightness", 1.0)
            settings.set_editor_property("override_auto_exposure_bias", True)
            settings.set_editor_property("auto_exposure_bias", 0.0)
            component.set_editor_property("post_process_settings", settings)
        except Exception:
            pass

    try:
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(camera_location, camera_rotation)
    except Exception:
        pass

    unreal.log(
        "UnityRawSpriteReference camera: unity_pos=({0:.3f},{1:.3f},{2:.3f}), unity_rot_x={3:.2f}, "
        "vertical_fov={4:.2f}, ue_horizontal_fov={5:.2f}, ue_loc=({6:.2f},{7:.2f},{8:.2f})".format(
            UNITY_CAMERA_POSITION[0],
            UNITY_CAMERA_POSITION[1],
            UNITY_CAMERA_POSITION[2],
            UNITY_CAMERA_ROTATION_X,
            UNITY_CAMERA_VERTICAL_FOV,
            horizontal_fov,
            float(camera_location.x),
            float(camera_location.y),
            float(camera_location.z),
        )
    )
    return camera


def configure_reference_world_settings():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        return
    settings = world.get_world_settings()
    base_game_mode = unreal.load_class(None, "/Script/Engine.GameModeBase")
    if base_game_mode:
        settings.set_editor_property("default_game_mode", base_game_mode)


def build_raw_sprite_reference_scene():
    world = unreal.EditorLevelLibrary.get_editor_world()
    current_level_name = world.get_name() if world else ""
    target_level_name = TARGET_LEVEL.rsplit("/", 1)[-1]
    if current_level_name != target_level_name:
        raise RuntimeError(
            "Open {0} in the editor before running this script. Current world is {1}. "
            "Refusing to load or create maps through live MCP.".format(TARGET_LEVEL, current_level_name)
        )
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Target level asset does not exist: {0}".format(TARGET_LEVEL))

    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        unreal.EditorLevelLibrary.destroy_actor(actor)

    texture = import_raw_texture()
    material = create_raw_sprite_material(texture)
    spawn_raw_sprite_actor(material)
    spawn_unity_reference_camera()
    configure_reference_world_settings()

    world = unreal.EditorLevelLibrary.get_editor_world()
    saved = False
    try:
        saved = bool(unreal.EditorLevelLibrary.save_current_level())
    except Exception:
        saved = False
    if not saved and not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError("Failed to save raw Unity sprite reference level: {0}".format(TARGET_LEVEL))
    unreal.log("Saved raw Unity SpriteRenderer reference level: {0}".format(TARGET_LEVEL))


def main():
    build_raw_sprite_reference_scene()


if __name__ == "__main__":
    main()
