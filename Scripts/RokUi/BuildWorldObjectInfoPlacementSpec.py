"""Author the placement distances shared by resource and barbarian popups."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
ASSET = "/Game/RokUI/Hud/WBP_RokWorldObjectInfoPlacement"
SPEC = "world_object_info_placement"
ROOT_NAME = "WorldObjectInfoPlacementRoot"


def build():
    distances = (
        ("ObjectGap", 50),
        ("ViewportInset", 12),
        ("ArrowCornerInset", 18),
        ("ArrowEdgeOffset", 5),
    )
    return {
        "version": 1,
        "label": "World object popup placement distances",
        "root": ROOT_NAME,
        "nodes": [
            {
                "name": name,
                "class": "/Script/UMG.Border",
                "props": {"BrushColor": "(R=.20,G=.40,B=.55,A=.65)"},
                "slot": canvas_slot(index * 80, 0, 64, height),
            }
            for index, (name, height) in enumerate(distances)
        ],
        "notes": [
            "Each marker's Canvas Slot Size Y is a distance in world HUD design units.",
            "ObjectGap separates the popup from the projected selection, ViewportInset limits its screen-edge approach, and ArrowCornerInset and ArrowEdgeOffset bound the arrow center relative to the popup edges.",
            "The detached page is shared by both popups. Existing popup and arrow WBPs supply their geometry and appearance.",
        ],
        "references": [
            "Source/Rok/RokWorldMapHudWidget.cpp: UpdateWorldObjectInfoProjection",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Rss/UI_Pop_WorldObjectInfoMediator.cs: ChangeWinPos",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Rss/UI_Pop_WorldObjectMonsterMediator.cs: ChangeWinPos",
        ],
    }


if __name__ == "__main__":
    target = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={ROOT_NAME}")
