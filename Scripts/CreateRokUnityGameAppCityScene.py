import math
import os
import sys
import json

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokPrototypeScene as proto
import CreateRokRuntimeCityScene as runtime


TARGET_LEVEL = os.environ.get("ROK_UNITY_GAMEAPP_CITY_LEVEL", "/Game/Maps/L_RokUnityGameAppCity")
UNITY_PROJECT_ROOT = os.environ.get(
    "ROK_UNITY_GAMEAPP_ROOT",
    os.path.join(PROJECT_ROOT, "ref", "client-unity"),
)
UNITY_ASSETS_ROOT = os.path.join(UNITY_PROJECT_ROOT, "Assets")
UNITY_BUILDING_ROOT = os.path.join(
    UNITY_ASSETS_ROOT,
    "BundleAssets",
    "Map",
    "Building",
    "Building_use",
    "shared_building",
    "Building_101",
    "city_building_1",
)
UNITY_TERRAIN_ROOT = os.path.join(UNITY_ASSETS_ROOT, "BundleAssets", "Map", "Textures", "Terrian_Maps")
UNITY_LAND_TEXTURE_ROOT = os.path.join(UNITY_ASSETS_ROOT, "BundleAssets", "land", "Texture")
CLEAN_LAYER_ROOT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityPlacement", "clean")
PALM_CROP_ROOT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityGameAppCity", "Trees")
CONFIG_TABLE_ROOT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "ConfigTables")
UNITY_CITY_BOUNDARY_MANIFEST = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_city_boundary.json")
UNITY_CITY_VISUAL_LAYER_STACK_MANIFEST = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_city_visual_layer_stack.json")
UNITY_BUILDING_VISUAL_LAYERS_MANIFEST = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_building_visual_layers.json")
UNITY_BUILDING_BODY_GROUND_LAYERS_MANIFEST = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_building_body_ground_layers.json")
SHOW_INACTIVE_CITY_BOUNDARY_PREVIEW = os.environ.get("ROK_UNITY_SHOW_INACTIVE_CITY_BOUNDARY_PREVIEW", "0") == "1"
SHOW_CITY_BOUNDARY_SOURCE_PREVIEW = os.environ.get("ROK_UNITY_SHOW_CITY_BOUNDARY_SOURCE_PREVIEW", "0") == "1"
SHOW_CITY_VISUAL_LAYER_STACK_PREVIEW = os.environ.get("ROK_UNITY_SHOW_CITY_VISUAL_LAYER_STACK_PREVIEW", "0") == "1"
SHOW_BUILDING_EXPERIMENTAL_LAYERS = os.environ.get("ROK_UNITY_SHOW_BUILDING_EXPERIMENTAL_LAYERS", "0") == "1"
BUILDING_VISUAL_LAYERS_CACHE = None
BUILDING_BODY_GROUND_LAYERS_CACHE = None

CAMERA_ASPECT = 16.0 / 10.0
UE_UNITS_PER_UNITY_UNIT = 500.0
UNITY_CITY_CAMERA_VIEW_CENTER = (0.0, 0.0)
UNITY_CITY_CAMERA_VIEW_DISTANCE = 20.0
UNITY_CITY_CAMERA_VERTICAL_FOV = 7.5
UNITY_CITY_CAMERA_FORWARD = (0.0, -0.70710678, 0.70710678)


def normalized_vector(vector):
    length = math.sqrt(float(vector.x) * float(vector.x) + float(vector.y) * float(vector.y) + float(vector.z) * float(vector.z))
    if length <= 0.0001:
        return unreal.Vector(0.0, 0.70710678, -0.70710678)
    return unreal.Vector(float(vector.x) / length, float(vector.y) / length, float(vector.z) / length)


CAMERA_TARGET = unreal.Vector(
    -UNITY_CITY_CAMERA_VIEW_CENTER[0] * UE_UNITS_PER_UNITY_UNIT,
    UNITY_CITY_CAMERA_VIEW_CENTER[1] * UE_UNITS_PER_UNITY_UNIT,
    0.0,
)
CAMERA_LOCATION = CAMERA_TARGET - normalized_vector(unreal.Vector(
    UNITY_CITY_CAMERA_FORWARD[0],
    UNITY_CITY_CAMERA_FORWARD[2],
    UNITY_CITY_CAMERA_FORWARD[1],
)) * (UNITY_CITY_CAMERA_VIEW_DISTANCE * UE_UNITS_PER_UNITY_UNIT)
UNITY_INIT_CITY_CIV_TYPE = int(os.environ.get("ROK_UNITY_INIT_CITY_CIV_TYPE", "101"))
UNITY_INIT_CITY_AGE = int(os.environ.get("ROK_UNITY_INIT_CITY_AGE", "1"))
UNITY_INIT_GRID_X = float(os.environ.get("ROK_UNITY_INIT_GRID_X", "95.0"))
UNITY_INIT_GRID_Y = float(os.environ.get("ROK_UNITY_INIT_GRID_Y", "72.0"))
INIT_BUILDING_RESOLUTION_REPORT = os.path.join(
    PROJECT_ROOT,
    "Saved",
    "RokUnityMigration",
    "unity_gameapp_city_init_building_resolution.json",
)
DECORATION_SPRITE_FALLBACKS = {
    # Type 51 has real Unity sprite variants under city_building_1 and is
    # resolved by the variant path in resolve_building_sprite_name().
    # Do not map type 52/53 to type 51 visuals: that creates fake clutter in
    # the accepted city map. Missing decoration resources must stay missing
    # until their real Unity source path is extracted.
}


