"""Strict verifier for the opt-in genuine Slate task-row animation audit.

Self-tests are synthetic contract negatives only; they never constitute runtime
acceptance. The actual manifest must be emitted by the native subsystem.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import struct
import tempfile
import zlib

SCHEMA = "rok.task_completed_row_animation.runtime_audit.v6"
PHASES = (
    "00_fixture_idle", "01_first_mid", "02_first_complete",
    "03_hud_after_cancel", "04_normal_tasks", "05_hud_return",
)
LABELS = (
    "start_first_preview", "duplicate_during_first",
    "start_second_preview", "cancel_second_by_close",
    "open_normal_tasks_from_hud", "close_normal_tasks",
)
TARGETS = (
    "TaskCompletedAnimationPreview", "TaskCompletedAnimationPreview",
    "TaskCompletedAnimationPreview", "FeatureFrameClose", "TaskEntryButton",
    "FeatureFrameClose",
)
ACTION_IDS = (319, 319, 319, 1, 0, 1)
ACTION_STEPS = (2, 3, 6, 7, 9, 11)
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
METHOD = "ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent"


def require(condition: object, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def finite_number(value: object, name: str) -> float:
    require(isinstance(value, (int, float)) and not isinstance(value, bool), name)
    result = float(value)
    require(math.isfinite(result), name)
    return result


def png_pixels(data: bytes) -> tuple[int, int, int, int]:
    """Decode 8-bit noninterlaced RGB/RGBA/gray PNG and count nonblack pixels."""
    require(data.startswith(PNG_SIGNATURE), "canonical PNG8 signature")
    offset = 8
    chunks: list[bytes] = []
    width = height = channels = 0
    finished = False
    while offset + 12 <= len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        require(size <= len(data) - offset - 12, "PNG chunk bounds")
        payload = data[offset + 8:offset + 8 + size]
        crc = struct.unpack_from(">I", data, offset + 8 + size)[0]
        require(zlib.crc32(kind + payload) & 0xFFFFFFFF == crc, "PNG CRC")
        offset += 12 + size
        if kind == b"IHDR":
            require(width == 0 and size == 13, "single valid IHDR")
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", payload)
            require((depth, compression, filtering, interlace) == (8, 0, 0, 0), "PNG format")
            require(color in (0, 2, 6), "PNG color type")
            channels = {0: 1, 2: 3, 6: 4}[color]
        elif kind == b"IDAT":
            chunks.append(payload)
        elif kind == b"IEND":
            require(size == 0, "IEND length")
            finished = True
            break
    require(finished and offset == len(data) and width == 1440 and height == 810 and chunks, "PNG extent/chunks")
    stride = width * channels
    raw = zlib.decompress(b"".join(chunks))
    require(len(raw) == height * (stride + 1), "PNG decompressed extent")
    prev = bytearray(stride)
    nonblack = 0
    colorful = 0
    color_bins: set[tuple[int, ...]] = set()
    pos = 0
    for _ in range(height):
        filt = raw[pos]
        pos += 1
        row = bytearray(raw[pos:pos + stride])
        pos += stride
        require(filt <= 4, "PNG row filter")
        for x in range(stride):
            left = row[x - channels] if x >= channels else 0
            up = prev[x]
            upper_left = prev[x - channels] if x >= channels else 0
            if filt == 1:
                row[x] = (row[x] + left) & 255
            elif filt == 2:
                row[x] = (row[x] + up) & 255
            elif filt == 3:
                row[x] = (row[x] + (left + up) // 2) & 255
            elif filt == 4:
                p = left + up - upper_left
                d = (abs(p - left), abs(p - up), abs(p - upper_left))
                predictor = (left, up, upper_left)[d.index(min(d))]
                row[x] = (row[x] + predictor) & 255
        for x in range(0, stride, channels):
            rgb = row[x:x + min(3, channels)]
            if max(rgb) > 12:
                nonblack += 1
            if channels >= 3 and max(rgb) - min(rgb) > 12:
                colorful += 1
            if x % (channels * 37) == 0:
                color_bins.add(tuple(value // 16 for value in rgb))
        prev = row
    require(nonblack > width * height // 20 and colorful > 1000 and len(color_bins) >= 24,
            "PNG black/placeholder")
    return width, height, nonblack, colorful


def widget(state: dict, name: str) -> dict:
    result = state["widgets"][name]
    require(isinstance(result, dict), f"widget {name}")
    return result


def state_common(state: dict, manifest: dict, expected_pending: int = -1) -> None:
    pid = manifest["pid"]
    require(state["window_foreground"] is True, "foreground")
    require(state["window_owner_pid"] == state["foreground_owner_pid"] == pid, "owned PID")
    require(state["client_width"] == 1440 and state["client_height"] == 810, "client extent")
    require(state["own_window_identity"] == state["foreground_window_identity"], "window identity")
    require(state["modal_open"] is False, "modal boundary")
    require(state["business"] == manifest["baseline_business"], "business snapshot")
    business = state["business"]
    require(business["server_local_only"] is True and business["http_dispatch_attempts"] == 0
            and business["http_suppressed_requests"] == 0 and business["task_complete"] is False,
            "task/account/HTTP boundary")
    require(state["primary_press_count"] == business["primary_press_count"], "world input boundary")
    require(state["selected_actor"] == business["selected_actor"], "selection boundary")
    for field in ("camera_x", "camera_y", "camera_z", "camera_pitch", "camera_yaw", "camera_roll"):
        require(abs(finite_number(state[field], field) - finite_number(business[field], field)) <= .001,
                "camera boundary")
    probe = state["task_ui_settled"]
    if state["screen_open"]:
        require(probe["ready"] is True and probe["structure_valid"] is True, "settled task UI")
        require(probe["direct_children"] > 5 and probe["unsettled_children"] == 0
                and probe["unsettled_not_visible_children"] == 0, "animated child groups")
        require(probe["texture_count"] == 3 and probe["texture_rhi_ready"] is True
                and probe["remaining_texture_compilations"] == 0, "texture RHI/compilation")
        require(probe["portrait_gate_applicable"] is False and probe["portrait_present"] is False,
                "task has no Commander portrait")
    require(state["pending_action_id"] == expected_pending, "pending action")
    require(state["task_row_intent"] != "claim" or not state["screen_open"], "claim intent unavailable")


def phase_state(state: dict, phase: str) -> None:
    if phase in ("00_fixture_idle", "02_first_complete", "03_hud_after_cancel", "04_normal_tasks", "05_hud_return"):
        require(state["animation_active"] is False and state["animation_elapsed"] < 0,
                f"{phase} lingering animation")
    if phase == "01_first_mid":
        require(state["screen_open"] and state["local_fixture"] and state["animation_active"], f"{phase} active")
        elapsed = finite_number(state["animation_elapsed"], "elapsed")
        require(.04 <= elapsed <= .25, f"{phase} actual 0.3s midpoint")
        scale = finite_number(state["row_scale"]["x"], "row scale")
        require(.1 < scale < .99 and abs(scale - state["row_scale"]["y"]) < .01, f"{phase} row scale")
        require(state["animated_row_matches"] and state["callback_armed"], f"{phase} live row/callback")
        graphics = state["animated_graphics"]
        require(graphics["count"] > 0 and graphics["count"] == graphics["original_count"], f"{phase} graphics")
        require(any(x["valid"] and 0 <= x["current"] < x["original"] for x in graphics["entries"]),
                f"{phase} graphic fade")
    if phase in ("00_fixture_idle", "01_first_mid", "02_first_complete"):
        require(state["screen_open"] and state["local_fixture"] and state["selected_task_page"] == 1,
                f"{phase} fixture")
        require(widget(state, "TaskCompletedAnimationPreview")["visible"], f"{phase} preview")
        require(widget(state, "TaskCompletedAnimationFixtureNotice")["text"] ==
                "本地完成行动画参考 · 不代表任务完成或奖励可领", f"{phase} fixture notice")
    if phase in ("03_hud_after_cancel", "05_hud_return"):
        require(state["screen_open"] is False and widget(state, "TaskEntryButton")["visible"], f"{phase} HUD")
    if phase == "04_normal_tasks":
        require(state["screen_open"] and state["local_fixture"] is False and state["selected_task_page"] == 1,
                "normal task page")
        require(widget(state, "TaskCompletedAnimationPreview")["exists"] is False
                and widget(state, "TaskCompletedAnimationFixtureNotice")["exists"] is False,
                "normal mode fixture leakage")
        require(state["task_row_intent"] != "claim" and widget(state, "TaskAction_0")["visible"],
                "normal Claim isolation")
    callback = 0 if phase in ("00_fixture_idle", "01_first_mid", "04_normal_tasks", "05_hud_return") else 1
    require(state["visual_callback_count"] == callback, f"{phase} callback count")
    if phase in ("02_first_complete", "03_hud_after_cancel", "04_normal_tasks", "05_hud_return"):
        restore = state["graphic_restore"]
        require(restore["expected_count"] > 0
                and restore["checked_count"] == restore["expected_count"]
                and restore["mismatched_count"] == 0, f"{phase} opacity restore")
        if phase == "04_normal_tasks":
            # Rebuilt widgets may reuse the same UObject name/address; prove current normal graphics are restored,
            # and allow the original object identity to remain attached in the rebuilt live tree.
            require(restore["retained_original_count"] == restore["expected_count"]
                    and restore["detached_original_count"] == 0,
                    f"{phase} rebuilt normal graphics restored in live tree")
        elif phase != "02_first_complete":
            require(restore["retained_original_count"] == restore["expected_count"]
                    and restore["detached_original_count"] == restore["expected_count"],
                    f"{phase} original graphics detached after HUD close")
    if phase == "02_first_complete":
        require(abs(state["row_scale"]["x"] - 1) < .01, "completion scale restore")


def timing_contract(first: float, complete: float, second: float, cancel: float,
                    duplicate: float, mid_request: float) -> None:
    require(.285 <= complete - first <= 1.0 and complete < second < cancel
            and .035 <= cancel - second < .15, "wall-clock tween/cancel order")
    require(0 <= duplicate - first < .16 and duplicate < mid_request
            and 0 <= mid_request - first < .21, "duplicate before rendered midpoint deadline")


def rendered_frame_contract(capture: dict) -> None:
    require(capture["captured_frame"] == capture["processed_frame"]
            == capture["processed_state"]["frame"], "rendered PNG frame binding")
    require(abs(capture["captured_real_time_seconds"]
                - capture["processed_state"]["real_time_seconds"]) <= .05,
            "rendered PNG wall-clock binding")


def final_state_binding(final_state: dict, last_sample_state: dict) -> None:
    frame_delta = final_state["frame"] - last_sample_state["frame"]
    time_delta = finite_number(final_state["real_time_seconds"], "final state time") - finite_number(
        last_sample_state["real_time_seconds"], "last sampled state time")
    require(frame_delta == 1 and time_delta > 0,
            "final state is the next Tick after final capture with monotonic time")
    final_core = {k: v for k, v in final_state.items() if k not in ("frame", "real_time_seconds")}
    sample_core = {k: v for k, v in last_sample_state.items() if k not in ("frame", "real_time_seconds")}
    require(final_core == sample_core, "final state preserves all non-time UI/business fields")


def duplicate_during_first_contract(action: dict, samples: list[dict], first_started: float) -> None:
    after = action["state_after"]
    require(after["animation_active"] and after["visual_callback_count"] == 0,
            "duplicate did not restart/complete")
    require(abs(finite_number(after["animation_started_at_seconds"], "duplicate start")
                - first_started) < .0001,
            "duplicate changed the first start time recorded at the real midpoint")
    matching = [sample for sample in samples
                if sample["phase"] == "first" and sample["frame"] == action["frame"]]
    require(len(matching) == 1, "duplicate frame has one real first-animation sample")
    sample = matching[0]
    elapsed = finite_number(after["animation_elapsed"], "duplicate elapsed")
    sampled_elapsed = finite_number(sample["elapsed"], "same-frame sampled elapsed")
    require(0 < elapsed < .16 and abs(elapsed - sampled_elapsed) <= .01,
            "duplicate preserved live animation elapsed on the same rendered frame")
    for axis in ("x", "y"):
        scale = finite_number(after["row_scale"][axis], f"duplicate row scale {axis}")
        sampled_scale = finite_number(sample["row_scale"][axis], f"same-frame row scale {axis}")
        require(.1 < scale < 1 and abs(scale - sampled_scale) <= .01,
                "duplicate preserved live row scale on the same rendered frame")


def verify_manifest(manifest: dict, runtime_dir: Path, release: dict, *, allow_synthetic: bool = False) -> None:
    require(manifest["schema"] == SCHEMA and manifest["complete"] is True
            and manifest["passed"] is True and manifest["failures"] == [], "actual completed manifest")
    require(manifest["actual_runtime"] is True or allow_synthetic, "not actual runtime")
    require(manifest["project"].replace("\\", "/").rstrip("/").lower() == "d:/uedemo/rok", "project")
    require(manifest["engine_dir"].replace("\\", "/").rstrip("/").lower() == "d:/ue/ue_5.8/engine", "engine")
    require((manifest["engine_major"], manifest["engine_minor"], manifest["world"]) ==
            (5, 8, "/Game/Maps/L_RokUnityGameAppCity"), "UE/map")
    require(manifest["pid"] > 0 and manifest["world_time_advanced_by_audit"] is False, "PID/time")
    require(manifest["cli_fixture"] == "tasks_reference" and manifest["cli_audit_flag_verified"] is True,
            "explicit local fixture CLI")
    for key in ("task_claim_or_reward_accepted", "account_task_state_accepted", "unity_pixel_parity_accepted"):
        require(manifest[key] is False, key)
    require(manifest["baseline_phase"] == PHASES[0] and manifest["attempts"] == []
            and manifest["completion_events"] == 0, "baseline/attempts")
    require(manifest["inputs_sha256"] == release["runtime_input_sha256"], "source pins")
    require(len(manifest["actions"]) == 6 and len(manifest["captures"]) == 6, "action/capture count")
    require(manifest["first_mid_samples"] > 0 and manifest["second_mid_samples"] > 0, "per-frame mid evidence")
    first = finite_number(manifest["first_click_seconds"], "first click")
    complete = finite_number(manifest["first_callback_seconds"], "callback observed")
    second = finite_number(manifest["second_click_seconds"], "second click")
    cancel = finite_number(manifest["cancel_seconds"], "cancel click")
    timing_contract(first, complete, second, cancel,
                    manifest["actions"][1]["state_after"]["real_time_seconds"],
                    manifest["captures"][1]["requested_state"]["real_time_seconds"])
    phases = []
    prior_frame = -1
    prior_time = -math.inf
    for capture in manifest["captures"]:
        phase = capture["phase"]
        phases.append(phase)
        target = Path(capture["png"])
        require(target.resolve().parent == runtime_dir.resolve() and target.name == phase + ".png", "owned PNG path")
        data = target.read_bytes()
        require(capture["png_sha256"] == sha(data) and capture["png_bytes"] == len(data), "PNG hash/size")
        require((capture["png_width"], capture["png_height"]) == (1440, 810), "PNG recorded extent")
        png_pixels(data)
        frames = [capture["requested_frame"], capture["processed_frame"], capture["next_frame"]]
        require(prior_frame < frames[0] <= frames[1] < frames[2], "capture frame ordering")
        prior_frame = frames[2]
        rendered_frame_contract(capture)
        for name, frame in zip(("requested_state", "processed_state", "next_frame_state"), frames):
            state = capture[name]
            state_common(state, manifest)
            require(state["frame"] == frame, "sample frame binding")
            now = finite_number(state["real_time_seconds"], "state wall clock")
            require(now >= prior_time, "state clock monotonic")
            prior_time = now
            if phase != "01_first_mid" or name != "next_frame_state":
                phase_state(state, phase)
    require(tuple(phases) == PHASES, "capture phase order")
    captures = manifest["captures"]
    actions = manifest["actions"]
    first_started = finite_number(captures[1]["processed_state"]["animation_started_at_seconds"],
                                  "first product animation start from rendered midpoint state")
    second_started = finite_number(actions[3]["state_after"]["animation_started_at_seconds"],
                                   "second product animation start")
    require(first <= first_started < first + .16 and second <= second_started < second + .15,
            "real deferred start after Slate click within original deadlines")
    require(.285 <= complete - first_started <= 1.0 and .035 <= cancel - second_started < .15,
            "0.3s product tween and live cancellation measured from real animation start")
    require(captures[0]["next_frame"] < actions[0]["frame"] < actions[1]["frame"]
            < captures[1]["requested_frame"] <= captures[1]["captured_frame"]
            < captures[2]["requested_frame"] and captures[2]["next_frame"]
            < actions[2]["frame"] < actions[3]["frame"] < captures[3]["requested_frame"]
            and captures[3]["next_frame"] < actions[4]["frame"] < captures[4]["requested_frame"]
            and captures[4]["next_frame"] < actions[5]["frame"] < captures[5]["requested_frame"],
            "real Slate action and rendered PNG phase order")
    require(captures[1]["processed_state"]["animation_elapsed"] <= .25,
            "first duplicate and rendered midpoint deadline")
    for index, action in enumerate(manifest["actions"]):
        require(action["label"] == LABELS[index] and action["target"] == TARGETS[index]
                and action["action_id"] == ACTION_IDS[index]
                and action["step"] == ACTION_STEPS[index], "real action routing")
        require(action["method"] == METHOD and action["down_handled"] is True
                and action["up_handled"] is True and action["exact_button_in_path"] is True
                and action["umg_path_present"] is True and action["target_hovered"] is True
                and action["own_game_window"] is True and action["foreground"] is True
                and action["own_window_pid"] == manifest["pid"] and action["stable_frames"] >= 2,
                "genuine foreground Slate click")
        require(action["primary_before"] == action["primary_after"]
                and action["business_before"] == action["business_after"] == manifest["baseline_business"]
                and action["failure"] == "", "click business invariant")
        require("SButton" in action["hit_path_types"][action["exact_button_path_index"]], "SButton path")
        after = action["state_after"]
        state_common(after, manifest, (319, 319, 319, 1, -1, 1)[index])
        if index in (0, 2):
            require(after["animation_active"] is False and after["animation_elapsed"] < 0
                    and after["animated_graphics"]["count"] == 0,
                    "preview queued before real next-tick launch")
        if index == 1:
            duplicate_during_first_contract(manifest["actions"][index], samples=manifest["animation_samples"],
                                            first_started=first_started)
        if index == 3:
            require(after["visual_callback_count"] == 1, "cancel triggered callback")
            require(after["animation_active"] is True or after["screen_open"] is False,
                    "cancel was not initiated during the live row")
            restore = after["graphic_restore"]
            require(restore["expected_count"] > 0
                    and restore["retained_original_count"] == restore["expected_count"]
                    and restore["checked_count"] == restore["expected_count"]
                    and restore["detached_original_count"] == 0,
                    "cancel begins with actual original graphics still in live tree")
        if index == 4:
            require(after["local_fixture"] is False and widget(after, "TaskCompletedAnimationPreview")["exists"] is False,
                    "real normal HUD routing")
    samples = manifest["animation_samples"]
    require(len(samples) >= 2 and len(samples) < 500, "frame sample bounds")
    require(actions[0]["state_after"]["real_time_seconds"] <= first
            and actions[2]["state_after"]["real_time_seconds"] <= second,
            "click wall-clock binding")
    labels = {"first": 0, "second": 0}
    previous = -math.inf
    for sample in samples:
        phase = sample["phase"]
        require(phase in labels and sample["real_time_seconds"] >= previous, "frame sample phase/order")
        previous = sample["real_time_seconds"]
        elapsed = finite_number(sample["elapsed"], "frame elapsed")
        require(0 <= elapsed <= .3 and sample["graphics"], "live graphic sample")
        if phase == "first":
            require(first <= sample["real_time_seconds"] <= complete and sample["callback_count"] == 0,
                    "first live sample after real Slate click before one callback")
        if phase == "second":
            require(second <= sample["real_time_seconds"] <= cancel and sample["callback_count"] == 1,
                    "second live midpoint before real close without callback")
        if .04 <= elapsed <= .25:
            eased = 1 - (1 - elapsed / .3) ** 2
            expected_scale = 1 - .9 * eased
            require(abs(sample["row_scale"]["x"] - expected_scale) <= .08, "OutQuad row scale")
            require(all(x["original"] > 0 and abs(x["current"] - x["original"] * (1 - eased)) <= .08
                        for x in sample["graphics"]), "OutQuad child fade")
            labels[phase] += 1
    require(labels["first"] > 0 and labels["second"] > 0, "both live midpoint streams")
    final_state_binding(manifest["final_state"], manifest["captures"][-1]["next_frame_state"])


def verify_release(stage: Path) -> dict:
    release = json.loads((stage / "release_report.json").read_text(encoding="utf-8"))
    require(release["schema"] == "rok.task_completed_row_animation.runtime_audit_release.v6", "release schema")
    for rel, expected in release["stage_files"].items():
        data = (stage / rel).read_bytes()
        require(sha(data) == expected["sha256"] and len(data) == expected["bytes"], f"stage file {rel}")
    return release


def find_project_root(script_path: Path) -> Path:
    """Find the unique ancestor containing the project marker, independent of script location."""
    matches = [parent for parent in script_path.resolve().parents if (parent / "Rok.uproject").is_file()]
    require(len(matches) == 1, f"expected exactly one Rok.uproject ancestor, found {len(matches)}")
    return matches[0]


def audit_stage_for_script(script_path: Path) -> Path:
    return find_project_root(script_path) / "Saved" / "PendingTaskCompletedRowAnimationRuntimeAuditV6"


def location_fixture() -> None:
    """Exercise the real Saved-stage and Scripts layouts without touching the project Scripts tree."""
    with tempfile.TemporaryDirectory(prefix="task_supplement_locations_") as temp:
        root = Path(temp) / "Rok"
        (root / "Saved" / "PendingTaskCompletedRowAnimationRuntimeAuditV6").mkdir(parents=True)
        (root / "Scripts").mkdir()
        (root / "Rok.uproject").write_text("{}", encoding="utf-8")
        saved_script = root / "Saved" / "PendingTaskCompletedRowAnimationVerifierSupplementV2" / "verifier.py"
        scripts_script = root / "Scripts" / "verifier.py"
        saved_script.parent.mkdir()
        scripts_script.write_text("# path fixture", encoding="utf-8")
        require(audit_stage_for_script(saved_script) == root / "Saved" / "PendingTaskCompletedRowAnimationRuntimeAuditV6",
                "Saved-location stage resolution")
        require(audit_stage_for_script(scripts_script) == root / "Saved" / "PendingTaskCompletedRowAnimationRuntimeAuditV6",
                "Scripts-location stage resolution")


def self_test() -> int:
    # Meaningful contract negatives are synthetic and cannot be supplied as runtime evidence.
    location_fixture()
    stage = audit_stage_for_script(Path(__file__))
    verify_release(stage)
    state = {
        "animation_active": True, "screen_open": True, "local_fixture": True,
        "animation_elapsed": .12, "row_scale": {"x": .42, "y": .42},
        "animated_row_matches": True, "callback_armed": True,
        "graphic_restore": {"expected_count": 1, "checked_count": 1, "mismatched_count": 0},
        "animated_graphics": {"count": 1, "original_count": 1,
                              "entries": [{"valid": True, "original": 1.0, "current": .36}]},
        "visual_callback_count": 0, "selected_task_page": 1,
        "widgets": {"TaskCompletedAnimationPreview": {"visible": True},
                    "TaskCompletedAnimationFixtureNotice": {"text": "本地完成行动画参考 · 不代表任务完成或奖励可领"}},
    }
    phase_state(state, "01_first_mid")
    duplicate_fixture = {"frame": 127, "state_after": {
        "animation_active": True, "visual_callback_count": 0, "animation_elapsed": .02,
        "animation_started_at_seconds": 1.0, "row_scale": {"x": .90, "y": .90}}}
    duplicate_samples = [{"phase": "first", "frame": 127, "elapsed": .02,
                          "row_scale": {"x": .90, "y": .90}}]
    duplicate_during_first_contract(duplicate_fixture, duplicate_samples, first_started=1.0)
    final_fixture = {"frame": 11, "real_time_seconds": 1.02,
                     "business": {"value": 5}, "opacity": [1.0, 1.0],
                     "fixture": {"open": False, "selected": -1}}
    final_state_binding({"frame": 12, "real_time_seconds": 1.035,
                         "business": {"value": 5}, "opacity": [1.0, 1.0],
                         "fixture": {"open": False, "selected": -1}}, final_fixture)
    final_bad_cases = [
        ("final_business_field_drift", lambda f, x: f["business"].update(value=6)),
        ("final_opacity_changed", lambda f, x: f["opacity"].__setitem__(0, .5)),
        ("final_fixture_state_changed", lambda f, x: f["fixture"].update(open=True)),
        ("final_time_not_monotonic", lambda f, x: f.update(real_time_seconds=1.0)),
        ("final_frame_backward", lambda f, x: f.update(frame=10)),
        ("final_frame_wrong", lambda f, x: f.update(frame=13)),
    ]
    for label, mutate in final_bad_cases:
        final_state = {"frame": 12, "real_time_seconds": 1.035,
                       "business": {"value": 5}, "opacity": [1.0, 1.0],
                       "fixture": {"open": False, "selected": -1}}
        last_state = copy.deepcopy(final_fixture)
        mutate(final_state, last_state)
        try:
            final_state_binding(final_state, last_state)
        except (ValueError, KeyError, TypeError):
            continue
        raise ValueError(f"negative accepted: {label}")
    duplicate_bad_cases = [
        ("duplicate_timer_start_changed", lambda a, x: a["state_after"].update(animation_started_at_seconds=1.01)),
        ("duplicate_elapsed_reset", lambda a, x: a["state_after"].update(animation_elapsed=0)),
        ("duplicate_scale_reset", lambda a, x: a["state_after"]["row_scale"].update(x=1.0, y=1.0)),
        ("duplicate_scale_mismatch", lambda a, x: a["state_after"]["row_scale"].update(x=.80, y=.80)),
        ("duplicate_sample_frame_mismatch", lambda a, x: x[0].update(frame=128)),
        ("duplicate_sample_elapsed_mismatch", lambda a, x: x[0].update(elapsed=.12)),
    ]
    for label, mutate in duplicate_bad_cases:
        action = copy.deepcopy(duplicate_fixture)
        sample_rows = copy.deepcopy(duplicate_samples)
        mutate(action, sample_rows)
        try:
            duplicate_during_first_contract(action, sample_rows, first_started=1.0)
        except (ValueError, KeyError, TypeError):
            continue
        raise ValueError(f"negative accepted: {label}")
    mutations = [
        ("inactive", lambda x: x.update(animation_active=False)),
        ("time_before_mid", lambda x: x.update(animation_elapsed=.01)),
        ("time_after_mid", lambda x: x.update(animation_elapsed=.29)),
        ("scale_unmoved", lambda x: x["row_scale"].update(x=1.0)),
        ("scale_anisotropic", lambda x: x["row_scale"].update(y=.8)),
        ("row_lost", lambda x: x.update(animated_row_matches=False)),
        ("callback_unarmed", lambda x: x.update(callback_armed=False)),
        ("graphics_empty", lambda x: x["animated_graphics"].update(count=0)),
        ("opacity_unmoved", lambda x: x["animated_graphics"]["entries"][0].update(current=1.0)),
        ("callback_early", lambda x: x.update(visual_callback_count=1)),
        ("fixture_off", lambda x: x.update(local_fixture=False)),
        ("wrong_page", lambda x: x.update(selected_task_page=2)),
        ("preview_missing", lambda x: x["widgets"]["TaskCompletedAnimationPreview"].update(visible=False)),
        ("notice_forged", lambda x: x["widgets"]["TaskCompletedAnimationFixtureNotice"].update(text="可领奖")),
    ]
    for label, mutate in mutations:
        candidate = copy.deepcopy(state)
        mutate(candidate)
        try:
            phase_state(candidate, "01_first_mid")
        except (ValueError, KeyError, TypeError):
            continue
        raise ValueError(f"negative accepted: {label}")
    normal = copy.deepcopy(state)
    normal.update(animation_active=False, animation_elapsed=-1, local_fixture=False,
                  visual_callback_count=0, task_row_intent="go")
    normal["graphic_restore"].update(retained_original_count=1, detached_original_count=0)
    normal["widgets"].update({"TaskCompletedAnimationPreview": {"exists": False},
                              "TaskCompletedAnimationFixtureNotice": {"exists": False},
                              "TaskAction_0": {"visible": True}})
    phase_state(normal, "04_normal_tasks")
    for label, mutate in (
        ("normal_preview_leak", lambda x: x["widgets"]["TaskCompletedAnimationPreview"].update(exists=True)),
        ("normal_claim", lambda x: x.update(task_row_intent="claim")),
        ("normal_callback", lambda x: x.update(visual_callback_count=1)),
        ("normal_animation", lambda x: x.update(animation_active=True)),
        ("lost_original", lambda x: x["graphic_restore"].update(checked_count=0)),
        ("retained_missing", lambda x: x["graphic_restore"].update(retained_original_count=0)),
        ("normal_rebuild_incorrectly_detached", lambda x: x["graphic_restore"].update(detached_original_count=1)),
        ("original_not_restored", lambda x: x["graphic_restore"].update(mismatched_count=1)),
    ):
        candidate = copy.deepcopy(normal)
        mutate(candidate)
        try:
            phase_state(candidate, "04_normal_tasks")
        except (ValueError, KeyError, TypeError):
            continue
        raise ValueError(f"negative accepted: {label}")
    # Real HUD closure (03/05) must still prove every original graphic detached.
    closed = copy.deepcopy(normal)
    closed.update(screen_open=False, visual_callback_count=1)
    closed["graphic_restore"]["detached_original_count"] = 1
    closed["widgets"]["TaskEntryButton"] = {"visible": True}
    phase_state(closed, "03_hud_after_cancel")
    returned = copy.deepcopy(closed)
    returned["visual_callback_count"] = 0
    phase_state(returned, "05_hud_return")
    for phase, fixture in (("03_hud_after_cancel", closed), ("05_hud_return", returned)):
        candidate = copy.deepcopy(fixture)
        candidate["graphic_restore"]["detached_original_count"] = 0
        try:
            phase_state(candidate, phase)
        except (ValueError, KeyError, TypeError):
            continue
        raise ValueError(f"negative accepted: {phase}_missing_detach")
    timing_contract(.1, .42, .5, .6, .14, .18)
    for label, values in (
        ("early_callback", (.1, .2, .5, .6, .14, .18)),
        ("late_cancel", (.1, .42, .5, .7, .14, .18)),
        ("reversed_order", (.1, .42, .4, .55, .14, .18)),
        ("late_duplicate", (.1, .42, .5, .6, .27, .28)),
        ("late_mid_request", (.1, .42, .5, .6, .14, .32)),
        ("duplicate_after_mid", (.1, .42, .5, .6, .19, .18)),
    ):
        try:
            timing_contract(*values)
        except ValueError:
            continue
        raise ValueError(f"negative accepted: {label}")
    render = {"captured_frame": 20, "processed_frame": 20,
              "processed_state": {"frame": 20, "real_time_seconds": 1.0},
              "captured_real_time_seconds": 1.0}
    rendered_frame_contract(render)
    for label, mutate in (
        ("old_frame", lambda x: x.update(captured_frame=19)),
        ("future_frame", lambda x: x["processed_state"].update(frame=21)),
        ("late_callback_clock", lambda x: x.update(captured_real_time_seconds=1.1)),
    ):
        candidate = copy.deepcopy(render)
        mutate(candidate)
        try:
            rendered_frame_contract(candidate)
        except ValueError:
            continue
        raise ValueError(f"negative accepted: {label}")
    def synthetic_png(mode: str) -> bytes:
        scan = bytearray()
        for y in range(810):
            scan.append(0)
            for x in range(1440):
                if mode == "black":
                    pixel = (0, 0, 0)
                elif mode == "solid":
                    pixel = (200, 40, 80)
                else:
                    pixel = ((x + y) % 256, (x * 3 + y) % 256, (y * 7 + x) % 256)
                scan.extend(pixel)
        def chunk(kind: bytes, payload: bytes) -> bytes:
            return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
        return PNG_SIGNATURE + chunk(b"IHDR", struct.pack(">IIBBBBB", 1440, 810, 8, 2, 0, 0, 0)) \
            + chunk(b"IDAT", zlib.compress(bytes(scan), 6)) + chunk(b"IEND", b"")
    colorful_png = synthetic_png("color")
    require(png_pixels(colorful_png)[:2] == (1440, 810), "positive PNG fixture")
    for label, data in (
        ("old_bad_signature", colorful_png[:7] + b"\r" + colorful_png[8:]),
        ("black_png", synthetic_png("black")),
        ("solid_placeholder", synthetic_png("solid")),
        ("truncated_png", colorful_png[:-8]),
        ("tampered_crc", colorful_png[:-9] + bytes([colorful_png[-9] ^ 1]) + colorful_png[-8:]),
    ):
        try:
            png_pixels(data)
        except (ValueError, zlib.error):
            continue
        raise ValueError(f"negative accepted: {label}")
    with tempfile.TemporaryDirectory(prefix="task_audit_release_", dir=stage) as temp:
        temp_stage = Path(temp)
        release = json.loads((stage / "release_report.json").read_text(encoding="utf-8"))
        (temp_stage / "release_report.json").write_text(json.dumps(release), encoding="utf-8")
        for rel in release["stage_files"]:
            (temp_stage / rel).write_bytes((stage / rel).read_bytes())
        victim = temp_stage / "RokTaskCompletedRowAnimationAuditSubsystem.cpp"
        victim.write_bytes(victim.read_bytes() + b"\n")
        try:
            verify_release(temp_stage)
        except ValueError:
            pass
        else:
            raise ValueError("negative accepted: source SHA drift")
    require(len(mutations) == 14, "first-mid self-test count")
    return 51


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--stage", type=Path, default=audit_stage_for_script(Path(__file__)))
    args = parser.parse_args()
    if args.self_test:
        count = self_test()
        print(f"PASS {count} synthetic negatives; actual runtime pending")
        return 0
    require(args.manifest is not None, "--manifest required")
    release = verify_release(args.stage)
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    verify_manifest(manifest, args.manifest.resolve().parent, release)
    print("PASS genuine task completed-row Slate runtime manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
