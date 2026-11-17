"""Build page and reusable row/card specs from the captured feature mode trees."""

from copy import deepcopy
import json
import re
import sys

from CutItemSpec import CANVAS_SLOT, parse_fields, shift_slot, spec_path, walk


def read_spec(mode):
    return json.loads(spec_path(f"feature_{mode}").read_text(encoding="utf-8"))


def write_spec(source, name, nodes):
    result = {**source, "root": "PageRoot", "nodes": nodes}
    spec_path(name).write_text(json.dumps(result, ensure_ascii=False, indent=1), encoding="utf-8")


def read_origin(node):
    layout = node["slot"]["props"]["LayoutData"]
    offsets = parse_fields(re.search(r"Offsets=\(([^)]*)\)", layout).group(1))
    return offsets.get("Left", 0), offsets.get("Top", 0)


def canvas(name, slot, children=()):
    return {
        "name": name,
        "class": "/Script/UMG.CanvasPanel",
        "props": {"Visibility": "SelfHitTestInvisible"},
        "slot": deepcopy(slot),
        "children": list(children),
    }


def split_items(source, prefix, shade_name, fields, indices, templates):
    nodes = {node["name"]: node for node in source["nodes"]}
    for name, index in templates.items():
        origin = read_origin(nodes[f"{shade_name}{index}"])
        children = [deepcopy(nodes[f"{field}{index}"]) for field in fields]
        for child in children:
            shift_slot(child, origin)
        write_spec(source, name, children)

    for slot_index, index in enumerate(indices):
        for field in fields:
            nodes.pop(f"{field}{index}")
        shade = next(node for node in source["nodes"] if node["name"] == f"{shade_name}{index}")
        slot_name = f"{prefix}Slot_{slot_index}"
        nodes[slot_name] = canvas(slot_name, shade["slot"])
    return list(nodes.values())


def empty_label(namespace, name, key, text, point, size, font_size, *,
                color=(0.72, 0.72, 0.70), bold=False, layer=5):
    x, y = point
    width, height = size
    red, green, blue = color
    face = "Bold" if bold else "Regular"
    return {
        "name": name,
        "class": "/Script/UMG.TextBlock",
        "props": {
            "Text": f'NSLOCTEXT("{namespace}", "{key}", "{text}")',
            "Font": f'(TypefaceFontName="{face}",Size={font_size}.000000)',
            "ColorAndOpacity": f"(SpecifiedColor=(R={red:.6f},G={green:.6f},B={blue:.6f},A=1.000000))",
            "Justification": "Center",
            "ShadowOffset": "(X=1.000000,Y=1.000000)",
            "ShadowColorAndOpacity": "(R=0.000000,G=0.000000,B=0.000000,A=0.650000)",
            "Visibility": "HitTestInvisible",
        },
        "fonts": {"Font": "EngineDefault"},
        "slot": {"class": CANVAS_SLOT, "props": {
            "LayoutData": f"(Offsets=(Left={x},Top={y},Right={width},Bottom={height}))",
            "ZOrder": str(layer),
        }},
    }


def add_tab_visual_states(page, prefix, selected_index, normal_index, count):
    name_pattern = rf"{prefix}\d+"
    tabs = [node for node in page if re.fullmatch(name_pattern, node["name"])]
    assert len(tabs) == count
    tab_by_index = {int(node["name"].removeprefix(prefix)): node for node in tabs}
    visual_templates = [
        deepcopy(tab_by_index[index]["children"][0]["children"])
        for index in (selected_index, normal_index)
    ]
    for tab in tabs:
        content, = tab["children"]
        label, = [node for node in walk(content["children"]) if node["name"].endswith("Label")]
        visuals = []
        for state, template in zip(("Selected", "Normal"), visual_templates):
            children = deepcopy(template)
            style_root = f'{tab["name"]}{state}'
            for node in walk(children):
                node["name"] = re.sub(rf"^{name_pattern}", style_root, node["name"])
                if node["name"].endswith("Label"):
                    node["props"]["Text"] = label["props"]["Text"]
            visual = canvas(style_root, tab["slot"], children)
            shift_slot(visual, read_origin(tab))
            initially_selected = tab["name"] == f"{prefix}{selected_index}"
            if initially_selected != (state == "Selected"):
                visual["props"]["Visibility"] = "Collapsed"
            visuals.append(visual)
        content["children"] = visuals


def prepare_markers():
    source = read_spec("MapMarkers")
    page = split_items(source, "MapMarkerRow", "MapMarkerRowShade_", (
        "MapMarkerRowShade_", "MapMarkerName_", "MapMarkerDescription_", "MapMarkerRowHit_",
    ), range(4), {"feature_MapMarkerRow": 1, "feature_MapMarkerRowSelected": 0})

    add_tab_visual_states(page, "MapMarkerPage_", 0, 1, 5)
    page.append(empty_label("RokMapMarker", "MapMarkerEmpty", "Empty", "当前分类没有地图标记", (360, 385), (720, 50), 22))
    write_spec(source, "feature_MapMarkersPage", page)


def prepare_gems():
    source = read_spec("GemShop")
    page = split_items(source, "GemShopPackage", "GemShopPackageShade_", (
        "GemShopPackageShade_", "GemShopPackageIcon_", "GemShopPackageAmount_",
        "GemShopPackageBonus_", "GemShopPackageProduct_", "GemShopBuy_",
    ), range(6), {"feature_GemShopPackage": 0})
    page.append(empty_label("RokGemShop", "GemShopCatalogMissing", "CatalogMissing", "宝石礼包配置未加载，无法展示商品。", (360, 355), (720, 40), 18))
    write_spec(source, "feature_GemShopPage", page)


def prepare_charge():
    source = read_spec("Charge")
    service = read_spec("ChargeService")
    page = split_items(source, "ChargeGemPackage", "ChargeGemPackageShade_", (
        "ChargeGemPackageShade_", "ChargeGemPackageIcon_", "ChargeGemPackageAmount_",
        "ChargeGemPackageBonus_", "ChargeGemPackageProduct_", "ChargeGemPackageBuy_",
    ), range(6), {"feature_ChargeGemPackage": 0})
    add_tab_visual_states(page, "ChargePage_", 107, 101, 7)
    page.append(empty_label("RokCharge", "ChargeGemCatalogEmptyState", "GemCatalogEmpty",
                            "RechargeGemMall 目录尚未加载，未显示虚构礼包。", (510, 375), (650, 48), 22,
                            color=(0.83, 0.80, 0.76), bold=True, layer=6))
    write_spec(source, "feature_ChargePage", page)

    service_names = {"ChargePageServicePanel", "ChargeSelectedPageServiceBoundary", "ChargePageBoundaryAction"}
    service_nodes = [node for node in service["nodes"] if node["name"] in service_names]
    assert len(service_nodes) == len(service_names)
    write_spec(service, "feature_ChargeServicesOnly", service_nodes)


