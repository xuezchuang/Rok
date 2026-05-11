import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokPrototypeScene as proto
import CreateRokRuntimeCityScene as runtime


TARGET_LEVEL = os.environ.get("ROK_CITY_CLUSTER_LEVEL", "/Game/Maps/L_RokCityClusterPreview")
VISUAL_SCALE = float(os.environ.get("ROK_CITY_CLUSTER_VISUAL_SCALE", "0.58"))
SPRITE_SOURCE = os.environ.get("ROK_CITY_CLUSTER_SOURCE", "model").lower()
NAMED_CIVILIZATION = os.environ.get("ROK_CITY_CLUSTER_NAMED_CIV", "1")
NAMED_CUTOUT_ROOT = os.environ.get(
    "ROK_CITY_CLUSTER_NAMED_CUTOUT_ROOT",
    os.path.join(PROJECT_ROOT, "Saved", "RokDerivedSprites", "MaskMaxCutouts"),
)
SPAWN_SCENE_BASE = os.environ.get("ROK_CITY_CLUSTER_BASE", "1") != "0"
SPAWN_SCENE_ROADS = os.environ.get("ROK_CITY_CLUSTER_ROADS", "1") != "0"
SPAWN_SCENE_PLAZA = os.environ.get("ROK_CITY_CLUSTER_PLAZA", "0" if SPRITE_SOURCE == "named" else "1") != "0"
CLUSTER_ORTHO_WIDTH = float(os.environ.get("ROK_CITY_CLUSTER_ORTHO_WIDTH", "2250" if SPRITE_SOURCE == "named" else str(runtime.CAMERA_ORTHO_WIDTH)))


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


CLUSTER_LAYOUT = [
    # type, label, x, y, relative scale
    (1, "TownCenter", 70.0, 170.0, 0.82),
    (17, "CastleNorth", -220.0, 470.0, 0.56),
    (16, "AllianceCenter", 390.0, 430.0, 0.54),
    (13, "Academy", -390.0, 170.0, 0.56),
    (18, "Tavern", 430.0, 145.0, 0.54),
    (9, "BarracksWest", -490.0, -105.0, 0.58),
    (10, "StableEast", 520.0, -105.0, 0.58),
    (11, "ArcherySouthWest", -285.0, -390.0, 0.55),
    (12, "SiegeSouthEast", 275.0, -390.0, 0.55),
    (14, "HospitalSouth", 0.0, -560.0, 0.52),
    (9, "BarracksLowerWest", -600.0, -390.0, 0.48),
    (10, "StableLowerEast", 640.0, -365.0, 0.48),
    (11, "ArcheryUpperWest", -570.0, 390.0, 0.47),
    (12, "SiegeUpperEast", 620.0, 360.0, 0.47),
    (13, "AcademyLower", -425.0, -640.0, 0.44),
    (18, "TavernLower", 420.0, -640.0, 0.44),
    (14, "HospitalUpper", 60.0, 665.0, 0.43),
]


TERRAIN_SPRITES = [
    # name, x, y, scale, z, sort
    ("FloorTile_1_5_9", 0.0, 70.0, 1.10, -520.0, -9500),
    ("FloorTile_1_5_9", -350.0, 45.0, 0.88, -520.0, -9490),
    ("FloorTile_1_5_9", 360.0, 45.0, 0.88, -520.0, -9490),
    ("FloorTile_1_5_9", -330.0, -350.0, 0.88, -520.0, -9480),
    ("FloorTile_1_5_9", 340.0, -350.0, 0.88, -520.0, -9480),
    ("FloorTile_1_5_9", 0.0, -610.0, 0.74, -520.0, -9470),
    ("Road_1_1_5", -220.0, 260.0, 2.9, -500.0, -9300),
    ("Road_1_1_5", 220.0, 260.0, 2.9, -500.0, -9300),
    ("Road_1_1_5", -260.0, -120.0, 2.9, -500.0, -9290),
    ("Road_1_1_5", 260.0, -120.0, 2.9, -500.0, -9290),
    ("Road_1_1_5", -180.0, -485.0, 2.6, -500.0, -9280),
    ("Road_1_1_5", 185.0, -485.0, 2.6, -500.0, -9280),
    ("Road_1_5_0", -520.0, 130.0, 2.7, -500.0, -9270),
    ("Road_1_5_4", 525.0, 130.0, 2.7, -500.0, -9270),
    ("Road_1_5_8", -520.0, -485.0, 2.7, -500.0, -9260),
    ("Road_1_5_12", 525.0, -485.0, 2.7, -500.0, -9260),
]


