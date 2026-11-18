"""Author the vertical gaps for the profile kill and action-point tooltips."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
ASSET = "/Game/RokUI/Modal/Items/WBP_RokModal_ProfileTipLayout"
SPEC = "profile_tip_layout"
ROOT_NAME = "ProfileTipLayoutRoot"


def build():
    return {
        "version": 1,
        "label": "Profile tooltip anchor gaps",
        "root": ROOT_NAME,
        "nodes": [
            {
                "name": name,
                "class": "/Script/UMG.Border",
                "props": {"BrushColor": "(R=.20,G=.40,B=.55,A=.65)"},
                "slot": canvas_slot(index * 80, 0, 64, 12),
            }
            for index, name in enumerate(("KillTipGap", "ActionTipGap"))
        ],
        "notes": [
            "The marker height is the gap from the clicked button center to the top of the tip frame; 12 is the source default.",
            "The existing tip WBP supplies the fixed-size frame and arrow slots, including the arrow's vertical offset.",
            "This detached layout never enters the live widget tree. C++ places the tip from the clicked button's geometry.",
        ],
        "references": [
            "Source/Rok/RokUnityModalWidget.cpp: PositionProfileHelpTip",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/PlayerInfo/PlayerDataMediator.cs: ShowKill / ShowPowerAction",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_player/UIPlayerData/UI_KillTip.prefab",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_player/UIPlayerData/UI_Pop_PowerTextTip.prefab",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Common_HelpTips.prefab",
        ],
    }


if __name__ == "__main__":
    target = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={ROOT_NAME}")
