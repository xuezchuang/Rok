"""Import the exact GameButton UI click sound; no map or Blueprint changes."""
from pathlib import Path
import hashlib
import json
import os
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
if root != Path("D:/ueDemo/Rok").resolve():
    raise RuntimeError(f"Wrong project: {root}")
name = os.environ.get("ROK_UI_BUTTON_SOUND", "Sound_Ui_CommonClickButton")
owners = {"Sound_Ui_CommonClickButton": "GameButton.OnPointerClick / UI_Pop_WorldObjectInfo.SoundAssetName",
          "Sound_Ui_CommonClickButton1": "UI_IF_MainInterface/btn_blue.SoundAssetName",
          "Sound_Ui_PickUp": "UI_IF_MainInterface/UI_Model_more.SoundAssetName"}
if name not in owners:
    raise ValueError(f"Unverified source sound: {name}")
source = root / f"ref/client-unity/Assets/BundleAssets/Souds/{name}.mp3"
decoded = root / f"Saved/RokUnityMigration/{name}_mono.wav"
if not decoded.is_file():
    raise RuntimeError("Run ExportRokUnityButtonSound.Run in the isolated Unity probe first")
task = unreal.AssetImportTask()
task.set_editor_property("filename", str(decoded))
task.set_editor_property("destination_path", "/Game/RokPrototype/UI/Audio")
task.set_editor_property("destination_name", name)
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
task.set_editor_property("factory", unreal.SoundFactory())
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound = unreal.load_asset(f"/Game/RokPrototype/UI/Audio/{name}")
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
    "owner": owners[name]}
evidence_name = "unity_button_click_sound" if name == "Sound_Ui_CommonClickButton" else name
output = root / f"Saved/RokUnityMigration/{evidence_name}.json"
output.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
unreal.log("ROK_BUTTON_SOUND " + json.dumps(evidence))
