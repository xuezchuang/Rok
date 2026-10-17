#!/usr/bin/env python3
"""Strict proof of a real local-only Daily catalog Slate run. Self-tests are synthetic rejection only."""
from __future__ import annotations

import argparse
import copy
import hashlib
import io
import json
import re
from pathlib import Path

from PIL import Image

SCRIPT = Path(__file__).resolve()
ROOT = SCRIPT.parents[1] if SCRIPT.parent.name == "Scripts" else SCRIPT.parents[2]
STAGE = ROOT / "Saved/PendingDailyTaskReadonlyRuntimeAuditV2"
SCHEMA = "rok.daily_task_readonly_catalog.runtime_audit.v2"
SCOPE = "Actual HUD Tasks Daily 118-row config-only catalog, first two and last visible through real Slate; no account progress or claim"
PHASES = ("00_city_hud", "01_tasks_main", "02_daily_top", "03_daily_last_row", "04_hud_return")
CLICKS = (("open_tasks_from_hud", "TaskEntryButton", 0, "/Script/Rok.RokUiAnimatedButton"),
          ("open_daily_page_302", "TaskPage_2", 302, "/Script/Rok.RokUnityIndexedButton"),
          ("close_tasks_to_hud", "FeatureFrameClose", 1, "/Script/Rok.RokUnityIndexedButton"))
CAMERA = ("camera_x", "camera_y", "camera_z", "camera_pitch", "camera_yaw", "camera_roll",
          "camera_scale_x", "camera_scale_y", "camera_scale_z")
FALSE_CLAIMS = ("world_time_advanced_by_audit", "source_pixel_parity_claim", "unity_pixel_parity_accepted",
                "full_migration_accepted", "account_task_state_accepted", "claim_state_accepted")
AUDIT_FILES = ("Source/Rok/RokDailyTaskReadonlyCatalogAuditSubsystem.h",
               "Source/Rok/RokDailyTaskReadonlyCatalogAuditSubsystem.cpp",
               "Scripts/VerifyRokDailyTaskReadonlyCatalogRuntime.py")


def need(ok: bool, reason: str) -> None:
    if not ok:
        raise ValueError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def checked(root: Path, relative: str) -> Path:
    need(not Path(relative).is_absolute() and ".." not in Path(relative).parts, "input path escape")
    path = (root / relative).resolve(strict=True)
    need(root.resolve() in path.parents, "input outside project")
    return path


def expected_inputs() -> dict[str, str]:
    report = json.loads((STAGE / "release_report.json").read_text(encoding="utf-8"))
    need(report["schema"] == "rok.daily_task_readonly_catalog.audit_stage.v2", "stage schema")
    need(set(report["files"]) == set(AUDIT_FILES), "audit stage file set")
    cpp = (STAGE / "RokDailyTaskReadonlyCatalogAuditSubsystem.cpp").read_text(encoding="utf-8")
    pins = dict(re.findall(r'\{TEXT\("([^\"]+)"\), TEXT\("([0-9a-f]{64})"\)\}', cpp))
    need(pins == report["source_and_product_inputs_sha256"] and len(pins) >= 55,
         "native source pins differ from release report")
    need("if (!bSettled)" in cpp and "if (Child->IsVisible() &&" not in cpp
         and "ProcessMouseWheelOrGestureEvent" in cpp and "RequestScreenshot" in cpp,
         "native real input/hidden child/capture gate changed")
    return {**pins, **{p: report["files"][p]["sha256"] for p in AUDIT_FILES}}


def point(value: dict) -> tuple[float, float]:
    return float(value["x"]), float(value["y"])


def shown(widgets: dict, name: str) -> bool:
    row = widgets[name]
    return row["exists"] is True and row["visible"] is True and row["enabled"] is True


