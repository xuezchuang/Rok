"""Independently compare the chapter reward preview with raw source CSV/icon tables."""

from __future__ import annotations

import copy
import csv
import hashlib
import json
from pathlib import Path

from BuildRokUnityEconomyCatalog import localized_text, read_language_lookup

import BuildRokTaskChapterRewardPreview as producer
from rok_data_paths import DATA_ROOT, label, resolve

ROOT = Path(__file__).resolve().parents[1]
CSV = ROOT / "ref/config-tables/编译客户端配置/CSV"
CATALOG = DATA_ROOT / "TaskChapterRewardPreview/v1/catalog.json"
CHAPTER = DATA_ROOT / "TaskChapterConfigPreview/v2/catalog.json"
INVENTORY = ROOT / "Saved/RokUnityMigration/BatchReview20261009QuestChapterArtAndHallCamera/task_chapter_row_art_inventory_v1.json"
REPORT = ROOT / "Saved/RokUnityMigration/TaskChapterRewardPreview/v1/verify.json"
META = {100: ("Currency.csv", "l_desID", "iconID", "currency"),
        200: ("Item.csv", "l_nameID", "itemIcon", "item"),
        300: ("Arms.csv", "l_armsID", "icon", "soldier")}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(name: str) -> dict[int, dict[str, str]]:
    parsed = list(csv.DictReader((CSV / name).read_text(encoding="utf-8-sig").splitlines()))
    assert parsed and parsed[0]["ID"] == "int"
    result = {int(row["ID"]): row for row in parsed[1:]}
    assert len(result) == len(parsed) - 1
    return result


def verify_document(doc: dict, chapter: dict, inventory: dict) -> None:
    assert doc["schema"] == "rok.task_chapter_reward_preview.v1"
    assert doc["policy"] == "source_configuration_only_no_account_progress_ownership_claim_or_roll"
    assert doc["chapter_catalog_sha256"] == sha(CHAPTER)
    assert [doc[field] for field in ("task_count", "chapter_count", "task_group_count",
                                    "chapter_aggregate_group_count", "group_count", "reward_row_count")] == [39, 7, 39, 6, 45, 123]
    expected_sources = {entry["path"]: entry["sha256"] for entry in doc["sources"]}
    assert len(expected_sources) == len(doc["sources"])
    for path, hash_value in expected_sources.items():
        assert sha(resolve(path)) == hash_value, path
    for required in (CHAPTER, INVENTORY, CSV / "ItemPackageShow.csv", CSV / "Currency.csv",
                     CSV / "Item.csv", CSV / "Arms.csv", ROOT / "ref/config-tables/s_Language.xlsm",
                     ROOT / "Scripts/BuildRokUnityEconomyCatalog.py", Path(producer.__file__)):
        assert label(required) in expected_sources

    chapter_rows = {row["id"]: row for row in chapter["chapters"]}
    task_rows = {row["id"]: row for row in chapter["tasks"]}
    assert len(chapter_rows) == 7 and len(task_rows) == 39
    assert len(doc["chapters"]) == 7
    for number, item in enumerate(doc["chapters"], 1):
        assert item["chapter_id"] == number
        assert item["metadata_known"] is (number != 7)
        assert item["reward_group_id"] == chapter_rows[number]["reward_group_id"]
        if number == 7:
            assert item["reward_group_id"] is None

    packages = rows("ItemPackageShow.csv")
    metadata = {kind: rows(spec[0]) for kind, spec in META.items()}
    language, _ = read_language_lookup()
    atlas = {}
    for entry in inventory["static_reward_preview"]["reusable_atlases"]:
        namespace = entry["namespace"]
        size = entry["atlas_size"]
        sheet = resolve(entry["source_tpsheet"]["path"]).read_text(encoding="utf-8-sig").splitlines()
        sprites = {}
        for line in sheet:
            fields = [v.strip() for v in line.split(";")]
            if len(fields) >= 5 and fields[1:5] and all(v.isdecimal() for v in fields[1:5]):
                sprites[fields[0]] = [int(v) for v in fields[1:5]]
        atlas[namespace] = (size, sprites, entry["ue_target"] + "." + entry["ue_target"].rsplit("/", 1)[1])

    group_ids = set()
    package_ids = set()
    task_ids = set()
    aggregate_ids = set()
    icon_counts = {}
    assert len(doc["groups"]) == 45
    for group in doc["groups"]:
        group_id = group["reward_group_id"]
        chapter_id = group["chapter_id"]
        assert group_id not in group_ids and chapter_id in chapter_rows
        group_ids.add(group_id)
        if group["owner_kind"] == "task":
            task_id = group["task_id"]
            assert task_id in task_rows and task_id not in task_ids
            task_ids.add(task_id)
            assert chapter_id == task_rows[task_id]["chapter_id"]
            assert group_id == task_rows[task_id]["reward_group_id"]
        else:
            assert group["owner_kind"] == "chapter" and group["task_id"] is None
            assert chapter_id < 7 and chapter_id not in aggregate_ids
            aggregate_ids.add(chapter_id)
            assert group_id == chapter_rows[chapter_id]["reward_group_id"]
        assert group["metadata_known"] is (chapter_id != 7)
        expected_ids = sorted(key for key, package in packages.items()
                              if int(package["group"]) == group_id)
        assert expected_ids == [group_id * 1000 + index for index in range(1, len(expected_ids) + 1)]
        assert len(group["entries"]) == len(expected_ids) and expected_ids
        for order, (entry, package_id) in enumerate(zip(group["entries"], expected_ids), 1):
            source = packages[package_id]
            assert not source["civilization_limit"].strip()
            reward_type = int(source["type"])
            assert reward_type in META
            filename, locale_field, icon_field, kind = META[reward_type]
            source_meta = metadata[reward_type][int(source["typeData"])]
            locale_id = int(source_meta[locale_field])
            icon = source_meta[icon_field]
            namespace, sprite = icon[:-1].split("[", 1)
            size, sprites, object_path = atlas[namespace]
            assert entry == {
                "package_row_id": package_id, "reward_group_id": group_id,
                "reward_type": reward_type, "kind": kind,
                "type_data": int(source["typeData"]), "quantity": int(source["number"]),
                "locale_id": locale_id,
                "display_name_zh_cn": localized_text(language, locale_id, "cn"),
                "icon_key": icon, "atlas_object_path": object_path,
                "icon_rect": sprites[sprite], "icon_atlas_size": size,
                "stable_order": order,
            }
            assert entry["quantity"] > 0 and package_id not in package_ids
            package_ids.add(package_id)
            icon_counts[icon] = icon_counts.get(icon, 0) + 1
    assert len(task_ids) == 39 and aggregate_ids == set(range(1, 7))
    assert len(package_ids) == 123 and len(icon_counts) == 19
    assert icon_counts == inventory["static_reward_preview"]["resolved_icon_occurrences_by_key"]
    assert all(not (group["owner_kind"] == "chapter" and group["chapter_id"] == 7)
               for group in doc["groups"])


