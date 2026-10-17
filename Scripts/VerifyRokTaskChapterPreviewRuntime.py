#!/usr/bin/env python3
"""Independently verify the opt-in, real-Slate Task Chapter GameApp audit."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import struct
from pathlib import Path
from rok_data_paths import DATA_ROOT, resolve

ROOT = Path(__file__).resolve().parents[1]
CATALOG = DATA_ROOT / "TaskChapterConfigPreview/v2/catalog.json"
CATALOG_SHA = "3f3848e9f8898668ff2a8ebeddeace1dfc1d557f110cb1339c74882c396b7bb5"
APPROVED_UI = {
    "Source/Rok/RokUnityFeatureScreenWidget.h": "05ce3a6359d6e3d0370631046a81ebc125473b8d6e9be11d8b5ec199d8f21025",
    "Source/Rok/RokUnityFeatureScreenWidget.cpp": "6d3d47414592be00dfe4cc8879e17e4290f372cb6f5950706a9f1d2aaa0cd657",
    "Saved/RokUnityMigration/BatchReview20261009QuestChapterAndWorldHall/task_chapter_ui_implementation_revision3.json": "f8c800e63ea44524fe33ea19b8ac2d5c0ede5b582990741237dcd984fc8d18cd",
}
COMMON_ATLAS = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"
PHASES = [
    ("01_main_default", 1, None),
    ("02_chapter1_default", 0, 1),
    ("03_chapter2", 0, 2),
    ("04_chapter7_top", 0, 7),
    ("05_chapter7_scrolled", 0, 7),
    ("06_chapter1_return", 0, 1),
    ("07_main_return", 1, None),
    ("08_closed", None, None),
    ("09_main_reopened", 1, None),
    ("10_chapter1_reopened", 0, 1),
]
ACTIONS = [
    "open_main_from_hud",
    "open_chapter_tab",
    "select_chapter2",
    "select_chapter7_missing_metadata",
    "scroll_chapter7_source_tasks",
    "return_chapter1",
    "return_main_tab",
    "close_task_screen",
    "reopen_main_from_hud",
    "reopen_chapter_tab",
]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def near(value: float, expected: float, tolerance: float = 0.1) -> bool:
    return math.isfinite(value) and abs(value - expected) <= tolerance


def point(obj: dict) -> tuple[float, float]:
    return float(obj["x"]), float(obj["y"])


def widget(state: dict, name: str) -> dict:
    return state["widgets"][name]


def rect(entry: dict) -> tuple[float, float, float, float]:
    x0, y0 = point(entry["absolute_top_left"])
    x1, y1 = point(entry["absolute_bottom_right"])
    return x0, y0, x1, y1


def png_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    require(header[:8] == b"\x89PNG\r\n\x1a\n" and header[12:16] == b"IHDR", f"bad PNG: {path}")
    return struct.unpack(">II", header[16:24])


def check_catalog() -> dict:
    require(sha(CATALOG) == CATALOG_SHA, "chapter v2 source catalog SHA changed")
    data = json.loads(CATALOG.read_text(encoding="utf-8"))
    check_catalog_shape(data)
    for source in data["sources"]:
        path = resolve(source["path"])
        require(path.is_file() and sha(path) == source["sha256"], f"catalog source drift: {path}")
    return data


def check_catalog_shape(data: dict) -> None:
    require(data["schema"] == "rok.task_chapter_config_preview.v2", "catalog schema")
    require((data["mode"], data["account_state"], data["task_count"], data["chapter_count"])
            == ("source_configuration_preview", "unknown", 39, 7), "catalog boundary/count")
    require(data["missing_chapter_metadata_ids"] == [7], "chapter metadata boundary")
    require([len(row["task_ids"]) for row in data["chapters"]] == [3, 4, 6, 6, 7, 7, 6],
            "chapter task counts")
    tasks = {row["id"]: row for row in data["tasks"]}
    require(len(tasks) == 39, "duplicate task IDs")
    for chapter in data["chapters"]:
        for order, task_id in enumerate(chapter["task_ids"], 1):
            require(tasks[task_id]["chapter_id"] == chapter["id"] and tasks[task_id]["order"] == order,
                    "catalog group/order mismatch")


def self_test() -> dict:
    catalog = check_catalog()
    negative = 0
    for mutate in (
        lambda value: value["chapters"][6]["task_ids"].reverse(),
        lambda value: value["missing_chapter_metadata_ids"].clear(),
        lambda value: value["tasks"][0].__setitem__("chapter_id", 7),
    ):
        value = copy.deepcopy(catalog)
        mutate(value)
        try:
            check_catalog_shape(value)
        except ValueError:
            negative += 1
    require(negative == 3, "source grouping/metadata negative checks did not reject")
    baseline = {"task_id": 200900, "wallet_food": 100,
                "http_dispatch_attempts": 0, "http_suppressed_requests": 0}
    check_business({"business": baseline, "primary_press_count": 0}, baseline, 0)
    for altered in ({"business": {**baseline, "wallet_food": 101}, "primary_press_count": 0},
                    {"business": baseline, "primary_press_count": 1}):
        try:
            check_business(altered, baseline, 0)
        except ValueError:
            negative += 1
    try:
        check_state({"business": baseline, "primary_press_count": 0, "screen_open": False,
                     "chapter_root_attached_to_mode_canvas": False,
                     "rendered_task_ids": [101001], "metadata_known": False},
                    None, None, catalog, baseline, 0)
    except ValueError:
        negative += 1
    require(negative == 6, "business/world/stale-chapter negative checks did not reject")
    return {"status": "passed", "source_tasks": 39, "source_groups": 7,
            "negative_mutations_rejected": negative}


def check_business(state: dict, baseline: dict, primary: int) -> None:
    require(state["business"] == baseline, "account/task/wallet/queue/roster changed")
    require(state["primary_press_count"] == primary, "world primary press changed")
    require(state["business"]["http_dispatch_attempts"] == 0
            and state["business"]["http_suppressed_requests"] == 0, "HTTP boundary changed")


def check_state(state: dict, page: int | None, chapter_id: int | None, catalog: dict,
                baseline: dict, primary: int) -> None:
    check_business(state, baseline, primary)
    if page is None:
        require(state["screen_open"] is False, "closed Task screen remains open")
        require(state["chapter_root_attached_to_mode_canvas"] is False,
                "closed Task screen retained attached chapter root")
        require(state["rendered_task_ids"] == [] and state["metadata_known"] is False,
                "closed screen retained chapter content")
        return
    require(state["screen_open"] is True and state["task_page"] == page, "Task page selection")
    mode = widget(state, "FeatureModeCanvas")
    require(mode["exists"] and mode["visible"] and mode["visibility"] == "SelfHitTestInvisible",
            "design parent not visible/hit-test ready")
    require(near(mode["opacity"], 1.0, .01)
            and all(near(v, 1.0, .01) for v in point(mode["render_scale"])),
            "shared open animation not settled")
    require(all(near(v, e) for v, e in zip(point(mode["local_size"]), (1440, 810))),
            "FeatureModeCanvas design size")
    x0, y0, x1, y1 = rect(mode)
    require(near(x1 - x0, 1440, 1.0) and near(y1 - y0, 810, 1.0),
            "FeatureModeCanvas absolute/DPI size")
    require(state["source_task_count"] == 39 and state["source_group_count"] == 7,
            "source 39/7 count was not rendered")
    if page == 1:
        require(state["chapter_root_attached_to_mode_canvas"] is False,
                "main tab has attached chapter overlay")
        require(state["rendered_task_ids"] == [] and state["metadata_known"] is False,
                "main tab retained chapter content")
        return
    require(chapter_id is not None and state["chapter_id"] == chapter_id,
            "chapter selection mismatch")
    require(state["metadata_known"] is (chapter_id != 7), "chapter7 metadata fabricated")
    rows = catalog["chapters"][chapter_id - 1]["task_ids"]
    require(state["rendered_task_ids"] == rows, "chapter source IDs/order mismatch")
    root = widget(state, "TaskChapterRoot")
    scroll = widget(state, "TaskChapterScroll")
    require(root["visible"] and scroll["visible"] and scroll["enabled"],
            "chapter/scroll not visible")
    require(state["chapter_root_attached_to_mode_canvas"] is True,
            "chapter root not attached to current design canvas")
    require(root["parent"] == "FeatureModeCanvas", "chapter root hierarchy")
    for group_id in range(1, 8):
        button = widget(state, f"TaskChapterGroupButton_{group_id}")
        require(button["exists"] and button["visible"] and button["enabled"]
                and button["parent"] == "TaskChapterRoot", f"chapter group {group_id} button")
        image = widget(state, f"TaskChapterGroupButton_{group_id}Background_Center")
        require(image["exists"] and image["visible"] and image["brush_resource"] == COMMON_ATLAS
                and all(near(value, expected) for value, expected in
                        zip(point(image["texture_size"]), (2048, 1024))),
                f"chapter group {group_id} source atlas cold readiness")
    require(scroll["parent"] == "TaskChapterRoot"
            and all(near(value, expected, .2) for value, expected in
                    zip(point(scroll["local_size"]), (737, 468.5))),
            "source chapter viewport hierarchy/size")
    require(widget(state, "TaskChapterGroupTitle")["visible"]
            and widget(state, "TaskChapterMetaNotice")["visible"], "chapter title/boundary absent")
    expected_title = (catalog["chapters"][chapter_id - 1]["title"]["zh_cn"]
                      if chapter_id < 7 else "第7组 · 章节元数据缺失")
    require(widget(state, "TaskChapterGroupTitle").get("text") == expected_title,
            "chapter source/fallback title")
    require("未同步" in widget(state, "TaskChapterMetaNotice").get("text", ""),
            "account chapter progress boundary absent")
    if chapter_id == 7:
        require("未" in widget(state, "TaskChapterMetaNotice").get("text", ""),
                "chapter7 metadata unknown notice absent")
    for task_id in rows:
        row = widget(state, f"TaskChapterTaskRow_{task_id}")
        title = widget(state, f"TaskChapterTaskTitle_{task_id}")
        desc = widget(state, f"TaskChapterTaskDescription_{task_id}")
        require(row["exists"] and title["exists"] and desc["exists"],
                f"missing task row {task_id}")
        require(title["parent"] == f"TaskChapterTaskRow_{task_id}"
                and desc["parent"] == f"TaskChapterTaskRow_{task_id}"
                and all(near(value, expected, .2) for value, expected in
                        zip(point(row["local_size"]), (740, 104))),
                f"task {task_id} source row hierarchy/size")
        source = next(task for task in catalog["tasks"] if task["id"] == task_id)
        require(title.get("text") == source["title"]["zh_cn"], f"task {task_id} title")
        require(bool(desc.get("text")), f"task {task_id} description absent")
        require(not widget(state, f"TaskChapterTaskClaim_{task_id}")["exists"]
                and not widget(state, f"TaskChapterTaskGo_{task_id}")["exists"],
                "unexpected account action")


def check_inputs(manifest: dict) -> None:
    inputs = manifest["inputs_sha256"]
    required = [
        "Source/Rok/RokTaskChapterPreviewAuditSubsystem.h",
        "Source/Rok/RokTaskChapterPreviewAuditSubsystem.cpp",
        "Scripts/VerifyRokTaskChapterPreviewRuntime.py",
        "Source/Rok/RokTaskChapterConfigPreviewData.h",
        "Source/Rok/RokTaskChapterConfigPreviewData.cpp",
        "Source/Rok/RokUnityFeatureScreenWidget.h",
        "Source/Rok/RokUnityFeatureScreenWidget.cpp",
        "Saved/RokUnityMigration/TaskChapterConfigPreview/v2/catalog.json",
    ]
    require(all(key in inputs for key in required), "runtime source pin list incomplete")
    for name, digest in inputs.items():
        path = resolve(name)
        require(path.is_file() and sha(path) == digest, f"runtime source pin mismatch: {name}")
    for name, digest in APPROVED_UI.items():
        require(inputs.get(name) == digest, f"approved chapter UI source changed: {name}")
    require(inputs["Saved/RokUnityMigration/TaskChapterConfigPreview/v2/catalog.json"] == CATALOG_SHA,
            "approved v2 chapter catalog changed")


def verify(manifest_path: Path) -> dict:
    catalog = check_catalog()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(manifest["schema"] == "rok.task_chapter.runtime_audit.v1", "runtime schema")
    require(manifest["complete"] is True and manifest["passed"] is True and not manifest["failures"],
            "runtime C++ audit did not pass")
    require(Path(manifest["project"]).resolve() == ROOT.resolve()
            and manifest["world"] == "/Game/Maps/L_RokUnityGameAppCity", "wrong project/world")
    require(manifest["world_time_advanced_by_audit"] is False, "audit advanced world time")
    check_inputs(manifest)
    actions = manifest["actions"]
    captures = manifest["captures"]
    require([row["label"] for row in actions] == ACTIONS, "real Slate action order/count")
    require([row["phase"] for row in captures] == [phase for phase, _, _ in PHASES],
            "capture phase order/count")
    baseline = captures[0]["requested_state"]["business"]
    primary = captures[0]["requested_state"]["primary_press_count"]
    require(baseline["profile_kind"] == "local_preview" and baseline["role_id"] == ""
            and baseline["civilization_id"] == 0 and baseline["task_id"] == 200900,
            "runtime did not start from local-only source task")
    for index, action in enumerate(actions):
        require(action["frame"] > 0 and action["own_game_window"] is True
                and action["game_window_foreground"] is True,
                f"action {index} foreign/invalid frame")
        require(action["frame"] <= captures[index]["requested_frame"],
                f"action {index} happened after its capture")
        if index:
            require(action["frame"] > captures[index - 1]["next_frame_state"]["frame"],
                    f"action {index} replayed on prior capture frame")
        require(action["method"].startswith("Slate ProcessMouse"), f"action {index} bypassed Slate")
        require(action["primary_before"] == primary and action["primary_after"] == primary,
                f"action {index} world press") if index != 4 else None
        if index == 4:
            require(action["scroll_hit_path"] is True and action["handled"] is True
                    and action["offset_before"] <= 1
                    and any("SScrollBox" in value for value in action["hit_path_types"]),
                    "chapter7 wheel path")
        else:
            require(action["target_in_hit_path"] and action["target_hovered"]
                    and action["terminal_type"].endswith("SButton")
                    and action["down_handled"] and action["up_handled"],
                    f"action {index} not own-window handled UMG button")
            require(any("SObjectWidget" in value for value in action["hit_path_types"]),
                    f"action {index} not UMG")
    for index, ((phase, page, chapter), capture) in enumerate(zip(PHASES, captures)):
        png = Path(capture["png"])
        require(png.is_file() and png.parent.resolve() == manifest_path.parent.resolve()
                and sha(png) == capture["png_sha256"] and png.stat().st_size == capture["png_bytes"]
                and png_size(png) == (1440, 810), f"capture {phase} PNG/hash/dimensions")
        require(capture["requested_frame"] <= capture["processed_frame"]
                < capture["next_frame_state"]["frame"], f"capture {phase} natural frame order")
        for when in ("requested_state", "processed_state", "next_frame_state"):
            check_state(capture[when], page, chapter, catalog, baseline, primary)
        if phase == "05_chapter7_scrolled":
            state = capture["processed_state"]
            require(state["scroll_offset"] > 0 and state["scroll_offset"] <= state["scroll_end"] + 1,
                    "chapter7 genuine scroll offset")
            row = widget(state, "TaskChapterTaskRow_107006")
            scroll = widget(state, "TaskChapterScroll")
            _, row_y0, _, row_y1 = rect(row)
            _, scroll_y0, _, scroll_y1 = rect(scroll)
            require(row_y0 < scroll_y1 and row_y1 > scroll_y0,
                    "last source task not visible in scrolled viewport")
    check_state(manifest["final_state"], 0, 1, catalog, baseline, primary)
    return {"status": "passed", "manifest": str(manifest_path), "captures": len(captures),
            "button_actions": 9, "slate_wheel_actions": 1, "source_tasks": 39, "source_groups": 7,
            "boundary": "read-only configuration; account chapter progress and rewards unknown"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path, nargs="?")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        require(args.self_test or args.manifest is not None, "manifest path required")
        report = self_test() if args.self_test else verify(args.manifest.resolve())
    except (ValueError, KeyError, TypeError, OSError, json.JSONDecodeError) as error:
        report = {"status": "failed", "manifest": str(args.manifest), "error": str(error)}
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
