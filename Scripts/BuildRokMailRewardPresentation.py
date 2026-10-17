"""Build source-traced, read-only mail reward presentation fixtures.

Amounts in mixed_audit are synthetic probes, never account or server state.
"""
import csv
import hashlib
import json
import re
from pathlib import Path

from BuildRokUnityEconomyCatalog import LANGUAGE_WORKBOOK, localized_text, read_language_lookup
from rok_data_paths import DATA_ROOT, label

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = DATA_ROOT / "mail_reward_presentation.json"
CSV_ROOT = ROOT / "ref/config-tables/编译客户端配置/CSV"
MAIL = ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Mail/MailMediator.cs"
SPROTO = ROOT / "ref/client-unity/Assets/Scripts/Hotfix/Protocol/Sproto.cs"
EMAIL_PROXY = ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/EmailProxy.cs"
LANGUAGE_BIN = ROOT / "ref/client-unity/Assets/StreamingAssets/Config/Bin/Language.bin"
SHEET = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_item/item5.tpsheet"
ECONOMY = DATA_ROOT / "unity_economy_catalog.json"
GAMEPLAY = DATA_ROOT / "unity_gameplay_catalog.json"
TRAIN = DATA_ROOT / "train_army_detail.json"
SOURCES = [MAIL, SPROTO, EMAIL_PROXY, CSV_ROOT / "Currency.csv", CSV_ROOT / "Item.csv", CSV_ROOT / "Arms.csv",
           CSV_ROOT / "Mail.csv", CSV_ROOT / "ActivityCalendar.csv",
           LANGUAGE_WORKBOOK, LANGUAGE_BIN, SHEET, ECONOMY, GAMEPLAY, TRAIN]


def rows(name):
    return {int(row["ID"]): row for row in list(csv.DictReader((CSV_ROOT / name).read_text(encoding="utf-8-sig").splitlines()))[1:]}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_path(path):
    return label(path)