def prepare_alliance_office():
    source = read_spec("AllianceOffice")
    page = split_items(source, "AllianceOfficeReferenceRow", "AllianceOfficeReferenceRow_", (
        "AllianceOfficeReferenceRow_", "AllianceOfficeReferenceIcon_", "AllianceOfficeReferenceName_",
        "AllianceOfficeReferenceDescription_", "AllianceOfficeReferenceBonus_", "AllianceOfficeReferenceHit_",
    ), (1001, 2001, 3001, 4001), {"feature_AllianceOfficeRow": 2001, "feature_AllianceOfficeRowSelected": 1001})
    page.append(empty_label("RokAlliance", "AllianceOfficeReferenceUnavailable", "OfficeDefinitionsUnavailable",
                            "官职静态配置暂不可用；未读取任命或权限状态", (250, 395), (900, 38), 18,
                            color=(0.82, 0.80, 0.76), layer=6))
    write_spec(source, "feature_AllianceOfficePage", page)


def move_scroll_item_to_canvas(row):
    width = row["props"]["WidthOverride"]
    height = row["props"]["HeightOverride"]
    row["slot"] = {"class": CANVAS_SLOT, "props": {
        "LayoutData": f"(Offsets=(Left=0,Top=0,Right={width},Bottom={height}),"
                      "Anchors=(Minimum=(X=0,Y=0),Maximum=(X=0,Y=0)),Alignment=(X=0,Y=0))"
    }}


def prepare_task_chapter_catalog():
    source = read_spec("TaskChapterCatalog")
    missing_source = read_spec("TaskChapterCatalogGroup7")
    chapter, = [node for node in source["nodes"] if node["name"] == "TaskChapterRoot"]
    missing_chapter, = [node for node in missing_source["nodes"] if node["name"] == "TaskChapterRoot"]
    missing, = [node for node in missing_chapter["children"]
                if node["name"] == "TaskChapterAggregateRewardUnavailable"]
    missing["props"]["Visibility"] = "Collapsed"

    scroll, = [node for node in chapter["children"] if node["name"] == "TaskChapterScroll"]
    row_prefix = "TaskChapterTaskRowSize_"
    rows = [node for node in scroll["children"] if node["name"].startswith(row_prefix)]
    assert rows
    row_sample = deepcopy(rows[0])
    row_id = row_sample["name"].removeprefix(row_prefix)
    assert row_id.isdecimal()
    task_row, = [node for node in walk((row_sample,)) if node["name"] == f"TaskChapterTaskRow_{row_id}"]

    def cut_reward(parent, prefix, output, host_name, host_width):
        cells = [node for node in parent["children"] if re.fullmatch(f"{prefix}_[0-9]+_[0-9]+", node["name"])]
        assert cells, prefix
        cell = deepcopy(cells[0])
        old_suffix = cell["name"].removeprefix(prefix)
        for node in walk((cell,)):
            node["name"] = node["name"].replace(old_suffix, "_0_0", 1)
        shift_slot(cell, read_origin(cell))
        write_spec(source, output, [cell])

        slot = deepcopy(cells[0]["slot"])
        _, top = read_origin(cells[0])
        layout = slot["props"]["LayoutData"]
        offsets = parse_fields(re.search(r"Offsets=\(([^)]*)\)", layout).group(1))
        slot["props"]["LayoutData"] = (
            f"(Offsets=(Left=0,Top={top},Right={host_width},Bottom={offsets['Bottom']}))"
        )
        parent["children"] = [node for node in parent["children"] if node not in cells]
        parent["children"].append(canvas(host_name, slot))

    cut_reward(chapter, "TaskChapterAggregateReward", "feature_TaskChapterCatalogAggregateReward",
               "TaskChapterAggregateRewardHost", 1040)
    cut_reward(task_row, "TaskChapterTaskReward", "feature_TaskChapterCatalogTaskReward",
               "TaskRewardHost", 733)

    for node in walk((row_sample,)):
        node["name"] = node["name"].replace(f"_{row_id}", "_0", 1)
    move_scroll_item_to_canvas(row_sample)
    write_spec(source, "feature_TaskChapterCatalogRow", [row_sample])
    scroll["children"] = []
    chapter["children"].append(missing)
    write_spec(source, "feature_TaskChapterCatalogPage", source["nodes"])


def task_row_reward_elements(sample_slot):
    hosts = []
    for index in range(4):
        host = canvas(f"DailyRewardSlot{index}", sample_slot)
        shift_slot(host, (-134 * index, 0))
        hosts.append(host)

    overflow = empty_label("RokFeature", "TaskRewardMore_0", "TaskRewardMore", "另有 {0} 项",
                           (670, 86), (132, 29), 16, color=(1.0, 1.0, 1.0), layer=5)
    overflow["props"]["Justification"] = "Left"
    overflow["props"]["Visibility"] = "Visible"
    unavailable = empty_label("RokFeature", "TaskRewardUnavailable_0", "TaskRewardPreviewUnavailable",
                              "奖励预览暂无来源数据", (269, 86), (360, 28), 16,
                              color=(0.74, 0.72, 0.66), layer=4)
    unavailable["props"]["Justification"] = "Left"
    return [*hosts, overflow, unavailable]


def complete_task_tab_badges(page, selected_index):
    badge = find_node(page, f"TaskPageBadge_{selected_index}")
    badge_count = find_node(page, f"TaskPageBadgeCount_{selected_index}")
    badge_x, _ = read_origin(badge)
    _, selected_y = read_origin(find_node(page, f"TaskPage_{selected_index}"))
    names = {node["name"] for node in page}
    for index in range(3):
        if f"TaskPageBadge_{index}" in names:
            continue
        tab = find_node(page, f"TaskPage_{index}")
        tab_x, tab_y = read_origin(tab)
        offsets = parse_fields(re.search(r"Offsets=\(([^)]*)\)", tab["slot"]["props"]["LayoutData"]).group(1))
        new_badge_x = tab_x + offsets["Right"] - 24
        for sample_badge in (badge, badge_count):
            badge_variant = deepcopy(sample_badge)
            badge_variant["name"] = badge_variant["name"][:-1] + str(index)
            shift_slot(badge_variant, (badge_x - new_badge_x, selected_y - tab_y))
            page.append(badge_variant)


def task_scroll_content(name, children):
    for child in children:
        child["slot"] = {"class": "/Script/UMG.VerticalBoxSlot", "props": {}}
    return {"name": name, "class": "/Script/UMG.VerticalBox",
            "slot": {"class": "/Script/UMG.ScrollBoxSlot", "props": {}}, "children": children}


