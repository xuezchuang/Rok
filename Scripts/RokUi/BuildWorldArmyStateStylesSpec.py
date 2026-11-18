"""Prepare create-only shared rarity and march-state image brushes.

The detached page supplies presentation only. Existing WBP rows retain their
geometry and C++ retains phase/rarity decisions, data-driven portraits and actions.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
KEY = "world_army_state_styles"
ASSET = "/Game/RokUI/Hud/WBP_RokWorldArmyStateStyles"
ROOT_NAME = "WorldArmyStateStylesRoot"
OUTPUT = ROOT / f"Saved/RokUi/Specs/{KEY}.json"
UI = "/Game/RokPrototype/UI/CommanderAtlases/rok_commander_ui.rok_commander_ui"
HEAD = "/Game/RokPrototype/UI/CommanderAtlases/rok_commander_head.rok_commander_head"
MAP = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_map.rok_hud_atlas_map"
WHITE = (1, 1, 1, 1)
STYLES = {
    "ArmyFrameBase": (UI, (2048, 512), (1, 53, 135, 205), (134, 152), WHITE),
    "ArmyFrameAdvanced": (UI, (2048, 512), (1, 53, 135, 205), (134, 152), (.62, 1, .70, 1)),
    "ArmyFrameElite": (UI, (2048, 512), (1, 53, 135, 205), (134, 152), (.55, .78, 1, 1)),
    "ArmyFrameEpic": (HEAD, (1024, 2048), (729, 1895, 863, 2047), (134, 152), WHITE),
    "ArmyFrameLegendary": (HEAD, (1024, 2048), (593, 1895, 727, 2047), (134, 152), WHITE),
}
for state, x, y in (
    ("Rest", 305, 120), ("Collecting", 449, 96), ("Returning", 545, 84),
    ("Marching", 257, 72), ("Battling", 960, 285), ("Garrisoned", 305, 24),
    ("Rallying", 401, 50),
):
    STYLES[f"ArmyStatus{state}"] = (MAP, (1024, 512), (x, y, x+46, y+46), (46, 46), WHITE)


def color(tint):
    return "(" + ",".join(f"{key}={value:.12f}" for key, value in zip("RGBA", tint)) + ")"


def brush(atlas, atlas_size, rect, size):
    left, bottom, right, top = rect
    width, height = atlas_size
    return (
        "(TintColor=(SpecifiedColor=(R=1,G=1,B=1,A=1)),DrawAs=Image,"
        "ImageType=FullColor,Tiling=NoTile,Mirroring=NoMirror,"
        f"ImageSize=(X={size[0]},Y={size[1]}),Margin=(),"
        f'ResourceObject="/Script/Engine.Texture2D\'{atlas}\'",'
        f"UVRegion=(Min=(X={left/width:.12f},Y={(height-top)/height:.12f}),"
        f"Max=(X={right/width:.12f},Y={(height-bottom)/height:.12f}),bIsValid=True))"
    )


def build():
    return {
        "version": 1,
        "label": "Shared army rarity and march-state brush defaults",
        "root": ROOT_NAME,
        "nodes": [{
            "name": name, "class": "/Script/UMG.Image",
            "props": {
                "Visibility": "HitTestInvisible",
                "Brush": brush(atlas, atlas_size, rect, size),
                "ColorAndOpacity": color(tint),
            },
            "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
                "LayoutData": f"(Offsets=(Left={index%5*160},Top={index//5*170},Right={size[0]},Bottom={size[1]}))",
            }},
        } for index, (name, (atlas, atlas_size, rect, size, tint)) in enumerate(STYLES.items())],
        "notes": [
            "Create only. Existing row and screen WBPs remain authoritative for layout and content.",
            "This detached style page never enters the visible hierarchy. One page is resolved per synchronous build/refresh batch.",
            "Each style owns image resource/UV and tint. Existing resource/UV-only call sites retain the target brush fields; other call sites copy the whole style brush. Target size, slots, heights and visibility stay unchanged.",
            "World queue multiplies the authored row image tint by the selected style tint. Expedition retains each call site's previous replace-or-preserve tint behavior.",
            "World rarity uses exact 5/4/3/2 cases; Expedition keeps its >=5 legendary case. Unknown/default march phases retain the Rest glyph.",
            "Portrait coordinates remain catalog-derived. Army eligibility, IDs, callbacks, phase tooltips and selection logic stay in C++.",
            "Missing page, image or resource, and -RokUiLegacy, keep the previous native atlas/tint fallback.",
        ],
        "references": [
            "Source/Rok/RokWorldMapGatherPanel.cpp:RefreshWorldMarchQueue",
            "Source/Rok/RokUnityExpeditionScreen.cpp:BuildExpeditionTask/BuildArmyComposer",
        ],
    }


def verify(spec):
    expected = build()
    assert spec["version"] == 1 and spec["root"] == ROOT_NAME
    assert spec["nodes"] == expected["nodes"], "Initial spec must retain the previous atlas, UV and tint defaults"
    print("PASS: 12 initial image brushes, atlas regions, sizes and tint defaults (spec only)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    if not args.verify:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(build(), ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
        print(OUTPUT)
    verify(json.loads(OUTPUT.read_text(encoding="utf-8-sig")))
    print(f"Rok.UiWbp.GeneratePage {ASSET} {KEY} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={ROOT_NAME}")


if __name__ == "__main__":
    main()
