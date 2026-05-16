import os
import sys
import csv
import ast
import math
import json

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokPrototypeScene as proto
import CreateRokRuntimeCityScene as runtime


TARGET_LEVEL = os.environ.get("ROK_SINGLE_BUILDING_LEVEL", "/Game/Maps/L_RokCitySingleBuilding")
DIAGNOSTIC_BUILDING_TYPE = int(os.environ.get("ROK_SINGLE_BUILDING_TYPE", "1"))
DIAGNOSTIC_VISUAL_SCALE = float(os.environ.get("ROK_SINGLE_BUILDING_VISUAL_SCALE", "0.62"))
SINGLE_CAMERA_PAWN_BLUEPRINT = runtime.RUNTIME_BLUEPRINT_DESTINATION + "/BP_RokSingleBuildingCameraPawn"
SINGLE_GAME_MODE_BLUEPRINT = runtime.RUNTIME_BLUEPRINT_DESTINATION + "/BP_RokSingleBuildingGameMode"
ROK_BUILDING_ACTOR_CLASS = "/Script/Rok.RokBuildingActor"
ROK_CAMERA_PAWN_CLASS = "/Script/Rok.RokCameraPawn"
SINGLE_ADD_GROUND = os.environ.get("ROK_SINGLE_BUILDING_ADD_GROUND", "0") == "1"
SINGLE_ADD_UNITY_CITY_GROUND = os.environ.get("ROK_SINGLE_BUILDING_ADD_UNITY_CITY_GROUND", "1") != "0"
SINGLE_ADD_UNITY_CITY_SURFACE = os.environ.get("ROK_SINGLE_BUILDING_ADD_UNITY_CITY_SURFACE", "1") != "0"
SINGLE_ADD_UNITY_CITY_ROAD = os.environ.get("ROK_SINGLE_BUILDING_ADD_UNITY_CITY_ROAD", "1") != "0"
SINGLE_ADD_UNITY_GROUND_PATCH = os.environ.get("ROK_SINGLE_BUILDING_ADD_UNITY_GROUND_PATCH", "1") != "0"
SINGLE_ADD_UNITY_TERRAIN_PLATE = os.environ.get("ROK_SINGLE_BUILDING_ADD_UNITY_TERRAIN_PLATE", "0") == "1"
SINGLE_ADD_SHADOW = os.environ.get("ROK_SINGLE_BUILDING_ADD_SHADOW", "0") == "1"
SINGLE_SPLIT_BUILDING_GROUND = os.environ.get("ROK_SINGLE_BUILDING_SPLIT_GROUND", "0") == "1"
UNITY_SPRITE_TERRAIN_LAYER_OFFSET = float(os.environ.get("ROK_SINGLE_BUILDING_TERRAIN_LAYER_OFFSET", "26.0"))
UNITY_SPRITE_GROUND_LAYER_OFFSET = float(os.environ.get("ROK_SINGLE_BUILDING_GROUND_LAYER_OFFSET", "14.0"))
UNITY_SPRITE_BUILDING_LAYER_OFFSET = float(os.environ.get("ROK_SINGLE_BUILDING_BUILDING_LAYER_OFFSET", "-38.0"))
UNITY_PLACEMENT_CSV = os.path.join(PROJECT_ROOT, "Saved", "RokUnityPlacement", "unity_city_placement.csv")
UNITY_SCENE_NAME = os.environ.get("ROK_SINGLE_BUILDING_UNITY_SCENE", "Map_TTS.unity")
UNITY_BUILDING_NAME = os.environ.get("ROK_SINGLE_BUILDING_UNITY_NAME", "Building_Castle_02")
UNITY_SCENE_WORLD_SCALE = float(os.environ.get("ROK_SINGLE_BUILDING_SCENE_WORLD_SCALE", "170.0"))
UNITY_SCENE_VISUAL_SCALE = float(os.environ.get("ROK_SINGLE_BUILDING_SCENE_VISUAL_SCALE", "1.0"))
UNITY_CAMERA_VERTICAL_FOV = float(os.environ.get("ROK_SINGLE_BUILDING_UNITY_CAMERA_FOV", "12.0"))
UNITY_CAMERA_ASPECT_RATIO = float(os.environ.get("ROK_SINGLE_BUILDING_CAMERA_ASPECT", str(16.0 / 9.0)))
UNITY_CAMERA_POSITION = (
    float(os.environ.get("ROK_SINGLE_BUILDING_UNITY_CAMERA_X", "0.52")),
    float(os.environ.get("ROK_SINGLE_BUILDING_UNITY_CAMERA_Y", "15.82")),
    float(os.environ.get("ROK_SINGLE_BUILDING_UNITY_CAMERA_Z", "-16.37")),
)
UNITY_CAMERA_FORWARD = (0.0, -0.70710678, 0.70710678)
UNITY_CAMERA_UP = (0.0, 0.70710678, 0.70710678)
UNITY_SPRITE_MASK_CHANNEL = os.environ.get("ROK_SINGLE_BUILDING_MASK_CHANNEL", "R").upper()
UNITY_SPRITE_OPACITY_CLIP = float(os.environ.get("ROK_SINGLE_BUILDING_OPACITY_CLIP", "0.5"))
UNITY_SPRITE_FLIP_X = os.environ.get("ROK_SINGLE_BUILDING_FLIP_X", "0") == "1"
UNITY_ENGINE_PLANE_FLIP_Y = os.environ.get("ROK_SINGLE_BUILDING_ENGINE_PLANE_FLIP_Y", "1") != "0"
UNITY_USE_CLEAN_CUTOUT = os.environ.get("ROK_SINGLE_BUILDING_USE_CLEAN_CUTOUT", "1") != "0"
UNITY_CENTER_DIAGNOSTIC_CAMERA = os.environ.get("ROK_SINGLE_BUILDING_CENTER_CAMERA", "1") != "0"
CLEAN_CUTOUT_DIR = os.path.join(PROJECT_ROOT, "Saved", "RokUnityPlacement", "clean")
UNITY_CITY_GROUND_STYLE = os.environ.get("ROK_SINGLE_BUILDING_CITY_GROUND_STYLE", "city_background").lower()
UNITY_CITY_GROUND_TEXTURE = os.environ.get(
    "ROK_SINGLE_BUILDING_CITY_GROUND_TEXTURE",
    os.path.join(
        CLEAN_CUTOUT_DIR,
        {
            "sand": "UnityCityGround_build_ground_6_101_1.png",
            "grass": "UnityGrassGround_I_TYPE_Grass_01.png",
            "city_splat": "UnityCitySplatGround_city_splat_ground_alpha.png",
            "city_surface": "UnityCitySurface_map_incity4.png",
        }.get(UNITY_CITY_GROUND_STYLE, "UnityCityBackground_Terrain_grass_08_3g2.png"),
    ),
)
UNITY_CITY_GROUND_FALLBACK_TEXTURE = os.path.join(
    PROJECT_ROOT,
    "ref",
    "client-unity",
    "Assets",
    "BundleAssets",
    "Map",
    "Textures",
    "build_ground_6_101_1.psd" if UNITY_CITY_GROUND_STYLE == "sand" else "Terrian_Maps/TR_Grass_01.png",
)
UNITY_CITY_GROUND_POSITION = (
    float(os.environ.get("ROK_SINGLE_BUILDING_CITY_GROUND_X", "0.01499939")),
    float(os.environ.get("ROK_SINGLE_BUILDING_CITY_GROUND_Y", "-0.012")),
    float(os.environ.get("ROK_SINGLE_BUILDING_CITY_GROUND_Z", "-0.072006226")),
)
UNITY_CITY_GROUND_SIZE_UNITS = float(os.environ.get("ROK_SINGLE_BUILDING_CITY_GROUND_SIZE_UNITS", str(41.0 * 1.1)))
UNITY_CITY_GROUND_YAW = float(os.environ.get("ROK_SINGLE_BUILDING_CITY_GROUND_YAW", "45.0"))
UNITY_CITY_SURFACE_TEXTURE = os.environ.get(
    "ROK_SINGLE_BUILDING_CITY_SURFACE_TEXTURE",
    os.path.join(CLEAN_CUTOUT_DIR, "UnityCitySurface_map_incity4.png"),
)
UNITY_CITY_ROAD_TEXTURE = os.environ.get(
    "ROK_SINGLE_BUILDING_CITY_ROAD_TEXTURE",
    os.path.join(CLEAN_CUTOUT_DIR, "UnityCityRoad_dirtRoad01.png"),
)
UNITY_CITY_SURFACE_SCALE = float(os.environ.get("ROK_SINGLE_BUILDING_CITY_SURFACE_SCALE", "0.95"))
UNITY_CITY_ROAD_SCALE = float(os.environ.get("ROK_SINGLE_BUILDING_CITY_ROAD_SCALE", "1.15"))
DEFAULT_SINGLE_SPRITE_BASE = "Castle_6_5"
DEFAULT_SINGLE_SPRITE_PATH = os.path.join(PROJECT_ROOT, "ref", "resources", "Sprite", "{0}.png".format(DEFAULT_SINGLE_SPRITE_BASE))
DEFAULT_SINGLE_MASK_PATH = os.path.join(PROJECT_ROOT, "ref", "resources", "Sprite", "{0}_mask.png".format(DEFAULT_SINGLE_SPRITE_BASE))
SINGLE_FOOTPRINT_WIDTH = float(os.environ.get("ROK_SINGLE_BUILDING_FOOTPRINT_WIDTH", "460.0"))
SINGLE_FOOTPRINT_LENGTH = float(os.environ.get("ROK_SINGLE_BUILDING_FOOTPRINT_LENGTH", "330.0"))
SINGLE_FOOTPRINT_YAW = float(os.environ.get("ROK_SINGLE_BUILDING_FOOTPRINT_YAW", "45.0"))
SINGLE_CAMERA_ORTHO_WIDTH = float(os.environ.get("ROK_SINGLE_BUILDING_ORTHO_WIDTH", "1800.0"))


