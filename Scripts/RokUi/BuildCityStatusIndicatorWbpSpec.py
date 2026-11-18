"""Prepare the existing city status layout as an editable WBP; no editor required.

Uses only the project's existing HUD atlas and native layout conventions. No art
is imported or copied. Runtime state, texture selection and projection stay C++.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "Saved/RokUi/Specs/city_status_indicator.json"
ATLAS = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"
CANVAS = "/Script/UMG.CanvasPanel"
IMAGE = "/Script/UMG.Image"
SLOT = "/Script/UMG.CanvasPanelSlot"
TEXT = "/Script/UMG.TextBlock"
REQUIRED = {
    "CityStatusPop": CANVAS,
    "CityStatusText": TEXT,
    "CityHospitalMark": IMAGE,
    "CityUpgradeFlag": IMAGE,
    "CityStatusIdleLarge": IMAGE,
    "CityStatusIdleSmall": IMAGE,
    "CityStatusAction": "/Script/Rok.RokUnityIndexedButton",
}


def layout(rect, anchors=(0, 0, 0, 0), alignment=(0, 0), z=0):
    x, y, w, h = rect
    a, b, c, d = anchors
    u, v = alignment
    return {"class": SLOT, "props": {
        "LayoutData": (f"(Offsets=(Left={x:.6f},Top={y:.6f},Right={w:.6f},Bottom={h:.6f}),"
                       f"Anchors=(Minimum=(X={a:.6f},Y={b:.6f}),Maximum=(X={c:.6f},Y={d:.6f})),"
                       f"Alignment=(X={u:.6f},Y={v:.6f}))"),
        "ZOrder": str(z),
    }}


def node(name, kind, rect, *, props=None, children=None, anchors=(0, 0, 0, 0), alignment=(0, 0), z=0):
    result = {"name": name, "class": kind, "props": props or {},
              "slot": layout(rect, anchors, alignment, z)}
    if children is not None:
        result["children"] = children
    return result


def brush(rect):
    x, y, w, h = rect
    # The existing atlas brush stores Unity bottom-origin rectangles as Slate UVs.
    return ("(TintColor=(SpecifiedColor=(R=1,G=1,B=1,A=1)),DrawAs=Image,ImageType=FullColor,"
            f"ImageSize=(X={w:.6f},Y={h:.6f}),"
            f'ResourceObject="/Script/Engine.Texture2D\'{ATLAS}\'",'
            f"UVRegion=(Min=(X={x/1024:.9f},Y={(1024-y-h)/1024:.9f}),"
            f"Max=(X={(x+w)/1024:.9f},Y={(1024-y)/1024:.9f}),bIsValid=True))")


def background():
    # Preserve the existing native nine-patch behavior instead of changing its borders.
    xs, ys = (976, 992, 1001, 1016), (981, 963, 958, 941)
    positions_x, positions_y = (0, 16, -15), (0, 18, -17)
    extents_x, extents_y = (16, 15, 15), (18, 17, 17)
    patches = []
    for row in range(3):
        for column in range(3):
            rect = (xs[column], ys[row+1], xs[column+1]-xs[column], ys[row]-ys[row+1])
            patches.append(node(
                f"CityStatusBackground_{row}_{column}", IMAGE,
                (positions_x[column], positions_y[row], extents_x[column], extents_y[row]),
                anchors=(1 if column == 2 else 0, 1 if row == 2 else 0,
                         0 if column == 0 else 1, 0 if row == 0 else 1),
                props={"Brush": brush(rect), "Visibility": "HitTestInvisible"},
            ))
    return node("CityStatusBackground", CANVAS, (0, 0, 88, 50),
                props={"Visibility": "HitTestInvisible"}, children=patches)


def build():
    text = node("CityStatusText", REQUIRED["CityStatusText"], (6, 8, 76, 34), props={
        "Text": "",
        "Font": '(TypefaceFontName="Bold",Size=20.000000)',
        "ColorAndOpacity": "(SpecifiedColor=(R=0.35,G=0.33,B=0.27,A=1))",
        "Justification": "Center", "ShadowOffset": "(X=1,Y=1)",
        "ShadowColorAndOpacity": "(R=0,G=0,B=0,A=0.7)",
        "Visibility": "HitTestInvisible",
    })
    text["fonts"] = {"Font": "EngineDefault"}
    pop = node("CityStatusPop", CANVAS, (0, -100, 88, 62), alignment=(.5, 1),
               props={"Visibility": "Collapsed", "RenderTransformPivot": "(X=0.5,Y=1)"}, children=[
        background(),
        node("CityStatusArrow", IMAGE, (39, 50, 10, 12), props={
            "Brush": brush((215, 703, 10, 12)), "Visibility": "HitTestInvisible"}),
        text,
        node("CityHospitalMark", IMAGE, (24, 5, 40, 40), props={"Visibility": "Collapsed"}),
        node("CityStatusAction", REQUIRED["CityStatusAction"], (0, 0, 88, 62), props={
            "WidgetStyle": "(Normal=(DrawAs=NoDrawType),Hovered=(DrawAs=NoDrawType),Pressed=(DrawAs=NoDrawType),Disabled=(DrawAs=NoDrawType))",
            "Visibility": "Visible",
        }),
    ])
    return {"version": 1, "label": "Editable city status indicator", "root": "CityStatusIndicatorRoot",
            "nodes": [
                node("CityUpgradeFlag", IMAGE, (0, 0, 100, 100), props={"Visibility": "Collapsed"}),
                pop,
                node("CityStatusIdleLarge", IMAGE, (0, -35, 34, 34), alignment=(.5, .5), props={"Visibility": "Collapsed"}),
                node("CityStatusIdleSmall", IMAGE, (0, -35, 34, 34), alignment=(.5, .5), props={"Visibility": "Collapsed"}),
            ],
            "notes": [
                "Static layout/styles use existing Rok HUD conventions and existing imported atlas; no new art assets.",
                "C++ binds server-derived captions/medical mark, existing runtime flag/idle textures, visibility, projection and actions.",
                "The popup and idle image Canvas offsets remain Designer-editable; C++ captures them once before projection.",
                "Flag dimensions/position follow source-world projection; idle rise/fade remain native animation.",
            ]}


def walk(nodes):
    for item in nodes:
        yield item
        yield from walk(item.get("children", []))


def verify(spec, tree=None):
    entries = list(walk(spec["nodes"]))
    names = [entry["name"] for entry in entries]
    assert len(set(names)) == len(names), "Duplicate widget names"
    by_name = {entry["name"]: entry for entry in entries}
    for name, kind in REQUIRED.items():
        assert by_name[name]["class"] == kind, f"Wrong/missing binding: {name}"
        assert by_name[name]["slot"]["class"] == SLOT, f"Missing Canvas slot: {name}"
    assert len(by_name["CityStatusBackground"]["children"]) == 9
    assert by_name["CityStatusPop"]["props"]["Visibility"] == "Collapsed"
    assert by_name["CityStatusText"]["props"]["Text"] == "", "No invented server state in templates"
    for entry in entries:
        assert entry["class"] in {CANVAS, IMAGE, REQUIRED["CityStatusText"], REQUIRED["CityStatusAction"]}
        for value in entry.get("props", {}).values():
            assert "/ref/" not in value and "/Saved/" not in value, "No source-file or transient object references"
    native = (ROOT / "Source/Rok/RokCityStatusIndicators.inl").read_text(encoding="utf-8")
    for name in REQUIRED:
        assert f'TEXT("{name}")' in native, f"Native binding missing: {name}"
    assert "if (!bBoundPage)" in native and "Page->RemoveFromParent();" in native
    assert "Marker.PopBaseOffset" in native and "Marker.IdleBaseOffsets[Index]" in native
    if tree:
        actual = {entry["name"]: entry for entry in walk([tree])}
        for name, kind in REQUIRED.items():
            generated_kind = "RokUiTextBlock" if kind == TEXT else kind.rsplit(".", 1)[1]
            assert actual[name]["class"] == generated_kind, f"Generated asset binding mismatch: {name}"
        assert len(actual["CityStatusBackground"]["children"]) == 9
    print("PASS: city status WBP typed bindings, Canvas contracts, nine-patch, native offsets and legacy fallback" +
          ("; generated tree verified" if tree else "; asset/runtime verification not run"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true", help="Read and check the prepared spec without rewriting it")
    parser.add_argument("--tree", type=Path, help="Also inspect a RokUiWbp generated asset tree dump")
    args = parser.parse_args()
    if not args.verify:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(build(), ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
        print(OUTPUT)
    verify(json.loads(OUTPUT.read_text(encoding="utf-8-sig")),
           json.loads(args.tree.read_text(encoding="utf-8-sig")) if args.tree else None)


if __name__ == "__main__":
    main()
