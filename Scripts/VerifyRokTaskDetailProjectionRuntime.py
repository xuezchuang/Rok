"""Strictly verify the opt-in GameApp task-detail projection capture."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parents[1]
EXPECTED_PHASES = [
    "01_initial_hall1_task_1of2",
    "02_upgrade_pending_task_1of2",
    "03_natural_completion_task_2of2",
    "04_reopened_task_2of2",
]
EXPECTED_ACTIONS = [
    "open_initial_task_from_hud",
    "close_initial_task",
    "hall_upgrade_menu",
    "hall_timed_upgrade_start",
    "open_task_during_upgrade",
    "close_completed_task",
    "reopen_completed_task_from_hud",
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


def check_state(state: dict, level: int, progress: int, *, open_: bool, pending: bool = False) -> None:
    need(state["hall_level"] == level, "Hall level")
    need(state["hall_upgrade_in_progress"] is pending, "Hall pending state")
    need(state["active_upgrade_count"] == int(pending), "Upgrade queue count")
    need(state["upgrade_completion_events"] == int(level == 2), "Completion callback count")
    need(state["bootstrap_task_id"] == 200900, "Bootstrap task identity")
    need(state["bootstrap_task_type"] == 8 and state["bootstrap_task_param1"] == 1
         and state["bootstrap_task_param2"] == 2 and state["bootstrap_task_reward_group"] == 200900,
         "Bootstrap task configuration mutated")
    need(state["bootstrap_task_current"] == 1 and state["bootstrap_task_target"] == 2, "Bootstrap task mutated")
    need(state["bootstrap_task_complete"] is False, "Server task marked complete")
    need(state["local_projected"] is True and state["local_display_current"] == progress,
         "Local task projection")
    need(state["local_condition_met"] is (progress == 2), "Local condition flag")
    need(state["profile_state_kind"] == "local_preview" and not state["profile_role_id"]
         and state["profile_civilization_id"] == 0, "Account identity changed")
    need(state["http_dispatch_attempts"] == 0 and state["suppressed_requests"] == 0,
         "Unauthorized server request")
    need(state["task_screen_open"] is open_, "TaskInfo visibility")
    if open_:
        need(state["local_projection_rendered"] is True, "TaskInfo not using local display projection")
        need(state["local_condition_met_rendered"] is (progress == 2), "Rendered local condition")
        need(state["displayed_current"] == progress, "Rendered progress getter")
        need("本地" in state["task_title"] and f"({progress}/2)" in state["task_description"],
             "Displayed title/progress")
        need("服务器任务状态待同步" in state["task_tooltip"], "Task projection boundary tooltip")
        need(state["task_action_label"] == "前往" and state["task_action_enabled"] is True
             and state["task_action_visible"] is True, "Reward became claimable")
        need(state["task_scroll_visible"] is True, "Task scroll missing")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("run", type=pathlib.Path, help="Unique TaskDetailProjectionAudit run directory")
    args = parser.parse_args()
    run = args.run.resolve()
    need(run.is_dir() and run.parent.name == "TaskDetailProjectionAudit", "Unexpected run directory")
    manifest_path = run / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    need(manifest["schema"] == "rok.task_detail_projection.runtime_audit.v1", "Schema")
    need(manifest["complete"] is True and manifest["passed"] is True and not manifest["failures"],
         "Audit did not pass")
    need(manifest["world"] == "/Game/Maps/L_RokUnityGameAppCity", "World identity")
    need(pathlib.Path(manifest["project"]).resolve() == ROOT.resolve(), "Project identity")
    need(manifest["world_time_advanced_by_audit"] is False, "Audit changed world clock")
    for relative, expected in manifest["inputs_sha256"].items():
        path = ROOT / relative
        need(path.is_file() and sha(path) == expected, f"Input hash: {relative}")
    actions = manifest["actions"]
    need([a["label"] for a in actions] == EXPECTED_ACTIONS, "Ordinary Slate action sequence")
    for action in actions:
        need(action["method"] == "Slate ProcessMouseMoveEvent+DownEvent+UpEvent", "Injected action route")
        need(action["own_game_window"] is True and action["target_in_hit_path"] is True
             and action["target_hovered"] is True, "Not the own-window UMG target")
        need(action["terminal_type"] == "SButton" and any("SObjectWidget" in t for t in action["hit_path_types"]),
             "Slate path must terminate at UMG SButton (SViewport ancestry is permitted)")
        need(action["down_handled"] is True and action["up_handled"] is True, "Unconsumed Slate click")
        need(action["controller_primary_before"] == action["controller_primary_after"], "UI click leaked to world press")
    captures = manifest["captures"]
    need([c["phase"] for c in captures] == EXPECTED_PHASES, "Capture phase order")
    for i, capture in enumerate(captures):
        path = pathlib.Path(capture["png"])
        need(path.parent == run and path.name == EXPECTED_PHASES[i] + ".png", "PNG path")
        need(path.is_file() and path.stat().st_size == capture["png_bytes"]
             and sha(path) == capture["png_sha256"] and png_size(path) == (1440, 810), "PNG identity")
        requested = capture["requested_state"]
        processed = capture["processed_state"]
        if i == 0:
            check_state(requested, 1, 1, open_=True)
            check_state(processed, 1, 1, open_=True)
            need(requested["wallet_food"] == 5000 and requested["wallet_wood"] == 5000,
                 "Initial wallet")
        elif i == 1:
            check_state(requested, 1, 1, open_=True, pending=True)
            check_state(processed, 1, 1, open_=True, pending=True)
            need(requested["wallet_food"] == 1500 and requested["wallet_wood"] == 1500,
                 "Ordinary upgrade cost")
        else:
            check_state(requested, 2, 2, open_=True)
            check_state(processed, 2, 2, open_=True)
            need(requested["wallet_food"] == 1500 and requested["wallet_wood"] == 1500,
                 "Unexpected reward/economy mutation")
        need(requested["profile_local_building_score"] == (0 if i < 2 else 14),
             "Local power projection differs from completed Hall upgrade")
    immutable_task_fields = (
        "bootstrap_task_id", "bootstrap_task_name", "bootstrap_task_description",
        "bootstrap_task_type", "bootstrap_task_param1", "bootstrap_task_param2",
        "bootstrap_task_reward_group", "bootstrap_task_current", "bootstrap_task_target",
        "bootstrap_task_complete", "bootstrap_task_next_id", "bootstrap_task_chain_count",
    )
    original = captures[0]["requested_state"]
    for capture in captures:
        for field in immutable_task_fields:
            need(capture["requested_state"][field] == original[field]
                 and capture["processed_state"][field] == original[field],
                 f"Account bootstrap task mutated: {field}")
    check_state(manifest["final_state"], 2, 2, open_=True)
    need(manifest["upgrade_completion_events"] == 1, "Exactly one natural completion")
    need(manifest["upgrade_completed_wall_seconds"] - manifest["upgrade_pressed_wall_seconds"] >= 1.8,
         "Upgrade was completed without the two-second source timer")
    result = {
        "schema": "rok.task_detail_projection.runtime_verify.v1",
        "passed": True,
        "manifest_sha256": sha(manifest_path),
        "captures": EXPECTED_PHASES,
        "actions": EXPECTED_ACTIONS,
        "scope": "Local display condition and real Hall upgrade UI; no server task completion/reward claim",
    }
    out = run / "verified_task_detail_projection.json"
    out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