def parse_vector(value, default=None):
    if default is None:
        default = []
    try:
        parsed = ast.literal_eval(value)
    except Exception:
        return default
    return parsed if isinstance(parsed, (list, tuple)) else default


def resolve_project_path(path):
    if not path:
        return None
    candidate = path.replace("/", os.sep)
    if os.path.isabs(candidate):
        return candidate if os.path.exists(candidate) else None
    absolute = os.path.join(PROJECT_ROOT, candidate)
    return absolute if os.path.exists(absolute) else None


def load_clean_cutout_meta(sprite_base):
    if not UNITY_USE_CLEAN_CUTOUT:
        return None
    meta_path = os.path.join(CLEAN_CUTOUT_DIR, "{0}_main_clean_meta.json".format(sprite_base))
    if not os.path.exists(meta_path):
        return None
    with open(meta_path, "r", encoding="utf-8") as meta_file:
        meta = json.load(meta_file)
    clean_path = resolve_project_path(meta.get("clean_cropped"))
    if not clean_path:
        return None
    return meta


def unity_position_to_ue(position):
    return unreal.Vector(
        -float(position[0]) * UNITY_SCENE_WORLD_SCALE,
        float(position[2]) * UNITY_SCENE_WORLD_SCALE,
        float(position[1]) * UNITY_SCENE_WORLD_SCALE,
    )