def create_cluster_unlit_material(name, color):
    material_path = proto.asset_package_path(runtime.RUNTIME_MATERIAL_DESTINATION, name)
    existing = proto.load_asset_if_exists(material_path)
    if existing:
        return existing

    unreal.EditorAssetLibrary.make_directory(runtime.RUNTIME_MATERIAL_DESTINATION)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name,
        runtime.RUNTIME_MATERIAL_DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
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


def spawn_cluster_base(primitives, materials, include_roads=True, include_plaza=True):
    grass_material = materials.get("M_RokCluster_LightGrass", materials["M_Rok_Grass_Blockout"])
    runtime.spawn_runtime_plane(
        "RokCluster_GrassBase",
        primitives,
        (0.0, 0.0, -900.0),
        4800.0,
        3900.0,
        grass_material,
        "RokCityCluster/Terrain",
    )
    if include_plaza:
        runtime.spawn_runtime_plane(
            "RokCluster_CenterPlaza",
            primitives,
            (0.0, -40.0, -4.0),
            1260.0,
            900.0,
            materials["M_Rok_Plaza_Blockout"],
            "RokCityCluster/Terrain",
            yaw=45.0,
        )
    if include_roads:
        for label, x, y, width, height, yaw in [
            ("RokCluster_Road_NE", 0.0, 0.0, 2600.0, 72.0, 45.0),
            ("RokCluster_Road_NW", 0.0, 0.0, 2600.0, 72.0, -45.0),
            ("RokCluster_Road_South", 0.0, -640.0, 1450.0, 56.0, 0.0),
            ("RokCluster_Road_North", 0.0, 450.0, 1450.0, 56.0, 0.0),
        ]:
            actor = proto.spawn_mesh(
                label,
                primitives["cube"],
                (x, y, 1.5),
                (width / 100.0, height / 100.0, 0.018),
                materials["M_Rok_Path_Blockout"],
                (0.0, yaw, 0.0),
                "RokCityCluster/Roads",
            )
            runtime.configure_component_for_runtime(actor)


def create_named_layer_material(sprite_name):
    source_path = os.path.join(runtime.EXPORTED_SPRITE_ROOT, "{0}.png".format(sprite_name))
    if not os.path.exists(source_path):
        raise RuntimeError("Missing layer sprite: {0}".format(source_path))
    texture = proto.import_city_sprite_texture(source_path)
    if not texture:
        raise RuntimeError("Failed to import layer sprite: {0}".format(source_path))
    return runtime.create_building_material(texture, sprite_name, None), texture


def spawn_named_terrain_layers():
    for sprite_name, x, y, scale, z, sort_priority in TERRAIN_SPRITES:
        try:
            material, texture = create_named_layer_material(sprite_name)
            tex_width, tex_height = runtime.texture_dimensions(texture)
            runtime.spawn_sprite_actor(
                "RokClusterLayer_{0}_{1}_{2}".format(sprite_name, int(x), int(y)),
                material,
                x,
                y,
                tex_width * VISUAL_SCALE * scale,
                tex_height * VISUAL_SCALE * scale,
                sort_priority,
                "RokCityCluster/TerrainSprites",
                ["RokCityClusterPreview"],
                False,
                (1, 1),
                (0, 0),
                z_override=z,
            )
        except Exception as exc:
            unreal.log_warning("Skipped terrain sprite {0}: {1}".format(sprite_name, exc))


def resolve_texture(building_type, model_id):
    source_path = runtime.building_texture_path(model_id)
    mask_path = runtime.building_mask_path(model_id)
    if not source_path:
        source_path = runtime.fallback_building_texture_path(building_type)
        mask_path = runtime.fallback_building_mask_path(building_type)
    return source_path, mask_path


def resolve_named_texture(building_type):
    sprite_base = NAMED_SPRITE_BASES.get(building_type)
    if not sprite_base:
        raise RuntimeError("Missing named sprite for building type {0}".format(building_type))
    sprite_name = "{0}_{1}_5".format(sprite_base, NAMED_CIVILIZATION)
    cutout_path = os.path.join(NAMED_CUTOUT_ROOT, "{0}.png".format(sprite_name))
    source_path = cutout_path if os.path.exists(cutout_path) else os.path.join(runtime.EXPORTED_SPRITE_ROOT, "{0}.png".format(sprite_name))
    if not os.path.exists(source_path):
        raise RuntimeError("Missing named sprite texture: {0}".format(source_path))
    return sprite_name, source_path, None


