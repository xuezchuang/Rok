import json
from pathlib import Path
from rok_data_paths import resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
MANIFEST_PATH = OUTPUT_DIR / "unity_building_body_ground_layers.json"
VERIFY_PATH = OUTPUT_DIR / "unity_building_body_ground_layers_verify.json"


def exists(relative_path):
    return (resolve(relative_path)).exists()


def main():
    failures = []
    if not MANIFEST_PATH.exists():
        failures.append(f"Missing generated building body/ground layers manifest: {MANIFEST_PATH}")
        manifest = {}
    else:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    if manifest.get("schema") != "rok.unity_building_body_ground_layers.v1":
        failures.append(f"Unexpected schema: {manifest.get('schema')}")

    layers = manifest.get("layers", [])
    if not layers:
        failures.append("No building body/ground layer records")

    missing_body = [item.get("body_layer") for item in layers if not item.get("body_layer") or not exists(item.get("body_layer"))]
    missing_ground = [item.get("ground_patch_layer") for item in layers if not item.get("ground_patch_layer") or not exists(item.get("ground_patch_layer"))]
    if missing_body:
        failures.append(f"Missing body layer files: {missing_body[:12]}")
    if missing_ground:
        failures.append(f"Missing ground patch files: {missing_ground[:12]}")

    if manifest.get("body_layer_count", 0) != len(layers):
        failures.append("Body layer count does not match record count")
    if manifest.get("ground_patch_count", 0) <= 0:
        failures.append("No ground patch layers produced")

    result = {
        "manifest": str(MANIFEST_PATH.relative_to(PROJECT_ROOT)),
        "failure_count": len(failures),
        "failures": failures,
        "record_count": len(layers),
        "body_layer_count": manifest.get("body_layer_count", 0),
        "ground_patch_count": manifest.get("ground_patch_count", 0),
        "unique_sprite_count": manifest.get("unique_sprite_count", 0),
    }
    VERIFY_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")

    if failures:
        print("Rok Unity building body/ground layers verification FAILED")
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)

    print(
        "Rok Unity building body/ground layers verification passed: "
        f"records={result['record_count']} body={result['body_layer_count']} ground={result['ground_patch_count']}"
    )


if __name__ == "__main__":
    main()
