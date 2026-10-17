"""Build source-only chapter task/aggregate reward previews; never claim account rewards."""

from __future__ import annotations

import csv
import hashlib
import json
import re
from pathlib import Path

from BuildRokUnityEconomyCatalog import localized_text, read_language_lookup

ROOT = Path(__file__).resolve().parents[1]
CSV_ROOT = ROOT / "ref/config-tables/编译客户端配置/CSV"
CHAPTER = ROOT / "Saved/RokUnityMigration/TaskChapterConfigPreview/v2/catalog.json"
INVENTORY = ROOT / "Saved/RokUnityMigration/BatchReview20261009QuestChapterArtAndHallCamera/task_chapter_row_art_inventory_v1.json"
OUTPUT = ROOT / "Saved/RokUnityMigration/TaskChapterRewardPreview/v1/catalog.json"
CHAPTER_SHA = "3f3848e9f8898668ff2a8ebeddeace1dfc1d557f110cb1339c74882c396b7bb5"
INVENTORY_SHA = "8becc5e791af24e8c13b0cd895ef95aaa0c6abe868a9c4eeb163e2b5b08c26d6"
KINDS = {
    100: ("currency", "Currency.csv", "l_desID", "iconID"),
    200: ("item", "Item.csv", "l_nameID", "itemIcon"),
    300: ("soldier", "Arms.csv", "l_armsID", "icon"),
}
ICON_PATTERN = re.compile(r"^([a-z0-9_]+)\[([a-zA-Z0-9_]+)\]$")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def csv_rows(name: str) -> dict[int, dict[str, str]]:
    path = CSV_ROOT / name
    source = list(csv.DictReader(path.read_text(encoding="utf-8-sig").splitlines()))
    assert source and source[0]["ID"] == "int", path
    result: dict[int, dict[str, str]] = {}
    for row in source[1:]:
        key = int(row["ID"])
        assert key > 0 and key not in result, (path, key)
        result[key] = row
    return result


def source_chapter_and_inventory() -> tuple[dict, dict]:
    assert sha(CHAPTER) == CHAPTER_SHA, "sealed chapter source catalog changed"
    assert sha(INVENTORY) == INVENTORY_SHA, "reviewed reward/icon inventory changed"
    chapter = json.loads(CHAPTER.read_text(encoding="utf-8"))
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    assert chapter["schema"] == "rok.task_chapter_config_preview.v2"
    assert chapter["account_state"] == "unknown" and chapter["task_count"] == 39
    assert chapter["chapter_count"] == 7 and chapter["missing_chapter_metadata_ids"] == [7]
    assert inventory["schema"] == "rok.task_chapter_row_art_inventory.v1"
    assert inventory["catalog"]["sha256"] == CHAPTER_SHA
    return chapter, inventory


def atlas_specs(inventory: dict) -> dict[str, dict]:
    specs: dict[str, dict] = {}
    for entry in inventory["static_reward_preview"]["reusable_atlases"]:
        namespace = entry["namespace"]
        assert namespace not in specs
        for field in ("source_png", "source_meta", "source_tpsheet", "ue_uasset"):
            item = entry[field]
            assert sha(ROOT / item["path"]) == item["sha256"], item["path"]
        sheet_path = ROOT / entry["source_tpsheet"]["path"]
        lines = sheet_path.read_text(encoding="utf-8-sig").splitlines()
        width, height = (int(value) for value in next(
            line for line in lines if line.startswith(":size=")).split("=", 1)[1].split("x"))
        assert [width, height] == entry["atlas_size"]
        sprites: dict[str, list[int]] = {}
        for line in lines:
            if not line or line.startswith(("#", ":")) or ";" not in line:
                continue
            fields = [field.strip() for field in line.split(";")]
            if len(fields) < 5:
                continue
            name = fields[0]
            rect = [int(value) for value in fields[1:5]]
            assert name not in sprites and rect[2] > 0 and rect[3] > 0
            assert rect[0] >= 0 and rect[1] >= 0 and rect[0] + rect[2] <= width
            assert rect[1] + rect[3] <= height
            sprites[name] = rect
        ue_target = entry["ue_target"]
        specs[namespace] = {
            "size": [width, height], "sprites": sprites,
            "asset": f"{ue_target}.{ue_target.rsplit('/', 1)[1]}",
        }
    assert set(specs) == {"item2", "item3", "item5", "troops_head", "ui_common"}
    return specs


def source_paths(inventory: dict) -> list[Path]:
    paths = [CHAPTER, INVENTORY]
    paths.extend(CSV_ROOT / name for name in (
        "TaskChapter.csv", "TaskChapterData.csv", "ItemPackageShow.csv",
        "Currency.csv", "Item.csv", "Arms.csv"))
    paths.extend((
        ROOT / "ref/config-tables/s_Language.xlsm",
        ROOT / "Scripts/BuildRokUnityEconomyCatalog.py",
        ROOT / "Scripts/ExtractRokUnityConfigTables.py",
        ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/RewardGroupProxy.cs",
        ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestMediator.cs",
    ))
    for entry in inventory["static_reward_preview"]["reusable_atlases"]:
        paths.extend(ROOT / entry[field]["path"] for field in
                     ("source_png", "source_meta", "source_tpsheet", "ue_uasset"))
    paths.append(Path(__file__))
    assert len(paths) == len(set(paths))
    return paths


