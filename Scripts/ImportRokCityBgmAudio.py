"""Import four strictly verified Unity-decoded city BGM WAVs as new UE SoundWaves.

Run inside the verified UE5.8 editor Python environment. This script never replaces
an existing asset, saves a map, changes old SoundWaves, or touches account/network state.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import time
import wave
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path("D:/ueDemo/Rok").resolve()
BATCH = ROOT / "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial"
PROBE = ROOT / "Saved/UnityCityAudioProbeV1"
CATALOG = ROOT / "Saved/RokUnityMigration/CityAudio/catalog.json"
VERIFY_SCRIPT = ROOT / "Scripts/VerifyRokUnityCityAudioProbe.py"
PREPARE_PREFIX = "city_audio_probe_prepare_revision"
EXPECTED = {
    "Bgm_PeaceDay01": (1007, "Bgm_PeaceDay"),
    "Bgm_PeaceDay02": (1007, "Bgm_PeaceDay"),
    "Bgm_PeaceNight01": (1008, "Bgm_PeaceNight"),
    "Bgm_PeaceNight02": (1008, "Bgm_PeaceNight"),
}
TARGET_DIR = "/Game/RokPrototype/UI/Audio/CityBgm"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def repo_path(path: Path) -> str:
    return path.resolve().relative_to(ROOT).as_posix()


def contained(path: Path, parent: Path) -> Path:
    resolved, anchor = path.resolve(), parent.resolve()
    if not resolved.is_relative_to(anchor):
        raise ValueError(f"input path escapes {anchor}: {path}")
    return resolved


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def active_prepare_report() -> Path:
    candidates = []
    for path in BATCH.glob(f"{PREPARE_PREFIX}[0-9]*.json"):
        match = re.fullmatch(r"city_audio_probe_prepare_revision(\d+)\.json", path.name)
        if match:
            candidates.append((int(match.group(1)), path))
    if candidates:
        return max(candidates)[1]
    if (BATCH / "city_audio_probe_prepare.json").is_file():
        return BATCH / "city_audio_probe_prepare.json"
    raise RuntimeError("No prepared Unity city audio report exists")


def active_virtualization_value():
    for enum_name in ("VirtualizationMode", "EVirtualizationMode", "SoundWaveVirtualizationMode"):
        enum_type = getattr(unreal, enum_name, None)
        if enum_type is None:
            continue
        for value_name in ("PLAY_WHEN_SILENT", "PlayWhenSilent"):
            value = getattr(enum_type, value_name, None)
            if value is not None:
                return value
    raise RuntimeError("UE Python enum for EVirtualizationMode.PlayWhenSilent is unavailable")


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


def native_duration_tolerance(sample_rate: int, duration_seconds: float) -> float:
    if sample_rate <= 0 or not math.isfinite(duration_seconds) or duration_seconds <= 0:
        raise ValueError("invalid native sample rate or duration")
    # One source sample plus one float32 ULP at the duration magnitude.
    return (1.0 / sample_rate) + (abs(duration_seconds) * (2.0 ** -23))


def pcm_compression_value():
    enum_type = getattr(unreal, "SoundAssetCompressionType", None)
    value = getattr(enum_type, "PCM", None) if enum_type is not None else None
    if value is None:
        raise RuntimeError("UE Python enum SoundAssetCompressionType.PCM is unavailable")
    return value


def disk_package_files(object_path: str) -> list[dict]:
    package = object_path.split(".", 1)[0]
    if not package.startswith("/Game/"):
        raise ValueError(f"unexpected non-project asset path: {object_path}")
    stem = ROOT / "Content" / (package[len("/Game/"):].replace("/", os.sep))
    paths = [Path(str(stem) + suffix) for suffix in (".uasset", ".uexp", ".ubulk")]
    existing = [path for path in paths if path.is_file()]
    if not existing or not Path(str(stem) + ".uasset").is_file():
        raise RuntimeError(f"saved SoundWave package file missing: {stem}")
    return [{"path": repo_path(path), "sha256": sha(path), "bytes": path.stat().st_size} for path in existing]


def validate_native_inputs(run_arg: Path, verify_arg: Path, verify_sha: str) -> dict:
    run = contained(run_arg, PROBE / "OutputRuns")
    verify_report = contained(verify_arg, BATCH)
    if not verify_report.is_file() or sha(verify_report) != verify_sha.lower():
        raise RuntimeError("strict verifier report file/hash does not match the supplied pin")
    strict = read_json(verify_report)
    verifier_sha = sha(VERIFY_SCRIPT)
    if (strict.get("status") != "passed" or strict.get("schema") != "rok.unity_city_audio_probe.verify.v1" or
            strict.get("verifier") != "Scripts/VerifyRokUnityCityAudioProbe.py" or
            strict.get("verifier_sha256") != verifier_sha or strict.get("run") != repo_path(run)):
        raise RuntimeError("native run was not verified by the current strict Unity audio verifier")
    manifest_path = run / "native_city_audio.json"
    contract_path = PROBE / "probe_contract.json"
    state_path = PROBE / "probe_state.json"
    if not manifest_path.is_file() or not contract_path.is_file() or not state_path.is_file():
        raise RuntimeError("native manifest, contract, or recorded importer-state file is missing")
    manifest, contract = read_json(manifest_path), read_json(contract_path)
    exporter = PROBE / "Assets/Editor/ExportRokUnityCityAudioProbe.cs"
    if (strict.get("native_manifest_sha256") != sha(manifest_path) or
            strict.get("contract_sha256") != sha(contract_path) or
            manifest.get("contract_sha256") != sha(contract_path) or
            manifest.get("probe_state_sha256") != sha(state_path) or
            manifest.get("exporter_sha256") != sha(exporter)):
        raise RuntimeError("strict report/native manifest source-chain hashes do not agree")
    if manifest.get("schema") != "rok.unity_city_audio_probe.native.v2" or contract.get("schema") != "rok.unity_city_audio_probe.contract.v2":
        raise RuntimeError("unsupported native audio manifest/contract revision")
    prepared_path = active_prepare_report()
    prepared = read_json(prepared_path)
    if (prepared.get("contract", {}).get("sha256") != sha(contract_path) or
            prepared.get("exporter", {}).get("sha256") != sha(exporter) or
            prepared.get("scripts", [{} , {}])[1].get("sha256") != sha(VERIFY_SCRIPT)):
        raise RuntimeError("active prepared report no longer matches native decode inputs")
    rows = {row["name"]: row for row in manifest.get("clips", [])}
    strict_rows = {row["name"]: row for row in strict.get("tracks", [])}
    contract_rows = {row["name"]: row for row in contract.get("tracks", [])}
    if set(rows) != set(EXPECTED) or set(strict_rows) != set(EXPECTED) or set(contract_rows) != set(EXPECTED):
        raise RuntimeError("native/strict/contract inputs must contain exactly the four expected BGM clips")
    checked = []
    for name, (audio_id, group_name) in EXPECTED.items():
        clip, source = rows[name], contract_rows[name]
        if clip.get("audio_info_id") != audio_id or clip.get("audio_info_name") != group_name:
            raise RuntimeError(f"AudioInfo identity mismatch for {name}")
        if strict_rows[name].get("wav_sha256") != clip.get("wav_sha256"):
            raise RuntimeError(f"strict verification WAV hash mismatch: {name}")
        for key in ("source_path", "source_sha256", "meta_path", "meta_sha256", "guid"):
            if clip.get(key) != source.get(key):
                raise RuntimeError(f"native contract source pin mismatch {name}:{key}")
        source_file, source_meta = ROOT / clip["source_path"], ROOT / clip["meta_path"]
        if sha(source_file) != clip["source_sha256"] or sha(source_meta) != clip["meta_sha256"]:
            raise RuntimeError(f"Unity source MP3/meta changed since decode: {name}")
        if (clip.get("source_default_settings", {}).get("load_type") != 2 or
                clip.get("source_standalone_settings", {}).get("load_type") != 2 or
                clip.get("probe_default_settings", {}).get("load_type") != 0 or
                clip.get("probe_standalone_settings", {}).get("load_type") != 0 or
                clip.get("channels") != 1 or not clip.get("all_samples_finite")):
            raise RuntimeError(f"decode-only importer state/channel contract invalid: {name}")
        wav_path = contained(run / clip["wav_path"], run)
        if not wav_path.is_file() or sha(wav_path) != clip.get("wav_sha256") or wav_path.stat().st_size != clip.get("wav_bytes"):
            raise RuntimeError(f"verified WAV changed or missing: {name}")
        with wave.open(str(wav_path), "rb") as wav:
            if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getnframes()) != (1, 2, clip["frequency"], clip["sample_frames"]):
                raise RuntimeError(f"WAV format differs from native manifest: {name}")
        checked.append({"name": name, "audio_info_id": audio_id, "audio_info_name": group_name,
                        "wav_path": repo_path(wav_path), "wav_sha256": sha(wav_path), "wav_bytes": wav_path.stat().st_size,
                        "channels": clip["channels"], "sample_rate": clip["frequency"],
                        "duration_seconds": clip["length_seconds"]})
    return {"run": repo_path(run), "native_manifest": repo_path(manifest_path),
            "native_manifest_sha256": sha(manifest_path), "strict_verify_report": repo_path(verify_report),
            "strict_verify_sha256": sha(verify_report), "strict_verifier_sha256": verifier_sha,
            "contract_sha256": sha(contract_path), "probe_state_sha256": sha(state_path),
            "exporter_sha256": sha(exporter), "prepare_report": repo_path(prepared_path),
            "prepare_report_sha256": sha(prepared_path), "clips": checked}


def old_wave_inventory(catalog: dict) -> list[dict]:
    result = []
    for event in catalog.get("events", []):
        if int(event.get("id", -1)) in (1007, 1008):
            continue
        for clip in event.get("clips", []):
            object_path = clip["ue_object_path"]
            package = object_path.rsplit(".", 1)[0]
            sound = unreal.load_asset(package)
            if sound is None or not isinstance(sound, unreal.SoundWave):
                result.append({"event_id": event["id"], "name": clip["name"], "object_path": object_path,
                               "present": False, "changed": False})
            else:
                result.append({"event_id": event["id"], "name": clip["name"], "object_path": object_path,
                               "present": True, "changed": False, "properties": sound_properties(sound)})
    return result


def target_exists(name: str) -> bool:
    return unreal.EditorAssetLibrary.does_asset_exist(f"{TARGET_DIR}/{name}")


def write_journal(path: Path, data: dict) -> None:
    temporary = Path(str(path) + ".tmp")
    temporary.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", required=True, type=Path, help="Unity Audio native OutputRuns/<id>")
    parser.add_argument("--verify-report", required=True, type=Path, help="strict Unity audio verifier report")
    parser.add_argument("--verify-sha256", required=True, help="SHA-256 of --verify-report")
    args = parser.parse_args(argv)

    project = Path(unreal.Paths.project_dir()).resolve()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if project != ROOT or world is None or world.get_name() != "L_RokUnityGameAppCity":
        raise RuntimeError(f"Wrong UE identity: project={project}, world={world.get_name() if world else None}")
    if len(args.verify_sha256) != 64 or any(c not in "0123456789abcdefABCDEF" for c in args.verify_sha256):
        raise ValueError("--verify-sha256 must be a 64 digit hex SHA-256")

    input_pins = validate_native_inputs(args.run, args.verify_report, args.verify_sha256)
    catalog = read_json(CATALOG)
    catalog_sha = sha(CATALOG)
    if catalog.get("schema") != "rok.city_audio_source.v1" or catalog.get("policy") != "local_audio_only_no_network_no_account_state":
        raise RuntimeError("city AudioInfo router catalog schema/policy mismatch")
    event_rows = {int(event["id"]): event for event in catalog.get("events", [])}
    if 1007 not in event_rows or [row["name"] for row in event_rows[1007].get("clips", [])] != ["Bgm_PeaceDay01", "Bgm_PeaceDay02"]:
        raise RuntimeError("AudioInfo day BGM catalog clips changed")
    if 1008 not in event_rows or [row["name"] for row in event_rows[1008].get("clips", [])] != ["Bgm_PeaceNight01", "Bgm_PeaceNight02"]:
        raise RuntimeError("AudioInfo night BGM catalog clips changed")
    target_names = list(EXPECTED)
    existing = [name for name in target_names if target_exists(name)]
    if existing:
        raise RuntimeError("Refusing to replace existing BGM SoundWave asset(s): " + ", ".join(existing))
    play_when_silent = active_virtualization_value()
    pcm_compression = pcm_compression_value()
    old_sounds = old_wave_inventory(catalog)

    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ") + f"_{os.getpid()}"
    journal_path = BATCH / f"city_bgm_import_{run_id}.json"
    if journal_path.exists():
        raise RuntimeError(f"refusing to overwrite import journal: {journal_path}")
    journal = {"schema": "rok.city_bgm_import.v1", "status": "preflight_passed_import_pending",
               "created_utc": datetime.now(timezone.utc).isoformat(), "project": project.as_posix(),
               "map": world.get_name(), "catalog_path": repo_path(CATALOG), "catalog_sha256": catalog_sha,
               "input_pins": input_pins, "target_directory": TARGET_DIR,
               "requested_soundwave_properties": {"looping": False, "volume": 1.0, "pitch": 1.0,
                                                    "virtualization_mode": "PlayWhenSilent",
                                                    "sound_asset_compression_type": "PCM"},
               "old_router_soundwaves_read_only": old_sounds, "created_assets": [], "inflight_target": None,
               "asset_results": [], "failure": None,
               "qualification": "UE asset import/readback only; no audio playback/audible acceptance, no map save, no routing changes"}
    BATCH.mkdir(parents=True, exist_ok=True)
    with journal_path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(journal, stream, ensure_ascii=False, indent=2); stream.write("\n")

    journal["status"] = "importing"
    journal["destination_directory_created_by_importer"] = False
    write_journal(journal_path, journal)
    try:
        if not unreal.EditorAssetLibrary.does_directory_exist(TARGET_DIR):
            if not unreal.EditorAssetLibrary.make_directory(TARGET_DIR):
                raise RuntimeError(f"Could not create intended BGM destination folder: {TARGET_DIR}")
            journal["destination_directory_created_by_importer"] = True
            write_journal(journal_path, journal)
        native_clips = {clip["name"]: clip for clip in input_pins["clips"]}
        for name in target_names:
            if target_exists(name):
                raise RuntimeError(f"Target appeared after preflight; refusing overwrite: {name}")
            source = native_clips[name]
            wav_path = ROOT / source["wav_path"]
            target = f"{TARGET_DIR}/{name}"
            journal["inflight_target"] = {"name": name, "asset": target, "wav_sha256": source["wav_sha256"]}
            write_journal(journal_path, journal)

            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str(wav_path))
            task.set_editor_property("destination_path", TARGET_DIR)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", False)
            task.set_editor_property("save", True)
            task.set_editor_property("factory", unreal.SoundFactory())
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            sound = unreal.load_asset(target)
            if sound is None or not isinstance(sound, unreal.SoundWave):
                raise RuntimeError(f"WAV did not import as SoundWave: {target}")
            sound.set_editor_property("looping", False)
            sound.set_editor_property("volume", 1.0)
            sound.set_editor_property("pitch", 1.0)
            sound.set_editor_property("virtualization_mode", play_when_silent)
            # Preserve the native decoded PCM representation; do not inherit a project codec.
            sound.set_sound_asset_compression_type(pcm_compression)
            if not unreal.EditorAssetLibrary.save_loaded_asset(sound):
                raise RuntimeError(f"Could not save imported SoundWave: {target}")
            sound = unreal.load_asset(target)
            props = sound_properties(sound)
            if (props["looping"] is not False or props["volume"] != 1.0 or props["pitch"] != 1.0 or
                    "PLAY_WHEN_SILENT" not in props["virtualization_mode"].upper() and "PLAYWHENSILENT" not in props["virtualization_mode"].upper()):
                raise RuntimeError(f"SoundWave property readback mismatch: {target}: {props}")
            if "PCM" not in props["sound_asset_compression_type"].upper():
                raise RuntimeError(f"SoundWave compression readback mismatch: {target}: {props}")
            duration_tolerance = native_duration_tolerance(source["sample_rate"], source["duration_seconds"])
            if (props["channels"] != 1 or props["sample_rate"] != source["sample_rate"] or
                    props["duration_seconds"] <= 0 or
                    abs(props["duration_seconds"] - source["duration_seconds"]) > duration_tolerance):
                raise RuntimeError(f"SoundWave source format readback mismatch: {target}: {props}")
            result = {**source, "ue_object_path": target + "." + name, "properties": props,
                      "native_duration_tolerance_seconds": duration_tolerance,
                      "package_files": disk_package_files(props["asset"])}
            journal["created_assets"].append(target)
            journal["asset_results"].append(result)
            journal["inflight_target"] = None
            write_journal(journal_path, journal)
        journal["status"] = "imported_and_readback_passed"
        journal["finished_utc"] = datetime.now(timezone.utc).isoformat()
        write_journal(journal_path, journal)
        unreal.log("ROK_CITY_BGM_IMPORT " + json.dumps({"status": journal["status"], "manifest": repo_path(journal_path)}))
        return 0
    except Exception as exc:
        journal["status"] = "failed_partial_import_preserved"
        journal["failure"] = f"{type(exc).__name__}: {exc}"
        journal["finished_utc"] = datetime.now(timezone.utc).isoformat()
        write_journal(journal_path, journal)
        raise


if __name__ == "__main__":
    raise SystemExit(main())
