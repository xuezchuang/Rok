#!/usr/bin/env python3
"""Fail-closed review of the opt-in task 200900 real-Slate Hall Go route."""
from __future__ import annotations

import argparse
import copy
import hashlib
import io
import json
import re
import tempfile
from pathlib import Path

from PIL import Image


SCRIPT = Path(__file__).resolve()
ROOT = SCRIPT.parents[1] if SCRIPT.parent.name == "Scripts" else SCRIPT.parents[2]
STAGE = ROOT / "Saved/PendingMainTaskGoToTownHallRuntimeAuditV7"
SCHEMA = "rok.main_task_go_town_hall.runtime_audit.v1"
PHASES = ["00_city_hud", "01_tasks_fully_open", "02_exact_hall_building_info", "03_hud_return"]
ACTIONS = ["open_tasks_from_hud", "go_to_cached_hall_info", "close_hall_info"]
TARGETS = ["TaskEntryButton", "TaskAction_0", "Type2CloseButton"]
INDEXED_BUTTON_SOURCE = "Source/Rok/RokUnityFeatureScreenWidget.h"
INDEXED_BUTTON_CLASS = "/Script/Rok.RokUnityIndexedButton"
INDEXED_BUTTON_DECLARATION = re.compile(
    r"class\s+ROK_API\s+URokUnityIndexedButton\s*:\s*public\s+URokUiAnimatedButton\b"
)
EXPECTED_ACTION_WIDGET_CLASSES = (
    "/Script/Rok.RokUiAnimatedButton",  # TaskEntryButton is created by MainHud MakeAtlasButton.
    INDEXED_BUTTON_CLASS,                 # TaskAction_0 is created by Feature MakeActionButton.
    "/Script/Rok.RokUiAnimatedButton",  # Type2CloseButton is created by Modal MakeAtlasButton.
)
PINNED = {
    "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestMediator.cs": "1ac142e1d65155320eb73558a3402d2d8e961e663d545c5fde491cabb7a95a60",
    "ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/TaskScriptGlobalMediator.cs": "43e328f7897e7248096f9bea5efe1b96e3ce60aa1b62b0cdeb8a4ef2cb38e58d",
    "ref/config-tables/编译客户端配置/CSV/TaskMain.csv": "704a6fb0dd5d8601094f7207ea15f2b209d5be9ed2d0a778839d940bcac08b91",
    "Saved/RokUnityMigration/unity_runtime_bootstrap_state.json": "07fcf9600d8e79afba470ef92c71460dd217e82e22dfaafc236a4fd58b5b0994",
    "Saved/RokUnityMigration/unity_initial_city_state.json": "2633514629f9e39744226b2665f6f30e1c2d007e46a7d2ac019a45d72e498804",
    "Source/Rok/RokUnityFeatureScreenWidget.h": "d782e3b7a0f69af84068e2ed3b1ddeb285c141b96e8d714aebee12907971fe5f",
    "Source/Rok/RokUnityFeatureScreenWidget.cpp": "388221ef002653184e5d155cf5afe319730a5398008d4234ef3f8925a352aec1",
    "Source/Rok/RokMainHudWidget.h": "2740efb6b5c8a68222d5429f4d66e37e8e484302a215b6c46bbd8c783f67a2e8",
    "Source/Rok/RokMainHudWidget.cpp": "6cb5e93c80e60fc2a6663d584e6d531fadd0da2f9a4c7583b2477557640da0f1",
    "Source/Rok/RokBuildingUpgradeSubsystem.h": "b2253b3fa8232a873e6f4f07fe4cd0f3127eb4cbd3c720a356794186e93ea7b4",
    "Source/Rok/RokBuildingUpgradeSubsystem.cpp": "b20503a0b0dfdfe552502ef307e6d8e6a79c9677e7a185fc0f589cd6c45a8ae5",
    "Source/Rok/RokUnityModalWidget.h": "50cec5ddbdba9db12730edad021b8f359477d2b596a6504bc554dfda8ac220d3",
    "Source/Rok/RokUnityModalWidget.cpp": "9c2fca7d169a40b8a5a10ba4ef036b07a6fc76719871f35451c6f49be800171b",
    "Source/Rok/RokPlayerController.h": "6eaa6329a03804c3af143c9777e8654d5dd83cfe24f598779b6107c988ffe772",
    "Source/Rok/RokPlayerController.cpp": "f5c297bfb9b972db07e7725a9663a42e2c35f928625167e033e1bad97c6c5ef6",
    "Source/Rok/RokUnityRuntimeStateSubsystem.h": "e00dbdf4e20775f08d6498bcb06659d3516f29d824835c98a86533b8ca7e5112",
    "Source/Rok/RokUnityRuntimeStateSubsystem.cpp": "3776b514262c0e25093c0d32e6c8e68fbf927dfb0040616ead4e0ae49f3b2c9e",
    "Source/Rok/RokServerSubsystem.h": "4a07946d525ccb9eaedafcc30e68315e4ca5c52e94a88fc4c9b388833a07a5a9",
    "Source/Rok/RokServerSubsystem.cpp": "3d73a28ff42b787c02a5c5a50ceba5bcac47056cc0945608bfea9ff9bd91bde4",
    "Source/Rok/RokUiAnimatedButton.h": "4379cd36d5fba08d3b8c0fbf5c1076b8ce8b8cb27d2fec3c7fc2a4a65c62a3f7",
    "Source/Rok/RokUiAnimatedButton.cpp": "bf8da24bdce3dfb403b7174eaa496e95550547cc47c57dccaca6e57aa39bedcf",
    "Source/Rok/RokUiWindowAnimation.h": "1c39a74fd70ecec8f1e9b2fd135f65b312e86f2be60ebf323596d8a8359f9e3f",
    "Source/Rok/RokUiWindowAnimation.cpp": "bae58440eccb1ba826871acee2131b1223094b134cf0cfe04a730c798e2b41e4",
    "Source/Rok/RokTrainArmyMeshData.h": "e0c1e4f9f698366526ac740fd60f238076dc2c5ebc0e5ea85299a92fa9072945",
    "Source/Rok/RokTrainArmyMeshData.cpp": "e3dfd99ea973b2eba075c117d326ed16ddb44b5188c08740180c4d31c8593a9a",
    "Saved/PendingMainTaskGoToTownHallV2/implementation_report.json": "e8624b9a9c9cc04ed2e0b2f1ad79a0291328882a661c1f954a2e80719b1fed12",
}
AUDIT_PATHS = (
    "Source/Rok/RokMainTaskGoToTownHallAuditSubsystem.h",
    "Source/Rok/RokMainTaskGoToTownHallAuditSubsystem.cpp",
    "Scripts/VerifyRokMainTaskGoToTownHallRuntime.py",
)


