"""Build editable FeatureScreen text-only and state-style templates.

These cover remaining styles/layout in successful production WBP branches.
They do not create gameplay state, replace whole pages, or import artwork.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC_ROOT = ROOT / "Saved" / "RokUi" / "Specs"
STATES = {
    "GeneralSettingsRecommendationDefault": (15, False, (1, 1, 1, 1)),
    "GeneralSettingsRecommendationHighlight": (15, False, (83 / 255, 219 / 255, 91 / 255, 1)),
    "TaskChapterDialogueDefault": (17, True, (1, 1, 1, 1)),
    "TaskChapterDialogueHighlight": (17, True, (1, .43, .02, 1)),
    "WorkerQueueHirePriceAffordable": (20, True, (1, 1, 1, 1)),
    "WorkerQueueHirePriceInsufficient": (20, True, (1, .32, .26, 1)),
}


def text(name, rect, size, bold, color):
    x, y, w, h = rect
    return {"name": name, "class": "/Script/UMG.TextBlock", "fonts": {"Font": "EngineDefault"}, "props": {
        "Text": name,
        "Font": '(TypefaceFontName="%s",Size=%s)' % ("Bold" if bold else "Regular", size),
        "ColorAndOpacity": "(SpecifiedColor=(R=%s,G=%s,B=%s,A=%s))" % color,
        "ShadowOffset": "(X=0,Y=0)", "ShadowColorAndOpacity": "(R=0,G=0,B=0,A=0)",
        "Justification": "ETextJustify::Left", "Visibility": "HitTestInvisible",
    }, "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
        "LayoutData": f"(Offsets=(Left={x},Top={y},Right={w},Bottom={h}),Anchors=(Minimum=(X=0,Y=0),Maximum=(X=0,Y=0)),Alignment=(X=0,Y=0))",
        "ZOrder": "1"}}}


def build():
    return {
        "feature_text_states": {"version": 1, "label": "FeatureScreen text state styles", "root": "FeatureTextStatesRoot",
            "nodes": [text(name, (0, index * 42, 650, 36), *style) for index, (name, style) in enumerate(STATES.items())],
            "notes": ["Unattached style definitions; sample node labels are never displayed as account/game content.",
                "C++ retains recommendation calculation, chapter dialogue text, queue affordability and server writes.",
                "Worker price uses palette colors only; its font and placement remain in WorkerQueueSecondaryRow.",
                "Based on the existing UE FeatureScreen presentation; no third-party artwork or screenshot geometry."],
            "references": ["Source/Rok/RokUnityFeatureScreenWidget.cpp: ApplyGraphicsSettingsRecommendation, PopulateServerTaskChapterGuide, BuildWorkerQueue"]},
        "task_reward_text_only": {"version": 1, "label": "Task reward without icon", "root": "TaskRewardTextOnlyRoot",
            "nodes": [text("TaskRewardTextOnlyLabel", (0, 3, 152, 27), 17, False, (.95, .90, .75, 1))],
            "notes": ["Alternate layout/style is applied to the existing reward label, preserving its live text and tooltip.",
                "The default task WBP remains mounted; this handles its no-icon reward state."],
            "references": ["Source/Rok/RokFeatureTaskDaily.inl: BuildTaskRowPage"]},
    }


def verify(specs):
    assert set(STATES) == {node["name"] for node in specs["feature_text_states"]["nodes"]}
    for label, spec in specs.items():
        names = [node["name"] for node in spec["nodes"]]
        assert len(names) == len(set(names)), (label, "duplicate names")
        for node in spec["nodes"]:
            assert node["class"] == "/Script/UMG.TextBlock"
            assert node["fonts"] == {"Font": "EngineDefault"}
            assert "ResourceObject" not in str(node)
    source = (ROOT / "Source/Rok/RokUnityFeatureScreenWidget.cpp").read_text(encoding="utf-8-sig")
    for state in STATES:
        assert 'TEXT("' + state + '")' in source, (state, "missing runtime binding")
    helper = (ROOT / "Source/Rok/RokFeatureTextPresentation.cpp").read_text(encoding="utf-8-sig")
    assert 'TEXT("TaskRewardTextOnlyLabel")' in helper
    assert "RoleText->ApplyFontRole()" in helper
    assert "TargetLayout->SetLayout(Layout->GetLayout())" in helper
    print("FeatureTextStates: six runtime style bindings; TaskRewardTextOnly: authored fallback geometry. Contracts passed.")


def main():
    specs = build()
    fallback = specs["task_reward_text_only"]["nodes"][0]
    fallback["props"]["ShadowOffset"] = "(X=1,Y=1)"
    fallback["props"]["ShadowColorAndOpacity"] = "(R=0,G=0,B=0,A=0.65)"
    verify(specs)
    SPEC_ROOT.mkdir(parents=True, exist_ok=True)
    for name, spec in specs.items():
        target = SPEC_ROOT / (name + ".json")
        target.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(target)


if __name__ == "__main__":
    main()
