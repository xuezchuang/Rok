#!/usr/bin/env python3
"""Strict actual VIP HelpTip Slate receipt; --self-test is synthetic rejection only."""
from __future__ import annotations

import argparse
import copy
import hashlib
import io
import json
import math
import re
from pathlib import Path
from rok_data_paths import resolve

from PIL import Image

SCRIPT = Path(__file__).resolve()
ROOT = SCRIPT.parents[1] if SCRIPT.parent.name == "Scripts" else SCRIPT.parents[2]
STAGE = ROOT / "Saved/PendingVipDailyPointHelpTipRuntimeAuditV6"
SCHEMA = "rok.vip_daily_point_help_tip.runtime_audit.v3"
SCOPE = ("Actual anonymous HUD VIP daily-point help 800032 real Slate; disabled "
         "740/741/743/744 outside dismiss only, no purchase or claim")
PHASES = ("00_city_hud", "01_vip", "02_help_open", "03_inside_stays",
          "04_right_wheel_stays", "05_outside_740_closed", "06_reopened_740",
          "07_outside_741_closed", "08_reopened_741", "09_outside_743_closed",
          "10_reopened_743", "11_outside_744_closed", "12_reopened_744",
          "13_help_closed", "14_hud_return")
TIP_PHASES = {2, 3, 4, 6, 8, 10, 12}
CAMERA = ("camera_x", "camera_y", "camera_z", "camera_pitch", "camera_yaw",
          "camera_roll", "camera_scale_x", "camera_scale_y", "camera_scale_z")
FALSE_CLAIMS = ("world_time_advanced_by_audit", "source_pixel_parity_claim",
                "unity_pixel_parity_accepted", "full_migration_accepted",
                "account_task_state_accepted", "claim_state_accepted",
                "logged_in_enabled_purchase_claim_covered")
AUDIT_FILES = ("Source/Rok/RokVipDailyPointHelpTipAuditSubsystem.h",
               "Source/Rok/RokVipDailyPointHelpTipAuditSubsystem.cpp",
               "Scripts/VerifyRokVipDailyPointHelpTipRuntime.py")
ACTION_SPECS = (
    ("click", "open_vip_from_hud", "VipHitButton", 0, "/Script/UMG.Button"),
    ("click", "open_help_742", "VipHelp", 742, "/Script/Rok.RokUnityIndexedButton"),
    ("click", "inside_body_745_stays", "VipDailyPointHelpTipBodyHit", 745, "/Script/Rok.RokUnityIndexedButton"),
    ("right", "right_inside_tip_stays", "VipDailyPointHelpTipBodyHit", None, "/Script/Rok.RokUnityIndexedButton"),
    ("wheel", "wheel_vip_benefits_tip_stays", "VipBenefitsScroll", None, "/Script/UMG.ScrollBox"),
    ("outside", "outside_740_consumed", "VipDailyPointChest", 740, "/Script/Rok.RokUnityIndexedButton"),
    ("click", "reopen_after_740", "VipHelp", 742, "/Script/Rok.RokUnityIndexedButton"),
    ("outside", "outside_741_consumed", "VipAddPoints", 741, "/Script/Rok.RokUnityIndexedButton"),
    ("click", "reopen_after_741", "VipHelp", 742, "/Script/Rok.RokUnityIndexedButton"),
    ("outside", "outside_743_consumed", "VipClaimFreeChest", 743, "/Script/Rok.RokUnityIndexedButton"),
    ("click", "reopen_after_743", "VipHelp", 742, "/Script/Rok.RokUnityIndexedButton"),
    ("outside", "outside_744_consumed", "VipBuySpecialChest", 744, "/Script/Rok.RokUnityIndexedButton"),
    ("click", "reopen_after_744", "VipHelp", 742, "/Script/Rok.RokUnityIndexedButton"),
    ("outside", "close_help_742", "VipHelp", 742, "/Script/Rok.RokUnityIndexedButton"),
    ("click", "close_vip_to_hud", "FeatureFrameClose", 1, "/Script/Rok.RokUnityIndexedButton"),
)
# Every action must occur between these completed captures, in this order.
ACTION_INTERVALS = ((0, 1), (1, 2), (2, 3), (3, 4), (3, 4),
                    (4, 5), (5, 6), (6, 7), (7, 8), (8, 9),
                    (9, 10), (10, 11), (11, 12), (12, 13), (13, 14))


