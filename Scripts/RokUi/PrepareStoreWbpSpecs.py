"""Split the runtime Store capture into editable layout and product cell specs."""

from copy import deepcopy

from PrepareFeaturePageSpecs import add_tab_visual_states, read_spec, write_spec


def main():
    source = read_spec("Store")
    page = deepcopy(source["nodes"])
    cells = {node["name"]: node for node in page if node["name"].startswith("StoreCell_")}
    expected_names = {f"StoreCell_{index}" for index in range(12)}
    if cells.keys() != expected_names:
        raise SystemExit("Store baseline must contain the full first resources page (12 products).")

    product = cells["StoreCell_0"]["children"]
    expected_children = {"StoreCellQuality_0", "StoreCellSelected_0", "StoreCellName_0", "StoreCellPrice_0", "StoreCellHit_0"}
    if {node["name"] for node in product} != expected_children:
        raise SystemExit("First product has unexpected children or was captured before selection.")
    write_spec(source, "feature_StoreCell", product)

    for cell in cells.values():
        cell["children"] = []
        cell.setdefault("props", {})["Visibility"] = "SelfHitTestInvisible"

    add_tab_visual_states(page, "StoreCategory_", 0, 1, 4)
    write_spec(source, "feature_StorePage", page)


if __name__ == "__main__":
    main()
