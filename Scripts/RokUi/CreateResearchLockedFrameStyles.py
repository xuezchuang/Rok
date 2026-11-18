"""Create only the locked Research frame MI and detached Designer template.

After preparing the spec, run this script with UE 5.8 UnrealEditor-Cmd:
  Rok.uproject -run=pythonscript -script=<absolute path to this script>
  -NoCompile -NoLiveCoding -NoP4 -DisablePlugins=ModelContextProtocol
  -NullRHI -Unattended -NoSplash -NoSound
The caller must launch hidden. If the material instance already matches the
spec, reuse it so a failed WBP generation can be retried without overwriting it.
"""
import hashlib
import json
from pathlib import Path
import sys

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from BuildResearchLockedFrameStylesSpec import (  # noqa: E402
    ROOT, ASSET, MATERIAL, BASE, ATLAS, ROOT_NAME, IMAGE_NAME, OUTPUT, KEY, build)


def asset_file(asset):
    return ROOT / "Content" / (asset.removeprefix("/Game/") + ".uasset")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    project = Path(unreal.Paths.project_dir()).resolve()
    world = unreal.EditorLevelLibrary.get_editor_world()
    unreal.log(f"ROK_RESEARCH_LOCKED_FRAME_IDENTITY project={project} world={world.get_name() if world else '<none>'}")
    if project != ROOT.resolve() or not (project / "Rok.uproject").is_file():
        raise RuntimeError(f"Wrong Rok target: {project}")
    if not unreal.SystemLibrary.get_engine_version().startswith("5.8"):
        raise RuntimeError("Research frame generation requires UE 5.8")
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET) or asset_file(ASSET).exists():
        raise RuntimeError(f"Create-only asset already exists: {ASSET}")
    if json.loads(OUTPUT.read_text(encoding="utf-8-sig")) != build():
        raise RuntimeError(f"Regenerate the locked frame spec: {OUTPUT}")
    originals = [BASE, ATLAS, "/Game/RokPrototype/UI/Research/WBP_RokResearchNode",
                 "/Game/RokPrototype/UI/Research/WBP_RokResearchTree"]
    before = {path: sha(asset_file(path)) for path in originals}
    base = unreal.load_asset(BASE)
    atlas = unreal.load_asset(ATLAS)
    if not isinstance(base, unreal.Material) or base.get_editor_property("material_domain") != unreal.MaterialDomain.MD_UI:
        raise RuntimeError("Existing source-gray UI material is missing or unsupported")
    if not isinstance(atlas, unreal.Texture2D) or not atlas.get_editor_property("srgb"):
        raise RuntimeError("Expected existing sRGB research atlas")
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL) or asset_file(MATERIAL).exists():
        instance = unreal.load_asset(MATERIAL)
    else:
        folder, name = MATERIAL.rsplit("/", 1)
        unreal.EditorAssetLibrary.make_directory(folder)
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(instance, unreal.MaterialInstanceConstant):
            raise RuntimeError("Locked frame material instance creation failed")
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance, base)
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(instance, "SourceAtlas", atlas)
        if not unreal.EditorAssetLibrary.save_loaded_asset(instance):
            raise RuntimeError("Locked frame material instance save failed")
    if not isinstance(instance, unreal.MaterialInstanceConstant) or instance.get_editor_property("parent") != base:
        raise RuntimeError("Locked frame material instance has the wrong parent")
    if unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(instance, "SourceAtlas") != atlas:
        raise RuntimeError("Locked frame instance SourceAtlas override missing")
    command = (f"Rok.UiWbp.GeneratePage {ASSET} {KEY} "
               f"-ParentClass=/Script/Rok.RokUiPageWidget -RootName={ROOT_NAME}")
    unreal.SystemLibrary.execute_console_command(world, command)
    blueprint = unreal.load_asset(ASSET)
    if not isinstance(blueprint, unreal.WidgetBlueprint) or not blueprint.generated_class():
        raise RuntimeError("Locked frame WBP generation failed")
    for outer in (blueprint, blueprint.generated_class()):
        tree = unreal.find_object(outer, "WidgetTree")
        image = unreal.find_object(tree, IMAGE_NAME) if tree else None
        if not isinstance(image, unreal.Image):
            raise RuntimeError("Locked frame template image is missing")
        brush = image.get_editor_property("brush")
        if brush.get_editor_property("resource_object") != instance:
            raise RuntimeError("Locked frame template must reference the persistent grayscale instance")
    if before != {path: sha(asset_file(path)) for path in originals}:
        raise RuntimeError("An original Research asset changed during create-only generation")
    receipt = {"project": str(project), "world": world.get_name() if world else None,
               "engine": unreal.SystemLibrary.get_engine_version(), "asset": ASSET,
               "material": MATERIAL, "parent": BASE, "atlas": ATLAS,
               "unchanged_originals": before, "template_trees": 2, "visual_review": False}
    path = ROOT / "Saved/RokUi/Logs/research_locked_frame_styles_receipt.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    unreal.log("ROK_RESEARCH_LOCKED_FRAME_CREATED " + json.dumps(receipt))


main()
