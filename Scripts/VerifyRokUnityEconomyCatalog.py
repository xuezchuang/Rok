from __future__ import annotations

import csv
import hashlib
import json
import re
from collections import Counter
from pathlib import Path
from typing import Any

from ExtractRokUnityConfigTables import parse_table


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
ITEM_CSV = PROJECT_ROOT / "ref" / "config-tables" / "编译客户端配置" / "CSV" / "Item.csv"
RECHARGE_LIST_CSV = PROJECT_ROOT / "ref" / "config-tables" / "编译客户端配置" / "CSV" / "RechargeList.csv"
RECHARGE_GEM_MALL_CSV = PROJECT_ROOT / "ref" / "config-tables" / "编译客户端配置" / "CSV" / "RechargeGemMall.csv"
LANGUAGE_WORKBOOK = PROJECT_ROOT / "ref" / "config-tables" / "s_Language.xlsm"
CATALOG_PATH = MIGRATION_ROOT / "unity_economy_catalog.json"
CATALOG_MD_PATH = MIGRATION_ROOT / "unity_economy_catalog.md"
OUTPUT_PATH = MIGRATION_ROOT / "unity_economy_catalog_verify.json"

STORE_PROXY = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "Scripts" / "Hotfix" / "MVC" / "Proxy" / "StoreProxy.cs"
STORE_MEDIATOR = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "Scripts" / "Hotfix" / "MVC" / "View_Mediator" / "Store" / "StoreMediator.cs"
SPEED_MEDIATOR = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "Scripts" / "Hotfix" / "MVC" / "View_Mediator" / "SpeedUp" / "AddSpeedMediator.cs"
ENUM_CONSTANT = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "Scripts" / "Hotfix" / "MVC" / "CMD" / "EnumConstant.cs"
CONFIGS_DATA = PROJECT_ROOT / "ref" / "config-tables" / "gen" / "Configs.data"
RECHARGE_PROXY = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "Scripts" / "Hotfix" / "MVC" / "Proxy" / "RechargeProxy.cs"
CHARGE_GEM_SHOP_ITEM_VIEW = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "Scripts" / "Hotfix" / "MVC" / "View_Mediator" / "Common" / "Logic" / "UI_Item_ChargeGemShopItem_SubView.cs"
ITEM_ATLAS_ROOT = (
    PROJECT_ROOT
    / "ref"
    / "client-unity"
    / "Assets"
    / "BundleAssets"
    / "UI"
    / "UITextureStatic"
    / "ui_res_item"
)
ITEM_ATLAS_NAMES = tuple(f"item{index}" for index in range(1, 6))

RECHARGE_LIST_HEADERS = [
    "ID",
    "l_pagingID",
    "pagingType",
    "preposition",
    "timeType",
    "startTime",
    "durationTime",
    "sort",
    "icon",
]
RECHARGE_GEM_MALL_HEADERS = [
    "ID",
    "l_nameID",
    "price",
    "denarNum",
    "firstPresenter",
    "presenter",
    "gemIcon",
    "rechargeProgress",
    "mail",
]

SHOP_CURRENCY_TYPE_ID = 104
SPEED_APPLICABILITY_BY_SUB_TYPE = {
    20101: "common",
    20102: "building",
    20103: "training",
    20104: "research",
    20105: "healing",
}

