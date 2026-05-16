import csv
import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokPrototypeScene as proto
import RokCity2DLayout as city2d


TARGET_LEVEL = os.environ.get("ROK_RUNTIME_CITY_LEVEL", "/Game/Maps/L_RokCityRuntime")
GAME_MODE_CLASS = "/Script/Rok.RokGameModeBase"
CITY_SPRITE_ACTOR_CLASS = "/Script/Rok.RokCitySpriteActor"
CIVILIZATION = int(os.environ.get("ROK_RUNTIME_CITY_CIV", "101"))
AGE = int(os.environ.get("ROK_RUNTIME_CITY_AGE", "3"))
TARGET_SCENE_PASS = os.environ.get("ROK_RUNTIME_CITY_TARGET_SCENE", "1") != "0"
NAMED_CIVILIZATION = os.environ.get("ROK_RUNTIME_CITY_NAMED_CIV", "6")
NAMED_AGE = os.environ.get("ROK_RUNTIME_CITY_NAMED_AGE", "5")

CONFIG_TABLE_ROOT = os.path.join(PROJECT_ROOT, "ref", "config-tables")
LAND_TEXTURE_ROOT = os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "land", "Texture")
LAND_SHARED_TEXTURE_ROOT = os.path.join(LAND_TEXTURE_ROOT, "shared_landtexture")
WILD_BUILDING_ROOT = os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "Map", "Building", "Buildingwild_use")
EXPORTED_SPRITE_ROOT = os.path.join(PROJECT_ROOT, "ref", "resources", "Sprite")
if not os.path.isdir(EXPORTED_SPRITE_ROOT):
    EXPORTED_SPRITE_ROOT = os.path.join(PROJECT_ROOT, "ref", "resources", "rok导出资源", "Sprite")

RUNTIME_TEXTURE_DESTINATION = "/Game/RokPrototype/Textures/RuntimeCity"
RUNTIME_MATERIAL_DESTINATION = "/Game/RokPrototype/Materials/RuntimeCity"
REBUILT_RUNTIME_SPRITE_MATERIALS = set()
NAMED_CUTOUT_ROOT = os.environ.get(
    "ROK_NAMED_CUTOUT_ROOT",
    os.path.join(PROJECT_ROOT, "Saved", "RokDerivedSprites", "StructureCutouts"),
)
LAYERED_CUTOUT_ROOT = os.environ.get(
    "ROK_LAYERED_CUTOUT_ROOT",
    os.path.join(PROJECT_ROOT, "Saved", "RokDerivedSprites", "LayeredSceneCutouts"),
)
LAYERED_GROUND_PLATE_PATH = os.environ.get(
    "ROK_LAYERED_GROUND_PLATE",
    os.path.join(PROJECT_ROOT, "Saved", "RokDerivedSprites", "LayeredGround", "LayeredGroundPlate.png"),
)
LAYERED_GROUND_PLATE_MASK_PATH = os.environ.get(
    "ROK_LAYERED_GROUND_PLATE_MASK",
    os.path.join(PROJECT_ROOT, "Saved", "RokDerivedSprites", "LayeredGround", "LayeredGroundPlate_mask.png"),
)
LAYERED_GRASS_FILL_PATH = os.environ.get(
    "ROK_LAYERED_GRASS_FILL",
    os.path.join(PROJECT_ROOT, "Saved", "RokDerivedSprites", "LayeredGround", "LayeredGrassFill.png"),
)
CITY_BUILDING_TEXTURE_ROOT = os.path.join(
    PROJECT_ROOT,
    "ref",
    "client-unity",
    "Assets",
    "BundleAssets",
    "Map",
    "Building",
    "Building_use",
    "shared_building",
    "Building_{0}".format(CIVILIZATION),
)

UE_UNITS_PER_UNITY_UNIT = 920.0
UNITY_GRID_SIZE = 0.1
UNITY_GRID_SIZE_HALF = 0.05
CITY_CENTER_TILE = 24.0

CAMERA_LOCATION = unreal.Vector(-2200.0, -2200.0, 4450.0)
CAMERA_TARGET = unreal.Vector(0.0, 0.0, 0.0)
CAMERA_ORTHO_WIDTH = 1800.0
SPRITE_PLANE_PITCH = float(os.environ.get("ROK_CITY_SPRITE_PLANE_PITCH", "35.0"))
SPRITE_PLANE_YAW = float(os.environ.get("ROK_CITY_SPRITE_PLANE_YAW", "-135.0"))
SPRITE_PLANE_ROLL = float(os.environ.get("ROK_CITY_SPRITE_PLANE_ROLL", "0.0"))
SPRITE_PLANE_MODE = os.environ.get("ROK_CITY_SPRITE_PLANE_MODE", "camera")
SPRITE_TRANSLUCENT_ALPHA = os.environ.get("ROK_CITY_SPRITE_TRANSLUCENT_ALPHA", "0") == "1"
UNITY_PREFAB_SPRITE_SCALE = float(os.environ.get("ROK_CITY_PREFAB_SPRITE_SCALE", "0.5271909"))
UNITY_PREFAB_SPRITE_PPU = float(os.environ.get("ROK_CITY_PREFAB_SPRITE_PPU", "440.0"))
UNITY_PREFAB_SPRITE_OFFSET = (
    float(os.environ.get("ROK_CITY_PREFAB_OFFSET_X", "0.015")),
    float(os.environ.get("ROK_CITY_PREFAB_OFFSET_Y", "0.024")),
    float(os.environ.get("ROK_CITY_PREFAB_OFFSET_Z", "0.019")),
)

BUILDING_DISPLAY_NAMES = {
    1: "TownCenter",
    2: "Farm",
    3: "Sawmill",
    4: "Quarry",
    5: "GoldMine",
    6: "House",
    7: "Workshop",
    8: "Decoration",
    9: "Barracks",
    10: "Stable",
    11: "ArcheryRange",
    12: "SiegeWorkshop",
    13: "Academy",
    14: "Hospital",
    15: "Storage",
    16: "AllianceCenter",
    17: "Castle",
    18: "Tavern",
    19: "TradingPost",
    20: "Market",
    21: "CourierStation",
    28: "ScoutCamp",
    29: "WatchPost",
    30: "Monument",
    31: "Shrine",
}

# Dense city-base layout. Runtime placement still uses the Unity tile-to-local formula.
CITY_BUILDING_LAYOUT = [
    (1, 22, 22),
    (17, 27, 27), (16, 17, 27), (13, 22, 29), (18, 17, 22), (15, 27, 21),
    (9, 14, 18), (10, 19, 17), (11, 25, 17), (12, 30, 18), (14, 31, 23),
    (28, 33, 16), (30, 11, 25), (19, 24, 34), (20, 18, 34), (21, 30, 34),
    (6, 12, 14), (6, 16, 13), (6, 20, 12), (6, 24, 12), (6, 28, 13), (6, 32, 14),
    (7, 12, 30), (7, 15, 32), (7, 33, 30), (7, 36, 27),
    (8, 9, 21), (8, 35, 21), (29, 8, 29), (29, 36, 18), (31, 22, 37),
    (2, 7, 9), (2, 10, 10), (2, 13, 9), (2, 8, 13), (2, 12, 14), (2, 16, 10), (2, 16, 15),
    (3, 30, 8), (3, 34, 9), (3, 37, 11), (3, 31, 12), (3, 35, 14), (3, 39, 15),
    (4, 32, 24), (4, 35, 24), (4, 38, 25), (4, 33, 28), (4, 37, 29),
    (5, 7, 31), (5, 10, 34), (5, 13, 36), (5, 6, 36), (5, 14, 30),
    (6, 20, 16), (7, 23, 15), (8, 25, 31), (29, 18, 19),
]

ROAD_SPECS = [
    ("RuntimeRoad_Center_NE", 0.0, 0.0, 3300.0, 82.0, 45.0),
    ("RuntimeRoad_Center_NW", 0.0, 0.0, 3300.0, 82.0, -45.0),
    ("RuntimeRoad_West_NE", -720.0, 350.0, 2300.0, 58.0, 45.0),
    ("RuntimeRoad_East_NE", 720.0, -350.0, 2300.0, 58.0, 45.0),
    ("RuntimeRoad_North_NW", 710.0, 450.0, 2400.0, 58.0, -45.0),
    ("RuntimeRoad_South_NW", -710.0, -450.0, 2400.0, 58.0, -45.0),
    ("RuntimeRoad_ResourceSouth", -950.0, -1130.0, 1550.0, 54.0, 10.0),
    ("RuntimeRoad_ResourceEast", 1130.0, -1020.0, 1500.0, 54.0, -18.0),
    ("RuntimeRoad_MarketNorth", 520.0, 1160.0, 1550.0, 58.0, 12.0),
    ("RuntimeRoad_GoldWest", -1220.0, 950.0, 1280.0, 54.0, -16.0),
    ("RuntimeRoad_InnerRingA", 0.0, 520.0, 1450.0, 48.0, 0.0),
    ("RuntimeRoad_InnerRingB", 0.0, -520.0, 1450.0, 48.0, 0.0),
    ("RuntimeRoad_InnerRingC", -520.0, 0.0, 1450.0, 48.0, 90.0),
    ("RuntimeRoad_InnerRingD", 520.0, 0.0, 1450.0, 48.0, 90.0),
]

