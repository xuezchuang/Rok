#!/usr/bin/env python3
"""Prepare an isolated Unity 2019 source-curve oracle for Expedition UI clips."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "Saved/UnityExpeditionAnimationProbeV1"
BASE_PROJECT = ROOT / "Saved/UnityCityGammaFamilyStateProbeV2"
REPORT = ROOT / "Saved/RokUnityMigration/BatchReview20261009NightTimeAndExpedition/expedition_animation_probe_prepare.json"
REPORT_REVISION2 = ROOT / "Saved/RokUnityMigration/BatchReview20261009NightTimeAndExpedition/expedition_animation_probe_prepare_revision2.json"
REPORT_REVISION3 = ROOT / "Saved/RokUnityMigration/BatchReview20261009NightTimeAndExpedition/expedition_animation_probe_prepare_revision3.json"
MEDIATOR = "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Expedition/UI_IF_ExpeditionFightTaskMediator.cs"

CLIPS = [
    ("right_show", "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_MainIF_Right_On.anim", "cb512ce7f71778d4eba98e8566a407bc", "UA_ExpeditionTask_Right", "Show"),
    ("right_hide", "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_MainIF_Right_Off.anim", "1ef829f4e3905f041b41d5c682bb75b1", "UA_ExpeditionTask_Right", "Hide"),
    ("center_show", "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UA_ExpeditionTask_centerShow.anim", "e65ac7f2375d3a045ad7f17142a94352", "UA_ExpeditionTask_center", "Show"),
    ("center_hide", "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UA_ExpeditionTask_centerHide.anim", "26057d630b1e94045a48c8a937447e5b", "UA_ExpeditionTask_center", "Hide"),
]
CONTROLLERS = {
    "UA_ExpeditionTask_Right": "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UA_ExpeditionTask_Right.controller",
    "UA_ExpeditionTask_center": "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UA_ExpeditionTask_center.controller",
}

EXPORTER = r'''using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Animations;
using UnityEngine;

public static class ExportRokUnityExpeditionAnimationProbe
{
    [Serializable] public class Root { public string schema; public string created_utc; public string contract_sha256; public Source[] sources; public Controller[] controllers; public Clip[] clips; }
    [Serializable] public class Source { public string path; public string sha256; }
    [Serializable] public class Controller { public string name; public string asset_path; public State[] states; }
    [Serializable] public class State { public string name; public string motion_name; public string motion_asset_path; public int layer_index; public string state_machine_path; }
    [Serializable] public class Clip { public string id; public string name; public string asset_path; public string source_path; public string source_sha256; public float length; public float frame_rate; public bool legacy; public string wrap_mode; public FloatCurve[] float_curves; public ObjectCurve[] object_curves; }
    [Serializable] public class FloatCurve { public string binding_path; public string property_name; public string binding_type; public string pre_wrap_mode; public string post_wrap_mode; public Key[] keys; public Sample[] samples; }
    [Serializable] public class ObjectCurve { public string binding_path; public string property_name; public string binding_type; public ObjectKey[] keys; }
    [Serializable] public class Key { public float time; public float value; public Tangent in_tangent; public Tangent out_tangent; public float in_weight; public float out_weight; public string weighted_mode; public string left_tangent_mode; public string right_tangent_mode; }
    [Serializable] public class Tangent { public bool finite; public float value; public string special; }
    [Serializable] public class ObjectKey { public float time; public string object_name; public string object_asset_path; }
    [Serializable] public class Sample { public float time; public float value; }

    [MenuItem("Rok/Expedition Animation Probe/Export Source Curves")]
    public static void Run()
    {
        var exitCode = 0;
        try { RunExport(); }
        catch (Exception exception) { UnityEngine.Debug.LogException(exception); exitCode = 1; }
        finally { EditorApplication.Exit(exitCode); }
    }

    private static void RunExport()
    {
        var projectRoot = Directory.GetParent(Application.dataPath).FullName;
        var contractPath = Path.Combine(projectRoot, "probe_contract.json");
        var contractBytes = File.ReadAllBytes(contractPath);
        var contract = JsonUtility.FromJson<Contract>(System.Text.Encoding.UTF8.GetString(contractBytes));
        var root = new Root { schema = "rok.unity_expedition_animation_native.v2", created_utc = DateTime.UtcNow.ToString("o", CultureInfo.InvariantCulture), contract_sha256 = Sha256(contractBytes) };
        root.sources = contract.sources.Select(x => new Source { path = x.path, sha256 = x.sha256 }).ToArray();
        root.controllers = contract.controllers.Select(ReadController).ToArray();
        root.clips = contract.clips.Select(x => ReadClip(projectRoot, x)).ToArray();
        var runs = Path.Combine(projectRoot, "OutputRuns"); Directory.CreateDirectory(runs);
        var runName = DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff", CultureInfo.InvariantCulture) + "_" + Process.GetCurrentProcess().Id.ToString(CultureInfo.InvariantCulture);
        var runDir = Path.Combine(runs, runName); Directory.CreateDirectory(runDir);
        File.WriteAllText(Path.Combine(runDir, "native_animation_curves.json"), JsonUtility.ToJson(root, true), new System.Text.UTF8Encoding(false));
        UnityEngine.Debug.Log("ROK_EXPEDITION_ANIMATION_RUN=" + runDir);
    }

    [Serializable] private class Contract { public Source[] sources; public ControllerContract[] controllers; public ClipContract[] clips; }
    [Serializable] private class ControllerContract { public string name; public string asset_path; }
    [Serializable] private class ClipContract { public string id; public string source_path; public string sha256; public string asset_path; }

    private static Controller ReadController(ControllerContract c)
    {
        var controller = AssetDatabase.LoadAssetAtPath<AnimatorController>(c.asset_path);
        if (controller == null) throw new InvalidOperationException("Pinned AnimatorController missing: " + c.asset_path);
        var result = new Controller { name = c.name, asset_path = c.asset_path, states = new List<State>().ToArray() };
        var states = new List<State>();
        for (int layer = 0; layer < controller.layers.Length; layer++) Walk(controller.layers[layer].stateMachine, layer, "", states);
        result.states = states.ToArray();
        return result;
    }

    private static void Walk(AnimatorStateMachine machine, int layer, string prefix, List<State> output)
    {
        foreach (var child in machine.states)
        {
            var state = child.state;
            var motion = state.motion;
            output.Add(new State { name = state.name, motion_name = motion == null ? "" : motion.name, motion_asset_path = motion == null ? "" : AssetDatabase.GetAssetPath(motion), layer_index = layer, state_machine_path = prefix + machine.name });
        }
        foreach (var child in machine.stateMachines) Walk(child.stateMachine, layer, prefix + machine.name + "/", output);
    }

    private static Clip ReadClip(string projectRoot, ClipContract item)
    {
        var clip = AssetDatabase.LoadAssetAtPath<AnimationClip>(item.asset_path);
        if (clip == null) throw new InvalidOperationException("Pinned AnimationClip missing: " + item.asset_path);
        var result = new Clip { id = item.id, name = clip.name, asset_path = item.asset_path, source_path = item.source_path, source_sha256 = item.sha256, length = clip.length, frame_rate = clip.frameRate, legacy = clip.legacy, wrap_mode = clip.wrapMode.ToString() };
        var curves = new List<FloatCurve>();
        foreach (var binding in AnimationUtility.GetCurveBindings(clip))
        {
            var curve = AnimationUtility.GetEditorCurve(clip, binding);
            var keys = curve.keys;
            var record = new FloatCurve { binding_path = binding.path, property_name = binding.propertyName, binding_type = binding.type == null ? "" : binding.type.FullName, pre_wrap_mode = curve.preWrapMode.ToString(), post_wrap_mode = curve.postWrapMode.ToString() };
            record.keys = keys.Select((k, index) => new Key { time = k.time, value = k.value, in_tangent = EncodeTangent(k.inTangent), out_tangent = EncodeTangent(k.outTangent), in_weight = k.inWeight, out_weight = k.outWeight, weighted_mode = k.weightedMode.ToString(), left_tangent_mode = AnimationUtility.GetKeyLeftTangentMode(curve, index).ToString(), right_tangent_mode = AnimationUtility.GetKeyRightTangentMode(curve, index).ToString() }).ToArray();
            var sampleTimes = new SortedSet<float> { 0f, clip.length * 0.25f, clip.length * 0.5f, clip.length * 0.75f, clip.length };
            foreach (var key in keys) sampleTimes.Add(key.time);
            record.samples = sampleTimes.Select(t => new Sample { time = t, value = curve.Evaluate(t) }).ToArray();
            curves.Add(record);
        }
        result.float_curves = curves.ToArray();
        var objects = new List<ObjectCurve>();
        foreach (var binding in AnimationUtility.GetObjectReferenceCurveBindings(clip))
        {
            var keys = AnimationUtility.GetObjectReferenceCurve(clip, binding);
            objects.Add(new ObjectCurve { binding_path = binding.path, property_name = binding.propertyName, binding_type = binding.type == null ? "" : binding.type.FullName,
                keys = keys.Select(k => new ObjectKey { time = k.time, object_name = k.value == null ? "" : k.value.name, object_asset_path = k.value == null ? "" : AssetDatabase.GetAssetPath(k.value) }).ToArray() });
        }
        result.object_curves = objects.ToArray();
        return result;
    }

    private static string Sha256(byte[] bytes)
    {
        using (var sha = System.Security.Cryptography.SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant();
    }

    private static Tangent EncodeTangent(float value)
    {
        if (!float.IsNaN(value) && !float.IsInfinity(value)) return new Tangent { finite = true, value = value, special = "" };
        return new Tangent { finite = false, value = 0f, special = float.IsPositiveInfinity(value) ? "positive_infinity" : (float.IsNegativeInfinity(value) ? "negative_infinity" : "nan") };
    }
}
'''


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().lower()


def read_expected_clip(clip_row: tuple[str, str, str, str, str]) -> dict[str, object]:
    clip_id, source_rel, guid, controller_name, state_name = clip_row
    source = ROOT / source_rel
    text = source.read_text(encoding="utf-8-sig")
    curves = yaml_sections(text)
    sample_rate_match = re.search(r"^  m_SampleRate: ([0-9.]+)$", text, re.M)
    stop_time_match = re.search(r"^    m_StopTime: ([0-9.]+)$", text, re.M)
    if not sample_rate_match or not stop_time_match:
        raise RuntimeError(f"source clip frame-rate/stop-time metadata missing: {source_rel}")
    length = max((max((x for x in c["key_times"] if x is not None), default=0.0) for c in curves), default=0.0)
    return {"id": clip_id, "guid": guid, "source_path": source_rel, "float_curve_count": len(curves), "float_curve_key_counts": [len(c["key_times"]) for c in curves], "source_curve_bindings": [{"path": c["path"], "property_name": c["attribute"], "unity_class_id": c["class_id"], "key_times": c["key_times"], "key_values": c["key_values"], "tangent_modes": c["tangent_modes"], "step_constant": c["step_constant"], "serialized_section": c["source_kind"]} for c in curves], "source_sample_rate": float(sample_rate_match.group(1)), "source_stop_time": float(stop_time_match.group(1)), "parsed_key_time_max": length, "clip_controller_state": {"controller": controller_name, "state": state_name}}


def read_controller_graph(name: str, source_rel: str) -> dict[str, object]:
    text = (ROOT / source_rel).read_text(encoding="utf-8-sig")
    states = []
    for block in re.findall(r"(?ms)^--- !u!1102 .*?\nAnimatorState:\n(.*?)(?=^--- !u!|\Z)", text):
        state_name = re.search(r"^  m_Name: (.+)$", block, re.M)
        motion_guid = re.search(r"^  m_Motion: \{fileID: 7400000, guid: ([0-9a-f]+), type: 2\}$", block, re.M)
        if state_name:
            states.append({"name": state_name.group(1), "motion_guid": motion_guid.group(1) if motion_guid else ""})
    if not {"Show", "Hide"}.issubset({s["name"] for s in states}):
        raise RuntimeError(f"controller state graph missing Show/Hide in {source_rel}")
    source = ROOT / source_rel
    return {"name": name, "source_path": source_rel, "sha256": sha(source), "asset_path": "Assets/Source/" + Path(source_rel).name, "states": states}


def revise_native_report() -> int:
    if not PROJECT.is_dir() or not REPORT.is_file():
        raise SystemExit("revision requires the prepared probe project and original prepare report")
    if REPORT_REVISION2.exists():
        raise SystemExit(f"refusing to overwrite revision report: {REPORT_REVISION2}")
    original = json.loads(REPORT.read_text(encoding="utf-8"))
    for row in original["source_files"]:
        if sha(ROOT / row["path"]) != row["sha256"]:
            raise RuntimeError(f"pinned Unity source changed: {row['path']}")
    if len(original.get("project_settings_files", [])) != 19:
        raise RuntimeError("original report does not pin all 19 ProjectSettings files")
    exporter = PROJECT / "Assets/Editor/ExportRokUnityExpeditionAnimationProbe.cs"
    exporter.write_text(EXPORTER, encoding="utf-8", newline="\n")
    exporter_meta = PROJECT / "Assets/Editor/ExportRokUnityExpeditionAnimationProbe.cs.meta"
    verify_script = ROOT / "Scripts/VerifyRokUnityExpeditionAnimationProbe.py"
    prep_script = ROOT / "Scripts/PrepareRokUnityExpeditionAnimationProbe.py"
    updated = dict(original)
    updated.update({
        "schema": "rok.expedition_animation_probe_prepare.v2",
        "revision": 2,
        "status": "READY_FOR_ROOT_NATIVE2_RUN",
        "supersedes_prepare_report": REPORT.relative_to(ROOT).as_posix(),
        "expected_clip_curves": [read_expected_clip(row) for row in CLIPS],
        "controller_state_graphs": [read_controller_graph(name, path) for name, path in CONTROLLERS.items()],
        "exporter_sha256": sha(exporter),
        "exporter_meta_sha256": sha(exporter_meta),
        "scripts": [{"path": prep_script.relative_to(ROOT).as_posix(), "sha256": sha(prep_script)}, {"path": verify_script.relative_to(ROOT).as_posix(), "sha256": sha(verify_script)}],
        "native1_diagnostic": {
            "path": "Saved/UnityExpeditionAnimationProbeV1/OutputRuns/20261009_012300_251_46884/native_animation_curves.json",
            "sha256": sha(ROOT / "Saved/UnityExpeditionAnimationProbeV1/OutputRuns/20261009_012300_251_46884/native_animation_curves.json"),
            "validation_failure": "Unity serializes step/constant tangent infinities as non-standard JSON Infinity; Native1 remains immutable diagnostic evidence.",
            "expected_source_semantics": "m_BlocksRaycasts, m_Interactable, and m_RaycastTarget source YAML tangents are tangentMode 103 with Boolean values; native AnimationUtility reports Constant tangent modes and infinite tangents.",
        },
        "native_output_schema": "rok.unity_expedition_animation_native.v2; tangent values are tagged finite/special objects so JSON remains standards-compliant",
        "exit_behavior": "Exporter catches exceptions, writes a failure log, and calls EditorApplication.Exit(code) in finally; root command also includes -quit as an outer batch-mode guard.",
        "root_commands": list(original.get("root_commands", [])),
        "acceptance_limit": "source curve oracle only; does not establish Animator execution timing or full Unity UI screenshot fidelity",
    })
    updated["root_commands"][-2] = updated["root_commands"][-2].replace("-executeMethod ExportRokUnityExpeditionAnimationProbe.Run", "-executeMethod ExportRokUnityExpeditionAnimationProbe.Run -quit")
    updated["root_command"] = updated["root_commands"]
    REPORT_REVISION2.parent.mkdir(parents=True, exist_ok=True)
    REPORT_REVISION2.write_text(json.dumps(updated, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": updated["status"], "report": REPORT_REVISION2.relative_to(ROOT).as_posix(), "report_sha256": sha(REPORT_REVISION2), "clip_curve_total": sum(c["float_curve_count"] for c in updated["expected_clip_curves"])}))
    return 0


def yaml_sections(text: str) -> list[dict[str, object]]:
    """Use source editor bindings, cross-checked against serialized float/vector curves."""
    sections: list[dict[str, object]] = []

    def section_text(name: str) -> str:
        match = re.search(rf"(?ms)^  {re.escape(name)}:\s*\n(.*?)(?=^  m_[A-Z]|\Z)", text)
        return match.group(1) if match else ""

    def curve_blocks(raw: str) -> list[str]:
        starts = [m.start() for m in re.finditer(r"^  - curve:", raw, re.M)]
        return [raw[a:(starts[i + 1] if i + 1 < len(starts) else len(raw))] for i, a in enumerate(starts)]

    def parse_keys(block: str, vector: bool) -> list[dict[str, object]]:
        curve_match = re.search(r"(?ms)^      m_Curve:\s*\n(.*?)(?=^      m_PreInfinity:|\Z)", block)
        if not curve_match:
            return []
        raw = curve_match.group(1)
        starts = [m.start() for m in re.finditer(r"^      - serializedVersion: 3\s*$", raw, re.M)]
        parsed = []
        for i, start in enumerate(starts):
            part = raw[start:(starts[i + 1] if i + 1 < len(starts) else len(raw))]
            tm = re.search(r"^        time: ([^\s]+)", part, re.M)
            vm = re.search(r"^        value: (.*)$", part, re.M)
            mode = re.search(r"^        tangentMode: (\d+)$", part, re.M)
            in_slope = re.search(r"^        inSlope: (.*)$", part, re.M)
            out_slope = re.search(r"^        outSlope: (.*)$", part, re.M)
            in_weight = re.search(r"^        inWeight: (.*)$", part, re.M)
            out_weight = re.search(r"^        outWeight: (.*)$", part, re.M)
            if not (tm and vm and mode and in_slope and out_slope and in_weight and out_weight):
                raise RuntimeError("unsupported source animation keyframe YAML record")
            if vector:
                def xyz(value: str) -> dict[str, float]:
                    found = re.fullmatch(r"\{x: ([^,]+), y: ([^,]+), z: ([^}]+)\}", value.strip())
                    if not found:
                        raise RuntimeError(f"unsupported Unity vector curve value: {value}")
                    return {axis: float(found.group(i)) for i, axis in enumerate(("x", "y", "z"), start=1)}
                parsed.append({"time": float(tm.group(1)), "value": xyz(vm.group(1)), "tangent_mode": int(mode.group(1)), "in_slope": xyz(in_slope.group(1)), "out_slope": xyz(out_slope.group(1)), "in_weight": xyz(in_weight.group(1)), "out_weight": xyz(out_weight.group(1))})
            else:
                parsed.append({"time": float(tm.group(1)), "value": float(vm.group(1)), "tangent_mode": int(mode.group(1)), "in_slope": float(in_slope.group(1)), "out_slope": float(out_slope.group(1)), "in_weight": float(in_weight.group(1)), "out_weight": float(out_weight.group(1))})
        return parsed

    scale_blocks = curve_blocks(section_text("m_ScaleCurves"))
    for block in scale_blocks:
        path_match = re.search(r"^    path: (.*)$", block, re.M)
        path = path_match.group(1) if path_match else ""
        keys = parse_keys(block, vector=True)
        for axis in "xyz":
            sections.append({"attribute": f"m_LocalScale.{axis}", "path": path, "class_id": 224, "key_times": [k["time"] for k in keys], "key_values": [k["value"][axis] for k in keys], "tangent_modes": [k["tangent_mode"] for k in keys], "step_constant": False, "source_kind": "m_ScaleCurves"})

    float_blocks = curve_blocks(section_text("m_FloatCurves"))
    for block in float_blocks:
        attribute = re.search(r"^    attribute: (.*)$", block, re.M)
        path = re.search(r"^    path: (.*)$", block, re.M)
        class_id = re.search(r"^    classID: (\d+)$", block, re.M)
        if not (attribute and path and class_id):
            raise RuntimeError("source float curve is missing binding metadata")
        keys = parse_keys(block, vector=False)
        values = [k["value"] for k in keys]
        modes = [k["tangent_mode"] for k in keys]
        step_constant = bool(keys) and all(mode == 103 for mode in modes) and all(value in (0.0, 1.0) for value in values)
        sections.append({"attribute": attribute.group(1), "path": path.group(1), "class_id": int(class_id.group(1)), "key_times": [k["time"] for k in keys], "key_values": values, "tangent_modes": modes, "step_constant": step_constant, "source_kind": "m_FloatCurves"})

    editor_sections = []
    for block in curve_blocks(section_text("m_EditorCurves")):
        attribute = re.search(r"^    attribute: (.*)$", block, re.M)
        path = re.search(r"^    path: (.*)$", block, re.M)
        class_id = re.search(r"^    classID: (\d+)$", block, re.M)
        if not (attribute and path and class_id):
            raise RuntimeError("source editor curve is missing binding metadata")
        keys = parse_keys(block, vector=False)
        if not keys:
            raise RuntimeError("source editor curve has no keyframes")
        binding = (path.group(1), attribute.group(1), int(class_id.group(1)))
        matches = [row for row in sections if (row["path"], row["attribute"], row["class_id"]) == binding]
        if len(matches) != 1:
            raise RuntimeError(f"editor binding absent or duplicate in serialized curves: {binding}")
        serialized = matches[0]
        if len(keys) != len(serialized["key_times"]) or any(
            abs(key["time"] - time) > 1e-6 or abs(key["value"] - value) > 1e-5
            or (serialized["source_kind"] == "m_FloatCurves" and key["tangent_mode"] != mode)
            for key, time, value, mode in zip(keys, serialized["key_times"],
                                               serialized["key_values"], serialized["tangent_modes"])
        ):
            raise RuntimeError(f"editor and serialized keyframes differ: {binding}")
        editor_sections.append(serialized)
    editor_bindings = [(row["path"], row["attribute"], row["class_id"]) for row in editor_sections]
    if len(set(editor_bindings)) != len(editor_bindings):
        raise RuntimeError("duplicate source editor curve binding")
    # Unity's vector m_ScaleCurves stores a constant z for centerHide but has no
    # m_LocalScale.z editor binding. Only editor bindings appear in GetCurveBindings.
    for row in sections:
        binding = (row["path"], row["attribute"], row["class_id"])
        if binding not in editor_bindings and not (
            row["source_kind"] == "m_ScaleCurves"
            and len(set(row["key_values"])) == 1
            and row["key_values"][0] == 1.0
        ):
            raise RuntimeError(f"nonconstant serialized curve omitted from editor bindings: {binding}")
    for unsupported in ("m_PositionCurves", "m_RotationCurves", "m_EulerCurves"):
        if curve_blocks(section_text(unsupported)):
            raise RuntimeError(f"unexpected non-empty source animation curve section: {unsupported}")
    return editor_sections


def revise_editor_bindings_report() -> int:
    """Pin the source editor-binding inventory without changing native assets."""
    if not PROJECT.is_dir() or not REPORT_REVISION2.is_file():
        raise SystemExit("revision3 requires the existing isolated probe and revision2 report")
    if REPORT_REVISION3.exists():
        raise SystemExit(f"refusing to overwrite revision3 report: {REPORT_REVISION3}")
    previous = json.loads(REPORT_REVISION2.read_text(encoding="utf-8"))
    for row in previous["source_files"]:
        if sha(ROOT / row["path"]) != row["sha256"]:
            raise RuntimeError(f"pinned Unity source changed: {row['path']}")
    native2 = PROJECT / "OutputRuns/20261009_013025_916_42504/native_animation_curves.json"
    if not native2.is_file():
        raise FileNotFoundError(native2)
    updated = dict(previous)
    updated.update({
        "schema": "rok.expedition_animation_probe_prepare.v3",
        "revision": 3,
        "status": "SOURCE_EDITOR_BINDINGS_NATIVE2_READY_FOR_VERIFICATION",
        "supersedes_prepare_report": REPORT_REVISION2.relative_to(ROOT).as_posix(),
        "supersedes_prepare_sha256": sha(REPORT_REVISION2),
        "expected_clip_curves": [read_expected_clip(row) for row in CLIPS],
        "binding_source_rule": "m_EditorCurves defines the float binding set returned by AnimationUtility.GetCurveBindings; each editor keyframe is checked against m_FloatCurves/m_ScaleCurves. centerHide's serialized constant z=1 is not an editor binding.",
        "native2_diagnostic": {
            "path": native2.relative_to(ROOT).as_posix(),
            "sha256": sha(native2),
            "status": "root Unity export exit 0; retained raw, now verifiable with source editor bindings",
        },
        "source_preview_package": {
            "path": "Saved/RokUnityMigration/expedition_stage_preview.json",
            "sha256": sha(ROOT / "Saved/RokUnityMigration/expedition_stage_preview.json"),
        },
        "scripts": [
            {"path": "Scripts/PrepareRokUnityExpeditionAnimationProbe.py", "sha256": sha(ROOT / "Scripts/PrepareRokUnityExpeditionAnimationProbe.py")},
            {"path": "Scripts/VerifyRokUnityExpeditionAnimationProbe.py", "sha256": sha(ROOT / "Scripts/VerifyRokUnityExpeditionAnimationProbe.py")},
        ],
        "acceptance_limit": "source curve oracle and sampled AnimationCurve.Evaluate only; no Animator execution timing or full Unity UI visual proof",
    })
    REPORT_REVISION3.write_text(json.dumps(updated, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": updated["status"], "report": REPORT_REVISION3.relative_to(ROOT).as_posix(),
                      "report_sha256": sha(REPORT_REVISION3),
                      "source_editor_binding_total": sum(c["float_curve_count"] for c in updated["expected_clip_curves"])}))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--prepare", action="store_true")
    mode.add_argument("--revise-native", action="store_true")
    mode.add_argument("--revise-native2", action="store_true")
    args = parser.parse_args()
    if args.revise_native:
        return revise_native_report()
    if args.revise_native2:
        return revise_editor_bindings_report()
    if not args.prepare:
        parser.error("this is prepare-only; pass --prepare")
    if PROJECT.exists():
        raise SystemExit(f"refusing to overwrite existing isolated project: {PROJECT}")
    if REPORT.exists():
        raise SystemExit(f"refusing to overwrite existing prepare report: {REPORT}")
    settings_src = BASE_PROJECT / "ProjectSettings"
    package_src = BASE_PROJECT / "Packages"
    setting_files = sorted(p for p in settings_src.iterdir() if p.is_file())
    if len(setting_files) != 19:
        raise RuntimeError(f"expected sealed FamilyStateV2's 19 ProjectSettings files, got {len(setting_files)}")

    sources: dict[str, dict[str, str]] = {}
    clips_contract = []
    expected_clips = []
    for clip_id, source_rel, guid, controller_name, state_name in CLIPS:
        source = ROOT / source_rel
        meta = ROOT / f"{source_rel}.meta"
        if not source.is_file() or not meta.is_file():
            raise FileNotFoundError(f"missing pinned Unity clip or meta: {source_rel}")
        found = re.search(r"^guid: ([0-9a-f]+)$", meta.read_text(encoding="utf-8-sig"), re.M)
        if not found or found.group(1) != guid:
            raise RuntimeError(f"source GUID mismatch: {source_rel}")
        for p in (source, meta):
            sources[p.relative_to(ROOT).as_posix()] = {"path": p.relative_to(ROOT).as_posix(), "sha256": sha(p)}
        asset_path = "Assets/Source/" + Path(source_rel).name
        text = source.read_text(encoding="utf-8-sig")
        curves = yaml_sections(text)
        length = max((max((x for x in c["key_times"] if x is not None), default=0.0) for c in curves), default=0.0)
        sample_rate_match = re.search(r"^  m_SampleRate: ([0-9.]+)$", text, re.M)
        stop_time_match = re.search(r"^    m_StopTime: ([0-9.]+)$", text, re.M)
        if not sample_rate_match or not stop_time_match:
            raise RuntimeError(f"source clip frame-rate/stop-time metadata missing: {source_rel}")
        clips_contract.append({"id": clip_id, "source_path": source_rel, "sha256": sha(source), "guid": guid, "asset_path": asset_path})
        expected_clips.append(read_expected_clip((clip_id, source_rel, guid, controller_name, state_name)))

    controller_rows = []
    for name, source_rel in CONTROLLERS.items():
        source = ROOT / source_rel
        meta = ROOT / f"{source_rel}.meta"
        if not source.is_file() or not meta.is_file():
            raise FileNotFoundError(f"missing controller or meta: {source_rel}")
        for p in (source, meta):
            sources[p.relative_to(ROOT).as_posix()] = {"path": p.relative_to(ROOT).as_posix(), "sha256": sha(p)}
        text = source.read_text(encoding="utf-8-sig")
        states = []
        for block in re.findall(r"(?ms)^--- !u!1102 .*?\nAnimatorState:\n(.*?)(?=^--- !u!|\Z)", text):
            n = re.search(r"^  m_Name: (.+)$", block, re.M)
            g = re.search(r"^  m_Motion: \{fileID: 7400000, guid: ([0-9a-f]+), type: 2\}$", block, re.M)
            if n:
                states.append({"name": n.group(1), "motion_guid": g.group(1) if g else ""})
        state_names = {s["name"] for s in states}
        if not {"Show", "Hide"}.issubset(state_names):
            raise RuntimeError(f"controller state graph missing Show/Hide in {source_rel}: {state_names}")
        controller_rows.append({"name": name, "source_path": source_rel, "sha256": sha(source), "asset_path": "Assets/Source/" + Path(source_rel).name, "states": states})

    mediator = ROOT / MEDIATOR
    if not mediator.is_file():
        raise FileNotFoundError(f"missing source view mediator: {MEDIATOR}")
    sources[MEDIATOR] = {"path": MEDIATOR, "sha256": sha(mediator)}

    PROJECT.mkdir(parents=True)
    for sub in ("ProjectSettings", "Packages", "Assets/Source", "Assets/Editor", "OutputRuns"):
        (PROJECT / sub).mkdir(parents=True, exist_ok=True)
    for p in setting_files:
        shutil.copy2(p, PROJECT / "ProjectSettings" / p.name)
    for name in ("manifest.json", "packages-lock.json"):
        shutil.copy2(package_src / name, PROJECT / "Packages" / name)
    copied = []
    for row in [*clips_contract, *[{"id": n, "source_path": p, "sha256": sources[p]["sha256"], "asset_path": "Assets/Source/" + Path(p).name} for n, p in CONTROLLERS.items()]]:
        source = ROOT / row["source_path"]
        dest = PROJECT / row["asset_path"]
        shutil.copy2(source, dest)
        shutil.copy2(ROOT / f"{row['source_path']}.meta", Path(str(dest) + ".meta"))
        copied.extend([dest.relative_to(PROJECT).as_posix(), Path(str(dest) + ".meta").relative_to(PROJECT).as_posix()])
    exporter = PROJECT / "Assets/Editor/ExportRokUnityExpeditionAnimationProbe.cs"
    exporter.write_text(EXPORTER, encoding="utf-8", newline="\n")
    exporter_meta = PROJECT / "Assets/Editor/ExportRokUnityExpeditionAnimationProbe.cs.meta"
    exporter_meta.write_text("fileFormatVersion: 2\nguid: d57b00df84234a7f8b3b6d6936174f61\nMonoImporter:\n  externalObjects: {}\n  serializedVersion: 2\n  defaultReferences: []\n  executionOrder: 0\n  icon: {instanceID: 0}\n  userData: \n  assetBundleName: \n  assetBundleVariant: \n", encoding="utf-8", newline="\n")
    contract = {"schema": "rok.unity_expedition_animation_probe_contract.v1", "sources": list(sources.values()), "controllers": [{"name": r["name"], "asset_path": r["asset_path"], "source_path": r["source_path"], "sha256": r["sha256"]} for r in controller_rows], "clips": clips_contract}
    contract_path = PROJECT / "probe_contract.json"
    contract_path.write_text(json.dumps(contract, indent=2) + "\n", encoding="utf-8")
    project_files = []
    for p in sorted((PROJECT / "ProjectSettings").iterdir()):
        if p.is_file(): project_files.append({"path": p.relative_to(PROJECT).as_posix(), "sha256": sha(p)})
    package_files = [{"path": (PROJECT / "Packages" / n).relative_to(PROJECT).as_posix(), "sha256": sha(PROJECT / "Packages" / n)} for n in ("manifest.json", "packages-lock.json")]
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    prep_script = ROOT / "Scripts/PrepareRokUnityExpeditionAnimationProbe.py"
    verify_script = ROOT / "Scripts/VerifyRokUnityExpeditionAnimationProbe.py"
    unity_editor = r"C:\Program Files\Unity\Hub\Editor\2019.4.28f1c1\Editor\Unity.exe"
    prep = {"schema": "rok.expedition_animation_probe_prepare.v1", "status": "PREPARED_NO_UNITY_RUN", "prepared_utc": datetime.now(timezone.utc).isoformat(), "isolated_project": PROJECT.relative_to(ROOT).as_posix(), "base_project": BASE_PROJECT.relative_to(ROOT).as_posix(), "project_settings_files": project_files, "project_settings_count": len(project_files), "packages": package_files, "source_files": list(sources.values()), "source_trace": [{"path": MEDIATOR, "sha256": sources[MEDIATOR]["sha256"], "line_refs": [216, 229, 471, 477, 480, 483, 488, 491], "role": "view mode dispatch: Normal plays preview Hide and side/center Show; Preview plays preview Show and side/center Hide; Hide sends all three to Hide"}], "controller_state_graphs": controller_rows, "expected_clip_curves": expected_clips, "copied_assets": copied, "contract_path": contract_path.relative_to(ROOT).as_posix(), "contract_sha256": sha(contract_path), "exporter_path": exporter.relative_to(ROOT).as_posix(), "exporter_sha256": sha(exporter), "exporter_meta_sha256": sha(exporter_meta), "scripts": [{"path": prep_script.relative_to(ROOT).as_posix(), "sha256": sha(prep_script)}, {"path": verify_script.relative_to(ROOT).as_posix(), "sha256": sha(verify_script)}], "scope": "four pinned AnimationClips and two referenced AnimatorControllers only; no GameApp, UI prefab, login/account, network, or runtime visual scene", "unity_version_target": "2019.4.28f1c1", "root_command": f'& "{unity_editor}" -batchmode -projectPath "{PROJECT}" -executeMethod ExportRokUnityExpeditionAnimationProbe.Run -logFile "{ROOT / "Saved/Logs/UnityExpeditionAnimationProbe_<UTC>.log"}"', "verify_command_template": f'python -X utf8 "{verify_script}" --run "{PROJECT / "OutputRuns/<timestamp_pid>"}"', "acceptance_limit": "curve/source oracle only; does not establish Animator execution timing or full Unity UI screenshot fidelity"}
    REPORT.write_text(json.dumps(prep, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": prep["status"], "project": prep["isolated_project"], "clips": len(clips_contract), "source_files": len(sources), "report": REPORT.relative_to(ROOT).as_posix(), "report_sha256": sha(REPORT)}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
