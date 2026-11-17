"""Prepare upgrade page specs with all of the slots used at runtime."""

import copy
import json
import re

from CutItemSpec import SPEC_DIR


def read(name):
    return json.loads((SPEC_DIR / f"{name}.json").read_text(encoding="utf-8"))


def write(name, spec):
    (SPEC_DIR / f"{name}.json").write_text(
        json.dumps(spec, ensure_ascii=False, indent=1), encoding="utf-8"
    )


def find(nodes, name):
    return next(node for node in nodes if node["name"] == name)


def clone_slot(template, name, *, x=None, y=None, width=None, height=None, z=None):
    node = copy.deepcopy(template)
    node["name"] = name
    props = node["slot"]["props"]
    layout = props["LayoutData"]
    overrides = {"Left": x, "Top": y, "Right": width, "Bottom": height}
    for field, value in overrides.items():
        if value is None:
            continue
        pattern = rf"\b{field}=-?[\d.]+"
        layout, count = re.subn(pattern, f"{field}={value}", layout, count=1)
        if count == 0:
            layout = layout.replace("Offsets=(", f"Offsets=({field}={value},", 1)
    props["LayoutData"] = layout
    if z is not None:
        props["ZOrder"] = str(z)
    return node


def add_cost_slots(nodes):
    for index in range(2, 4):
        for kind, x in (("Icon", 686), ("Text", 732)):
            name = f"UpgradeCost{kind}_"
            nodes.append(clone_slot(find(nodes, name + "0"), name + str(index), x=x + index * 126))


def prepare_preview_pane():
    source = read("building_info")
    pane = copy.deepcopy(find(source["nodes"], "BuildingPreviewPane"))
    pane["slot"]["props"]["LayoutData"] = (
        "(Offsets=(Left=0.000000,Top=0.000000,Right=450.000000,Bottom=620.000000),"
        "Anchors=(Minimum=(X=0.000000,Y=0.000000),Maximum=(X=0.000000,Y=0.000000)),"
        "Alignment=(X=0.000000,Y=0.000000))"
    )

    image = find(pane["children"], "BuildingInfoImage")
    upgrade_image = copy.deepcopy(image)
    upgrade_image["name"] = "BuildingUpgradeImage"
    upgrade_image["props"]["Visibility"] = "Collapsed"
    pane["children"].append(upgrade_image)

    fallback = copy.deepcopy(find(source["nodes"], "BuildingInfoDescription"))
    fallback["name"] = "BuildingSourcePreviewUnavailable"
    fallback["props"]["ColorAndOpacity"] = (
        "(SpecifiedColor=(R=1.000000,G=1.000000,B=1.000000,A=1.000000))"
    )
    fallback["props"]["Font"] = (
        "(OutlineSettings=(OutlineColor=(R=0.000000,G=0.000000,B=0.000000,A=1.000000)),"
        "TypefaceFontName=\"Regular\",Size=15.000000,MonospacedWidth=1.000000)"
    )
    fallback["props"]["ShadowOffset"] = "(X=1.000000,Y=1.000000)"
    fallback["props"]["ShadowColorAndOpacity"] = "(R=0.000000,G=0.000000,B=0.000000,A=0.550000)"
    fallback["props"]["AutoWrapText"] = "False"
    fallback["props"]["Justification"] = "Center"
    fallback["props"]["Visibility"] = "Collapsed"
    fallback["slot"]["props"]["LayoutData"] = (
        "(Offsets=(Left=0.000000,Top=320.100006,Right=450.000000,Bottom=40.000000),"
        "Anchors=(Minimum=(X=0.000000,Y=0.000000),Maximum=(X=0.000000,Y=0.000000)),"
        "Alignment=(X=0.000000,Y=0.000000))"
    )
    fallback["slot"]["props"]["ZOrder"] = "2"
    pane["children"].append(fallback)

    result = {
        "version": source["version"],
        "label": "building_preview_pane",
        "root": "BuildingPreviewPaneWbpRoot",
        "nodes": [pane],
        "notes": [
            "Pane clipping, preview image brushes, image slot defaults and fallback text style are WBP-owned.",
            "C++ updates only the selected model texture, source-fit image geometry, and context-specific fallback Y.",
        ],
    }
    write("building_preview_pane", result)


def main():
    prepare_preview_pane()

    generic = read("building_upgrade")
    nodes = generic["nodes"]
    for index in range(1, 3):
        for kind in ("Label", "Current", "Delta"):
            name = f"UpgradeStat{kind}_"
            nodes.append(clone_slot(find(nodes, name + "0"), name + str(index), y=263 + 54 * index))
    add_cost_slots(nodes)
    nodes.append(clone_slot(find(nodes, "UpgradeCostIcon_0"), "InstantUpgradeGem",
                            x=768, y=691, width=28, height=28, z=5))
    write("page_building_upgrade", generic)

    barracks = read("building_upgrade_barracks")
    description = find(barracks["nodes"], "BarracksLevelDescriptionRoot")["children"]
    add_cost_slots(description)
    attributes = find(description, "BarracksUpgradeAttributes")
    attributes["name"] = "BarracksUpgradeAttributesSlot"
    attributes["class"] = "/Script/UMG.CanvasPanel"
    attributes["props"] = {"Visibility": "HitTestInvisible"}
    write("page_barracks_upgrade_source", barracks)

    # Other levels use the same barracks geometry with the ordinary attribute panel.
    special = {"UpgradeCurrentLevelRegion", "UpgradeNextLevelRegion", "UpgradeLevelArrow", "BarracksUpgradeAttributesSlot"}
    description[:] = [node for node in description if node["name"] not in special]
    standard = {"BuildingUpgradeTopFrame", "UpgradeCurrentLevel", "UpgradeNextLevel", "UpgradeLevelArrow"}
    description.extend(node for node in nodes if node["name"] in standard or node["name"].startswith("UpgradeStat"))
    write("page_barracks_upgrade", barracks)


if __name__ == "__main__":
    main()