RESOURCE_DECOR_SPECS = [
    ("FarmPatch", "farm_1.png", "farm_1_mask.png", (-1360, -1420), 270, 230),
    ("FarmPatch", "farm_2.png", "farm_2_mask.png", (-1080, -1500), 260, 220),
    ("FarmPatch", "farm_3.png", "farm_3_mask.png", (-780, -1410), 260, 220),
    ("FarmPatch", "farm_4.png", "farm_4_mask.png", (-1240, -1170), 250, 210),
    ("WoodPile", "wood_1.png", "wood_1_mask.png", (930, -1420), 255, 230),
    ("WoodPile", "wood_11.png", "wood_11_mask.png", (1210, -1330), 245, 225),
    ("WoodPile", "wood_12.png", "wood_12_mask.png", (1450, -1110), 245, 225),
    ("StonePile", "stone_1.png", "stone_1_mask.png", (1110, 260), 250, 215),
    ("StonePile", "stone_2.png", "stone_2_mask.png", (1390, 390), 250, 215),
    ("StonePile", "stone_3.png", "stone_3_mask.png", (1260, 660), 250, 215),
    ("GoldPile", "gold_1.png", "gold_1_mask.png", (-1420, 900), 245, 215),
    ("GoldPile", "gold_2.png", "gold_2_mask.png", (-1160, 1160), 245, 215),
    ("GoldPile", "gold_3.png", "gold_3_mask.png", (-870, 1030), 245, 215),
]

EXPORT_DECOR_SPECS = [
    ("AllianceFlag", "AllianceCenter_flag_1_5_00.png", (-550, 1220), 145, 220),
    ("AllianceFlag", "AllianceCenter_flag_1_5_04.png", (780, 1120), 145, 220),
    ("AllianceFlag", "AllianceCenter_flag_1_5_08.png", (1320, -250), 145, 220),
    ("AllianceFlag", "AllianceCenter_flag_1_5_12.png", (-1320, -120), 145, 220),
]

BUILDING_FALLBACK_SPRITES = {
    5: ("Goldmine_1_5.png", "Goldmine_1_5_mask.png"),
    6: ("Castle_1_3.png", "Castle_1_3_mask.png"),
    8: ("Monument_1_4.png", "Monument_1_4_mask.png"),
    12: ("Barracks_1_5.png", "Barracks_1_5_mask.png"),
    16: ("AllianceCenter_1_5.png", "AllianceCenter_1_5_mask.png"),
    19: ("Castle_6_3.png", "Castle_6_3_mask.png"),
    29: ("ScoutCamp_1_4.png", "ScoutCamp_1_4_mask.png"),
    30: ("Monument_1_4.png", "Monument_1_4_mask.png"),
    31: ("Monument_6_4.png", "Monument_6_4_mask.png"),
}

NAMED_SPRITE_BASES = {
    1: "TownCenter",
    9: "Barracks",
    10: "Stable",
    11: "Archery",
    12: "SiegeWorkshop",
    13: "Campus",
    14: "Hospital",
    16: "AllianceCenter",
    17: "Castle",
    18: "Tavern",
}

# First target-scene pass based on the user's RoK city screenshot: dense center city,
# resource production rows in the foreground, and wall anchors at the lower edge.
TARGET_CITY_BUILDING_LAYOUT = [
    (1, "TownCenter", 40, 150, 0.88),
    (17, "CastleUpper", -360, 430, 0.58),
    (16, "AllianceCenter", 360, 420, 0.58),
    (13, "AcademyWest", -430, 150, 0.56),
    (18, "TavernEast", 455, 120, 0.54),
    (9, "BarracksWest", -610, -130, 0.58),
    (10, "StableCenter", -110, -130, 0.60),
    (11, "ArcheryEast", 430, -145, 0.58),
    (12, "SiegeEast", 745, -210, 0.54),
    (14, "HospitalUpperEast", 690, 235, 0.50),
    (9, "BarracksUpperWest", -720, 300, 0.46),
    (10, "StableUpperCenter", -115, 505, 0.45),
    (11, "ArcheryLowerWest", -520, -470, 0.47),
    (12, "SiegeLowerEast", 500, -560, 0.47),
    (13, "AcademyLower", -105, -610, 0.44),
    (18, "TavernLowerEast", 795, -545, 0.42),
    (14, "HospitalLower", 155, -760, 0.42),
]

TARGET_TERRAIN_LAYERS = [
    ("FloorTile_1_5_9", 0, 65, 1.18, -9400),
    ("FloorTile_1_5_9", -390, 45, 0.92, -9390),
    ("FloorTile_1_5_9", 390, 45, 0.92, -9390),
    ("FloorTile_1_5_9", -410, -330, 0.92, -9380),
    ("FloorTile_1_5_9", 380, -355, 0.92, -9380),
    ("FloorTile_1_5_9", 0, -650, 0.78, -9370),
    ("Road_1_1_5", -260, 245, 3.1, -9300),
    ("Road_1_1_5", 260, 245, 3.1, -9300),
    ("Road_1_1_5", -285, -135, 3.0, -9290),
    ("Road_1_1_5", 285, -135, 3.0, -9290),
    ("Road_1_1_5", -205, -515, 2.65, -9280),
    ("Road_1_1_5", 210, -515, 2.65, -9280),
    ("Road_1_5_0", -690, 90, 2.25, -9270),
    ("Road_1_5_4", 690, 75, 2.25, -9270),
    ("Road_1_5_8", -660, -565, 2.10, -9260),
    ("Road_1_5_12", 690, -570, 2.10, -9260),
]

TARGET_RESOURCE_LAYOUT = [
    ("Farm", "farm_1.png", "farm_1_mask.png", -1040, -545, 300, 250, "food"),
    ("Farm", "farm_2.png", "farm_2_mask.png", -760, -610, 295, 245, "food"),
    ("Farm", "farm_3.png", "farm_3_mask.png", -475, -560, 295, 245, "food"),
    ("Farm", "farm_4.png", "farm_4_mask.png", -910, -820, 285, 235, "food"),
    ("Farm", "farm_5.png", "farm_5_mask.png", -610, -870, 285, 235, "food"),
    ("Stone", "stone_1.png", "stone_1_mask.png", 280, -565, 260, 225, "stone"),
    ("Stone", "stone_2.png", "stone_2_mask.png", 545, -650, 260, 225, "stone"),
    ("Stone", "stone_3.png", "stone_3_mask.png", 820, -565, 260, 225, "stone"),
    ("Wood", "wood_1.png", None, 1020, -250, 265, 235, "wood"),
    ("Wood", "wood_11.png", "wood_11_mask.png", 1260, -405, 255, 225, "wood"),
    ("Wood", "wood_12.png", "wood_12_mask.png", 1120, -675, 255, 225, "wood"),
    ("Gold", "gold_1.png", "gold_1_mask.png", -1280, -1010, 265, 230, "gold"),
    ("Gold", "gold_2.png", "gold_2_mask.png", -1035, -1190, 265, 230, "gold"),
    ("Gold", "gold_3.png", "gold_3_mask.png", -760, -1120, 265, 230, "gold"),
]

LAYERED_TERRAIN_LAYERS = city2d.GROUND_TILES
LAYERED_CITY_BUILDING_LAYOUT = city2d.BUILDINGS
LAYERED_DECOR_LAYOUT = city2d.DECOR
LAYERED_WALL_SPRITE_LAYOUT = city2d.WALL_SPRITES

LAYERED_WALL_MODEL_LAYOUT = [
    ("RokLayeredWall_Main", "SM_Rok_CityWall", (-610.0, 610.0), 0.0, 0.020, -35.0),
    ("RokLayeredWall_Gate", "SM_Rok_CityGate", (-435.0, 435.0), 0.0, 2.85, -35.0),
    ("RokLayeredWall_Tower_Left", "SM_Rok_CityTower", (-930.0, 535.0), 0.0, 0.026, -35.0),
    ("RokLayeredWall_Tower_Right", "SM_Rok_CityTower", (-45.0, 790.0), 0.0, 0.026, -35.0),
]

