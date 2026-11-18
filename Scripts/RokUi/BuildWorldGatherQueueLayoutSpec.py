"""Author vertical sizing for the shared gather and attack dispatch sidebar."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ASSET = "/Game/RokUI/Hud/WBP_RokWorldGatherQueueLayout"
SPEC = "world_gather_queue_layout"
ROOT_NAME = "WorldGatherQueueLayoutRoot"


def build():
    return {
        "version": 1,
        "label": "World dispatch sidebar vertical layout",
        "root": ROOT_NAME,
        "nodes": [
            {
                "name": name,
                "class": "/Script/UMG.Border",
                "props": {"BrushColor": f"(R={red},G=.40,B=.55,A=.75)"},
                "slot": {
                    "class": "/Script/UMG.CanvasPanelSlot",
                    "props": {"LayoutData": f"(Offsets=(Left={index * 170},Right=146,Bottom={height}))"},
                },
            }
            for index, (name, height, red) in enumerate((
                ("QueueShadeBottomInset", 62, .15),
                ("QueueShadeMinimumHeight", 100, .28),
            ))
        ],
        "notes": [
            "Marker heights supply the bottom inset and minimum height in design units.",
            "WorldGatherQueueShade in WBP_RokWorldGatherDispatch supplies width and top inset (66 by default).",
            "At default values a top-aligned shade is max(100, design height - 128) tall.",
            "For vertical stretch or other vertical alignment, the sidebar uses the authored slot directly.",
            "The detached page is presentation data for both gather and attack, with no live visual children.",
        ],
        "references": [
            "Source/Rok/RokWorldMapGatherPanel.cpp: BindGatherPanel / UpdateGatherRoute",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_troops/UICreateArmy/UI_Pop_ArmySelect.prefab",
        ],
    }


if __name__ == "__main__":
    destination = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={ROOT_NAME}")