def need(ok: bool, reason: str) -> None:
    if not ok:
        raise ValueError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def checked(root: Path, relative: str) -> Path:
    need(not Path(relative).is_absolute() and ".." not in Path(relative).parts, "path escape")
    path = resolve(relative).resolve(strict=True)
    need(root.resolve() in path.parents, "path outside project")
    return path


def expected_inputs() -> dict[str, str]:
    report = json.loads((STAGE / "release_report.json").read_text(encoding="utf-8"))
    need(report["schema"] == "rok.vip_daily_point_help_tip.audit_stage.v6"
         and set(report["files"]) == set(AUDIT_FILES), "frozen stage schema/files")
    cpp = (STAGE / "RokVipDailyPointHelpTipAuditSubsystem.cpp").read_text(encoding="utf-8")
    pairs = re.findall(r'\{TEXT\("([^\"]+)"\), TEXT\("([0-9a-f]{64})"\)\}', cpp)
    pins = dict(pairs)
    need(len(pins) == len(pairs) >= 65 and pins == report["source_and_product_inputs_sha256"],
         "native exact input pins")
    need("if (!bSettled)" in cpp and "if (Child->IsVisible() &&" not in cpp
         and "ProcessMouseButtonDownEvent" in cpp and "ProcessMouseWheelOrGestureEvent" in cpp
         and "RequestScreenshot" in cpp and "GetPendingActionIdForAudit" in cpp,
         "real input/hidden-child/capture gate drift")
    return {**pins, **{p: report["files"][p]["sha256"] for p in AUDIT_FILES}}


def source_help() -> str:
    rows = json.loads(checked(ROOT, "Saved/RokUnityMigration/ConfigTables/Language.json")
                      .read_text(encoding="utf-8"))["rows"]
    found = [r["cn"] for r in rows if r["id"] == 800032]
    need(len(found) == 1 and isinstance(found[0], str) and found[0]
         and "\ufffd" not in found[0], "source locale 800032")
    return found[0]


def point(value: dict) -> tuple[float, float]:
    return float(value["x"]), float(value["y"])


def client_rect(value: dict, pid: int, identity: str,
                reference: tuple[float, float, float, float] | None = None
                ) -> tuple[float, float, float, float]:
    left, top = point(value["client_absolute_top_left"])
    right, bottom = point(value["client_absolute_bottom_right"])
    rect = (left, top, right, bottom)
    need(value["client_absolute_valid"] is True and value["client_owner_pid"] == pid
         and value["client_window_identity"] == identity
         and re.fullmatch(r"0x[0-9A-Fa-f]+", identity) is not None
         and all(math.isfinite(v) and v.is_integer() for v in rect)
         and right - left == 1440 and bottom - top == 810
         and (reference is None or rect == reference), "owned absolute client rectangle")
    return rect


def check_tip_center(state: dict) -> None:
    w = state["widgets"]
    center = w["VipDailyPointHelpTipBackground_Center"]
    frame = w["VipDailyPointHelpTipBackground"]
    width, height = point(center["local_size"])
    frame_width, frame_height = point(frame["local_size"])
    uv_min, uv_max = point(center["uv_min"]), point(center["uv_max"])
    need(state["tip_background_center_geometry_uv_rhi_alpha_valid"] is True
         and state["tip_background_center_common_atlas"] is True
         and width >= 1 and height >= 1 and frame_width > 42 and frame_height > 47
         and center["uv_valid"] is True
         and all(abs(a - b) <= .00001 for a, b in zip(uv_min, (795 / 2048, 699 / 1024)))
         and all(abs(a - b) <= .00001 for a, b in zip(uv_max, (825 / 2048, 724 / 1024)))
         and center["image_alpha"] >= .995 and center["brush_alpha"] >= .995
         and center["opacity"] >= .995,
         "VIP HelpTip source 30x25 center geometry/UV/RHI/untinted alpha")


def shown(widgets: dict, name: str, enabled: bool | None = True) -> bool:
    row = widgets[name]
    return row["exists"] is True and row["visible"] is True and (
        enabled is None or row["enabled"] is enabled)