RESOURCE_MARKER_ICONS = {
    "food": "img_food.png",
    "wood": "img_wood.png",
    "stone": "img_stone.png",
    "gold": "img_gold.png",
}


def find_config_csv_dir():
    for root, _, files in os.walk(CONFIG_TABLE_ROOT):
        if "BuildingModelData.csv" in files and "BuildingTypeConfig.csv" in files:
            return root
    raise RuntimeError("Could not find generated config CSV directory under {0}".format(CONFIG_TABLE_ROOT))


CONFIG_CSV_DIR = find_config_csv_dir()
BUILDING_MODEL_CSV = os.path.join(CONFIG_CSV_DIR, "BuildingModelData.csv")
BUILDING_TYPE_CSV = os.path.join(CONFIG_CSV_DIR, "BuildingTypeConfig.csv")
CAMERA_PARAM_CSV = os.path.join(CONFIG_CSV_DIR, "CameraParamLod.csv")
CITY_AGE_CSV = os.path.join(CONFIG_CSV_DIR, "CityAgeSize.csv")


def read_csv_records(path):
    with open(path, "r", encoding="utf-8-sig", newline="") as csv_file:
        reader = csv.DictReader(csv_file)
        rows = []
        for row in reader:
            first_value = next(iter(row.values()))
            if first_value in ("int", "string", "float"):
                continue
            rows.append(row)
        return rows


def load_building_model_map():
    result = {}
    for row in read_csv_records(BUILDING_MODEL_CSV):
        if int(row["civType"]) != CIVILIZATION or int(row["age"]) != AGE:
            continue
        result[int(row["type"])] = row["modelId"]
    return result


def load_building_type_map():
    result = {}
    for row in read_csv_records(BUILDING_TYPE_CSV):
        result[int(row["type"])] = {
            "width": int(row["width"]),
            "length": int(row["length"]),
        }
    return result


def load_city_age_record():
    for row in read_csv_records(CITY_AGE_CSV):
        if int(row["age"]) == AGE:
            return row
    return None


def load_camera_record():
    for row in read_csv_records(CAMERA_PARAM_CSV):
        if row["ID"] == "city":
            return row
    return None


def source_if_exists(*parts):
    path = os.path.join(*parts)
    return path if os.path.exists(path) else None


