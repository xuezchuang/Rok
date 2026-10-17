import json
import re
from collections import Counter
from pathlib import Path
from rok_data_paths import resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
FULL_CATALOG_PATH = OUTPUT_DIR / "unity_full_ui_catalog.json"
COVERAGE_PATH = OUTPUT_DIR / "unity_ui_runtime_coverage.json"
VERIFY_PATH = OUTPUT_DIR / "unity_ui_runtime_coverage_verify.json"

EXPECTED_MAPPING_IDS = {
    "scout",
    "city_hud",
    "world_hud",
    "server_list",
    "player_profile",
    "player_head_selection",
    "player_power_details",
    "player_ranking",
    "player_civilization",
    "player_civilization_flag_item",
    "player_civilization_confirmation",
    "settings_hub",
    "city_manager",
    "army_show",
    "general_settings",
    "notification_settings",
    "language_settings",
    "help",
    "account_settings",
    "army_statistics",
    "store",
    "resources",
    "building_info",
    "building_upgrade",
    "building_menu",
    "build_city",
    "worker_queue",
    "hospital",
    "train_army",
    "speed_up",
    "activity",
    "mail",
    "tasks",
    "commander",
    "items",
    "alliance",
    "vip",
    "chat",
    "world_position",
    "bookmarks",
    "resource_search",
    "expedition",
    "war_warning",
    "charge",
    "buff_list",
    "gem_shop",
}
BEHAVIOR_STATES = (
    "preview_shell",
    "real_local_loop",
    "service_boundary",
    "unimplemented",
)


def load_json(path, failures):
    if not path.is_file():
        failures.append(f"Missing generated input: {path}")
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        failures.append(f"Unable to read {path}: {error}")
        return {}


def read_source(repo_path, failures):
    path = resolve(repo_path)
    if not path.is_file():
        failures.append(f"Missing declared UE source: {repo_path}")
        return ""
    return path.read_text(encoding="utf-8", errors="ignore")


def verify_widget(mapping, failures):
    widget = mapping.get("runtime_widget", {})
    feature_id = mapping.get("id")
    class_name = widget.get("class", "")
    header_path = widget.get("header", "")
    cpp_path = widget.get("cpp", "")
    method = widget.get("open_method", "")
    if "Subsystem" in class_name or "Subsystem" in header_path:
        failures.append(
            f"{feature_id}: subsystem must not be counted as a widget"
        )
    header = read_source(header_path, failures)
    cpp = read_source(cpp_path, failures)
    inheritance_pattern = re.compile(
        rf"class\s+(?:\w+_API\s+)?{re.escape(class_name)}\s*"
        rf":\s*public\s+UUserWidget\b"
    )
    checks = {
        "header_exists": bool(header),
        "cpp_exists": bool(cpp),
        "class_declared": bool(
            re.search(rf"\bclass\b[^\n]*\b{re.escape(class_name)}\b", header)
        ),
        "inherits_user_widget": bool(inheritance_pattern.search(header)),
        "method_declared": bool(
            re.search(rf"\b{re.escape(method)}\s*\(", header)
        ),
        "method_defined": bool(
            re.search(
                rf"\b{re.escape(class_name)}::{re.escape(method)}\s*\(",
                cpp,
            )
        ),
    }
    for key, current_value in checks.items():
        if widget.get(key) != current_value:
            failures.append(
                f"{feature_id}: stale widget evidence {key}; "
                f"artifact={widget.get(key)} current={current_value}"
            )
        if not current_value:
            failures.append(
                f"{feature_id}: runtime widget check failed: {key}"
            )


def verify_activation(mapping, failures):
    activation = mapping.get("runtime_activation")
    if not activation:
        return
    source = read_source(activation.get("path", ""), failures)
    current = all(
        token in source for token in activation.get("tokens", [])
    )
    if activation.get("tokens_present") != current:
        failures.append(
            f"{mapping.get('id')}: stale runtime activation evidence"
        )
    if not current:
        failures.append(
            f"{mapping.get('id')}: runtime activation tokens are missing"
        )


