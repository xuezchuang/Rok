#!/usr/bin/env python3
"""Verify preparation and Unity-native source-curve output for Expedition clips."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "Saved/UnityExpeditionAnimationProbeV1"
REPORT = ROOT / "Saved/RokUnityMigration/BatchReview20261009NightTimeAndExpedition/expedition_animation_probe_prepare.json"
REPORT_REVISION2 = ROOT / "Saved/RokUnityMigration/BatchReview20261009NightTimeAndExpedition/expedition_animation_probe_prepare_revision2.json"
REPORT_REVISION3 = ROOT / "Saved/RokUnityMigration/BatchReview20261009NightTimeAndExpedition/expedition_animation_probe_prepare_revision3.json"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().lower()


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"), parse_constant=lambda token: (_ for _ in ()).throw(ValueError(f"non-standard JSON token: {token}")))


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def verify_prepare() -> dict:
    report = load_json(REPORT)
    require(report.get("status") == "PREPARED_NO_UNITY_RUN", "prepare report status mismatch")
    require(report.get("project_settings_count") == 19, "expected exactly 19 copied ProjectSettings files")
    settings = report.get("project_settings_files", [])
    require(len(settings) == 19, "ProjectSettings inventory length mismatch")
    for row in settings:
        copied = PROJECT / row["path"]
        source = resolve(report["base_project"]) / row["path"]
        require(copied.is_file() and source.is_file(), f"missing ProjectSettings file: {row['path']}")
        require(sha(copied) == row["sha256"] == sha(source), f"ProjectSettings hash mismatch: {row['path']}")
    for row in report["packages"]:
        copied = PROJECT / row["path"]
        source = resolve(report["base_project"]) / row["path"]
        require(copied.is_file() and sha(copied) == row["sha256"] == sha(source), f"package hash mismatch: {row['path']}")
    for row in report["source_files"]:
        source = resolve(row["path"])
        require(source.is_file() and sha(source) == row["sha256"], f"pinned source changed: {row['path']}")
    contract_path = resolve(report["contract_path"])
    require(contract_path.is_file() and sha(contract_path) == report["contract_sha256"], "project contract hash mismatch")
    contract = load_json(contract_path)
    require(contract.get("schema") == "rok.unity_expedition_animation_probe_contract.v1", "contract schema mismatch")
    require(len(contract.get("clips", [])) == 4, "contract must pin exactly the four source clips")
    require(len(contract.get("controllers", [])) == 2, "contract must pin the two referenced controllers")
    for row in contract["clips"]:
        copied = PROJECT / row["asset_path"]
        copied_meta = Path(str(copied) + ".meta")
        source_meta = ROOT / f"{row['source_path']}.meta"
        require(copied.is_file() and copied_meta.is_file(), f"missing clip copy/meta: {row['asset_path']}")
        require(sha(resolve(row["source_path"])) == row["sha256"] and sha(copied) == row["sha256"], f"clip bytes differ: {row['id']}")
        require(sha(source_meta) == sha(copied_meta), f"clip meta bytes differ: {row['id']}")
        guid = re.search(r"^guid: ([0-9a-f]+)$", copied_meta.read_text(encoding="utf-8-sig"), re.M)
        require(guid and guid.group(1) == row["guid"], f"clip GUID mismatch: {row['id']}")
    for row in contract["controllers"]:
        copied = PROJECT / row["asset_path"]
        copied_meta = Path(str(copied) + ".meta")
        source_meta = ROOT / f"{row['source_path']}.meta"
        require(copied.is_file() and copied_meta.is_file(), f"missing controller copy/meta: {row['name']}")
        require(sha(resolve(row["source_path"])) == row["sha256"] and sha(copied) == row["sha256"], f"controller bytes differ: {row['name']}")
        require(sha(source_meta) == sha(copied_meta), f"controller meta differs: {row['name']}")
    require(len(report.get("expected_clip_curves", [])) == 4, "expected clip curve inventory missing")
    for row in report["expected_clip_curves"]:
        require(row["float_curve_count"] == len(row["float_curve_key_counts"]), f"source curve count malformed: {row['id']}")
        require(all(count > 0 for count in row["float_curve_key_counts"]), f"empty source curve in {row['id']}")
        require(row["source_sample_rate"] > 0 and row["source_stop_time"] > 0, f"missing source clip length/rate: {row['id']}")
        require(all(math.isfinite(t) for binding in row["source_curve_bindings"] for t in binding["key_times"]), f"non-finite Unity YAML key time: {row['id']}")
    return {"prepare": "pass", "clips": 4, "controllers": 2, "project_settings": 19, "curve_inventory": sum(x["float_curve_count"] for x in report["expected_clip_curves"])}


def verify_revision2_prepare() -> dict:
    base = verify_prepare()
    report = load_json(REPORT_REVISION2)
    require(report.get("schema") == "rok.expedition_animation_probe_prepare.v2", "revision2 prepare schema mismatch")
    require(report.get("status") == "READY_FOR_ROOT_NATIVE2_RUN", "revision2 status mismatch")
    require(report.get("supersedes_prepare_report") == REPORT.relative_to(ROOT).as_posix(), "revision2 provenance mismatch")
    require(report.get("project_settings_count") == 19 and len(report.get("project_settings_files", [])) == 19, "revision2 ProjectSettings pin mismatch")
    for row in report["source_files"]:
        require(sha(resolve(row["path"])) == row["sha256"], f"revision2 Unity source hash mismatch: {row['path']}")
    for row in report["scripts"]:
        require(sha(resolve(row["path"])) == row["sha256"], f"revision2 script hash mismatch: {row['path']}")
    exporter = PROJECT / "Assets/Editor/ExportRokUnityExpeditionAnimationProbe.cs"
    require(sha(exporter) == report["exporter_sha256"], "revision2 exporter hash mismatch")
    require("EditorApplication.Exit(exitCode)" in exporter.read_text(encoding="utf-8"), "exporter does not exit on success/failure")
    require("-quit" in " ".join(report.get("root_commands", [])), "root command missing -quit")
    native1 = report.get("native1_diagnostic", {})
    native1_path = resolve(native1.get("path", ""))
    require(native1_path.is_file() and sha(native1_path) == native1.get("sha256"), "preserved Native1 diagnostic hash mismatch")
    require(len(report.get("expected_clip_curves", [])) == 4, "revision2 source curve inventory missing")
    for clip in report["expected_clip_curves"]:
        require(clip["float_curve_count"] == len(clip["source_curve_bindings"]), f"revision2 binding count mismatch: {clip['id']}")
        for binding in clip["source_curve_bindings"]:
            require(len(binding["key_times"]) == len(binding["key_values"]) == len(binding["tangent_modes"]), f"revision2 key schema mismatch: {clip['id']}/{binding['property_name']}")
            require(all(math.isfinite(float(x)) for x in binding["key_times"] + binding["key_values"]), f"revision2 nonfinite source data: {clip['id']}/{binding['property_name']}")
            if binding["step_constant"]:
                require(binding["serialized_section"] == "m_FloatCurves" and all(mode == 103 for mode in binding["tangent_modes"]) and all(value in (0, 1) for value in binding["key_values"]), f"invalid source proof for step tangent: {clip['id']}/{binding['property_name']}")
    return {**base, "revision2": "pass", "curve_inventory": sum(c["float_curve_count"] for c in report["expected_clip_curves"])}


def verify_revision3_prepare() -> dict:
    from PrepareRokUnityExpeditionAnimationProbe import CLIPS, read_expected_clip

    base = verify_prepare()
    previous = load_json(REPORT_REVISION2)
    report = load_json(REPORT_REVISION3)
    require(report.get("schema") == "rok.expedition_animation_probe_prepare.v3"
            and report.get("status") == "SOURCE_EDITOR_BINDINGS_NATIVE2_READY_FOR_VERIFICATION",
            "revision3 prepare identity mismatch")
    require(report.get("supersedes_prepare_report") == REPORT_REVISION2.relative_to(ROOT).as_posix()
            and report.get("supersedes_prepare_sha256") == sha(REPORT_REVISION2),
            "revision3 provenance mismatch")
    require(report["contract_sha256"] == previous["contract_sha256"]
            and report["exporter_sha256"] == previous["exporter_sha256"],
            "revision3 changed the Unity contract or exporter")
    require(sha(resolve(report["exporter_path"])) == report["exporter_sha256"],
            "Unity exporter changed after Native2")
    for row in report["scripts"]:
        require(sha(resolve(row["path"])) == row["sha256"],
                f"revision3 script changed: {row['path']}")
    require(report["expected_clip_curves"] == [read_expected_clip(row) for row in CLIPS],
            "source editor-binding inventory changed")
    require({row["id"]: row["float_curve_count"] for row in report["expected_clip_curves"]}
            == {"right_show": 3, "right_hide": 3, "center_show": 8, "center_hide": 6},
            "source editor-binding count differs")
    native2 = report["native2_diagnostic"]
    require(sha(resolve(native2["path"])) == native2["sha256"], "Native2 raw changed")
    package = report["source_preview_package"]
    require(sha(resolve(package["path"])) == package["sha256"],
            "source preview curve package changed")
    return {**base, "revision3": "pass", "curve_inventory": 20}


def source_hermite(points: list[list[float]], time: float) -> float:
    if time <= points[0][0]:
        return points[0][1]
    for (start, value), (end, next_value) in zip(points, points[1:]):
        if time <= end + 1e-6:
            unit = max(0.0, min(1.0, (time - start) / (end - start)))
            smooth = unit * unit * (3.0 - 2.0 * unit)
            return value + (next_value - value) * smooth
    return points[-1][1]


NUMERIC_CURVES = {
    "right_show": {("", "m_AnchoredPosition.x"): "side_show_x",
                   ("", "m_Alpha"): "side_show_alpha"},
    "right_hide": {("", "m_AnchoredPosition.x"): "side_hide_x",
                   ("", "m_Alpha"): "side_hide_alpha"},
    "center_show": {("img", "m_LocalScale.x"): "center_show_scale",
                    ("img", "m_LocalScale.y"): "center_show_scale",
                    ("pl_flagMes", "m_Alpha"): "center_show_flag_alpha"},
    "center_hide": {("", "m_LocalScale.x"): "center_hide_scale",
                    ("", "m_LocalScale.y"): "center_hide_scale",
                    ("", "m_Alpha"): "center_hide_alpha"},
}


def verify_run(run_dir: Path) -> dict:
    prep = verify_revision3_prepare()
    resolved = run_dir.resolve()
    runs_root = (PROJECT / "OutputRuns").resolve()
    require(resolved.parent == runs_root and resolved.is_dir(), "run path must be a unique direct child of this probe OutputRuns")
    native_path = resolved / "native_animation_curves.json"
    require(native_path.is_file(), "native_animation_curves.json missing")
    native = load_json(native_path)
    report = load_json(REPORT_REVISION3)
    require(native.get("schema") == "rok.unity_expedition_animation_native.v2", "native result schema mismatch")
    require(native.get("contract_sha256") == report["contract_sha256"], "native contract hash mismatch")
    require(len(native.get("clips", [])) == 4, "native clip count mismatch")
    require(len(native.get("controllers", [])) == 2, "native controller count mismatch")
    source_map = {r["path"]: r["sha256"] for r in report["source_files"]}
    for row in native.get("sources", []):
        require(source_map.get(row.get("path")) == row.get("sha256"), f"native pinned source mismatch: {row}")
    require(len(native.get("sources", [])) == len(source_map), "native source list is incomplete")

    expected_graphs = {c["name"]: c for c in report["controller_state_graphs"]}
    actual_graphs = {c["name"]: c for c in native["controllers"]}
    require(set(expected_graphs) == set(actual_graphs), "controller names differ")
    clip_by_asset = {c["asset_path"]: c for c in native["clips"]}
    for name, expected in expected_graphs.items():
        actual = actual_graphs[name]
        actual_states = actual.get("states", [])
        require({s["name"] for s in actual_states} == {s["name"] for s in expected["states"]}, f"controller state names differ: {name}")
        # Unity native API resolves GUIDs to asset paths; check every motion, including the empty entry state.
        for source_state in expected["states"]:
            native_state = next(s for s in actual_states if s["name"] == source_state["name"])
            if source_state["motion_guid"]:
                clip_contract = next((c for c in load_json(resolve(report["contract_path"]))["clips"] if c["guid"] == source_state["motion_guid"]), None)
                require(clip_contract is not None, f"source state GUID not mapped to clip: {name}/{source_state['name']}")
                require(Path(native_state.get("motion_asset_path", "")).as_posix() == clip_contract["asset_path"], f"controller GUID-to-motion mismatch: {name}/{source_state['name']}")
            else:
                require(not native_state.get("motion_asset_path"), f"unexpected motion on source-empty state: {name}/{source_state['name']}")
        required = [x for x in expected["states"] if x["name"] in ("Show", "Hide")]
        require({x["name"] for x in required} == {"Show", "Hide"}, f"expected Show/Hide state graph missing: {name}")
        for state in required:
            clip = next((c for c in report["expected_clip_curves"] if c["clip_controller_state"]["controller"] == name and c["clip_controller_state"]["state"] == state["name"]), None)
            require(clip is not None, f"no source clip mapped to {name}/{state['name']}")
            native_clip = next((c for c in native["clips"] if c["id"] == clip["id"]), None)
            require(native_clip is not None, f"native clip missing for {name}/{state['name']}")
            require(any(s["name"] == state["name"] and Path(s.get("motion_asset_path", "")).as_posix() == native_clip["asset_path"] for s in actual.get("states", [])), f"state-to-motion binding mismatch: {name}/{state['name']}")

    expected_clips = {x["id"]: x for x in report["expected_clip_curves"]}
    actual_clips = {x["id"]: x for x in native["clips"]}
    require(set(expected_clips) == set(actual_clips), "native clip IDs differ from pinned source")
    package = load_json(resolve(report["source_preview_package"]["path"]))
    source_curves = package["animation_zero_tangent_hermite_curves"]
    require(set(source_curves) == set(name for mapping in NUMERIC_CURVES.values()
                                     for name in mapping.values()),
            "source preview numeric curve set differs")
    total_curves = 0
    total_samples = 0
    for clip_id, expected in expected_clips.items():
        actual = actual_clips[clip_id]
        require(actual.get("source_sha256") == source_map[expected["source_path"]], f"clip source SHA mismatch: {clip_id}")
        require(actual.get("length", 0) > 0 and math.isfinite(actual["length"]), f"invalid native clip length: {clip_id}")
        require(actual.get("frame_rate", 0) > 0 and math.isfinite(actual["frame_rate"]), f"invalid frame rate: {clip_id}")
        require(abs(float(actual["length"]) - float(expected["source_stop_time"])) < 1e-5, f"Unity-native clip length differs from serialized stop time: {clip_id}")
        require(abs(float(actual["frame_rate"]) - float(expected["source_sample_rate"])) < 1e-5, f"Unity-native clip frame rate differs from serialized sample rate: {clip_id}")
        curves = actual.get("float_curves", [])
        require(len(curves) == expected["float_curve_count"], f"float curve count mismatch: {clip_id}")
        require(sorted(len(c.get("keys", [])) for c in curves) == sorted(expected["float_curve_key_counts"]), f"float curve key-count multiset mismatch: {clip_id}")
        expected_bindings = expected["source_curve_bindings"]
        expected_by_binding = {(b["path"], b["property_name"]): b for b in expected_bindings}
        require(len(expected_by_binding) == len(expected_bindings), f"duplicate source binding keys: {clip_id}")
        actual_binding_keys = [(c.get("binding_path", ""), c.get("property_name", "")) for c in curves]
        require(len(set(actual_binding_keys)) == len(actual_binding_keys) and set(actual_binding_keys) == set(expected_by_binding), f"binding set mismatch: {clip_id}")
        for curve in curves:
            source_binding = expected_by_binding[(curve.get("binding_path", ""), curve.get("property_name", ""))]
            require(bool(curve.get("binding_type")), f"Unity binding type absent {clip_id}[{curve['property_name']}]")
            expected_type = {224: "UnityEngine.RectTransform", 225: "UnityEngine.CanvasGroup"}.get(source_binding["unity_class_id"])
            if expected_type:
                require(curve["binding_type"] == expected_type, f"Unity binding type/class differs {clip_id}[{curve['property_name']}]")
            keys = curve["keys"]
            require(len(keys) == len(source_binding["key_times"]), f"curve key count mismatch {clip_id}[{curve['property_name']}]")
            require(all(math.isfinite(float(k.get(field, float("nan")))) for k in keys for field in ("time", "value", "in_weight", "out_weight")), f"non-finite key time/value/weight {clip_id}[{curve['property_name']}]")
            require([round(float(k["time"]), 6) for k in keys] == [round(float(t), 6) for t in source_binding["key_times"]], f"source key times differ {clip_id}[{curve['property_name']}]")
            require(all(abs(float(k["value"]) - float(v)) < 1e-5 for k, v in zip(keys, source_binding["key_values"])), f"source key values differ {clip_id}[{curve['property_name']}]")
            for key in keys:
                for tangent_name, mode_name in (("in_tangent", "left_tangent_mode"), ("out_tangent", "right_tangent_mode")):
                    tangent = key.get(tangent_name, {})
                    require(isinstance(tangent, dict) and isinstance(tangent.get("finite"), bool), f"tagged tangent missing {clip_id}[{curve['property_name']}]")
                    if tangent["finite"]:
                        require(math.isfinite(float(tangent.get("value", float("nan")))) and tangent.get("special", "") == "", f"invalid finite tangent {clip_id}[{curve['property_name']}]")
                    else:
                        require(source_binding["step_constant"], f"nonfinite tangent on non-step source curve {clip_id}[{curve['property_name']}]")
                        require(tangent.get("special") in ("positive_infinity", "negative_infinity") and float(tangent.get("value", 1)) == 0.0, f"unsupported nonfinite tangent tag {clip_id}[{curve['property_name']}]")
                        require(key.get(mode_name) == "Constant", f"infinite tangent without native Constant tangent mode {clip_id}[{curve['property_name']}]")
                if source_binding["step_constant"]:
                    require(key.get("left_tangent_mode") == key.get("right_tangent_mode") == "Constant", f"source step curve lost Constant interpolation {clip_id}[{curve['property_name']}]")
            samples = curve.get("samples", [])
            require(len(samples) >= 5 + len({round(float(k["time"]), 6) for k in keys} - {round(float(actual["length"])*v, 6) for v in (0.0, 0.25, 0.5, 0.75, 1.0)}), f"insufficient key and fixed-time samples {clip_id}[{curve['property_name']}]")
            require(all(math.isfinite(float(s.get("time", float("nan")))) and math.isfinite(float(s.get("value", float("nan")))) for s in samples), f"non-finite curve sample {clip_id}[{curve['property_name']}]")
            require(all(any(abs(float(s["time"]) - float(k["time"])) < 1e-6 for s in samples) for k in keys), f"missing key-time sample {clip_id}[{curve['property_name']}]")
            for fraction in (0.0, 0.25, 0.5, 0.75, 1.0):
                fixed_time = float(actual["length"]) * fraction
                require(any(abs(float(s["time"]) - fixed_time) < 1e-6 for s in samples), f"missing fixed clip sample {fraction}: {clip_id}[{curve['property_name']}]")
            numeric_name = NUMERIC_CURVES[clip_id].get((curve["binding_path"], curve["property_name"]))
            if numeric_name:
                points = source_curves[numeric_name]
                require([float(p[0]) for p in points] == sorted(float(p[0]) for p in points)
                        and len(points) >= 2, f"invalid source preview curve: {numeric_name}")
                for sample in samples:
                    expected_value = source_hermite(points, float(sample["time"]))
                    tolerance = 0.01 if numeric_name.endswith("_x") else 0.0003
                    require(abs(float(sample["value"]) - expected_value) <= tolerance,
                            f"native AnimationCurve.Evaluate differs from source preview Hermite: {clip_id}/{numeric_name}@{sample['time']}")
        require(isinstance(actual.get("object_curves"), list), f"object-reference curve list missing: {clip_id}")
        total_curves += len(curves) + len(actual.get("object_curves", []))
        total_samples += sum(len(c.get("samples", [])) for c in curves)
    require(total_samples > 0, "no curve samples exported")
    return {"prepare": "pass", "native": "pass", "clips": len(actual_clips), "controllers": len(actual_graphs), "curves": total_curves, "float_samples": total_samples, "numeric_source_curves": 8, "numeric_native_bindings": 10, "acceptance_scope": "source-curve oracle only; no runtime Animator/UI visual acceptance"}


def main() -> int:
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--prepare", action="store_true")
    group.add_argument("--prepare-revision2", action="store_true")
    group.add_argument("--prepare-revision3", action="store_true")
    group.add_argument("--run", type=Path)
    args = parser.parse_args()
    result = (verify_prepare() if args.prepare else
              verify_revision2_prepare() if args.prepare_revision2 else
              verify_revision3_prepare() if args.prepare_revision3 else verify_run(args.run))
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
