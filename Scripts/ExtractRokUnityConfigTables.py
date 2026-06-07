from __future__ import annotations

import argparse
import csv
import json
import os
import re
import zipfile
from pathlib import Path
from typing import Any
import xml.etree.ElementTree as ET


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_UNITY_ROOT = Path(os.environ.get("ROK_UNITY_ROOT", r"D:\ueDemo\Rok-unity"))
DEFAULT_OUTPUT_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "ConfigTables"

MAIN_NS = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
REL_NS = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
PKG_REL_NS = "http://schemas.openxmlformats.org/package/2006/relationships"
NS = {"m": MAIN_NS, "r": REL_NS, "pr": PKG_REL_NS}


CRITICAL_TABLES = {
    "Arms",
    "BuildingCityWall",
    "BuildingLevelData",
    "BuildingModelData",
    "BuildingResourcesProduce",
    "BuildingTownCenter",
    "BuildingTypeConfig",
    "GameSprite",
    "Hero",
    "HeroLevel",
    "HeroSkill",
    "InitBuilding",
    "Language",
    "OpenUi",
    "ResourceGatherPoint",
    "ResourceGatherRule",
    "ResourcesPlunderLoss",
    "Study",
    "StudyData",
}


def column_index(cell_ref: str) -> int:
    letters = "".join(ch for ch in cell_ref if ch.isalpha())
    value = 0
    for ch in letters:
        value = value * 26 + (ord(ch.upper()) - ord("A") + 1)
    return value - 1


def row_index(cell_ref: str) -> int:
    digits = "".join(ch for ch in cell_ref if ch.isdigit())
    return int(digits) if digits else 0


