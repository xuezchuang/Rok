import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CONFIG_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "ConfigTables"
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
RESOLUTION_PATH = MIGRATION_ROOT / "unity_gameapp_city_init_building_resolution.json"
OUTPUT_JSON = MIGRATION_ROOT / "unity_initial_city_state.json"
OUTPUT_MD = MIGRATION_ROOT / "unity_initial_city_state.md"

DECORATION_TYPES = {50, 51, 52, 53, 54, 55}
RESOURCE_BY_BUILDING_TYPE = {
    2: "food",
    3: "wood",
    4: "stone",
    5: "gold",
}
UE_TYPE_NAME = {
    1: "TownCenter",
    2: "Farm",
    3: "Lumbermill",
    4: "Quarry",
    5: "Goldmine",
    6: "CityWall",
    7: "BuilderHut",
    8: "GuardTower",
    9: "Barracks",
    10: "Stable",
    11: "ArcheryRange",
    12: "ScoutCamp",
    13: "Academy",
    14: "Hospital",
    15: "Storehouse",
    21: "AllianceCenter",
    29: "NoticeBoard",
    50: "Decoration",
    51: "Decoration",
    52: "Decoration",
    53: "Decoration",
    54: "Decoration",
    55: "Decoration",
}


def load_table(name):
    path = CONFIG_ROOT / f"{name}.json"
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def int_field(row, name, default=0):
    value = row.get(name, default)
    if value in ("", None):
        return default
    return int(value)


def build_lookup(rows, *fields):
    lookup = {}
    for row in rows:
        key = tuple(int_field(row, field) for field in fields)
        lookup[key] = row
    return lookup


def make_resource_stock():
    # Temporary local-simulation bootstrap until Unity login rewards/items are migrated.
    # Keep this explicit in the manifest so it is not mistaken for accepted Unity economy data.
    return {
        "food": 5000,
        "wood": 5000,
        "stone": 1500,
        "gold": 600,
        "source": "temporary_ue_local_sim_bootstrap",
    }


