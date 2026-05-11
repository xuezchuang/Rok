import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokPrototypeScene as proto
import CreateRokRuntimeCityScene as runtime


TARGET_LEVEL = os.environ.get("ROK_BUILDING_SHOWCASE_LEVEL", "/Game/Maps/L_RokCityMilitaryShowcase")
BUILDING_TYPES = [
    int(value)
    for value in os.environ.get("ROK_BUILDING_SHOWCASE_TYPES", "9,10,11,12,13,14").split(",")
    if value.strip()
]
VISUAL_SCALE = float(os.environ.get("ROK_BUILDING_SHOWCASE_VISUAL_SCALE", "0.62"))
SPACING = float(os.environ.get("ROK_BUILDING_SHOWCASE_SPACING", "760.0"))


DISPLAY_NAMES = {
    9: "Barracks",
    10: "Stable",
    11: "ArcheryRange",
    12: "SiegeWorkshop",
    13: "Academy",
    14: "Hospital",
    18: "Tavern",
}


def spawn_showcase_building(building_type, x, y, building_models, building_types):
    model_id = building_models.get(building_type)
    if not model_id:
        raise RuntimeError("Missing model for building type {0}".format(building_type))

    source_path = runtime.building_texture_path(model_id)
    mask_path = runtime.building_mask_path(model_id)
    if not source_path:
        source_path = runtime.fallback_building_texture_path(building_type)
        mask_path = runtime.fallback_building_mask_path(building_type)
    if not source_path:
        raise RuntimeError("Missing texture for model {0}".format(model_id))

    texture = proto.import_city_sprite_texture(source_path)
    if not texture:
        raise RuntimeError("Failed to import texture: {0}".format(source_path))

    material = runtime.create_building_material(texture, model_id, mask_path)
    tex_width, tex_height = runtime.texture_dimensions(texture)
    visual_width, visual_height = runtime.prefab_visual_size(tex_width, tex_height)
    visual_width *= VISUAL_SCALE
    visual_height *= VISUAL_SCALE

    footprint = building_types.get(building_type, {"width": 5, "length": 5})
    actor = runtime.spawn_sprite_actor(
        "RokShowcase_{0}_{1}".format(DISPLAY_NAMES.get(building_type, "Building"), model_id),
        material,
        x,
        y,
        visual_width,
        visual_height,
        int((y + 3000.0) * 10.0 + building_type),
        "RokBuildingShowcase/Buildings",
        ["RokBuilding", "CityBuilding", "RokBuildingShowcase"],
        True,
        (footprint["width"], footprint["length"]),
        (0, 0),
    )
    unreal.log(
        "Showcase building: type={0}, model={1}, texture={2}x{3}, visual={4:.1f}x{5:.1f}".format(
            building_type,
            model_id,
            tex_width,
            tex_height,
            visual_width,
            visual_height,
        )
    )
    return actor


def build_showcase_scene():
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    else:
        if not unreal.EditorLevelLibrary.new_level(TARGET_LEVEL):
            raise RuntimeError("Failed to create showcase map: {0}".format(TARGET_LEVEL))

    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        unreal.EditorLevelLibrary.destroy_actor(actor)

    proto.ensure_project_primitive_meshes()
    proto.setup_lighting()

    building_models = runtime.load_building_model_map()
    building_types = runtime.load_building_type_map()
    start_x = -0.5 * SPACING * (len(BUILDING_TYPES) - 1)
    for index, building_type in enumerate(BUILDING_TYPES):
        spawn_showcase_building(
            building_type,
            start_x + index * SPACING,
            0.0,
            building_models,
            building_types,
        )

    runtime.spawn_camera(runtime.load_camera_record())
    runtime.configure_world_settings()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError("Failed to save showcase map: {0}".format(TARGET_LEVEL))
    unreal.EditorAssetLibrary.save_asset(TARGET_LEVEL, only_if_is_dirty=False)
    unreal.log("Building showcase map saved: {0}".format(TARGET_LEVEL))


if __name__ == "__main__":
    build_showcase_scene()
