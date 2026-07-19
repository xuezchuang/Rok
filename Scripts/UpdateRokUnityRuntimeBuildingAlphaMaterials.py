import importlib
import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

ASSET_SUFFIX = os.environ.get("ROK_RUNTIME_CITY_ALL_BUILDINGS_ASSET_SUFFIX", "GameAppRuntimeV6Color")
EXTRA_BUILDING_COUNT = max(0, int(os.environ.get("ROK_RUNTIME_EXTRA_INTERIOR_BUILDINGS", "24")))
EXTRA_BUILDING_START = max(0, int(os.environ.get("ROK_RUNTIME_EXTRA_INTERIOR_BUILDING_START", "0")))


def update_materials():
    os.environ["ROK_RUNTIME_CITY_ALL_BUILDINGS_ASSET_SUFFIX"] = ASSET_SUFFIX
    scene = importlib.import_module("CreateRokUnityRuntimeCityAllBuildingsScene")
    scene = importlib.reload(scene)

    entries = []
    end_index = EXTRA_BUILDING_START + EXTRA_BUILDING_COUNT
    for index, (_building_type, model_id, _grid_x, _grid_y) in enumerate(
        scene.EXTRA_INTERIOR_LAYOUT[EXTRA_BUILDING_START:end_index],
        start=EXTRA_BUILDING_START,
    ):
        safe_model = scene.fresh.safe_tag(model_id)
        baked_path = os.path.join(scene.INTERIOR_BAKED_TEXTURE_ROOT, "{0}_unity_alpha.png".format(model_id))
        if not os.path.exists(baked_path):
            raise RuntimeError(
                "Missing baked Unity building alpha texture; run "
                "Scripts/PrepareRokUnityRuntimeInteriorBuildingTextures.py: {0}".format(baked_path)
            )
        baked_texture = scene.import_building_texture(
            baked_path,
            "T_RokUnityInteriorPreview_{0}_BakedAlpha".format(safe_model),
        )
        entries.append((index, safe_model, baked_texture))

    updated = []
    for index, safe_model, baked_texture in entries:
        material_name = "M_RokUnityInteriorPreview_{0}_{1}_RQ2995".format(safe_model, index + 1)
        material = scene.create_building_material(
            baked_texture,
            material_name,
        )
        compile_errors = [str(item) for item in unreal.MaterialEditingLibrary.recompile_material(material)]
        if compile_errors:
            raise RuntimeError("Material compile failed for {0}: {1}".format(material_name, compile_errors))
        updated.append(material.get_path_name())

    test_directory = "/Game/RokUnityRuntimeCityAllBuildings_AlphaFormulaTest"
    test_map = "/Game/Maps/L_RokUnityGameAppCity_AlphaTest"
    if unreal.EditorAssetLibrary.does_directory_exist(test_directory):
        unreal.EditorAssetLibrary.delete_directory(test_directory)
    if unreal.EditorAssetLibrary.does_asset_exist(test_map):
        unreal.EditorAssetLibrary.delete_asset(test_map)
    unreal.log("Updated Rok Unity building alpha materials: {0}".format(len(updated)))
    print("materials_updated={0}".format(len(updated)))


if __name__ == "__main__":
    update_materials()
