import json
import re
from collections import Counter, defaultdict
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
UNITY_UI_ROOT = (
    PROJECT_ROOT
    / "ref"
    / "client-unity"
    / "Assets"
    / "BundleAssets"
    / "UI"
)
UI_PREFAB_ROOT = UNITY_UI_ROOT / "UIPrefabs"
MVC_ROOT = (
    PROJECT_ROOT
    / "ref"
    / "client-unity"
    / "Assets"
    / "Scripts"
    / "Hotfix"
    / "MVC"
)
VIEW_ROOT = MVC_ROOT / "View_Mediator"

OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
OUTPUT_JSON = OUTPUT_DIR / "unity_full_ui_catalog.json"
OUTPUT_MD = OUTPUT_DIR / "unity_full_ui_catalog.md"

VIEW_CLASS_PATTERN = re.compile(
    r"\b(?:partial\s+)?class\s+([A-Za-z_]\w*View)\b"
)
VIEW_NAME_PATTERN = re.compile(
    r'\b(?:public\s+)?const\s+string\s+VIEW_NAME\s*=\s*"([^"]+)"'
)
MEDIATOR_CLASS_PATTERN = re.compile(
    r"\bclass\s+([A-Za-z_]\w*Mediator)\b"
)


def repo_path(path):
    return path.relative_to(PROJECT_ROOT).as_posix()


def sorted_paths(paths):
    return sorted(paths, key=lambda path: path.as_posix().casefold())


def read_source(path):
    return path.read_text(encoding="utf-8", errors="ignore")


def classify_prefab_basename(basename):
    classification_rules = (
        ("UI_Win_", "window"),
        ("UI_IF_", "interface"),
        ("UI_Hud_", "hud"),
        ("UI_Pop_", "popup"),
        ("UI_Tip_", "tip"),
        ("UI_Item_", "item"),
        ("UI_LItem_", "list_item"),
        ("UI_Model_", "model"),
        ("UI_Common_", "common"),
        ("UI_Effect_", "effect"),
        ("UE_", "effect"),
    )
    folded_name = basename.casefold()
    for prefix, classification in classification_rules:
        if folded_name.startswith(prefix.casefold()):
            return classification
    return "other"


def build_prefab_inventory():
    ui_prefab_paths = sorted_paths(UI_PREFAB_ROOT.rglob("*.prefab"))
    all_ui_prefab_paths = sorted_paths(UNITY_UI_ROOT.rglob("*.prefab"))

    prefab_paths_by_basename = defaultdict(list)
    for path in ui_prefab_paths:
        prefab_paths_by_basename[path.stem.casefold()].append(path)

    duplicate_groups = []
    for key in sorted(prefab_paths_by_basename):
        paths = sorted_paths(prefab_paths_by_basename[key])
        if len(paths) <= 1:
            continue
        duplicate_groups.append(
            {
                "basename_key": key,
                "display_basename": paths[0].stem,
                "path_count": len(paths),
                "paths": [repo_path(path) for path in paths],
            }
        )

    all_ui_prefabs = []
    all_ui_section_counts = Counter()
    for path in all_ui_prefab_paths:
        relative = path.relative_to(UNITY_UI_ROOT)
        section = relative.parts[0] if relative.parts else "."
        all_ui_section_counts[section] += 1
        all_ui_prefabs.append(
            {
                "path": repo_path(path),
                "basename": path.stem,
                "section": section,
            }
        )

    return {
        "ui_prefab_paths": ui_prefab_paths,
        "all_ui_prefabs": all_ui_prefabs,
        "all_ui_section_counts": dict(sorted(all_ui_section_counts.items())),
        "prefab_paths_by_basename": prefab_paths_by_basename,
        "duplicate_groups": duplicate_groups,
    }


