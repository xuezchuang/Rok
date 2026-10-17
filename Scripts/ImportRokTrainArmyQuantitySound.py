"""Run in the verified Rok city editor; UE5.8 SoundFactory supports source MP3."""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if Path(unreal.Paths.project_dir()).resolve() != ROOT.resolve() or world.get_name() != 'L_RokUnityGameAppCity':
    raise RuntimeError('Wrong project/map for TrainArmy quantity sound import')
source = ROOT / 'ref/client-unity/Assets/BundleAssets/Souds/Sound_Ui_CommonScroll.mp3'
assert source.is_file()
target = '/Game/RokPrototype/UI/Audio/Sound_Ui_CommonScroll'
if not unreal.EditorAssetLibrary.does_asset_exist(target):
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path, task.destination_name = target.rsplit('/', 1)
    task.automated = True
    task.save = True
    task.factory = unreal.SoundFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
if not isinstance(unreal.load_asset(target), unreal.SoundWave):
    raise RuntimeError('Source scroll sound import failed')
print('TrainArmy source scroll sound ready: ' + target)
