"""Set Expedition map textures to lossless BGRA8 with no mip chain."""

from pathlib import Path
import unreal


EXPECTED_ROOT = Path(r"D:\ue\Rok").resolve()
PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
ASSET_ROOT = "/Game/RokPrototype/UI/ExpeditionSource/Art"
SOURCE_ROOT = PROJECT_ROOT / "Content/RokData/Expedition"
TEXTURES = (
    "img_bg_battle_map1.jpg",
    "img_bg_battle_map2.jpg",
    "img_bg_battle_map3.jpg",
    "selected_glow.png",
    "img_bat_first.png",
)


def main():
    if PROJECT_ROOT != EXPECTED_ROOT:
        raise RuntimeError(f"Wrong project identity: {PROJECT_ROOT}; expected {EXPECTED_ROOT}")
    if not (PROJECT_ROOT / "Rok.uproject").is_file():
        raise RuntimeError(f"Rok.uproject missing at {PROJECT_ROOT}")

    for source_name in TEXTURES:
        source = SOURCE_ROOT / source_name
        if not source.is_file():
            raise RuntimeError(f"Missing source map texture: {source}")
        asset_name = Path(source_name).stem
        asset_path = f"{ASSET_ROOT}/{asset_name}"
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(asset, unreal.Texture2D):
            raise RuntimeError(f"Expected existing Expedition Texture2D: {asset_path}")

        asset.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        asset.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        asset.set_editor_property("srgb", True)
        if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save configured Expedition texture: {asset_path}")

        unreal.log(
            f"Expedition texture configured: {asset_path}; "
            "compression=TC_EditorIcon mip_gen=TMGS_NoMipmaps srgb=True"
        )


main()
