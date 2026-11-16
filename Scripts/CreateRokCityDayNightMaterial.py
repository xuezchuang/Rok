"""Create the city interior's Unity Gamma lighting post-process material."""
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[1]
FOLDER = "/Game/RokUnity/City/Lighting"
NAME = "M_RokCityDayNightGamma"


def main():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    if Path(unreal.Paths.project_dir()).resolve() != ROOT or world.get_name() != "L_RokUnityGameAppCity":
        raise RuntimeError("Unexpected city lighting asset target")
    path = f"{FOLDER}/{NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_REPLACING_TONEMAPPER)
    edit = unreal.MaterialEditingLibrary
    scene = edit.create_material_expression(material, unreal.MaterialExpressionSceneTexture, -500, -80)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    gain = edit.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, 100)
    gain.set_editor_property("parameter_name", "SceneGammaGain")
    gain.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    shader = edit.create_material_expression(material, unreal.MaterialExpressionCustom, -150, 0)
    shader.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    shader.set_editor_property("code", """
float3 LinearColor = max(SceneRGB.rgb / View.PreExposure, 0.0);
float3 GammaColor = lerp(LinearColor * 12.92,
    1.055 * pow(LinearColor, 1.0 / 2.4) - 0.055,
    step(0.0031308, LinearColor));
return saturate(GammaColor * GammaGain.rgb);
""")
    inputs = []
    for name in ("SceneRGB", "GammaGain"):
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        inputs.append(pin)
    shader.set_editor_property("inputs", inputs)
    if (not edit.connect_material_expressions(scene, "Color", shader, "SceneRGB")
            or not edit.connect_material_expressions(gain, "RGBA", shader, "GammaGain")
            or not edit.connect_material_property(shader, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)):
        raise RuntimeError("Could not connect city day/night material")
    if edit.recompile_material(material) or not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Could not save city day/night material")
    unreal.log(f"ROK_CITY_NIGHT_GAMMA_MATERIAL {path}")


if __name__ == "__main__":
    main()