def import_runtime_texture_as(source_path, asset_name, is_mask=False):
    package_path = proto.asset_package_path(RUNTIME_TEXTURE_DESTINATION, proto.safe_asset_name(asset_name))
    existing = proto.load_asset_if_exists(package_path)
    force_reimport = os.environ.get("ROK_FORCE_REIMPORT_TEXTURES", "0") == "1"
    if existing and not force_reimport:
        return existing

    unreal.EditorAssetLibrary.make_directory(RUNTIME_TEXTURE_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", RUNTIME_TEXTURE_DESTINATION)
    task.set_editor_property("destination_name", proto.safe_asset_name(asset_name))
    task.set_editor_property("factory", unreal.TextureFactory())
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", False)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = proto.load_asset_if_exists(package_path)
    if texture:
        texture.set_editor_property("srgb", not is_mask)
        texture.set_editor_property("never_stream", True)
        try:
            texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            texture.set_editor_property(
                "compression_settings",
                unreal.TextureCompressionSettings.TC_MASKS if is_mask else unreal.TextureCompressionSettings.TC_EDITOR_ICON,
            )
            texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
            texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        except Exception:
            pass
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def building_texture_path(model_id):
    age_dir = "city_building_{0}".format(AGE)
    path = os.path.join(CITY_BUILDING_TEXTURE_ROOT, age_dir, "{0}.png".format(model_id))
    if os.path.exists(path):
        return path
    for root, _, files in os.walk(CITY_BUILDING_TEXTURE_ROOT):
        filename = "{0}.png".format(model_id)
        if filename in files:
            return os.path.join(root, filename)
    return None


def building_mask_path(model_id):
    color_path = building_texture_path(model_id)
    if not color_path:
        return None
    root = os.path.dirname(color_path)
    mask_path = os.path.join(root, "{0}_mask.png".format(model_id))
    return mask_path if os.path.exists(mask_path) else None


def fallback_building_texture_path(building_type):
    spec = BUILDING_FALLBACK_SPRITES.get(building_type)
    if not spec:
        return None
    return source_if_exists(EXPORTED_SPRITE_ROOT, spec[0])


def fallback_building_mask_path(building_type):
    spec = BUILDING_FALLBACK_SPRITES.get(building_type)
    if not spec or len(spec) < 2:
        return None
    return source_if_exists(EXPORTED_SPRITE_ROOT, spec[1])


def named_building_texture_path(building_type):
    sprite_base = NAMED_SPRITE_BASES.get(building_type)
    if not sprite_base:
        return None, None
    sprite_name = "{0}_{1}_{2}".format(sprite_base, NAMED_CIVILIZATION, NAMED_AGE)
    cutout_path = os.path.join(NAMED_CUTOUT_ROOT, "{0}.png".format(sprite_name))
    source_path = cutout_path if os.path.exists(cutout_path) else os.path.join(EXPORTED_SPRITE_ROOT, "{0}.png".format(sprite_name))
    if not os.path.exists(source_path):
        return None, None
    return sprite_name, source_path


def layered_building_texture_path(building_type):
    sprite_name, source_path = named_building_texture_path(building_type)
    if not sprite_name:
        return None, None
    layered_path = os.path.join(LAYERED_CUTOUT_ROOT, "{0}.png".format(sprite_name))
    if os.path.exists(layered_path):
        return sprite_name, layered_path
    return sprite_name, source_path


def unity_tile_to_ue(building_type, tile_x, tile_y, building_types):
    size = building_types.get(building_type, {"width": 5, "length": 5})
    unity_x = tile_x * UNITY_GRID_SIZE + size["width"] * UNITY_GRID_SIZE_HALF - UNITY_GRID_SIZE_HALF
    unity_y = tile_y * UNITY_GRID_SIZE + size["length"] * UNITY_GRID_SIZE_HALF - UNITY_GRID_SIZE_HALF
    center_offset = CITY_CENTER_TILE * UNITY_GRID_SIZE
    return (
        (unity_x - center_offset) * UE_UNITS_PER_UNITY_UNIT,
        (unity_y - center_offset) * UE_UNITS_PER_UNITY_UNIT,
    )


def texture_dimensions(texture):
    return proto.texture_size(texture)


def create_textured_surface_material(texture, material_name, blend_mode=unreal.BlendMode.BLEND_OPAQUE, opacity_clip=0.0):
    existing = proto.load_asset_if_exists(proto.asset_package_path(RUNTIME_MATERIAL_DESTINATION, material_name))
    if existing:
        return existing

    unreal.EditorAssetLibrary.make_directory(RUNTIME_MATERIAL_DESTINATION)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        material_name,
        RUNTIME_MATERIAL_DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    material.set_editor_property("blend_mode", blend_mode)
    material.set_editor_property("two_sided", True)
    if opacity_clip > 0.0:
        material.set_editor_property("opacity_mask_clip_value", opacity_clip)
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    texture_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -480, 0
    )
    texture_node.set_editor_property("texture", texture)
    unreal.MaterialEditingLibrary.connect_material_property(texture_node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(texture_node, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if blend_mode == unreal.BlendMode.BLEND_MASKED:
        unreal.MaterialEditingLibrary.connect_material_property(texture_node, "A", unreal.MaterialProperty.MP_OPACITY_MASK)

    roughness = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -480, 220)
    roughness.set_editor_property("r", 0.94)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def create_soft_shadow_material():
    material_name = "M_RuntimeCity_SoftShadow"
    existing = proto.load_asset_if_exists(proto.asset_package_path(RUNTIME_MATERIAL_DESTINATION, material_name))
    if existing:
        return existing

    unreal.EditorAssetLibrary.make_directory(RUNTIME_MATERIAL_DESTINATION)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        material_name,
        RUNTIME_MATERIAL_DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -380, -40)
    color.set_editor_property("constant", unreal.LinearColor(0.015, 0.025, 0.018, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    opacity = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -380, 170)
    opacity.set_editor_property("r", 0.38)
    unreal.MaterialEditingLibrary.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def create_unlit_constant_material(material_name, color):
    material_path = proto.asset_package_path(RUNTIME_MATERIAL_DESTINATION, material_name)
    existing = proto.load_asset_if_exists(material_path)
    if existing:
        unreal.EditorAssetLibrary.delete_asset(material_path)

    unreal.EditorAssetLibrary.make_directory(RUNTIME_MATERIAL_DESTINATION)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        material_name,
        RUNTIME_MATERIAL_DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    material.set_editor_property("two_sided", True)
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    color_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -360, -40
    )
    color_node.set_editor_property("constant", color)
    unreal.MaterialEditingLibrary.connect_material_property(color_node, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(color_node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def import_runtime_texture(source_path):
    texture = proto.import_texture_to_destination(source_path, RUNTIME_TEXTURE_DESTINATION)
    if texture:
        texture.set_editor_property("srgb", True)
        texture.set_editor_property("never_stream", True)
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def make_runtime_materials():
    materials = proto.make_materials()
    materials["M_RuntimeCity_SoftShadow"] = create_soft_shadow_material()
    materials["M_RuntimeCity_LayeredGrassBase"] = create_unlit_constant_material(
        "M_RuntimeCity_LayeredGrassBase",
        unreal.LinearColor(0.20, 0.36, 0.14, 1.0),
    )
    materials["M_RuntimeCity_LayeredGrassBlend"] = create_unlit_constant_material(
        "M_RuntimeCity_LayeredGrassBlend",
        unreal.LinearColor(0.23, 0.41, 0.16, 1.0),
    )
    if os.path.exists(LAYERED_GRASS_FILL_PATH):
        grass_texture = import_runtime_texture_as(LAYERED_GRASS_FILL_PATH, "LayeredGrassFill")
        if grass_texture:
            materials["M_RuntimeCity_LayeredGrassTexture"] = create_textured_surface_material(
                grass_texture,
                "M_RuntimeCity_LayeredGrassTexture",
            )

    surface_sources = {
        "M_RuntimeCity_GrassSceneA": source_if_exists(LAND_SHARED_TEXTURE_ROOT, "build_SceneMap_A.png"),
        "M_RuntimeCity_GrassSceneE": source_if_exists(LAND_TEXTURE_ROOT, "build_SceneMap_E.png"),
        "M_RuntimeCity_RoadDirt": source_if_exists(LAND_SHARED_TEXTURE_ROOT, "dirtRoad01.png"),
        "M_RuntimeCity_GreenMask": source_if_exists(LAND_TEXTURE_ROOT, "ground_mask_green_02.png"),
    }
    for material_name, source_path in surface_sources.items():
        if not source_path:
            continue
        texture = import_runtime_texture(source_path)
        if texture:
            materials[material_name] = create_textured_surface_material(texture, material_name)
    return materials


def configure_component_for_runtime(actor, sort_priority=0, selectable=False):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        return
    for prop, value in [
        ("cast_shadow", False),
        ("receive_decals", False),
        ("use_as_occluder", False),
    ]:
        try:
            component.set_editor_property(prop, value)
        except Exception:
            pass
    try:
        component.set_editor_property("translucency_sort_priority", sort_priority)
    except Exception:
        pass
    if selectable:
        component.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
        component.set_collision_response_to_channel(
            unreal.CollisionChannel.ECC_VISIBILITY,
            unreal.CollisionResponseType.ECR_BLOCK,
        )
    else:
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)


def spawn_runtime_plane(label, primitives, location, width, height, material, folder, yaw=0.0, sort_priority=0, selectable=False):
    actor = proto.spawn_mesh(
        label,
        primitives["plane"],
        location,
        (width / 100.0, height / 100.0, 1.0),
        material,
        (0.0, yaw, 0.0),
        folder,
    )
    configure_component_for_runtime(actor, sort_priority, selectable)
    return actor


def spawn_ground(primitives, materials):
    base_material = materials.get("M_RuntimeCity_GrassSceneA", materials["M_Rok_Grass_Blockout"])
    green_material = materials.get("M_RuntimeCity_GreenMask", materials["M_Rok_Grass_Far_Blockout"])
    plaza_material = materials.get("M_RuntimeCity_GrassSceneE", materials["M_Rok_Plaza_Blockout"])

    spawn_runtime_plane("RuntimeCity_GrassBase", primitives, (0.0, 0.0, -8.0), 6200.0, 6200.0, base_material, "RokRuntimeCity/Terrain")
    spawn_runtime_plane("RuntimeCity_GrassNorthBlend", primitives, (0.0, 1050.0, -6.5), 3600.0, 1700.0, green_material, "RokRuntimeCity/Terrain", yaw=8.0)
    spawn_runtime_plane("RuntimeCity_GrassSouthBlend", primitives, (-600.0, -1320.0, -6.0), 3000.0, 1350.0, green_material, "RokRuntimeCity/Terrain", yaw=-12.0)
    spawn_runtime_plane("RuntimeCity_CentralPlaza", primitives, (0.0, 0.0, -4.0), 1250.0, 920.0, plaza_material, "RokRuntimeCity/Terrain", yaw=45.0)
    spawn_runtime_plane("RuntimeCity_MarketPlaza", primitives, (410.0, 1110.0, -3.0), 1080.0, 520.0, plaza_material, "RokRuntimeCity/Terrain", yaw=8.0)


def spawn_layered_ground(primitives, materials):
    base_material = materials.get("M_RuntimeCity_LayeredGrassTexture", materials["M_RuntimeCity_LayeredGrassBase"])
    spawn_runtime_plane(
        "RuntimeLayered_GrassBase",
        primitives,
        (0.0, 0.0, -10.0),
        6200.0,
        6200.0,
        base_material,
        "RokRuntimeCity/Terrain",
    )


def spawn_layered_wall_models(materials):
    material = materials.get("M_Rok_Stone_Blockout")
    spawned = []
    for label, asset_name, center_xy, bottom_z, scale, yaw in LAYERED_WALL_MODEL_LAYOUT:
        actor = proto.spawn_centered_asset(
            label,
            "/Game/RokPrototype/Meshes/City/{0}.{0}".format(asset_name),
            center_xy,
            bottom_z,
            scale,
            material,
            yaw,
            "RokRuntimeCity/Wall",
        )
        if not actor:
            continue
        try:
            actor.tags = ["RokLayeredScene", "RokWallAnchor"]
        except Exception:
            pass
        configure_component_for_runtime(actor, sort_priority=-9500, selectable=False)
        spawned.append(actor)
    unreal.log("Runtime layered wall model anchors spawned: {0}".format(len(spawned)))
    return spawned


def spawn_layered_wall_sprites():
    spawned = []
    skipped = []
    for label, color_name, mask_name, x, y, relative_scale, sort_offset in LAYERED_WALL_SPRITE_LAYOUT:
        cutout_path = source_if_exists(LAYERED_CUTOUT_ROOT, color_name)
        source_path = cutout_path or source_if_exists(EXPORTED_SPRITE_ROOT, color_name)
        if not source_path:
            skipped.append("{0}: missing sprite".format(label))
            continue
        texture = import_runtime_texture_as(source_path, "LayeredWall_{0}".format(os.path.splitext(color_name)[0]))
        if not texture:
            skipped.append("{0}: import failed".format(label))
            continue
        if cutout_path:
            mask_path = source_if_exists(LAYERED_CUTOUT_ROOT, "{0}_mask.png".format(os.path.splitext(color_name)[0]))
        else:
            mask_path = source_if_exists(EXPORTED_SPRITE_ROOT, mask_name) if mask_name else None
        material = create_runtime_city_sprite_material(
            texture,
            "LayeredWall_{0}".format(os.path.splitext(color_name)[0]),
            mask_path,
            True,
            False,
        )
        tex_width, tex_height = texture_dimensions(texture)
        actor = spawn_sprite_actor(
            label,
            material,
            float(x),
            float(y),
            tex_width * 0.60 * float(relative_scale),
            tex_height * 0.60 * float(relative_scale),
            layered_sprite_sort_priority(x, y, sort_offset),
            "RokRuntimeCity/Wall",
            ["RokLayeredScene", "RokWallAnchor"],
            False,
            anchor_mode="bottom",
        )
        spawned.append(actor)
    unreal.log("Runtime layered wall sprites spawned: {0}; skipped: {1}".format(len(spawned), "; ".join(skipped)))
    return spawned


def spawn_roads(primitives, materials):
    road_material = materials.get("M_RuntimeCity_RoadDirt", materials["M_Rok_Path_Blockout"])
    for label, x, y, width, height, yaw in ROAD_SPECS:
        actor = proto.spawn_mesh(
            label,
            primitives["cube"],
            (x, y, 1.5),
            (width / 100.0, height / 100.0, 0.018),
            road_material,
            (0.0, yaw, 0.0),
            "RokRuntimeCity/Roads",
        )
        configure_component_for_runtime(actor)


def spawn_city_wall(primitives, materials):
    wall_material = materials["M_Rok_WallCap_Blockout"]
    tower_material = materials["M_Rok_Stone_Blockout"]
    for label, x, y, width, height in [
        ("RuntimeWall_North", 0.0, 1700.0, 3450.0, 68.0),
        ("RuntimeWall_South", 0.0, -1700.0, 3450.0, 68.0),
        ("RuntimeWall_East", 1700.0, 0.0, 68.0, 3450.0),
        ("RuntimeWall_West", -1700.0, 0.0, 68.0, 3450.0),
    ]:
        actor = proto.spawn_mesh(
            label,
            primitives["cube"],
            (x, y, 25.0),
            (width / 100.0, height / 100.0, 0.36),
            wall_material,
            (0.0, 0.0, 0.0),
            "RokRuntimeCity/Wall",
        )
        configure_component_for_runtime(actor)

    for index, (x, y) in enumerate([(-1700, -1700), (-1700, 1700), (1700, -1700), (1700, 1700), (0, -1700), (0, 1700), (-1700, 0), (1700, 0)]):
        actor = proto.spawn_mesh(
            "RuntimeWall_Tower_{0:02d}".format(index),
            primitives["cube"],
            (float(x), float(y), 48.0),
            (1.5, 1.5, 0.74),
            tower_material,
            (0.0, 45.0, 0.0),
            "RokRuntimeCity/Wall",
        )
        configure_component_for_runtime(actor)


def create_building_material(texture, model_id, mask_path):
    if SPRITE_PLANE_MODE in ("billboard", "camera", "prefab"):
        return create_runtime_city_sprite_material(texture, model_id, mask_path, False, SPRITE_TRANSLUCENT_ALPHA)
    if mask_path:
        mask_texture = proto.import_city_sprite_texture(mask_path)
        if mask_texture:
            return proto.create_masked_city_sprite_material(texture, mask_texture)
    return proto.create_sprite_material(texture)


def create_runtime_city_sprite_material(
    color_texture,
    model_id,
    mask_path,
    disable_depth_test=False,
    translucent_alpha=False,
    mask_channel="R",
    opacity_clip=0.5,
):
    suffix = "_Billboard" if disable_depth_test else ("_SoftAlpha" if translucent_alpha else "")
    material_name = "M_RuntimeCitySprite_{0}{1}".format(proto.safe_asset_name(model_id), suffix)
    material_path = proto.asset_package_path(RUNTIME_MATERIAL_DESTINATION, material_name)
    existing = proto.load_asset_if_exists(material_path)
    if existing:
        if material_name in REBUILT_RUNTIME_SPRITE_MATERIALS:
            return existing
        unreal.EditorAssetLibrary.delete_asset(material_path)

    unreal.EditorAssetLibrary.make_directory(RUNTIME_MATERIAL_DESTINATION)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        material_name,
        RUNTIME_MATERIAL_DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    material.set_editor_property(
        "blend_mode",
        unreal.BlendMode.BLEND_TRANSLUCENT if (disable_depth_test or translucent_alpha) else unreal.BlendMode.BLEND_MASKED,
    )
    material.set_editor_property("two_sided", True)
    if disable_depth_test:
        try:
            material.set_editor_property("disable_depth_test", True)
        except Exception:
            pass
    elif not translucent_alpha:
        material.set_editor_property("opacity_mask_clip_value", opacity_clip)
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    color_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -560, -40
    )
    color_node.set_editor_property("texture", color_texture)
    unreal.MaterialEditingLibrary.connect_material_property(color_node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(color_node, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    if mask_path:
        mask_texture = import_runtime_texture_as(mask_path, "{0}_Mask".format(model_id), True)
    else:
        mask_texture = None
    if mask_texture:
        channel_name = mask_channel if mask_channel in ("R", "G", "B", "A") else "R"
        mask_node = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionTextureSample, -560, 180
        )
        mask_node.set_editor_property("texture", mask_texture)
        unreal.MaterialEditingLibrary.connect_material_property(
            mask_node,
            channel_name,
            unreal.MaterialProperty.MP_OPACITY if (disable_depth_test or translucent_alpha) else unreal.MaterialProperty.MP_OPACITY_MASK,
        )
    else:
        unreal.MaterialEditingLibrary.connect_material_property(
            color_node,
            "A",
            unreal.MaterialProperty.MP_OPACITY if (disable_depth_test or translucent_alpha) else unreal.MaterialProperty.MP_OPACITY_MASK,
        )

    roughness = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -560, 360)
    roughness.set_editor_property("r", 0.92)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    REBUILT_RUNTIME_SPRITE_MATERIALS.add(material_name)
    return material