def prepare_task_daily_server():
    source = read_spec("TaskDailyServer")
    empty_source = read_spec("TaskDailyServerEmpty")
    page = source["nodes"]
    scroll = find_node(page, "TaskDailyCatalogScroll")
    empty = deepcopy(find_node(empty_source["nodes"], "TaskDailyServerEmptySize"))
    milestones = find_node(scroll["children"], "TaskDailyMilestones")
    milestone_nodes = {
        int(node["name"].rsplit("_", 1)[-1]): node for node in milestones["children"]
        if re.fullmatch(r"TaskDailyMilestone_\d+", node["name"])
    }
    for points, label in ((20, "Milestone"), (40, "ClaimMilestone")):
        sample = deepcopy(milestone_nodes[points])
        shift_slot(sample, read_origin(sample))
        for node in walk((sample,)):
            node["name"] = node["name"].replace(f"_{points}", "_0", 1)
        write_spec(source, f"feature_TaskDailyServer{label}", [sample])
    for index, node in enumerate(milestone_nodes.values()):
        milestones["children"].remove(node)
        milestones["children"].append(canvas(f"TaskDailyMilestoneSlot_{index}", node["slot"]))

    sizes = [node for node in scroll["children"] if node["name"].startswith("TaskDailyServerRowSize_")]
    assert len(sizes) >= 2
    finished, = sizes[0]["children"]
    pending, = sizes[1]["children"]
    assert finished["name"] == "TaskRow_0" and pending["name"] == "TaskRow_1"
    action = find_node(finished["children"], "TaskAction_0")
    alternative = deepcopy(find_node(pending["children"], "TaskAction_1Background"))
    for node in walk((alternative,)):
        node["name"] = node["name"].replace("TaskAction_1Background", "TaskActionGoBackground_0", 1)
    action["children"][0]["children"].append(alternative)
    finished["slot"] = {
        "class": CANVAS_SLOT,
        "props": {"LayoutData": "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),"
                                "Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)))"},
    }

    rewards = [node for node in finished["children"] if re.fullmatch(r"TaskRewardPreview_0_\d+", node["name"])]
    assert rewards, "The finished sample must contain catalog rewards"
    sample = deepcopy(rewards[0])
    shift_slot(sample, read_origin(sample))
    write_spec(source, "feature_TaskDailyServerReward", [sample])
    for node in rewards:
        finished["children"].remove(node)
    finished["children"].extend(task_row_reward_elements(rewards[0]["slot"]))
    write_spec(source, "feature_TaskDailyServerRow", [finished])
    scroll["children"] = [node for node in scroll["children"] if node not in sizes]
    scroll["children"].append(empty)

    complete_task_tab_badges(page, 2)
    write_spec(source, "feature_TaskDailyServerPage", page)


def prepare_task_main_local():
    source = read_spec("TaskMainLocal")
    reference = read_spec("TaskMainReference")
    daily = read_spec("TaskDailyServer")
    scroll = find_node(source["nodes"], "TaskMainScroll")
    ref_scroll = find_node(reference["nodes"], "TaskMainScroll")

    def empty_section(size_name, label, key, value):
        result = deepcopy(find_node(read_spec("TaskDailyServerEmpty")["nodes"], "TaskDailyServerEmptySize"))
        result["name"] = size_name
        child, = result["children"]
        child["name"] = label
        child["props"]["Text"] = f'NSLOCTEXT("RokFeature", "{key}", "{value}")'
        child["props"]["ColorAndOpacity"] = "(SpecifiedColor=(R=0.720000,G=0.720000,B=0.700000,A=1.000000))"
        child["props"]["Justification"] = "Center"
        return result

    main_empty = empty_section("TaskMainEmptySize", "TaskMainEmpty", "TaskMainEmpty", "暂无主线任务")
    side_empty = empty_section("TaskSideEmptySize", "TaskSideEmpty", "TaskSideEmpty", "暂无支线任务")
    fixture = deepcopy(find_node(ref_scroll["children"], "TaskCompletedAnimationFixtureSize"))
    scroll["children"] = [
        find_node(scroll["children"], "TaskMainTagSize"),
        task_scroll_content("TaskMainContent", [main_empty]),
        fixture,
        find_node(scroll["children"], "TaskSideTagSize"),
        task_scroll_content("TaskSideContent", [find_node(scroll["children"], "TaskSideNotSyncedSize"), side_empty]),
    ]
    row = deepcopy(find_node(reference["nodes"], "TaskRow_0"))
    action = find_node(row["children"], "TaskAction_0")
    go_background = find_node(action["children"], "TaskAction_0Background")
    for node in walk((go_background,)):
        node["name"] = node["name"].replace("TaskAction_0Background", "TaskActionGoBackground_0", 1)
    claim_background = deepcopy(find_node(daily["nodes"], "TaskAction_0Background"))
    action["children"][0]["children"].append(claim_background)
    row["slot"] = {
        "class": CANVAS_SLOT,
        "props": {"LayoutData": "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),"
                                "Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)))"},
    }
    row["children"] = [
        node for node in row["children"]
        if not re.fullmatch(r"TaskRewardPreview_0_\d+|TaskRewardUnavailable_0|TaskRewardMore_0", node["name"])
    ]
    # The same strip and cells appear in the synced rows. Only the local
    # title/description geometry and status text differ.
    reward_sample = find_node(daily["nodes"], "TaskRewardPreview_0_0")
    row["children"].extend(task_row_reward_elements(reward_sample["slot"]))
    write_spec(source, "feature_TaskMainLocalRow", [row])
    write_spec(source, "feature_TaskMainLocalPage", source["nodes"])


def prepare_task_main_server():
    source = read_spec("TaskMainServer")
    waiting = read_spec("TaskMainServerWaiting")
    scroll = find_node(source["nodes"], "TaskMainScroll")
    waiting_scroll = find_node(waiting["nodes"], "TaskMainScroll")
    scroll["children"] = [
        find_node(scroll["children"], "TaskMainTagSize"),
        task_scroll_content("TaskMainContent", [find_node(waiting_scroll["children"], "TaskMainEmptySize")]),
        find_node(scroll["children"], "TaskSideTagSize"),
        task_scroll_content("TaskSideContent", [find_node(waiting_scroll["children"], "TaskSideNotSyncedSize")]),
    ]
    complete_task_tab_badges(source["nodes"], 1)
    write_spec(source, "feature_TaskMainServerPage", source["nodes"])


def prepare_task_daily_catalog():
    source = read_spec("TaskDailyCatalog")
    scroll, = [node for node in source["nodes"] if node["name"] == "TaskDailyCatalogScroll"]
    row_prefix = "TaskDailyCatalogRowSize_"
    header, = [node for node in scroll["children"] if node["name"] == "TaskDailyCatalogHeaderSize"]
    sizes = [node for node in scroll["children"] if node["name"].startswith(row_prefix)]
    assert sizes
    row = deepcopy(sizes[0])
    task_id = row["name"].removeprefix(row_prefix)
    assert task_id.isdecimal()
    for node in walk((row,)):
        node["name"] = node["name"].replace(f"_{task_id}", "_0")
    move_scroll_item_to_canvas(row)
    write_spec(source, "feature_TaskDailyCatalogRow", [row])
    scroll["children"] = [header]
    source["nodes"].append(empty_label("RokFeature", "TaskPageEmpty", "TaskDailyCatalogUnavailable",
        "每日任务配置目录未加载", (370, 350), (650, 50), 24, layer=6))
    write_spec(source, "feature_TaskDailyCatalogPage", source["nodes"])


def find_node(nodes, name):
    match, = [node for node in walk(nodes) if node["name"] == name]
    return match


