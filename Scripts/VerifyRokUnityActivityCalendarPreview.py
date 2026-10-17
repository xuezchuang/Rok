#!/usr/bin/env python3
"""Independently verify the static ActivityCalendar catalog against Unity sources."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import struct
import sys
from pathlib import Path
from rok_data_paths import resolve


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DIR = ROOT / "Saved/RokUnityMigration/ActivityCalendarPreviewV1"
SPRITE_REF = re.compile(r"^([A-Za-z0-9_]+)\[([A-Za-z0-9_]+)\]$")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rel(path: Path) -> str:
    return path.resolve().relative_to(ROOT).as_posix()


def png_size(path: Path) -> list[int]:
    data = path.read_bytes()
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise ValueError(f"Not a valid PNG/IHDR: {path}")
    width, height = struct.unpack(">II", data[16:24])
    return [width, height]


def load_csv(path: Path) -> tuple[list[str], list[str], list[dict[str, str]]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.reader(stream)
        header = next(reader)
        type_row = next(reader)
        rows = []
        for source_line, values in enumerate(reader, start=3):
            if not values or all(not field for field in values):
                continue
            if len(values) != len(header):
                raise ValueError(f"Source CSV line {source_line} has {len(values)} fields; expected {len(header)}")
            rows.append({key: value for key, value in zip(header, values)})
    return header, type_row, rows


def load_sheet(path: Path) -> tuple[list[int], dict[str, tuple[list[int], int]]]:
    size = None
    sprites: dict[str, tuple[list[int], int]] = {}
    for line_no, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), start=1):
        size_match = re.match(r"^:size=(\d+)x(\d+)$", line.strip())
        if size_match:
            size = [int(size_match.group(1)), int(size_match.group(2))]
        sprite_match = re.match(r"^([^;#][^;]*);(\d+);(\d+);(\d+);(\d+);", line)
        if sprite_match:
            key, x, y, width, height = sprite_match.groups()
            sprites[key] = ([int(x), int(y), int(width), int(height)], line_no)
    if size is None:
        raise ValueError(f"No :size metadata in {path}")
    return size, sprites


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dir", type=Path, default=DEFAULT_DIR)
    args = parser.parse_args()
    out_dir = args.dir.resolve()
    if out_dir != DEFAULT_DIR.resolve():
        raise SystemExit("--dir must be the owned ActivityCalendarPreviewV1 directory")

    catalog_path = out_dir / "activity_calendar_preview.json"
    prepare_path = out_dir / "activity_calendar_preview_prepare.json"
    verify_path = out_dir / "activity_calendar_preview_verify.json"
    if verify_path.exists():
        raise FileExistsError(f"Refusing to overwrite verifier report: {verify_path}")
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    prepare = json.loads(prepare_path.read_text(encoding="utf-8"))
    errors: list[str] = []
    checks: dict[str, object] = {}

    if catalog.get("schema") != "rok.unity_activity_calendar_preview.v1":
        errors.append("catalog schema mismatch")
    if prepare.get("runtime_accepted") is not False or prepare.get("status") != "prepared_static_catalog":
        errors.append("prepare report must remain static-only and unaccepted")
    if prepare.get("catalog", {}).get("sha256") != sha256(catalog_path):
        errors.append("prepare report catalog hash does not match catalog bytes")

    # Verify every pinned source input against current disk bytes.
    source_records = catalog.get("source_files", [])
    seen_source_paths: set[str] = set()
    for source in source_records:
        source_path = source.get("path", "")
        if source_path in seen_source_paths:
            errors.append(f"duplicate pinned source path: {source_path}")
            continue
        seen_source_paths.add(source_path)
        path = resolve(source_path)
        if not path.is_file():
            errors.append(f"pinned source missing: {source_path}")
        elif sha256(path) != source.get("sha256"):
            errors.append(f"pinned source hash changed: {source_path}")

    build_path = ROOT / "Scripts/BuildRokUnityActivityCalendarPreview.py"
    if not build_path.is_file() or sha256(build_path) != catalog.get("generated_by", {}).get("sha256"):
        errors.append("catalog builder hash does not match the pinned generator")
    if sha256(build_path) != prepare.get("generator", {}).get("sha256"):
        errors.append("prepare report generator hash mismatch")
    verify_script = ROOT / "Scripts/VerifyRokUnityActivityCalendarPreview.py"

    source_table = catalog.get("source_table", {})
    csv_path = resolve(source_table.get("path", ""))
    header, type_row, source_rows = load_csv(csv_path)
    catalog_rows = catalog.get("rows", [])
    source_ids = [row.get("ID", "") for row in source_rows]
    catalog_ids = [row.get("unity_id") for row in catalog_rows]
    if len(source_ids) != len(set(source_ids)):
        errors.append("source Unity IDs are not unique")
    if source_ids != catalog_ids:
        errors.append("catalog Unity ID order/values differ from source")
    if len(source_rows) != source_table.get("row_count") or len(source_rows) != prepare.get("source_config_rows"):
        errors.append("source/catalog/prepare row counts differ")
    if source_table.get("stable_unity_ids") != source_ids:
        errors.append("stable ID list differs from current source order")
    expected_field_schema = [{"name": name, "unity_type": type_row[index]} for index, name in enumerate(header)]
    if source_table.get("fields") != expected_field_schema:
        errors.append("serialized CSV header/type schema differs from source")

    sheet_cache: dict[str, tuple[list[int], dict[str, tuple[list[int], int]]]] = {}
    png_cache: dict[str, list[int]] = {}
    source_mismatches = []
    resolved_refs = 0
    unresolved_refs: list[dict[str, str]] = []
    for index, (source_row, catalog_row) in enumerate(zip(source_rows, catalog_rows), start=3):
        if catalog_row.get("source_line") != index:
            errors.append(f"row {source_row.get('ID')}: source line mismatch")
        if catalog_row.get("fields") != source_row:
            source_mismatches.append(source_row.get("ID"))
        page_class = {"1": "hot", "2": "daily"}.get(source_row.get("page", ""), "other_or_not_menu")
        if catalog_row.get("ui_config_hints", {}).get("page_group_from_source") != page_class:
            errors.append(f"row {source_row.get('ID')}: page classification mismatch")
        activity_type = source_row.get("activityType", "")
        expected_variant = "hell" if activity_type == "800" else "ordinary_activity"
        if catalog_row.get("ui_config_hints", {}).get("item_variant_if_runtime_row_exists") != expected_variant:
            errors.append(f"row {source_row.get('ID')}: activity item variant mismatch")

        for field in ("icon", "background"):
            value = source_row.get(field, "")
            ref = catalog_row.get("visual_refs", {}).get(field, {})
            if ref.get("source_value") != value:
                errors.append(f"row {source_row.get('ID')}: {field} source value mismatch")
            if not value:
                if ref.get("status") != "empty":
                    errors.append(f"row {source_row.get('ID')}: empty {field} not represented as empty")
                continue
            match = SPRITE_REF.fullmatch(value)
            if not match:
                errors.append(f"row {source_row.get('ID')}: unparseable {field} reference {value!r}")
                continue
            asset, sprite = match.groups()
            sheet_path = ref.get("tpsheet_path")
            if not sheet_path or Path(sheet_path).stem != asset:
                errors.append(f"row {source_row.get('ID')}: {field} sheet path does not match asset key")
                continue
            try:
                atlas_size, sprites = sheet_cache.setdefault(sheet_path, load_sheet(resolve(sheet_path)))
            except Exception as exc:
                errors.append(f"cannot parse {sheet_path}: {exc}")
                continue
            found = sprites.get(sprite)
            if found is None:
                if ref.get("status") != "missing" or ref.get("sprite_metadata") is not None:
                    errors.append(f"row {source_row.get('ID')}: unresolved sprite status is inconsistent for {sprite}")
                unresolved_refs.append({"unity_id": source_row.get("ID", ""), "field": field, "source_value": value, "sheet_path": sheet_path})
                continue
            rect, sheet_line = found
            x, y, width, height = rect
            if x + width > atlas_size[0] or y + height > atlas_size[1]:
                errors.append(f"row {source_row.get('ID')}: sprite {sprite} exceeds atlas bounds")
            if ref.get("sprite_metadata", {}).get("rect_xywh") != rect:
                errors.append(f"row {source_row.get('ID')}: catalog sprite rectangle differs from source sheet")
            if ref.get("sprite_metadata", {}).get("source_line") != sheet_line:
                errors.append(f"row {source_row.get('ID')}: catalog sprite source line differs")
            if ref.get("atlas_size") != atlas_size:
                errors.append(f"row {source_row.get('ID')}: catalog atlas dimensions differ")
            png_path = ROOT / sheet_path.replace(".tpsheet", ".png")
            if not png_path.is_file():
                errors.append(f"atlas PNG missing for {sheet_path}")
            else:
                actual_png_size = png_cache.setdefault(rel(png_path), png_size(png_path))
                if actual_png_size != atlas_size:
                    errors.append(f"sheet/PNG dimensions differ for {sheet_path}: {atlas_size} vs {actual_png_size}")
            if ref.get("status") != "resolved":
                errors.append(f"row {source_row.get('ID')}: {field} reference not marked resolved")
            resolved_refs += 1
    if source_mismatches:
        errors.append(f"raw field copies differ from CSV for IDs: {source_mismatches[:10]}")

    loc_values = []
    for row in catalog_rows:
        loc = row.get("localization", {})
        source_id = row.get("fields", {}).get("l_nameID", "")
        if loc.get("name_string_id") != source_id or loc.get("resolved_text") is not None:
            errors.append(f"row {row.get('unity_id')}: localization must preserve ID and remain unresolved")
        if source_id and not source_id.isdigit():
            errors.append(f"row {row.get('unity_id')}: l_nameID is not numeric")
        loc_values.append(source_id)
    if catalog.get("asset_resolution", {}).get("localization_text_resolved") is not False:
        errors.append("catalog incorrectly claims localization text was resolved")
    catalog_unresolved = catalog.get("asset_resolution", {}).get("missing_references", [])
    expected_unresolved = [
        {"id": item["unity_id"], "field": item["field"], "value": item["source_value"], "reason": "sprite key absent from pinned sheet"}
        for item in unresolved_refs
    ]
    if catalog_unresolved != expected_unresolved:
        errors.append("unresolved reference list does not exactly match source sheet lookup results")

    unknown = catalog.get("unknown_or_blocked_runtime_fields", [])
    required_unknown_markers = ("server time", "ActivityTimeInfo", "red dots", "localized string text")
    for marker in required_unknown_markers:
        if not any(marker.lower() in item.lower() for item in unknown):
            errors.append(f"runtime unknown list is missing required marker: {marker}")
    if any(key in catalog for key in ("schedule_instances", "current_server_time", "current_activity_rows")):
        errors.append("catalog must not fabricate runtime schedule instances")
    if catalog.get("schedule_instances_generated", 0) != 0:
        errors.append("catalog prepare must not generate live schedule instances")

    prefab_text = (ROOT / "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_activity/UIEventDate/UI_Win_EventDate.prefab").read_text(encoding="utf-8-sig")
    for animation in catalog.get("rendering_contract", {}).get("known_layout", {}).get("animation", []):
        guid = animation.get("controller_guid", "")
        controller_path = animation.get("controller_path", "")
        meta_path = ROOT / (controller_path + ".meta")
        if f"guid: {guid}" not in prefab_text:
            errors.append(f"prefab does not reference declared Animator GUID {guid}")
        if not meta_path.is_file() or f"guid: {guid}" not in meta_path.read_text(encoding="utf-8-sig"):
            errors.append(f"Animator controller meta GUID mismatch: {controller_path}")
        if not (resolve(controller_path)).is_file():
            errors.append(f"Animator controller asset missing: {controller_path}")

    checks = {
        "pinned_source_hashes": len(source_records),
        "stable_config_rows": len(source_rows),
        "unique_unity_ids": len(set(source_ids)),
        "raw_source_fields_checked": len(source_rows) * len(header),
        "resolved_icon_and_background_references": resolved_refs,
        "unresolved_but_preserved_source_references": unresolved_refs,
        "parsed_png_dimensions": png_cache,
        "numeric_localization_ids_preserved": len(loc_values),
        "localization_text_claimed_resolved": False,
        "runtime_schedule_instances": 0,
        "runtime_state_fabricated": False,
        "verifier_script_sha256": sha256(verify_script),
    }
    report = {
        "schema": "rok.unity_activity_calendar_preview_verify.v1",
        "passed": not errors,
        "runtime_accepted": False,
        "catalog": {"path": "Saved/RokUnityMigration/ActivityCalendarPreviewV1/activity_calendar_preview.json", "sha256": sha256(catalog_path)},
        "prepare_report": {"path": "Saved/RokUnityMigration/ActivityCalendarPreviewV1/activity_calendar_preview_prepare.json", "sha256": sha256(prepare_path)},
        "checks": checks,
        "failures": errors,
    }
    verify_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": not errors, "rows": len(source_rows), "resolved_refs": resolved_refs, "failures": errors, "report": verify_path.relative_to(ROOT).as_posix()}, ensure_ascii=False))
    return 0 if not errors else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"activity calendar catalog verification failed: {exc}", file=sys.stderr)
        raise
