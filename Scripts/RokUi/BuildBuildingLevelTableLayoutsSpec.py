"""Author the resource building and barracks level-table row bounds."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
SPEC = "building_level_table_layouts"
ASSET = "/Game/RokUI/Modal/Pages/WBP_RokModal_BuildingLevelTableLayouts"


def build():
    return {
        "version": 1,
        "label": "Building level table row layouts",
        "root": "BuildingLevelTableLayoutsRoot",
        "nodes": [
            {
                "name": f"{table}Layout",
                "class": "/Script/UMG.CanvasPanel",
                "slot": canvas_slot(0, offset, 1024, 100),
                "children": [
                    {
                        "name": f"{table}{name}",
                        "class": "/Script/UMG.Border",
                        "props": {"BrushColor": color},
                        "slot": canvas_slot(0, y, 1024, 50),
                    }
                    for name, y, color in (
                        ("Cell", 0, "(R=.20,G=.40,B=.55,A=.65)"),
                        ("Next", 50, "(R=.55,G=.40,B=.20,A=.65)"),
                    )
                ],
            }
            for table, offset in (("ResourceLevel", 0), ("BarracksLevel", 130))
        ],
        "notes": [
            "Cell owns the first row position and extent. The Y distance from Cell to Next sets the vertical step.",
            "The table scroll content follows the last row bottom. Each row WBP continues to own its cells and highlights.",
        ],
        "references": ["Source/Rok/RokUnityModalWidget.cpp", "BuildingLevelData", "BuildingResourcesProduce", "Arms"],
    }


if __name__ == "__main__":
    data = build()
    target = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={data['root']}")
