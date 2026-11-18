"""Prepare four editable positions for the training resource cost row."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
NAME = "train_resource_cost_layout"
ASSET = "/Game/RokUI/Feature/WBP_RokFeature_TrainResourceCostLayout"
CELL_WIDTH = 137
ROW_WIDTH = 4 * CELL_WIDTH


def canvas_slot(x, y, width, height, anchor_x=0):
    return {
        "class": "/Script/UMG.CanvasPanelSlot",
        "props": {
            "LayoutData": (
                f"(Offsets=(Left={x},Top={y},Right={width},Bottom={height}),"
                f"Anchors=(Minimum=(X={anchor_x},Y=0),Maximum=(X={anchor_x},Y=0)),"
                "Alignment=(X=0,Y=0))"
            ),
        },
    }


def build():
    rows = []
    for count in range(1, 5):
        positions = []
        for index in range(count):
            positions.append({
                "name": f"TrainCostSlot_{count}_{index}",
                "class": "/Script/UMG.Border",
                "props": {"BrushColor": "(R=0.20,G=0.40,B=0.55,A=0.65)"},
                "slot": canvas_slot((index - count / 2) * CELL_WIDTH, 13.95, CELL_WIDTH, 40, 0.5),
            })
        rows.append({
            "name": f"TrainCostLayout_{count}_Resources",
            "class": "/Script/UMG.CanvasPanel",
            "slot": canvas_slot(0, (count - 1) * 64, ROW_WIDTH, 58),
            "children": positions,
        })
    return {
        "version": 1,
        "label": "Training resource cost row positions",
        "root": "TrainResourceCostLayoutRoot",
        "nodes": rows,
        "notes": [
            "C++ selects a row by the number of nonzero costs and reads only the cell anchors, alignment and offsets.",
            "The parent row positions and marker sizes/colors lay out the Designer preview only.",
            "The defaults preserve the existing C++ row placement at 548 units. Center anchors follow the authored cost container width.",
        ],
        "references": ["Source/Rok/RokTrainResourceCost.h", "Source/Rok/RokUnityFeatureScreenWidget.cpp: UpdateTrainResourceCost"],
    }


if __name__ == "__main__":
    spec = build()
    target = ROOT / "Saved/RokUi/Specs" / f"{NAME}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {NAME} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={spec['root']}")