def build(write=True):
    language, _ = read_language_lookup()
    currency = rows("Currency.csv")
    items = rows("Item.csv")
    arms = rows("Arms.csv")
    economy = json.loads(ECONOMY.read_text(encoding="utf-8"))
    gameplay = json.loads(GAMEPLAY.read_text(encoding="utf-8"))
    train = json.loads(TRAIN.read_text(encoding="utf-8"))
    mail = rows("Mail.csv")[100030]
    activity = rows("ActivityCalendar.csv")[10101]
    assert int(mail["type"]) == 1 and int(mail["group"]) == 1 and int(mail["receiveAuto"]) == 0
    assert int(mail["l_nameID"]) == 762137 and int(mail["l_subheadingID"]) == 762138 and int(mail["l_mesID"]) == 550111
    assert int(activity["l_nameID"]) == 300229
    activity_name = localized_text(language, int(activity["l_nameID"]), "cn")
    title = localized_text(language, int(mail["l_nameID"]), "cn")
    subtitle_source = localized_text(language, int(mail["l_subheadingID"]), "cn")
    body_source = localized_text(language, int(mail["l_mesID"]), "cn")
    assert subtitle_source.count("{activity_0}") == 1 and body_source.count("{activity_0}") == 1
    subtitle = subtitle_source.replace("{activity_0}", activity_name)
    body_rich = body_source.replace("{activity_0}", activity_name)
    body_plain = re.sub(r"</?(?:color(?:=[^>]+)?|b)>", "", body_rich)
    body_match = re.fullmatch(r'(.*?)<color=green><b>("[^"\n]+")</b></color>(.*)', body_rich, re.DOTALL)
    assert body_match and body_match.group(2) == '"' + activity_name + '"'
    greeting, body_prefix = body_match.group(1).split("\n", 1)
    body_activity_quoted, body_suffix = body_match.group(2), body_match.group(3)
    assert body_plain == greeting + "\n" + body_prefix + body_activity_quoted + body_suffix
    claim_label = localized_text(language, 570008, "cn")
    assert title == "活动奖励补发" and activity_name == "征服之始-领取宝箱" and claim_label == "领取"
    item_id, troop_id = 502090001, 10304
    item = items[item_id]
    army = arms[troop_id]
    item_meta = next(entry for entry in economy["items"] if entry["id"] == item_id)
    troop_meta = next(entry for entry in gameplay["troops"] if entry["id"] == troop_id)
    train_meta = next(entry for entry in train["entries"] if entry["id"] == troop_id)
    item_name = localized_text(language, int(item["l_nameID"]), "cn")
    troop_name = localized_text(language, int(army["l_armsID"]), "cn")
    assert item_name and troop_name
    assert item_meta["display_name_zh_cn"] == item_name and item_meta["icon"] == item["itemIcon"]
    assert item_meta["quality"] == int(item["quality"]) == 4
    assert troop_meta["icon"] == army["icon"] and train_meta["name"] == troop_name
    assert item_meta["icon_rect"] == [257, 641, 126, 126]
    assert item_meta["icon_atlas_size"] == [1024, 2048]
    sheet_lines = SHEET.read_text(encoding="utf-8-sig").splitlines()
    assert ":size=1024x2048" in sheet_lines
    assert "item_5_1031;257;641;126;126;" in next(line for line in sheet_lines if line.startswith("item_5_1031;"))
    currency_meta = []
    for currency_id, field in ((100, "food"), (101, "wood"), (102, "stone"), (103, "gold"), (104, "denar")):
        record = currency[currency_id]
        assert record["name"] == field
        currency_meta.append({"id": currency_id, "field": field,
                              "name": localized_text(language, int(record["l_desID"]), "cn"),
                              "icon": record["iconID"]})
    assert [entry["name"] for entry in currency_meta] == ["食物", "木材", "铁矿石", "金币", "钻石"]
    empty = {"food": 0, "wood": 0, "stone": 0, "gold": 0, "denar": 0, "soldiers": [], "items": []}
    reference = {**empty, "items": [{"itemId": item_id, "itemNum": 3}]}
    mixed = {"food": 101, "wood": 202, "stone": 303, "gold": 404, "denar": 505,
             "soldiers": [{"id": troop_id, "num": 7}], "items": [{"itemId": item_id, "itemNum": 3}]}
    result = {
        "schema": "rok.mail_reward_presentation.v1",
        "source_behavior": "MailMediator.OnEnclosureList: positive food/wood/stone/gold/denar, then every soldier, then every item",
        "origin": "Unity reference shape; fixture amounts are synthetic and read-only, not a player mailbox or inventory",
        "sources": {source_path(path): sha256(path) for path in SOURCES},
        "metadata": {
            "currencies": currency_meta,
            "item": {"id": item_id, "name_locale_id": int(item["l_nameID"]), "name": item_name,
                     "icon": item["itemIcon"], "icon_rect": item_meta["icon_rect"],
                     "icon_atlas_size": item_meta["icon_atlas_size"], "quality": int(item["quality"]),
                     "atlas_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item5.rok_fullscreen_item5"},
            "soldier": {"id": troop_id, "name_locale_id": int(army["l_armsID"]), "name": troop_name,
                        "icon": army["icon"], "level": troop_meta["level"], "arms_type": troop_meta["arms_type"]},
        },
        "mail_template": {
            "mail_id": 100030, "activity_id": 10101,
            "locale_ids": {"title": 762137, "subtitle": 762138, "body": 550111,
                           "activity_name": 300229, "claim": 570008},
            "title": title, "activity_name": activity_name,
            "subtitle": subtitle, "body_rich": body_rich, "body_plain": body_plain,
            "body_greeting": greeting, "body_prefix": body_prefix,
            "body_activity_quoted": body_activity_quoted, "body_suffix": body_suffix,
            "claim_label": claim_label, "send_time": None,
            "qualification": "Mail.csv source template paired with synthetic RewardInfo; no EmailInfo or dynamic send time",
        },
        "fixtures": {
            "reference_single": {"role": "existing mail_reference selected attachment; not account mail", "payload": reference},
            "mixed_audit": {"role": "synthetic source-shaped mixed reward audit", "payload": mixed},
            "empty_alternate": {"role": "synthetic empty alternate mail selection", "payload": empty},
        },
    }
    if write:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {OUTPUT} (reference=1, mixed=7, empty=0)")
    return result


if __name__ == "__main__":
    build()
