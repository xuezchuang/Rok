"""Source-derived resource popup prose; does not supply live gathering state."""
import csv
import html
import json
import re
import sys
from pathlib import Path
from BuildRokUnityEconomyCatalog import read_language_lookup

ROOT = Path(__file__).resolve().parents[1]
TABLE = ROOT / "ref/config-tables/编译客户端配置/CSV/ResourceGatherType.csv"
OUTPUT = ROOT / "Saved/RokUnityMigration/world_resource_descriptions.json"


def to_slate(text):
    # These four source descriptions use only non-nested <b> emphasis.
    # Reject new markup until it has an intentional UE mapping.
    opened = False
    for tag in re.findall(r"<[^>]*>", text):
        assert tag in ("<b>", "</b>"), f"Unsupported source markup: {tag}"
        assert (tag == "<b>") != opened, "Unbalanced/nested source emphasis"
        opened = tag == "<b>"
    assert not opened, "Unclosed source emphasis"
    result = html.escape(text, quote=False).replace("&lt;b&gt;", "<b>").replace("&lt;/b&gt;", "</>")
    assert html.unescape(re.sub(r"<b>|</>", "", result)) == re.sub(r"</?b>", "", text)
    return result


def build():
    language, _ = read_language_lookup()
    descriptions = {}
    with TABLE.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))[1:]  # skip source type row
    for kind, type_id in (("food", 1), ("wood", 2), ("stone", 3), ("gold", 4)):
        matches = [r for r in rows if int(r["type"]) == type_id]
        ids = {int(r["l_descId"]) for r in matches}
        assert len(ids) == 1, f"{kind}: level-dependent descriptions need explicit lookup"
        locale_id = ids.pop()
        rich_text = language[locale_id]["cn"]
        assert rich_text.strip(), kind
        descriptions[kind] = {
            "locale_id": locale_id,
            "resource_type_ids": [int(r["ID"]) for r in matches],
            "rich_text": rich_text,
            "slate_text": to_slate(rich_text),
            "text": re.sub(r"</?b>", "", rich_text),
        }
    return {"schema": "rok.world_resource_descriptions.v1",
            "source_table": TABLE.relative_to(ROOT).as_posix(),
            "source_language": "ref/config-tables/s_Language.xlsm",
            "descriptions": descriptions}


if __name__ == "__main__":
    result = build()
    if "--verify" in sys.argv:
        assert json.loads(OUTPUT.read_text(encoding="utf-8")) == result
    else:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print("PASS: four resource descriptions match Unity config and Chinese localization")
