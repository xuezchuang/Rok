"""Author the selected card and queue colors for the local speed-up page."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ASSET = "/Game/RokUI/Feature/WBP_RokFeature_LocalSpeedUpPresentation"
SPEC = "local_speed_up_presentation"
ROOT_NAME = "LocalSpeedUpPresentationRoot"

COLORS = {
    "SpeedUpCardNormal": (.075, .08, .07, .98),
    "SpeedUpCardSelected": (.24, .20, .10, .98),
    "SpeedUpQueueActiveDescription": (.94, .90, .65, 1),
    "SpeedUpQueueInactiveDescription": (.78, .78, .74, 1),
    "SpeedUpQueueActiveFill": (.25, .76, .34, 1),
    "SpeedUpQueueInactiveFill": (.25, .28, .26, 1),
}


def build():
    return {
        "version": 1,
        "label": "Local speed-up state colors",
        "root": ROOT_NAME,
        "nodes": [
            {
                "name": name,
                "class": "/Script/UMG.Border",
                "props": {"BrushColor": f"(R={r},G={g},B={b},A={a})"},
                "slot": {
                    "class": "/Script/UMG.CanvasPanelSlot",
                    "props": {"LayoutData": f"(Offsets=(Left={index % 2 * 140},Top={index // 2 * 55},Right=120,Bottom=40))"},
                },
            }
            for index, (name, (r, g, b, a)) in enumerate(COLORS.items())
        ],
        "notes": [
            "BrushColor on each border is copied to the named element on the existing local speed-up page or card.",
            "The detached page never enters the live widget tree. The slots lay out the swatches in Designer.",
            "Preview, missing and replaced local queues use the inactive palette. The natural completion path closes as before.",
            "The local catalog uses the Unity item data, while these appearance defaults come from the existing UE local page.",
        ],
        "references": [
            "Source/Rok/RokUnityFeatureScreenWidget.cpp: BuildSpeedUp / UpdateSpeedUpQueuePresentation",
            "Source/Rok/RokSpeedUpQueueLifecycle.cpp",
            "ref/config-tables/编译客户端配置/CSV/Item.csv",
        ],
    }


if __name__ == "__main__":
    destination = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={ROOT_NAME}")
