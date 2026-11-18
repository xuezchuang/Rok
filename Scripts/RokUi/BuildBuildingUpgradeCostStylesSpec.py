"""Prepare create-only shared building/barracks upgrade resource brush defaults.

Existing upgrade pages retain their slots, image tint and positive-cost compaction.
The detached template owns only the four resource brushes, never gameplay state.
"""
import argparse
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
KEY = "building_upgrade_cost_styles"
ASSET = "/Game/RokUI/Modal/Items/WBP_RokBuildingUpgradeCostStyles"
ROOT_NAME = "BuildingUpgradeCostStylesRoot"
OUTPUT = ROOT / f"Saved/RokUi/Specs/{KEY}.json"
ATLAS = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"
ATLAS_SIZE = (2048, 1024)
SOURCE = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png"
# Resource identity is independent of the positive-cost display slot index.
STYLES = {
    "UpgradeCostFood": (100, "img_com_cur1", (865, 349, 80, 80)),
    "UpgradeCostWood": (101, "img_com_cur2", (1085, 489, 80, 80)),
    "UpgradeCostStone": (102, "img_com_cur3", (947, 431, 80, 80)),
    "UpgradeCostGold": (103, "img_com_cur4", (865, 431, 80, 80)),
}


def brush(rect):
    left, bottom, width, height = rect
    atlas_width, atlas_height = ATLAS_SIZE
    return (
        "(TintColor=(SpecifiedColor=(R=1,G=1,B=1,A=1)),DrawAs=Image,"
        "ImageType=FullColor,Tiling=NoTile,Mirroring=NoMirror,"
        "ImageSize=(X=46,Y=46),Margin=(),"
        f'ResourceObject="/Script/Engine.Texture2D\'{ATLAS}\'",'
        f"UVRegion=(Min=(X={left/atlas_width:.12f},Y={(atlas_height-bottom-height)/atlas_height:.12f}),"
        f"Max=(X={(left+width)/atlas_width:.12f},Y={(atlas_height-bottom)/atlas_height:.12f}),bIsValid=True))"
    )


def build():
    return {
        "version": 1,
        "label": "Designer-owned shared building upgrade resource brushes",
        "root": ROOT_NAME,
        "nodes": [{
            "name": name, "class": "/Script/UMG.Image",
            "props": {"Visibility": "HitTestInvisible", "Brush": brush(rect)},
            "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
                "LayoutData": f"(Offsets=(Left={index*64},Top=0,Right=46,Bottom=46))",
            }},
        } for index, (name, (_, _, rect)) in enumerate(STYLES.items())],
        "notes": [
            "Create only. Never regenerate the existing BuildingUpgrade, BarracksUpgrade or BarracksUpgradeSource pages.",
            "One detached style page is resolved per production upgrade page bind and never enters the visible hierarchy.",
            "Each image owns its complete brush, including resource, UV, ImageSize and brush tint. Existing visible page slots and image ColorAndOpacity remain authored in their page WBPs.",
            "C++ selects by resource identity, preserves food/wood/stone/gold order, compacts strictly positive costs and hides unused slots. Amounts, formats and all upgrade actions remain unchanged.",
            "Missing template, optional marker or marker resource keeps the existing per-icon native atlas fallback. Authored brushes do not require the fallback atlas to load. -RokUiLegacy keeps its native path.",
            "Instant upgrade gem, cost text, availability, quotes, resource validation, server writes and barracks attributes are outside this brush set.",
            "Unity bottom-origin atlas rectangles are converted to top-origin Slate UVs exactly as MakeAtlasBrush.",
        ],
        "references": [
            "Source/Rok/RokUnityModalWidget.cpp:BuildingUpgradeCosts/BindBuildingUpgradePage/FoodCurrencyRect/WoodCurrencyRect/StoneCurrencyRect/GoldCurrencyRect",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateMediator.cs:ResourcesConsume",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/CurrencyProxy.cs:GeticonIdByType",
            "ref/config-tables/编译客户端配置/CSV/Currency.csv:100/101/102/103",
            "Scripts/ImportRokUiReferenceTextures.py:MAIN_HUD_ATLASES",
            str(SOURCE.relative_to(ROOT)).replace("\\", "/"),
            str(SOURCE.relative_to(ROOT)).replace("\\", "/") + ".meta",
        ],
    }


def verify(spec):
    assert spec["version"] == 1 and spec["root"] == ROOT_NAME
    assert spec["nodes"] == build()["nodes"], "Initial spec must preserve native brush defaults"
    assert len(spec["nodes"]) == 4
    for _, _, (left, bottom, width, height) in STYLES.values():
        assert 0 <= left < left+width <= ATLAS_SIZE[0]
        assert 0 <= bottom < bottom+height <= ATLAS_SIZE[1]
    checked = []
    if SOURCE.is_file():
        with SOURCE.open("rb") as stream:
            header = stream.read(24)
        assert header[:8] == b"\x89PNG\r\n\x1a\n" and header[12:16] == b"IHDR"
        assert struct.unpack(">II", header[16:24]) == ATLAS_SIZE
        checked.append("PNG dimensions")
    metadata = Path(str(SOURCE) + ".meta")
    if metadata.is_file():
        data = metadata.read_text(encoding="utf-8-sig")
        for _, sprite, rect in STYLES.values():
            match = re.search(r"name: " + sprite + r"\s+rect:\s+serializedVersion: \d+\s+x: (\d+)\s+y: (\d+)\s+width: (\d+)\s+height: (\d+)", data)
            assert match and tuple(map(int, match.groups())) == rect, sprite
        checked.append("Unity sprite metadata")
    print("PASS: four initial cost brushes, UV bounds, 46x46 sizes and white tints; "
          + ", ".join(checked) + " (static checks only)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true", help="Check the initial spec without rewriting it")
    args = parser.parse_args()
    if not args.verify:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(OUTPUT)
    verify(json.loads(OUTPUT.read_text(encoding="utf-8-sig")))
    print(f"Rok.UiWbp.GeneratePage {ASSET} {KEY} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={ROOT_NAME}")


if __name__ == "__main__":
    main()
