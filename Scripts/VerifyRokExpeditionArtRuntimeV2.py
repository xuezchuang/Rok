"""Verify a real, local-only Expedition source-art GameViewport audit run."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parents[1]
CATALOG = ROOT / "Saved/RokUnityMigration/expedition_stage_preview.json"
ART_CATALOG = ROOT / "Saved/RokUnityMigration/ExpeditionVisualAssets/v1/atlas_manifest.json"
RESULT_CATALOG = ROOT / "Saved/RokUnityMigration/ExpeditionVisualAssets/v2/result_visual_manifest.json"
NATIVE_CURVES = ROOT / ("Saved/UnityExpeditionAnimationProbeV1/OutputRuns/"
                        "20261009_013025_916_42504/native_animation_curves.json")
IMPORT_REPORT = ROOT / ("Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/"
                        "expedition_visual_ue_import_20261009_015909_339908.json")
RESULT_IMPORT_REPORT = ROOT / ("Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/"
                               "expedition_result_ue_import_20261009_020448_951277.json")
UI_REPORT = ROOT / ("Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/"
                    "expedition_art_ui_implementation.json")
UI_REVISION = ROOT / ("Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/"
                      "expedition_art_ui_implementation_revision2.json")
UI_REVISION3 = ROOT / ("Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/"
                       "expedition_art_ui_implementation_revision3.json")
SOURCE_REVIEW = ROOT / ("Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/"
                        "expedition_art_source_oracle_review.json")
PHASES = [
    "01_map_stage1_open", "02_map_stage1_selected",
    "03_task_stage1_start", "04_task_stage1_mid", "05_task_stage1_end",
    "06_preview_stage1_start", "07_preview_stage1_mid", "08_preview_stage1_end",
    "09_normal_stage1_start", "10_normal_stage1_mid", "11_normal_stage1_end",
    "12_map_stage1_return", "13_map_stage2_selected",
    "14_task_stage2_start", "15_task_stage2_mid", "16_task_stage2_end",
    "17_preview_stage2_end", "18_normal_stage2_end",
    "19_challenge_boundary", "20_map_stage2_return",
    "21_closed", "22_reopened_map_stage1",
]
ACTIONS = [
    "open_from_hud", "select_source_stage1", "open_task_config_stage1",
    "preview_stage1", "back_to_normal_stage1", "back_to_map_stage1",
    "select_source_stage2", "open_task_config_stage2", "preview_stage2",
    "back_to_normal_stage2", "challenge_source_boundary", "back_to_map_stage2",
    "close_expedition", "reopen_from_hud",
]
PHASE_CONTRACT = [
    (1, False, False, "Idle"), (1, False, False, "Idle"),
    (1, True, False, "NormalTransition"),
    (1, True, False, "NormalTransition"), (1, True, False, "Idle"),
    (1, True, True, "PreviewTransition"),
    (1, True, True, "PreviewTransition"), (1, True, True, "Idle"),
    (1, True, False, "NormalTransition"),
    (1, True, False, "NormalTransition"), (1, True, False, "Idle"),
    (1, False, False, "Idle"), (2, False, False, "Idle"),
    (2, True, False, "NormalTransition"),
    (2, True, False, "NormalTransition"), (2, True, False, "Idle"),
    (2, True, True, "Idle"), (2, True, False, "Idle"),
    (2, True, False, "Idle"), (2, False, False, "Idle"),
    (2, False, False, "Idle"), (1, False, False, "Idle"),
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


def check_design_frame(screen_root: dict, mode_canvas: dict) -> None:
    """The outer UserWidget uses DPI-local units; ModeCanvas owns 1440x810."""
    need(screen_root["exists"] is True and mode_canvas["exists"] is True
         and mode_canvas["parent_widget_name"] == "FeatureExpandedDesignCanvas",
         "Expedition design-coordinate parent hierarchy")
    for name, node in (("screen", screen_root), ("mode", mode_canvas)):
        values = [node["local_size"][axis] for axis in ("x", "y")]
        values += [node[key][axis] for key in ("absolute_top_left", "absolute_bottom_right")
                   for axis in ("x", "y")]
        need(all(type(value) in (int, float) and math.isfinite(value) for value in values),
             f"Nonfinite {name} canvas geometry")
        need(node["local_size"]["x"] > 0 and node["local_size"]["y"] > 0,
             f"Empty {name} canvas geometry")
    for axis, design in (("x", 1440), ("y", 810)):
        need(abs(mode_canvas["local_size"][axis] - design) <= 0.01,
             f"FeatureModeCanvas design size: {axis}")
        for edge in ("absolute_top_left", "absolute_bottom_right"):
            need(abs(screen_root[edge][axis] - mode_canvas[edge][axis]) <= 1.5,
                 f"DPI outer root and design canvas absolute {edge}/{axis}")
        extent = (mode_canvas["absolute_bottom_right"][axis]
                  - mode_canvas["absolute_top_left"][axis])
        need(abs(extent - design) <= 1.5,
             f"FeatureModeCanvas absolute viewport extent: {axis}")
    root_x = ((screen_root["absolute_bottom_right"]["x"]
               - screen_root["absolute_top_left"]["x"]) / screen_root["local_size"]["x"])
    root_y = ((screen_root["absolute_bottom_right"]["y"]
               - screen_root["absolute_top_left"]["y"]) / screen_root["local_size"]["y"])
    need(0.5 <= root_x <= 2 and abs(root_x - root_y) <= 0.001,
         "Outer UserWidget DPI scale is inconsistent")


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
                curves: dict, sample: str = "requested_state") -> None:
    check_business(state["business"], baseline)
    need(state["controller_primary_press_count"] == primary_count,
         f"Delayed world input after Slate click: {index}")
    need(state["pending_action_id"] == -1,
         f"Queued action remained during source capture: {index}")
    stage, task, preview, phase = PHASE_CONTRACT[index]
    is_closed = index == 20
    need(state["screen_open"] is not is_closed, f"Screen open state: {index}")
    if is_closed:
        return
    need(state["expedition_mode"] is True and state["rendered_stage_count"] == 80,
         f"Expedition data/render mode: {index}")
    need(state["task_config_visible"] is task
         and state["selected_stage_id"] == stage and state["preview_mode"] is preview
         and state["animation_phase"] == phase, f"Stage/mode/animation: {index}")
    elapsed = state["animation_elapsed"]
    need(type(elapsed) in (int, float) and math.isfinite(elapsed),
         f"Nonfinite animation clock: {index}")
    need(elapsed >= 0, f"Negative animation elapsed: {index}")
    if phase in ("PreviewTransition", "NormalTransition"):
        clip = "center_hide_scale" if phase == "PreviewTransition" else "center_show_scale"
        need(elapsed <= curves[clip][-1][0] + 0.01,
             f"Transition sample exceeded source clip: {index}/{sample}")
    if sample in ("requested_state", "processed_state"):
        if index in (2, 5, 8, 13):
            need(elapsed < 0.12, f"Transition first PNG frame: {index}/{sample}")
        elif index == 6:
            need(0.12 <= elapsed <= 0.34, "Preview PNG mid-frame outside source clip")
        elif index in (3, 9, 14):
            need(0.25 <= elapsed <= 0.59, "Normal PNG mid-frame outside source clip")
    for name in ("ExpeditionStageButton_1", "ExpeditionStageButton_2",
                 "ExpeditionMapScroll"):
        need(state["widgets"][name]["exists"] is not task,
             f"Map control leaked across full-screen Task layer: {name}/{index}")
    if not task:
        config_entry = state["widgets"]["ExpeditionTaskConfigOpenButton"]
        need(config_entry["exists"] is True and config_entry["visible"] is True
             and config_entry["enabled"] is True,
             f"Map source config entry missing: {index}")
        for name in ("ExpeditionStageButton_1", "ExpeditionStageButton_2"):
            item = widget(state, name)
            need(item["visible"] is True and item["enabled"] is True
                 and item["local_size"]["x"] == 126
                 and item["local_size"]["y"] == 100,
                 f"Source map stage hit area unavailable: {name}/{index}")
        return
    need(state["widgets"]["ExpeditionTaskConfigOpenButton"]["exists"] is False,
         f"Map config entry leaked behind full-screen Task: {index}")
    for name in ("ExpeditionStageButton_1", "ExpeditionStageButton_2",
                 "ExpeditionStageTitle_1", "ExpeditionStageTitle_2"):
        need(state["widgets"][name]["exists"] is False,
             f"Map stage control present behind full-screen Task: {name}/{index}")
    for name in ("ExpeditionPreviewRoot", "ExpeditionSideRoot",
                 "ExpeditionCenterRoot", "ExpeditionBackButton"):
        w = widget(state, name)
        size = w["local_size"]
        need(not w["visible"] or (size["x"] > 0 and size["y"] > 0),
             f"Visible source panel has no cached geometry: {name}")
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
    # The V2 CenterShow image/flag tracks live on child panels. The parent
    # keeps only CenterHide's source root transform in PreviewTransition.
    expected_center_scale = (hermite(curves["center_hide_scale"], elapsed)
                             if preview else 1.0)
    expected_center_alpha = (hermite(curves["center_hide_alpha"], elapsed)
                             if preview else 1.0)
    need(abs(center["render_scale"]["x"] - expected_center_scale) <= 0.002
         and abs(center["render_scale"]["y"] - expected_center_scale) <= 0.002
         and abs(center["render_opacity"] - expected_center_alpha) <= 0.002,
         f"Source center root animation curve mismatch: {index}")
    need(widget(state, "ExpeditionBackButton")["visible"] is True,
         "Back must remain visible in Preview and Normal")
    if index in (4, 10, 15, 17, 18):
        need(widget(state, "ExpeditionPreviewButton")["visible"] is True,
             "Normal mode Preview button hidden")
        need(widget(state, "ExpeditionChallenge")["visible"] is True,
             "Normal mode Challenge button hidden")
    if index in (7, 16):
        need(widget(state, "ExpeditionPreviewButton")["visible"] is False,
             "Preview mode still offers Preview action")
        need(widget(state, "ExpeditionChallenge")["visible"] is False,
             "Preview mode exposes challenge")
    if index == 18:
        need(state["last_boundary_action_id"] == 1120,
             "Challenge did not remain a read-only source boundary")


def check_art_state(state: dict, index: int, art_source: dict, curves: dict,
                    native_flag_y: list[list[float]], source: dict,
                    result_source: dict) -> None:
    mode = state["art_widgets"]["FeatureModeCanvas"]
    if index == 20:
        need(mode["visible"] is False,
             "Closed phase retained a visible FeatureModeCanvas")
        return
    scale = mode["render_scale"]
    need(mode["exists"] is True and mode["visible"] is True
         and mode["slate_visibility"] == "SelfHitTestInvisible"
         and type(mode["render_opacity"]) in (int, float)
         and math.isfinite(mode["render_opacity"])
         and abs(mode["render_opacity"] - 1) <= 0.01
         and all(type(scale[axis]) in (int, float) and math.isfinite(scale[axis])
                 and abs(scale[axis] - 1) <= 0.01 for axis in ("x", "y")),
         f"Shared FeatureModeCanvas window animation not ready: {index}")
    check_design_frame(state["art_widgets"]["ExpeditionScreenRoot"], mode)
    selected_stage, task_visible, to_preview, _ = PHASE_CONTRACT[index]
    need(state["account_state_known"] is False,
         "Source-style flag was presented as account unlock state")
    art = state["art_widgets"]
    notice = art["ExpeditionConfigurationNotice"]
    need(notice["exists"] is True and notice["visible"] is True
         and ("账号进度未同步" if task_visible else "解锁与星级未同步") in notice["text"],
         "Source-style flag lacks account-unknown boundary label")
    for stage in (1, 2):
        need(art[f"ExpeditionStageType_{stage}"]["exists"] is False,
             "Unproven type-to-img_def/img_atk switch was rendered")
    need(art["ExpeditionPreviewPanelImage"]["exists"] is False,
         "Small source img_firstBg was stretched as an unverified preview background")
    need(art["ExpeditionCenterImgArt"]["exists"] is False,
         "Disabled common pl_com_1009 image was rendered as source result art")
    sprites = art_source["selected_sprite_references"]

    def sprite(name: str, sprite_name: str) -> dict:
        found = [row for row in sprites if row["name"] == name
                 and row["sprite_name"] == sprite_name]
        need(len(found) == 1, f"Ambiguous source sprite: {name}/{sprite_name}")
        return found[0]

    battle_path = ("/Game/RokPrototype/UI/ExpeditionSource/"
                   "rok_expedition_battle_atlas.rok_expedition_battle_atlas")
    task_path = ("/Game/RokPrototype/UI/ExpeditionSource/"
                 "rok_expedition_task_panel.rok_expedition_task_panel")

    def source_image(selector: str, source_row: dict, texture_path: str,
                     texture_size: tuple[int, int], brush_size: tuple[int, int],
                     *, visible: bool | None = None) -> None:
        image = art[selector]
        need(image["exists"] is True and image["is_image"] is True,
             f"Source UImage missing: {selector}")
        if visible is not None:
            need(image["visible"] is visible, f"Source image visibility: {selector}")
        need(image["brush_resource_path"] == texture_path,
             f"Fallback/non-source Brush resource: {selector}")
        need((image["texture_size"]["x"], image["texture_size"]["y"]) == texture_size
             and abs(image["brush_image_size"]["x"] - brush_size[0]) < 0.01
             and abs(image["brush_image_size"]["y"] - brush_size[1]) < 0.01,
             f"Source texture/Brush dimensions: {selector}")
        uv = source_row["slate_uv"]
        actual = (image["brush_uv_min"]["x"], image["brush_uv_min"]["y"],
                  image["brush_uv_max"]["x"], image["brush_uv_max"]["y"])
        need(all(type(value) in (int, float) and math.isfinite(value)
                 for value in actual)
             and all(abs(a - b) <= 0.00001 for a, b in zip(actual, uv)),
             f"Source sprite UV: {selector}")
        size = image["canvas_size"] if visible is False else image["local_size"]
        need(size["x"] > 0 and size["y"] > 0,
             f"Source image size/geometry: {selector}")

    flag = sprite("img_flag2", "img_bat_flag4")
    task = sprite("bg", "img_battle_result_task")
    if task_visible:
        center_geometry = art["ExpeditionCenterRoot"]["local_size"]
        need(abs(center_geometry["x"] - 1440) <= 0.01
             and abs(center_geometry["y"] - 810) <= 0.01,
             "Task CenterRoot escaped design-coordinate canvas")
        for stage in (1, 2):
            need(art[f"ExpeditionStageFlag_{stage}"]["exists"] is False,
                 "Map flag leaked behind full-screen Task")
        source_image("ExpeditionTaskPanelImage", task, task_path,
                     (676, 761), (676, 761))
        need(art["ExpeditionTaskPanelImage"]["parent_widget_name"]
             == "ExpeditionCenterFlagMes", "Task source bg escaped animated flagMes layer")
        for selector in ("ExpeditionChallenge", "ExpeditionTroopCount"):
            need(art[selector]["exists"] is True
                 and art[selector]["parent_widget_name"] == "ExpeditionCenterFlagMes",
                 f"Source Task control escaped flagMes alpha/Y layer: {selector}")
        center_visibility = ("HitTestInvisible" if to_preview
                             and PHASE_CONTRACT[index][3] == "PreviewTransition"
                             else "Collapsed" if to_preview else "SelfHitTestInvisible")
        need(art["ExpeditionCenterRoot"]["slate_visibility"] == center_visibility
             and art["ExpeditionChallenge"]["enabled"] is not to_preview,
             f"Source CenterHide/Show input state: {index}")
        result_sprite = result_source["atlas"]["sprites"]
        need(len(result_sprite) == 1 and result_sprite[0]["name"] == "pl_result_1000_1",
             "Ambiguous source result sprite")
        result = {"slate_uv": result_sprite[0]["uv_top_left_origin"]}
        result_path = ("/Game/RokPrototype/UI/ExpeditionSource/"
                       "rok_expedition_result_atlas.rok_expedition_result_atlas")
        for selector in ("ExpeditionCenterFrameLeft", "ExpeditionCenterFrameRight"):
            source_image(selector, result, result_path, (512, 512), (454, 138))
            need(art[selector]["parent_widget_name"] == "ExpeditionCenterImg",
                 f"Result frame escaped CenterShow img: {selector}")
        def canvas_rect(selector: str, position: tuple[float, float],
                        size: tuple[float, float]) -> None:
            node = art[selector]
            need("canvas_position" in node and "canvas_size" in node
                 and all(abs(node[key][axis] - expected) <= 0.02
                         for key, values in (("canvas_position", position),
                                             ("canvas_size", size))
                         for axis, expected in zip(("x", "y"), values)),
                 f"Source Task canvas rectangle: {selector}")
        canvas_rect("ExpeditionTaskPanelImage", (382, 47.5), (676, 761))
        canvas_rect("ExpeditionCenterImg", (266, 93), (908, 102))
        canvas_rect("ExpeditionCenterFrameLeft", (0, -18), (454, 138))
        canvas_rect("ExpeditionCenterFrameRight", (454, -18), (454, 138))
        canvas_rect("ExpeditionCenterFlagMes", (0, 0), (1440, 810))
        canvas_rect("ExpeditionCenterRoot", (0, 0), (1440, 810))
        canvas_rect("ExpeditionSideRoot", (1294, 12.25), (146, 777.3))
        canvas_rect("ExpeditionPreviewRoot", (0, 0), (1440, 810))
        need(art["ExpeditionMapSelectedPanel"]["exists"] is False,
             "Map selection panel leaked behind full-screen Task")
        left = art["ExpeditionCenterFrameLeft"]
        right = art["ExpeditionCenterFrameRight"]
        need(abs(left["render_scale"]["x"] + 1) < 0.001
             and abs(right["render_scale"]["x"] - 1) < 0.001
             and abs(left["render_scale"]["y"] - 1) < 0.001
             and abs(right["render_scale"]["y"] - 1) < 0.001,
             "Source result frame mirror orientation")
        center_img = art["ExpeditionCenterImg"]
        flag_mes = art["ExpeditionCenterFlagMes"]
        need(center_img["exists"] is True and center_img["is_image"] is False
             and flag_mes["exists"] is True and flag_mes["is_image"] is False,
             "Source CenterShow child panels missing")
        finite_transform(center_img, "ExpeditionCenterImg")
        finite_transform(flag_mes, "ExpeditionCenterFlagMes")
        elapsed = state["animation_elapsed"]
        expected_img_scale = (1.0 if to_preview
                              else hermite(curves["center_show_scale"], elapsed))
        expected_flag_alpha = (1.0 if to_preview
                               else hermite(curves["center_show_flag_alpha"], elapsed))
        expected_flag_y = 0.0 if to_preview else -hermite(native_flag_y, elapsed)
        need(abs(center_img["render_scale"]["x"] - expected_img_scale) <= 0.002
             and abs(center_img["render_scale"]["y"] - expected_img_scale) <= 0.002
             and abs(flag_mes["render_opacity"] - expected_flag_alpha) <= 0.002
             and abs(flag_mes["render_translation"]["x"]) <= 0.02
             and abs(flag_mes["render_translation"]["y"] - expected_flag_y) <= 0.02,
             f"Source CenterShow child img/flag animation mismatch: {index}")
        return
    need(art["ExpeditionTaskPanelImage"]["exists"] is False
         and art["ExpeditionCenterFrameLeft"]["exists"] is False
         and art["ExpeditionCenterFrameRight"]["exists"] is False
         and art["ExpeditionTroopCount"]["exists"] is False,
         "Full-screen Task art leaked onto source map")
    selection = art["ExpeditionMapSelectedPanel"]
    challenge = art["ExpeditionChallenge"]
    need(selection["exists"] is True and selection["visible"] is True
         and challenge["exists"] is True and challenge["visible"] is True
         and challenge["enabled"] is True
         and challenge["parent_widget_name"] == "ExpeditionMapSelectedPanel",
         "Source map selection panel/challenge missing")
    for selector, position, size in (
        ("ExpeditionMapSelectedPanel", (165.25, 616.8), (1150.3, 131.9)),
        ("ExpeditionChallenge", (31.15, 30.95), (202, 84)),
    ):
        node = art[selector]
        need(all(abs(node[key][axis] - value) <= 0.02
                 for key, pair in (("canvas_position", position), ("canvas_size", size))
                 for axis, value in zip(("x", "y"), pair)),
             f"Source map control canvas rectangle: {selector}")
    for stage in (1, 2):
        source_image(f"ExpeditionStageFlag_{stage}", flag, battle_path,
                     (2048, 256), (61, 87), visible=False)
        need(art[f"ExpeditionStageFlag_{stage}"]["parent_widget_name"]
             == "ExpeditionMapContent", "Source flag slot escaped map content")
    mode_canvas = art["FeatureModeCanvas"]
    scroll = art["ExpeditionMapScroll"]
    need(mode_canvas["exists"] is True and scroll["exists"] is True
         and scroll["visible"] is True
          and abs(mode_canvas["local_size"]["x"] - 1440) < 0.01
          and abs(mode_canvas["local_size"]["y"] - 810) < 0.01
         and abs(scroll["local_size"]["x"] - 1292) < 0.01
         and abs(scroll["local_size"]["y"] - 625) < 0.01,
         "Source map viewport cached geometry")
    scale = ((scroll["absolute_bottom_right"]["x"]
              - scroll["absolute_top_left"]["x"]) / 1292.0)
    need(math.isfinite(scale) and 0.5 <= scale <= 2.0,
         "Map viewport Slate/DPI scale")

    def near(actual: float, expected: float, why: str) -> None:
        need(type(actual) in (int, float) and math.isfinite(actual)
             and abs(actual - expected) <= 1.5, why)

    near(scroll["absolute_top_left"]["x"] - mode_canvas["absolute_top_left"]["x"],
         74.0 * scale, "Source map viewport X")
    near(scroll["absolute_top_left"]["y"] - mode_canvas["absolute_top_left"]["y"],
         109.0 * scale, "Source map viewport Y")
    for stage in (1, 2):
        rows = [row for row in source["stages"] if row["id"] == stage]
        need(len(rows) == 1, f"Source battle coordinate: {stage}")
        button = state["widgets"][f"ExpeditionStageButton_{stage}"]
        flag_widget = art[f"ExpeditionStageFlag_{stage}"]
        coordinate = rows[0]["battle_coordinate"]
        near(button["local_size"]["x"], 126, f"Source stage root width: {stage}")
        near(button["local_size"]["y"], 100, f"Source stage root height: {stage}")
        near(button["absolute_top_left"]["x"] - scroll["absolute_top_left"]["x"],
             (coordinate[0] - 63) * scale, f"Source stage root X: {stage}")
        near(button["absolute_top_left"]["y"] - scroll["absolute_top_left"]["y"],
             (coordinate[1] - 50) * scale, f"Source stage root Y: {stage}")
        near(flag_widget["canvas_position"]["x"],
             coordinate[0] - 63 + 32.5, f"Source flag slot X: {stage}")
        near(flag_widget["canvas_position"]["y"],
             coordinate[1] - 50 - 24.8, f"Source flag slot Y: {stage}")
    need(selected_stage in (1, 2), "Unsupported stage art selector")
    stage_rows = [row for row in source["stages"] if row["id"] == selected_stage]
    need(len(stage_rows) == 1 and stage_rows[0]["type"] == 1,
         "Source flag style for audited stage changed")


def verify(run: pathlib.Path) -> dict:
    run = run.resolve()
    need(run.is_dir() and run.parent.name == "ExpeditionArtAudit",
         "Not an ExpeditionArtAudit run")
    manifest = json.loads((run / "manifest.json").read_text(encoding="utf-8-sig"))
    need(manifest["schema"] == "rok.expedition_art.runtime_audit.v2",
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
         and manifest["source_stage_ids"] == [1, 2]
         and {1, 2}.issubset(stage_ids), "Source stage identity")
    for relative, expected in source["source_files_sha256"].items():
        path = ROOT / relative
        need(path.is_file() and sha(path) == expected, f"Unity source drift: {relative}")
    art_source = json.loads(ART_CATALOG.read_text(encoding="utf-8-sig"))
    need(art_source["schema"] == "rok.expedition.visual_assets.v1"
         and len(art_source["atlases"]) == 4,
         "Source art manifest/schema")
    for relative, expected in art_source["sources_sha256"].items():
        path = ROOT / relative
        need(path.is_file() and sha(path) == expected, f"Unity art source drift: {relative}")
    for atlas in art_source["atlases"]:
        prepared = ROOT / atlas["prepared"]
        original = ROOT / atlas["source"]
        need(prepared.is_file() and original.is_file()
             and sha(prepared) == atlas["sha256"] == sha(original)
             and list(png_size(prepared)) == atlas["size"],
             f"Prepared source atlas drift: {atlas['prepared']}")
    result_source = json.loads(RESULT_CATALOG.read_text(encoding="utf-8-sig"))
    need(result_source["schema"] == "rok.expedition.result_visual_assets.v2"
         and result_source["v1_manifest"]["sha256"] == sha(ART_CATALOG),
         "Source result visual manifest/schema")
    for relative, expected in result_source["sources_sha256"].items():
        path = ROOT / relative
        need(path.is_file() and sha(path) == expected,
             f"Source result art drift: {relative}")
    result_atlas = result_source["atlas"]
    result_png = ROOT / result_atlas["prepared"]
    original_png = ROOT / result_atlas["source"]
    need(result_png.is_file() and original_png.is_file()
         and sha(result_png) == result_atlas["prepared_sha256"]
         == result_atlas["sha256"] == sha(original_png)
         and list(png_size(result_png)) == result_atlas["size"] == [512, 512],
         "Prepared ui_result atlas drift")
    frames = result_source["visual_usage"]["visible_children"]
    need(len(frames) == 2 and {row["local_scale"]["x"] for row in frames} == {-1, 1}
         and all(row["source_image_enabled"] is True and row["active_in_prefab"] is True
                 and row["size_delta"] == {"x": 454, "y": 138} for row in frames),
         "Source result mirrored frame identity")
    imported = json.loads(IMPORT_REPORT.read_text(encoding="utf-8-sig"))
    need(imported["schema"] == "rok.expedition.visual_import.v1"
         and imported["passed"] is True and imported["map_saved"] is False
         and imported["source_manifest_sha256"] == sha(ART_CATALOG)
         and len(imported["assets"]) == 2,
         "UE source art import chain")
    for asset in imported["assets"]:
        source_png = pathlib.Path(asset["source_png"])
        uasset = pathlib.Path(asset["uasset"])
        need(source_png.resolve().is_relative_to(ROOT.resolve())
             and uasset.resolve().is_relative_to(ROOT.resolve())
             and source_png.is_file() and uasset.is_file()
             and sha(source_png) == asset["source_sha256"]
             and sha(uasset) == asset["uasset_sha256"]
             and list(png_size(source_png)) == asset["size"],
             f"UE source texture import drift: {asset['target']}")
    result_import = json.loads(RESULT_IMPORT_REPORT.read_text(encoding="utf-8-sig"))
    result_uasset = pathlib.Path(result_import["uasset"])
    need(result_import["schema"] == "rok.expedition.result_visual_import.v2"
         and result_import["passed"] is True and result_import["map_saved"] is False
         and result_import["engine_runtime_acceptance"] is False
         and result_import["source_manifest_sha256"] == sha(RESULT_CATALOG)
         and result_import["source_sha256"] == sha(result_png)
         and result_import["target"] == result_source["visual_usage"]["ue_target"]
         and result_import["size"] == [512, 512]
         and result_uasset.resolve().is_relative_to(ROOT.resolve())
         and result_uasset.is_file()
         and sha(result_uasset) == result_import["uasset_sha256"],
         "UE ui_result source import chain")
    ui_report = json.loads(UI_REPORT.read_text(encoding="utf-8-sig"))
    need(ui_report["schema"] == "rok.expedition_art_ui_implementation.v1"
         and ui_report["source_manifest_sha256"] == sha(ART_CATALOG)
         and ui_report["result_visual_manifest_sha256"] == sha(RESULT_CATALOG)
         and ui_report["native_curve_oracle_sha256"] == sha(NATIVE_CURVES),
         "UI source geometry/art contract drift")
    for relative, expected in ui_report["changed_files"].items():
        if relative in ("Source/Rok/RokUnityFeatureScreenWidget.cpp",
                        "Source/Rok/RokUnityFeatureScreenWidget.h"):
            continue  # The immutable revision2 report pins the final product source.
        path = ROOT / relative
        need(path.is_file() and sha(path) == expected,
             f"UI source implementation drift: {relative}")
    ui_revision = json.loads(UI_REVISION.read_text(encoding="utf-8-sig"))
    need(ui_revision["schema"] == "rok.expedition_art_ui_implementation_revision2.v1"
         and ui_revision["prior_report_sha256"] == sha(UI_REPORT)
         and ui_revision["source_proof"]["native_curve_oracle_sha256"] == sha(NATIVE_CURVES)
         and ui_revision["change"]["task_troop_content_parent"] == "ExpeditionCenterFlagMes"
         and ui_revision["change"]["task_start_boundary_button_parent"] == "ExpeditionCenterFlagMes",
         "UI FlagMes source hierarchy revision drift")
    for relative, expected in ui_revision["files_sha256"].items():
        if relative == "Source/Rok/RokUnityFeatureScreenWidget.cpp":
            continue  # Revision3 pins the final source input-state implementation.
        path = ROOT / relative
        need(path.is_file() and sha(path) == expected,
             f"UI source hierarchy revision drift: {relative}")
    ui_revision3 = json.loads(UI_REVISION3.read_text(encoding="utf-8-sig"))
    need(ui_revision3["schema"] == "rok.expedition_art_ui_implementation_revision3.v1"
         and ui_revision3["prior_report_sha256"] == sha(UI_REVISION)
         and ui_revision3["source_oracle_sha256"] == sha(NATIVE_CURVES),
         "UI source input-state revision drift")
    for relative, expected in ui_revision3["files_sha256"].items():
        path = ROOT / relative
        need(path.is_file() and sha(path) == expected,
             f"UI source input-state implementation drift: {relative}")
    source_review = json.loads(SOURCE_REVIEW.read_text(encoding="utf-8-sig"))
    need(source_review["schema"] == "rok.expedition.art_source_oracle_review.v1"
         and source_review["evidence"]["atlas_manifest"]["sha256"] == sha(ART_CATALOG)
         and source_review["evidence"]["native_curves"]["sha256"] == sha(NATIVE_CURVES),
         "Source art/animation oracle drift")
    for item in source_review["source_pins"]:
        path = ROOT / item["path"]
        need(path.is_file() and sha(path) == item["sha256"],
             f"Unity art/prefab/mediator source drift: {item['path']}")
    native = json.loads(NATIVE_CURVES.read_text(encoding="utf-8-sig"))
    need(native["schema"] == "rok.unity_expedition_animation_native.v2",
         "Native animation oracle schema")
    for row in native["sources"]:
        path = ROOT / row["path"]
        need(path.is_file() and sha(path) == row["sha256"],
             f"Native animation source drift: {row['path']}")
    show_clips = [clip for clip in native["clips"] if clip["id"] == "center_show"]
    need(len(show_clips) == 1, "Native CenterShow clip identity")
    y_curves = [row for row in show_clips[0]["float_curves"]
                if row["binding_path"] == "pl_flagMes"
                and row["property_name"] == "m_AnchoredPosition.y"]
    need(len(y_curves) == 1, "Native pl_flagMes anchored-Y binding")
    native_flag_y = [[key["time"], key["value"]] for key in y_curves[0]["keys"]]
    need(len(native_flag_y) == 3 and all(
        abs(actual[0] - expected[0]) < 0.00001
        and abs(actual[1] - expected[1]) < 0.00001
        for actual, expected in zip(native_flag_y,
                                    ((0, 10), (0.33333334, 10), (0.5833333, 0)))),
        "Native CenterShow source Y keyframes changed")
    need(all(key["in_tangent"]["finite"] is True
             and key["out_tangent"]["finite"] is True
             and abs(key["in_tangent"]["value"]) < 0.00001
             and abs(key["out_tangent"]["value"]) < 0.00001
             for key in y_curves[0]["keys"]), "Native Y interpolation is not zero-tangent")
    for clip_id, expected_value, expected_duration, report_key in (
        ("center_show", 1.0, 0.5833333, "UA_ExpeditionTask_centerShow"),
        ("center_hide", 0.0, 0.33333334, "UA_ExpeditionTask_centerHide"),
    ):
        clips = [clip for clip in native["clips"] if clip["id"] == clip_id]
        need(len(clips) == 1, f"Native source input clip: {clip_id}")
        for property_name in ("m_Interactable", "m_BlocksRaycasts"):
            bindings = [row for row in clips[0]["float_curves"]
                        if row["binding_path"] == ""
                        and row["property_name"] == property_name]
            need(len(bindings) == 1 and len(bindings[0]["keys"]) == 2,
                 f"Native root CanvasGroup input binding: {clip_id}/{property_name}")
            actual = [[key["time"], key["value"]] for key in bindings[0]["keys"]]
            reported = ui_revision3["source_input_curves"][report_key]["root_" + property_name]
            need(len(reported) == 2 and all(
                abs(a[0] - b[0]) < 0.00001 and abs(a[1] - b[1]) < 0.00001
                for a, b in zip(actual, reported))
                 and abs(actual[0][0]) < 0.00001
                 and abs(actual[1][0] - expected_duration) < 0.00001
                 and all(abs(row[1] - expected_value) < 0.00001 for row in actual),
                 f"Native root source input bool changed: {clip_id}/{property_name}")
    pins = manifest["inputs_sha256"]
    required = {"Scripts/VerifyRokExpeditionArtRuntimeV2.py",
                "Scripts/PrepareRokExpeditionVisualAssets.py",
                "Scripts/VerifyRokExpeditionVisualAssets.py",
                "Scripts/PrepareRokExpeditionResultVisualAssets.py",
                "Scripts/VerifyRokExpeditionResultVisualAssets.py",
                "Scripts/ImportRokExpeditionResultVisualAsset.py",
                "Saved/RokUnityMigration/expedition_stage_preview.json",
                "Saved/RokUnityMigration/ExpeditionVisualAssets/v1/atlas_manifest.json",
                "Saved/RokUnityMigration/ExpeditionVisualAssets/v2/result_visual_manifest.json",
                "Saved/RokUnityMigration/ExpeditionVisualAssets/v1/atlases/ui_battle.png",
                "Saved/RokUnityMigration/ExpeditionVisualAssets/v1/atlases/img_battle_result_task.png",
                "Saved/RokUnityMigration/ExpeditionVisualAssets/v2/ui_result.png",
                "Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/expedition_art_ui_implementation.json",
                "Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/expedition_art_ui_implementation_revision2.json",
                "Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/expedition_art_ui_implementation_revision3.json",
                "Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/expedition_art_source_oracle_review.json",
                "Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/expedition_visual_ue_import_20261009_015909_339908.json",
                "Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/expedition_result_ue_import_20261009_020448_951277.json",
                "Saved/UnityExpeditionAnimationProbeV1/OutputRuns/20261009_013025_916_42504/native_animation_curves.json",
                "Content/RokPrototype/UI/ExpeditionSource/rok_expedition_battle_atlas.uasset",
                "Content/RokPrototype/UI/ExpeditionSource/rok_expedition_task_panel.uasset",
                "Content/RokPrototype/UI/ExpeditionSource/rok_expedition_result_atlas.uasset",
                "Source/Rok/RokUnityFeatureScreenWidget.cpp",
                "Source/Rok/RokExpeditionStagePreviewAuditSubsystem.cpp"}
    need(required.issubset(pins), "Missing runtime producer/input pin")
    for relative, expected in pins.items():
        path = ROOT / relative
        need(path.is_file() and sha(path) == expected, f"Runtime input drift: {relative}")
    warm = manifest["texture_warmup"]
    need(warm["load_started"] is True and warm["ready_before_hud_click"] is True
         and type(warm["start_frame"]) in (int, float)
         and type(warm["ready_frame"]) in (int, float)
         and math.isfinite(warm["start_frame"]) and math.isfinite(warm["ready_frame"])
         and 0 < warm["start_frame"] < warm["ready_frame"],
         "Source atlases were not warmed across natural frames")
    warm_specs = (
        ("/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_battle_atlas.rok_expedition_battle_atlas",
         "Content/RokPrototype/UI/ExpeditionSource/rok_expedition_battle_atlas.uasset", 2048, 256),
        ("/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_task_panel.rok_expedition_task_panel",
         "Content/RokPrototype/UI/ExpeditionSource/rok_expedition_task_panel.uasset", 676, 761),
        ("/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_result_atlas.rok_expedition_result_atlas",
         "Content/RokPrototype/UI/ExpeditionSource/rok_expedition_result_atlas.uasset", 512, 512),
    )
    need(len(warm["textures"]) == len(warm_specs), "Source atlas warm set changed")
    for row, (object_path, asset_path, width, height) in zip(warm["textures"], warm_specs):
        expected_size = {"x": width, "y": height}
        need(row["object_path"] == object_path and row["uasset_path"] == asset_path
             and row["uasset_sha256"] == pins[asset_path]
             and row["expected_size"] == expected_size
             and row["actual_size"] == expected_size
             and row["rhi_extent"] == expected_size
             and row["rhi_ready"] is True,
             f"Source atlas warm resource/hash/size mismatch: {asset_path}")
    baseline = manifest["initial_business_state"]
    curves = source["animation_zero_tangent_hermite_curves"]
    validate_curves(curves)
    check_business(baseline, baseline)
    primary_count = manifest["initial_controller_primary_press_count"]
    need(isinstance(primary_count, int) and primary_count >= 0,
         "Invalid initial controller input counter")
    actions = manifest["actions"]
    need([a["label"] for a in actions] == ACTIONS, "Real Slate action order")
    need(actions[0]["frame"] > warm["ready_frame"],
         "HUD Slate click preceded source atlas warm readiness")
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
    blocked = manifest["blocked_slate_actions"]
    need(len(blocked) == 1, "Expected one real disabled Challenge Slate attempt")
    probe = blocked[0]
    need(probe["label"] == "blocked_preview_challenge_stage1"
         and probe["method"] == "Slate ProcessMouseMoveEvent+DownEvent+UpEvent"
         and probe["own_game_window"] is True
         and probe["target_in_hit_path"] is False
         and probe["feature_input_blocker_in_hit_path"] is True
         and probe["down_handled"] is True and probe["up_handled"] is True
         and "SButton" in probe["terminal_type"]
         and any("SObjectWidget" in node for node in probe["hit_path_types"])
         and probe["next_frame"] > probe["frame"],
         "Disabled Challenge did not route through the actual feature blocker")
    center = probe["absolute_center"]
    challenge_before = probe["before_state"]["widgets"]["ExpeditionChallenge"]
    expected_center = {
        axis: (challenge_before["absolute_top_left"][axis]
               + challenge_before["absolute_bottom_right"][axis]) / 2
        for axis in ("x", "y")
    }
    need(all(type(center[axis]) in (int, float) and math.isfinite(center[axis])
             and abs(center[axis] - expected_center[axis]) <= 0.5
             for axis in ("x", "y")),
         "Blocked Slate pointer did not use actual Challenge cached geometry")
    for key in ("before_state", "down_state", "up_state", "next_frame_state"):
        state = probe[key]
        check_business(state["business"], baseline)
        need(state["controller_primary_press_count"] == primary_count
             and state["pending_action_id"] == -1
             and state["last_boundary_action_id"] == -1
             and state["screen_open"] is True
             and state["task_config_visible"] is True
             and state["selected_stage_id"] == 1
             and state["preview_mode"] is True
             and state["art_widgets"]["ExpeditionChallenge"]["enabled"] is False,
             f"Disabled Challenge caused action, world input or state change: {key}")
    for key in ("before_state", "down_state", "up_state"):
        state = probe[key]
        need(state["animation_phase"] == "PreviewTransition"
             and state["art_widgets"]["ExpeditionCenterRoot"]["slate_visibility"]
             == "HitTestInvisible", f"Source CenterHide input state: {key}")
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
            check_state(capture[key], index, baseline, primary_count, curves, key)
            check_art_state(capture[key], index, art_source, curves,
                            native_flag_y, source, result_source)
        if PHASE_CONTRACT[index][3] in ("PreviewTransition", "NormalTransition"):
            clocks = [capture[key]["animation_elapsed"] for key in
                      ("requested_state", "processed_state", "next_frame_state")]
            need(clocks == sorted(clocks), f"Animation clock regressed across screenshot: {index}")
    check_state(manifest["final_state"], 21, baseline, primary_count, curves)
    check_art_state(manifest["final_state"], 21, art_source, curves,
                    native_flag_y, source, result_source)
    return {"schema": "rok.expedition_art.runtime_verify.v2",
            "passed": True, "run": str(run), "capture_count": len(captures),
            "slate_action_count": len(actions), "source_stage_ids": [1, 2],
            "source_stage_count": len(stage_ids),
            "visual_boundary": "Source image brush/UV and child animation metadata checked; pixel comparison remains pending"}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("run", type=pathlib.Path)
    args = parser.parse_args()
    result = verify(args.run)
    output = args.run.resolve() / "verified_expedition_art_v2.json"
    need(not output.exists(), "Refusing to overwrite existing verification report")
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
