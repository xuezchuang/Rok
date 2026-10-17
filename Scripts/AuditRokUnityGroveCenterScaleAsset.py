"""Read-only exact Grove asset flags; full Editor only, no saves or rebuilds."""
import json
from pathlib import Path
import unreal

def main():
    from CreateRokUnityGroveCenterScale import MESH
    root=Path(unreal.Paths.project_dir()).resolve()
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if root!=Path('D:/ueDemo/Rok').resolve() or not world or world.get_name()!='L_RokUnityWorldMapPrototype':
        raise RuntimeError('Wrong Grove asset probe identity')
    mesh=unreal.load_asset(MESH)
    subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if not isinstance(mesh,unreal.StaticMesh) or not subsystem:
        raise RuntimeError('Missing exact Grove mesh or full Editor mesh subsystem')
    settings=subsystem.get_nanite_settings(mesh)
    report=dict(project=str(root),world=world.get_name(),mesh=mesh.get_path_name(),
        allow_cpu_access=mesh.get_editor_property('allow_cpu_access'),
        nanite_enabled=settings.get_editor_property('enabled'),
        source_lod_count=subsystem.get_lod_count(mesh),
        render_lod_count=mesh.get_num_lods(),
        lod0_uv_channels=subsystem.get_num_uv_channels(mesh,0),
        full_precision_uvs=subsystem.get_lod_build_settings(mesh,0).get_editor_property('use_full_precision_u_vs'),
        scope='Read-only asset properties, source LOD count and actual render LOD count; runtime buffer probe remains separate.')
    (root/'Saved/RokUnityMigration/GroveCenterScale/ue_asset_flags.json').write_text(json.dumps(report,indent=2))
    unreal.log('ROK_GROVE_ASSET_FLAGS '+json.dumps(report))

if __name__=='__main__': main()