def prepare_mail_reward_layouts():
    reference = read_spec("MailReferenceRow")
    mixed = read_spec("MailRewardsMixed")
    tooltip = read_spec("MailReferenceTip")
    reference_nodes = [deepcopy(find_node(reference["nodes"], name))
                       for name in ("MailRewardItem", "MailRewardName")]
    origin = read_origin(reference_nodes[0])
    for node in reference_nodes:
        shift_slot(node, origin)
    write_spec(reference, "feature_MailRewardReferenceCard", reference_nodes)
    for index, label in ((6, "Item"), (0, "Currency")):
        node = deepcopy(find_node(mixed["nodes"], f"MailRewardFixtureRoot_{index}"))
        shift_slot(node, read_origin(node))
        write_spec(mixed, f"feature_MailReward{label}Card", [node])

    children = deepcopy(find_node(tooltip["nodes"], "MailRewardTooltipRoot")["children"])
    background = find_node(children, "MailRewardTooltipBackground")
    origin = read_origin(background)
    width = canvas_offsets(background)["Right"]
    for node in children:
        shift_slot(node, origin)
    for patch in background["children"]:
        edge = patch["name"].rsplit("_", 1)[-1]
        offsets = canvas_offsets(patch)
        if edge.endswith("Right"):
            anchor_mail_tip_width(patch, 1, 1, offsets["Left"] - width, offsets["Right"])
        elif edge in ("Top", "Center", "Bottom"):
            anchor_mail_tip_width(patch, 0, 1, offsets["Left"], width - offsets["Left"] - offsets["Right"])
    anchor_mail_tip_width(background, 0, 1, 0, 0)
    anchor_mail_tip_width(find_node(children, "MailRewardTooltipBodyHit"), 0, 1, 0, 0)
    anchor_mail_tip_width(find_node(children, "MailRewardTooltipText"), 0, 1, 20, 20)
    write_spec(tooltip, "feature_MailRewardTooltip", children)


def canvas_offsets(node):
    layout = node["slot"]["props"]["LayoutData"]
    return {"Left": 0, "Top": 0, **parse_fields(re.search(r"Offsets=\(([^)]*)\)", layout).group(1))}


def anchor_mail_tip_width(node, lo, hi, left, right):
    props = node["slot"]["props"]
    offsets = canvas_offsets(node)
    props["LayoutData"] = (
        f"(Offsets=(Left={left},Top={offsets['Top']},Right={right},Bottom={offsets['Bottom']}),"
        f"Anchors=(Minimum=(X={lo},Y=0),Maximum=(X={hi},Y=0)),Alignment=(X=0,Y=0))"
    )


def prepare_mail_reference():
    source = read_spec("MailReferenceRow")
    missing = read_spec("MailReferenceMissing")
    alternate = read_spec("MailReferenceAlternate")
    page = source["nodes"]
    scroll = find_node(page, "MailListScroll")
    selected = deepcopy(find_node(scroll["children"], "MailRowSize_0"))
    normal = deepcopy(find_node(scroll["children"], "MailRowSize_2"))
    row_selected = find_node(selected["children"], "MailRow_0")
    row_normal = find_node(normal["children"], "MailRow_2")
    unread = deepcopy(find_node(row_normal["children"], "MailUnread_2"))
    unread["name"] = "MailUnread_0"
    row_selected["children"].append(unread)
    for row, size, index, label in ((row_normal, normal, 2, "MailReferenceNormalPresentationRow"),
                                    (row_selected, selected, 0, "MailReferenceSelectedPresentationRow")):
        host = find_node(row["children"], f"MailAttachmentPulse_{index}")
        host["children"] = []
        move_scroll_item_to_canvas(size)
        write_spec(source, f"feature_{label}", [size])
    scroll["children"] = []

    add_tab_visual_states(page, "MailTab_", 0, 1, 6)
    detail = find_node(page, "MailDetailContent")
    detail["children"] = [node for node in detail["children"] if not node["name"].startswith("MailReward")]
    detail["children"].append(deepcopy(find_node(missing["nodes"], "MailDetailUnavailable")))
    page.append(deepcopy(find_node(alternate["nodes"], "MailEmptyState")))
    write_spec(source, "feature_MailReferencePage", page)


def prepare_mail_local():
    source = read_spec("MailLocal")
    ordinary = read_spec("MailEmpty")
    reference = read_spec("MailReferenceRow")
    scroll = find_node(source["nodes"], "MailListScroll")
    unread = find_node(reference["nodes"], "MailUnread_1")
    for index, variant in ((0, "feature_MailLocalRowSelected"), (1, "feature_MailLocalRow")):
        row = deepcopy(find_node(scroll["children"], f"MailRowSize_{index}"))
        dot = deepcopy(unread)
        dot["name"] = f"MailUnread_{index}"
        find_node([row], f"MailRow_{index}")["children"].append(dot)
        move_scroll_item_to_canvas(row)
        write_spec(source, variant, [row])
    scroll["children"] = []

    nodes = source["nodes"]
    for prefix in ("MailLocalTabRedPoint_", "MailLocalTabUnreadCount_"):
        source_badge = deepcopy(find_node(nodes, f"{prefix}1"))
        source_tab_x, _ = read_origin(find_node(nodes, "MailTab_1"))
        nodes[:] = [node for node in nodes if not node["name"].startswith(prefix)]
        for index in range(6):
            x, _ = read_origin(find_node(nodes, f"MailTab_{index}"))
            badge = deepcopy(source_badge)
            badge["name"] = f"{prefix}{index}"
            shift_slot(badge, (source_tab_x - x, 0))
            if index in (0, 4):
                badge["props"]["Visibility"] = "Collapsed"
            nodes.append(badge)
    add_tab_visual_states(nodes, "MailTab_", 0, 1, 6)
    write_spec(source, "feature_MailLocalPage", nodes)
    write_spec(ordinary, "feature_MailEmptyOnly", [find_node(ordinary["nodes"], "MailEmptyState")])


def prepare_alliance_join():
    source = read_spec("AllianceJoinFixture")
    ordinary = read_spec("AllianceJoin")
    page = split_items(source, "AllianceJoinRow", "AllianceJoinRowBg_", (
        "AllianceJoinRowBg_", "AllianceJoinFlag_", "AllianceJoinName_",
        "AllianceJoinStats_", "AllianceJoinType_", "AllianceJoinRowHit_",
    ), range(5), {"feature_AllianceJoinRow": 1, "feature_AllianceJoinRowSelected": 0})
    no_results = empty_label("RokAlliance", "AllianceJoinFixtureEmpty", "FixtureSearchNoMatch",
                            "本地参考列表没有匹配的联盟。", (198, 365), (670, 60), 20,
                            color=(0.78, 0.77, 0.72))
    no_selection = empty_label("RokAlliance", "AllianceJoinDetailEmpty", "FixtureSelectEmpty",
                               "本地参考列表暂无可选联盟。", (908, 397), (310, 54), 17,
                               color=(0.82, 0.82, 0.79), layer=6)
    for node in (no_results, no_selection):
        node["props"]["Visibility"] = "Collapsed"
        page.append(node)
    write_spec(source, "feature_AllianceJoinPage", page)
    empty_nodes = [find_node(ordinary["nodes"], name)
                   for name in ("AllianceJoinEmptyState", "AllianceJoinServiceState")]
    write_spec(ordinary, "feature_AllianceJoinOrdinaryOnly", empty_nodes)


def prepare_army_show():
    marches = read_spec("ArmyShowMarches")
    waiting = read_spec("ArmyShowWaiting")
    for source, name, filename in (
        (marches, "ArmyShowMarchRowSize_0", "feature_ArmyShowMarchRow"),
        (waiting, "ArmyShowServerEmptySize", "feature_ArmyShowMessage"),
    ):
        scroll = find_node(source["nodes"], "ArmyShowListScroll")
        row = deepcopy(find_node(scroll["children"], name))
        stretch_scroll_item_to_canvas(row)
        write_spec(source, filename, [row])


