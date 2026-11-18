"""Prepare editable icon and amount positions for hospital treatment costs."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
NAME = "hospital_resource_cost_layout"
ASSET = "/Game/RokUI/Feature/WBP_RokFeature_HospitalResourceCostLayout"


def canvas_slot(x, y, width, height):
    return {
        "class": "/Script/UMG.CanvasPanelSlot",
        "props": {
            "LayoutData": (
                f"(Offsets=(Left={x},Top={y},Right={width},Bottom={height}),"
                "Anchors=(Minimum=(X=0,Y=0),Maximum=(X=0,Y=0)),"
                "Alignment=(X=0,Y=0))"
            ),
        },
    }


def build():
    rows = []
    for count in range(1, 5):
        positions = []
        for index in range(count):
            x = 947 - 65 * count + 130 * index
            positions.extend([
                {
                    "name": f"HospitalCostIconSlot_{count}_{index}",
                    "class": "/Script/UMG.Border",
                    "props": {"BrushColor": "(R=0.20,G=0.40,B=0.55,A=0.65)"},
                    "slot": canvas_slot(x, 552, 38, 38),
                },
                {
                    "name": f"HospitalCostValueSlot_{count}_{index}",
                    "class": "/Script/UMG.TextBlock",
                    "props": {"Text": "1,000", "ColorAndOpacity": "(SpecifiedColor=(R=0.9,G=0.85,B=0.7,A=1))"},
                    "slot": canvas_slot(x + 38, 561, 96, 28),
                },
            ])
        rows.append({
            "name": f"HospitalCostLayout_{count}_Resources",
            "class": "/Script/UMG.CanvasPanel",
            "slot": canvas_slot(-659, -537 + (count - 1) * 80, 1440, 900),
            "children": positions,
        })
    return {
        "version": 1,
        "label": "Hospital treatment resource cost positions",
        "root": "HospitalResourceCostLayoutRoot",
        "nodes": rows,
        "notes": [
            "C++ selects a row by the number of positive costs and preserves food, wood, stone, gold ordering.",
            "Each icon and amount marker independently supplies anchors, alignment and position in the HospitalTreatmentContent canvas.",
            "Parent row positions and marker sizes, colors and sample text lay out the Designer preview only.",
            "Zero costs hide all resource widgets. Treatment queue, pending actions and amount formatting remain runtime behavior.",
            "A missing layout or incomplete count row uses the unchanged native positions for the entire resource row.",
        ],
        "references": [
            "Source/Rok/RokUnityFeatureScreenWidget.cpp: UpdateHospitalActions",
            "Source/Rok/RokHospitalTreatment.h: FRokHospitalQuote.Resources",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Hospital/HospitalMediator.cs: RefrshCostRes",
        ],
    }


if __name__ == "__main__":
    spec = build()
    target = ROOT / "Saved/RokUi/Specs" / f"{NAME}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {NAME} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={spec['root']}")
