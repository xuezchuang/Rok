"""Import a texture-page spec's PNGs and create its WBP with UE 5.8.

Set ROK_UI_TEXTURE_PAGE_SPEC to the generated spec key and run this script with
UnrealEditor-Cmd -run=pythonscript -NullRHI -NoCompile -NoLiveCoding -NoP4.
"""

import json
import os
from pathlib import Path
import re
import struct

import unreal


PACKAGE = re.compile(r"/Game/RokUI/(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+")
TEXTURE_SETTINGS = {
    "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
    "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
    "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
    "srgb": True,
    "never_stream": True,
}


def package_file(project, package):
    if not PACKAGE.fullmatch(package):
        raise ValueError(f"Expected a /Game/RokUI package: {package}")
    return project / "Content" / (package.removeprefix("/Game/") + ".uasset")


def import_texture(project, entry):
    source = (project / entry["source"]).resolve()
    package = entry["asset"]
    size = tuple(entry["size"])
    output = package_file(project, package)
    if not source.is_relative_to(project / "Content/RokData") or source.suffix.lower() != ".png":
        raise ValueError(f"Expected a PNG in Content/RokData: {source}")
    with source.open("rb") as stream:
        header = stream.read(24)
    if header[:8] != b"\x89PNG\r\n\x1a\n" or struct.unpack(">II", header[16:24]) != size:
        raise ValueError(f"Unexpected PNG dimensions: {source}")

    if output.exists() or unreal.EditorAssetLibrary.does_asset_exist(package):
        texture = unreal.EditorAssetLibrary.load_asset(package)
    else:
        folder, name = package.rsplit("/", 1)
        factory = unreal.TextureFactory()
        task = unreal.AssetImportTask()
        for key, value in {
            "filename": str(source),
            "destination_path": folder,
            "destination_name": name,
            "factory": factory,
            "automated": True,
            "replace_existing": False,
            "save": False,
        }.items():
            task.set_editor_property(key, value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(package)
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(f"Texture import failed: {package}")
        for key, value in TEXTURE_SETTINGS.items():
            texture.set_editor_property(key, value)
        if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
            raise RuntimeError(f"Texture save failed: {package}")

    if not output.is_file() or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Expected a saved Texture2D: {package}")
    if (texture.blueprint_get_size_x(), texture.blueprint_get_size_y()) != size:
        raise RuntimeError(f"Unexpected imported texture dimensions: {package}")
    if any(texture.get_editor_property(key) != value for key, value in TEXTURE_SETTINGS.items()):
        raise RuntimeError(f"Unexpected texture settings: {package}")


def main():
    project = Path(unreal.Paths.project_dir()).resolve()
    expected = Path(__file__).resolve().parents[2]
    world = unreal.EditorLevelLibrary.get_editor_world()
    unreal.log(f"ROK_UI_TEXTURE_PAGE_IDENTITY project={project} world={world.get_name() if world else '<none>'}")
    if project != expected or not (project / "Rok.uproject").is_file():
        raise RuntimeError(f"Wrong Rok project: {project}")
    if not unreal.SystemLibrary.get_engine_version().startswith("5.8"):
        raise RuntimeError("Texture page generation requires UE 5.8")
    key = os.environ["ROK_UI_TEXTURE_PAGE_SPEC"]
    if not re.fullmatch(r"[a-z0-9_]+", key):
        raise ValueError(f"Invalid spec key: {key}")
    spec = json.loads((project / "Saved/RokUi/Specs" / f"{key}.json").read_text(encoding="utf-8-sig"))
    package = spec["asset"]
    root = spec["root"]
    if not re.fullmatch(r"[A-Za-z0-9_]+", root):
        raise ValueError(f"Invalid root: {root}")
    output = package_file(project, package)
    if output.exists() or unreal.EditorAssetLibrary.does_asset_exist(package):
        raise RuntimeError(f"Create-only WBP already exists: {package}")
    for entry in spec["texture_imports"]:
        import_texture(project, entry)

    command = f"Rok.UiWbp.GeneratePage {package} {key} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={root}"
    unreal.SystemLibrary.execute_console_command(world, command)
    blueprint = unreal.EditorAssetLibrary.load_asset(package)
    if not output.is_file() or not isinstance(blueprint, unreal.WidgetBlueprint) or not blueprint.generated_class():
        raise RuntimeError(f"WBP creation failed: {package}")
    unreal.log(f"ROK_UI_TEXTURE_PAGE_VERIFIED {package} textures={len(spec['texture_imports'])}")


main()
