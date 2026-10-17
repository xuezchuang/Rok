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
RECHARGE_LIST_CSV = (
    PROJECT_ROOT
    / "ref"
    / "config-tables"
    / "编译客户端配置"
    / "CSV"
    / "RechargeList.csv"
)
RECHARGE_GEM_MALL_CSV = (
    PROJECT_ROOT
    / "ref"
    / "config-tables"
    / "编译客户端配置"
    / "CSV"
    / "RechargeGemMall.csv"
)
LANGUAGE_WORKBOOK = PROJECT_ROOT / "ref" / "config-tables" / "s_Language.xlsm"
OUTPUT_JSON = MIGRATION_ROOT / "unity_economy_catalog.json"
OUTPUT_MD = MIGRATION_ROOT / "unity_economy_catalog.md"

STORE_PROXY = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "Scripts" / "Hotfix" / "MVC" / "Proxy" / "StoreProxy.cs"
STORE_MEDIATOR = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "Scripts" / "Hotfix" / "MVC" / "View_Mediator" / "Store" / "StoreMediator.cs"
SPEED_MEDIATOR = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "Scripts" / "Hotfix" / "MVC" / "View_Mediator" / "SpeedUp" / "AddSpeedMediator.cs"
ENUM_CONSTANT = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "Scripts" / "Hotfix" / "MVC" / "CMD" / "EnumConstant.cs"
CONFIGS_DATA = PROJECT_ROOT / "ref" / "config-tables" / "gen" / "Configs.data"
GUILD_JOIN_MEDIATOR = (
    PROJECT_ROOT
    / "ref"
    / "client-unity"
    / "Assets"
    / "Scripts"
    / "Hotfix"
    / "MVC"
    / "View_Mediator"
    / "Alliance"
    / "UI_Win_GuildJoinMediator.cs"
)
GUILD_SETTING_MEDIATOR = (
    PROJECT_ROOT
    / "ref"
    / "client-unity"
    / "Assets"
    / "Scripts"
    / "Hotfix"
    / "MVC"
    / "View_Mediator"
    / "Alliance"
    / "UI_Win_GuildSettingMediator.cs"
)
RECHARGE_PROXY = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "Scripts" / "Hotfix" / "MVC" / "Proxy" / "RechargeProxy.cs"
CHARGE_GEM_SHOP_ITEM_VIEW = (
    PROJECT_ROOT
    / "ref"
    / "client-unity"
    / "Assets"
    / "Scripts"
    / "Hotfix"
    / "MVC"
    / "View_Mediator"
    / "Common"
    / "Logic"
    / "UI_Item_ChargeGemShopItem_SubView.cs"
)
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

ITEM_HEADERS = [
    "ID",
    "l_nameID",
    "l_tipsID",
    "subType",
    "type",
    "l_typeDes",
    "typeGroup",
    "itemIcon",
    "lv",
    "quality",
    "batchUse ",
    "l_buttonDes",
    "itemFunction",
    "data1",
    "data2",
    "l_desID",
    "desData1",
    "desData2",
    "l_topID",
    "topData",
    "get",
    "shortcutPrice",
    "shopPrice",
    "redDotPrompt",
    "rank",
]

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
SHOP_CURRENCY_RUNTIME_NAME = "denar"
SPEED_APPLICABILITY_BY_SUB_TYPE = {
    20101: "common",
    20102: "building",
    20103: "training",
    20104: "research",
    20105: "healing",
}
ALLIANCE_CONFIG_KEYS = (
    "allianceAbbreviationMin",
    "allianceAbbreviationMax",
    "allianceNameMin",
    "allianceNameMax",
    "allianceNoticeNum",
    "allianceEstablishCost",
)


def project_relative(path: Path) -> str:
    return path.relative_to(PROJECT_ROOT).as_posix()


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
                f"Expected one stable integer value for {key} in {CONFIGS_DATA}, "
                f"got {sorted(unique_values)}"
            )
        values[key] = unique_values.pop()
    return values


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


