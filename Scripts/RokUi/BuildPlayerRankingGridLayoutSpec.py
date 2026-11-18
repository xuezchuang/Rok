"""Author the detached three-column player-ranking overview grid."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
SPEC = "player_ranking_grid_layout"
ASSET = "/Game/RokUI/Modal/Pages/WBP_RokModal_PlayerRankingGridLayout"


def marker(name, x, y):
    return {
        "name": name,
        "class": "/Script/UMG.Border",
        "props": {"BrushColor": "(R=.20,G=.40,B=.55,A=.65)"},
        "slot": canvas_slot(x, y, 340, 144),
    }


def build():
    return {
        "version": 1,
        "label": "Player ranking overview grid layout",
        "root": "PlayerRankingGridLayoutRoot",
        "nodes": [marker(f"PlayerRankingCell_{column}", column * 350, 66)
                  for column in range(3)] + [
            marker("PlayerRankingNextRow", 0, 270),
        ],
        "notes": [
            "Detached markers only. Existing overview and tile WBPs retain content, brushes and state.",
            "All three PlayerRankingCell markers own their column's first effective position and tile extent.",
            "PlayerRankingNextRow effective Y minus PlayerRankingCell_0 effective Y sets the shared row pitch. Its other geometry is a Designer preview.",
            "Marker slots require fixed top-left anchors, explicit positive sizes and no AutoSize. Alignment contributes to effective position.",
            "Any missing or unsupported marker or non-positive/non-finite row pitch retains the entire original native grid.",
            "The original grid has three columns at X=0/350/700, first Y=66, row pitch=204 and tile size=340x144.",
            "Positions are local to the existing PlayerRankingMainPanel; the overview panel position, extent and boundary remain authored there.",
            "Catalog order, category IDs, selection callbacks, tile icon geometry and ranking detail are unchanged.",
        ],
        "references": [
            "Source/Rok/RokUnityModalWidget.cpp: BuildPlayerRanking",
            "Source/Rok/RokLeaderboardOverviewLayout.h",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_player/UIRanking/UI_Win_Ranking.prefab",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Ranking/UI_Win_RankingMediator.cs",
        ],
    }


if __name__ == "__main__":
    data = build()
    target = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={data['root']}")
