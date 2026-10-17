"""Bind source-proven local initial city identity for a bounded UE local simulation."""
import csv
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MIGRATION = ROOT / "Saved/RokUnityMigration"
LEGACY_RESOLUTION = MIGRATION / "unity_gameapp_city_init_building_resolution.json"
VISUAL_RESOLUTION = MIGRATION / "unity_initial_building_visual_resolution.json"
INITIAL_STATE = MIGRATION / "unity_initial_city_state.json"
GRID_CATALOG = MIGRATION / "CityConstructionGridSources/catalog.json"
CONSTRUCTION_CATALOG = MIGRATION / "CityConstructionSources/catalog.json"
CSV_ROOT = ROOT / "ref/config-tables/编译客户端配置/CSV"
SOURCE_TABLES = tuple(CSV_ROOT / f"{name}.csv" for name in (
    "InitBuilding", "BuildingModelData", "BuildingTypeConfig", "BuildingLevelData",
    "BuildingResourcesProduce", "CityAgeSize"))
GENERATOR = Path(__file__).resolve()
OUT = MIGRATION / "LocalCitySimulationContext/catalog.json"
SOURCES = (LEGACY_RESOLUTION, VISUAL_RESOLUTION, INITIAL_STATE, GRID_CATALOG,
           CONSTRUCTION_CATALOG, GENERATOR, *SOURCE_TABLES)
TARGET_LEVEL = "/Game/Maps/L_RokUnityGameAppCity"


def relative(path):
    return path.relative_to(ROOT).as_posix()


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


