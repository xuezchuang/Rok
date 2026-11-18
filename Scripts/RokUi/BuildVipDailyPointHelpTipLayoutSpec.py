"""Prepare the anchor gap and screen inset for the VIP daily-point tip."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
KEY = "vip_daily_point_help_tip_layout"
ASSET = "/Game/RokUI/Feature/WBP_RokFeature_VipDailyPointHelpTipLayout"
ROOT_NAME = "VipDailyPointHelpTipLayoutRoot"


def build():
    return {
        "version": 1,
        "label": "VIP daily-point tip anchor gap and viewport inset",
        "root": ROOT_NAME,
        "nodes": [
            {
                "name": name,
                "class": "/Script/UMG.Border",
                "props": {"BrushColor": "(R=.20,G=.40,B=.55,A=.65)"},
                "slot": canvas_slot(index * 80, 0, 64, height),
            }
            for index, (name, height) in enumerate((("VipTipAnchorGap", 12), ("VipTipViewportInset", 8)))
        ],
        "notes": [
            "VipTipAnchorGap's fixed Canvas slot height is the distance from the help button center to the frame below it.",
            "VipTipViewportInset's fixed Canvas slot height is applied at both vertical edges of the design viewport.",
            "The existing tip WBP owns the content padding, frame and arrow. C++ measures the help text and clamps the position.",
        ],
        "references": [
            "Source/Rok/RokFeatureVip.inl: BuildVipDailyPointHelpTip",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Vip/UI_Win_VipMediator.cs: HelpTip with arrowUp",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Pop_TextTip.prefab",
        ],
    }


if __name__ == "__main__":
    target = ROOT / "Saved/RokUi/Specs" / f"{KEY}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {KEY} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={ROOT_NAME}")
