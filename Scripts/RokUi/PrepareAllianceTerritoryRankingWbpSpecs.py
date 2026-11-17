"""Build territory and ranking WBP specs from offline joined-alliance replays."""

from copy import deepcopy

from CutItemSpec import CANVAS_SLOT, shift_slot
from PrepareFeaturePageSpecs import canvas, empty_label, read_origin, read_spec, write_spec


def fill_slot(layer):
    return {"class": CANVAS_SLOT, "props": {
        "LayoutData": "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),"
                      "Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)))",
        "ZOrder": str(layer),
    }}


def node_map(source):
    return {node["name"]: node for node in source["nodes"]}


def find(nodes, name):
    return next(node for node in nodes if node["name"] == name)


def prepare_ranking():
    overview = read_spec("AllianceRankingOverview")
    detail = read_spec("AllianceRankingDetail")
    nodes = node_map(overview)
    detail_nodes = node_map(detail)
    sample_id = 300
    fields = ("Panel", "Name", "Icon", "Empty", "Hit")
    card = [deepcopy(nodes[f"AllianceRankingCategory{field}_{sample_id}"]) for field in fields]
    origin = read_origin(card[0])
    for node in card:
        shift_slot(node, origin)
    missing_icon = empty_label("RokAllianceRanking", "IconMissing", "IconMissing", "图标未接入", (194, 72), (135, 30), 15,
                               color=(0.82, 0.78, 0.68), layer=5)
    missing_icon["props"]["Visibility"] = "Collapsed"
    write_spec(overview, "feature_AllianceRankingReferenceCard", [*card, missing_icon])

    slots = []
    for index, category_id in enumerate(range(300, 306)):
        panel = nodes[f"AllianceRankingCategoryPanel_{category_id}"]
        slots.append(canvas(f"AllianceRankingCategorySlot_{index}", panel["slot"]))
        for field in fields:
            nodes.pop(f"AllianceRankingCategory{field}_{category_id}")
    overview_group = canvas("AllianceRankingOverview", fill_slot(3), [nodes.pop("AllianceRankingOverviewBoundary"), *slots])
    detail_names = ("DetailTitle", "EmptyListPanel", "EmptyList", "NoSelfData")
    detail_group = canvas("AllianceRankingDetail", fill_slot(3), [detail_nodes[f"AllianceRanking{name}"] for name in detail_names])
    detail_group["props"]["Visibility"] = "Collapsed"
    unavailable = empty_label("RokAllianceRanking", "AllianceRankingCatalogUnavailable", "CatalogUnavailable",
                              "排行榜分类暂不可用", (250, 360), (940, 55), 22, color=(0.88, 0.84, 0.76), layer=6)
    unavailable["props"]["Visibility"] = "Collapsed"
    write_spec(overview, "feature_AllianceRankingReferencePage", [*nodes.values(), overview_group, detail_group, unavailable])


def territory_scroll_content(nodes):
    scroll = nodes["AllianceTerritoryReferenceScroll"]
    extent, = scroll["children"]
    content, = extent["children"]
    return scroll, extent, content


def size_box(reference, name, height, content):
    holder = {**reference, "name": name, "props": {**reference["props"], "HeightOverride": str(height)}}
    holder["children"] = [content]
    return holder


def prepare_territory():
    overview = read_spec("AllianceTerritoryCollapsed")
    detail = read_spec("AllianceTerritoryBuildingDetail")
    nodes, detail_nodes = node_map(overview), node_map(detail)
    scroll, extent, content = territory_scroll_content(nodes)
    children = []
    for index in range(4):
        expanded_nodes = node_map(read_spec(f"AllianceTerritoryGroup{index}"))
        _, expanded_extent, expanded_content = territory_scroll_content(expanded_nodes)
        header_name = f"AllianceTerritoryGroup_{index}"
        header = deepcopy(find(content["children"], header_name))
        y = read_origin(header)[1]
        next_name = f"AllianceTerritoryGroup_{index + 1}"
        header_end = (read_origin(find(content["children"], next_name))[1] if index < 3
                      else float(extent["props"]["HeightOverride"]))
        header["slot"] = deepcopy(content["slot"])
        expanded_header = find(expanded_content["children"], header_name)
        expanded_bg = deepcopy(find(expanded_header["children"], f"AllianceTerritoryGroupBg_{index}"))
        expanded_bg["name"] = f"AllianceTerritoryGroupBgExpanded_{index}"
        expanded_bg["props"]["Visibility"] = "Collapsed"
        header["children"].insert(1, expanded_bg)
        children.append(size_box(extent, f"AllianceTerritoryHeaderSize_{index}", header_end - y, header))

        row = deepcopy(find(expanded_content["children"], f"AllianceTerritoryRow_{index}"))
        row_top = read_origin(row)[1]
        row_end = (read_origin(find(expanded_content["children"], next_name))[1] if index < 3
                   else float(expanded_extent["props"]["HeightOverride"]))
        shift_slot(row, (0, row_top))
        row_root = canvas(f"AllianceTerritoryExpandedLayout_{index}", content["slot"], [row])
        holder = size_box(extent, f"AllianceTerritoryExpandedSize_{index}", row_end - row_top, row_root)
        holder["props"]["Visibility"] = "Collapsed"
        children.append(holder)
    scroll["children"] = children
    selection = nodes.pop("AllianceTerritoryReferenceSelection")
    overview_group = canvas("AllianceTerritoryOverview", fill_slot(4), [nodes.pop(scroll["name"]), selection])
    detail_group = canvas("AllianceTerritoryDetail", fill_slot(4), [detail_nodes[name] for name in (
        "AllianceTerritoryReturnToDefinitions", "AllianceTerritoryDefinitionDetailScroll",
    )])
    detail_group["props"]["Visibility"] = "Collapsed"
    write_spec(overview, "feature_AllianceTerritoryReferencePage", [*nodes.values(), overview_group, detail_group])


if __name__ == "__main__":
    prepare_ranking()
    prepare_territory()
