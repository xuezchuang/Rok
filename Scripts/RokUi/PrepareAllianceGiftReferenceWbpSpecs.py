"""Build the Alliance gift reference page and row layout from the offline HUD replay."""

from copy import deepcopy
import re

from CutItemSpec import CANVAS_SLOT, shift_slot
from PrepareFeaturePageSpecs import canvas, empty_label, read_origin, read_spec, write_spec


def row_names(source, expected_count):
    rows = [node for node in source["nodes"] if re.fullmatch(r"AllianceGiftReferenceRow_\d+", node["name"])]
    if len(rows) != expected_count:
        raise ValueError(f"Expected {expected_count} gift definition rows, got {len(rows)}")
    return sorted(rows, key=lambda node: read_origin(node)[1])


def main():
    level_one = read_spec("AllianceGiftReferenceLevel1")
    special = read_spec("AllianceGiftReferenceSpecial")
    regular_rows = row_names(level_one, 7)
    special_rows = row_names(special, 10)
    nodes = {node["name"]: node for node in level_one["nodes"]}
    even, odd = deepcopy(regular_rows[:2])
    sample_id = even["name"].rsplit("_", 1)[1]
    detail = deepcopy(nodes[f"AllianceGiftReferenceDefinition_{sample_id}"])
    first_origin = read_origin(even)
    shift_slot(even, first_origin)
    shift_slot(detail, first_origin)
    shift_slot(odd, read_origin(odd))
    odd["name"] = "OddRow"
    odd["props"]["Visibility"] = "Collapsed"
    write_spec(level_one, "feature_AllianceGiftReferenceRow", [even, odd, detail])

    names = ["AllianceGiftReference" + name for name in ("Previous", "Next", "Special", "Level", "Threshold")]
    controls = [nodes.pop(name) for name in names]
    for row in regular_rows:
        nodes.pop(row["name"])
        nodes.pop(row["name"].replace("ReferenceRow_", "ReferenceDefinition_"))
    slots = [canvas(f"AllianceGiftReferenceSlot_{index}", row["slot"])
             for index, row in enumerate(special_rows)]
    definitions = canvas("AllianceGiftReferenceDefinitions", {"class": CANVAS_SLOT, "props": {
        "LayoutData": "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),"
                      "Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)))",
        "ZOrder": "4",
    }}, [*controls, *slots])
    missing = empty_label("RokAllianceGift", "AllianceGiftReferenceUnavailable", "ReferenceUnavailable",
                          "礼物静态配置不可用；未生成礼物或领取状态", (250, 365), (900, 50), 20,
                          color=(0.88, 0.82, 0.70), layer=6)
    missing["props"]["Visibility"] = "Collapsed"
    write_spec(level_one, "feature_AllianceGiftReferencePage", [*nodes.values(), definitions, missing])
    print(sample_id)


if __name__ == "__main__":
    main()