def prepare_alliance_flags():
    sprites = read_spec("AllianceFlagEmblem")
    colors = read_spec("AllianceFlagClothColor")
    panel_prefix = "AllianceFlagItemPanel_"
    item_fields = ("Panel", "Hit", "Id")
    for source, field, item_id, filename in (
        (sprites, "Sprite", 320, "feature_AllianceFlagSpriteOption"),
        (colors, "Swatch", 201, "feature_AllianceFlagColorOption"),
    ):
        origin = read_origin(find_node(source["nodes"], f"{panel_prefix}{item_id}"))
        widgets = [
            deepcopy(find_node(source["nodes"], f"AllianceFlagItem{kind}_{item_id}"))
            for kind in (*item_fields, field)
        ]
        for widget in widgets:
            shift_slot(widget, origin)
        write_spec(source, filename, widgets)

    panels = [widget for widget in sprites["nodes"] if widget["name"].startswith(panel_prefix)]
    assert len(panels) == 24, "The emblem tab must provide all option positions"
    page = [widget for widget in sprites["nodes"] if not widget["name"].startswith("AllianceFlagItem")]
    for index, panel in enumerate(panels):
        slot = canvas(f"AllianceFlagOptionSlot_{index}", panel["slot"])
        layout_data = slot["slot"]["props"]["LayoutData"]
        slot["slot"]["props"]["LayoutData"] = re.sub(r"Bottom=[^,)]+", "Bottom=101.000000", layout_data, count=1)
        page.append(slot)
    write_spec(sprites, "feature_AllianceFlagEditorPage", page)


def prepare_resource_search():
    source = read_spec("ResourceSearch")
    guide_source = read_spec("ResourceSearchGuide")
    dock = find_node(source["nodes"], "ResourceSearchDockRoot")
    popup = find_node(source["nodes"], "ResourceSearchPopupRoot")

    cell_sample = find_node(dock["children"], "ResourceSearchTypeCell_0")
    hit_sample = find_node(dock["children"], "ResourceSearchType_0")
    origin = read_origin(cell_sample)
    template = deepcopy([cell_sample, hit_sample])
    for node in template:
        shift_slot(node, origin)
    write_spec(source, "feature_ResourceSearchType", template)

    cells = [find_node(dock["children"], f"ResourceSearchTypeCell_{index}") for index in range(5)]
    dock["children"] = [node for node in dock["children"] if not node["name"].startswith("ResourceSearchType")]
    for index, cell in enumerate(cells):
        dock["children"].append(canvas(f"ResourceSearchTypeSlot_{index}", cell["slot"]))

    guide = deepcopy(find_node(guide_source["nodes"], "ResourceSearchGuide"))
    popup["children"].append(guide)
    write_spec(source, "feature_ResourceSearchPage", source["nodes"])


def prepare_worker_queue():
    idle = read_spec("WorkerQueue")
    busy = read_spec("WorkerQueueBusy")
    hire = read_spec("WorkerQueueHire")
    renew = read_spec("WorkerQueueRenew")

    lists = {
        "Primary": ("WorkerQueuePrimaryIllustration", "WorkerQueuePrimaryTitle", "WorkerQueueRuntimeState",
                    "WorkerQueuePrimaryProgress", "WorkerQueuePrimaryProgressText", "WorkerQueuePrimaryAction",
                    "WorkerQueuePrimaryButtonLabel"),
        "Secondary": ("WorkerQueueSecondaryIllustration", "WorkerQueueSecondaryTitle", "WorkerQueueSecondaryNote",
                      "WorkerQueueSecondaryProgress", "WorkerQueueSecondaryProgressText",
                      "WorkerQueueSecondQueueUnlockOrExtend", "WorkerQueueSecondaryButtonLabel"),
    }
    names = {name for widgets in lists.values() for name in widgets}
    page = [node for node in busy["nodes"] if node["name"] not in names]
    page.append(deepcopy(find_node(renew["nodes"], "WorkerQueuePlacementNote")))

    for worker, names in lists.items():
        widgets = [deepcopy(find_node(busy["nodes"], name)) for name in names]
        illustration, *_, button, _label = widgets
        left, _ = read_origin(button)
        _, top = read_origin(illustration)
        slot = {"class": CANVAS_SLOT, "props": {
            "LayoutData": f"(Offsets=(Left={left},Top={top},Right=980,Bottom=137))",
            "ZOrder": "4",
        }}
        page.append(canvas(f"WorkerQueue{worker}RowSlot", slot))

        action = button["name"]
        content, = button["children"]
        idle_label = deepcopy(find_node(idle["nodes"], f"{action}Label"))
        content["children"].append(idle_label)
        if worker == "Secondary":
            background = deepcopy(find_node(hire["nodes"], f"{action}Background"))
            for node in walk([background]):
                node["name"] = node["name"].replace(f"{action}Background", "WorkerQueueHireBackground")
            content["children"].append(background)
            widgets.extend(deepcopy(find_node(hire["nodes"], f"WorkerQueueHire{suffix}"))
                           for suffix in ("Currency", "Cost", "Term"))

        for node in widgets:
            shift_slot(node, (left, top))
        write_spec(busy, f"feature_WorkerQueue{worker}Row", widgets)

    write_spec(busy, "feature_WorkerQueuePage", page)


def stretch_scroll_item_to_canvas(row):
    row["slot"] = {"class": CANVAS_SLOT, "props": {
        "LayoutData": "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),"
                      "Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)),Alignment=(X=0,Y=0))",
    }}


def prepare_army_statistics():
    local = read_spec("ArmyStatistics")
    waiting = read_spec("ArmyStatisticsWaiting")
    synced = read_spec("ArmyStatisticsSynced")

    for source, name, filename in (
        (local, "ArmyStatisticsRowSize_0", "feature_ArmyStatisticsRow"),
        (waiting, "ArmyStatisticsServerEmptySize", "feature_ArmyStatisticsMessage"),
    ):
        scroll = find_node(source["nodes"], "ArmyStatisticsTroopList")
        row = deepcopy(find_node(scroll["children"], name))
        stretch_scroll_item_to_canvas(row)
        write_spec(source, filename, [row])

    nodes = local["nodes"]
    find_node(nodes, "ArmyStatisticsTroopList")["children"] = []
    nodes.append(deepcopy(find_node(synced["nodes"], "ArmyStatisticsBreakdown")))
    for area in ("Inside", "Outside"):
        area_source = read_spec(f"ArmyStatistics{area}")
        for suffix in ("Empty", "EmptyShade"):
            nodes.append(deepcopy(find_node(area_source["nodes"], f"ArmyStatistics{area}{suffix}")))
    add_tab_visual_states(nodes, "ArmyStatisticsPage_", 0, 1, 3)
    write_spec(local, "feature_ArmyStatisticsPage", nodes)


