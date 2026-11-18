"""Prepare create-only authored feedback styles and complete Help action rows.

No engine or binary asset writes. Existing artwork WBPs are reused unchanged.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC_ROOT = ROOT / "Saved/RokUi/Specs"
TRANSPARENT = "(Normal=(DrawAs=NoDrawType),Hovered=(DrawAs=NoDrawType),Pressed=(DrawAs=NoDrawType),Disabled=(DrawAs=NoDrawType),NormalPadding=(Left=0,Top=0,Right=0,Bottom=0),PressedPadding=(Left=0,Top=0,Right=0,Bottom=0))"
CONTRACTS = (
    ("button_feedback_styles", "/Game/RokUI/Hud/WBP_RokButtonFeedbackStyles", "ButtonFeedbackStylesRoot"),
    ("help_group_entry", "/Game/RokUI/Help/WBP_RokHelpGroupEntry", "HelpGroupEntryRoot"),
    ("help_question_entry", "/Game/RokUI/Help/WBP_RokHelpQuestionEntry", "HelpQuestionEntryRoot"),
)


def full_slot(z=0):
    return {"class": "/Script/UMG.CanvasPanelSlot", "props": {
        "LayoutData": "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)),Alignment=(X=0,Y=0))",
        "ZOrder": str(z),
    }}


def color(value, alpha=1):
    return f"(R={value:.9f},G={value:.9f},B={value:.9f},A={alpha:.9f})"


def style():
    states = (("Normal", color(1)), ("Hovered", color(245/255)),
              ("Pressed", color(200/255)), ("Disabled", color(200/255, 128/255)))
    return "(" + ",".join(f"{name}=(DrawAs=NoDrawType,TintColor=(SpecifiedColor={tint}))" for name, tint in states) + ")"


def entry(kind, art):
    catalog = json.loads((ROOT / "Content/RokData/help_catalog.json").read_text(encoding="utf-8-sig"))
    rect = catalog["layouts"][kind.lower()]["btn_btn"]["rect"]
    assert rect[:2] == [0, 0], "Full-row authored action requires source-local zero origin"
    return {
        "version": 1, "label": f"Complete clickable Help {kind.lower()} row",
        "root": f"Help{kind}EntryRoot",
        "nodes": [
            {"name": f"Help{kind}Extent", "class": "/Script/UMG.SizeBox", "slot": full_slot(),
             "props": {"Visibility": "Collapsed", "WidthOverride": str(rect[2]), "HeightOverride": str(rect[3]),
                       "bOverride_WidthOverride": "True", "bOverride_HeightOverride": "True"}},
            {"name": f"Help{kind}Artwork", "class": art, "slot": full_slot(),
             "props": {"Visibility": "HitTestInvisible"}},
            {"name": f"Help{kind}Action", "class": "/Script/Rok.RokUnityIndexedButton", "slot": full_slot(10),
             "props": {"Visibility": "Visible", "WidgetStyle": TRANSPARENT}},
        ],
        "notes": [
            "The existing row artwork WBP supplies exact source glyph and image geometry.",
            "This WBP owns the full-row clickable region and all button styles; no native overlay on the authored route.",
            "Edit the HelpGroupExtent / HelpQuestionExtent SizeBox width and height to change mounted row size and the stretched hit rectangle together.",
            "C++ supplies source text, group/question identity, arrow state, real expansion/detail callbacks, and dynamic list placement.",
            "Native overlays remain available when an entry class/action is unavailable or -RokUiLegacy is selected.",
        ],
    }


def build():
    return {
        "button_feedback_styles": {
            "version": 1, "label": "Designer-owned source ColorBlock states", "root": "ButtonFeedbackStylesRoot",
            "nodes": [{"name": name, "class": "/Script/UMG.Button", "props": {
                "WidgetStyle": style(), "Visibility": "HitTestInvisible"},
                "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
                    "LayoutData": f"(Offsets=(Left={index*180},Top=0,Right=160,Bottom=48))"}}}
                for index, name in enumerate(("WorldFilterStyle", "PlacementConfirmStyle", "PlacementCancelStyle"))],
            "notes": [
                "Runtime copies Normal/Hovered/Pressed/Disabled TintColor values; this style-only page never enters the screen hierarchy.",
                "Values are Unity source Gamma ColorBlock values, not pre-linearized display values.",
                "World filters preserve their existing direct Slate path. Placement preserves SourceTint material gamma handling and GrayChildrens validity behavior.",
                "C++ retains state priority, focus behavior, validity checks, reset/closing lifecycle and 0.1-second transition timing.",
                "Source: UI_Model_Ck_MainIFLod.prefab ColorBlock and placement GameButton source ColorBlock already traced in native implementations.",
            ],
        },
        "help_group_entry": entry("Group", "/Game/RokUI/Help/WBP_RokHelpGroupRow.WBP_RokHelpGroupRow_C"),
        "help_question_entry": entry("Question", "/Game/RokUI/Help/WBP_RokHelpQuestionRow.WBP_RokHelpQuestionRow_C"),
    }


def verify(specs):
    for key, _, root in CONTRACTS:
        assert specs[key]["root"] == root and specs[key]["version"] == 1
    palette = specs["button_feedback_styles"]["nodes"]
    assert [n["name"] for n in palette] == ["WorldFilterStyle", "PlacementConfirmStyle", "PlacementCancelStyle"]
    for node in palette:
        assert all(f"{state}=(DrawAs=NoDrawType,TintColor=" in node["props"]["WidgetStyle"]
                   for state in ("Normal", "Hovered", "Pressed", "Disabled"))
    for key, kind in (("help_group_entry", "Group"), ("help_question_entry", "Question")):
        extent, *nodes = specs[key]["nodes"]
        assert extent["name"] == f"Help{kind}Extent" and extent["class"] == "/Script/UMG.SizeBox"
        assert float(extent["props"]["WidthOverride"]) > 0 and float(extent["props"]["HeightOverride"]) > 0
        assert len(nodes) == 2 and nodes[1]["name"] == f"Help{kind}Action"
        assert nodes[1]["class"] == "/Script/Rok.RokUnityIndexedButton"
        assert nodes[1]["props"]["WidgetStyle"] == TRANSPARENT
        assert "Maximum=(X=1,Y=1)" in nodes[1]["slot"]["props"]["LayoutData"]
        assert nodes[0]["class"].endswith(f"WBP_RokHelp{kind}Row_C")
        assert nodes[0]["props"]["Visibility"] == "HitTestInvisible"
        assert (ROOT / f"Content/RokUI/Help/WBP_RokHelp{kind}Row.uasset").is_file()
    help_cpp = (ROOT / "Source/Rok/RokHelpWidget.cpp").read_text(encoding="utf-8-sig")
    for kind in ("Group", "Question"):
        assert f'WBP_RokHelp{kind}Entry_C' in help_cpp
        assert f'TEXT("Help{kind}Action")' in help_cpp
        assert f"if (!bAuthored{kind}Action)" in help_cpp
    assert "RowActions.Reset()" in help_cpp and "&URokHelpWidget::HandleAction" in help_cpp
    assert 'RowExtent(Row,TEXT("HelpGroupExtent"),GroupSize)' in help_cpp
    assert 'RowExtent(Row,TEXT("HelpQuestionExtent"),QuestionSize)' in help_cpp
    assert "Fixed->SetWidthOverride(ContentWidth)" in help_cpp
    helper = (ROOT / "Source/Rok/RokButtonFeedbackStyle.h").read_text(encoding="utf-8-sig")
    assert "URokUiSettings::Resolve(Class)" in helper and "Button->GetStyle()" in helper
    world = (ROOT / "Source/Rok/RokWorldMapHudWidget.cpp").read_text(encoding="utf-8-sig")
    main = (ROOT / "Source/Rok/RokMainHudWidget.cpp").read_text(encoding="utf-8-sig")
    assert 'FilterFeedbackStyle.Resolve(this, TEXT("WorldFilterStyle"))' in world
    assert "FRokWorldMapFilterTint::Resolve" in world and "FilterTints[Index].Advance" in world
    for kind in ("Confirm", "Cancel"):
        assert f'Placement{kind}FeedbackStyle.Resolve(this, TEXT("Placement{kind}Style"))' in main
    assert 'SetVectorParameterValue(TEXT("SourceTint"), Current)' in main
    print("PASS: authored state color specs, complete Help action regions, existing routes and legacy fallback (static only)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    if args.verify:
        specs = {key: json.loads((SPEC_ROOT / f"{key}.json").read_text(encoding="utf-8-sig")) for key, _, _ in CONTRACTS}
    else:
        specs = build()
        SPEC_ROOT.mkdir(parents=True, exist_ok=True)
        for key, _, _ in CONTRACTS:
            path = SPEC_ROOT / f"{key}.json"
            path.write_text(json.dumps(specs[key], ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
            print(path)
    verify(specs)
    for key, asset, root in CONTRACTS:
        print(f"Rok.UiWbp.GeneratePage {asset} {key} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={root}")


if __name__ == "__main__":
    main()
