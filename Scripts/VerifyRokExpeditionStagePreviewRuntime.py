"""Verify a real, local-only Expedition stage-preview GameViewport audit run."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parents[1]
CATALOG = ROOT / "Saved/RokUnityMigration/expedition_stage_preview.json"
PHASES = [
    "01_normal_stage1_open", "02_preview_stage1_start",
    "03_preview_stage1_mid", "04_preview_stage1_end",
    "05_normal_stage1_start", "06_normal_stage1_mid",
    "07_normal_stage1_end", "08_repeat_back_normal_boundary",
    "09_normal_stage2_selected", "10_preview_stage2_end",
    "11_normal_stage2_end", "12_challenge_boundary",
    "13_closed", "14_reopened_normal_stage1",
]
ACTIONS = [
    "open_from_hud", "preview_stage1", "back_to_normal_stage1",
    "repeat_back_normal_boundary", "select_source_stage2", "preview_stage2",
    "back_to_normal_stage2", "challenge_source_boundary",
    "close_expedition", "reopen_from_hud",
]
PHASE_CONTRACT = [
    (1, False, "Idle"), (1, True, "PreviewTransition"),
    (1, True, "PreviewTransition"), (1, True, "Idle"),
    (1, False, "NormalTransition"), (1, False, "NormalTransition"),
    (1, False, "Idle"), (1, False, "Idle"),
    (2, False, "Idle"), (2, True, "Idle"),
    (2, False, "Idle"), (2, False, "Idle"),
    (2, False, "Idle"), (1, False, "Idle"),
]


def need(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png_size(path: pathlib.Path) -> tuple[int, int]:
    data = path.read_bytes()
    need(data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR", f"PNG header: {path}")
    return struct.unpack(">II", data[16:24])


def widget(state: dict, name: str) -> dict:
    value = state["widgets"][name]
    need(value["exists"] is True, f"Missing widget: {name}")
    return value


def hermite(points: list[list[float]], time: float) -> float:
    if time <= points[0][0]:
        return points[0][1]
    for (ta, va), (tb, vb) in zip(points, points[1:]):
        if time <= tb:
            unit = (time - ta) / (tb - ta)
            smooth = unit * unit * (3 - 2 * unit)
            return va + (vb - va) * smooth
    return points[-1][1]


def validate_curves(curves: dict) -> None:
    expected = {"side_show_x", "side_hide_x", "side_show_alpha", "side_hide_alpha",
                "center_show_scale", "center_show_flag_alpha",
                "center_hide_scale", "center_hide_alpha"}
    need(set(curves) == expected, "Incomplete source animation curve set")
    for key, points in curves.items():
        need(isinstance(points, list) and len(points) >= 2,
             f"Invalid source curve: {key}")
        previous_time = -1.0
        for point in points:
            need(isinstance(point, list) and len(point) == 2
                 and all(type(value) in (int, float) and math.isfinite(value)
                         for value in point)
                 and point[0] > previous_time, f"Invalid source curve keyframe: {key}")
            previous_time = point[0]
        need(points[0][0] == 0, f"Source curve does not begin at zero: {key}")


def finite_transform(root: dict, name: str) -> None:
    values = [root["render_opacity"], root["render_translation"]["x"],
              root["render_translation"]["y"], root["render_scale"]["x"],
              root["render_scale"]["y"]]
    need(all(type(value) in (int, float) and math.isfinite(value) for value in values),
         f"Nonfinite UMG transform: {name}")


def check_business(business: dict, baseline: dict) -> None:
    need(business == baseline, "Task, wallet, roster, queue or account state changed")
    need(business["profile_kind"] == "local_preview" and business["role_id"] == ""
         and business["civilization_id"] == 0, "Account identity is not local-only")
    need(business["http_dispatch_attempts"] == 0
         and business["http_suppressed_requests"] == 0, "Server boundary crossed")
    need(business["task_id"] == 200900 and business["task_current"] == 1
         and business["task_target"] == 2 and business["task_complete"] is False,
         "Original task snapshot changed")
    need(business["active_upgrade_count"] == 0
         and business["active_construction_count"] == 0, "Local queue changed")


def check_state(state: dict, index: int, baseline: dict, primary_count: int,
                curves: dict) -> None:
    check_business(state["business"], baseline)
    need(state["controller_primary_press_count"] == primary_count,
         f"Delayed world input after Slate click: {index}")
    stage, preview, phase = PHASE_CONTRACT[index]
    is_closed = index == 12
    need(state["screen_open"] is not is_closed, f"Screen open state: {index}")
    if is_closed:
        return
    need(state["expedition_mode"] is True and state["rendered_stage_count"] == 80,
         f"Expedition data/render mode: {index}")
    need(state["selected_stage_id"] == stage and state["preview_mode"] is preview
         and state["animation_phase"] == phase, f"Stage/mode/animation: {index}")
    elapsed = state["animation_elapsed"]
    need(type(elapsed) in (int, float) and math.isfinite(elapsed),
         f"Nonfinite animation clock: {index}")
    if index == 1 or index == 4:
        need(0 <= elapsed < 0.12, f"Transition first frame: {index}")
    elif index == 2:
        need(0.12 <= elapsed <= 0.34, "Preview mid-frame outside source clip")
    elif index == 5:
        need(0.25 <= elapsed <= 0.59, "Normal mid-frame outside source clip")
    else:
        need(elapsed >= 0, f"Negative animation elapsed: {index}")
    for name in ("ExpeditionStageButton_1", "ExpeditionStageButton_2",
                 "ExpeditionStageTitle_1", "ExpeditionStageTitle_2",
                 "ExpeditionPreviewRoot", "ExpeditionSideRoot",
                 "ExpeditionCenterRoot", "ExpeditionBackButton"):
        w = widget(state, name)
        size = w["local_size"]
        need(size["x"] > 0 and size["y"] > 0, f"No cached geometry: {name}")
    side = widget(state, "ExpeditionSideRoot")
    preview_root = widget(state, "ExpeditionPreviewRoot")
    center = widget(state, "ExpeditionCenterRoot")
    for name, root in (("SideRoot", side), ("PreviewRoot", preview_root),
                       ("CenterRoot", center)):
        finite_transform(root, name)
    side_prefix = "side_hide" if preview else "side_show"
    expected_x = hermite(curves[side_prefix + "_x"], elapsed)
    expected_alpha = hermite(curves[side_prefix + "_alpha"], elapsed)
    need(abs(side["render_translation"]["x"] - expected_x) <= 0.02
         and abs(side["render_translation"]["y"]) <= 0.02
         and abs(side["render_opacity"] - expected_alpha) <= 0.002,
         f"Source side animation curve mismatch: {index}")
    preview_prefix = "side_show" if preview else "side_hide"
    expected_preview_x = hermite(curves[preview_prefix + "_x"], elapsed)
    expected_preview_alpha = hermite(curves[preview_prefix + "_alpha"], elapsed)
    need(abs(preview_root["render_translation"]["x"] - expected_preview_x) <= 0.02
         and abs(preview_root["render_translation"]["y"]) <= 0.02
         and abs(preview_root["render_opacity"] - expected_preview_alpha) <= 0.002,
         f"Source preview panel animation curve mismatch: {index}")
    center_scale_curve = "center_hide_scale" if preview else "center_show_scale"
    expected_center_scale = hermite(curves[center_scale_curve], elapsed)
    # Normal CenterRoot stays opaque in the UE slice. Source center_show_flag_alpha
    # applies to a child flag image and is not evidence of that unported layer.
    expected_center_alpha = (hermite(curves["center_hide_alpha"], elapsed)
                             if preview else 1.0)
    need(abs(center["render_scale"]["x"] - expected_center_scale) <= 0.002
         and abs(center["render_scale"]["y"] - expected_center_scale) <= 0.002
         and abs(center["render_opacity"] - expected_center_alpha) <= 0.002,
         f"Source center root animation curve mismatch: {index}")
    need(widget(state, "ExpeditionBackButton")["visible"] is True,
         "Back must remain visible in Preview and Normal")
    if index in (0, 6, 8, 10, 11, 13):
        need(widget(state, "ExpeditionPreviewButton")["visible"] is True,
             "Normal mode Preview button hidden")
        need(widget(state, "ExpeditionChallenge")["visible"] is True,
             "Normal mode Challenge button hidden")
    if index in (3, 9):
        need(widget(state, "ExpeditionPreviewButton")["visible"] is False,
             "Preview mode still offers Preview action")
        need(widget(state, "ExpeditionChallenge")["visible"] is False,
             "Preview mode exposes challenge")
    if index == 7:
        need(state["last_boundary_action_id"] == 1122,
             "Repeated Back escaped read-only Normal boundary")
    if index == 11:
        need(state["last_boundary_action_id"] == 1120,
             "Challenge did not remain a read-only source boundary")


def verify(run: pathlib.Path) -> dict:
    run = run.resolve()
    need(run.is_dir() and run.parent.name == "ExpeditionStagePreviewAudit",
         "Not an ExpeditionStagePreviewAudit run")
    manifest = json.loads((run / "manifest.json").read_text(encoding="utf-8-sig"))
    need(manifest["schema"] == "rok.expedition_stage_preview.runtime_audit.v1",
         "Runtime schema")
    need(manifest["complete"] is True and manifest["passed"] is True
         and manifest["failures"] == [], "Runtime audit incomplete/failed")
    need(pathlib.Path(manifest["project"]).resolve() == ROOT.resolve()
         and manifest["world"] == "/Game/Maps/L_RokUnityGameAppCity",
         "Wrong project or map")
    need(manifest["world_time_advanced_by_audit"] is False,
         "Audit advanced the world clock")
    source = json.loads(CATALOG.read_text(encoding="utf-8-sig"))
    need(source["schema"] == "rok.expedition_stage_preview.v1"
         and source["source_stage_count"] == 80
         and source["account_state"] == "unsynced"
         and source["preview_only"] is True,
         "Source catalog boundary")
    stage_ids = [row["id"] for row in source["stages"]]
    need(len(stage_ids) == 80 and len(set(stage_ids)) == 80
         and set(manifest["source_stage_ids"]) == {1, 2}
         and {1, 2}.issubset(stage_ids), "Source stage identity")
    for relative, expected in source["source_files_sha256"].items():
        path = ROOT / relative
        need(path.is_file() and sha(path) == expected, f"Unity source drift: {relative}")
    pins = manifest["inputs_sha256"]
    required = {"Scripts/VerifyRokExpeditionStagePreviewRuntime.py",
                "Saved/RokUnityMigration/expedition_stage_preview.json",
                "Source/Rok/RokUnityFeatureScreenWidget.cpp",
                "Source/Rok/RokExpeditionStagePreviewAuditSubsystem.cpp"}
    need(required.issubset(pins), "Missing runtime producer/input pin")
    for relative, expected in pins.items():
        path = ROOT / relative
        need(path.is_file() and sha(path) == expected, f"Runtime input drift: {relative}")
    baseline = manifest["initial_business_state"]
    curves = source["animation_zero_tangent_hermite_curves"]
    validate_curves(curves)
    check_business(baseline, baseline)
    primary_count = manifest["initial_controller_primary_press_count"]
    need(isinstance(primary_count, int) and primary_count >= 0,
         "Invalid initial controller input counter")
    actions = manifest["actions"]
    need([a["label"] for a in actions] == ACTIONS, "Real Slate action order")
    for a in actions:
        need(a["method"] == "Slate ProcessMouseMoveEvent+DownEvent+UpEvent"
             and a["own_game_window"] is True and a["target_in_hit_path"] is True
             and a["target_hovered"] is True, "Action bypassed own-window UMG route")
        need("SButton" in a["terminal_type"]
             and any("SObjectWidget" in t for t in a["hit_path_types"]),
             "Slate route did not end in UMG SButton")
        need(a["down_handled"] is True and a["up_handled"] is True
             and a["controller_primary_before"] == primary_count
             and a["controller_primary_after"] == primary_count,
             "UI click was unconsumed or reached world input")
    captures = manifest["captures"]
    need([c["phase"] for c in captures] == PHASES, "Runtime phase order/count")
    for index, capture in enumerate(captures):
        png = pathlib.Path(capture["png"])
        need(png.resolve().parent == run and png.name == PHASES[index] + ".png",
             f"PNG path: {index}")
        need(png.is_file() and png.stat().st_size == capture["png_bytes"]
             and sha(png) == capture["png_sha256"]
             and png_size(png) == (1440, 810), f"Raw viewport PNG: {index}")
        need(capture["requested_frame"] >= 0, f"Frame marker: {index}")
        for key in ("requested_state", "processed_state", "next_frame_state"):
            check_state(capture[key], index, baseline, primary_count, curves)
    check_state(manifest["final_state"], 13, baseline, primary_count, curves)
    return {"schema": "rok.expedition_stage_preview.runtime_verify.v1",
            "passed": True, "run": str(run), "capture_count": len(captures),
            "slate_action_count": len(actions), "source_stage_ids": [1, 2],
            "source_stage_count": len(stage_ids),
            "visual_boundary": "PNG evidence captured; source atlas/animation pixel comparison remains pending"}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("run", type=pathlib.Path)
    args = parser.parse_args()
    result = verify(args.run)
    output = args.run.resolve() / "verified_expedition_stage_preview.json"
    need(not output.exists(), "Refusing to overwrite existing verification report")
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