def prepare_activity_center():
    lords = read_spec("ActivityCenter")
    calendar = read_spec("ActivityCalendar")
    sketch = read_spec("ActivityCalendarSketch")
    neutral = read_spec("ActivityCalendarNeutral")

    menu = deepcopy(find_node(lords["nodes"], "ActivityMenuRow_3"))
    menu_selected = deepcopy(find_node(lords["nodes"], "ActivityMenuBackground_1"))
    for node in walk([menu_selected]):
        node["name"] = node["name"].replace("ActivityMenuBackground_1", "ActivityMenuSelectedBackground_3")
    menu["children"].append(menu_selected)
    stretch_scroll_item_to_canvas(menu)
    write_spec(lords, "feature_ActivityMenuRow", [menu])

    nodes = [node for node in lords["nodes"] if not node["name"].startswith("ActivityMenuRow_")]
    for index in range(5):
        row = find_node(lords["nodes"], f"ActivityMenuRow_{index}")
        nodes.append(canvas(f"ActivityMenuSlot_{index}", row["slot"]))

    calendar_nodes = [deepcopy(node) for node in calendar["nodes"] if node["name"].startswith("ActivityCalendar")]
    content = find_node(calendar_nodes, "ActivityCalendarReferenceContent")
    assert len(content["children"]) == 89, "Calendar configuration row count changed"
    selected, normal, *_ = content["children"]
    neutral_row_icon = deepcopy(find_node(content["children"], "ActivityCalendarNeutralIcon_100001"))
    selected_id = selected["name"].rsplit("_", 1)[-1]
    normal_id = normal["name"].rsplit("_", 1)[-1]
    selected_frame = deepcopy(find_node([selected], f"ActivityCalendarReferenceRowFrame_{selected_id}"))
    for node in walk([selected_frame]):
        node["name"] = node["name"].replace(f"ActivityCalendarReferenceRowFrame_{selected_id}",
                                            f"ActivityCalendarSelectedRowFrame_{normal_id}")
    for node in walk([neutral_row_icon]):
        node["name"] = node["name"].replace("_100001", f"_{normal_id}")
    row = deepcopy(normal)
    row["children"].extend([selected_frame, neutral_row_icon])
    for node in walk([row]):
        node["name"] = node["name"].replace(f"_{normal_id}", "_0")
    stretch_scroll_item_to_canvas(row)
    write_spec(calendar, "feature_ActivityCalendarRow", [row])

    content["children"] = []
    nodes.extend(calendar_nodes)
    nodes.append(deepcopy(find_node(sketch["nodes"], "ActivityCalendarSelectedSketch")))
    nodes.append(deepcopy(find_node(neutral["nodes"], "ActivityCalendarSelectedNeutralIcon")))
    nodes.append(empty_label("RokFeature", "ActivityCalendarSourceUnavailable", "ActivityCalendarSourceUnavailable",
        "活动配置暂不可用", (555, 275), (630, 60), 22, color=(1.0, 1.0, 1.0), layer=5))
    write_spec(lords, "feature_ActivityCenterPage", nodes)


def prepare_city_manager():
    main = read_spec("CityManager")
    war = read_spec("CityManagerWar")
    detail = read_spec("CityManagerShield")
    nodes = main["nodes"]

    groups = find_node(nodes, "CityManagerListContent")
    group_rows = [node for node in groups["children"] if node["name"].startswith("CityManagerGroupRow_")]
    assert len(group_rows) == 13, "City manager group list changed"
    group_sample = deepcopy(find_node(group_rows, "CityManagerGroupRow_201"))
    group_sample["children"].append(deepcopy(find_node(group_rows, "CityManagerRelocateButton")))
    stretch_scroll_item_to_canvas(group_sample)
    write_spec(main, "feature_CityManagerGroupRow", [group_sample])
    groups["children"] = [node for node in groups["children"] if node not in group_rows]
    for row in group_rows:
        group_id = row["name"].rsplit("_", 1)[-1]
        groups["children"].append(canvas(f"CityManagerGroupSlot_{group_id}", row["slot"]))

    war_nodes = [deepcopy(node) for node in war["nodes"]
                 if node["name"].startswith("CityManagerWar") or node["name"] == "CityManagerBack"]
    levels = find_node(war_nodes, "CityManagerWarLevelContent")
    fields = ("CityManagerWarLevelBackground", "CityManagerWarLevel", "CityManagerWarDuration")
    war_sample = [deepcopy(find_node(levels["children"], f"{field}_0")) for field in fields]
    origin = read_origin(war_sample[0])
    for node in war_sample:
        shift_slot(node, origin)
    write_spec(war, "feature_CityManagerWarLevelRow", war_sample)
    levels["children"] = [
        canvas(f"CityManagerWarSlot_{index}", find_node(levels["children"], f"{fields[0]}_{index}")["slot"])
        for index in range(13)
    ]
    nodes.extend(war_nodes)

    detail_nodes = [deepcopy(node) for node in detail["nodes"] if node["name"].startswith("CityManagerDetail")]
    items = find_node(detail_nodes, "CityManagerDetailContent")
    icon = next(node for node in items["children"] if node["name"].startswith("CityManagerDetailIcon_"))
    sample_id = icon["name"].rsplit("_", 1)[-1]
    fields = ("RowBackground", "Icon", "ItemName", "ItemDescription", "Action")
    item_sample = [deepcopy(find_node(items["children"], f"CityManagerDetail{field}_{sample_id}")) for field in fields]
    origin = read_origin(item_sample[0])
    for node in item_sample:
        shift_slot(node, origin)
    for node in walk(item_sample):
        node["name"] = node["name"].replace(f"_{sample_id}", "_0")
    write_spec(detail, "feature_CityManagerDetailRow", item_sample)
    items["children"] = []
    nodes.extend(detail_nodes)
    write_spec(main, "feature_CityManagerPage", nodes)



def prepare_hospital():
    empty = read_spec("Hospital")
    wounded = read_spec("HospitalWounded")
    confirm = read_spec("HospitalConfirm")
    nodes = [deepcopy(node) for node in wounded["nodes"] if node["name"] != "HospitalExplanationWidget"]
    content = find_node(nodes, "HospitalMainContent")
    empty_content = find_node(empty["nodes"], "HospitalMainContent")
    empty_bed = deepcopy(find_node(empty_content["children"], "HospitalBed"))
    empty_bed["name"] = "HospitalBedEmpty"
    state_label = deepcopy(find_node(empty_content["children"], "HospitalWoundedState"))
    children = content["children"]
    stable = {"HospitalBed", "HospitalCapacityBadge", "HospitalCapacityValue",
              "HospitalCapacityInfo", "HospitalCivilizationMark"}
    treatment = [node for node in children if node["name"] not in stable]
    assert all(node["name"].startswith("Hospital") for node in treatment)
    list_node = find_node(treatment, "HospitalTreatmentList")
    unit = deepcopy(next(node for node in list_node["children"]
                         if any(child["name"] == "HospitalTroopRow_10101" for child in node["children"])))
    unit["name"] = "HospitalTroopSize_10101"
    unit["props"]["WidthOverride"] = str(parse_fields(
        re.search(r"Offsets=\(([^)]*)\)", list_node["slot"]["props"]["LayoutData"]).group(1))["Right"])
    unit["props"]["bOverride_WidthOverride"] = "True"
    move_scroll_item_to_canvas(unit)
    write_spec(wounded, "feature_HospitalTroopRow", [unit])
    list_node["children"] = []
    fill = {"class": CANVAS_SLOT, "props": {"LayoutData":
           "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)))"}}
    treatment_panel = canvas("HospitalTreatmentContent", fill, treatment)
    content["children"] = [node for node in children if node["name"] in stable] + [empty_bed, state_label, treatment_panel]
    write_spec(wounded, "feature_HospitalPage", nodes)

    overlay = prepare_centered_confirmation(confirm, "HospitalConfirmation", "HospitalConfirmationMask", fill)
    write_spec(confirm, "feature_HospitalConfirmPage", [overlay])



