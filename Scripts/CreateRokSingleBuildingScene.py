import os
import sys
import csv
import ast
import math
import json

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
UNITY_REFERENCE_ROOT = os.environ.get("ROK_UNITY_REFERENCE_ROOT", os.path.join(PROJECT_ROOT, "ref"))
if not os.path.isabs(UNITY_REFERENCE_ROOT):
    UNITY_REFERENCE_ROOT = os.path.join(PROJECT_ROOT, UNITY_REFERENCE_ROOT)
UNITY_CLIENT_ROOT = os.path.join(UNITY_REFERENCE_ROOT, "client-unity")
UNITY_RESOURCE_ROOT = os.path.join(UNITY_REFERENCE_ROOT, "resources")
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokPrototypeScene as proto
import CreateRokRuntimeCityScene as runtime


TARGET_LEVEL = os.environ.get("ROK_SINGLE_BUILDING_LEVEL", "/Game/Maps/L_RokCitySingleBuilding")
SOURCE_ACCEPTANCE_LEVEL = "/Game/Maps/L_RokCitySingleBuilding"
SOURCE_ACCEPTANCE_MODE = TARGET_LEVEL == SOURCE_ACCEPTANCE_LEVEL
DIAGNOSTIC_BUILDING_TYPE = int(os.environ.get("ROK_SINGLE_BUILDING_TYPE", "1"))
DIAGNOSTIC_VISUAL_SCALE = float(os.environ.get("ROK_SINGLE_BUILDING_VISUAL_SCALE", "0.62"))
ROK_BUILDING_ACTOR_CLASS = "/Script/Rok.RokBuildingActor"
ROK_CAMERA_PAWN_CLASS = "/Script/Rok.RokCameraPawn"
SINGLE_ADD_GROUND = os.environ.get("ROK_SINGLE_BUILDING_ADD_GROUND", "0") == "1"
SINGLE_ADD_UNITY_CITY_GROUND = os.environ.get("ROK_SINGLE_BUILDING_ADD_UNITY_CITY_GROUND", "1") != "0"
SINGLE_ADD_UNITY_CITY_SURFACE = os.environ.get("ROK_SINGLE_BUILDING_ADD_UNITY_CITY_SURFACE", "0") == "1"
SINGLE_ADD_UNITY_CITY_ROAD = os.environ.get("ROK_SINGLE_BUILDING_ADD_UNITY_CITY_ROAD", "0") == "1"
SINGLE_ADD_UNITY_GROUND_PATCH = os.environ.get("ROK_SINGLE_BUILDING_ADD_UNITY_GROUND_PATCH", "0") == "1"
SINGLE_ADD_UNITY_TERRAIN_PLATE = os.environ.get("ROK_SINGLE_BUILDING_ADD_UNITY_TERRAIN_PLATE", "0") == "1"
SINGLE_ADD_SHADOW = os.environ.get("ROK_SINGLE_BUILDING_ADD_SHADOW", "0") == "1"
SINGLE_SPLIT_BUILDING_GROUND = os.environ.get("ROK_SINGLE_BUILDING_SPLIT_GROUND", "0") == "1"
UNITY_SPRITE_TERRAIN_LAYER_OFFSET = float(os.environ.get("ROK_SINGLE_BUILDING_TERRAIN_LAYER_OFFSET", "26.0"))
UNITY_SPRITE_GROUND_LAYER_OFFSET = float(os.environ.get("ROK_SINGLE_BUILDING_GROUND_LAYER_OFFSET", "14.0"))
UNITY_SPRITE_BUILDING_LAYER_OFFSET = float(os.environ.get("ROK_SINGLE_BUILDING_BUILDING_LAYER_OFFSET", "-38.0"))
UNITY_PLACEMENT_CSV = os.path.join(PROJECT_ROOT, "Saved", "RokUnityPlacement", "unity_city_placement.csv")
UNITY_PLACEMENT_JSON = os.path.join(PROJECT_ROOT, "Saved", "RokUnityPlacement", "unity_city_placement.json")
UNITY_SCENE_NAME = os.environ.get("ROK_SINGLE_BUILDING_UNITY_SCENE", "Map_TTS.unity")
UNITY_BUILDING_NAME = os.environ.get("ROK_SINGLE_BUILDING_UNITY_NAME", "Building_Castle_02")
UNITY_SCENE_WORLD_SCALE = float(os.environ.get("ROK_SINGLE_BUILDING_SCENE_WORLD_SCALE", "170.0"))
UNITY_SCENE_VISUAL_SCALE = float(os.environ.get("ROK_SINGLE_BUILDING_SCENE_VISUAL_SCALE", "1.0"))
UNITY_CAMERA_VERTICAL_FOV = float(os.environ.get("ROK_SINGLE_BUILDING_UNITY_CAMERA_FOV", "12.0"))
UNITY_CAMERA_ASPECT_RATIO = float(os.environ.get("ROK_SINGLE_BUILDING_CAMERA_ASPECT", str(16.0 / 9.0)))
SINGLE_CAMERA_MODE = os.environ.get("ROK_SINGLE_BUILDING_CAMERA_MODE", "unity_perspective").lower()
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
UNITY_CITY_GROUND_STYLE = os.environ.get("ROK_SINGLE_BUILDING_CITY_GROUND_STYLE", "sand").lower()
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
    UNITY_CLIENT_ROOT,
    "Assets",
    "BundleAssets",
    "Map",
    "Textures",
    "build_ground_6_101_1.psd" if UNITY_CITY_GROUND_STYLE == "sand" else "Terrian_Maps/TR_Grass_01.png",
)
UNITY_CITY_GROUND_POSITION = (
    float(os.environ.get("ROK_SINGLE_BUILDING_CITY_GROUND_X", "0.01499939")),
    float(os.environ.get("ROK_SINGLE_BUILDING_CITY_GROUND_Y", "0.0")),
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
EXPECTED_RUNTIME_ATLAS_RECT_BOTTOM_LEFT = [513, 402, 418, 331]
EXPECTED_RUNTIME_ATLAS_RECT_TOP_LEFT = [513, 291, 418, 331]
EXPECTED_RUNTIME_ATLAS_SIZE = [1024, 1024]
EXPECTED_RUNTIME_COLOR_ATLAS = "ref/resources/Texture2D/city_build_6_5_0.png"
EXPECTED_RUNTIME_MASK_ATLAS = "ref/resources/Texture2D/city_build_mask_6_5_0.png"
EXPECTED_RUNTIME_COLOR_PIXEL_SHA256 = "5188d1fef08a0b2b51e7198607e732a601af4a7e3cd1c60be2f4d9174b8e433e"
EXPECTED_RUNTIME_MASK_PIXEL_SHA256 = "93c07f8a0e2208a0733812fdad14f5675d6d50973bd6a8fca9bee52eb31c32ce"
DEFAULT_SINGLE_SPRITE_PATH = os.path.join(UNITY_RESOURCE_ROOT, "Sprite", "{0}.png".format(DEFAULT_SINGLE_SPRITE_BASE))
DEFAULT_SINGLE_MASK_PATH = os.path.join(UNITY_RESOURCE_ROOT, "Sprite", "{0}_mask.png".format(DEFAULT_SINGLE_SPRITE_BASE))
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
        if SOURCE_ACCEPTANCE_MODE:
            raise RuntimeError("Map_TTS source acceptance requires clean cutout metadata: {0}".format(meta_path))
        return None
    with open(meta_path, "r", encoding="utf-8") as meta_file:
        meta = json.load(meta_file)
    clean_path = resolve_project_path(meta.get("clean_color"))
    if not clean_path:
        if SOURCE_ACCEPTANCE_MODE:
            raise RuntimeError("Map_TTS source acceptance requires the full-canvas clean_color output")
        return None
    if SOURCE_ACCEPTANCE_MODE and list(meta.get("source_size") or []) != [418, 331]:
        raise RuntimeError("Unexpected Castle_6_5 clean source size: {0}".format(meta.get("source_size")))
    if SOURCE_ACCEPTANCE_MODE:
        validate_map_tts_atlas_evidence(meta.get("runtime_atlas") or {}, "clean cutout metadata")
    return meta


def validate_map_tts_atlas_evidence(evidence, source_label):
    if (
        evidence.get("atlas_size") != EXPECTED_RUNTIME_ATLAS_SIZE
        or evidence.get("color_atlas_path") != EXPECTED_RUNTIME_COLOR_ATLAS
        or evidence.get("mask_atlas_path") != EXPECTED_RUNTIME_MASK_ATLAS
        or evidence.get("texture_rect_unity_bottom_left") != EXPECTED_RUNTIME_ATLAS_RECT_BOTTOM_LEFT
        or evidence.get("texture_rect_top_left") != EXPECTED_RUNTIME_ATLAS_RECT_TOP_LEFT
        or evidence.get("observed_crop_transform") != "identity"
        or evidence.get("observed_rotation_degrees") != 0
        or evidence.get("observed_flip_x") is not False
        or evidence.get("observed_flip_y") is not False
        or evidence.get("unity_packing_metadata_status") != "unresolved_in_snapshot"
        or not evidence.get("color_pixel_match")
        or not evidence.get("mask_pixel_match")
        or evidence.get("color_pixel_sha256") != EXPECTED_RUNTIME_COLOR_PIXEL_SHA256
        or evidence.get("mask_pixel_sha256") != EXPECTED_RUNTIME_MASK_PIXEL_SHA256
        or evidence.get("source_sprite_meta_status") != "unresolved_in_snapshot"
    ):
        raise RuntimeError("Map_TTS runtime-atlas evidence mismatch in {0}: {1}".format(source_label, evidence))
    return evidence


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


def quaternion_multiply(a, b):
    ax, ay, az, aw = [float(value) for value in a]
    bx, by, bz, bw = [float(value) for value in b]
    return [
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    ]


def quaternion_rotate(rotation, vector):
    value = [float(vector[0]), float(vector[1]), float(vector[2]), 0.0]
    conjugate = [-float(rotation[0]), -float(rotation[1]), -float(rotation[2]), float(rotation[3])]
    return quaternion_multiply(quaternion_multiply(rotation, value), conjugate)[:3]


def make_unity_sprite_rotation(unity_rotation=None):
    source_rotation = unity_rotation or [0.38268343, 0.0, 0.0, 0.92387956]
    normal = unity_direction_to_ue(quaternion_rotate(source_rotation, [0.0, 0.0, 1.0]))
    up = unity_direction_to_ue(quaternion_rotate(source_rotation, [0.0, 1.0, 0.0]))
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
    ground_record = load_unity_ground_record()
    ground_texture_path = UNITY_CITY_GROUND_TEXTURE
    if not os.path.exists(ground_texture_path):
        ground_texture_path = UNITY_CITY_GROUND_FALLBACK_TEXTURE
    if not os.path.exists(ground_texture_path):
        if SOURCE_ACCEPTANCE_MODE:
            raise RuntimeError("Map_TTS source ground texture is missing: {0}".format(ground_texture_path))
        unreal.log_warning("Unity city ground texture is missing: {0}".format(ground_texture_path))
        return None

    if UNITY_CITY_GROUND_STYLE == "sand":
        ground_asset_name = "UnityCityGround_build_ground_6_101_1_MapTTS_SourceV2"
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

    material = create_map_tts_ground_material(texture, "M_RokSingleBuilding_{0}_DefaultLitOpaque".format(ground_asset_name))
    ground_size = UNITY_CITY_GROUND_SIZE_UNITS * UNITY_SCENE_WORLD_SCALE
    source_position = list(ground_record.get("local_position") or UNITY_CITY_GROUND_POSITION)
    location = unity_position_to_ue(source_position)
    source_rotation = list(ground_record.get("local_rotation") or [0.0, 0.38268343, 0.0, 0.92387956])
    ground_yaw = math.degrees(2.0 * math.atan2(float(source_rotation[1]), float(source_rotation[3])))
    actor = runtime.spawn_runtime_plane(
        "RokSingleUnityCityGround_{0}".format(UNITY_CITY_GROUND_STYLE),
        primitives,
        (float(location.x), float(location.y), float(location.z)),
        ground_size,
        ground_size,
        material,
        "RokSingleBuilding/UnityGround",
        yaw=ground_yaw,
        sort_priority=-10000,
    )
    if actor:
        actor.tags = [
            "RokTerrainLayer",
            "RokSingleBuildingUnityCityGround",
            "RokUnityPlacementSource",
            "UnityScene_{0}".format(proto.safe_asset_name(ground_record.get("scene") or "")),
            "UnityGround_{0}".format(proto.safe_asset_name(ground_record.get("name") or "")),
            "UnityPrefabInstanceFileId_{0}".format(ground_record.get("instance_file_id", "")),
            "UnityPrefabGuid_{0}".format(ground_record.get("source_prefab_guid", "")),
            "UnityMaterialGuid_{0}".format(ground_record.get("material_guid", "")),
            "UnityParent_{0}".format(proto.safe_asset_name(ground_record.get("parent_hierarchy_path") or "")),
            "UnityGroundMaterial_StandardOpaqueZWrite",
            "UnityGroundGeometry_InferredPlaneProxy41x41",
            "UnityGroundPrefabGeometry_Unresolved",
        ]
    return actor


def create_map_tts_ground_material(texture, material_name):
    material_path = proto.asset_package_path(runtime.RUNTIME_MATERIAL_DESTINATION, material_name)
    existing = proto.load_asset_if_exists(material_path)
    if existing:
        blend_mode = existing.get_editor_property("blend_mode")
        shading_model = existing.get_editor_property("shading_model")
        if "OPAQUE" not in str(blend_mode).upper() or "DEFAULT_LIT" not in str(shading_model).upper():
            raise RuntimeError("Existing Map_TTS ground material has incompatible semantics: {0}".format(material_path))
        return existing

    unreal.EditorAssetLibrary.make_directory(runtime.RUNTIME_MATERIAL_DESTINATION)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        material_name,
        runtime.RUNTIME_MATERIAL_DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        raise RuntimeError("Failed to create Map_TTS ground material: {0}".format(material_path))
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", False)
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    except Exception:
        pass

    texture_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -420, -40
    )
    texture_node.set_editor_property("texture", texture)
    unreal.MaterialEditingLibrary.connect_material_property(
        texture_node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
    )
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -420, 180
    )
    roughness.set_editor_property("r", 0.92)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    compile_errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if compile_errors:
        raise RuntimeError("Map_TTS ground material compile failed: {0}".format(compile_errors))
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


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


