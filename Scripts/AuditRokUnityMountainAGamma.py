"""Read-only editor audit: records existing mountain A actors and source UV assets."""
import json
from pathlib import Path
import unreal


def main():
    from CreateRokUnityMountainAGammaMaterial import NAME, RAW_NAME, CODE
    root=Path(unreal.Paths.project_dir()).resolve()
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if root!=Path('D:/ueDemo/Rok').resolve() or not world or world.get_name()!='L_RokUnityWorldMapPrototype':
        raise RuntimeError('Wrong mountain audit identity')
    base='/Game/RokUnityWorldMapPrototype/'
    material=unreal.load_asset(base+'Materials/'+NAME)
    raw=unreal.load_asset(base+'Textures/'+RAW_NAME)
    if not isinstance(material,unreal.Material) or not isinstance(raw,unreal.Texture2D) or raw.get_editor_property('srgb'):
        raise RuntimeError('Missing raw Gamma material/texture')
    expressions=unreal.MaterialEditingLibrary.get_material_expressions(material)
    custom=[n for n in expressions if isinstance(n,unreal.MaterialExpressionCustom)]
    samples=[n for n in expressions if isinstance(n,unreal.MaterialExpressionTextureSample)]
    if len(custom)!=1 or custom[0].get_editor_property('code')!=CODE or len(samples)!=1 or samples[0].get_editor_property('texture')!=raw:
        raise RuntimeError('Wrong mountain shader graph')
    if samples[0].get_editor_property('sampler_type')!=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR:
        raise RuntimeError('Wrong mountain raw sampler')
    rows=[]
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        for mesh in actor.get_components_by_class(unreal.StaticMeshComponent):
            for slot in range(mesh.get_num_materials()):
                if mesh.get_material(slot)==material:
                    position=actor.get_actor_location()
                    rows.append({'actor':actor.get_actor_label(),'mesh':mesh.static_mesh.get_path_name() if mesh.static_mesh else None,'slot':slot,'world_position':[position.x,position.y,position.z],'tags':[str(t) for t in actor.tags]})
    if not rows:
        raise RuntimeError('No actual current-world mountain A material users')
    report={'project':str(root),'world':world.get_name(),'material':material.get_path_name(),'raw':raw.get_path_name(),'actual_users':rows,'visual_passed':False,'scope':'Read-only asset/binding inspection; screenshots and continuous runtime remain required.'}
    path=root/'Saved/RokUnityMigration/MountainAGamma/ue_bindings.json'
    path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    unreal.log('ROK_MOUNTAIN_A_GAMMA_BINDINGS_PASS '+str(len(rows)))


if __name__=='__main__':
    main()
