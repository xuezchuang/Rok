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


def build_import_task(source_file, destination_name):
    task = unreal.AssetImportTask()
    task.filename = source_file
    task.destination_path = DESTINATION_PATH
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

    tasks = []
    missing = []
    for relative_path, destination_name in TEXTURES:
        source_file = os.path.join(SOURCE_ROOT, *relative_path.split("/"))
        if not os.path.exists(source_file):
            missing.append(source_file)
            continue
        tasks.append(build_import_task(source_file, destination_name))

    if missing:
        raise RuntimeError("Missing Rok UI source textures: {0}".format(", ".join(missing)))
    if not tasks:
        raise RuntimeError("No Rok UI texture import tasks were created.")

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    imported_assets = []
    for _, destination_name in TEXTURES:
        asset_path = "{0}/{1}".format(DESTINATION_PATH, destination_name)
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            raise RuntimeError("Rok UI import failed for {0}".format(asset_path))
        configure_texture(asset_path)
        imported_assets.append(asset_path)

    unreal.log("Rok UI processed texture import complete: {0}".format(", ".join(imported_assets)))


if __name__ == "__main__":
    main()