def need(condition: bool, reason: str) -> None:
    if not condition:
        raise ValueError(reason)


def sha_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha(path: Path) -> str:
    return sha_bytes(path.read_bytes())


def expected_inputs() -> dict[str, str]:
    report = json.loads((STAGE / "release_report.json").read_text(encoding="utf-8"))
    need(report.get("schema") == "rok.main_task_go_town_hall.audit_stage.v7", "staged report schema")
    staged = report["files"]
    expected = dict(PINNED)
    for path in AUDIT_PATHS:
        need(path in staged, f"missing staged audit pin: {path}")
        expected[path] = staged[path]["sha256"]
    return expected


def checked_project_file(root: Path, relative: str) -> Path:
    path = (root / relative).resolve(strict=True)
    need(path == root or root in path.parents, f"input outside project: {relative}")
    return path


def _point(value: dict) -> tuple[float, float]:
    return float(value["x"]), float(value["y"])


def _present(state: dict, name: str) -> bool:
    value = state["widgets"][name]
    return value["exists"] is True and value["visible"] is True


def check_business(business: dict) -> None:
    need(business["hall_cached_exact_binding"] is True, "Hall cache/binding absent")
    need(business["hall_key"] == "UnityInitId_5" and business["hall_init_id"] == 5
         and business["hall_source_type"] == 1 and business["hall_type"] == 1,
         "not exact source Hall")
    need("UnityInitId_5" in business["hall_tags"] and "UnityBuildingType_1" in business["hall_tags"],
         "source Hall tags missing")
    need(business["hall_level"] == 1 and business["hall_upgrade_in_progress"] is False
         and abs(business["hall_upgrade_started_at"]) < 1e-6
         and abs(business["hall_upgrade_finishes_at"]) < 1e-6,
         "Hall not idle cached L1")
    need(business["task_id"] == 200900 and business["task_type"] == 8
         and business["task_param1"] == 1 and business["task_param2"] == 2
         and business["task_current"] == 1 and business["task_target"] == 2
         and business["task_complete"] is False, "wrong active source task")
    need(business["active_upgrades"] == 0 and business["pending_completed_upgrades"] == 0
         and business["active_construction"] == 0
         and business["construction_ticket_1_exists"] is False
         and business["completed_upgrade_events"] == 0, "queue/ticket/completion boundary")
    need(business["profile_kind"] == "local_preview" and business["profile_role_id"] == ""
         and business["profile_civilization_id"] == 0
         and business["profile_has_role_id"] is False
         and business["profile_has_civilization"] is False, "account not local preview")
    need(business["server_local_only"] is True and business["http_dispatch_attempts"] == 0
         and business["http_suppressed_requests"] == 0, "HTTP boundary violated")
    need(business["hall_actor_path"] in business["city_actor_paths"]
         and len(business["city_actor_paths"]) == len(set(business["city_actor_paths"])),
         "city actor identity set invalid")
    need(isinstance(business["starter_heroes"], list) and len(business["starter_heroes"]) == 5
         and isinstance(business["starter_troops"], list) and len(business["starter_troops"]) == 4,
         "local bootstrap roster changed")
    for key in ("hall_power", "hall_produce_per_hour", "hall_gather_min", "hall_gather_max",
                "wallet_food", "wallet_wood", "wallet_stone", "wallet_gold", "owned_queues",
                "rate_food", "rate_wood", "rate_stone", "rate_gold", "task_chain",
                "trainable_troop_ids", "task_name", "task_description", "task_require",
                "task_icon", "profile_display_name", "profile_civilization_name",
                "profile_local_hero_score", "profile_local_troop_power",
                "profile_local_preview_score", "profile_local_building_upgrade_score"):
        need(key in business, f"business field missing: {key}")
    need(all(set(("id", "name", "description", "type", "param1", "param2", "require",
                  "reward", "next_id", "progress_rule", "current", "target", "complete", "icon")) <= set(x)
             for x in business["task_chain"]), "task chain projection incomplete")
    need(all(set(("id", "level", "star", "max_star", "rare", "score", "skill_count",
                  "talent_count", "icon", "model")) <= set(x) for x in business["starter_heroes"]),
         "hero snapshot incomplete")
    need(all(set(("id", "arms_type", "level", "count", "power_per_unit", "power",
                  "train_seconds", "icon", "model")) <= set(x) for x in business["starter_troops"]),
         "troop snapshot incomplete")


