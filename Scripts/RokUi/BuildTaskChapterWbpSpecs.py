"""Turn the captured server chapter layout into a page, row and two reward templates."""

from copy import deepcopy
import re

from CutItemSpec import CANVAS_SLOT, shift_slot, walk
from PrepareFeaturePageSpecs import canvas, find_node, read_origin, read_spec, write_spec


PREFIX = "TaskChapterServer"


def slot(x, y, width, height, z=0):
    return {
        "class": CANVAS_SLOT,
        "props": {
            "LayoutData": f"(Offsets=(Left={x},Top={y},Right={width},Bottom={height}))",
            "ZOrder": str(z),
        },
    }


def detached(node, name):
    result = deepcopy(node)
    result["name"] = name
    return result


def build_row(mixed):
    scroll = find_node(mixed["nodes"], "TaskChapterScroll")
    sources = {child["name"]: child for child in scroll["children"]}
    sample = detached(sources["TaskChapterTask_102002Size"], "Size")
    sample["slot"] = slot(0, 0, 737, 104)
    row = find_node([sample], "TaskChapterTask_102002Row")
    row["name"] = "Row"
    actions = [
        detached(find_node([sources["TaskChapterTask_102003Size"]], "TaskChapterTask_102003Go"), "Go"),
        detached(find_node([sources["TaskChapterTask_102001Size"]], "TaskChapterTask_102001State"), "State"),
    ]
    for node in list(walk(row["children"])):
        name = node["name"]
        if name == "TaskChapterTaskDescription_102002":
            node["name"] = "Description"
        elif name.startswith("TaskChapterTask_102002"):
            node["name"] = name.removeprefix("TaskChapterTask_102002")
    art = [child for child in row["children"] if child["name"].startswith("TaskChapterTaskReward_")]
    icon = detached(next(child for child in art if child["name"].endswith("_0Icon")), "Icon")
    quantity = detached(next(child for child in art if child["name"].endswith("_0Quantity")), "Quantity")
    origin = read_origin(icon)
    for child in (icon, quantity):
        shift_slot(child, origin)
    write_spec(mixed, f"feature_{PREFIX}TaskReward", [icon, quantity])
    for child in art:
        row["children"].remove(child)
    row["children"].append(canvas("TaskRewardHost", slot(*origin, 600, 29, 2)))
    for node in walk(actions):
        node["name"] = node["name"].removeprefix("TaskChapterTask_102003")
    row["children"].extend(actions)
    write_spec(mixed, f"feature_{PREFIX}Row", [sample])


def build_page(mixed, waiting, empty, complete):
    nodes = deepcopy(mixed["nodes"])
    chapter = find_node(nodes, "TaskChapterRoot")
    aggregate = [child for child in chapter["children"] if child["name"].startswith("TaskChapterAggregateReward_")]
    cell = [detached(node, node["name"].removeprefix("TaskChapterAggregateReward_2_0"))
            for node in aggregate if node["name"].startswith("TaskChapterAggregateReward_2_0")]
    origin = read_origin(next(node for node in cell if node["name"] == "Quality"))
    for node in cell:
        shift_slot(node, origin)
    write_spec(mixed, f"feature_{PREFIX}AggregateReward", cell)
    for node in aggregate:
        chapter["children"].remove(node)
    chapter["children"].append(canvas("TaskChapterAggregateRewardHost", slot(*origin, 695, 55, 1)))

    scroll = find_node(nodes, "TaskChapterScroll")
    scroll["children"] = [deepcopy(find_node(empty["nodes"], "TaskChapterServerEmptySize"))]
    chapter["children"].extend([
        detached(find_node(complete["nodes"], "TaskChapterMetaNotice"), "TaskChapterClaimProgress"),
        deepcopy(find_node(complete["nodes"], "TaskChapterClaimAction")),
    ])
    pending = detached(find_node(waiting["nodes"], "TaskChapterMetaNotice"), "TaskChapterPendingMessage")
    chapter["children"] = [canvas("TaskChapterContent", slot(0, 0, 737, 632), chapter["children"]), pending]

    guide_names = {
        "TaskChapterDialogPanel", "TaskChapterDialogTop", "TaskChapterDialogBottom",
        "TaskChapterDialogArrow", "TaskChapterDialogue", "TaskChapterGuide",
    }
    guide = [node for node in nodes if node["name"] in guide_names]
    if len(guide) != len(guide_names):
        raise ValueError("Expected the original chapter story and model in the mixed capture")
    guide_layout = canvas("TaskChapterGuideLayout", slot(0, 0, 1440, 810, 3))
    for node in guide:
        if node["name"] == "TaskChapterGuide":
            guide_layout["children"].append(canvas("TaskChapterGuideModelHost", node["slot"]))
        else:
            if node["name"] == "TaskChapterDialogue":
                node["props"].pop("TextStyleSet", None)
                node["props"].pop("Text", None)
            guide_layout["children"].append(node)
        nodes.remove(node)
    nodes.append(guide_layout)
    for index in range(3):
        find_node(nodes, f"TaskPageBadge_{index}")
        find_node(nodes, f"TaskPageBadgeCount_{index}")
    write_spec({**mixed, "notes": []}, f"feature_{PREFIX}Page", nodes)


def main():
    mixed, waiting, empty, complete = (read_spec(f"{PREFIX}{state}") for state in ("Mixed", "Waiting", "Empty", "Complete"))
    build_row(mixed)
    build_page(mixed, waiting, empty, complete)


if __name__ == "__main__":
    main()
