"""Prepare the native profile data-presentation WBP; never run UE or edit assets.

This is the normal profile page's existing optional civilization/action section,
not a missing-configuration state. Geometry starts at the current UE placement;
the Designer owns its slots. The template contains native Image/ProgressBar widgets
and no copied textures or third-party style resources. C++ binds the already-used
civilization icon only when that existing data flag is present.
"""

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def slot(x, y, width, height, layer):
    return {"class": "/Script/UMG.CanvasPanelSlot", "props": {
        "LayoutData": (
            f"(Offsets=(Left={x:.6f},Top={y:.6f},Right={width:.6f},Bottom={height:.6f}),"
            "Anchors=(Minimum=(X=0.000000,Y=0.000000),Maximum=(X=0.000000,Y=0.000000)),"
            "Alignment=(X=0.000000,Y=0.000000))"
        ),
        "ZOrder": str(layer),
    }}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "Saved/RokUi/Specs")
    args = parser.parse_args()
    spec = {
        "version": 1,
        "label": "Native Rok profile data presentation",
        "root": "ProfileDataPresentationRoot",
        "nodes": [
            {
                "name": "CivilizationIcon",
                "class": "/Script/UMG.Image",
                "props": {"Visibility": "HitTestInvisible"},
                "slot": slot(1053, 243, 53, 51, 0),
            },
            {
                "name": "ActionFill",
                "class": "/Script/UMG.ProgressBar",
                "props": {
                    "Visibility": "HitTestInvisible",
                    "Percent": "0.000000",
                    "WidgetStyle": "(BackgroundImage=(DrawAs=NoDrawType))",
                    "FillColorAndOpacity": "(R=0.200000,G=0.650000,B=0.850000,A=1.000000)",
                    "BorderPadding": "(X=0.000000,Y=0.000000)",
                    "BarFillType": "LeftToRight",
                },
                "slot": slot(577.39, 427.73, 528.15, 18, 1),
            },
        ],
        "notes": [
            "C++ supplies the current civilization icon and action-point ratio; it does not author these slots.",
            "ActionFill uses the engine's native ProgressBar style, not the source atlas strip.",
            "Unknown role data removes the corresponding pieces; no account values are fabricated.",
            "No fonts or third-party texture/style resources are serialized in this template.",
        ],
    }
    args.output.mkdir(parents=True, exist_ok=True)
    path = args.output / "modal_profile_data_presentation.json"
    path.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared {path}")
    print("Rok.UiWbp.GeneratePage /Game/RokUI/Modal/Items/WBP_RokModal_ProfileDataPresentation "
          "modal_profile_data_presentation -ParentClass=/Script/Rok.RokUiPageWidget "
          "-RootName=ProfileDataPresentationRoot")


if __name__ == "__main__":
    main()
