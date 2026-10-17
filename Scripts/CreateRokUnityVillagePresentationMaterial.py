"""Create the village deposit day shader; never change a map or existing art.

Source: map_building_with_mask-5.shader, MaskSprite.cs, LightManager.cs.
Unity uses Gamma-space arithmetic. UE scene translucency still blends in linear
space, so matching this material formula does not close visual edge acceptance.
Night lighting comes from cm_city_mask_night.shader. NightLight is the resolved
Color3 * pulse input supplied by an explicit local day/night snapshot; its zero
default preserves the existing day rendering and material asset path.
"""
from pathlib import Path
import unreal


DESTINATION = "/Game/RokUnityWorldMapPrototype/Materials"
NAME = "M_RokVillagePresentationDay"
EXPECTED_WORLD = "L_RokUnityWorldMapPrototype"
DAY_SHADER_CODE = """
float4 col = MainSample * Renderer;
float red2 = MaskSample.r * 2.0;
float team1 = saturate(red2 - 1.0);
float3 gammaColor = col.rgb * ((1.0 - team1 - MaskSample.g)
    + 2.0 * Team1.rgb * team1 + 2.0 * Team2.rgb * MaskSample.g);
gammaColor *= Lighting.rgb;
gammaColor += NightLight.rgb * MaskSample.b;
float3 linearColor = lerp(gammaColor / 12.92,
    pow(max((gammaColor + 0.055) / 1.055, 0.0), 2.4),
    step(0.04045, gammaColor));
float opacity = col.a * saturate(red2) * Transparency;
return float4(linearColor, opacity);
"""


def main():
    root = Path(unreal.Paths.project_dir()).resolve()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if root != Path("D:/ueDemo/Rok").resolve() or world is None or world.get_name() != EXPECTED_WORLD:
        raise RuntimeError(f"Wrong editor identity: {root}, {world.get_name() if world else None}")
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    edit = unreal.MaterialEditingLibrary
    unreal.EditorAssetLibrary.make_directory(DESTINATION)
    # A new raw white default keeps LinearColor sampler validation consistent.
    # Do not alter the engine texture or any imported source art.
    neutral_name = "T_RokVillagePresentationRawDefault"
    neutral = unreal.load_asset(DESTINATION + "/" + neutral_name)
    if neutral is None:
        white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
        if not isinstance(white, unreal.Texture2D):
            raise RuntimeError("Engine white texture unavailable")
        neutral = tools.duplicate_asset(neutral_name, DESTINATION, white)
    if not isinstance(neutral, unreal.Texture2D):
        raise RuntimeError("Unexpected neutral texture asset type")
    neutral.set_editor_property("srgb", False)
    neutral.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    neutral.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    if not unreal.EditorAssetLibrary.save_loaded_asset(neutral):
        raise RuntimeError("Could not save raw parameter default")
    material = unreal.load_asset(DESTINATION + "/" + NAME)
    if material is None:
        material = tools.create_asset(NAME, DESTINATION, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(material, unreal.Material):
        raise RuntimeError("Unexpected village material asset type")
    edit.delete_all_material_expressions(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)

    def node(kind):
        return edit.create_material_expression(material, kind)

    def connect(source, output, target, input_name):
        if not edit.connect_material_expressions(source, output, target, input_name):
            raise RuntimeError(f"Failed connection {output} -> {input_name}")

    def vector(name, values):
        result = node(unreal.MaterialExpressionVectorParameter)
        result.set_editor_property("parameter_name", name)
        result.set_editor_property("default_value", unreal.LinearColor(*values))
        return result

    samples = []
    for parameter in ("Sprite", "Mask"):
        sample = node(unreal.MaterialExpressionTextureSampleParameter2D)
        sample.set_editor_property("parameter_name", parameter)
        sample.set_editor_property("texture", neutral)
        sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        samples.append(sample)
    color1 = vector("Color1", (0, 0.254902, 1, 1))
    color2 = vector("Color2", (0.772549, 0.3254902, 0.2392157, 1))
    renderer = vector("RendererColor", (1, 1, 1, 1))
    lighting = vector("SpriteColor", (1.07203974, 1.07203974, 1.07203974, 1))
    night_light = vector("NightLight", (0, 0, 0, 0))
    transparency = node(unreal.MaterialExpressionScalarParameter)
    transparency.set_editor_property("parameter_name", "CityTransparency")
    transparency.set_editor_property("default_value", 1)
    shader = node(unreal.MaterialExpressionCustom)
    shader.set_editor_property("description", "Unity village day/night Gamma formula then linear emissive")
    shader.set_editor_property("code", DAY_SHADER_CODE)
    shader.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    names = ("MainSample", "MaskSample", "Team1", "Team2", "Renderer", "Lighting", "Transparency", "NightLight")
    inputs = []
    for name in names:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        inputs.append(custom_input)
    shader.set_editor_property("inputs", inputs)
    # Explicit RGBA matters: VectorParameter's first output is RGB only.
    for source, output, name in zip(
        (*samples, color1, color2, renderer, lighting, transparency, night_light),
        ("RGBA", "RGBA", "RGBA", "RGBA", "RGBA", "RGBA", "", "RGBA"), names
    ):
        connect(source, output, shader, name)
    rgb = node(unreal.MaterialExpressionComponentMask)
    alpha = node(unreal.MaterialExpressionComponentMask)
    for channel in ("r", "g", "b"):
        rgb.set_editor_property(channel, True)
    rgb.set_editor_property("a", False)
    for channel in ("r", "g", "b"):
        alpha.set_editor_property(channel, False)
    alpha.set_editor_property("a", True)
    # MaterialEditingLibrary uses shortened UI pin names; its empty input
    # name explicitly selects GetInput(0), the ComponentMask expression input.
    connect(shader, "", rgb, "")
    connect(shader, "", alpha, "")
    if not edit.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Village emissive binding failed")
    if not edit.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError("Village opacity binding failed")
    edit.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Village material save failed")
    unreal.log("ROK_VILLAGE_DAY_MATERIAL " + DESTINATION + "/" + NAME)


if __name__ == "__main__":
    main()