def verify_entry(feature_id, item, failures):
    owner_class = item.get("owner_class", "")
    handler = item.get("handler", "")
    header = read_source(item.get("header", ""), failures)
    cpp = read_source(item.get("cpp", ""), failures)
    checks = {
        "owner_header_exists": bool(header),
        "owner_cpp_exists": bool(cpp),
        "owner_class_declared": bool(
            re.search(
                rf"\bclass\b[^\n]*\b{re.escape(owner_class)}\b",
                header,
            )
        ),
        "handler_declared": bool(
            re.search(rf"\b{re.escape(handler)}\s*\(", header)
        ),
        "handler_defined": bool(
            re.search(
                rf"\b{re.escape(owner_class)}::{re.escape(handler)}\s*\(",
                cpp,
            )
        ),
        "binding_present": f"&{owner_class}::{handler}" in cpp,
    }
    factory = item.get("control_factory")
    if factory:
        expected_name = (
            factory.get("seed", "") + factory.get("generated_suffix", "")
        )
        if item.get("control_name") != expected_name:
            failures.append(
                f"{feature_id}: generated control name is inconsistent: "
                f"{item.get('control_name')} != {expected_name}"
            )
        checks["control_present"] = (
            f'TEXT("{factory.get("seed", "")}")' in cpp
            and 'TEXT("%sButton")' in cpp
            and factory.get("name", "") in cpp
        )
    else:
        checks["control_present"] = (
            f'TEXT("{item.get("control_literal", "")}")' in cpp
        )

    for key, current_value in checks.items():
        if item.get(key) != current_value:
            failures.append(
                f"{feature_id}/{item.get('control_name')}: "
                f"stale entry evidence {key}; "
                f"artifact={item.get(key)} current={current_value}"
            )
        if not current_value:
            failures.append(
                f"{feature_id}/{item.get('control_name')}: "
                f"entry check failed: {key}"
            )

    route_evidence = item.get("route_evidence")
    if route_evidence:
        route_source = read_source(
            route_evidence.get("path", ""),
            failures,
        )
        current_route = all(
            token in route_source
            for token in route_evidence.get("tokens", [])
        )
        if route_evidence.get("tokens_present") != current_route:
            failures.append(
                f"{feature_id}/{item.get('control_name')}: "
                "stale route evidence"
            )
        if not current_route:
            failures.append(
                f"{feature_id}/{item.get('control_name')}: "
                "context-sensitive route evidence is missing"
            )


def verify_behavior(mapping, failures):
    feature_id = mapping.get("id")
    behavior = mapping.get("behavior", {})
    declared_true_states = {
        state for state in BEHAVIOR_STATES if behavior.get(state)
    }
    evidence_states = Counter()
    for item in behavior.get("evidence", []):
        state = item.get("state")
        if state not in BEHAVIOR_STATES:
            failures.append(
                f"{feature_id}: unknown behavior evidence state {state}"
            )
            continue
        source = read_source(item.get("path", ""), failures)
        current = all(
            token in source for token in item.get("tokens", [])
        )
        if item.get("tokens_present") != current:
            failures.append(
                f"{feature_id}: stale {state} evidence"
            )
        if not current:
            failures.append(
                f"{feature_id}: missing source evidence for {state}"
            )
        evidence_states[state] += 1
    for state in declared_true_states - {"unimplemented"}:
        if evidence_states[state] == 0:
            failures.append(
                f"{feature_id}: {state} has no source evidence"
            )
    if behavior.get("unimplemented"):
        failures.append(
            f"{feature_id}: explicit mapping cannot also be unimplemented"
        )