EXPECTED_BASELINE = {
    "item_count": 616,
    "localized_name_count": 614,
    "unresolved_name_count": 2,
    "icon_count": 615,
    "resolved_icon_sprite_count": 425,
    "shop_item_count": 81,
    "shop_category_count": 4,
    "speed_item_count": 65,
    "acquisition_source_item_count": 206,
    "charge_page_count": 7,
    "charge_paging_type_count": 7,
    "charge_gem_package_count": 6,
}
EXPECTED_SHOP_CATEGORY_COUNTS = {1: 35, 2: 10, 3: 24, 5: 12}
EXPECTED_SPEED_COUNTS = {
    "building": 13,
    "common": 13,
    "healing": 13,
    "research": 13,
    "training": 13,
}
EXPECTED_CHARGE_PAGING_TYPES = set(range(101, 108))
ALLIANCE_CONFIG_KEYS = (
    "allianceAbbreviationMin",
    "allianceAbbreviationMax",
    "allianceNameMin",
    "allianceNameMax",
    "allianceNoticeNum",
    "allianceEstablishCost",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_named_config_ints() -> dict[str, int]:
    source_text = CONFIGS_DATA.read_text(encoding="utf-8-sig", errors="strict")
    values: dict[str, int] = {}
    for key in ALLIANCE_CONFIG_KEYS:
        matches = re.findall(rf"\b{re.escape(key)}\s*=\s*(-?\d+)\b", source_text)
        unique_values = {int(value) for value in matches}
        if len(unique_values) != 1:
            raise RuntimeError(
                f"Expected one stable integer value for {key}, got {sorted(unique_values)}"
            )
        values[key] = unique_values.pop()
    return values


def int_value(row: dict[str, str], field: str) -> int:
    raw = row.get(field, "").strip()
    return int(raw) if raw else 0


def int_list_value(raw: str) -> list[int]:
    return [
        int(part)
        for part in raw.replace(",", "|").split("|")
        if part.strip()
    ]


def read_source_rows() -> list[dict[str, str]]:
    with ITEM_CSV.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        next(reader)
        rows = []
        for source_row, row in enumerate(reader, start=3):
            if not any((value or "").strip() for value in row.values()):
                continue
            row["__source_row"] = str(source_row)
            rows.append(row)
        return rows


def read_typed_source_rows(
    path: Path,
    expected_headers: list[str],
) -> tuple[list[dict[str, str]], list[str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        headers = next(reader)
        types = next(reader)
        if headers != expected_headers:
            raise RuntimeError(f"Unexpected {path.name} headers: {headers}")
        if len(types) != len(headers):
            raise RuntimeError(
                f"{path.name} type row has {len(types)} columns; expected {len(headers)}"
            )

        rows: list[dict[str, str]] = []
        for source_row, values in enumerate(reader, start=3):
            if not any(value.strip() for value in values):
                continue
            if len(values) != len(headers):
                raise RuntimeError(
                    f"{path.name} row {source_row} has {len(values)} columns; expected {len(headers)}"
                )
            row = dict(zip(headers, values))
            row["__source_row"] = str(source_row)
            rows.append(row)
        return rows, types


def expected_charge_page(row: dict[str, str]) -> dict[str, Any]:
    return {
        "id": int_value(row, "ID"),
        "paging_locale_id": int_value(row, "l_pagingID"),
        "paging_type": int_value(row, "pagingType"),
        "preposition": int_value(row, "preposition"),
        "sort": int_value(row, "sort"),
        "icon": row.get("icon", "").strip(),
        "source": {
            "path": "ref/config-tables/编译客户端配置/CSV/RechargeList.csv",
            "row": int(row["__source_row"]),
        },
    }


def expected_charge_gem_package(row: dict[str, str]) -> dict[str, Any]:
    return {
        "id": int_value(row, "ID"),
        "name_locale_id": int_value(row, "l_nameID"),
        "product_id": int_value(row, "price"),
        "denar_num": int_value(row, "denarNum"),
        "first_presenter": int_value(row, "firstPresenter"),
        "presenter": int_value(row, "presenter"),
        "gem_icon": row.get("gemIcon", "").strip(),
        "recharge_progress": int_value(row, "rechargeProgress"),
        "mail": int_value(row, "mail"),
        "source": {
            "path": "ref/config-tables/编译客户端配置/CSV/RechargeGemMall.csv",
            "row": int(row["__source_row"]),
        },
    }


def read_language_lookup() -> dict[int, dict[str, Any]]:
    table = parse_table(LANGUAGE_WORKBOOK, PROJECT_ROOT / "ref")
    return {
        int(row["id"]): row
        for row in table["rows"]
        if isinstance(row.get("id"), int) and row["id"] > 0
    }


def localized_text(language_by_id: dict[int, dict[str, Any]], locale_id: int, field: str) -> str:
    if locale_id <= 0:
        return ""
    return str(language_by_id.get(locale_id, {}).get(field, "") or "")


def read_enabled_shop_category_types() -> tuple[int, ...]:
    source_text = STORE_PROXY.read_text(encoding="utf-8-sig", errors="strict")
    category_types = {
        int(match)
        for match in re.findall(
            r"m_showTypeDic\s*\[\s*(\d+)\s*\]\s*=\s*true",
            source_text,
        )
    }
    if not category_types:
        raise RuntimeError(f"No enabled normal-shop Item.type values found in {STORE_PROXY}")
    return tuple(sorted(category_types))


def read_item_atlas_sprites() -> dict[tuple[str, str], dict[str, Any]]:
    sprites: dict[tuple[str, str], dict[str, Any]] = {}
    for atlas_name in ITEM_ATLAS_NAMES:
        path = ITEM_ATLAS_ROOT / f"{atlas_name}.tpsheet"
        lines = path.read_text(encoding="utf-8-sig").splitlines()
        size_line = next(
            (line for line in lines if line.startswith(":size=")),
            "",
        )
        size_match = re.fullmatch(r":size=(\d+)x(\d+)", size_line)
        if not size_match:
            raise RuntimeError(f"Unable to parse atlas size from {path}")
        atlas_size = [int(size_match.group(1)), int(size_match.group(2))]
        for line in lines:
            if not line or line.startswith(("#", ":")):
                continue
            parts = [part.strip() for part in line.split(";")]
            sprites[(atlas_name, parts[0])] = {
                "rect": [int(parts[index]) for index in range(1, 5)],
                "atlas_size": atlas_size,
            }
    return sprites


def expected_icon_fields(
    icon_reference: str,
    atlas_sprites: dict[tuple[str, str], dict[str, Any]],
) -> dict[str, Any]:
    match = re.fullmatch(r"([^\[]+)\[([^\]]+)\]", icon_reference)
    if not match:
        return {
            "icon_atlas": "",
            "icon_sprite": "",
            "icon_rect": [],
            "icon_atlas_size": [],
        }
    atlas_name, sprite_name = match.groups()
    resolved = atlas_sprites.get((atlas_name, sprite_name))
    return {
        "icon_atlas": atlas_name if resolved else "",
        "icon_sprite": sprite_name,
        "icon_rect": list(resolved["rect"]) if resolved else [],
        "icon_atlas_size": list(resolved["atlas_size"]) if resolved else [],
    }


def check_unity_source_contracts(
    failures: list[str],
    shop_category_types: tuple[int, ...],
) -> dict[str, bool]:
    source_text = {
        "store_proxy": STORE_PROXY.read_text(encoding="utf-8-sig", errors="replace"),
        "store_mediator": STORE_MEDIATOR.read_text(encoding="utf-8-sig", errors="replace"),
        "speed_mediator": SPEED_MEDIATOR.read_text(encoding="utf-8-sig", errors="replace"),
        "enum_constant": ENUM_CONSTANT.read_text(encoding="utf-8-sig", errors="replace"),
        "recharge_proxy": RECHARGE_PROXY.read_text(encoding="utf-8-sig", errors="replace"),
        "charge_gem_shop_item_view": CHARGE_GEM_SHOP_ITEM_VIEW.read_text(
            encoding="utf-8-sig",
            errors="replace",
        ),
    }
    checks: dict[str, bool] = {
        "shop_price_gate": bool(re.search(r"shopPrice\s*>\s*0", source_text["store_proxy"])),
        "shop_purchase_request": "Shop_BuyShopItem.request" in source_text["store_mediator"],
        "shop_currency_enum_use": "EnumCurrencyType.denar" in source_text["store_mediator"],
        "shop_currency_id_104": bool(
            re.search(r"\bdenar\s*=\s*104\b", source_text["enum_constant"])
        ),
        "speed_duration_data1": bool(
            re.search(r"m_itemList\s*\[[^\]]+\]\.data1", source_text["speed_mediator"])
        ),
        "speed_service_request": "Role_SpeedUp.request" in source_text["speed_mediator"],
        "charge_page_sort": bool(
            re.search(
                r"ItemCfg\.Sort\s*\(\s*\(a,b\).*?a\.sort\.CompareTo\(b\.sort\)",
                source_text["recharge_proxy"],
                re.DOTALL,
            )
        ),
        "charge_product_price_lookup": bool(
            re.search(
                r"QueryRecord<Data\.PriceDefine>\s*\(\s*cfgData\.price\s*\)",
                source_text["charge_gem_shop_item_view"],
            )
        ),
        "charge_platform_purchase": "CallSdkBuyByPcid" in source_text["charge_gem_shop_item_view"]
        and "IGGPayment.shareInstance().buyItem" in source_text["recharge_proxy"],
    }
    for category_type in shop_category_types:
        checks[f"shop_category_{category_type}"] = bool(
            re.search(
                rf"m_showTypeDic\s*\[\s*{category_type}\s*\]\s*=\s*true",
                source_text["store_proxy"],
            )
        )
    enum_names = {
        20101: "common",
        20102: "buiding",
        20103: "train",
        20104: "research",
        20105: "heal",
    }
    for sub_type, enum_name in enum_names.items():
        checks[f"speed_sub_type_{sub_type}"] = bool(
            re.search(
                rf"\b{enum_name}\s*=\s*{sub_type}\b",
                source_text["speed_mediator"],
            )
        )
    for name, passed in checks.items():
        if not passed:
            failures.append(f"Unity source contract check failed: {name}")
    return checks


def main() -> int:
    failures: list[str] = []
    if not CATALOG_PATH.exists():
        raise RuntimeError(f"Missing economy catalog: {CATALOG_PATH}")

    catalog = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    source_rows = read_source_rows()
    recharge_list_rows, recharge_list_types = read_typed_source_rows(
        RECHARGE_LIST_CSV,
        RECHARGE_LIST_HEADERS,
    )
    recharge_gem_mall_rows, recharge_gem_mall_types = read_typed_source_rows(
        RECHARGE_GEM_MALL_CSV,
        RECHARGE_GEM_MALL_HEADERS,
    )
    language_by_id = read_language_lookup()
    shop_category_types = read_enabled_shop_category_types()
    atlas_sprites = read_item_atlas_sprites()
    alliance_config = read_named_config_ints()
    items = catalog.get("items", [])
    charge_pages = catalog.get("charge_pages", [])
    charge_gem_packages = catalog.get("charge_gem_packages", [])
    summary = catalog.get("summary", {})

    if catalog.get("schema") != "rok.unity_economy_catalog.v1":
        failures.append(f"Unexpected schema: {catalog.get('schema')!r}")
    if not CATALOG_MD_PATH.exists():
        failures.append(f"Missing economy catalog markdown: {CATALOG_MD_PATH}")

    if len(source_rows) != EXPECTED_BASELINE["item_count"]:
        failures.append(
            f"Current Item.csv row count expected {EXPECTED_BASELINE['item_count']}, got {len(source_rows)}"
        )
    if len(items) != len(source_rows):
        failures.append(
            f"Catalog/source item count mismatch: catalog={len(items)} source={len(source_rows)}"
        )

    source_ids = [int_value(row, "ID") for row in source_rows]
    catalog_ids = [int(item.get("id", 0)) for item in items]
    if catalog_ids != source_ids:
        failures.append("Catalog item IDs or ordering do not exactly match Item.csv")
    if len(catalog_ids) != len(set(catalog_ids)):
        failures.append("Catalog contains duplicate item IDs")

    sources = catalog.get("sources", {})
    item_source = sources.get("item_csv", {})
    recharge_list_source = sources.get("recharge_list_csv", {})
    recharge_gem_mall_source = sources.get("recharge_gem_mall_csv", {})
    language_source = sources.get("language_workbook", {})
    if item_source.get("sha256") != sha256(ITEM_CSV):
        failures.append("Item.csv SHA-256 does not match catalog evidence")
    if language_source.get("sha256") != sha256(LANGUAGE_WORKBOOK):
        failures.append("s_Language.xlsm SHA-256 does not match catalog evidence")
    if recharge_list_source.get("sha256") != sha256(RECHARGE_LIST_CSV):
        failures.append("RechargeList.csv SHA-256 does not match catalog evidence")
    if recharge_gem_mall_source.get("sha256") != sha256(RECHARGE_GEM_MALL_CSV):
        failures.append("RechargeGemMall.csv SHA-256 does not match catalog evidence")
    if recharge_list_source.get("headers") != RECHARGE_LIST_HEADERS:
        failures.append("RechargeList.csv header contract mismatch")
    if recharge_list_source.get("types") != recharge_list_types:
        failures.append("RechargeList.csv type contract mismatch")
    if recharge_gem_mall_source.get("headers") != RECHARGE_GEM_MALL_HEADERS:
        failures.append("RechargeGemMall.csv header contract mismatch")
    if recharge_gem_mall_source.get("types") != recharge_gem_mall_types:
        failures.append("RechargeGemMall.csv type contract mismatch")

    item_by_id = {int(item.get("id", 0)): item for item in items}
    for row in source_rows:
        item_id = int_value(row, "ID")
        item = item_by_id.get(item_id)
        if item is None:
            failures.append(f"Missing catalog item {item_id}")
            continue

        name_locale_id = int_value(row, "l_nameID")
        type_description_locale_id = int_value(row, "l_typeDes")
        description_locale_id = int_value(row, "l_desID")
        icon_reference = row.get("itemIcon", "").strip()
        expected_fields = {
            "name_locale_id": name_locale_id,
            "display_name_zh_cn": localized_text(language_by_id, name_locale_id, "cn"),
            "display_name_en": localized_text(language_by_id, name_locale_id, "en"),
            "tips_locale_id": int_value(row, "l_tipsID"),
            "sub_type": int_value(row, "subType"),
            "type": int_value(row, "type"),
            "type_description_locale_id": type_description_locale_id,
            "type_description_zh_cn": localized_text(
                language_by_id, type_description_locale_id, "cn"
            ),
            "type_group": int_value(row, "typeGroup"),
            "icon": icon_reference,
            **expected_icon_fields(icon_reference, atlas_sprites),
            "required_level": int_value(row, "lv"),
            "quality": int_value(row, "quality"),
            "batch_use": int_value(row, "batchUse ") != 0,
            "button_locale_id": int_value(row, "l_buttonDes"),
            "item_function": int_value(row, "itemFunction"),
            "data1": int_value(row, "data1"),
            "data2": int_value(row, "data2"),
            "description_locale_id": description_locale_id,
            "description_template_zh_cn": localized_text(
                language_by_id, description_locale_id, "cn"
            ),
            "description_template_en": localized_text(
                language_by_id, description_locale_id, "en"
            ),
            "description_data1": int_value(row, "desData1"),
            "description_data2": int_value(row, "desData2"),
            "top_locale_id": int_value(row, "l_topID"),
            "top_data": int_value(row, "topData"),
            "acquisition_source_ids": int_list_value(row.get("get", "")),
            "shortcut_price": int_value(row, "shortcutPrice"),
            "shop_price": int_value(row, "shopPrice"),
            "red_dot_prompt": int_value(row, "redDotPrompt"),
            "rank": int_value(row, "rank"),
        }
        for field, expected in expected_fields.items():
            if item.get(field) != expected:
                failures.append(
                    f"Item {item_id} field {field} expected {expected!r}, got {item.get(field)!r}"
                )

        item_type = int_value(row, "type")
        shop_price = int_value(row, "shopPrice")
        expected_shop_item = shop_price > 0 and item_type in shop_category_types
        if item.get("is_normal_shop_item") is not expected_shop_item:
            failures.append(f"Item {item_id} normal-shop eligibility mismatch")
        expected_currency = SHOP_CURRENCY_TYPE_ID if expected_shop_item else 0
        if item.get("shop_currency_type_id") != expected_currency:
            failures.append(
                f"Item {item_id} shop currency expected {expected_currency}, got {item.get('shop_currency_type_id')}"
            )

        source = item.get("source", {})
        if source.get("path") != "ref/config-tables/编译客户端配置/CSV/Item.csv":
            failures.append(f"Item {item_id} source path mismatch")
        if source.get("row") != int(row["__source_row"]):
            failures.append(f"Item {item_id} source row mismatch")

        sub_type = int_value(row, "subType")
        data1 = int_value(row, "data1")
        expected_applicability = SPEED_APPLICABILITY_BY_SUB_TYPE.get(sub_type)
        if expected_applicability and data1 > 0:
            if item.get("speed_duration_seconds") != data1:
                failures.append(f"Item {item_id} speed duration must equal Item.data1")
            if item.get("speed_applicability") != expected_applicability:
                failures.append(f"Item {item_id} speed applicability mismatch")
        elif "speed_duration_seconds" in item or "speed_applicability" in item:
            failures.append(f"Item {item_id} has unsupported derived speed fields")

    expected_charge_pages = sorted(
        (expected_charge_page(row) for row in recharge_list_rows),
        key=lambda page: (page["sort"], page["id"]),
    )
    if charge_pages != expected_charge_pages:
        failures.append(
            "charge_pages do not exactly match RechargeList.csv fields, source rows, and sort order"
        )
    charge_page_ids = [int(page.get("id", 0)) for page in charge_pages]
    charge_paging_types = [int(page.get("paging_type", 0)) for page in charge_pages]
    charge_page_sorts = [int(page.get("sort", 0)) for page in charge_pages]
    if len(charge_pages) != 7:
        failures.append(f"Expected 7 charge pages, got {len(charge_pages)}")
    if set(charge_paging_types) != EXPECTED_CHARGE_PAGING_TYPES:
        failures.append(
            f"Expected pagingType values 101..107, got {charge_paging_types}"
        )
    if charge_page_sorts != sorted(charge_page_sorts):
        failures.append(f"Charge pages are not ordered by sort: {charge_page_sorts}")
    if len(charge_page_sorts) != len(set(charge_page_sorts)):
        failures.append(f"Charge page sort values are not unique: {charge_page_sorts}")
    if len(charge_page_ids) != len(set(charge_page_ids)):
        failures.append("Charge pages contain duplicate IDs")

    charge_page_by_type = {
        int(page.get("paging_type", 0)): page for page in charge_pages
    }
    for page in charge_pages:
        page_id = int(page.get("id", 0))
        paging_type = int(page.get("paging_type", 0))
        preposition = int(page.get("preposition", 0))
        if (
            page_id <= 0
            or int(page.get("paging_locale_id", 0)) <= 0
            or paging_type <= 0
            or int(page.get("sort", 0)) <= 0
            or not str(page.get("icon", ""))
        ):
            failures.append(f"Charge page {page_id} has invalid key fields")
        if preposition > 0:
            prerequisite = charge_page_by_type.get(preposition)
            if prerequisite is None:
                failures.append(
                    f"Charge page {page_id} references missing preposition pagingType {preposition}"
                )
            elif int(prerequisite.get("sort", 0)) >= int(page.get("sort", 0)):
                failures.append(
                    f"Charge page {page_id} preposition {preposition} must sort earlier"
                )

    expected_charge_gem_packages = sorted(
        (expected_charge_gem_package(row) for row in recharge_gem_mall_rows),
        key=lambda package: package["id"],
    )
    if charge_gem_packages != expected_charge_gem_packages:
        failures.append(
            "charge_gem_packages do not exactly match RechargeGemMall.csv fields and source rows"
        )
    if len(charge_gem_packages) < 6:
        failures.append(
            f"Expected at least 6 charge gem packages, got {len(charge_gem_packages)}"
        )
    charge_package_ids = [
        int(package.get("id", 0)) for package in charge_gem_packages
    ]
    charge_product_ids = [
        int(package.get("product_id", 0)) for package in charge_gem_packages
    ]
    if len(charge_package_ids) != len(set(charge_package_ids)):
        failures.append("Charge gem packages contain duplicate IDs")
    if len(charge_product_ids) != len(set(charge_product_ids)):
        failures.append("Charge gem packages contain duplicate product IDs")
    for package in charge_gem_packages:
        package_id = int(package.get("id", 0))
        if (
            package_id <= 0
            or int(package.get("name_locale_id", 0)) <= 0
            or int(package.get("product_id", 0)) <= 0
            or int(package.get("denar_num", 0)) <= 0
            or int(package.get("first_presenter", -1)) < 0
            or int(package.get("presenter", -1)) < 0
            or not str(package.get("gem_icon", ""))
            or int(package.get("recharge_progress", 0)) <= 0
            or int(package.get("mail", 0)) <= 0
        ):
            failures.append(f"Charge gem package {package_id} has invalid key fields")
        if any(
            forbidden_field in package
            for forbidden_field in ("price", "cash_price", "display_price")
        ):
            failures.append(
                f"Charge gem package {package_id} misrepresents product_id as a price"
            )

    computed_shop_items = [item for item in items if item.get("is_normal_shop_item")]
    computed_speed_items = [
        item for item in items if int(item.get("speed_duration_seconds", 0)) > 0
    ]
    computed_summary = {
        "item_count": len(items),
        "localized_name_count": sum(bool(item.get("display_name_zh_cn")) for item in items),
        "unresolved_name_count": sum(
            not bool(item.get("display_name_zh_cn")) for item in items
        ),
        "icon_count": sum(bool(item.get("icon")) for item in items),
        "resolved_icon_sprite_count": sum(
            bool(item.get("icon_rect")) for item in items
        ),
        "shop_item_count": len(computed_shop_items),
        "shop_category_count": len({int(item.get("type", 0)) for item in computed_shop_items}),
        "speed_item_count": len(computed_speed_items),
        "acquisition_source_item_count": sum(
            bool(item.get("acquisition_source_ids")) for item in items
        ),
        "charge_page_count": len(charge_pages),
        "charge_paging_type_count": len(set(charge_paging_types)),
        "charge_gem_package_count": len(charge_gem_packages),
    }
    for key, expected in EXPECTED_BASELINE.items():
        actual = computed_summary.get(key)
        if actual != expected:
            failures.append(f"Expected current {key}={expected}, got {actual}")
        if summary.get(key) != actual:
            failures.append(
                f"Summary {key}={summary.get(key)} does not match computed {actual}"
            )

    shop_category_counts = Counter(
        int(item.get("type", 0)) for item in computed_shop_items
    )
    if dict(shop_category_counts) != EXPECTED_SHOP_CATEGORY_COUNTS:
        failures.append(
            f"Expected shop category counts {EXPECTED_SHOP_CATEGORY_COUNTS}, got {dict(shop_category_counts)}"
        )
    summary_shop_counts = {
        int(key): int(value)
        for key, value in summary.get("shop_category_counts", {}).items()
    }
    if summary_shop_counts != dict(shop_category_counts):
        failures.append("Summary shop category counts do not match catalog items")

    speed_counts = Counter(
        str(item.get("speed_applicability", "")) for item in computed_speed_items
    )
    if dict(speed_counts) != EXPECTED_SPEED_COUNTS:
        failures.append(
            f"Expected speed applicability counts {EXPECTED_SPEED_COUNTS}, got {dict(speed_counts)}"
        )
    if summary.get("speed_applicability_counts") != dict(sorted(speed_counts.items())):
        failures.append("Summary speed applicability counts do not match catalog items")

    contracts = catalog.get("contracts", {})
    normal_shop_contract = contracts.get("normal_shop", {})
    if normal_shop_contract.get("item_type_categories") != list(shop_category_types):
        failures.append("Normal-shop category contract mismatch")
    if normal_shop_contract.get("currency_type_id") != SHOP_CURRENCY_TYPE_ID:
        failures.append("Normal-shop currency contract mismatch")
    speed_contract = contracts.get("speed_items", {})
    if speed_contract.get("duration_field") != "data1":
        failures.append("Speed duration-field contract must be Item.data1")
    if speed_contract.get("applicability_field") != "subType":
        failures.append("Speed applicability-field contract must be Item.subType")
    charge_contract = contracts.get("charge_ui", {})
    if charge_contract.get("page_order_field") != "RechargeList.sort":
        failures.append("Charge page-order contract must be RechargeList.sort")
    if charge_contract.get("ordered_paging_types") != charge_paging_types:
        failures.append("Charge ordered paging-type contract mismatch")
    if charge_contract.get("gem_package_product_id_field") != "product_id":
        failures.append("Charge gem package contract must expose product_id")
    if charge_contract.get("product_id_source_field") != "RechargeGemMall.price":
        failures.append("Charge product_id source contract mismatch")
    product_id_semantics = str(charge_contract.get("product_id_semantics", ""))
    if "lookup key" not in product_id_semantics or "not a cash amount" not in product_id_semantics:
        failures.append("Charge product_id semantics must reject cash-price interpretation")
    purchase_boundary = str(charge_contract.get("purchase_boundary", ""))
    if "IGGPayment" not in purchase_boundary or "server" not in purchase_boundary:
        failures.append("Charge purchase contract must retain platform/server authority")

    alliance_contract = contracts.get("alliance_ui", {})
    expected_alliance_contract = {
        "establish_cost": alliance_config["allianceEstablishCost"],
        "currency_type_id": SHOP_CURRENCY_TYPE_ID,
        "name_min_characters": alliance_config["allianceNameMin"],
        "name_max_characters": alliance_config["allianceNameMax"],
        "tag_min_characters": alliance_config["allianceAbbreviationMin"],
        "tag_max_characters": alliance_config["allianceAbbreviationMax"],
        "notice_max_characters": alliance_config["allianceNoticeNum"],
        "search_min_characters": 3,
        "search_service_request": "Guild_SearchGuild",
        "join_service_request": "Guild_ApplyJoinGuild",
        "create_service_request": "Guild_CreateGuild",
        "authority": "server",
    }
    if alliance_contract != expected_alliance_contract:
        failures.append(
            "Alliance UI contract does not match Configs.data and Unity service boundaries"
        )

    source_contract_checks = check_unity_source_contracts(
        failures,
        shop_category_types,
    )
    result = {
        "schema": "rok.unity_economy_catalog_verify.v1",
        "catalog": str(CATALOG_PATH.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "source": str(ITEM_CSV.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "expected_baseline": EXPECTED_BASELINE,
        "computed_summary": computed_summary,
        "enabled_shop_category_types": list(shop_category_types),
        "shop_category_counts": {
            str(key): shop_category_counts[key] for key in sorted(shop_category_counts)
        },
        "speed_applicability_counts": dict(sorted(speed_counts.items())),
        "ordered_charge_paging_types": charge_paging_types,
        "charge_gem_product_ids": charge_product_ids,
        "unity_source_contract_checks": source_contract_checks,
        "failure_count": len(failures),
        "failures": failures,
        "passed": not failures,
    }
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    print("RokUnityEconomyCatalogVerify")
    print(
        f"items={computed_summary['item_count']} "
        f"localized={computed_summary['localized_name_count']} "
        f"shop={computed_summary['shop_item_count']} "
        f"speed={computed_summary['speed_item_count']} "
        f"charge_pages={computed_summary['charge_page_count']} "
        f"charge_gem_packages={computed_summary['charge_gem_package_count']}"
    )
    print(f"failure_count={len(failures)}")
    print(f"Wrote {OUTPUT_PATH}")
    for failure in failures:
        print(f"ERROR: {failure}")
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