def unity_direction_to_ue(direction):
    vector = unreal.Vector(-float(direction[0]), float(direction[2]), float(direction[1]))
    try:
        return vector.get_safe_normal()
    except Exception:
        size = max(0.0001, (vector.x * vector.x + vector.y * vector.y + vector.z * vector.z) ** 0.5)
        return unreal.Vector(vector.x / size, vector.y / size, vector.z / size)


def vector_dot(a, b):
    return float(a.x) * float(b.x) + float(a.y) * float(b.y) + float(a.z) * float(b.z)


def unreal_math_libraries():
    libraries = []
    for name in ("MathLibrary", "KismetMathLibrary"):
        library = getattr(unreal, name, None)
        if library:
            libraries.append(library)
    return libraries


def ue_horizontal_fov_from_unity_vertical_fov(vertical_fov, aspect_ratio):
    return math.degrees(2.0 * math.atan(math.tan(math.radians(vertical_fov) * 0.5) * aspect_ratio))


def make_unity_sprite_rotation():
    normal = unity_direction_to_ue(UNITY_CAMERA_FORWARD)
    up = unity_direction_to_ue(UNITY_CAMERA_UP)
    for library in unreal_math_libraries():
        function = getattr(library, "make_rot_from_zy", None)
        if function:
            try:
                return function(normal, up)
            except Exception:
                pass
    return unreal.MathLibrary.find_look_at_rotation(unreal.Vector(0.0, 0.0, 0.0), normal)


def unity_sprite_layer_location(location, camera_forward_offset):
    normal = unity_direction_to_ue(UNITY_CAMERA_FORWARD)
    return unreal.Vector(
        float(location.x) + float(normal.x) * camera_forward_offset,
        float(location.y) + float(normal.y) * camera_forward_offset,
        float(location.z) + float(normal.z) * camera_forward_offset,
    )


def spawn_unity_city_ground(primitives):
    ground_texture_path = UNITY_CITY_GROUND_TEXTURE
    if not os.path.exists(ground_texture_path):
        ground_texture_path = UNITY_CITY_GROUND_FALLBACK_TEXTURE
    if not os.path.exists(ground_texture_path):
        unreal.log_warning("Unity city ground texture is missing: {0}".format(ground_texture_path))
        return None

    if UNITY_CITY_GROUND_STYLE == "sand":
        ground_asset_name = "UnityCityGround_build_ground_6_101_1"
    elif UNITY_CITY_GROUND_STYLE == "grass":
        ground_asset_name = "UnityCityGround_I_TYPE_Grass_01"
    elif UNITY_CITY_GROUND_STYLE == "city_splat":
        ground_asset_name = "UnityCityGround_city_splat_ground_alpha"
    elif UNITY_CITY_GROUND_STYLE == "city_surface":
        ground_asset_name = "UnityCitySurface_map_incity4"
    else:
        ground_asset_name = "UnityCityBackground_Terrain_grass_08_3g2"
    texture = runtime.import_runtime_texture_as(
        ground_texture_path,
        ground_asset_name,
    )
    if not texture:
        raise RuntimeError("Failed to import Unity city ground texture: {0}".format(ground_texture_path))

    material = runtime.create_runtime_city_sprite_material(
        texture,
        ground_asset_name,
        None,
        False,
        False,
        "A",
        0.05,
    )
    ground_size = UNITY_CITY_GROUND_SIZE_UNITS * UNITY_SCENE_WORLD_SCALE
    location = unity_position_to_ue(UNITY_CITY_GROUND_POSITION)
    actor = runtime.spawn_runtime_plane(
        "RokSingleUnityCityGround_{0}".format(UNITY_CITY_GROUND_STYLE),
        primitives,
        (float(location.x), float(location.y), float(location.z)),
        ground_size,
        ground_size,
        material,
        "RokSingleBuilding/UnityGround",
        yaw=UNITY_CITY_GROUND_YAW,
        sort_priority=-10000,
    )
    if actor:
        actor.tags = ["RokTerrainLayer", "RokSingleBuildingUnityCityGround", "RokUnityPlacementSource"]
    return actor