def verify_summary(coverage, full_catalog, failures):
    summary = coverage.get("summary", {})
    mappings = coverage.get("mappings", [])
    mapping_ids = [item.get("id") for item in mappings]
    if set(mapping_ids) != EXPECTED_MAPPING_IDS:
        failures.append(
            "Explicit mapping ids differ from the required known feature set"
        )
    if len(mapping_ids) != len(set(mapping_ids)):
        failures.append("Explicit mapping ids are not unique")

    full_prefab_paths = {
        item.get("path") for item in full_catalog.get("ui_prefabs", [])
    }
    mapped_paths = {
        item.get("unity_source", {}).get("prefab") for item in mappings
    }
    unmapped_records = coverage.get("unimplemented_catalog_prefabs", [])
    unmapped_paths = {item.get("path") for item in unmapped_records}
    expected_unmapped = full_prefab_paths - mapped_paths
    if unmapped_paths != expected_unmapped:
        failures.append(
            "Unimplemented prefab inventory is not the exact catalog "
            "remainder"
        )
    if any(
        item.get("implementation_state") != "unimplemented"
        for item in unmapped_records
    ):
        failures.append(
            "An unmapped prefab lacks the unimplemented classification"
        )

    runtime_screen_mapping_count = sum(
        1
        for item in mappings
        if item.get("runtime_widget", {}).get("inherits_user_widget")
        and item.get("runtime_widget", {}).get("method_declared")
        and item.get("runtime_widget", {}).get("method_defined")
    )
    entry_mapping_count = sum(
        len(item.get("entries", [])) for item in mappings
    )
    expected = {
        "full_catalog_ui_prefab_count": len(full_prefab_paths),
        "declared_mapping_count": len(mappings),
        "mapped_unique_prefab_count": len(mapped_paths),
        "unimplemented_catalog_prefab_count": len(expected_unmapped),
        "runtime_screen_mapping_count": runtime_screen_mapping_count,
        "runtime_widget_class_count": len(
            {
                item.get("runtime_widget", {}).get("class")
                for item in mappings
                if item.get("runtime_widget", {}).get("inherits_user_widget")
                and item.get("runtime_widget", {}).get("method_declared")
                and item.get("runtime_widget", {}).get("method_defined")
            }
        ),
        "entry_mapping_count": entry_mapping_count,
        "unique_entry_control_count": len(
            {
                (
                    entry.get("owner_class"),
                    entry.get("control_name"),
                )
                for item in mappings
                for entry in item.get("entries", [])
            }
        ),
        "runtime_screen_widget_count": runtime_screen_mapping_count,
        "entry_count": entry_mapping_count,
        "hud_entry_count": sum(
            1
            for item in mappings
            for entry in item.get("entries", [])
            if entry.get("kind") == "hud_entry"
        ),
    }
    for state in BEHAVIOR_STATES:
        expected[f"{state}_mapping_count"] = sum(
            1
            for item in mappings
            if item.get("behavior", {}).get(state)
        )
    for key, value in expected.items():
        if summary.get(key) != value:
            failures.append(
                f"Summary mismatch for {key}: "
                f"artifact={summary.get(key)} expected={value}"
            )

    family_counts = Counter(
        item.get("family") for item in unmapped_records
    )
    artifact_family_counts = {
        item.get("family"): item.get("prefab_count")
        for item in coverage.get("unimplemented_by_family", [])
    }
    if artifact_family_counts != dict(family_counts):
        failures.append("Unimplemented family statistics are inconsistent")


def main():
    failures = []
    full_catalog = load_json(FULL_CATALOG_PATH, failures)
    coverage = load_json(COVERAGE_PATH, failures)
    if coverage:
        if coverage.get("schema") != "rok.unity_ui_runtime_coverage.v1":
            failures.append(
                f"Unexpected coverage schema: {coverage.get('schema')}"
            )
        for mapping in coverage.get("mappings", []):
            source = mapping.get("unity_source", {})
            prefab_path = source.get("prefab")
            catalog_paths = {
                item.get("path")
                for item in full_catalog.get("ui_prefabs", [])
            }
            current_exists = prefab_path in catalog_paths
            if source.get("catalog_record_exists") != current_exists:
                failures.append(
                    f"{mapping.get('id')}: stale Unity prefab evidence"
                )
            if not current_exists:
                failures.append(
                    f"{mapping.get('id')}: Unity prefab absent from catalog"
                )
            verify_widget(mapping, failures)
            verify_activation(mapping, failures)
            for item in mapping.get("entries", []):
                verify_entry(mapping.get("id"), item, failures)
            verify_behavior(mapping, failures)
        verify_summary(coverage, full_catalog, failures)

    result = {
        "schema": "rok.unity_ui_runtime_coverage.verify.v1",
        "coverage": COVERAGE_PATH.relative_to(PROJECT_ROOT).as_posix(),
        "passed": not failures,
        "failure_count": len(failures),
        "failures": failures,
        "checked_mapping_count": len(coverage.get("mappings", [])),
        "checked_entry_count": sum(
            len(item.get("entries", []))
            for item in coverage.get("mappings", [])
        ),
        "checked_unimplemented_prefab_count": len(
            coverage.get("unimplemented_catalog_prefabs", [])
        ),
    }
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    VERIFY_PATH.write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    if failures:
        print(
            f"Unity UI runtime coverage verification failed: "
            f"{len(failures)} issue(s)."
        )
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)
    print(
        "Unity UI runtime coverage verification passed: "
        f"{result['checked_mapping_count']} mappings, "
        f"{result['checked_entry_count']} entry controls, "
        f"{result['checked_unimplemented_prefab_count']} unimplemented "
        "catalog prefabs checked."
    )


if __name__ == "__main__":
    main()