def make_unity_camera_rotation(camera_record=None):
    source_rotation = (camera_record or {}).get("world_rotation") or [0.38268343, 0.0, 0.0, 0.92387956]
    forward = unity_direction_to_ue(quaternion_rotate(source_rotation, [0.0, 0.0, 1.0]))
    up = unity_direction_to_ue(quaternion_rotate(source_rotation, [0.0, 1.0, 0.0]))
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


def load_unity_placement_manifest():
    if not os.path.exists(UNITY_PLACEMENT_JSON):
        raise RuntimeError(
            "Unity placement manifest is missing; run Scripts/ExtractRokUnityCityPlacement.py first: {0}".format(
                UNITY_PLACEMENT_JSON
            )
        )
    with open(UNITY_PLACEMENT_JSON, "r", encoding="utf-8-sig") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != "rok.unity_city_placement.v3":
        raise RuntimeError("Unsupported Unity placement manifest schema: {0}".format(manifest.get("schema")))
    return manifest


def load_unity_camera_record():
    records = [
        record
        for record in load_unity_placement_manifest().get("cameras", [])
        if record.get("scene") == UNITY_SCENE_NAME
        and record.get("name") == "Main Camera"
        and record.get("active")
    ]
    if len(records) != 1:
        raise RuntimeError("Expected one active Unity Main Camera record for {0}, found {1}".format(UNITY_SCENE_NAME, len(records)))
    return records[0]


