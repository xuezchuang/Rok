#!/usr/bin/env python3
"""Prepare an isolated Unity 2019 audio decode probe without launching Unity."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path
from rok_data_paths import DATA_ROOT, label, resolve

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityAudioProbeV1"
BATCH = ROOT / "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial"
REPORT = BATCH / "city_audio_probe_prepare.json"
REPORT_REVISION2 = BATCH / "city_audio_probe_prepare_revision2.json"
REPORT_REVISION3 = BATCH / "city_audio_probe_prepare_revision3.json"
REPORT_REVISION4 = BATCH / "city_audio_probe_prepare_revision4.json"
REPORT_REVISION5 = BATCH / "city_audio_probe_prepare_revision5.json"
REPORT_REVISION6 = BATCH / "city_audio_probe_prepare_revision6.json"
REPORT_REVISION7 = BATCH / "city_audio_probe_prepare_revision7.json"
REPORT_REVISION8 = BATCH / "city_audio_probe_prepare_revision8.json"
REPORT_REVISION9 = BATCH / "city_audio_probe_prepare_revision9.json"
REPORT_REVISION10 = BATCH / "city_audio_probe_prepare_revision10.json"
REPORT_REVISION11 = BATCH / "city_audio_probe_prepare_revision11.json"
REPORT_REVISION12 = BATCH / "city_audio_probe_prepare_revision12.json"
REPORT_REVISION13 = BATCH / "city_audio_probe_prepare_revision13.json"
REPORT_REVISION14 = BATCH / "city_audio_probe_prepare_revision14.json"
BASE = ROOT / "Saved/UnityExpeditionAnimationProbeV1"
BASE_REPORT = ROOT / "Saved/RokUnityMigration/BatchReview20261009NightTimeAndExpedition/expedition_animation_probe_prepare_revision3.json"
AUDIOINFO = DATA_ROOT / "ConfigTables/AudioInfo.json"
WORKBOOK = ROOT / "ref/config-tables/s_AudioInfo.xlsm"
UNITY_VERSION = "2019.4.28f1c1"
TRACKS = [
    ("Bgm_PeaceDay01", 1007, "Bgm_PeaceDay"),
    ("Bgm_PeaceDay02", 1007, "Bgm_PeaceDay"),
    ("Bgm_PeaceNight01", 1008, "Bgm_PeaceNight"),
    ("Bgm_PeaceNight02", 1008, "Bgm_PeaceNight"),
]

EXPORTER = r'''using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using UnityEditor;
using UnityEngine;

public static class ExportRokUnityCityAudioProbe
{
    [Serializable] private class Contract { public string schema; public string unity_version_target; public Track[] tracks; }
    [Serializable] private class Track { public string name; public int audio_info_id; public string audio_info_name; public string source_path; public string source_sha256; public string meta_path; public string meta_sha256; public string guid; public string unity_asset_path; public bool force_to_mono; public bool normalize; }
    [Serializable] private class Root { public string schema; public string created_utc; public int pid; public string unity_project; public string repository_root; public string unity_version; public string contract_sha256; public string probe_state_sha256; public string exporter_sha256; public string decode_method; public string decode_scope; public string importer_override_policy; public Clip[] clips; }
    [Serializable] private class SampleSettingsRecord { public int load_type; public int sample_rate_setting; public uint sample_rate_override; public int compression_format; public float quality; }
    [Serializable] private class StateClip { public string name; public string source_meta_sha256; public SampleSettingsRecord source_default; public SampleSettingsRecord source_standalone; public bool source_had_standalone_override; }
    [Serializable] private class ProbeState { public string schema; public string contract_sha256; public StateClip[] clips; }
    [Serializable] private class Clip { public string name; public int audio_info_id; public string audio_info_name; public string source_path; public string source_sha256; public string meta_path; public string meta_sha256; public string guid; public string asset_path; public SampleSettingsRecord source_default_settings; public SampleSettingsRecord source_standalone_settings; public SampleSettingsRecord probe_default_settings; public SampleSettingsRecord probe_standalone_settings; public bool source_had_standalone_override; public string importer_override_marker; public string decode_scope; public bool importer_force_to_mono; public bool source_meta_normalize; public int channels; public int sample_frames; public int frequency; public double length_seconds; public string pcm_f32le_path; public long pcm_f32le_bytes; public string pcm_f32le_sha256; public string wav_path; public long wav_bytes; public string wav_sha256; public string wav_pcm16_sha256; public float peak_abs; public double rms; public bool all_samples_finite; }

    [MenuItem("Rok/City Audio Probe/Export PCM")]
    public static void Run()
    {
        int exitCode = 0;
        try { Export(); }
        catch (Exception ex) { Debug.LogException(ex); exitCode = 1; }
        finally { EditorApplication.Exit(exitCode); }
    }

    private static void Export()
    {
        string project = Path.GetFullPath(Directory.GetParent(Application.dataPath).FullName);
        string root = Path.GetFullPath(Path.Combine(project, "..", ".."));
        if (root.Replace('\\', '/') != "D:/ueDemo/Rok") throw new InvalidOperationException("Wrong isolated project root: " + root);
        if (Application.unityVersion != "2019.4.28f1c1") throw new InvalidOperationException("Unexpected Unity version: " + Application.unityVersion);
        string contractPath = Path.Combine(project, "probe_contract.json");
        byte[] contractBytes = File.ReadAllBytes(contractPath);
        var contract = JsonUtility.FromJson<Contract>(Encoding.UTF8.GetString(contractBytes));
        if (contract == null || contract.tracks == null || contract.tracks.Length != 4) throw new InvalidOperationException("Contract must contain exactly four tracks");
        string outputRoot = Path.Combine(project, "OutputRuns");
        Directory.CreateDirectory(outputRoot);
        string runName = DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff", CultureInfo.InvariantCulture) + "_" + System.Diagnostics.Process.GetCurrentProcess().Id.ToString(CultureInfo.InvariantCulture);
        string run = Path.Combine(outputRoot, runName);
        Directory.CreateDirectory(run);
        string pcmDir = Path.Combine(run, "PCM32F"); Directory.CreateDirectory(pcmDir);
        string wavDir = Path.Combine(run, "WAV_PCM16"); Directory.CreateDirectory(wavDir);
        var result = new Root { schema = "rok.unity_city_audio_probe.native.v2", created_utc = DateTime.UtcNow.ToString("o", CultureInfo.InvariantCulture), pid = System.Diagnostics.Process.GetCurrentProcess().Id, unity_project = project.Replace('\\', '/'), repository_root = root.Replace('\\', '/'), unity_version = Application.unityVersion, contract_sha256 = Sha(contractBytes), decode_method = "UnityEditor.AudioImporter DecompressOnLoad override followed by AudioClip.GetData", decode_scope = "isolated project only; this decodes source MP3 content and does not reproduce source Streaming playback", importer_override_policy = "preserve source default/Standalone sample settings; set only loadType=DecompressOnLoad in isolated copy; source MP3/meta remain hash-pinned and read-only", clips = new Clip[contract.tracks.Length] };

        string statePath = Path.Combine(project, "probe_state.json");
        ProbeState state = null;
        if (File.Exists(statePath))
        {
            state = JsonUtility.FromJson<ProbeState>(File.ReadAllText(statePath, Encoding.UTF8));
            if (state == null || state.schema != "rok.unity_city_audio_probe.state.v1" || state.contract_sha256 != Sha(contractBytes) || state.clips == null || state.clips.Length != contract.tracks.Length)
                throw new InvalidOperationException("Existing probe_state.json is invalid or belongs to another source contract");
        }
        else
        {
            state = new ProbeState { schema = "rok.unity_city_audio_probe.state.v1", contract_sha256 = Sha(contractBytes), clips = new StateClip[contract.tracks.Length] };
            for (int i = 0; i < contract.tracks.Length; i++)
            {
                Track item = contract.tracks[i];
                string asset = item.unity_asset_path;
                if (Sha(File.ReadAllBytes(Path.Combine(project, asset.Replace('/', Path.DirectorySeparatorChar)) + ".meta")) != item.meta_sha256)
                    throw new InvalidOperationException("Initial isolated importer meta is not byte-identical to source: " + item.name);
                var importer = AssetImporter.GetAtPath(asset) as AudioImporter;
                if (importer == null) throw new InvalidOperationException("AudioImporter missing while recording source settings: " + item.name);
                AudioImporterSampleSettings defaults = importer.defaultSampleSettings;
                AudioImporterSampleSettings standalone = importer.GetOverrideSampleSettings("Standalone");
                if (defaults.loadType != AudioClipLoadType.Streaming || standalone.loadType != AudioClipLoadType.Streaming)
                    throw new InvalidOperationException("Expected source Streaming settings before isolated decode override: " + item.name);
                state.clips[i] = new StateClip { name = item.name, source_meta_sha256 = item.meta_sha256,
                    source_default = Record(defaults), source_standalone = Record(standalone),
                    source_had_standalone_override = importer.ContainsSampleSettingsOverride("Standalone") };
            }
            using (var stateStream = new FileStream(statePath, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            using (var writer = new StreamWriter(stateStream, new UTF8Encoding(false))) writer.Write(JsonUtility.ToJson(state, true) + "\n");
        }
        result.probe_state_sha256 = Sha(File.ReadAllBytes(statePath));
        result.exporter_sha256 = Sha(File.ReadAllBytes(Path.Combine(project, "Assets/Editor/ExportRokUnityCityAudioProbe.cs")));

        for (int i = 0; i < contract.tracks.Length; i++)
        {
            Track item = contract.tracks[i];
            string source = Path.GetFullPath(Path.Combine(root, item.source_path.Replace('/', Path.DirectorySeparatorChar)));
            string meta = Path.GetFullPath(Path.Combine(root, item.meta_path.Replace('/', Path.DirectorySeparatorChar)));
            if (!File.Exists(source) || Sha(File.ReadAllBytes(source)) != item.source_sha256) throw new InvalidOperationException("Pinned MP3 changed: " + item.name);
            if (!File.Exists(meta) || Sha(File.ReadAllBytes(meta)) != item.meta_sha256) throw new InvalidOperationException("Pinned meta changed: " + item.name);
            string asset = item.unity_asset_path;
            string absoluteAsset = Path.GetFullPath(Path.Combine(project, asset.Replace('/', Path.DirectorySeparatorChar)));
            Directory.CreateDirectory(Path.GetDirectoryName(absoluteAsset));
            string absoluteMeta = absoluteAsset + ".meta";
            if (File.Exists(absoluteAsset))
            {
                if (Sha(File.ReadAllBytes(absoluteAsset)) != item.source_sha256) throw new InvalidOperationException("Existing isolated MP3 differs from source: " + item.name);
            }
            else File.Copy(source, absoluteAsset, false);
            if (!File.Exists(absoluteMeta)) File.Copy(meta, absoluteMeta, false);
            AssetDatabase.ImportAsset(asset, ImportAssetOptions.ForceSynchronousImport);
            var importer = AssetImporter.GetAtPath(asset) as AudioImporter;
            if (importer == null) throw new InvalidOperationException("AudioImporter missing: " + item.name);
            StateClip saved = state.clips[i];
            string marker = "ROK_CITY_AUDIO_PROBE_OVERRIDE_V1|" + item.meta_sha256;
            if (saved == null || saved.name != item.name || saved.source_meta_sha256 != item.meta_sha256)
                throw new InvalidOperationException("No matching pinned source importer state: " + item.name);
            if (importer.userData.Length != 0 && importer.userData != marker)
                throw new InvalidOperationException("Unexpected isolated importer userData marker: " + item.name);
            AudioImporterSampleSettings defaultsNow = importer.defaultSampleSettings;
            AudioImporterSampleSettings standaloneNow = importer.GetOverrideSampleSettings("Standalone");
            if (!EquivalentExceptLoadType(defaultsNow, saved.source_default) || !EquivalentExceptLoadType(standaloneNow, saved.source_standalone))
                throw new InvalidOperationException("Isolated importer settings changed beyond the allowed loadType override: " + item.name);
            if (importer.userData == marker && (defaultsNow.loadType != AudioClipLoadType.DecompressOnLoad || standaloneNow.loadType != AudioClipLoadType.DecompressOnLoad))
                throw new InvalidOperationException("Recorded override marker does not match active DecompressOnLoad settings: " + item.name);
            defaultsNow.loadType = AudioClipLoadType.DecompressOnLoad;
            standaloneNow.loadType = AudioClipLoadType.DecompressOnLoad;
            importer.defaultSampleSettings = defaultsNow;
            if (!importer.SetOverrideSampleSettings("Standalone", standaloneNow))
                throw new InvalidOperationException("Could not set isolated Standalone DecompressOnLoad override: " + item.name);
            importer.userData = marker;
            importer.SaveAndReimport();
            importer = AssetImporter.GetAtPath(asset) as AudioImporter;
            if (importer == null || importer.defaultSampleSettings.loadType != AudioClipLoadType.DecompressOnLoad || importer.GetOverrideSampleSettings("Standalone").loadType != AudioClipLoadType.DecompressOnLoad || importer.userData != marker)
                throw new InvalidOperationException("DecompressOnLoad override did not persist after reimport: " + item.name);
            if (importer.forceToMono != item.force_to_mono || item.normalize || !importer.forceToMono)
                throw new InvalidOperationException("Source meta mono/normalization contract changed: " + item.name);
            var clip = AssetDatabase.LoadAssetAtPath<AudioClip>(asset);
            if (clip == null) throw new InvalidOperationException("AudioClip missing after decode override reimport: " + item.name);
            if (clip.channels != 1) throw new InvalidOperationException("Expected Unity-imported mono clip: " + item.name + " channels=" + clip.channels);
            clip.LoadAudioData();
            int frames = clip.samples;
            if (frames <= 0 || clip.frequency <= 0) throw new InvalidOperationException("Invalid Unity AudioClip dimensions: " + item.name);
            var samples = new float[frames];
            if (!clip.GetData(samples, 0)) throw new InvalidOperationException("AudioClip.GetData failed: " + item.name);
            if (!BitConverter.IsLittleEndian) throw new InvalidOperationException("This exporter requires little-endian PCM output");
            string pcmPath = Path.Combine(pcmDir, item.name + ".f32le");
            string wavPath = Path.Combine(wavDir, item.name + ".wav");
            float peak = 0f; double sumSq = 0.0; bool finite = true;
            using (var raw = new BinaryWriter(new FileStream(pcmPath, FileMode.CreateNew, FileAccess.Write, FileShare.None)))
            using (var wavWriter = new BinaryWriter(new FileStream(wavPath, FileMode.CreateNew, FileAccess.Write, FileShare.None)))
            {
                long dataBytes = (long)samples.Length * 2;
                wavWriter.Write(Encoding.ASCII.GetBytes("RIFF")); wavWriter.Write((int)(36 + dataBytes)); wavWriter.Write(Encoding.ASCII.GetBytes("WAVEfmt "));
                wavWriter.Write(16); wavWriter.Write((short)1); wavWriter.Write((short)1); wavWriter.Write(clip.frequency); wavWriter.Write(clip.frequency * 2); wavWriter.Write((short)2); wavWriter.Write((short)16);
                wavWriter.Write(Encoding.ASCII.GetBytes("data")); wavWriter.Write((int)dataBytes);
                for (int n = 0; n < samples.Length; n++)
                {
                    float value = samples[n];
                    if (float.IsNaN(value) || float.IsInfinity(value)) finite = false;
                    peak = Mathf.Max(peak, Mathf.Abs(value)); sumSq += (double)value * value;
                    raw.Write(value);
                    int q = Mathf.RoundToInt(Mathf.Clamp(value, -1f, 1f) * 32767f);
                    wavWriter.Write((short)q);
                }
            }
            if (!finite) throw new InvalidOperationException("Non-finite Unity PCM sample: " + item.name);
            byte[] pcm = File.ReadAllBytes(pcmPath); byte[] wavBytes = File.ReadAllBytes(wavPath);
            using (var stream = new FileStream(wavPath, FileMode.Open, FileAccess.Read, FileShare.Read))
            using (var reader = new BinaryReader(stream)) { stream.Position = 40; byte[] data = reader.ReadBytes((int)((long)frames * 2)); if (data.Length != frames * 2) throw new InvalidOperationException("Truncated WAV data: " + item.name); }
            result.clips[i] = new Clip { name = item.name, audio_info_id = item.audio_info_id, audio_info_name = item.audio_info_name, source_path = item.source_path, source_sha256 = item.source_sha256, meta_path = item.meta_path, meta_sha256 = item.meta_sha256, guid = item.guid, asset_path = asset.Replace('\\', '/'), source_default_settings = saved.source_default, source_standalone_settings = saved.source_standalone, probe_default_settings = Record(importer.defaultSampleSettings), probe_standalone_settings = Record(importer.GetOverrideSampleSettings("Standalone")), source_had_standalone_override = saved.source_had_standalone_override, importer_override_marker = importer.userData, decode_scope = "isolated project only; source Streaming playback not reproduced", importer_force_to_mono = importer.forceToMono, source_meta_normalize = item.normalize, channels = clip.channels, sample_frames = frames, frequency = clip.frequency, length_seconds = clip.length, pcm_f32le_path = "PCM32F/" + item.name + ".f32le", pcm_f32le_bytes = pcm.LongLength, pcm_f32le_sha256 = Sha(pcm), wav_path = "WAV_PCM16/" + item.name + ".wav", wav_bytes = wavBytes.LongLength, wav_sha256 = Sha(wavBytes), wav_pcm16_sha256 = Sha(wavBytes.Skip(44).ToArray()), peak_abs = peak, rms = Math.Sqrt(sumSq / samples.Length), all_samples_finite = finite };
        }
        string manifestPath = Path.Combine(run, "native_city_audio.json");
        File.WriteAllText(manifestPath, JsonUtility.ToJson(result, true) + "\n", new UTF8Encoding(false));
        Debug.Log("ROK_CITY_AUDIO_RUN=" + run.Replace('\\', '/') + " manifest_sha256=" + Sha(File.ReadAllBytes(manifestPath)));
    }

    private static SampleSettingsRecord Record(AudioImporterSampleSettings s) { return new SampleSettingsRecord { load_type = (int)s.loadType, sample_rate_setting = (int)s.sampleRateSetting, sample_rate_override = s.sampleRateOverride, compression_format = (int)s.compressionFormat, quality = s.quality }; }
    private static bool EquivalentExceptLoadType(AudioImporterSampleSettings actual, SampleSettingsRecord expected) { return actual.sampleRateSetting == (AudioSampleRateSetting)expected.sample_rate_setting && actual.sampleRateOverride == expected.sample_rate_override && (int)actual.compressionFormat == expected.compression_format && Math.Abs(actual.quality - expected.quality) < 0.00001f && (actual.loadType == (AudioClipLoadType)expected.load_type || actual.loadType == AudioClipLoadType.DecompressOnLoad); }

    private static string Sha(byte[] data) { using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(data)).Replace("-", "").ToLowerInvariant(); }
}
'''


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def record(path: Path) -> dict:
    return {"path": label(path), "sha256": sha(path), "bytes": path.stat().st_size}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--prepare", action="store_true")
    modes.add_argument("--revise-report", action="store_true")
    args = parser.parse_args()
    if args.revise_report:
        if REPORT_REVISION14.exists() or not REPORT_REVISION13.is_file() or not PROBE.is_dir():
            raise SystemExit("revision14 requires existing prepared probe/revision13 report and refuses overwrite")
        previous = json.loads(REPORT_REVISION13.read_text(encoding="utf-8-sig"))
        contract = PROBE / "probe_contract.json"
        contract_data = json.loads(contract.read_text(encoding="utf-8-sig"))
        for row in previous["source_records"]:
            if sha(resolve(row["path"])) != row["sha256"]:
                raise RuntimeError(f"pinned source changed: {row['path']}")
        for track in previous["tracks"]:
            if sha(resolve(track["source_path"])) != track["source_sha256"] or sha(resolve(track["meta_path"])) != track["meta_sha256"]:
                raise RuntimeError(f"pinned track changed: {track['name']}")
        exporter_path = PROBE / "Assets/Editor/ExportRokUnityCityAudioProbe.cs"
        exporter_path.write_text(EXPORTER, encoding="utf-8")
        if contract_data.get("schema") != "rok.unity_city_audio_probe.contract.v2":
            raise RuntimeError("revision6 expects the v2 decode-only contract")
        revised = dict(previous)
        revised.update({"schema": "rok.unity_city_audio_probe.prepare.v14", "revision": 14,
            "supersedes": {"path": label(REPORT_REVISION13), "sha256": sha(REPORT_REVISION13)},
            "decode_policy": contract_data["decoding_policy"],
            "contract": {"path": label(contract), "sha256": sha(contract)},
            "scripts": [{"path": "Scripts/PrepareRokUnityCityAudioProbe.py", "sha256": sha(ROOT / "Scripts/PrepareRokUnityCityAudioProbe.py")},
                        {"path": "Scripts/VerifyRokUnityCityAudioProbe.py", "sha256": sha(ROOT / "Scripts/VerifyRokUnityCityAudioProbe.py")}],
            "exporter": {"path": "Saved/UnityCityAudioProbeV1/Assets/Editor/ExportRokUnityCityAudioProbe.cs",
                         "sha256": sha(exporter_path),
                         "meta_sha256": sha(PROBE / "Assets/Editor/ExportRokUnityCityAudioProbe.cs.meta")}})
        REPORT_REVISION14.write_text(json.dumps(revised, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({"status": revised["status"], "report": label(REPORT_REVISION14),
                          "report_sha256": sha(REPORT_REVISION14)}, ensure_ascii=False))
        return 0
    if PROBE.exists() or REPORT.exists():
        raise SystemExit("Refusing to overwrite probe or prepare report")
    base_report = json.loads(BASE_REPORT.read_text(encoding="utf-8-sig"))
    if base_report.get("unity_version_target") != UNITY_VERSION or base_report.get("status") not in {
        "SOURCE_EDITOR_BINDINGS_NATIVE2_READY_FOR_VERIFICATION", "NATIVE_SOURCE_CURVE_ORACLE_READY_FOR_VERIFICATION",
    }:
        raise RuntimeError("Pinned isolated Unity 2019 base project report is not in an accepted prepared state")
    settings_rows = base_report["project_settings_files"]
    package_rows = base_report["packages"]
    if len(settings_rows) != 19 or len(package_rows) != 2:
        raise RuntimeError("Expected exactly 19 pinned ProjectSettings files and two Packages files")
    for row in settings_rows + package_rows:
        src = BASE / row["path"]
        if not src.is_file() or sha(src) != row["sha256"]:
            raise RuntimeError(f"Unity probe template drift: {src}")
    version = (BASE / "ProjectSettings/ProjectVersion.txt").read_text(encoding="utf-8-sig")
    if UNITY_VERSION not in version:
        raise RuntimeError("Unity template ProjectVersion differs from pinned Unity 2019.4.28f1c1")

    table = json.loads(AUDIOINFO.read_text(encoding="utf-8-sig"))
    rows = {int(row["ID"]): row for row in table["rows"]}
    expected_rows = {
        1007: {"name": "Bgm_PeaceDay", "group": 1, "mode": 2, "audios": "Bgm_PeaceDay01|Bgm_PeaceDay02"},
        1008: {"name": "Bgm_PeaceNight", "group": 1, "mode": 2, "audios": "Bgm_PeaceNight01|Bgm_PeaceNight02"},
        4005: {"name": "Sound_Ui_CommonScroll", "group": 4, "mode": 0, "audios": "Sound_Ui_CommonScroll"},
    }
    for row_id, expected in expected_rows.items():
        if row_id not in rows or any(rows[row_id].get(key) != value for key, value in expected.items()):
            raise RuntimeError(f"AudioInfo source-row contract changed: ID {row_id}")
    if not WORKBOOK.is_file():
        raise FileNotFoundError(WORKBOOK)

    source_records = [record(WORKBOOK), record(AUDIOINFO)]
    audio_info_source = ROOT / "ref/client-unity/Assets/Scripts/Hotfix/Config/AudioInfoConfig.cs"
    service_source = ROOT / "ref/client-unity/Assets/Skyunion/RunTime/AudioService/AudioService.cs"
    for path in (audio_info_source, service_source):
        source_records.append(record(path))
    tracks = []
    for name, row_id, row_name in TRACKS:
        source_rel = f"ref/client-unity/Assets/BundleAssets/Souds/{name}.mp3"
        meta_rel = source_rel + ".meta"
        source = resolve(source_rel)
        meta = resolve(meta_rel)
        if not source.is_file() or not meta.is_file():
            raise FileNotFoundError(source)
        meta_text = meta.read_text(encoding="utf-8-sig")
        guid_match = re.search(r"^guid: ([0-9a-f]{32})$", meta_text, re.M)
        if not guid_match or "forceToMono: 1" not in meta_text or "normalize: 0" not in meta_text:
            raise RuntimeError(f"Source audio importer contract missing/changed: {meta_rel}")
        row = rows[row_id]
        if name not in row["audios"].split("|") or row["name"] != row_name:
            raise RuntimeError(f"MP3 is not referenced by pinned AudioInfo row {row_id}: {name}")
        source_records.extend([record(source), record(meta)])
        tracks.append({"name": name, "audio_info_id": row_id, "audio_info_name": row_name,
            "source_path": source_rel, "source_sha256": sha(source), "source_bytes": source.stat().st_size,
            "meta_path": meta_rel, "meta_sha256": sha(meta), "meta_bytes": meta.stat().st_size,
            "guid": guid_match.group(1), "unity_asset_path": f"Assets/Sounds/{name}.mp3",
            "force_to_mono": True, "normalize": False,
            "source_import_settings": parse_import_settings(meta_text)})
    source_records.append(record(ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/CMD/StartUpCommand.cs"))
    source_records.append(record(ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/CityGlobalMediator.cs"))
    source_records.append(record(ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/RS.cs"))

    PROBE.mkdir(parents=True)
    for sub in ("ProjectSettings", "Packages", "Assets/Sounds", "Assets/Editor", "OutputRuns"):
        (PROBE / sub).mkdir(parents=True, exist_ok=True)
    for row in settings_rows:
        shutil.copy2(BASE / row["path"], PROBE / row["path"])
    for row in package_rows:
        shutil.copy2(BASE / row["path"], PROBE / row["path"])
    copied_tracks = []
    for track in tracks:
        src, meta = resolve(track["source_path"]), resolve(track["meta_path"])
        dst = PROBE / track["unity_asset_path"]
        shutil.copy2(src, dst)
        shutil.copy2(meta, Path(str(dst) + ".meta"))
        copied_tracks.append({"asset": dst.relative_to(PROBE).as_posix(), "sha256": sha(dst),
                              "meta": (Path(str(dst) + ".meta")).relative_to(PROBE).as_posix(),
                              "meta_sha256": sha(Path(str(dst) + ".meta"))})

    contract = {"schema": "rok.unity_city_audio_probe.contract.v1", "unity_version_target": UNITY_VERSION,
        "source_records": source_records, "audio_info_rows": [rows[i] for i in (1007, 1008, 4005)],
        "tracks": tracks, "scope": "Only four source city BGM clips mapped by AudioInfo IDs 1007/1008; no game scene, account, network, playback or runtime UI."}
    contract_path = PROBE / "probe_contract.json"
    contract_path.write_text(json.dumps(contract, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    exporter = PROBE / "Assets/Editor/ExportRokUnityCityAudioProbe.cs"
    exporter.write_text(EXPORTER, encoding="utf-8", newline="\n")
    exporter_meta = PROBE / "Assets/Editor/ExportRokUnityCityAudioProbe.cs.meta"
    exporter_meta.write_text("fileFormatVersion: 2\nguid: 8b131c4fe04b4e36b4803b06f80ab147\nMonoImporter:\n  externalObjects: {}\n  serializedVersion: 2\n  defaultReferences: []\n  executionOrder: 0\n  icon: {instanceID: 0}\n  userData: \n  assetBundleName: \n  assetBundleVariant: \n", encoding="utf-8", newline="\n")
    settings_copied = [record(PROBE / row["path"]) for row in settings_rows]
    packages_copied = [record(PROBE / row["path"]) for row in package_rows]
    verify_path = ROOT / "Scripts/VerifyRokUnityCityAudioProbe.py"
    report = {"schema": "rok.unity_city_audio_probe.prepare.v1", "status": "PREPARED_NO_UNITY_RUN",
        "prepared_utc": datetime.now(timezone.utc).isoformat(), "isolated_project": label(PROBE),
        "unity_version_target": UNITY_VERSION, "template_project": label(BASE),
        "template_report": {"path": label(BASE_REPORT), "sha256": sha(BASE_REPORT)},
        "project_settings_count": len(settings_copied), "project_settings_files": settings_copied,
        "packages": packages_copied, "source_records": source_records, "audio_info_rows": [rows[i] for i in (1007, 1008, 4005)],
        "tracks": tracks, "copied_assets": copied_tracks,
        "contract": {"path": label(contract_path), "sha256": sha(contract_path)},
        "exporter": {"path": label(exporter), "sha256": sha(exporter), "meta_sha256": sha(exporter_meta)},
        "scripts": [{"path": "Scripts/PrepareRokUnityCityAudioProbe.py", "sha256": sha(ROOT / "Scripts/PrepareRokUnityCityAudioProbe.py")},
                    {"path": label(verify_path), "sha256": sha(verify_path)}],
        "root_unity_command": f'& "C:/Program Files/Unity/Hub/Editor/{UNITY_VERSION}/Editor/Unity.exe" -batchmode -quit -projectPath "{PROBE}" -executeMethod ExportRokUnityCityAudioProbe.Run -logFile "{ROOT}/Saved/Logs/UnityCityAudioProbe_<UTC>.log"',
        "root_verify_command_template": f'& "<bundled-python>" "{verify_path}" --run "{PROBE}/OutputRuns/<run_id>"',
        "qualification": "Unity imported AudioClip.GetData float PCM oracle and PCM16 WAV export; not audible playback, city scene BGM timing, or UE asset acceptance."}
    BATCH.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "project": report["isolated_project"],
                      "tracks": len(tracks), "report": label(REPORT), "report_sha256": sha(REPORT)}, ensure_ascii=False))
    return 0


def parse_import_settings(text: str) -> dict:
    result = {}
    for key in ("loadType", "sampleRateSetting", "sampleRateOverride", "compressionFormat", "quality", "conversionMode"):
        match = re.search(rf"^    {key}: ([^\r\n]+)$", text, re.M)
        if match:
            result[key] = match.group(1)
    return result


if __name__ == "__main__":
    raise SystemExit(main())
