"""Derive the VIP page, reusable benefit row and help card from runtime captures."""

from copy import deepcopy
import re

from CutItemSpec import CANVAS_SLOT, parse_fields
from PrepareFeaturePageSpecs import read_spec, write_spec


def find(nodes, name):
    return next(node for node in nodes if node["name"] == name)


def rect(node):
    layout = node["slot"]["props"]["LayoutData"]
    values = parse_fields(re.search(r"Offsets=\(([^)]*)\)", layout)[1])
    return tuple(values.get(key, 0) for key in ("Left", "Top", "Right", "Bottom"))


def set_rect(node, bounds, anchors=(0, 0, 0, 0)):
    left, top, right, bottom = bounds
    min_x, min_y, max_x, max_y = anchors
    node["slot"]["props"]["LayoutData"] = (
        f"(Offsets=(Left={left:.6f},Top={top:.6f},Right={right:.6f},Bottom={bottom:.6f}),"
        f"Anchors=(Minimum=(X={min_x},Y={min_y}),Maximum=(X={max_x},Y={max_y})))"
    )


def make_tip(source):
    tip = deepcopy(find(source["nodes"], "VipDailyPointHelpTipRoot"))["children"]
    frame = find(tip, "VipDailyPointHelpTipBackground")
    x, y, width, height = rect(frame)
    sides = ["TopLeft", "Top", "TopRight", "Left", "Center", "Right", "BottomLeft", "Bottom", "BottomRight"]
    for index, name in enumerate(sides):
        child = find(frame["children"], f"VipDailyPointHelpTipBackground_{name}")
        left, top, w, h = rect(child)
        column, row = index % 3, index // 3
        min_x, max_x = ((0, 0), (0, 1), (1, 1))[column]
        min_y, max_y = ((0, 0), (0, 1), (1, 1))[row]
        right = width - left - w if column == 1 else w
        bottom = height - top - h if row == 1 else h
        set_rect(child, (left - min_x * width, top - min_y * height, right, bottom), (min_x, min_y, max_x, max_y))

    set_rect(frame, (0, 0, width, 80))
    set_rect(find(tip, "VipDailyPointHelpTipBodyHit"), (0, 0, width, height))
    text = find(tip, "VipDailyPointHelpTipText")
    left, top, _, text_height = rect(text)
    set_rect(text, (left - x, top - y, 550, text_height))
    arrow = find(tip, "VipDailyPointHelpTipArrow")
    _, top, w, h = rect(arrow)
    set_rect(arrow, (0, top - y, w, h))
    write_spec(source, "feature_VipDailyPointHelpTip", tip)


def make_row(source):
    scroll = find(source["nodes"], "VipBenefitsScroll")
    sample = find(scroll["children"], "VipBenefitRowSize_0")
    row = deepcopy(find(sample["children"], "VipBenefitRow_0"))
    row["name"] = "Row"
    size = sample["props"]
    row["slot"] = {"class": CANVAS_SLOT, "props": {}}
    set_rect(row, (0, 0, float(size["WidthOverride"]), float(size["HeightOverride"])))
    text = find(row["children"], "VipBenefit_0")
    text["name"] = "NewText"
    plain = deepcopy(text)
    plain["name"] = "PlainText"
    _, top, _, h = rect(plain)
    set_rect(plain, (8, top, 346, h))
    row["children"].append(plain)
    write_spec(source, "feature_VipBenefitRow", [row])


def main():
    offline, fixture, help_capture = map(read_spec, ("VipOffline", "VipFixture", "VipHelp"))
    make_tip(help_capture)
    make_row(fixture)
    page = deepcopy(offline["nodes"])
    find(page, "VipBenefitsScroll")["children"] = []
    for name in ("VipDailyPointRedPoint", "VipFreeChestRedPoint", "VipFixtureTag"):
        page.append(deepcopy(find(fixture["nodes"], name)))
    fill = deepcopy(find(help_capture["nodes"], "VipProgressFill"))
    left, top, _, height = rect(fill)
    _, _, an_authored_full_width, _ = rect(find(page, "VipProgressTrack"))
    set_rect(fill, (left, top, an_authored_full_width, height))
    page.append(fill)

    button = find(page, "VipClaimFreeChest")["children"][0]
    fixture_button = find(fixture["nodes"], "VipClaimFreeChest")["children"][0]
    unavailable = find(button["children"], "VipClaimFreeChestBackground")
    available = deepcopy(find(fixture_button["children"], "VipClaimFreeChestBackground"))
    unavailable["name"] = "VipFreeUnavailableBackground"
    available["name"] = "VipFreeClaimableBackground"
    available["props"]["Visibility"] = "Collapsed"
    for state, node in (("Available", available), ("Unavailable", unavailable)):
        for patch in node["children"]:
            patch["name"] = state + patch["name"]
    button["children"].append(available)
    write_spec(offline, "feature_VipPage", page)


if __name__ == "__main__":
    main()
