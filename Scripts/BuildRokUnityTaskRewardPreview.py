"""Generate a read-only TaskMain reward preview from Unity config rows."""

from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path

from BuildRokUnityEconomyCatalog import localized_text, read_language_lookup

ROOT = Path(__file__).resolve().parents[1]
CSV_ROOT = ROOT / "ref/config-tables/编译客户端配置/CSV"
TASK = CSV_ROOT / "TaskMain.csv"
PACKAGE = CSV_ROOT / "ItemPackageShow.csv"
CURRENCY = CSV_ROOT / "Currency.csv"
LANGUAGE = ROOT / "ref/config-tables/s_Language.xlsm"
SHEET = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.tpsheet"
PROXY = ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/RewardGroupProxy.cs"
MEDIATOR = ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestMediator.cs"
LANGUAGE_PARSER = ROOT / "Scripts/BuildRokUnityEconomyCatalog.py"
OUTPUT = ROOT / "Saved/RokUnityMigration/task_reward_preview.json"
SOURCES = (TASK, PACKAGE, CURRENCY, LANGUAGE, SHEET, PROXY, MEDIATOR, LANGUAGE_PARSER, Path(__file__))


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path: Path) -> dict[int, dict[str, str]]:
    source = list(csv.DictReader(path.read_text(encoding="utf-8-sig").splitlines()))
    assert source and source[0]["ID"] == "int", path
    values: dict[int, dict[str, str]] = {}
    for row in source[1:]:
        key = int(row["ID"])
        assert key not in values, (path, key)
        values[key] = row
    return values


def sheet_sprites() -> tuple[list[int], dict[str, list[int]]]:
    lines = SHEET.read_text(encoding="utf-8-sig").splitlines()
    size = next(line for line in lines if line.startswith(":size=")).removeprefix(":size=")
    width, height = (int(value) for value in size.split("x"))
    sprites: dict[str, list[int]] = {}
    for line in lines:
        if not line.startswith("img_com_cur"):
            continue
        fields = line.split(";")
        sprites[fields[0]] = [int(value) for value in fields[1:5]]
    return [width, height], sprites


def build() -> dict:
    tasks = rows(TASK)
    packages = rows(PACKAGE)
    currencies = rows(CURRENCY)
    language, _ = read_language_lookup()
    atlas_size, sprites = sheet_sprites()
    task_id = 200900
    task = tasks[task_id]
    group_id = int(task["reward"])
    assert group_id == task_id and int(task["type"]) == 8
    assert (int(task["param1"]), int(task["param2"])) == (1, 2)
    entries = []
    serial = 1
    while (package := packages.get(group_id * 1000 + serial)) is not None:
        assert int(package["group"]) == group_id
        assert not package["civilization_limit"].strip(), "Civilization-specific preview requires an account"
        assert int(package["type"]) == 100, "This bounded preview supports source currency rows only"
        currency_id = int(package["typeData"])
        metadata = currencies.get(currency_id)
        assert metadata is not None, currency_id
        icon_key = metadata["iconID"]
        assert icon_key.startswith("ui_common[") and icon_key.endswith("]"), icon_key
        sprite_name = icon_key.removeprefix("ui_common[").removesuffix("]")
        rect = sprites.get(sprite_name)
        assert rect is not None, sprite_name
        locale_id = int(metadata["l_desID"])
        name = localized_text(language, locale_id, "cn")
        assert name, locale_id
        quantity = int(package["number"])
        assert quantity > 0
        entries.append({
            "package_row_id": int(package["ID"]), "kind": "currency", "currency_id": currency_id,
            "quantity": quantity, "name_key": metadata["name"], "locale_id": locale_id,
            "display_name_zh_cn": name, "icon_key": icon_key,
            "icon_rect": rect, "icon_atlas_size": atlas_size,
            "source_random_group": int(package["randomGroup"]), "source_odds": int(package["odds"]),
            "source_show_type": int(package["showType"]), "source_show_group_icon": int(package["showGroupIcon"]),
        })
        serial += 1
    assert len(entries) == 3 and serial == 4
    assert {row_id for row_id, row in packages.items() if int(row["group"]) == group_id} == {
        entry["package_row_id"] for entry in entries}, "Reward group has a noncontiguous row"
    entries.sort(key=lambda entry: entry["currency_id"])
    assert [(entry["currency_id"], entry["quantity"]) for entry in entries] == [
        (100, 2000), (101, 2000), (104, 50)]
    return {
        "schema": "rok.task_reward_preview.v1",
        "scope": "TaskMain 200900 read-only source reward preview; no account inventory or claim state",
        "task_id": task_id, "reward_group_id": group_id,
        "resolver": "RewardGroupProxy.GetRewardDataByGroup(sort:true): contiguous ItemPackageShow IDs, Currency ID order",
        "atlas_asset": "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common",
        "entries": entries,
        "sources": [{"path": path.relative_to(ROOT).as_posix(), "sha256": sha(path)} for path in SOURCES],
    }


def main() -> None:
    result = build()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"{OUTPUT.relative_to(ROOT)}: {len(result['entries'])} source reward entries")


if __name__ == "__main__":
    main()
