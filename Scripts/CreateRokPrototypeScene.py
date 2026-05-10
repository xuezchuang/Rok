import math
import os
import re
import json
import subprocess

import unreal


TARGET_LEVEL = os.environ.get("ROK_TARGET_LEVEL", "/Game/Maps/L_RokPrototype_Lit")
BASE_LEVEL = os.environ.get("ROK_BASE_LEVEL", "none")
if BASE_LEVEL.lower() in ["", "none", "null", "-"]:
    BASE_LEVEL = None
INCLUDE_3D_WORLD_CONTEXT = os.environ.get("ROK_INCLUDE_3D_WORLD_CONTEXT", "0") == "1"
INCLUDE_DEBUG_LABELS = os.environ.get("ROK_INCLUDE_DEBUG_LABELS", "0") == "1"
UNITY_RECONSTRUCTION_MODE = os.environ.get("ROK_UNITY_RECONSTRUCTION_MODE", "1") == "1"

MATERIAL_COLORS = {
    "M_Rok_Grass_Blockout": unreal.LinearColor(0.22, 0.46, 0.25, 1.0),
    "M_Rok_Grass_Far_Blockout": unreal.LinearColor(0.18, 0.34, 0.22, 1.0),
    "M_Rok_Grass_Dark_Blockout": unreal.LinearColor(0.12, 0.31, 0.20, 1.0),
    "M_Rok_Path_Blockout": unreal.LinearColor(0.70, 0.58, 0.38, 1.0),
    "M_Rok_Plaza_Blockout": unreal.LinearColor(0.62, 0.55, 0.45, 1.0),
    "M_Rok_Stone_Blockout": unreal.LinearColor(0.46, 0.43, 0.38, 1.0),
    "M_Rok_WallCap_Blockout": unreal.LinearColor(0.70, 0.66, 0.57, 1.0),
    "M_Rok_Terracotta_Blockout": unreal.LinearColor(0.64, 0.21, 0.12, 1.0),
    "M_Rok_Wood_Blockout": unreal.LinearColor(0.34, 0.20, 0.11, 1.0),
    "M_Rok_Water_Blockout": unreal.LinearColor(0.05, 0.22, 0.34, 1.0),
    "M_Rok_Gold_Blockout": unreal.LinearColor(0.95, 0.68, 0.20, 1.0),
    "M_Rok_Red_Blockout": unreal.LinearColor(0.76, 0.08, 0.06, 1.0),
    "M_Rok_Blue_Blockout": unreal.LinearColor(0.08, 0.22, 0.72, 1.0),
    "M_Rok_Farm_Blockout": unreal.LinearColor(0.78, 0.61, 0.20, 1.0),
    "M_Rok_Quarry_Blockout": unreal.LinearColor(0.42, 0.40, 0.36, 1.0),
    "M_Rok_Shadow_Blockout": unreal.LinearColor(0.09, 0.12, 0.10, 1.0),
}

ENGINE_PRIMITIVE_PATHS = {
    "cube": "/Engine/BasicShapes/Cube.Cube",
    "plane": "/Engine/BasicShapes/Plane.Plane",
    "sphere": "/Engine/BasicShapes/Sphere.Sphere",
    "cylinder": "/Engine/BasicShapes/Cylinder.Cylinder",
    "cone": "/Engine/BasicShapes/Cone.Cone",
}

PRIMITIVE_PATHS = {
    "cube": "/Game/RokPrototype/Meshes/Proxy/SM_Rok_ProxyCube.SM_Rok_ProxyCube",
    "plane": "/Game/RokPrototype/Meshes/Proxy/SM_Rok_ProxyPlane.SM_Rok_ProxyPlane",
    "sphere": "/Game/RokPrototype/Meshes/Proxy/SM_Rok_ProxySphere.SM_Rok_ProxySphere",
    "cylinder": "/Game/RokPrototype/Meshes/Proxy/SM_Rok_ProxyCylinder.SM_Rok_ProxyCylinder",
    "cone": "/Game/RokPrototype/Meshes/Proxy/SM_Rok_ProxyCone.SM_Rok_ProxyCone",
}

SPRITE_TEXTURE_DESTINATION = "/Game/RokPrototype/Textures/ReferenceSprites"
SPRITE_MATERIAL_DESTINATION = "/Game/RokPrototype/Materials/ReferenceSprites"
CITY_SPRITE_TEXTURE_DESTINATION = "/Game/RokPrototype/Textures/CitySprites"
CITY_SPRITE_MATERIAL_DESTINATION = "/Game/RokPrototype/Materials/CitySprites"
UNITY_SPRITE_SCENE = os.path.join(
    unreal.Paths.project_dir(),
    "ref",
    "client-unity",
    "Assets",
    "BundleAssets",
    "Map",
    "Scenes",
    "Map_TTS.unity",
)
REFERENCE_SPRITE_DIR = os.path.join(
    unreal.Paths.project_dir(),
    "ref",
    "resources",
    "Sprite",
)
REFERENCE_UNITY_DIR = os.path.join(unreal.Paths.project_dir(), "ref", "client-unity", "Assets", "BundleAssets")
REFERENCE_WILD_BUILDING_DIR = os.path.join(REFERENCE_UNITY_DIR, "Map", "Building", "Buildingwild_use")
REFERENCE_CITY_BUILDING_DIR = os.path.join(
    REFERENCE_UNITY_DIR,
    "Map",
    "Building",
    "Building_use",
    "shared_building",
)
UNITY_PROCESSED_SPRITE_MANIFEST = os.path.join(
    unreal.Paths.project_dir(),
    "Saved",
    "RokUnitySpriteScene",
    "Map_TTS",
    "sprites.json",
)
CITY_BUILD_CUTOUT_MANIFEST = os.path.join(
    unreal.Paths.project_dir(),
    "Saved",
    "RokCityBuildCutouts",
    "city_build_cutouts.json",
)

REFERENCE_STATIC_MESH_IMPORTS = [
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Map", "FBX", "CityWall_101_1.FBX"),
        "destination": "/Game/RokPrototype/Meshes/City",
        "name": "SM_Rok_CityWall",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Map", "FBX", "CityGate_101_1.FBX"),
        "destination": "/Game/RokPrototype/Meshes/City",
        "name": "SM_Rok_CityGate",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Map", "FBX", "CityTower_101_1_1.FBX"),
        "destination": "/Game/RokPrototype/Meshes/City",
        "name": "SM_Rok_CityTower",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Map", "FBX", "build_Checkpoint_Big.FBX"),
        "destination": "/Game/RokPrototype/Meshes/City",
        "name": "SM_Rok_Checkpoint",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Map", "FBX", "alliance_flag_icon1.FBX"),
        "destination": "/Game/RokPrototype/Meshes/City",
        "name": "SM_Rok_AllianceFlag",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "land", "Fbx", "build_mountain_A.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_MountainA",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "land", "Fbx", "build_mountain_B.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_MountainB",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "land", "Fbx", "build_bridge_A.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_Bridge",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Map", "Env", "Big world", "build_SceneTree_1.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_TreeA",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Map", "Env", "Big world", "build_SceneTree_2.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_TreeB",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Map", "FBX", "ground.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_Ground",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Map", "Env", "Plane", "Plane180_180.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_Plane180",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Map", "Env", "Plane", "Plane60_60.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_Plane60",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Map", "Env", "Plane", "Plane30_30.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_Plane30",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "land", "Fbx", "Rive_CO_A.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_RiverA",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "land", "Fbx", "river_co_01.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_RiverB",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "land", "Fbx", "Ocean_CO_A.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_Ocean",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Effect", "Meshes", "m_stone001.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_StoneResource",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Barbarian", "GuardianFormation", "test_model", "model.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_MarchUnit",
        "required": True,
    },
    {
        "source": os.path.join(REFERENCE_UNITY_DIR, "Effect", "Ani_Meshes", "qizi.FBX"),
        "destination": "/Game/RokPrototype/Meshes/Environment",
        "name": "SM_Rok_MarchFlag",
        "required": True,
    },
]

SPRITE_NAME_TO_SOURCE = {
    "Building_MainCity_01": "Castle_1_5.png",
    "Building_MainCity_02": "Castle_1_5.png",
    "Building_Castle_02": "Castle_6_5.png",
    "City_Main": "city.png",
    "Building_Tower_01": "GuardTowerUI_1_5.png",
    "Building_Tower_02": "GuardTowerUI_1_4.png",
    "Building_Farm": "FarmWindmill_ani_5_00.png",
    "Building_Hospital": "Hospital_1_5.png",
    "Building_WorkMan_02": "Barracks_1_5.png",
    "Building_Military_Camp_01": "Barracks_1_5.png",
    "Building_Military_Camp_02": "Stable_1_5.png",
    "Building_Book_02": "Tavern_1_5.png",
    "Building_Wood": "Lumbermill_1_5.png",
    "Building_Stone": "Quarry_1_5.png",
    "Building_Gold_02": "Goldmine_1_5.png",
    "Building_Storehouse_01": "Shop_1_4.png",
    "Building_Storehouse_02": "Shop_1_4.png",
    "Building_Hero_01": "Monument_1_4.png",
    "Building_Hero_02": "Monument_6_4.png",
    "Building_Arc_01": "Archery_1_5.png",
    "Building_Arc_02": "Archery_6_5.png",
    "Building_Patrol_01": "ScoutCamp_1_4.png",
    "Building_Patrol_02": "ScoutCamp_6_4.png",
    "World_2": "CityWall_1_4_c.png",
    "World_4": "CityWall_1_4_c.png",
    "zhongxinchengbao": "Castle_10_5.png",
    "zhedang02": "CityWall_1_4_shadow.png",
}

