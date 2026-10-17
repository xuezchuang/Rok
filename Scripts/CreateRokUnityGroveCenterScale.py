"""Repeatable post-step after world generation. Run only in the exact editor map."""
import hashlib
import json
from pathlib import Path
import unreal

BASE = '/Game/RokUnityWorldMapPrototype/'
MESH = BASE+'Meshes/grove/StaticMeshes/SM_RokGroveCenterScale'
MATERIAL = BASE+'Materials/M_WorldSource_GroveCenterScale'
TAG = 'RokGroveCenterScaleV1'
PREFAB = 'Grove_co_tree_01_adorning'

def main():
    root = Path(unreal.Paths.project_dir()).resolve()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if root != Path('D:/ueDemo/Rok').resolve() or not world or world.get_name() != 'L_RokUnityWorldMapPrototype':
        raise RuntimeError('Grove upgrade requires exact Rok project and world map')
    unreal.log('ROK_GROVE_CENTER_IDENTITY '+str(root)+' '+world.get_name())
    out = root/'Saved/RokUnityMigration/GroveCenterScale'
    data = json.loads((out/'manifest.json').read_text())
    transport = json.loads((out/'gltf_verify.json').read_text())
    for filename, key in (('manifest.json','input_sha256'),('grove.gltf','gltf_sha256'),('grove.bin','bin_sha256')):
        if hashlib.sha256((out/filename).read_bytes()).hexdigest()!=transport[key]:
            raise RuntimeError('Stale or modified Grove transport: '+filename)
    for item in data['source_files']:
        assert hashlib.sha256((root/item['path']).read_bytes()).hexdigest() == item['sha256']
    actors = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
              if 'UnityPrefab_'+PREFAB in map(str, a.tags)]
    expected = {f"RokWorldObject_Tile{i['tile'][0]}_{i['tile'][1]}_Object{i['source_instance']['template_index']:02d}_{PREFAB}" for i in data['instances']}
    if len(actors) != 11 or {a.get_actor_label() for a in actors} != expected:
        raise RuntimeError('Exact source 11-instance identity mismatch')
    components = [a.get_component_by_class(unreal.StaticMeshComponent) for a in actors]
    assert all(components)
    def transform(a):
        p,r,s=a.get_actor_location(),a.get_actor_rotation(),a.get_actor_scale3d()
        return [p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z]
    before = [transform(a) for a in actors]
    # Read the current source material rather than changing shared textures or Gamma.
    original = unreal.load_asset(BASE+'Materials/M_WorldSource_map_tree_nomask_1')
    if not isinstance(original, unreal.Material):
        raise RuntimeError('Expected existing map_tree_nomask material')
    edit = unreal.MaterialEditingLibrary
    samples = [n for n in edit.get_material_expressions(original) if isinstance(n, unreal.MaterialExpressionTextureSample)]
    if len(samples) != 1:
        raise RuntimeError('Existing source tree sampling graph changed')
    texture = samples[0].get_editor_property('texture')
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(out/'grove.gltf'))
    task.set_editor_property('destination_path', BASE+'Meshes')
    task.set_editor_property('destination_name', 'SM_RokGroveCenterScale')
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(MESH)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError('glTF import did not produce exact Grove mesh: '+str(task.imported_object_paths))
    mesh.set_editor_property('allow_cpu_access', True)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    # Interchange enabled Nanite on this low-poly source mesh in the actual import.
    # Its per-tree WPO path uses the original readable render vertices explicitly.
    nanite = subsystem.get_nanite_settings(mesh)
    nanite.set_editor_property('enabled', False)
    subsystem.set_nanite_settings(mesh, nanite, True)
    build = subsystem.get_lod_build_settings(mesh, 0)
    build.set_editor_property('use_full_precision_u_vs', True)
    build.set_editor_property('generate_lightmap_u_vs', False)
    subsystem.set_lod_build_settings(mesh, 0, build)
    # Default import may append a generated lightmap channel. Source has only 3.
    while subsystem.get_num_uv_channels(mesh, 0) > 3:
        if not subsystem.remove_uv_channel(mesh, 0, subsystem.get_num_uv_channels(mesh, 0)-1):
            raise RuntimeError('Cannot remove generated non-source UV channel')
    if subsystem.get_num_uv_channels(mesh, 0) != 3:
        raise RuntimeError('glTF must retain exactly three UV channels')
    material = unreal.load_asset(MATERIAL)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_WorldSource_GroveCenterScale', BASE+'Materials', unreal.Material, unreal.MaterialFactoryNew())
    edit.delete_all_material_expressions(material)
    for prop in ('blend_mode', 'shading_model', 'two_sided', 'opacity_mask_clip_value'):
        material.set_editor_property(prop, original.get_editor_property(prop))
    def node(cls, **props):
        n = edit.create_material_expression(material, cls)
        for k, v in props.items(): n.set_editor_property(k, v)
        return n
    def connect(a, pin, b, name):
        if not edit.connect_material_expressions(a, pin, b, name): raise RuntimeError('Grove graph connection '+name)
    def custom(code, names, output):
        inputs=[]
        for name in names:
            entry=unreal.CustomInput(); entry.set_editor_property('input_name', name); inputs.append(entry)
        return node(unreal.MaterialExpressionCustom, code=code, inputs=inputs, output_type=output)
    uv = [node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=i) for i in range(3)]
    flip = custom('return float2(UV.x, 1.0-UV.y);', ['UV'], unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    connect(uv[0], '', flip, 'UV')
    sample = node(unreal.MaterialExpressionTextureSample, texture=texture, sampler_type=samples[0].get_editor_property('sampler_type'))
    connect(flip, '', sample, 'UVs')
    lights = [n for n in edit.get_material_expressions(original) if isinstance(n, unreal.MaterialExpressionVectorParameter) and str(n.get_editor_property('parameter_name'))=='UnitySpriteColor']
    if len(lights)!=1: raise RuntimeError('Missing source lighting parameter')
    light = node(unreal.MaterialExpressionVectorParameter, parameter_name='UnitySpriteColor', default_value=lights[0].get_editor_property('default_value'))
    multiply = node(unreal.MaterialExpressionMultiply)
    connect(sample, 'RGB', multiply, 'A'); connect(light, 'RGB', multiply, 'B')
    assert edit.connect_material_property(multiply, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert edit.connect_material_property(sample, 'A', unreal.MaterialProperty.MP_OPACITY)
    scale = node(unreal.MaterialExpressionScalarParameter, parameter_name='RokSourceTreeScale', default_value=data['bake_scale'])
    worldpos = node(unreal.MaterialExpressionWorldPosition, world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    local = node(unreal.MaterialExpressionTransformPosition, transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD, transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    # MaterialGraphNode shortens the source Input pin to NAME_None; empty selects input 0.
    connect(worldpos, '', local, '')
    delta = custom(f'return (P-18000.0*float3(RG.x,BA.x,RG.y))*(Scale/{data["bake_scale"]:.17g}-1.0);', ['P','RG','BA','Scale'], unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    for a,p,n in ((local,'','P'),(uv[1],'','RG'),(uv[2],'','BA'),(scale,'','Scale')): connect(a,p,delta,n)
    # Transform positions, then subtract: includes nonuniform actor scale exactly.
    displaced = node(unreal.MaterialExpressionAdd)
    connect(local,'',displaced,'A'); connect(delta,'',displaced,'B')
    toworld = node(unreal.MaterialExpressionTransformPosition, transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL, transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    connect(displaced,'',toworld,'')
    offset = node(unreal.MaterialExpressionSubtract)
    connect(toworld,'',offset,'A'); connect(worldpos,'',offset,'B')
    assert edit.connect_material_property(offset, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    edit.recompile_material(material)
    mesh.set_material(0, material)
    # Source maximum is 1.2. Expanded bounds accommodate WPO only; no geometry edit.
    source_axes=(0,2,1)
    extent=[]
    for axis in source_axes:
        extent.append(max(abs((p[axis]-c[axis])*(s-data['bake_scale'])*100) for p,c in zip(data['positions'], data['centers_unity']) for s in (0.23,1.2)))
    mesh.set_editor_property('positive_bounds_extension', unreal.Vector(*extent))
    mesh.set_editor_property('negative_bounds_extension', unreal.Vector(*extent))
    for asset in (mesh, material):
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset): raise RuntimeError('Grove asset save failed')
    actual = dict(
        nanite_enabled=subsystem.get_nanite_settings(mesh).get_editor_property('enabled'),
        allow_cpu_access=mesh.get_editor_property('allow_cpu_access'),
        source_lod_count=subsystem.get_lod_count(mesh),
        render_lod_count=mesh.get_num_lods(),
        full_precision_uvs=subsystem.get_lod_build_settings(mesh,0).get_editor_property('use_full_precision_u_vs'),
        uv_channels=subsystem.get_num_uv_channels(mesh,0))
    if actual != dict(nanite_enabled=False, allow_cpu_access=True, source_lod_count=1,
                      render_lod_count=1, full_precision_uvs=True, uv_channels=3):
        raise RuntimeError('Grove post-build transport flags invalid: '+json.dumps(actual))
    for actor, component in zip(actors, components):
        component.set_static_mesh(mesh); component.set_material(0,material)
        actor.set_editor_property('tags', list(dict.fromkeys([str(t) for t in actor.tags]+[TAG])))
    after = [transform(a) for a in actors]
    if before != after: raise RuntimeError('Source actor transform changed')
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
        raise RuntimeError('Grove map binding save failed')
    (out/'ue_upgrade.json').write_text(json.dumps(dict(project=str(root),world=world.get_name(), actors=[a.get_actor_label() for a in actors], transforms_unchanged=True, mesh=mesh.get_path_name(), material=material.get_path_name(), **actual, source_texture=texture.get_path_name(), visual_passed=False), indent=2))
    unreal.log('ROK_GROVE_CENTER_UPGRADE_COMPLETE 11 source actors; transforms unchanged')

if __name__=='__main__': main()
