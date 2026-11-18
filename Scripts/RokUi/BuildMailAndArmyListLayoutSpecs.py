"""Author the system-mail host and the army list's three row extents."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]


def layout_node(name, bounds, *, host=False):
    slot = canvas_slot(*bounds)
    if host:
        slot["props"]["ZOrder"] = "6"
    return {
        "name": name,
        "class": "/Script/UMG.CanvasPanel" if host else "/Script/UMG.Border",
        "props": {"Visibility": "SelfHitTestInvisible"} if host else
                 {"BrushColor": "(R=.20,G=.40,B=.55,A=.65)"},
        "slot": slot,
    }


def write_spec(asset, name, nodes, notes, source):
    root_name = asset + "Root"
    data = {"version": 1, "label": asset, "root": root_name, "nodes": nodes,
            "notes": notes, "references": [source]}
    path = ROOT / "Saved/RokUi/Specs" / f"{name}.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_{asset} {name} "
          f"-ParentClass=/Script/Rok.RokUiPageWidget -RootName={root_name}")


def main():
    write_spec("ServerMailLayout", "server_mail_layout", [
        layout_node("ServerMailHost", (547, 128, 830, 590), host=True),
    ], [
        "The existing ServerMailScreen fills this Canvas host at runtime.",
        "The host slot owns geometry and its ZOrder supplies the layer relative to the mail frame.",
    ], "Source/Rok/RokSystemMailScreen.cpp: TryBuildSessionMailWbp")
    write_spec("ArmyShowListLayout", "army_show_list_layout", [
        layout_node("ArmyShowWaitingExtent", (0, 0, 1050, 120)),
        layout_node("ArmyShowEmptyExtent", (0, 136, 1050, 120)),
        layout_node("ArmyShowMarchExtent", (0, 272, 1050, 124)),
    ], [
        "Only the slot sizes are consumed. The positions arrange the samples in Designer.",
        "C++ selects waiting, empty or march, while existing row WBPs keep their contents and style.",
    ], "Source/Rok/RokUnityFeatureScreenWidget.cpp: BuildArmyShow")


if __name__ == "__main__":
    main()
