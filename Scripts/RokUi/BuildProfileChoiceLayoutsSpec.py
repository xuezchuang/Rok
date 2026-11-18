"""Author avatar/frame grid cells and both civilization flag layouts."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
SPEC = "profile_choice_layouts"
ASSET = "/Game/RokUI/Modal/Pages/WBP_RokModal_ProfileChoiceLayouts"


def marker(name, x, y, width, height, selected=False):
    return {
        "name": name,
        "class": "/Script/UMG.Border",
        "props": {"BrushColor": "(R=.55,G=.40,B=.20,A=.65)" if selected else
                  "(R=.20,G=.40,B=.55,A=.65)"},
        "slot": canvas_slot(x, y, width, height),
    }


def build():
    return {
        "version": 1,
        "label": "Player head grid and civilization flag layouts",
        "root": "ProfileChoiceLayoutsRoot",
        "nodes": [
            {
                "name": "PlayerHeadGridLayout",
                "class": "/Script/UMG.CanvasPanel",
                "slot": canvas_slot(0, 0, 624, 270),
                "children": [marker(f"PlayerHeadCell_{column}", 26.8 + column * 145,
                                    0, 135, 135) for column in range(4)] + [
                    marker("PlayerHeadNextRow", 26.8, 135, 135, 135),
                ],
            },
            {
                "name": "CivilizationFlagLayout",
                "class": "/Script/UMG.CanvasPanel",
                "slot": canvas_slot(0, 300, 1440, 821),
                "children": [
                    marker("CivilizationFlagNormal", 1255.5, 662.85, 170, 150),
                    marker("CivilizationFlagNext", 1056.5, 662.85, 170, 150),
                    marker("CivilizationFlagSelected", 1236.5, 616.85, 208, 204, True),
                ],
            },
        ],
        "notes": [
            "Detached markers only. Existing page/cell WBPs continue to own content, art and state.",
            "All four PlayerHeadCell markers are required and own each column's first position and extent.",
            "The effective Y distance from PlayerHeadCell_0 to PlayerHeadNextRow sets row pitch. Only the next marker's Y position is consumed.",
            "Grid scroll height includes the last occupied cell bottom in every column, including unequal heights and Y offsets.",
            "CivilizationFlagNormal and CivilizationFlagSelected own the two flag layouts. CivilizationFlagNext supplies the shared negative X pitch from the normal flag; its other geometry is a Designer preview.",
            "Marker slots use fixed top-left anchors and explicit sizes; alignment is included when resolving effective positions. Parent group slots arrange Designer samples only.",
            "Missing or invalid grid/flag marker sets retain the corresponding original native layout as a unit.",
            "Civilization list/detail animation roots and the selected flag pulse remain owned by their existing widgets.",
        ],
        "references": [
            "Source/Rok/RokUnityModalWidget.cpp: BuildPlayerHeadSelection, BuildCivilizationSelection",
            "Source/Rok/RokPlayerHeadLayout.h",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_player/UIPlayerHeadSet/UI_LC_PlayerHeadPic.prefab",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/PlayerInfo/UI_Win_PlayerHeadPicMediator.cs",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_login/UILogin/CreateCharFlagItem.prefab",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Login/CreateCharMediator.cs",
        ],
    }


if __name__ == "__main__":
    data = build()
    target = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={data['root']}")
