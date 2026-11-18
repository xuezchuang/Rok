"""Prepare the existing Rok expedition list and commander display layouts."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEC_DIR = ROOT / "Saved/RokUi/Specs"
CANVAS = "/Script/UMG.CanvasPanel"


def canvas_widget(name, rect, *, widget_class=CANVAS, fill=False, hidden=True):
    x, y, width, height = rect
    end = 1 if fill else 0
    return {
        "name": name,
        "class": widget_class,
        "props": {"Visibility": "Collapsed" if hidden else "HitTestInvisible"},
        "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {"LayoutData": (
            f"(Offsets=(Left={x},Top={y},Right={width},Bottom={height}),"
            f"Anchors=(Minimum=(X=0,Y=0),Maximum=(X={end},Y={end})),Alignment=(X=0,Y=0))"
        )}},
    }


def prepare():
    return {
        "expedition_list_layout": {
            "version": 1, "label": "Rok expedition list spacing", "root": "ExpeditionListLayoutRoot",
            "nodes": [
                canvas_widget("StageOffset", (-45, -62, 100, 100)),
                canvas_widget("RewardRow", (396, 658, 79, 63)),
                canvas_widget("EnemyRow", (728, 452, 88, 100)),
                canvas_widget("PreviewEnemyGrid", (1100, 139, 140, 118)),
                canvas_widget("StarRow", (486, 248, 430, 50)),
                canvas_widget("HeroChoiceSize", (0, 0, 1000, 96)),
                canvas_widget("SoldierSize", (0, 0, 580, 101)),
            ],
            "notes": ["Positions and sizes in the Canvas slots configure the existing WBP rows.",
                      "The stage position remains catalog-driven. Edit StageOffset to move the visual relative to that position.",
                      "RewardRow and EnemyRow widths set horizontal spacing. StarRow height sets vertical spacing.",
                      "PreviewEnemyGrid size controls both grid axes. The last two markers give the row extents."],
        },
        "expedition_commander_display": {
            "version": 1, "label": "Rok expedition commander display", "root": "ExpeditionCommanderDisplayRoot",
            "nodes": [canvas_widget("ExpeditionArmyPortrait", (0, 0, 0, 0),
                                    widget_class="/Script/Rok.RokCommanderMeshWidget", fill=True, hidden=False)],
            "notes": ["The authored slot fills the existing presentation host. C++ loads the selected commander."],
        },
    }


def main():
    specs = prepare()
    native = (ROOT / "Source/Rok/RokUnityExpeditionScreen.cpp").read_text(encoding="utf-8-sig")
    SPEC_DIR.mkdir(parents=True, exist_ok=True)
    for name, spec in specs.items():
        names = [node["name"] for node in spec["nodes"]]
        if len(names) != len(set(names)) or any(f'TEXT("{node}")' not in native for node in names):
            raise RuntimeError(f"Unexpected binding names in {name}")
        (SPEC_DIR / f"{name}.json").write_text(json.dumps(spec, indent=2) + "\n", encoding="utf-8")
    print("Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokExpeditionListLayout "
          "expedition_list_layout -RootName=ExpeditionListLayoutRoot")
    print("Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokExpeditionCommanderDisplay "
          "expedition_commander_display -RootName=ExpeditionCommanderDisplayRoot")


if __name__ == "__main__":
    main()
