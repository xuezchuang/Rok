#!/usr/bin/env python3
"""Fail-closed verifier for the opt-in live GameApp temporary-placement audit."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "Saved" / "RokUnityMigration" / "CityPlacementRuntimeAudit"
PHASES = [
    "01_hall2_legal_prerequisite",
    "02_farm_preview",
    "03_ui_origin_no_drag",
    "04_world_press_camera_lock",
    "05_subgrid_no_change",
    "06_crosscell_valid",
    "07_occupied_invalid",
    "08_outbound_invalid",
    "09_release_camera_restored",
    "10_cancel_restores_city",
    "11_wood_switch_one_ghost",
]
UI_ACTIONS = {
    "hall_upgrade_menu", "hall_timed_upgrade_start", "build_city_open_farm",
    "farm_source_card", "placement_cancel_button", "build_city_reopen",
    "farm_reopen_card", "switch_to_wood_build_city", "wood_source_card",
}
WORLD_ACTIONS = {
    "world_press_farm_ghost", "subgrid_0p04_move", "cross_source_grid_cell",
    "drag_to_occupied_grid", "drag_to_outbound_grid", "release_world_drag",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def grid(state: dict) -> tuple[int, int]:
    return int(state["grid"]["x"]), int(state["grid"]["y"])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path, required=True,
                        help="Exact CityPlacementRuntimeAudit run directory")
    args = parser.parse_args()
    run = args.run.resolve()
    require(run.parent == EVIDENCE.resolve(), "Run must be a direct child of CityPlacementRuntimeAudit")
    manifest_path = run / "manifest.json"
    require(manifest_path.is_file(), "Missing manifest.json")
    data = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    require(data["schema"] == "rok.city_placement.runtime_audit.v1", "Wrong schema")
    require(data["project"].replace("\\", "/").rstrip("/").lower() ==
            str(ROOT).replace("\\", "/").rstrip("/").lower(), "Wrong project identity")
    require(data["world"] == "/Game/Maps/L_RokUnityGameAppCity", "Wrong GameApp world")
    require(data["complete"] is True and data["passed"] is True, "Runtime audit did not pass")
    require(data["failures"] == [], "Runtime recorded failures")
    require(data["capture_count"] == len(PHASES) ==
            data["expected_capture_count"], "Wrong capture count")
    require(data["production_placement_delegate"] is True and
            data["audit_listener_bound"] is False, "Placement used an audit-only callback")
    require(data["formal_construction_started"] is False and
            data["visual_fidelity_accepted"] is False, "Scope overclaimed")
    window = data["own_game_window_preparation"]
    require(window["verified_own_game_window"] and window["verified_1440x810_client"] and
            window["attempted"] and window["visible_after_show"] and
            not window["minimized_after_show"] and window["foreground_is_game_after_call"],
            "The identified Rok GameApp window was not prepared for physical pointer audit")
    require(data["world_travel_covered"] is False and
            data["other_resolutions_covered"] is False, "Unmeasured scope claimed")

    inputs = data["inputs_sha256"]
    require(len(inputs) >= 10, "Missing source and production input hashes")
    for relative, digest in inputs.items():
        path = ROOT / relative
        require(path.is_file(), f"Missing hashed input: {relative}")
        require(sha256(path) == digest.lower(), f"Input changed since runtime: {relative}")

    base = data["authorized_hall_prerequisite_and_placement_baseline"]
    require(base["authorized_hall_cost_food"] == base["authorized_hall_cost_wood"] == 3500,
            "Hall cost mismatch")
    require(base["authorized_hall_duration_seconds"] == 2 and
            base["authorized_hall_power_delta"] == 14 and
            base["hall_completion_events"] == 1, "Hall completion mismatch")
    require((base["wallet_food"], base["wallet_wood"],
             base["wallet_stone"], base["wallet_gold"]) == (1500, 1500, 1500, 600),
            "Hall settlement wallet mismatch")
    require(base["initial_source_rows"] == 26 and
            base["initial_runtime_actors"] == 24, "Source/runtime city sets conflated")
    require(base["initial_source_actor_instances"] >= 24 and
            base["initial_unity_init_tagged_actors"] == 24,
            "Missing fixed SourceActor and UnityInitId baselines")
    require(base["active_upgrade_queues"] == base["active_local_tickets"] == 0 and
            base["owned_queues"] == 1,
            "Expected zero active queues and one permanent starter worker capacity")

    captures = data["captures"]
    require([entry["phase_name"] for entry in captures] == PHASES, "Capture order differs")
    states: dict[str, dict] = {}
    for item in captures:
        name = item["phase_name"]
        path = Path(item["file"]).resolve()
        require(path == run / f"{name}.png", f"PNG escaped run directory: {name}")
        require(path.is_file() and path.stat().st_size == item["file_bytes"] > 10000,
                f"PNG missing/too small: {name}")
        require(path.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n", f"Invalid PNG: {name}")
        require(sha256(path) == item["png_sha256"].lower(), f"PNG hash mismatch: {name}")
        require(item["pre_capture_ready_frames"] >= 2 and
                item["post_capture_ready_frames"] >= 2 and
                item["pre_capture_render_ready"]["observation_frame"] <=
                item["processed"]["frame"] and
                item["post_capture_render_ready"]["observation_frame"] >=
                item["processed"]["frame"] + 2,
                f"Screenshot lacked two-frame pre/post render readiness: {name}")
        for stage in ("pre_capture_render_ready", "post_capture_render_ready"):
            ready = item[stage]
            require(ready["global_compile_remaining_read_only"] == 0,
                    f"Global async assets remained at {name}/{stage}")
            require(ready["ghost_expected"] == item["processed"]["active"] and
                    ready["ghost_present"] == item["processed"]["active"],
                    f"Ghost changed while capturing {name}/{stage}")
            if ready["ghost_expected"]:
                require(ready["distinct_target_render_frame"] is True,
                        f"Target preview did not render on a distinct frame: {name}/{stage}")
                for role in ("building", "bottom"):
                    component = ready[role]
                    require(component["mesh_ready"] and component["material_ready"] and
                            component["component_ready"] and component["textures_ready"] and
                            component["rendered_after_targeted_compile"] and
                            len(component["textures"]) >= 1 and
                            all(texture["ready"] for texture in component["textures"]),
                            f"Target preview asset or rendered component not ready: {name}/{stage}/{role}")
        state = item["processed"]
        states[name] = state
        require(state["hall_completion_events"] == 1 and state["placement_baseline_frozen"],
                f"Hall prerequisite not settled in {name}")
        require((state["food"], state["wood"], state["stone"], state["gold"]) ==
                (1500, 1500, 1500, 600), f"Placement charged currency in {name}")
        require(state["upgrade_queues"] == state["local_construction_tickets"] == 0 and
                state["owned_queues"] == 1,
                f"Placement changed active queue/ticket or permanent worker ownership in {name}")
        require(state["power_delta"] == 14 and
                state["profile_sha256"] == base["profile_sha256"] and
                state["task_sha256"] == base["task_sha256"],
                f"Placement changed profile, task or power in {name}")
        require(state["selection_is_initial_hall"], f"Selection lost in {name}")
        require(state["source_actor_count"] == base["initial_source_actor_instances"] and
                state["new_source_actor_count"] == 0 and
                state["unity_init_tagged_actor_count"] ==
                base["initial_unity_init_tagged_actors"] and
                state["new_unity_init_tagged_actor_count"] == 0 and
                state["formal_constructed_tagged_actor_count"] == 0 and
                state["first_formal_ticket_exists"] is False,
                f"Formal/new city actor or ledger ticket observed in {name}")

    def check(name: str, active: bool, dragging: bool, valid: bool,
              type_id: int, ghosts: int) -> dict:
        s = states[name]
        require((s["active"], s["dragging"], s["grid_valid"],
                 s["type_id"], s["ghost_actor_count"]) ==
                (active, dragging, valid, type_id, ghosts),
                f"Placement snapshot mismatch in {name}")
        require(s["camera_input_suspended"] == dragging and
                s["controls_visible"] == active, f"Camera/HUD mismatch in {name}")
        return s

    check(PHASES[0], False, False, False, 0, 0)
    farm = check(PHASES[1], True, False, True, 2, 1)
    ui = check(PHASES[2], True, False, True, 2, 1)
    press = check(PHASES[3], True, True, True, 2, 1)
    sub = check(PHASES[4], True, True, True, 2, 1)
    crossed = check(PHASES[5], True, True, True, 2, 1)
    occupied = check(PHASES[6], True, True, False, 2, 1)
    outbound = check(PHASES[7], True, True, False, 2, 1)
    check(PHASES[8], True, False, False, 2, 1)
    check(PHASES[9], False, False, False, 0, 0)
    check(PHASES[10], True, False, True, 3, 1)
    require(grid(farm) == grid(ui) == grid(press) == grid(sub),
            "UI-origin or sub-grid move changed Farm grid")
    require(grid(crossed) != grid(farm) and
            grid(occupied) != grid(crossed) and
            grid(outbound) != grid(occupied), "World drag did not visit distinct target grids")
    require("占用" in occupied["reason"] and
            "范围" in outbound["reason"], "Invalid source footprint reasons absent")

    actions = data["actions"]
    for pointer_action, captured in (
        ("cross_source_grid_cell", crossed),
        ("drag_to_occupied_grid", occupied),
        ("drag_to_outbound_grid", outbound),
    ):
        preflight = [a for a in actions if a["action"] == "held_grid_target_preflight"
                     and a["pointer_action"] == pointer_action]
        require(len(preflight) == 1 and preflight[0]["preflight_exact_source_grid"] and
                grid({"grid": preflight[0]["target_grid"]}) == grid(captured) and
                grid({"grid": preflight[0]["source_math_predicted_grid"]}) == grid(captured) and
                abs(preflight[0]["target_ground_world"]["z"]) < 1e-5,
                f"Held pointer did not preflight the exact source grid: {pointer_action}")
        routed = [a for a in actions if a["action"] == pointer_action]
        require(len(routed) == 1 and
                grid({"grid": routed[0]["source_predicted_grid"]}) == grid(captured),
                f"Routed held pointer diverged from source drag math: {pointer_action}")
    names = [item["action"] for item in actions]
    require(UI_ACTIONS.issubset(names) and WORLD_ACTIONS.issubset(names),
            "Missing actual input route actions")
    require("ui_origin_press_move_release" in names and "escape_cancel" in names,
            "Missing UI-origin or Escape route")
    escape = [a for a in actions if a["action"] == "escape_cancel"]
    escape_later = [a for a in actions
                    if a["action"] == "escape_next_input_frame_observation"]
    require(len(escape) == len(escape_later) == 1 and
            escape[0]["method"] == "GameViewportClient::InputKey Escape" and
            escape[0]["scope"] == "engine_game_viewport_keyboard_route_not_os_focus" and
            escape[0]["device_valid"] and escape[0]["target_local_player_present"] and
            escape[0]["target_controller_matches"] and
            escape[0]["device_platform_user_id"] ==
            escape[0]["controller_platform_user_id"] and
            isinstance(escape[0]["down_returned_handled_immediate"], bool) and
            isinstance(escape[0]["up_returned_handled_immediate"], bool) and
            escape_later[0]["sent_frame"] == escape[0]["sent_frame"] and
            escape_later[0]["observed_frame"] > escape[0]["sent_frame"],
            "Escape device route or later game input frame evidence missing")
    escape_state = escape_later[0]["state_before_finish"]
    require(escape_later[0]["escape_cancel_count_after"] ==
            escape[0]["escape_cancel_count_before"] + 1 and
            escape_later[0]["escape_cancel_count_after"] ==
            escape_state["escape_cancel_count"] and
            escape[0]["last_escape_cancel_frame_before"] <
            escape_later[0]["last_escape_cancel_frame"] ==
            escape_state["last_escape_cancel_frame"] and
            escape[0]["sent_frame"] <
            escape_later[0]["last_escape_cancel_frame"] <=
            escape_later[0]["observed_frame"],
            "The Controller Escape handler did not cancel this preview exactly once")
    require(not escape_state["active"] and not escape_state["dragging"] and
            not escape_state["camera_input_suspended"] and
            escape_state["ghost_actor_count"] == 0 and
            escape_state["selection_is_initial_hall"] and
            (escape_state["food"], escape_state["wood"],
             escape_state["stone"], escape_state["gold"]) ==
            (base["wallet_food"], base["wallet_wood"],
             base["wallet_stone"], base["wallet_gold"]) and
            escape_state["upgrade_queues"] == escape_state["local_construction_tickets"] == 0 and
            escape_state["profile_sha256"] == base["profile_sha256"] and
            escape_state["task_sha256"] == base["task_sha256"],
            "Next-frame Escape did not cancel without mutating city authority")
    world_down = [a for a in actions if a["action"] == "world_press_farm_ghost"]
    world_up = [a for a in actions if a["action"] == "release_world_drag"]
    world_observations = [a for a in actions
                          if a["action"] == "world_press_next_frame_observation"]
    require(len(world_down) == len(world_up) == 1,
            "World input press/release action must each occur once")
    require(len(world_observations) == 1 and
            world_observations[0]["state_before_failure_cleanup"]["last_placement_press_frame"] ==
            world_observations[0]["state_before_failure_cleanup"]["last_primary_press_frame"] and
            world_observations[0]["state_before_failure_cleanup"]["world_frame_projection"]
            ["grid_center_world_valid"] and
            world_observations[0]["state_before_failure_cleanup"]["preview_geometry"]
            ["building"]["present"],
            "Placement press lacks same-frame Controller stage or source geometry evidence")
    down, up = world_down[0], world_up[0]
    for action in (down, up):
        for when in ("before_route", "after_route"):
            route = action[when]
            require(route["hit_path_valid"] and route["hit_path_contains_viewport"] and
                    not route["hit_path_contains_umg"] and
                    route["game_window_present"] and route["game_hwnd_owned_by_process"] and
                    isinstance(route["captor_path_widget_types"], list),
                    f"World input route/owner diagnostics missing: {action['action']}/{when}")
        require(action["before_route"]["os_cursor_hits_own_window"],
                f"OS world pointer did not hit Rok window: {action['action']}")
    require(down["primary_press_count_after"] > down["primary_press_count_before"] or
            press["primary_press_event_count"] > down["primary_press_count_before"],
            "Slate Down did not reach Controller primary input")
    require(press["placement_drag_begin_count"] > down["drag_begin_count_before"] and
            press["last_primary_press_outcome"] == "placement_dispatched",
            "Controller did not begin source-ghost drag")
    require(up["pointer_release_count_after"] > up["pointer_release_count_before"] or
            states[PHASES[8]]["placement_pointer_release_count"] >
            up["pointer_release_count_before"],
            "Slate Up did not reach Controller placement release")
    ui_origin = [a for a in actions if a["action"] == "ui_origin_press_move_release"]
    require(len(ui_origin) == 1 and ui_origin[0]["target"] == "TaskProgress_0" and
            ui_origin[0]["down_target_and_hud_in_hit_path"] and
            ui_origin[0]["up_target_and_hud_in_hit_path"] and
            ui_origin[0]["down_handled"] and ui_origin[0]["up_handled"],
            "UI-origin Down/Up was not consumed through actual task HUD path")
    next_frame_checks = [a for a in actions
                         if a["action"] == "check_preview_snapshot" and
                         a["step"] == 70 and a["frame"] > ui_origin[0]["frame"]]
    require(len(next_frame_checks) == 1 and
            next_frame_checks[0]["active"] and
            not next_frame_checks[0]["dragging"] and
            next_frame_checks[0]["valid"] and
            next_frame_checks[0]["ghost_actor_count"] == 1,
            "UI-origin no-drag was not verified on a later game frame")
    settled_steps: set[int] = set()
    for action in actions:
        if action["action"] == "target_settled":
            require(action["method"] ==
                    "host animation idle + two stable cached-geometry frames" and
                    action["width"] > 0 and action["height"] > 0,
                    "Invalid settled target evidence")
            settled_steps.add(action["step"])
        if action["action"] in UI_ACTIONS:
            require(action["step"] in settled_steps,
                    f"UMG target was clicked before animation/geometry settled: {action['action']}")
            require(action["method"] == "Slate MoveAway+Move+Down+Up" and
                    action["target_in_hit_path"] and action["target_hovered"] and
                    action["down_handled"] and action["up_handled"],
                    f"UMG action bypassed Slate: {action['action']}")
        if action["action"] in WORLD_ACTIONS:
            require(action["viewport_hit_path"] and not action["ui_hit_path"],
                    f"World pointer bypassed SViewport: {action['action']}")
            if "handled" in action:
                require(action["handled"], f"Unrouted world button: {action['action']}")
    final = data["final_state"]
    require(not final["active"] and not final["dragging"] and
            final["ghost_actor_count"] == 0 and final["selection_is_initial_hall"] and
            final["hall_completion_events"] == 1 and
            final["source_actor_count"] == base["initial_source_actor_instances"] and
            final["new_source_actor_count"] == 0 and
            final["unity_init_tagged_actor_count"] ==
            base["initial_unity_init_tagged_actors"] and
            final["new_unity_init_tagged_actor_count"] == 0 and
            final["formal_constructed_tagged_actor_count"] == 0 and
            final["first_formal_ticket_exists"] is False and
            final["profile_sha256"] == base["profile_sha256"] and
            final["task_sha256"] == base["task_sha256"],
            "Escape did not restore immutable baseline")
    print(json.dumps({"passed": True, "run": str(run), "captures": len(captures),
                      "actual_slate_actions": len(actions), "hall_completion_events": 1},
                     ensure_ascii=False))


if __name__ == "__main__":
    main()