def rel(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return str(path)


def load_shared_strings(workbook: zipfile.ZipFile) -> list[str]:
    if "xl/sharedStrings.xml" not in workbook.namelist():
        return []
    root = ET.fromstring(workbook.read("xl/sharedStrings.xml"))
    values: list[str] = []
    for item in root.findall("m:si", NS):
        parts = [text.text or "" for text in item.findall(".//m:t", NS)]
        values.append("".join(parts))
    return values


def load_sheet_map(workbook: zipfile.ZipFile) -> dict[str, str]:
    book_root = ET.fromstring(workbook.read("xl/workbook.xml"))
    relationships: dict[str, str] = {}
    rel_path = "xl/_rels/workbook.xml.rels"
    if rel_path in workbook.namelist():
        rel_root = ET.fromstring(workbook.read(rel_path))
        for item in rel_root.findall("pr:Relationship", NS):
            rel_id = item.attrib.get("Id")
            target = item.attrib.get("Target", "")
            if rel_id and target:
                relationships[rel_id] = "xl/" + target.lstrip("/")

    sheets: dict[str, str] = {}
    for sheet in book_root.findall("m:sheets/m:sheet", NS):
        name = sheet.attrib.get("name", "")
        rel_id = sheet.attrib.get(f"{{{REL_NS}}}id", "")
        target = relationships.get(rel_id)
        if name and target:
            sheets[name] = target
    return sheets


def read_cell_value(cell: ET.Element, shared_strings: list[str]) -> str:
    cell_type = cell.attrib.get("t")
    if cell_type == "inlineStr":
        return "".join(text.text or "" for text in cell.findall(".//m:t", NS))
    value = cell.find("m:v", NS)
    if value is None:
        return ""
    raw = value.text or ""
    if cell_type == "s":
        try:
            return shared_strings[int(raw)]
        except (ValueError, IndexError):
            return raw
    if cell_type == "b":
        return "true" if raw == "1" else "false"
    return raw


def parse_sheet_rows(workbook: zipfile.ZipFile, sheet_path: str, shared_strings: list[str]) -> list[list[str]]:
    sheet_root = ET.fromstring(workbook.read(sheet_path))
    sparse_rows: dict[int, dict[int, str]] = {}
    max_col = 0
    for row in sheet_root.findall("m:sheetData/m:row", NS):
        for cell in row.findall("m:c", NS):
            ref = cell.attrib.get("r", "")
            if not ref:
                continue
            row_num = row_index(ref)
            col_num = column_index(ref)
            max_col = max(max_col, col_num)
            sparse_rows.setdefault(row_num, {})[col_num] = read_cell_value(cell, shared_strings)

    rows: list[list[str]] = []
    for row_num in range(1, max(sparse_rows.keys(), default=0) + 1):
        values = [sparse_rows.get(row_num, {}).get(col, "") for col in range(max_col + 1)]
        while values and values[-1] == "":
            values.pop()
        rows.append(values)
    return rows


def clean_header(value: str, fallback: str) -> str:
    value = value.strip()
    if not value:
        return fallback
    value = re.sub(r"\s+", "_", value)
    value = re.sub(r"[^0-9A-Za-z_]", "_", value)
    value = re.sub(r"_+", "_", value).strip("_")
    return value or fallback


def normalize_value(value: str, value_type: str) -> Any:
    stripped = value.strip()
    if stripped == "":
        return ""
    lower_type = value_type.lower()
    if lower_type in {"int", "int32", "int64", "uint32", "uint64", "long"}:
        try:
            return int(float(stripped))
        except ValueError:
            return stripped
    if lower_type in {"float", "double", "number"}:
        try:
            return float(stripped)
        except ValueError:
            return stripped
    if lower_type in {"bool", "boolean"}:
        return stripped.lower() in {"1", "true", "yes"}
    return stripped


def parse_table(path: Path, unity_root: Path) -> dict[str, Any]:
    table_name = path.stem[2:] if path.stem.startswith("s_") else path.stem
    with zipfile.ZipFile(path) as workbook:
        shared_strings = load_shared_strings(workbook)
        sheets = load_sheet_map(workbook)
        sheet_name = table_name if table_name in sheets else f"s_{table_name}"
        if sheet_name not in sheets:
            candidates = [name for name in sheets if not name.startswith("@")]
            if not candidates:
                raise RuntimeError(f"No data sheet found in {path}")
            sheet_name = candidates[0]
        rows = parse_sheet_rows(workbook, sheets[sheet_name], shared_strings)

    while len(rows) < 4:
        rows.append([])

    header_row = rows[0]
    type_row = rows[1] if len(rows) > 1 else []
    option_row = rows[2] if len(rows) > 2 else []
    desc_row = rows[3] if len(rows) > 3 else []
    max_cols = max(len(header_row), len(type_row), len(desc_row))
    headers = [clean_header(header_row[index] if index < len(header_row) else "", f"Column{index + 1}") for index in range(max_cols)]

    columns = []
    for index, name in enumerate(headers):
        columns.append({
            "name": name,
            "source_name": header_row[index] if index < len(header_row) else "",
            "type": type_row[index] if index < len(type_row) else "",
            "description": desc_row[index] if index < len(desc_row) else "",
            "options": option_row[index] if index < len(option_row) else "",
        })

    records = []
    for source_row_index, row in enumerate(rows[4:], start=5):
        if not any(str(value).strip() for value in row):
            continue
        record = {}
        for index, name in enumerate(headers):
            value = row[index] if index < len(row) else ""
            value_type = columns[index]["type"]
            record[name] = normalize_value(value, value_type)
        records.append(record)

    key_column = "ID" if "ID" in headers else headers[0]
    return {
        "table_name": table_name,
        "source_path": rel(path, unity_root),
        "sheet_name": sheet_name,
        "key_column": key_column,
        "column_count": len(columns),
        "row_count": len(records),
        "columns": columns,
        "rows": records,
    }


def write_table_outputs(table: dict[str, Any], output_root: Path) -> dict[str, Any]:
    output_root.mkdir(parents=True, exist_ok=True)
    table_name = table["table_name"]
    json_path = output_root / f"{table_name}.json"
    csv_path = output_root / f"{table_name}.csv"

    json_path.write_text(json.dumps(table, ensure_ascii=False, indent=2), encoding="utf-8")
    headers = [column["name"] for column in table["columns"]]
    with csv_path.open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=headers)
        writer.writeheader()
        for row in table["rows"]:
            writer.writerow({header: row.get(header, "") for header in headers})

    return {
        "table_name": table_name,
        "source_path": table["source_path"],
        "json_path": rel(json_path, PROJECT_ROOT),
        "csv_path": rel(csv_path, PROJECT_ROOT),
        "key_column": table["key_column"],
        "column_count": table["column_count"],
        "row_count": table["row_count"],
    }