def spawn_shadow_and_pad(label, primitives, materials, x, y, footprint_width, footprint_length, visual_width):
    shadow_width = max(155.0, visual_width * 0.58)
    shadow_height = max(72.0, footprint_length * 0.48)
    spawn_runtime_plane(
        "RuntimeShadow_{0}".format(label),
        primitives,
        (x + 18.0, y - 22.0, 6.0),
        shadow_width,
        shadow_height,
        materials["M_RuntimeCity_SoftShadow"],
        "RokRuntimeCity/Shadows",
        yaw=45.0,
        sort_priority=-100,
    )

def fixed_sprite_plane_rotation(x, y, z):
    if SPRITE_PLANE_MODE == "billboard":
        return proto.make_rotator(0.0, 0.0, 0.0)
    if SPRITE_PLANE_MODE != "camera":
        return proto.make_rotator(SPRITE_PLANE_PITCH, SPRITE_PLANE_YAW, SPRITE_PLANE_ROLL)

    target = CAMERA_LOCATION - unreal.Vector(x, y, z)
    try:
        return unreal.MathLibrary.make_rot_from_zy(target, unreal.Vector(0.0, 0.0, 1.0))
    except Exception:
        pass
    try:
        return unreal.KismetMathLibrary.make_rot_from_zy(target, unreal.Vector(0.0, 0.0, 1.0))
    except Exception:
        pass
    try:
        return unreal.MathLibrary.make_rot_from_z(target)
    except Exception:
        pass
    try:
        return unreal.KismetMathLibrary.make_rot_from_z(target)
    except Exception:
        pass
    return proto.make_rotator(SPRITE_PLANE_PITCH, SPRITE_PLANE_YAW, SPRITE_PLANE_ROLL)


def prefab_sprite_component_rotation():
    x_axis = unreal.Vector(1.0, 0.0, 0.0)
    y_axis = unreal.Vector(0.0, 0.70710678, 0.70710678)
    try:
        return unreal.MathLibrary.make_rot_from_xy(x_axis, y_axis)
    except Exception:
        pass
    try:
        return unreal.KismetMathLibrary.make_rot_from_xy(x_axis, y_axis)
    except Exception:
        pass
    return proto.make_rotator(45.0, 0.0, 0.0)


def prefab_sprite_component_offset():
    return unreal.Vector(
        UNITY_PREFAB_SPRITE_OFFSET[0] * UE_UNITS_PER_UNITY_UNIT,
        UNITY_PREFAB_SPRITE_OFFSET[2] * UE_UNITS_PER_UNITY_UNIT,
        UNITY_PREFAB_SPRITE_OFFSET[1] * UE_UNITS_PER_UNITY_UNIT,
    )


def apply_prefab_sprite_transform(actor):
    if SPRITE_PLANE_MODE != "prefab":
        return
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        return
    component.set_editor_property("relative_rotation", prefab_sprite_component_rotation())
    component.set_editor_property("relative_location", prefab_sprite_component_offset())


def apply_camera_sprite_anchor(actor, height, anchor_mode):
    if SPRITE_PLANE_MODE != "camera":
        return
    if anchor_mode != "bottom":
        return
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        return
    component.set_editor_property("relative_location", unreal.Vector(0.0, height * 0.5, 0.0))


def prefab_visual_size(texture_width, texture_height):
    scale = UE_UNITS_PER_UNITY_UNIT * UNITY_PREFAB_SPRITE_SCALE / max(1.0, UNITY_PREFAB_SPRITE_PPU)
    return texture_width * scale, texture_height * scale


def scaled_vector(vector, scale):
    return unreal.Vector(float(vector.x) * scale, float(vector.y) * scale, float(vector.z) * scale)


def vector_delta(a, b):
    return unreal.Vector(float(a.x) - float(b.x), float(a.y) - float(b.y), float(a.z) - float(b.z))


