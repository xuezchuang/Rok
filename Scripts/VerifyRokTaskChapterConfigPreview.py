"""Independently compare the chapter preview with immutable Unity CSV sources."""

from __future__ import annotations

import csv
import hashlib
import json
from collections import defaultdict
from pathlib import Path

from BuildRokUnityEconomyCatalog import LANGUAGE_WORKBOOK, localized_text, read_language_lookup


ROOT = Path(__file__).resolve().parents[1]
CSV_ROOT = ROOT / "ref/config-tables/编译客户端配置/CSV"
OUTPUT = ROOT / "Saved/RokUnityMigration/TaskChapterConfigPreview/v2/catalog.json"
SOURCE_PATHS = [
    "ref/config-tables/编译客户端配置/CSV/TaskChapter.csv",
    "ref/config-tables/编译客户端配置/CSV/TaskChapterData.csv",
    LANGUAGE_WORKBOOK.relative_to(ROOT).as_posix(),
    "Saved/RokUnityMigration/unity_gameplay_catalog.json",
    "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestView.cs",
    "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestMediator.cs",
    "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_task/UIQuest/UI_Win_Quest.prefab",
    "Scripts/BuildRokUnityEconomyCatalog.py",
    "Scripts/BuildRokTaskChapterConfigPreview.py",
]
FORBIDDEN_ACCOUNT_FIELDS = {"player_chapter_id", "progress", "unlocked", "completed", "claimable",
                            "reward_claimed", "account_id", "role_id", "current_count"}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def table(path: Path) -> list[tuple[int, dict[str, str]]]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.DictReader(handle))
    assert rows and rows[0]["ID"] == "int", path
    return list(enumerate(rows[1:], 3))


def loc(language: dict, source_id: str) -> dict:
    identifier = int(source_id)
    text = localized_text(language, identifier, "cn") if identifier else ""
    return {"id": identifier, "zh_cn": text or None,
            "status": "resolved" if text else "missing_or_unset"}