RESOURCE_SPRITE_SOURCE = {
    "farm": os.path.join(REFERENCE_WILD_BUILDING_DIR, "farm_1.png"),
    "gold": os.path.join(REFERENCE_WILD_BUILDING_DIR, "gold_1.png"),
}


def asset_object_path(package_path, name):
    return "{0}/{1}.{1}".format(package_path, name)


def asset_package_path(package_path, name):
    return "{0}/{1}".format(package_path, name)


def normalize_asset_load_path(asset_path):
    leaf = asset_path.rsplit("/", 1)[-1]
    asset_name = leaf.split(".", 1)[0]
    object_suffix = ".{0}".format(asset_name)
    if asset_path.endswith(object_suffix):
        return asset_path[: -len(object_suffix)]
    return asset_path


def load_asset_if_exists(asset_path):
    package_path = normalize_asset_load_path(asset_path)
    if not unreal.EditorAssetLibrary.does_asset_exist(package_path):
        return None
    return unreal.EditorAssetLibrary.load_asset(package_path)


def load_primitive(name):
    mesh = unreal.EditorAssetLibrary.load_asset(PRIMITIVE_PATHS[name])
    if not mesh:
        raise RuntimeError("Missing Rok proxy primitive mesh: {0}".format(PRIMITIVE_PATHS[name]))
    return mesh


def ensure_project_primitive_meshes():
    destination = "/Game/RokPrototype/Meshes/Proxy"
    unreal.EditorAssetLibrary.make_directory(destination)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name, destination_path in PRIMITIVE_PATHS.items():
        if unreal.EditorAssetLibrary.does_asset_exist(destination_path):
            continue
        source_path = ENGINE_PRIMITIVE_PATHS[name]
        source_mesh = unreal.EditorAssetLibrary.load_asset(source_path)
        if not source_mesh:
            raise RuntimeError("Missing engine primitive mesh: {0}".format(source_path))
        asset_name = destination_path.rsplit("/", 1)[-1].split(".", 1)[0]
        duplicated = asset_tools.duplicate_asset(asset_name, destination, source_mesh)
        if not duplicated:
            raise RuntimeError("Failed to create Rok proxy primitive {0} from {1}".format(destination_path, source_path))
        unreal.EditorAssetLibrary.save_loaded_asset(duplicated)
        if not unreal.EditorAssetLibrary.does_asset_exist(destination_path):
            raise RuntimeError("Rok proxy primitive did not save: {0}".format(destination_path))
    unreal.log("Rok project proxy primitive meshes ready.")


def make_rotator(pitch=0.0, yaw=0.0, roll=0.0):
    # Unreal's Python constructor order is roll, pitch, yaw.
    return unreal.Rotator(roll, pitch, yaw)


def create_constant_material(name, color):
    destination = "/Game/RokPrototype/Materials"
    existing = load_asset_if_exists(asset_package_path(destination, name))
    if existing:
        return existing

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(name, destination, unreal.Material, unreal.MaterialFactoryNew())
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant4Vector, -300, 0
    )
    expression.set_editor_property("constant", color)
    unreal.MaterialEditingLibrary.connect_material_property(
        expression, "", unreal.MaterialProperty.MP_BASE_COLOR
    )

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -300, 180
    )
    roughness.set_editor_property("r", 0.82)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def make_materials():
    return {name: create_constant_material(name, color) for name, color in MATERIAL_COLORS.items()}


def assign_material(actor, material):
    if not actor or not material:
        return
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if component:
        slot_count = component.get_num_materials()
        for slot_index in range(max(slot_count, 1)):
            component.set_material(slot_index, material)


def rotate_xy(x, y, yaw_degrees):
    radians = math.radians(yaw_degrees)
    cos_yaw = math.cos(radians)
    sin_yaw = math.sin(radians)
    return x * cos_yaw - y * sin_yaw, x * sin_yaw + y * cos_yaw


def load_static_mesh(asset_path):
    mesh = load_asset_if_exists(asset_path)
    if isinstance(mesh, unreal.StaticMesh):
        return mesh
    unreal.log_warning("Missing or invalid static mesh: {0}".format(asset_path))
    return None


def configure_static_mesh_import_options():
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("automated_import_should_detect_type", False)
    static_data = options.get_editor_property("static_mesh_import_data")
    static_data.set_editor_property("combine_meshes", True)
    static_data.set_editor_property("generate_lightmap_u_vs", True)
    static_data.set_editor_property("auto_generate_collision", True)
    return options


def import_reference_static_mesh(import_spec):
    source_path = import_spec["source"]
    destination = import_spec["destination"]
    asset_name = import_spec["name"]
    package_path = asset_package_path(destination, asset_name)
    existing = load_asset_if_exists(package_path)
    force_reimport = os.environ.get("ROK_FORCE_REIMPORT", "0") == "1"
    if isinstance(existing, unreal.StaticMesh) and not force_reimport:
        return existing

    if not os.path.exists(source_path):
        if import_spec.get("required"):
            raise RuntimeError("Missing reference FBX source: {0}".format(source_path))
        unreal.log_warning("Missing optional reference FBX source: {0}".format(source_path))
        return None

    unreal.EditorAssetLibrary.make_directory(destination)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", configure_static_mesh_import_options())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    imported = load_asset_if_exists(package_path)
    if not isinstance(imported, unreal.StaticMesh):
        imported_paths = task.get_editor_property("imported_object_paths")
        imported_meshes = []
        for path in imported_paths:
            imported_candidate = load_asset_if_exists(path)
            if isinstance(imported_candidate, unreal.StaticMesh):
                imported_meshes.append(imported_candidate)
        if imported_meshes:
            imported = imported_meshes[0]

    if not isinstance(imported, unreal.StaticMesh):
        if import_spec.get("required"):
            raise RuntimeError("Failed to import required reference FBX: {0}".format(source_path))
        unreal.log_warning("Failed to import optional reference FBX: {0}".format(source_path))
        return None
    return imported


def import_reference_static_meshes():
    imported = []
    for import_spec in REFERENCE_STATIC_MESH_IMPORTS:
        mesh = import_reference_static_mesh(import_spec)
        if mesh:
            imported.append(mesh.get_path_name())
    unreal.log("Reference static meshes ready: {0}".format(", ".join(imported)))
    return imported


def safe_asset_name(value):
    cleaned = re.sub(r"[^0-9A-Za-z_]+", "_", value.strip())
    cleaned = re.sub(r"_+", "_", cleaned).strip("_")
    return cleaned or "Asset"


def find_reference_sprite_source(sprite_name):
    if sprite_name in SPRITE_NAME_TO_SOURCE:
        filename = SPRITE_NAME_TO_SOURCE[sprite_name]
    else:
        normalized = sprite_name.split(" (")[0]
        filename = SPRITE_NAME_TO_SOURCE.get(normalized)
    if not filename:
        return None

    source_path = os.path.join(REFERENCE_SPRITE_DIR, filename)
    if os.path.exists(source_path):
        return source_path

    unreal.log_warning("Missing reference sprite source: {0}".format(source_path))
    return None


def import_texture_to_destination(source_path, destination_path):
    asset_name = safe_asset_name(os.path.splitext(os.path.basename(source_path))[0])
    package_path = asset_package_path(destination_path, asset_name)
    existing = load_asset_if_exists(package_path)
    force_reimport = os.environ.get("ROK_FORCE_REIMPORT_TEXTURES", "0") == "1"
    if existing and not force_reimport:
        return existing

    unreal.EditorAssetLibrary.make_directory(destination_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", destination_path)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("factory", unreal.TextureFactory())
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", False)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    imported = load_asset_if_exists(package_path)
    if not imported:
        imported_paths = task.get_editor_property("imported_object_paths")
        if imported_paths:
            imported = load_asset_if_exists(imported_paths[0])
    if not imported:
        unreal.log_warning("Failed to import texture: {0}".format(source_path))
    return imported


def import_reference_texture(source_path):
    return import_texture_to_destination(source_path, SPRITE_TEXTURE_DESTINATION)


def import_city_sprite_texture(source_path):
    texture = import_texture_to_destination(source_path, CITY_SPRITE_TEXTURE_DESTINATION)
    if texture:
        texture.set_editor_property("srgb", True)
        texture.set_editor_property("never_stream", True)
        try:
            texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        except Exception:
            pass
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def create_sprite_material(texture):
    material_name = "M_Sprite2DCutout_{0}".format(safe_asset_name(texture.get_name()))
    existing = load_asset_if_exists(asset_package_path(SPRITE_MATERIAL_DESTINATION, material_name))
    if existing:
        return existing

    unreal.EditorAssetLibrary.make_directory(SPRITE_MATERIAL_DESTINATION)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        material_name,
        SPRITE_MATERIAL_DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("opacity_mask_clip_value", 0.5)
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    texture_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -480, 0
    )
    texture_node.set_editor_property("texture", texture)
    unreal.MaterialEditingLibrary.connect_material_property(
        texture_node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        texture_node, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        texture_node, "A", unreal.MaterialProperty.MP_OPACITY_MASK
    )

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -480, 220
    )
    roughness.set_editor_property("r", 0.92)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def create_masked_city_sprite_material(color_texture, mask_texture):
    material_name = "M_CitySprite_{0}".format(safe_asset_name(color_texture.get_name()))
    existing = load_asset_if_exists(asset_package_path(CITY_SPRITE_MATERIAL_DESTINATION, material_name))
    if existing:
        return existing

    unreal.EditorAssetLibrary.make_directory(CITY_SPRITE_MATERIAL_DESTINATION)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        material_name,
        CITY_SPRITE_MATERIAL_DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("opacity_mask_clip_value", 0.08)
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    color_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -560, -80
    )
    color_node.set_editor_property("texture", color_texture)
    unreal.MaterialEditingLibrary.connect_material_property(
        color_node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        color_node, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )

    mask_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -560, 180
    )
    mask_node.set_editor_property("texture", mask_texture)
    unreal.MaterialEditingLibrary.connect_material_property(
        mask_node, "R", unreal.MaterialProperty.MP_OPACITY_MASK
    )

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -560, 400
    )
    roughness.set_editor_property("r", 0.9)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def texture_size(texture):
    width = 256
    height = 256
    if hasattr(texture, "blueprint_get_size_x"):
        width = max(1, texture.blueprint_get_size_x())
        height = max(1, texture.blueprint_get_size_y())
    return width, height