def import_layer_material(texture_path, asset_name, masked=False):
    if not os.path.exists(texture_path):
        unreal.log_warning("Unity layer texture is missing: {0}".format(texture_path))
        return None
    texture = runtime.import_runtime_texture_as(texture_path, asset_name)
    if not texture:
        raise RuntimeError("Failed to import Unity layer texture: {0}".format(texture_path))
    return runtime.create_runtime_city_sprite_material(
        texture,
        asset_name,
        None,
        False,
        False,
        "A",
        0.05,
    )


def spawn_unity_city_surface(primitives, center_location, visual_width, visual_height):
    material = import_layer_material(UNITY_CITY_SURFACE_TEXTURE, "UnityCitySurface_map_incity4", True)
    if not material:
        return None
    surface_width = max(visual_width * UNITY_CITY_SURFACE_SCALE, 520.0)
    surface_height = max(visual_height * (UNITY_CITY_SURFACE_SCALE * 0.64), 360.0)
    actor = runtime.spawn_runtime_plane(
        "RokSingleUnityCitySurface",
        primitives,
        (float(center_location.x), float(center_location.y), float(center_location.z) - 8.0),
        surface_width,
        surface_height,
        material,
        "RokSingleBuilding/UnityGround",
        yaw=45.0,
        sort_priority=-9000,
    )
    if actor:
        actor.tags = ["RokTerrainLayer", "RokSingleBuildingCitySurface", "RokUnityPlacementSource"]
    return actor


def spawn_unity_city_road(primitives, center_location, visual_width, visual_height):
    material = import_layer_material(UNITY_CITY_ROAD_TEXTURE, "UnityCityRoad_dirtRoad01", True)
    if not material:
        return None
    road_width = max(visual_width * UNITY_CITY_ROAD_SCALE, 520.0)
    road_height = max(visual_height * 0.38, 190.0)
    road_location = unreal.Vector(
        float(center_location.x),
        float(center_location.y) - visual_height * 0.24,
        float(center_location.z) - 5.0,
    )
    actor = runtime.spawn_runtime_plane(
        "RokSingleUnityCityRoad",
        primitives,
        (float(road_location.x), float(road_location.y), float(road_location.z)),
        road_width,
        road_height,
        material,
        "RokSingleBuilding/UnityGround",
        yaw=45.0,
        sort_priority=-8500,
    )
    if actor:
        actor.tags = ["RokTerrainLayer", "RokSingleBuildingCityRoad", "RokUnityPlacementSource"]
    return actor


def make_unity_camera_rotation():
    forward = unity_direction_to_ue(UNITY_CAMERA_FORWARD)
    up = unity_direction_to_ue(UNITY_CAMERA_UP)
    for library in unreal_math_libraries():
        for function_name in ("make_rot_from_xz", "make_rot_from_zx", "make_rot_from_x"):
            function = getattr(library, function_name, None)
            if function:
                try:
                    if function_name == "make_rot_from_zx":
                        return function(up, forward)
                    if function_name == "make_rot_from_x":
                        return function(forward)
                    return function(forward, up)
                except Exception:
                    pass
    camera_location = unity_position_to_ue(UNITY_CAMERA_POSITION)
    return unreal.MathLibrary.find_look_at_rotation(camera_location, camera_location + forward)


def load_unity_scene_building_row():
    if not os.path.exists(UNITY_PLACEMENT_CSV):
        if not os.path.exists(DEFAULT_SINGLE_SPRITE_PATH):
            raise RuntimeError("Missing default single-building sprite: {0}".format(DEFAULT_SINGLE_SPRITE_PATH))
        unreal.log_warning(
            "Unity placement CSV is missing; using fixed single-building diagnostic source {0}.".format(
                DEFAULT_SINGLE_SPRITE_PATH
            )
        )
        return {
            "scene": UNITY_SCENE_NAME,
            "category": "city_building",
            "active": "True",
            "name": UNITY_BUILDING_NAME,
            "mapped_sprite_path": os.path.relpath(DEFAULT_SINGLE_SPRITE_PATH, PROJECT_ROOT).replace("\\", "/"),
            "world_position": "[-0.104, 0.334, 0.079]",
            "world_scale": "[0.240767, 0.240767, 0.240767]",
            "sprite_size": "[0.0, 0.0]",
        }

    fallback = None
    with open(UNITY_PLACEMENT_CSV, "r", encoding="utf-8-sig", newline="") as csv_file:
        for row in csv.DictReader(csv_file):
            if row.get("scene") != UNITY_SCENE_NAME:
                continue
            if row.get("category") != "city_building":
                continue
            if row.get("active") != "True":
                continue
            if not row.get("mapped_sprite_path"):
                continue
            if row.get("name") == UNITY_BUILDING_NAME:
                return row
            if fallback is None:
                fallback = row

    if fallback:
        unreal.log_warning(
            "Unity building {0} was not found in {1}; using first active city building {2}.".format(
                UNITY_BUILDING_NAME,
                UNITY_SCENE_NAME,
                fallback.get("name"),
            )
        )
        return fallback
    raise RuntimeError("No active Unity city building rows found in {0}".format(UNITY_PLACEMENT_CSV))


