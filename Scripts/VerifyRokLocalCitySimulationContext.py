"""Verify the bounded local city identity against reports, raw CSV, and catalogs."""
import csv
import hashlib
import json

from BuildRokLocalCitySimulationContext import (
    ROOT, OUT, SOURCES, LEGACY_RESOLUTION, VISUAL_RESOLUTION, INITIAL_STATE,
    GRID_CATALOG, CONSTRUCTION_CATALOG, SOURCE_TABLES, build,
)
from rok_data_paths import label


REPORT = ROOT / "Saved/RokUnityMigration/LocalCitySimulationContext/verify.json"


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def source_rows(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        records = list(csv.DictReader(stream))
    assert records and records[0][next(iter(records[0]))] == "int"
    return records[1:]


def verify():
    manifest = read(OUT)
    assert manifest == build(write=False), "Source binding package is stale"
    assert manifest["schema"] == "rok.local_city_simulation_context.v1"
    assert manifest["mode"] == "local_simulation"
    assert manifest["target_level"] == "/Game/Maps/L_RokUnityGameAppCity"
    assert manifest["sources"] == {
        label(path): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in SOURCES}
    legacy, visual, initial, grid, construction = map(read, (
        LEGACY_RESOLUTION, VISUAL_RESOLUTION, INITIAL_STATE, GRID_CATALOG, CONSTRUCTION_CATALOG))
    variant = manifest["visual_variant"]
    assert variant["civilization"] == legacy["civ_type"] == visual["civ_type"] == 101
    assert variant["age"] == legacy["age"] == visual["age"] == 1
    assert variant["qualification"] == "source_visual_variant_not_account_identity"
    evidence = variant["evidence"]
    assert evidence["legacy_resolution_path"] == label(LEGACY_RESOLUTION)
    assert evidence["current_visual_resolution_path"] == label(VISUAL_RESOLUTION)
    assert (evidence["legacy_spawnable_count"], evidence["legacy_missing_count"],
            evidence["legacy_fallback_count"], evidence["current_failure_count"]) == (
                legacy["spawnable"], len(legacy["missing"]), len(legacy["fallbacks"]), len(visual["failures"]))
    assert (legacy["spawnable"], len(legacy["missing"]), len(legacy["fallbacks"]), len(visual["failures"])) == (12, 14, 4, 0)
    init_rows = source_rows(SOURCE_TABLES[0])
    assert len(init_rows) == len(initial["buildings"]) == len(visual["records"]) == len(grid["initial_occupancy"]) == 26
    assert sorted(int(row["ID"]) for row in init_rows) == list(range(1, 27))
    hall_csv = next(row for row in init_rows if int(row["ID"]) == 5)
    hall_state = next(row for row in initial["buildings"] if row["init_id"] == 5)
    hall_grid = next(row for row in grid["initial_occupancy"] if row["init_id"] == 5)
    hall_visual = next(row for row in visual["records"] if row["init_id"] == 5)
    assert tuple(int(hall_csv[key]) for key in ("type", "level", "posX", "posY")) == (1, 1, -5, 5)
    assert (hall_state["type_id"], hall_state["level"], hall_state["grid"]) == (1, 1, {"x": -5, "y": 5})
    assert (hall_grid["type"], hall_grid["level"], hall_grid["pos_x"], hall_grid["pos_y"]) == (1, 1, -5, 5)
    assert (hall_visual["building_type"], hall_visual["grid_x"], hall_visual["grid_y"]) == (1, -5, 5)
    assert manifest["initial_city"]["manifest_path"] == label(INITIAL_STATE)
    assert manifest["initial_city"]["building_count"] == 26
    assert manifest["initial_city"]["bootstrap_resources"] == initial["bootstrap_resources"]
    assert manifest["initial_city"]["bootstrap_resources"]["source"] == "temporary_ue_local_sim_bootstrap"
    assert manifest["initial_city"]["hall"] == {
        "init_id": 5, "type": 1, "level": 1, "grid_x": -5, "grid_y": 5,
        "actor_identity_tag": "UnityInitId_5"}
    assert manifest["anchor"] == {
        "status": "pending_world_transform_evidence", "grid_x": -5, "grid_y": 5,
        "world_transform": None}
    assert manifest["grid"] == {
        "catalog_path": label(GRID_CATALOG), "initial_age": 1,
        "size": 41, "normal_value": 5}
    assert manifest["construction"] == {
        "catalog_path": label(CONSTRUCTION_CATALOG),
        "exact_visual_variant": {"civilization": 101, "age": 1}}
    hall_model = next(row for row in construction["models"] if
                      (row["type"], row["civilization"], row["age"]) == (1, 101, 1))
    assert hall_model["model_id"] == hall_visual["model_id"] == hall_state["visual"]["model_id"]
    source_ages = source_rows(SOURCE_TABLES[-1])
    thresholds = [{"age": int(row["age"]), "town_hall_level": int(row["townLevel"]),
                   "grid_size": int(row["size"])} for row in source_ages]
    assert manifest["age_progression"]["config_thresholds"] == thresholds
    assert [(row["age"], row["town_hall_level"], row["size"]) for row in grid["ages"]] == [
        (row["age"], row["town_hall_level"], row["grid_size"]) for row in thresholds]
    assert manifest["age_progression"]["supported_simulation_ages"] == [1]
    assert manifest["age_progression"]["map_visual_conversion"] == "pending"
    assert manifest["age_progression"]["higher_age_policy"] == "configuration_threshold_known_but_local_map_visual_transition_unverified"
    report = {"schema": "rok.local_city_simulation_context.verify.v1", "status": "passed",
              "catalog_sha256": hashlib.sha256(OUT.read_bytes()).hexdigest(),
              "source_count": len(SOURCES), "initial_buildings_cross_checked": 26,
              "supported_simulation_ages": [1], "hall_init_id": 5,
              "world_transform_status": "pending_world_transform_evidence"}
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"verified local city context: 26 source buildings, age 1, hall init 5")
    return report


if __name__ == "__main__":
    verify()