def parse_vector_field(body, field_name, fallback):
    match = re.search(
        r"{0}: \{{x: ([^,}}]+), y: ([^,}}]+), z: ([^,}}]+)\}}".format(re.escape(field_name)),
        body,
    )
    if not match:
        return fallback
    return tuple(float(value) for value in match.groups())


def parse_unity_sprite_scene():
    if not os.path.exists(UNITY_SPRITE_SCENE):
        unreal.log_warning("Missing Unity sprite scene reference: {0}".format(UNITY_SPRITE_SCENE))
        return []

    with open(UNITY_SPRITE_SCENE, "r", encoding="utf-8", errors="ignore") as scene_file:
        text = scene_file.read()

    chunk_headers = list(re.finditer(r"^--- !u!(\d+) &(\d+)\n", text, re.MULTILINE))
    chunks = []
    for index, header in enumerate(chunk_headers):
        next_start = chunk_headers[index + 1].start() if index + 1 < len(chunk_headers) else len(text)
        chunks.append((header.group(1), header.group(2), text[header.end():next_start]))

    game_object_names = {}
    game_object_transforms = {}
    transforms = {}
    sprites = []
    for chunk_type, file_id, body in chunks:
        game_object_match = re.search(r"m_GameObject: \{fileID: (\d+)\}", body)
        if chunk_type == "1":
            name_match = re.search(r"m_Name: (.*)", body)
            game_object_names[file_id] = name_match.group(1).strip() if name_match else file_id
        elif chunk_type == "4":
            game_object_id = game_object_match.group(1) if game_object_match else None
            if game_object_id:
                game_object_transforms[game_object_id] = file_id
            father_match = re.search(r"m_Father: \{fileID: (\d+)\}", body)
            transforms[file_id] = {
                "position": parse_vector_field(body, "m_LocalPosition", (0.0, 0.0, 0.0)),
                "scale": parse_vector_field(body, "m_LocalScale", (1.0, 1.0, 1.0)),
                "euler": parse_vector_field(body, "m_LocalEulerAnglesHint", (0.0, 0.0, 0.0)),
                "father": father_match.group(1) if father_match else "0",
            }
        elif chunk_type == "212":
            game_object_id = game_object_match.group(1) if game_object_match else None
            sprite_match = re.search(r"m_Sprite: \{fileID: [^,}]+, guid: ([0-9a-f]+), type: 3\}", body)
            if game_object_id and sprite_match:
                sprites.append({
                    "name": game_object_names.get(game_object_id, game_object_id),
                    "transform": game_object_transforms.get(game_object_id),
                    "guid": sprite_match.group(1),
                })

    world_cache = {}

    def world_transform(transform_id):
        if not transform_id or transform_id == "0" or transform_id not in transforms:
            return (0.0, 0.0, 0.0), (1.0, 1.0, 1.0), (0.0, 0.0, 0.0)
        if transform_id in world_cache:
            return world_cache[transform_id]
        transform = transforms[transform_id]
        parent_position, parent_scale, _ = world_transform(transform["father"])
        local_position = transform["position"]
        local_scale = transform["scale"]
        world_position = tuple(parent_position[i] + local_position[i] * parent_scale[i] for i in range(3))
        world_scale = tuple(parent_scale[i] * local_scale[i] for i in range(3))
        world_cache[transform_id] = (world_position, world_scale, transform["euler"])
        return world_cache[transform_id]

    rows = []
    for sprite in sprites:
        source_path = find_reference_sprite_source(sprite["name"])
        if not source_path:
            continue
        position, scale, euler = world_transform(sprite["transform"])
        rows.append({
            "name": sprite["name"],
            "source_path": source_path,
            "position": position,
            "scale": scale,
            "euler": euler,
        })
    return rows


def spawn_reference_sprite_card(primitives, row, index, material_cache):
    texture = import_city_sprite_texture(row["source_path"])
    if not texture:
        texture = import_reference_texture(row["source_path"])
    if not texture:
        return None

    texture_width, _ = texture_size(texture)
    unity_position = row["position"]
    unity_scale = row["scale"]
    world_scale = 170.0
    sprite_scale = 2.35
    width = max(42.0, texture_width * unity_scale[0] * sprite_scale)
    center_xy = (
        unity_position[0] * world_scale,
        unity_position[2] * world_scale,
    )
    z_order = 32.0 + unity_position[1] * 18.0 + index * 0.35
    actor = spawn_city_sprite_card_from_source(
        "Rok2D_{0}_{1:02d}".format(safe_asset_name(row["name"]), index + 1),
        row["source_path"],
        primitives,
        center_xy,
        z_order,
        width,
        yaw=0.0,
        folder="RokPrototype/Unity2D/Sprites",
        add_building_tags=row["name"].split(" (")[0].startswith("Building"),
    )
    return actor


def add_unity_reference_sprite_scene(primitives):
    rows = parse_unity_sprite_scene()
    material_cache = {}
    spawned = []
    for index, row in enumerate(rows):
        actor = spawn_reference_sprite_card(primitives, row, index, material_cache)
        if actor:
            spawned.append(actor)
    unreal.log("Spawned {0} Unity Map_TTS reference sprite cards".format(len(spawned)))
    return spawned


def ensure_processed_unity_sprite_scene():
    if os.path.exists(UNITY_PROCESSED_SPRITE_MANIFEST) and os.environ.get("ROK_FORCE_PREPARE_UNITY_SPRITES", "0") != "1":
        return

    script_path = os.path.join(unreal.Paths.project_dir(), "Scripts", "PrepareRokUnitySpriteScene.ps1")
    if not os.path.exists(script_path):
        raise RuntimeError("Missing Unity sprite prepare script: {0}".format(script_path))

    command = [
        "powershell",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        script_path,
        "-ProjectRoot",
        unreal.Paths.project_dir(),
        "-SceneName",
        "Map_TTS",
    ]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError("Failed to prepare Unity sprite scene:\nSTDOUT:\n{0}\nSTDERR:\n{1}".format(
            result.stdout,
            result.stderr,
        ))


def ensure_city_build_cutouts():
    if os.path.exists(CITY_BUILD_CUTOUT_MANIFEST) and os.environ.get("ROK_FORCE_PREPARE_CITY_CUTOUTS", "0") != "1":
        return

    script_path = os.path.join(unreal.Paths.project_dir(), "Scripts", "PrepareRokCityBuildCutouts.ps1")
    if not os.path.exists(script_path):
        raise RuntimeError("Missing city build cutout prepare script: {0}".format(script_path))

    command = [
        "powershell",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        script_path,
        "-ProjectRoot",
        unreal.Paths.project_dir(),
    ]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError("Failed to prepare city build cutouts:\nSTDOUT:\n{0}\nSTDERR:\n{1}".format(
            result.stdout,
            result.stderr,
        ))


def add_city_build_cutout_scene(primitives):
    ensure_city_build_cutouts()
    with open(CITY_BUILD_CUTOUT_MANIFEST, "r", encoding="utf-8-sig") as manifest_file:
        rows = json.load(manifest_file)

    rows = sorted(rows, key=lambda row: (int(row["crop"]["y"]), int(row["crop"]["x"])))
    layout = [
        (0.0, 0.0, 1.08), (-310.0, -80.0, 0.76), (310.0, -70.0, 0.76),
        (-170.0, 210.0, 0.70), (185.0, 220.0, 0.70), (-480.0, 180.0, 0.62),
        (480.0, 170.0, 0.62), (-450.0, -330.0, 0.64), (450.0, -320.0, 0.64),
        (-130.0, -395.0, 0.58), (150.0, -400.0, 0.58), (-640.0, -80.0, 0.54),
        (640.0, -70.0, 0.54), (-610.0, 390.0, 0.50), (610.0, 380.0, 0.50),
        (-340.0, 480.0, 0.50), (340.0, 480.0, 0.50), (-30.0, 520.0, 0.48),
    ]
    if not rows:
        unreal.log_warning("No city build cutouts available")
        return []

    selected = []
    used_atlas = {}
    for row in rows:
        atlas = row["atlas"]
        used_atlas[atlas] = used_atlas.get(atlas, 0) + 1
        if used_atlas[atlas] <= 4:
            selected.append(row)
        if len(selected) >= len(layout):
            break
    if len(selected) < len(layout):
        selected = rows[:len(layout)]

    material_cache = {}
    spawned = []
    for index, (row, placement) in enumerate(zip(selected, layout)):
        source_path = row["processed_png"]
        texture = import_city_sprite_texture(source_path)
        if not texture:
            continue
        material = material_cache.get(texture.get_path_name())
        if not material:
            material = create_sprite_material(texture)
            material_cache[texture.get_path_name()] = material

        texture_width, texture_height = texture_size(texture)
        x, y, visual_scale = placement
        target_width = max(120.0, min(360.0, texture_width * visual_scale))
        target_height = target_width * float(texture_height) / float(max(1, texture_width))
        sort_priority = int(y + 1000.0)
        actor = spawn_mesh(
            "RokCityBuild_{0:03d}".format(index),
            primitives["plane"],
            (x, y, 32.0 + sort_priority * 0.002),
            (target_width / 100.0, target_height / 100.0, 1.0),
            material,
            (0.0, 0.0, 0.0),
            "RokPrototype/CityBuildCutouts",
        )
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component:
            try:
                component.set_editor_property("translucency_sort_priority", sort_priority)
            except Exception:
                pass
        spawned.append(actor)

    unreal.log("Spawned {0} clean city build cutout actors".format(len(spawned)))
    return spawned


