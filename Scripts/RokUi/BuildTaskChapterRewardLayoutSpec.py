"""Create a separate Designer layout for dynamic chapter reward cells.

The existing chapter pages and reward icon templates need no regeneration.
"""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
NAME = "chapter_reward_layouts"
ASSET = "/Game/RokUI/Feature/WBP_RokFeature_TaskChapterRewardLayouts"
LAYOUTS = {
    "ServerAggregate": (55, 55, 69, 0, 0),
    "ServerTask": (103, 29, 103, 0, 0),
    "CatalogAggregate": (56.28, 56.28, -68.88, 1, 1),
    "CatalogTask": (104, 40, -104, 1, 1),
}


def marker(name, x, width, height, anchor, alignment):
    return {
        "name": name,
        "class": "/Script/UMG.Border",
        "props": {"BrushColor": "(R=0.20,G=0.40,B=0.55,A=0.65)"},
        "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
            "LayoutData": (
                f"(Offsets=(Left={x},Top=0,Right={width},Bottom={height}),"
                f"Anchors=(Minimum=(X={anchor},Y=0),Maximum=(X={anchor},Y=0)),"
                f"Alignment=(X={alignment},Y=0))"
            ),
            "ZOrder": "0",
            "bAutoSize": "False",
        }},
    }


def build():
    rows = []
    for index, (name, (width, height, pitch, anchor, alignment)) in enumerate(LAYOUTS.items()):
        rows.append({
            "name": name + "Layout",
            "class": "/Script/UMG.CanvasPanel",
            "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
                "LayoutData": f"(Offsets=(Left=0,Top={index * 90},Right=695,Bottom=75))",
            }},
            "children": [marker(name + suffix, x, width, height, anchor, alignment)
                         for suffix, x in (("Cell", 0), ("Next", pitch))],
        })
    return {
        "version": 1,
        "label": "Chapter dynamic reward layouts",
        "root": "TaskChapterRewardLayoutsRoot",
        "nodes": rows,
        "notes": [
            "Create-only shared layout: do not regenerate existing chapter pages or reward icon templates.",
            "C++ reads each Cell Canvas slot and the Cell-to-Next position delta; outer rows are Designer preview only.",
            "Cell owns size, anchors, alignment, Z order and auto size; Next owns repetition pitch.",
            "Server rewards repeat left-to-right. Catalog task rewards repeat from the right; aggregate order is preserved by reversing the layout index.",
            "Each of the four layouts preserves its separate previous defaults. Missing assets or markers retain the previous native placement.",
        ],
        "references": ["Source/Rok/RokFeatureTaskChapterRewardLayout.cpp",
                       "Source/Rok/RokFeatureTaskChapterServer.inl",
                       "Source/Rok/RokUnityFeatureScreenWidget.cpp: BuildTaskChapter"],
    }


if __name__ == "__main__":
    target = ROOT / "Saved/RokUi/Specs" / f"{NAME}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    spec = build()
    target.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {NAME} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={spec['root']}")