def check_business(b: dict) -> None:
    need("UnityInitId_5" in b["hall_tags"] and "UnityBuildingType_1" in b["hall_tags"]
         and b["hall_actor_path"] in b["city_actor_paths"], "Hall source actor identity")
    need(b["active_upgrades"] == b["pending_completed_upgrades"] == b["active_construction"] == 0
         and b["construction_ticket_1_exists"] is False and b["completed_upgrade_events"] == 0,
         "build/construction queue not idle")
    need(b["profile_kind"] == "local_preview" and b["profile_has_role_id"] is False
         and b["profile_role_id"] == "" and b["profile_has_civilization"] is False
         and b["profile_civilization_id"] == 0, "anonymous account boundary")
    need(b["server_local_only"] is True and b["http_dispatch_attempts"] == 0
         and b["http_suppressed_requests"] == 0, "HTTP boundary")
    need(len(b["starter_heroes"]) == 5 and len(b["starter_troops"]) == 4
         and len(b["city_actor_paths"]) == len(set(b["city_actor_paths"])), "bootstrap snapshot")
    for name in ("wallet_food", "wallet_wood", "wallet_stone", "wallet_gold", "owned_queues",
                 "rate_food", "rate_wood", "rate_stone", "rate_gold", "task_id", "task_current",
                 "task_target", "task_chain", "trainable_troop_ids", "profile_local_hero_score",
                 "profile_local_troop_power", "profile_local_preview_score",
                 "profile_has_vip_level", "profile_vip_level"):
        need(name in b, f"business field absent: {name}")


def check_claim(d: dict, help_text: str) -> None:
    need(d["schema"] == SCHEMA and d["complete"] is True and d["passed"] is True
         and d["failures"] == [] and d["attempts"] == []
         and d["scope"] == SCOPE and d["baseline_phase"] == PHASES[0], "run failed/incomplete")
    need(d["locale_id"] == 800032 and d["source_help_text"] == help_text
         and d["outside_dismissals"] == 4 and d["completion_events"] == 0,
         "source help/outside count")
    need(all(d[name] is False for name in FALSE_CLAIMS), "accepted forbidden claim")
    need(d["engine_major"] == 5 and d["engine_minor"] == 8
         and Path(d["project"]).resolve() == ROOT.resolve()
         and Path(d["engine_dir"]).resolve() == Path("D:/ue/UE_5.8/Engine").resolve()
         and d["world"] == "/Game/Maps/L_RokUnityGameAppCity"
         and isinstance(d["pid"], int) and d["pid"] > 0,
         "project/engine/map/PID identity")


def check_settled(p: dict) -> None:
    need(p["ready"] is True and p["structure_valid"] is True and p["direct_children"] > 5
         and p["unsettled_children"] == 0
         and 0 <= p["not_visible_children"] <= p["direct_children"]
         and p["unsettled_not_visible_children"] == 0
         and p["remaining_texture_compilations"] == 0 and p["texture_count"] == 4
         and p["texture_rhi_ready"] is True and p["portrait_gate_applicable"] is False
         and p["portrait_present"] is False, "VIP animation/texture RHI/hidden children")