def add_processed_unity_sprite_scene(primitives):
    ensure_processed_unity_sprite_scene()
    with open(UNITY_PROCESSED_SPRITE_MANIFEST, "r", encoding="utf-8-sig") as manifest_file:
        rows = json.load(manifest_file)

    material_cache = {}
    spawned = []
    world_scale = 170.0
    for row in rows:
        row_name = str(row["name"])
        if not (row_name.startswith("Building_") or row_name == "zhongxinchengbao"):
            continue
        source_path = row["processed_png"]
        texture = import_city_sprite_texture(source_path)
        if not texture:
            continue
        material = material_cache.get(texture.get_path_name())
        if not material:
            material = create_sprite_material(texture)
            material_cache[texture.get_path_name()] = material

        position = row["position"]
        scale = row["scale"]
        size = row["size"]
        width = max(32.0, float(size["x"]) * float(scale["x"]) * world_scale)
        height = max(32.0, float(size["y"]) * float(scale["y"]) * world_scale)
        sort_priority = int(float(position["z"]) * 1000.0) + int(row["index"])
        location = (
            float(position["x"]) * world_scale,
            float(position["z"]) * world_scale,
            28.0 + sort_priority * 0.002,
        )
        actor = spawn_mesh(
            "UnityMapTTS_{0:03d}_{1}".format(int(row["index"]), safe_asset_name(row["name"])),
            primitives["plane"],
            location,
            (width / 100.0, height / 100.0, 1.0),
            material,
            (0.0, 0.0, 0.0),
            "RokPrototype/UnityMapTTS",
        )
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component:
            try:
                component.set_editor_property("translucency_sort_priority", sort_priority)
            except Exception:
                pass
        spawned.append(actor)

    unreal.log("Spawned {0} processed Unity Map_TTS sprite actors".format(len(spawned)))
    return spawned


def city_layout_profile(sprite_name):
    normalized = sprite_name.split(" (")[0]
    if "MainCity" in normalized or "Castle" in normalized or normalized == "zhongxinchengbao":
        return (180.0, 150.0, 132.0, "M_Rok_Gold_Blockout", "M_Rok_Stone_Blockout")
    if "Tower" in normalized:
        return (70.0, 70.0, 122.0, "M_Rok_Terracotta_Blockout", "M_Rok_Stone_Blockout")
    if "Hospital" in normalized:
        return (126.0, 104.0, 82.0, "M_Rok_WallCap_Blockout", "M_Rok_Stone_Blockout")
    if "Military" in normalized or "WorkMan" in normalized:
        return (126.0, 106.0, 78.0, "M_Rok_Terracotta_Blockout", "M_Rok_Wood_Blockout")
    if "Arc" in normalized:
        return (136.0, 96.0, 76.0, "M_Rok_Terracotta_Blockout", "M_Rok_Wood_Blockout")
    if "Farm" in normalized or "Wood" in normalized or "Stone" in normalized or "Gold" in normalized:
        return (102.0, 82.0, 58.0, "M_Rok_Wood_Blockout", "M_Rok_Farm_Blockout")
    if "Book" in normalized or "Hero" in normalized or "Patrol" in normalized:
        return (104.0, 88.0, 74.0, "M_Rok_Blue_Blockout", "M_Rok_Stone_Blockout")
    if "Storehouse" in normalized:
        return (116.0, 92.0, 70.0, "M_Rok_WallCap_Blockout", "M_Rok_Stone_Blockout")
    return (96.0, 80.0, 64.0, "M_Rok_Terracotta_Blockout", "M_Rok_Stone_Blockout")


def add_layout_building_detail(primitives, materials, label, x, y, height, sprite_name):
    normalized = sprite_name.split(" (")[0]
    if "Tower" in normalized:
        cube(label + "_UpperBlock", primitives, materials, (x, y, height + 34.0), (48, 48, 68), "M_Rok_Stone_Blockout", folder="RokPrototype/UnityReferenceScene/Volumetric")
        cube(label + "_RoofBlock", primitives, materials, (x, y, height + 76.0), (66, 66, 20), "M_Rok_Terracotta_Blockout", folder="RokPrototype/UnityReferenceScene/Volumetric")
    elif "MainCity" in normalized or "Castle" in normalized or normalized == "zhongxinchengbao":
        cube(label + "_UpperKeep", primitives, materials, (x, y, height + 44.0), (96, 82, 80), "M_Rok_Stone_Blockout", folder="RokPrototype/UnityReferenceScene/Volumetric")
        cube(label + "_CrownRoof", primitives, materials, (x, y, height + 92.0), (122, 104, 22), "M_Rok_Gold_Blockout", folder="RokPrototype/UnityReferenceScene/Volumetric")
        cube(label + "_FlagBlock", primitives, materials, (x - 58.0, y - 20.0, height + 36.0), (12, 40, 62), "M_Rok_Red_Blockout", yaw=-12.0, folder="RokPrototype/UnityReferenceScene/Volumetric")
    elif "Farm" in normalized or "Wood" in normalized:
        cylinder(label + "_MillPost", primitives, materials, (x + 28.0, y + 8.0, height + 26.0), 10.0, 52.0, "M_Rok_Wood_Blockout", folder="RokPrototype/UnityReferenceScene/Volumetric")
        cube(label + "_MillBlade", primitives, materials, (x + 28.0, y + 8.0, height + 58.0), (68, 10, 10), "M_Rok_WallCap_Blockout", yaw=35.0, folder="RokPrototype/UnityReferenceScene/Volumetric")


def add_unity_layout_city_volumes(primitives, materials):
    rows = parse_unity_sprite_scene()
    spawned = []
    for index, row in enumerate(rows):
        sprite_name = row["name"]
        normalized = sprite_name.split(" (")[0]
        if normalized.startswith("zhedang") or normalized.startswith("World_") or normalized == "City_Main":
            continue
        unity_position = row["position"]
        world_scale = 170.0
        x = unity_position[0] * world_scale
        y = unity_position[2] * world_scale
        width, depth, height, roof, body = city_layout_profile(sprite_name)
        label = "RokLayout_{0}_{1:02d}".format(safe_asset_name(sprite_name), index + 1)
        cube(label, primitives, materials, (x, y, 18.0 + height * 0.5), (width, depth, height), body, yaw=0.0, folder="RokPrototype/UnityReferenceScene/Volumetric")
        cube(label + "_Roof", primitives, materials, (x, y, 36.0 + height), (width + 26.0, depth + 22.0, 28.0), roof, yaw=0.0, folder="RokPrototype/UnityReferenceScene/Volumetric")
        add_layout_building_detail(primitives, materials, label, x, y, 36.0 + height, sprite_name)
        spawned.append(label)
    unreal.log("Spawned {0} Unity Map_TTS layout volumetric city buildings".format(len(spawned)))
    return spawned


def spawn_centered_asset(
    label,
    asset_path,
    center_xy,
    bottom_z,
    uniform_scale,
    material,
    yaw=0.0,
    folder="RokPrototype/ReferenceModels",
):
    mesh = load_static_mesh(asset_path)
    if not mesh:
        return None

    bounds = mesh.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent
    local_min_z = origin.z - extent.z
    offset_x, offset_y = rotate_xy(origin.x * uniform_scale, origin.y * uniform_scale, yaw)
    location = (
        center_xy[0] - offset_x,
        center_xy[1] - offset_y,
        bottom_z - local_min_z * uniform_scale,
    )
    return spawn_mesh(
        label,
        mesh,
        location,
        (uniform_scale, uniform_scale, uniform_scale),
        material,
        (0.0, yaw, 0.0),
        folder,
    )


def spawn_reference_city_model(label, asset_name, center_xy, bottom_z, scale, material, yaw=0.0):
    return spawn_centered_asset(
        label,
        "/Game/RokPrototype/Meshes/City/{0}.{0}".format(asset_name),
        center_xy,
        bottom_z,
        scale,
        material,
        yaw,
        "RokPrototype/ReferenceModels/City",
    )


def spawn_reference_environment_model(label, asset_name, center_xy, bottom_z, scale, material, yaw=0.0):
    return spawn_centered_asset(
        label,
        "/Game/RokPrototype/Meshes/Environment/{0}.{0}".format(asset_name),
        center_xy,
        bottom_z,
        scale,
        material,
        yaw,
        "RokPrototype/ReferenceModels/Environment",
    )


