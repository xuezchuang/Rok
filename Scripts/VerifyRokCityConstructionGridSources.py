"""Independently compare the city grid package to Unity literals and CSV rows."""
import csv
import hashlib
import json
import re
from collections import defaultdict

from BuildRokCityConstructionGridSources import (
    AGE_TABLE, CITY_OBJECT, INIT_TABLE, MEDIATOR, OUT, PROXY, ROOT, SOURCES,
    TYPE_TABLE,
)
from rok_data_paths import label


REPORT = ROOT / "Saved/RokUnityMigration/CityConstructionGridSources/verify.json"
SIZES = (41, 45, 49, 53, 57)


def csv_data(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    assert rows and rows[0][next(iter(rows[0]))] == "int"
    return rows[1:]


def verify():
    package = json.loads(OUT.read_text(encoding="utf-8"))
    assert package["schema"] == "rok.city_construction_grid_sources.v1"
    hashes = {label(path): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in SOURCES}
    assert package["sources"] == hashes
    text = PROXY.read_text(encoding="utf-8-sig")
    source_object = CITY_OBJECT.read_text(encoding="utf-8-sig")
    mediator = MEDIATOR.read_bytes().decode("latin1")
    for key, expected in (("CITY_GRID_STATE_NORMAL", 5), ("CITY_GRID_RESERVED_OFFSET", 7),
                          ("CITY_BUILDING_TYPE_OFFSET", 100)):
        assert re.search(rf"\b{key}\s*=\s*{expected}\s*;", source_object)
    assert package["normal_value"] == 5 and package["reserved_offset"] == 7
    assert package["building_type_offset"] == 100
    for piece in ("float line = y + a;", "float col = x + a + 1f;",
                  "float key = line * size + col;", "return new Vector2(bt.width, bt.length);"):
        assert piece in mediator
    assert package["grid_index"] == {
        "formula": "(y + (size - 1) / 2) * size + (x + (size - 1) / 2 + 1)",
        "x_offset": 1, "traversal": "row-major y ascending, x ascending",
        "rectangle_traversal": "x outer, y inner; positive width and length",
    }
    assert len(package["ages"]) == len(csv_data(AGE_TABLE)) == 5
    assert re.search(r"cityAgeSize\s*=\s*new int\[\]\[\]\s*\{\s*size41\s*,\s*size45\s*,\s*size49\s*,\s*size53\s*,\s*size57\s*\}", text)
    for age, csv_row, size in zip(package["ages"], csv_data(AGE_TABLE), SIZES):
        match = re.search(rf"\bsize{size}\s*=\s*new int\[\]\s*\{{([^}}]*)\}}", text)
        assert match, f"Missing source array size{size}"
        source_cells = [int(token) for token in re.findall(r"-?\d+", match.group(1))]
        assert len(source_cells) == size * size
        assert (age["age"], age["town_hall_level"], age["size"], age["source_row"]) == (
            int(csv_row["age"]), int(csv_row["townLevel"]), int(csv_row["size"]),
            int(csv_row["ID"]) + 2)
        assert age["age"] == SIZES.index(size) + 1
        assert age["raw_cells"] == source_cells
        # MakeCityMap converts source type markers; MakeCityMapEmpty clears only >7.
        independently_empty = []
        for cell in source_cells:
            if cell > 100:
                cell = cell - 100 + 7
            if cell > 7:
                cell = 5
            independently_empty.append(cell)
        assert age["empty_cells"] == independently_empty
        assert set(independently_empty) == {4, 5}
        assert age["buildable_cell_count"] == independently_empty.count(5)
    type_rows = csv_data(TYPE_TABLE)
    types = {int(row["type"]): (int(row["width"]), int(row["length"])) for row in type_rows}
    assert len(types) == len(type_rows)
    init_rows = csv_data(INIT_TABLE)
    initial = package["initial_occupancy"]
    assert len(init_rows) == len(initial) == 26
    assert len({row["init_id"] for row in initial}) == 26
    occupancy = defaultdict(lambda: defaultdict(list))
    for raw, row in zip(init_rows, initial):
        building_type = int(raw["type"])
        x, y = int(raw["posX"]), int(raw["posY"])
        width, length = types[building_type]
        assert (row["init_id"], row["type"], row["level"], row["pos_x"], row["pos_y"],
                row["width"], row["length"], row["source_row"]) == (
                    int(raw["ID"]), building_type, int(raw["level"]), x, y, width, length,
                    int(raw["ID"]) + 2)
        assert width >= 0 and length >= 0 and ((width == 0) == (length == 0))
        for age in package["ages"]:
            size = age["size"]
            half = (size - 1) // 2
            indices = []
            for cell_x in range(x, x + width):
                for cell_y in range(y, y + length):
                    assert -half <= cell_x <= half and -half <= cell_y <= half
                    index = (cell_y + half) * size + (cell_x + half + 1)
                    assert 0 <= index < size * size
                    assert age["empty_cells"][index] == 5, (age["age"], row["init_id"], index)
                    indices.append(index)
                    occupancy[age["age"]][index].append(row["init_id"])
            assert len(indices) == width * length == len(set(indices))
            if age["age"] == 1:
                assert row["cell_indices"] == indices
    expected_overlaps = [
        {"age": age, "index": index, "init_ids": ids}
        for age in range(1, 6)
        for index, ids in sorted(occupancy[age].items()) if len(ids) > 1
    ]
    assert package["overlaps"] == expected_overlaps
    # Known source currently has no overlapping initial rectangles. If Unity source changes,
    # require review instead of silently promoting the new collision pattern.
    assert expected_overlaps == [], f"Source now contains {len(expected_overlaps)} overlaps; review needed"
    assert package["summary"] == {
        "age_count": 5, "initial_building_count": 26,
        "occupied_initial_building_count": 24, "overlap_cell_count": 0,
    }
    result = {"schema": "rok.city_construction_grid_sources.verify.v1", "status": "passed",
              "catalog_sha256": hashlib.sha256(OUT.read_bytes()).hexdigest(),
              "source_count": len(hashes), "ages_checked": 5,
              "cells_checked": sum(size * size for size in SIZES),
              "initial_buildings_checked": 26, "occupied_initial_buildings": 24,
              "overlap_cells": 0}
    REPORT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return result


if __name__ == "__main__":
    result = verify()
    print(f"{REPORT.relative_to(ROOT)}: {result['status']}, "
          f"{result['cells_checked']} cells, {result['initial_buildings_checked']} initial buildings")
