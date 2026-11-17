"""Derive the construction drawer and reusable card states from its runtime capture."""

from copy import deepcopy
import re

from PrepareFeaturePageSpecs import CANVAS_SLOT, empty_label, read_origin, read_spec, write_spec
from CutItemSpec import parse_fields, walk


def lookup(nodes, name):
    return next(node for node in nodes if node["name"] == name)


def update_offsets(node, **changes):
    props = node["slot"]["props"]
    layout = props["LayoutData"]
    offsets = re.search(r"Offsets=\(([^)]*)\)", layout)
    values = parse_fields(offsets[1])
    values.update(changes)
    new = ",".join(f"{key}={values.get(key, 0):.6f}" for key in ("Left", "Top", "Right", "Bottom"))
    props["LayoutData"] = layout[:offsets.start()] + f"Offsets=({new})" + layout[offsets.end():]


def copy_icon(node, name, rect, *, selected, origin):
    result = deepcopy(node)
    result["name"] = name
    x, y, width, height = rect
    left, top = origin
    update_offsets(result, Left=left + (116.5 - width) / 2, Top=top + (92 - height) / 2, Right=width, Bottom=height)
    brush = result["props"]["Brush"]
    uv = (f"UVRegion=(Min=(X={x / 512:.9f},Y={(1024 - y - height) / 1024:.9f}),"
          f"Max=(X={(x + width) / 512:.9f},Y={(1024 - y) / 1024:.9f}),bIsValid=True)")
    brush, uv_count = re.subn(r"UVRegion=\(Min=\([^)]*\),Max=\([^)]*\),bIsValid=\w+\)", uv, brush, count=1)
    brush, size_count = re.subn(r"ImageSize=\([^)]*\)", f"ImageSize=(X={width},Y={height})", brush, count=1)
    if uv_count != 1 or size_count != 1:
        raise SystemExit(f"Unexpected construction tab atlas brush on {node['name']}.")
    result["props"]["Brush"] = brush
    result["props"]["Visibility"] = "HitTestInvisible" if selected else "Collapsed"
    return result


def prepare_tabs(drawer):
    selected = [(322, 870, 75, 67), (162, 431, 67, 58), (162, 543, 84, 56)]
    normal = [(434, 961, 69, 62), (322, 816, 64, 52), (162, 491, 78, 50)]
    children = drawer["children"]
    tabs = [lookup(children, f"BuildCityCategory_{i}") for i in range(3)]
    background_samples = [deepcopy(tab["children"][0]["children"][0]) for tab in tabs[:2]]
    for i, tab in enumerate(tabs):
        background_states = []
        for state, sample in zip(("Selected", "Normal"), background_samples):
            image = deepcopy(sample)
            image["name"] = f"{tab['name']}{state}Background"
            shown = (i == 0) == (state == "Selected")
            image["props"]["Visibility"] = "HitTestInvisible" if shown else "Collapsed"
            background_states.append(image)
        tab["children"][0]["children"] = background_states
        icon = lookup(children, f"BuildCityCategoryIcon_{i}")
        origin = read_origin(tab)
        if i == 0:
            children.append(copy_icon(icon, f"BuildCityCategoryNormalIcon_{i}", normal[i], selected=False, origin=origin))
        else:
            children.append(copy_icon(icon, icon["name"], selected[i], selected=False, origin=origin))
            icon["name"] = f"BuildCityCategoryNormalIcon_{i}"
    badge_source = next((i for i in range(2) if any(n["name"] == f"BuildCityCategoryRed_{i}" for n in children)), None)
    if badge_source is None:
        raise SystemExit("Construction capture needs a buildable category badge.")
    _, source_y = read_origin(tabs[badge_source])
    for i in range(2):
        if i == badge_source or any(n["name"] == f"BuildCityCategoryRed_{i}" for n in children):
            continue
        _, target_y = read_origin(tabs[i])
        for stem in ("BuildCityCategoryRed", "BuildCityCategoryRedCount"):
            template = deepcopy(lookup(children, f"{stem}_{badge_source}"))
            template["name"] = f"{stem}_{i}"
            _, top = read_origin(template)
            update_offsets(template, Top=top + target_y - source_y)
            template["props"]["Visibility"] = "Collapsed"
            children.append(template)


