"""Build a typed, read-only BuildingModelData index; reuse existing build rules."""
import csv
import hashlib
import json
from pathlib import Path
from rok_data_paths import DATA_ROOT, label

ROOT = Path(__file__).resolve().parents[1]
CSV_ROOT = ROOT / "ref/config-tables/编译客户端配置/CSV"
CLIENT = ROOT / "ref/client-unity/Assets"
OUT = DATA_ROOT / "CityConstructionSources/catalog.json"
GAMEPLAY = DATA_ROOT / "unity_gameplay_catalog.json"
PROXY = CLIENT / "Scripts/Hotfix/MVC/Proxy/CityBuildingProxy.cs"
TABLES = {name: CSV_ROOT / f"{name}.csv" for name in
          ("BuildingModelData", "BuildingTypeConfig", "BuildingLevelData", "BuildingCountLimit")}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_rows(name):
    with TABLES[name].open(encoding="utf-8-sig", newline="") as stream:
        return list(enumerate(list(csv.DictReader(stream))[1:], 3))


def relative(path):
    return label(path) if path else None


def build(write=True):
    gameplay = json.loads(GAMEPLAY.read_text(encoding="utf-8"))
    assert gameplay["schema_version"] and len(gameplay["building_types"]) == 30
    by_type = {r["type_id"]: r for r in gameplay["building_types"]}
    types = {int(raw["type"]): raw for _, raw in read_rows("BuildingTypeConfig")}
    levels = {(int(raw["type"]), int(raw["level"])): raw for _, raw in read_rows("BuildingLevelData")}
    limits = {(int(raw["buildType"]), int(raw["townLevel"])): raw for _, raw in read_rows("BuildingCountLimit")}
    assert len(types) == 30 and len(by_type) == 30
    scene = CLIENT / "BundleAssets/Scene/City_building"
    ui = CLIENT / "BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuilding"
    def prefab_index(directory):
        found = {}
        for path in directory.rglob("*.prefab"):
            if path.stem in found:
                raise AssertionError(("ambiguous prefab", path.stem, found[path.stem], path))
            found[path.stem] = path
        return found
    scene_prefabs, ui_prefabs = prefab_index(scene), prefab_index(ui)
    rows, ids, keys = [], set(), set()
    source_assets = {}
    for source_row, raw in read_rows("BuildingModelData"):
        row_id, building_type, civilization, age = (int(raw[key]) for key in ("ID", "type", "civType", "age"))
        key = (building_type, civilization, age)
        assert row_id == int(f"{building_type}{civilization}{age}")
        assert row_id not in ids and key not in keys and building_type > 0
        assert raw["modelId"] and raw["imgRes"]
        ids.add(row_id); keys.add(key)
        scene_path = scene_prefabs.get(raw["modelId"])
        ui_path = ui_prefabs.get(raw["imgRes"])
        for path in (scene_path, ui_path):
            if path:
                source_assets[relative(path)] = digest(path)
        rows.append({"id": row_id, "type": building_type, "civilization": civilization,
                     "age": age, "model_id": raw["modelId"], "image_resource": raw["imgRes"],
                     "scene_prefab": relative(scene_path), "ui_prefab": relative(ui_path),
                     "missing_visual_sources": [name for name, path in
                                                (("scene_prefab", scene_path), ("ui_prefab", ui_path)) if not path],
                     "has_building_type_config": building_type in types,
                     "source_row": source_row})
    assert len(rows) == 870 and len(ids) == len(keys) == 870
    assert len({r["type"] for r in rows}) == 29
    # The first-build and footprint rules already live in the broad catalog.
    # Cross-check them, but do not clone a second runtime copy of their costs.
    for building_type, type_row in types.items():
        catalog = by_type[building_type]
        footprint = [int(type_row["width"]), int(type_row["length"])]
        assert catalog["size"] == {"width": footprint[0], "length": footprint[1]}
        first = levels.get((building_type, 1))
        if first:
            source = catalog["level_summary"]["level1"]
            assert source["id"] == int(first["ID"]) and source["size"] == {"width": int(first["width"]), "length": int(first["length"])}
            assert source["build_seconds"] == int(first["buildingTime"]) and source["power"] == int(first["power"])
            assert source["cost"] == {"food": int(first["food"]), "wood": int(first["wood"]),
                                      "stone": int(first["stone"]), "gold": int(first["coin"]), "denar": int(first["denar"])}
            assert source["requirements"] == [{"type": int(first[f"reqType{i}"]), "level": int(first[f"reqLevel{i}"])} for i in (1, 2, 3)]
        for limit in catalog["count_limits"]:
            raw = limits[(building_type, limit["town_level"])]
            assert limit["limit"] == int(raw["buildCountLimit"])
    academy = by_type[13]
    level1 = academy["level_summary"]["level1"]
    assert level1["id"] == 1301 and level1["requirements"][0] == {"type": 1, "level": 4}
    assert level1["cost"] == {"food": 500, "wood": 800, "stone": 0, "gold": 0, "denar": 0}
    assert level1["build_seconds"] == 6 and level1["power"] == 5
    assert academy["size"] == {"width": 6, "length": 6}
    assert next(x for x in academy["count_limits"] if x["town_level"] == 4)["limit"] == 1
    academy_model = next(x for x in rows if (x["type"], x["civilization"], x["age"]) == (13, 101, 2))
    assert academy_model["model_id"] == "build_13_101_2" and academy_model["scene_prefab"] and academy_model["ui_prefab"]
    proxy = PROXY.read_text(encoding="utf-8-sig")
    assert "GetModelIdByType" in proxy and "BuildingModelDataDefine" in proxy and "GetImgIdByType" in proxy
    output = {"schema": "rok.city_construction_sources.v1",
              "scope": "Exact type/civilization/age model and image mapping only; building rules remain in unity_gameplay_catalog and BuildingLevelData registry.",
              "unknown_boundary": "No player civilization, age, town hall, inventory, queue, tile occupancy or building count is assumed.",
              "sources": {relative(p): digest(p) for p in (*TABLES.values(), GAMEPLAY, PROXY)},
              "building_type_ids": sorted(types),
              "source_assets": dict(sorted(source_assets.items())),
              "summary": {"model_rows": len(rows), "model_types": 29, "catalog_types": len(types),
                          "model_only_nonbuildable_types": sorted({r["type"] for r in rows} - set(types)),
                          "building_types_without_model": sorted(set(types) - {r["type"] for r in rows}),
                          "scene_prefab_rows": sum(bool(r["scene_prefab"]) for r in rows),
                          "ui_prefab_rows": sum(bool(r["ui_prefab"]) for r in rows),
                          "academy_reference": {"type": 13, "civilization": 101, "age": 2,
                                                "level1_id": 1301, "town_hall_required": 4,
                                                "footprint": [6, 6], "count_limit_at_town_hall_4": 1}},
              "models": rows}
    if write:
        OUT.parent.mkdir(parents=True, exist_ok=True)
        OUT.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {OUT}: {len(rows)} exact model keys")
    return output


if __name__ == "__main__":
    build()
