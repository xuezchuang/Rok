#!/usr/bin/env python3
"""Independently verify the opt-in, real-Slate Task Chapter Reward GameApp audit."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "Saved/RokUnityMigration/TaskChapterConfigPreview/v2/catalog.json"
CATALOG_SHA = "3f3848e9f8898668ff2a8ebeddeace1dfc1d557f110cb1339c74882c396b7bb5"
REWARD_CATALOG = ROOT / "Saved/RokUnityMigration/TaskChapterRewardPreview/v1/catalog.json"
REWARD_CATALOG_SHA = "eab124c2aea9cc265dd2a185c7383f7ab5df02493303bba6dcd18c7c42041d4e"
CAPTURED_VERIFIER_SHA = "d7c72f26eed75963c4f29e4c65af849c15be3dc7a366df6a88210ba518b1f0fb"
CAPTURED_VERIFIER_SNAPSHOT = ROOT / "Saved/RokUnityMigration/BatchReview20261009QuestChapterArtAndHallCamera/VerifyRokTaskChapterRewardRuntime_revision2_snapshot.py"
APPROVED_PINS = {
    "Source/Rok/RokUnityFeatureScreenWidget.h": "78bd1d8dc3367c2303a22edccf3aaa5c1d3fbe013b610e741f29eea87c0f42dd",
    "Source/Rok/RokUnityFeatureScreenWidget.cpp": "b238e5068e2baf1fe16ad529a06b46ab0d4b24ab5131e57862df6bc93341e480",
    "Saved/RokUnityMigration/BatchReview20261009QuestChapterArtAndHallCamera/task_chapter_reward_ui_implementation_revision4.json": "22afc908086e903e4864cb8950c0f6b9a2a532a125a972425b8803b3f0670acb",
    "Saved/RokUnityMigration/BatchReview20261009QuestChapterArtAndHallCamera/task_chapter_reward_data_implementation_v1.json": "5d6a1eb3330823358a51653ef7111d83b754c2b069c31d837c7fb7be0a6e3031",
    "Saved/RokUnityMigration/BatchReview20261009QuestChapterAndWorldHall/parallel_migration_batch_20261009T041001199603Z_quest_chapter_hall.json": "a229232900c6d27ef0b616886d94c897a8b151696f503acb8518dd3b4a893e88",
}
COMMON_ATLAS = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"
PHASES = [
    ("01_main_default", 1, None),
    ("02_chapter1_default", 0, 1),
    ("03_chapter2", 0, 2),
    ("04_chapter3_top", 0, 3),
    ("05_chapter3_scrolled", 0, 3),
    ("06_chapter4_top", 0, 4),
    ("07_chapter4_scrolled", 0, 4),
    ("08_chapter5_top", 0, 5),
    ("09_chapter5_scrolled", 0, 5),
    ("10_chapter6_top", 0, 6),
    ("11_chapter6_scrolled", 0, 6),
    ("12_chapter7_top", 0, 7),
    ("13_chapter7_scrolled", 0, 7),
    ("14_chapter1_return", 0, 1),
    ("15_main_return", 1, None),
    ("16_closed", None, None),
    ("17_main_reopened", 1, None),
    ("18_chapter1_reopened", 0, 1),
]
ACTIONS = [
    "open_main_from_hud",
    "open_chapter_tab",
    "select_chapter2",
    "select_chapter3",
    "scroll_chapter3_source_rewards",
    "select_chapter4",
    "scroll_chapter4_source_rewards",
    "select_chapter5",
    "scroll_chapter5_source_rewards",
    "select_chapter6",
    "scroll_chapter6_source_rewards",
    "select_chapter7_missing_metadata",
    "scroll_chapter7_source_rewards",
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


def exact_quantity(text: str, source: int) -> bool:
    return text == f"{source:,}"


def check_catalog() -> dict:
    require(sha(CATALOG) == CATALOG_SHA, "chapter v2 source catalog SHA changed")
    data = json.loads(CATALOG.read_text(encoding="utf-8"))
    check_catalog_shape(data)
    for source in data["sources"]:
        path = ROOT / source["path"]
        require(path.is_file() and sha(path) == source["sha256"], f"catalog source drift: {path}")
    return data


def check_reward_catalog() -> dict:
    require(sha(REWARD_CATALOG) == REWARD_CATALOG_SHA, "reward catalog source pin changed")
    data = json.loads(REWARD_CATALOG.read_text(encoding="utf-8"))
    require(data["schema"] == "rok.task_chapter_reward_preview.v1"
            and (data["task_group_count"], data["chapter_aggregate_group_count"],
                 data["group_count"], data["reward_row_count"]) == (39, 6, 45, 123),
            "reward catalog schema/count")
    require(len(data["groups"]) == 45 and len(data["chapters"]) == 7
            and data["chapters"][6]["metadata_known"] is False
            and data["chapters"][6]["reward_group_id"] is None,
            "chapter 7 aggregate boundary")
    require(sum(len(group["entries"]) for group in data["groups"]) == 123,
            "reward row count")
    require(len({row["icon_key"] for group in data["groups"] for row in group["entries"]}) == 19,
            "19 source reward icons")
    require(len({row["atlas_object_path"] for group in data["groups"] for row in group["entries"]}) == 5,
            "five source reward atlases")
    for source in data["sources"]:
        path = ROOT / source["path"]
        require(path.is_file() and sha(path) == source["sha256"],
                f"reward source drift: {source['path']}")
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
    rewards = check_reward_catalog()
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
                    None, None, catalog, baseline, 0, rewards)
    except ValueError:
        negative += 1
    require(negative == 6, "business/world/stale-chapter negative checks did not reject")
    require(exact_quantity("5,000", 5000) and not exact_quantity("5000", 5000)
            and not exact_quantity("5,001", 5000), "source N0 reward quantity check")
    negative += 2
    return {"status": "passed", "source_tasks": 39, "source_groups": 7,
            "reward_groups": len(rewards["groups"]), "reward_rows": rewards["reward_row_count"],
            "negative_mutations_rejected": negative}


def check_business(state: dict, baseline: dict, primary: int) -> None:
    require(state["business"] == baseline, "account/task/wallet/queue/roster changed")
    require(state["primary_press_count"] == primary, "world primary press changed")
    require(state["business"]["http_dispatch_attempts"] == 0
            and state["business"]["http_suppressed_requests"] == 0, "HTTP boundary changed")


def check_state(state: dict, page: int | None, chapter_id: int | None, catalog: dict,
                baseline: dict, primary: int, rewards: dict,
                visible_geometry_ids: set[int] | None = None,
                check_aggregate_text_bounds: bool = False) -> None:
    check_business(state, baseline, primary)
    if page is None:
        require(state["screen_open"] is False, "closed Task screen remains open")
        require(state["chapter_root_attached_to_mode_canvas"] is False,
                "closed Task screen retained attached chapter root")
        require(state["rendered_task_ids"] == [] and state["metadata_known"] is False
                and state["rendered_task_reward_package_ids"] == []
                and state["rendered_aggregate_reward_package_ids"] == [],
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
        require(state["rendered_task_ids"] == [] and state["metadata_known"] is False
                and state["rendered_task_reward_package_ids"] == []
                and state["rendered_aggregate_reward_package_ids"] == [],
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
    chapter_groups = [group for group in rewards["groups"] if group["chapter_id"] == chapter_id]
    task_ids = [row["package_row_id"] for group in chapter_groups if group["owner_kind"] == "task"
                for row in group["entries"]]
    aggregate_ids = [row["package_row_id"] for group in chapter_groups if group["owner_kind"] == "chapter"
                     for row in group["entries"]]
    require(state["rendered_task_reward_package_ids"] == task_ids,
            f"chapter {chapter_id} task reward IDs/order")
    require(state["rendered_aggregate_reward_package_ids"] == aggregate_ids,
            f"chapter {chapter_id} aggregate reward IDs/order")
    missing = widget(state, "TaskChapterAggregateRewardUnavailable")
    require((missing["exists"] and missing["visible"] and not aggregate_ids)
            if chapter_id == 7 else (not missing["exists"] and bool(aggregate_ids)),
            "chapter aggregate metadata/absence boundary")
    for group in chapter_groups:
        for index, source in enumerate(group["entries"]):
            base = (f"TaskChapterAggregateReward_{chapter_id}_{index}" if group["owner_kind"] == "chapter"
                    else f"TaskChapterTaskReward_{group['task_id']}_{index}")
            cell = widget(state, base)
            icon = widget(state, base + "Icon")
            quantity = widget(state, base + "Quantity")
            fit = widget(state, base + "QuantityFit") if group["owner_kind"] == "chapter" else None
            require(cell["exists"] and icon["exists"] and quantity["exists"]
                    and icon["parent"] == base
                    and quantity["parent"] == (base + "QuantityFit" if fit is not None else base),
                    f"reward cell hierarchy {base}")
            expected_cell = (56.28, 56.28) if group["owner_kind"] == "chapter" else (104, 40)
            expected_icon = (50.4, 50.4) if group["owner_kind"] == "chapter" else (33.6, 33.6)
            expected_parent = "TaskChapterRoot" if group["owner_kind"] == "chapter" else f"TaskChapterTaskRow_{group['task_id']}"
            require(cell["parent"] == expected_parent
                    and all(near(v, e, 0.2) for v, e in zip(point(cell["local_size"]), expected_cell))
                    and all(near(v, e, 0.2) for v, e in zip(point(icon["local_size"]), expected_icon)),
                    f"reward source cell geometry {base}")
            parent = widget(state, expected_parent)
            cell_x, cell_y = point(cell["absolute_top_left"])
            parent_x, parent_y = point(parent["absolute_top_left"])
            expected_x = (1040 - (len(group["entries"]) - 1 - index) * 68.88 - 56.28
                          if group["owner_kind"] == "chapter" else 733 - (index + 1) * 104)
            expected_y = 96 if group["owner_kind"] == "chapter" else 44
            row_in_viewport = True
            if group["owner_kind"] == "task":
                rx0, ry0, rx1, ry1 = rect(parent)
                vx0, vy0, vx1, vy1 = rect(scroll)
                row_in_viewport = rx0 < vx1 and rx1 > vx0 and ry0 < vy1 and ry1 > vy0
            if row_in_viewport:
                require(near(cell_x - parent_x, expected_x, 1.0)
                        and near(cell_y - parent_y, expected_y, 1.0),
                        f"visible reward source cell order/placement {base}")
                if (visible_geometry_ids is not None and group["owner_kind"] == "task"
                        and index == len(group["entries"]) - 1):
                    visible_geometry_ids.add(group["task_id"])
            if group["owner_kind"] == "chapter":
                icon_x, icon_y = point(icon["absolute_top_left"])
                fit_x, fit_y = point(fit["absolute_top_left"])
                require(fit["exists"] and fit["parent"] == base
                        and fit["scale_to_fit"] is True and fit["down_only"] is True
                        and quantity["fit_slot_right_aligned"] is True
                        and quantity["fit_slot_top_aligned"] is True
                        and all(near(v, e, 0.2) for v, e in zip(point(fit["local_size"]), (48.13, 12.6)))
                        and near(icon_x - cell_x, 2.94, 0.5) and near(icon_y - cell_y, 2.94, 0.5)
                        and near(fit_x - cell_x, 2.9, 0.5)
                        and near(fit_y - cell_y, 41.2, 0.5),
                        f"chapter aggregate source overlay geometry {base}")
                if check_aggregate_text_bounds:
                    desired_width, desired_height = point(quantity["text_slate_desired_size"])
                    text_x0, text_y0 = point(quantity["text_desired_absolute_top_left"])
                    text_x1, text_y1 = point(quantity["text_desired_absolute_bottom_right"])
                    _, _, cell_x1, cell_y1 = rect(cell)
                    _, _, fit_x1, fit_y1 = rect(fit)
                    rendered_width, rendered_height = text_x1 - text_x0, text_y1 - text_y0
                    require(quantity["text_desired_alignment_basis"] == "slate_cached_geometry_scaled"
                            and desired_width > 0 and desired_height > 0
                            and rendered_width > 0 and rendered_height > 0
                            and rendered_width <= desired_width + 0.5
                            and rendered_height <= desired_height + 0.5
                            and text_x0 >= max(cell_x, fit_x) - 0.5
                            and text_y0 >= max(cell_y, fit_y) - 0.5
                            and text_x1 <= min(cell_x1, fit_x1) + 0.5
                            and text_y1 <= min(cell_y1, fit_y1) + 0.5
                            and near(text_x1, fit_x1, 0.5)
                            and near(text_y0, fit_y, 0.5),
                            f"chapter aggregate text clipped/outside source cell {base}")
                    if index + 1 < len(group["entries"]):
                        next_icon = widget(state,
                                           f"TaskChapterAggregateReward_{chapter_id}_{index + 1}Icon")
                        next_icon_left, _ = point(next_icon["absolute_top_left"])
                        require(text_x1 + 1.0 <= next_icon_left,
                                f"chapter aggregate text overlaps adjacent icon {base}")
            require(icon["brush_resource"] == source["atlas_object_path"]
                    and icon["rhi_ready"] is True,
                    f"reward atlas/RHI {base}")
            width, height = source["icon_atlas_size"]
            require(all(near(v, e, 0.1) for v, e in zip(point(icon["texture_size"]), (width, height)))
                    and all(near(v, e, 0.1) for v, e in zip(point(icon["rhi_extent"]), (width, height))),
                    f"reward real atlas extent {base}")
            x, y, w, h = source["icon_rect"]
            top = height - y - h
            require(all(near(v, e, 0.00001) for v, e in zip(point(icon["uv_min"]), (x / width, top / height)))
                    and all(near(v, e, 0.00001) for v, e in zip(point(icon["uv_max"]),
                                                                    ((x + w) / width, (top + h) / height))),
                    f"reward UV/source rect {base}")
            require(exact_quantity(quantity.get("text", ""), source["quantity"]),
                    f"reward exact source quantity {base}")


def check_inputs(manifest: dict) -> None:
    inputs = manifest["inputs_sha256"]
    required = [
        "Source/Rok/RokTaskChapterRewardAuditSubsystem.h",
        "Source/Rok/RokTaskChapterRewardAuditSubsystem.cpp",
        "Scripts/VerifyRokTaskChapterRewardRuntime.py",
        "Source/Rok/RokTaskChapterConfigPreviewData.h",
        "Source/Rok/RokTaskChapterConfigPreviewData.cpp",
        "Source/Rok/RokTaskChapterRewardPreviewData.h",
        "Source/Rok/RokTaskChapterRewardPreviewData.cpp",
        "Source/Rok/RokUnityFeatureScreenWidget.h",
        "Source/Rok/RokUnityFeatureScreenWidget.cpp",
        "Saved/RokUnityMigration/TaskChapterConfigPreview/v2/catalog.json",
        "Saved/RokUnityMigration/TaskChapterRewardPreview/v1/catalog.json",
    ]
    require(all(key in inputs for key in required), "runtime source pin list incomplete")
    for name, digest in inputs.items():
        path = ROOT / name
        if name == "Scripts/VerifyRokTaskChapterRewardRuntime.py" and digest == CAPTURED_VERIFIER_SHA:
            require(CAPTURED_VERIFIER_SNAPSHOT.is_file()
                    and sha(CAPTURED_VERIFIER_SNAPSHOT) == digest,
                    "captured verifier revision2 snapshot changed")
            continue
        require(path.is_file() and sha(path) == digest, f"runtime source pin mismatch: {name}")
    require(inputs["Saved/RokUnityMigration/TaskChapterConfigPreview/v2/catalog.json"] == CATALOG_SHA,
            "approved v2 chapter catalog changed")
    require(inputs["Saved/RokUnityMigration/TaskChapterRewardPreview/v1/catalog.json"] == REWARD_CATALOG_SHA,
            "runtime reward catalog pin changed")
    for name, digest in APPROVED_PINS.items():
        require(inputs.get(name) == digest, f"approved source pin changed: {name}")


def verify(manifest_path: Path) -> dict:
    catalog = check_catalog()
    rewards = check_reward_catalog()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(manifest["schema"] == "rok.task_chapter_reward.runtime_audit.v1", "runtime schema")
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
    visible_geometry_ids: set[int] = set()
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
                f"action {index} world press")
        if index in (4, 6, 8, 10, 12):
            require(action["scroll_hit_path"] is True and action["handled"] is True
                    and action["chapter"] == (index // 2 + 1)
                    and action["offset_before"] <= 1
                    and any("SScrollBox" in value for value in action["hit_path_types"]),
                    "chapter wheel path")
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
            check_state(capture[when], page, chapter, catalog, baseline, primary, rewards,
                        visible_geometry_ids if when == "processed_state" else None,
                        check_aggregate_text_bounds=when == "processed_state")
        if phase.endswith("_scrolled"):
            state = capture["processed_state"]
            require(state["scroll_offset"] > 0 and state["scroll_offset"] <= state["scroll_end"] + 1,
                    "chapter genuine scroll offset")
            last_id = catalog["chapters"][chapter - 1]["task_ids"][-1]
            row = widget(state, f"TaskChapterTaskRow_{last_id}")
            scroll = widget(state, "TaskChapterScroll")
            _, row_y0, _, row_y1 = rect(row)
            _, scroll_y0, _, scroll_y1 = rect(scroll)
            require(row_y0 < scroll_y1 and row_y1 > scroll_y0,
                    "last source task not visible in scrolled viewport")
    check_state(manifest["final_state"], 0, 1, catalog, baseline, primary, rewards)
    require(visible_geometry_ids == {task["id"] for task in catalog["tasks"]},
            "all 39 task reward row positions require a visible processed-frame check")
    return {"status": "passed", "manifest": str(manifest_path), "captures": len(captures),
            "button_actions": 13, "slate_wheel_actions": 5, "source_tasks": 39,
            "reward_groups": 45, "reward_rows": 123,
            "visible_reward_geometry_tasks": len(visible_geometry_ids),
            "boundary": "read-only configuration; account chapter progress, ownership and claims unknown"}


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