def csv_rows(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        records = list(csv.DictReader(stream))
    if not records or records[0][next(iter(records[0]))] != "int":
        raise ValueError(f"Missing CSV type row: {path}")
    return records[1:]


def exact_ids(records):
    ids = [record["init_id"] for record in records]
    if len(ids) != 26 or len(set(ids)) != 26:
        raise ValueError("Expected 26 unique source InitBuilding identities")
    return set(ids)


def build(write=True):
    legacy, visual, initial, grid, construction = map(load, (
        LEGACY_RESOLUTION, VISUAL_RESOLUTION, INITIAL_STATE, GRID_CATALOG, CONSTRUCTION_CATALOG))
    if legacy["target_level"] != initial["target_level"] or initial["target_level"] != TARGET_LEVEL:
        raise ValueError("Initial city target map identity changed")
    if (legacy["civ_type"], legacy["age"], visual["civ_type"], visual["age"]) != (101, 1, 101, 1):
        raise ValueError("Source initial city visual variant is not civilization 101 / age 1")
    if visual["schema"] != "rok.unity_initial_building_visual_resolution.v1" or visual["failures"]:
        raise ValueError("Current visual resolution report has failures")
    if initial["schema_version"] != 1 or initial["visual_resolution_failures"]:
        raise ValueError("Initial city manifest visual resolution is not complete")
    if grid["schema"] != "rok.city_construction_grid_sources.v1" or construction["schema"] != "rok.city_construction_sources.v1":
        raise ValueError("Construction source catalog schema changed")
    if grid["overlaps"] or grid["ages"][0]["age"] != 1 or grid["ages"][0]["size"] != 41:
        raise ValueError("Initial age 1 grid changed or has overlapping source rectangles")
    if grid["summary"]["initial_building_count"] != 26:
        raise ValueError("Grid initial building count changed")
    legacy_ids = exact_ids(legacy["records"])
    visual_ids = exact_ids(visual["records"])
    initial_ids = exact_ids(initial["buildings"])
    grid_ids = exact_ids(grid["initial_occupancy"])
    if not legacy_ids == visual_ids == initial_ids == grid_ids:
        raise ValueError("Initial city source identity sets differ")
    if len(legacy["missing"]) != 14 or len(legacy["fallbacks"]) != 4 or legacy["spawnable"] != 12:
        raise ValueError("Legacy initial map resolution status changed; review required")
    if visual["building_count"] != initial["building_count"] != 26:
        raise ValueError("Current initial city building count changed")
    by_legacy = {row["init_id"]: row for row in legacy["records"]}
    by_visual = {row["init_id"]: row for row in visual["records"]}
    by_initial = {row["init_id"]: row for row in initial["buildings"]}
    by_grid = {row["init_id"]: row for row in grid["initial_occupancy"]}
    by_model = {(row["type"], row["civilization"], row["age"]): row for row in construction["models"]}
    source_init = {int(row["ID"]): row for row in csv_rows(SOURCE_TABLES[0])}
    if set(source_init) != initial_ids:
        raise ValueError("InitBuilding CSV identities differ from generated manifests")
    for init_id in sorted(initial_ids):
        old, current, state, footprint, source = (
            by_legacy[init_id], by_visual[init_id], by_initial[init_id], by_grid[init_id], source_init[init_id])
        building_type, level, x, y = (int(source[field]) for field in ("type", "level", "posX", "posY"))
        model = by_model.get((building_type, 101, 1))
        if model is None or not model["has_building_type_config"]:
            raise ValueError(f"No exact city model key for init {init_id}")
        if (old["building_type"], old["posX"], old["posY"], old["model_id"]) != (
                building_type, x, y, model["model_id"]):
            raise ValueError(f"Legacy source identity mismatch for init {init_id}")
        if (current["building_type"], current["level"], current["grid_x"], current["grid_y"],
                current["model_id"]) != (building_type, level, x, y, model["model_id"]):
            raise ValueError(f"Current visual identity mismatch for init {init_id}")
        if (state["type_id"], state["level"], state["grid"], state["visual"]["model_id"]) != (
                building_type, level, {"x": x, "y": y}, model["model_id"]):
            raise ValueError(f"Initial city state mismatch for init {init_id}")
        if (footprint["type"], footprint["level"], footprint["pos_x"], footprint["pos_y"]) != (
                building_type, level, x, y):
            raise ValueError(f"Grid occupancy mismatch for init {init_id}")
    hall = by_initial[5]
    source_hall = source_init[5]
    if (int(source_hall["type"]), int(source_hall["level"]), int(source_hall["posX"]),
            int(source_hall["posY"])) != (1, 1, -5, 5):
        raise ValueError("Source initial city hall anchor changed")
    if hall["grid"] != {"x": -5, "y": 5} or "UnityInitId_5" not in hall["actor_tags"]:
        raise ValueError("Initial city hall manifest anchor changed")
    thresholds = [{"age": row["age"], "town_hall_level": row["town_hall_level"], "grid_size": row["size"]}
                  for row in grid["ages"]]
    age_csv = csv_rows(SOURCE_TABLES[-1])
    if thresholds != [{"age": int(row["age"]), "town_hall_level": int(row["townLevel"]),
                       "grid_size": int(row["size"])} for row in age_csv]:
        raise ValueError("CityAgeSize source disagrees with grid catalog")
    manifest = {
        "schema": "rok.local_city_simulation_context.v1",
        "mode": "local_simulation",
        "target_level": TARGET_LEVEL,
        "visual_variant": {
            "civilization": 101, "age": 1,
            "qualification": "source_visual_variant_not_account_identity",
            "evidence": {"legacy_resolution_path": relative(LEGACY_RESOLUTION),
                         "current_visual_resolution_path": relative(VISUAL_RESOLUTION),
                         "legacy_spawnable_count": legacy["spawnable"],
                         "legacy_missing_count": len(legacy["missing"]),
                         "legacy_fallback_count": len(legacy["fallbacks"]),
                         "current_failure_count": len(visual["failures"])}},
        "initial_city": {
            "manifest_path": relative(INITIAL_STATE), "building_count": 26,
            "bootstrap_resources": initial["bootstrap_resources"],
            "hall": {"init_id": 5, "type": 1, "level": hall["level"],
                     "grid_x": hall["grid"]["x"], "grid_y": hall["grid"]["y"],
                     "actor_identity_tag": "UnityInitId_5"}},
        "grid": {"catalog_path": relative(GRID_CATALOG), "initial_age": 1,
                 "size": grid["ages"][0]["size"], "normal_value": grid["normal_value"]},
        "construction": {"catalog_path": relative(CONSTRUCTION_CATALOG),
                         "exact_visual_variant": {"civilization": 101, "age": 1}},
        "anchor": {"status": "pending_world_transform_evidence", "grid_x": hall["grid"]["x"],
                   "grid_y": hall["grid"]["y"], "world_transform": None},
        "age_progression": {"supported_simulation_ages": [1], "config_thresholds": thresholds,
                            "map_visual_conversion": "pending",
                            "higher_age_policy": "configuration_threshold_known_but_local_map_visual_transition_unverified"},
        "sources": {relative(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in SOURCES},
    }
    if write:
        OUT.parent.mkdir(parents=True, exist_ok=True)
        OUT.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {OUT}: age 1, {manifest['initial_city']['building_count']} source buildings")
    return manifest


if __name__ == "__main__":
    build()