BUILDING_LAYOUT = [
    ("TownCenter", "build_1_101_1", 0.0, -185.0, 610.0, 0),
    ("CityHall", "build_9_101_1", 455.0, -400.0, 270.0, 30),
    ("GateHouse", "build_6_101_1", -470.0, 250.0, 300.0, -20),
    ("HouseNorth", "build_3_101_1", 360.0, 215.0, 240.0, 15),
    ("HouseSouth", "build_17_101_1", 255.0, -780.0, 235.0, 25),
    ("Market", "build_18_101_1", -360.0, -720.0, 245.0, 20),
    ("Supply", "build_11_101_1", 690.0, -120.0, 230.0, 50),
    ("Workshop", "build_14_101_1", -700.0, -170.0, 225.0, 60),
]

ROAD_LAYOUT = []

TREE_LAYOUT = [
    ("PalmPatch_01", -870.0, 650.0, 230.0, -20),
    ("PalmPatch_05", -500.0, 700.0, 150.0, -10),
    ("PalmPatch_06", 40.0, 720.0, 185.0, 0),
    ("PalmPatch_07", 785.0, 625.0, 185.0, 10),
    ("PalmPatch_05", -990.0, -95.0, 155.0, 20),
    ("PalmPatch_01", 950.0, -235.0, 210.0, 30),
    ("PalmPatch_06", -760.0, -780.0, 165.0, 40),
    ("PalmPatch_07", 760.0, -825.0, 175.0, 50),
]

WALL_SPRITE_LAYOUT = []

CITY_WALL_2D_LAYOUT = [
    ("NorthShadow", 0.0, 1095.0, -2.0, 3260.0, 110.0, "shadow"),
    ("SouthShadow", 0.0, -1215.0, -2.0, 3260.0, 110.0, "shadow"),
    ("WestShadow", -1605.0, -60.0, -2.0, 110.0, 2360.0, "shadow"),
    ("EastShadow", 1605.0, -60.0, -2.0, 110.0, 2360.0, "shadow"),
    ("NorthWall", 0.0, 1038.0, 6.0, 3180.0, 90.0, "wall"),
    ("SouthWallLeft", -835.0, -1168.0, 6.0, 1450.0, 90.0, "wall"),
    ("SouthWallRight", 835.0, -1168.0, 6.0, 1450.0, 90.0, "wall"),
    ("WestWall", -1548.0, -60.0, 6.0, 90.0, 2220.0, "wall"),
    ("EastWall", 1548.0, -60.0, 6.0, 90.0, 2220.0, "wall"),
    ("NorthInnerEdge", 0.0, 982.0, 8.0, 3180.0, 18.0, "edge"),
    ("SouthInnerEdgeLeft", -835.0, -1112.0, 8.0, 1450.0, 18.0, "edge"),
    ("SouthInnerEdgeRight", 835.0, -1112.0, 8.0, 1450.0, 18.0, "edge"),
    ("WestInnerEdge", -1492.0, -60.0, 8.0, 18.0, 2220.0, "edge"),
    ("EastInnerEdge", 1492.0, -60.0, 8.0, 18.0, 2220.0, "edge"),
    ("CornerNW", -1548.0, 1038.0, 10.0, 180.0, 180.0, "tower"),
    ("CornerNE", 1548.0, 1038.0, 10.0, 180.0, 180.0, "tower"),
    ("CornerSW", -1548.0, -1168.0, 10.0, 180.0, 180.0, "tower"),
    ("CornerSE", 1548.0, -1168.0, 10.0, 180.0, 180.0, "tower"),
    ("CornerNW_Cap", -1548.0, 1038.0, 12.0, 112.0, 112.0, "edge"),
    ("CornerNE_Cap", 1548.0, 1038.0, 12.0, 112.0, 112.0, "edge"),
    ("CornerSW_Cap", -1548.0, -1168.0, 12.0, 112.0, 112.0, "edge"),
    ("CornerSE_Cap", 1548.0, -1168.0, 12.0, 112.0, 112.0, "edge"),
    ("SouthGateBase", 0.0, -1168.0, 11.0, 410.0, 125.0, "wall"),
    ("SouthGateOpening", 0.0, -1110.0, 13.0, 250.0, 34.0, "edge"),
    ("SouthGateLeftTower", -260.0, -1168.0, 12.0, 130.0, 170.0, "tower"),
    ("SouthGateRightTower", 260.0, -1168.0, 12.0, 130.0, 170.0, "tower"),
]


def source_if_exists(*parts):
    path = os.path.join(*parts)
    return path if os.path.exists(path) else None


def source_exists_for_sprite(sprite_name):
    return source_if_exists(UNITY_BUILDING_ROOT, "{0}.png".format(sprite_name)) is not None


def resolve_building_sprite_name(building_type, model_id, init_id):
    if source_exists_for_sprite(model_id):
        return model_id, "exact"

    prefix = "{0}_".format(model_id)
    variants = sorted(
        os.path.splitext(name)[0]
        for name in os.listdir(UNITY_BUILDING_ROOT)
        if name.startswith(prefix) and name.endswith(".png") and not name.endswith("_mask.png")
    ) if os.path.isdir(UNITY_BUILDING_ROOT) else []
    if variants:
        return variants[init_id % len(variants)], "variant:{0}".format(model_id)

    fallbacks = DECORATION_SPRITE_FALLBACKS.get(building_type, [])
    usable_fallbacks = [name for name in fallbacks if source_exists_for_sprite(name)]
    if usable_fallbacks:
        return usable_fallbacks[init_id % len(usable_fallbacks)], "decoration_fallback:{0}".format(model_id)

    return None, "missing:{0}".format(model_id)