def build() -> dict:
    chapter, inventory = source_chapter_and_inventory()
    atlas = atlas_specs(inventory)
    packages = csv_rows("ItemPackageShow.csv")
    metadata = {kind: csv_rows(spec[1]) for kind, spec in KINDS.items()}
    language, _ = read_language_lookup()
    chapter_refs = {row["id"]: row for row in chapter["chapters"]}
    assert set(chapter_refs) == set(range(1, 8))
    task_refs = chapter["tasks"]
    assert len(task_refs) == 39
    chapter_catalog = []
    requested_groups: list[tuple[str, int | None, int, int, bool]] = []
    for row in chapter["chapters"]:
        chapter_id = row["id"]
        group_id = row["reward_group_id"]
        known = chapter_id != 7
        assert (group_id is not None) == known
        chapter_catalog.append({
            "chapter_id": chapter_id,
            "metadata_known": known,
            "reward_group_id": group_id,
        })
        if known:
            requested_groups.append(("chapter", None, chapter_id, group_id, known))
    for task in task_refs:
        requested_groups.append(("task", task["id"], task["chapter_id"],
                                 task["reward_group_id"], task["chapter_id"] != 7))
    assert len(requested_groups) == 45
    assert len({entry[3] for entry in requested_groups}) == 45

    selected_ids = {entry[3] for entry in requested_groups}
    selected_packages: dict[int, list[dict[str, str]]] = {group: [] for group in selected_ids}
    for package in packages.values():
        group_id = int(package["group"])
        if group_id in selected_packages:
            selected_packages[group_id].append(package)
    groups = []
    icon_occurrences: dict[str, int] = {}
    for owner_kind, task_id, chapter_id, group_id, metadata_known in requested_groups:
        source_rows = sorted(selected_packages[group_id], key=lambda row: int(row["ID"]))
        assert source_rows and [int(row["ID"]) for row in source_rows] == [
            group_id * 1000 + order for order in range(1, len(source_rows) + 1)]
        entries = []
        for order, package in enumerate(source_rows, 1):
            assert not package["civilization_limit"].strip(), "account civilization required"
            reward_type = int(package["type"])
            assert reward_type in KINDS, (group_id, reward_type)
            kind, _, locale_field, icon_field = KINDS[reward_type]
            type_data = int(package["typeData"])
            type_meta = metadata[reward_type].get(type_data)
            assert type_meta is not None, (reward_type, type_data)
            quantity = int(package["number"])
            assert 0 < quantity <= 2**63 - 1
            locale_id = int(type_meta[locale_field])
            display_name = localized_text(language, locale_id, "cn")
            assert locale_id > 0 and display_name, (reward_type, type_data, locale_id)
            icon_key = type_meta[icon_field]
            match = ICON_PATTERN.fullmatch(icon_key)
            assert match, icon_key
            namespace, sprite = match.groups()
            sheet = atlas.get(namespace)
            assert sheet and sprite in sheet["sprites"], icon_key
            icon_occurrences[icon_key] = icon_occurrences.get(icon_key, 0) + 1
            entries.append({
                "package_row_id": int(package["ID"]),
                "reward_group_id": group_id,
                "reward_type": reward_type,
                "kind": kind,
                "type_data": type_data,
                "quantity": quantity,
                "locale_id": locale_id,
                "display_name_zh_cn": display_name,
                "icon_key": icon_key,
                "atlas_object_path": sheet["asset"],
                "icon_rect": sheet["sprites"][sprite],
                "icon_atlas_size": sheet["size"],
                "stable_order": order,
            })
        groups.append({
            "owner_kind": owner_kind,
            "task_id": task_id,
            "chapter_id": chapter_id,
            "reward_group_id": group_id,
            "metadata_known": metadata_known,
            "entries": entries,
        })
    assert sum(len(group["entries"]) for group in groups) == 123
    assert sum(len(group["entries"]) for group in groups if group["owner_kind"] == "task") == 105
    assert len(icon_occurrences) == 19
    assert icon_occurrences == inventory["static_reward_preview"]["resolved_icon_occurrences_by_key"]
    return {
        "schema": "rok.task_chapter_reward_preview.v1",
        "policy": "source_configuration_only_no_account_progress_ownership_claim_or_roll",
        "chapter_catalog_sha256": CHAPTER_SHA,
        "task_count": 39,
        "chapter_count": 7,
        "task_group_count": 39,
        "chapter_aggregate_group_count": 6,
        "group_count": 45,
        "reward_row_count": 123,
        "chapters": chapter_catalog,
        "groups": groups,
        "sources": [{"path": path.relative_to(ROOT).as_posix(), "sha256": sha(path)}
                    for path in source_paths(inventory)],
    }


def main() -> None:
    result = build()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes((json.dumps(result, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    print(f"{OUTPUT.relative_to(ROOT)}: {result['group_count']} groups, {result['reward_row_count']} rows")


if __name__ == "__main__":
    main()
