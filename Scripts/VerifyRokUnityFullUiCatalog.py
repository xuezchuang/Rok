import json
from pathlib import Path
from rok_data_paths import resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
CATALOG_PATH = OUTPUT_DIR / "unity_full_ui_catalog.json"
VERIFY_PATH = OUTPUT_DIR / "unity_full_ui_catalog_verify.json"

EXPECTED_COUNTS = {
    "ui_prefab_count": 1150,
    "ui_prefab_unique_basename_count": 1148,
    "ui_prefab_family_count": 20,
    "all_ui_prefab_count": 1240,
    "physical_view_source_file_count": 1253,
    "logical_view_count": 813,
    "subview_count": 441,
    "screen_side_view_count": 372,
    "mediator_candidate_file_count": 252,
    "concrete_mediator_count": 251,
}

EXPECTED_RELATIONSHIP_COUNTS = {
    "matched_logical_view_count": 759,
    "unmatched_logical_view_count": 54,
    "matched_ui_prefab_path_count": 720,
    "unmatched_ui_prefab_path_count": 430,
}

EXPECTED_DUPLICATE_PREFABS = {
    "ui_item_mainifeventactivity": {
        (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_common/UICommon/"
            "UI_Item_MainIFEventActivity.prefab"
        ),
        (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_noaddressable/"
            "UI_Item_MainIFEventActivity.prefab"
        ),
    },
    "ui_item_mainifeventcharge": {
        (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_common/UICommon/"
            "UI_Item_MainIFEventCharge.prefab"
        ),
        (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_noaddressable/"
            "UI_Item_MainIFEventCharge.prefab"
        ),
    },
}

EXPECTED_NON_CONCRETE_MEDIATOR_FILE = (
    "ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/"
    "WorldEffectMediators/IBaseWorldEffectMediator.cs"
)


def load_catalog(failures):
    if not CATALOG_PATH.exists():
        failures.append(f"Missing generated catalog: {CATALOG_PATH}")
        return {}
    try:
        return json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        failures.append(f"Unable to read generated catalog: {error}")
        return {}


def verify_summary(catalog, failures):
    summary = catalog.get("summary", {})
    for key, expected in EXPECTED_COUNTS.items():
        actual = summary.get(key)
        if actual != expected:
            failures.append(
                f"Unexpected {key}: expected {expected}, got {actual}"
            )


def verify_relationships(catalog, failures):
    summary = catalog.get("relationship_summary", {})
    for key, expected in EXPECTED_RELATIONSHIP_COUNTS.items():
        actual = summary.get(key)
        if actual != expected:
            failures.append(
                f"Unexpected relationship {key}: "
                f"expected {expected}, got {actual}"
            )

    logical_views = catalog.get("logical_views", [])
    matched_views = sum(
        1 for item in logical_views if item.get("prefab_paths")
    )
    if matched_views != summary.get("matched_logical_view_count"):
        failures.append(
            "Relationship summary does not match logical view records: "
            f"summary={summary.get('matched_logical_view_count')} "
            f"records={matched_views}"
        )

    mapped_prefab_paths = {
        path
        for item in logical_views
        for path in item.get("prefab_paths", [])
    }
    if len(mapped_prefab_paths) != summary.get(
        "matched_ui_prefab_path_count"
    ):
        failures.append(
            "Relationship summary does not match mapped prefab paths: "
            f"summary={summary.get('matched_ui_prefab_path_count')} "
            f"records={len(mapped_prefab_paths)}"
        )


def verify_inventory_paths(catalog, failures):
    ui_prefabs = catalog.get("ui_prefabs", [])
    ui_prefab_paths = [item.get("path") for item in ui_prefabs]
    if len(ui_prefab_paths) != len(set(ui_prefab_paths)):
        failures.append("UIPrefab path inventory contains duplicate paths")

    all_ui_prefabs = catalog.get("all_ui_prefabs", [])
    all_ui_prefab_paths = [item.get("path") for item in all_ui_prefabs]
    if len(all_ui_prefab_paths) != len(set(all_ui_prefab_paths)):
        failures.append("All-UI prefab inventory contains duplicate paths")

    for path_text in ui_prefab_paths + all_ui_prefab_paths:
        if not isinstance(path_text, str):
            failures.append(f"Invalid prefab path value: {path_text!r}")
            continue
        if "\\" in path_text:
            failures.append(f"Prefab path is not normalized: {path_text}")
            continue
        if not (resolve(path_text)).is_file():
            failures.append(f"Missing inventoried prefab: {path_text}")

    family_counts = {
        item.get("name"): item.get("prefab_count")
        for item in catalog.get("prefab_families", [])
    }
    if sum(value or 0 for value in family_counts.values()) != len(ui_prefabs):
        failures.append(
            "Prefab family counts do not sum to the UIPrefab inventory"
        )


