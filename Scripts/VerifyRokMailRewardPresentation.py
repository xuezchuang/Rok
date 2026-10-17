"""Independently check the mail fixture against Unity tables, source and atlas."""
import csv
import hashlib
import json
import re
from pathlib import Path
from rok_data_paths import label, resolve
import re

from BuildRokUnityEconomyCatalog import localized_text, read_language_lookup
from BuildRokMailRewardPresentation import ROOT, OUTPUT, CSV_ROOT, MAIL, SPROTO, EMAIL_PROXY, SHEET, SOURCES


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def table(name):
    path = CSV_ROOT / (name + ".csv")
    return {int(row["ID"]): row for row in list(csv.DictReader(path.read_text(encoding="utf-8-sig").splitlines()))[1:]}


def verify():
    package = json.loads(OUTPUT.read_text(encoding="utf-8"))
    check(package["schema"] == "rok.mail_reward_presentation.v1", "schema")
    check(set(package["fixtures"]) == {"reference_single", "mixed_audit", "empty_alternate"}, "fixture set")
    check("synthetic" in package["origin"] and "read-only" in package["origin"], "fixture provenance")
    check(set(package["sources"]) == {label(path) for path in SOURCES}, "required source set")
    for relative, digest in package["sources"].items():
        source = resolve(relative)
        check(source.is_file() and hashlib.sha256(source.read_bytes()).hexdigest() == digest, f"source hash {relative}")
    mail_source = MAIL.read_text(encoding="utf-8-sig")
    start = mail_source.index("private void OnEnclosureList(")
    end = mail_source.index("private void OnEnclosureListFly(", start)
    method = mail_source[start:end]
    fields = ("food", "wood", "stone", "gold", "denar")
    positions = [re.search(rf"if\s*\(\s*m_currentSelectEmail\.rewards\.{field}\s*>\s*0\s*\)", method).start()
                 for field in fields]
    check(positions == sorted(positions), "Unity currency order")
    check(method.index("rewards.soldiers!=null") < method.index("rewards.items!=null"), "Unity list order")
    check("soldiers[i].num.ToString(\"N0\")" in method and "items[i].itemNum.ToString(\"N0\")" in method,
          "Unity list amounts stay unfiltered")
    sproto = SPROTO.read_text(encoding="utf-8-sig")
    check("class RewardInfo : SprotoTypeBase" in sproto and "class RewardItem : SprotoTypeBase" in sproto
          and "class SoldierInfo : SprotoTypeBase" in sproto, "Sproto reward shape")
    email_proxy = EMAIL_PROXY.read_text(encoding="utf-8-sig")
    check('case "activity":' in email_proxy and 'QueryRecord<ActivityCalendarDefine>(int.Parse(value))' in email_proxy
          and 'LanguageUtils.getText(activity.l_nameID)' in email_proxy, "EmailProxy activity placeholder semantics")
    language, _ = read_language_lookup()
    currencies, items, arms = table("Currency"), table("Item"), table("Arms")
    mails, activities = table("Mail"), table("ActivityCalendar")
    metadata = package["metadata"]
    for index, field in enumerate(fields):
        source = currencies[100 + index]
        actual = metadata["currencies"][index]
        check(actual == {"id": 100 + index, "field": field,
                         "name": localized_text(language, int(source["l_desID"]), "cn"), "icon": source["iconID"]},
              f"currency metadata {field}")
    item = items[502090001]
    sheet = SHEET.read_text(encoding="utf-8-sig").splitlines()
    atlas_size = [int(v) for v in next(line[6:] for line in sheet if line.startswith(":size=")).split("x")]
    sprite = next(line.split(";") for line in sheet if line.startswith("item_5_1031;"))
    item_expected = {"id": 502090001, "name_locale_id": int(item["l_nameID"]),
                     "name": localized_text(language, int(item["l_nameID"]), "cn"),
                     "icon": item["itemIcon"], "icon_rect": [int(v) for v in sprite[1:5]],
                     "icon_atlas_size": atlas_size, "quality": int(item["quality"]),
                     "atlas_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item5.rok_fullscreen_item5"}
    check(metadata["item"] == item_expected, "source item metadata")
    check(item_expected["quality"] == 4 and item_expected["icon_rect"] == [257, 641, 126, 126]
          and item_expected["name"] == "赫雀瑟枝", "reference attachment identity")
    army = arms[10304]
    troop_expected = {"id": 10304, "name_locale_id": int(army["l_armsID"]),
                      "name": localized_text(language, int(army["l_armsID"]), "cn"),
                      "icon": army["icon"], "level": int(army["armsLv"]), "arms_type": int(army["armsType"])}
    check(metadata["soldier"] == troop_expected and troop_expected["name"] == "弩兵", "source soldier metadata")
    mail, activity = mails[100030], activities[10101]
    check(int(mail["type"]) == 1 and int(mail["group"]) == 1 and int(mail["receiveAuto"]) == 0,
          "system mail template status")
    locale_ids = {"title": int(mail["l_nameID"]), "subtitle": int(mail["l_subheadingID"]),
                  "body": int(mail["l_mesID"]), "activity_name": int(activity["l_nameID"]), "claim": 570008}
    check(locale_ids == {"title": 762137, "subtitle": 762138, "body": 550111,
                         "activity_name": 300229, "claim": 570008}, "mail template locale identity")
    activity_name = localized_text(language, locale_ids["activity_name"], "cn")
    subtitle_raw = localized_text(language, locale_ids["subtitle"], "cn")
    body_raw = localized_text(language, locale_ids["body"], "cn")
    check(subtitle_raw.count("{activity_0}") == 1 and body_raw.count("{activity_0}") == 1,
          "mail template activity argument")
    body_rich = body_raw.replace("{activity_0}", activity_name)
    body_match = re.fullmatch(r'(.*?)<color=green><b>("[^"\n]+")</b></color>(.*)', body_rich, re.DOTALL)
    check(body_match is not None and body_match.group(2) == '"' + activity_name + '"', "mail body highlight")
    greeting, body_prefix = body_match.group(1).split("\n", 1)
    template_expected = {
        "mail_id": 100030, "activity_id": 10101, "locale_ids": locale_ids,
        "title": localized_text(language, locale_ids["title"], "cn"), "activity_name": activity_name,
        "subtitle": subtitle_raw.replace("{activity_0}", activity_name), "body_rich": body_rich,
        "body_plain": re.sub(r"</?(?:color(?:=[^>]+)?|b)>", "", body_rich),
        "body_greeting": greeting, "body_prefix": body_prefix,
        "body_activity_quoted": body_match.group(2), "body_suffix": body_match.group(3),
        "claim_label": localized_text(language, 570008, "cn"), "send_time": None,
        "qualification": "Mail.csv source template paired with synthetic RewardInfo; no EmailInfo or dynamic send time",
    }
    check(package["mail_template"] == template_expected, "source-derived mail template")
    check(template_expected["title"] == "活动奖励补发" and template_expected["activity_name"] == "征服之始-领取宝箱"
          and template_expected["claim_label"] == "领取" and template_expected["send_time"] is None,
          "reference mail copy and unknown dynamic time")
    fixtures = package["fixtures"]
    def payload(key): return fixtures[key]["payload"]
    reference, mixed, empty = payload("reference_single"), payload("mixed_audit"), payload("empty_alternate")
    check(all(reference[field] == 0 for field in fields) and reference["soldiers"] == []
          and reference["items"] == [{"itemId": 502090001, "itemNum": 3}], "existing single attachment")
    check([mixed[field] for field in fields] == [101, 202, 303, 404, 505]
          and mixed["soldiers"] == [{"id": 10304, "num": 7}]
          and mixed["items"] == [{"itemId": 502090001, "itemNum": 3}], "mixed source-shaped fixture")
    check(all(empty[field] == 0 for field in fields) and not empty["soldiers"] and not empty["items"], "empty alternate")
    print("PASS mail reward presentation: source hashes, Unity order, item/troop/5 currency metadata, fixtures 1/7/0")


if __name__ == "__main__":
    verify()