def spawn_reference_environment_plane(label, asset_name, center_xy, bottom_z, scale, material, yaw=0.0, folder="RokPrototype/ReferenceModels/Environment"):
    return spawn_centered_asset(
        label,
        "/Game/RokPrototype/Meshes/Environment/{0}.{0}".format(asset_name),
        center_xy,
        bottom_z,
        scale,
        material,
        yaw,
        folder,
    )


def spawn_resource_sprite_card(label, sprite_key, center_xy, bottom_z, scale, yaw=0.0):
    unreal.log_warning("Reference sprite resource cards are disabled for active maps: {0}".format(label))
    return None


def spawn_mesh(label, mesh, location, scale, material, rotation=(0.0, 0.0, 0.0), folder="RokPrototype"):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_object(
        mesh,
        unreal.Vector(location[0], location[1], location[2]),
        make_rotator(rotation[0], rotation[1], rotation[2]),
    )
    actor.set_actor_label(label)
    actor.set_actor_scale3d(unreal.Vector(scale[0], scale[1], scale[2]))
    actor.set_folder_path(folder)
    assign_material(actor, material)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if component:
        for prop, value in [
            ("cast_shadow", False),
            ("receive_decals", False),
            ("use_as_occluder", False),
            ("collision_enabled", unreal.CollisionEnabled.NO_COLLISION),
        ]:
            try:
                component.set_editor_property(prop, value)
            except Exception:
                pass
    return actor


def find_city_sprite_pair(filename):
    color_path = filename if os.path.isabs(filename) else os.path.join(REFERENCE_SPRITE_DIR, filename)
    stem, extension = os.path.splitext(filename)
    mask_path = "{0}_mask{1}".format(stem, extension) if os.path.isabs(filename) else os.path.join(REFERENCE_SPRITE_DIR, "{0}_mask{1}".format(stem, extension))
    if os.path.exists(color_path) and os.path.exists(mask_path):
        return color_path, mask_path

    if os.path.exists(color_path):
        return color_path, None

    unreal.log_warning(
        "Missing city building sprite pair: color={0}, mask={1}".format(color_path, mask_path)
    )
    return None, None


def masked_material_from_source(source_path):
    stem, extension = os.path.splitext(source_path)
    mask_path = "{0}_mask{1}".format(stem, extension)
    if not os.path.exists(mask_path):
        return None

    color_texture = import_city_sprite_texture(source_path)
    mask_texture = import_city_sprite_texture(mask_path)
    if not color_texture or not mask_texture:
        return None
    return create_masked_city_sprite_material(color_texture, mask_texture)


def spawn_city_sprite_card_from_source(
    label,
    source_path,
    primitives,
    center_xy,
    z_order,
    world_width,
    yaw=0.0,
    folder="RokPrototype/City/Buildings",
    add_building_tags=True,
):
    material = masked_material_from_source(source_path)
    texture = None
    if not material:
        texture = import_reference_texture(source_path)
        if not texture:
            return None
        material = create_sprite_material(texture)
    else:
        texture = import_city_sprite_texture(source_path)
    if not texture:
        return None

    width, height = texture_size(texture)
    world_depth = world_width * float(height) / float(max(1, width))
    actor = spawn_mesh(
        label,
        primitives["plane"],
        (center_xy[0], center_xy[1], z_order),
        (world_width / 100.0, world_depth / 100.0, 1.0),
        material,
        (0.0, yaw, 0.0),
        folder=folder,
    )
    if add_building_tags:
        actor.tags = list(set(list(actor.tags) + ["RokBuilding", "CityBuilding"]))
    return actor


def spawn_city_building_sprite_card(label, filename, primitives, center_xy, bottom_z, world_width, yaw=0.0):
    color_path, mask_path = find_city_sprite_pair(filename)
    if not color_path:
        return None

    if not mask_path:
        return spawn_city_sprite_card_from_source(
            label,
            color_path,
            primitives,
            center_xy,
            bottom_z,
            world_width,
            yaw=yaw,
            folder="RokPrototype/City/Buildings",
            add_building_tags=True,
        )

    return spawn_city_sprite_card_from_source(
        label,
        color_path,
        primitives,
        center_xy,
        bottom_z,
        world_width,
        yaw=yaw,
        folder="RokPrototype/City/Buildings",
        add_building_tags=True,
    )


def cube(label, primitives, materials, location, size, material_name, yaw=0.0, pitch=0.0, roll=0.0, folder="RokPrototype"):
    return spawn_mesh(
        label,
        primitives["cube"],
        location,
        (size[0] / 100.0, size[1] / 100.0, size[2] / 100.0),
        materials[material_name],
        (pitch, yaw, roll),
        folder,
    )


def cylinder(label, primitives, materials, location, radius, height, material_name, folder="RokPrototype"):
    return spawn_mesh(
        label,
        primitives["cylinder"],
        location,
        (radius / 50.0, radius / 50.0, height / 100.0),
        materials[material_name],
        folder=folder,
    )


def sphere(label, primitives, materials, location, radius, material_name, scale_z=1.0, folder="RokPrototype"):
    return spawn_mesh(
        label,
        primitives["sphere"],
        location,
        (radius / 50.0, radius / 50.0, radius / 50.0 * scale_z),
        materials[material_name],
        folder=folder,
    )


def cone(label, primitives, materials, location, radius, height, material_name, folder="RokPrototype"):
    return spawn_mesh(
        label,
        primitives["cone"],
        location,
        (radius / 50.0, radius / 50.0, height / 100.0),
        materials[material_name],
        folder=folder,
    )


def add_text_label(text, location, size=54.0, yaw=0.0):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.TextRenderActor,
        unreal.Vector(location[0], location[1], location[2]),
        make_rotator(65.0, yaw, 0.0),
    )
    actor.set_actor_label("Label_{0}".format(text.replace(" ", "_")))
    actor.set_folder_path("RokPrototype/Labels")
    component = actor.get_component_by_class(unreal.TextRenderComponent)
    component.set_text(text)
    component.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    component.set_world_size(size)
    component.set_text_render_color(unreal.Color(245, 232, 188, 255))
    return actor


def setup_lighting():
    sun = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight,
        unreal.Vector(-2200.0, -1800.0, 3200.0),
        make_rotator(-38.0, -35.0, 0.0),
    )
    sun.set_actor_label("KeySun_RokPrototype")
    sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sun_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sun_component.set_intensity(4.2)
    sun_component.set_editor_property("use_temperature", True)
    sun_component.set_editor_property("temperature", 5600.0)
    sun_component.set_editor_property("light_source_angle", 3.2)
    sun_component.set_editor_property("indirect_lighting_intensity", 1.1)
    sun_component.set_editor_property("atmosphere_sun_light", True)
    if UNITY_RECONSTRUCTION_MODE:
        sun_component.set_intensity(0.25)
        try:
            sun_component.set_editor_property("cast_shadows", False)
        except Exception:
            pass

    skylight = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyLight,
        unreal.Vector(0.0, 0.0, 900.0),
        make_rotator(0.0, 0.0, 0.0),
    )
    skylight.set_actor_label("SkyLight_RokPrototype")
    sky_component = skylight.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky_component.set_intensity(1.15)
    sky_component.set_editor_property("lower_hemisphere_is_black", False)
    sky_component.set_editor_property("indirect_lighting_intensity", 1.0)

    rim = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PointLight,
        unreal.Vector(620.0, 760.0, 560.0),
        make_rotator(0.0, 0.0, 0.0),
    )
    rim.set_actor_label("RimLight_WorldMap_RokPrototype")
    rim_component = rim.get_component_by_class(unreal.PointLightComponent)
    rim_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    rim_component.set_intensity(500.0)
    rim_component.set_editor_property("attenuation_radius", 1800.0)
    rim_component.set_editor_property("source_radius", 700.0)
    rim_component.set_editor_property("cast_shadows", False)

    fill = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PointLight,
        unreal.Vector(-260.0, -420.0, 460.0),
        make_rotator(0.0, 0.0, 0.0),
    )
    fill.set_actor_label("CityFillLight_RokPrototype")
    fill_component = fill.get_component_by_class(unreal.PointLightComponent)
    fill_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    fill_component.set_intensity(350.0)
    fill_component.set_editor_property("attenuation_radius", 1200.0)
    fill_component.set_editor_property("source_radius", 650.0)
    fill_component.set_editor_property("cast_shadows", False)

    sky = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyAtmosphere,
        unreal.Vector(0.0, 0.0, 0.0),
        make_rotator(0.0, 0.0, 0.0),
    )
    sky.set_actor_label("SkyAtmosphere_RokPrototype")

    fog = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.ExponentialHeightFog,
        unreal.Vector(0.0, 0.0, 80.0),
        make_rotator(0.0, 0.0, 0.0),
    )
    fog.set_actor_label("SoftMapFog_RokPrototype")
    fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fog_component.set_editor_property("fog_density", 0.006)
    fog_component.set_editor_property("fog_height_falloff", 0.09)
    fog_component.set_editor_property("fog_max_opacity", 0.34)
    fog_component.set_editor_property("start_distance", 900.0)
    if UNITY_RECONSTRUCTION_MODE:
        fog_component.set_editor_property("fog_density", 0.0)
        fog_component.set_editor_property("fog_max_opacity", 0.0)

    post = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PostProcessVolume,
        unreal.Vector(0.0, 0.0, 300.0),
        make_rotator(0.0, 0.0, 0.0),
    )
    post.set_actor_label("PostProcess_RTS_Exposure")
    post.set_editor_property("unbound", True)
    settings = post.get_editor_property("settings")
    for prop, value in [
        ("override_auto_exposure_method", False),
        ("override_auto_exposure_bias", True),
        ("auto_exposure_bias", -0.25),
        ("override_auto_exposure_min_brightness", True),
        ("auto_exposure_min_brightness", 0.9),
        ("override_auto_exposure_max_brightness", True),
        ("auto_exposure_max_brightness", 1.1),
        ("override_bloom_intensity", True),
        ("bloom_intensity", 0.08),
        ("override_vignette_intensity", True),
        ("vignette_intensity", 0.0),
    ]:
        try:
            settings.set_editor_property(prop, value)
        except Exception:
            pass
    post.set_editor_property("settings", settings)


