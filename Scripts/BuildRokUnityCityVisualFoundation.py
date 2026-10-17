import json
from pathlib import Path
from rok_data_paths import DATA_ROOT, label, resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
OUTPUT_JSON = OUTPUT_DIR / "unity_city_visual_foundation.json"
OUTPUT_MD = OUTPUT_DIR / "unity_city_visual_foundation.md"

BOUNDARY_PATH = OUTPUT_DIR / "unity_city_boundary.json"
INIT_BUILDING_RESOLUTION_PATH = DATA_ROOT / "unity_gameapp_city_init_building_resolution.json"
INITIAL_CITY_STATE_PATH = DATA_ROOT / "unity_initial_city_state.json"

GROUND_SOURCE_CANDIDATES = [
    "Saved/RokUnityPlacement/clean/UnityCityGround_build_ground_6_101_1.png",
    "Saved/RokUnityPlacement/clean/UnityGrassGround_I_TYPE_Grass_01.png",
    "Saved/RokUnityPlacement/clean/UnityCityBackground_Terrain_grass_08_3g2.png",
    "Saved/RokUnityPlacement/clean/UnityCitySurface_map_incity4.png",
    "ref/client-unity/Assets/BundleAssets/Map/Textures/Terrian_Maps/build_SceneMap_2_lv.tif",
    "ref/client-unity/Assets/BundleAssets/Map/Textures/Materials/build_ground_6_101_1.mat",
]


def load_json(path):
    if not path.exists():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def exists(relative_path):
    return resolve(relative_path).exists()


def build_manifest():
    boundary = load_json(BOUNDARY_PATH)
    building_resolution = load_json(INIT_BUILDING_RESOLUTION_PATH)
    initial_city_state = load_json(INITIAL_CITY_STATE_PATH)

    ground_sources = [
        {
            "path": path,
            "exists": exists(path),
        }
        for path in GROUND_SOURCE_CANDIDATES
    ]

    building_records = building_resolution.get("records", [])
    spawnable_buildings = [
        record
        for record in building_records
        if record.get("sprite_name")
    ]
    fallback_buildings = building_resolution.get("fallbacks", [])
    missing_buildings = building_resolution.get("missing", [])
    unresolved_decorations = [
        record
        for record in missing_buildings
        if record.get("building_type") in {52, 53}
    ]
    core_missing_buildings = [
        record
        for record in missing_buildings
        if record.get("building_type") not in {52, 53}
    ]
    bootstrap_buildings = initial_city_state.get("buildings", [])

    manifest = {
        "schema": "rok.unity_city_visual_foundation.v1",
        "ue_target_level": "/Game/Maps/L_RokUnityGameAppCity",
        "source_manifests": {
            "city_boundary": label(BOUNDARY_PATH),
            "init_building_resolution": label(INIT_BUILDING_RESOLUTION_PATH),
            "initial_city_state": label(INITIAL_CITY_STATE_PATH),
        },
        "ground_layer": {
            "source_candidates": ground_sources,
            "preferred_existing_source": next((item["path"] for item in ground_sources if item["exists"]), ""),
        },
        "boundary_layer": {
            "source_schema": boundary.get("schema", ""),
            "instance_count": boundary.get("boundary_instance_count", 0),
            "active_instance_count": boundary.get("active_boundary_instance_count", 0),
            "inactive_instance_count": boundary.get("inactive_boundary_instance_count", 0),
            "asset_count": len(boundary.get("boundary_assets", [])),
            "material_count": len(boundary.get("boundary_materials", [])),
        },
        "building_layer": {
            "resolution_record_count": len(building_records),
            "spawnable_count": len(spawnable_buildings),
            "missing_count": len(missing_buildings),
            "core_missing_count": len(core_missing_buildings),
            "unresolved_decoration_count": len(unresolved_decorations),
            "fallback_count": len(fallback_buildings),
            "initial_city_state_building_count": len(bootstrap_buildings),
            "sample_spawnable": spawnable_buildings[:12],
            "sample_missing": missing_buildings[:12],
            "sample_unresolved_decorations": unresolved_decorations[:12],
        },
        "acceptance_targets": [
            "Generated map uses Unity source boundary actors tagged RokUnityCityBoundarySource.",
            "Generated map uses Unity InitBuilding-derived building actors tagged RokUnityConfigInitBuilding.",
            "Ground layer uses an existing Unity-derived source texture from the preferred ground source list.",
            "Demo-only UnityCityBoundary2D actors are absent from the accepted map.",
        ],
    }
    return manifest


def write_markdown(manifest):
    ground = manifest["ground_layer"]
    boundary = manifest["boundary_layer"]
    building = manifest["building_layer"]
    lines = [
        "# Rok Unity City Visual Foundation",
        "",
        f"- Schema: `{manifest['schema']}`",
        f"- UE target level: `{manifest['ue_target_level']}`",
        f"- Preferred ground source: `{ground['preferred_existing_source']}`",
        f"- Boundary instances: {boundary['instance_count']} active {boundary['active_instance_count']} inactive {boundary['inactive_instance_count']}",
        f"- Boundary assets/materials: {boundary['asset_count']} / {boundary['material_count']}",
        f"- Building resolution records: {building['resolution_record_count']}",
        f"- Spawnable buildings: {building['spawnable_count']}",
        f"- Missing buildings: {building['missing_count']}",
        f"- Core missing buildings: {building['core_missing_count']}",
        f"- Unresolved decorations: {building['unresolved_decoration_count']}",
        f"- Fallback buildings: {building['fallback_count']}",
        f"- Initial city-state buildings: {building['initial_city_state_building_count']}",
        "",
        "## Ground Sources",
        "",
    ]
    for item in ground["source_candidates"]:
        status = "ok" if item["exists"] else "missing"
        lines.append(f"- {status}: `{item['path']}`")
    lines.extend([
        "",
        "## Acceptance Targets",
        "",
    ])
    for target in manifest["acceptance_targets"]:
        lines.append(f"- {target}")
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    manifest = build_manifest()
    OUTPUT_JSON.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    write_markdown(manifest)
    print(
        "Rok Unity city visual foundation manifest generated: "
        f"ground={manifest['ground_layer']['preferred_existing_source']} "
        f"boundary={manifest['boundary_layer']['instance_count']} "
        f"buildings={manifest['building_layer']['spawnable_count']}"
    )


if __name__ == "__main__":
    main()
