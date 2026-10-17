#!/usr/bin/env python3
"""Fail-closed verifier for ordinary-HUD Farm/Wood local construction evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "Saved" / "RokUnityMigration" / "LocalConstructionRuntimeAudit"
PHASES = [
    "01_hall2_prerequisite", "02_farm_valid_preview", "03_farm_pending_once",
    "04_farm_completed_registered", "05_farm_zoomed_out_selected",
    "05_farm_world_selected",
    "06_wood_valid_preview", "07_wood_pending_once",
    "08_wood_completed_registered", "09_wood_world_selected",
]
CAPTURE_PHASES = PHASES[:2] + ["02b_farm_invalid_gray_preview"] + PHASES[2:]
MENU_PHASES = {"02_farm_valid_preview", "02b_farm_invalid_gray_preview",
               "06_wood_valid_preview"}
UI_ACTIONS = {
    "hall_upgrade_menu", "hall_timed_upgrade_start", "build_city_open_farm",
    "farm_source_card", "farm_confirm_ordinary_hud", "build_city_open_wood",
    "wood_source_card", "wood_confirm_ordinary_hud",
}
WORLD_ACTIONS = {
    "farm_world_down", "farm_release_invalid_drag", "farm_invalid_ghost_world_down",
    "farm_release_valid_drag", "farm_pending_world_down", "farm_pending_world_up",
    "farm_completed_world_down", "farm_completed_world_up",
    "farm_zoomed_world_down", "farm_zoomed_world_up",
    "wood_pending_world_down", "wood_pending_world_up",
    "wood_completed_world_down", "wood_completed_world_up",
}


def need(value: bool, why: str) -> None:
    if not value:
        raise AssertionError(why)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_menu_revision(revision: dict, revision3: dict, hashes: dict) -> None:
    contract = revision["source_contract"]
    need(revision["schema"] == "rok.placement_menu_hud_ui_revision2.v1" and
         contract["dynamic_button_ids"] == [2, 1] and
         contract["button_transition"] == "Unity UI Selectable.ColorTint" and
         contract["button_tint_gamma_rgba"]["disabled"] ==
         [.78431374, .78431374, .78431374, .5019608] and
         contract["tint_fade_unscaled_seconds"] == .1 and
         "UA_BuildingMenu On/Off" in contract["menu_animation"] and
         "0.25 seconds" in contract["menu_animation"] and
         revision["ue_implementation"]["audit_getters"] ==
         ["IsPlacementMenuAnimatingForAudit", "IsPlacementMenuClosingForAudit",
          "GetPlacementMenuAnimationElapsedSecondsForAudit"],
         "Placement menu ColorTint/opening source contract differs")
    for relative, sha in revision["source_sha256"].items():
        source = ROOT / relative
        need(source.is_file() and digest(source) == sha,
             f"Placement menu Unity source changed: {relative}")
    close_source = "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_close_input_source.json"
    close_report = json.loads((ROOT / close_source).read_text(encoding="utf-8-sig"))
    need(close_source in hashes and
         close_report["schema"] == "rok.placement_menu_close_input_source.v1" and
         "CanvasGroup continues to allow raycasts" in
         close_report["findings"]["hit_testing_during_close"]["result"],
         "Placement menu Close interaction source contract differs")
    for source_row in close_report["source_files"]:
        relative = source_row["path"]
        need(digest(ROOT / relative) == source_row["sha256"],
             f"Placement menu Close source changed: {relative}")
    animation_source = "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_animation_source_inventory.json"
    animation_report = json.loads((ROOT / animation_source).read_text(encoding="utf-8-sig"))
    need(animation_source in hashes and
         animation_report["schema"] == "rok.placement_menu_animation_source_inventory.v1" and
         animation_report["animation_contract"]["open_clip"]["duration_seconds"] == .25 and
         animation_report["animation_contract"]["close_clip"]["duration_seconds"] == .25,
         "BuildingMenu source animation duration differs")
    for source_row in animation_report["source_files"]:
        relative = source_row["path"]
        need(digest(ROOT / relative) == source_row["sha256"],
             f"BuildingMenu animation source changed: {relative}")
    previous_source = "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_previous_selection_source.json"
    previous_report = json.loads((ROOT / previous_source).read_text(encoding="utf-8-sig"))
    need(previous_source in hashes and
         previous_report["schema"] == "rok.placement_previous_selection_source.v1" and
         "does not prescribe keeping the previously selected regular building menu" in
         previous_report["conclusion"]["finding"],
         "Placement previous-selection source contract differs")
    for source_row in previous_report["files"]:
        relative = source_row["path"]
        need(digest(ROOT / relative) == source_row["sha256"],
             f"Placement previous-selection source changed: {relative}")
    gray_source = "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_gray_render_diagnosis.json"
    gray_report = json.loads((ROOT / gray_source).read_text(encoding="utf-8-sig"))
    need(gray_source in hashes and
         gray_report["schema"] == "rok.placement_menu.gray_render_diagnosis.v1" and
         "(128/255)^2" in gray_report["root_cause"]["ue"],
         "Placement gray shader diagnosis source differs")
    for relative, sha in ((gray_report["root_cause"]["source_shader_path"],
                           gray_report["root_cause"]["source_shader_sha256"]),
                          (gray_report["root_cause"]["research_material_generator"],
                           gray_report["root_cause"]["research_material_generator_sha256"])):
        need(digest(ROOT / relative) == sha,
             f"Placement gray shader diagnostic source changed: {relative}")
    material_source = "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_source.json"
    material_report = json.loads((ROOT / material_source).read_text(encoding="utf-8-sig"))
    need(material_source in hashes and
         material_report["schema"] == "rok.placement.gray_material_source.v1" and
         material_report["target"] ==
         "/Game/RokPrototype/UI/PlacementAtlases/M_RokPlacementGrayUI" and
         "material never reads VertexColor" in material_report["tint_contract"] and
         material_report["parameters"]["SourceTint"] == [1, 1, 1, 1],
         "Dedicated Placement gray material source formula differs")
    for source_row in material_report["source_files"]:
        relative = source_row["path"]
        need(digest(ROOT / relative) == source_row["sha256"],
             f"Dedicated gray material source changed: {relative}")
    material_v2_source = "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_source_v2.json"
    material_v2 = json.loads((ROOT / material_v2_source).read_text(encoding="utf-8-sig"))
    need(material_v2_source in hashes and
         material_v2["schema"] == "rok.placement.gray_material_source.v2" and
         "output RGBA index 5" in material_v2["tint_contract"] and
         material_v2["target"] == material_report["target"] and
         material_v2["parameters"] == material_report["parameters"],
         "Dedicated Placement gray material RGBA graph differs")
    for source_row in material_v2["source_files"]:
        relative = source_row["path"]
        need(digest(ROOT / relative) == source_row["sha256"],
             f"Dedicated gray material v2 source changed: {relative}")
    material_v3_source = "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_source_v3.json"
    material_v3 = json.loads((ROOT / material_v3_source).read_text(encoding="utf-8-sig"))
    need(material_v3_source in hashes and
         material_v3["schema"] == "rok.placement.gray_material_source.v3" and
         material_v3["target"] == material_v2["target"] and
         material_v3["parameters"] == material_v2["parameters"] and
         "output RGBA index 5" in material_v3["tint_contract"] and
         "sRGBEncode(sampleLinear)" in material_v3["ue_formula"] and
         "sRGBDecode(grayGamma)" in material_v3["ue_formula"] and
         "material never reads VertexColor" in material_v3["tint_contract"] and
         hashlib.sha256(material_v3["hlsl"].encode("utf-8")).hexdigest() ==
         material_v3["hlsl_sha256"],
         "Dedicated Placement gray material final Gamma/RGBA graph differs")
    for source_row in material_v3["source_files"]:
        relative = source_row["path"]
        need(digest(ROOT / relative) == source_row["sha256"],
             f"Dedicated gray material v3 source changed: {relative}")
    need(digest(ROOT / material_v3["producer"]["path"]) ==
         material_v3["producer"]["sha256"],
         "Dedicated gray material final producer changed")
    asset_path = "Content/RokPrototype/UI/PlacementAtlases/M_RokPlacementGrayUI.uasset"
    create_path = ("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/"
                   "PlacementGrayMaterial/20261008T230559679259Z_46968_ebba69ea/create.json")
    root_create_path = ("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/"
                        "placement_gray_material_root_create.json")
    create = json.loads((ROOT / create_path).read_text(encoding="utf-8-sig"))
    root_create = json.loads((ROOT / root_create_path).read_text(encoding="utf-8-sig"))
    need(create_path in hashes and root_create_path in hashes and
         create["schema"] == "rok.placement.gray_material_create.v1" and
         create["project"] == root_create["project"] == str(ROOT) and
         create["world"] == root_create["world"] == "L_RokUnityGameAppCity" and
         root_create["schema"] == "rok.placement.gray_material_root_create.v1" and
         root_create["passed"] is True and
         create["asset"] == material_v3["target"] and
         create["uasset_sha256"] == root_create["asset_sha256"] == hashes[asset_path].lower() and
         create["source_proof"] == material_v3_source and
         create["source_proof_sha256"] == hashes[material_v3_source].lower() and
         create["script_sha256"] == root_create["script_sha256"] ==
         material_v3["producer"]["sha256"] and
         create["hlsl_sha256"] == material_v3["hlsl_sha256"] and
         create["source_tint_default_gamma_rgba"] == material_v3["parameters"]["SourceTint"] and
         create["verified_source_atlas_output"]["index"] == 5 and
         create["verified_source_tint_output"]["index"] == 5 and
         "SourceTint.RGBA(output_index=5) -> Custom.SourceTint" in create["connections"] and
         create["reads_vertex_color"] is False and
         create["visual_verified"] is False and create["rhi_verified"] is False and
         root_create["runtime_pixels_verified"] is False,
         "Dedicated Placement gray material UE creation proof differs")
    need(revision3["schema"] == "rok.placement_menu_hud_ui_revision3.v1" and
         revision3["cause_report_sha256"] == hashes[gray_source].lower() and
         revision3["unity_contract"]["gamma_color_tint_disabled"] ==
         [200 / 255, 200 / 255, 200 / 255, 128 / 255] and
         revision3["unity_contract"]["color_fade_seconds"] == .1 and
         revision3["unity_contract"]["vertex_tint_applied_once"] is True and
         revision3["ue_change"]["dedicated_material"] ==
         "/Game/RokPrototype/UI/PlacementAtlases/M_RokPlacementGrayUI" and
         revision3["ue_change"]["gray_image_color_and_opacity"] == [1, 1, 1, 1],
         "Placement dedicated gray material source contract differs")
    for relative in ("Source/Rok/RokMainHudWidget.h", "Source/Rok/RokMainHudWidget.cpp"):
        need(revision3["implementation_sha256"][relative] == hashes[relative].lower(),
             f"Placement menu revision3 HUD hash differs: {relative}")


def verify_menu(menu: dict, *, disabled: bool) -> None:
    atlas = "/Game/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud.rok_placement_ui_hud"
    need(menu["previous_selection_actor"] == "" and
         menu["previous_selected_panel_collapsed"] and
         menu["previous_selected_panel_visibility"] == 1,
         "Active placement screenshot retained the prior selected-building menu")
    need(menu["screenshot_client_rect_valid"] and
         abs(menu["screenshot_client_size"]["x"] - 1440) < 1 and
         abs(menu["screenshot_client_size"]["y"] - 810) < 1,
         "Placement screenshot client rect does not match the source viewport")
    need(abs(menu["source_disabled_color_transition_seconds"] - .1) < 1e-6,
         "Placement disabled ColorTint duration differs from Unity Selectable")
    need(menu["hover_clear_performed"] and menu["hover_clear_settle_seconds"] >= .11,
         "Placement popup hover was not cleared by a settled real pointer move")
    need(abs(menu["source_open_duration_seconds"] - .25) < 1e-6 and
         abs(menu["source_open_end_opacity"] - 1) < 1e-6 and
         abs(menu["root_render_opacity"] - menu["source_open_end_opacity"]) < .001 and
         menu["source_open_end_opacity_reached"] and
         menu["source_open_clip_finished"] and
         menu["source_open_elapsed_seconds"] >= .249,
         "Placement popup was captured before source opening clip finished")
    need(0 < menu["camera_city_dxf"] <= 270 and
         menu["world_button_anchor_projected"] and
         menu["projection_slot_error"] < 2 and
         abs(menu["render_scale"]["x"] - menu["expected_source_render_scale"]) < .01 and
         abs(menu["render_scale"]["y"] - menu["expected_source_render_scale"]) < .01,
         "Placement popup world anchor/camera scaling differs from source")
    for key, local, uv_min, uv_max in (
        ("confirm", (279.2, 167.5), (721 / 1024, 361 / 1024), (809 / 1024, 449 / 1024)),
        ("cancel", (179.2, 167.5), (1 / 1024, 501 / 1024), (89 / 1024, 589 / 1024)),
    ):
        icon = menu[f"{key}_image"]
        button = menu[f"{key}_button"]
        color = menu[f"{key}_linear_color"]
        material_tint = menu[f"{key}_source_tint_gamma"]
        expected_tint = (200 / 255, 200 / 255, 200 / 255, 128 / 255) \
            if key == "confirm" and disabled else (1, 1, 1, 1)
        need(icon["present"] and icon["visible_with_ancestors"] and icon["enabled"] and
             button["present"] and button["visible_with_ancestors"] and
             button["enabled"] is (not disabled if key == "confirm" else True) and
             all(abs(icon["local_size"][axis] - 87) < .1 and
                 abs(button["local_size"][axis] - 87) < .1 and
                 abs(icon["absolute_center"][axis] - button["absolute_center"][axis]) < 1
                 for axis in ("x", "y")) and
             menu[f"{key}_atlas_ready"] and menu[f"{key}_uv_matches_source"] and
             menu[f"{key}_gray_material_ready"] and menu[f"{key}_source_atlas"] == atlas and
             menu[f"{key}_source_end_color"] and menu[f"{key}_source_end_tint"] and
             all(abs(color[channel] - 1) < .002 for channel in ("r", "g", "b", "a")) and
             all(abs(material_tint[channel] - value) < .002
                 for channel, value in zip(("r", "g", "b", "a"), expected_tint)) and
             all(abs(menu[f"{key}_root_local_top_left"][axis] - value) < 1
                 for axis, value in zip(("x", "y"), local)) and
             all(abs(menu[f"{key}_uv_min"][axis] - value) < 1e-5
                 for axis, value in zip(("x", "y"), uv_min)) and
             all(abs(menu[f"{key}_uv_max"][axis] - value) < 1e-5
                 for axis, value in zip(("x", "y"), uv_max)),
             f"Placement {key} cached geometry/brush differs from source")
    need(menu["controls"]["visible_with_ancestors"] and
         menu["controls"]["enabled"] and
         abs(menu["controls"]["local_size"]["x"] - 472.7) < .1 and
         abs(menu["controls"]["local_size"]["y"] - 220) < .1 and
         menu["cancel_brush_resource"] == atlas and
         (menu["confirm_brush_resource"] != atlas if disabled
          else menu["confirm_brush_resource"] == atlas) and
         (menu["confirm_gray_base_material"] ==
          "/Game/RokPrototype/UI/PlacementAtlases/M_RokPlacementGrayUI.M_RokPlacementGrayUI"
          if disabled else menu["confirm_gray_base_material"] == ""),
         "Placement popup source root/disabled gray brush differs")


def verify_hover_clear_actions(actions: list[dict]) -> None:
    expected = {"02_farm_valid_preview", "02b_farm_invalid_gray_preview",
                "06_wood_valid_preview"}
    clears = [action for action in actions if action["action"] == "placement_menu_hover_clear"]
    need(len(clears) == len(expected) and
         {action["phase_name"] for action in clears} == expected,
         "Missing real non-popup pointer move for a placement screenshot")
    for action in clears:
        need(action["method"].startswith("real Slate mouse move") and
             action["route"]["os_cursor_hits_own_window"] and
             action["inside_game_client"] and
             action["primary_press_before"] == action["primary_press_after"] and
             action["drag_begin_before"] == action["drag_begin_after"] and
             action["grid_before"] == action["grid_after"] and
             action["selection_unchanged"] and not action["dragging_after"] and
             action["outside_popup_geometry"] and
             action["from_absolute"] != action["to_absolute"],
             f"Placement hover clear changed input state: {action['phase_name']}")


def verify_disabled_pointer_boundary(disabled: dict) -> None:
    stages = ("before", "after_down", "after_up", "after_next_game_frame")
    counts = [disabled[f"primary_press_{stage}"] for stage in stages]
    outcomes = [disabled[f"primary_press_outcome_{stage}"] for stage in stages]
    need(disabled["down_handled"] and disabled["up_handled"] and
         disabled["before_route"]["hit_path_contains_umg"] and
         disabled["before_route"]["os_cursor_hits_own_window"] and
         counts[0] <= counts[1] <= counts[2] <= counts[3] <= counts[0] + 1 and
         all(outcomes[index] == (outcomes[index - 1] if counts[index] == counts[index - 1]
                                 else "ui_owned") for index in range(1, 4)) and
         disabled["drag_begin_before"] == disabled["drag_begin_after_down"] ==
         disabled["drag_begin_after_up"] == disabled["drag_begin_after_next_game_frame"] and
         disabled["selection_before"] == disabled["selection_after_down"] ==
         disabled["selection_after_up"] == disabled["selection_after_next_game_frame"] == "" and
         disabled["grid_before"] == disabled["grid_after_down"] ==
         disabled["grid_after_up"] == disabled["grid_after_next_game_frame"] and
         not any(disabled[key] for key in
                 ("dragging_before", "dragging_after_down", "dragging_after_up",
                  "dragging_after_next_game_frame")) and
         all(abs(disabled[key] - disabled["camera_dxf_before"]) < .001 for key in
             ("camera_dxf_after_up", "camera_dxf_after_next_game_frame")),
         "Disabled HUD pointer dispatched placement or selected world")


def check_ticket(state: dict, key: str, *, kind: int, completed: bool,
                 pending: bool, food: int, wood: int) -> None:
    ticket = state[key]
    need(ticket["exists"] and ticket["id"] > 0 and ticket["type_id"] == kind,
         f"Missing {key} identity")
    need(ticket["footprint"] == {"x": 5, "y": 5} and ticket["power"] == 5,
         f"{key} footprint or power differs")
    need((ticket["cost_food"], ticket["cost_wood"]) == (food, wood),
         f"{key} source cost differs")
    need(abs(ticket["finishes_at_world_seconds"] -
             ticket["started_at_world_seconds"] - 2) < .001,
         f"{key} source two-second timer differs")
    need(ticket["completed"] is completed and ticket["pending"] is pending,
         f"{key} completion/pending transition differs")
    need(ticket["actor_valid"] and not ticket["actor_hidden"] and
         ticket["actor_grid"] == ticket["grid"] and
         ticket["actor_footprint"] == ticket["footprint"] and
         ticket["formal_tag"] is completed and
         ticket["pending_tag"] is pending and
         not ticket["unity_init_tag"] and
         not ticket["completion_failure"] and
         ticket["completion_failure_reason"] == "",
         f"{key} actor/source tags or completion failure differ")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", required=True, type=Path)
    args = parser.parse_args()
    run = args.run.resolve()
    need(run.parent == EVIDENCE.resolve(), "Run escaped LocalConstructionRuntimeAudit")
    path = run / "manifest.json"
    need(path.is_file(), "Missing manifest.json")
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    need(data["schema"] == "rok.local_construction.runtime_audit.v1", "Wrong schema")
    need(data["project"].replace("\\", "/").rstrip("/").lower() ==
         str(ROOT).replace("\\", "/").rstrip("/").lower(), "Wrong project")
    need(data["world"] == "/Game/Maps/L_RokUnityGameAppCity", "Wrong city world")
    need(data["complete"] is True and data["passed"] is True and
         data["failures"] == [], "Runtime did not pass")
    need(data["capture_count"] == data["expected_capture_count"] == len(CAPTURE_PHASES),
         "Wrong screenshot count")
    need(data["production_placement_delegate"] is True and
         data["audit_listener_bound"] is False and
         data["construction_started_only_by_production_hud_delegate"] is True and
         data["delegate_assurance"] == "source_pinned_code_and_observed_controller_outcome" and
         data["world_time_advanced_by_audit"] is False,
         "Audit faked construction or time")
    need(data["server_local_only_run"] is True and
         data["server_http_dispatch_attempts"] == 0 and
         data["server_suppressed_local_requests"] == 0,
         "GameApp construction audit crossed its local-only server boundary")
    need(data["formal_construction_started"] is True and
         data["visual_fidelity_accepted"] is False and
         data["world_travel_covered"] is False and
         data["other_resolutions_covered"] is False,
         "Construction scope was understated or visual scope overclaimed")
    need(data["local_completion_events"] == 2 and
         0 < data["farm_ticket_id"] < data["wood_ticket_id"],
         "Farm/Wood completions or IDs differ")
    need(data["farm_completion_observed_wall_seconds"] -
         data["farm_submit_wall_seconds"] >= 1.9 and
         data["wood_completion_observed_wall_seconds"] -
         data["wood_submit_wall_seconds"] >= 1.9,
         "Farm/Wood completion was not observed after natural wall-clock wait")
    window = data["own_game_window_preparation"]
    need(window["verified_own_game_window"] and window["verified_1440x810_client"] and
         window["visible_after_show"] and window["foreground_is_game_after_call"],
         "Ordinary UI/world pointer did not use the verified Rok game window")
    hashes = data["inputs_sha256"]
    required = {
        "Source/Rok/RokLocalConstructionRuntimeAuditSubsystem.h",
        "Source/Rok/RokLocalConstructionRuntimeAuditSubsystem.cpp",
        "Scripts/VerifyRokLocalConstructionRuntime.py",
        "Scripts/VerifyRokPlacementMenuRuntime.py",
        "Source/Rok/RokPlayerController.cpp",
        "Source/Rok/RokStrategyCameraPawn.h",
        "Source/Rok/RokStrategyCameraPawn.cpp",
        "Source/Rok/RokCitySpriteActor.cpp",
        "Source/Rok/RokBuildingSelectionIndicator.cpp",
        "Source/Rok/RokMainHudWidget.cpp",
        "Source/Rok/RokMainHudWidget.h",
        "Source/Rok/RokBuildingPanelWidget.h",
        "Source/Rok/RokBuildingPanelWidget.cpp",
        "Source/Rok/RokUiWindowAnimation.h",
        "Source/Rok/RokUiWindowAnimation.cpp",
        "Content/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud.uasset",
        "Saved/RokUnityMigration/PlacementUiAtlas/20261008_221417_249217_45548/atlas_manifest.json",
        "Saved/RokUnityMigration/PlacementUiAtlas/20261008_221417_249217_45548/ue_import_report.json",
        "Content/RokPrototype/UI/PlacementAtlases/M_RokPlacementGrayUI.uasset",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_hud_ui_static.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_hud_ui_revision2.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_hud_ui_revision3.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_close_input_source.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_animation_source_inventory.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_previous_selection_source.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_gray_render_diagnosis.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_source.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_source_v2.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_source_v3.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/PlacementGrayMaterial/20261008T230559679259Z_46968_ebba69ea/create.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_root_create.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_ui_asset_inventory.json",
        "Source/Rok/RokBuildingUpgradeSubsystem.cpp",
        "Source/Rok/RokLocalConstructionVisualFactory.cpp",
        "Source/Rok/RokCityConstructionGrid.cpp",
        "Source/Rok/RokCityResourceProduction.cpp",
        "Source/Rok/RokServerSubsystem.h",
        "Source/Rok/RokServerSubsystem.cpp",
        "Saved/RokUnityMigration/ConfigTables/BuildingResourcesProduce.json",
    }
    need(required.issubset(hashes), "Missing source trace hashes")
    for relative, sha in hashes.items():
        source = ROOT / relative
        need(source.is_file() and digest(source) == sha.lower(),
             f"Runtime input changed or missing: {relative}")
    ui_source = json.loads((ROOT / "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_hud_ui_static.json").read_text(encoding="utf-8-sig"))
    ui_revision = json.loads((ROOT / "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_hud_ui_revision2.json").read_text(encoding="utf-8-sig"))
    ui_revision3 = json.loads((ROOT / "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_hud_ui_revision3.json").read_text(encoding="utf-8-sig"))
    verify_menu_revision(ui_revision, ui_revision3, hashes)
    atlas_source = json.loads((ROOT / "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_ui_asset_inventory.json").read_text(encoding="utf-8-sig"))
    import_report = json.loads((ROOT / "Saved/RokUnityMigration/PlacementUiAtlas/20261008_221417_249217_45548/ue_import_report.json").read_text(encoding="utf-8-sig"))
    need(ui_source["schema"] == "rok.placement_menu_hud_ui_static.v1" and
         atlas_source["schema"] == "rok.placement_ui_asset_inventory.v1" and
         ui_source["source_geometry"]["menu_root_size"] == [472.7, 220] and
         ui_source["source_geometry"]["tp21"].endswith("87x87") and
         ui_source["source_geometry"]["tp22"].endswith("87x87") and
         atlas_source["source_atlas"]["size"] == [1024, 1024] and
         import_report["schema"] == "rok.placement_ui_atlas_import.v1" and
         import_report["passed"] is True and
         import_report["asset_uasset_sha256"] == hashes["Content/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud.uasset"] and
         {s["name"]: s["source_rect_bottom_left"] for s in atlas_source["sprites"]} ==
         {"btn_hud_1008": [721, 575, 88, 88], "btn_hud_1004": [1, 435, 88, 88]},
         "Placement popup source manifest geometry/sprites changed")
    base = data["authorized_hall_prerequisite_and_placement_baseline"]
    need((base["authorized_hall_cost_food"], base["authorized_hall_cost_wood"],
          base["authorized_hall_duration_seconds"], base["authorized_hall_power_delta"],
          base["hall_completion_events"]) == (3500, 3500, 2, 14, 1),
         "Hall prerequisite deviated from source")
    need((base["wallet_food"], base["wallet_wood"], base["wallet_stone"],
          base["wallet_gold"]) == (1500, 1500, 1500, 600),
         "Hall wallet baseline differs")
    need(base["initial_source_rows"] == 26 and base["initial_runtime_actors"] == 24 and
         base["initial_unity_init_tagged_actors"] == 24 and
         base["active_upgrade_queues"] == base["active_local_tickets"] == 0 and
         base["owned_queues"] == 1, "Initial state and permanent worker conflated")
    need(base["server_local_only_run"] is True and
         base["server_http_dispatch_attempts"] == 0 and
         base["server_suppressed_local_requests"] == 0,
         "Server boundary was not zero at the Hall2 placement baseline")
    phases = data["captures"]
    need([c["phase_name"] for c in phases] == CAPTURE_PHASES, "Capture order differs")
    states: dict[str, dict] = {}
    for entry in phases:
        name = entry["phase_name"]
        png = Path(entry["file"]).resolve()
        need(png == run / f"{name}.png" and png.is_file(),
             f"Screenshot missing or escaped run directory: {name}")
        need(png.stat().st_size == entry["file_bytes"] > 10000 and
             png.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n" and
             digest(png) == entry["png_sha256"].lower(),
             f"Screenshot bytes/hash invalid: {name}")
        need(entry["pre_capture_ready_frames"] >= 2 and
             entry["post_capture_ready_frames"] >= 2 and
             entry["post_capture_render_ready"]["observation_frame"] >=
             entry["processed"]["frame"] + 2,
             f"Screenshot lacked natural render frames: {name}")
        for field in ("pre_capture_render_ready", "post_capture_render_ready"):
            ready = entry[field]
            need(ready["global_compile_remaining_read_only"] == 0 and
                 ready["ghost_expected"] == ready["ghost_present"],
                 f"Assets or ghost changed during capture: {name}/{field}")
            need(ready["placement_menu_expected"] is (name in MENU_PHASES),
                 f"Placement menu readiness scope differs: {name}/{field}")
            if name in MENU_PHASES:
                menu = ready["placement_menu"]
                need(menu["ready"] and menu["geometry_ready"] and
                     menu["brush_ready"] and menu["projection_ready"] and
                     menu["stable_geometry_frames"] >= 2 and
                     menu["observed_frame"] == ready["observation_frame"],
                     f"Placement menu did not settle on source assets: {name}/{field}")
                verify_menu(menu, disabled=name == "02b_farm_invalid_gray_preview")
            if name in (PHASES[4], PHASES[5], PHASES[9]):
                panel = ready["selected_panel"]
                need(ready["selected_panel_expected"] and panel["ready"] and
                     panel["menu_widget"] == "BuildingMenuRoot" and
                     panel["title_widget"] == "BuildingTitleRoot" and
                     panel["widgets_visible"] and
                     panel["source_final_animation_sample"] and
                     panel["cached_geometry_valid"] and
                     panel["stable_geometry_frames"] >= 2 and
                     panel["first_stable_frame"] < panel["observed_frame"] ==
                     ready["observation_frame"] and
                     abs(panel["menu_opacity"] - panel["source_final_opacity"]) <= .001 and
                     abs(panel["title_scale_x"] - panel["source_final_title_scale"]) <= .001 and
                     abs(panel["title_scale_y"] - panel["source_final_title_scale"]) <= .001 and
                     panel["menu_width"] > 0 and panel["menu_height"] > 0 and
                     panel["title_width"] > 0 and panel["title_height"] > 0 and
                     all(math.isfinite(panel[k]) for k in (
                         "menu_center_x", "menu_center_y", "title_center_x", "title_center_y")),
                     f"Selected HUD menu/title animation or geometry had not settled: {name}/{field}")
            else:
                need(not ready["selected_panel_expected"],
                     f"Non-selected capture unexpectedly waited for HUD menu: {name}/{field}")
            if name in PHASES[2:]:
                actor = ready["construction_actor"]
                need(ready["construction_actor_expected"] and
                     ready["construction_actor_present"] and
                     ready["distinct_target_render_frame"] and
                     actor["mesh_ready"] and actor["material_ready"] and
                     actor["component_ready"] and actor["textures_ready"] and
                     actor["rendered_after_targeted_compile"] and
                     actor["last_render_time_on_screen"] > 0 and
                     actor["textures"] and all(t["ready"] for t in actor["textures"]),
                     f"Constructed Farm/Wood did not render on distinct frames: {name}/{field}")
            if name in (PHASES[5], PHASES[9]):
                preparation = entry["selection_arrow_asset_preparation"]
                before = preparation["before"]
                after = preparation["after"]
                need(preparation["scope"] ==
                     "four selected indicator component meshes, materials and their used textures only" and
                     preparation["indicator_actor_count"] == 1 and
                     preparation["arrow_component_count"] == 4 and
                     preparation["unique_referenced_asset_count"] >= 5 and
                     preparation["targeted_compile_finished"] and
                     preparation["preparation_wall_seconds"] >= 0 and
                     preparation["finished_frame"] >= preparation["started_frame"] and
                     len(before) == len(after) == 4 and
                     [a["name"] for a in before] == [a["name"] for a in after] and
                     all(a["mesh"] and a["material"] and
                         not a["mesh_compiling"] and a["mesh_render_data_ready"] and
                         a["material_complete"] and a["used_textures"] and
                         all(not t["compiling"] and t["resource_ready"]
                             for t in a["used_textures"])
                         for a in after),
                     f"Selected source arrows lacked scoped asset preparation: {name}")
                arrows = ready["selection_arrow_components"]
                need(ready["selection_arrows_expected"] and
                     ready["selection_indicator_count"] == 1 and
                     ready["selection_rendered_arrow_count"] == 4 and
                     ready["selection_earliest_arrow_render_time"] > 0 and
                     len(arrows) == 4 and
                     all(a["ready_and_rendered_after_selection"] and
                         a["render_state_created"] and a["mesh_ready"] and
                         a["material_ready"] and a["rendered_after_selection"] and
                         a["recently_rendered"] and
                         a["selection_ready_observed_world_seconds"] >=
                         preparation["finished_world_seconds"] and
                         a["last_render_time_on_screen"] >
                         preparation["finished_world_seconds"]
                         for a in arrows),
                     f"Selected source arrows lacked real rendered frames: {name}/{field}")
            if name == PHASES[4]:
                need(ready["selection_no_arrows_expected"] and
                     not ready["selection_arrows_expected"] and
                     ready["selection_indicator_count"] == 1 and
                     ready["selection_indicator_hidden"] and
                     ready["selection_camera_city_dxf"] > 250,
                     f"Far-zoom Farm image did not preserve source arrow suppression: {field}")
        s = entry["processed"]
        states[name] = s
        need(s["hall_completion_events"] == 1 and s["placement_baseline_frozen"] and
             s["server_local_only_run"] is True and
             s["server_http_dispatch_attempts"] == 0 and
             s["server_suppressed_local_requests"] == 0 and
             s["task_sha256"] == base["task_sha256"] and
             s["profile_authority_normalized_sha256"] == base["profile_sha256"] and
             s["upgrade_queues"] == 0 and s["owned_queues"] == 1 and
             s["unity_init_tagged_actor_count"] == 24 and
             s["new_unity_init_tagged_actor_count"] == 0,
             f"Task, profile authority or initial city mutated in {name}")
    # The wallet moves at each UI confirmation, never at ghost placement, selection or completion.
    expected = [
        (1500, 1500, 0, 14, 0, 0),
        (1500, 1500, 0, 14, 0, 0),
        (1500, 1500, 0, 14, 0, 0),
        (1500, 1400, 1, 14, 1, 0),
        (1500, 1400, 0, 19, 1, 1),
        (1500, 1400, 0, 19, 1, 1),
        (1500, 1400, 0, 19, 1, 1),
        (1500, 1400, 0, 19, 1, 1),
        (1400, 1400, 1, 19, 2, 1),
        (1400, 1400, 0, 24, 2, 2),
        (1400, 1400, 0, 24, 2, 2),
    ]
    for entry, (food, wood, active, power, new_actors, formal) in zip(phases, expected):
        s = states[entry["phase_name"]]
        need((s["food"], s["wood"], s["stone"], s["gold"]) ==
             (food, wood, 1500, 600), f"Unexpected fee in {entry['phase_name']}")
        need((s["local_construction_tickets"], s["power_delta"],
              s["new_source_actor_count"], s["formal_constructed_tagged_actor_count"]) ==
             (active, power, new_actors, formal),
             f"Queue/power/actor transition differs in {entry['phase_name']}")
        need(s["profile_local_power"] == power,
             f"Profile projection did not track source local power in {entry['phase_name']}")
    farm_preview = states[PHASES[1]]
    farm_invalid = states["02b_farm_invalid_gray_preview"]
    wood_preview = states[PHASES[6]]
    for state, kind in ((farm_preview, 2), (wood_preview, 3)):
        need(state["active"] and not state["dragging"] and state["grid_valid"] and
             state["can_commit"] and state["type_id"] == kind and
             state["ghost_actor_count"] == 1 and not state["camera_input_suspended"],
             f"Invalid ordinary HUD preview for type {kind}")
    need(farm_invalid["active"] and not farm_invalid["dragging"] and
         not farm_invalid["grid_valid"] and not farm_invalid["can_commit"] and
         farm_invalid["type_id"] == 2 and farm_invalid["ghost_actor_count"] == 1 and
         farm_invalid["confirm_click_count"] == 0 and
         farm_invalid["local_construction_tickets"] == 0 and
         farm_invalid["new_source_actor_count"] == 0 and
         not farm_invalid["camera_input_suspended"],
         "Invalid gray preview did not preserve ghost-only source state")
    for name in (PHASES[2], PHASES[3], PHASES[4], PHASES[5],
                 PHASES[7], PHASES[8], PHASES[9]):
        s = states[name]
        need(not s["active"] and s["ghost_actor_count"] == 0 and
             not s["camera_input_suspended"], f"Ghost/camera leaked in {name}")
    check_ticket(states[PHASES[2]], "farm_ticket", kind=2, completed=False,
                 pending=True, food=0, wood=100)
    for name in (PHASES[3], PHASES[4], PHASES[5],
                 PHASES[6], PHASES[7], PHASES[8], PHASES[9]):
        check_ticket(states[name], "farm_ticket", kind=2, completed=True,
                     pending=False, food=0, wood=100)
    check_ticket(states[PHASES[7]], "wood_ticket", kind=3, completed=False,
                 pending=True, food=100, wood=0)
    for name in (PHASES[8], PHASES[9]):
        check_ticket(states[name], "wood_ticket", kind=3, completed=True,
                     pending=False, food=100, wood=0)
    for kind, first_phase, sample_key, ticket_key in (
        (2, PHASES[3], "farm_first_completion_production", "farm_ticket"),
        (3, PHASES[8], "wood_first_completion_production", "wood_ticket"),
    ):
        state = states[first_phase]
        sample = state[sample_key]
        ticket = state[ticket_key]
        whole = max(0, math.floor(sample["world_seconds"]) -
                    math.ceil(sample["ticket_finishes_at"]))
        amount = min(4000.0, whole * 400.0 / 3600.0)
        need(sample["first_completion_event_sample"] and
             sample["query_success"] and sample["query_error"] == "" and
             sample["id"] == ticket["id"] and
             sample["ticket_finishes_at"] == ticket["finishes_at_world_seconds"] and
             sample["world_seconds"] >= sample["ticket_finishes_at"] and
             sample["world_seconds"] <= state["world_time_seconds"] and
             sample["observed_frame"] <= state["frame"] and
             sample["source_rate_per_hour"] == 400 and
             sample["source_gather_min"] == 66 and
             sample["source_whole_elapsed_seconds"] == whole and
             abs(sample["expected_accrued_amount"] - amount) < 1e-5 and
             abs(sample["accrued_amount"] - amount) < 1e-5 and
             sample["accrued_floor"] == math.floor(amount) and
             not sample["can_collect"] and not sample["at_capacity"] and
             amount < 66,
             f"Type {kind} initial production query did not match source clock")
    need(not states[PHASES[2]]["selection_is_farm"] and
         not states[PHASES[7]]["selection_is_wood"],
         "Pending construction became selectable")
    need(states[PHASES[5]]["selection_is_farm"] and
         states[PHASES[4]]["selection_is_farm"] and
         states[PHASES[9]]["selection_is_wood"],
         "Completed actors not selected by ordinary world input")
    actions = data["actions"]
    verify_hover_clear_actions(actions)
    names = [a["action"] for a in actions]
    need(UI_ACTIONS.issubset(names) and WORLD_ACTIONS.issubset(names) and
         "invalid_disabled_confirm_pointer" in names and
         "farm_repeat_confirm_pointer" in names and
         "wood_repeat_confirm_pointer" in names and
         "farm_zoomed_out_selection_no_arrows" in names and
         "farm_zoom_source_lod_settled" in names and
         "farm_zoomed_in_selection_arrows" in names and
         "wood_selection_arrows" in names,
         "Missing ordinary UMG/world input actions")
    zoom_out = [a for a in actions if a["action"] ==
                "farm_zoomed_out_selection_no_arrows"]
    need(len(zoom_out) == 1 and
         zoom_out[0]["expected_visible_arrow_count"] == 0 and
         zoom_out[0]["camera_city_dxf"] > 250 and
         zoom_out[0]["source_unity_lod_scale"] > 1 and
         not zoom_out[0]["actor_can_show_move_arrows"] and
         zoom_out[0]["indicator_actor_count"] == 1 and
         not zoom_out[0]["indicator_visible"] and
         zoom_out[0]["visible_arrow_component_count"] == 0 and
         zoom_out[0]["arrow_components"] == [] and
         zoom_out[0]["full_source_state"]["selection_is_farm"] and
         zoom_out[0]["full_source_state"]["camera_city_dxf"] ==
         zoom_out[0]["camera_city_dxf"] and
         zoom_out[0]["observed_frame"] < states[PHASES[4]]["frame"] and
         states[PHASES[4]]["camera_city_dxf"] > 250,
         "Source far-zoom Farm selection/arrow suppression differs")
    wheels = [a for a in actions if a["action"] == "farm_source_viewport_wheel_up"]
    need(1 <= len(wheels) <= 12 and
         wheels[0]["sent_frame"] > states[PHASES[4]]["frame"] and
         all(a["method"] ==
             "SlateApplication::OnMouseWheel positive delta to actual SViewport" and
             a["wheel_delta"] == 1 and a["viewport_hit_path"] and
             not a["umg_hit_path"] and not a["slate_mouse_captured"] and
             a["slate_wheel_handled"] and
             a["observed_frame"] > a["sent_frame"] and
             a["dxf_after_game_frame"] < a["dxf_before"] - .01
             for a in wheels) and
         all(next_wheel["sent_frame"] > wheel["observed_frame"]
             for wheel, next_wheel in zip(wheels, wheels[1:])),
         "Farm camera zoom did not follow distinct real Slate viewport wheel frames")
    settled = [a for a in actions if a["action"] == "farm_zoom_source_lod_settled"]
    need(len(settled) == 1 and
         settled[0]["wheel_event_count"] == len(wheels) and
         settled[0]["observed_frame"] >= wheels[-1]["observed_frame"] and
         settled[0]["camera_city_dxf"] <= 245 and
         settled[0]["farm_unity_lod_scale"] <= 1.0001 and
         states[PHASES[5]]["farm_zoom_wheel_event_count"] == len(wheels) and
         states[PHASES[5]]["farm_zoom_completed_dxf"] <= 245 and
         states[PHASES[5]]["camera_city_dxf"] <= 245 and
         states[PHASES[9]]["camera_city_dxf"] <= 245,
         "Source near-zoom/LOD threshold was not reached before selected screenshots")
    for action in actions:
        if action["action"] in UI_ACTIONS:
            need(action["method"] == "Slate MoveAway+Move+Down+Up" and
                 action["target_in_hit_path"] and action["target_hovered"] and
                 action["down_handled"] and action["up_handled"],
                 f"UMG target was not hit: {action['action']}")
        if action["action"] in ("farm_confirm_ordinary_hud", "wood_confirm_ordinary_hud"):
            need(action["gesture_contract"].startswith("one real Slate UI press/release") and
                 all(route["hit_path_valid"] and route["hit_path_contains_umg"] and
                     route["hit_path_widget_types"][-1] == "SButton" and
                     route["os_cursor_hits_own_window"] and
                     route["game_hwnd_owned_by_process"] and
                     not route["viewport_has_mouse_capture"]
                     for route in (action["before_route"], action["down_route"],
                                   action["up_route"])) and
                 action["primary_press_before"] ==
                 action["primary_press_after_down"] == action["primary_press_after_up"] and
                 action["primary_press_outcome_before"] ==
                 action["primary_press_outcome_after_down"] ==
                 action["primary_press_outcome_after_up"] and
                 action["selection_after_down"] == action["selection_before"] and
                 action["selection_after_up"] ==
                 action["selection_after_next_game_frame"] == "" and
                 (not action["building_menu_visible_after_up"] or
                  action["old_selected_panel_closing_after_up"]) and
                 (not action["old_selected_panel_closing_after_up"] or
                  (action["selected_panel_visibility_before"] == 4 and
                   action["selected_panel_visibility_after_up"] == 3 and
                   action["selection_before"] != "")) and
                 (not action["selected_panel_visible_after_next_game_frame"] or
                  (action["old_selected_panel_closing_after_up"] and
                   action["selected_panel_visibility_after_next_game_frame"] == 3)) and
                 not action["selected_panel_visible_after_close"] and
                 action["selected_panel_visibility_after_close"] == 1 and
                 0 <= action["selected_panel_root_opacity_before"] <= 1 and
                 0 <= action["selected_panel_root_opacity_after_up"] <= 1 and
                 (not action["old_selected_panel_closing_after_up"] or
                  action["selected_panel_root_opacity_after_close"] <= .001) and
                 action["observed_after_frame"] <= action["selected_panel_close_end_frame"] and
                 (not action["old_selected_panel_closing_after_up"] or
                  .20 <= action["selected_panel_close_end_wall_elapsed_seconds"] <= 1.0) and
                 action["confirm_count_after_up"] == action["confirm_count_before"] + 1 and
                 action["confirm_count_after_next_game_frame"] == action["confirm_count_after_up"] and
                 action["active_tickets_after_up"] == action["active_tickets_before"] + 1 and
                 action["active_tickets_after_next_game_frame"] == action["active_tickets_after_up"] and
                 action["wallet_food_after_up"] <= action["wallet_food_before"] and
                 action["wallet_wood_after_up"] <= action["wallet_wood_before"] and
                 action["wallet_food_after_next_game_frame"] == action["wallet_food_after_up"] and
                 action["wallet_wood_after_next_game_frame"] == action["wallet_wood_after_up"] and
                 action["primary_press_after_next_game_frame"] == action["primary_press_before"] and
                 action["observed_after_frame"] > action["sent_frame"],
                 f"Same confirm gesture leaked to world selection or duplicated construction: {action['action']}")
        if action["action"] in WORLD_ACTIONS:
            need(action["viewport_hit_path"] and not action["ui_hit_path"],
                 f"World pointer was routed through UI: {action['action']}")
        if action["action"] in ("farm_zoomed_in_selection_arrows", "wood_selection_arrows"):
            need(action["actor_can_show_move_arrows"] and
                 action["camera_city_dxf"] <= 245 and
                 action["source_unity_lod_scale"] <= 1.0001 and
                 action["expected_visible_arrow_count"] == 4 and
                 action["indicator_actor_count"] == 1 and
                 action["indicator_visible"] and
                 action["visible_arrow_component_count"] == 4 and
                 len(set(action["arrow_components"])) == 4,
                 f"Selected building move-arrow feedback missing: {action['action']}")
    for label, phase, count, food, wood in (
        ("farm_repeat_confirm_pointer", PHASES[2], 1, 1500, 1400),
        ("wood_repeat_confirm_pointer", PHASES[7], 2, 1400, 1400),
    ):
        matches = [a for a in actions if a["action"] == label]
        need(len(matches) == 1, f"Missing or repeated pointer action: {label}")
        action = matches[0]
        need(action["confirm_count_before"] ==
             action["confirm_count_after_immediate"] ==
             action["confirm_count_after_next_game_frame"] ==
             states[phase]["confirm_click_count"] == count and
             action["wallet_food_before"] ==
             action["wallet_food_after_immediate"] ==
             action["wallet_food_after_next_game_frame"] ==
             states[phase]["food"] == food and
             action["wallet_wood_before"] ==
             action["wallet_wood_after_immediate"] ==
             action["wallet_wood_after_next_game_frame"] ==
             states[phase]["wood"] == wood and
             action["sent_frame"] < action["observed_after_frame"] < states[phase]["frame"],
             f"Disabled/repeat pointer mutated count or wallet: {label}")
        need(action["gesture_contract"].startswith("separate later tap") and
             action["active_tickets_before"] == action["active_tickets_after_immediate"] ==
             action["active_tickets_after_next_game_frame"] and
             (action["confirm_enabled_in_hit_path"] is action["placement_menu_closing_before"]) and
             (action["placement_menu_closing_before"] or
              action["old_selected_panel_closed_before_repeat"]) and
             0 <= action["placement_menu_root_opacity_before"] <= 1 and
             (not action["placement_menu_closing_before"] or
              0 <= action["placement_menu_close_elapsed_before"] <= .25) and
             0 <= action["placement_menu_root_opacity_after_up"] <= 1 and
             (not action["placement_menu_closing_before"] or
              action["primary_press_before"] == action["primary_press_after_up"] ==
              action["primary_press_after_next_game_frame"] and
              action["selection_before"] == action["selection_after_up"] ==
              action["selection_after_next_game_frame"] == ""),
             f"Separate Close-phase tap duplicated construction or leaked to world: {label}")
        if action["placement_menu_closing_before"]:
            confirm_name = "farm_confirm_ordinary_hud" if label.startswith("farm_") else "wood_confirm_ordinary_hud"
            confirms = [a for a in actions if a["action"] == confirm_name]
            need(len(confirms) == 1, f"Missing first confirm gesture: {confirm_name}")
            first = confirms[0]
            need(all(abs(action["placement_menu_last_anchor_before"][axis] -
                         first["placement_menu_last_anchor_after_up"][axis]) < .1 and
                     abs(action["placement_menu_last_anchor_after_up"][axis] -
                         action["placement_menu_last_anchor_before"][axis]) < .1 and
                     abs(action["placement_menu_render_scale_before"][axis] -
                         first["placement_menu_render_scale_after_up"][axis]) < .01
                     for axis in ("x", "y")),
                 f"Closing placement menu did not retain its last source anchor: {label}")
    invalid = [a for a in actions if a["action"] == "invalid_disabled_confirm_pointer"]
    need(len(invalid) == 1, "Missing or repeated disabled confirm pointer")
    disabled = invalid[0]
    need(farm_preview["frame"] < farm_invalid["frame"] < disabled["sent_frame"] <
         disabled["observed_after_frame"] < states[PHASES[2]]["frame"] and
         disabled["confirm_count_before"] ==
         disabled["confirm_count_after_immediate"] ==
         disabled["confirm_count_after_next_game_frame"] ==
         farm_preview["confirm_click_count"] == 0 and
         disabled["wallet_food_before"] ==
         disabled["wallet_food_after_immediate"] ==
         disabled["wallet_food_after_next_game_frame"] ==
         farm_preview["food"] == 1500 and
         disabled["wallet_wood_before"] ==
         disabled["wallet_wood_after_immediate"] ==
         disabled["wallet_wood_after_next_game_frame"] ==
         farm_preview["wood"] == 1500 and
         disabled["active_tickets_before"] ==
         disabled["active_tickets_after_immediate"] ==
         disabled["active_tickets_after_next_game_frame"] ==
         farm_preview["local_construction_tickets"] == 0 and
         disabled["invalid_preview_active_after_next_game_frame"] is True,
         "Disabled confirm changed controller, wallet, ticket or preview next frame")
    verify_disabled_pointer_boundary(disabled)
    need(states[PHASES[2]]["confirm_click_count"] ==
         farm_preview["confirm_click_count"] + 1,
         "Farm confirm did not reach Controller exactly once after disabled attempt")
    need(states[PHASES[7]]["confirm_click_count"] ==
         states[PHASES[2]]["confirm_click_count"] + 1 and
         states[PHASES[7]]["last_confirm_outcome"] == "committed",
         "Wood confirm was missing or duplicated")
    need(data["final_state"]["selection_is_wood"] and
         data["final_state"]["local_construction_tickets"] == 0,
         "Final Wood selection or queue differs")
    if data["gamma_census_invoked"]:
        need(isinstance(data["gamma_census_returned"], bool) and
             isinstance(data["gamma_census_manifest_path"], str),
             "Optional read-only Gamma census result missing")
    print(f"PASS: local Farm/Wood ordinary HUD construction, {len(phases)} PNGs, "
          f"run={run}")


if __name__ == "__main__":
    main()