def add_camera():
    camera = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.CameraActor,
        unreal.Vector(0.0, 0.0, 3200.0),
        make_rotator(-90.0, 0.0, 0.0),
    )
    camera.set_actor_label("Camera_Rok2D_Prototype")
    component = camera.get_component_by_class(unreal.CameraComponent)
    try:
        component.set_editor_property("projection_mode", unreal.CameraProjectionMode.ORTHOGRAPHIC)
    except Exception:
        component.set_editor_property("projection_mode", 1)
    component.set_editor_property("ortho_width", 2800.0)


def create_ground(primitives, materials):
    spawn_reference_environment_plane(
        "WorldBackdrop_FarGround",
        "SM_Rok_Plane180",
        (0.0, 0.0),
        -18.0,
        125.0,
        materials["M_Rok_Grass_Far_Blockout"],
        folder="RokPrototype/Terrain",
    )

    spawn_reference_environment_plane(
        "Ground_StrategyMap_Clean",
        "SM_Rok_Ground",
        (0.0, 0.0),
        -4.0,
        18.0,
        materials["M_Rok_Grass_Blockout"],
        folder="RokPrototype/Terrain",
    )

    if INCLUDE_3D_WORLD_CONTEXT:
        spawn_reference_environment_plane(
            "OriginalRiver_West",
            "SM_Rok_RiverA",
            (-1180.0, -430.0),
            1.0,
            4.6,
            materials["M_Rok_Water_Blockout"],
            -24.0,
            folder="RokPrototype/Terrain",
        )
        spawn_reference_environment_plane(
            "OriginalRiver_South",
            "SM_Rok_RiverB",
            (420.0, -1280.0),
            1.0,
            4.0,
            materials["M_Rok_Water_Blockout"],
            18.0,
            folder="RokPrototype/Terrain",
        )

    if UNITY_RECONSTRUCTION_MODE:
        return

    for index, (x, y, sx, sy, yaw) in enumerate([
        (-1450, 820, 560, 260, -18),
        (1380, 930, 620, 280, 22),
        (-1320, -1320, 520, 220, 30),
        (1470, -810, 480, 240, -28),
    ]):
        spawn_reference_environment_model(
            "OriginalForestMass_{0:02d}".format(index + 1),
            "SM_Rok_TreeA" if index % 2 == 0 else "SM_Rok_TreeB",
            (x, y),
            4.0,
            0.065 if index % 2 == 0 else 0.026,
            materials["M_Rok_Grass_Dark_Blockout"],
            yaw,
        )


def add_roads(primitives, materials):
    spawn_reference_environment_plane(
        "City_Plaza_Round",
        "SM_Rok_Plane60",
        (0.0, 0.0),
        4.0,
        12.0,
        materials["M_Rok_Plaza_Blockout"],
        folder="RokPrototype/City",
    )
    for index, angle in enumerate([0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0]):
        radians = math.radians(angle)
        length = 1220.0 if index % 2 == 0 else 820.0
        x = math.cos(radians) * length * 0.5
        y = math.sin(radians) * length * 0.5
        spawn_reference_environment_plane(
            "RadialRoad_{0:03d}".format(int(angle)),
            "SM_Rok_Plane30",
            (x, y),
            5.0,
            2.8 if index % 2 == 0 else 1.9,
            materials["M_Rok_Path_Blockout"],
            angle,
            folder="RokPrototype/Roads",
        )

    for radius, height, label in [(610.0, 6.0, "InnerRingRoad"), (1180.0, 5.0, "OuterRingRoad")]:
        spawn_reference_environment_plane(
            label,
            "SM_Rok_Plane180",
            (0.0, 0.0),
            height,
            radius / 90.0,
            materials["M_Rok_Path_Blockout"],
            folder="RokPrototype/Roads",
        )

    city_streets = [
        ("CityStreet_NS_01", 0.0, 0.0, 0.0, 760.0, 70.0),
        ("CityStreet_EW_01", 0.0, 0.0, 90.0, 760.0, 70.0),
        ("CityStreet_NE_01", 235.0, 238.0, 45.0, 540.0, 54.0),
        ("CityStreet_NW_01", -235.0, 238.0, -45.0, 540.0, 54.0),
        ("CityStreet_SE_01", 260.0, -260.0, -45.0, 560.0, 54.0),
        ("CityStreet_SW_01", -260.0, -260.0, 45.0, 560.0, 54.0),
    ]
    for label, x, y, yaw, length, width in city_streets:
        cube(label, primitives, materials, (x, y, 9.0), (length, width, 8), "M_Rok_Path_Blockout", yaw=yaw, folder="RokPrototype/Roads/City")


def add_city_wall(primitives, materials):
    original_wall = spawn_reference_city_model(
        "OriginalCityWall_101_1",
        "SM_Rok_CityWall",
        (0.0, 0.0),
        0.0,
        0.026,
        materials["M_Rok_Stone_Blockout"],
    )
    original_gate = spawn_reference_city_model(
        "OriginalCityGate_101_1",
        "SM_Rok_CityGate",
        (0.0, -600.0),
        0.0,
        4.4,
        materials["M_Rok_Stone_Blockout"],
    )
    if original_wall and original_gate:
        for index, angle in enumerate([0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0]):
            radians = math.radians(angle)
            spawn_reference_city_model(
                "OriginalCityTower_101_1_{0:02d}".format(index + 1),
                "SM_Rok_CityTower",
                (math.cos(radians) * 610.0, math.sin(radians) * 610.0),
                0.0,
                0.032,
                materials["M_Rok_Stone_Blockout"],
                angle,
            )
        return

    wall_radius = 500.0
    segment_length = 380.0
    for index in range(8):
        angle = index * 45.0 + 22.5
        radians = math.radians(angle)
        x = math.cos(radians) * wall_radius
        y = math.sin(radians) * wall_radius
        cube(
            "CityWall_Segment_{0:02d}".format(index + 1),
            primitives,
            materials,
            (x, y, 54.0),
            (segment_length, 54, 108),
            "M_Rok_Stone_Blockout",
            yaw=angle + 90.0,
            folder="RokPrototype/City/Wall",
        )
        cube(
            "CityWall_Cap_{0:02d}".format(index + 1),
            primitives,
            materials,
            (x, y, 116.0),
            (segment_length + 28, 68, 16),
            "M_Rok_WallCap_Blockout",
            yaw=angle + 90.0,
            folder="RokPrototype/City/Wall",
        )

    for index in range(8):
        angle = index * 45.0
        radians = math.radians(angle)
        x = math.cos(radians) * 540.0
        y = math.sin(radians) * 540.0
        cylinder("GuardTower_{0:02d}".format(index + 1), primitives, materials, (x, y, 72.0), 62.0, 144.0, "M_Rok_Stone_Blockout", folder="RokPrototype/City/Wall")
        cone("GuardTower_Roof_{0:02d}".format(index + 1), primitives, materials, (x, y, 166.0), 76.0, 54.0, "M_Rok_Terracotta_Blockout", folder="RokPrototype/City/Wall")

    cube("MainGate_South", primitives, materials, (0.0, -546.0, 70.0), (240, 82, 140), "M_Rok_Stone_Blockout", folder="RokPrototype/City/Wall")
    cube("MainGate_Roof_South", primitives, materials, (0.0, -548.0, 154.0), (280, 112, 36), "M_Rok_Terracotta_Blockout", folder="RokPrototype/City/Wall")


def add_city_wall_2d(primitives):
    wall_sprites = [
        ("OriginalCityWall_101_1", city_building_source("Building_101", "city_building_4", "build_6_101_4.png"), (0, -515), 520),
        ("OriginalCityGate_101_1", city_building_source("Building_101", "city_building_4", "build_10_101_4.png"), (0, -565), 360),
        ("OriginalCityTower_101_1_01", city_building_source("Building_101", "city_building_3", "build_8_101_3.png"), (-465, -380), 190),
        ("OriginalCityTower_101_1_02", city_building_source("Building_101", "city_building_3", "build_8_101_3.png"), (465, -380), 190),
        ("OriginalCityTower_101_1_03", city_building_source("Building_101", "city_building_3", "build_9_101_3.png"), (-470, 260), 190),
        ("OriginalCityTower_101_1_04", city_building_source("Building_101", "city_building_3", "build_9_101_3.png"), (470, 260), 190),
    ]
    for index, (label, source, center, width) in enumerate(wall_sprites):
        spawn_city_sprite_card_from_source(
            label,
            source,
            primitives,
            center,
            20.0 + index * 0.25,
            width,
            folder="RokPrototype/City/Wall2D",
            add_building_tags=False,
        )


