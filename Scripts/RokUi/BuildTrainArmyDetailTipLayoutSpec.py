"""Prepare the attribute and skill tip's vertical spacing markers."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
SPEC = "train_army_detail_tip_layout"
ASSET = "/Game/RokUI/Feature/WBP_RokFeature_TrainArmyDetailTipLayout"


def build():
    return {
        "version": 1,
        "label": "Train army detail tip spacing",
        "root": "TrainArmyDetailTipLayoutRoot",
        "nodes": [
            {
                "name": name,
                "class": "/Script/UMG.Border",
                "props": {"BrushColor": "(R=.20,G=.40,B=.55,A=.65)"},
                "slot": canvas_slot(x, 0, 64, height),
            }
            for name, x, height in (("AttributeGap", 0, 23), ("SkillGap", 80, 30))
        ],
        "notes": [
            "Each marker's height supplies the spacing between the button center and the tip arrow allowance.",
            "The existing tip WBP supplies frame width, text insets and arrow size. The clicked detail button supplies the anchor.",
            "The tip still wraps and measures the body text. Missing layout values retain the original defaults.",
        ],
        "references": [
            "Source/Rok/RokUnityFeatureScreenWidget.cpp: ShowTrainArmyDetailTip",
            "Saved/RokUi/Specs/feature_TrainArmyDetailTip.json",
        ],
    }


if __name__ == "__main__":
    data = build()
    target = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={data['root']}")