def load_unity_ground_record():
    records = [
        record
        for record in load_unity_placement_manifest().get("prefab_instances", [])
        if record.get("scene") == UNITY_SCENE_NAME
        and record.get("name", "").startswith("CityWall_Ground_1_1_0")
        and record.get("active")
    ]
    if len(records) != 1:
        raise RuntimeError("Expected one active Unity CityWall_Ground record for {0}, found {1}".format(UNITY_SCENE_NAME, len(records)))
    return records[0]


def load_unity_scene_building_row():
    if not os.path.exists(UNITY_PLACEMENT_CSV):
        raise RuntimeError(
            "Unity placement CSV is missing; run Scripts/ExtractRokUnityCityPlacement.py first: {0}".format(
                UNITY_PLACEMENT_CSV
            )
        )

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
                if row.get("mapping_source") == "unresolved":
                    raise RuntimeError("Unity building source is unresolved: {0}".format(UNITY_BUILDING_NAME))
                return row
    raise RuntimeError("Unity building {0} was not found in {1}; source fallback is forbidden".format(
        UNITY_BUILDING_NAME,
        UNITY_SCENE_NAME,
    ))


def find_diagnostic_model(building_models):
    model_id = building_models.get(DIAGNOSTIC_BUILDING_TYPE)
    if model_id:
        return model_id
    return "build_1_101_{0}".format(runtime.AGE)


