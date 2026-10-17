"""Independently compare the generated TaskMain reward preview to Unity source."""

from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path

from BuildRokUnityEconomyCatalog import localized_text, read_language_lookup
from rok_data_paths import DATA_ROOT, resolve

ROOT = Path(__file__).resolve().parents[1]
CATALOG = DATA_ROOT / "task_reward_preview.json"
CSV_ROOT = ROOT / "ref/config-tables/编译客户端配置/CSV"
SHEET = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.tpsheet"


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise AssertionError(reason)


def csv_rows(name: str) -> dict[int, dict[str, str]]:
    with (CSV_ROOT / name).open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        typed = next(reader)
        require(typed["ID"] == "int", f"{name} type line")
        result: dict[int, dict[str, str]] = {}
        for row in reader:
            key = int(row["ID"])
            require(key not in result, f"{name} duplicate ID {key}")
            result[key] = row
        return result


def verify() -> dict:
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    require(catalog.get("schema") == "rok.task_reward_preview.v1", "schema")
    task_id = catalog.get("task_id")
    group = catalog.get("reward_group_id")
    require(task_id == 200900 and group == int(csv_rows("TaskMain.csv")[task_id]["reward"]), "Task reward group")
    source_paths = [entry["path"] for entry in catalog["sources"]]
    require(set(source_paths) == {
        "ref/config-tables/编译客户端配置/CSV/TaskMain.csv",
        "ref/config-tables/编译客户端配置/CSV/ItemPackageShow.csv",
        "ref/config-tables/编译客户端配置/CSV/Currency.csv",
        "ref/config-tables/s_Language.xlsm",
        "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.tpsheet",
        "ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/RewardGroupProxy.cs",
        "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestMediator.cs",
        "Scripts/BuildRokUnityEconomyCatalog.py",
        "Scripts/BuildRokUnityTaskRewardPreview.py",
    }, "source set")
    for entry in catalog["sources"]:
        path = resolve(entry["path"])
        require(path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"],
                f"source hash {path}")
    package = csv_rows("ItemPackageShow.csv")
    currency = csv_rows("Currency.csv")
    source_entries = []
    suffix = 1
    while group * 1000 + suffix in package:
        row = package[group * 1000 + suffix]
        require(int(row["group"]) == group and int(row["type"]) == 100
                and not row["civilization_limit"], "unsupported source row")
        currency_id = int(row["typeData"])
        source_entries.append((int(row["ID"]), currency_id, int(row["number"])))
        suffix += 1
    require(suffix == 4 and len(source_entries) == 3, "contiguous source group size")
    require({row_id for row_id, row in package.items() if int(row["group"]) == group} == {
        row[0] for row in source_entries}, "noncontiguous group row")
    source_entries.sort(key=lambda item: item[1])
    require(source_entries == [(200900002, 100, 2000), (200900003, 101, 2000),
                               (200900001, 104, 50)], "source order/amount")
    manifest_entries = catalog["entries"]
    require(len(manifest_entries) == 3, "catalog count")
    language, _ = read_language_lookup()
    sheet = SHEET.read_text(encoding="utf-8-sig").splitlines()
    size = next(line.split("=", 1)[1] for line in sheet if line.startswith(":size="))
    atlas_size = [int(value) for value in size.split("x")]
    for entry, (row_id, currency_id, amount) in zip(manifest_entries, source_entries):
        metadata = currency[currency_id]
        require(entry["kind"] == "currency" and entry["package_row_id"] == row_id
                and entry["currency_id"] == currency_id and entry["quantity"] == amount,
                "typed source identity/order/quantity")
        package_row = package[row_id]
        require(entry["source_random_group"] == int(package_row["randomGroup"])
                and entry["source_odds"] == int(package_row["odds"])
                and entry["source_show_type"] == int(package_row["showType"])
                and entry["source_show_group_icon"] == int(package_row["showGroupIcon"]),
                "source display provenance")
        require(entry["name_key"] == metadata["name"] and entry["locale_id"] == int(metadata["l_desID"])
                and entry["icon_key"] == metadata["iconID"]
                and entry["display_name_zh_cn"] == localized_text(language, int(metadata["l_desID"]), "cn"),
                "currency metadata")
        icon = metadata["iconID"].split("[", 1)[1].rstrip("]")
        sprite = next(line for line in sheet if line.startswith(icon + ";"))
        rect = [int(value) for value in sprite.split(";")[1:5]]
        require(entry["icon_rect"] == rect and entry["icon_atlas_size"] == atlas_size,
                "atlas sprite geometry")
    require([(entry["locale_id"], entry["icon_key"]) for entry in manifest_entries] == [
        (124012, "ui_common[img_com_cur1]"), (124011, "ui_common[img_com_cur2]"),
        (124034, "ui_common[img_com_cur5]")], "locale/icon contract")
    return {"passed": True, "catalog_sha256": hashlib.sha256(CATALOG.read_bytes()).hexdigest(),
            "entries": len(manifest_entries), "currency_ids": [row[1] for row in source_entries]}


if __name__ == "__main__":
    print(json.dumps(verify(), ensure_ascii=False))
