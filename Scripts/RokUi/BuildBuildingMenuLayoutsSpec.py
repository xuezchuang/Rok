"""Author the projected city building menu's alternate button positions."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
SPEC = "building_menu_layouts"
ASSET = "/Game/RokUI/Building/WBP_RokBuildingMenuLayouts"
VARIANTS = {
    "TwoButtons": (92.35, 192.35, 292.35),
    "BuilderHut": (142.35, 192.35, 242.35),
    "Decoration": (142.35, 192.35, 242.35),
}


def build():
    groups = []
    for index, (variant, positions) in enumerate(VARIANTS.items()):
        markers = [
            {
                "name": f"{variant}{action}",
                "class": "/Script/UMG.Border",
                "props": {"BrushColor": "(R=.20,G=.40,B=.55,A=.65)"},
                "slot": canvas_slot(x, y, 88, 88),
            }
            for action, x, y in zip(("Info", "Upgrade", "Feature"), positions, (218, 230, 218))
        ]
        groups.append({
            "name": f"{variant}Row",
            "class": "/Script/UMG.CanvasPanel",
            "slot": canvas_slot(0, index * 340, 473, 340),
            "children": markers,
        })
    return {
        "version": 1,
        "label": "Projected building menu button layouts",
        "root": "BuildingMenuLayoutsRoot",
        "nodes": groups,
        "notes": [
            "Each marker's position is relative to the existing live BuildingMenuRoot.",
            "The original WBP supplies the three-button layout and button sizes, styles and hitboxes.",
            "Projecting the selected building adds the same vertical offset to every button.",
            "The selected building determines which row and actions are visible.",
        ],
        "references": ["Source/Rok/RokBuildingPanelWidget.cpp", "BuildingMenuData", "BuildingTypeConfig"],
    }


if __name__ == "__main__":
    data = build()
    target = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={data['root']}")
