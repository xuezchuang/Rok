"""Author collapsed HUD endpoints while the base WBPs keep the opened layout."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
ASSET = "/Game/RokUI/Hud/WBP_RokHudFoldLayout"
SPEC = "hud_fold_layout"


def marker(name, x, y, width, height, *, angle=0, travel=0):
    return {
        "name": name,
        "class": "/Script/UMG.Border",
        "props": {
            "BrushColor": "(R=.20,G=.40,B=.55,A=.65)",
            "RenderTransform": f"(Translation=(X={travel},Y=0),Scale=(X=1,Y=1),Angle={angle})",
        },
        "slot": canvas_slot(x, y, width, height),
    }


def build():
    return {
        "version": 1,
        "label": "City HUD fold layout",
        "root": "HudFoldLayoutRoot",
        "nodes": [
            marker("CollapsedTaskBackground", 5.63, 44.52, 166, 43),
            marker("CollapsedTaskClip", 15.63, 82.3, 146, 0),
            marker("CollapsedTaskArrow", 138.13, 49.52, 25, 34, angle=-180),
            marker("CollapsedFeatureBackground", 0, 130, 101.10, 98),
            marker("CollapsedFeatureArrow", 122, 130, 87, 88, angle=90),
            marker("CollapsedFeatureButtons", 0, 240, 100, 100, travel=440.5),
        ],
        "notes": [
            "Task background uses slot size, clip uses position and size, and arrow uses position and render angle.",
            "Feature background uses slot width, arrow uses render angle, and buttons use render translation X.",
            "The template does not enter the live tree. Existing HUD WBPs retain opened geometry and the C++ transitions retain their timing and easing.",
        ],
        "references": ["Source/Rok/RokMainHudWidget.cpp", "Source/Rok/RokHudAnimation.h"],
    }


if __name__ == "__main__":
    data = build()
    destination = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={data['root']}")