def check_claim(data: dict) -> None:
    need(data["schema"] == SCHEMA and data["complete"] is True and data["passed"] is True
         and data["failures"] == [] and data["attempts"] == [], "run incomplete or failed")
    need(data["scope"] == SCOPE and data["baseline_phase"] == PHASES[0]
         and data["completion_events"] == 0 and data["daily_count"] == 118
         and data["first_daily_id"] == 401001 and data["last_daily_id"] == 405024,
         "scope/daily source IDs/completion mismatch")
    for name in FALSE_CLAIMS:
        need(data[name] is False, f"overclaim: {name}")
    need(data["engine_major"] == 5 and data["engine_minor"] == 8
         and Path(data["project"]).resolve() == ROOT.resolve()
         and Path(data["engine_dir"]).resolve() == Path("D:/ue/UE_5.8/Engine").resolve()
         and data["world"] == "/Game/Maps/L_RokUnityGameAppCity"
         and isinstance(data["pid"], int) and data["pid"] > 0, "project/engine/map/PID identity")


def check_business(b: dict) -> None:
    need("UnityInitId_5" in b["hall_tags"] and "UnityBuildingType_1" in b["hall_tags"]
         and b["hall_actor_path"] in b["city_actor_paths"], "Hall source actor identity")
    need(b["active_upgrades"] == b["pending_completed_upgrades"] == b["active_construction"] == 0
         and b["construction_ticket_1_exists"] is False and b["completed_upgrade_events"] == 0,
         "build/construction queue not idle")
    need(b["profile_kind"] == "local_preview" and b["profile_has_role_id"] is False
         and b["profile_role_id"] == "" and b["profile_has_civilization"] is False
         and b["profile_civilization_id"] == 0, "account boundary")
    need(b["server_local_only"] is True and b["http_dispatch_attempts"] == 0
         and b["http_suppressed_requests"] == 0, "HTTP boundary")
    need(len(b["starter_heroes"]) == 5 and len(b["starter_troops"]) == 4
         and len(b["city_actor_paths"]) == len(set(b["city_actor_paths"])), "bootstrap snapshot")
    for name in ("wallet_food", "wallet_wood", "wallet_stone", "wallet_gold", "owned_queues",
                 "rate_food", "rate_wood", "rate_stone", "rate_gold", "task_id", "task_current",
                 "task_target", "task_chain", "trainable_troop_ids", "profile_local_hero_score",
                 "profile_local_troop_power", "profile_local_preview_score"):
        need(name in b, f"business field missing: {name}")


def source_rows() -> tuple[dict[int, dict], tuple[int, ...]]:
    catalog = json.loads(checked(ROOT, "Saved/RokUnityMigration/unity_gameplay_catalog.json").read_text(encoding="utf-8"))
    rows = catalog["daily_tasks"]
    ids = tuple(sorted(x["id"] for x in rows))
    need(len(rows) == len(set(ids)) == 118 and ids[0] == 401001 and ids[-1] == 405024,
         "source daily rows/IDs")
    return {r["id"]: r for r in rows}, ids


def projected(row: dict) -> dict[str, str]:
    task_id = row["id"]
    name = row["name"] if row["name"] and "\ufffd" not in row["name"] else "标题配置文本不可用"
    desc = row["desc"] if row["desc"] and "\ufffd" not in row["desc"] else "描述配置文本不可用"
    for key, value in (("p1", row["param1"]), ("p2", row["param2"]), ("p3", row["require"])):
        desc = desc.replace("{" + key + "}", format(value, ","))
    return {"Title": f"#{task_id} · {name}", "Description": desc,
            "Target": f"目标配置：类型 {row['type']} · 参数 {row['param1']}/{row['param2']} · 需求 {row['require']}",
            "Reward": f"奖励配置组 #{row['reward']} · 内容及可领取状态未验证"}


def check_settled(probe: dict) -> None:
    need(probe["ready"] is True and probe["structure_valid"] is True and probe["direct_children"] > 5
         and probe["unsettled_children"] == 0
         and 0 <= probe["not_visible_children"] <= probe["direct_children"]
         and probe["unsettled_not_visible_children"] == 0
         and probe["remaining_texture_compilations"] == 0 and probe["texture_count"] == 4
         and probe["texture_rhi_ready"] is True and probe["portrait_gate_applicable"] is False
         and probe["portrait_present"] is False, "Tasks animation/texture/RHI/portrait applicability")