def build_view_inventory(prefab_paths_by_basename):
    source_files = sorted_paths(VIEW_ROOT.rglob("*View.cs"))
    view_classes = {}
    parse_failures = []

    for path in source_files:
        source = read_source(path)
        class_match = VIEW_CLASS_PATTERN.search(source)
        if not class_match:
            parse_failures.append(repo_path(path))
            continue

        class_name = class_match.group(1)
        record = view_classes.setdefault(
            class_name,
            {
                "source_paths": [],
                "view_names": set(),
                "source_categories": set(),
            },
        )
        record["source_paths"].append(repo_path(path))
        record["view_names"].update(VIEW_NAME_PATTERN.findall(source))
        relative = path.relative_to(VIEW_ROOT)
        record["source_categories"].add(
            relative.parts[0] if relative.parts else "."
        )

    framework_view_classes = []
    if "UI_SubView" in view_classes:
        framework_view_classes.append("UI_SubView")
        view_classes.pop("UI_SubView")

    logical_views = []
    relationship_rows = []
    matched_prefab_paths = set()
    relationship_strategy_counts = Counter()
    view_type_counts = Counter()
    view_source_category_counts = Counter()
    view_name_declared_count = 0
    prefab_to_view_classes = defaultdict(set)

    for class_name in sorted(view_classes, key=str.casefold):
        source_record = view_classes[class_name]
        source_paths = sorted(source_record["source_paths"], key=str.casefold)
        view_names = sorted(source_record["view_names"], key=str.casefold)
        source_categories = sorted(
            source_record["source_categories"], key=str.casefold
        )
        view_type = (
            "subview" if class_name.endswith("SubView") else "screen_side"
        )

        if view_names:
            match_candidates = list(view_names)
            matched_strategy = "view_name_exact"
            unmatched_strategy = "view_name_unmatched"
            view_name_declared_count += 1
        else:
            inferred_name = (
                class_name.removesuffix("_SubView")
                if class_name.endswith("_SubView")
                else class_name.removesuffix("View")
            )
            match_candidates = [inferred_name]
            matched_strategy = "class_name_inferred"
            unmatched_strategy = "class_name_unmatched"

        matched_paths = []
        for candidate in match_candidates:
            matched_paths.extend(
                prefab_paths_by_basename.get(candidate.casefold(), [])
            )
        matched_paths = sorted_paths(set(matched_paths))
        match_strategy = (
            matched_strategy if matched_paths else unmatched_strategy
        )

        matched_path_strings = [repo_path(path) for path in matched_paths]
        for matched_path in matched_path_strings:
            matched_prefab_paths.add(matched_path)
            prefab_to_view_classes[matched_path].add(class_name)
            relationship_rows.append(
                {
                    "view_class": class_name,
                    "prefab_path": matched_path,
                    "strategy": match_strategy,
                }
            )

        relationship_strategy_counts[match_strategy] += 1
        view_type_counts[view_type] += 1
        for category in source_categories:
            view_source_category_counts[category] += 1

        logical_views.append(
            {
                "class_name": class_name,
                "view_type": view_type,
                "source_paths": source_paths,
                "source_categories": source_categories,
                "view_names": view_names,
                "match_candidates": match_candidates,
                "match_strategy": match_strategy,
                "prefab_paths": matched_path_strings,
            }
        )

    relationship_rows.sort(
        key=lambda item: (
            item["view_class"].casefold(),
            item["prefab_path"].casefold(),
        )
    )
    matched_view_count = sum(
        1 for item in logical_views if item["prefab_paths"]
    )

    return {
        "physical_source_files": [repo_path(path) for path in source_files],
        "physical_source_file_count": len(source_files),
        "parsed_source_file_count": len(source_files) - len(parse_failures),
        "parse_failures": parse_failures,
        "framework_view_classes": framework_view_classes,
        "logical_views": logical_views,
        "logical_view_count": len(logical_views),
        "view_name_declared_count": view_name_declared_count,
        "view_type_counts": dict(sorted(view_type_counts.items())),
        "view_source_category_counts": dict(
            sorted(view_source_category_counts.items())
        ),
        "relationship_rows": relationship_rows,
        "relationship_strategy_counts": dict(
            sorted(relationship_strategy_counts.items())
        ),
        "matched_view_count": matched_view_count,
        "matched_prefab_paths": matched_prefab_paths,
        "prefab_to_view_classes": prefab_to_view_classes,
    }


def mediator_scope(path):
    relative = path.relative_to(MVC_ROOT)
    if not relative.parts:
        return "."
    if relative.parts[0] == "View_Mediator" and len(relative.parts) > 1:
        return f"View_Mediator/{relative.parts[1]}"
    return relative.parts[0]


def build_mediator_inventory():
    source_files = sorted_paths(MVC_ROOT.rglob("*Mediator.cs"))
    class_sources = defaultdict(set)
    source_scopes = defaultdict(set)
    no_concrete_class_files = []

    for path in source_files:
        source = read_source(path)
        class_names = MEDIATOR_CLASS_PATTERN.findall(source)
        if not class_names:
            no_concrete_class_files.append(repo_path(path))
            continue
        for class_name in class_names:
            class_sources[class_name].add(repo_path(path))
            source_scopes[class_name].add(mediator_scope(path))

    concrete_mediators = []
    mediator_scope_counts = Counter()
    for class_name in sorted(class_sources, key=str.casefold):
        scopes = sorted(source_scopes[class_name], key=str.casefold)
        for scope in scopes:
            mediator_scope_counts[scope] += 1
        concrete_mediators.append(
            {
                "class_name": class_name,
                "source_paths": sorted(
                    class_sources[class_name], key=str.casefold
                ),
                "source_scopes": scopes,
            }
        )

    return {
        "candidate_source_files": [repo_path(path) for path in source_files],
        "candidate_source_file_count": len(source_files),
        "concrete_mediators": concrete_mediators,
        "concrete_mediator_count": len(concrete_mediators),
        "no_concrete_class_files": sorted(
            no_concrete_class_files, key=str.casefold
        ),
        "source_scope_counts": dict(sorted(mediator_scope_counts.items())),
    }


