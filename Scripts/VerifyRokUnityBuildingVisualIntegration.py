import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
SHADOW_MANIFEST_PATH = OUTPUT_DIR / "unity_building_visual_layers.json"
BODY_GROUND_MANIFEST_PATH = OUTPUT_DIR / "unity_building_body_ground_layers.json"
VERIFY_PATH = OUTPUT_DIR / "unity_building_visual_integration_verify.json"


def load_json(path):
    if not path.exists():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def exists(relative_path):
    return (PROJECT_ROOT / relative_path).exists()


def main():
    failures = []
    shadow = load_json(SHADOW_MANIFEST_PATH)
    body_ground = load_json(BODY_GROUND_MANIFEST_PATH)

    if shadow.get("schema") != "rok.unity_building_visual_layers.v1":
        failures.append("Missing or invalid building shadow manifest")
    if body_ground.get("schema") != "rok.unity_building_body_ground_layers.v1":
        failures.append("Missing or invalid building body/ground manifest")

    shadow_layers = {int(item.get("init_id", 0)): item for item in shadow.get("layers", [])}
    body_layers = {int(item.get("init_id", 0)): item for item in body_ground.get("layers", [])}
    init_ids = sorted(set(shadow_layers) | set(body_layers))

    for init_id in init_ids:
        shadow_item = shadow_layers.get(init_id)
        body_item = body_layers.get(init_id)
        if not shadow_item:
            failures.append(f"Init {init_id} missing shadow record")
            continue
        if not body_item:
            failures.append(f"Init {init_id} missing body/ground record")
            continue
        if not exists(shadow_item.get("shadow_layer", "")):
            failures.append(f"Init {init_id} missing shadow texture: {shadow_item.get('shadow_layer')}")
        if not exists(body_item.get("body_layer", "")):
            failures.append(f"Init {init_id} missing body texture: {body_item.get('body_layer')}")
        if not exists(body_item.get("ground_patch_layer", "")):
            failures.append(f"Init {init_id} missing ground patch texture: {body_item.get('ground_patch_layer')}")

    result = {
        "failure_count": len(failures),
        "failures": failures,
        "integrated_init_count": len(init_ids),
        "shadow_record_count": len(shadow_layers),
        "body_ground_record_count": len(body_layers),
        "expected_runtime_tags": [
            "UnityBuildingVisualLayer_GroundPatch",
            "UnityBuildingVisualLayer_Shadow",
            "UnityBuildingVisualLayer_BodyClean",
        ],
    }
    VERIFY_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")

    if failures:
        print("Rok Unity building visual integration verification FAILED")
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)

    print(
        "Rok Unity building visual integration verification passed: "
        f"init={result['integrated_init_count']} "
        f"shadow={result['shadow_record_count']} body={result['body_ground_record_count']}"
    )


if __name__ == "__main__":
    main()
