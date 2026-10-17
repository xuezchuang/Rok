#!/usr/bin/env python3
"""Build a read-only, source-traceable ActivityCalendar configuration catalog."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT / "Saved/RokUnityMigration/ActivityCalendarPreviewV1"
CSV_PATH = "ref/config-tables/编译客户端配置/CSV/ActivityCalendar.csv"
ACTIVITY_ASSET_DIR = "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_activity"
SOURCES = [
    (CSV_PATH, "Unity client activity calendar config", [1, 2]),
    ("ref/client-unity/Assets/Scripts/Hotfix/Config/ActivityCalendarConfig.cs", "Unity config row schema", [8, 18, 28, 33, 38, 43, 48, 53]),
    ("ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/ActivityProxy.cs", "Runtime activity filtering, server-time schedule and week calculations", [571, 585, 667, 773, 775, 845, 872]),
    ("ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/PlayerProxy.cs", "Runtime ActivityTimeInfo provider", [1166, 1175]),
    ("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Activity/EventDateMediator.cs", "Menu classification, date list, selection and date text", [22, 30, 77, 116, 217, 305, 310, 430, 475, 519, 1048, 1144]),
    ("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Activity/EventDateView.cs", "View hierarchy bindings and date/event layouts", [31, 49, 55, 101, 125]),
    ("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_activity/UIEventDate/UI_Win_EventDate.prefab", "Unity event-date layout and Animator controller references", [669, 971, 1390, 1662, 1672, 1692, 1719, 1729]),
    ("ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1.controller", "Prefab Animator controller", []),
    ("ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1.controller.meta", "Animator controller GUID provenance", []),
    ("ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_TextTipAnime.controller", "Prefab Animator controller", []),
    ("ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_TextTipAnime.controller.meta", "Animator controller GUID provenance", []),
    (f"{ACTIVITY_ASSET_DIR}/activity_icon.tpsheet", "Activity icon atlas sprite metadata", []),
    (f"{ACTIVITY_ASSET_DIR}/activity_icon.png", "Activity icon atlas texture", []),
    (f"{ACTIVITY_ASSET_DIR}/activity_iconag.tpsheet", "Arabic activity icon atlas sprite metadata", []),
    (f"{ACTIVITY_ASSET_DIR}/activity_img.tpsheet", "Activity image atlas sprite metadata", []),
    (f"{ACTIVITY_ASSET_DIR}/activity_img.png", "Activity image atlas texture", []),
    (f"{ACTIVITY_ASSET_DIR}/activity_img1.tpsheet", "Activity image atlas variant sprite metadata", []),
    (f"{ACTIVITY_ASSET_DIR}/activity_img1.png", "Activity image atlas variant texture", []),
    ("ref/config-tables/编译客户端配置/CSV/LanguageSet.csv", "Supported locale IDs only; not localized string values", [1, 2]),
]
SPRITE_REF = re.compile(r"^([A-Za-z0-9_]+)\[([A-Za-z0-9_]+)\]$")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rel(path: Path) -> str:
    return path.resolve().relative_to(ROOT).as_posix()


def read_activity_rows(path: Path) -> tuple[list[str], list[str], list[dict[str, str]]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.reader(stream)
        header = next(reader)
        type_row = next(reader)
        records: list[dict[str, str]] = []
        for line_no, values in enumerate(reader, start=3):
            if not values or all(not value for value in values):
                continue
            if len(values) != len(header):
                raise ValueError(f"CSV line {line_no}: {len(values)} values, expected {len(header)}")
            records.append({key: value for key, value in zip(header, values)})
    return header, type_row, records


def parse_sheet(path: Path) -> dict[str, dict[str, object]]:
    sprites: dict[str, dict[str, object]] = {}
    for line_no, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), start=1):
        match = re.match(r"^([^;#][^;]*);(\d+);(\d+);(\d+);(\d+);", line)
        if match:
            key, x, y, width, height = match.groups()
            sprites[key] = {"source_line": line_no, "rect_xywh": [int(x), int(y), int(width), int(height)]}
    return sprites


def parse_sheet_size(path: Path) -> list[int] | None:
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        match = re.match(r"^:size=(\d+)x(\d+)$", line.strip())
        if match:
            return [int(match.group(1)), int(match.group(2))]
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()
    out_dir = args.out_dir.resolve()
    if out_dir != DEFAULT_OUT.resolve():
        raise SystemExit("--out-dir must be the owned ActivityCalendarPreviewV1 directory")
    out_dir.mkdir(parents=True, exist_ok=True)

    sources = []
    for source_path, role, lines in SOURCES:
        path = ROOT / source_path
        if not path.is_file():
            raise FileNotFoundError(f"Required source missing: {source_path}")
        sources.append({"path": source_path, "sha256": sha256(path), "role": role, "line_refs": lines})
    source_by_path = {row["path"]: row for row in sources}

    csv_path = ROOT / CSV_PATH
    header, type_row, raw_rows = read_activity_rows(csv_path)
    ids = [row.get("ID", "") for row in raw_rows]
    if any(not item for item in ids) or len(ids) != len(set(ids)):
        raise ValueError("ActivityCalendar IDs must be nonempty and unique")

    sheets: dict[str, dict[str, dict[str, object]]] = {}
    sheet_sizes: dict[str, list[int] | None] = {}
    for source_path, _, _ in SOURCES:
        if source_path.endswith(".tpsheet"):
            sheet_key = Path(source_path).stem
            sheets[sheet_key] = parse_sheet(ROOT / source_path)
            sheet_sizes[sheet_key] = parse_sheet_size(ROOT / source_path)

    rows = []
    missing_sprite_refs: list[dict[str, object]] = []
    for source_line, raw in enumerate(raw_rows, start=3):
        ui_page = {"1": "hot", "2": "daily"}.get(raw.get("page", ""), "other_or_not_menu")
        activity_type = raw.get("activityType", "")
        refs = {}
        for field in ("icon", "background"):
            value = raw.get(field, "")
            match = SPRITE_REF.fullmatch(value)
            if not value:
                refs[field] = {"source_value": value, "status": "empty"}
                continue
            if not match:
                refs[field] = {"source_value": value, "status": "unparsed"}
                missing_sprite_refs.append({"id": raw["ID"], "field": field, "value": value, "reason": "not asset[sprite] syntax"})
                continue
            asset, sprite = match.groups()
            atlas = sheets.get(asset)
            metadata = atlas.get(sprite) if atlas is not None else None
            status = "resolved" if metadata is not None else "missing"
            refs[field] = {
                "source_value": value,
                "asset_key": asset,
                "sprite_key": sprite,
                "tpsheet_path": next((s["path"] for s in sources if Path(s["path"]).stem == asset and s["path"].endswith(".tpsheet")), None),
                "atlas_size": sheet_sizes.get(asset),
                "sprite_metadata": metadata,
                "status": status,
            }
            if metadata is None:
                missing_sprite_refs.append({"id": raw["ID"], "field": field, "value": value, "reason": "sprite key absent from pinned sheet"})
        rows.append({
            "unity_id": raw["ID"],
            "source_line": source_line,
            "fields": raw,
            "ui_config_hints": {
                "page_group_from_source": ui_page,
                "item_variant_if_runtime_row_exists": "hell" if activity_type == "800" else "ordinary_activity",
                "runtime_row_requires_server_activity_time_info": True,
                "config_order_is_preserved_not_sorted_by_this_builder": raw.get("order", ""),
            },
            "visual_refs": refs,
            "localization": {
                "name_string_id": raw.get("l_nameID", ""),
                "resolved_text": None,
                "status": "id_present_text_unavailable_in_snapshot",
            },
        })

    # Account activity instances are intentionally absent: these depend on server ActivityTimeInfo.
    catalog = {
        "schema": "rok.unity_activity_calendar_preview.v1",
        "purpose": "Static read-only Unity config catalog; not account schedule, participation, activity progress, or server status.",
        "source_files": sources,
        "source_table": {
            "path": CSV_PATH,
            "sha256": source_by_path[CSV_PATH]["sha256"],
            "header_source_line": 1,
            "type_source_line": 2,
            "fields": [{"name": name, "unity_type": type_row[index]} for index, name in enumerate(header)],
            "row_count": len(rows),
            "stable_unity_ids": ids,
        },
        "rendering_contract": {
            "menu_categories": {"page_1": "hot", "page_2": "daily", "other": "not added by EventDateMediator menu builder"},
            "menu_order": "Runtime menu is built from PlayerProxy ActivityTimeInfo dictionary; EventDateMediator separates page 1 and page 2 while preserving that input order. ActivityCalendar.order is retained as source data but these methods do not sort by it.",
            "special_variant": "activityType 800 selects the Hell item prefab; other config-backed rows use the ordinary activity item prefab.",
            "calendar_week": "Seven server-time days beginning Sunday (DayOfWeek=0); schedule bars are built from server ActivityTimeInfo start/end, clipped to the current server week.",
            "visibility": "GetActivityMenuList and GetActivityDateList require runtime player ActivityTimeInfo plus server time; static ActivityCalendar rows alone cannot determine current visible menu items or date bars.",
            "known_layout": {
                "prefab": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_activity/UIEventDate/UI_Win_EventDate.prefab",
                "view_script": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Activity/EventDateView.cs",
                "nodes": ["rect/listbg/sv_list", "rect/pl_date/date/UI_DateItem0..UI_DateItem6", "rect/pl_date/sv_content", "rect/pl_event", "pl_pos/pl_offset"],
                "animation": [
                    {"node":"pl_pos", "component":"Animator", "controller_guid":"79ffb0d19ef4cd34193888a1a1bb63e4", "controller_path":"ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1.controller"},
                    {"node":"pl_offset", "component":"Animator", "controller_guid":"531c207699e82e74cb64467a2486e942", "controller_path":"ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_TextTipAnime.controller"}
                ],
                "animation_trigger_semantics": "Animator components/controllers are statically referenced; this catalog does not claim a particular runtime state/clip was played."
            }
        },
        "unknown_or_blocked_runtime_fields": [
            "current server time / timezone conversion and current highlighted date",
            "per-account ActivityTimeInfo instances, actual start/end times, current menu visibility and selected event",
            "activity schedule/progress, red dots, participation, ranking rows, reward eligibility or reward claims",
            "localized string text for l_nameID/l_sketchID/l_taglineID/l_ruleID (the extracted LanguageSet table lists locale settings but is not a string dictionary)",
            "localized count/date values beyond the source format key IDs"
        ],
        "rows": rows,
        "asset_resolution": {
            "parsed_references": sum(1 for row in rows for ref in row["visual_refs"].values() if ref.get("status") == "resolved"),
            "missing_references": missing_sprite_refs,
            "localization_text_resolved": False,
            "localization_note": "l_nameID remains a stable integer key; the inspected extracted config includes locale IDs but not localized string values."
        },
        "generated_by": {"path": "Scripts/BuildRokUnityActivityCalendarPreview.py", "sha256": sha256(Path(__file__))}
    }

    catalog_path = out_dir / "activity_calendar_preview.json"
    if catalog_path.exists():
        raise FileExistsError(f"Refusing to overwrite generated catalog: {rel(catalog_path)}")
    catalog_bytes = (json.dumps(catalog, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    catalog_path.write_bytes(catalog_bytes)
    report = {
        "schema": "rok.activity_calendar_preview_prepare.v1",
        "status": "prepared_static_catalog",
        "runtime_accepted": False,
        "source_config_rows": len(rows),
        "unique_unity_ids": len(set(ids)),
        "parsed_icon_and_background_refs": catalog["asset_resolution"]["parsed_references"],
        "missing_sprite_refs": missing_sprite_refs,
        "localization_text_resolved": False,
        "schedule_instances_generated": 0,
        "server_fields_preserved_as_unknown": True,
        "catalog": {"path": rel(catalog_path), "sha256": hashlib.sha256(catalog_bytes).hexdigest()},
        "generator": {"path": "Scripts/BuildRokUnityActivityCalendarPreview.py", "sha256": sha256(Path(__file__))},
        "verifier_expected": "Scripts/VerifyRokUnityActivityCalendarPreview.py"
    }
    report_path = out_dir / "activity_calendar_preview_prepare.json"
    if report_path.exists():
        raise FileExistsError(f"Refusing to overwrite prepare report: {rel(report_path)}")
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"catalog": rel(catalog_path), "rows": len(rows), "refs": report["parsed_icon_and_background_refs"], "missing_refs": len(missing_sprite_refs), "prepare": rel(report_path)}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # report a concise source/data error
        print(f"activity calendar catalog build failed: {exc}", file=sys.stderr)
        raise
