import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
MANIFEST_PATH = OUTPUT_DIR / "unity_city_boundary.json"
VERIFY_PATH = OUTPUT_DIR / "unity_city_boundary_verify.json"


def main():
    failures = []
    if not MANIFEST_PATH.exists():
        failures.append(f"Missing generated city boundary manifest: {MANIFEST_PATH}")
        manifest = {}
    else:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    if manifest.get("schema") != "rok.unity_city_boundary.v1":
        failures.append(f"Unexpected schema: {manifest.get('schema')}")

    instances = manifest.get("boundary_instances", [])
    if not instances:
        failures.append("No Unity city-boundary prefab instances found")

    map_tts_02_names = {
        item.get("primary_name")
        for item in instances
        if item.get("scene", "").endswith("Map_TTS_02.unity")
    }
    for required in {"Citywall_1_1", "CityWall_Ground_1_1_0", "City_Wall_02"}:
        if required not in map_tts_02_names:
            failures.append(f"Map_TTS_02 missing boundary instance: {required}")

    active_names = {item.get("primary_name") for item in instances if item.get("active") is True}
    if "CityWall_Ground_1_1_0" not in active_names:
        failures.append("Expected active CityWall_Ground_1_1_0 source instance")

    asset_paths = {item.get("path", "") for item in manifest.get("boundary_assets", [])}
    for required_suffix in {
        "ref/client-unity/Assets/BundleAssets/Map/FBX/CityWall_101_1.FBX",
        "ref/client-unity/Assets/BundleAssets/Map/FBX/CityGate_101_1.FBX",
        "ref/client-unity/Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityWall_101_1_mat.mat",
        "ref/client-unity/Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityGate_101_1_mat.mat",
    }:
        if required_suffix not in asset_paths:
            failures.append(f"Missing expected boundary source asset: {required_suffix}")

    material_records = manifest.get("boundary_materials", [])
    if not any(item.get("main_texture_path") for item in material_records):
        failures.append("No boundary material main textures resolved from Unity GUIDs")

    result = {
        "manifest": str(MANIFEST_PATH.relative_to(PROJECT_ROOT)),
        "failure_count": len(failures),
        "failures": failures,
        "boundary_instance_count": manifest.get("boundary_instance_count", 0),
        "active_boundary_instance_count": manifest.get("active_boundary_instance_count", 0),
        "boundary_asset_count": len(manifest.get("boundary_assets", [])),
        "boundary_material_count": len(material_records),
    }
    VERIFY_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")

    if failures:
        print("Rok Unity city boundary verification FAILED")
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)

    print(
        "Rok Unity city boundary verification passed: "
        f"instances={result['boundary_instance_count']} "
        f"active={result['active_boundary_instance_count']} "
        f"assets={result['boundary_asset_count']}"
    )


if __name__ == "__main__":
    main()
