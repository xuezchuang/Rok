from __future__ import annotations

import argparse
import json
from pathlib import Path
from rok_data_paths import DATA_ROOT, resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = DATA_ROOT / "ConfigTables" / "config_tables_manifest.json"

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


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify extracted Rok Unity config table JSON/CSV outputs.")
    parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST), help="Path to config_tables_manifest.json.")
    args = parser.parse_args()

    manifest_path = Path(args.manifest)
    if not manifest_path.exists():
        raise SystemExit(f"Missing config table manifest: {manifest_path}")

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    tables = manifest.get("tables", [])
    names = {item.get("table_name") for item in tables}
    failures: list[str] = []

    if manifest.get("table_count", 0) < 100:
        failures.append(f"expected at least 100 tables, found {manifest.get('table_count', 0)}")
    if manifest.get("total_rows", 0) < 1000:
        failures.append(f"expected at least 1000 data rows, found {manifest.get('total_rows', 0)}")

    missing_critical = sorted(CRITICAL_TABLES - names)
    if missing_critical:
        failures.append("missing critical tables: " + ", ".join(missing_critical))

    for item in tables:
        table_name = item.get("table_name", "<unknown>")
        json_path = resolve(item.get("json_path", ""))
        csv_path = resolve(item.get("csv_path", ""))
        if not json_path.exists():
            failures.append(f"missing JSON for {table_name}: {json_path}")
        if not csv_path.exists():
            failures.append(f"missing CSV for {table_name}: {csv_path}")
        if item.get("column_count", 0) <= 0:
            failures.append(f"table has no columns: {table_name}")
        if table_name in CRITICAL_TABLES and item.get("row_count", 0) <= 0:
            failures.append(f"critical table has no rows: {table_name}")

    print("RokUnityConfigTablesVerify")
    print(f"  tables={manifest.get('table_count', 0)}")
    print(f"  total_rows={manifest.get('total_rows', 0)}")
    for name in sorted(CRITICAL_TABLES):
        item = next((table for table in tables if table.get("table_name") == name), None)
        if item:
            print(f"  critical.{name}=rows:{item.get('row_count', 0)} columns:{item.get('column_count', 0)}")

    if failures:
        for failure in failures:
            print(f"ERROR: {failure}")
        return 1
    print("Rok Unity config table verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