def billboard_camera_up_vector():
    rotation = unreal.MathLibrary.find_look_at_rotation(CAMERA_LOCATION, CAMERA_TARGET)
    try:
        return unreal.MathLibrary.get_up_vector(rotation)
    except Exception:
        return unreal.Vector(0.0, 0.0, 1.0)


def sprite_actor_location(x, y, height, z_override, anchor_mode):
    if SPRITE_PLANE_MODE == "billboard" and anchor_mode == "bottom":
        anchor_location = unreal.Vector(float(x), float(y), float(z_override) if z_override is not None else 10.0)
        center_location = anchor_location + scaled_vector(billboard_camera_up_vector(), float(height) * 0.5)
        return center_location, vector_delta(anchor_location, center_location)

    z = z_override if z_override is not None else (max(48.0, height * 0.5 + 12.0) if SPRITE_PLANE_MODE == "billboard" else 8.0)
    center_location = unreal.Vector(float(x), float(y), float(z))
    return center_location, unreal.Vector(0.0, 0.0, -float(z))


def spawn_sprite_actor(label, material, x, y, width, height, sort_priority, folder, tags, selectable, footprint=(1, 1), tile=(0, 0), z_override=None, visible=True, anchor_mode="bottom"):
    sprite_actor_class = unreal.load_class(None, CITY_SPRITE_ACTOR_CLASS)
    if not sprite_actor_class:
        raise RuntimeError("Missing runtime sprite actor class: {0}".format(CITY_SPRITE_ACTOR_CLASS))

    location, visual_anchor_offset = sprite_actor_location(x, y, height, z_override, anchor_mode)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        sprite_actor_class,
        location,
        fixed_sprite_plane_rotation(location.x, location.y, location.z),
    )
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.configure_sprite(material, width, height, sort_priority)
    try:
        actor.set_actor_hidden_in_game(not visible)
    except Exception:
        pass
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if component:
        try:
            component.set_visibility(visible)
        except Exception:
            pass
    legacy_billboard = actor.get_component_by_class(unreal.MaterialBillboardComponent)
    if legacy_billboard:
        try:
            legacy_billboard.set_visibility(False)
        except Exception:
            pass
    apply_prefab_sprite_transform(actor)
    apply_camera_sprite_anchor(actor, height, anchor_mode)
    try:
        actor.configure_placement(
            unreal.IntPoint(int(footprint[0]), int(footprint[1])),
            unreal.IntPoint(int(tile[0]), int(tile[1])),
            visual_anchor_offset,
        )
    except Exception:
        pass
    try:
        actor.set_selection_collision_enabled(selectable)
    except Exception:
        pass
    actor.tags = list(set(list(actor.tags) + tags))
    return actor


def spawn_buildings(primitives, materials, building_models, building_types):
    spawned = []
    skipped = []
    for index, (building_type, tile_x, tile_y) in enumerate(CITY_BUILDING_LAYOUT):
        model_id = building_models.get(building_type)
        if not model_id:
            skipped.append("type {0}: no model".format(building_type))
            continue
        source_path = building_texture_path(model_id)
        mask_path = building_mask_path(model_id)
        if not source_path:
            source_path = fallback_building_texture_path(building_type)
            mask_path = fallback_building_mask_path(building_type)
        if not source_path:
            skipped.append("{0}: no texture".format(model_id))
            continue

        texture = proto.import_city_sprite_texture(source_path)
        if not texture:
            skipped.append("{0}: import failed".format(model_id))
            continue
        material = create_building_material(texture, model_id, mask_path)
        tex_width, tex_height = texture_dimensions(texture)
        footprint = building_types.get(building_type, {"width": 5, "length": 5})
        footprint_width = max(1, footprint["width"]) * UNITY_GRID_SIZE * UE_UNITS_PER_UNITY_UNIT
        footprint_length = max(1, footprint["length"]) * UNITY_GRID_SIZE * UE_UNITS_PER_UNITY_UNIT
        if SPRITE_PLANE_MODE in ("billboard", "camera", "prefab"):
            visual_width, visual_height = prefab_visual_size(tex_width, tex_height)
        else:
            visual_width = max(210.0, max(footprint_width, footprint_length) * 1.22)
            if building_type == 1:
                visual_width *= 1.28
            elif building_type in (2, 3, 4, 5, 6, 7, 8):
                visual_width *= 0.88
            visual_height = visual_width * float(tex_height) / float(max(1, tex_width))
        x, y = unity_tile_to_ue(building_type, tile_x, tile_y, building_types)
        sort_priority = int((y + 4200.0) * 10.0) + index
        label = "RokRuntime_{0}_{1}_{2:02d}".format(BUILDING_DISPLAY_NAMES.get(building_type, "Building"), model_id, index)

        spawn_shadow_and_pad(label, primitives, materials, x, y, footprint_width, footprint_length, visual_width)
        actor = spawn_sprite_actor(
            label,
            material,
            x,
            y,
            visual_width,
            visual_height,
            sort_priority,
            "RokRuntimeCity/Buildings",
            ["RokBuilding", "CityBuilding"],
            True,
            (footprint["width"], footprint["length"]),
            (tile_x, tile_y),
        )
        spawned.append(actor)

    unreal.log("Runtime city buildings spawned: {0}; skipped: {1}".format(len(spawned), "; ".join(skipped)))
    return spawned


def import_sprite_pair(root, color_name, mask_name=None):
    color_path = source_if_exists(root, color_name)
    if not color_path:
        return None
    color_texture = import_runtime_texture(color_path)
    if not color_texture:
        return None
    mask_texture = None
    if mask_name:
        mask_path = source_if_exists(root, mask_name)
        if mask_path:
            mask_texture = import_runtime_texture(mask_path)
    if mask_texture:
        return proto.create_masked_city_sprite_material(color_texture, mask_texture)
    return proto.create_sprite_material(color_texture)


def spawn_resource_and_decor(primitives, materials):
    spawned = []
    for index, (name, color_name, mask_name, (x, y), width, height) in enumerate(RESOURCE_DECOR_SPECS):
        material = import_sprite_pair(WILD_BUILDING_ROOT, color_name, mask_name)
        if not material:
            continue
        spawn_runtime_plane(
            "RuntimeShadow_{0}_{1:02d}".format(name, index),
            primitives,
            (float(x) + 18.0, float(y) - 24.0, 6.5),
            width * 0.72,
            height * 0.34,
            materials["M_RuntimeCity_SoftShadow"],
            "RokRuntimeCity/Shadows",
            yaw=45.0,
        )
        actor = spawn_sprite_actor(
            "RokDecor_{0}_{1:02d}".format(name, index),
            material,
            float(x),
            float(y),
            float(width),
            float(height),
            int((y + 4200) * 10) + index,
            "RokRuntimeCity/Resources",
            ["RokDecoration"],
            False,
        )
        spawned.append(actor)

    for index, (name, color_name, (x, y), width, height) in enumerate(EXPORT_DECOR_SPECS):
        material = import_sprite_pair(EXPORTED_SPRITE_ROOT, color_name)
        if not material:
            continue
        actor = spawn_sprite_actor(
            "RokDecor_{0}_{1:02d}".format(name, index),
            material,
            float(x),
            float(y),
            float(width),
            float(height),
            int((y + 4200) * 10) + 500 + index,
            "RokRuntimeCity/Decor",
            ["RokDecoration"],
            False,
        )
        spawned.append(actor)

    # Small flat accents make the city floor read less like a single empty plane.
    accent_material = materials.get("M_RuntimeCity_GreenMask", materials["M_Rok_Grass_Dark_Blockout"])
    for index, (x, y, width, height, yaw) in enumerate([
        (-1520, -580, 420, 210, -20), (-1240, 520, 360, 180, 15), (1480, -610, 420, 210, 18),
        (1420, 910, 380, 190, -8), (0, -1380, 580, 220, 0), (0, 1440, 580, 220, 0),
        (-760, -760, 260, 120, 35), (760, 760, 260, 120, 35), (-760, 760, 260, 120, -35),
        (760, -760, 260, 120, -35),
    ]):
        actor = spawn_runtime_plane(
            "RuntimeDecor_GrassAccent_{0:02d}".format(index),
            primitives,
            (float(x), float(y), 0.5),
            float(width),
            float(height),
            accent_material,
            "RokRuntimeCity/Decor",
            yaw=float(yaw),
        )
        spawned.append(actor)

    unreal.log("Runtime city resource/decor actors spawned: {0}".format(len(spawned)))
    return spawned