def within(row: dict, scroll: dict) -> bool:
    rt, rb = point(row["absolute_top_left"]), point(row["absolute_bottom_right"])
    st, sb = point(scroll["absolute_top_left"]), point(scroll["absolute_bottom_right"])
    return st[1] - .5 <= rt[1] < rb[1] <= sb[1] + .5 and st[0] - .5 <= rt[0] < rb[0] <= sb[0] + .5


def check_state(s: dict, phase: str, baseline: dict, primary: int, selection: str,
                camera: tuple, pid: int, rows: dict[int, dict], ids: tuple[int, ...]) -> None:
    need(s["window_foreground"] is True and s["window_owner_pid"] == pid
         and s["foreground_owner_pid"] == pid and s["client_width"] == 1440
         and s["client_height"] == 810 and s["own_window_identity"] == s["foreground_window_identity"]
         and re.fullmatch(r"0x[0-9A-Fa-f]+", s["own_window_identity"]), f"window/PID/extent: {phase}")
    need(s["business"] == baseline and s["primary_press_count"] == primary
         and s["selected_actor"] == selection and s["modal_open"] is False,
         f"business/world input: {phase}")
    need(all(abs(s[k] - v) <= .001 for k, v in zip(CAMERA, camera)), f"camera moved: {phase}")
    w = s["widgets"]
    if phase in (PHASES[0], PHASES[4]):
        need(s["screen_open"] is False and shown(w, "TaskEntryButton"), "HUD not mounted")
        return
    mode = w["FeatureModeCanvas"]
    need(s["screen_open"] is True and shown(w, "TaskPage_2") and shown(w, "FeatureFrameClose")
         and mode["visible"] is True and abs(mode["opacity"] - 1) <= .01
         and all(abs(v - 1) <= .01 for v in point(mode["scale"])), "Tasks not settled")
    check_settled(s["task_ui_settled"])
    if phase == PHASES[1]:
        need(w["TaskDailyCatalogScroll"]["exists"] is False, "Daily catalog opened before tab 302")
        return
    scroll = w["TaskDailyCatalogScroll"]
    need(shown(w, "TaskDailyCatalogScroll") and shown(w, "TaskDailyCatalogHeader")
         and s["daily_count"] == 118 and s["scroll_end"] > 10000,
         "118-row Daily catalog/scroll absent")
    need(w["TaskDailyCatalogHeader"]["text"] ==
         "每日任务配置目录 · 非账户任务状态；进度、完成与领取状态未知",
         "read-only unknown-account-state notice absent")
    samples = s["sample_rows"]
    need(set(samples) == {str(ids[0]), str(ids[1]), str(ids[-1])}, "row sample IDs")
    for task_id in (ids[0], ids[1], ids[-1]):
        parts = samples[str(task_id)]
        expected = projected(rows[task_id])
        need(parts["Row"]["exists"] is True, f"row absent: {task_id}")
        for kind, value in expected.items():
            need(parts[kind]["exists"] is True and parts[kind]["text"] == value,
                 f"source title/description/target/reward mismatch: {task_id}/{kind}")
    if phase == PHASES[2]:
        need(s["scroll_offset"] < s["scroll_end"] - 100 and all(
            within(samples[str(task_id)]["Row"], scroll) for task_id in ids[:2]),
            "first two rows not actually visible at top")
    else:
        need(abs(s["scroll_offset"] - s["scroll_end"]) <= 1
             and within(samples[str(ids[-1])]["Row"], scroll), "last row not reached/visible")


