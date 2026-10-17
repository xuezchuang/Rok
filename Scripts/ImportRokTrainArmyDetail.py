"""Run in the verified Rok city editor to import only source training atlases."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
project = Path(unreal.Paths.project_dir()).resolve()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if project != ROOT.resolve() or world.get_name() != 'L_RokUnityGameAppCity':
    raise RuntimeError(f'Wrong editor identity: {project}, {world.get_name()}')
catalog = json.loads((ROOT / 'Saved/RokUnityMigration/train_army_detail.json').read_text(encoding='utf-8'))
verified = []
for info in catalog['atlases'].values():
    source, target = ROOT / info['source'], info['target']
    if not source.is_file() or not target.startswith('/Game/RokPrototype/UI/TrainArmyDetail/'):
        raise RuntimeError('Invalid training atlas source or target')
    if not unreal.EditorAssetLibrary.does_asset_exist(target):
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path, task.destination_name = target.rsplit('/', 1)
        task.automated, task.replace_existing, task.save = True, False, False
        task.factory = unreal.TextureFactory()
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(target)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError('Failed training atlas import: ' + target)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('srgb', True)
    if not unreal.EditorAssetLibrary.save_asset(target):
        raise RuntimeError('Failed training atlas save: ' + target)
    verified.append(target)
print(json.dumps({'verified_atlases': verified, 'visual_verified': False}))