def import_runtime_sprite_material(sprite_name, material_key=None):
    source_path = source_if_exists(EXPORTED_SPRITE_ROOT, sprite_name)
    if not source_path:
        return None, None
    texture = import_runtime_texture_as(source_path, "Runtime_{0}".format(os.path.splitext(sprite_name)[0]))
    if not texture:
        return None, None
    material = create_runtime_city_sprite_material(
        texture,
        material_key or os.path.splitext(sprite_name)[0],
        None,
        False,
        False,
    )
    return material, texture


def spawn_target_terrain_layers():
    spawned = []
    for index, (sprite_name, x, y, scale, sort_priority) in enumerate(TARGET_TERRAIN_LAYERS):
        material = import_sprite_pair(EXPORTED_SPRITE_ROOT, "{0}.png".format(sprite_name), "{0}_mask.png".format(sprite_name))
        if not material:
            continue
        texture = proto.import_city_sprite_texture(os.path.join(EXPORTED_SPRITE_ROOT, "{0}.png".format(sprite_name)))
        tex_width, tex_height = texture_dimensions(texture) if texture else (420, 300)
        actor = spawn_sprite_actor(
            "RokRuntimeFloor_{0}_{1:02d}".format(sprite_name, index),
            material,
            float(x),
            float(y),
            tex_width * 0.58 * scale,
            tex_height * 0.58 * scale,
            sort_priority,
            "RokRuntimeCity/TerrainSprites",
            ["RokTerrainLayer"],
            False,
            z_override=-520.0,
            anchor_mode="center",
        )
        spawned.append(actor)
    return spawned


def spawn_layered_terrain_layers():
    ground_plate_actor = spawn_layered_ground_plate()
    if ground_plate_actor:
        return [ground_plate_actor]

    spawned = []
    for index, (sprite_name, x, y, scale, sort_priority) in enumerate(LAYERED_TERRAIN_LAYERS):
        material = import_sprite_pair(EXPORTED_SPRITE_ROOT, "{0}.png".format(sprite_name), "{0}_mask.png".format(sprite_name))
        if not material:
            continue
        texture = proto.import_city_sprite_texture(os.path.join(EXPORTED_SPRITE_ROOT, "{0}.png".format(sprite_name)))
        tex_width, tex_height = texture_dimensions(texture) if texture else (420, 300)
        actor = spawn_sprite_actor(
            "RokLayeredFloor_{0}_{1:02d}".format(sprite_name, index),
            material,
            float(x),
            float(y),
            tex_width * 0.58 * scale,
            tex_height * 0.58 * scale,
            sort_priority,
            "RokRuntimeCity/TerrainSprites",
            ["RokTerrainLayer", "RokLayeredScene"],
            False,
            z_override=-520.0,
            anchor_mode="center",
        )
        spawned.append(actor)
    unreal.log("Runtime layered terrain sprites spawned: {0}".format(len(spawned)))
    return spawned


def spawn_layered_ground_plate():
    if not os.path.exists(LAYERED_GROUND_PLATE_PATH):
        return None
    texture = import_runtime_texture_as(LAYERED_GROUND_PLATE_PATH, "LayeredGroundPlate")
    if not texture:
        return None
    mask_path = LAYERED_GROUND_PLATE_MASK_PATH if os.path.exists(LAYERED_GROUND_PLATE_MASK_PATH) else None
    material = create_runtime_city_sprite_material(texture, "LayeredGroundPlate", mask_path, False, True)
    return spawn_sprite_actor(
        "RokLayeredGroundPlate",
        material,
        city2d.GROUND_PLATE_CENTER[0],
        city2d.GROUND_PLATE_CENTER[1],
        city2d.GROUND_PLATE_WORLD_SIZE[0],
        city2d.GROUND_PLATE_WORLD_SIZE[1],
        -9400,
        "RokRuntimeCity/TerrainSprites",
        ["RokTerrainLayer", "RokLayeredScene", "RokLayeredGroundPlate"],
        False,
        z_override=2.0,
        anchor_mode="center",
    )


def layered_sprite_sort_priority(x, y, offset=0):
    return city2d.layered_sort_priority(x, y, offset)


def spawn_target_named_buildings(building_types, visible=True):
    spawned = []
    skipped = []
    for index, (building_type, display_name, x, y, relative_scale) in enumerate(TARGET_CITY_BUILDING_LAYOUT):
        sprite_name, source_path = layered_building_texture_path(building_type)
        if not source_path:
            skipped.append("{0}: missing named sprite".format(display_name))
            continue
        texture = import_runtime_texture_as(source_path, "StructureCutout_{0}".format(sprite_name))
        if not texture:
            skipped.append("{0}: import failed".format(sprite_name))
            continue
        material = create_runtime_city_sprite_material(texture, "StructureCutout_{0}".format(sprite_name), None, False, False)
        tex_width, tex_height = texture_dimensions(texture)
        visual_width = tex_width * 0.60 * relative_scale
        visual_height = tex_height * 0.60 * relative_scale
        footprint = building_types.get(building_type, {"width": 5, "length": 5})
        actor = spawn_sprite_actor(
            "RokRuntimeTarget_{0}_{1}".format(display_name, sprite_name),
            material,
            float(x),
            float(y),
            visual_width,
            visual_height,
            layered_sprite_sort_priority(x, y, index),
            "RokRuntimeCity/Buildings",
            ["RokBuilding", "CityBuilding", "RokTargetScene"],
            True,
            (footprint["width"], footprint["length"]),
            (0, 0),
            visible=visible,
        )
        spawned.append(actor)
    unreal.log("Runtime target named buildings spawned: {0}; skipped: {1}".format(len(spawned), "; ".join(skipped)))
    return spawned


def spawn_layered_buildings(primitives, materials, building_types):
    spawned = []
    skipped = []
    for index, (building_type, display_name, x, y, relative_scale) in enumerate(LAYERED_CITY_BUILDING_LAYOUT):
        sprite_name, source_path = layered_building_texture_path(building_type)
        if not source_path:
            skipped.append("{0}: missing named sprite".format(display_name))
            continue
        texture = import_runtime_texture_as(source_path, "LayeredStructure_{0}".format(sprite_name))
        if not texture:
            skipped.append("{0}: import failed".format(sprite_name))
            continue
        material = create_runtime_city_sprite_material(texture, "LayeredStructure_{0}".format(sprite_name), None, False, False)
        tex_width, tex_height = texture_dimensions(texture)
        visual_width = tex_width * 0.60 * relative_scale
        visual_height = tex_height * 0.60 * relative_scale
        footprint = building_types.get(building_type, {"width": 5, "length": 5})
        footprint_width = max(190.0, footprint["width"] * UNITY_GRID_SIZE * UE_UNITS_PER_UNITY_UNIT)
        footprint_length = max(145.0, footprint["length"] * UNITY_GRID_SIZE * UE_UNITS_PER_UNITY_UNIT)
        actor = spawn_sprite_actor(
            "RokLayered_{0}_{1}".format(display_name, sprite_name),
            material,
            float(x),
            float(y),
            visual_width,
            visual_height,
            layered_sprite_sort_priority(x, y, index),
            "RokRuntimeCity/Buildings",
            ["RokBuilding", "CityBuilding", "RokLayeredScene"],
            True,
            (footprint["width"], footprint["length"]),
            (0, 0),
        )
        spawned.append(actor)
    unreal.log("Runtime layered buildings spawned: {0}; skipped: {1}".format(len(spawned), "; ".join(skipped)))
    return spawned


def spawn_layered_decor():
    spawned = []
    for index, (sprite_name, mask_name, x, y, scale) in enumerate(LAYERED_DECOR_LAYOUT):
        source_path = source_if_exists(EXPORTED_SPRITE_ROOT, sprite_name)
        if not source_path:
            continue
        texture = import_runtime_texture_as(source_path, "LayeredDecor_{0}".format(os.path.splitext(sprite_name)[0]))
        if not texture:
            continue
        mask_path = source_if_exists(EXPORTED_SPRITE_ROOT, mask_name) if mask_name else None
        material = create_runtime_city_sprite_material(
            texture,
            "LayeredDecor_{0}".format(os.path.splitext(sprite_name)[0]),
            mask_path,
            True,
            False,
        )
        tex_width, tex_height = texture_dimensions(texture)
        actor = spawn_sprite_actor(
            "RokLayeredDecor_{0}_{1:02d}".format(os.path.splitext(sprite_name)[0], index),
            material,
            float(x),
            float(y),
            tex_width * 0.54 * scale,
            tex_height * 0.54 * scale,
            layered_sprite_sort_priority(x, y, 2200 + index),
            "RokRuntimeCity/Decor",
            ["RokDecoration", "RokLayeredScene"],
            False,
        )
        spawned.append(actor)
    unreal.log("Runtime layered decor spawned: {0}".format(len(spawned)))
    return spawned


