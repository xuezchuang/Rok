"""Independently verify exact model keys and Academy first-build source chain."""
import csv
import hashlib
import json
from collections import Counter

from BuildRokCityConstructionSources import ROOT, OUT, TABLES, GAMEPLAY, PROXY, build
from rok_data_paths import label, resolve

REPORT = ROOT / "Saved/RokUnityMigration/CityConstructionSources/verify.json"


def rows(name):
    with TABLES[name].open(encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))[1:]


def verify():
    package = json.loads(OUT.read_text(encoding="utf-8"))
    assert package == build(write=False), "catalog must reproduce from unchanged source"
    assert package["schema"] == "rok.city_construction_sources.v1"
    raw_models = rows("BuildingModelData")
    models = package["models"]
    assert len(raw_models) == len(models) == 870
    assert len({r["id"] for r in models}) == len({(r["type"], r["civilization"], r["age"]) for r in models}) == 870
    assert Counter(r["type"] for r in models)[99] == 30
    building_type_ids = {int(source["type"]) for source in rows("BuildingTypeConfig")}
    assert package["building_type_ids"] == sorted(building_type_ids)
    assert package["summary"]["model_only_nonbuildable_types"] == [99]
    assert package["summary"]["building_types_without_model"] == [54, 55]
    for source, row in zip(raw_models, models):
        assert row["id"] == int(source["ID"]) == int(source["type"] + source["civType"] + source["age"])
        assert (row["type"], row["civilization"], row["age"]) == tuple(int(source[key]) for key in ("type", "civType", "age"))
        assert row["model_id"] == source["modelId"] and row["image_resource"] == source["imgRes"]
        assert row["missing_visual_sources"] == [name for name in ("scene_prefab", "ui_prefab") if row[name] is None]
        assert row["has_building_type_config"] == (row["type"] in building_type_ids)
        for key in ("scene_prefab", "ui_prefab"):
            if row[key]:
                asset = resolve(row[key])
                assert asset.is_file() and asset.stem == (row["model_id"] if key == "scene_prefab" else row["image_resource"])
                assert package["source_assets"][row[key]] == hashlib.sha256(asset.read_bytes()).hexdigest()
    expected_inputs = {label(path): hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in (*TABLES.values(), GAMEPLAY, PROXY)}
    assert package["sources"] == expected_inputs
    gameplay = json.loads(GAMEPLAY.read_text(encoding="utf-8"))
    academy = next(r for r in gameplay["building_types"] if r["type_id"] == 13)
    first = academy["level_summary"]["level1"]
    assert (first["id"], first["build_seconds"], first["power"]) == (1301, 6, 5)
    assert first["cost"] == {"food": 500, "wood": 800, "stone": 0, "gold": 0, "denar": 0}
    assert first["requirements"] == [{"type": 1, "level": 4}, {"type": 0, "level": 0}, {"type": 0, "level": 0}]
    assert academy["size"] == {"width": 6, "length": 6}
    assert next(x for x in academy["count_limits"] if x["town_level"] == 4)["limit"] == 1
    for age in (1, 2):
        academy_model = next(r for r in models if (r["type"], r["civilization"], r["age"]) == (13, 101, age))
        assert academy_model["model_id"] == "build_13_101_2" and not academy_model["missing_visual_sources"]
    result = {"schema": "rok.city_construction_sources.verify.v1", "status": "passed",
              "catalog_sha256": hashlib.sha256(OUT.read_bytes()).hexdigest(),
              "rows_checked": 870, "exact_keys_checked": 870, "source_file_count": len(expected_inputs),
              "visual_asset_count": len(package["source_assets"]),
              "missing_scene_prefab_rows": sum(r["scene_prefab"] is None for r in models),
              "missing_ui_prefab_rows": sum(r["ui_prefab"] is None for r in models),
              "academy_level1_checked": True, "model_only_type_99_not_buildable": True}
    REPORT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"verified {len(models)} exact model rows; {len(package['source_assets'])} source prefabs hashed")
    return result


if __name__ == "__main__":
    verify()
