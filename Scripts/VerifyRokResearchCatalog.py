"""Verify the generated research catalog against independent Unity source reads."""
import csv
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path

from BuildRokResearchCatalog import ROOT, OUT, CSV_ROOT, SOURCES, build

REPORT = ROOT / "Saved/RokUnityMigration/research_catalog_verify.json"


def source_rows(name):
    path = CSV_ROOT / f"{name}.csv"
    return list(csv.DictReader(path.read_text(encoding="utf-8-sig").splitlines()))[1:]


def verify():
    actual = json.loads(OUT.read_text(encoding="utf-8"))
    expected = build(write=False)
    assert actual == expected, "catalog differs from fresh source extraction"
    assert actual["schema"] == "rok.research_catalog.v1"
    assert actual["sources"] == {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in SOURCES}
    study, study_data = source_rows("Study"), source_rows("StudyData")
    rows, overrides = actual["rows"], actual["civilization_overrides"]
    assert len(study) == len(rows) == 410 and len(study_data) == len(overrides) == 8
    ids, keys, groups = set(), set(), defaultdict(list)
    for raw, row in zip(study, rows):
        identity = (int(raw["studyType"]), int(raw["studyLv"]))
        assert row["id"] == int(raw["ID"]) and (row["study_type"], row["level"]) == identity
        assert row["id"] not in ids and identity not in keys
        ids.add(row["id"]); keys.add(identity); groups[row["study_type"]].append(row)
        assert row["type"] == int(raw["type"]) and row["campus_level"] == int(raw["campusLv"])
        assert row["columns"] == int(raw["columns"])
        assert row["location"] == (int(raw["location"]) if raw["location"] else None)
        assert row["base_cost"] == {key: int(raw[f"need{key.capitalize()}"]) for key in ("food", "wood", "stone", "gold")}
        assert row["base_seconds"] == int(raw["costTime"]) and row["power"] == int(raw["power"])
        assert row["icon"]["source"] == raw["icon"] and row["name_locale_id"] == int(raw["l_nameID"])
        assert row["buff_type"] == raw["buffType"] and row["buff_type_new"] == raw["buffTypeNew"]
        assert row["buff_data"] == raw["buffData"] and row["description_data"] == raw["desData"]
        for slot, pre in enumerate(row["preconditions"], 1):
            assert pre == {"slot": slot, "study_type": int(raw[f"preconditionStudy{slot}"]),
                           "level": int(raw[f"preconditionLv{slot}"])}
    assert len(groups) == 71 and Counter(row["type"] for row in rows) == {1: 193, 2: 217}
    assert Counter(row["type"] for row in rows if row["level"] == 1) == {1: 28, 2: 43}
    for group in groups.values():
        levels = sorted(row["level"] for row in group)
        assert levels == list(range(1, levels[-1] + 1))
        assert all(row["max_level"] == levels[-1] for row in group)
    for row in rows:
        for pre in row["preconditions"]:
            assert (pre["study_type"] == 0 and pre["level"] == 0) or (pre["study_type"], pre["level"]) in keys
    override_keys = set()
    for raw, override in zip(study_data, overrides):
        key = (int(raw["civilizationID"]), int(raw["studyID"]))
        assert override["id"] == int(raw["ID"]) and (override["civilization_id"], override["study_row_id"]) == key
        assert key not in override_keys and key[1] in ids and override["icon"]["source"] == raw["icon"]
        override_keys.add(key)
    proxy = (ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/ResearchProxy.cs").read_text(encoding="utf-8-sig")
    assert "GetTechnologyByLevel" in proxy and "GetTechnologyByType" in proxy and "GetTechnologyList" in proxy
    assert "civilizationID * 1000000" in proxy or "civilizationID*1000000" in proxy or "civilizationID* 1000000" in proxy
    result = {"schema": "rok.research_catalog_verify.v1", "status": "passed", "catalog_sha256": hashlib.sha256(OUT.read_bytes()).hexdigest(),
              "source_count": len(SOURCES), "rows_checked": len(rows), "study_types_checked": len(groups),
              "level_one_by_type": {"1": 28, "2": 43}, "civilization_overrides_checked": len(overrides)}
    REPORT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"research catalog verified: {len(rows)} rows, {len(groups)} groups, {len(overrides)} civilization overrides")
    return result


if __name__ == "__main__":
    verify()
