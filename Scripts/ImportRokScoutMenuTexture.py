"""UE5.8 one-asset import. Run in the real Rok city map; does not modify the map."""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
actual_project = Path(unreal.Paths.project_dir()).resolve()
world = unreal.EditorLevelLibrary.get_editor_world()
actual_world = world.get_name() if world else ''
print('Scout texture import identity:', actual_project, actual_world)
if actual_project != ROOT or actual_world != 'L_RokUnityGameAppCity':
    raise RuntimeError('Scout import requires exact Rok project and L_RokUnityGameAppCity world')
source = ROOT / 'Saved/RokUiProcessed/rok_building_menu_scout.png'
if not source.is_file(): raise RuntimeError('Run BuildRokScoutMenuTexture.py first')
task = unreal.AssetImportTask()
task.filename = str(source)
task.destination_path = '/Game/RokPrototype/UI/BuildingMenu'
task.destination_name = 'rok_building_menu_scout'
task.automated = True
task.replace_existing = True
task.save = False
task.factory = unreal.TextureFactory()
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset('/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_scout')
if not isinstance(texture, unreal.Texture2D): raise RuntimeError('Scout icon import failed')
for name, value in [('srgb', True), ('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
                    ('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI),
                    ('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON),
                    ('never_stream', True)]:
    texture.set_editor_property(name, value)
if not unreal.EditorAssetLibrary.save_loaded_asset(texture): raise RuntimeError('Scout icon save failed')
print('Imported exact Scout menu icon:', texture.get_path_name())
