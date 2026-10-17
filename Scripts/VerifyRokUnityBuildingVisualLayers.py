import json
from pathlib import Path
from rok_data_paths import resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
MANIFEST_PATH = OUTPUT_DIR / "unity_building_visual_layers.json"
VERIFY_PATH = OUTPUT_DIR / "unity_building_visual_layers_verify.json"


def exists(relative_path):
    return (resolve(relative_path)).exists()


def main():
    failures = []
    if not MANIFEST_PATH.exists():
        failures.append(f"Missing generated building visual layers manifest: {MANIFEST_PATH}")
        manifest = {}
    else:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    if manifest.get("schema") != "rok.unity_building_visual_layers.v1":
        failures.append(f"Unexpected schema: {manifest.get('schema')}")

    layers = manifest.get("layers", [])
    if not layers:
        failures.append("No building visual layer records")

    generated = [item for item in layers if item.get("generated")]
    if len(generated) != len(layers):
        failures.append(f"Only {len(generated)} of {len(layers)} records generated shadows")

    missing_shadow_files = [
        item.get("shadow_layer")
        for item in generated
        if not item.get("shadow_layer") or not exists(item.get("shadow_layer"))
    ]
    if missing_shadow_files:
        failures.append(f"Missing generated shadow files: {missing_shadow_files[:12]}")

    if manifest.get("mask_backed_count", 0) <= 0:
        failures.append("No building visual layers are backed by Unity mask sources")

    result = {
        "manifest": str(MANIFEST_PATH.relative_to(PROJECT_ROOT)),
        "failure_count": len(failures),
        "failures": failures,
        "record_count": len(layers),
        "generated_shadow_count": len(generated),
        "mask_backed_count": manifest.get("mask_backed_count", 0),
        "unique_shadow_count": manifest.get("unique_shadow_count", 0),
    }
    VERIFY_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")

    if failures:
        print("Rok Unity building visual layers verification FAILED")
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)

    print(
        "Rok Unity building visual layers verification passed: "
        f"records={result['record_count']} shadows={result['generated_shadow_count']} masks={result['mask_backed_count']}"
    )


if __name__ == "__main__":
    main()
