"""Author the training action and activity list presentation templates."""

import json
from pathlib import Path

from BuildFeatureTextPresentationSpecs import text
from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
WHITE = (1, 1, 1, 1)


def marker(name, x, y, width, height):
    return {
        "name": name,
        "class": "/Script/UMG.Border",
        "props": {"BrushColor": "(R=.20,G=.40,B=.55,A=.65)"},
        "slot": canvas_slot(x, y, width, height),
    }


def write_spec(asset, spec_name, nodes, notes):
    destination = ROOT / "Saved/RokUi/Specs" / f"{spec_name}.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    root = asset + "Root"
    spec = {"version": 1, "label": asset, "root": root, "nodes": nodes, "notes": notes,
            "references": ["Source/Rok/RokUnityFeatureScreenWidget.cpp"]}
    destination.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_{asset} {spec_name} "
          f"-ParentClass=/Script/Rok.RokUiPageWidget -RootName={root}")


def main():
    write_spec("TrainArmyActionPresentation", "train_army_action_presentation", [
        marker("TrainStartLabelReady", 0, 12, 274, 38),
        marker("TrainStartLabelOther", 300, 23, 274, 38),
        text("TrainTierUnlocked", (0, 76, 260, 34), 16, False, WHITE),
        text("TrainTierLocked", (300, 76, 260, 34), 16, False, (.35, .35, .34, 1)),
    ], [
        "TrainStartLabelReady and TrainStartLabelOther provide the label slot's Y offset.",
        "Tier samples provide color only. The active training and tier WBPs still own all other style properties.",
    ])
    write_spec("ActivityListPresentation", "activity_list_presentation", [
        marker("CalendarRowBounds", 0, 0, 324, 110),
        marker("CalendarRowGap", 0, 110, 324, 0),
        text("ActivityMenuNormal", (350, 0, 350, 30), 20, True, WHITE),
        text("ActivityMenuSelected", (350, 38, 350, 30), 20, True, (.40, .39, .33, 1)),
        text("CalendarTitleNormal", (350, 76, 350, 30), 22, True, WHITE),
        text("CalendarTitleSelected", (350, 114, 350, 30), 22, True, (.33, .30, .25, 1)),
    ], [
        "CalendarRowBounds supplies width and height. CalendarRowGap supplies additional space between rows from its height.",
        "State labels supply color only. The row WBPs retain their own text layout and font.",
    ])


if __name__ == "__main__":
    main()
