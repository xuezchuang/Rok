import os
import sys

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


def find_diagnostic_model(building_models):
    model_id = building_models.get(DIAGNOSTIC_BUILDING_TYPE)
    if model_id:
        return model_id
    return "build_1_101_{0}".format(runtime.AGE)


def spawn_single_building(primitives, materials):
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
    spawn_single_building(primitives, materials)
    runtime.spawn_camera(runtime.load_camera_record())
    runtime.configure_world_settings()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError("Failed to save single building map: {0}".format(TARGET_LEVEL))
    unreal.EditorAssetLibrary.save_asset(TARGET_LEVEL, only_if_is_dirty=False)
    unreal.log("Single building diagnostic map saved: {0}".format(TARGET_LEVEL))


if __name__ == "__main__":
    build_single_building_scene()
