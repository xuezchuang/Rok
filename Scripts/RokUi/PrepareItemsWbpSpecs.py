"""Derive the bag page, scroll row, and stack cell from two runtime captures."""

from copy import deepcopy
import re

from PrepareFeaturePageSpecs import (
    CANVAS_SLOT, add_tab_visual_states, empty_label, read_spec, write_spec,
)


def by_name(nodes, name):
    return next(node for node in nodes if node["name"] == name)


def main():
    source = read_spec("ItemsFixture")
    empty = read_spec("ItemsEmpty")
    page = deepcopy(source["nodes"])
    scroll = by_name(page, "ItemsInventoryScroll")
    row_size = by_name(scroll["children"], "ItemsInventoryRowSize_0")
    row = deepcopy(by_name(row_size["children"], "ItemsInventoryRow_0"))
    row["name"] = "InventoryRow"
    row["props"] = {"Visibility": "SelfHitTestInvisible"}
    width = row_size["props"]["WidthOverride"]
    height = row_size["props"]["HeightOverride"]
    row["slot"] = {"class": CANVAS_SLOT, "props": {
        "LayoutData": f"(Offsets=(Right={width},Bottom={height}))",
    }}
    if len(row["children"]) != 4:
        raise SystemExit("Items fixture must contain a complete four-column inventory row.")
    cell = deepcopy(by_name(row["children"], "ItemsCell_0")["children"])
    expected_cell = {"ItemsCellSelect_0", "ItemsCellQuality_0", "ItemsCellIcon_0", "ItemsCellTop_0",
                     "ItemsCellTopText_0", "ItemsCellQuantity_0", "ItemsCellHit_0"}
    if {node["name"] for node in cell} != expected_cell:
        raise SystemExit("Items fixture must include icon, top label and selected state in its first cell.")
    for index, host in enumerate(row["children"]):
        if host["name"] != f"ItemsCell_{index}":
            raise SystemExit("Items fixture inventory row has unexpected column order.")
        host["name"] = f"CellSlot_{index}"
        host["props"] = {"Visibility": "SelfHitTestInvisible"}
        host["children"] = []
    write_spec(source, "feature_ItemsInventoryRow", [row])
    write_spec(source, "feature_ItemsCell", cell)

    scroll["children"] = []
    # The source places the partial-inventory list 32 design units lower and shortens it by 32.
    bounds = {"name": "ItemsInventoryPartialBounds", "class": "/Script/UMG.CanvasPanel",
              "props": {"Visibility": "Collapsed"}, "slot": deepcopy(scroll["slot"])}
    layout = bounds["slot"]["props"]["LayoutData"]
    layout = re.sub(r"Top=(-?[\d.]+)", lambda m: f"Top={float(m[1]) + 32.0:.6f}", layout, count=1)
    layout = re.sub(r"Bottom=(-?[\d.]+)", lambda m: f"Bottom={float(m[1]) - 32.0:.6f}", layout, count=1)
    bounds["slot"]["props"]["LayoutData"] = layout
    page.append(bounds)
    page.append(deepcopy(by_name(empty["nodes"], "ItemsInventoryEmptyState")))
    page.append(empty_label("RokItems", "ItemsInventorySyncState", "InventoryPartial", "部分道具配置待同步",
                            (593, 210 - 45), (630, 32), 16, color=(0.96, 0.78, 0.43), layer=12))
    detail = by_name(page, "ItemsDetailPanel")
    detail["children"].append(empty_label("RokItems", "ItemsRoutedUse", "RoutedUseLabel", "请在对应功能中使用",
                                         (86, 534), (240, 40), 17, color=(0.26, 0.23, 0.17), layer=6))
    add_tab_visual_states(page, "ItemsCategory_", 0, 1, 5)
    write_spec(source, "feature_ItemsPage", page)


if __name__ == "__main__":
    main()