def check_click(a: dict, spec: tuple[str, str, int, str], pid: int,
                baseline: dict, primary: int) -> None:
    label, target, action_id, cls = spec
    need(a["label"] == label and a["target"] == target and a["action_id"] == action_id
         and a["target_widget_class"] == cls
         and a["method"] == "ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent",
         f"click identity: {label}")
    need(all(a[k] is True for k in ("target_exists", "exact_button_in_path", "umg_path_present",
                                     "target_hovered", "own_game_window", "foreground", "down_handled", "up_handled"))
         and a["failure"] == "" and a["stable_frames"] >= 2, f"real Slate click: {label}")
    i, types, nodes = a["exact_button_path_index"], a["hit_path_types"], a["hit_path_nodes"]
    need(isinstance(i, int) and 0 <= i < len(types) and len(nodes) == len(types)
         and "SButton" in types[i] and any("SObjectWidget" in t for t in types)
         and nodes[i]["slate_identity"] == a["target_slate_identity"]
         and len({n["slate_identity"] for n in nodes}) == len(nodes), f"exact hit path: {label}")
    need(a["own_window_pid"] == pid and a["own_window_identity"] == a["foreground_window_identity"]
         and a["business_before"] == a["business_after"] == baseline
         and a["primary_before"] == a["primary_after"] == primary, f"click boundary: {label}")
    center, left, right, size = map(point, (a["absolute_center"], a["absolute_top_left"],
                                           a["absolute_bottom_right"], a["local_size"]))
    need(size[0] > 0 and size[1] > 0 and left[0] < center[0] < right[0]
         and left[1] < center[1] < right[1], f"click geometry: {label}")


def check_wheel(a: dict, pid: int, baseline: dict, primary: int) -> None:
    need(a["label"] == "scroll_daily_to_last_row" and a["target"] == "TaskDailyCatalogScroll"
         and a["target_widget_class"] == "/Script/UMG.ScrollBox" and a["action_id"] == 0
         and a["method"] == "ProcessMouseWheelOrGestureEvent" and a["wheel_delta"] == -2048
         and a["exact_scroll_in_path"] is True and a["own_game_window"] is True
         and a["foreground"] is True and a["wheel_handled"] is True
         and a["own_window_pid"] == pid and a["failure"] == ""
         and len(a["hit_path_types"]) > 0, "real foreground Slate wheel")
    need(a["business_before"] == a["business_after"] == baseline
         and a["primary_before"] == a["primary_after"] == primary
         and a["scroll_before"] >= 0 and a["scroll_after"] >= a["scroll_before"],
         "wheel changed business/world or moved backward")


def check_png(path: Path, digest: str, count: int) -> None:
    raw = path.read_bytes()
    need(len(raw) == count and hashlib.sha256(raw).hexdigest() == digest, "original PNG hash/bytes")
    with Image.open(io.BytesIO(raw)) as im:
        need(im.format == "PNG" and im.size == (1440, 810), "original PNG format/extent")
        im.verify()
    with Image.open(io.BytesIO(raw)) as im:
        im.load()
        need(im.convert("RGB").getcolors(maxcolors=256) is None, "blank/placeholder PNG")