def find_diagnostic_model(building_models):
    model_id = building_models.get(DIAGNOSTIC_BUILDING_TYPE)
    if model_id:
        return model_id
    return "build_1_101_{0}".format(runtime.AGE)


def make_footprint_highlight_material():
    return runtime.create_unlit_constant_material(
        "M_RokSingleBuilding_FootprintHighlight",
        unreal.LinearColor(0.95, 0.72, 0.18, 1.0),
    )


def spawn_rook_building_actor(label, material, root_location, width, height, pivot_offset, footprint_width, footprint_length, mask_source, clean_meta):
    building_actor_class = unreal.load_class(None, ROK_BUILDING_ACTOR_CLASS)
    if not building_actor_class:
        raise RuntimeError("Missing building actor class: {0}. Build RokEditor before running this script.".format(ROK_BUILDING_ACTOR_CLASS))

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(building_actor_class, root_location, unreal.Rotator(0.0, 0.0, 0.0))
    actor.set_actor_label(label)
    actor.set_folder_path("RokSingleBuilding/Building")
    actor.configure_card(material, width, height, pivot_offset, make_unity_sprite_rotation())
    actor.configure_footprint(footprint_width, footprint_length, make_footprint_highlight_material(), SINGLE_FOOTPRINT_YAW)
    actor.tags = ["RokBuilding", "CityBuilding", "RokSingleBuildingDiagnostic", "RokUnityPlacementSource"]

    card_component = None
    try:
        card_component = actor.get_card_mesh_component()
    except Exception:
        pass
    if not card_component:
        card_component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if card_component:
        try:
            card_component.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
        except Exception:
            pass
        try:
            card_component.set_editor_property("render_custom_depth", False)
        except Exception:
            pass
        if UNITY_SPRITE_FLIP_X or UNITY_ENGINE_PLANE_FLIP_Y:
            scale = card_component.get_editor_property("relative_scale3d")
            card_component.set_editor_property(
                "relative_scale3d",
                unreal.Vector(
                    -abs(float(scale.x)) if UNITY_SPRITE_FLIP_X else float(scale.x),
                    -abs(float(scale.y)) if UNITY_ENGINE_PLANE_FLIP_Y else float(scale.y),
                    float(scale.z),
                ),
            )

    bounds_text = "unavailable"
    try:
        bounds_origin, bounds_extent = actor.get_actor_bounds(False)
        bounds_text = "origin=({0:.1f},{1:.1f},{2:.1f}) extent=({3:.1f},{4:.1f},{5:.1f})".format(
            float(bounds_origin.x),
            float(bounds_origin.y),
            float(bounds_origin.z),
            float(bounds_extent.x),
            float(bounds_extent.y),
            float(bounds_extent.z),
        )
    except Exception:
        pass
    unreal.log(
        "ROKBuildingActor: root=({0:.1f},{1:.1f},{2:.1f}), visual={3:.1f}x{4:.1f}, pivot=({5:.1f},{6:.1f},{7:.1f}), footprint={8:.1f}x{9:.1f}, mask={10}, clip={11:.2f}, clean_crop={12}, bounds={13}".format(
            float(root_location.x),
            float(root_location.y),
            float(root_location.z),
            width,
            height,
            float(pivot_offset.x),
            float(pivot_offset.y),
            float(pivot_offset.z),
            footprint_width,
            footprint_length,
            mask_source,
            UNITY_SPRITE_OPACITY_CLIP,
            clean_meta.get("crop_box") if clean_meta else None,
            bounds_text,
        )
    )
    return actor


