"""Import the Unity-decoded construction clips without modifying the city map."""
import unreal

from CreateRokCityResourcePresentationAssets import identity
from rok_data_paths import PROJECT_ROOT


def apply():
    identity()
    source = PROJECT_ROOT / "Saved/UnityCityConstructionAudio/Wav"
    for name in ("Sound_Ui_BuildingStartLevelup", "sfx_buildingUp", "Sound_Ui_Speedup"):
        destination = "/Game/RokPrototype/UI/Audio"
        path = f"{destination}/{name}"
        task = None
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            task = unreal.AssetImportTask()
            task.filename = str(source / f"{name}.wav")
            task.destination_path = destination
            task.destination_name = name
            task.automated = True
            task.factory = unreal.SoundFactory()
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        sound = unreal.load_asset(path)
        if not isinstance(sound, unreal.SoundWave) or sound.get_editor_property("num_channels") != 1:
            raise RuntimeError(f"Expected a mono SoundWave at {path}")
        if not task:
            continue
        sound.set_editor_property("volume", 1.0)
        sound.set_editor_property("pitch", 1.0)
        sound.set_editor_property("looping", False)
        sound.set_sound_asset_compression_type(unreal.SoundAssetCompressionType.PCM)
        if not unreal.EditorAssetLibrary.save_loaded_asset(sound):
            raise RuntimeError(f"Could not save {path}")
        print(f"Imported construction source clip {path}")


if __name__ == "__main__":
    apply()
