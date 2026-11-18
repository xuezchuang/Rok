"""Build the separate WBP palette for speed-up prices and chapter group labels.

Only ColorAndOpacity is consumed; the mounted pages keep their font/layout.
Existing FeatureTextStates and full page assets do not need regeneration.
"""
import json
from pathlib import Path

from BuildFeatureTextPresentationSpecs import text

ROOT = Path(__file__).resolve().parents[2]
SPEC_PATH = ROOT / "Saved/RokUi/Specs/feature_text_colors.json"
STATES = {
    "BuildingSpeedUpPriceAffordable": (19, True, (1, 1, 1, 1)),
    "BuildingSpeedUpPriceInsufficient": (19, True, (1, .32, .26, 1)),
    "TaskChapterCatalogGroupSelected": (16, True, (1, 1, 1, 1)),
    "TaskChapterCatalogGroupUnselected": (16, True, (.78, .74, .66, 1)),
}


def build():
    return {
        "version": 1, "label": "FeatureScreen state colors", "root": "FeatureTextColorsRoot",
        "nodes": [text(name, (0, index * 42, 650, 36), *style)
            for index, (name, style) in enumerate(STATES.items())],
        "notes": ["Unattached palette: only ColorAndOpacity is read, including Slate color mode.",
            "C++ owns Gems >= ShortcutPrice and the selected chapter group ID.",
            "BuyPrice and TaskChapterGroupButton_*Label retain the mounted WBP font/layout.",
            "Legacy C++ presentation remains the fallback when the palette or a state is missing.",
            "Create FeatureTextColors only; do not regenerate FeatureTextStates or whole pages."],
        "references": ["Source/Rok/RokUnityFeatureScreenWidget.cpp: BuildServerBuildingSpeedUp, BuildTaskChapter",
            "Source/Rok/RokFeatureTextPresentation.cpp: TryGetFeatureTextColor"],
    }


def verify(spec):
    assert set(STATES) == {node["name"] for node in spec["nodes"]}
    assert len(spec["nodes"]) == len(STATES)
    for node in spec["nodes"]:
        assert node["class"] == "/Script/UMG.TextBlock"
        assert "ResourceObject" not in str(node)
    source = (ROOT / "Source/Rok/RokUnityFeatureScreenWidget.cpp").read_text(encoding="utf-8-sig")
    for state in STATES:
        assert 'TEXT("' + state + '")' in source, (state, "missing runtime binding")
    helper = (ROOT / "Source/Rok/RokFeatureTextPresentation.cpp").read_text(encoding="utf-8-sig")
    color_helper = helper.split("::TryGetFeatureTextColor(", 1)[1].split("::ApplyTaskRewardTextOnlyState(", 1)[0]
    assert 'TEXT("FeatureTextColors")' in color_helper
    assert "OutColor = Label->GetColorAndOpacity();" in color_helper
    assert "GetSpecifiedColor" not in color_helper
    print("FeatureTextColors: four color-only bindings; source/spec contracts passed.")


def main():
    spec = build()
    verify(spec)
    SPEC_PATH.parent.mkdir(parents=True, exist_ok=True)
    SPEC_PATH.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(SPEC_PATH)


if __name__ == "__main__":
    main()
