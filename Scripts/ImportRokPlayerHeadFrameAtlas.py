"""Run only in the verified Rok editor. Missing source art is never substituted."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
project = Path(unreal.Paths.project_dir()).resolve()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if project != ROOT.resolve() or world.get_name() != 'L_RokUnityGameAppCity':
    raise RuntimeError(f'Wrong MCP/editor identity: project={project}, world={world.get_name()}')

catalog = json.loads((ROOT / 'Saved/RokUnityMigration/player_head_catalog.json').read_text(encoding='utf-8'))
imported = []
for info in catalog['atlases'].values():
    if not info['source']:
        continue
    source = ROOT / info['source']
    if not source.is_file():
        raise FileNotFoundError(source)
    target = info['target']
    if unreal.EditorAssetLibrary.does_asset_exist(target):
        continue
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path, task.destination_name = target.rsplit('/', 1)
    task.automated = True
    task.replace_existing = False
    task.save = False
    task.factory = unreal.TextureFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(target)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError('Texture import failed: ' + target)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('srgb', True)
    unreal.EditorAssetLibrary.save_asset(target)
    imported.append(target)
print(json.dumps({'imported_atlases': imported, 'missing_art_ids': catalog['missing_art_ids'],
                  'visual_verified': False}, ensure_ascii=False))
