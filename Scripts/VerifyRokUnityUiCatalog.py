import json
from pathlib import Path
from rok_data_paths import DATA_ROOT, label


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
CATALOG_PATH = DATA_ROOT / "unity_ui_catalog.json"
VERIFY_PATH = OUTPUT_DIR / "unity_ui_catalog_verify.json"

CORE_MAIN_MODULES = {
    "HotTask",
    "Features",
    "Queue",
    "Map",
    "PlayerResources",
    "CityOrWorld",
    "Mail",
}

CORE_SLICES = {
    "main_interface",
    "building_menu",
    "build_city",
    "worker_queue",
    "quest_task",
}


def main():
    failures = []
    if not CATALOG_PATH.exists():
        failures.append(f"Missing generated UI catalog: {CATALOG_PATH}")
        catalog = {}
    else:
        catalog = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))

    if catalog.get("schema") != "rok.unity_ui_catalog.v1":
        failures.append(f"Unexpected schema: {catalog.get('schema')}")

    modules = set(catalog.get("main_interface_modules", []))
    missing_modules = sorted(CORE_MAIN_MODULES - modules)
    if missing_modules:
        failures.append(f"Missing core MainInterface modules: {missing_modules}")

    slices = catalog.get("ui_slices", [])
    slice_ids = {item.get("id") for item in slices}
    missing_slices = sorted(CORE_SLICES - slice_ids)
    if missing_slices:
        failures.append(f"Missing core UI slices: {missing_slices}")

    for item in slices:
        for key, exists in item.get("source_exists", {}).items():
            if not exists:
                failures.append(f"{item.get('id')} missing source {key}: {item.get(key)}")
        if not item.get("ue_target_exists"):
            failures.append(f"{item.get('id')} missing UE target: {item.get('ue_target')}")
        stats = item.get("prefab_stats", {})
        if not stats.get("exists"):
            failures.append(f"{item.get('id')} missing prefab stats")
        if stats.get("named_object_count", 0) <= 0:
            failures.append(f"{item.get('id')} has no named prefab objects")

    missing_texture_sources = [
        item.get("path")
        for item in catalog.get("ui_texture_sources", [])
        if not item.get("exists")
    ]
    if missing_texture_sources:
        failures.append(f"Missing Unity UI texture sources: {missing_texture_sources}")

    missing_ue_textures = [
        item.get("path")
        for item in catalog.get("ue_texture_targets", [])
        if not item.get("exists")
    ]
    if missing_ue_textures:
        failures.append(f"Missing processed UE UI textures: {missing_ue_textures}")

    result = {
        "catalog": str(Path(label(CATALOG_PATH))),
        "failure_count": len(failures),
        "failures": failures,
        "main_interface_module_count": len(modules),
        "ui_slice_count": len(slices),
    }
    VERIFY_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")

    if failures:
        print("Rok Unity UI catalog verification FAILED")
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)

    print(
        "Rok Unity UI catalog verification passed: "
        f"modules={result['main_interface_module_count']} slices={result['ui_slice_count']}"
    )


if __name__ == "__main__":
    main()