def check_state(s: dict, index: int, baseline: dict, first: dict, pid: int, help_text: str) -> None:
    phase = PHASES[index]
    need(s["window_foreground"] is True and s["window_owner_pid"] == pid
         and s["foreground_owner_pid"] == pid and s["client_width"] == 1440
         and s["client_height"] == 810 and s["own_window_identity"] == s["foreground_window_identity"]
         and re.fullmatch(r"0x[0-9A-Fa-f]+", s["own_window_identity"]), f"window: {phase}")
    reference = client_rect(first, pid, first["own_window_identity"])
    client_rect(s, pid, first["own_window_identity"], reference)
    need(s["business"] == baseline and s["primary_press_count"] == first["primary_press_count"]
         and s["selected_actor"] == first["selected_actor"] and s["modal_open"] is False
         and all(abs(s[k] - first[k]) <= .001 for k in CAMERA), f"business/camera: {phase}")
    need(s["pending_action_id"] == -1 and s["source_help_text"] == help_text,
         f"pending action/source text: {phase}")
    need(s["hud_textures_ready"] is True, f"HUD atlas RHI/compiler: {phase}")
    need(s["vip_test_fixture_state"] is False, f"fixture account route: {phase}")
    w = s["widgets"]
    if index in (0, 14):
        need(s["screen_open"] is False and s["help_tip_visible"] is False
             and shown(w, "VipHitButton"), f"HUD state: {phase}")
        return
    need(s["screen_open"] is True and shown(w, "VipHelp")
         and shown(w, "FeatureFrameClose") and shown(w, "VipBenefitsScroll", None)
         and w["FeatureModeCanvas"]["visible"] is True
         and abs(w["FeatureModeCanvas"]["opacity"] - 1) <= .01
         and all(abs(v - 1) <= .01 for v in point(w["FeatureModeCanvas"]["scale"])),
         f"VIP screen state: {phase}")
    check_settled(s["vip_ui_settled"])
    for name in ("VipDailyPointChest", "VipAddPoints", "VipClaimFreeChest", "VipBuySpecialChest"):
        need(shown(w, name, False), f"anonymous {name} must stay disabled: {phase}")
    need(s["help_tip_visible"] is (index in TIP_PHASES), f"tip state: {phase}")
    if index in TIP_PHASES:
        need(all(shown(w, name, None) for name in
                 ("VipDailyPointHelpTipRoot", "VipDailyPointHelpTipBodyHit",
                  "VipDailyPointHelpTipBackground", "VipDailyPointHelpTipBackground_Center",
                  "VipDailyPointHelpTipText", "VipDailyPointHelpTipArrow")),
             f"tip widget tree: {phase}")
        need(w["VipDailyPointHelpTipText"]["text"] == help_text
             and s["tip_background_center_common_atlas"] is True
             and s["tip_font_face"] == "Regular"
             and abs(s["tip_font_size"] - 13.5) <= .1
             and all(0 <= s[f"tip_text_color_{channel}"] < .2 for channel in "rgb"),
             f"source copy/light-frame atlas/dark regular font: {phase}")
        check_tip_center(s)
    else:
        need(s["help_tip_visible"] is False, f"tip remained open: {phase}")


def check_action(a: dict, spec: tuple, interval: tuple[int, int], captures: list,
                 pid: int, baseline: dict, primary: int, first: dict) -> None:
    kind, label, target, action_id, cls = spec
    low, high = interval
    need(captures[low]["next_frame"] < a["frame"] < captures[high]["requested_frame"],
         f"action capture order: {label}")
    need(a["label"] == label and a["target"] == target
         and a["target_widget_class"] == cls and a["failure"] == ""
         and a["own_game_window"] is True and a["foreground"] is True
         and a["own_window_pid"] == pid
         and a["business_before"] == a["business_after"] == baseline
         and a["primary_before"] == a["primary_after"] == primary,
         f"action identity/boundary: {label}")
    need(len(a["hit_path_types"]) > 0, f"Slate hit path absent: {label}")
    reference = client_rect(first, pid, first["own_window_identity"])
    left, top, right, bottom = client_rect(a, pid, first["own_window_identity"], reference)
    center = point(a["absolute_center"])
    need(all(math.isfinite(v) for v in center)
         and left <= center[0] < right and top <= center[1] < bottom,
         f"action geometry: {label}")
    if kind == "click":
        need(a["action_id"] == action_id and a["target_exists"] is True
             and a["exact_button_in_path"] is True and a["umg_path_present"] is True
             and a["target_hovered"] is True and a["down_handled"] is True
             and a["up_handled"] is True and a["stable_frames"] >= 2
             and a["method"] == "ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent",
             f"real exact Slate button: {label}")
        i, types, nodes = a["exact_button_path_index"], a["hit_path_types"], a["hit_path_nodes"]
        need(0 <= i < len(types) == len(nodes) and "SButton" in types[i]
             and nodes[i]["slate_identity"] == a["target_slate_identity"]
             and len({node["slate_identity"] for node in nodes}) == len(nodes),
             f"exact Slate node identity: {label}")
    elif kind == "outside":
        need(a["action_id"] == action_id and a["screen_in_hit_path"] is True
             and a["down_handled"] is True and a["tip_closed_after"] is True
             and a["pending_action_before"] == a["pending_action_after"] == -1
             and a["modal_open_after"] is False and a["target_enabled"] is (action_id == 742)
             and a["method"] == "ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent"
             and len(a["hit_path_types"]) == len(a["hit_path_nodes"]),
             f"outside preview not consumed or underlying action risk: {label}")
        left, right = point(a["target_top_left"]), point(a["target_bottom_right"])
        need(left[0] < center[0] < right[0] and left[1] < center[1] < right[1],
             f"outside pointer not over target geometry: {label}")
    else:
        need(a["screen_in_hit_path"] is True and a["target_in_hit_path"] is True
             and a["tip_still_open"] is True
             and a["pending_action_after"] == -1,
             f"right/wheel dismissed or scheduled action: {label}")
        if kind == "right":
            need(a["method"] == "ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent",
                 "right event source")
        else:
            need(a["method"] == "ProcessMouseWheelOrGestureEvent" and a["wheel_handled"] is True
                 and a["scroll_before"] >= 0 and a["scroll_after"] >= 0,
                 "wheel event source/scroll state")


