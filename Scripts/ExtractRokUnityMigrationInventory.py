from __future__ import annotations

import argparse
import collections
import hashlib
import json
import os
from pathlib import Path
from typing import Iterable


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_UNITY_ROOT = Path(os.environ.get("ROK_UNITY_ROOT", r"D:\ueDemo\Rok-unity"))
DEFAULT_OUTPUT_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"


IMPORTANT_CONFIG_NAMES = {
    "BuildingLevelData",
    "BuildingModelData",
    "BuildingTypeConfig",
    "BuildingTownCenter",
    "BuildingCityWall",
    "BuildingResourcesProduce",
    "InitBuilding",
    "ResourcesPlunderLoss",
    "ResourceGatherPoint",
    "ResourceGatherRule",
    "Hero",
    "HeroLevel",
    "HeroSkill",
    "Arms",
    "Study",
    "StudyData",
    "OpenUi",
    "OpenLodUi",
    "GameSprite",
    "Language",
}


def rel(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return str(path)


def iter_files(root: Path, skip_dirs: set[str] | None = None) -> Iterable[Path]:
    if not root.exists():
        return
    skip_dirs = skip_dirs or set()
    for current, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d not in skip_dirs and not d.startswith(".")]
        current_path = Path(current)
        for name in files:
            yield current_path / name


def extension_counts(root: Path, skip_dirs: set[str] | None = None) -> dict[str, int]:
    counts: collections.Counter[str] = collections.Counter()
    for path in iter_files(root, skip_dirs):
        suffix = path.suffix.lower() or "<none>"
        counts[suffix] += 1
    return dict(sorted(counts.items(), key=lambda item: (-item[1], item[0])))


def sample_paths(paths: Iterable[Path], root: Path, limit: int = 24) -> list[str]:
    return [rel(path, root) for path in sorted(paths)[:limit]]


def hash_file(path: Path) -> str | None:
    if not path.exists() or not path.is_file():
        return None
    digest = hashlib.sha1()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def collect_client(client_root: Path, unity_root: Path) -> dict:
    assets = client_root / "Assets"
    scenes_root = assets / "BundleAssets" / "Map" / "Scenes"
    building_root = (
        assets
        / "BundleAssets"
        / "Map"
        / "Building"
        / "Building_use"
        / "shared_building"
    )
    scripts_root = assets / "Scripts"
    shader_root = assets / "Shader"
    protocol_root = assets / "Protocol"
    addressables_root = assets / "AddressableAssetsData"

    scene_files = list(scenes_root.glob("*.unity")) if scenes_root.exists() else []
    map_aux_files = [
        path
        for path in scenes_root.glob("*")
        if path.suffix.lower() in {".tmx", ".asset", ".terrainlayer"}
    ] if scenes_root.exists() else []
    building_pngs = [
        path for path in iter_files(building_root)
        if path.suffix.lower() == ".png" and not path.stem.endswith("_mask")
    ]
    building_masks = [
        path for path in iter_files(building_root)
        if path.suffix.lower() == ".png" and path.stem.endswith("_mask")
    ]
    csharp_files = [path for path in iter_files(scripts_root) if path.suffix.lower() == ".cs"]
    prefabs = [path for path in iter_files(assets) if path.suffix.lower() == ".prefab"]
    materials = [path for path in iter_files(assets) if path.suffix.lower() == ".mat"]
    shaders = [
        path for path in iter_files(shader_root)
        if path.suffix.lower() in {".shader", ".cginc", ".hlsl"}
    ]

    script_domains: collections.Counter[str] = collections.Counter()
    for path in csharp_files:
        try:
            relative = path.relative_to(scripts_root)
            script_domains[relative.parts[0] if len(relative.parts) > 1 else "<root>"] += 1
        except ValueError:
            script_domains["<unknown>"] += 1

    return {
        "root": rel(client_root, unity_root),
        "exists": client_root.exists(),
        "extension_counts": extension_counts(client_root, {"Library", "Temp", "Obj", "Logs"}),
        "scenes": {
            "count": len(scene_files),
            "samples": sample_paths(scene_files, unity_root),
            "auxiliary_count": len(map_aux_files),
            "auxiliary_samples": sample_paths(map_aux_files, unity_root),
        },
        "visual_sources": {
            "building_sprite_count": len(building_pngs),
            "building_mask_count": len(building_masks),
            "building_sprite_samples": sample_paths(building_pngs, unity_root),
            "building_mask_samples": sample_paths(building_masks, unity_root),
            "prefab_count": len(prefabs),
            "prefab_samples": sample_paths(prefabs, unity_root),
            "material_count": len(materials),
            "material_samples": sample_paths(materials, unity_root),
            "shader_count": len(shaders),
            "shader_samples": sample_paths(shaders, unity_root),
        },
        "script_sources": {
            "csharp_count": len(csharp_files),
            "domains": dict(sorted(script_domains.items(), key=lambda item: (-item[1], item[0]))),
            "samples": sample_paths(csharp_files, unity_root),
        },
        "protocol_sources": {
            "asset_protocol_count": len(list(iter_files(protocol_root))) if protocol_root.exists() else 0,
            "samples": sample_paths(iter_files(protocol_root), unity_root) if protocol_root.exists() else [],
        },
        "addressables": {
            "exists": addressables_root.exists(),
            "file_count": len(list(iter_files(addressables_root))) if addressables_root.exists() else 0,
            "samples": sample_paths(iter_files(addressables_root), unity_root) if addressables_root.exists() else [],
        },
    }


