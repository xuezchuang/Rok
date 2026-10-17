#!/usr/bin/env python3
"""Check source-derived placement popup geometry/brushes in an immutable runtime run.

This validates metadata and PNG provenance, not visual pixel equality to Unity.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from VerifyRokLocalConstructionRuntime import (
    CAPTURE_PHASES, EVIDENCE, MENU_PHASES, ROOT, digest, need, verify_menu,
    verify_menu_revision,
    verify_hover_clear_actions,
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", required=True, type=Path)
    args = parser.parse_args()
    run = args.run.resolve()
    need(run.parent == EVIDENCE.resolve(), "Run escaped LocalConstructionRuntimeAudit")
    path = run / "manifest.json"
    need(path.is_file(), "Missing runtime manifest")
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    need(data["schema"] == "rok.local_construction.runtime_audit.v1" and
         data["complete"] is True and data["passed"] is True and
         data["failures"] == [], "Placement runtime did not complete cleanly")
    hashes = data["inputs_sha256"]
    atlas_asset_path = "Content/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud.uasset"
    import_report_path = "Saved/RokUnityMigration/PlacementUiAtlas/20261008_221417_249217_45548/ue_import_report.json"
    static_source_path = "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_hud_ui_static.json"
    revision_source_path = "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_hud_ui_revision2.json"
    revision3_source_path = "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_hud_ui_revision3.json"
    inventory_source_path = "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_ui_asset_inventory.json"
    source_paths = (
        "Source/Rok/RokMainHudWidget.h",
        "Source/Rok/RokMainHudWidget.cpp",
        "Source/Rok/RokUiWindowAnimation.h",
        "Source/Rok/RokUiWindowAnimation.cpp",
        atlas_asset_path,
        "Saved/RokUnityMigration/PlacementUiAtlas/20261008_221417_249217_45548/atlas_manifest.json",
        import_report_path,
        "Content/RokPrototype/UI/PlacementAtlases/M_RokPlacementGrayUI.uasset",
        static_source_path,
        revision_source_path,
        revision3_source_path,
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_close_input_source.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_animation_source_inventory.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_previous_selection_source.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_gray_render_diagnosis.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_source.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_source_v2.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_source_v3.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/PlacementGrayMaterial/20261008T230559679259Z_46968_ebba69ea/create.json",
        "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_root_create.json",
        inventory_source_path,
    )
    for relative in source_paths:
        source = ROOT / relative
        need(relative in hashes and source.is_file() and
             digest(source) == hashes[relative].lower(),
             f"Placement source or imported asset changed: {relative}")
    static = json.loads((ROOT / static_source_path).read_text(encoding="utf-8-sig"))
    revision = json.loads((ROOT / revision_source_path).read_text(encoding="utf-8-sig"))
    revision3 = json.loads((ROOT / revision3_source_path).read_text(encoding="utf-8-sig"))
    verify_menu_revision(revision, revision3, hashes)
    inventory = json.loads((ROOT / inventory_source_path).read_text(encoding="utf-8-sig"))
    import_report = json.loads((ROOT / import_report_path).read_text(encoding="utf-8-sig"))
    need(static["schema"] == "rok.placement_menu_hud_ui_static.v1" and
         inventory["schema"] == "rok.placement_ui_asset_inventory.v1" and
         static["source_geometry"]["menu_root_size"] == [472.7, 220] and
         inventory["source_atlas"]["size"] == [1024, 1024] and
         import_report["schema"] == "rok.placement_ui_atlas_import.v1" and
         import_report["passed"] is True and
         import_report["asset_uasset_sha256"] ==
         hashes[atlas_asset_path] and
         {s["name"]: s["source_rect_bottom_left"] for s in inventory["sprites"]} ==
         {"btn_hud_1008": [721, 575, 88, 88], "btn_hud_1004": [1, 435, 88, 88]},
         "Placement source geometry or sprite rect changed")
    captures = data["captures"]
    verify_hover_clear_actions(data["actions"])
    need([entry["phase_name"] for entry in captures] == CAPTURE_PHASES,
         "Missing or reordered Farm/Wood placement screenshots")
    for entry in captures:
        name = entry["phase_name"]
        if name not in MENU_PHASES:
            continue
        png = Path(entry["file"]).resolve()
        need(png == run / f"{name}.png" and png.is_file() and
             png.stat().st_size == entry["file_bytes"] > 10000 and
             png.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n" and
             digest(png) == entry["png_sha256"].lower(),
             f"Placement screenshot provenance differs: {name}")
        need(entry["pre_capture_ready_frames"] >= 2 and
             entry["post_capture_ready_frames"] >= 2 and
             entry["processed"]["placement_menu"]["ready"],
             f"Placement popup lacked stable real UI state: {name}")
        for field in ("pre_capture_render_ready", "post_capture_render_ready"):
            ready = entry[field]
            need(ready["placement_menu_expected"],
                 f"Placement popup readiness not requested: {name}/{field}")
            menu = ready["placement_menu"]
            need(menu["ready"] and menu["stable_geometry_frames"] >= 2 and
                 menu["observed_frame"] == ready["observation_frame"],
                 f"Placement popup geometry did not settle: {name}/{field}")
            verify_menu(menu, disabled=name == "02b_farm_invalid_gray_preview")
    invalid = next(entry for entry in captures
                   if entry["phase_name"] == "02b_farm_invalid_gray_preview")
    state = invalid["processed"]
    need(state["active"] and not state["dragging"] and
         not state["grid_valid"] and not state["can_commit"] and
         state["ghost_actor_count"] == 1 and
         state["new_source_actor_count"] == state["local_construction_tickets"] == 0,
         "Gray popup screenshot did not show invalid ghost-only preview")
    print(f"PASS: placement popup cached geometry, source brushes and invalid gray PNG; run={run}")


if __name__ == "__main__":
    main()