def write_init_building_resolution_report(records):
    os.makedirs(os.path.dirname(INIT_BUILDING_RESOLUTION_REPORT), exist_ok=True)
    with open(INIT_BUILDING_RESOLUTION_REPORT, "w", encoding="utf-8") as handle:
        json.dump({
            "target_level": TARGET_LEVEL,
            "civ_type": UNITY_INIT_CITY_CIV_TYPE,
            "age": UNITY_INIT_CITY_AGE,
            "records": records,
            "spawnable": sum(1 for record in records if record.get("sprite_name")),
            "missing": [record for record in records if not record.get("sprite_name")],
            "fallbacks": [record for record in records if "fallback" in record.get("resolution", "") or record.get("resolution", "").startswith("variant")],
        }, handle, ensure_ascii=False, indent=2)


def read_config_table(table_name):
    path = os.path.join(CONFIG_TABLE_ROOT, "{0}.json".format(table_name))
    if not os.path.exists(path):
        unreal.log_warning("Missing extracted Unity config table: {0}".format(path))
        return []
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle).get("rows", [])


def int_field(row, name, default=0):
    try:
        return int(row.get(name, default))
    except (TypeError, ValueError):
        return default


def build_model_lookup():
    lookup = {}
    fallback = {}
    for row in read_config_table("BuildingModelData"):
        building_type = int_field(row, "type")
        model_id = str(row.get("modelId", "")).strip()
        if building_type <= 0 or not model_id:
            continue
        if int_field(row, "civType") == UNITY_INIT_CITY_CIV_TYPE and int_field(row, "age") == UNITY_INIT_CITY_AGE:
            lookup[building_type] = model_id
        fallback.setdefault(building_type, model_id)
    for building_type, model_id in fallback.items():
        lookup.setdefault(building_type, model_id)
    return lookup


def build_type_size_lookup():
    sizes = {}
    for row in read_config_table("BuildingTypeConfig"):
        building_type = int_field(row, "type")
        if building_type > 0:
            sizes[building_type] = (
                max(1, int_field(row, "width", 3)),
                max(1, int_field(row, "length", 3)),
            )
    return sizes


def load_config_init_building_layout():
    init_rows = read_config_table("InitBuilding")
    if not init_rows:
        return []

    model_lookup = build_model_lookup()
    size_lookup = build_type_size_lookup()
    layout = []
    records = []
    for row in init_rows:
        building_type = int_field(row, "type")
        init_id = int_field(row, "ID")
        model_id = model_lookup.get(building_type)
        record = {
            "init_id": init_id,
            "building_type": building_type,
            "model_id": model_id or "",
            "posX": int_field(row, "posX"),
            "posY": int_field(row, "posY"),
        }
        if not model_id:
            record["resolution"] = "missing_model"
            record["sprite_name"] = ""
            records.append(record)
            continue

        sprite_name, resolution = resolve_building_sprite_name(building_type, model_id, init_id)
        record["resolution"] = resolution
        record["sprite_name"] = sprite_name or ""
        records.append(record)
        if not sprite_name:
            continue

        building_level = int_field(row, "level", 1)
        grid_x = int_field(row, "posX")
        grid_y = int_field(row, "posY")
        width, length = size_lookup.get(building_type, (3, 3))
        visual_width = float(max(width, length)) * 78.0
        layout.append({
            "init_id": init_id,
            "building_type": building_type,
            "label": "Init_{0:03d}_Type_{1}".format(init_id, building_type),
            "sprite_name": sprite_name,
            "x": float(grid_x) * UNITY_INIT_GRID_X,
            "y": float(grid_y) * UNITY_INIT_GRID_Y,
            "visual_width": max(180.0, visual_width),
            "sort_offset": init_id,
            "source_tags": [
                "RokUnityConfigInitBuilding",
                "UnityInitId_{0}".format(init_id),
                "UnityBuildingType_{0}".format(building_type),
                "UnityBuildingLevel_{0}".format(building_level),
                "UnityGridX_{0}".format(grid_x),
                "UnityGridY_{0}".format(grid_y),
                "UnitySpriteResolution_{0}".format(resolution.replace(":", "_")),
            ],
        })

    write_init_building_resolution_report(records)
    missing = [record for record in records if not record.get("sprite_name")]
    fallbacks = [record for record in records if "fallback" in record.get("resolution", "") or record.get("resolution", "").startswith("variant")]
    if missing:
        unreal.log_warning("Skipped {0} Unity InitBuilding entries without usable sprites: {1}".format(
            len(missing),
            "; ".join("type {0}: {1}".format(record["building_type"], record["resolution"]) for record in missing[:8]),
        ))
    if fallbacks:
        unreal.log_warning("Resolved {0} Unity InitBuilding decorative variants through fallback sprites.".format(len(fallbacks)))
    return layout


def ue_horizontal_fov_from_unity_vertical_fov(vertical_fov, aspect_ratio):
    return math.degrees(2.0 * math.atan(math.tan(math.radians(vertical_fov) * 0.5) * aspect_ratio))


def city_default_camera_rotation():
    return unreal.MathLibrary.find_look_at_rotation(CAMERA_LOCATION, CAMERA_TARGET)