def collect_resources(resources_root: Path, unity_root: Path) -> dict:
    sprite_root = resources_root / "Sprite"
    texture_root = resources_root / "Texture2D"
    sprite_pngs = [path for path in iter_files(sprite_root) if path.suffix.lower() == ".png"]
    masks = [path for path in sprite_pngs if path.stem.endswith("_mask")]
    body_sprites = [path for path in sprite_pngs if not path.stem.endswith("_mask")]
    textures = [
        path for path in iter_files(texture_root)
        if path.suffix.lower() in {".png", ".jpg", ".jpeg", ".tga", ".tif", ".tiff", ".psd"}
    ]
    return {
        "root": rel(resources_root, unity_root),
        "exists": resources_root.exists(),
        "extension_counts": extension_counts(resources_root),
        "sprites": {
            "body_count": len(body_sprites),
            "mask_count": len(masks),
            "body_samples": sample_paths(body_sprites, unity_root),
            "mask_samples": sample_paths(masks, unity_root),
        },
        "textures": {
            "count": len(textures),
            "samples": sample_paths(textures, unity_root),
        },
    }


def collect_configs(config_root: Path, unity_root: Path) -> dict:
    source_tables = sorted(config_root.glob("s_*.xlsm")) if config_root.exists() else []
    client_bin_root = config_root / "编译客户端配置" / "Bin"
    client_bins = sorted(client_bin_root.glob("*.bin")) if client_bin_root.exists() else []
    gen_configs = config_root / "gen" / "Configs.data"
    important = []
    available_by_stem = {path.stem[2:] if path.stem.startswith("s_") else path.stem: path for path in source_tables}
    for name in sorted(IMPORTANT_CONFIG_NAMES):
        path = available_by_stem.get(name)
        if path:
            important.append(rel(path, unity_root))

    return {
        "root": rel(config_root, unity_root),
        "exists": config_root.exists(),
        "source_table_count": len(source_tables),
        "source_table_samples": sample_paths(source_tables, unity_root, 36),
        "client_bin_count": len(client_bins),
        "client_bin_samples": sample_paths(client_bins, unity_root, 36),
        "important_tables_present": important,
        "important_tables_missing": sorted(IMPORTANT_CONFIG_NAMES - set(available_by_stem.keys())),
        "server_configs_data": {
            "exists": gen_configs.exists(),
            "relative_path": rel(gen_configs, unity_root),
            "size_bytes": gen_configs.stat().st_size if gen_configs.exists() else 0,
            "sha1": hash_file(gen_configs),
        },
    }


