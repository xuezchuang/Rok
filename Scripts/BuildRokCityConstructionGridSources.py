"""Extract Unity city grid masks and initial rectangles without creating buildings."""
import csv
import hashlib
import json
import re
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CSV_ROOT = ROOT / "ref/config-tables/编译客户端配置/CSV"
PROXY = ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/CityBuildingProxy.cs"
MEDIATOR = ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/CityGlobalMediator.cs"
CITY_OBJECT = ROOT / "ref/client-unity/Assets/Scripts/Hotfix/Common/Utils/CityObjData.cs"
AGE_TABLE = CSV_ROOT / "CityAgeSize.csv"
INIT_TABLE = CSV_ROOT / "InitBuilding.csv"
TYPE_TABLE = CSV_ROOT / "BuildingTypeConfig.csv"
OUT = ROOT / "Saved/RokUnityMigration/CityConstructionGridSources/catalog.json"
SOURCES = (PROXY, MEDIATOR, CITY_OBJECT, AGE_TABLE, INIT_TABLE, TYPE_TABLE)
SIZES = (41, 45, 49, 53, 57)


def source_rows(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows or rows[0][next(iter(rows[0]))] != "int":
        raise ValueError(f"Missing CSV type row: {path}")
    return list(enumerate(rows[1:], 3))


def source_arrays():
    source = PROXY.read_text(encoding="utf-8-sig")
    arrays = {}
    for size in SIZES:
        match = re.search(rf"\bsize{size}\s*=\s*new int\[\]\s*\{{([^}}]*)\}}", source)
        if not match:
            raise ValueError(f"Missing size{size} literal array")
        cells = [int(value) for value in re.findall(r"-?\d+", match.group(1))]
        if len(cells) != size * size:
            raise ValueError(f"size{size} has {len(cells)} cells, expected {size * size}")
        arrays[size] = cells
    if not re.search(r"cityAgeSize\s*=\s*new int\[\]\[\]\s*\{\s*size41\s*,\s*size45\s*,\s*size49\s*,\s*size53\s*,\s*size57\s*\}", source):
        raise ValueError("City age array traversal changed")
    return arrays


def assert_source_rules():
    proxy = PROXY.read_text(encoding="utf-8-sig")
    # This legacy C# file is not UTF-8; the checked C# expressions are ASCII.
    mediator = MEDIATOR.read_bytes().decode("latin1")
    city_object = CITY_OBJECT.read_text(encoding="utf-8-sig")
    constants = {key: int(re.search(rf"\b{key}\s*=\s*(\d+)\s*;", city_object).group(1))
                 for key in ("CITY_GRID_STATE_NORMAL", "CITY_GRID_RESERVED_OFFSET", "CITY_BUILDING_TYPE_OFFSET")}
    if constants != {"CITY_GRID_STATE_NORMAL": 5, "CITY_GRID_RESERVED_OFFSET": 7,
                     "CITY_BUILDING_TYPE_OFFSET": 100}:
        raise ValueError(f"City grid constants changed: {constants}")
    for fragment in ("clone[i] = mapdata[i] - CityObjData.CITY_BUILDING_TYPE_OFFSET +",
                     "if (CityObjData.CITY_GRID_RESERVED_OFFSET < data[i])",
                     "data[i] = CityObjData.CITY_GRID_STATE_NORMAL"):
        if fragment not in proxy:
            raise ValueError(f"City map conversion changed: {fragment}")
    for fragment in ("float line = y + a;", "float col = x + a + 1f;",
                     "float key = line * size + col;",
                     "return new Vector2(bt.width, bt.length);",
                     "for (int xindex = (int) info.pos.x; xindex < xend; xindex++)",
                     "for (int yindex = (int) info.pos.y; yindex < yend; yindex++)"):
        if fragment not in mediator:
            raise ValueError(f"City occupancy coordinate rule changed: {fragment}")
    return constants


def grid_index(x, y, size):
    half = (size - 1) // 2
    if not (-half <= x <= half and -half <= y <= half):
        return None
    index = (y + half) * size + (x + half + 1)
    return index if 0 <= index < size * size else None


def build(write=True):
    constants = assert_source_rules()
    arrays = source_arrays()
    age_rows = source_rows(AGE_TABLE)
    if len(age_rows) != 5:
        raise ValueError(f"Expected five CityAgeSize ages, got {len(age_rows)}")
    ages = []
    for ordinal, (line, row) in enumerate(age_rows, 1):
        age, size = int(row["age"]), int(row["size"])
        if age != ordinal or size != SIZES[ordinal - 1] or int(row["ID"]) != age:
            raise ValueError(f"Age/size mapping changed at CityAgeSize.csv:{line}")
        raw = arrays[size]
        converted = [value - 100 + 7 if value > 100 else value for value in raw]
        empty = [5 if value > 7 else value for value in converted]
        if set(empty) != {4, 5}:
            raise ValueError(f"Unknown age {age} normalized grid values: {set(empty)}")
        ages.append({"age": age, "town_hall_level": int(row["townLevel"]), "size": size,
                     "raw_cells": raw, "empty_cells": empty,
                     "buildable_cell_count": empty.count(5), "source_row": line})
    type_rows = source_rows(TYPE_TABLE)
    types = {}
    for line, row in type_rows:
        building_type = int(row["type"])
        if building_type in types:
            raise ValueError(f"Duplicate BuildingTypeConfig type {building_type}")
        types[building_type] = (int(row["width"]), int(row["length"]))
    initial = []
    ids = set()
    occupancy_by_age = {age["age"]: defaultdict(list) for age in ages}
    for line, row in source_rows(INIT_TABLE):
        init_id = int(row["ID"])
        building_type = int(row["type"])
        if init_id in ids or building_type not in types:
            raise ValueError(f"Duplicate/unknown InitBuilding at row {line}")
        ids.add(init_id)
        width, length = types[building_type]
        if width < 0 or length < 0 or (width == 0) != (length == 0):
            raise ValueError(f"Invalid footprint for building type {building_type}")
        x, y = int(row["posX"]), int(row["posY"])
        indices_by_age = {}
        for age in ages:
            indices = []
            for cell_x in range(x, x + width):
                for cell_y in range(y, y + length):
                    index = grid_index(cell_x, cell_y, age["size"])
                    if index is None:
                        raise ValueError(f"InitBuilding {init_id} falls outside age {age['age']} at {cell_x},{cell_y}")
                    if age["empty_cells"][index] != 5:
                        raise ValueError(f"InitBuilding {init_id} covers blocked age {age['age']} cell {index}")
                    indices.append(index)
                    occupancy_by_age[age["age"]][index].append(init_id)
            if len(indices) != width * length or len(indices) != len(set(indices)):
                raise ValueError(f"InitBuilding {init_id} has duplicate occupancy in age {age['age']}")
            indices_by_age[age["age"]] = indices
        initial.append({"init_id": init_id, "type": building_type, "level": int(row["level"]),
                        "pos_x": x, "pos_y": y, "width": width, "length": length,
                        "cell_indices": indices_by_age[1], "source_row": line})
    if len(initial) != 26 or len(ids) != 26:
        raise ValueError(f"Unexpected InitBuilding count: {len(initial)}")
    overlaps = [{"age": age, "index": index, "init_ids": ids}
                for age, used in occupancy_by_age.items() for index, ids in sorted(used.items()) if len(ids) > 1]
    manifest = {
        "schema": "rok.city_construction_grid_sources.v1",
        "sources": {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                    for path in SOURCES},
        "grid_index": {"formula": "(y + (size - 1) / 2) * size + (x + (size - 1) / 2 + 1)",
                       "x_offset": 1, "traversal": "row-major y ascending, x ascending",
                       "rectangle_traversal": "x outer, y inner; positive width and length"},
        "normal_value": constants["CITY_GRID_STATE_NORMAL"],
        "reserved_offset": constants["CITY_GRID_RESERVED_OFFSET"],
        "building_type_offset": constants["CITY_BUILDING_TYPE_OFFSET"],
        "ages": ages,
        "initial_occupancy": initial,
        "overlaps": overlaps,
        "summary": {"age_count": len(ages), "initial_building_count": len(initial),
                    "occupied_initial_building_count": sum(bool(row["cell_indices"]) for row in initial),
                    "overlap_cell_count": len(overlaps)},
    }
    if write:
        OUT.parent.mkdir(parents=True, exist_ok=True)
        OUT.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return manifest


if __name__ == "__main__":
    result = build()
    print(f"{OUT.relative_to(ROOT)}: {len(result['ages'])} ages, "
          f"{len(result['initial_occupancy'])} initial buildings, "
          f"{len(result['overlaps'])} overlapping cells")