def spawn_single_building(primitives, materials):
    unity_row = load_unity_scene_building_row()
    source_path = resolve_project_path(unity_row.get("mapped_sprite_path"))
    if not source_path:
        raise RuntimeError("Missing Unity mapped sprite path for {0}: {1}".format(unity_row.get("name"), unity_row.get("mapped_sprite_path")))

    sprite_base = os.path.splitext(os.path.basename(source_path))[0]
    clean_meta = load_clean_cutout_meta(sprite_base)
    source_size_scale = [1.0, 1.0]
    if clean_meta:
        clean_key = "body_clean_cropped" if SINGLE_SPLIT_BUILDING_GROUND and clean_meta.get("body_clean_cropped") else "clean_cropped"
        source_path = resolve_project_path(clean_meta[clean_key])
        source_size = clean_meta.get("source_size") or [1.0, 1.0]
        crop_size = clean_meta.get("crop_size") or source_size
        source_size_scale = [
            float(crop_size[0]) / float(max(1.0, source_size[0])),
            float(crop_size[1]) / float(max(1.0, source_size[1])),
        ]
        mask_path = None
        mask_source = "clean RGBA alpha"
    else:
        mask_path = os.path.join(os.path.dirname(source_path), "{0}_mask.png".format(sprite_base))
        if not os.path.exists(mask_path):
            mask_path = None
        mask_source = "mask {0}".format(UNITY_SPRITE_MASK_CHANNEL) if mask_path else "source alpha"

    texture_name = "UnitySingle_{0}{1}".format(sprite_base, "_CleanMain" if clean_meta else "")
    texture = runtime.import_runtime_texture_as(source_path, texture_name)
    if not texture:
        raise RuntimeError("Failed to import Unity scene building texture: {0}".format(source_path))

    material = runtime.create_runtime_city_sprite_material(
        texture,
        texture_name,
        mask_path,
        False,
        False,
        UNITY_SPRITE_MASK_CHANNEL,
        UNITY_SPRITE_OPACITY_CLIP,
    )
    sprite_size = parse_vector(unity_row.get("sprite_size"), [0.0, 0.0])
    world_scale = parse_vector(unity_row.get("world_scale"), [1.0, 1.0, 1.0])
    tex_width, tex_height = runtime.texture_dimensions(texture)
    visual_width = float(sprite_size[0]) * float(world_scale[0]) * UNITY_SCENE_WORLD_SCALE * UNITY_SCENE_VISUAL_SCALE
    visual_height = float(sprite_size[1]) * float(world_scale[1]) * UNITY_SCENE_WORLD_SCALE * UNITY_SCENE_VISUAL_SCALE
    visual_width *= source_size_scale[0]
    visual_height *= source_size_scale[1]
    if visual_width <= 1.0 or visual_height <= 1.0:
        visual_width, visual_height = runtime.prefab_visual_size(tex_width, tex_height)
        visual_width *= UNITY_SCENE_VISUAL_SCALE
        visual_height *= UNITY_SCENE_VISUAL_SCALE

    world_position = parse_vector(unity_row.get("world_position"), [0.0, 0.0, 0.0])
    location = unity_position_to_ue(world_position)
    root_location = unreal.Vector(float(location.x), float(location.y), 0.0)

    if SINGLE_ADD_UNITY_CITY_GROUND:
        spawn_unity_city_ground(primitives)
    if SINGLE_ADD_UNITY_CITY_SURFACE:
        spawn_unity_city_surface(primitives, root_location, visual_width, visual_height)
    if SINGLE_ADD_UNITY_CITY_ROAD:
        spawn_unity_city_road(primitives, root_location, visual_width, visual_height)

    if SINGLE_ADD_SHADOW:
        runtime.spawn_shadow_and_pad(
            unity_row.get("name") or sprite_base,
            primitives,
            materials,
            float(location.x),
            float(location.y),
            max(140.0, visual_width * 0.45),
            max(100.0, visual_height * 0.32),
            visual_width,
        )
    if SINGLE_ADD_UNITY_TERRAIN_PLATE and clean_meta and clean_meta.get("terrain_plate"):
        terrain_path = resolve_project_path(clean_meta.get("terrain_plate"))
        if terrain_path:
            terrain_texture = runtime.import_runtime_texture_as(terrain_path, "{0}_UnityTerrainPlate".format(texture_name))
            if terrain_texture:
                terrain_material = runtime.create_runtime_city_sprite_material(
                    terrain_texture,
                    "{0}_UnityTerrainPlate".format(texture_name),
                    None,
                    False,
                    False,
                    "A",
                    0.05,
                )
                terrain_size = clean_meta.get("terrain_plate_size") or clean_meta.get("crop_size") or [1.0, 1.0]
                crop_size = clean_meta.get("crop_size") or terrain_size
                terrain_width = visual_width * float(terrain_size[0]) / float(max(1.0, crop_size[0]))
                terrain_height = visual_height * float(terrain_size[1]) / float(max(1.0, crop_size[1]))
                spawn_unity_sprite_actor(
                    "RokSingleUnityTerrain_{0}_{1}".format(proto.safe_asset_name(unity_row.get("name") or "Building"), sprite_base),
                    terrain_material,
                    unity_sprite_layer_location(location, UNITY_SPRITE_TERRAIN_LAYER_OFFSET),
                    terrain_width,
                    terrain_height,
                    int((6000.0 - (float(location.x) + float(location.y))) * 10.0) - 500,
                    ["RokTerrainLayer", "RokSingleBuildingTerrainPlate", "RokUnityPlacementSource"],
                )
    if SINGLE_ADD_UNITY_GROUND_PATCH and clean_meta and clean_meta.get("ground_patch"):
        ground_path = resolve_project_path(clean_meta.get("ground_patch"))
        if ground_path:
            ground_texture = runtime.import_runtime_texture_as(ground_path, "{0}_UnityGroundPatch".format(texture_name))
            if ground_texture:
                ground_material = runtime.create_runtime_city_sprite_material(
                    ground_texture,
                    "{0}_UnityGroundPatch".format(texture_name),
                    None,
                    False,
                    False,
                    "A",
                    0.20,
                )
                spawn_unity_sprite_actor(
                    "RokSingleUnityGround_{0}_{1}".format(proto.safe_asset_name(unity_row.get("name") or "Building"), sprite_base),
                    ground_material,
                    unity_sprite_layer_location(location, UNITY_SPRITE_GROUND_LAYER_OFFSET),
                    visual_width,
                    visual_height,
                    int((6000.0 - (float(location.x) + float(location.y))) * 10.0) - 250,
                    ["RokTerrainLayer", "RokSingleBuildingGroundPatch", "RokUnityPlacementSource"],
                )
    camera_forward = unity_direction_to_ue(UNITY_CAMERA_FORWARD)
    camera_up = unity_direction_to_ue(UNITY_CAMERA_UP)
    pivot_offset = unreal.Vector(
        float(camera_forward.x) * UNITY_SPRITE_BUILDING_LAYER_OFFSET + float(camera_up.x) * visual_height * 0.5,
        float(camera_forward.y) * UNITY_SPRITE_BUILDING_LAYER_OFFSET + float(camera_up.y) * visual_height * 0.5,
        float(camera_forward.z) * UNITY_SPRITE_BUILDING_LAYER_OFFSET + float(camera_up.z) * visual_height * 0.5,
    )
    actor = spawn_rook_building_actor(
        "RokSingleUnity_{0}_{1}".format(proto.safe_asset_name(unity_row.get("name") or "Building"), sprite_base),
        material,
        root_location,
        visual_width,
        visual_height,
        pivot_offset,
        SINGLE_FOOTPRINT_WIDTH,
        SINGLE_FOOTPRINT_LENGTH,
        mask_source,
        clean_meta or {},
    )
    unreal.log(
        "Single Unity building: scene={0}, name={1}, sprite={2}, unity_world=({3:.3f},{4:.3f},{5:.3f}), ue=({6:.1f},{7:.1f}), visual={8:.1f}x{9:.1f}, texture={10}x{11}, mask={12}, clip={13:.2f}, clean={14}".format(
            unity_row.get("scene"),
            unity_row.get("name"),
            os.path.basename(source_path),
            float(world_position[0]) if len(world_position) > 0 else 0.0,
            float(world_position[1]) if len(world_position) > 1 else 0.0,
            float(world_position[2]) if len(world_position) > 2 else 0.0,
            float(location.x),
            float(location.y),
            visual_width,
            visual_height,
            tex_width,
            tex_height,
            UNITY_SPRITE_MASK_CHANNEL,
            UNITY_SPRITE_OPACITY_CLIP,
            bool(clean_meta),
        )
    )
    return actor


