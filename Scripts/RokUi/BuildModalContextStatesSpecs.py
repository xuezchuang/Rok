"""Prepare create-only modal text/tint states and the unavailable gain label.

Never launches UE. The parent runs the printed GeneratePage commands serially.
"""
import argparse
import json
from pathlib import Path
from BuildTrainResourceCostLayoutSpec import canvas_slot

ROOT = Path(__file__).resolve().parents[2]
STATES_ASSET = "/Game/RokUI/Modal/Items/WBP_RokModal_ContextStates"
UNAVAILABLE_ASSET = "/Game/RokUI/Modal/Items/WBP_RokModal_GainEffectUnavailable"
TEXT_COLORS = {
    "PlayerActionTipRecoveryDefault": (0, 0, 0, 1),
    "PlayerActionTipRecoveryRemaining": (0, 128 / 255, 0, 1),
    "ChangeNameValid": (.43, .84, .45, 1),
    "ChangeNameInvalid": (.96, .58, .26, 1),
    "ChangeNameServiceUnavailable": (.96, .45, .24, 1),
    "GainEffectTabSelectedLabel": (1, .86, .42, 1),
    "GainEffectTabUnselectedLabel": (.46, .40, .31, 1),
}
PLATE_COLORS = {
    "GainEffectTabSelectedPlate": (1, .92, .68, 1),
    "GainEffectTabUnselectedPlate": (.72, .70, .66, 1),
}


def color(value):
    return "(R=%.9f,G=%.9f,B=%.9f,A=%.9f)" % value


def slot(x, y, width, height, z=0):
    result = canvas_slot(x, y, width, height)
    result["props"]["ZOrder"] = str(z)
    return result


def text(name, tint, rect, size=13.5, shadow=0, centered=False):
    return {"name": name, "class": "/Script/UMG.TextBlock", "fonts": {"Font": "EngineDefault"},
        "props": {
            "Font": f'(TypefaceFontName="Regular",Size={size})',
            "ColorAndOpacity": f"(SpecifiedColor={color(tint)})",
            "ShadowOffset": "(X=1,Y=1)" if shadow else "(X=0,Y=0)",
            "ShadowColorAndOpacity": color((0, 0, 0, shadow or 1)),
            "Justification": "Center" if centered else "Left",
            "Visibility": "HitTestInvisible",
        }, "slot": slot(*rect)}


def build():
    states = [text(name, tint, (0, index * 32, 400, 28))
              for index, (name, tint) in enumerate(TEXT_COLORS.items())]
    states += [{"name": name, "class": "/Script/UMG.Image",
        "props": {"ColorAndOpacity": color(tint), "Visibility": "HitTestInvisible"},
        "slot": slot(420, index * 48, 40, 40)}
        for index, (name, tint) in enumerate(PLATE_COLORS.items())]
    return {
        "modal_context_states": {
            "version": 1, "label": "Modal context text and tint states", "root": "ModalContextStatesRoot",
            "nodes": states,
            "notes": [
                "Detached marker page is never mounted. C++ selects state; Designer owns its colors.",
                "Action-point Default and Remaining markers additionally own font, outline and shadow; EngineDefault role resolves before building the transient rich-text rows.",
                "Action-point legacy font is FontPoints(18)=13.5, Regular, black/green, zero shadow offset; no recovery value or timer is fabricated.",
                "Rename and gain labels consume only marker ColorAndOpacity; visible pages retain fonts, slots, shadows, brushes and atlas data.",
                "Missing template/marker and -RokUiLegacy preserve each previous native default.",
            ],
            "references": ["Source/Rok/RokUnityModalWidget.cpp:UpdateChangeNameValidationState/HandleChangeNameConfirmClicked/BuildGainEffect/HandleProfileActionHelpClicked"],
        },
        "modal_gain_effect_unavailable": {
            "version": 1, "label": "Gain effect configuration unavailable label", "root": "GainEffectUnavailableRoot",
            "nodes": [text("GainEffectDefinitionsUnavailable", (.38, .31, .22, 1),
                           (20, 120, 660, 60), size=16, shadow=.55, centered=True)],
            "notes": [
                "Mounted only for the existing economy-tab missing-configuration state. C++ supplies the unchanged localized message.",
                "Label slot is relative to GainEffectListRoot; wrapper fills that root at legacy layer 2 and does not capture input.",
                "Create only; preserve existing gain-effect page, tab switch behavior and live list state.",
            ],
        },
    }


def verify(specs):
    states = specs["modal_context_states"]["nodes"]
    assert {node["name"] for node in states} == set(TEXT_COLORS) | set(PLATE_COLORS)
    for spec in specs.values():
        assert spec["version"] == 1 and spec["root"]
        names = [node["name"] for node in spec["nodes"]]
        assert len(names) == len(set(names))
        for node in spec["nodes"]:
            assert node["slot"]["class"] == "/Script/UMG.CanvasPanelSlot"
            assert "LayoutData" in node["slot"]["props"]
            assert "ResourceObject" not in str(node), "Native markers cannot hold artwork references"
            if node["class"] == "/Script/UMG.TextBlock":
                assert node["fonts"] == {"Font": "EngineDefault"}
                assert node["props"]["Justification"] in ("Left", "Center", "Right")
            else:
                assert node["class"] == "/Script/UMG.Image" and node["name"] in PLATE_COLORS
    for node in states[:2]:
        assert node["props"]["Font"] == '(TypefaceFontName="Regular",Size=13.5)'
        assert node["props"]["ShadowOffset"] == "(X=0,Y=0)"
        assert node["props"]["ShadowColorAndOpacity"] == color((0, 0, 0, 1))
    missing = specs["modal_gain_effect_unavailable"]["nodes"]
    assert len(missing) == 1 and missing[0]["name"] == "GainEffectDefinitionsUnavailable"
    assert missing[0]["slot"] == slot(20, 120, 660, 60)
    assert missing[0]["props"]["Justification"] == "Center"
    print("Modal specs valid: nine native style markers and one unavailable-label slot.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "Saved/RokUi/Specs")
    args = parser.parse_args()
    specs = build()
    verify(specs)
    args.output.mkdir(parents=True, exist_ok=True)
    for key, spec in specs.items():
        path = args.output / f"{key}.json"
        path.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        asset = STATES_ASSET if key == "modal_context_states" else UNAVAILABLE_ASSET
        print(path)
        print(f"Rok.UiWbp.GeneratePage {asset} {key} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={spec['root']}")


if __name__ == "__main__":
    main()