def check_state(state: dict, phase: str, baseline: dict, primary: int, camera: tuple,
                controller_selection: str) -> None:
    need(state["business"] == baseline, f"business mutated: {phase}")
    need(state["primary_press_count"] == primary, f"world primary click leaked: {phase}")
    need(state["selected_actor"] == controller_selection,
         f"controller selection changed: {phase}")
    current_camera = tuple(state[key] for key in
                           ("camera_x", "camera_y", "camera_z", "camera_pitch", "camera_yaw", "camera_roll",
                            "camera_scale_x", "camera_scale_y", "camera_scale_z"))
    need(all(abs(a-b) <= .001 for a,b in zip(current_camera, camera)), f"camera moved: {phase}")
    if phase in ("00_city_hud", "03_hud_return"):
        need(state["screen_open"] is False and state["modal_open"] is False
             and _present(state, "TaskEntryButton"), "did not return to idle HUD")
    elif phase == "01_tasks_fully_open":
        mode = state["widgets"]["FeatureModeCanvas"]
        need(state["screen_open"] is True and state["task_page"] == 1
             and state["task_local_projection"] is True and state["task_displayed_current"] == 1
             and state["modal_open"] is False and _present(state, "TaskAction_0")
             and _present(state, "TaskTitle_0") and _present(state, "TaskDescription_0")
             and mode["visible"] is True and abs(mode["opacity"]-1) <= .01
             and all(abs(v-1) <= .01 for v in _point(mode["scale"])),
             "Tasks page did not settle on source task 200900")
    else:
        need(phase == "02_exact_hall_building_info" and state["screen_open"] is False
             and state["modal_open"] is True and state["hall_info_exact"] is True
             and state["modal_selected_actor"] == baseline["hall_actor_path"]
             and all(_present(state, name) for name in
                     ("Type2CloseButton", "BuildingInfoPreviewFrame", "BuildingInfoAttributeFrame")),
             "exact Hall BuildingInfo content not mounted")