def prepare_centered_confirmation(source, overlay_name, mask_name, fill):
    overlay = deepcopy(find_node(source["nodes"], overlay_name))
    dimensions = re.search(r"Offsets=\(([^)]*)\)", overlay["slot"]["props"]["LayoutData"])
    size = parse_fields(dimensions.group(1))
    center = (size["Right"] / 2, size["Bottom"] / 2)
    overlay["slot"] = fill
    for child in overlay["children"]:
        if child["name"] == mask_name:
            child["slot"]["props"]["LayoutData"] = fill["props"]["LayoutData"]
        else:
            shift_slot(child, center)
            layout = child["slot"]["props"]["LayoutData"]
            child["slot"]["props"]["LayoutData"] = re.sub(
                r"Anchors=\(Minimum=\([^)]*\),Maximum=\([^)]*\)\)",
                "Anchors=(Minimum=(X=0.5,Y=0.5),Maximum=(X=0.5,Y=0.5))",
                layout, count=1)
    return overlay

def prepare_speed_up():
    catalog = read_spec("SpeedUp")
    empty = read_spec("SpeedUpEmpty")
    server = read_spec("SpeedUpServer")
    confirm = read_spec("SpeedUpServerConfirm")
    fields = ("Shade", "Quality", "Icon", "Name", "Details", "Action")
    source_nodes = {node["name"]: node for node in catalog["nodes"]}
    source_shade = source_nodes["SpeedUpItemShade_0"]
    sample_origin = read_origin(source_shade)
    sample = []
    for field in fields:
        name = f"SpeedUpItem{field}_0"
        if field == "Icon" and name not in source_nodes:
            icon = deepcopy(next(node for node in catalog["nodes"] if node["name"].startswith("SpeedUpItemIcon_")))
            index = icon["name"].rsplit("_", 1)[1]
            shift_slot(icon, read_origin(source_nodes[f"SpeedUpItemShade_{index}"]))
            icon["name"] = name
            sample.append(icon)
        else:
            node = deepcopy(source_nodes[name])
            shift_slot(node, sample_origin)
            sample.append(node)
    write_spec(catalog, "feature_SpeedUpCatalogCard", sample)
    nodes = [deepcopy(node) for node in catalog["nodes"] if not node["name"].startswith("SpeedUpItem")]
    for index in range(6):
        shade = source_nodes[f"SpeedUpItemShade_{index}"]
        nodes.append(canvas(f"SpeedUpCardSlot_{index}", shade["slot"]))
    fill = {"class": CANVAS_SLOT, "props": {"LayoutData":
           "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)))"}}
    fallback_layer = canvas("SpeedUpFallback", fill, [
        deepcopy(find_node(empty["nodes"], "SpeedUpEmptyCatalog")),
        deepcopy(find_node(empty["nodes"], "SpeedUpItemAction_0")),
    ])
    fallback_layer["slot"]["props"]["ZOrder"] = "7"
    nodes.append(fallback_layer)
    write_spec(catalog, "feature_SpeedUpPage", nodes)

    details = [deepcopy(node) for node in server["nodes"] if not node["name"].startswith("FeatureFrame")]
    scroll = find_node(details, "BuildingSpeedUpItems")
    for buying, asset in ((False, "feature_BuildingSpeedUpOwnedRow"), (True, "feature_BuildingSpeedUpBuyRow")):
        row = deepcopy(next(row for row in scroll["children"] if any(
            node["name"].startswith("SpeedUpBuyLabel_") for node in walk([row])) == buying))
        action = next(node for node in walk([row]) if node["name"].startswith("SpeedUpItemAction_"))
        index = action["name"].rsplit("_", 1)[1]
        for node in walk([row]):
            node["name"] = re.sub(rf"_{index}(?=_|$)", "_0", node["name"])
        row["name"] = "SpeedUpRowSize_0"
        row["props"]["WidthOverride"] = "1030"
        row["props"]["bOverride_WidthOverride"] = "True"
        move_scroll_item_to_canvas(row)
        write_spec(server, asset, [row])
    scroll["children"] = []
    frame = [deepcopy(node) for node in server["nodes"] if node["name"].startswith("FeatureFrame")]
    queue_layer = canvas("BuildingSpeedUpQueueContent", fill, details)
    queue_layer["slot"]["props"]["ZOrder"] = "4"
    frame.append(queue_layer)
    write_spec(server, "feature_BuildingSpeedUpPage", frame)
    overlay = prepare_centered_confirmation(confirm, "BuildingSpeedUpConfirmation", "BuildingSpeedUpConfirmShade", fill)
    write_spec(confirm, "feature_BuildingSpeedUpConfirmPage", [overlay])



def vertical_canvas_anchors(node, lo, hi, top, bottom):
    props = node["slot"]["props"]
    fields = parse_fields(re.search(r"Offsets=\(([^)]*)\)", props["LayoutData"]).group(1))
    props["LayoutData"] = (f"(Offsets=(Left={fields.get('Left', 0)},Top={top},Right={fields.get('Right', 0)},Bottom={bottom}),"
                           f"Anchors=(Minimum=(X=0,Y={lo}),Maximum=(X=0,Y={hi})))")