def collect_server(server_root: Path, unity_root: Path) -> dict:
    work_root = server_root / "data" / "work"
    protocol_root = work_root / "common" / "protocol"
    lua_files = [path for path in iter_files(work_root, {"logs"}) if path.suffix.lower() == ".lua"]
    c_files = [path for path in iter_files(work_root, {"logs"}) if path.suffix.lower() == ".c"]
    cpp_files = [path for path in iter_files(work_root, {"logs"}) if path.suffix.lower() == ".cpp"]
    header_files = [path for path in iter_files(work_root, {"logs"}) if path.suffix.lower() == ".h"]
    sprotos = [path for path in iter_files(protocol_root) if path.suffix.lower() == ".sproto"]
    main_services = [path for path in (work_root / "server").glob("*_server/Main.lua")] if (work_root / "server").exists() else []

    return {
        "root": rel(server_root, unity_root),
        "exists": server_root.exists(),
        "work_root": rel(work_root, unity_root),
        "extension_counts": extension_counts(work_root, {"logs"}) if work_root.exists() else {},
        "language_counts": {
            "lua": len(lua_files),
            "c": len(c_files),
            "cpp": len(cpp_files),
            "headers": len(header_files),
        },
        "protocol": {
            "sproto_count": len(sprotos),
            "samples": sample_paths(sprotos, unity_root),
        },
        "services": {
            "main_service_count": len(main_services),
            "samples": sample_paths(main_services, unity_root),
        },
    }


def build_migration_lanes(inventory: dict) -> list[dict]:
    client = inventory["client_unity"]
    configs = inventory["config_tables"]
    resources = inventory["resources"]
    server = inventory["server"]
    return [
        {
            "lane": "city_visual",
            "unity_sources": [
                "client-unity/Assets/BundleAssets/Map/Scenes",
                "client-unity/Assets/BundleAssets/Map/Building/Building_use",
                "resources/Sprite",
            ],
            "ue_targets": [
                "/Game/Maps/L_RokCitySingleBuilding",
                "/Game/Maps/L_RokUnityGameAppCity",
                "ARokCitySpriteActor",
            ],
            "source_count": client["scenes"]["count"]
            + client["visual_sources"]["building_sprite_count"]
            + resources["sprites"]["body_count"],
        },
        {
            "lane": "gameplay_config",
            "unity_sources": [
                "config-tables/s_*.xlsm",
                "config-tables/编译客户端配置/Bin/*.bin",
                "config-tables/gen/Configs.data",
            ],
            "ue_targets": [
                "DataTables/DataAssets",
                "C++ data registry",
            ],
            "source_count": configs["source_table_count"] + configs["client_bin_count"],
        },
        {
            "lane": "client_runtime",
            "unity_sources": [
                "client-unity/Assets/Scripts",
                "client-unity/Assets/AddressableAssetsData",
                "client-unity/Assets/Protocol",
            ],
            "ue_targets": [
                "GameInstance/bootstrap",
                "Subsystems",
                "UMG screens",
            ],
            "source_count": client["script_sources"]["csharp_count"]
            + client["addressables"]["file_count"]
            + client["protocol_sources"]["asset_protocol_count"],
        },
        {
            "lane": "server_protocol_boundary",
            "unity_sources": [
                "server/data/work/server",
                "server/data/work/common/protocol",
            ],
            "ue_targets": [
                "local simulation adapter",
                "future network protocol adapter",
            ],
            "source_count": server["language_counts"]["lua"] + server["protocol"]["sproto_count"],
        },
    ]


