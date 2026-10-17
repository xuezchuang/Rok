"""Opt-in editor-only source Ring_07 texture and alpha material; never edits maps or prior assets."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path("D:/ueDemo/Rok")
DEST = "/Game/RokUnityWorldMapPrototype/TileRipple"
TEXTURE_NAME = "T_Ring_07_Source"
MATERIAL_NAME = "M_RokTileRippleSourceAlpha"
SOURCE = ROOT / "ref/client-unity/Assets/BundleAssets/land/Texture/Ring_07.png"
SOURCE_SHA = "1112dbcab4e82c35afb4686153aff77b0ae1397695bc466f99db24ccea7d1237"
META = Path(str(SOURCE) + ".meta")
META_SHA = "d2676b0686754f92d55e4d2b9b39cee0850b6597f4e9bf4e8f9cdd3d4d176a6e"
REPORT = ROOT / "Saved/PendingUnityTileRippleRendererV8/asset_import_manifest.json"


def check_identity() -> None:
    project = Path(unreal.Paths.project_dir()).resolve()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if project != ROOT.resolve() or not world or world.get_name() != "L_RokUnityWorldMapPrototype":
        raise RuntimeError(f"Wrong Editor identity: {project}, {world.get_name() if world else None}")
    for path, expected in ((SOURCE, SOURCE_SHA), (META, META_SHA)):
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise RuntimeError(f"Source asset changed: {path}")


def connect_graph(material, texture=None) -> None:
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)  # Unity Cull Off.
    edit = unreal.MaterialEditingLibrary
    sample = edit.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D)
    sample.set_editor_property("parameter_name", "RingTexture")
    if texture is not None:
        sample.set_editor_property("texture", texture)
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    vertex = edit.create_material_expression(material, unreal.MaterialExpressionVertexColor)
    rgb = edit.create_material_expression(material, unreal.MaterialExpressionMultiply)
    alpha = edit.create_material_expression(material, unreal.MaterialExpressionMultiply)
    connections = (
        ("TextureSample.RGB -> RGBMultiply.A", lambda: edit.connect_material_expressions(sample, "RGB", rgb, "A")),
        ("VertexColor.output0_RGB -> RGBMultiply.B", lambda: edit.connect_material_expressions(vertex, "", rgb, "B")),
        ("TextureSample.A -> AlphaMultiply.A", lambda: edit.connect_material_expressions(sample, "A", alpha, "A")),
        ("VertexColor.A -> AlphaMultiply.B", lambda: edit.connect_material_expressions(vertex, "A", alpha, "B")),
        ("RGBMultiply.output0 -> EmissiveColor", lambda: edit.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)),
        ("AlphaMultiply.output0 -> Opacity", lambda: edit.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)),
    )
    failures = []
    for label, connect in connections:
        try:
            if connect() is not True:
                failures.append(label + ": returned false")
        except Exception as exc:
            failures.append(label + f": {type(exc).__name__}: {exc}")
    if failures:
        raise RuntimeError("Tile ripple material graph connection failed: " + "; ".join(failures))


def main() -> None:
    check_identity()
    for method in ("blueprint_get_size_x", "blueprint_get_size_y"):
        if not callable(getattr(unreal.Texture2D, method, None)):
            raise RuntimeError(f"UE5.8 Texture2D Python API unavailable: {method}")
    # Verify all six graph connections on an unsaved transient material before any Content write.
    transient_material = unreal.new_object(unreal.Material)
    if not isinstance(transient_material, unreal.Material):
        raise RuntimeError("UE5.8 transient Material construction failed")
    connect_graph(transient_material)
    texture_path = DEST + "/" + TEXTURE_NAME
    material_path = DEST + "/" + MATERIAL_NAME
    # Never overwrite an existing asset. The first write must be inspected and accepted by root.
    if unreal.EditorAssetLibrary.does_asset_exist(texture_path) or unreal.EditorAssetLibrary.does_asset_exist(material_path):
        raise RuntimeError("Tile ripple diagnostic assets already exist; inspect instead of overwriting")
    created: list[str] = []
    try:
        unreal.EditorAssetLibrary.make_directory(DEST)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(SOURCE))
        task.set_editor_property("destination_path", DEST)
        task.set_editor_property("destination_name", TEXTURE_NAME)
        task.set_editor_property("factory", unreal.TextureFactory())
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", True)
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        tools.import_asset_tasks([task])
        if unreal.EditorAssetLibrary.does_asset_exist(texture_path):
            created.append(texture_path)
        texture = unreal.load_asset(texture_path)
        if not isinstance(texture, unreal.Texture2D) or texture.blueprint_get_size_x() != 128 or texture.blueprint_get_size_y() != 128:
            raise RuntimeError("Ring_07 source texture import failed")
        texture.set_editor_property("srgb", True)
        if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
            raise RuntimeError("Ring_07 source texture save failed")
        material = tools.create_asset(MATERIAL_NAME, DEST, unreal.Material, unreal.MaterialFactoryNew())
        if unreal.EditorAssetLibrary.does_asset_exist(material_path):
            created.append(material_path)
        if not isinstance(material, unreal.Material):
            raise RuntimeError("Tile ripple alpha material creation failed")
        connect_graph(material, texture)
        unreal.MaterialEditingLibrary.recompile_material(material)
        if not unreal.EditorAssetLibrary.save_loaded_asset(material):
            raise RuntimeError("Tile ripple material save failed")
        packages = []
        for name in (TEXTURE_NAME, MATERIAL_NAME):
            relative = f"Content/RokUnityWorldMapPrototype/TileRipple/{name}.uasset"
            payload = (ROOT / relative).read_bytes()
            packages.append({"path": relative, "sha256": hashlib.sha256(payload).hexdigest(),
                             "byte_count": len(payload)})
        script = ROOT / "Scripts/CreateRokTileRippleDiagnosticAssets.py"
        manifest = {
            "schema": "rok.tile_ripple.diagnostic_assets.v1",
            "project": str(ROOT),
            "map": "/Game/Maps/L_RokUnityWorldMapPrototype",
            "source_png_sha256": SOURCE_SHA,
            "source_meta_sha256": META_SHA,
            "creation_script_sha256": hashlib.sha256(script.read_bytes()).hexdigest(),
            "packages": packages,
        }
        REPORT.parent.mkdir(parents=True, exist_ok=True)
        REPORT.write_text(json.dumps(manifest, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n",
                          encoding="utf-8", newline="\n")
        unreal.log(f"ROK_TILE_RIPPLE_DIAGNOSTIC_ASSETS {texture_path} {material_path} {REPORT}")
    except Exception:
        # Only assets absent at preflight can be removed here; existing content is never deleted.
        for path in reversed(created):
            unreal.EditorAssetLibrary.delete_asset(path)
        raise


if __name__ == "__main__":
    main()
