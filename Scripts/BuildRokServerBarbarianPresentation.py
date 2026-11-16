"""Build source-traceable close formations for every ordinary Monster model.

Supplement to BuildRokUnityWorldEntityPresentation.py; does not alter its
sampled world-population manifest or assets.
"""

import csv
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import BuildRokUnityWorldEntityPresentation as source  # noqa: E402
from rok_data_paths import DATA_ROOT, resolve  # noqa: E402

OUTPUT_DIR = DATA_ROOT / "ServerBarbarianPresentation"
OUTPUT_PATH = DATA_ROOT / "server_barbarian_presentation.json"
SCHEMA = "rok.server_barbarian_presentation.v1"


def monster_rows():
    path = resolve(source.MONSTER_TABLE)
    with path.open(encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.reader(handle))
    headers = {name: index for index, name in enumerate(rows[0])}
    result = []
    for row in rows[2:]:
        if row[headers["type"]] != "1":
            continue
        model = row[headers["modelId"]]
        if not re.fullmatch(r"BarbarianFormation_\d+", model):
            result.append({"monster_id": int(row[headers["ID"]]), "level": int(row[headers["level"]]), "model_id": model, "status": "unsupported", "reason": "model is not an ordinary BarbarianFormation prefab"})
            continue
        result.append({"monster_id": int(row[headers["ID"]]), "level": int(row[headers["level"]]), "model_id": model, "status": "pending"})
    return sorted(result, key=lambda row: row["monster_id"])


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    source.OUTPUT_DIR = OUTPUT_DIR
    monsters = monster_rows()
    formations = {}
    required_units = set()
    for model in sorted({row["model_id"] for row in monsters if row["status"] == "pending"}, key=lambda value: int(value.rsplit("_", 1)[1])):
        try:
            formation = source.parse_formation(model)
            if not formation["unit_count"]:
                raise RuntimeError("formation has no serialized units")
            formations[model] = formation
            required_units.update(unit["unit_prefab"] for row in formation["rows"] for unit in row["units"])
        except (OSError, ValueError, RuntimeError) as exc:
            for row in monsters:
                if row["model_id"] == model:
                    row.update(status="unsupported", reason=str(exc))

    guid_index = source.build_guid_index()
    unit_library = {}
    unit_errors = {}
    for name in sorted(required_units):
        tier = re.fullmatch(r"Arms([1-4])_\d+", name)
        if tier:
            source.TROOP_UNIT_DIR = f"ref/client-unity/Assets/BundleAssets/Troop/troop_unit_t{tier.group(1)}"
        elif not re.fullmatch(r"Monster_\d+", name):
            unit_errors[name] = "unit name has no known source directory"
            continue
        try:
            unit = source.parse_unit_prefab(name, guid_index)
            unit["frame_crops"] = source.crop_idle_frames(unit)
            unit_library[name] = unit
        except (OSError, ValueError, RuntimeError, KeyError) as exc:
            unit_errors[name] = str(exc)

    for model, formation in list(formations.items()):
        missing = sorted({unit["unit_prefab"] for row in formation["rows"] for unit in row["units"]} & unit_errors.keys())
        if missing:
            del formations[model]
            for row in monsters:
                if row["model_id"] == model:
                    row.update(status="unsupported", reason="missing actual unit assets: " + ", ".join(missing))
        else:
            for row in monsters:
                if row["model_id"] == model:
                    row["status"] = "supported"

    sampled = json.loads(source.OUTPUT_PATH.read_text(encoding="utf-8"))
    manifest = {
        "schema": SCHEMA,
        "monster_table": source.source_record(resolve(source.MONSTER_TABLE)),
        "source_presentation": source.source_record(source.OUTPUT_PATH),
        "source_scale_curve": sampled["unit_scale_curve"],
        "monsters": monsters,
        "formations": formations,
        "unit_library": unit_library,
        "unit_errors": unit_errors,
    }
    OUTPUT_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"server barbarian presentation: {len(formations)} models, {len(unit_library)} unit types, {sum(row['status'] == 'supported' for row in monsters)}/{len(monsters)} rows supported")


if __name__ == "__main__":
    main()