def check_png(path: Path, digest: str, count: int) -> None:
    raw = path.read_bytes()
    need(len(raw) == count and hashlib.sha256(raw).hexdigest() == digest,
         "original PNG hash/byte count")
    with Image.open(io.BytesIO(raw)) as image:
        need(image.format == "PNG" and image.size == (1440, 810), "PNG format/extent")
        image.verify()
    with Image.open(io.BytesIO(raw)) as image:
        image.load()
        need(image.convert("RGB").getcolors(maxcolors=256) is None,
             "blank/placeholder PNG")


def validate(data: dict, run: Path) -> None:
    help_text = source_help()
    check_claim(data, help_text)
    expected = expected_inputs()
    need(data["inputs_sha256"] == expected, "runtime exact input set")
    for name, digest in expected.items():
        need(sha(checked(ROOT, name)) == digest, f"input bytes drift: {name}")
    header = checked(ROOT, "Source/Rok/RokUnityFeatureScreenWidget.h").read_text(encoding="utf-8-sig")
    need("friend class URokCommanderSkillReadonlyDetailAuditSubsystem;" in header
         and "friend class URokDailyTaskReadonlyCatalogAuditSubsystem;" in header
         and header.count("friend class URokVipDailyPointHelpTipAuditSubsystem;") == 2,
         "minimal friend access/older audit friend preservation")
    baseline = data["baseline_business"]
    check_business(baseline)
    captures, actions = data["captures"], data["actions"]
    need(len(captures) == len(PHASES) == 15 and len(actions) == len(ACTION_SPECS) == 15
         and tuple(c["phase"] for c in captures) == PHASES
         and tuple(a["label"] for a in actions) == tuple(s[1] for s in ACTION_SPECS),
         "15 real captures/actions exact order")
    first = captures[0]["requested_state"]
    previous = -1
    for index, cap in enumerate(captures):
        frames = (cap["requested_frame"], cap["processed_frame"], cap["next_frame"])
        need(previous < frames[0] <= frames[1] < frames[2],
             f"capture frame/time: {PHASES[index]}")
        previous = frames[2]
        path = Path(cap["png"])
        need(path.is_absolute() and path.resolve(strict=True) ==
             (run / f"{PHASES[index]}.png").resolve(strict=True)
             and path.parent.resolve() == run.resolve(), "PNG actual run path")
        need(cap["png_width"] == 1440 and cap["png_height"] == 810,
             "PNG declared extent")
        check_png(path, cap["png_sha256"], cap["png_bytes"])
        for field, frame in zip(("requested_state", "processed_state", "next_frame_state"), frames):
            state = cap[field]
            need(state["frame"] == frame, "three actual samples/frame mismatch")
            check_state(state, index, baseline, first, data["pid"], help_text)
    need(len({cap["png_sha256"] for cap in captures}) >= 5
         and captures[1]["png_sha256"] != captures[2]["png_sha256"]
         and captures[2]["png_sha256"] != captures[5]["png_sha256"],
         "VIP/tip/closed PNGs indistinguishable")
    for action, spec, interval in zip(actions, ACTION_SPECS, ACTION_INTERVALS):
        check_action(action, spec, interval, captures, data["pid"], baseline,
                     first["primary_press_count"], first)
    check_state(data["final_state"], 14, baseline, first, data["pid"], help_text)
    need(data["final_state"]["frame"] >= captures[-1]["next_frame"], "final before HUD capture")


