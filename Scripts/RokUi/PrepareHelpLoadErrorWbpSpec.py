"""Prepare the shared native Help load-error layout; never launch UE or edit assets.

Run: python -B Scripts/RokUi/PrepareHelpLoadErrorWbpSpec.py
Send the printed GeneratePage command to the parent agent's serialized asset step.
The existing fallback uses a default UTextBlock (Roboto Bold 24, white, transparent
shadow). Intentionally omit a font-role override so generation preserves that
native asset-font default instead of switching to a different composite font.
"""

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "Saved/RokUi/Specs")
    args = parser.parse_args()
    spec = {
        "version": 1,
        "label": "Shared native Rok Help load-error presentation",
        "root": "HelpLoadErrorRoot",
        "nodes": [{
            "name": "HelpLoadError",
            "class": "/Script/UMG.TextBlock",
            "props": {"Text": "", "Visibility": "Visible"},
            "slot": {
                "class": "/Script/UMG.CanvasPanelSlot",
                "props": {
                    "LayoutData": (
                        "(Offsets=(Left=300.000000,Top=300.000000,Right=800.000000,Bottom=100.000000),"
                        "Anchors=(Minimum=(X=0.000000,Y=0.000000),Maximum=(X=0.000000,Y=0.000000)),"
                        "Alignment=(X=0.000000,Y=0.000000))"
                    ),
                    "ZOrder": "0",
                },
            },
        }],
        "notes": [
            "Used for the four existing Help catalog/layout/glyph failure display branches.",
            "C++ supplies the original LoadError verbatim and preserves failure return behavior.",
            "The error's slot and native default text style are editable in the WBP.",
            "No custom texture, glyph mesh, external font or third-party visual assets are copied.",
        ],
    }
    args.output.mkdir(parents=True, exist_ok=True)
    path = args.output / "help_load_error.json"
    path.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared {path}")
    print("Rok.UiWbp.GeneratePage /Game/RokUI/Help/WBP_RokHelpLoadError "
          "help_load_error -ParentClass=/Script/Rok.RokUiPageWidget -RootName=HelpLoadErrorRoot")


if __name__ == "__main__":
    main()
