"""Build a source-only Quest Chapter configuration preview. No player state."""

from __future__ import annotations

import csv
import hashlib
import json
from collections import defaultdict
from pathlib import Path

from BuildRokUnityEconomyCatalog import LANGUAGE_WORKBOOK, localized_text, read_language_lookup
from rok_data_paths import DATA_ROOT, label


ROOT = Path(__file__).resolve().parents[1]
CSV_ROOT = ROOT / "ref/config-tables/编译客户端配置/CSV"
TASK_CSV = CSV_ROOT / "TaskChapter.csv"
CHAPTER_CSV = CSV_ROOT / "TaskChapterData.csv"
GAMEPLAY_CATALOG = DATA_ROOT / "unity_gameplay_catalog.json"
VIEW = ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestView.cs"
MEDIATOR = ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestMediator.cs"
PREFAB = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_task/UIQuest/UI_Win_Quest.prefab"
OUTPUT = DATA_ROOT / "TaskChapterConfigPreview/v2/catalog.json"
LOCALE_HELPER = ROOT / "Scripts/BuildRokUnityEconomyCatalog.py"
SOURCES = (TASK_CSV, CHAPTER_CSV, LANGUAGE_WORKBOOK, GAMEPLAY_CATALOG, VIEW, MEDIATOR, PREFAB,
           LOCALE_HELPER,
           Path(__file__).resolve())


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source(path: Path) -> dict:
    return {"path": label(path), "sha256": sha(path)}


def csv_rows(path: Path, columns: tuple[str, ...]) -> list[tuple[int, dict[str, str]]]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        assert reader.fieldnames == list(columns), (path, reader.fieldnames)
        rows = list(reader)
    assert rows and all(set(row) == set(columns) and None not in row for row in rows)
    assert [row["ID"] for row in rows][0] == "int", (path, "missing compiled type row")
    return list(enumerate(rows[1:], 3))


def integer(row: dict[str, str], field: str) -> int:
    value = row[field]
    assert value and value.strip() == value and (value.isdigit() or
           (value.startswith("-") and value[1:].isdigit())), (field, value)
    return int(value)


def locale(language: dict, identifier: int) -> dict:
    assert identifier >= 0
    text = localized_text(language, identifier, "cn") if identifier else ""
    return {"id": identifier, "zh_cn": text if text else None,
            "status": "resolved" if text else "missing_or_unset"}


def build() -> dict:
    language, _ = read_language_lookup()
    task_fields = ("ID", "l_nameId", "l_desc", "type", "param1", "param2", "require",
                   "reward", "order", "chapterId", "iconImg")
    chapter_fields = ("ID", "l_titleNameId", "l_descId1", "l_descId2", "preImg", "prebg",
                      "preText", "preSound", "dialogBegin", "dialogEnd", "reward",
                      "imgCivRes", "imgAgeRes")
    raw_tasks = csv_rows(TASK_CSV, task_fields)
    raw_chapters = csv_rows(CHAPTER_CSV, chapter_fields)
    assert len(raw_tasks) == 39 and len(raw_chapters) == 6
    chapters = []
    chapter_ids = set()
    for line, raw in raw_chapters:
        identifier = integer(raw, "ID")
        assert identifier > 0 and identifier not in chapter_ids
        chapter_ids.add(identifier)
        chapters.append({
            "id": identifier, "source_csv_line": line,
            "title": locale(language, integer(raw, "l_titleNameId")),
            "description_1": locale(language, integer(raw, "l_descId1")),
            "description_2": locale(language, integer(raw, "l_descId2")),
            "preview_text": locale(language, integer(raw, "preText")),
            "preview_image": raw["preImg"], "preview_background": raw["prebg"],
            "preview_sound": raw["preSound"],
            "dialog_begin_ids": [int(value) for value in raw["dialogBegin"].split("|") if value],
            "dialog_end_ids": [int(value) for value in raw["dialogEnd"].split("|") if value],
            "reward_group_id": integer(raw, "reward"),
            "civilization_character_assets": raw["imgCivRes"].split("|") if raw["imgCivRes"] else [],
            "age_character_assets": raw["imgAgeRes"].split("|") if raw["imgAgeRes"] else [],
        })
    tasks = []
    task_ids = set()
    orders = defaultdict(set)
    for line, raw in raw_tasks:
        identifier, chapter_id, order = (integer(raw, key) for key in ("ID", "chapterId", "order"))
        assert identifier > 0 and identifier not in task_ids
        assert chapter_id > 0 and order > 0 and order not in orders[chapter_id]
        task_ids.add(identifier)
        orders[chapter_id].add(order)
        tasks.append({
            "id": identifier, "chapter_id": chapter_id, "order": order,
            "source_csv_line": line,
            "title": locale(language, integer(raw, "l_nameId")),
            "description": locale(language, integer(raw, "l_desc")),
            "type": integer(raw, "type"), "param1": integer(raw, "param1"),
            "param2": integer(raw, "param2"), "require": integer(raw, "require"),
            "reward_group_id": integer(raw, "reward"), "icon_source_key": raw["iconImg"],
        })
    tasks.sort(key=lambda row: (row["chapter_id"], row["order"], row["source_csv_line"]))
    missing_chapter_ids = sorted(set(orders) - chapter_ids)
    assert missing_chapter_ids == [7], ("unexpected TaskChapterData gap", missing_chapter_ids)
    for identifier in missing_chapter_ids:
        chapters.append({"id": identifier, "source_csv_line": None,
                         "metadata_status": "missing_in_TaskChapterData",
                         "title": None, "description_1": None, "description_2": None,
                         "preview_text": None, "preview_image": None,
                         "preview_background": None, "preview_sound": None,
                         "dialog_begin_ids": None, "dialog_end_ids": None,
                         "reward_group_id": None, "civilization_character_assets": None,
                         "age_character_assets": None})
    for chapter in chapters:
        chapter.setdefault("metadata_status", "source_row")
        chapter["task_ids"] = [row["id"] for row in tasks if row["chapter_id"] == chapter["id"]]
        assert chapter["task_ids"]
    chapters.sort(key=lambda row: row["id"])
    gameplay = json.loads(GAMEPLAY_CATALOG.read_text(encoding="utf-8"))
    existing = {int(row["id"]): row for row in gameplay["chapter_tasks"]}
    assert len(existing) == len(tasks) == 39 and set(existing) == task_ids
    for task in tasks:
        prior = existing[task["id"]]
        for compiled, catalog in (("type", "type"), ("param1", "param1"), ("param2", "param2"),
                                  ("require", "require"), ("reward_group_id", "reward"), ("order", "order"),
                                  ("icon_source_key", "icon")):
            assert task[compiled] == prior[catalog], (task["id"], compiled)
    return {
        "schema": "rok.task_chapter_config_preview.v2",
        "mode": "source_configuration_preview",
        "account_state": "unknown",
        "reward_policy": "configured_group_id_only_no_claim_or_contents",
        "task_count": len(tasks), "chapter_count": len(chapters),
        "chapter_metadata_row_count": len(raw_chapters),
        "missing_chapter_metadata_ids": missing_chapter_ids,
        "sources": [source(path) for path in SOURCES],
        "chapters": chapters, "tasks": tasks,
    }


def main() -> None:
    catalog = build()
    encoded = (json.dumps(catalog, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes(encoded)
    print(f"{OUTPUT.relative_to(ROOT)}: {catalog['chapter_count']} chapters, {catalog['task_count']} tasks, sha256={sha(OUTPUT)}")


if __name__ == "__main__":
    main()