def spawn_unity_sprite_actor(label, material, location, width, height, sort_priority, extra_tags=None):
    sprite_actor_class = unreal.load_class(None, runtime.CITY_SPRITE_ACTOR_CLASS)
    if not sprite_actor_class:
        raise RuntimeError("Missing runtime sprite actor class: {0}".format(runtime.CITY_SPRITE_ACTOR_CLASS))

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(sprite_actor_class, location, make_unity_sprite_rotation())
    actor.set_actor_label(label)
    actor.set_folder_path("RokSingleBuilding/UnityPlacement")
    actor.configure_sprite(material, width, height, sort_priority)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if component:
        component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))
        component.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 0.0))
        x_scale = width / 100.0
        y_scale = height / 100.0
        if UNITY_SPRITE_FLIP_X:
            x_scale = -abs(x_scale)
        if UNITY_ENGINE_PLANE_FLIP_Y:
            y_scale = -abs(y_scale)
        component.set_editor_property("relative_scale3d", unreal.Vector(x_scale, y_scale, 1.0))
    try:
        actor.configure_placement(unreal.IntPoint(1, 1), unreal.IntPoint(0, 0), unreal.Vector(0.0, 0.0, 0.0))
        actor.set_selection_collision_enabled(True)
    except Exception:
        pass
    tags = ["RokBuilding", "CityBuilding", "RokSingleBuildingDiagnostic", "RokUnityPlacementSource"]
    if extra_tags:
        tags = extra_tags
    actor.tags = tags
    return actor