def prepare_train_army_detail():
    tier1 = read_spec("TrainArmyDetail")
    tier2 = read_spec("TrainArmyDetailTier2")
    attribute_tip = read_spec("TrainArmyDetailAttributeTip")
    panel = deepcopy(find_node(tier1["nodes"], "TrainArmyDetailRoot"))
    back = deepcopy(find_node(tier1["nodes"], "TrainArmyDetailBackButton"))
    attribute_nodes = tuple(f"TrainArmyDetail{field}_" for field in ("Label", "Attribute", "AttrIcon", "AttrButton"))
    skill_nodes = ("TrainArmyDetailSkillButton_", "TrainArmyDetailSkillIcon_")
    panel_sample = panel["children"]

    for template, fields in (("feature_TrainArmyDetailAttribute", attribute_nodes),
                             ("feature_TrainArmyDetailSkill", skill_nodes)):
        nodes = [deepcopy(find_node(panel_sample, f"{field}0")) for field in fields]
        button = next(node for node in nodes if "Button" in node["name"])
        origin = read_origin(button)
        for node in nodes:
            shift_slot(node, origin)
        write_spec(tier1, template, nodes)

    panel["children"] = [node for node in panel_sample if not node["name"].startswith(attribute_nodes + skill_nodes)]
    for index in range(7):
        slot = find_node(panel_sample, f"TrainArmyDetailAttrButton_{index}")["slot"]
        panel["children"].append(canvas(f"TrainArmyDetailAttributeSlot_{index}", slot))
    for count, source in ((2, tier1), (3, tier2)):
        source_panel = find_node(source["nodes"], "TrainArmyDetailRoot")
        for index in range(count):
            slot = find_node(source_panel["children"], f"TrainArmyDetailSkillButton_{index}")["slot"]
            panel["children"].append(canvas(f"TrainArmyDetailSkillSlot{count}_{index}", slot))
    two_skills = [find_node(panel["children"], f"TrainArmyDetailSkillSlot2_{index}") for index in range(2)]
    left_x, right_x = sorted(read_origin(skill)[0] for skill in two_skills)
    one_skill = deepcopy(two_skills[0])
    one_skill["name"] = "TrainArmyDetailSkillSlot1_0"
    shift_slot(one_skill, (read_origin(one_skill)[0] - (left_x + right_x) * 0.5, 0))
    panel["children"].append(one_skill)
    feedback = deepcopy(find_node(tier1["nodes"], "FeatureBoundaryFeedback"))
    write_spec(tier1, "feature_TrainArmyDetailPage", [panel, back, feedback])

    layer = find_node(attribute_tip["nodes"], "TrainArmyDetailTipLayer")
    mask = deepcopy(find_node(layer["children"], "TrainArmyDetailTipClickAway"))
    content_nodes = [deepcopy(node) for node in layer["children"] if node["name"] != mask["name"]]
    background = find_node(content_nodes, "TrainArmyDetailTipBackground")
    background_props = background["slot"]["props"]
    rect = parse_fields(re.search(r"Offsets=\(([^)]*)\)", background_props["LayoutData"]).group(1))
    fill = "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)))"
    mask["slot"]["props"]["LayoutData"] = fill
    content = canvas("TrainArmyDetailTipContent", background["slot"], content_nodes)
    for node in content_nodes:
        shift_slot(node, (rect["Left"], rect["Top"]))
    background_props["LayoutData"] = fill
    find_node(content_nodes, "TrainArmyDetailTipInside")["slot"]["props"]["LayoutData"] = fill
    vertical_canvas_anchors(find_node(content_nodes, "TrainArmyDetailTipText"), 0, 1, 20, 20)
    arrow = find_node(content_nodes, "TrainArmyDetailTipArrow")
    vertical_canvas_anchors(arrow, 1, 1, read_origin(arrow)[1] - rect["Bottom"], 12)
    for part in background["children"]:
        edge = part["name"].rsplit("_", 1)[-1]
        offsets = parse_fields(re.search(r"Offsets=\(([^)]*)\)", part["slot"]["props"]["LayoutData"]).group(1))
        if edge.startswith("Bottom"):
            vertical_canvas_anchors(part, 1, 1, offsets["Top"] - rect["Bottom"], offsets["Bottom"])
        elif edge in {"Left", "Center", "Right"}:
            vertical_canvas_anchors(part, 0, 1, offsets["Top"], rect["Bottom"] - offsets["Top"] - offsets["Bottom"])
        else:
            assert edge.startswith("Top"), edge
    write_spec(attribute_tip, "feature_TrainArmyDetailTip", [mask, content])



def prepare_train_army_parameter():
    idle = read_spec("TrainArmyParameterIdle")
    locked = read_spec("TrainArmyParameterLocked")
    busy = read_spec("TrainArmyParameterBusy")
    confirm = read_spec("TrainArmyParameterConfirm")
    panel = deepcopy(find_node(idle["nodes"], "TrainParameterRoot"))
    panel_sample = panel["children"]
    fields = ("TrainArmyTierQuality", "TrainArmyTierIcon", "TrainArmyTier", "TrainArmyTierLabel", "TrainParameterHeadSelect")
    card = [deepcopy(find_node(panel_sample, f"{field}_0")) for field in fields]
    origin = read_origin(find_node(card, "TrainArmyTierIcon_0"))
    for node in card:
        shift_slot(node, origin)
    write_spec(idle, "feature_TrainArmyTierCard", card)

    tier_names = {f"{field}_{index}" for field in fields for index in range(5)}
    queue_names = {"TrainArmyServerQueueStatus", "TrainArmyServerQueueProgress"}
    panel["children"] = [node for node in panel_sample if node["name"] not in tier_names | queue_names]
    for index in range(5):
        slot = find_node(panel_sample, f"TrainArmyTierIcon_{index}")["slot"]
        panel["children"].append(canvas(f"TrainArmyTierSlot_{index}", slot))
    panel["children"].append(deepcopy(find_node(locked["nodes"], "TrainArmyStudyStatus")))
    for name in sorted(queue_names):
        panel["children"].append(deepcopy(find_node(busy["nodes"], name)))
    nodes = [panel]
    for name in ("TrainArmyBuildingContext", "TrainArmyDetailButton", "FeatureBoundaryFeedback"):
        nodes.append(deepcopy(find_node(idle["nodes"], name)))
    write_spec(idle, "feature_TrainArmyParameterPage", nodes)

    fill = {"class": CANVAS_SLOT, "props": {"LayoutData":
           "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)))"}}
    overlay = prepare_centered_confirmation(confirm, "TrainArmyInstantConfirmation", "TrainArmyConfirmMask", fill)
    write_spec(confirm, "feature_TrainArmyParameterConfirmPage", [overlay])



def prepare_train_army_frame():
    source = read_spec("TrainArmyParameter")
    nodes = [deepcopy(node) for node in source["nodes"]
             if node["name"].startswith("FeatureFrame") or node["name"] == "FeatureBoundaryFeedback"]
    for name in ("TrainParameterLeftBacking", "TrainParameterLeftArt"):
        art = deepcopy(find_node(source["nodes"], name))
        art["props"]["Visibility"] = "Collapsed"
        nodes.append(art)
    model = find_node(source["nodes"], "TrainArmyAnimatedModel")
    nodes.append(canvas("TrainArmyModelHost", model["slot"]))
    write_spec(source, "feature_TrainArmyFramePage", nodes)

if __name__ == "__main__":
    generators = {"MapMarkers": prepare_markers, "GemShop": prepare_gems, "Charge": prepare_charge,
                  "AllianceOffice": prepare_alliance_office, "TaskDailyCatalog": prepare_task_daily_catalog,
                  "TaskChapterCatalog": prepare_task_chapter_catalog, "TaskDailyServer": prepare_task_daily_server,
                  "TaskMainLocal": prepare_task_main_local, "TaskMainServer": prepare_task_main_server,
                  "MailLocal": prepare_mail_local, "MailReference": prepare_mail_reference,
                  "MailRewards": prepare_mail_reward_layouts, "AllianceJoin": prepare_alliance_join,
                  "ArmyShow": prepare_army_show, "AllianceFlagEditor": prepare_alliance_flags,
                  "ResourceSearch": prepare_resource_search, "WorkerQueue": prepare_worker_queue,
                  "ArmyStatistics": prepare_army_statistics, "ActivityCenter": prepare_activity_center,
                  "CityManager": prepare_city_manager, "Hospital": prepare_hospital, "SpeedUp": prepare_speed_up,
                  "TrainArmyDetail": prepare_train_army_detail,
                  "TrainArmyParameter": prepare_train_army_parameter, "TrainArmyFrame": prepare_train_army_frame}
    generators[sys.argv[1]]()
