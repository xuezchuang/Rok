import os

import unreal


DESTINATION_PATH = "/Game/RokPrototype/UI/Processed"
SOURCE_ROOT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Saved", "RokUiProcessed")

TEXTURES = [
    ("rok_main_top.png", "rok_main_top"),
    ("rok_main_bottom.png", "rok_main_bottom"),
    ("rok_food_icon.png", "rok_food_icon"),
    ("rok_wood_icon.png", "rok_wood_icon"),
    ("rok_stone_icon.png", "rok_stone_icon"),
    ("rok_gold_icon.png", "rok_gold_icon"),
    ("rok_upgrade_icon.png", "rok_upgrade_icon"),
]

BUILDING_MENU_TEXTURES = [
    ("rok_building_menu_title.png", "rok_building_menu_title"),
    ("rok_building_menu_level_bg.png", "rok_building_menu_level_bg"),
    ("rok_building_menu_info.png", "rok_building_menu_info"),
    ("rok_building_menu_upgrade.png", "rok_building_menu_upgrade"),
    ("rok_building_menu_feature.png", "rok_building_menu_feature"),
    ("rok_building_menu_remove.png", "rok_building_menu_remove"),
    ("rok_building_menu_button_shadow.png", "rok_building_menu_button_shadow"),
    ("rok_building_select_arrow_down.png", "rok_building_select_arrow_down"),
    ("rok_building_select_arrow_left.png", "rok_building_select_arrow_left"),
    ("rok_building_select_arrow_right.png", "rok_building_select_arrow_right"),
    ("rok_building_select_arrow_up.png", "rok_building_select_arrow_up"),
]


def build_import_task(source_file, destination_name, destination_path=DESTINATION_PATH):
    task = unreal.AssetImportTask()
    task.filename = source_file
    task.destination_path = destination_path
    task.destination_name = destination_name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.factory = unreal.TextureFactory()
    return task


def configure_texture(asset_path):
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not texture:
        return

    try:
        texture.set_editor_property("sRGB", True)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    except Exception as exc:
        unreal.log_warning("Rok UI import: texture UI settings skipped for {0}: {1}".format(asset_path, exc))
    unreal.EditorAssetLibrary.save_loaded_asset(texture)


def main():
    unreal.EditorAssetLibrary.make_directory(DESTINATION_PATH)
    building_menu_destination = "/Game/RokPrototype/UI/BuildingMenu"
    unreal.EditorAssetLibrary.make_directory(building_menu_destination)

    tasks = []
    missing = []
    imports = [(relative_path, destination_name, DESTINATION_PATH) for relative_path, destination_name in TEXTURES]
    imports.extend(
        (relative_path, destination_name, building_menu_destination)
        for relative_path, destination_name in BUILDING_MENU_TEXTURES
    )
    for relative_path, destination_name, destination_path in imports:
        source_file = os.path.join(SOURCE_ROOT, *relative_path.split("/"))
        if not os.path.exists(source_file):
            missing.append(source_file)
            continue
        tasks.append(build_import_task(source_file, destination_name, destination_path))

    if missing:
        raise RuntimeError("Missing Rok UI source textures: {0}".format(", ".join(missing)))
    if not tasks:
        raise RuntimeError("No Rok UI texture import tasks were created.")

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    imported_assets = []
    for _, destination_name, destination_path in imports:
        asset_path = "{0}/{1}".format(destination_path, destination_name)
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            raise RuntimeError("Rok UI import failed for {0}".format(asset_path))
        configure_texture(asset_path)
        imported_assets.append(asset_path)

    unreal.log("Rok UI processed texture import complete: {0}".format(", ".join(imported_assets)))


if __name__ == "__main__":
    main()
