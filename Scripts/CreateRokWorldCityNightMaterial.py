"""Create world map city and ground materials for the Unity day/night cycle."""
from pathlib import Path

import unreal


def create_ground_material(folder):
    name = "M_RokWorldGroundGammaLighting"
    path = f"{folder}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return
    texture = unreal.load_asset("/Game/RokUnityWorldMapPrototype/Textures/T_WorldGround_I_TYPE_SceneMap_mask_1")
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    edit = unreal.MaterialEditingLibrary
    sample = edit.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -450, 0)
    sample.set_editor_property("parameter_name", "GroundTexture")
    sample.set_editor_property("texture", texture)
    color = edit.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -450, 180)
    color.set_editor_property("parameter_name", "UnitySpriteColor")
    color.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    shader = edit.create_material_expression(material, unreal.MaterialExpressionCustom, -180, 0)
    shader.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    shader.set_editor_property("code", """
float3 gamma = lerp(Ground.rgb * 12.92, 1.055 * pow(max(Ground.rgb, 0.0), 1.0 / 2.4) - 0.055,
    step(0.0031308, Ground.rgb)) * Lighting.rgb;
return lerp(gamma / 12.92, pow(max((gamma + 0.055) / 1.055, 0.0), 2.4), step(0.04045, gamma));
""")
    pins = []
    for name in ("Ground", "Lighting"):
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    shader.set_editor_property("inputs", pins)
    if (not edit.connect_material_expressions(sample, "RGB", shader, "Ground")
            or not edit.connect_material_expressions(color, "RGB", shader, "Lighting")
            or not edit.connect_material_property(shader, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            or not edit.connect_material_property(sample, "A", unreal.MaterialProperty.MP_OPACITY)):
        raise RuntimeError("Could not bind Unity world ground lighting")
    if edit.recompile_material(material) or not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Could not save Unity world ground lighting")
    unreal.log(f"ROK_WORLD_GROUND_GAMMA_MATERIAL {path}")


def main():
    root = Path(__file__).resolve().parents[1]
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if Path(unreal.Paths.project_dir()).resolve() != root or world.get_name() != "L_RokUnityWorldMapPrototype":
        raise RuntimeError("Unexpected world city material target")

    name = "M_WorldPlayerCity_build_1_101_2"
    folder = "/Game/RokUnityWorldMapPrototype/Materials"
    create_ground_material(folder)
    path = f"{folder}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return
    base = unreal.load_asset("/Game/RokUnity/City/SourceBuildings/build_1_101_2/M_build_1_101_2_SourceRG")
    material = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(name, folder, base)
    edit = unreal.MaterialEditingLibrary
    shader, = [node for node in edit.get_material_expressions(material)
               if isinstance(node, unreal.MaterialExpressionCustom)]
    # Unity cm_city_mask_night.shader adds the blue mask channel in Gamma.
    code = shader.get_editor_property("code")
    marker = "gammaColor *= Lighting.rgb;"
    if code.count(marker) != 1:
        raise RuntimeError("City hall source material no longer matches the RG shader")
    shader.set_editor_property("code", code.replace(marker, marker + "\ngammaColor += NightLight.rgb * MaskSample.b;"))
    shader.set_editor_property("description", "Unity cm_city_mask_night RGB mask in Gamma")
    input_pin = unreal.CustomInput()
    input_pin.set_editor_property("input_name", "NightLight")
    shader.set_editor_property("inputs", [*shader.get_editor_property("inputs"), input_pin])
    night = edit.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -380, 300)
    night.set_editor_property("parameter_name", "NightLight")
    night.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 1))
    if not edit.connect_material_expressions(night, "RGBA", shader, "NightLight"):
        raise RuntimeError("Could not bind world city window lighting")
    if edit.recompile_material(material) or not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Could not save the world city night material")
    unreal.log(f"ROK_WORLD_CITY_NIGHT_MATERIAL {path}")


if __name__ == "__main__":
    main()
