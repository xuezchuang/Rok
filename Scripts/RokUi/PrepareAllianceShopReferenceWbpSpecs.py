"""Derive the alliance shop reference page and item row from the offline HUD replay."""

from copy import deepcopy
import re

from CutItemSpec import CANVAS_SLOT, shift_slot
from PrepareFeaturePageSpecs import canvas, empty_label, read_origin, read_spec, write_spec


def full_size_slot(layer):
    return {"class": CANVAS_SLOT, "props": {
        "LayoutData": "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),"
                      "Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)))",
        "ZOrder": str(layer),
    }}


def rows_by_slot(nodes, expected_count):
    rows = [node for node in nodes.values() if re.fullmatch(r"AllianceShopRow_\d+", node["name"])]
    if len(rows) != expected_count:
        raise ValueError(f"Expected {expected_count} shop rows, got {len(rows)}")
    return sorted(rows, key=lambda node: read_origin(node)[1])


def tab_label(button):
    content, = button["children"]
    label, = [node for node in content["children"] if node["name"] == button["name"] + "Label"]
    return content, label


def make_tabs(captures):
    tabs = []
    for index, capture in enumerate(captures):
        name = f"AllianceShopGroup_{index + 1}"
        tab = deepcopy(captures[0].pop(name))
        content, default = tab_label(tab)
        if index == 0:
            _, normal = tab_label(captures[1][name])
            content["children"].remove(default)
            content["children"].append(deepcopy(normal))
            selected = default
        else:
            _, selected = tab_label(capture[name])
            selected = deepcopy(selected)
        selected["name"] = name + "SelectedLabel"
        if index == 0:
            content["children"][-1]["props"]["Visibility"] = "Collapsed"
        else:
            selected["props"]["Visibility"] = "Collapsed"
        content["children"].append(selected)
        tabs.append(tab)
    return tabs


def main():
    sources = [read_spec(f"AllianceShopReferenceGroup{group}") for group in range(1, 5)]
    captures = [{node["name"]: node for node in source["nodes"]} for source in sources]
    nodes = captures[0]
    first_rows = rows_by_slot(nodes, 4)
    all_slots = rows_by_slot(captures[3], 9)
    sample_id = first_rows[0]["name"].rsplit("_", 1)[1]
    sample = [deepcopy(nodes[f"AllianceShop{field}_{sample_id}"]) for field in ("Row", "Icon", "Name", "Select")]
    normal = deepcopy(first_rows[1])
    shift_slot(normal, read_origin(normal))
    normal["name"] = sample[0]["name"]
    normal["props"]["Visibility"] = "Collapsed"
    for child in sample:
        shift_slot(child, read_origin(first_rows[0]))
    sample[0]["name"] = "SelectedRow"
    write_spec(sources[0], "feature_AllianceShopReferenceRow", [normal, *sample])

    tabs = make_tabs(captures)
    panels = [nodes.pop(name) for name in ("AllianceShopListPanel", "AllianceShopDetailPanel")]
    detail_names = ("Quality", "Icon", "Name", "Identity", "BuyConfigPrice", "StockConfigPrice", "ServerBoundary")
    detail = canvas("AllianceShopReferenceDetail", full_size_slot(4),
                    [nodes.pop(f"AllianceShopDetail{name}") for name in detail_names])
    for row in first_rows:
        item_id = row["name"].rsplit("_", 1)[1]
        for field in ("Row", "Icon", "Name", "Select"):
            nodes.pop(f"AllianceShop{field}_{item_id}", None)
    slots = [canvas(f"AllianceShopReferenceSlot_{index}", row["slot"])
             for index, row in enumerate(all_slots)]
    definitions = canvas("AllianceShopReferenceDefinitions", full_size_slot(3), [*tabs, *panels, *slots, detail])
    missing = empty_label("RokAllianceShop", "AllianceShopReferenceUnavailable", "ReferenceUnavailable",
                          "联盟商店静态配置不可用；未读取库存或购买状态", (250, 365), (900, 50), 20,
                          color=(0.88, 0.82, 0.70), layer=6)
    missing["props"]["Visibility"] = "Collapsed"
    write_spec(sources[0], "feature_AllianceShopReferencePage", [*nodes.values(), definitions, missing])
    print(sample_id)


if __name__ == "__main__":
    main()
