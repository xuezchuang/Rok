"""Prepare a complete clickable march entry using the existing artwork WBP.

No engine, art import, account state or shared configuration changes are needed.
The list still owns the live entry count and row height; the WBP owns the hit
rectangle, button styles, content alignment and nested presentation.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "Saved/RokUi/Specs/world_march_entry.json"
ART = "/Game/RokUI/Hud/WBP_RokWorldMarchRow.WBP_RokWorldMarchRow_C"
ACTION = "/Script/Rok.RokUnityIndexedButton"


def build():
    return {
        "version": 1,
        "label": "Complete world march entry with existing artwork",
        "root": "WorldMarchEntrySpecRoot",
        "nodes": [{
            "name": "WorldMarchEntryAction", "class": ACTION,
            "props": {
                "Visibility": "Visible",
                "WidgetStyle": (
                    "(Normal=(DrawAs=NoDrawType),Hovered=(DrawAs=NoDrawType),"
                    "Pressed=(DrawAs=NoDrawType),Disabled=(DrawAs=NoDrawType),"
                    "NormalPadding=(Left=0,Top=0,Right=0,Bottom=0),"
                    "PressedPadding=(Left=0,Top=0,Right=0,Bottom=0))"
                ),
            },
            "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
                "LayoutData": "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)),Alignment=(X=0,Y=0))",
            }},
            "children": [{
                "name": "WorldMarchEntryArtwork", "class": ART,
                "props": {"Visibility": "HitTestInvisible"},
                "slot": {"class": "/Script/UMG.ButtonSlot", "props": {
                    "Padding": "(Left=0,Top=0,Right=0,Bottom=0)",
                    "HorizontalAlignment": "HAlign_Center",
                    "VerticalAlignment": "VAlign_Center",
                }},
            }],
        }],
        "notes": [
            "Reuses the existing editable WBP_RokWorldMarchRow; no atlas regions or art assets are copied.",
            "The full-row action and its states, hit area, padding and content alignment are editable in this WBP.",
            "C++ supplies the real army ID, portrait, rarity, selection, march status, tooltip and click callback.",
            "Existing WBP_RokHudListLayout continues to own row and viewport heights; native layout remains the fallback.",
        ],
    }


def verify(spec):
    assert spec["version"] == 1 and len(spec["nodes"]) == 1
    button = spec["nodes"][0]
    assert button["name"] == "WorldMarchEntryAction" and button["class"] == ACTION
    assert button["slot"]["class"] == "/Script/UMG.CanvasPanelSlot"
    assert "Maximum=(X=1,Y=1)" in button["slot"]["props"]["LayoutData"]
    assert all(f"{state}=(DrawAs=NoDrawType)" in button["props"]["WidgetStyle"]
               for state in ("Normal", "Hovered", "Pressed", "Disabled"))
    assert len(button["children"]) == 1
    art = button["children"][0]
    assert art["class"] == ART and art["slot"]["class"] == "/Script/UMG.ButtonSlot"
    assert art["props"]["Visibility"] == "HitTestInvisible"
    assert art["slot"]["props"]["HorizontalAlignment"] == "HAlign_Center"
    assert art["slot"]["props"]["VerticalAlignment"] == "VAlign_Center"
    assert "children" not in art, "Reuse the existing art class instead of duplicating its layout"
    assert (ROOT / "Content/RokUI/Hud/WBP_RokWorldMarchRow.uasset").is_file(), "Existing artwork WBP dependency missing"
    native = (ROOT / "Source/Rok/RokWorldMapGatherPanel.cpp").read_text(encoding="utf-8")
    body = native.split("void URokWorldMapHudWidget::RefreshWorldMarchQueue()", 1)[1].split(
        "void URokWorldMapHudWidget::BuildWorldOverlayArt()", 1)[0]
    assert "WBP_RokWorldMarchEntry_C" in body and 'TEXT("WorldMarchEntryAction")' in body
    assert "if (!Entry)" in body and "Entry->RemoveFromParent();" in body
    assert "RowSize->SetHeightOverride(WorldMarchRowHeight);" in body
    assert "Button->ConfigureAction(March.ArmyIndex);" in body
    assert "&URokWorldMapHudWidget::HandleWorldMarchClicked" in body
    assert "HighlightedArmy == March.ArmyIndex" in body
    for name in ("WorldMarchRarityFrame", "WorldMarchPortrait", "WorldMarchSelectionFrame", "WorldMarchStatusIcon"):
        assert f'TEXT("{name}")' in body
    print("PASS: complete march entry spec, nested existing artwork, native army identity/selection, and legacy fallback; engine not run")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    if not args.verify:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(build(), ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
        print(OUTPUT)
    verify(json.loads(OUTPUT.read_text(encoding="utf-8-sig")))


if __name__ == "__main__":
    main()
