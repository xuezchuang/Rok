"""Upgrade only the existing opaque mountain A material; never generate a map."""
from pathlib import Path
import unreal

NAME = 'M_WorldSource_build_mountain_A'
RAW_NAME = 'T_WorldSource_build_mountain_A_RawGamma'
CODE = '''float3 g = RawRGB * Lighting;
return float3(g.r <= 0.04045 ? g.r / 12.92 : pow((g.r + 0.055) / 1.055, 2.4),
              g.g <= 0.04045 ? g.g / 12.92 : pow((g.g + 0.055) / 1.055, 2.4),
              g.b <= 0.04045 ? g.b / 12.92 : pow((g.b + 0.055) / 1.055, 2.4));'''


def create(sprite_color, reuse=False):
    import CreateRokUnityWorldMapPrototype as world
    path = world.object_path(world.MATERIAL_DESTINATION, NAME)
    raw_path = world.object_path(world.TEXTURE_DESTINATION, RAW_NAME)
    if reuse:
        material = unreal.load_asset(path)
        raw = unreal.load_asset(raw_path)
        if not isinstance(material, unreal.Material) or not isinstance(raw, unreal.Texture2D) or raw.get_editor_property('srgb'):
            raise RuntimeError('Mountain A raw Gamma assets must be upgraded before reuse')
        return material, path
    source = world.PROJECT_ROOT / 'ref/client-unity/Assets/BundleAssets/land/Texture/build_mountain_A.png'
    raw, _ = world.import_texture(source, RAW_NAME, srgb=False)
    # Preserve source mip availability. Compression differs across engine importers;
    # record actual settings during upgrade rather than claiming pixel identity.
    material = unreal.load_asset(path)
    if not isinstance(material, unreal.Material):
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, world.MATERIAL_DESTINATION, unreal.Material, unreal.MaterialFactoryNew())
    edit = unreal.MaterialEditingLibrary
    edit.delete_all_material_expressions(material)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided', False)
    sample = edit.create_material_expression(material, unreal.MaterialExpressionTextureSample)
    sample.set_editor_property('texture', raw)
    sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    light = edit.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
    light.set_editor_property('parameter_name', world.SPRITE_COLOR_PARAMETER)
    light.set_editor_property('default_value', world.color_record_to_linear(sprite_color))
    shader = edit.create_material_expression(material, unreal.MaterialExpressionCustom)
    shader.set_editor_property('code', CODE)
    shader.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in ('RawRGB', 'Lighting'):
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        inputs.append(entry)
    shader.set_editor_property('inputs', inputs)
    for node, pin, name in ((sample, 'RGB', 'RawRGB'), (light, 'RGB', 'Lighting')):
        if not edit.connect_material_expressions(node, pin, shader, name):
            raise RuntimeError('Mountain A Gamma graph connection failed')
    if not edit.connect_material_property(shader, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Mountain A Gamma emissive connection failed')
    edit.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('Mountain A material save failed')
    return material, path


def main():
    import json
    root = Path(unreal.Paths.project_dir()).resolve()
    level = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if root != Path('D:/ueDemo/Rok').resolve() or level is None or level.get_name() != 'L_RokUnityWorldMapPrototype':
        raise RuntimeError('Wrong mountain upgrade project/world')
    source = json.loads((root/'Saved/RokUnityMigration/unity_village_day_night.json').read_text(encoding='utf-8'))
    day = source['presets'][1]
    values = [day['ambient'][i] + day['directional'][i]*day['intensity']*.75 for i in range(3)]
    material, path = create(dict(zip(('r', 'g', 'b', 'a'), (*values, 1))))
    raw = unreal.load_asset('/Game/RokUnityWorldMapPrototype/Textures/'+RAW_NAME)
    report = {'material': path, 'raw_texture': raw.get_path_name(), 'srgb': raw.get_editor_property('srgb'),
              'mip_gen_settings': str(raw.get_editor_property('mip_gen_settings')),
              'compression_settings': str(raw.get_editor_property('compression_settings')),
              'max_texture_size': raw.get_editor_property('max_texture_size'),
              'scope': 'Only mountain A material and a separate raw texture; map/geometry/other material families unchanged.'}
    (root/'Saved/RokUnityMigration/MountainAGamma').mkdir(exist_ok=True)
    (root/'Saved/RokUnityMigration/MountainAGamma/ue_material.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    unreal.log('ROK_MOUNTAIN_A_GAMMA_MATERIAL_COMPLETE')


if __name__ == '__main__':
    main()