def build_catalog():
    prefab_inventory = build_prefab_inventory()
    view_inventory = build_view_inventory(
        prefab_inventory["prefab_paths_by_basename"]
    )
    mediator_inventory = build_mediator_inventory()

    matched_prefab_paths = view_inventory["matched_prefab_paths"]
    prefab_to_view_classes = view_inventory["prefab_to_view_classes"]
    ui_prefab_paths = prefab_inventory["ui_prefab_paths"]

    prefab_basename_classification_counts = Counter()
    prefab_family_records = defaultdict(list)
    ui_prefabs = []

    for path in ui_prefab_paths:
        relative = path.relative_to(UI_PREFAB_ROOT)
        family = relative.parts[0] if relative.parts else "."
        full_path = repo_path(path)
        basename_classification = classify_prefab_basename(path.stem)
        view_classes = sorted(
            prefab_to_view_classes.get(full_path, set()), key=str.casefold
        )

        prefab_basename_classification_counts[basename_classification] += 1
        prefab_family_records[family].append(full_path)
        ui_prefabs.append(
            {
                "path": full_path,
                "relative_path": relative.as_posix(),
                "basename": path.stem,
                "basename_key": path.stem.casefold(),
                "family": family,
                "basename_classification": basename_classification,
                "view_classes": view_classes,
            }
        )

    families = []
    for family in sorted(prefab_family_records, key=str.casefold):
        paths = sorted(prefab_family_records[family], key=str.casefold)
        mapped_count = sum(path in matched_prefab_paths for path in paths)
        families.append(
            {
                "name": family,
                "prefab_count": len(paths),
                "mapped_prefab_count": mapped_count,
                "unmapped_prefab_count": len(paths) - mapped_count,
            }
        )

    mapped_view_count = view_inventory["matched_view_count"]
    logical_view_count = view_inventory["logical_view_count"]
    relationship_summary = {
        "relationship_count": len(view_inventory["relationship_rows"]),
        "matched_logical_view_count": mapped_view_count,
        "unmatched_logical_view_count": logical_view_count - mapped_view_count,
        "matched_ui_prefab_path_count": len(matched_prefab_paths),
        "unmatched_ui_prefab_path_count": (
            len(ui_prefabs) - len(matched_prefab_paths)
        ),
        "strategy_counts": view_inventory["relationship_strategy_counts"],
    }

    catalog = {
        "schema": "rok.unity_full_ui_catalog.v1",
        "source_roots": {
            "ui": repo_path(UNITY_UI_ROOT),
            "ui_prefabs": repo_path(UI_PREFAB_ROOT),
            "view_mediator": repo_path(VIEW_ROOT),
            "mvc": repo_path(MVC_ROOT),
        },
        "summary": {
            "ui_prefab_count": len(ui_prefabs),
            "ui_prefab_unique_basename_count": len(
                prefab_inventory["prefab_paths_by_basename"]
            ),
            "ui_prefab_family_count": len(families),
            "all_ui_prefab_count": len(prefab_inventory["all_ui_prefabs"]),
            "physical_view_source_file_count": view_inventory[
                "physical_source_file_count"
            ],
            "logical_view_count": logical_view_count,
            "subview_count": view_inventory["view_type_counts"].get(
                "subview", 0
            ),
            "screen_side_view_count": view_inventory["view_type_counts"].get(
                "screen_side", 0
            ),
            "mediator_candidate_file_count": mediator_inventory[
                "candidate_source_file_count"
            ],
            "concrete_mediator_count": mediator_inventory[
                "concrete_mediator_count"
            ],
        },
        "prefab_families": families,
        "ui_prefabs": ui_prefabs,
        "all_ui_prefabs": prefab_inventory["all_ui_prefabs"],
        "duplicate_prefab_basenames": prefab_inventory["duplicate_groups"],
        "logical_views": view_inventory["logical_views"],
        "view_parse": {
            "physical_source_files": view_inventory[
                "physical_source_files"
            ],
            "parsed_source_file_count": view_inventory[
                "parsed_source_file_count"
            ],
            "parse_failures": view_inventory["parse_failures"],
            "framework_view_classes_excluded": view_inventory[
                "framework_view_classes"
            ],
            "view_name_declared_logical_view_count": view_inventory[
                "view_name_declared_count"
            ],
        },
        "view_prefab_relationships": view_inventory["relationship_rows"],
        "relationship_summary": relationship_summary,
        "mediators": mediator_inventory["concrete_mediators"],
        "mediator_parse": {
            "candidate_source_files": mediator_inventory[
                "candidate_source_files"
            ],
            "no_concrete_class_files": mediator_inventory[
                "no_concrete_class_files"
            ],
        },
        "classifications": {
            "all_ui_prefab_section_counts": prefab_inventory[
                "all_ui_section_counts"
            ],
            "prefab_basename_prefix_counts": dict(
                sorted(prefab_basename_classification_counts.items())
            ),
            "view_type_counts": view_inventory["view_type_counts"],
            "view_source_category_counts": view_inventory[
                "view_source_category_counts"
            ],
            "mediator_source_scope_counts": mediator_inventory[
                "source_scope_counts"
            ],
        },
    }
    return catalog


