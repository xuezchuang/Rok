"""Create the editable task styles used when binding existing task WBPs.

This creates a new style template only. Existing task rows/pages are never
regenerated; their layout, live text, tooltips and server actions stay intact.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "Saved/RokUi/Specs/task_presentation.json"
ASSET = "/Game/RokUI/Styles/WBP_RokTaskPresentation"
STATES = {
    "HudMainTitle": (15, "Bold", "EngineDefault", (1, .68, .25, 1), "Clip", 0),
    "HudSideTitle": (15, "Bold", "EngineDefault", (.96, .94, .9, 1), "Clip", 0),
    "ServerTitle": (18, "Regular", "UnitySource", (1, 1, 1, 1), "Clip", 0),
    "ServerDescription": (16.5, "Regular", "UnitySource", (1, 1, 1, 1), "Ellipsis", 0),
    "TabBadgeNormal": (11, "Bold", "EngineDefault", (1, 1, 1, 1), "Clip", 0),
    "TabBadgeOverflow": (7, "Bold", "EngineDefault", (1, 1, 1, 1), "Clip", 4),
}


def build():
    nodes = []
    for index, (name, (size, weight, role, color, overflow, offset_y)) in enumerate(STATES.items()):
        nodes.append({
            "name": name, "class": "/Script/UMG.TextBlock", "fonts": {"Font": role},
            "props": {
                "Text": "", "Font": f'(TypefaceFontName="{weight}",Size={size})',
                "ColorAndOpacity": "(SpecifiedColor=(R=%s,G=%s,B=%s,A=%s))" % color,
                "TextOverflowPolicy": overflow,
                "RenderTransform": f"(Translation=(X=0,Y={offset_y}),Scale=(X=1,Y=1))",
                "Visibility": "HitTestInvisible",
            },
            "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
                "LayoutData": f"(Offsets=(Left=0,Top={index*42},Right=650,Bottom=36))",
            }},
        })
    return {"version": 1, "label": "Task text presentation states", "root": "TaskPresentationRoot",
            "nodes": nodes, "notes": [
                "Unattached style states; labels have no game or account content.",
                "HUD title color only is copied from HudMainTitle/HudSideTitle.",
                "HUD fitting ceilings and bounds come directly from WBP_RokHudTaskRow's TaskTitle/TaskTitleBox and TaskProgress before shrinkage.",
                "ServerTitle/ServerDescription provide font and overflow only; existing row placement, colors and dynamic tooltips remain authored/live.",
                "TabBadgeNormal/TabBadgeOverflow provide font/color and RenderTransform; Translation is relative to each existing page label's slot.",
                "C++ retains fitting, text, 99+ selection, visibility, reward eligibility, actions and legacy fallback.",
            ], "references": [
                "Source/Rok/RokMainHudWidget.cpp: BindTaskRow, ApplyServerTasks",
                "Source/Rok/RokFeatureTaskDaily.inl: BuildTaskRowPage, BindServerTaskPageActions",
                "Source/Rok/RokTaskPresentation.cpp",
            ]}


def verify(spec, tree=None):
    nodes = {node["name"]: node for node in spec["nodes"]}
    assert len(nodes) == len(STATES) and set(nodes) == set(STATES)
    for name, node in nodes.items():
        assert node["class"] == "/Script/UMG.TextBlock"
        assert node["props"]["Text"] == ""
        assert node["fonts"] == {"Font": STATES[name][2]}
        assert "ResourceObject" not in str(node)
    assert nodes["ServerDescription"]["props"]["TextOverflowPolicy"] == "Ellipsis"
    assert "Y=4" in nodes["TabBadgeOverflow"]["props"]["RenderTransform"]
    helper = (ROOT / "Source/Rok/RokTaskPresentation.cpp").read_text(encoding="utf-8-sig")
    hud = (ROOT / "Source/Rok/RokMainHudWidget.cpp").read_text(encoding="utf-8-sig")
    daily = (ROOT / "Source/Rok/RokFeatureTaskDaily.inl").read_text(encoding="utf-8-sig")
    for name in STATES:
        assert f'TEXT("{name}")' in helper, name
    assert ASSET + ".WBP_RokTaskPresentation_C" in helper
    assert "RoleText->ApplyFontRole()" in helper
    assert "Slot->GetSize()" in helper and "Fit.Font.Size" in helper
    assert "TaskTextFits.Reset()" in hud
    assert "CaptureFit(*Title, *TitleBounds" in hud and "CaptureFit(*Progress, *Progress" in hud
    assert "ApplyTitleColor(TaskPresentationTemplate" in hud
    assert "ApplyTabBadge(TaskPresentationTemplate" in daily
    assert daily.count("ApplyServerRowText(TaskPresentationTemplate") == 2
    if tree:
        def walk(node):
            yield node
            for child in node.get("children", []):
                yield from walk(child)
        actual = {node["name"]: node for node in walk(tree)}
        for name in STATES:
            assert actual[name]["class"] == "RokUiTextBlock", name
    print("PASS: task presentation bindings, font roles, fitting ceilings/bounds and legacy fallback" +
          ("; generated tree names/classes verified" if tree else "; asset/runtime verification not run"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--tree", type=Path)
    args = parser.parse_args()
    if not args.verify:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(OUTPUT)
    verify(json.loads(OUTPUT.read_text(encoding="utf-8-sig")),
           json.loads(args.tree.read_text(encoding="utf-8-sig")) if args.tree else None)


if __name__ == "__main__":
    main()