def make_footprint_highlight_material():
    material_path = proto.asset_package_path(runtime.RUNTIME_MATERIAL_DESTINATION, "M_RokSingleBuilding_FootprintHighlight")
    existing = proto.load_asset_if_exists(material_path)
    if existing:
        return existing
    return runtime.create_unlit_constant_material(
        "M_RokSingleBuilding_FootprintHighlight",
        unreal.LinearColor(0.95, 0.72, 0.18, 1.0),
    )


def spawn_rook_building_actor(label, material, root_location, width, height, pivot_offset, footprint_width, footprint_length, mask_source, clean_meta, source_tags, unity_rotation):
    building_actor_class = unreal.load_class(None, ROK_BUILDING_ACTOR_CLASS)
    if not building_actor_class:
        raise RuntimeError("Missing building actor class: {0}. Build RokEditor before running this script.".format(ROK_BUILDING_ACTOR_CLASS))

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(building_actor_class, root_location, unreal.Rotator(0.0, 0.0, 0.0))
    actor.set_actor_label(label)
    actor.set_folder_path("RokSingleBuilding/Building")
    actor.configure_card(material, width, height, pivot_offset, make_unity_sprite_rotation(unity_rotation))
    actor.configure_footprint(footprint_width, footprint_length, make_footprint_highlight_material(), SINGLE_FOOTPRINT_YAW)
    actor.tags = [
        "RokBuilding",
        "CityBuilding",
        "RokSingleBuildingSourceAcceptance",
        "RokUnityPlacementSource",
    ] + list(source_tags)
    try:
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            if component.get_name() == "FootprintHighlightComponent":
                component.set_visibility(False)
                component.set_editor_property("hidden_in_game", True)
    except Exception:
        pass

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
    try:
        runtime_atlas = json.loads(unity_row.get("runtime_atlas") or "null") or {}
    except (TypeError, ValueError):
        raise RuntimeError("Map_TTS placement CSV runtime_atlas is not valid JSON")
    if SOURCE_ACCEPTANCE_MODE:
        validate_map_tts_atlas_evidence(runtime_atlas, "placement CSV")
        if unity_row.get("source_sprite_meta_status") != "unresolved_in_snapshot":
            raise RuntimeError("Map_TTS source Sprite pivot/PPU metadata provenance was lost")
    clean_meta = load_clean_cutout_meta(sprite_base)
    if SOURCE_ACCEPTANCE_MODE and (clean_meta.get("runtime_atlas") or {}) != runtime_atlas:
        raise RuntimeError("Clean cutout runtime-atlas evidence does not match the placement manifest")
    source_size_scale = [1.0, 1.0]
    if clean_meta:
        # Keep the cleaned texture at the original sprite canvas size so the
        # Unity SpriteRenderer pivot remains exact. Cropped outputs are useful
        # diagnostics but require an additional pivot-center compensation.
        clean_key = "body_clean" if SINGLE_SPLIT_BUILDING_GROUND and clean_meta.get("body_clean") else "clean_color"
        source_path = resolve_project_path(clean_meta[clean_key])
        mask_path = None
        mask_source = "clean RGBA alpha"
    else:
        mask_path = os.path.join(os.path.dirname(source_path), "{0}_mask.png".format(sprite_base))
        if not os.path.exists(mask_path):
            mask_path = None
        mask_source = "mask {0}".format(UNITY_SPRITE_MASK_CHANNEL) if mask_path else "source alpha"

    texture_name = "UnitySingle_{0}{1}".format(sprite_base, "_CleanFullCanvas_MapTTS_SourceV2" if clean_meta else "")
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
    if SOURCE_ACCEPTANCE_MODE and (int(tex_width), int(tex_height)) != (418, 331):
        raise RuntimeError("Map_TTS clean building texture must be 418x331, got {0}x{1}".format(tex_width, tex_height))
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
    unity_rotation = parse_vector(unity_row.get("world_rotation"), [0.38268343, 0.0, 0.0, 0.92387956])
    camera_forward = unity_direction_to_ue(quaternion_rotate(unity_rotation, [0.0, 0.0, 1.0]))
    pivot_offset = unreal.Vector(
        float(camera_forward.x) * UNITY_SPRITE_BUILDING_LAYER_OFFSET,
        float(camera_forward.y) * UNITY_SPRITE_BUILDING_LAYER_OFFSET,
        float(location.z) + float(camera_forward.z) * UNITY_SPRITE_BUILDING_LAYER_OFFSET,
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
        [
            "UnityScene_{0}".format(proto.safe_asset_name(unity_row.get("scene") or "")),
            "UnityObject_{0}".format(proto.safe_asset_name(unity_row.get("name") or "")),
            "UnityGameObjectFileId_{0}".format(unity_row.get("game_object_file_id", "")),
            "UnityTransformFileId_{0}".format(unity_row.get("transform_file_id", "")),
            "UnityRendererFileId_{0}".format(unity_row.get("renderer_file_id", "")),
            "UnitySpriteFileId_{0}".format(unity_row.get("sprite_file_id", "")),
            "UnitySpriteGuid_{0}".format(unity_row.get("sprite_guid", "")),
            "UnityMapping_{0}".format(proto.safe_asset_name(unity_row.get("mapping_source") or "")),
            "UnityHierarchy_{0}".format(proto.safe_asset_name(unity_row.get("hierarchy_path") or "")),
            "UnityFlipX_{0}".format(1 if unity_row.get("flip_x") == "True" else 0),
            "UnityFlipY_{0}".format(1 if unity_row.get("flip_y") == "True" else 0),
            "UnityAtlas_city_build_6_5_0",
            "UnityAtlasMask_city_build_mask_6_5_0",
            "UnityAtlasRectBL_513_402_418_331",
            "UnityAtlasRectTL_513_291_418_331",
            "UnityAtlasObservedCropTransform_Identity",
            "UnityAtlasPixelMatch_ColorMask",
            "UELayerOffset_{0:g}".format(UNITY_SPRITE_BUILDING_LAYER_OFFSET),
            "UnitySpriteMeta_Unresolved",
            "UnitySpritePackingMeta_Unresolved",
            "UnitySpritePivotMeta_Unresolved",
        ],
        unity_rotation,
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


def spawn_unity_perspective_camera(target_location=None):
    camera_record = load_unity_camera_record()
    camera_location = unity_position_to_ue(camera_record.get("world_position") or UNITY_CAMERA_POSITION)
    camera_rotation = make_unity_camera_rotation(camera_record)
    vertical_fov = float(camera_record.get("vertical_fov", UNITY_CAMERA_VERTICAL_FOV))
    horizontal_fov = ue_horizontal_fov_from_unity_vertical_fov(vertical_fov, UNITY_CAMERA_ASPECT_RATIO)

    camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.CameraActor, camera_location, camera_rotation)
    camera.set_actor_label("Camera_UnityPerspective_Map_TTS")
    camera.set_folder_path("RokSingleBuilding/Camera")
    camera.tags = [
        "RokSingleBuildingCamera",
        "RokUnityPerspectiveCamera",
        "Map_TTS",
        "VerticalFov{0:g}".format(vertical_fov),
        "UnityCameraFileId_{0}".format(camera_record.get("camera_file_id", "")),
        "UnityTransformFileId_{0}".format(camera_record.get("transform_file_id", "")),
        "UnityClearFlags_{0}".format(camera_record.get("clear_flags", "")),
        "UnityBackground_{0}".format(proto.safe_asset_name("_".join("{0:g}".format(float(value)) for value in camera_record.get("background_color", [])))),
        "UnityHDR_{0}".format(1 if camera_record.get("hdr") else 0),
        "UnityMSAA_{0}".format(1 if camera_record.get("msaa") else 0),
        "UnityOcclusionCulling_{0}".format(1 if camera_record.get("occlusion_culling") else 0),
        "UnityNearClip_{0:g}".format(float(camera_record.get("near_clip", 0.3))),
        "UnityFarClip_{0:g}".format(float(camera_record.get("far_clip", 1000.0))),
        "UnityCameraExposure_Default",
    ]
    try:
        camera.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)
    except Exception:
        pass

    component = camera.get_component_by_class(unreal.CameraComponent)
    if not component:
        raise RuntimeError("Unity perspective camera is missing CameraComponent")
    try:
        component.set_editor_property("projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
    except Exception:
        pass
    component.set_editor_property("field_of_view", horizontal_fov)
    component.set_editor_property("aspect_ratio", UNITY_CAMERA_ASPECT_RATIO)
    component.set_editor_property("constrain_aspect_ratio", True)
    try:
        component.set_editor_property("perspective_near_clip_plane", float(camera_record.get("near_clip", 0.3)) * UNITY_SCENE_WORLD_SCALE)
    except Exception:
        pass
    try:
        component.set_editor_property("post_process_blend_weight", 0.0)
    except Exception:
        pass

    try:
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(camera_location, camera_rotation)
    except Exception:
        pass
    unreal.log(
        "UnityPerspectiveCamera: unity_pos=({0:.3f},{1:.3f},{2:.3f}), vertical_fov={3:.2f}, ue_horizontal_fov={4:.2f}, ue_loc=({5:.1f},{6:.1f},{7:.1f})".format(
            UNITY_CAMERA_POSITION[0],
            UNITY_CAMERA_POSITION[1],
            UNITY_CAMERA_POSITION[2],
            vertical_fov,
            horizontal_fov,
            float(camera_location.x),
            float(camera_location.y),
            float(camera_location.z),
        )
    )
    return camera


def configure_single_building_world_settings():
    world = unreal.EditorLevelLibrary.get_editor_world()
    try:
        if SINGLE_CAMERA_MODE == "unity_perspective":
            base_game_mode = unreal.load_class(None, "/Script/Engine.GameModeBase")
            world.get_world_settings().set_editor_property("default_game_mode", base_game_mode)
        else:
            world.get_world_settings().set_editor_property("default_game_mode", None)
    except Exception:
        pass


def assert_source_acceptance_configuration():
    if not SOURCE_ACCEPTANCE_MODE:
        return
    invalid = []
    if SINGLE_CAMERA_MODE != "unity_perspective":
        invalid.append("camera mode must be unity_perspective")
    if abs(UNITY_CAMERA_ASPECT_RATIO - (16.0 / 9.0)) > 0.000001:
        invalid.append("camera aspect must be 16:9")
    if not SINGLE_ADD_UNITY_CITY_GROUND or UNITY_CITY_GROUND_STYLE != "sand":
        invalid.append("the active LV_02 sand ground is required")
    if SINGLE_ADD_GROUND or SINGLE_ADD_UNITY_CITY_SURFACE or SINGLE_ADD_UNITY_CITY_ROAD:
        invalid.append("legacy/background/surface/road diagnostic layers must be disabled")
    if SINGLE_ADD_UNITY_GROUND_PATCH or SINGLE_ADD_UNITY_TERRAIN_PLATE or SINGLE_ADD_SHADOW:
        invalid.append("ground-patch/terrain-plate/fake-shadow diagnostic layers must be disabled")
    if not UNITY_USE_CLEAN_CUTOUT or SINGLE_SPLIT_BUILDING_GROUND:
        invalid.append("full-canvas clean cutout without color split is required")
    if abs(UNITY_SCENE_WORLD_SCALE - 170.0) > 0.000001 or abs(UNITY_SCENE_VISUAL_SCALE - 1.0) > 0.000001:
        invalid.append("source conversion scale must remain 170 with visual scale 1")
    if invalid:
        raise RuntimeError("Invalid Map_TTS source acceptance configuration: " + "; ".join(invalid))


def setup_map_tts_lighting_proxy():
    sun = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight,
        unreal.Vector(-1600.0, -1600.0, 2600.0),
        unreal.Rotator(-48.0, -35.0, 0.0),
    )
    sun.set_actor_label("MapTTS_DefaultSkyboxLightProxy")
    sun.set_folder_path("RokSingleBuilding/LightingProxy")
    sun.tags = ["RokSingleBuildingLightingProxy", "UnityRenderSettings_NoExplicitSceneLight", "UE_DefaultSkyboxAmbientProxy"]
    sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
    if sun_component:
        sun_component.set_editor_property("intensity", 2.2)
        sun_component.set_editor_property("cast_shadows", False)

    sky = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0.0, 0.0, 1200.0))
    sky.set_actor_label("MapTTS_AmbientSkyProxy")
    sky.set_folder_path("RokSingleBuilding/LightingProxy")
    sky.tags = ["RokSingleBuildingLightingProxy", "UnityAmbientMode_SkyboxProxy"]
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    if sky_component:
        sky_component.set_editor_property("intensity", 0.55)
        sky_component.set_editor_property("lower_hemisphere_is_black", False)
        sky_component.set_editor_property("lower_hemisphere_color", unreal.LinearColor(0.047, 0.043, 0.035, 1.0))