def add_building(
    primitives,
    materials,
    label,
    x,
    y,
    width,
    depth,
    height,
    roof="M_Rok_Terracotta_Blockout",
    body="M_Rok_Stone_Blockout",
    yaw=0.0,
    style="house",
):
    folder = "RokPrototype/City/Buildings"
    cube(label, primitives, materials, (x, y, height * 0.5 + 8.0), (width, depth, height), body, yaw=yaw, folder=folder)
    cube(label + "_Roof_Main", primitives, materials, (x, y, height + 28.0), (width + 38, depth + 32, 30), roof, yaw=yaw, roll=0.0, folder=folder)
    cube(label + "_Roof_Ridge", primitives, materials, (x, y, height + 52.0), (width + 20, 18, 22), roof, yaw=yaw, folder=folder)
    cube(label + "_Foundation", primitives, materials, (x, y, 8.0), (width + 32, depth + 30, 14), "M_Rok_WallCap_Blockout", yaw=yaw, folder=folder)

    if style in ("barracks", "archery", "stable"):
        cube(label + "_TrainingYard", primitives, materials, (x, y - depth * 0.68, 7.0), (width + 60, 80, 10), "M_Rok_Plaza_Blockout", yaw=yaw, folder=folder)
        for offset in (-0.36, 0.0, 0.36):
            cube(label + "_Banner_{0}".format(int((offset + 1.0) * 100)), primitives, materials, (x + width * offset, y - depth * 0.98, 52.0), (12, 18, 80), "M_Rok_Red_Blockout", yaw=yaw, folder=folder)
    elif style == "hospital":
        cube(label + "_CrossPlate", primitives, materials, (x, y - depth * 0.08, height + 60.0), (42, 12, 44), "M_Rok_Red_Blockout", yaw=yaw, folder=folder)
        cube(label + "_CrossStem", primitives, materials, (x, y - depth * 0.08, height + 60.0), (12, 42, 44), "M_Rok_Red_Blockout", yaw=yaw, folder=folder)
    elif style == "farm":
        for row in range(3):
            cube(label + "_CropRow_{0}".format(row + 1), primitives, materials, (x + (row - 1) * width * 0.28, y, 26.0), (22, depth + 40, 34), "M_Rok_Grass_Dark_Blockout", yaw=yaw, folder=folder)
    elif style == "tower":
        cylinder(label + "_WatchTower", primitives, materials, (x + width * 0.35, y + depth * 0.28, height + 56.0), 26.0, 100.0, body, folder=folder)
        cube(label + "_TowerCap", primitives, materials, (x + width * 0.35, y + depth * 0.28, height + 116.0), (72, 72, 24), roof, yaw=yaw, folder=folder)
    elif style == "tavern":
        cube(label + "_Awning", primitives, materials, (x - width * 0.36, y - depth * 0.48, height * 0.55), (70, 24, 18), "M_Rok_Blue_Blockout", yaw=yaw, folder=folder)
    elif style == "store":
        for offset in (-0.28, 0.28):
            cylinder(label + "_Barrel_{0}".format(int((offset + 1.0) * 100)), primitives, materials, (x + width * offset, y - depth * 0.64, 28.0), 22.0, 44.0, "M_Rok_Wood_Blockout", folder=folder)


def city_building_source(civilization, folder, filename):
    return os.path.join(REFERENCE_CITY_BUILDING_DIR, civilization, folder, filename)


def add_city_buildings(primitives, materials):
    sprite_buildings = [
        ("OriginalTownCenter_CheckpointBig", city_building_source("Building_101", "city_building_3", "build_1_101_3.png"), 0, 0, 430, 0),
        ("OriginalBarracks_01", city_building_source("Building_101", "city_building_2", "build_11_101_2.png"), -345, -205, 275, 0),
        ("OriginalStable_01", city_building_source("Building_101", "city_building_2", "build_12_101_2.png"), 315, -215, 275, 0),
        ("OriginalArcheryRange_01", city_building_source("Building_101", "city_building_2", "build_18_101_2.png"), -355, 155, 275, 0),
        ("OriginalHospital_01", city_building_source("Building_101", "city_building_3", "build_13_101_3.png"), 325, 155, 275, 0),
        ("OriginalAcademy_01", city_building_source("Building_101", "city_building_3", "build_11_101_3.png"), -65, 335, 255, 0),
        ("OriginalTavern_01", city_building_source("Building_101", "city_building_2", "build_14_101_2.png"), 175, 335, 250, 0),
        ("OriginalStorehouse_01", city_building_source("Building_101", "city_building_2", "build_20_101_2.png"), 0, -375, 250, 0),
        ("OriginalQuarryYard_01", city_building_source("Building_101", "city_building_2", "build_4_101_2.png"), 535, -65, 270, 0),
        ("OriginalGoldMine_01", city_building_source("Building_101", "city_building_2", "build_5_101_2.png"), -535, -100, 270, 0),
        ("OriginalLumberMill_01", city_building_source("Building_101", "city_building_3", "build_3_101_3.png"), 505, 295, 255, 0),
        ("OriginalFarmHouse_01", city_building_source("Building_101", "city_building_2", "build_2_101_2.png"), -525, 315, 280, 0),
        ("OriginalWorkshop_01", city_building_source("Building_101", "city_building_3", "build_7_101_3.png"), 20, 520, 245, 0),
        ("OriginalScoutCamp_01", city_building_source("Building_101", "city_building_2", "build_28_101_2.png"), 515, -390, 260, 0),
        ("OriginalMarket_01", city_building_source("Building_101", "city_building_2", "build_3_101_2.png"), -515, -395, 250, 0),
        ("OriginalHouse_01", city_building_source("Building_101", "city_building_1", "build_3_101_1.png"), -170, -455, 220, 0),
        ("OriginalHouse_02", city_building_source("Building_101", "city_building_1", "build_11_101_1.png"), 215, -475, 220, 0),
        ("OriginalHouse_03", city_building_source("Building_101", "city_building_1", "build_17_101_1.png"), -575, 110, 220, 0),
        ("OriginalHouse_04", city_building_source("Building_101", "city_building_1", "build_18_101_1.png"), 590, 115, 220, 0),
        ("OriginalHouse_05", city_building_source("Building_101", "city_building_3", "build_20_101_3.png"), -210, 545, 215, 0),
        ("OriginalHouse_06", city_building_source("Building_101", "city_building_3", "build_21_101_3.png"), 295, 540, 215, 0),
    ]
    fallback_data = {
        "OriginalTownCenter_CheckpointBig": (0, 0, 220, 180, 120, "M_Rok_Terracotta_Blockout", "M_Rok_Stone_Blockout", 0, "tower"),
        "OriginalBarracks_01": (-345, -205, 160, 126, 86, "M_Rok_Terracotta_Blockout", "M_Rok_Stone_Blockout", -18, "barracks"),
        "OriginalStable_01": (315, -215, 166, 126, 78, "M_Rok_Wood_Blockout", "M_Rok_Stone_Blockout", 18, "stable"),
        "OriginalArcheryRange_01": (-355, 155, 178, 116, 76, "M_Rok_Terracotta_Blockout", "M_Rok_Wood_Blockout", 24, "archery"),
        "OriginalHospital_01": (325, 155, 166, 132, 82, "M_Rok_WallCap_Blockout", "M_Rok_Stone_Blockout", -22, "hospital"),
    }
    for label, filename, x, y, width, yaw in sprite_buildings:
        if spawn_city_building_sprite_card(label, filename, primitives, (x, y), 24.0, width, yaw):
            continue
        fallback = fallback_data.get(label)
        if fallback:
            add_building(
                primitives,
                materials,
                label,
                fallback[0],
                fallback[1],
                fallback[2],
                fallback[3],
                fallback[4],
                fallback[5],
                fallback[6],
                fallback[7],
                fallback[8],
            )

    courtyard_units = [
        ("OriginalInfantrySquad_01", -305, -345, -18),
        ("OriginalInfantrySquad_02", -390, -235, -10),
        ("OriginalArcherSquad_01", -405, 15, 24),
        ("OriginalCavalrySquad_01", 310, -360, 18),
        ("OriginalHospitalAttendant_01", 405, 85, -22),
        ("OriginalMarketCrowd_01", -585, -300, 25),
    ]
    for label, x, y, yaw in courtyard_units:
        spawn_reference_environment_model(label, "SM_Rok_MarchUnit", (x, y), 18.0, 0.24, materials["M_Rok_Red_Blockout"], yaw)

    for index, (x, y) in enumerate([(-350, -20), (350, -15), (-170, -330), (170, -330), (-350, 290), (350, 300)]):
        mesh_name = "SM_Rok_TreeA" if index % 2 == 0 else "SM_Rok_TreeB"
        if not spawn_reference_environment_model(
            "OriginalCityDecorTree_{0:02d}".format(index + 1),
            mesh_name,
            (x, y),
            8.0,
            0.034 if mesh_name == "SM_Rok_TreeA" else 0.014,
            materials["M_Rok_Grass_Dark_Blockout"],
            index * 41.0,
        ):
            cylinder("CityDecor_Tree_{0:02d}_Trunk".format(index + 1), primitives, materials, (x, y, 28), 14, 56, "M_Rok_Wood_Blockout", folder="RokPrototype/City/Decor")
            sphere("CityDecor_Tree_{0:02d}_Crown".format(index + 1), primitives, materials, (x, y, 82), 58, "M_Rok_Grass_Dark_Blockout", scale_z=1.15, folder="RokPrototype/City/Decor")