def main() -> None:
    doc = json.loads(CATALOG.read_text(encoding="utf-8"))
    chapter = json.loads(CHAPTER.read_text(encoding="utf-8"))
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    verify_document(doc, chapter, inventory)
    # Reproducibility includes exact UTF-8/newline representation, not just parsed values.
    expected_bytes = (json.dumps(producer.build(), ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    assert CATALOG.read_bytes() == expected_bytes
    negative = 0
    for mutate in (
        lambda copy: copy["groups"][0]["entries"][0].__setitem__("quantity", 99),
        lambda copy: copy["groups"][0]["entries"][0]["icon_rect"].__setitem__(0, 1),
        lambda copy: copy["chapters"][6].__setitem__("reward_group_id", 100007),
        lambda copy: copy.__setitem__("policy", "account_owned"),
        lambda copy: copy["groups"][0]["entries"][0].__setitem__("package_row_id", 999),
    ):
        damaged = copy.deepcopy(doc)
        mutate(damaged)
        try:
            verify_document(damaged, chapter, inventory)
        except (AssertionError, KeyError, ValueError):
            negative += 1
        else:
            raise AssertionError("Source/account corruption was accepted")
    assert negative == 5
    result = {"schema": "rok.task_chapter_reward_preview_verify.v1", "status": "passed",
              "catalog_path": label(CATALOG), "catalog_sha256": sha(CATALOG),
              "groups_checked": 45, "rows_checked": 123, "negative_cases_rejected": negative,
              "chapter_7_aggregate_metadata_known": False,
              "account_state": "unknown", "engine_started": False}
    REPORT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: 45 groups, 123 source rows, {negative} negative cases; {sha(CATALOG)}")


if __name__ == "__main__":
    main()
