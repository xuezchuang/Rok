"""Reproducible material-only UI_10010 slice; never changes a map or Blueprint.

Unity 2019.4.28f1 Mobile/Mobile-Particle-Add.shader uses Blend SrcAlpha One,
ZWrite Off, Cull Off and texture * primary. Native Unity probe confirms the
reference materials resolve to this shader (builtin fileID 10720).
"""
from pathlib import Path
import json
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
if root != Path("D:/ueDemo/Rok").resolve():
    raise RuntimeError(f"Wrong project: {root}")
probe = json.loads((root / "Saved/RokUnityMigration/power_effect_shader_probe.json").read_text())
if len(probe["materials"]) != 2 or any(
    item["shader"] != "Mobile/Particles/Additive" or item["shaderFileId"] != 10720
    for item in probe["materials"]
):
    raise RuntimeError("Run ProbeRokUnityPowerEffect in the source Unity version first")

destination = "/Game/RokPrototype/UI/Materials"
name = "M_RokPowerGainUI"
path = destination + "/" + name
unreal.EditorAssetLibrary.make_directory(destination)
material = unreal.load_asset(path)
if material is None:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, destination, unreal.Material, unreal.MaterialFactoryNew())
if not isinstance(material, unreal.Material):
    raise RuntimeError(f"Unexpected asset type: {path}")
edit = unreal.MaterialEditingLibrary
edit.delete_all_material_expressions(material)
material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
material.set_editor_property("two_sided", True)

def node(kind):
    return edit.create_material_expression(material, kind)

def connect(source, output, target, input_name):
    if not edit.connect_material_expressions(source, output, target, input_name):
        raise RuntimeError(f"Material connection failed: {source.get_name()}.{output} -> {target.get_name()}.{input_name}")

uv = node(unreal.MaterialExpressionTextureCoordinate)
tiling = node(unreal.MaterialExpressionScalarParameter)
tiling.set_editor_property("parameter_name", "UTiling")
tiling.set_editor_property("default_value", 1.0)
one = node(unreal.MaterialExpressionConstant)
one.set_editor_property("r", 1.0)
scale = node(unreal.MaterialExpressionAppendVector)
connect(tiling, "", scale, "A")
connect(one, "", scale, "B")
scaled_uv = node(unreal.MaterialExpressionMultiply)
connect(uv, "", scaled_uv, "A")
connect(scale, "", scaled_uv, "B")
texture = node(unreal.MaterialExpressionTextureSampleParameter2D)
texture.set_editor_property("parameter_name", "Sprite")
texture.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
connect(scaled_uv, "", texture, "UVs")
vertex = node(unreal.MaterialExpressionVertexColor)
rgb = node(unreal.MaterialExpressionMultiply)
alpha = node(unreal.MaterialExpressionMultiply)
connect(texture, "RGB", rgb, "A")
# VertexColor's combined RGB output is unnamed in UE; "RGB" silently fails.
connect(vertex, "", rgb, "B")
connect(texture, "A", alpha, "A")
connect(vertex, "A", alpha, "B")
if not edit.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError("Could not bind UI final color")
if not edit.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY):
    raise RuntimeError("Could not bind UI opacity")
# UE 5.8's Substrate Slate shader reads FrontMaterial, not the legacy color
# pins. Rebuilding an already upgraded material must rebuild this root too.
ui = node(unreal.MaterialExpressionSubstrateUI)
connect(rgb, "", ui, "Color")
connect(alpha, "", ui, "Opacity")
if not edit.connect_material_property(ui, "", unreal.MaterialProperty.MP_FRONT_MATERIAL):
    raise RuntimeError("Could not bind Substrate UI front material")
edit.recompile_material(material)
if not unreal.EditorAssetLibrary.save_loaded_asset(material):
    raise RuntimeError("Power gain UI material save failed")
assert material.get_editor_property("material_domain") == unreal.MaterialDomain.MD_UI
assert material.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_ADDITIVE
assert edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_FRONT_MATERIAL) == ui
unreal.log("ROK_POWER_GAIN_UI_MATERIAL " + path + " domain=UI blend=Additive parameters=Sprite,UTiling")
