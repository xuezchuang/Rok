"""Read-only UE editor verification for a new city BGM import journal."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path("D:/ueDemo/Rok").resolve()
BATCH = ROOT / "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial"
EXPECTED = {
    "Bgm_PeaceDay01": 1007,
    "Bgm_PeaceDay02": 1007,
    "Bgm_PeaceNight01": 1008,
    "Bgm_PeaceNight02": 1008,
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def contained(path: Path, parent: Path) -> Path:
    resolved, anchor = path.resolve(), parent.resolve()
    if not resolved.is_relative_to(anchor):
        raise ValueError(f"path escapes expected root: {path}")
    return resolved


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def sound_properties(sound) -> dict:
    mode = sound.get_editor_property("virtualization_mode")
    compression = sound.get_editor_property("sound_asset_compression_type")
    try:
        imported_sample_rate = int(sound.get_editor_property("imported_sample_rate"))
    except Exception:
        imported_sample_rate = None
    return {
        "asset": sound.get_path_name(),
        "duration_seconds": float(sound.get_editor_property("duration")),
        "channels": int(sound.get_editor_property("num_channels")),
        "sample_rate": int(sound.get_editor_property("sample_rate")),
        "imported_sample_rate": imported_sample_rate,
        "sound_asset_compression_type": str(compression),
        "looping": bool(sound.get_editor_property("looping")),
        "volume": float(sound.get_editor_property("volume")),
        "pitch": float(sound.get_editor_property("pitch")),
        "virtualization_mode": str(mode),
    }


def disk_package_files(object_path: str) -> list[dict]:
    package = object_path.split(".", 1)[0]
    if not package.startswith("/Game/"):
        raise ValueError(f"unexpected asset package path: {object_path}")
    stem = ROOT / "Content" / package[len("/Game/"):].replace("/", os.sep)
    actual = [Path(str(stem) + ext) for ext in (".uasset", ".uexp", ".ubulk") if Path(str(stem) + ext).is_file()]
    if not actual or not Path(str(stem) + ".uasset").is_file():
        raise RuntimeError(f"SoundWave package file missing: {stem}")
    return [{"path": str(path.relative_to(ROOT)).replace("\\", "/"), "sha256": sha(path), "bytes": path.stat().st_size} for path in actual]


def verify(args) -> dict:
    project = Path(unreal.Paths.project_dir()).resolve()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if project != ROOT or world is None or world.get_name() != "L_RokUnityGameAppCity":
        raise RuntimeError(f"Wrong UE identity: project={project}, world={world.get_name() if world else None}")
    manifest_path = contained(args.manifest, BATCH)
    if not manifest_path.is_file() or sha(manifest_path) != args.sha256.lower():
        raise RuntimeError("import manifest SHA-256 mismatch")
    manifest = read_json(manifest_path)
    if manifest.get("schema") != "rok.city_bgm_import.v1" or manifest.get("status") != "imported_and_readback_passed":
        raise RuntimeError("import manifest is incomplete or failed")
    if manifest.get("project") != ROOT.as_posix() or manifest.get("map") != "L_RokUnityGameAppCity":
        raise RuntimeError("import manifest project/map identity mismatch")
    if set(manifest.get("created_assets", [])) != {f"/Game/RokPrototype/UI/Audio/CityBgm/{name}" for name in EXPECTED}:
        raise RuntimeError("import manifest does not list exactly the four requested new assets")
    sys.path.insert(0, str(ROOT / "Scripts"))
    import ImportRokCityBgmAudio as importer
    pins = manifest["input_pins"]
    fresh_inputs = importer.validate_native_inputs(
        ROOT / pins["run"], ROOT / pins["strict_verify_report"], pins["strict_verify_sha256"])
    for key in ("native_manifest_sha256", "strict_verify_sha256", "contract_sha256", "probe_state_sha256", "exporter_sha256"):
        if fresh_inputs.get(key) != pins.get(key):
            raise RuntimeError(f"native audio input chain changed after import: {key}")

    checked = []
    native_clips = {row["name"]: row for row in fresh_inputs["clips"]}
    for name, event_id in EXPECTED.items():
        target = f"/Game/RokPrototype/UI/Audio/CityBgm/{name}"
        sound = unreal.load_asset(target)
        if sound is None or not isinstance(sound, unreal.SoundWave):
            raise RuntimeError(f"Imported SoundWave missing: {target}")
        props = sound_properties(sound)
        if (props["looping"] or props["volume"] != 1.0 or props["pitch"] != 1.0 or
                ("PLAY_WHEN_SILENT" not in props["virtualization_mode"].upper() and
                 "PLAYWHENSILENT" not in props["virtualization_mode"].upper())):
            raise RuntimeError(f"SoundWave property mismatch: {target}: {props}")
        if "PCM" not in props["sound_asset_compression_type"].upper():
            raise RuntimeError(f"SoundWave compression is not PCM: {target}: {props}")
        native = native_clips[name]
        tolerance = importer.native_duration_tolerance(native["sample_rate"], native["duration_seconds"])
        if (props["channels"] != 1 or props["sample_rate"] != native["sample_rate"] or
                props["duration_seconds"] <= 0 or
                abs(props["duration_seconds"] - native["duration_seconds"]) > tolerance):
            raise RuntimeError(f"SoundWave format metadata invalid: {target}: {props}")
        if props["imported_sample_rate"] not in (None, 0, native["sample_rate"]):
            raise RuntimeError(f"Imported sample-rate metadata differs from source: {target}: {props}")
        prior = next((row for row in manifest["asset_results"] if row.get("name") == name), None)
        if prior is None or prior.get("audio_info_id") != event_id or prior.get("ue_object_path") != target + "." + name:
            raise RuntimeError(f"Imported SoundWave mapping mismatch: {name}")
        current_files = disk_package_files(props["asset"])
        if current_files != prior.get("package_files"):
            raise RuntimeError(f"SoundWave package hashes/bytes changed since import: {name}")
        if props != prior.get("properties"):
            raise RuntimeError(f"SoundWave readback properties changed since import: {name}")
        checked.append({"name": name, "audio_info_id": event_id, "object_path": props["asset"],
                        "properties": props, "package_files": current_files,
                        "native_duration_tolerance_seconds": tolerance,
                        "wav_sha256": prior["wav_sha256"], "wav_bytes": prior["wav_bytes"]})

    old_checked = []
    for previous in manifest.get("old_router_soundwaves_read_only", []):
        if not previous.get("present"):
            old_checked.append({"object_path": previous["object_path"], "present": False, "unchanged": True})
            continue
        package = previous["object_path"].rsplit(".", 1)[0]
        old = unreal.load_asset(package)
        if old is None or not isinstance(old, unreal.SoundWave):
            raise RuntimeError(f"Previously observed router SoundWave disappeared: {package}")
        props = sound_properties(old)
        if props != previous.get("properties"):
            raise RuntimeError(f"Existing router SoundWave properties changed: {package}")
        old_checked.append({"object_path": previous["object_path"], "present": True,
                            "unchanged": True, "properties": props})

    return {"schema": "rok.city_bgm_import.verify.v1", "status": "passed",
            "goal_complete": False, "full_migration_accepted": False,
            "project": project.as_posix(), "map": world.get_name(),
            "import_manifest": str(manifest_path.relative_to(ROOT)).replace("\\", "/"),
            "import_manifest_sha256": sha(manifest_path), "native_inputs_reverified": fresh_inputs,
            "assets": checked, "old_router_soundwaves_read_only": old_checked,
            "qualification": "UE asset and property readback only; no playback/audible acceptance or routing changes"}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--sha256", required=True)
    args = parser.parse_args(argv)
    result = verify(args)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    out = BATCH / f"city_bgm_import_verify_{stamp}_{os.getpid()}.json"
    with out.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2); stream.write("\n")
    unreal.log("ROK_CITY_BGM_IMPORT_VERIFY " + json.dumps({"status": result["status"], "report": str(out.relative_to(ROOT))}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