def self_test() -> int:
    help_text = source_help()
    claim = {"schema": SCHEMA, "complete": True, "passed": True, "failures": [],
             "attempts": [], "scope": SCOPE, "baseline_phase": PHASES[0],
             "locale_id": 800032, "source_help_text": help_text,
             "outside_dismissals": 4, "completion_events": 0, "engine_major": 5,
             "engine_minor": 8, "project": str(ROOT), "engine_dir": "D:/ue/UE_5.8/Engine",
             "world": "/Game/Maps/L_RokUnityGameAppCity", "pid": 1234,
             **{name: False for name in FALSE_CLAIMS}}
    check_claim(claim, help_text)
    settled = {"ready": True, "structure_valid": True, "direct_children": 12,
               "unsettled_children": 0, "not_visible_children": 3,
               "unsettled_not_visible_children": 0, "remaining_texture_compilations": 0,
               "texture_count": 4, "texture_rhi_ready": True,
               "portrait_gate_applicable": False, "portrait_present": False}
    check_settled(settled)
    cases = 0
    tip = {"tip_background_center_geometry_uv_rhi_alpha_valid": True,
           "tip_background_center_common_atlas": True,
           "widgets": {"VipDailyPointHelpTipBackground": {"local_size": {"x": 130, "y": 80}},
                       "VipDailyPointHelpTipBackground_Center": {
                           "local_size": {"x": 88, "y": 33},
                           "uv_valid": True,
                           "uv_min": {"x": 795 / 2048, "y": 699 / 1024},
                           "uv_max": {"x": 825 / 2048, "y": 724 / 1024},
                           "image_alpha": 1, "brush_alpha": 1, "opacity": 1}}}
    check_tip_center(tip)
    for path, bad in (("tip_background_center_geometry_uv_rhi_alpha_valid", False),
                      ("tip_background_center_common_atlas", False),
                      ("center_width", 0), ("center_height", 0),
                      ("frame_height", 47), ("uv_valid", False),
                      ("uv_min_x", 0), ("uv_max_y", 1),
                      ("image_alpha", .5), ("brush_alpha", .5), ("opacity", .5)):
        row = copy.deepcopy(tip)
        center = row["widgets"]["VipDailyPointHelpTipBackground_Center"]
        if path == "center_width": center["local_size"]["x"] = bad
        elif path == "center_height": center["local_size"]["y"] = bad
        elif path == "frame_height": row["widgets"]["VipDailyPointHelpTipBackground"]["local_size"]["y"] = bad
        elif path == "uv_min_x": center["uv_min"]["x"] = bad
        elif path == "uv_max_y": center["uv_max"]["y"] = bad
        elif path in ("uv_valid", "image_alpha", "brush_alpha", "opacity"): center[path] = bad
        else: row[path] = bad
        try: check_tip_center(row)
        except (ValueError, KeyError, TypeError): cases += 1
        else: raise RuntimeError(f"bad tip center accepted: {path}")
    for name, bad in (("schema", "rok.vip_daily_point_help_tip.runtime_audit.v2"),
                      ("complete", False), ("passed", False), ("failures", ["x"]),
                      ("attempts", [{}]), ("locale_id", 0), ("source_help_text", "bad"),
                      ("outside_dismissals", 3), ("completion_events", 1),
                      ("engine_minor", 7), ("pid", 0), ("world", "/Game/Maps/Other"),
                      ("logged_in_enabled_purchase_claim_covered", True),
                      ("claim_state_accepted", True), ("full_migration_accepted", True)):
        row = copy.deepcopy(claim); row[name] = bad
        try: check_claim(row, help_text)
        except (ValueError, KeyError, TypeError): cases += 1
        else: raise RuntimeError(f"bad claim accepted: {name}")
    for name, bad in (("ready", False), ("structure_valid", False),
                      ("direct_children", 0), ("unsettled_children", 1),
                      ("unsettled_not_visible_children", 1),
                      ("remaining_texture_compilations", 1), ("texture_count", 3),
                      ("texture_rhi_ready", False), ("portrait_gate_applicable", True),
                      ("portrait_present", True)):
        row = copy.deepcopy(settled); row[name] = bad
        try: check_settled(row)
        except (ValueError, KeyError, TypeError): cases += 1
        else: raise RuntimeError(f"bad settled accepted: {name}")
    outside = {"label": "outside_740_consumed", "target": "VipDailyPointChest",
               "target_widget_class": "/Script/Rok.RokUnityIndexedButton", "failure": "",
               "own_game_window": True, "foreground": True, "own_window_pid": 1234,
               "business_before": {}, "business_after": {}, "primary_before": 1,
               "primary_after": 1, "hit_path_types": ["SWindow", "SObjectWidget"],
               "absolute_center": {"x": 624, "y": 200}, "action_id": 740,
               "client_absolute_valid": True, "client_owner_pid": 1234,
               "client_window_identity": "0x1234",
               "client_absolute_top_left": {"x": 424, "y": 50},
               "client_absolute_bottom_right": {"x": 1864, "y": 860},
               "screen_in_hit_path": True, "down_handled": True,
               "tip_closed_after": True, "pending_action_before": -1,
               "pending_action_after": -1, "modal_open_after": False,
               "target_enabled": False,
               "method": "ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent",
               "hit_path_nodes": [{}, {}],
               "target_top_left": {"x": 604, "y": 180},
               "target_bottom_right": {"x": 644, "y": 220}, "frame": 11}
    first_window = {"own_window_identity": "0x1234",
                    "client_absolute_valid": True, "client_owner_pid": 1234,
                    "client_window_identity": "0x1234",
                    "client_absolute_top_left": {"x": 424, "y": 50},
                    "client_absolute_bottom_right": {"x": 1864, "y": 860}}
    caps = [{"next_frame": 10, "requested_frame": 9},
            {"next_frame": 13, "requested_frame": 12}]
    check_action(outside, ACTION_SPECS[5], (0, 1), caps, 1234, {}, 1, first_window)
    for name, bad in (("target_enabled", True), ("down_handled", False),
                      ("tip_closed_after", False), ("screen_in_hit_path", False),
                      ("pending_action_after", 740), ("modal_open_after", True),
                      ("business_after", {"wallet_food": 1}), ("own_window_pid", 999),
                      ("action_id", 741), ("absolute_center", {"x": 999, "y": 999})):
        row = copy.deepcopy(outside); row[name] = bad
        try: check_action(row, ACTION_SPECS[5], (0, 1), caps, 1234, {}, 1, first_window)
        except (ValueError, KeyError, TypeError): cases += 1
        else: raise RuntimeError(f"bad outside action accepted: {name}")
    for name, change in (
        ("client_valid_false", lambda r: r.update(client_absolute_valid=False)),
        ("client_foreign_owner", lambda r: r.update(client_owner_pid=999)),
        ("client_foreign_window", lambda r: r.update(client_window_identity="0x9999")),
        ("client_rect_missing", lambda r: r.pop("client_absolute_top_left")),
        ("client_wrong_size", lambda r: r["client_absolute_bottom_right"].update(x=1863)),
        ("client_nan", lambda r: r["client_absolute_top_left"].update(x=float("nan"))),
        ("client_inconsistent", lambda r: (
            r["client_absolute_top_left"].update(x=425),
            r["client_absolute_bottom_right"].update(x=1865))),
        ("center_left_of_client", lambda r: r["absolute_center"].update(x=423)),
        ("center_on_right_edge", lambda r: r["absolute_center"].update(x=1864)),
    ):
        row = copy.deepcopy(outside); change(row)
        try: check_action(row, ACTION_SPECS[5], (0, 1), caps, 1234, {}, 1, first_window)
        except (ValueError, KeyError, TypeError): cases += 1
        else: raise RuntimeError(f"bad client rectangle accepted: {name}")
    for value in ("", "../escape", "/absolute"):
        try: checked(ROOT, value)
        except (ValueError, FileNotFoundError, IsADirectoryError): cases += 1
        else: raise RuntimeError(f"bad source path accepted: {value}")
    expected_inputs()
    need(cases >= 58, "meaningful negative count")
    return cases


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()
    need(args.self_test != bool(args.manifest), "choose self-test or actual manifest")
    if args.self_test:
        print(json.dumps({"synthetic_negative_cases": self_test(),
                          "actual_runtime_verified": False}))
        return
    manifest = args.manifest.resolve(strict=True)
    run = manifest.parent
    need(manifest.name == "manifest.json" and run.parent.resolve() ==
         (STAGE / "RuntimeAudit").resolve(), "foreign actual run path")
    data = json.loads(manifest.read_text(encoding="utf-8"))
    validate(data, run)
    print(json.dumps({"actual_runtime_verified": True, "png_count": len(PHASES),
                      "slate_actions": len(ACTION_SPECS), "outside_dismissals": 4}))


if __name__ == "__main__":
    main()