def check(data: dict) -> None:
    assert data["schema"] == "rok.task_chapter_config_preview.v2"
    assert data["mode"] == "source_configuration_preview"
    assert data["account_state"] == "unknown"
    assert data["reward_policy"] == "configured_group_id_only_no_claim_or_contents"
    assert data["task_count"] == 39 and data["chapter_count"] == 7
    assert data["chapter_metadata_row_count"] == 6
    assert data["missing_chapter_metadata_ids"] == [7]
    expected_sources = [{"path": path, "sha256": digest(ROOT / path)} for path in SOURCE_PATHS]
    assert data["sources"] == expected_sources, "source chain drift"

    def no_account_state(value: object) -> None:
        if isinstance(value, dict):
            assert not FORBIDDEN_ACCOUNT_FIELDS.intersection(value), "account state in config preview"
            for nested in value.values():
                no_account_state(nested)
        elif isinstance(value, list):
            for nested in value:
                no_account_state(nested)

    no_account_state(data)
    language, _ = read_language_lookup()
    raw_chapters = table(CSV_ROOT / "TaskChapterData.csv")
    raw_tasks = table(CSV_ROOT / "TaskChapter.csv")
    assert len(raw_chapters) == 6 and len(raw_tasks) == 39
    chapters = data["chapters"]
    tasks = data["tasks"]
    assert len(chapters) == 7 and len(tasks) == 39
    chapter_by_id = {row["id"]: row for row in chapters}
    task_by_id = {row["id"]: row for row in tasks}
    assert len(chapter_by_id) == 7 and len(task_by_id) == 39
    assert sorted(chapter_by_id) == [1, 2, 3, 4, 5, 6, 7]
    assert [row["id"] for row in chapters] == sorted(chapter_by_id)
    for line, raw in raw_chapters:
        row = chapter_by_id[int(raw["ID"])]
        assert row["source_csv_line"] == line and row["metadata_status"] == "source_row"
        for actual, key in (("title", "l_titleNameId"), ("description_1", "l_descId1"),
                            ("description_2", "l_descId2"), ("preview_text", "preText")):
            assert row[actual] == loc(language, raw[key]), (row["id"], actual)
        for actual, key in (("preview_image", "preImg"), ("preview_background", "prebg"),
                            ("preview_sound", "preSound")):
            assert row[actual] == raw[key], (row["id"], actual)
        for actual, key in (("dialog_begin_ids", "dialogBegin"), ("dialog_end_ids", "dialogEnd")):
            assert row[actual] == [int(v) for v in raw[key].split("|") if v]
        assert row["reward_group_id"] == int(raw["reward"])
        assert row["civilization_character_assets"] == (raw["imgCivRes"].split("|") if raw["imgCivRes"] else [])
        assert row["age_character_assets"] == (raw["imgAgeRes"].split("|") if raw["imgAgeRes"] else [])
    missing = chapter_by_id[7]
    assert missing["metadata_status"] == "missing_in_TaskChapterData"
    assert missing["source_csv_line"] is None
    assert all(missing[key] is None for key in ("title", "description_1", "description_2",
                                                 "preview_text", "preview_image", "preview_background",
                                                 "preview_sound", "dialog_begin_ids", "dialog_end_ids",
                                                 "reward_group_id", "civilization_character_assets",
                                                 "age_character_assets"))

    by_chapter: dict[int, list[dict]] = defaultdict(list)
    gameplay = json.loads((ROOT / "Saved/RokUnityMigration/unity_gameplay_catalog.json").read_text(encoding="utf-8"))
    existing = {row["id"]: row for row in gameplay["chapter_tasks"]}
    assert len(existing) == 39 and set(existing) == set(task_by_id)
    for line, raw in raw_tasks:
        identifier = int(raw["ID"])
        row = task_by_id[identifier]
        assert row["source_csv_line"] == line
        assert row["chapter_id"] == int(raw["chapterId"])
        assert row["order"] == int(raw["order"])
        assert row["title"] == loc(language, raw["l_nameId"])
        assert row["description"] == loc(language, raw["l_desc"])
        for actual, source in (("type", "type"), ("param1", "param1"), ("param2", "param2"),
                               ("require", "require"), ("reward_group_id", "reward")):
            assert row[actual] == int(raw[source]), (identifier, actual)
        assert row["icon_source_key"] == raw["iconImg"]
        prior = existing[identifier]
        assert prior["order"] == row["order"] and prior["reward"] == row["reward_group_id"]
        by_chapter[row["chapter_id"]].append(row)
    assert sorted(by_chapter) == list(range(1, 8))
    assert tasks == sorted(tasks, key=lambda row: (row["chapter_id"], row["order"], row["source_csv_line"]))
    for chapter in chapters:
        sequence = sorted(by_chapter[chapter["id"]], key=lambda row: row["order"])
        assert len({row["order"] for row in sequence}) == len(sequence)
        assert chapter["task_ids"] == [row["id"] for row in sequence]


def negative_checks(data: dict) -> None:
    import copy

    for edit in (
        lambda item: item["tasks"][0].__setitem__("require", item["tasks"][0]["require"] + 1),
        lambda item: item["tasks"][0].__setitem__("order", 99),
        lambda item: item["chapters"][6].__setitem__("title", {"id": 0, "zh_cn": "假标题", "status": "resolved"}),
        lambda item: item.__setitem__("account_state", "completed"),
        lambda item: item["chapters"][6].__setitem__("dialog_begin_ids", [999]),
        lambda item: item["chapters"][6].__setitem__("dialog_end_ids", [999]),
        lambda item: item["chapters"][6].__setitem__("civilization_character_assets", ["fake"]),
        lambda item: item["chapters"][6].__setitem__("age_character_assets", ["fake"]),
    ):
        altered = copy.deepcopy(data)
        edit(altered)
        try:
            check(altered)
        except AssertionError:
            continue
        raise AssertionError("mutated source/account input was accepted")


def main() -> None:
    data = json.loads(OUTPUT.read_text(encoding="utf-8"))
    check(data)
    negative_checks(data)
    print(f"PASS {OUTPUT.relative_to(ROOT)}: 39 tasks, 7 groups, 6 chapter metadata rows, "
          f"chapter 7 metadata missing, 8 negative cases; sha256={digest(OUTPUT)}")


if __name__ == "__main__":
    main()
