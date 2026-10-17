"""Material-only generator; does not open, replace, or save a level."""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
if root != Path("D:/ueDemo/Rok").resolve():
    raise RuntimeError(f"Wrong project: {root}")
destination = "/Game/RokUnityWorldMapPrototype/Materials"
name = "M_RokWorldCityPulse"
path = destination + "/" + name
unreal.EditorAssetLibrary.make_directory(destination)
material = unreal.load_asset(path)
if material is None:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, destination, unreal.Material, unreal.MaterialFactoryNew())
unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
material.set_editor_property("two_sided", True)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
edit = unreal.MaterialEditingLibrary
texture = edit.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D)
texture.set_editor_property("parameter_name", "Sprite")
# A default texture is necessary for material compilation; runtime binds circle006.
texture.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
tint = edit.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
tint.set_editor_property("parameter_name", "Tint")
opacity = edit.create_material_expression(material, unreal.MaterialExpressionScalarParameter)
opacity.set_editor_property("parameter_name", "ParticleAlpha")
rgb = edit.create_material_expression(material, unreal.MaterialExpressionMultiply)
alpha = edit.create_material_expression(material, unreal.MaterialExpressionMultiply)
edit.connect_material_expressions(texture, "RGB", rgb, "A")
edit.connect_material_expressions(tint, "RGB", rgb, "B")
edit.connect_material_expressions(texture, "A", alpha, "A")
edit.connect_material_expressions(opacity, "", alpha, "B")
edit.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
edit.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
unreal.log("ROK_CITY_PULSE_MATERIAL " + path)