def add_resource_nodes(primitives, materials):
    farms = [(-1040, 240), (-980, 420), (-830, 350), (-910, 170)]
    for index, (x, y) in enumerate(farms):
        label = "OriginalFarmField_{0:02d}".format(index + 1)
        spawn_reference_environment_plane(
            label,
            "SM_Rok_Plane30",
            (x, y),
            9.0,
            2.1,
            materials["M_Rok_Farm_Blockout"],
            12.0 + index * 9.0,
            folder="RokPrototype/Resources/Farm",
        )

    woods = [(1060, 180), (1170, 350), (950, 420), (1220, 70), (860, 260)]
    for index, (x, y) in enumerate(woods):
        mesh_name = "SM_Rok_TreeA" if index % 2 == 0 else "SM_Rok_TreeB"
        if not spawn_reference_environment_model(
            "OriginalWoodGrove_{0:02d}".format(index + 1),
            mesh_name,
            (x, y),
            10.0,
            0.045 if mesh_name == "SM_Rok_TreeA" else 0.018,
            materials["M_Rok_Grass_Dark_Blockout"],
            index * 31.0,
        ):
            cube("WoodNode_{0:02d}_Patch".format(index + 1), primitives, materials, (x, y, 30), (116, 92, 42), "M_Rok_Grass_Dark_Blockout", yaw=index * 18.0, folder="RokPrototype/Resources/Wood")

    for index, (x, y, radius) in enumerate([(760, -780, 88), (910, -930, 66), (660, -1010, 58)]):
        label = "OriginalStoneNode_{0:02d}".format(index + 1)
        spawn_reference_environment_plane(
            label,
            "SM_Rok_StoneResource",
            (x, y),
            10.0,
            2.8 if index == 0 else 2.1,
            materials["M_Rok_Quarry_Blockout"],
            index * 43.0,
            folder="RokPrototype/Resources/Stone",
        )

    for index, (x, y) in enumerate([(-760, -860), (-610, -1010), (-870, -1060)]):
        label = "OriginalGoldNode_{0:02d}".format(index + 1)
        spawn_reference_environment_plane(
            label,
            "SM_Rok_StoneResource",
            (x, y),
            10.0,
            2.0,
            materials["M_Rok_Gold_Blockout"],
            -18.0 + index * 22.0,
            folder="RokPrototype/Resources/Gold",
        )


def add_world_map_systems(primitives, materials):
    spawn_reference_environment_plane(
        "AllianceTerritory_Ring",
        "SM_Rok_Plane60",
        (690.0, 710.0),
        10.0,
        8.4,
        materials["M_Rok_Blue_Blockout"],
        folder="RokPrototype/WorldMap",
    )
    original_flag = spawn_reference_city_model(
        "OriginalAllianceFlag_Icon1",
        "SM_Rok_AllianceFlag",
        (610.0, 650.0),
        16.0,
        950.0,
        materials["M_Rok_Blue_Blockout"],
        -18.0,
    )
    if not original_flag:
        cylinder("AllianceFlag_Pole", primitives, materials, (610.0, 650.0, 100.0), 10.0, 200.0, "M_Rok_Wood_Blockout", folder="RokPrototype/WorldMap")
        cube("AllianceFlag_Cloth", primitives, materials, (648.0, 650.0, 180.0), (80, 8, 54), "M_Rok_Blue_Blockout", folder="RokPrototype/WorldMap")

    route_points = [(-70, -545), (-250, -740), (-420, -900), (-590, -1015), (-760, -1060)]
    for index in range(len(route_points) - 1):
        ax, ay = route_points[index]
        bx, by = route_points[index + 1]
        dx = bx - ax
        dy = by - ay
        length = math.sqrt(dx * dx + dy * dy)
        yaw = math.degrees(math.atan2(dy, dx))
        spawn_reference_environment_plane(
            "MarchRoute_{0:02d}".format(index + 1),
            "SM_Rok_Plane30",
            ((ax + bx) * 0.5, (ay + by) * 0.5),
            20.0,
            length / 300.0,
            materials["M_Rok_Gold_Blockout"],
            yaw,
            folder="RokPrototype/WorldMap/March",
        )

    for index, (x, y) in enumerate(route_points[1:-1]):
        if not spawn_reference_environment_model(
            "OriginalMarchUnit_{0:02d}".format(index + 1),
            "SM_Rok_MarchUnit",
            (x, y),
            26.0,
            0.055,
            materials["M_Rok_Red_Blockout"],
            -35.0 + index * 12.0,
        ):
            cylinder("MarchUnit_{0:02d}".format(index + 1), primitives, materials, (x, y, 50.0), 34, 74, "M_Rok_Red_Blockout", folder="RokPrototype/WorldMap/March")
        if not spawn_reference_environment_model(
            "OriginalMarchFlag_{0:02d}".format(index + 1),
            "SM_Rok_MarchFlag",
            (x + 32.0, y + 18.0),
            42.0,
            0.18,
            materials["M_Rok_Red_Blockout"],
            -35.0 + index * 12.0,
        ):
            cone("MarchUnit_{0:02d}_Banner".format(index + 1), primitives, materials, (x, y, 112.0), 30, 54, "M_Rok_Red_Blockout", folder="RokPrototype/WorldMap/March")


def add_mountains_and_trees(primitives, materials):
    mountains = [
        (-1560, 1240, 220, 260),
        (-1320, 1390, 160, 210),
        (1510, 1210, 210, 250),
        (1300, 1430, 145, 190),
        (1540, -1320, 180, 240),
        (-1510, -1460, 170, 210),
    ]
    for index, (x, y, radius, height) in enumerate(mountains):
        mesh_name = "SM_Rok_MountainA" if index % 2 == 0 else "SM_Rok_MountainB"
        if not spawn_reference_environment_model(
            "OriginalMountain_{0:02d}".format(index + 1),
            mesh_name,
            (x, y),
            0.0,
            0.12 if mesh_name == "SM_Rok_MountainA" else 0.18,
            materials["M_Rok_Quarry_Blockout"],
            index * 37.0,
        ):
            cone("Mountain_{0:02d}".format(index + 1), primitives, materials, (x, y, height * 0.5), radius, height, "M_Rok_Quarry_Blockout", folder="RokPrototype/Terrain/Mountains")

    spawn_reference_environment_model(
        "OriginalBridge_Route",
        "SM_Rok_Bridge",
        (-980.0, -520.0),
        2.0,
        0.72,
        materials["M_Rok_Wood_Blockout"],
        24.0,
    )

    for index in range(28):
        angle = index * (math.pi * 2.0 / 28.0)
        radius = 1420 + (index % 4) * 95
        x = math.cos(angle) * radius
        y = math.sin(angle) * radius
        if -0.25 < math.sin(angle) < 0.18 and math.cos(angle) < -0.45:
            continue
        mesh_name = "SM_Rok_TreeA" if index % 2 == 0 else "SM_Rok_TreeB"
        if not spawn_reference_environment_model(
            "OriginalWorldTreePatch_{0:02d}".format(index + 1),
            mesh_name,
            (x, y),
            4.0,
            0.052 if mesh_name == "SM_Rok_TreeA" else 0.021,
            materials["M_Rok_Grass_Dark_Blockout"],
            index * 17.0,
        ):
            cube("WorldTreePatch_{0:02d}".format(index + 1), primitives, materials, (x, y, 32), (130, 96, 44), "M_Rok_Grass_Dark_Blockout", yaw=index * 17.0, folder="RokPrototype/Terrain/Trees")


def add_reference_labels():
    add_text_label("City Lv.12", (0.0, -705.0, 190.0), 64.0)
    add_text_label("Alliance Territory", (690.0, 420.0, 120.0), 44.0, yaw=18.0)
    add_text_label("Marching Army", (-510.0, -850.0, 96.0), 40.0, yaw=-35.0)
    add_text_label("Farm / Wood / Stone / Gold", (-1050.0, 20.0, 86.0), 36.0, yaw=12.0)


def build_scene(level_path):
    if unreal.EditorAssetLibrary.does_asset_exist(level_path):
        unreal.EditorLevelLibrary.load_level(level_path)
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            unreal.EditorLevelLibrary.destroy_actor(actor)
    else:
        unreal.EditorLoadingAndSavingUtils.new_blank_map(False)

    ensure_project_primitive_meshes()
    primitives = {name: load_primitive(name) for name in PRIMITIVE_PATHS}
    materials = make_materials()

    setup_lighting()
    add_camera()
    create_ground(primitives, materials)
    if UNITY_RECONSTRUCTION_MODE:
        add_city_build_cutout_scene(primitives)
    else:
        add_roads(primitives, materials)
        add_city_wall_2d(primitives)
        add_city_buildings(primitives, materials)
        add_resource_nodes(primitives, materials)

    if INCLUDE_3D_WORLD_CONTEXT and not UNITY_RECONSTRUCTION_MODE:
        add_world_map_systems(primitives, materials)
        add_mountains_and_trees(primitives, materials)
    if INCLUDE_DEBUG_LABELS:
        add_reference_labels()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, level_path):
        raise RuntimeError("Failed to save prototype level: {0}".format(level_path))


def main():
    ensure_project_primitive_meshes()
    import_reference_static_meshes()
    build_scene(TARGET_LEVEL)
    if BASE_LEVEL and unreal.EditorAssetLibrary.does_asset_exist(BASE_LEVEL):
        unreal.EditorAssetLibrary.delete_asset(BASE_LEVEL)
    if BASE_LEVEL and unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        unreal.EditorAssetLibrary.duplicate_asset(TARGET_LEVEL, BASE_LEVEL)
        unreal.EditorAssetLibrary.save_asset(BASE_LEVEL, only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_asset(TARGET_LEVEL, only_if_is_dirty=False)
    unreal.log("Clean RoK-style prototype scene created at {0}".format(TARGET_LEVEL))


if __name__ == "__main__":
    main()