def check_png(path: Path, expected_sha: str, expected_bytes: int) -> None:
    data = path.read_bytes()
    need(len(data) == expected_bytes and sha_bytes(data) == expected_sha, "PNG hash/bytes mismatch")
    with Image.open(io.BytesIO(data)) as image:
        need(image.format == "PNG" and image.size == (1440, 810), "PNG format/extent mismatch")
        image.verify()
    with Image.open(io.BytesIO(data)) as image:
        image.load()
        need(image.size == (1440, 810), "PNG pixel decode failed")


def validate(data: dict, run_dir: Path, expected: dict[str, str], root: Path,
             verify_files: bool = True, require_engine: bool = True) -> None:
    need(data["schema"] == SCHEMA and data["complete"] is True and data["passed"] is True
         and data["failures"] == [], "run did not complete cleanly")
    need(Path(data["project"]).resolve() == root.resolve() and data["world"] == "/Game/Maps/L_RokUnityGameAppCity",
         "project/map identity mismatch")
    need(data["engine_major"] == 5 and data["engine_minor"] == 8, "wrong engine version")
    if require_engine:
        need(Path(data["engine_dir"]).resolve() == Path("D:/ue/UE_5.8/Engine").resolve(),
             "wrong UE5.8 install")
    need(data["scope"].startswith("Task 200900 cached idle L1 Hall")
         and data["baseline_phase"] == PHASES[1]
         and data["world_time_advanced_by_audit"] is False
         and data["source_pixel_parity_claim"] is False, "scope/baseline overclaim")
    need(data["completion_events"] == 0 and data["attempts"] == [], "failed click attempt or completion event")
    inputs = data["inputs_sha256"]
    need(set(inputs) == set(expected), "source/producer input set mismatch")
    for relative, required_sha in expected.items():
        need(inputs[relative] == required_sha, f"input pin differs: {relative}")
        if verify_files:
            need(sha(checked_project_file(root, relative)) == required_sha, f"input bytes changed: {relative}")
    need(INDEXED_BUTTON_SOURCE in expected, "indexed-button inheritance source is not pinned")
    indexed_header = checked_project_file(root, INDEXED_BUTTON_SOURCE).read_text(encoding="utf-8-sig")
    need(INDEXED_BUTTON_DECLARATION.search(indexed_header) is not None,
         "pinned header does not prove URokUnityIndexedButton derives from URokUiAnimatedButton")
    actions = data["actions"]
    captures = data["captures"]
    need(len(actions) == 3 and len(captures) == 4, "actions/captures incomplete")
    need([a["label"] for a in actions] == ACTIONS and [c["phase"] for c in captures] == PHASES,
         "real action/phase order mismatch")
    baseline = data["baseline_business"]
    check_business(baseline)
    first_baseline_state = captures[1]["requested_state"]
    need(first_baseline_state["business"] == baseline, "baseline not taken at Tasks fully open")
    controller_selection = first_baseline_state["selected_actor"]
    need(isinstance(controller_selection, str), "controller selection is not an actor path string")
    initial_business = captures[0]["requested_state"]["business"]
    primary = first_baseline_state["primary_press_count"]
    camera = tuple(first_baseline_state[key] for key in
                   ("camera_x", "camera_y", "camera_z", "camera_pitch", "camera_yaw", "camera_roll",
                    "camera_scale_x", "camera_scale_y", "camera_scale_z"))
    previous_frame = -1
    for index, capture in enumerate(captures):
        phase = PHASES[index]
        request, processed, later = (capture[k] for k in
                                     ("requested_state", "processed_state", "next_frame_state"))
        frames = (capture["requested_frame"], capture["processed_frame"], capture["next_frame"])
        need(frames[0] <= frames[1] < frames[2] and previous_frame < frames[0],
             f"processed screenshot frame order: {phase}")
        need((request["frame"], processed["frame"], later["frame"]) == frames,
             f"state was not captured at declared frames: {phase}")
        previous_frame = frames[2]
        png = Path(capture["png"])
        need(png.is_absolute() and png.resolve() == (run_dir / f"{phase}.png").resolve(strict=True),
             f"PNG is not the unique real phase file: {phase}")
        need(capture["png_width"] == 1440 and capture["png_height"] == 810,
             f"declared PNG extent wrong: {phase}")
        check_png(png, capture["png_sha256"], capture["png_bytes"])
        for state in (request, processed, later):
            if index == 0:
                need(state["business"] == initial_business, "business changed during initial HUD screenshot")
                need(state["selected_actor"] == controller_selection,
                     "controller selection changed during initial HUD screenshot")
                need(state["business"]["http_dispatch_attempts"] == 0
                     and state["business"]["http_suppressed_requests"] == 0,
                     "HTTP active before Tasks baseline")
                need(state["screen_open"] is False and state["modal_open"] is False
                     and _present(state, "TaskEntryButton"), "initial HUD capture invalid")
            else:
                check_state(state, phase, baseline, primary, camera, controller_selection)
    for index, action in enumerate(actions):
        need(captures[index]["next_frame"] < action["frame"] < captures[index+1]["requested_frame"],
             f"click did not precede its resulting frame: {ACTIONS[index]}")
        need(action["target"] == TARGETS[index]
             and action["method"] == "ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent"
             and action["target_exists"] is True and action["exact_button_in_path"] is True
             and action["target_hovered"] is True and action["umg_path_present"] is True
             and action["own_game_window"] is True
             and action["foreground"] is True and action["down_handled"] is True
             and action["up_handled"] is True and action["failure"] == ""
             and action["stable_frames"] >= 2, f"real Slate proof missing: {ACTIONS[index]}")
        path_index = action["exact_button_path_index"]
        types = action["hit_path_types"]
        nodes = action["hit_path_nodes"]
        need(isinstance(path_index, int) and 0 <= path_index < len(types)
             and len(nodes) == len(types) and "SButton" in types[path_index]
             and any("SObjectWidget" in x for x in types)
             and nodes[path_index]["type"] == types[path_index]
             and nodes[path_index]["slate_identity"] == action["target_slate_identity"]
             and len(set(node["slate_identity"] for node in nodes)) == len(nodes),
             f"hit path did not contain the exact UButton: {ACTIONS[index]}")
        need(action["target_widget_class"] == EXPECTED_ACTION_WIDGET_CLASSES[index]
             and action["target_widget_identity"].startswith("0x")
             and action["own_window_identity"] == action["foreground_window_identity"]
             and action["own_window_identity"].startswith("0x")
             and action["own_window_pid"] == data["pid"],
             "target class/own foreground process identity wrong")
        need(action["primary_before"] == primary and action["primary_after"] == primary,
             "world primary event leaked through UI route")
        need(action["business_before"]["http_dispatch_attempts"] == 0
             and action["business_after"]["http_dispatch_attempts"] == 0
             and action["business_before"]["http_suppressed_requests"] == 0
             and action["business_after"]["http_suppressed_requests"] == 0,
             "HTTP boundary crossed during UI click")
        if index >= 1:
            need(action["business_before"] == baseline and action["business_after"] == baseline,
                 f"synchronous business mutation in {ACTIONS[index]}")
        else:
            need(action["business_before"] == initial_business,
                 "opening Tasks did not start from captured HUD state")
        need(action["screen_open_before"] is (index == 1)
             and action["modal_open_before"] is (index == 2),
             f"click stack before-state wrong: {ACTIONS[index]}")
        center = _point(action["absolute_center"])
        top_left = _point(action["absolute_top_left"])
        bottom_right = _point(action["absolute_bottom_right"])
        need(top_left[0] <= center[0] <= bottom_right[0] and top_left[1] <= center[1] <= bottom_right[1],
             "click point outside target cached geometry")
    check_state(data["final_state"], PHASES[3], baseline, primary, camera, controller_selection)
    need(data["final_state"]["frame"] >= captures[-1]["next_frame"], "final readback before final capture")


