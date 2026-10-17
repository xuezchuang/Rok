"""Import the exact GameToggle UI click sound; no map or Blueprint changes."""
from pathlib import Path
import hashlib
import json
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
if root != Path("D:/ueDemo/Rok").resolve():
    raise RuntimeError(f"Wrong project: {root}")
source = root / "ref/client-unity/Assets/BundleAssets/Souds/Sound_Ui_CommonSidePage.mp3"
decoded = root / "Saved/RokUnityMigration/Sound_Ui_CommonSidePage_mono.wav"
if not decoded.is_file():
    raise RuntimeError("Run ExportRokUnityFilterSound.Run in the isolated Unity probe first")
task = unreal.AssetImportTask()
task.set_editor_property("filename", str(decoded))
task.set_editor_property("destination_path", "/Game/RokPrototype/UI/Audio")
task.set_editor_property("destination_name", "Sound_Ui_CommonSidePage")
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
task.set_editor_property("factory", unreal.SoundFactory())
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound = unreal.load_asset("/Game/RokPrototype/UI/Audio/Sound_Ui_CommonSidePage")
if not isinstance(sound, unreal.SoundWave):
    raise RuntimeError("Source click sound did not import as a SoundWave")
channels = sound.get_editor_property("num_channels")
duration = sound.get_editor_property("duration")
if channels != 1 or duration <= 0:
    raise RuntimeError(f"Unity requires mono; imported channels={channels}, duration={duration}")
sound.set_editor_property("looping", False)
unreal.EditorAssetLibrary.save_loaded_asset(sound)
evidence = {"source": str(source.relative_to(root)),
    "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
    "decoded_sha256": hashlib.sha256(decoded.read_bytes()).hexdigest(),
    "decoder": "Unity 2019.4.28f1c1 AudioImporter with source meta forceToMono=1 normalize=0",
    "asset": sound.get_path_name(), "channels": channels, "duration": duration,
    "owner": "GameToggle.OnPointerClick / UI_Model_Ck_MainIFLod.SoundAssetName"}
output = root / "Saved/RokUnityMigration/unity_filter_click_sound.json"
output.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
unreal.log("ROK_FILTER_SOUND " + json.dumps(evidence))
