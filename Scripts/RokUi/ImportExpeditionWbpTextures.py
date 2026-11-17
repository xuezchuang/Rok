"""Import the Unity-derived Expedition PNG/JPEG art for WBP image brushes."""

from pathlib import Path
import unreal


EXPECTED_ROOT = Path(r"D:\ue\Rok").resolve()
PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_ROOT = PROJECT_ROOT / "Content/RokData/Expedition"
DESTINATION = "/Game/RokPrototype/UI/ExpeditionSource/Art"
ASSETS = (
    "btn_bat_back.png", "btn_bat_frame.png", "btn_bat_rank.png", "btn_bat_rule.png",
    "btn_bat_shop.png", "hero_background_14.png", "icon_com_star1.png", "icon_com_star2.png",
    "img_bat_box.png", "img_bat_first.png", "img_bat_flag1.png", "img_bat_flag2.png",
    "img_bat_flag4.png", "img_bat_flagfra.png", "img_bat_flagfra2.png", "img_bat_queue1.png",
    "img_bat_queue2.png", "img_bat_trop3.png", "img_bat_trop4.png", "img_bg_battle_frame1.png",
    "img_bg_battle_map1.jpg", "img_bg_battle_map2.jpg", "img_bg_battle_map3.jpg", "img_com_cur8.png",
    "selected_glow.png",
)


def import_texture(source: Path):
    asset_name = source.stem
    asset_path = f"{DESTINATION}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(asset, unreal.Texture2D):
            raise RuntimeError(f"Existing art path is not a Texture2D: {asset_path}")
        return asset

    task = unreal.AssetImportTask()
    for key, value in {
        "filename": str(source),
        "destination_path": DESTINATION,
        "destination_name": asset_name,
        "automated": True,
        "save": True,
        "replace_existing": False,
        "factory": unreal.TextureFactory(),
    }.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(asset, unreal.Texture2D):
        raise RuntimeError(f"Texture import failed: {source} -> {asset_path}")
    return asset


def main():
    if PROJECT_ROOT != EXPECTED_ROOT:
        raise RuntimeError(f"Wrong project identity: {PROJECT_ROOT}; expected {EXPECTED_ROOT}")
    if not (PROJECT_ROOT / "Rok.uproject").is_file():
        raise RuntimeError(f"Rok.uproject missing at {PROJECT_ROOT}")
    for name in ASSETS:
        source = SOURCE_ROOT / name
        if not source.is_file():
            raise RuntimeError(f"Missing generated source artwork: {source}")
    unreal.EditorAssetLibrary.make_directory(DESTINATION)
    for name in ASSETS:
        import_texture(SOURCE_ROOT / name)
    if not all(unreal.EditorAssetLibrary.does_asset_exist(f"{DESTINATION}/{Path(name).stem}")
               for name in ASSETS):
        raise RuntimeError("One or more Expedition textures did not persist")
    unreal.log(f"Imported/verified {len(ASSETS)} Expedition WBP textures in {DESTINATION}")


main()