def write_markdown(catalog):
    summary = catalog["summary"]
    relationships = catalog["relationship_summary"]
    lines = [
        "# Rok Unity Full UI Catalog",
        "",
        "This is a path-preserving source inventory. It measures Unity source "
        "coverage; it is not UE runtime migration acceptance.",
        "",
        "## Baseline",
        "",
        f"- UIPrefabs: {summary['ui_prefab_count']}",
        (
            "- Unique UIPrefab basenames: "
            f"{summary['ui_prefab_unique_basename_count']}"
        ),
        f"- UIPrefab families: {summary['ui_prefab_family_count']}",
        f"- All BundleAssets/UI prefabs: {summary['all_ui_prefab_count']}",
        (
            "- Logical Views: "
            f"{summary['logical_view_count']} "
            f"({summary['screen_side_view_count']} screen-side, "
            f"{summary['subview_count']} SubView)"
        ),
        (
            "- Concrete Mediators: "
            f"{summary['concrete_mediator_count']} "
            f"from {summary['mediator_candidate_file_count']} candidate files"
        ),
        (
            "- View to prefab coverage: "
            f"{relationships['matched_logical_view_count']} logical Views, "
            f"{relationships['matched_ui_prefab_path_count']} prefab paths"
        ),
        (
            "- Unmatched inventory: "
            f"{relationships['unmatched_logical_view_count']} logical Views, "
            f"{relationships['unmatched_ui_prefab_path_count']} prefab paths"
        ),
        "",
        "## Prefab Families",
        "",
        "| Family | Prefabs | Mapped | Unmapped |",
        "| --- | ---: | ---: | ---: |",
    ]

    for family in catalog["prefab_families"]:
        lines.append(
            "| {name} | {count} | {mapped} | {unmapped} |".format(
                name=family["name"],
                count=family["prefab_count"],
                mapped=family["mapped_prefab_count"],
                unmapped=family["unmapped_prefab_count"],
            )
        )

    lines.extend(
        [
            "",
            "## Relationship Strategies",
            "",
            "| Strategy | Logical Views |",
            "| --- | ---: |",
        ]
    )
    for strategy, count in sorted(
        relationships["strategy_counts"].items()
    ):
        lines.append(f"| {strategy} | {count} |")

    lines.extend(
        [
            "",
            "## Duplicate Prefab Basenames",
            "",
        ]
    )
    for duplicate in catalog["duplicate_prefab_basenames"]:
        lines.append(f"- `{duplicate['display_basename']}`")
        for path in duplicate["paths"]:
            lines.append(f"  - `{path}`")

    lines.extend(
        [
            "",
            "## View Classification",
            "",
            "| Type | Logical Views |",
            "| --- | ---: |",
        ]
    )
    for view_type, count in sorted(
        catalog["classifications"]["view_type_counts"].items()
    ):
        lines.append(f"| {view_type} | {count} |")

    lines.extend(
        [
            "",
            "## Prefab Basename Classification",
            "",
            "| Prefix classification | Prefabs |",
            "| --- | ---: |",
        ]
    )
    for classification, count in sorted(
        catalog["classifications"][
            "prefab_basename_prefix_counts"
        ].items()
    ):
        lines.append(f"| {classification} | {count} |")

    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    catalog = build_catalog()
    OUTPUT_JSON.write_text(
        json.dumps(catalog, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    write_markdown(catalog)

    summary = catalog["summary"]
    relationships = catalog["relationship_summary"]
    print(f"Wrote {OUTPUT_JSON}")
    print(f"Wrote {OUTPUT_MD}")
    print(
        "Inventory: "
        f"UIPrefabs={summary['ui_prefab_count']} "
        f"families={summary['ui_prefab_family_count']} "
        f"all_ui_prefabs={summary['all_ui_prefab_count']} "
        f"views={summary['logical_view_count']} "
        f"mediators={summary['concrete_mediator_count']}"
    )
    print(
        "Relationships: "
        f"views={relationships['matched_logical_view_count']} "
        f"prefab_paths={relationships['matched_ui_prefab_path_count']}"
    )


if __name__ == "__main__":
    main()
