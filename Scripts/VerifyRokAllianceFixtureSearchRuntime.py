"""Strict original-PNG/native-Slate verifier for one Alliance search audit run."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import struct
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
STAGE = Path(__file__).resolve().parents[1]
SCHEMA = "rok.alliance_fixture_search.runtime_audit.v3"
FIXTURE_PHASES = (
    "00_city_hud", "01_alliance_welcome", "02_join_default_five",
    "03_ROK_original_1", "04_sNoW_original_0", "05_RO_minimum",
    "06_ZZZ_no_result", "07_empty_restores_five", "08_ROK_before_row",
    "09_row_1_preserves_draft", "10_welcome_reset", "11_join_reset_five",
    "12_hud_closed", "13_welcome_reopened", "14_join_reopened_five", "15_final_hud",
)
PRODUCTION_PHASES = (
    "00_city_hud", "01_alliance_welcome", "02_join_empty",
    "03_search_server_boundary", "04_welcome_return", "05_final_hud",
)
FIXTURE_INTERVALS = (
    (("open_alliance_from_hud", "AllianceFeatureButton", 0),),
    (("open_join_list", "AllianceWelcomeJoin", 600),),
    (("type_ROK", "edit", "ROK"), ("search_ROK", "AllianceSearch", 624)),
    (("type_sNoW", "edit", "sNoW"), ("search_sNoW", "AllianceSearch", 624)),
    (("type_RO", "edit", "RO"), ("search_RO_short", "AllianceSearch", 624)),
    (("type_ZZZ", "edit", "ZZZ"), ("search_ZZZ", "AllianceSearch", 624)),
    (("clear_search", "edit", ""), ("search_empty_restore", "AllianceSearch", 624)),
    (("type_ROK_again", "edit", "ROK"), ("search_ROK_again", "AllianceSearch", 624)),
    (("select_visible_original_1", "AllianceJoinRowHit_1", 611),),
    (("back_to_welcome", "AllianceJoinBack", 621),),
    (("reenter_join_reset", "AllianceWelcomeJoin", 600),),
    (("close_to_hud", "FeatureFrameClose", 1),),
    (("reopen_alliance_from_hud", "AllianceFeatureButton", 0),),
    (("reopen_join_reset", "AllianceWelcomeJoin", 600),),
    (("final_close_to_hud", "FeatureFrameClose", 1),),
)
PRODUCTION_INTERVALS = (
    (("open_alliance_from_hud", "AllianceFeatureButton", 0),),
    (("open_join_list", "AllianceWelcomeJoin", 600),),
    (("type_ROK_production", "edit", "ROK"),
     ("search_production_boundary", "AllianceSearch", 624)),
    (("back_to_welcome", "AllianceJoinBack", 621),),
    (("close_to_hud", "FeatureFrameClose", 1),),
)
PRODUCT_CPP = "b09141b49e0b76301b4c042b5ca0c9deca7cc578653994d2946dbc26f18e25c8"
PRODUCT_READY = "ca2eaa929c1db1acaba7378a7b36c1ff6a9378e7a2a3c6cb815524efe2fe1da7"
PRODUCT_RELEASE = "8e1cbd52bb4f9ee5be59a6576054005f37f62b77dd37f4c7aa827f5dc4e18290"
PRODUCT_H = "b5de200485d4d191c90b8203fffd305535b1b6ad5b6e7b9970ba385b057f98bd"
REVIEW = "06639c21e8ff8f76d1830bbfc969f4cdfebd33712104e4e08ac1e86433488c5d"
AUDIT_V1_READY = "282825dfb9cb8ff48f1b519c4edb5e628746fc592e5fa7f5e118cb8bb6273468"
AUDIT_V1_RELEASE = "14833eebfac60d2d8a8260c7e0735a67bb3b3cc94ae70bac89684ac1bef89139"
AUDIT_V1_REVIEW = "85317bf21e15a12fa3969daac290bdd316b705ab02c1d653ca221b1927fdcd09"
AUDIT_V2_READY = "4905e4ce0998d0be39185a511abe2e189483c87b17de9e9eb0fdf038540ffaf7"
AUDIT_V2_RELEASE = "1af3177c83db471a1dd44986de7b8b5322fea016b991a88dcf886fad9a27eba3"
AUDIT_V2_REVIEW = "845467c2532186e42c607f6a393c32f9e50178887af97e03afcc17068425aa2e"
OFFICE_V3_READY = "d027b1c3e09109b343ad1df5fa879e6fac691357052b87e65c2ed51e40f2038c"
OFFICE_V3_CPP = "962143edafef9ac0a524d91212dd12b82eec277fe1391b512b924693a70366b7"
MODAL_CPP = "9dc34a7279c15f7cf153c8f063cb06d565be9f9792e50def35bac2195241f05c"
MODAL_H = "4c1059fefad4a983f1b666446c1a87e255aeaa37fec3a1c9e097df910ea6b7ef"
NAMES = ("[SNOW] 雪原同盟", "[ROK] 王国远征军", "[CN1] 九州盟约", "[SUN] 日曜骑士团", "[WLD] 世界之巅")
CAMERA = ("camera_x", "camera_y", "camera_z", "camera_pitch", "camera_yaw", "camera_roll",
          "camera_scale_x", "camera_scale_y", "camera_scale_z")


def need(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def exact_file(path: Path, root: Path) -> Path:
    resolved = path.resolve(strict=True)
    need(resolved.is_file() and (resolved == root.resolve() or root.resolve() in resolved.parents),
         "path outside expected directory")
    return resolved


def expected_pins() -> dict[str, str]:
    text = (STAGE / "Source/Rok/RokAllianceFixtureSearchAuditSubsystem.cpp").read_text(encoding="utf-8-sig")
    pins = dict(re.findall(r'\{TEXT\("([^"\r\n]+)"\), TEXT\("([0-9a-f]{64})"\)\}', text))
    need(len(pins) == 11 and pins["Source/Rok/RokUnityFeatureScreenWidget.cpp"] == PRODUCT_CPP
         and pins["Source/Rok/RokUnityModalWidget.cpp"] == MODAL_CPP
         and pins["Source/Rok/RokUnityModalWidget.h"] == MODAL_H,
         "exact source pin set")
    friend = STAGE / "Source/Rok/RokUnityFeatureScreenWidget.h"
    need(sha(friend) == pins["Source/Rok/RokUnityFeatureScreenWidget.h"], "final friend pin")
    for name, expected in (("ready_for_root_review.json", PRODUCT_READY),
                           ("release_report.json", PRODUCT_RELEASE)):
        need(sha(ROOT / "Saved/PendingAllianceFixtureSearchV2" / name) == expected, "product freeze " + name)
    need(sha(ROOT / "Saved/PendingAllianceFixtureSearchIndependentReviewV1/review.json") == REVIEW,
         "inherited Product V1 independent review freeze")
    need(sha(ROOT / "Saved/PendingAllianceOfficeRowLayoutPolishV3/ready_for_root_review.json")
         == OFFICE_V3_READY, "office product V3 ready freeze")
    need(sha(ROOT / "Saved/PendingAllianceOfficeRowLayoutPolishV3/Source/Rok/RokUnityFeatureScreenWidget.cpp")
         == OFFICE_V3_CPP, "office product V3 CPP freeze")
    for name, expected in (("ready_for_root_review.json", AUDIT_V1_READY),
                           ("release_report.json", AUDIT_V1_RELEASE)):
        need(sha(ROOT / "Saved/PendingAllianceFixtureSearchRuntimeAuditV1" / name) == expected,
             "audit V1 freeze " + name)
    need(sha(ROOT / "Saved/PendingAllianceFixtureSearchRuntimeAuditIndependentReviewV1/review.json")
         == AUDIT_V1_REVIEW, "audit V1 independent review freeze")
    for name, expected in (("ready_for_root_review.json", AUDIT_V2_READY),
                           ("release_report.json", AUDIT_V2_RELEASE)):
        need(sha(ROOT / "Saved/PendingAllianceFixtureSearchRuntimeAuditV2" / name) == expected,
             "audit V2 freeze " + name)
    need(sha(ROOT / "Saved/PendingAllianceFixtureSearchRuntimeAuditIndependentReviewV2/review.json")
         == AUDIT_V2_REVIEW, "audit V2 independent review freeze")
    need(sha(ROOT / "Saved/PendingAllianceFixtureSearchV2/Source/Rok/RokUnityFeatureScreenWidget.cpp") == PRODUCT_CPP,
         "product CPP freeze")
    need(sha(ROOT / "Saved/PendingAllianceFixtureSearchV2/Source/Rok/RokUnityFeatureScreenWidget.h") == PRODUCT_H,
         "product H freeze")
    h = friend.read_text(encoding="utf-8-sig")
    for name in ("URokAllianceFixtureSearchAuditSubsystem", "URokTaskCompletedRowAnimationAuditSubsystem",
                 "URokAllianceOfficeReadonlyAuditSubsystem", "URokAllianceTechnologyReadonlyAuditSubsystem"):
        need(h.count("friend class " + name + ";") == 2, "preserved audit friend " + name)
    need("UEditableTextBox" in h and "AllianceFixtureSearchInput" in h, "product friend state")
    product_h = (ROOT / "Saved/PendingAllianceFixtureSearchV2/Source/Rok/RokUnityFeatureScreenWidget.h").read_text(
        encoding="utf-8-sig")
    need(h.replace("\tfriend class URokAllianceFixtureSearchAuditSubsystem;\n", "") == product_h,
         "friend header is Product V2 H plus only two audit friend declarations")
    # Future Buff2 modal files are frozen inputs for root integration; the old live Modal does not satisfy these pins.
    return pins


def static_guard() -> None:
    expected_pins()
    cpp = (STAGE / "Source/Rok/RokAllianceFixtureSearchAuditSubsystem.cpp").read_text(encoding="utf-8-sig")
    h = (STAGE / "Source/Rok/RokAllianceFixtureSearchAuditSubsystem.h").read_text(encoding="utf-8-sig")
    need(len(FIXTURE_PHASES) == 16 and sum(map(len, FIXTURE_INTERVALS)) == 21
         and len(PRODUCTION_PHASES) == 6 and sum(map(len, PRODUCTION_INTERVALS)) == 6
         and "bFixture ? 21 : 6" in cpp, "fixed action/capture count agreement")
    for required in ("ProcessMouseButtonDownEvent", "ProcessMouseButtonUpEvent", "ProcessKeyDownEvent",
                     "ProcessKeyUpEvent", "ProcessKeyCharEvent", "GetKeyboardFocusedWidget",
                     "LocateWindowUnderMouse", "SEditableText", "AllianceJoinRowHit_1",
                     "-RokAllianceFixtureSearchAudit", "-RokLocalOnly", "PendingActionId",
                     "FScreenshotRequest::RequestScreenshot", "GFrameCounter", "UtcNow().GetTicks()",
                     "GetHttpDispatchAttemptCount", "GetSuppressedLocalRequestCount"):
        need(required in cpp or required in h, "native audit route missing " + required)
    for forbidden in ("->SetText(", "->OpenAlliance(", "->HandleAction(", "FlushPendingActionForTesting(",
                      "AdvanceWorldTime(", "SearchAlliance(", "SendJionAlliance("):
        need(forbidden not in cpp, "forbidden audit shortcut " + forbidden)
    need("UE_BUILD_SHIPPING || !WITH_DEV_AUTOMATION_TESTS" in cpp, "nonshipping gate")
    need('Attempt->SetNumberField(TEXT("pending_after")' in cpp
         and 'const bool bDeferredCorrect = Indexed ? PendingAfter == Indexed->ActionId : PendingAfter == INDEX_NONE;'
         in cpp and 'Down && Up && bDeferredCorrect' in cpp,
         "native indexed deferred click gate")


def widget(state: dict, name: str) -> dict:
    return state["widgets"][name]


def shown(item: dict) -> bool:
    return item["exists"] is True and item["visible"] is True and item["enabled"] is True


def absent(item: dict) -> bool:
    return item["exists"] is False and item["visible"] is False


def check_png(path: Path, digest: str, byte_count: int) -> None:
    data = path.read_bytes()
    need(len(data) == byte_count and sha(path) == digest and len(data) > 24
         and data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR"
         and struct.unpack(">II", data[16:24]) == (1440, 810), "original PNG hash/extent")


def check_chronology(previous_frame: int, previous_ticks: int,
                     frames: tuple[int, int, int], ticks: tuple[int, int, int]) -> None:
    need(previous_frame < frames[0] <= frames[1] < frames[2]
         and previous_ticks <= ticks[0] <= ticks[1] <= ticks[2],
         "capture frame/UTC progression")


def check_business(b: dict) -> None:
    need(b["server_local_only"] is True and b["http_dispatch_attempts"] == 0
         and b["http_suppressed_requests"] == 0 and b["active_upgrades"] == 0
         and b["active_construction"] == 0 and b["pending_completed_upgrades"] == 0
         and b["completed_upgrade_events"] == 0 and b["construction_ticket_1_exists"] is False
         and b["profile_kind"] == "local_preview" and b["profile_has_role_id"] is False
         and b["profile_role_id"] == "" and b["profile_has_civilization"] is False
         and len(b["starter_heroes"]) > 0 and len(b["starter_troops"]) > 0
         and len(b["task_chain"]) > 0 and len(b["city_actor_paths"]) > 0,
         "anonymous resource/troop/task/server baseline")


def check_state(s: dict, phase: int, mode: str, baseline: dict, camera: tuple,
                primary: int, selection: str, pid: int, window: str) -> None:
    need(s["pid"] == pid and s["window_owner_pid"] == s["foreground_owner_pid"] == pid
         and s["window_foreground"] is True and s["own_window_identity"] == window
         and s["foreground_window_identity"] == window and re.fullmatch(r"0x[0-9a-fA-F]+", window)
         and (s["client_width"], s["client_height"]) == (1440, 810), "owned foreground window")
    need(s["business"] == baseline and s["modal_open"] is False
         and s["primary_press_count"] == primary and s["selected_actor"] == selection
         and all(abs(s[k] - x) <= .001 for k, x in zip(CAMERA, camera)), "business/camera invariant")
    w = s["widgets"]
    hud = phase == 0 or (mode == "fixture" and phase in (12, 15)) or (mode == "production" and phase == 5)
    if hud:
        need(s["screen_open"] is False and shown(w["AllianceFeatureButton"]), "real HUD phase")
        return
    need(s["screen_open"] is True and s["active_mode"] == 6 and s["pending_action_id"] == -1
         and s["fixture_state"] is (mode == "fixture")
         and s["alliance_ui_settled"]["ready"] is True
         and s["alliance_ui_settled"]["unsettled_children"] == 0
         and s["alliance_ui_settled"]["remaining_texture_compilations"] == 0
         and s["alliance_ui_settled"]["texture_count"] == 4
         and s["alliance_ui_settled"]["texture_rhi_ready"] is True
         and shown(w["FeatureModeCanvas"]) and shown(w["FeatureFrameClose"]), "settled Alliance frame")
    welcome = phase in ((1, 10, 13) if mode == "fixture" else (1, 4))
    if welcome:
        need(s["alliance_preview_state"] == 0 and s["selected_original_index"] == 0
             and s["draft"] == s["submitted_query"] == s["input_text"] == ""
             and s["visible_original_indices"] == [] and shown(w["AllianceWelcomeJoin"])
             and absent(w["AllianceSearchInput"]) and w["FeatureFrameTitle"]["text"] == "联盟",
             "welcome reset")
        return
    need(s["alliance_preview_state"] == 1 and shown(w["AllianceSearchInput"])
         and shown(w["AllianceSearch"]) and shown(w["AllianceJoinBack"])
         and w["FeatureFrameTitle"]["text"] == "加入联盟", "join list route")
    if mode == "production":
        need(s["visible_original_indices"] == [] and s["selected_original_index"] == 0
             and s["draft"] == s["submitted_query"] == ""
             and s["input_text"] == ("ROK" if phase == 3 else "")
             and shown(w["AllianceJoinEmptyState"]) and shown(w["AllianceJoinServiceState"])
             and absent(w["AllianceJoinFixtureTag"]) and absent(w["AllianceJoinConfirm"])
             and absent(w["AllianceJoinDetailName"]) and all(absent(r["RowHit"]) for r in s["rows"]),
             "ordinary zero-row server boundary")
        if phase == 3:
            need("联盟搜索需要服务器数据" in w["FeatureBoundaryFeedback"]["text"],
                 "production search boundary feedback")
        return
    expected = {
        2: ("", list(range(5)), 0), 3: ("ROK", [1], 1), 4: ("sNoW", [0], 0),
        5: ("RO", [], -1), 6: ("ZZZ", [], -1), 7: ("", list(range(5)), 0),
        8: ("ROK", [1], 1), 9: ("ROK", [1], 1),
        11: ("", list(range(5)), 0), 14: ("", list(range(5)), 0),
    }
    query, ids, selected = expected[phase]
    need(s["input_text"] == s["draft"] == s["submitted_query"] == query
         and s["visible_original_indices"] == ids and s["selected_original_index"] == selected
         and shown(w["AllianceJoinFixtureTag"])
         and "本地参考态（非账号状态）" in w["AllianceJoinFixtureTag"]["text"], "fixture query/results/selection")
    need(len(s["rows"]) == 5, "five original ID observations")
    for i, row in enumerate(s["rows"]):
        need(row["original_index"] == i and row["action_id"] == 610 + i, "original ID/action mapping")
        if i in ids:
            need(shown(row["RowHit"]) and shown(row["Name"])
                 and row["RowHit"]["name"] == f"AllianceJoinRowHit_{i}"
                 and row["Name"]["text"] == NAMES[i], "visible original ID row")
        else:
            need(absent(row["RowHit"]) and absent(row["Name"]), "filtered row was still present")
    if ids:
        need(absent(w["AllianceJoinFixtureEmpty"]) and shown(w["AllianceJoinDetailName"])
             and w["AllianceJoinDetailName"]["text"] == NAMES[selected]
             and shown(w["AllianceJoinConfirm"]) and absent(w["AllianceJoinDetailEmpty"]),
             "selected detail maps original ID")
    else:
        need(shown(w["AllianceJoinFixtureEmpty"]) and shown(w["AllianceJoinDetailEmpty"])
             and absent(w["AllianceJoinDetailName"]) and absent(w["AllianceJoinConfirm"])
             and w["AllianceJoinFixtureEmpty"]["text"] == ("本地参考搜索请至少输入3个字符。"
                    if phase == 5 else "本地参考列表没有匹配的联盟。"),
             "empty list/detail/JoinConfirm safety")


def check_edit(a: dict, label: str, text: str, before: str, baseline: dict,
               primary: int, pid: int, window: str) -> None:
    need(a["label"] == label and a["kind"] == "edit" and a["target"] == "AllianceSearchInput"
         and a["target_widget_class"] == "/Script/UMG.EditableTextBox"
         and "Slate pointer focus click" in a["method"] and "SetText" not in a["method"]
         and a["exact_input_in_path"] is True and a["editable_text_in_path"] is True
         and a["umg_path_present"] is True and a["keyboard_focus_in_hit_path"] is True
         and "EditableText" in a["keyboard_focused_type"]
         and a["keyboard_focused_identity"] in [n["slate_identity"] for n in a["hit_path_nodes"]]
         and a["target_slate_identity"] in [n["slate_identity"] for n in a["hit_path_nodes"]]
         and a["own_game_window"] is True and a["foreground"] is True
         and a["own_window_identity"] == a["foreground_window_identity"] == window
         and a["own_window_pid"] == pid and a["target_hovered"] is True
         and a["stable_frames"] >= 2 and a["focus_down_handled"] is True
         and a["focus_up_handled"] is True and a["select_all_handled"] is True
         and a["text_before"] == before and a["text_after_erase"] == ""
         and (a["backspace_handled"] is True or (before == "" and a["backspace_empty_noop"] is True))
         and a["requested_text"] == a["text_after"] == text
         and a["all_characters_handled"] is True and a["focus_retained"] is True
         and [x["character"] for x in a["character_events"]] == list(text)
         and all(x["handled"] is True for x in a["character_events"])
         and a["business_before"] == a["business_after"] == baseline
         and a["primary_before"] == a["primary_after"] == primary and a["failure"] == "",
         "real Slate pointer focus/key route " + label)


def check_click(a: dict, label: str, target: str, action_id: int, baseline: dict,
                primary: int, pid: int, window: str) -> None:
    need(a["label"] == label and a["target"] == target and a["action_id"] == action_id
         and a["method"] == "ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent"
         and a["exact_button_in_path"] is True and a["umg_path_present"] is True
         and a["target_hovered"] is True and a["own_game_window"] is True
         and a["foreground"] is True and a["own_window_pid"] == pid
         and a["own_window_identity"] == a["foreground_window_identity"] == window
         and a["down_handled"] is True and a["up_handled"] is True
         and a["stable_frames"] >= 2 and a["pending_before"] == -1
         and (a["pending_after"] == -1 if action_id == 0 else a["pending_after"] == action_id)
         and a["business_before"] == a["business_after"] == baseline
         and a["primary_before"] == a["primary_after"] == primary and a["failure"] == "",
         "real Slate button route " + label)
    need(a["target_slate_identity"] in [n["slate_identity"] for n in a["hit_path_nodes"]]
         and a["target_widget_identity"].startswith("0x"), "exact hit path identity " + label)


def validate(d: dict, run: Path) -> None:
    pins = expected_pins()
    static_guard()
    need(d["schema"] == SCHEMA and d["mode"] in ("fixture", "production")
         and d["complete"] is True and d["passed"] is True and d["failures"] == []
         and d["attempts"] == [] and d["completion_events"] == 0
         and d["baseline_phase"] == "00_city_hud" and d["engine_major"] == 5
         and d["engine_minor"] == 8 and d["world"] == "/Game/Maps/L_RokUnityGameAppCity"
         and Path(d["project"]).resolve() == ROOT.resolve()
         and Path(d["engine_dir"]).resolve() == Path("D:/ue/UE_5.8/Engine").resolve()
         and d["pid"] > 0 and int(d["utc_ticks"]) > 0,
         "runtime claim/world/engine")
    for flag in ("world_time_advanced_by_audit", "alliance_membership_accepted",
                 "account_alliance_state_accepted", "server_query_response_accepted",
                 "unity_pixel_parity_accepted", "full_migration_accepted"):
        need(d[flag] is False, "scope claim " + flag)
    need(re.fullmatch(r"[0-9]+_" + str(d["pid"]), run.name), "unique UTCticks_PID output")
    expected = dict(pins)
    for relative in ("Source/Rok/RokAllianceFixtureSearchAuditSubsystem.h",
                     "Source/Rok/RokAllianceFixtureSearchAuditSubsystem.cpp",
                     "Scripts/VerifyRokAllianceFixtureSearchRuntime.py"):
        expected[relative] = sha(STAGE / relative)
    need(d["inputs_sha256"] == expected, "exact pinned input map")
    for relative, digest in expected.items():
        need(sha(exact_file(ROOT / relative, ROOT)) == digest, "live source pin drift " + relative)
    phases = FIXTURE_PHASES if d["mode"] == "fixture" else PRODUCTION_PHASES
    intervals = FIXTURE_INTERVALS if d["mode"] == "fixture" else PRODUCTION_INTERVALS
    caps, acts = d["captures"], d["actions"]
    need(len(caps) == len(phases) and len(acts) == sum(map(len, intervals))
         and tuple(c["phase"] for c in caps) == phases,
         "fixed capture/action sequence")
    baseline = d["baseline_business"]
    check_business(baseline)
    first = caps[0]["requested_state"]
    primary, selection = first["primary_press_count"], first["selected_actor"]
    camera = tuple(first[k] for k in CAMERA)
    window = first["own_window_identity"]
    previous_frame = 0
    previous_ticks = 0
    for i, c in enumerate(caps):
        frames = (c["requested_frame"], c["processed_frame"], c["next_frame"])
        ticks = tuple(c[k]["utc_ticks"] for k in ("requested_state", "processed_state", "next_frame_state"))
        check_chronology(previous_frame, previous_ticks, frames, ticks)
        previous_frame = frames[2]
        png = exact_file(Path(c["png"]), run)
        need(png == (run / (phases[i] + ".png")).resolve(strict=True)
             and c["png_width"] == 1440 and c["png_height"] == 810,
             "original screenshot location/extent")
        check_png(png, c["png_sha256"], c["png_bytes"])
        for key, frame in zip(("requested_state", "processed_state", "next_frame_state"), frames):
            s = c[key]
            need(s["frame"] == frame and s["utc_ticks"] >= previous_ticks,
                 "state frame and UTC order")
            previous_ticks = s["utc_ticks"]
            check_state(s, i, d["mode"], baseline, camera, primary, selection, d["pid"], window)
    need(len({c["png_sha256"] for c in caps}) >= (7 if d["mode"] == "fixture" else 4),
         "screen diversity")
    n = 0
    for i, specs in enumerate(intervals):
        previous_action_frame = caps[i]["next_frame"]
        previous_action_ticks = caps[i]["next_frame_state"]["utc_ticks"]
        before_text = caps[i]["next_frame_state"]["input_text"]
        for label, target, value in specs:
            a = acts[n]
            need(a["label"] == label and previous_action_frame < a["frame"] < caps[i + 1]["requested_frame"]
                 and a["utc_ticks"] >= previous_action_ticks, "action frame/UTC order")
            previous_action_frame, previous_action_ticks = a["frame"], a["utc_ticks"]
            if target == "edit":
                check_edit(a, label, value, before_text, baseline, primary, d["pid"], window)
                before_text = value
            else:
                check_click(a, label, target, value, baseline, primary, d["pid"], window)
                if label == "select_visible_original_1":
                    need(caps[i]["next_frame_state"]["visible_original_indices"] == [1]
                         and target == "AllianceJoinRowHit_1" and value == 611,
                         "selected filtered original ID")
            n += 1
    if d["mode"] == "fixture":
        default_y = caps[2]["next_frame_state"]["rows"][0]["RowBg"]["absolute_top_left"]["y"]
        filtered_y = caps[3]["next_frame_state"]["rows"][1]["RowBg"]["absolute_top_left"]["y"]
        need(abs(default_y - filtered_y) <= .5, "filtered row occupies first visible slot")
    last = caps[-1]["next_frame_state"]
    final = d["final_state"]
    check_final_progression(last, final)
    check_state(final, len(phases) - 1, d["mode"], baseline, camera, primary, selection,
                d["pid"], window)


def check_final_progression(last: dict, final: dict) -> None:
    # AdvanceCapture returns from one NativeTick; Finish executes in exactly the next Tick.
    need(final["frame"] == last["frame"] + 1 and final["utc_ticks"] > last["utc_ticks"],
         "finish must be exactly next frame with later UTC")
    observed, completed = copy.deepcopy(last), copy.deepcopy(final)
    for sample in (observed, completed):
        del sample["frame"]
        del sample["utc_ticks"]
    need(completed == observed, "finish state drift beyond frame and observation time")


def reject(call, label: str) -> None:
    try:
        call()
    except (ValueError, KeyError, TypeError, OSError):
        return
    raise RuntimeError("negative accepted: " + label)


def self_test() -> dict:
    static_guard()
    n = 0
    w = lambda name, exists=True, visible=True, text="": {
        "name": name, "exists": exists, "visible": visible, "enabled": exists,
        "text": text, "absolute_top_left": {"x": 0, "y": 0}}
    baseline = {"server_local_only": True, "http_dispatch_attempts": 0, "http_suppressed_requests": 0,
                "active_upgrades": 0, "active_construction": 0, "pending_completed_upgrades": 0,
                "completed_upgrade_events": 0, "construction_ticket_1_exists": False,
                "profile_kind": "local_preview", "profile_has_role_id": False,
                "profile_role_id": "", "profile_has_civilization": False,
                "starter_heroes": [{}], "starter_troops": [{}], "task_chain": [{}], "city_actor_paths": ["x"]}
    check_business(baseline)
    for key, bad in (("http_dispatch_attempts", 1), ("http_suppressed_requests", 1),
                     ("active_upgrades", 1), ("starter_troops", []), ("task_chain", []),
                     ("profile_has_role_id", True)):
        x = copy.deepcopy(baseline); x[key] = bad
        reject(lambda x=x: check_business(x), key); n += 1
    action = {
        "label": "type_ROK", "kind": "edit", "target": "AllianceSearchInput",
        "target_widget_class": "/Script/UMG.EditableTextBox",
        "method": "Slate pointer focus click; Ctrl+A; Backspace; ProcessKeyCharEvent",
        "exact_input_in_path": True, "editable_text_in_path": True, "umg_path_present": True,
        "keyboard_focus_in_hit_path": True, "keyboard_focused_type": "SEditableText",
        "keyboard_focused_identity": "0x2", "target_slate_identity": "0x1",
        "hit_path_nodes": [{"slate_identity": "0x1"}, {"slate_identity": "0x2"}],
        "own_game_window": True, "foreground": True, "own_window_identity": "0x3",
        "foreground_window_identity": "0x3", "own_window_pid": 1234, "target_hovered": True,
        "stable_frames": 2, "focus_down_handled": True, "focus_up_handled": True,
        "select_all_handled": True, "text_before": "", "text_after_erase": "",
        "backspace_handled": False, "backspace_empty_noop": True,
        "requested_text": "ROK", "text_after": "ROK", "all_characters_handled": True,
        "focus_retained": True, "character_events": [{"character": c, "handled": True} for c in "ROK"],
        "business_before": baseline, "business_after": baseline,
        "primary_before": 0, "primary_after": 0, "failure": "",
    }
    check_edit(action, "type_ROK", "ROK", "", baseline, 0, 1234, "0x3")
    for key, bad in (("method", "SetText"), ("target", "OtherInput"),
                     ("exact_input_in_path", False), ("editable_text_in_path", False),
                     ("keyboard_focus_in_hit_path", False), ("keyboard_focused_identity", "0x9"),
                     ("focus_down_handled", False), ("select_all_handled", False),
                     ("all_characters_handled", False), ("text_after_erase", "old"),
                     ("text_after", ""), ("own_window_pid", 9), ("foreground", False),
                     ("primary_after", 1)):
        x = copy.deepcopy(action); x[key] = bad
        reject(lambda x=x: check_edit(x, "type_ROK", "ROK", "", baseline, 0, 1234, "0x3"), key); n += 1
    x = copy.deepcopy(action); x["character_events"][1]["handled"] = False
    reject(lambda: check_edit(x, "type_ROK", "ROK", "", baseline, 0, 1234, "0x3"), "unhandled char"); n += 1
    x = copy.deepcopy(action); x["business_after"]["task_chain"] = []
    reject(lambda: check_edit(x, "type_ROK", "ROK", "", baseline, 0, 1234, "0x3"), "business mutation"); n += 1
    click = {"label": "select_visible_original_1", "target": "AllianceJoinRowHit_1", "action_id": 611,
             "method": "ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent",
             "exact_button_in_path": True, "umg_path_present": True, "target_hovered": True,
             "own_game_window": True, "foreground": True, "own_window_pid": 1234,
             "own_window_identity": "0x3", "foreground_window_identity": "0x3",
             "down_handled": True, "up_handled": True, "stable_frames": 2, "pending_before": -1,
             "pending_after": 611,
             "business_before": baseline, "business_after": baseline, "primary_before": 0,
             "primary_after": 0, "failure": "", "target_slate_identity": "0x1",
             "target_widget_identity": "0x2", "hit_path_nodes": [{"slate_identity": "0x1"}]}
    check_click(click, "select_visible_original_1", "AllianceJoinRowHit_1", 611, baseline, 0, 1234, "0x3")
    for key, bad in (("target", "AllianceJoinRowHit_0"), ("action_id", 610),
                     ("exact_button_in_path", False), ("down_handled", False),
                     ("up_handled", False), ("own_window_pid", 1), ("pending_before", 624),
                     ("pending_after", -1), ("pending_after", 610)):
        x = copy.deepcopy(click); x[key] = bad
        reject(lambda x=x: check_click(x, "select_visible_original_1", "AllianceJoinRowHit_1", 611,
                                       baseline, 0, 1234, "0x3"), key); n += 1
    x = copy.deepcopy(click); del x["pending_after"]
    reject(lambda: check_click(x, "select_visible_original_1", "AllianceJoinRowHit_1", 611,
                               baseline, 0, 1234, "0x3"), "missing pending_after"); n += 1
    hud = copy.deepcopy(click)
    hud.update(label="open_alliance_from_hud", target="AllianceFeatureButton", action_id=0,
               pending_after=-1)
    check_click(hud, "open_alliance_from_hud", "AllianceFeatureButton", 0, baseline, 0, 1234, "0x3")
    hud["pending_after"] = 600
    reject(lambda: check_click(hud, "open_alliance_from_hud", "AllianceFeatureButton", 0,
                               baseline, 0, 1234, "0x3"), "HUD pending contamination"); n += 1
    no = {"pid": 1234, "window_owner_pid": 1234, "foreground_owner_pid": 1234,
          "window_foreground": True, "own_window_identity": "0x3", "foreground_window_identity": "0x3",
          "client_width": 1440, "client_height": 810, "business": baseline, "modal_open": False,
          "primary_press_count": 0, "selected_actor": "", **{k: 0 for k in CAMERA},
          "screen_open": True, "active_mode": 6, "pending_action_id": -1, "fixture_state": True,
          "alliance_ui_settled": {"ready": True, "unsettled_children": 0,
                                  "remaining_texture_compilations": 0, "texture_count": 4,
                                  "texture_rhi_ready": True},
          "widgets": {name: w(name) for name in ("FeatureModeCanvas", "FeatureFrameClose",
                        "AllianceSearchInput", "AllianceSearch", "AllianceJoinBack",
                        "AllianceJoinFixtureTag", "AllianceJoinFixtureEmpty", "AllianceJoinDetailEmpty")},
          "alliance_preview_state": 1, "input_text": "ZZZ", "draft": "ZZZ",
          "submitted_query": "ZZZ", "visible_original_indices": [], "selected_original_index": -1,
          "rows": [{"original_index": i, "action_id": 610+i,
                    "RowHit": w(f"AllianceJoinRowHit_{i}", False, False),
                    "Name": w(f"AllianceJoinName_{i}", False, False)} for i in range(5)]}
    no["widgets"].update({name: w(name, False, False) for name in
                          ("AllianceJoinDetailName", "AllianceJoinConfirm")})
    no["widgets"]["FeatureFrameTitle"] = w("FeatureFrameTitle", text="加入联盟")
    no["widgets"]["AllianceJoinFixtureTag"]["text"] = "联盟列表 · 本地参考态（非账号状态）"
    no["widgets"]["AllianceJoinFixtureEmpty"]["text"] = "本地参考列表没有匹配的联盟。"
    check_state(no, 6, "fixture", baseline, (0,)*len(CAMERA), 0, "", 1234, "0x3")
    for key, bad in (("selected_original_index", 0), ("visible_original_indices", [0]),
                     ("draft", ""), ("window_owner_pid", 2), ("pending_action_id", 624)):
        x = copy.deepcopy(no); x[key] = bad
        reject(lambda x=x: check_state(x, 6, "fixture", baseline, (0,)*len(CAMERA), 0, "", 1234, "0x3"), key); n += 1
    for key in ("AllianceJoinDetailName", "AllianceJoinConfirm"):
        x = copy.deepcopy(no); x["widgets"][key] = w(key)
        reject(lambda x=x: check_state(x, 6, "fixture", baseline, (0,)*len(CAMERA), 0, "", 1234, "0x3"),
               "empty residual " + key); n += 1
    x = copy.deepcopy(no); x["rows"][1]["RowHit"] = w("AllianceJoinRowHit_1")
    reject(lambda: check_state(x, 6, "fixture", baseline, (0,)*len(CAMERA), 0, "", 1234, "0x3"),
           "filtered row still present"); n += 1
    x = copy.deepcopy(no); x["widgets"]["AllianceJoinFixtureEmpty"]["text"] = "min fake"
    reject(lambda: check_state(x, 6, "fixture", baseline, (0,)*len(CAMERA), 0, "", 1234, "0x3"),
           "wrong no-result hint"); n += 1
    prod = copy.deepcopy(no)
    prod["fixture_state"] = False
    prod["selected_original_index"] = 0
    prod["draft"] = prod["submitted_query"] = ""
    prod["input_text"] = "ROK"
    prod["widgets"]["AllianceJoinFixtureTag"] = w("AllianceJoinFixtureTag", False, False)
    prod["widgets"]["AllianceJoinEmptyState"] = w("AllianceJoinEmptyState")
    prod["widgets"]["AllianceJoinServiceState"] = w("AllianceJoinServiceState")
    prod["widgets"]["FeatureBoundaryFeedback"] = w("FeatureBoundaryFeedback", text="联盟搜索需要服务器数据，当前列表未改变。")
    check_state(prod, 3, "production", baseline, (0,)*len(CAMERA), 0, "", 1234, "0x3")
    x = copy.deepcopy(prod); x["rows"][0]["RowHit"] = w("AllianceJoinRowHit_0")
    reject(lambda: check_state(x, 3, "production", baseline, (0,)*len(CAMERA), 0, "", 1234, "0x3"),
           "ordinary mode forged row"); n += 1
    x = copy.deepcopy(prod); x["visible_original_indices"] = [0]
    reject(lambda: check_state(x, 3, "production", baseline, (0,)*len(CAMERA), 0, "", 1234, "0x3"),
           "ordinary mode forged index"); n += 1
    with tempfile.TemporaryDirectory(dir=STAGE) as tmp:
        fake = Path(tmp) / "fake.png"
        fake.write_bytes(b"\x89PNG\r\n\x1a\n" + b"\0"*4 + b"IHDR" + struct.pack(">II", 1440, 810) + b"x")
        reject(lambda: check_png(fake, "0"*64, fake.stat().st_size), "PNG SHA forgery"); n += 1
        reject(lambda: check_png(fake, sha(fake), fake.stat().st_size + 1), "PNG byte count"); n += 1
    check_chronology(1, 10, (2, 2, 3), (11, 12, 13))
    for frames, ticks in (((1, 2, 3), (11, 12, 13)), ((2, 2, 2), (11, 12, 13)),
                          ((2, 3, 4), (9, 12, 13)), ((2, 3, 4), (11, 13, 12))):
        reject(lambda f=frames, t=ticks: check_chronology(1, 10, f, t), "frame/ticks"); n += 1
    last = {"frame": 20, "utc_ticks": 100, "business": copy.deepcopy(baseline),
            "screen_open": False, "input_text": "", "pending_action_id": -1}
    final = copy.deepcopy(last); final.update(frame=21, utc_ticks=101)
    check_final_progression(last, final)
    for label, changes in (("same finish frame", {"frame": 20}),
                           ("extra finish frame", {"frame": 22}),
                           ("equal finish UTC", {"utc_ticks": 100}),
                           ("reverse finish UTC", {"utc_ticks": 99}),
                           ("finish UI drift", {"input_text": "stale"})):
        x = copy.deepcopy(final); x.update(changes)
        reject(lambda x=x: check_final_progression(last, x), label); n += 1
    x = copy.deepcopy(final); x["business"]["http_dispatch_attempts"] = 1
    reject(lambda: check_final_progression(last, x), "finish business drift"); n += 1
    for forbidden in ("->SetText(", "->HandleAction("):
        reject(lambda f=forbidden: need(f not in "probe " + f, "forbidden shortcut"), forbidden); n += 1
    return {"synthetic_negative_cases": n, "actual_ue_build": False, "actual_runtime": False}


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--self-test", action="store_true")
    p.add_argument("--manifest", type=Path)
    a = p.parse_args()
    need(a.self_test != bool(a.manifest), "choose --self-test or --manifest")
    if a.self_test:
        print(json.dumps(self_test(), ensure_ascii=False, sort_keys=True)); return
    path = a.manifest.resolve(strict=True)
    run = path.parent
    need(path == run / "manifest.json" and run.parent == STAGE / "RuntimeAudit", "owned manifest path")
    validate(json.loads(path.read_text(encoding="utf-8-sig")), run)
    print(json.dumps({"actual_runtime_verified": True, "manifest": str(path)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
