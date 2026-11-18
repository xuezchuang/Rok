"""Prepare a separate Designer layout for the commander talent row extent.

The existing talent editor and node Widget Blueprints need no regeneration.
"""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
NAME = "commander_talent_row_layout"
ASSET = "/Game/RokUI/Feature/WBP_RokFeature_CommanderTalentRowLayout"


def build():
    return {
        "version": 1,
        "label": "Commander talent row outer extent",
        "root": "CommanderTalentRowLayoutRoot",
        "nodes": [{
            "name": "RowExtent",
            "class": "/Script/UMG.Border",
            "props": {"BrushColor": "(R=0.20,G=0.40,B=0.55,A=0.65)"},
            "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
                "LayoutData": (
                    "(Offsets=(Left=0,Top=0,Right=402,Bottom=40),"
                    "Anchors=(Minimum=(X=0,Y=0),Maximum=(X=0,Y=0)),"
                    "Alignment=(X=0,Y=0))"
                ),
                "bAutoSize": "False",
            }},
        }],
        "notes": [
            "Create-only detached layout: preserve the existing talent editor and node Widget Blueprints.",
            "C++ reads the RowExtent Canvas slot size once before creating dynamic talent rows.",
            "Edit RowExtent width and height to control the row outer extent; its position and color are Designer preview only.",
            "Keep fixed anchors and auto size disabled. Missing or invalid markers retain the native 402 x 40 extent.",
            "The legacy fallback and all talent labels, actions and enabled state remain unchanged.",
        ],
        "references": ["Source/Rok/RokFeatureCommanderProgression.inl: BuildCommanderProgression"],
    }


if __name__ == "__main__":
    spec = build()
    target = ROOT / "Saved/RokUi/Specs" / f"{NAME}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {NAME} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={spec['root']}")
