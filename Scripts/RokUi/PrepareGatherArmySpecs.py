"""Convert native world gather observations into three editable presentation specs."""

from copy import deepcopy
import json
from pathlib import Path
import re


SPECS = Path(__file__).resolve().parents[2] / "Saved/RokUi/Specs"


def read(name):
    return json.loads((SPECS / f"gather_army_reference_{name}.json").read_text(encoding="utf-8-sig"))


def write(name, source, nodes):
    spec = {"version": source["version"], "label": name, "root": name, "nodes": nodes}
    (SPECS / f"{name}.json").write_text(json.dumps(spec, ensure_ascii=False, indent=1), encoding="utf-8")


def find(nodes, name):
    for node in nodes:
        if node["name"] == name:
            return node
        match = find(node.get("children", []), name)
        if match:
            return match
    return None


def require(nodes, name):
    found = find(nodes, name)
    if found is None:
        raise ValueError(f"Native observation lacks {name}")
    return found


def canvas_slot(x, y, width, height, z=0):
    return {"class": "/Script/UMG.CanvasPanelSlot", "props": {
        "LayoutData": f"(Offsets=(Left={x},Top={y},Right={width},Bottom={height}),Anchors=(Minimum=(X=0.0,Y=0.0),Maximum=(X=0.0,Y=0.0)),Alignment=(X=0.0,Y=0.0))",
        "ZOrder": str(z),
    }}


def first_row(scroll, prefix):
    return next(row for row in scroll["children"] if row["name"].startswith(prefix))


def emit_row(name, source, row, width, height, prefix):
    row = deepcopy(row)
    row["slot"] = canvas_slot(0, 0, width, height)
    match = re.search(r"(_\d+)$", row["name"])
    if match is None:
        raise ValueError(row["name"])
    suffix = match.group(1)
    write(name, source, [row])
    return f"-Prefix={prefix} -Suffix={suffix}"


def main():
    army = read("main")
    deputy = read("deputy_picker")
    selected = read("deputy_selected")
    layout = deepcopy(require(army["nodes"], "ExpeditionArmyRoot")["children"])
    troops = require(layout, "ExpeditionArmyTroops")
    soldier = first_row(troops, "ExpeditionSoldierSize_")
    troops["children"] = []

    chooser = deepcopy(require(deputy["nodes"], "ExpeditionHeroChoiceScroll"))
    hero = first_row(chooser, "ExpeditionHeroChoiceSize_")
    button_label = next(node for node in hero["children"][0]["children"] if node["name"].startswith("ExpeditionHeroChoiceButton_"))["children"][0]["children"]
    next(node for node in button_label if node["name"].endswith("Label"))["name"] = "Label"
    chooser["children"] = [require(chooser["children"], "GatherRemoveDeputy")]
    layout.append(chooser)
    for name in ("GatherDeputyHead", "GatherDeputyHeadFrame"):
        layout.append(deepcopy(require(selected["nodes"], name)))

    # The successful fixture necessarily has a commander and troop rows. These
    # labels keep the original no-catalog and empty-row locations editable too.
    unavailable = deepcopy(require(layout, "ExpeditionArmyCommanderName"))
    unavailable["name"] = "GatherCommanderUnavailable"
    unavailable["slot"] = canvas_slot(202, 144, 391, 46, 7)
    unavailable["props"]["Text"] = 'INVTEXT("统帅数据尚未同步")'
    unavailable["props"]["Justification"] = "Center"
    unavailable["props"]["Font"] = re.sub(r"\bSize=[\d.]+", "Size=23.000000", unavailable["props"]["Font"])
    layout.append(unavailable)
    empty = deepcopy(unavailable)
    empty["name"] = "ExpeditionNoAvailableSoldiers"
    empty["slot"] = canvas_slot(651, 248, 550, 38, 7)
    empty["props"]["Text"] = 'INVTEXT("暂无可编入远征的部队")'
    empty["props"]["Font"] = re.sub(r"\bSize=[\d.]+", "Size=19.000000", empty["props"]["Font"])
    layout.append(empty)

    # Gear or catalog state can omit some of the five source skill nodes.
    for prefix, x, y, width, height in (("ExpeditionArmySkill", 197, 662, 58, 58), ("GatherSkillLevel", 201, 697, 32, 24)):
        prototype = require(layout, f"{prefix}_0")
        for index in range(1, 5):
            name = f"{prefix}_{index}"
            if find(layout, name) is None:
                clone = deepcopy(prototype)
                clone["name"] = name
                clone["slot"] = canvas_slot(x + 68 * index, y, width, height, 7)
                layout.append(clone)

    write("gather_army_page", army, layout)
    print(emit_row("gather_army_hero_row", deputy, hero, 1000, 96, "ExpeditionHeroChoice"))
    print(emit_row("gather_army_soldier_row", army, soldier, 580, 101, "ExpeditionSoldier"))


if __name__ == "__main__":
    main()
