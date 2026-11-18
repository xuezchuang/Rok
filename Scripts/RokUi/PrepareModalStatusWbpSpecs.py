"""Prepare native Rok text-only status templates; never launch UE or edit assets.

Run: py -3.12 -B Scripts/RokUi/PrepareModalStatusWbpSpecs.py
Then pass the printed commands to EditorCmd.ps1 in the parent agent's serialized
asset-generation step. Existing assets must be preserved (GeneratePage is create-only).
No third-party textures, glyph meshes, atlases or distinctive visual assets are used.
"""

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def message_node(name, rect, size, color, *, centered=False, visible=False, shadow=0.55, fill=False):
    x, y, width, height = rect
    red, green, blue, alpha = color
    anchor_end = 1 if fill else 0
    return {
        "name": name,
        "class": "/Script/UMG.TextBlock",
        "fonts": {"Font": "EngineDefault"},
        "props": {
            "Text": "",
            "Font": f'(TypefaceFontName="Regular",Size={size:.6f})',
            "ColorAndOpacity": (
                f"(SpecifiedColor=(R={red:.6f},G={green:.6f},B={blue:.6f},A={alpha:.6f}))"
            ),
            "Justification": "ETextJustify::Center" if centered else "ETextJustify::Left",
            "ShadowOffset": "(X=1.000000,Y=1.000000)" if shadow else "(X=0.000000,Y=0.000000)",
            "ShadowColorAndOpacity": f"(R=0.000000,G=0.000000,B=0.000000,A={shadow:.6f})",
            "Visibility": "HitTestInvisible" if visible else "Collapsed",
        },
        "slot": {
            "class": "/Script/UMG.CanvasPanelSlot",
            "props": {
                "LayoutData": (
                    f"(Offsets=(Left={x:.6f},Top={y:.6f},Right={width:.6f},Bottom={height:.6f}),"
                    f"Anchors=(Minimum=(X=0.000000,Y=0.000000),Maximum=(X={anchor_end:.6f},Y={anchor_end:.6f})),"
                    "Alignment=(X=0.000000,Y=0.000000))"
                ),
                "ZOrder": "0",
            },
        },
    }


def prepare(output):
    beige = (0.38, 0.31, 0.22, 1.0)
    specs = {
        "modal_status_messages": {
            "version": 1,
            "label": "Native Rok modal status messages",
            "root": "ModalStatusRoot",
            "nodes": [
                message_node("Standard", (212, 300, 700, 60), 16, beige, centered=True),
                message_node("Wide", (212, 300, 760, 60), 16, beige, centered=True),
                message_node("BuildingLevelError", (400, 350, 650, 60), 22, (1, 0, 0, 1)),
                message_node("BarracksHeaderError", (700, 570, 480, 30), 15, (1, 0, 0, 1)),
            ],
            "notes": [
                "Only the selected layout is shown by C++; text and legacy identity are runtime values.",
                "Designer owns each layout's rectangle, base font, color and shadow.",
                "Matches the existing native text-only fallback; no third-party visual assets.",
            ],
        },
        "modal_unavailable_status_messages": {
            "version": 1,
            "label": "Rok unavailable catalog and attribute messages",
            "root": "ModalUnavailableStatusRoot",
            "nodes": [
                message_node("PlayerHeadCatalogError", (300, 230, 800, 60), 24, (1, 1, 1, 1)),
                message_node("PlayerRankingCatalogError", (400, 350, 640, 50), 20, (1, 0, 0, 1)),
                message_node("BarracksAttributeError", (0, 0, 0, 0), 15, (1, 1, 1, 1), fill=True),
                message_node("BuffDefinitionError", (42, 132, 416, 60), 16, beige, centered=True),
            ],
            "notes": ["C++ supplies the original error text and selects one layout.",
                      "The barracks label fills the existing attributes slot, including Designer edits to that slot."],
        },
        "help_customer_service_boundary": {
            "version": 1,
            "label": "Native Rok customer-service boundary message",
            "root": "HelpCustomerServiceBoundaryRoot",
            "nodes": [message_node(
                "HelpCustomerServiceBoundary", (460, 320, 600, 30), 14, (1, 1, 1, 1),
                visible=True, shadow=0,
            )],
            "notes": ["Message is supplied on the existing Help customer-service click; no external page is opened."],
        },
    }
    output.mkdir(parents=True, exist_ok=True)
    for name, spec in specs.items():
        (output / f"{name}.json").write_text(
            json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
        )
    return specs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "Saved/RokUi/Specs")
    args = parser.parse_args()
    specs = prepare(args.output)
    print(f"Prepared {len(specs)} specs in {args.output}")
    print("Rok.UiWbp.GeneratePage /Game/RokUI/Modal/Items/WBP_RokModal_StatusMessages "
          "modal_status_messages -ParentClass=/Script/Rok.RokUiPageWidget -RootName=ModalStatusRoot")
    print("Rok.UiWbp.GeneratePage /Game/RokUI/Modal/Items/WBP_RokModal_UnavailableStatusMessages "
          "modal_unavailable_status_messages -ParentClass=/Script/Rok.RokUiPageWidget -RootName=ModalUnavailableStatusRoot")
    print("Rok.UiWbp.GeneratePage /Game/RokUI/Help/WBP_RokHelpCustomerServiceBoundary "
          "help_customer_service_boundary -ParentClass=/Script/Rok.RokUiPageWidget "
          "-RootName=HelpCustomerServiceBoundaryRoot")


if __name__ == "__main__":
    main()