def spawn_cluster_building(building_type, display_name, x, y, relative_scale, building_models, building_types, primitives, materials):
    if SPRITE_SOURCE == "named":
        model_id, source_path, mask_path = resolve_named_texture(building_type)
    else:
        model_id = building_models.get(building_type)
        if not model_id:
            raise RuntimeError("Missing model for building type {0}".format(building_type))
        source_path, mask_path = resolve_texture(building_type, model_id)
    if not source_path:
        raise RuntimeError("Missing texture for model {0}".format(model_id))

    texture = proto.import_city_sprite_texture(source_path)
    if not texture:
        raise RuntimeError("Failed to import texture: {0}".format(source_path))

    material = runtime.create_building_material(texture, model_id, mask_path)
    tex_width, tex_height = runtime.texture_dimensions(texture)
    if SPRITE_SOURCE == "named":
        visual_width = tex_width * VISUAL_SCALE * relative_scale
        visual_height = tex_height * VISUAL_SCALE * relative_scale
    else:
        visual_width, visual_height = runtime.prefab_visual_size(tex_width, tex_height)
        visual_width *= VISUAL_SCALE * relative_scale
        visual_height *= VISUAL_SCALE * relative_scale

    footprint = building_types.get(building_type, {"width": 5, "length": 5})
    footprint_width = max(1, footprint["width"]) * runtime.UNITY_GRID_SIZE * runtime.UE_UNITS_PER_UNITY_UNIT
    footprint_length = max(1, footprint["length"]) * runtime.UNITY_GRID_SIZE * runtime.UE_UNITS_PER_UNITY_UNIT
    if SPRITE_SOURCE != "named":
        runtime.spawn_shadow_and_pad(
            "Cluster_{0}_{1}".format(display_name, model_id),
            primitives,
            materials,
            x,
            y,
            footprint_width,
            footprint_length,
            visual_width,
        )
    return runtime.spawn_sprite_actor(
        "RokCluster_{0}_{1}".format(display_name, model_id),
        material,
        x,
        y,
        visual_width,
        visual_height,
        int((y + 5000.0) * 10.0 + building_type),
        "RokCityCluster/Buildings",
        ["RokBuilding", "CityBuilding", "RokCityClusterPreview"],
        True,
        (footprint["width"], footprint["length"]),
        (0, 0),
    )


def build_cluster_scene():
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    else:
        if not unreal.EditorLevelLibrary.new_level(TARGET_LEVEL):
            raise RuntimeError("Failed to create cluster map: {0}".format(TARGET_LEVEL))

    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        unreal.EditorLevelLibrary.destroy_actor(actor)

    proto.ensure_project_primitive_meshes()
    primitives = {name: proto.load_primitive(name) for name in proto.PRIMITIVE_PATHS}
    materials = proto.make_materials()
    materials["M_RuntimeCity_SoftShadow"] = runtime.create_soft_shadow_material()
    materials["M_RokCluster_LightGrass"] = create_cluster_unlit_material(
        "M_RokCluster_LightGrassV3",
        unreal.LinearColor(0.37, 0.55, 0.20, 1.0),
    )
    proto.setup_lighting()
    if SPAWN_SCENE_BASE:
        spawn_cluster_base(primitives, materials, SPAWN_SCENE_ROADS, SPAWN_SCENE_PLAZA)
    if SPRITE_SOURCE == "named":
        spawn_named_terrain_layers()

    building_models = runtime.load_building_model_map()
    building_types = runtime.load_building_type_map()
    for building_type, display_name, x, y, relative_scale in CLUSTER_LAYOUT:
        try:
            spawn_cluster_building(
                building_type,
                display_name,
                x,
                y,
                relative_scale,
                building_models,
                building_types,
                primitives,
                materials,
            )
        except Exception as exc:
            unreal.log_warning("Skipped cluster building type {0}: {1}".format(building_type, exc))

    runtime.CAMERA_ORTHO_WIDTH = CLUSTER_ORTHO_WIDTH
    runtime.spawn_camera(runtime.load_camera_record())
    runtime.configure_world_settings()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError("Failed to save cluster map: {0}".format(TARGET_LEVEL))
    unreal.EditorAssetLibrary.save_asset(TARGET_LEVEL, only_if_is_dirty=False)
    unreal.log("City cluster preview map saved: {0}".format(TARGET_LEVEL))


if __name__ == "__main__":
    build_cluster_scene()