def main():
    init_table = load_table("InitBuilding")
    level_table = load_table("BuildingLevelData")
    produce_table = load_table("BuildingResourcesProduce")

    level_by_type_level = build_lookup(level_table["rows"], "type", "level")
    produce_by_type_level = build_lookup(produce_table["rows"], "type", "level")
    if RESOLUTION_PATH.exists():
        resolution_records = json.loads(RESOLUTION_PATH.read_text(encoding="utf-8")).get("records", [])
    else:
        resolution_records = []
    resolution_by_init_id = {int_field(row, "init_id"): row for row in resolution_records}

    buildings = []
    missing_level_config = []
    production_per_hour = {"food": 0, "wood": 0, "stone": 0, "gold": 0}
    total_power = 0

    for row in sorted(init_table["rows"], key=lambda item: int_field(item, "ID")):
        init_id = int_field(row, "ID")
        type_id = int_field(row, "type")
        level = int_field(row, "level", 1)
        level_config = level_by_type_level.get((type_id, level))
        next_level_config = level_by_type_level.get((type_id, level + 1))
        produce_config = produce_by_type_level.get((type_id, level))
        resolution = resolution_by_init_id.get(init_id, {})
        resource_key = RESOURCE_BY_BUILDING_TYPE.get(type_id, "")

        if not level_config:
            missing_level_config.append({"init_id": init_id, "type_id": type_id, "level": level})

        if produce_config and resource_key:
            production_per_hour[resource_key] += int_field(produce_config, "produceSpeed")

        total_power += int_field(level_config or {}, "power")
        buildings.append({
            "init_id": init_id,
            "type_id": type_id,
            "ue_type": UE_TYPE_NAME.get(type_id, "Unknown"),
            "display_name": row.get("name", ""),
            "level": level,
            "grid": {
                "x": int_field(row, "posX"),
                "y": int_field(row, "posY"),
            },
            "is_decoration": type_id in DECORATION_TYPES,
            "actor_tags": [
                "RokUnityConfigInitBuilding",
                f"UnityInitId_{init_id}",
                f"UnityBuildingType_{type_id}",
                f"UnityBuildingLevel_{level}",
                f"UnityGridX_{int_field(row, 'posX')}",
                f"UnityGridY_{int_field(row, 'posY')}",
            ],
            "visual": {
                "model_id": resolution.get("model_id", ""),
                "sprite_name": resolution.get("sprite_name", ""),
                "resolution": resolution.get("resolution", "missing"),
            },
            "level_config": {
                "width": int_field(level_config or {}, "width"),
                "length": int_field(level_config or {}, "length"),
                "build_seconds": int_field(level_config or {}, "buildingTime"),
                "power": int_field(level_config or {}, "power"),
                "cost": {
                    "food": int_field(level_config or {}, "food"),
                    "wood": int_field(level_config or {}, "wood"),
                    "stone": int_field(level_config or {}, "stone"),
                    "gold": int_field(level_config or {}, "coin"),
                },
            },
            "next_level": {
                "level": level + 1 if next_level_config else 0,
                "build_seconds": int_field(next_level_config or {}, "buildingTime"),
                "power": int_field(next_level_config or {}, "power"),
                "cost": {
                    "food": int_field(next_level_config or {}, "food"),
                    "wood": int_field(next_level_config or {}, "wood"),
                    "stone": int_field(next_level_config or {}, "stone"),
                    "gold": int_field(next_level_config or {}, "coin"),
                },
            },
            "production": {
                "resource": resource_key,
                "per_hour": int_field(produce_config or {}, "produceSpeed"),
                "gather_min": int_field(produce_config or {}, "gatherMin"),
                "gather_max": int_field(produce_config or {}, "gatherMax"),
            },
        })

    manifest = {
        "schema_version": 1,
        "source_tables": {
            "init_building": "Saved/RokUnityMigration/ConfigTables/InitBuilding.json",
            "building_level_data": "Saved/RokUnityMigration/ConfigTables/BuildingLevelData.json",
            "building_resources_produce": "Saved/RokUnityMigration/ConfigTables/BuildingResourcesProduce.json",
            "visual_resolution": "Saved/RokUnityMigration/unity_gameapp_city_init_building_resolution.json",
        },
        "target_level": "/Game/Maps/L_RokUnityGameAppCity",
        "building_count": len(buildings),
        "gameplay_building_count": sum(1 for item in buildings if not item["is_decoration"]),
        "decoration_count": sum(1 for item in buildings if item["is_decoration"]),
        "total_power": total_power,
        "production_per_hour": production_per_hour,
        "bootstrap_resources": make_resource_stock(),
        "missing_level_config": missing_level_config,
        "buildings": buildings,
    }

    MIGRATION_ROOT.mkdir(parents=True, exist_ok=True)
    OUTPUT_JSON.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# Unity Initial City State",
        "",
        f"- Target level: `{manifest['target_level']}`",
        f"- Init buildings: {manifest['building_count']}",
        f"- Gameplay buildings: {manifest['gameplay_building_count']}",
        f"- Decorations: {manifest['decoration_count']}",
        f"- Total starting power from BuildingLevelData: {manifest['total_power']}",
        f"- Production/hour: food={production_per_hour['food']}, wood={production_per_hour['wood']}, stone={production_per_hour['stone']}, gold={production_per_hour['gold']}",
        f"- Missing level configs: {len(missing_level_config)}",
        "",
        "| InitId | Type | Name | Level | Grid | Sprite | Next Cost | Next Seconds |",
        "| --- | --- | --- | --- | --- | --- | --- | --- |",
    ]
    for item in buildings:
        cost = item["next_level"]["cost"]
        cost_text = f"F{cost['food']} W{cost['wood']} S{cost['stone']} G{cost['gold']}"
        lines.append(
            f"| {item['init_id']} | {item['type_id']} {item['ue_type']} | {item['display_name']} | "
            f"{item['level']} | {item['grid']['x']},{item['grid']['y']} | {item['visual']['sprite_name']} | "
            f"{cost_text} | {item['next_level']['build_seconds']} |"
        )
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(json.dumps({
        "output": str(OUTPUT_JSON),
        "building_count": manifest["building_count"],
        "gameplay_building_count": manifest["gameplay_building_count"],
        "decoration_count": manifest["decoration_count"],
        "total_power": manifest["total_power"],
        "missing_level_config": len(missing_level_config),
    }, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