def validate(data: dict, run: Path) -> None:
    check_claim(data)
    expected = expected_inputs()
    need(data["inputs_sha256"] == expected, "runtime input SHA set")
    for name, digest in expected.items():
        need(sha(checked(ROOT, name)) == digest, f"input bytes drift: {name}")
    header = checked(ROOT, "Source/Rok/RokUnityFeatureScreenWidget.h").read_text(encoding="utf-8-sig")
    need("friend class URokDailyTaskReadonlyCatalogAuditSubsystem;" in header, "indexed action read access missing")
    baseline = data["baseline_business"]
    check_business(baseline)
    rows, ids = source_rows()
    need(data["first_daily_id"] == ids[0] and data["last_daily_id"] == ids[-1], "source IDs drift")
    captures, actions = data["captures"], data["actions"]
    need(len(captures) == 5 and tuple(c["phase"] for c in captures) == PHASES
         and 4 <= len(actions) <= 15 and data["scroll_events"] == len(actions) - 3
         and tuple(a["label"] for a in actions) ==
         (CLICKS[0][0], CLICKS[1][0]) + ("scroll_daily_to_last_row",) * (len(actions) - 3) + (CLICKS[2][0],),
         "5 PNG/3 clicks/1-12 real wheels order")
    first = captures[0]["requested_state"]
    primary, selection = first["primary_press_count"], first["selected_actor"]
    camera = tuple(first[k] for k in CAMERA)
    previous = -1
    for index, cap in enumerate(captures):
        phase = PHASES[index]
        frames = (cap["requested_frame"], cap["processed_frame"], cap["next_frame"])
        need(previous < frames[0] <= frames[1] < frames[2], f"capture frames/time: {phase}")
        previous = frames[2]
        path = Path(cap["png"])
        need(path.is_absolute() and path.resolve(strict=True) == (run / f"{phase}.png").resolve(strict=True)
             and path.parent.resolve() == run.resolve(), f"PNG path/source: {phase}")
        need(cap["png_width"] == 1440 and cap["png_height"] == 810, "PNG declared extent")
        check_png(path, cap["png_sha256"], cap["png_bytes"])
        for key, frame in zip(("requested_state", "processed_state", "next_frame_state"), frames):
            s = cap[key]
            need(s["frame"] == frame, f"sample frame mismatch: {phase}/{key}")
            check_state(s, phase, baseline, primary, selection, camera, data["pid"], rows, ids)
    need(captures[2]["png_sha256"] != captures[3]["png_sha256"]
         and captures[1]["png_sha256"] != captures[2]["png_sha256"], "Daily top/end or tab screenshot repeated")
    for action, lower, upper in ((actions[0], captures[0], captures[1]),
                                 (actions[1], captures[1], captures[2]),
                                 (actions[-1], captures[3], captures[4])):
        need(lower["next_frame"] < action["frame"] < upper["requested_frame"], "click outside phases")
    for action in actions[2:-1]:
        need(captures[2]["next_frame"] < action["frame"] < captures[3]["requested_frame"],
             "wheel outside Daily interval")
    for action, spec in zip((actions[0], actions[1], actions[-1]), CLICKS):
        check_click(action, spec, data["pid"], baseline, primary)
    for action in actions[2:-1]:
        check_wheel(action, data["pid"], baseline, primary)
    check_state(data["final_state"], PHASES[-1], baseline, primary, selection,
                camera, data["pid"], rows, ids)
    need(data["final_state"]["frame"] >= captures[-1]["next_frame"], "final state before final PNG")