def make_card(source, sample, name, *, buildable):
    type_id = sample["name"].rsplit("_", 1)[1]
    body = deepcopy(lookup(sample["children"], f"BuildCityCard_{type_id}"))
    body["name"] = "Card"
    width = sample["props"]["WidthOverride"]
    height = sample["props"]["HeightOverride"]
    body["slot"] = {"class": CANVAS_SLOT, "props": {"LayoutData": f"(Offsets=(Right={width},Bottom={height}))"}}
    children = body["children"]
    preview = lookup(children, f"BuildCityCardPreview_{type_id}")
    preview["props"].pop("Brush", None)  # Keep the live civilization/age texture out of the asset.
    update_offsets(preview, Left=19, Top=0, Right=294, Bottom=264)
    children.append({"name": "PreviewCenter", "class": "/Script/UMG.CanvasPanel",
                     "props": {"Visibility": "Collapsed"}, "slot": {"class": CANVAS_SLOT, "props": {
                         "LayoutData": "(Offsets=(Left=166,Top=138,Right=8,Bottom=8))",
                     }}})
    guide = {"name": "TaskGuide", "class": "/Script/UMG.Image", "props": {
        "Visibility": "HitTestInvisible", "RenderTransform": "(Angle=180.000000)",
    }, "slot": {"class": CANVAS_SLOT, "props": {
        "LayoutData": f"(Offsets=(Left={(float(width) - 49) / 2:.6f},Top=-149,Right=49,Bottom=149))", "ZOrder": "10",
    }}}
    children.append(guide)
    if buildable:
        for stem, inset in (("Icon", 0), ("Amount", 42)):
            initial = deepcopy(lookup(children, f"BuildCityCardResource{stem}_{type_id}_0"))
            for at, x in ((0, 74), (1, 184)):
                spec_name = f"BuildCityCardResource{stem}_{type_id}_{at}"
                node = next((child for child in children if child["name"] == spec_name), None)
                if node is None:
                    node = {**deepcopy(initial), "name": spec_name}
                    children.append(node)
                update_offsets(node, Left=x + inset)
            initial["name"] = f"Solo{stem}"
            update_offsets(initial, Left=129 + inset)
            children.append(initial)
    write_spec(source, name, [body])
    return type_id


def main():
    source = read_spec("BuildCity")
    page = deepcopy(source["nodes"])
    drawer = lookup(page, "BuildCityAnimatedRoot")
    scroll = lookup(list(walk(drawer["children"])), "BuildCityBuildingScroll")
    cards = scroll["children"]
    available = next(card for card in cards if any(node["name"].startswith("BuildCityCardResourceIcon_")
                                                   for node in walk(card["children"])))
    locked = next(card for card in cards if any(node["name"].startswith("BuildCityCardLocked_")
                                                for node in walk(card["children"])))
    available_id = make_card(source, available, "feature_BuildCityCardAvailable", buildable=True)
    locked_id = make_card(source, locked, "feature_BuildCityCardLocked", buildable=False)
    scroll["children"] = []
    empty = empty_label("RokBuildCity", "BuildCityCatalogState", "CatalogUnavailable", "Unity 建筑配置运行时尚未加载",
                        (330, -320), (780, 50), 23, color=(0.78, 0.76, 0.68), layer=5)
    empty["slot"]["props"]["LayoutData"] = "(Offsets=(Left=330,Top=-320,Right=780,Bottom=50),Anchors=(Minimum=(X=0,Y=1),Maximum=(X=0,Y=1)))"
    drawer["children"].append(empty)
    prepare_tabs(drawer)
    write_spec(source, "feature_BuildCityPage", page)
    print(f"{available_id} {locked_id}")


if __name__ == "__main__":
    main()
