from __future__ import annotations

import argparse
import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INVENTORY = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_migration_inventory.json"


def require(condition: bool, message: str, failures: list[str]) -> None:
    if not condition:
        failures.append(message)


def verify_inventory(inventory: dict) -> list[str]:
    failures: list[str] = []

    for key in ["client_unity", "resources", "config_tables", "server"]:
        require(inventory.get(key, {}).get("exists", False), f"missing root section: {key}", failures)

    client = inventory.get("client_unity", {})
    visual = client.get("visual_sources", {})
    scripts = client.get("script_sources", {})
    resources = inventory.get("resources", {})
    configs = inventory.get("config_tables", {})
    server = inventory.get("server", {})

    require(client.get("scenes", {}).get("count", 0) >= 5, "expected at least 5 Unity map scenes", failures)
    require(visual.get("building_sprite_count", 0) > 0, "expected Unity building sprites", failures)
    require(visual.get("building_mask_count", 0) > 0, "expected Unity building masks", failures)
    require(visual.get("shader_count", 0) > 0, "expected Unity shader sources", failures)
    require(scripts.get("csharp_count", 0) > 100, "expected substantial Unity C# client script corpus", failures)
    require(resources.get("sprites", {}).get("body_count", 0) > 100, "expected extracted body sprites", failures)
    require(resources.get("sprites", {}).get("mask_count", 0) > 100, "expected extracted sprite masks", failures)
    require(configs.get("source_table_count", 0) > 100, "expected many source config spreadsheets", failures)
    require(configs.get("client_bin_count", 0) > 100, "expected many compiled client config bins", failures)
    require(configs.get("server_configs_data", {}).get("exists", False), "missing server Configs.data bundle", failures)
    require(server.get("language_counts", {}).get("lua", 0) > 100, "expected Skynet/Lua server business corpus", failures)
    require(server.get("protocol", {}).get("sproto_count", 0) > 0, "expected server sproto protocol files", failures)

    lanes = inventory.get("migration_lanes", [])
    lane_names = {lane.get("lane") for lane in lanes}
    for expected in ["city_visual", "gameplay_config", "client_runtime", "server_protocol_boundary"]:
        require(expected in lane_names, f"missing migration lane: {expected}", failures)
    for lane in lanes:
        require(lane.get("source_count", 0) > 0, f"migration lane has no sources: {lane.get('lane')}", failures)

    return failures


def print_summary(inventory: dict) -> None:
    client = inventory["client_unity"]
    visual = client["visual_sources"]
    scripts = client["script_sources"]
    resources = inventory["resources"]
    configs = inventory["config_tables"]
    server = inventory["server"]

    print("UnityMigrationInventoryVerify")
    print(f"  unity_root={inventory['unity_root']}")
    print(f"  scenes={client['scenes']['count']}")
    print(f"  csharp={scripts['csharp_count']}")
    print(f"  prefabs={visual['prefab_count']}")
    print(f"  materials={visual['material_count']}")
    print(f"  shaders={visual['shader_count']}")
    print(f"  building_sprites={visual['building_sprite_count']}")
    print(f"  building_masks={visual['building_mask_count']}")
    print(f"  resource_sprites={resources['sprites']['body_count']}")
    print(f"  resource_masks={resources['sprites']['mask_count']}")
    print(f"  config_tables={configs['source_table_count']}")
    print(f"  config_bins={configs['client_bin_count']}")
    print(f"  server_lua={server['language_counts']['lua']}")
    print(f"  server_sproto={server['protocol']['sproto_count']}")
    for lane in inventory["migration_lanes"]:
        print(f"  lane.{lane['lane']}={lane['source_count']}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify the generated Rok Unity migration inventory.")
    parser.add_argument("--inventory", default=str(DEFAULT_INVENTORY), help="Path to unity_migration_inventory.json.")
    args = parser.parse_args()

    inventory_path = Path(args.inventory)
    if not inventory_path.exists():
        raise SystemExit(f"Missing inventory file: {inventory_path}")

    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    print_summary(inventory)
    failures = verify_inventory(inventory)
    if failures:
        for failure in failures:
            print(f"ERROR: {failure}")
        raise SystemExit(1)
    print("Unity migration inventory verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
