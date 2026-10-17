"""Reproducible, read-only Study/StudyData source catalog; no player state."""
import csv
import hashlib
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

from BuildRokUnityEconomyCatalog import LANGUAGE_WORKBOOK, localized_text, read_language_lookup

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Saved/RokUnityMigration/research_catalog.json"
CSV_ROOT = ROOT / "ref/config-tables/编译客户端配置/CSV"
UI_ROOT = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_research"
PROXY = ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/ResearchProxy.cs"
MEDIATORS = [ROOT / f"ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Research/{name}.cs"
             for name in ("ResearchMainMediator", "ResearchItemMediator", "ResearchUpateMediator")]
SOURCES = [CSV_ROOT / "Study.csv", CSV_ROOT / "StudyData.csv", LANGUAGE_WORKBOOK,
           ROOT / "ref/client-unity/Assets/StreamingAssets/Config/Bin/Language.bin", PROXY, *MEDIATORS,
           *[UI_ROOT / f"research_icon{index}.{extension}" for index in (1, 2) for extension in ("tpsheet", "png")]]


def table(name):
    path = CSV_ROOT / f"{name}.csv"
    return list(enumerate(list(csv.DictReader(path.read_text(encoding="utf-8-sig").splitlines()))[1:], 3))


def atlas_sprites(name):
    lines = (UI_ROOT / f"{name}.tpsheet").read_text(encoding="utf-8-sig").splitlines()
    size = [int(part) for part in next(line[6:] for line in lines if line.startswith(":size=")).split("x")]
    sprites = {parts[0]: [int(value) for value in parts[1:5]] for line in lines
               if line and not line.startswith(("#", ":")) for parts in [line.split(";")]}
    return size, sprites


def build(write=True):
    language, _ = read_language_lookup()
    raw_study, raw_data = table("Study"), table("StudyData")
    assert len(raw_study) == 410 and len(raw_data) == 8
    atlases = {name: atlas_sprites(name) for name in ("research_icon1", "research_icon2")}

    def locale(value):
        identifier = int(value or 0)
        text = localized_text(language, identifier, "cn")
        assert not identifier or text, ("missing Chinese locale", identifier)
        return text

    def icon(value):
        match = re.fullmatch(r"(research_icon[12])\[([^]]+)\]", value)
        assert match, ("invalid research icon", value)
        atlas, sprite = match.groups()
        size, sprites = atlases[atlas]
        assert sprite in sprites, ("missing research sprite", value)
        return {"source": value, "atlas": atlas, "sprite": sprite, "rect": sprites[sprite], "atlas_size": size}

    rows = []
    ids, keys = set(), set()
    levels = defaultdict(list)
    for source_row, raw in raw_study:
        identifier, study_type, level = (int(raw[key]) for key in ("ID", "studyType", "studyLv"))
        assert identifier not in ids and (study_type, level) not in keys, ("duplicate Study key", identifier, study_type, level)
        ids.add(identifier); keys.add((study_type, level)); levels[study_type].append(level)
        preconditions = []
        for index in range(1, 5):
            predecessor, required = (int(raw[f"precondition{field}{index}"]) for field in ("Study", "Lv"))
            assert (predecessor == 0) == (required == 0), ("incomplete prerequisite", identifier, index)
            preconditions.append({"slot": index, "study_type": predecessor, "level": required})
        costs = {key: int(raw[f"need{key.capitalize()}"]) for key in ("food", "wood", "stone", "gold")}
        assert all(value >= 0 for value in costs.values())
        location = int(raw["location"]) if raw["location"] else None
        assert int(raw["type"]) in (1, 2) and int(raw["columns"]) > 0 and int(raw["campusLv"]) > 0
        rows.append({
            "id": identifier, "type": int(raw["type"]), "study_type": study_type, "level": level,
            "campus_level": int(raw["campusLv"]), "preconditions": preconditions,
            "base_cost": costs, "base_seconds": int(raw["costTime"]), "power": int(raw["power"]),
            "name_locale_id": int(raw["l_nameID"]), "name": locale(raw["l_nameID"]),
            "description_locale_id": int(raw["l_des"]), "description": locale(raw["l_des"]),
            "function_locale_raw": raw["l_function"],
            "function_text": locale(raw["l_function"]) if raw["l_function"].isdigit() else "",
            "buff_type": raw["buffType"], "buff_type_new": raw["buffTypeNew"],
            "buff_data": raw["buffData"], "buff_description_locale_id": int(raw["l_buffDesID"]),
            "buff_description": locale(raw["l_buffDesID"]), "description_data": raw["desData"],
            "icon": icon(raw["icon"]), "columns": int(raw["columns"]), "location": location,
            "source_row": source_row,
        })
    assert len(ids) == len(rows) == 410 and len(levels) == 71
    assert Counter(row["type"] for row in rows) == {1: 193, 2: 217}
    assert Counter(row["type"] for row in rows if row["level"] == 1) == {1: 28, 2: 43}
    for study_type, sequence in levels.items():
        assert sorted(sequence) == list(range(1, max(sequence) + 1)), ("non-contiguous levels", study_type, sequence)
    for row in rows:
        row["max_level"] = max(levels[row["study_type"]])
        for prerequisite in row["preconditions"]:
            if prerequisite["study_type"]:
                assert (prerequisite["study_type"], prerequisite["level"]) in keys, ("missing prerequisite", row["id"], prerequisite)

    overrides = []
    override_keys = set()
    for source_row, raw in raw_data:
        identifier, study_row_id, civilization_id = (int(raw[key]) for key in ("ID", "studyID", "civilizationID"))
        key = (civilization_id, study_row_id)
        assert study_row_id in ids and key not in override_keys and identifier not in {r["id"] for r in overrides}
        override_keys.add(key)
        overrides.append({"id": identifier, "study_row_id": study_row_id, "civilization_id": civilization_id,
                          "name_locale_id": int(raw["l_studyNameID"]), "name": locale(raw["l_studyNameID"]),
                          "description_locale_id": int(raw["l_desID"]), "description": locale(raw["l_desID"]),
                          "icon": icon(raw["icon"]), "source_row": source_row})

    output = {
        "schema": "rok.research_catalog.v1",
        "source_behavior": "ResearchProxy.GetTechnologyByType groups level-one nodes by columns; GetTechnologyByLevel uses studyType+level; StudyData lookup uses civilizationID*1000000+Study.ID",
        "runtime_boundary": "Configuration only. Current player levels, queue, speed, resources and server state must be supplied by a caller.",
        "sources": {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest() for path in SOURCES},
        "summary": {"rows": len(rows), "study_types": len(levels), "type_rows": {"1": 193, "2": 217},
                    "level_one_nodes": {"1": 28, "2": 43}, "civilization_overrides": len(overrides)},
        "rows": rows, "civilization_overrides": overrides,
    }
    if write:
        OUT.parent.mkdir(parents=True, exist_ok=True)
        OUT.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {OUT}: 410 rows / 71 study types / 8 civilization overrides")
    return output


if __name__ == "__main__":
    build()