def write_summary(inventory: dict, output_path: Path) -> None:
    lines = [
        "# Rok Unity Migration Inventory",
        "",
        "Generated by `Scripts/ExtractRokUnityMigrationInventory.py`.",
        "",
        "## Roots",
        "",
    ]
    for key in ["client_unity", "resources", "config_tables", "server"]:
        item = inventory[key]
        lines.append(f"- `{key}`: `{item['root']}` exists={item['exists']}")

    lines += [
        "",
        "## Migration Lanes",
        "",
    ]
    for lane in inventory["migration_lanes"]:
        lines.append(f"- `{lane['lane']}` source_count={lane['source_count']}")

    client = inventory["client_unity"]
    resources = inventory["resources"]
    configs = inventory["config_tables"]
    server = inventory["server"]
    lines += [
        "",
        "## Key Counts",
        "",
        f"- Unity scenes: {client['scenes']['count']}",
        f"- Unity C# scripts: {client['script_sources']['csharp_count']}",
        f"- Unity prefabs: {client['visual_sources']['prefab_count']}",
        f"- Unity materials: {client['visual_sources']['material_count']}",
        f"- Unity shaders/includes: {client['visual_sources']['shader_count']}",
        f"- Building sprites/masks: {client['visual_sources']['building_sprite_count']} / {client['visual_sources']['building_mask_count']}",
        f"- Extracted resource sprites/masks: {resources['sprites']['body_count']} / {resources['sprites']['mask_count']}",
        f"- Config source tables: {configs['source_table_count']}",
        f"- Client config bins: {configs['client_bin_count']}",
        f"- Server Lua/C/C++/headers: {server['language_counts']['lua']} / {server['language_counts']['c']} / {server['language_counts']['cpp']} / {server['language_counts']['headers']}",
        f"- Server sproto files: {server['protocol']['sproto_count']}",
        "",
        "## Immediate UE Migration Batches",
        "",
        "1. City visual pipeline: promote Unity scene/prefab placement extraction into UE map generation and reject placeholder/card artifacts by verification.",
        "2. Gameplay config pipeline: import building, resource, hero, troop, study, UI-open, and localization tables into typed UE data assets/tables.",
        "3. Runtime shell: map Unity bootstrap/UI-root behavior to UE GameInstance, subsystems, PlayerController, camera pawn, and UMG screens.",
        "4. Protocol boundary: keep the Skynet/Lua server contract as the authority shape for future network adapters while local simulation stays interface-compatible.",
        "",
    ]
    output_path.write_text("\n".join(lines), encoding="utf-8")


def build_inventory(unity_root: Path) -> dict:
    unity_root = unity_root.resolve()
    inventory = {
        "unity_root": str(unity_root),
        "project_root": str(PROJECT_ROOT),
        "client_unity": collect_client(unity_root / "client-unity", unity_root),
        "resources": collect_resources(unity_root / "resources", unity_root),
        "config_tables": collect_configs(unity_root / "config-tables", unity_root),
        "server": collect_server(unity_root / "server", unity_root),
    }
    inventory["migration_lanes"] = build_migration_lanes(inventory)
    return inventory


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract a complete source inventory for Rok Unity-to-UE migration.")
    parser.add_argument("--unity-root", default=str(DEFAULT_UNITY_ROOT), help="Root containing client-unity/resources/config-tables/server.")
    parser.add_argument("--output-root", default=str(DEFAULT_OUTPUT_ROOT), help="Directory for generated migration inventory files.")
    args = parser.parse_args()

    unity_root = Path(args.unity_root)
    output_root = Path(args.output_root)
    output_root.mkdir(parents=True, exist_ok=True)

    inventory = build_inventory(unity_root)
    json_path = output_root / "unity_migration_inventory.json"
    summary_path = output_root / "unity_migration_inventory.md"
    json_path.write_text(json.dumps(inventory, ensure_ascii=False, indent=2), encoding="utf-8")
    write_summary(inventory, summary_path)

    print(f"Wrote {json_path}")
    print(f"Wrote {summary_path}")
    for lane in inventory["migration_lanes"]:
        print(f"{lane['lane']}: source_count={lane['source_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