def write_manifest(summaries: list[dict[str, Any]], output_root: Path, unity_root: Path) -> None:
    manifest = {
        "unity_root": str(unity_root.resolve()),
        "output_root": rel(output_root, PROJECT_ROOT),
        "table_count": len(summaries),
        "total_rows": sum(item["row_count"] for item in summaries),
        "tables": sorted(summaries, key=lambda item: item["table_name"]),
    }
    manifest_path = output_root / "config_tables_manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# Rok Unity Config Table Migration",
        "",
        f"- Unity root: `{manifest['unity_root']}`",
        f"- Output root: `{manifest['output_root']}`",
        f"- Table count: {manifest['table_count']}",
        f"- Total rows: {manifest['total_rows']}",
        "",
        "## Tables",
        "",
    ]
    for item in manifest["tables"]:
        marker = "critical" if item["table_name"] in CRITICAL_TABLES else "table"
        lines.append(f"- `{item['table_name']}` ({marker}): rows={item['row_count']} columns={item['column_count']} key=`{item['key_column']}`")
    (output_root / "config_tables_manifest.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def discover_tables(config_root: Path, selected: list[str], all_tables: bool) -> list[Path]:
    all_paths = sorted(config_root.glob("s_*.xlsm"))
    if all_tables or not selected:
        return all_paths
    wanted = {name[2:] if name.startswith("s_") else name for name in selected}
    paths = []
    for path in all_paths:
        table_name = path.stem[2:] if path.stem.startswith("s_") else path.stem
        if table_name in wanted:
            paths.append(path)
    missing = sorted(wanted - {path.stem[2:] if path.stem.startswith("s_") else path.stem for path in paths})
    if missing:
        raise RuntimeError("Missing selected config tables: " + ", ".join(missing))
    return paths


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract Unity Rok xlsm config tables into UE-friendly JSON/CSV.")
    parser.add_argument("--unity-root", default=str(DEFAULT_UNITY_ROOT), help="Root containing config-tables.")
    parser.add_argument("--output-root", default=str(DEFAULT_OUTPUT_ROOT), help="Generated output directory.")
    parser.add_argument("--tables", nargs="*", default=[], help="Specific tables without the s_ prefix. Empty means all tables.")
    parser.add_argument("--all", action="store_true", help="Extract all s_*.xlsm tables. This is the default when --tables is omitted.")
    args = parser.parse_args()

    unity_root = Path(args.unity_root)
    config_root = unity_root / "config-tables"
    if not config_root.exists():
        raise SystemExit(f"Missing config-tables root: {config_root}")

    output_root = Path(args.output_root)
    output_root.mkdir(parents=True, exist_ok=True)

    table_paths = discover_tables(config_root, args.tables, args.all or not args.tables)
    summaries = []
    failures = []
    for path in table_paths:
        try:
            table = parse_table(path, unity_root)
            summaries.append(write_table_outputs(table, output_root))
        except Exception as exc:
            failures.append(f"{path.name}: {exc}")

    write_manifest(summaries, output_root, unity_root)
    print(f"Extracted config tables: {len(summaries)}")
    print(f"Total rows: {sum(item['row_count'] for item in summaries)}")
    print(f"Wrote {output_root / 'config_tables_manifest.json'}")
    if failures:
        for failure in failures:
            print(f"ERROR: {failure}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