def textured_material(source_path, asset_name, material_name, masked=False):
    texture = runtime.import_runtime_texture_as(source_path, asset_name)
    if not texture:
        raise RuntimeError("Failed to import texture: {0}".format(source_path))
    blend = unreal.BlendMode.BLEND_MASKED if masked else unreal.BlendMode.BLEND_OPAQUE
    return runtime.create_textured_surface_material(texture, material_name, blend, 0.08 if masked else 0.0)


def building_paths(sprite_name):
    color = source_if_exists(UNITY_BUILDING_ROOT, "{0}.png".format(sprite_name))
    mask = source_if_exists(UNITY_BUILDING_ROOT, "{0}_mask.png".format(sprite_name))
    if not color:
        raise RuntimeError("Missing Unity building sprite: {0}".format(sprite_name))
    return color, mask


def spawn_scene_sprite(label, sprite_name, x, y, visual_width, sort_offset, folder, tags, selectable=False):
    color_path, mask_path = building_paths(sprite_name)
    texture = runtime.import_runtime_texture_as(color_path, "UnityGameApp_{0}".format(sprite_name))
    if not texture:
        raise RuntimeError("Failed to import Unity building texture: {0}".format(color_path))
    material = runtime.create_runtime_city_sprite_material(
        texture,
        "UnityGameApp_{0}".format(sprite_name),
        mask_path,
        False,
        False,
        "R",
        0.32,
    )
    tex_width, tex_height = runtime.texture_dimensions(texture)
    visual_height = float(visual_width) * float(tex_height) / max(1.0, float(tex_width))
    sort_priority = int((float(y) + 5000.0) * 10.0) + int(sort_offset)
    actor = runtime.spawn_sprite_actor(
        "RokUnityGameApp_{0}_{1}".format(label, sprite_name),
        material,
        float(x),
        float(y),
        float(visual_width),
        visual_height,
        sort_priority,
        folder,
        ["RokUnityGameAppCity"] + list(tags),
        selectable,
        (5, 5),
        (0, 0),
        z_override=0.0,
        anchor_mode="bottom",
    )
    return actor


def spawn_building(label, sprite_name, x, y, visual_width, sort_offset):
    return spawn_scene_sprite(
        label,
        sprite_name,
        x,
        y,
        visual_width,
        sort_offset,
        "RokUnityGameAppCity/Buildings",
        ["RokBuilding", "CityBuilding"],
        True,
    )


def spawn_config_init_buildings():
    layout = load_config_init_building_layout()
    if not layout:
        unreal.log_warning("Falling back to hardcoded Unity GameApp building layout.")
        for building in BUILDING_LAYOUT:
            spawn_building(*building)
        return len(BUILDING_LAYOUT), 0

    spawned = 0
    for item in layout:
        if SHOW_BUILDING_EXPERIMENTAL_LAYERS:
            spawn_building_ground_patch_layer(item)
            spawn_building_visual_layers(item)
            actor = spawn_building_body_layer(item)
        else:
            actor = None
        if not actor:
            actor = spawn_scene_sprite(
                item["label"],
                item["sprite_name"],
                item["x"],
                item["y"],
                item["visual_width"],
                item["sort_offset"],
                "RokUnityGameAppCity/Buildings",
                ["RokBuilding", "CityBuilding"] + item["source_tags"],
                True,
            )
        if actor:
            spawned += 1
    return spawned, len(layout)


