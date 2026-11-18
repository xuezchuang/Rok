"""Create the four context slots around the shared, source-fitted preview pane.

Only writes a generated spec under Saved/. The corresponding WBP generator is
create-only, so rerunning this script never replaces Designer-authored assets.
"""

import copy
import json

from CutItemSpec import SPEC_DIR


CONTEXTS = (
    ("building_info", "BuildingInfo", (230.0, 340.1)),
    ("resource_building_info", "ResourceBuildingInfo", (230.0, 340.1)),
    ("building_upgrade", "BuildingUpgrade", (225.0, 340.1)),
    ("building_upgrade_barracks", "BarracksUpgrade", (225.0, 351.7)),
)


def walk(nodes):
    for node in nodes:
        yield node
        yield from walk(node.get("children", []))


def build():
    slots = []
    for source_name, prefix, center in CONTEXTS:
        source_path = SPEC_DIR / f"{source_name}.json"
        if not source_path.is_file():
            raise FileNotFoundError(
                f"Required native capture missing: {source_path}. "
                "Restore the native modal specs or capture them with CaptureSpecs.ps1 before generation."
            )
        source = json.loads(source_path.read_text(encoding="utf-8"))
        captured = next(node for node in walk(source["nodes"]) if node["name"] == "BuildingPreviewPane")
        slot = copy.deepcopy(captured)
        slot["name"] = f"{prefix}PreviewSlot"
        slot["children"] = [{
            "name": f"{prefix}PreviewCenter",
            "class": "/Script/UMG.CanvasPanel",
            "props": {"Visibility": "HitTestInvisible"},
            "slot": {
                "class": "/Script/UMG.CanvasPanelSlot",
                "props": {"LayoutData": (
                    f"(Offsets=(Left={center[0]:.6f},Top={center[1]:.6f},Right=0,Bottom=0),"
                    "Anchors=(Minimum=(X=0,Y=0),Maximum=(X=0,Y=0)),Alignment=(X=0,Y=0))"
                )},
            },
        }]
        slots.append(slot)
    return {
        "version": 1,
        "label": "building_preview_layout",
        "root": "PageRoot",
        "nodes": slots,
        "notes": [
            "Outer slot offsets come from captured native building preview panes.",
            "Center marker positions preserve the native source-fit/glow centers.",
            "Designer: edit PreviewSlot Position/Size and its PreviewCenter Position.",
            "Keep slot and center anchors fixed at the top-left; center markers have zero size.",
            "Runtime source art, country/age selection, aspect fitting, glow and missing-art state remain C++ owned.",
        ],
    }


if __name__ == "__main__":
    spec = build()
    target = SPEC_DIR / "building_preview_layout.json"
    target.write_text(json.dumps(spec, ensure_ascii=False, indent=1), encoding="utf-8")
    print(target)
