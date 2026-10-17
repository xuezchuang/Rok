"""Independently review one finished, local-only RokSpeedUpAudit run.

Inputs are read-only. An explicit output path outside SpeedUpAudit is required;
the script never edits the audit manifest, screenshots, or Unity reference files.
Pillow is used to inspect the actual rendered PNG pixels.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import sys

from PIL import Image, ImageChops, ImageStat


ROOT = Path(__file__).resolve().parents[1]
AUDIT_ROOT = (ROOT / "Saved/RokUnityMigration/SpeedUpAudit").resolve()
SOURCE_GEOMETRY = ROOT / "Saved/RokUnityMigration/PowerGain/source_geometry.json"
CAPTURE_NAMES = [
    "01_city_source_barracks",
    "02_barracks_blocked_cityhall_requirement",
    "03_real_cityhall_prerequisite_queue",
    "04_cityhall_completed_power_plus14",
    "05_source_upgrade_20_seconds",
    "06_real_queue_started",
    "07_t1_opening",
    "08_queue_countdown",
    "09_directory_selection_no_time_change",
    "10_t1_manual_close",
    "11_reopened_original_queue",
    "12_before_natural_completion",
    "13_t1_natural_completion",
    "14_natural_power_gain_early",
    "15_natural_power_gain_later",
    "16_completed_once_hud_score",
    "17_actual_worker_queue_idle",
]
RESOURCES = ("food", "wood", "stone", "gold")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def near(a: object, b: float, tolerance: float = 0.001) -> bool:
    try:
        return math.isfinite(float(a)) and abs(float(a) - b) <= tolerance
    except (TypeError, ValueError):
        return False


def valid_resource(value: object) -> bool:
    return isinstance(value, dict) and all(
        isinstance(value.get(key), int) and not isinstance(value[key], bool) and value[key] >= 0
        for key in RESOURCES
    )


def resource_delta(before: dict, after: dict) -> dict[str, int]:
    return {key: int(before[key] - after[key]) for key in RESOURCES}


def capture_state(capture: dict) -> dict:
    return capture.get("state_at_request", {})


def state_building(state: dict, key: str) -> dict:
    return state.get(key, {})


def mean_difference(a: Image.Image, b: Image.Image, rect: tuple[int, ...]) -> float:
    difference = ImageChops.difference(a.crop(rect), b.crop(rect))
    return sum(ImageStat.Stat(difference).mean) / 3


def mean_brightness(image: Image.Image, rect: tuple[int, ...]) -> float:
    return sum(ImageStat.Stat(image.crop(rect)).mean) / 3


def scaled_rect(size: tuple[int, int], rect: tuple[int, ...]) -> tuple[int, ...]:
    width, height = size
    return (
        round(rect[0] * width / 1440),
        round(rect[1] * height / 810),
        round(rect[2] * width / 1440),
        round(rect[3] * height / 810),
    )


def review(manifest_path: Path) -> dict:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    checks: list[dict] = []
    failures: list[str] = []

    def check(name: str, passed: bool, evidence: object = None) -> None:
        checks.append({"name": name, "passed": bool(passed), "evidence": evidence})
        if not passed:
            failures.append(name)

    check("audit_version_5_complete", manifest.get("audit_version") == 5
          and manifest.get("complete") is True, {"version": manifest.get("audit_version"),
                                                   "complete": manifest.get("complete")})
    check("original_audit_reported_success", manifest.get("passed") is True
          and manifest.get("failures") == [], {"passed": manifest.get("passed"),
                                                "failures": manifest.get("failures")})
    project = str(manifest.get("project", "")).replace("\\", "/").rstrip("/")
    check("exact_project_and_city_map", project.lower() == "d:/uedemo/rok"
          and manifest.get("map") == "/Game/Maps/L_RokUnityGameAppCity",
          {"project": project, "map": manifest.get("map")})
    check("audit_scope_excludes_server_and_physical_input",
          "No authoritative inventory" in manifest.get("scope", "")
          and "No physical pointer" in manifest.get("input_method", ""),
          {"scope": manifest.get("scope"), "input_method": manifest.get("input_method")})

    captures = manifest.get("captures", [])
    check("seventeen_ordered_capture_states", isinstance(captures, list)
          and [entry.get("name") for entry in captures] == CAPTURE_NAMES
          and manifest.get("expected_capture_count") == 17,
          {"count": len(captures) if isinstance(captures, list) else None})
    captured_by_name = {entry.get("name"): entry for entry in captures if isinstance(entry, dict)}
    images: list[dict] = []
    opened: dict[str, Image.Image] = {}
    for name in CAPTURE_NAMES:
        item = captured_by_name.get(name, {})
        raw_path = item.get("path")
        screenshot = Path(raw_path).resolve() if isinstance(raw_path, str) else None
        safe = (screenshot is not None and screenshot.parent == manifest_path.parent
                and screenshot.name == f"{name}.png")
        exists = bool(safe and screenshot.is_file())
        image_record: dict = {"name": name, "path": str(screenshot) if screenshot else None,
                              "within_run": safe, "exists": exists}
        if exists:
            image_record["sha256"] = sha256(screenshot)
            image_record["file_bytes"] = screenshot.stat().st_size
            try:
                with Image.open(screenshot) as source:
                    source.verify()
                with Image.open(screenshot) as source:
                    image_record["size"] = list(source.size)
                    image_record["format"] = source.format
                    if name in CAPTURE_NAMES[11:16]:
                        opened[name] = source.convert("RGB")
            except Exception as error:  # Corrupt captures must become a failed check.
                image_record["image_error"] = str(error)
        image_valid = (exists and image_record.get("format") == "PNG"
                       and image_record.get("size") == [1440, 810]
                       and image_record.get("file_bytes") == item.get("file_bytes")
                       and item.get("width") == 1440 and item.get("height") == 810)
        images.append(image_record)
        check(f"capture_{name}_real_png", image_valid,
              {key: value for key, value in image_record.items() if key != "path"})
        requested, processed = item.get("state_at_request", {}), item.get("state_at_processed", {})
        check(f"capture_{name}_processed_after_request", bool(processed)
              and processed.get("frame", -1) >= requested.get("frame", 0)
              and processed.get("world_seconds", -1) >= requested.get("world_seconds", 0),
              {"request_frame": requested.get("frame"), "processed_frame": processed.get("frame")})
    check("capture_hashes_all_distinct", len({item.get("sha256") for item in images}) == 17
          and all(item.get("sha256") for item in images),
          {"unique_hashes": len({item.get("sha256") for item in images})})

    events = [item for item in manifest.get("actions", []) if item.get("event") == "OnLocalUpgradeCompleted"]
    expected_events = [(5, 1, 2, 7, 21), (6, 1, 2, 5, 10)]
    event_summary = [
        {key: event.get(key) for key in ("sequence", "init_id", "old_level", "new_level",
                                         "old_power", "new_power", "world_seconds")}
        for event in events
    ]
    check("two_exact_source_completion_events", len(events) == 2 and all(
        (event.get("sequence"), event.get("init_id"), event.get("old_level"),
         event.get("new_level"), event.get("old_power"), event.get("new_power"))
        == (index, *expected)
        for index, (event, expected) in enumerate(zip(events, expected_events), start=1)
    ), event_summary)
    hall_deadline = manifest.get("last_observed_state", {}).get("prerequisite_finishes_at")
    barracks_deadline = manifest.get("last_observed_state", {}).get("original_finishes_at")
    check("completion_events_after_source_deadlines", len(events) == 2
          and isinstance(hall_deadline, (int, float))
          and isinstance(barracks_deadline, (int, float))
          and hall_deadline - 0.0001 <= events[0].get("world_seconds", -1) <= hall_deadline + 0.1
          and barracks_deadline - 0.0001 <= events[1].get("world_seconds", -1) <= barracks_deadline + 0.1
          and events[0].get("world_seconds", 0) < events[1].get("world_seconds", 0),
          {"events": event_summary, "hall_deadline": hall_deadline,
           "barracks_deadline": barracks_deadline})
    check("natural_wall_durations", 1.9 <= manifest.get("prerequisite_natural_wall_seconds", -1) < 10
          and 19.5 <= manifest.get("natural_completion_wall_seconds", -1) < 40,
          {"hall": manifest.get("prerequisite_natural_wall_seconds"),
           "barracks": manifest.get("natural_completion_wall_seconds")})
    action_names = [item.get("button") for item in manifest.get("actions", []) if "button" in item]
    check("actual_umg_delegate_sequence", action_names.count("TimedUpgradeButton") == 2
          and action_names.count("BuildingUpgradeButton") == 3
          and "SpeedUpItemAction_1" in action_names
          and all(item.get("input") == "Actual OnClicked delegate; not physical pointer input"
                  for item in manifest.get("actions", []) if "button" in item), action_names)

    last = manifest.get("last_observed_state", {})
    named = {name: capture_state(captured_by_name.get(name, {})) for name in CAPTURE_NAMES}
    hall_active = state_building(named["03_real_cityhall_prerequisite_queue"],
                                 "last_prerequisite_observation")
    hall_done = state_building(named["04_cityhall_completed_power_plus14"],
                               "last_prerequisite_observation")
    target_active = state_building(named["06_real_queue_started"], "last_barracks_observation")
    before_finish = state_building(named["12_before_natural_completion"],
                                   "last_barracks_observation")
    target_done = state_building(named["16_completed_once_hud_score"],
                                 "last_barracks_observation")
    check("hall_and_barracks_real_queue_transitions",
          hall_active.get("active") is True and hall_active.get("level") == 1
          and hall_done.get("active") is False and hall_done.get("level") == 2
          and target_active.get("active") is True and target_active.get("level") == 1
          and before_finish.get("active") is True and before_finish.get("level") == 1
          and target_done.get("active") is False and target_done.get("level") == 2,
          {"hall_start": hall_active, "hall_done": hall_done,
           "barracks_start": target_active, "barracks_before": before_finish,
           "barracks_done": target_done})
    check("source_queue_intervals_two_and_twenty_seconds",
          near(hall_active.get("finishes_at", 0) - hall_active.get("started_at", 0), 2)
          and near(target_active.get("finishes_at", 0) - target_active.get("started_at", 0), 20),
          {"hall_seconds": hall_active.get("finishes_at", 0) - hall_active.get("started_at", 0),
           "barracks_seconds": target_active.get("finishes_at", 0) - target_active.get("started_at", 0)})
    queue_samples = manifest.get("queue_samples", [])
    active_samples = [sample for sample in queue_samples if sample.get("active") is True]
    check("normal_queue_samples_before_and_after_deadline", len(active_samples) >= 2
          and any(sample.get("active") is False and sample.get("level") == 2 for sample in queue_samples)
          and all(sample.get("world_seconds", 0) <= sample.get("finishes_at", 0) + 0.1
                  and near(sample.get("finishes_at", 0) - sample.get("started_at", 0), 20)
                  for sample in active_samples)
          and all(b.get("world_seconds", 0) >= a.get("world_seconds", 0)
                  for a, b in zip(queue_samples, queue_samples[1:])),
          {"sample_count": len(queue_samples), "active_count": len(active_samples)})

    resources = {key: manifest.get(key) for key in (
        "resources_before_prerequisite", "resources_after_prerequisite_debit",
        "resources_before_timed_upgrade", "resources_after_one_source_debit")}
    valid_resources = all(valid_resource(value) for value in resources.values())
    debits = None
    if valid_resources:
        debits = {
            "city_hall": resource_delta(resources["resources_before_prerequisite"],
                                        resources["resources_after_prerequisite_debit"]),
            "barracks": resource_delta(resources["resources_before_timed_upgrade"],
                                       resources["resources_after_one_source_debit"]),
        }
    check("source_local_resource_debits_once_each", valid_resources
          and resources["resources_before_prerequisite"] == {"food": 5000, "wood": 5000,
                                                             "stone": 1500, "gold": 600}
          and resources["resources_after_prerequisite_debit"] == resources["resources_before_timed_upgrade"]
          and debits == {"city_hall": {"food": 3500, "wood": 3500, "stone": 0, "gold": 0},
                         "barracks": {"food": 500, "wood": 800, "stone": 0, "gold": 0}},
          {"resources": resources, "debits": debits})
    check("profile_preview_score_6200_6214_6219", manifest.get("initial_preview_score") == 6200
          and manifest.get("baseline_preview_score") == 6214
          and named["04_cityhall_completed_power_plus14"].get("profile_preview_score") == 6214
          and named["12_before_natural_completion"].get("profile_preview_score") == 6214
          and named["16_completed_once_hud_score"].get("profile_preview_score") == 6219
          and last.get("profile_preview_score") == 6219
          and last.get("profile_building_delta") == 19,
          {"initial": manifest.get("initial_preview_score"), "after_hall": manifest.get("baseline_preview_score"),
           "after_barracks": last.get("profile_preview_score"), "building_delta": last.get("profile_building_delta")})
    check("final_hud_6219_and_effect_stopped", last.get("hud_power_text") == "6,219"
          and named["16_completed_once_hud_score"].get("hud_power_text") == "6,219"
          and last.get("power_playing") is False and manifest.get("completion_events") == 2
          and manifest.get("power_play_count") == 2, last)

    power_samples = manifest.get("power_samples", [])
    intervals: list[list[dict]] = []
    for sample in power_samples:
        if not intervals or sample.get("playing") and not intervals[-1][-1].get("playing"):
            intervals.append([])
        intervals[-1].append(sample)
    interval_summary = [
        {"samples": len(interval), "first_score": interval[0].get("local_score"),
         "last_score": interval[-1].get("local_score"),
         "first_playback": interval[0].get("playback_seconds"),
         "last_playback": interval[-1].get("playback_seconds"),
         "finished": interval[-1].get("playing") is False}
        for interval in intervals if interval
    ]
    check("two_real_tick_power_playback_intervals", len(intervals) == 2
          and all(len(interval) >= 10 and near(interval[0].get("playback_seconds"), 0, 0.1)
                  and interval[-1].get("playing") is False
                  and 0.95 <= interval[-1].get("playback_seconds", -1) <= 1.15
                  and all(b.get("world_seconds", 0) >= a.get("world_seconds", 0)
                          for a, b in zip(interval, interval[1:]))
                  for interval in intervals)
          and [interval[0].get("local_score") for interval in intervals] == [6214, 6219],
          interval_summary)
    early = named["14_natural_power_gain_early"]
    late = named["15_natural_power_gain_later"]
    finished = named["16_completed_once_hud_score"]
    check("effect_request_states_follow_second_completion", len(events) == 2
          and all(state.get("completion_events") == 2 and state.get("power_play_count") == 2
                  and state.get("power_playing") is True for state in (early, late))
          and 0.08 <= early.get("power_playback_seconds", -1) <= 0.5
          and 0.45 <= late.get("power_playback_seconds", -1) <= 0.9
          and early.get("power_playback_seconds", 0) < late.get("power_playback_seconds", 0)
          and finished.get("power_playing") is False,
          {"early_seconds": early.get("power_playback_seconds"),
           "late_seconds": late.get("power_playback_seconds")})

    visual_metrics = None
    if all(name in opened for name in CAPTURE_NAMES[13:16]):
        early_image = opened[CAPTURE_NAMES[13]]
        late_image = opened[CAPTURE_NAMES[14]]
        idle_image = opened[CAPTURE_NAMES[15]]
        size = early_image.size
        effect_rect = scaled_rect(size, (235, 2, 290, 58))
        control_rect = scaled_rect(size, (300, 4, 440, 60))
        visual_metrics = {
            "effect_roi": effect_rect,
            "control_roi": control_rect,
            "early_to_idle_mean_abs_rgb": mean_difference(early_image, idle_image, effect_rect),
            "late_to_idle_mean_abs_rgb": mean_difference(late_image, idle_image, effect_rect),
            "early_to_late_mean_abs_rgb": mean_difference(early_image, late_image, effect_rect),
            "control_early_to_idle_mean_abs_rgb": mean_difference(early_image, idle_image, control_rect),
            "control_late_to_idle_mean_abs_rgb": mean_difference(late_image, idle_image, control_rect),
            "early_effect_roi_mean_rgb": mean_brightness(early_image, effect_rect),
            "late_effect_roi_mean_rgb": mean_brightness(late_image, effect_rect),
            "idle_effect_roi_mean_rgb": mean_brightness(idle_image, effect_rect),
        }
    check("visible_local_power_effect_changes_then_fades", visual_metrics is not None
          and visual_metrics["early_to_idle_mean_abs_rgb"] >= 20
          and visual_metrics["late_to_idle_mean_abs_rgb"] >= 15
          and visual_metrics["early_to_late_mean_abs_rgb"] >= 5
          and visual_metrics["early_to_idle_mean_abs_rgb"] >= visual_metrics["control_early_to_idle_mean_abs_rgb"] + 15
          and visual_metrics["late_to_idle_mean_abs_rgb"] >= visual_metrics["control_late_to_idle_mean_abs_rgb"] + 10
          and visual_metrics["early_effect_roi_mean_rgb"] > visual_metrics["idle_effect_roi_mean_rgb"] + 15
          and visual_metrics["late_effect_roi_mean_rgb"] > visual_metrics["idle_effect_roi_mean_rgb"] + 10,
          visual_metrics)

    geometry: dict = {}
    geometry_sha = None
    if SOURCE_GEOMETRY.is_file():
        geometry = json.loads(SOURCE_GEOMETRY.read_text(encoding="utf-8-sig"))
        geometry_sha = sha256(SOURCE_GEOMETRY)
    source_paths = {item.get("path") for item in geometry.get("sources", [])
                    if isinstance(item.get("path"), str)}
    samples = geometry.get("samples", [])
    source_inputs = []
    for item in geometry.get("sources", []):
        relative = item.get("path")
        source = (ROOT / relative).resolve() if isinstance(relative, str) else None
        safe = source is not None and source.is_relative_to(ROOT)
        actual_hash = sha256(source) if safe and source.is_file() else None
        source_inputs.append({"path": relative, "sha256": actual_hash,
                              "matches_export": actual_hash == item.get("sha256")})
    check("ui10010_export_matches_read_only_unity_sources", len(source_inputs) == 8
          and all(item["matches_export"] for item in source_inputs), source_inputs)
    check("ui10010_composite_source_draw_contract", geometry.get("unityVersion") == "2019.4.28f1c1"
          and geometry.get("lifetime") == 1 and geometry.get("sampleRate") == 120
          and len(geometry.get("mesh", {}).get("positions", [])) == 6
          and len(geometry.get("mesh", {}).get("indices", [])) == 12
          and len(samples) == 121 and bool(samples)
          and len(samples[0].get("particle", {}).get("indices", [])) == 0
          and len(samples[1].get("particle", {}).get("indices", [])) > 0
          and any(path and path.endswith("UI_10010.prefab") for path in source_paths)
          and any(path and path.endswith("UI_10010.anim") for path in source_paths)
          and any(path and path.endswith("m_liupian001.FBX") for path in source_paths)
          and any(path and path.endswith("tile003.mat") for path in source_paths)
          and any(path and path.endswith("glow002.mat") for path in source_paths),
          {"geometry_sha256": geometry_sha, "mesh_vertices": len(geometry.get("mesh", {}).get("positions", [])),
           "particle_frames": len(samples), "source_paths": sorted(source_paths)})
    power_widget = (ROOT / "Source/Rok/RokPowerGainWidget.cpp").read_text(encoding="utf-8-sig")
    hud = (ROOT / "Source/Rok/RokMainHudWidget.cpp").read_text(encoding="utf-8-sig")
    audit_source = (ROOT / "Source/Rok/RokSpeedUpAuditSubsystem.cpp").read_text(encoding="utf-8-sig")
    forbidden_direct_mutation = re.search(
        r"(?:->|\.)\s*(?:StartUpgrade|AdvanceWindowAnimationForTesting|AdvanceHudAnimations|"
        r"SetPowerPresentation|SetTimeDilation|SpawnActor|FlushPendingActionForTesting)\s*\(",
        audit_source,
    )
    check("audit_uses_actual_buttons_without_direct_queue_or_effect_mutation",
          "Button->OnClicked.Broadcast()" in audit_source
          and "OnLocalUpgradeCompleted.AddUObject" in audit_source
          and "RokAutoSelectBuilding=UnityInitId_6" in audit_source
          and forbidden_direct_mutation is None,
          {"audit_source_sha256": sha256(ROOT / "Source/Rok/RokSpeedUpAuditSubsystem.cpp"),
           "forbidden_direct_call": forbidden_direct_mutation.group(0)
           if forbidden_direct_mutation else None})
    check("ue_composite_draw_and_live_hud_binding_present",
          all(token in power_widget for token in ("MakeCustomVerts", "Draw(O.Mesh", "Draw(O.ParticleFrames",
                                                  "ObservePower", "Current > Previous && bVisible"))
          and all(token in hud for token in ("SetPowerPresentation(ProfileState.LocalPreviewScore)",
                                             "PowerGain->ObservePower", "RefreshAccumulator >= 0.25f")),
          {"power_widget_sha256": sha256(ROOT / "Source/Rok/RokPowerGainWidget.cpp"),
           "hud_sha256": sha256(ROOT / "Source/Rok/RokMainHudWidget.cpp")})

    return {
        "schema_version": 1,
        "reviewed_at_utc": datetime.now(timezone.utc).isoformat(),
        "passed": not failures,
        "scope": "Independent review of local timed-upgrade completion, score projection, HUD playback, and localized PNG change. Physical pointer input, server combatPower/inventory, and full Unity pixel equivalence are outside this verifier.",
        "manifest": str(manifest_path),
        "manifest_sha256": sha256(manifest_path),
        "checks": checks,
        "failures": failures,
        "events": event_summary,
        "resource_debits": debits,
        "power_intervals": interval_summary,
        "visual_metrics": visual_metrics,
        "images": images,
        "source_geometry_sha256": geometry_sha,
        "unity_source_inputs": source_inputs,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path,
                        help="Fresh SpeedUpAudit/<UTC>_<PID>/manifest.json")
    parser.add_argument("--output", required=True, type=Path,
                        help="New scoped_runtime_review.json outside SpeedUpAudit")
    args = parser.parse_args()
    manifest_path = args.manifest.resolve()
    output_path = args.output.resolve()
    if not manifest_path.is_file() or manifest_path.name != "manifest.json" \
            or not manifest_path.is_relative_to(AUDIT_ROOT):
        parser.error("--manifest must be an existing SpeedUpAudit run's manifest.json")
    if output_path == manifest_path or output_path.is_relative_to(AUDIT_ROOT):
        parser.error("--output must be outside SpeedUpAudit; historical runs are immutable inputs")
    if output_path.exists():
        parser.error("--output already exists; choose a new review path")
    report = review(manifest_path)
    report["report_path"] = str(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Local power runtime review: passed={report['passed']} checks={len(report['checks'])} "
          f"failures={len(report['failures'])} output={output_path}")
    for failure in report["failures"]:
        print(f"FAIL: {failure}", file=sys.stderr)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