def spawn_legacy_config_building(primitives, materials):
    building_models = runtime.load_building_model_map()
    building_types = runtime.load_building_type_map()
    model_id = find_diagnostic_model(building_models)
    source_path = runtime.building_texture_path(model_id)
    mask_path = runtime.building_mask_path(model_id)
    if not source_path:
        source_path = runtime.fallback_building_texture_path(DIAGNOSTIC_BUILDING_TYPE)
        mask_path = runtime.fallback_building_mask_path(DIAGNOSTIC_BUILDING_TYPE)
    if not source_path:
        raise RuntimeError("Missing diagnostic building texture for model: {0}".format(model_id))

    texture = proto.import_city_sprite_texture(source_path)
    if not texture:
        raise RuntimeError("Failed to import diagnostic building texture: {0}".format(source_path))

    material = runtime.create_building_material(texture, model_id, mask_path)
    tex_width, tex_height = runtime.texture_dimensions(texture)
    footprint = building_types.get(DIAGNOSTIC_BUILDING_TYPE, {"width": 6, "length": 6})
    footprint_width = max(1, footprint["width"]) * runtime.UNITY_GRID_SIZE * runtime.UE_UNITS_PER_UNITY_UNIT
    footprint_length = max(1, footprint["length"]) * runtime.UNITY_GRID_SIZE * runtime.UE_UNITS_PER_UNITY_UNIT
    if runtime.SPRITE_PLANE_MODE in ("billboard", "camera", "prefab"):
        visual_width, visual_height = runtime.prefab_visual_size(tex_width, tex_height)
    else:
        visual_width = max(420.0, max(footprint_width, footprint_length) * 1.62)
        visual_height = visual_width * float(tex_height) / float(max(1, tex_width))
    visual_width *= DIAGNOSTIC_VISUAL_SCALE
    visual_height *= DIAGNOSTIC_VISUAL_SCALE

    actor = runtime.spawn_sprite_actor(
        "RokSingleBuilding_{0}".format(model_id),
        material,
        0.0,
        0.0,
        visual_width,
        visual_height,
        10000,
        "RokSingleBuilding/Building",
        ["RokBuilding", "CityBuilding", "RokSingleBuildingDiagnostic"],
        True,
        (footprint["width"], footprint["length"]),
        (0, 0),
    )
    unreal.log(
        "Single building diagnostic: model={0}, texture={1}x{2}, visual={3:.1f}x{4:.1f}, scale={5:.3f}, actor_rot=({6:.2f},{7:.2f},{8:.2f})".format(
            model_id,
            tex_width,
            tex_height,
            visual_width,
            visual_height,
            DIAGNOSTIC_VISUAL_SCALE,
            float(actor.get_actor_rotation().pitch),
            float(actor.get_actor_rotation().yaw),
            float(actor.get_actor_rotation().roll),
        )
    )
    return actor


def spawn_single_background(primitives, materials):
    ground_material = materials.get("M_RuntimeCity_LayeredGrassTexture", materials["M_RuntimeCity_LayeredGrassBase"])
    runtime.spawn_runtime_plane(
        "RokSingleBuilding_GroundPreview",
        primitives,
        (0.0, 0.0, -18.0),
        1250.0,
        980.0,
        ground_material,
        "RokSingleBuilding/Terrain",
        yaw=45.0,
        sort_priority=-5000,
        selectable=False,
    )


def spawn_single_camera(target_location=None):
    camera_class = unreal.load_class(None, ROK_CAMERA_PAWN_CLASS)
    if not camera_class:
        raise RuntimeError("Missing camera pawn class: {0}. Build RokEditor before running this script.".format(ROK_CAMERA_PAWN_CLASS))

    target = target_location if target_location else unreal.Vector(0.0, 0.0, 0.0)
    camera_location = unreal.Vector(float(target.x) - 1650.0, float(target.y) - 1650.0, 2850.0)
    rotation = unreal.Rotator(-55.0, 45.0, 0.0)
    camera = unreal.EditorLevelLibrary.spawn_actor_from_class(camera_class, camera_location, rotation)
    camera.set_actor_label("Camera_RokSingleBuildingOrtho")
    camera.set_folder_path("RokSingleBuilding/Camera")
    camera.tags = list(set(list(camera.tags) + ["RokSingleBuildingCamera"]))
    component = camera.get_component_by_class(unreal.CameraComponent)
    if not component:
        raise RuntimeError("Rok camera pawn is missing CameraComponent")
    try:
        component.set_editor_property("projection_mode", unreal.CameraProjectionMode.ORTHOGRAPHIC)
    except Exception:
        component.set_editor_property("projection_mode", 1)
    try:
        component.set_editor_property("ortho_width", SINGLE_CAMERA_ORTHO_WIDTH)
    except Exception:
        pass
    settings = None
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
    unreal.log(
        "ROKCameraPawn: loc=({0:.1f},{1:.1f},{2:.1f}), rot=(-55,45,0), projection=Orthographic, ortho_width={3:.1f}, fixed_exposure=True".format(
            float(camera_location.x),
            float(camera_location.y),
            float(camera_location.z),
            SINGLE_CAMERA_ORTHO_WIDTH,
        )
    )
    return camera


def configure_single_building_world_settings():
    world = unreal.EditorLevelLibrary.get_editor_world()
    try:
        world.get_world_settings().set_editor_property("default_game_mode", None)
    except Exception:
        pass


def build_single_building_scene():
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    else:
        if not unreal.EditorLevelLibrary.new_level(TARGET_LEVEL):
            raise RuntimeError("Failed to create empty single building map: {0}".format(TARGET_LEVEL))

    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        unreal.EditorLevelLibrary.destroy_actor(actor)

    proto.ensure_project_primitive_meshes()
    primitives = {name: proto.load_primitive(name) for name in proto.PRIMITIVE_PATHS}
    materials = runtime.make_runtime_materials()

    proto.setup_lighting()
    if SINGLE_ADD_GROUND:
        spawn_single_background(primitives, materials)
    building_actor = spawn_single_building(primitives, materials)
    spawn_single_camera(building_actor.get_actor_location() if building_actor else None)
    configure_single_building_world_settings()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError("Failed to save single building map: {0}".format(TARGET_LEVEL))
    unreal.EditorAssetLibrary.save_asset(TARGET_LEVEL, only_if_is_dirty=False)
    unreal.log("Single building diagnostic map saved: {0}".format(TARGET_LEVEL))


if __name__ == "__main__":
    build_single_building_scene()
