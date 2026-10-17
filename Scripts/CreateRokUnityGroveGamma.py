"""RGB-only post-step for the existing eleven Grove instances; root runs full Editor."""
import hashlib,json
from pathlib import Path
import unreal
from PrepareRokUnityGroveGammaProduct import MATERIAL,RAW,CODE

def main():
    root=Path(unreal.Paths.project_dir()).resolve()
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if root!=Path('D:/ueDemo/Rok').resolve() or not world or world.get_name()!='L_RokUnityWorldMapPrototype':
        raise RuntimeError('Wrong Grove Gamma project/world')
    out=root/'Saved/RokUnityMigration/GroveGamma';spec=json.loads((out/'product_input.json').read_text())
    texture_mode='unity_decoded_1024'
    if spec['mode']!=texture_mode or spec['shader_code']!=CODE:
        raise RuntimeError('Unapproved Grove Gamma transport mode/code')
    for item in spec['input_files']+[spec['producer']]:
        if hashlib.sha256((root/item['path']).read_bytes()).hexdigest()!=item['sha256']:
            raise RuntimeError('Stale GroveGamma product input '+item['path'])
    material=unreal.load_asset(MATERIAL)
    if not isinstance(material,unreal.Material): raise RuntimeError('Missing exact Grove center material')
    actors=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if 'RokGroveCenterScaleV1' in map(str,a.tags)]
    source=json.loads((root/'Saved/RokUnityMigration/GroveCenterScale/manifest.json').read_text())
    expected={f"RokWorldObject_Tile{i['tile'][0]}_{i['tile'][1]}_Object{i['source_instance']['template_index']:02d}_Grove_co_tree_01_adorning" for i in source['instances']}
    if len(actors)!=11 or {a.get_actor_label() for a in actors}!=expected: raise RuntimeError('Exact11 Grove source actors required')
    def actor_state(a):
        p,r,s=a.get_actor_location(),a.get_actor_rotation(),a.get_actor_scale3d();c=a.get_component_by_class(unreal.StaticMeshComponent)
        if not c or c.get_material(0)!=material: raise RuntimeError('Unexpected Grove material binding')
        return dict(actor=a.get_actor_label(),transform=[p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z],mesh=c.static_mesh.get_path_name(),sort_priority=c.get_editor_property('translucency_sort_priority'))
    before=[actor_state(a) for a in actors];edit=unreal.MaterialEditingLibrary
    opacity=edit.get_material_property_input_node(material,unreal.MaterialProperty.MP_OPACITY)
    opacity_pin=edit.get_material_property_input_node_output_name(material,unreal.MaterialProperty.MP_OPACITY)
    if not isinstance(opacity,unreal.MaterialExpressionTextureSample) or opacity_pin!='A': raise RuntimeError('Original opacity sample contract changed')
    old_texture=opacity.get_editor_property('texture');uv=edit.get_inputs_for_material_expression(material,opacity)[0]
    if not isinstance(uv,unreal.MaterialExpressionCustom) or uv.get_editor_property('code')!='return float2(UV.x, 1.0-UV.y);': raise RuntimeError('Source UV0 conversion changed')
    wpo=edit.get_material_property_input_node(material,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    def graph(node,visited=None):
        visited={} if visited is None else visited
        if node and node.get_path_name() not in visited:
            inputs=edit.get_inputs_for_material_expression(material,node)
            visited[node.get_path_name()]=dict(type=node.get_class().get_name(),inputs=[x.get_path_name() if x else None for x in inputs],code=node.get_editor_property('code') if isinstance(node,unreal.MaterialExpressionCustom) else None)
            if isinstance(node,unreal.MaterialExpressionScalarParameter):
                visited[node.get_path_name()].update(parameter=str(node.get_editor_property('parameter_name')),default=node.get_editor_property('default_value'))
            if isinstance(node,unreal.MaterialExpressionTextureCoordinate):
                visited[node.get_path_name()]['uv_index']=node.get_editor_property('coordinate_index')
            for x in inputs: graph(x,visited)
        return visited
    before_wpo=graph(wpo)
    before_opacity=graph(opacity)
    if not wpo or len(before_wpo)<8: raise RuntimeError('Missing validated source-center WPO graph')
    preserved_props={k:material.get_editor_property(k) for k in ('blend_mode','shading_model','two_sided','opacity_mask_clip_value')}
    # Actual saved Grove graph contains a disconnected historical same-name
    # parameter. Only the rendered Emissive dependency identifies the source light.
    emissive=edit.get_material_property_input_node(material,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    emissive_dependencies=graph(emissive)
    lights=[n for n in edit.get_material_expressions(material) if n.get_path_name() in emissive_dependencies and isinstance(n,unreal.MaterialExpressionVectorParameter) and str(n.get_editor_property('parameter_name'))=='UnitySpriteColor']
    if len(lights)!=1: raise RuntimeError('Exact reachable Emissive lighting parameter required')
    task=unreal.AssetImportTask();task.set_editor_property('filename',str(out/'native_imported_texture.png'))
    task.set_editor_property('destination_path',RAW.rsplit('/',1)[0]);task.set_editor_property('destination_name',RAW.rsplit('/',1)[1])
    for k,v in dict(automated=True,replace_existing=True,replace_existing_settings=True,save=False,factory=unreal.TextureFactory()).items():task.set_editor_property(k,v)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);raw=unreal.load_asset(RAW)
    if not isinstance(raw,unreal.Texture2D): raise RuntimeError('Missing decoded Grove raw texture')
    for k,v in dict(srgb=False,compression_settings=unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP,mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,filter=unreal.TextureFilter.TF_BILINEAR,address_x=unreal.TextureAddress.TA_CLAMP,address_y=unreal.TextureAddress.TA_CLAMP,max_texture_size=1024,never_stream=True).items():raw.set_editor_property(k,v)
    def unique(cls,label):
        nodes=[n for n in edit.get_material_expressions(material) if isinstance(n,cls) and n.get_editor_property('desc')==label]
        if len(nodes)>1: raise RuntimeError('Duplicate GroveGamma graph node')
        node=nodes[0] if nodes else edit.create_material_expression(material,cls);node.set_editor_property('desc',label);return node
    sample=unique(unreal.MaterialExpressionTextureSample,'RokGroveGammaRawRGBV1');sample.set_editor_property('texture',raw);sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    gamma=unique(unreal.MaterialExpressionCustom,'RokGroveGammaV1');gamma.set_editor_property('description','RokGroveGammaV1');gamma.set_editor_property('code',CODE);gamma.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs=[]
    for name in ('RawRGB','Lighting'):
        entry=unreal.CustomInput();entry.set_editor_property('input_name',name);inputs.append(entry)
    gamma.set_editor_property('inputs',inputs)
    for a,p,b,k in ((uv,'',sample,'UVs'),(sample,'RGB',gamma,'RawRGB'),(lights[0],'RGB',gamma,'Lighting')):
        if not edit.connect_material_expressions(a,p,b,k): raise RuntimeError('GroveGamma graph connection '+k)
    if not edit.connect_material_property(gamma,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR): raise RuntimeError('GroveGamma emissive connection failed')
    edit.recompile_material(material)
    if graph(wpo)!=before_wpo or edit.get_material_property_input_node(material,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)!=wpo: raise RuntimeError('Source-center WPO changed')
    if graph(opacity)!=before_opacity or edit.get_material_property_input_node(material,unreal.MaterialProperty.MP_OPACITY)!=opacity or opacity.get_editor_property('texture')!=old_texture or edit.get_material_property_input_node_output_name(material,unreal.MaterialProperty.MP_OPACITY)!=opacity_pin: raise RuntimeError('Original alpha changed')
    if any(material.get_editor_property(k)!=v for k,v in preserved_props.items()) or [actor_state(a) for a in actors]!=before: raise RuntimeError('Existing rendering/placement changed')
    if raw.get_editor_property('srgb') or raw.get_editor_property('compression_settings')!=unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP: raise RuntimeError('Raw texture flags changed')
    for asset in (raw,material):
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset): raise RuntimeError('GroveGamma save failed')
    old_texture_settings={k:str(old_texture.get_editor_property(k)) for k in ('srgb','compression_settings','mip_gen_settings','filter','max_texture_size')}
    result=dict(project=str(root),world=world.get_name(),material=material.get_path_name(),raw_texture=raw.get_path_name(),source_mode=texture_mode,source_texture_sha256=hashlib.sha256((out/'native_imported_texture.png').read_bytes()).hexdigest(),old_alpha_texture=old_texture.get_path_name(),old_alpha_settings=old_texture_settings,old_alpha_input_unchanged=True,wpo_graph_unchanged=True,actor_transforms_and_sort_unchanged=True,actors=before,srgb=raw.get_editor_property('srgb'),compression=str(raw.get_editor_property('compression_settings')),gamma_code=CODE,visual_passed=False,scope='RGB-only source Gamma order; old alpha/cull/sort/WPO unchanged. No map save needed or performed.')
    (out/'ue_upgrade.json').write_text(json.dumps(result,indent=2)+'\n');unreal.log('ROK_GROVE_GAMMA_UPGRADE_COMPLETE 11 RGB-only; WPO and alpha unchanged')
if __name__=='__main__':main()
