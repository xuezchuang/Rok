"""Derive the joined alliance page and reusable office row from real HUD captures."""

from copy import deepcopy
import re

from CutItemSpec import CANVAS_SLOT, shift_slot, walk
from PrepareFeaturePageSpecs import canvas, empty_label, read_origin, read_spec, write_spec


def full_size_slot(layer=0):
    return {"class": CANVAS_SLOT, "props": {
        "LayoutData": "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),"
                      "Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)))",
        "ZOrder": str(layer),
    }}


def make_tab(captures, index):
    name = f"AlliancePageTab_{index}"
    tab = deepcopy(captures[0][name])
    hit_name = f"AlliancePageHit_{index}"
    hit, = [node for node in tab["children"] if node["name"] == hit_name]
    states = []
    for state, snapshot in (("Selected", index), ("Normal", (index + 1) % len(captures))):
        children = deepcopy([node for node in captures[snapshot][name]["children"]
                             if node["name"] != hit_name])
        for node in walk(children):
            node["name"] = state + node["name"]
        visual = canvas(name + state, full_size_slot(), children)
        if (state == "Selected") != (index == 0):
            visual["props"]["Visibility"] = "Collapsed"
        states.append(visual)
    tab["children"] = [*states, hit]
    return tab


def split_office_rows(source, content):
    fields = ("Row", "Icon", "Name", "Description", "Bonus", "DefinitionHit")
    rows = [node for node in content.values() if re.fullmatch(r"AllianceOfficeRow_\d+", node["name"])]
    if len(rows) != 4:
        raise ValueError("Members capture must show the four catalog-backed office rows")
    rows.sort(key=lambda node: read_origin(node)[1])
    ids = [node["name"].rsplit("_", 1)[1] for node in rows]
    sample_id = ids[0]
    sample = [deepcopy(content[f"AllianceOffice{field}_{sample_id}"]) for field in fields]
    normal = deepcopy(rows[1])
    shift_slot(normal, read_origin(rows[1]))
    normal["name"] = sample[0]["name"]
    for node in sample:
        shift_slot(node, read_origin(rows[0]))
    sample[0]["name"] = "SelectedRow"
    normal["props"]["Visibility"] = "Collapsed"
    write_spec(source, "feature_AllianceJoinedOfficeRow", [normal, *sample])

    for index, (identity, row) in enumerate(zip(ids, rows)):
        for field in fields:
            content.pop(f"AllianceOffice{field}_{identity}", None)
        slot = deepcopy(row["slot"])
        slot["props"]["ZOrder"] = "4"
        name = f"AllianceJoinedOfficeSlot_{index}"
        content[name] = canvas(name, slot)
    missing = empty_label("RokAlliance", "AllianceOfficeDefinitionsUnavailable", "OfficeDefinitionsUnavailable",
                          "官职静态配置暂不可用；未读取任命或权限状态", (250, 395), (900, 38), 18,
                          color=(0.82, 0.80, 0.76), layer=6)
    missing["props"]["Visibility"] = "Collapsed"
    content[missing["name"]] = missing
    return sample_id


def main():
    sources = [read_spec(f"AllianceJoined{state}") for state in ("Overview", "Members", "Settings")]
    captures = [{node["name"]: node for node in src["nodes"]} for src in sources]
    for capture, required in zip(captures, ("AllianceJoinedName", "AllianceLeaderName", "AllianceSettingHit_5")):
        if required not in capture:
            raise ValueError(f"Baseline did not reach the expected joined page: missing {required}")
    common = captures[0].keys() & captures[1].keys() & captures[2].keys()
    tabs = {f"AlliancePageTab_{index}": make_tab(captures, index) for index in range(3)}
    page = [tabs.get(name, node) for name, node in captures[0].items() if name in common]
    for index, capture in enumerate(captures):
        content = {name: node for name, node in capture.items() if name not in common}
        if index == 1:
            office_id = split_office_rows(sources[index], content)
        group = canvas(f"AllianceJoinedPage_{index}", full_size_slot(4), content.values())
        if index != 0:
            group["props"]["Visibility"] = "Collapsed"
        page.append(group)
    names = [node["name"] for node in walk(page)]
    if len(set(names)) != len(names):
        raise ValueError("Joined page would contain duplicate Designer widget names")
    write_spec(sources[0], "feature_AllianceJoinedPage", page)
    print(office_id)


if __name__ == "__main__":
    main()
