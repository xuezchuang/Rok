#!/usr/bin/env python3
"""Verify the isolated Unity city-BGM PCM export and its pinned sources."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
import tempfile
import wave
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityAudioProbeV1"
BATCH = ROOT / "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial"
PREPARE_REPORT = BATCH / "city_audio_probe_prepare.json"
PREPARE_REPORT_REVISION2 = BATCH / "city_audio_probe_prepare_revision2.json"
PREPARE_REPORT_REVISION3 = BATCH / "city_audio_probe_prepare_revision3.json"
PREPARE_REPORT_REVISION4 = BATCH / "city_audio_probe_prepare_revision4.json"
PREPARE_REPORT_REVISION5 = BATCH / "city_audio_probe_prepare_revision5.json"
PREPARE_REPORT_REVISION6 = BATCH / "city_audio_probe_prepare_revision6.json"
PREPARE_REPORT_REVISION7 = BATCH / "city_audio_probe_prepare_revision7.json"
PREPARE_REPORT_REVISION8 = BATCH / "city_audio_probe_prepare_revision8.json"
PREPARE_REPORT_REVISION9 = BATCH / "city_audio_probe_prepare_revision9.json"
PREPARE_REPORT_REVISION10 = BATCH / "city_audio_probe_prepare_revision10.json"
PREPARE_REPORT_REVISION11 = BATCH / "city_audio_probe_prepare_revision11.json"
PREPARE_REPORT_REVISION12 = BATCH / "city_audio_probe_prepare_revision12.json"
PREPARE_REPORT_REVISION13 = BATCH / "city_audio_probe_prepare_revision13.json"
PREPARE_REPORT_REVISION14 = BATCH / "city_audio_probe_prepare_revision14.json"
TRACK_NAMES = ("Bgm_PeaceDay01", "Bgm_PeaceDay02", "Bgm_PeaceNight01", "Bgm_PeaceNight02")


def fail(message: str) -> None:
    raise ValueError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def contained(path: Path, parent: Path) -> Path:
    resolved = path.resolve()
    if not resolved.is_relative_to(parent.resolve()):
        fail(f"path escapes expected root: {path}")
    return resolved


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def active_prepare_report() -> Path:
    for revision in range(14, 1, -1):
        candidate = BATCH / f"city_audio_probe_prepare_revision{revision}.json"
        if candidate.is_file():
            return candidate
    return PREPARE_REPORT


def verify_prepared() -> dict:
    selected_report = active_prepare_report()
    report = load_json(selected_report)
    contract_path = contained(ROOT / report["contract"]["path"], PROBE)
    contract = load_json(contract_path)
    if report["status"] != "PREPARED_NO_UNITY_RUN" or contract["schema"] != "rok.unity_city_audio_probe.contract.v2":
        fail("prepare report/contract schema or state mismatch")
    script_pins = {row["path"]: row["sha256"] for row in report.get("scripts", [])}
    for rel in ("Scripts/PrepareRokUnityCityAudioProbe.py", "Scripts/VerifyRokUnityCityAudioProbe.py"):
        if script_pins.get(rel) != sha(ROOT / rel):
            fail(f"prepare report script pin stale: {rel}")
    if sha(contract_path) != report["contract"]["sha256"] or len(contract["tracks"]) != 4:
        fail("contract hash/track count mismatch")
    exporter_path = contained(ROOT / report["exporter"]["path"], PROBE)
    if not exporter_path.is_file() or sha(exporter_path) != report["exporter"]["sha256"]:
        fail("isolated exporter source hash differs from the prepared report")
    if len(report["project_settings_files"]) != 19 or len(report["packages"]) != 2:
        fail("unexpected isolated Unity project settings/package count")
    for row in report["project_settings_files"] + report["packages"]:
        path = contained(ROOT / row["path"], PROBE)
        if not path.is_file() or sha(path) != row["sha256"]:
            fail(f"copied Unity project template changed: {row['path']}")
    if (PROBE / "Library").exists():
        # Unity creates this only when root later runs the isolated project.
        library_status = "created by Unity run; excluded from pinned source package"
    else:
        library_status = "absent before Unity run"
    if {track["name"] for track in contract["tracks"]} != set(TRACK_NAMES):
        fail("contract track names do not match the four AudioInfo-linked city tracks")
    for track in contract["tracks"]:
        if str(track.get("source_import_settings", {}).get("loadType")) != "2":
            fail(f"source meta is not pinned to Streaming for decode-only comparison: {track['name']}")
        for key in ("source_path", "meta_path"):
            src = contained(ROOT / track[key], ROOT)
            expected = track["source_sha256"] if key == "source_path" else track["meta_sha256"]
            if not src.is_file() or sha(src) != expected:
                fail(f"Unity source input drift: {track[key]}")
        copied = contained(PROBE / track["unity_asset_path"], PROBE)
        copied_meta = Path(str(copied) + ".meta")
        if sha(copied) != track["source_sha256"]:
            fail(f"isolated Unity MP3 copy differs: {track['name']}")
    state_path = PROBE / "probe_state.json"
    if state_path.is_file():
        state = load_json(state_path)
        if state.get("schema") != "rok.unity_city_audio_probe.state.v1" or state.get("contract_sha256") != sha(contract_path):
            fail("probe_state does not match the current pinned contract")
        if len(state.get("clips", [])) != 4:
            fail("probe_state must retain all four original importer-setting records")
        by_name = {row["name"]: row for row in state["clips"]}
        for track in contract["tracks"]:
            saved = by_name.get(track["name"])
            if not saved or saved.get("source_meta_sha256") != track["meta_sha256"]:
                fail(f"probe_state source meta pin mismatch: {track['name']}")
            meta_path = PROBE / track["unity_asset_path"]
            meta_text = Path(str(meta_path) + ".meta").read_text(encoding="utf-8-sig")
            if f"guid: {track['guid']}" not in meta_text or f"ROK_CITY_AUDIO_PROBE_OVERRIDE_V1|{track['meta_sha256']}" not in meta_text:
                fail(f"isolated importer override marker/guid missing: {track['name']}")
            if not re.search(r"^    loadType: 0$", meta_text, re.M):
                fail(f"isolated importer default is not DecompressOnLoad: {track['name']}")
        override_state = "isolated DecompressOnLoad override recorded"
    else:
        for track in contract["tracks"]:
            copied_meta = PROBE / (track["unity_asset_path"] + ".meta")
            if sha(copied_meta) != track["meta_sha256"]:
                fail(f"pre-run isolated meta is not byte-identical to source: {track['name']}")
        override_state = "not yet applied; isolated source meta is exact"
    return {"status": "prepared_sources_verified", "prepare_report": selected_report.relative_to(ROOT).as_posix(), "tracks": len(contract["tracks"]),
            "project_settings": 19, "packages": 2, "unity_library": library_status, "importer_override": override_state,
            "contract_sha256": sha(contract_path)}


def pcm_to_i16(value: float) -> int:
    clipped = min(1.0, max(-1.0, value))
    # Mathf uses float32 operands and rounds the product back to float32 before midpoint-to-even.
    clipped_f32 = struct.unpack("<f", struct.pack("<f", clipped))[0]
    scale_f32 = struct.unpack("<f", struct.pack("<f", 32767.0))[0]
    product_f32 = struct.unpack("<f", struct.pack("<f", clipped_f32 * scale_f32))[0]
    return int(round(product_f32))


def verify_run(run_dir: Path, verify_preparation: bool = True) -> dict:
    if verify_preparation:
        verify_prepared()
    run_dir = contained(run_dir, PROBE / "OutputRuns")
    manifest_path = run_dir / "native_city_audio.json"
    if not manifest_path.is_file():
        fail(f"missing native Unity export manifest: {manifest_path}")
    manifest = load_json(manifest_path)
    contract_path = PROBE / "probe_contract.json"
    contract = load_json(contract_path)
    if manifest.get("schema") != "rok.unity_city_audio_probe.native.v2":
        fail("native manifest schema mismatch")
    if manifest.get("unity_project", "").replace("\\", "/").rstrip("/") != PROBE.as_posix():
        fail("native export was not run in the isolated Unity project")
    if manifest.get("repository_root", "").replace("\\", "/").rstrip("/") != ROOT.as_posix():
        fail("native source repository identity mismatch")
    if manifest.get("unity_version") != "2019.4.28f1c1" or manifest.get("contract_sha256") != sha(contract_path):
        fail("Unity version or contract pin mismatch")
    if (contract.get("schema") != "rok.unity_city_audio_probe.contract.v2" or
            manifest.get("decode_method") != "UnityEditor.AudioImporter DecompressOnLoad override followed by AudioClip.GetData" or
            "does not reproduce source Streaming playback" not in manifest.get("decode_scope", "")):
        fail("decode-only qualification/override policy is missing")
    selected_report = active_prepare_report()
    prepare = load_json(selected_report)
    exporter_path = contained(ROOT / prepare["exporter"]["path"], PROBE)
    if manifest.get("exporter_sha256") != sha(exporter_path) or manifest.get("exporter_sha256") != prepare["exporter"]["sha256"]:
        fail("native manifest exporter hash does not match prepared exporter source")
    state_path = PROBE / "probe_state.json"
    if not state_path.is_file() or manifest.get("probe_state_sha256") != sha(state_path):
        fail("manifest does not pin the persistent original importer state")
    state = load_json(state_path)
    if state.get("contract_sha256") != sha(contract_path) or len(state.get("clips", [])) != 4:
        fail("persistent importer state mismatch")
    state_by_name = {row["name"]: row for row in state["clips"]}
    clips = manifest.get("clips")
    if not isinstance(clips, list) or len(clips) != 4 or {row.get("name") for row in clips} != set(TRACK_NAMES):
        fail("native manifest must contain exactly the four pinned audio clips")
    expected = {row["name"]: row for row in contract["tracks"]}
    verified = []
    for clip in clips:
        source = expected[clip["name"]]
        for key in ("audio_info_id", "audio_info_name", "source_path", "source_sha256", "meta_path", "meta_sha256", "guid"):
            if clip.get(key) != source.get(key):
                fail(f"native source pin mismatch {clip['name']}:{key}")
        original = state_by_name.get(clip["name"])
        if not original or original.get("source_meta_sha256") != source["meta_sha256"]:
            fail(f"original importer state missing/source hash mismatch: {clip['name']}")
        marker = f"ROK_CITY_AUDIO_PROBE_OVERRIDE_V1|{source['meta_sha256']}"
        if (clip.get("source_default_settings") != original.get("source_default") or
                clip.get("source_standalone_settings") != original.get("source_standalone") or
                clip.get("source_default_settings", {}).get("load_type") != 2 or
                clip.get("source_standalone_settings", {}).get("load_type") != 2 or
                clip.get("probe_default_settings", {}).get("load_type") != 0 or
                clip.get("probe_standalone_settings", {}).get("load_type") != 0 or
                clip.get("importer_override_marker") != marker or
                clip.get("decode_scope") != "isolated project only; source Streaming playback not reproduced"):
            fail(f"isolated decode override missing/unrecorded or source settings drift: {clip['name']}")
        if clip["channels"] != 1 or not clip["importer_force_to_mono"] or clip["source_meta_normalize"]:
            fail(f"Unity importer channel/normalization differs from source meta: {clip['name']}")
        frames, frequency = int(clip["sample_frames"]), int(clip["frequency"])
        length = float(clip["length_seconds"])
        if frames <= 0 or frequency <= 0 or not math.isfinite(length) or abs(length - frames / frequency) > 1.0 / frequency + 1e-6:
            fail(f"AudioClip sample count/frequency/duration mismatch: {clip['name']}")

        pcm_path = contained(run_dir / clip["pcm_f32le_path"], run_dir)
        if not pcm_path.is_file():
            fail(f"missing Unity GetData float PCM: {clip['name']}")
        pcm = pcm_path.read_bytes()
        if len(pcm) != frames * 4 or len(pcm) != clip["pcm_f32le_bytes"] or hashlib.sha256(pcm).hexdigest() != clip["pcm_f32le_sha256"]:
            fail(f"Unity GetData float PCM hash/length mismatch: {clip['name']}")
        samples = [row[0] for row in struct.iter_unpack("<f", pcm)]
        if any(not math.isfinite(value) for value in samples) or not clip["all_samples_finite"]:
            fail(f"non-finite GetData PCM: {clip['name']}")
        peak = max((abs(value) for value in samples), default=0.0)
        rms = math.sqrt(sum(value * value for value in samples) / frames)
        if abs(peak - float(clip["peak_abs"])) > 1e-5 or abs(rms - float(clip["rms"])) > 1e-6:
            fail(f"PCM statistics differ from native manifest: {clip['name']}")

        wav_path = contained(run_dir / clip["wav_path"], run_dir)
        wav_bytes = wav_path.read_bytes()
        if hashlib.sha256(wav_bytes).hexdigest() != clip["wav_sha256"] or len(wav_bytes) != clip["wav_bytes"]:
            fail(f"WAV hash/byte count mismatch: {clip['name']}")
        if wav_bytes[:4] != b"RIFF" or wav_bytes[8:16] != b"WAVEfmt " or wav_bytes[36:40] != b"data":
            fail(f"WAV container header invalid: {clip['name']}")
        with wave.open(str(wav_path), "rb") as audio:
            if (audio.getnchannels(), audio.getsampwidth(), audio.getframerate(), audio.getnframes()) != (1, 2, frequency, frames):
                fail(f"WAV format differs from Unity AudioClip: {clip['name']}")
            wav_pcm = audio.readframes(frames)
        if hashlib.sha256(wav_pcm).hexdigest() != clip["wav_pcm16_sha256"]:
            fail(f"WAV PCM hash mismatch: {clip['name']}")
        expected_pcm16 = b"".join(struct.pack("<h", pcm_to_i16(value)) for value in samples)
        if wav_pcm != expected_pcm16:
            fail(f"WAV PCM16 is not the pinned GetData conversion: {clip['name']}")
        if not any(value != 0.0 for value in samples) or not any(wav_pcm):
            fail(f"Unity export is all-zero PCM: {clip['name']}")
        verified.append({"name": clip["name"], "audio_info_id": clip["audio_info_id"],
                         "channels": 1, "sample_frames": frames, "frequency": frequency,
                         "duration_seconds": length, "pcm_f32le_sha256": clip["pcm_f32le_sha256"],
                         "wav_sha256": clip["wav_sha256"], "wav_path": clip["wav_path"]})
    return {"status": "passed", "run": run_dir.relative_to(ROOT).as_posix(),
            "native_manifest_sha256": sha(manifest_path), "contract_sha256": sha(contract_path),
            "tracks": verified, "qualification": "Unity AudioClip.GetData PCM export; not audible UE playback or city-scene runtime acceptance"}


def _selftest_state(contract: dict) -> dict:
    rows = []
    for track in contract["tracks"]:
        settings = {"load_type": 2, "sample_rate_setting": 2, "sample_rate_override": 44100,
                    "compression_format": 1, "quality": 1.0}
        rows.append({"name": track["name"], "source_meta_sha256": track["meta_sha256"],
                     "source_default": dict(settings), "source_standalone": dict(settings),
                     "source_had_standalone_override": True})
    return {"schema": "rok.unity_city_audio_probe.state.v1", "contract_sha256": sha(PROBE / "probe_contract.json"), "clips": rows}


def _build_selftest_fixture(run_dir: Path, contract: dict, state: dict, case: str) -> None:
    clips = []
    for i, track in enumerate(contract["tracks"]):
        pcm_path = run_dir / "PCM32F" / f"{track['name']}.f32le"
        wav_path = run_dir / "WAV_PCM16" / f"{track['name']}.wav"
        pcm_path.parent.mkdir(parents=True, exist_ok=True)
        wav_path.parent.mkdir(parents=True, exist_ok=True)
        edge = struct.unpack("<f", struct.pack("<f", 1.5 / 32767.0))[0]
        samples = ([0.0, 0.0] if case == "all_zero_pcm" and i == 0 else
                   [edge, -edge] if case == "rounding_edge_wrong_pcm16" and i == 0 else [0.25, -0.5])
        pcm = b"".join(struct.pack("<f", value) for value in samples)
        pcm_path.write_bytes(pcm)
        if case == "rounding_edge_wrong_pcm16" and i == 0:
            # Deliberately reproduces the previous float64 intermediate, which disagrees with Mathf.
            encoded = b"".join(struct.pack("<h", int(round(value * 32767.0))) for value in samples)
        else:
            encoded = b"".join(struct.pack("<h", pcm_to_i16(value)) for value in samples)
        with wave.open(str(wav_path), "wb") as audio:
            audio.setnchannels(1); audio.setsampwidth(2); audio.setframerate(44100); audio.writeframes(encoded)
        state_clip = state["clips"][i]
        clip = {"name": track["name"], "audio_info_id": track["audio_info_id"], "audio_info_name": track["audio_info_name"],
                "source_path": track["source_path"], "source_sha256": track["source_sha256"],
                "meta_path": track["meta_path"], "meta_sha256": track["meta_sha256"], "guid": track["guid"],
                "source_default_settings": state_clip["source_default"], "source_standalone_settings": state_clip["source_standalone"],
                "probe_default_settings": {**state_clip["source_default"], "load_type": 0},
                "probe_standalone_settings": {**state_clip["source_standalone"], "load_type": 0},
                "source_had_standalone_override": True,
                "importer_override_marker": f"ROK_CITY_AUDIO_PROBE_OVERRIDE_V1|{track['meta_sha256']}",
                "decode_scope": "isolated project only; source Streaming playback not reproduced",
                "importer_force_to_mono": True, "source_meta_normalize": False,
                "channels": 2 if case == "wrong_channel_count" and i == 0 else 1,
                "sample_frames": 2, "frequency": 44100, "length_seconds": 2 / 44100,
                "pcm_f32le_path": f"PCM32F/{track['name']}.f32le", "pcm_f32le_bytes": len(pcm),
                "pcm_f32le_sha256": hashlib.sha256(pcm).hexdigest(), "wav_path": f"WAV_PCM16/{track['name']}.wav",
                "wav_bytes": wav_path.stat().st_size, "wav_sha256": sha(wav_path),
                "wav_pcm16_sha256": hashlib.sha256(encoded).hexdigest(),
                "peak_abs": max(abs(value) for value in samples),
                "rms": math.sqrt(sum(value * value for value in samples) / len(samples)), "all_samples_finite": True}
        if case == "unrecorded_override" and i == 0:
            clip["probe_standalone_settings"]["load_type"] = 2
        if case == "missing_pcm" and i == 0:
            clip["pcm_f32le_path"] = "PCM32F/missing.f32le"
        clips.append(clip)
    manifest = {"schema": "rok.unity_city_audio_probe.native.v2", "unity_project": PROBE.as_posix(),
                "repository_root": ROOT.as_posix(), "unity_version": "2019.4.28f1c1",
                "contract_sha256": sha(PROBE / "probe_contract.json"), "probe_state_sha256": sha(PROBE / "probe_state.json"),
                "exporter_sha256": sha(PROBE / "Assets/Editor/ExportRokUnityCityAudioProbe.cs"),
                "decode_method": "UnityEditor.AudioImporter DecompressOnLoad override followed by AudioClip.GetData",
                "decode_scope": "isolated project only; this decodes source MP3 content and does not reproduce source Streaming playback",
                "clips": clips}
    (run_dir / "native_city_audio.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def run_negative_selftests() -> dict:
    state_path = PROBE / "probe_state.json"
    if state_path.exists():
        fail("negative self-tests must run before Unity creates probe_state.json; refusing to mutate native state")
    contract = load_json(PROBE / "probe_contract.json")
    state = _selftest_state(contract)
    state_path.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
    cases = {"missing_pcm": "missing Unity GetData float PCM",
             "all_zero_pcm": "Unity export is all-zero PCM",
             "wrong_channel_count": "Unity importer channel/normalization differs",
             "unrecorded_override": "isolated decode override missing/unrecorded",
             "rounding_edge_wrong_pcm16": "WAV PCM16 is not the pinned GetData conversion"}
    observed = []
    try:
        with tempfile.TemporaryDirectory(prefix=".city_audio_negative_", dir=PROBE / "OutputRuns") as temp:
            for case, expected_error in cases.items():
                run_dir = Path(temp) / case
                run_dir.mkdir()
                _build_selftest_fixture(run_dir, contract, state, case)
                try:
                    verify_run(run_dir, verify_preparation=False)
                except (ValueError, OSError, KeyError, TypeError) as exc:
                    if expected_error not in str(exc):
                        fail(f"negative test {case} failed for unexpected reason: {exc}")
                    observed.append({"case": case, "rejected": True, "expected_error": expected_error})
                else:
                    fail(f"negative test unexpectedly passed: {case}")
    finally:
        state_path.unlink(missing_ok=True)
    edge = struct.unpack("<f", struct.pack("<f", 1.5 / 32767.0))[0]
    if pcm_to_i16(edge) != 2 or pcm_to_i16(-edge) != -2:
        fail("Unity Mathf float32 midpoint-to-even quantization edge mismatch")
    return {"status": "passed", "tests": observed, "test_count": len(observed),
            "rounding_edge_positive_negative_pcm16": [pcm_to_i16(edge), pcm_to_i16(-edge)],
            "scope": "Python verifier synthetic fixtures only; no Unity/UE run or source mutation"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--prepared", action="store_true")
    group.add_argument("--run", type=Path)
    group.add_argument("--negative-self-test", action="store_true")
    args = parser.parse_args()
    try:
        result = verify_prepared() if args.prepared else run_negative_selftests() if args.negative_self_test else verify_run(args.run)
    except (ValueError, KeyError, TypeError, OSError, json.JSONDecodeError, struct.error, wave.Error) as exc:
        print(json.dumps({"status": "failed", "error": str(exc)}, ensure_ascii=False, indent=2))
        return 1
    if args.negative_self_test:
        out = BATCH / "city_audio_probe_negative_selftest_revision14.json"
    elif args.prepared:
        out_name = ("city_audio_probe_prepare_verify_revision14.json" if PREPARE_REPORT_REVISION14.is_file()
                    else "city_audio_probe_prepare_verify_revision13.json" if PREPARE_REPORT_REVISION13.is_file()
                    else "city_audio_probe_prepare_verify_revision12.json" if PREPARE_REPORT_REVISION12.is_file()
                    else "city_audio_probe_prepare_verify_revision11.json" if PREPARE_REPORT_REVISION11.is_file()
                    else "city_audio_probe_prepare_verify_revision10.json" if PREPARE_REPORT_REVISION10.is_file()
                    else "city_audio_probe_prepare_verify_revision9.json" if PREPARE_REPORT_REVISION9.is_file()
                    else "city_audio_probe_prepare_verify_revision8.json" if PREPARE_REPORT_REVISION8.is_file()
                    else "city_audio_probe_prepare_verify_revision7.json" if PREPARE_REPORT_REVISION7.is_file()
                    else "city_audio_probe_prepare_verify_revision6.json" if PREPARE_REPORT_REVISION6.is_file()
                    else "city_audio_probe_prepare_verify_revision5.json" if PREPARE_REPORT_REVISION5.is_file()
                    else "city_audio_probe_prepare_verify_revision4.json" if PREPARE_REPORT_REVISION4.is_file()
                    else "city_audio_probe_prepare_verify_revision3.json" if PREPARE_REPORT_REVISION3.is_file()
                    else "city_audio_probe_prepare_verify_revision2.json" if PREPARE_REPORT_REVISION2.is_file()
                    else "city_audio_probe_prepare_verify.json")
        out = BATCH / out_name
    else:
        out = BATCH / f"city_audio_probe_native_verify_{args.run.name}.json"
    if out.exists():
        print(json.dumps({"status": "failed", "error": f"refusing to overwrite {out}"}, ensure_ascii=False))
        return 1
    result.update({"schema": "rok.unity_city_audio_probe.verify.v1", "verified_utc": datetime.now(timezone.utc).isoformat(),
                   "verifier": "Scripts/VerifyRokUnityCityAudioProbe.py", "verifier_sha256": sha(Path(__file__))})
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": result["status"], "report": out.relative_to(ROOT).as_posix(), "sha256": sha(out)}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