def int_value(row: dict[str, str], field: str, source_name: str = "Item.csv") -> int:
    raw = row.get(field, "").strip()
    if not raw:
        return 0
    try:
        return int(raw)
    except ValueError as exc:
        raise RuntimeError(
            f"{source_name} field {field!r} expected int, got {raw!r}"
        ) from exc


def int_list_value(raw: str) -> list[int]:
    values: list[int] = []
    for part in raw.replace(",", "|").split("|"):
        part = part.strip()
        if not part:
            continue
        try:
            values.append(int(part))
        except ValueError as exc:
            raise RuntimeError(f"Item.csv acquisition source expected int, got {part!r}") from exc
    return values


def read_item_csv() -> tuple[list[dict[str, str]], list[str]]:
    with ITEM_CSV.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        headers = next(reader)
        types = next(reader)
        if headers != ITEM_HEADERS:
            raise RuntimeError(f"Unexpected Item.csv headers: {headers}")
        if len(types) != len(headers):
            raise RuntimeError(f"Item.csv type row has {len(types)} columns; expected {len(headers)}")

        rows: list[dict[str, str]] = []
        for source_row, values in enumerate(reader, start=3):
            if not any(value.strip() for value in values):
                continue
            if len(values) != len(headers):
                raise RuntimeError(
                    f"Item.csv row {source_row} has {len(values)} columns; expected {len(headers)}"
                )
            row = dict(zip(headers, values))
            row["__source_row"] = str(source_row)
            rows.append(row)
    return rows, types


