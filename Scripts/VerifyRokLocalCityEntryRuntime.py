"""Verify the isolated GameApp BuildCity card-to-placement handoff audit."""

import argparse
import csv
import hashlib
import json
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / "Saved/RokUnityMigration/LocalCityEntryAudit"
CAPTURES = (
    "01_buildcity_source_cards",
    "02_unbound_card_keeps_screen",
    "03_audit_listener_handoff",
)
BUTTONS = (
    "BuildCityButton",
    "BuildCityCardButton_2_unbound",
    "BuildCityCardButton_2_audit_listener",
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(run: Path) -> dict:
    manifest_path = run / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    assert manifest["schema"] == "rok.local_city_entry.runtime.v1"
    assert manifest["complete"] and manifest["passed"]
    assert manifest["audit_flag"] == "-RokLocalCityEntryAudit"
    assert Path(manifest["project"]).resolve() == ROOT.resolve()
    assert manifest["world"] == "/Game/Maps/L_RokUnityGameAppCity"
    assert manifest["listener_audit_only"] is True
    assert manifest["placement_implemented"] is False
    assert not manifest["failures"]
    assert manifest["expected_capture_count"] == len(CAPTURES)
    assert manifest["callback_count"] == 1
    assert manifest["callback_type_id"] == 2
    assert manifest["callback_observed_screen_closed"] is True
    assert "Production placement and visual factory are not implemented" in manifest["scope"]

    for relative, record in manifest["input_hashes"].items():
        path = ROOT / relative
        assert path.is_file(), relative
        assert path.stat().st_size == record["bytes"], relative
        assert sha256(path) == record["sha256"], relative

    baseline = manifest["baseline"]
    source = json.loads((ROOT / "Saved/RokUnityMigration/unity_initial_city_state.json").read_text(
        encoding="utf-8-sig"))
    scene = json.loads((ROOT / "Saved/RokUnityMigration/unity_gameapp_city_runtime_scene_manifest.json").read_text(
        encoding="utf-8-sig"))
    readback = json.loads((ROOT / "Saved/RokUnityMigration/BatchReview20261009/current_city_pose_readback.json").read_text(
        encoding="utf-8-sig"))
    source_rows = {row["init_id"]: row for row in source["buildings"]}
    actor_rows = {row["init_id"]: row for row in scene["building_actors"]}
    boundary_ids = {key for key, row in source_rows.items()
                    if row["visual"]["representation"] == "runtime_boundary_mesh_collection"}
    assert source["building_count"] == len(source_rows) == 26
    assert scene["source_initial_building_count"] == 26
    assert scene["source_skipped_boundary_building_count"] == len(boundary_ids) == 2
    assert boundary_ids == {1, 7}
    assert scene["buildings_spawned"] == len(actor_rows) == 24
    assert set(actor_rows) == set(source_rows) - boundary_ids
    assert readback["map_saved"] is False and len(readback["source_actors"]) == 24
    assert readback["world"].startswith("/Game/Maps/L_RokUnityGameAppCity.")
    assert sha256(ROOT / "Content/Maps/L_RokUnityGameAppCity.umap") == readback["map_sha256"]
    bound_evidence = baseline["runtime_bound_actor_evidence"]
    assert len(bound_evidence) == 24

    def tagged_int(tags, prefix):
        values = [int(tag[len(prefix):]) for tag in tags if tag.startswith(prefix)]
        assert len(values) == 1, (prefix, tags)
        return values[0]

    actual_ids = set()
    for actor in readback["source_actors"]:
        tags = actor["tags"]
        assert "RokUnityConfigInitBuilding" in tags
        assert "RokUnityRuntimeBuildingSource" in tags
        init_id = tagged_int(tags, "UnityInitId_")
        assert init_id in actor_rows and init_id not in actual_ids
        actual_ids.add(init_id)
        expected = actor_rows[init_id]
        source_row = source_rows[init_id]
        assert tagged_int(tags, "UnityBuildingType_") == expected["building_type"] == source_row["type_id"]
        assert tagged_int(tags, "UnityBuildingLevel_") == expected["level"] == source_row["level"]
        assert tagged_int(tags, "UnityGridX_") == expected["grid_x"] == source_row["grid"]["x"]
        assert tagged_int(tags, "UnityGridY_") == expected["grid_y"] == source_row["grid"]["y"]
        actor_evidence = bound_evidence[str(init_id)]
        assert actor_evidence["runtime_name"] == actor["name"]
        assert actor_evidence["source_identity"] == (
            f"type={expected['building_type']};level={expected['level']};"
            f"grid={expected['grid_x']},{expected['grid_y']}")
        assert len(actor_evidence["state_sha256"]) == 64
    assert actual_ids == set(actor_rows)
    assert baseline["source_initial_building_count"] == len(source_rows)
    assert baseline["runtime_bound_building_actor_count"] == len(actual_ids)
    assert set(baseline["runtime_bound_init_ids"]) == actual_ids
    assert set(baseline["boundary_mesh_source_init_ids"]) == boundary_ids
    assert baseline["active_local_construction_count"] == 0
    assert baseline["owned_build_queue_count"] >= 1
    assert baseline["active_upgrade_queue_count"] >= 0
    for name in ("profile_sha256", "resources_sha256"):
        assert len(baseline[name]) == 64

    previews = manifest["source_entry_previews"]
    assert set(previews) == {"2", "3", "13"}
    with (ROOT / "ref/config-tables/编译客户端配置/CSV/BuildingLevelData.csv").open(
        encoding="utf-8-sig", newline=""
    ) as stream:
        rows = {row["type"]: row for row in csv.DictReader(stream)
                if row["type"] in previews and row["level"] == "1"}
    assert set(rows) == set(previews)
    for type_id, expected_prereq in (("2", 0), ("3", 2), ("13", 4)):
        row = rows[type_id]
        preview = previews[type_id]
        assert int(row["width"]) == preview["footprint_width"]
        assert int(row["length"]) == preview["footprint_length"]
        assert int(row["wood"]) == preview["cost_wood"]
        assert int(row["food"]) == preview["cost_food"]
        assert int(row["buildingTime"]) == preview["build_seconds"]
        assert int(row["reqLevel1"]) == expected_prereq
        assert int(row["reqType1"]) == (1 if expected_prereq else 0)
    farm = previews["2"]
    assert farm["context_verified"] and farm["source_eligible"]
    assert not farm["can_commit"] and not farm["position_evaluated"]
    assert farm["missing_type_id"] == 0
    assert (farm["footprint_width"], farm["footprint_length"]) == (5, 5)
    assert farm["cost_wood"] == 100 and farm["build_seconds"] == 2
    assert farm["visual_civilization"] > 0 and farm["resolved_age"] > 0
    assert farm["source_model_id"]
    for type_id, level in (("3", 2), ("13", 4)):
        preview = previews[type_id]
        assert preview["context_verified"] and not preview["source_eligible"]
        assert not preview["can_commit"] and not preview["position_evaluated"]
        assert preview["missing_type_id"] == 1
        assert preview["required_level"] == level and preview["current_level"] == 1
        assert str(level) in preview["failure_reason"]

    actions = manifest["actions"]
    assert len(actions) == len(BUTTONS)
    for action, name in zip(actions, BUTTONS):
        assert action["button"] == name
        assert action["method"] == "Slate MoveAway+Move+Down+Up"
        assert action["target_in_hit_path"] and action["target_hovered"]
        assert action["down_handled"] and action["up_handled"]

    captures = manifest["captures"]
    assert len(captures) == len(CAPTURES)
    for row, name in zip(captures, CAPTURES):
        assert row["phase_name"] == name
        path = Path(row["file"])
        assert path.resolve() == (run / f"{name}.png").resolve()
        assert path.stat().st_size == row["file_bytes"] > 0
        assert sha256(path) == row["png_sha256"]
        with Image.open(path) as image:
            assert image.format == "PNG" and image.size == (1440, 810)
        assert row["request"]["state_unchanged"]
        assert row["processed"]["state_unchanged"]
    initial, unbound, handoff = (row["processed"] for row in captures)
    assert initial["screen_open"] and initial["build_city_mode"]
    assert initial["callback_count"] == 0 and not initial["audit_listener_bound"]
    assert unbound["screen_open"] and unbound["build_city_mode"]
    assert unbound["selected_type_id"] == 2
    assert unbound["callback_count"] == 0 and not unbound["audit_listener_bound"]
    assert "该建筑暂时无法放置" in unbound["feedback"]
    assert not handoff["screen_open"] and not handoff["build_city_mode"]
    assert handoff["callback_count"] == 1 and handoff["callback_type_id"] == 2
    assert handoff["callback_observed_screen_closed"]
    assert handoff["audit_listener_bound"]

    screen = (ROOT / "Source/Rok/RokUnityFeatureScreenWidget.cpp").read_text(encoding="utf-8-sig")
    branch = screen[screen.index("const bool bPlacementTarget = TypeId == 2"):
                    screen.index("Rok build city route: placement boundary")]
    assert "PreviewLocalConstructionEntry(" in branch
    assert branch.index("!OnBuildCityPlacementRequested.IsBound()") < branch.index("CloseScreen();")
    assert branch.index("CloseScreen();") < branch.index("OnBuildCityPlacementRequested.Broadcast(TypeId);")
    assert "StartLocalConstruction(" not in branch
    print(f"PASS: {run} | 3 real Slate clicks, 3 PNGs, Type2 audit-only close-before-broadcast")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path, help="Specific LocalCityEntryAudit run directory")
    args = parser.parse_args()
    candidates = [args.run] if args.run else sorted(RUNS.glob("*_*/"), reverse=True)
    assert candidates, f"No LocalCityEntryAudit runs under {RUNS}"
    verify(candidates[0])


if __name__ == "__main__":
    main()