def verify_view_inventory(catalog, failures):
    views = catalog.get("logical_views", [])
    class_names = [item.get("class_name") for item in views]
    if len(class_names) != len(set(class_names)):
        failures.append("Logical View class names are not unique")
    if "UI_SubView" in class_names:
        failures.append("Framework class UI_SubView was not excluded")

    actual_subviews = sum(
        1 for item in views if item.get("view_type") == "subview"
    )
    actual_screen_views = sum(
        1 for item in views if item.get("view_type") == "screen_side"
    )
    if actual_subviews != EXPECTED_COUNTS["subview_count"]:
        failures.append(
            f"Logical SubView records: expected "
            f"{EXPECTED_COUNTS['subview_count']}, got {actual_subviews}"
        )
    if actual_screen_views != EXPECTED_COUNTS["screen_side_view_count"]:
        failures.append(
            f"Screen-side View records: expected "
            f"{EXPECTED_COUNTS['screen_side_view_count']}, "
            f"got {actual_screen_views}"
        )

    parse = catalog.get("view_parse", {})
    if parse.get("parse_failures"):
        failures.append(
            f"View source parse failures: {parse.get('parse_failures')}"
        )
    if parse.get("framework_view_classes_excluded") != ["UI_SubView"]:
        failures.append(
            "Unexpected framework View exclusion set: "
            f"{parse.get('framework_view_classes_excluded')}"
        )


def verify_mediators(catalog, failures):
    mediators = catalog.get("mediators", [])
    class_names = [item.get("class_name") for item in mediators]
    if len(class_names) != len(set(class_names)):
        failures.append("Concrete Mediator class names are not unique")

    no_class_files = set(
        catalog.get("mediator_parse", {}).get(
            "no_concrete_class_files", []
        )
    )
    if no_class_files != {EXPECTED_NON_CONCRETE_MEDIATOR_FILE}:
        failures.append(
            "Unexpected Mediator files without a concrete class: "
            f"{sorted(no_class_files)}"
        )


def verify_duplicate_basenames(catalog, failures):
    actual_duplicates = {
        item.get("basename_key"): set(item.get("paths", []))
        for item in catalog.get("duplicate_prefab_basenames", [])
    }
    if actual_duplicates != EXPECTED_DUPLICATE_PREFABS:
        failures.append(
            "Duplicate prefab basenames changed: "
            f"expected={EXPECTED_DUPLICATE_PREFABS} "
            f"actual={actual_duplicates}"
        )

    ui_prefabs = catalog.get("ui_prefabs", [])
    for basename_key, expected_paths in EXPECTED_DUPLICATE_PREFABS.items():
        inventoried_paths = {
            item.get("path")
            for item in ui_prefabs
            if item.get("basename_key") == basename_key
        }
        if inventoried_paths != expected_paths:
            failures.append(
                f"Duplicate basename {basename_key} was collapsed: "
                f"{sorted(inventoried_paths)}"
            )


def main():
    failures = []
    catalog = load_catalog(failures)

    if catalog.get("schema") != "rok.unity_full_ui_catalog.v1":
        failures.append(f"Unexpected schema: {catalog.get('schema')}")

    if catalog:
        verify_summary(catalog, failures)
        verify_relationships(catalog, failures)
        verify_inventory_paths(catalog, failures)
        verify_view_inventory(catalog, failures)
        verify_mediators(catalog, failures)
        verify_duplicate_basenames(catalog, failures)

    result = {
        "catalog": str(CATALOG_PATH.relative_to(PROJECT_ROOT)).replace(
            "\\", "/"
        ),
        "failure_count": len(failures),
        "failures": failures,
        "expected_counts": EXPECTED_COUNTS,
        "actual_counts": catalog.get("summary", {}),
        "expected_relationship_counts": EXPECTED_RELATIONSHIP_COUNTS,
        "actual_relationship_counts": catalog.get(
            "relationship_summary", {}
        ),
    }
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    VERIFY_PATH.write_text(
        json.dumps(result, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    if failures:
        print("Rok Unity full UI catalog verification FAILED")
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)

    summary = catalog["summary"]
    relationships = catalog["relationship_summary"]
    print(
        "Rok Unity full UI catalog verification passed: "
        f"families={summary['ui_prefab_family_count']} "
        f"ui_prefabs={summary['ui_prefab_count']} "
        f"all_ui_prefabs={summary['all_ui_prefab_count']} "
        f"views={summary['logical_view_count']} "
        f"mediators={summary['concrete_mediator_count']} "
        f"mapped_views={relationships['matched_logical_view_count']} "
        f"mapped_prefabs={relationships['matched_ui_prefab_path_count']}"
    )


if __name__ == "__main__":
    main()
