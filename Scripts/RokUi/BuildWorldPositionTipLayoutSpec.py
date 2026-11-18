"""Author the horizontal padding for the coordinate-search middle tip."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
ASSET = "/Game/RokUI/Feature/WBP_RokFeature_WorldPositionTipLayout"
SPEC = "world_position_tip_layout"
ROOT_NAME = "WorldPositionTipLayoutRoot"


def build():
    return {
        "version": 1,
        "label": "Coordinate-search middle tip padding",
        "root": ROOT_NAME,
        "nodes": [
            {
                "name": "MiddleTipHorizontalPadding",
                "class": "/Script/UMG.Border",
                "props": {"BrushColor": "(R=.20,G=.40,B=.55,A=.65)"},
                "slot": canvas_slot(0, 0, 50, 16),
            },
        ],
        "notes": [
            "The marker's Canvas Slot Size X is the total horizontal padding added to the measured text width.",
            "WorldPositionMiddleTipRoot on the search page supplies the maximum width. WorldPositionMiddleTipBackground retains its authored Y position and height.",
            "The detached layout only supplies the padding. Input validation and the middle-tip animation are handled by C++.",
        ],
        "references": [
            "Source/Rok/RokUnityFeatureScreenWidget.cpp: BuildWorldPositionSearch / ShowWorldPositionMiddleTip",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Search/WorldPosSearchMediator.cs",
            "ref/client-unity/Assets/Scripts/Hotfix/Common/Utils/Tip.cs: TipManager.MidDequeue",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Common_MidTips.prefab",
        ],
    }


if __name__ == "__main__":
    target = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={ROOT_NAME}")