def read_typed_csv(
    path: Path,
    expected_headers: list[str],
    source_name: str,
) -> tuple[list[dict[str, str]], list[str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        headers = next(reader)
        types = next(reader)
        if headers != expected_headers:
            raise RuntimeError(f"Unexpected {source_name} headers: {headers}")
        if len(types) != len(headers):
            raise RuntimeError(
                f"{source_name} type row has {len(types)} columns; expected {len(headers)}"
            )

        rows: list[dict[str, str]] = []
        for source_row, values in enumerate(reader, start=3):
            if not any(value.strip() for value in values):
                continue
            if len(values) != len(headers):
                raise RuntimeError(
                    f"{source_name} row {source_row} has {len(values)} columns; expected {len(headers)}"
                )
            row = dict(zip(headers, values))
            row["__source_row"] = str(source_row)
            rows.append(row)
    return rows, types


def read_language_lookup() -> tuple[dict[int, dict[str, Any]], dict[str, Any]]:
    table = parse_table(LANGUAGE_WORKBOOK, PROJECT_ROOT / "ref")
    lookup = {
        int(row["id"]): row
        for row in table["rows"]
        if isinstance(row.get("id"), int) and row["id"] > 0
    }
    return lookup, table


def localized_text(language_by_id: dict[int, dict[str, Any]], locale_id: int, field: str) -> str:
    if locale_id <= 0:
        return ""
    return str(language_by_id.get(locale_id, {}).get(field, "") or "")


def read_item_atlas_sprites() -> dict[tuple[str, str], dict[str, Any]]:
    sprites: dict[tuple[str, str], dict[str, Any]] = {}
    for atlas_name in ITEM_ATLAS_NAMES:
        sheet_path = ITEM_ATLAS_ROOT / f"{atlas_name}.tpsheet"
        lines = sheet_path.read_text(encoding="utf-8-sig").splitlines()
        size_line = next(
            (line for line in lines if line.startswith(":size=")),
            "",
        )
        size_match = re.fullmatch(r":size=(\d+)x(\d+)", size_line)
        if not size_match:
            raise RuntimeError(f"Unable to parse atlas size from {sheet_path}")
        atlas_size = [int(size_match.group(1)), int(size_match.group(2))]
        for line in lines:
            if not line or line.startswith(("#", ":")):
                continue
            parts = [part.strip() for part in line.split(";")]
            if len(parts) < 5:
                raise RuntimeError(
                    f"Unexpected sprite row in {sheet_path}: {line!r}"
                )
            sprite_name = parts[0]
            rect = [int(parts[index]) for index in range(1, 5)]
            key = (atlas_name, sprite_name)
            if key in sprites:
                raise RuntimeError(f"Duplicate atlas sprite {key}")
            sprites[key] = {
                "atlas": atlas_name,
                "sprite": sprite_name,
                "rect": rect,
                "atlas_size": atlas_size,
                "source": project_relative(sheet_path),
            }
    return sprites


def resolve_item_icon(
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


def build_item(
    row: dict[str, str],
    language_by_id: dict[int, dict[str, Any]],
    shop_category_types: tuple[int, ...],
    atlas_sprites: dict[tuple[str, str], dict[str, Any]],
) -> dict[str, Any]:
    item_id = int_value(row, "ID")
    name_locale_id = int_value(row, "l_nameID")
    type_description_locale_id = int_value(row, "l_typeDes")
    description_locale_id = int_value(row, "l_desID")
    item_type = int_value(row, "type")
    sub_type = int_value(row, "subType")
    data1 = int_value(row, "data1")
    shop_price = int_value(row, "shopPrice")
    is_normal_shop_item = shop_price > 0 and item_type in shop_category_types

    icon_reference = row.get("itemIcon", "").strip()
    item: dict[str, Any] = {
        "id": item_id,
        "name_locale_id": name_locale_id,
        "display_name_zh_cn": localized_text(language_by_id, name_locale_id, "cn"),
        "display_name_en": localized_text(language_by_id, name_locale_id, "en"),
        "tips_locale_id": int_value(row, "l_tipsID"),
        "sub_type": sub_type,
        "type": item_type,
        "type_description_locale_id": type_description_locale_id,
        "type_description_zh_cn": localized_text(language_by_id, type_description_locale_id, "cn"),
        "type_group": int_value(row, "typeGroup"),
        "icon": icon_reference,
        **resolve_item_icon(icon_reference, atlas_sprites),
        "required_level": int_value(row, "lv"),
        "quality": int_value(row, "quality"),
        "batch_use": int_value(row, "batchUse ") != 0,
        "button_locale_id": int_value(row, "l_buttonDes"),
        "item_function": int_value(row, "itemFunction"),
        "data1": data1,
        "data2": int_value(row, "data2"),
        "description_locale_id": description_locale_id,
        "description_template_zh_cn": localized_text(language_by_id, description_locale_id, "cn"),
        "description_template_en": localized_text(language_by_id, description_locale_id, "en"),
        "description_data1": int_value(row, "desData1"),
        "description_data2": int_value(row, "desData2"),
        "top_locale_id": int_value(row, "l_topID"),
        "top_data": int_value(row, "topData"),
        "acquisition_source_ids": int_list_value(row.get("get", "")),
        "shortcut_price": int_value(row, "shortcutPrice"),
        "shop_price": shop_price,
        "shop_currency_type_id": SHOP_CURRENCY_TYPE_ID if is_normal_shop_item else 0,
        "is_normal_shop_item": is_normal_shop_item,
        "red_dot_prompt": int_value(row, "redDotPrompt"),
        "rank": int_value(row, "rank"),
        "source": {
            "path": project_relative(ITEM_CSV),
            "row": int(row["__source_row"]),
        },
    }

    speed_applicability = SPEED_APPLICABILITY_BY_SUB_TYPE.get(sub_type)
    if speed_applicability and data1 > 0:
        item["speed_duration_seconds"] = data1
        item["speed_applicability"] = speed_applicability
    return item


def build_charge_page(row: dict[str, str]) -> dict[str, Any]:
    source_name = "RechargeList.csv"
    return {
        "id": int_value(row, "ID", source_name),
        "paging_locale_id": int_value(row, "l_pagingID", source_name),
        "paging_type": int_value(row, "pagingType", source_name),
        "preposition": int_value(row, "preposition", source_name),
        "sort": int_value(row, "sort", source_name),
        "icon": row.get("icon", "").strip(),
        "source": {
            "path": project_relative(RECHARGE_LIST_CSV),
            "row": int(row["__source_row"]),
        },
    }


def build_charge_gem_package(row: dict[str, str]) -> dict[str, Any]:
    source_name = "RechargeGemMall.csv"
    return {
        "id": int_value(row, "ID", source_name),
        "name_locale_id": int_value(row, "l_nameID", source_name),
        # Unity uses RechargeGemMall.price as a Price table lookup key. It is
        # not a cash amount; platform metadata owns the display price.
        "product_id": int_value(row, "price", source_name),
        "denar_num": int_value(row, "denarNum", source_name),
        "first_presenter": int_value(row, "firstPresenter", source_name),
        "presenter": int_value(row, "presenter", source_name),
        "gem_icon": row.get("gemIcon", "").strip(),
        "recharge_progress": int_value(row, "rechargeProgress", source_name),
        "mail": int_value(row, "mail", source_name),
        "source": {
            "path": project_relative(RECHARGE_GEM_MALL_CSV),
            "row": int(row["__source_row"]),
        },
    }


def main() -> int:
    for required_path in (
        ITEM_CSV,
        RECHARGE_LIST_CSV,
        RECHARGE_GEM_MALL_CSV,
        LANGUAGE_WORKBOOK,
        STORE_PROXY,
        STORE_MEDIATOR,
        SPEED_MEDIATOR,
        ENUM_CONSTANT,
        CONFIGS_DATA,
        GUILD_JOIN_MEDIATOR,
        GUILD_SETTING_MEDIATOR,
        RECHARGE_PROXY,
        CHARGE_GEM_SHOP_ITEM_VIEW,
        *(ITEM_ATLAS_ROOT / f"{name}.tpsheet" for name in ITEM_ATLAS_NAMES),
    ):
        if not required_path.exists():
            raise RuntimeError(f"Missing economy catalog source: {required_path}")

    source_rows, source_types = read_item_csv()
    recharge_list_rows, recharge_list_types = read_typed_csv(
        RECHARGE_LIST_CSV,
        RECHARGE_LIST_HEADERS,
        "RechargeList.csv",
    )
    recharge_gem_mall_rows, recharge_gem_mall_types = read_typed_csv(
        RECHARGE_GEM_MALL_CSV,
        RECHARGE_GEM_MALL_HEADERS,
        "RechargeGemMall.csv",
    )
    language_by_id, language_table = read_language_lookup()
    shop_category_types = read_enabled_shop_category_types()
    atlas_sprites = read_item_atlas_sprites()
    alliance_config = read_named_config_ints()
    items = [
        build_item(
            row,
            language_by_id,
            shop_category_types,
            atlas_sprites,
        )
        for row in source_rows
    ]
    charge_pages = sorted(
        (build_charge_page(row) for row in recharge_list_rows),
        key=lambda page: (page["sort"], page["id"]),
    )
    charge_gem_packages = sorted(
        (build_charge_gem_package(row) for row in recharge_gem_mall_rows),
        key=lambda package: package["id"],
    )

    item_ids = [item["id"] for item in items]
    if any(item_id <= 0 for item_id in item_ids):
        raise RuntimeError("Item.csv contains a non-positive item ID")
    if len(item_ids) != len(set(item_ids)):
        raise RuntimeError("Item.csv contains duplicate item IDs")

    charge_page_ids = [page["id"] for page in charge_pages]
    charge_paging_types = [page["paging_type"] for page in charge_pages]
    if any(page_id <= 0 for page_id in charge_page_ids):
        raise RuntimeError("RechargeList.csv contains a non-positive page ID")
    if len(charge_page_ids) != len(set(charge_page_ids)):
        raise RuntimeError("RechargeList.csv contains duplicate page IDs")
    if len(charge_paging_types) != len(set(charge_paging_types)):
        raise RuntimeError("RechargeList.csv contains duplicate pagingType values")

    charge_package_ids = [package["id"] for package in charge_gem_packages]
    charge_product_ids = [package["product_id"] for package in charge_gem_packages]
    if any(package_id <= 0 for package_id in charge_package_ids):
        raise RuntimeError("RechargeGemMall.csv contains a non-positive package ID")
    if len(charge_package_ids) != len(set(charge_package_ids)):
        raise RuntimeError("RechargeGemMall.csv contains duplicate package IDs")
    if any(product_id <= 0 for product_id in charge_product_ids):
        raise RuntimeError("RechargeGemMall.csv contains a non-positive Price lookup key")
    if len(charge_product_ids) != len(set(charge_product_ids)):
        raise RuntimeError("RechargeGemMall.csv contains duplicate Price lookup keys")

    shop_items = [item for item in items if item["is_normal_shop_item"]]
    speed_items = [item for item in items if item.get("speed_duration_seconds", 0) > 0]
    category_counts = Counter(item["type"] for item in shop_items)
    speed_counts = Counter(item["speed_applicability"] for item in speed_items)
    localized_name_count = sum(bool(item["display_name_zh_cn"]) for item in items)
    icon_count = sum(bool(item["icon"]) for item in items)
    resolved_icon_sprite_count = sum(
        bool(item["icon_rect"]) for item in items
    )
    acquisition_source_item_count = sum(bool(item["acquisition_source_ids"]) for item in items)

    summary = {
        "item_count": len(items),
        "localized_name_count": localized_name_count,
        "unresolved_name_count": len(items) - localized_name_count,
        "icon_count": icon_count,
        "resolved_icon_sprite_count": resolved_icon_sprite_count,
        "shop_item_count": len(shop_items),
        "shop_category_count": len(category_counts),
        "shop_category_counts": {str(key): category_counts[key] for key in sorted(category_counts)},
        "speed_item_count": len(speed_items),
        "speed_applicability_counts": {
            key: speed_counts[key] for key in sorted(speed_counts)
        },
        "acquisition_source_item_count": acquisition_source_item_count,
        "charge_page_count": len(charge_pages),
        "charge_paging_type_count": len(set(charge_paging_types)),
        "charge_gem_package_count": len(charge_gem_packages),
    }

    catalog = {
        "schema": "rok.unity_economy_catalog.v1",
        "sources": {
            "item_csv": {
                "path": project_relative(ITEM_CSV),
                "sha256": sha256(ITEM_CSV),
                "headers": ITEM_HEADERS,
                "types": source_types,
            },
            "recharge_list_csv": {
                "path": project_relative(RECHARGE_LIST_CSV),
                "sha256": sha256(RECHARGE_LIST_CSV),
                "headers": RECHARGE_LIST_HEADERS,
                "types": recharge_list_types,
            },
            "recharge_gem_mall_csv": {
                "path": project_relative(RECHARGE_GEM_MALL_CSV),
                "sha256": sha256(RECHARGE_GEM_MALL_CSV),
                "headers": RECHARGE_GEM_MALL_HEADERS,
                "types": recharge_gem_mall_types,
            },
            "language_workbook": {
                "path": project_relative(LANGUAGE_WORKBOOK),
                "sha256": sha256(LANGUAGE_WORKBOOK),
                "sheet": language_table["sheet_name"],
                "row_count": language_table["row_count"],
            },
            "unity_shop_proxy": project_relative(STORE_PROXY),
            "unity_shop_mediator": project_relative(STORE_MEDIATOR),
            "unity_speed_mediator": project_relative(SPEED_MEDIATOR),
            "unity_currency_enum": project_relative(ENUM_CONSTANT),
            "unity_configs_data": {
                "path": project_relative(CONFIGS_DATA),
                "sha256": sha256(CONFIGS_DATA),
            },
            "unity_guild_join_mediator": project_relative(GUILD_JOIN_MEDIATOR),
            "unity_guild_setting_mediator": project_relative(
                GUILD_SETTING_MEDIATOR
            ),
            "unity_recharge_proxy": project_relative(RECHARGE_PROXY),
            "unity_charge_gem_shop_item_view": project_relative(
                CHARGE_GEM_SHOP_ITEM_VIEW
            ),
            "item_atlas_sheets": [
                {
                    "atlas": name,
                    "path": project_relative(
                        ITEM_ATLAS_ROOT / f"{name}.tpsheet"
                    ),
                    "sha256": sha256(
                        ITEM_ATLAS_ROOT / f"{name}.tpsheet"
                    ),
                }
                for name in ITEM_ATLAS_NAMES
            ],
        },
        "contracts": {
            "normal_shop": {
                "item_type_categories": list(shop_category_types),
                "eligibility": "shopPrice > 0 and Item.type is enabled by StoreProxy",
                "price_field": "shopPrice",
                "currency_type_id": SHOP_CURRENCY_TYPE_ID,
                "currency_runtime_name": SHOP_CURRENCY_RUNTIME_NAME,
                "purchase_service_request": "Shop_BuyShopItem",
            },
            "speed_items": {
                "duration_field": "data1",
                "applicability_field": "subType",
                "applicability_by_sub_type": {
                    str(key): value for key, value in SPEED_APPLICABILITY_BY_SUB_TYPE.items()
                },
                "consume_service_request": "Role_SpeedUp",
            },
            "charge_ui": {
                "page_order_field": "RechargeList.sort",
                "ordered_paging_types": charge_paging_types,
                "gem_package_product_id_field": "product_id",
                "product_id_source_field": "RechargeGemMall.price",
                "product_id_semantics": "Price table lookup key; not a cash amount or display price",
                "display_price_boundary": "Price.rechargeID resolved through IGGPayment platform metadata",
                "purchase_boundary": "IGGPayment platform purchase and authoritative server fulfillment; this catalog is read-only",
            },
            "alliance_ui": {
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
            },
        },
        "summary": summary,
        "items": items,
        "charge_pages": charge_pages,
        "charge_gem_packages": charge_gem_packages,
    }

    MIGRATION_ROOT.mkdir(parents=True, exist_ok=True)
    OUTPUT_JSON.write_text(
        json.dumps(catalog, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    unresolved_items = [item for item in items if not item["display_name_zh_cn"]]
    lines = [
        "# Rok Unity Economy Catalog",
        "",
        f"- Schema: `{catalog['schema']}`",
        f"- Item source: `{project_relative(ITEM_CSV)}`",
        f"- Charge-page source: `{project_relative(RECHARGE_LIST_CSV)}`",
        f"- Gem-package source: `{project_relative(RECHARGE_GEM_MALL_CSV)}`",
        f"- Language source: `{project_relative(LANGUAGE_WORKBOOK)}` / `{language_table['sheet_name']}`",
        f"- Alliance create cost: {alliance_config['allianceEstablishCost']} gems (from `{project_relative(CONFIGS_DATA)}`)",
        f"- Items: {summary['item_count']}",
        f"- Chinese display names resolved: {summary['localized_name_count']}/{summary['item_count']}",
        f"- Icons present: {summary['icon_count']}/{summary['item_count']}",
        f"- Item1-item5 atlas sprites resolved: {summary['resolved_icon_sprite_count']}/{summary['item_count']}",
        f"- Normal-shop items: {summary['shop_item_count']}",
        f"- Speed-duration items: {summary['speed_item_count']}",
        f"- Items with acquisition-source IDs: {summary['acquisition_source_item_count']}",
        f"- Charge pages: {summary['charge_page_count']}",
        f"- Charge gem packages: {summary['charge_gem_package_count']}",
        "",
        "## Normal Shop Categories",
        "",
        f"The category key is the authoritative `Item.type`. `StoreProxy` enables types {', '.join(str(item_type) for item_type in shop_category_types)} and admits only rows with `shopPrice > 0`.",
        "",
        "| Item.type | Rows |",
        "| ---: | ---: |",
    ]
    for item_type in shop_category_types:
        lines.append(f"| {item_type} | {category_counts.get(item_type, 0)} |")

    lines.extend(
        [
            "",
            f"All normal-shop prices use Unity currency type `{SHOP_CURRENCY_TYPE_ID}` (`{SHOP_CURRENCY_RUNTIME_NAME}` / 宝石) in `StoreMediator`; this catalog does not contain balances or execute purchases.",
            "",
            "## Speed Applicability",
            "",
            "`AddSpeedMediator` uses `Item.subType` for applicability and `Item.data1` as the duration in seconds.",
            "",
            "| Item.subType | Applicability | Rows |",
            "| ---: | --- | ---: |",
        ]
    )
    for sub_type, applicability in SPEED_APPLICABILITY_BY_SUB_TYPE.items():
        lines.append(f"| {sub_type} | `{applicability}` | {speed_counts.get(applicability, 0)} |")

    lines.extend(
        [
            "",
            "## Charge Pages",
            "",
            "`UICharge` obtains its page list from `RechargeList` and orders it by `sort`.",
            "",
            "| Sort | Paging type | Config ID | Locale ID | Preposition | Icon |",
            "| ---: | ---: | ---: | ---: | ---: | --- |",
        ]
    )
    for page in charge_pages:
        lines.append(
            f"| {page['sort']} | {page['paging_type']} | {page['id']} | "
            f"{page['paging_locale_id']} | {page['preposition']} | `{page['icon']}` |"
        )

    lines.extend(
        [
            "",
            "## Charge Gem Packages",
            "",
            "`product_id` is copied from `RechargeGemMall.price`, which is a `Price` table lookup key. It is not a cash amount or a display price.",
            "",
            "| Config ID | Product ID | Gems | First bonus | Repeat bonus | Progress | Mail |",
            "| ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
        ]
    )
    for package in charge_gem_packages:
        lines.append(
            f"| {package['id']} | {package['product_id']} | {package['denar_num']} | "
            f"{package['first_presenter']} | {package['presenter']} | "
            f"{package['recharge_progress']} | {package['mail']} |"
        )

    lines.extend(
        [
            "",
            "## Known Source Gaps",
            "",
        ]
    )
    if unresolved_items:
        for item in unresolved_items:
            lines.append(
                f"- Item `{item['id']}` references name locale `{item['name_locale_id']}`, but the current `s_Language` source has no Chinese text."
            )
    else:
        lines.append("- None.")

    lines.extend(
        [
            "",
            "## Runtime Boundary",
            "",
            "- The UE subsystem is a read-only config catalog. It does not own bag counts, currency balances, or item mutations.",
            "- Normal-store purchase remains the Unity `Shop_BuyShopItem` service boundary.",
            "- Applying a speed item remains the Unity `Role_SpeedUp` service boundary and requires an authoritative queue plus inventory state.",
            "- Charge gem purchase remains the `Price.rechargeID` -> IGGPayment platform flow with authoritative server fulfillment; `product_id` must not be presented as a cash price.",
        ]
    )
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(f"Wrote {OUTPUT_JSON}")
    print(f"Wrote {OUTPUT_MD}")
    print(
        "Economy catalog: "
        f"items={summary['item_count']} "
        f"localized={summary['localized_name_count']} "
        f"shop={summary['shop_item_count']} "
        f"speed={summary['speed_item_count']} "
        f"charge_pages={summary['charge_page_count']} "
        f"charge_gem_packages={summary['charge_gem_package_count']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
