import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
MANIFEST_PATH = OUTPUT_DIR / "unity_city_visual_layer_stack.json"
VERIFY_PATH = OUTPUT_DIR / "unity_city_visual_layer_stack_verify.json"


def main():
    failures = []
    if not MANIFEST_PATH.exists():
        failures.append(f"Missing generated city visual layer stack manifest: {MANIFEST_PATH}")
        manifest = {}
    else:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    if manifest.get("schema") != "rok.unity_city_visual_layer_stack.v1":
        failures.append(f"Unexpected schema: {manifest.get('schema')}")

    layers = manifest.get("layers", [])
    roles = {layer.get("role") for layer in layers}
    for role in manifest.get("required_roles", []):
        if role not in roles:
            failures.append(f"Missing required visual layer role: {role}")

    missing_sources = [layer.get("source") for layer in layers if not layer.get("exists")]
    if missing_sources:
        failures.append(f"Missing visual layer sources: {missing_sources}")

    z_values = [layer.get("z", 0.0) for layer in layers]
    if z_values != sorted(z_values):
        failures.append("Visual layer Z order is not ascending")

    result = {
        "manifest": str(MANIFEST_PATH.relative_to(PROJECT_ROOT)),
        "failure_count": len(failures),
        "failures": failures,
        "layer_count": len(layers),
        "existing_layer_count": manifest.get("existing_layer_count", 0),
        "roles": sorted(roles),
    }
    VERIFY_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")

    if failures:
        print("Rok Unity city visual layer stack verification FAILED")
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)

    print(
        "Rok Unity city visual layer stack verification passed: "
        f"layers={result['layer_count']} roles={','.join(result['roles'])}"
    )


if __name__ == "__main__":
    main()