def is_target_level_loaded():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        return False
    try:
        return str(world.get_outermost().get_name()) == TARGET_LEVEL
    except Exception:
        return world.get_name() == TARGET_LEVEL.rsplit("/", 1)[-1]


def build_single_building_scene():
    assert_source_acceptance_configuration()
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        if not is_target_level_loaded():
            unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    else:
        if not unreal.EditorLevelLibrary.new_level(TARGET_LEVEL):
            raise RuntimeError("Failed to create empty single building map: {0}".format(TARGET_LEVEL))

    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        unreal.EditorLevelLibrary.destroy_actor(actor)

    proto.ensure_project_primitive_meshes()
    primitives = {name: proto.load_primitive(name) for name in proto.PRIMITIVE_PATHS}
    materials = runtime.make_runtime_materials() if (SINGLE_ADD_GROUND or SINGLE_ADD_SHADOW) else {}

    if SOURCE_ACCEPTANCE_MODE:
        setup_map_tts_lighting_proxy()
    else:
        proto.setup_lighting()
    if SINGLE_ADD_GROUND:
        spawn_single_background(primitives, materials)
    building_actor = spawn_single_building(primitives, materials)
    if SINGLE_CAMERA_MODE == "orthographic":
        spawn_single_camera(building_actor.get_actor_location() if building_actor else None)
    else:
        spawn_unity_perspective_camera(building_actor.get_actor_location() if building_actor else None)
    configure_single_building_world_settings()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError("Failed to save single building map: {0}".format(TARGET_LEVEL))
    unreal.EditorAssetLibrary.save_asset(TARGET_LEVEL, only_if_is_dirty=False)
    unreal.log("Single building diagnostic map saved: {0}".format(TARGET_LEVEL))


if __name__ == "__main__":
    build_single_building_scene()