def get_building_visual_layers_manifest():
    global BUILDING_VISUAL_LAYERS_CACHE
    if BUILDING_VISUAL_LAYERS_CACHE is not None:
        return BUILDING_VISUAL_LAYERS_CACHE
    BUILDING_VISUAL_LAYERS_CACHE = {}
    if not os.path.exists(UNITY_BUILDING_VISUAL_LAYERS_MANIFEST):
        unreal.log_warning("Missing Unity building visual layers manifest: {0}".format(UNITY_BUILDING_VISUAL_LAYERS_MANIFEST))
        return BUILDING_VISUAL_LAYERS_CACHE
    with open(UNITY_BUILDING_VISUAL_LAYERS_MANIFEST, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != "rok.unity_building_visual_layers.v1":
        raise RuntimeError("Unexpected Unity building visual layers schema: {0}".format(manifest.get("schema")))
    for layer in manifest.get("layers", []):
        BUILDING_VISUAL_LAYERS_CACHE[int(layer.get("init_id", 0))] = layer
    return BUILDING_VISUAL_LAYERS_CACHE


def spawn_building_visual_layers(item):
    layer = get_building_visual_layers_manifest().get(int(item.get("init_id", 0)))
    if not layer or not layer.get("shadow_layer"):
        return None
    source_path = os.path.join(PROJECT_ROOT, layer["shadow_layer"])
    if not os.path.exists(source_path):
        unreal.log_warning("Missing Unity building shadow layer source: {0}".format(source_path))
        return None
    texture = runtime.import_runtime_texture_as(
        source_path,
        "UnityGameApp_BuildingShadow_{0}_{1}".format(layer.get("init_id", 0), layer.get("sprite_name", "")),
    )
    if not texture:
        return None
    material = runtime.create_runtime_city_sprite_material(
        texture,
        "UnityGameApp_BuildingShadow_{0}_{1}".format(layer.get("init_id", 0), layer.get("sprite_name", "")),
        None,
        False,
        False,
        "A",
        0.05,
    )
    tex_width, tex_height = runtime.texture_dimensions(texture)
    visual_width = float(item["visual_width"]) * 1.04
    visual_height = visual_width * float(tex_height) / max(1.0, float(tex_width))
    actor = runtime.spawn_sprite_actor(
        "RokUnityGameApp_BuildingShadow_Init_{0}_{1}".format(layer.get("init_id", 0), layer.get("sprite_name", "")),
        material,
        float(item["x"]),
        float(item["y"]) + 10.0,
        visual_width,
        visual_height,
        int((float(item["y"]) + 5000.0) * 10.0) + int(item["sort_offset"]) - 5,
        "RokUnityGameAppCity/BuildingVisualLayers",
        [
            "RokUnityGameAppCity",
            "RokUnityBuildingVisualLayer",
            "UnityBuildingVisualLayer_Shadow",
            "UnityInitId_{0}".format(layer.get("init_id", 0)),
            "UnityBuildingType_{0}".format(layer.get("building_type", 0)),
            "UnitySprite_{0}".format(layer.get("sprite_name", "")),
            "UnityShadowSource_{0}".format(layer.get("shadow_layer", "").replace("/", "_").replace(".", "_")),
        ],
        False,
        (5, 5),
        (0, 0),
        z_override=-2.0,
        anchor_mode="bottom",
    )
    return actor


def get_building_body_ground_layers_manifest():
    global BUILDING_BODY_GROUND_LAYERS_CACHE
    if BUILDING_BODY_GROUND_LAYERS_CACHE is not None:
        return BUILDING_BODY_GROUND_LAYERS_CACHE
    BUILDING_BODY_GROUND_LAYERS_CACHE = {}
    if not os.path.exists(UNITY_BUILDING_BODY_GROUND_LAYERS_MANIFEST):
        unreal.log_warning("Missing Unity building body/ground layers manifest: {0}".format(UNITY_BUILDING_BODY_GROUND_LAYERS_MANIFEST))
        return BUILDING_BODY_GROUND_LAYERS_CACHE
    with open(UNITY_BUILDING_BODY_GROUND_LAYERS_MANIFEST, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != "rok.unity_building_body_ground_layers.v1":
        raise RuntimeError("Unexpected Unity building body/ground layers schema: {0}".format(manifest.get("schema")))
    for layer in manifest.get("layers", []):
        BUILDING_BODY_GROUND_LAYERS_CACHE[int(layer.get("init_id", 0))] = layer
    return BUILDING_BODY_GROUND_LAYERS_CACHE


def spawn_building_ground_patch_layer(item):
    layer = get_building_body_ground_layers_manifest().get(int(item.get("init_id", 0)))
    if not layer or not layer.get("has_ground_patch") or not layer.get("ground_patch_layer"):
        return None
    source_path = os.path.join(PROJECT_ROOT, layer["ground_patch_layer"])
    if not os.path.exists(source_path):
        unreal.log_warning("Missing Unity building ground patch source: {0}".format(source_path))
        return None
    texture = runtime.import_runtime_texture_as(
        source_path,
        "UnityGameApp_BuildingGroundPatch_{0}_{1}".format(layer.get("init_id", 0), layer.get("sprite_name", "")),
    )
    if not texture:
        return None
    material = runtime.create_runtime_city_sprite_material(
        texture,
        "UnityGameApp_BuildingGroundPatch_{0}_{1}".format(layer.get("init_id", 0), layer.get("sprite_name", "")),
        None,
        False,
        False,
        "A",
        0.05,
    )
    tex_width, tex_height = runtime.texture_dimensions(texture)
    visual_width = float(item["visual_width"]) * 1.02
    visual_height = visual_width * float(tex_height) / max(1.0, float(tex_width))
    actor = runtime.spawn_sprite_actor(
        "RokUnityGameApp_BuildingGroundPatch_Init_{0}_{1}".format(layer.get("init_id", 0), layer.get("sprite_name", "")),
        material,
        float(item["x"]),
        float(item["y"]),
        visual_width,
        visual_height,
        int((float(item["y"]) + 5000.0) * 10.0) + int(item["sort_offset"]) - 8,
        "RokUnityGameAppCity/BuildingVisualLayers",
        [
            "RokUnityGameAppCity",
            "RokUnityBuildingVisualLayer",
            "UnityBuildingVisualLayer_GroundPatch",
            "UnityInitId_{0}".format(layer.get("init_id", 0)),
            "UnityBuildingType_{0}".format(layer.get("building_type", 0)),
            "UnitySprite_{0}".format(layer.get("sprite_name", "")),
            "UnityGroundPatchSource_{0}".format(layer.get("ground_patch_layer", "").replace("/", "_").replace(".", "_")),
        ],
        False,
        (5, 5),
        (0, 0),
        z_override=-3.0,
        anchor_mode="bottom",
    )
    return actor


def spawn_building_body_layer(item):
    layer = get_building_body_ground_layers_manifest().get(int(item.get("init_id", 0)))
    if not layer or not layer.get("body_layer"):
        return None
    source_path = os.path.join(PROJECT_ROOT, layer["body_layer"])
    if not os.path.exists(source_path):
        unreal.log_warning("Missing Unity building body layer source: {0}".format(source_path))
        return None
    texture = runtime.import_runtime_texture_as(
        source_path,
        "UnityGameApp_BuildingBodyClean_{0}_{1}".format(layer.get("init_id", 0), layer.get("sprite_name", "")),
    )
    if not texture:
        return None
    material = runtime.create_runtime_city_sprite_material(
        texture,
        "UnityGameApp_BuildingBodyClean_{0}_{1}".format(layer.get("init_id", 0), layer.get("sprite_name", "")),
        None,
        False,
        False,
        "A",
        0.18,
    )
    tex_width, tex_height = runtime.texture_dimensions(texture)
    visual_width = float(item["visual_width"])
    visual_height = visual_width * float(tex_height) / max(1.0, float(tex_width))
    actor = runtime.spawn_sprite_actor(
        "RokUnityGameApp_BuildingBody_Init_{0}_{1}".format(layer.get("init_id", 0), layer.get("sprite_name", "")),
        material,
        float(item["x"]),
        float(item["y"]),
        visual_width,
        visual_height,
        int((float(item["y"]) + 5000.0) * 10.0) + int(item["sort_offset"]),
        "RokUnityGameAppCity/Buildings",
        [
            "RokUnityGameAppCity",
            "RokBuilding",
            "CityBuilding",
            "RokUnityBuildingVisualLayer",
            "UnityBuildingVisualLayer_BodyClean",
            "UnityInitId_{0}".format(layer.get("init_id", 0)),
            "UnityBuildingType_{0}".format(layer.get("building_type", 0)),
            "UnityGridX_{0}".format(layer.get("grid_x", 0)),
            "UnityGridY_{0}".format(layer.get("grid_y", 0)),
            "UnitySprite_{0}".format(layer.get("sprite_name", "")),
            "UnityBodySource_{0}".format(layer.get("body_layer", "").replace("/", "_").replace(".", "_")),
        ] + list(item.get("source_tags", [])),
        True,
        (5, 5),
        (0, 0),
        z_override=0.0,
        anchor_mode="bottom",
    )
    return actor


def spawn_ground(primitives, materials):
    if SHOW_CITY_VISUAL_LAYER_STACK_PREVIEW and spawn_city_visual_layer_stack(primitives):
        return

    grass_path = source_if_exists(CLEAN_LAYER_ROOT, "UnityGrassGround_I_TYPE_Grass_01.png")
    if not grass_path:
        grass_path = source_if_exists(UNITY_TERRAIN_ROOT, "build_SceneMap_2_lv.tif")
    if not grass_path:
        grass_path = source_if_exists(UNITY_LAND_TEXTURE_ROOT, "ground_mask_green_02.png")
    if not grass_path:
        raise RuntimeError("Missing Unity terrain source for GameApp city ground")

    grass_material = textured_material(grass_path, "UnityGameApp_GrassGround", "M_UnityGameApp_GrassGround", False)
    runtime.spawn_runtime_plane(
        "RokUnityGameApp_Ground",
        primitives,
        (0.0, -40.0, -12.0),
        3100.0,
        2300.0,
        grass_material,
        "RokUnityGameAppCity/Terrain",
    )

    if ROAD_LAYOUT:
        road_material = runtime.create_unlit_constant_material(
            "M_UnityGameApp_RoadSand",
            unreal.LinearColor(0.47, 0.48, 0.24, 1.0),
        )
        for label, x, y, width, height, yaw in ROAD_LAYOUT:
            runtime.spawn_runtime_plane(
                "RokUnityGameApp_{0}".format(label),
                primitives,
                (float(x), float(y), -6.0),
                float(width),
                float(height),
                road_material,
                "RokUnityGameAppCity/Roads",
                yaw=float(yaw),
                sort_priority=-8000,
            )


def create_layer_material(layer):
    source_path = os.path.join(PROJECT_ROOT, layer["source"])
    asset_name = "UnityGameApp_Layer_{0}".format(layer["id"])
    if layer.get("blend") == "alpha":
        texture = runtime.import_runtime_texture_as(source_path, asset_name)
        if not texture:
            raise RuntimeError("Failed to import visual layer texture: {0}".format(source_path))
        return runtime.create_runtime_city_sprite_material(
            texture,
            asset_name,
            None,
            False,
            False,
            "A",
            0.18,
        )
    return textured_material(
        source_path,
        asset_name,
        "M_{0}".format(asset_name),
        False,
    )


def spawn_city_visual_layer_stack(primitives):
    if not os.path.exists(UNITY_CITY_VISUAL_LAYER_STACK_MANIFEST):
        unreal.log_warning("Missing Unity city visual layer stack manifest; using fallback ground path: {0}".format(UNITY_CITY_VISUAL_LAYER_STACK_MANIFEST))
        return False

    with open(UNITY_CITY_VISUAL_LAYER_STACK_MANIFEST, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != "rok.unity_city_visual_layer_stack.v1":
        raise RuntimeError("Unexpected Unity city visual layer stack schema: {0}".format(manifest.get("schema")))

    spawned = 0
    for layer in manifest.get("layers", []):
        if not layer.get("exists"):
            unreal.log_warning("Skipping missing Unity visual layer source: {0}".format(layer.get("source")))
            continue

        material = create_layer_material(layer)
        actor = runtime.spawn_runtime_plane(
            "RokUnityGameApp_Layer_{0}".format(layer["id"]),
            primitives,
            (float(layer.get("x", 0.0)), float(layer.get("y", 0.0)), float(layer.get("z", 0.0))),
            float(layer.get("width", 100.0)),
            float(layer.get("height", 100.0)),
            material,
            "RokUnityGameAppCity/VisualLayers",
            yaw=float(layer.get("yaw", 0.0)),
            sort_priority=-10000 + int(layer.get("sort_index", spawned)),
        )
        if actor:
            actor.tags = [
                "RokUnityGameAppCity",
                "RokUnityCityVisualLayer",
                layer.get("ue_actor_tag", "RokUnityCityVisualLayer_{0}".format(layer["id"])),
                "UnityLayerRole_{0}".format(layer.get("role", "unknown")),
                "UnityLayerSource_{0}".format(layer.get("source", "").replace("/", "_").replace(".", "_")),
            ]
            spawned += 1

    unreal.log("Spawned {0} Unity city visual layer actors from manifest.".format(spawned))
    return spawned > 0


def spawn_palm(label, sprite_name, x, y, visual_width, sort_offset):
    source_path = source_if_exists(PALM_CROP_ROOT, "{0}.png".format(sprite_name))
    if not source_path:
        unreal.log_warning("Missing prepared Unity palm sprite: {0}".format(sprite_name))
        return None
    texture = runtime.import_runtime_texture_as(source_path, "UnityGameApp_{0}".format(sprite_name))
    if not texture:
        return None
    material = runtime.create_runtime_city_sprite_material(
        texture,
        "UnityGameApp_{0}".format(sprite_name),
        None,
        False,
        False,
        "A",
        0.18,
    )
    tex_width, tex_height = runtime.texture_dimensions(texture)
    visual_height = float(visual_width) * float(tex_height) / max(1.0, float(tex_width))
    return runtime.spawn_sprite_actor(
        "RokUnityGameApp_{0}_{1}".format(label, sprite_name),
        material,
        float(x),
        float(y),
        float(visual_width),
        visual_height,
        int((float(y) + 5000.0) * 10.0) + int(sort_offset),
        "RokUnityGameAppCity/Trees",
        ["RokUnityGameAppCity", "RokUnityGameAppTree"],
        False,
        (1, 1),
        (0, 0),
        z_override=0.0,
        anchor_mode="bottom",
    )


def spawn_city_wall_boundary():
    if not SHOW_CITY_BOUNDARY_SOURCE_PREVIEW:
        unreal.log("Unity city-boundary source preview is disabled; manifest remains available for migration evidence.")
        return 0

    if not os.path.exists(UNITY_CITY_BOUNDARY_MANIFEST):
        unreal.log_warning("Missing Unity city-boundary manifest; skipping source-driven city wall layer: {0}".format(UNITY_CITY_BOUNDARY_MANIFEST))
        return 0

    with open(UNITY_CITY_BOUNDARY_MANIFEST, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != "rok.unity_city_boundary.v1":
        raise RuntimeError("Unexpected Unity city-boundary manifest schema: {0}".format(manifest.get("schema")))

    plane_mesh = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane.Plane")
    if not plane_mesh:
        raise RuntimeError("Missing /Engine/BasicShapes/Plane")

    ground_source = source_if_exists(CLEAN_LAYER_ROOT, "UnityCityGround_build_ground_6_101_1.png")
    if not ground_source:
        ground_source = source_if_exists(CLEAN_LAYER_ROOT, "UnityCityBackground_Terrain_grass_08_3g2.png")
    if not ground_source:
        unreal.log_warning("Missing prepared Unity city-boundary ground texture; source actors will still be tagged.")

    ground_material = None
    if ground_source:
        ground_material = textured_material(
            ground_source,
            "UnityGameApp_CityBoundaryGround_Source",
            "M_UnityGameApp_CityBoundaryGround_Source",
            False,
        )

    inactive_wall_source = source_if_exists(
        PROJECT_ROOT,
        "ref",
        "client-unity",
        "Assets",
        "BundleAssetsTest",
        "Env",
        "City",
        "CityWall_1_1_c.png",
    )
    inactive_wall_material = None
    if inactive_wall_source:
        inactive_wall_material = textured_material(
            inactive_wall_source,
            "UnityGameApp_InactiveCityWall_Source",
            "M_UnityGameApp_InactiveCityWall_Source",
            True,
        )

    spawned = 0
    for index, instance in enumerate(manifest.get("boundary_instances", [])):
        name = instance.get("primary_name") or "Boundary_{0}".format(index)
        active = bool(instance.get("active"))
        scene_name = os.path.splitext(os.path.basename(instance.get("scene", "UnityScene")))[0]
        pos = instance.get("local_position", {})
        scale = instance.get("local_scale", {})
        euler = instance.get("local_euler_hint", {})

        is_ground = "Ground" in name
        if is_ground and not active:
            continue
        if not is_ground and not (active or SHOW_INACTIVE_CITY_BOUNDARY_PREVIEW):
            material = inactive_wall_material
            width = 160.0
            height = 120.0
            location = unreal.Vector(float(pos.get("x", 0.0)) * 1000.0, float(pos.get("z", 0.0)) * 1000.0, -40.0 - float(index))
            hidden = True
        elif is_ground:
            material = ground_material
            width = 3000.0 * float(scale.get("x", 1.0))
            height = 2150.0 * float(scale.get("z", 1.0))
            location = unreal.Vector(0.0, -40.0, -10.0 - float(index) * 0.2)
            hidden = False
        else:
            material = inactive_wall_material
            width = 900.0 * float(scale.get("x", 1.0))
            height = 420.0 * float(scale.get("z", 1.0))
            location = unreal.Vector(float(pos.get("x", 0.0)) * 1000.0, float(pos.get("z", 0.0)) * 1000.0, 18.0 + float(index))
            hidden = False

        if not material:
            material = runtime.create_unlit_constant_material(
                "M_UnityGameApp_CityBoundary_SourceFallback",
                unreal.LinearColor(0.55, 0.48, 0.34, 1.0),
            )

        actor = unreal.EditorLevelLibrary.spawn_actor_from_object(
            plane_mesh,
            location,
            unreal.Rotator(0.0, float(euler.get("y", 0.0)), 0.0),
        )
        if not actor:
            unreal.log_warning("Failed to spawn Unity city-boundary source actor: {0}".format(name))
            continue

        actor.set_actor_label("RokUnityGameApp_UnityBoundary_{0}_{1}".format(scene_name, name.replace(" ", "_").replace("(", "").replace(")", "")))
        actor.set_folder_path("RokUnityGameAppCity/UnitySourceBoundary")
        actor.set_actor_scale3d(unreal.Vector(float(width) / 100.0, float(height) / 100.0, 1.0))
        actor.set_actor_hidden_in_game(hidden)
        actor.set_is_temporarily_hidden_in_editor(False)
        actor.tags = [
            "RokUnityGameAppCity",
            "RokUnityCityBoundarySource",
            "UnityScene_{0}".format(scene_name),
            "UnityBoundaryName_{0}".format(name.replace(" ", "_")),
            "UnityPrefabGuid_{0}".format(instance.get("source_prefab_guid", "")),
            "UnityBoundaryActive_{0}".format(str(active)),
        ]
        for material_path in instance.get("material_override_paths", []):
            if material_path:
                actor.tags.append("UnityMaterial_{0}".format(material_path.replace("/", "_").replace(".", "_")))
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component:
            component.set_material(0, material)
            component.set_editor_property("cast_shadow", False)
        spawned += 1

    unreal.log("Spawned {0} Unity city-boundary source actors from manifest.".format(spawned))
    return spawned


def spawn_walls_and_trees():
    spawn_city_wall_boundary()
    for label, sprite_name, x, y, visual_width, sort_offset in WALL_SPRITE_LAYOUT:
        spawn_scene_sprite(
            label,
            sprite_name,
            x,
            y,
            visual_width,
            sort_offset,
            "RokUnityGameAppCity/Wall",
            ["RokUnityGameAppWall"],
            False,
        )
    for index, (sprite_name, x, y, visual_width, sort_offset) in enumerate(TREE_LAYOUT):
        spawn_palm("Tree_{0:02d}".format(index + 1), sprite_name, x, y, visual_width, sort_offset)


def spawn_camera():
    rotation = city_default_camera_rotation()
    camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.CameraActor, CAMERA_LOCATION, rotation)
    camera.set_actor_label("Camera_RokUnityGameAppCity")
    camera.set_folder_path("RokUnityGameAppCity/Camera")
    camera.tags = [
        "RokUnityGameAppCity",
        "RokUnityGameAppCamera",
        "UnityCameraParam_city_default",
        "UnityCameraDxf_150",
        "UnityCameraDistance_20",
        "UnityCameraVerticalFov_7p5",
    ]
    component = camera.get_component_by_class(unreal.CameraComponent)
    component.set_editor_property("projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
    component.set_editor_property(
        "field_of_view",
        ue_horizontal_fov_from_unity_vertical_fov(UNITY_CITY_CAMERA_VERTICAL_FOV, CAMERA_ASPECT),
    )
    component.set_editor_property("aspect_ratio", CAMERA_ASPECT)
    settings = component.get_editor_property("post_process_settings")
    for prop, value in [
        ("override_auto_exposure_method", True),
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
            settings.set_editor_property(prop, value)
        except Exception:
            pass
    try:
        settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
    except Exception:
        pass
    component.set_editor_property("post_process_settings", settings)


def spawn_player_start():
    rotation = city_default_camera_rotation()
    player_start = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart,
        CAMERA_LOCATION,
        rotation,
    )
    player_start.set_actor_label("PlayerStart_RokUnityGameAppCity")
    player_start.set_folder_path("RokUnityGameAppCity/Player")
    player_start.tags = ["RokUnityGameAppCity", "RokUnityGameAppPlayerStart", "UnityCameraParam_city_default"]
    return player_start


def is_target_level_loaded():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        return False
    return str(world.get_outermost().get_name()) == TARGET_LEVEL


def build_scene():
    if not os.path.isdir(UNITY_PROJECT_ROOT):
        raise RuntimeError("Unity project root is missing: {0}".format(UNITY_PROJECT_ROOT))

    if is_target_level_loaded():
        unreal.log("Unity GameApp city target map already loaded: {0}".format(TARGET_LEVEL))
    elif unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    else:
        if not unreal.EditorLevelLibrary.new_level(TARGET_LEVEL):
            unreal.log("Target map did not exist yet; continuing in the currently loaded template world.")

    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        unreal.EditorLevelLibrary.destroy_actor(actor)

    runtime.CAMERA_LOCATION = CAMERA_LOCATION
    runtime.CAMERA_TARGET = CAMERA_TARGET
    runtime.SPRITE_PLANE_MODE = "camera"
    runtime.ENGINE_PLANE_FLIP_Y = True

    proto.ensure_project_primitive_meshes()
    primitives = {name: proto.load_primitive(name) for name in proto.PRIMITIVE_PATHS}
    materials = runtime.make_runtime_materials()

    spawn_ground(primitives, materials)
    spawn_walls_and_trees()
    spawned_buildings, source_buildings = spawn_config_init_buildings()
    spawn_player_start()
    spawn_camera()
    runtime.configure_world_settings()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError("Failed to save Unity GameApp city map: {0}".format(TARGET_LEVEL))
    unreal.EditorAssetLibrary.save_asset(TARGET_LEVEL, only_if_is_dirty=False)
    unreal.log("Unity GameApp city map saved: {0}; buildings={1}; trees={2}".format(
        TARGET_LEVEL,
        spawned_buildings,
        len(TREE_LAYOUT),
    ))
    unreal.log("Unity GameApp city source buildings used: {0}".format(source_buildings))


if __name__ == "__main__":
    build_scene()