def spawn_target_marker(label, x, y, icon_name, sort_priority):
    bubble_material, bubble_texture = import_runtime_sprite_material("popup_common_l.png", "RuntimePopupBubble")
    icon_material, icon_texture = import_runtime_sprite_material(icon_name, "RuntimeMarker_{0}".format(os.path.splitext(icon_name)[0]))
    spawned = []
    if bubble_material and bubble_texture:
        bubble_width, bubble_height = texture_dimensions(bubble_texture)
        spawned.append(spawn_sprite_actor(
            "RokMarkerBubble_{0}".format(label),
            bubble_material,
            float(x),
            float(y) + 85.0,
            min(150.0, bubble_width * 0.82),
            min(130.0, bubble_height * 0.82),
            sort_priority + 1,
            "RokRuntimeCity/SceneMarkers",
            ["RokSceneMarker"],
            False,
            z_override=980.0,
            anchor_mode="center",
        ))
    if icon_material and icon_texture:
        icon_width, icon_height = texture_dimensions(icon_texture)
        spawned.append(spawn_sprite_actor(
            "RokMarkerIcon_{0}".format(label),
            icon_material,
            float(x),
            float(y) + 100.0,
            min(72.0, icon_width * 0.80),
            min(72.0, icon_height * 0.80),
            sort_priority + 2,
            "RokRuntimeCity/SceneMarkers",
            ["RokSceneMarker"],
            False,
            z_override=990.0,
            anchor_mode="center",
        ))
    return spawned


def spawn_target_upgrade_marker(label, x, y, sort_priority):
    material, texture = import_runtime_sprite_material("upgrade_hint_board.png", "RuntimeUpgradeHintBoard")
    if not material or not texture:
        return None
    tex_width, tex_height = texture_dimensions(texture)
    return spawn_sprite_actor(
        "RokMarkerUpgrade_{0}".format(label),
        material,
        float(x),
        float(y),
        min(84.0, tex_width * 0.72),
        min(92.0, tex_height * 0.72),
        sort_priority,
        "RokRuntimeCity/SceneMarkers",
        ["RokSceneMarker", "RokUpgradeMarker"],
        False,
        z_override=900.0,
        anchor_mode="center",
    )


def spawn_target_resources_and_markers(resources_visible=True):
    if not resources_visible:
        unreal.log("Runtime target resources/markers skipped because the full city composite is active.")
        return []
    spawned = []
    marker_count = 0
    for index, (name, color_name, mask_name, x, y, width, height, marker_key) in enumerate(TARGET_RESOURCE_LAYOUT):
        material = import_sprite_pair(WILD_BUILDING_ROOT, color_name, mask_name)
        if not material:
            continue
        actor = spawn_sprite_actor(
            "RokTargetResource_{0}_{1:02d}".format(name, index),
            material,
            float(x),
            float(y),
            float(width),
            float(height),
            int((float(y) + 5000.0) * 10.0) + 700 + index,
            "RokRuntimeCity/Resources",
            ["RokDecoration", "RokTargetScene"],
            False,
            visible=resources_visible,
        )
        spawned.append(actor)
        icon_name = RESOURCE_MARKER_ICONS.get(marker_key)
        if icon_name:
            spawned.extend(spawn_target_marker("{0}_{1:02d}".format(name, index), x, y + height * 0.33, icon_name, 88000 + index * 10))
            marker_count += 1
        if index % 2 == 0:
            marker = spawn_target_upgrade_marker("{0}_{1:02d}".format(name, index), x + width * 0.48, y - height * 0.30, 83000 + index)
            if marker:
                spawned.append(marker)
    unreal.log("Runtime target resources/markers spawned: {0}; marker groups={1}".format(len(spawned), marker_count))
    return spawned


def spawn_target_wall_anchors(primitives, materials):
    wall_material = materials["M_Rok_WallCap_Blockout"]
    tower_material = materials["M_Rok_Stone_Blockout"]
    specs = [
        ("TargetWall_SouthLeft", -1260, -1515, 980, 110, 35),
        ("TargetWall_SouthRight", 1140, -1525, 1380, 120, 35),
        ("TargetWall_EastGate", 1510, -980, 130, 780, 0),
        ("TargetWall_WestGate", -1510, -760, 125, 760, 0),
    ]
    for label, x, y, width, height, yaw in specs:
        actor = proto.spawn_mesh(
            label,
            primitives["cube"],
            (float(x), float(y), 42.0),
            (float(width) / 100.0, float(height) / 100.0, 0.44),
            wall_material,
            (0.0, float(yaw), 0.0),
            "RokRuntimeCity/Wall",
        )
        configure_component_for_runtime(actor)
    for index, (x, y) in enumerate([(-1540, -1220), (-1040, -1560), (910, -1580), (1520, -1120)]):
        actor = proto.spawn_mesh(
            "TargetWall_Tower_{0:02d}".format(index),
            primitives["cube"],
            (float(x), float(y), 82.0),
            (1.55, 1.55, 0.92),
            tower_material,
            (0.0, 45.0, 0.0),
            "RokRuntimeCity/Wall",
        )
        configure_component_for_runtime(actor)


def spawn_camera(camera_record):
    rotation = unreal.MathLibrary.find_look_at_rotation(CAMERA_LOCATION, CAMERA_TARGET)
    camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.CameraActor, CAMERA_LOCATION, rotation)
    camera.set_actor_label("Camera_RokRuntimeCity")
    camera.set_folder_path("RokRuntimeCity/Camera")
    component = camera.get_component_by_class(unreal.CameraComponent)
    try:
        component.set_editor_property("projection_mode", unreal.CameraProjectionMode.ORTHOGRAPHIC)
    except Exception:
        component.set_editor_property("projection_mode", 1)
    component.set_editor_property("ortho_width", CAMERA_ORTHO_WIDTH)
    if camera_record:
        unreal.log("Runtime city camera source: city dist={0}, fov={1}, forward=({2},{3},{4})".format(
            camera_record.get("dist"),
            camera_record.get("fov"),
            camera_record.get("forwardX"),
            camera_record.get("forwardY"),
            camera_record.get("forwardZ"),
        ))


def configure_world_settings():
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_settings = world.get_world_settings()
    game_mode_class = unreal.load_class(None, GAME_MODE_CLASS)
    if game_mode_class:
        world_settings.set_editor_property("default_game_mode", game_mode_class)


def build_runtime_city():
    if TARGET_SCENE_PASS:
        if unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
            if not unreal.EditorAssetLibrary.delete_asset(TARGET_LEVEL):
                raise RuntimeError("Failed to delete stale runtime city map before rebuild: {0}".format(TARGET_LEVEL))
        if not unreal.EditorLevelLibrary.new_level(TARGET_LEVEL):
            raise RuntimeError("Failed to create fresh runtime city map: {0}".format(TARGET_LEVEL))
    elif unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    else:
        if not unreal.EditorLevelLibrary.new_level(TARGET_LEVEL):
            unreal.log("Target map does not exist yet; building from the currently loaded template world.")

    if not TARGET_SCENE_PASS:
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            unreal.EditorLevelLibrary.destroy_actor(actor)

    proto.ensure_project_primitive_meshes()
    primitives = {name: proto.load_primitive(name) for name in proto.PRIMITIVE_PATHS}
    materials = make_runtime_materials()
    building_models = load_building_model_map()
    building_types = load_building_type_map()
    city_age = load_city_age_record()
    camera_record = load_camera_record()

    proto.setup_lighting()
    if TARGET_SCENE_PASS:
        spawn_layered_ground(primitives, materials)
        spawn_layered_wall_sprites()
        spawn_layered_terrain_layers()
        spawn_layered_decor()
        spawn_layered_buildings(primitives, materials, building_types)
    else:
        spawn_ground(primitives, materials)
        spawn_roads(primitives, materials)
        spawn_city_wall(primitives, materials)
        spawn_buildings(primitives, materials, building_models, building_types)
        spawn_resource_and_decor(primitives, materials)
    spawn_camera(camera_record)
    configure_world_settings()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError("Failed to save runtime city map: {0}".format(TARGET_LEVEL))
    unreal.EditorAssetLibrary.save_asset(TARGET_LEVEL, only_if_is_dirty=False)
    if city_age:
        unreal.log("Runtime city map saved: {0}; age={1}; city mesh hint={2}".format(
            TARGET_LEVEL,
            city_age.get("age"),
            city_age.get("mapMesh"),
        ))


if __name__ == "__main__":
    build_runtime_city()