def self_test() -> int:
    # Synthetic fixtures exercise rejection gates. They are not actual runtime evidence.
    claim = {"schema": SCHEMA, "complete": True, "passed": True, "failures": [], "attempts": [],
             "scope": SCOPE, "baseline_phase": PHASES[0], "completion_events": 0,
             "daily_count": 118, "first_daily_id": 401001, "last_daily_id": 405024,
             "engine_major": 5, "engine_minor": 8, "project": str(ROOT), "engine_dir": "D:/ue/UE_5.8/Engine",
             "world": "/Game/Maps/L_RokUnityGameAppCity", "pid": 1234,
             **{name: False for name in FALSE_CLAIMS}}
    check_claim(claim)
    probe = {"ready": True, "structure_valid": True, "direct_children": 12,
             "unsettled_children": 0, "not_visible_children": 3,
             "unsettled_not_visible_children": 0, "remaining_texture_compilations": 0,
             "texture_count": 4, "texture_rhi_ready": True,
             "portrait_gate_applicable": False, "portrait_present": False}
    check_settled(probe)
    cases = 0
    for key, bad in (("complete", False), ("passed", False), ("failures", ["failed"]),
                     ("attempts", [{}]), ("daily_count", 117), ("first_daily_id", 0),
                     ("last_daily_id", 0), ("engine_minor", 7), ("pid", 0),
                     ("completion_events", 1), ("world", "/Game/Maps/Other"),
                     ("account_task_state_accepted", True), ("claim_state_accepted", True)):
        item = copy.deepcopy(claim); item[key] = bad
        try: check_claim(item)
        except (ValueError, KeyError, TypeError): cases += 1
        else: raise RuntimeError(f"negative claim accepted: {key}")
    for key, bad in (("ready", False), ("structure_valid", False), ("direct_children", 0),
                     ("unsettled_children", 1), ("unsettled_not_visible_children", 1),
                     ("remaining_texture_compilations", 1), ("texture_count", 3),
                     ("texture_rhi_ready", False), ("portrait_gate_applicable", True),
                     ("portrait_present", True)):
        item = copy.deepcopy(probe); item[key] = bad
        try: check_settled(item)
        except (ValueError, KeyError, TypeError): cases += 1
        else: raise RuntimeError(f"negative settled accepted: {key}")
    for value in ("", "bad", "../escape"):
        try: checked(ROOT, value)
        except (ValueError, FileNotFoundError, IsADirectoryError): cases += 1
        else: raise RuntimeError(f"bad path accepted: {value}")
    wheel = {"label": "scroll_daily_to_last_row", "target": "TaskDailyCatalogScroll",
             "target_widget_class": "/Script/UMG.ScrollBox", "action_id": 0,
             "method": "ProcessMouseWheelOrGestureEvent", "wheel_delta": -2048,
             "exact_scroll_in_path": True, "own_game_window": True, "foreground": True,
             "wheel_handled": True, "own_window_pid": 1234, "failure": "",
             "hit_path_types": ["SWindow", "SScrollBox"], "business_before": {"http_dispatch_attempts": 0},
             "business_after": {"http_dispatch_attempts": 0}, "primary_before": 3, "primary_after": 3,
             "scroll_before": 0, "scroll_after": 100}
    check_wheel(wheel, 1234, wheel["business_before"], 3)
    for key, bad in (("wheel_handled", False), ("exact_scroll_in_path", False),
                     ("foreground", False), ("own_window_pid", 99), ("scroll_after", -1),
                     ("business_after", {"http_dispatch_attempts": 1}), ("primary_after", 4),
                     ("target_widget_class", "/Script/UMG.Button")):
        item = copy.deepcopy(wheel); item[key] = bad
        try: check_wheel(item, 1234, wheel["business_before"], 3)
        except (ValueError, KeyError, TypeError): cases += 1
        else: raise RuntimeError(f"negative wheel accepted: {key}")
    scroll = {"absolute_top_left": {"x": 100, "y": 100},
              "absolute_bottom_right": {"x": 1000, "y": 700}}
    row = {"absolute_top_left": {"x": 110, "y": 120},
           "absolute_bottom_right": {"x": 900, "y": 240}}
    need(within(row, scroll), "in-view geometry fixture")
    for bad in (700, 701, 1000):
        item = copy.deepcopy(row); item["absolute_top_left"]["y"] = bad
        try: need(within(item, scroll), "offscreen row")
        except ValueError: cases += 1
        else: raise RuntimeError(f"offscreen row accepted: {bad}")
    rows, ids = source_rows()
    need(len(ids) == 118 and projected(rows[ids[0]])["Title"].startswith("#401001 · "),
         "source projection self-test")
    expected_inputs()
    return cases


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()
    need(args.self_test != bool(args.manifest), "choose exactly one of --self-test or --manifest")
    if args.self_test:
        print(json.dumps({"synthetic_negative_cases": self_test(), "actual_runtime_verified": False}))
        return
    run = args.manifest.resolve(strict=True).parent
    data = json.loads(args.manifest.read_text(encoding="utf-8"))
    validate(data, run)
    print(json.dumps({"actual_runtime_verified": True, "png_count": 5,
                      "slate_actions": len(data["actions"]), "daily_rows": 118}))


if __name__ == "__main__":
    main()
