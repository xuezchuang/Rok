"""Build the technology reference page and its scrolling row from the live replay."""

from copy import deepcopy

from CutItemSpec import CANVAS_SLOT
from PrepareFeaturePageSpecs import canvas, empty_label, read_spec, write_spec


def fill_slot(layer):
    return {"class": CANVAS_SLOT, "props": {
        "LayoutData": "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),"
                      "Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)))",
        "ZOrder": str(layer),
    }}


def tab_content(button):
    content, = button["children"]
    label, = [widget for widget in content["children"] if widget["name"] == button["name"] + "Label"]
    return content, label


def make_tabs(captures):
    result = []
    for index, selected_capture in enumerate(captures):
        name = f"AllianceTechnologyCategory_{index + 1}"
        default = deepcopy(captures[0][name])
        content, default_label = tab_content(default)
        _, selected = tab_content(selected_capture[name])
        _, normal = tab_content(captures[(index + 1) % len(captures)][name])
        selected, normal = deepcopy(selected), deepcopy(normal)
        selected["name"] = name + "SelectedLabel"
        hidden = normal if index == 0 else selected
        hidden["props"]["Visibility"] = "Collapsed"
        content["children"].remove(default_label)
        content["children"].extend((normal, selected))
        result.append(default)
    return result


def split_row(source, scroll):
    extent, = scroll["children"]
    list_view, = extent["children"]
    first, second, *_ = list_view["children"]
    row = deepcopy(first)
    sample = row["name"].rsplit("_", 1)[1]
    second_id = second["name"].rsplit("_", 1)[1]
    selected, = [node for node in row["children"] if node["name"] == f"AllianceTechnologyDefinitionBg_{sample}"]
    regular, = [node for node in second["children"] if node["name"] == f"AllianceTechnologyDefinitionBg_{second_id}"]
    regular = deepcopy(regular)
    regular["name"] = selected["name"]
    regular["props"]["Visibility"] = "Collapsed"
    selected["name"] = "SelectedBg"
    row["children"].insert(0, regular)
    write_spec(source, "feature_AllianceTechnologyReferenceRow", [row])
    scroll["children"] = []
    return sample


def main():
    sources = [read_spec(f"AllianceTechnologyReference{state}") for state in ("Develop", "Territory", "War")]
    captures = [{node["name"]: node for node in source["nodes"]} for source in sources]
    tabs = make_tabs(captures)
    nodes = deepcopy(captures[0])
    for tab in tabs:
        nodes.pop(tab["name"])
    panels = [nodes.pop(name) for name in ("AllianceTechnologyListPanel", "AllianceTechnologyDetailPanel")]
    scroll = nodes.pop("AllianceTechnologyDefinitionScroll")
    sample = split_row(sources[0], scroll)
    detail_children = [nodes.pop("AllianceTechnology" + name) for name in (
        "DetailTitle", "DetailIcon", "DetailIdentity", "DetailDescription", "DetailPrerequisites",
        "ConfiguredPrevious", "ConfiguredNext",
    )]
    no_icon = empty_label("RokAlliance", "AllianceTechnologyIconUnavailable", "TechnologyIconUnavailable",
                          "源图标资源不可用", (640, 187), (108, 52), 15, color=(0.84, 0.76, 0.63), layer=6)
    no_icon["props"]["Visibility"] = "Collapsed"
    details = canvas("AllianceTechnologyReferenceDetail", fill_slot(5), [*detail_children, no_icon])
    body = canvas("AllianceTechnologyReferenceDefinitions", fill_slot(3), [*tabs, *panels, scroll, details])
    missing = empty_label("RokAlliance", "AllianceTechnologyReferenceUnavailable", "TechnologyReferenceUnavailable",
                          "联盟科技静态配置不可用；未读取账户研究状态", (250, 365), (900, 50), 20,
                          color=(0.88, 0.82, 0.70), layer=6)
    missing["props"]["Visibility"] = "Collapsed"
    write_spec(sources[0], "feature_AllianceTechnologyReferencePage", [*nodes.values(), body, missing])
    print(sample)


if __name__ == "__main__":
    main()