def self_test() -> int:
    """Synthetic verifier-only cases; not a UE runtime or screenshot acceptance."""
    with tempfile.TemporaryDirectory(prefix="rok_hall_go_verify_") as tmp:
        directory = Path(tmp)
        image = Image.new("RGBA", (1440, 810), (10, 20, 30, 255))
        png_data = io.BytesIO()
        image.save(png_data, format="PNG")
        sample = png_data.getvalue()
        business = {
            "hall_cached_exact_binding": True, "hall_key": "UnityInitId_5", "hall_init_id": 5,
            "hall_source_type": 1, "hall_type": 1, "hall_actor_path": "/Game/Maps/City.Hall_5",
            "hall_tags": ["UnityBuildingType_1", "UnityInitId_5"], "city_actor_paths": ["/Game/Maps/City.Hall_5"],
            "hall_level": 1, "hall_upgrade_in_progress": False,
            "hall_upgrade_started_at": 0, "hall_upgrade_finishes_at": 0,
            "task_id": 200900, "task_type": 8, "task_param1": 1, "task_param2": 2,
            "task_current": 1, "task_target": 2, "task_complete": False,
            "active_upgrades": 0, "pending_completed_upgrades": 0, "active_construction": 0,
            "construction_ticket_1_exists": False, "completed_upgrade_events": 0,
            "profile_kind": "local_preview", "profile_has_role_id": False, "profile_role_id": "",
            "profile_has_civilization": False, "profile_civilization_id": 0,
            "profile_display_name": "", "profile_civilization_name": "",
            "profile_local_hero_score": 0, "profile_local_troop_power": 0,
            "profile_local_preview_score": 0, "profile_local_building_upgrade_score": 0,
            "server_local_only": True, "http_dispatch_attempts": 0, "http_suppressed_requests": 0,
            "starter_heroes": [{"id": i, "level": 1, "star": 0, "max_star": 0, "rare": 0,
                                "score": 0, "skill_count": 0, "talent_count": 0,
                                "icon": "", "model": ""} for i in range(5)],
            "starter_troops": [{"id": i, "arms_type": 0, "level": 1, "count": 0,
                               "power_per_unit": 0, "power": 0, "train_seconds": 0,
                               "icon": "", "model": ""} for i in range(4)],
            "hall_power": 7, "hall_produce_per_hour": 0, "hall_gather_min": 0, "hall_gather_max": 0,
            "wallet_food": 5000, "wallet_wood": 5000, "wallet_stone": 0, "wallet_gold": 0,
            "owned_queues": 1, "rate_food": 0, "rate_wood": 0, "rate_stone": 0,
            "rate_gold": 0, "task_name": "", "task_description": "", "task_require": 0,
            "task_icon": "", "task_chain": [{"id": 200900, "name": "", "description": "",
                "type": 8, "param1": 1, "param2": 2, "require": 0, "reward": 0,
                "next_id": 0, "progress_rule": "building_level", "current": 1,
                "target": 2, "complete": False, "icon": ""}], "trainable_troop_ids": [1],
        }
        def widget(name: str, visible: bool = True) -> dict:
            return {"exists": True, "visible": visible, "name": name, "enabled": True,
                    "opacity": 1, "scale": {"x": 1, "y": 1}}
        def state(phase: int, frame: int) -> dict:
            widgets = {name: widget(name, phase == expected_phase) for name, expected_phase in
                       {"TaskEntryButton": 0, "FeatureModeCanvas": 1, "TaskAction_0": 1,
                        "TaskTitle_0": 1, "TaskDescription_0": 1,
                        "Type2CloseButton": 2, "BuildingInfoPreviewFrame": 2,
                        "BuildingInfoAttributeFrame": 2}.items()}
            widgets["TaskEntryButton"]["visible"] = phase in (0,3)
            return {"frame": frame, "business": copy.deepcopy(business), "primary_press_count": 0,
                    "camera_x": 0, "camera_y": 0, "camera_z": 100,
                    "camera_pitch": -45, "camera_yaw": 0, "camera_roll": 0,
                    "camera_scale_x": 1, "camera_scale_y": 1, "camera_scale_z": 1,
                    "widgets": widgets, "screen_open": phase == 1, "task_page": 1 if phase == 1 else -1,
                    "task_local_projection": phase == 1, "task_displayed_current": 1 if phase == 1 else 0,
                    "modal_open": phase == 2, "hall_info_exact": phase == 2,
                    "modal_selected_actor": business["hall_actor_path"] if phase == 2 else "",
                    "selected_actor": ""}
        captures = []
        for i, phase in enumerate(PHASES):
            file = directory / f"{phase}.png"
            file.write_bytes(sample)
            first = 10 + i*10
            captures.append({"phase": phase, "png": str(file), "png_sha256": sha_bytes(sample),
                             "png_bytes": len(sample), "png_width": 1440, "png_height": 810,
                             "requested_frame": first, "processed_frame": first+1, "next_frame": first+2,
                             "requested_state": state(i, first), "processed_state": state(i, first+1),
                             "next_frame_state": state(i, first+2)})
        actions = []
        for i, label in enumerate(ACTIONS):
            actions.append({"label": label, "target": TARGETS[i], "target_exists": True,
                            "method": "ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent",
                            "frame": 15 + i*10, "exact_button_in_path": True,
                            "exact_button_path_index": 2,
                            "hit_path_types": ["SWindow", "SObjectWidget", "SButton", "STextBlock"],
                            "hit_path_nodes": [{"index": k, "type": t, "slate_identity": f"0x{k+1}"}
                                               for k,t in enumerate(["SWindow", "SObjectWidget", "SButton", "STextBlock"])],
                            "terminal_type": "STextBlock", "target_hovered": True,
                            "umg_path_present": True, "target_widget_class": "/Script/UMG.Button",
                            "target_widget_identity": "0x10", "target_slate_identity": "0x3",
                            "own_game_window": True, "foreground": True, "down_handled": True,
                            "own_window_identity": "0x100", "foreground_window_identity": "0x100",
                            "own_window_pid": 123,
                            "up_handled": True, "failure": "", "stable_frames": 2,
                            "primary_before": 0, "primary_after": 0,
                            "business_before": copy.deepcopy(business),
                            "business_after": copy.deepcopy(business),
                            "screen_open_before": i == 1,
                            "modal_open_before": i == 2,
                            "absolute_center": {"x": 110, "y": 110},
                            "absolute_top_left": {"x": 100, "y": 100},
                            "absolute_bottom_right": {"x": 120, "y": 120}})
        source_file = directory / "synthetic/input"
        source_file.parent.mkdir()
        source_file.write_bytes(b"synthetic pinned source")
        indexed_header_path = directory / INDEXED_BUTTON_SOURCE
        indexed_header_path.parent.mkdir(parents=True)
        indexed_header_bytes = b"class ROK_API URokUnityIndexedButton : public URokUiAnimatedButton {};\n"
        indexed_header_path.write_bytes(indexed_header_bytes)
        expected = {"synthetic/input": sha(source_file), INDEXED_BUTTON_SOURCE: sha(indexed_header_path)}
        data = {"schema": SCHEMA, "complete": True, "passed": True, "failures": [],
                "project": str(directory), "world": "/Game/Maps/L_RokUnityGameAppCity",
                "engine_dir": str(directory), "engine_major": 5, "engine_minor": 8,
                "pid": 123,
                "scope": "Task 200900 cached idle L1 Hall synthetic verifier fixture",
                "baseline_phase": PHASES[1], "world_time_advanced_by_audit": False,
                "source_pixel_parity_claim": False, "completion_events": 0,
                "inputs_sha256": expected, "actions": actions, "attempts": [],
                "captures": captures, "baseline_business": business,
                "final_state": state(3, 43)}
        # Positive fixtures model the three exact production action classes.
        data["actions"][0]["target_widget_class"] = EXPECTED_ACTION_WIDGET_CLASSES[0]
        data["actions"][1]["target_widget_class"] = EXPECTED_ACTION_WIDGET_CLASSES[1]
        data["actions"][2]["target_widget_class"] = EXPECTED_ACTION_WIDGET_CLASSES[2]
        validate(data, directory, expected, directory, verify_files=True, require_engine=False)
        mutations = [
            lambda d: d["inputs_sha256"].update({"synthetic/input": "b"*64}),
            lambda d: d["actions"][1].update({"exact_button_in_path": False}),
            lambda d: d["actions"][1].update({"target_widget_class": "/Script/Rok.ForeignButtonSubclass"}),
            lambda d: d["actions"][0].update({"target_widget_class": INDEXED_BUTTON_CLASS}),
            lambda d: d["actions"][1].update({"target_widget_class": "/Script/Rok.RokUiAnimatedButton"}),
            lambda d: d["actions"][2].update({"target_widget_class": INDEXED_BUTTON_CLASS}),
            lambda d: d["actions"][1].update({"target_slate_identity": "0x999"}),
            lambda d: d["actions"][1].update({"foreground_window_identity": "0x200"}),
            lambda d: d["actions"][1].update({"terminal_type": "SButton", "hit_path_types": ["SWindow", "SObjectWidget"]}),
            lambda d: d["actions"][1].update({"foreground": False}),
            lambda d: d["actions"][1].update({"down_handled": False}),
            lambda d: d["actions"][1].update({"primary_after": 1}),
            lambda d: d["actions"][1]["business_after"].update({"wallet_food": 4999}),
            lambda d: d["captures"][2]["processed_state"]["business"].update({"wallet_food": 4999}),
            lambda d: d["captures"][2]["next_frame_state"]["business"].update({"hall_power": 21}),
            lambda d: d["captures"][1]["next_frame_state"]["business"].update({"http_dispatch_attempts": 1}),
            lambda d: d["captures"][2].update({"png_sha256": "0"*64}),
            lambda d: d["captures"][2].update({"processed_frame": d["captures"][2]["next_frame"]}),
            lambda d: d["captures"][2]["requested_state"].update({"hall_info_exact": False}),
            lambda d: d.update({"attempts": [{"failure": "missed"}]}),
            lambda d: d["final_state"].update({"modal_open": True}),
            lambda d: d["captures"][0]["processed_state"].update({"selected_actor": "unexpected"}),
            lambda d: d["captures"][2]["requested_state"].update({"selected_actor": "unexpected"}),
            lambda d: d["captures"][3]["next_frame_state"].update({"selected_actor": "unexpected"}),
            lambda d: d["captures"][2]["processed_state"].update({"modal_selected_actor": "wrong Hall"}),
            lambda d: d["captures"][2]["requested_state"]["widgets"]["Type2CloseButton"].update({"exists": False}),
            lambda d: d["captures"][2]["requested_state"]["widgets"]["BuildingInfoPreviewFrame"].update({"exists": False}),
            lambda d: d["captures"][2]["requested_state"]["widgets"]["BuildingInfoAttributeFrame"].update({"exists": False}),
        ]
        for index, mutation in enumerate(mutations):
            altered = copy.deepcopy(data)
            mutation(altered)
            try:
                validate(altered, directory, expected, directory, verify_files=True, require_engine=False)
            except (ValueError, KeyError, TypeError):
                continue
            raise ValueError(f"negative case {index} passed unexpectedly")
        bad_header_bytes = b"class ROK_API URokUnityIndexedButton : public UButton {};\n"
        indexed_header_path.write_bytes(bad_header_bytes)
        wrong_inheritance_expected = dict(expected)
        wrong_inheritance_expected[INDEXED_BUTTON_SOURCE] = sha(indexed_header_path)
        wrong_inheritance_data = copy.deepcopy(data)
        wrong_inheritance_data["inputs_sha256"][INDEXED_BUTTON_SOURCE] = wrong_inheritance_expected[INDEXED_BUTTON_SOURCE]
        try:
            validate(wrong_inheritance_data, directory, wrong_inheritance_expected, directory,
                     verify_files=True, require_engine=False)
        except ValueError:
            pass
        else:
            raise ValueError("wrong indexed-button inheritance passed unexpectedly")
        indexed_header_path.write_bytes(indexed_header_bytes)
        source_file.write_bytes(b"unreviewed source change")
        try:
            validate(data, directory, expected, directory, verify_files=True, require_engine=False)
        except ValueError:
            pass
        else:
            raise ValueError("mutated input file passed unexpectedly")
        source_file.write_bytes(b"synthetic pinned source")
        first_png = directory / f"{PHASES[0]}.png"
        first_png.write_bytes(sample + b"unreviewed image bytes")
        try:
            validate(data, directory, expected, directory, verify_files=True, require_engine=False)
        except ValueError:
            pass
        else:
            raise ValueError("mutated screenshot passed unexpectedly")
        return len(mutations) + 3


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path, help="absolute real runtime run directory")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    need(args.self_test or args.run is not None, "provide --run and/or --self-test")
    negatives = self_test() if args.self_test else 0
    if args.run:
        need(args.run.is_absolute(), "--run must be absolute")
        run = args.run.resolve(strict=True)
        need((ROOT / "Saved/RokUnityMigration/MainTaskGoToTownHallRuntimeAudit").resolve() in run.parents,
             "run not in unique audit output tree")
        data = json.loads((run / "manifest.json").read_text(encoding="utf-8"))
        validate(data, run, expected_inputs(), ROOT)
    print(json.dumps({"schema": "rok.main_task_go_town_hall.runtime_verify.v1",
                      "passed": True, "run": str(args.run) if args.run else None,
                      "negative_cases": negatives}, ensure_ascii=False))


if __name__ == "__main__":
    main()
