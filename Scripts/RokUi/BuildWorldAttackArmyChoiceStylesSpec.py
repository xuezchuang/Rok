"""Prepare create-only plain/selected brushes for the existing army-choice WBP.

No engine or binary writes. The current row WBP keeps its geometry and content;
the detached style page supplies only Normal/Hovered/Pressed brush values.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
KEY = "world_attack_army_choice_styles"
OUTPUT = ROOT / f"Saved/RokUi/Specs/{KEY}.json"
ASSET = "/Game/RokUI/Hud/WBP_RokWorldAttackArmyChoiceStyles"
ROOT_NAME = "WorldAttackArmyChoiceStylesRoot"
ATLAS = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"
STATES = ("Normal", "Hovered", "Pressed")
RECTS = {
    "WorldAttackArmyPlainStyle": (1098, 571, 1300, 652),
    "WorldAttackArmySelectedStyle": (1766, 586, 2040, 670),
}


def brush(rect):
    left, bottom, right, top = rect
    # Same Unity bottom-origin atlas conversion as MakeAtlasBrush; keep the old
    # 130x68 desired image size for both rectangles and all three button states.
    return (
        "(TintColor=(SpecifiedColor=(R=1,G=1,B=1,A=1)),DrawAs=Image,"
        "ImageType=FullColor,Tiling=NoTile,Mirroring=NoMirror,ImageSize=(X=130,Y=68),Margin=(),"
        "OutlineSettings=(CornerRadii=(X=0,Y=0,Z=0,W=0),"
        "Color=(SpecifiedColor=(R=0,G=0,B=0,A=0)),RoundingType=HalfHeightRadius),"
        f'ResourceObject="/Script/Engine.Texture2D\'{ATLAS}\'",'
        f"UVRegion=(Min=(X={left/2048:.12f},Y={(1024-top)/1024:.12f}),"
        f"Max=(X={right/2048:.12f},Y={(1024-bottom)/1024:.12f}),bIsValid=True))"
    )


def build():
    return {
        "version": 1,
        "label": "Designer-owned existing-army plain and selected brushes",
        "root": ROOT_NAME,
        "nodes": [{
            "name": name, "class": "/Script/UMG.Button",
            "props": {
                "Visibility": "HitTestInvisible",
                "WidgetStyle": "(" + ",".join(f"{state}={brush(rect)}" for state in STATES) + ")",
            },
            "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
                "LayoutData": f"(Offsets=(Left={index*160},Top=0,Right=130,Bottom=68))",
            }},
        } for index, (name, rect) in enumerate(RECTS.items())],
        "notes": [
            "Create only; never regenerate the existing WBP_RokWorldAttackArmyChoice row.",
            "This detached page never enters the screen hierarchy. Its two buttons own only the copied Normal/Hovered/Pressed brushes.",
            "Defaults retain the previous native plain/selected atlas rectangles, white tint, Image draw type and 130x68 image size in every state.",
            "The existing row WBP retains geometry, text, Disabled brush, foregrounds, padding, sounds and focus behavior.",
            "C++ keeps server army eligibility, selected army identity, caption, callbacks and list population.",
            "Missing style page/button or -RokUiLegacy retains the previous native texture/UV fallback.",
            "Reference is the existing RefreshAttackArmyChoices presentation; no new Unity phase or rarity behavior is introduced.",
        ],
    }


def verify(spec):
    assert spec["version"] == 1 and spec["root"] == ROOT_NAME
    assert [node["name"] for node in spec["nodes"]] == list(RECTS)
    for node in spec["nodes"]:
        assert node["class"] == "/Script/UMG.Button"
        assert node["props"]["Visibility"] == "HitTestInvisible"
        assert node["slot"]["class"] == "/Script/UMG.CanvasPanelSlot"
        expected = "(" + ",".join(f"{state}={brush(RECTS[node['name']])}" for state in STATES) + ")"
        assert node["props"]["WidgetStyle"] == expected, "Initial template must preserve the previous brush defaults"
        assert "Disabled=" not in expected, "Native row keeps its authored Disabled brush"
        assert "children" not in node, "Style template must not duplicate row geometry/content"
    cpp = (ROOT / "Source/Rok/RokWorldMapGatherPanel.cpp").read_text(encoding="utf-8-sig")
    body = cpp.split("URokWorldMapHudWidget::FAttackArmyChoiceLayout URokWorldMapHudWidget::BuildAttackArmyChoiceLayout", 1)[1].split(
        "void URokWorldMapHudWidget::SelectAttackArmy", 1)[0]
    assert f"{ASSET}.WBP_RokWorldAttackArmyChoiceStyles_C" in body
    assert "WBP_RokWorldAttackArmyChoice.WBP_RokWorldAttackArmyChoice_C" in body
    for name in RECTS:
        assert f'TEXT("{name}")' in body
    assert "FButtonStyle Style = Button->GetStyle();" in body
    assert "Style.SetNormal(Authored.Normal).SetHovered(Authored.Hovered).SetPressed(Authored.Pressed);" in body
    assert "Choice.bAuthoredSelectionStyle ? nullptr : LoadUiTexture(Common)" in body
    assert "BuildAttackArmyChoiceLayout(March.ArmyIndex, March.ArmyIndex == GatherExistingArmyIndex)" in body
    for rect in RECTS.values():
        assert "FIntRect(" + ", ".join(map(str, rect)) + ")" in body
    assert "CanRetargetArmy(March, GatherTargetId)" in body
    assert "Choice.Button->ConfigureAction(March.ArmyIndex);" in body
    assert "&URokWorldMapHudWidget::SelectAttackArmy" in body
    assert "Choice.Label->SetText(FText::FromString(Caption));" in body
    assert "AttackArmyChoices->AddChild(Choice.Root);" in body
    assert "SetDisabled(" not in body and "SetContent(Label);" in body
    header = (ROOT / "Source/Rok/RokWorldMapHudWidget.h").read_text(encoding="utf-8-sig")
    assert "bool bAuthoredSelectionStyle = false;" in header
    hud = (ROOT / "Source/Rok/RokWorldMapHudWidget.cpp").read_text(encoding="utf-8-sig")
    page = hud.split("URokUiPageWidget* URokWorldMapHudWidget::CreateWorldHudPage", 1)[1].split(
        "URokUiPageWidget* URokWorldMapHudWidget::AddWorldHudPage", 1)[0]
    assert "URokUiSettings::Resolve(Class)" in page
    ui = (ROOT / "Source/Rok/RokUiWidgets.h").read_text(encoding="utf-8-sig")
    assert "if (IsLegacyForced() || Class.IsNull())" in ui
    print("PASS: plain/selected authored brush defaults, retained row geometry/state fields, army actions and legacy fallback (static only)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true", help="Check the prepared default spec without rewriting it")
    args = parser.parse_args()
    if not args.verify:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(build(), ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
        print(OUTPUT)
    verify(json.loads(OUTPUT.read_text(encoding="utf-8-sig")))
    print(f"Rok.UiWbp.GeneratePage {ASSET} {KEY} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={ROOT_NAME}")


if __name__ == "__main__":
    main()
