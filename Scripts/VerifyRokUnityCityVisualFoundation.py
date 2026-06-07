import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
MANIFEST_PATH = OUTPUT_DIR / "unity_city_visual_foundation.json"
VERIFY_PATH = OUTPUT_DIR / "unity_city_visual_foundation_verify.json"


def main():
    failures = []
    if not MANIFEST_PATH.exists():
        failures.append(f"Missing generated city visual foundation manifest: {MANIFEST_PATH}")
        manifest = {}
    else:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    if manifest.get("schema") != "rok.unity_city_visual_foundation.v1":
        failures.append(f"Unexpected schema: {manifest.get('schema')}")

    ground = manifest.get("ground_layer", {})
    if not ground.get("preferred_existing_source"):
        failures.append("No existing Unity-derived ground source found")

    boundary = manifest.get("boundary_layer", {})
    if boundary.get("source_schema") != "rok.unity_city_boundary.v1":
        failures.append("City boundary source manifest is missing or has the wrong schema")
    if boundary.get("instance_count", 0) <= 0:
        failures.append("No Unity city-boundary instances available")

    building = manifest.get("building_layer", {})
    if building.get("resolution_record_count", 0) <= 0:
        failures.append("No Unity InitBuilding resolution records available")
    if building.get("spawnable_count", 0) <= 0:
        failures.append("No spawnable Unity InitBuilding actors available")
    if building.get("core_missing_count", 0) > 0:
        failures.append(f"Core InitBuilding records are missing usable sprites: {building.get('core_missing_count')}")

    result = {
        "manifest": str(MANIFEST_PATH.relative_to(PROJECT_ROOT)),
        "failure_count": len(failures),
        "failures": failures,
        "preferred_ground_source": ground.get("preferred_existing_source", ""),
        "boundary_instance_count": boundary.get("instance_count", 0),
        "boundary_active_instance_count": boundary.get("active_instance_count", 0),
        "building_spawnable_count": building.get("spawnable_count", 0),
        "building_missing_count": building.get("missing_count", 0),
        "building_core_missing_count": building.get("core_missing_count", 0),
        "unresolved_decoration_count": building.get("unresolved_decoration_count", 0),
        "building_fallback_count": building.get("fallback_count", 0),
    }
    VERIFY_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")

    if failures:
        print("Rok Unity city visual foundation verification FAILED")
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)

    print(
        "Rok Unity city visual foundation verification passed: "
        f"ground={result['preferred_ground_source']} "
        f"boundary={result['boundary_instance_count']} "
        f"buildings={result['building_spawnable_count']}"
    )


if __name__ == "__main__":
    main()
