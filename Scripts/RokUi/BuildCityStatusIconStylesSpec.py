"""Prepare create-only construction-state and harvest-resource brush defaults.

The detached page is never mounted. Existing construction and harvest WBPs retain
their layout and image ColorAndOpacity; C++ retains state, projection and actions.
"""
import argparse
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]
KEY = "city_status_icon_styles"
ASSET = "/Game/RokUI/Hud/WBP_RokCityStatusIconStyles"
ROOT_NAME = "CityStatusIconStylesRoot"
OUTPUT = ROOT / f"Saved/RokUi/Specs/{KEY}.json"
MAINMENU = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"
COMMON = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"
SOURCE = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common"
ATLASES = {
    MAINMENU: ((1024, 1024), SOURCE / "ui_mainmenu.png"),
    COMMON: ((2048, 1024), SOURCE / "ui_common.png"),
}
# Unity bottom-origin rectangles; sizes preserve the current native brush defaults.
STYLES = {
    "ConstructionNewBuild": (MAINMENU, (525, 520, 50, 50), (28, 28)),
    "ConstructionUpgrade": (MAINMENU, (394, 535, 50, 50), (28, 28)),
    "HarvestFood": (COMMON, (865, 349, 80, 80), (60, 60)),
    "HarvestWood": (COMMON, (1085, 489, 80, 80), (60, 60)),
    "HarvestStone": (COMMON, (947, 431, 80, 80), (60, 60)),
    "HarvestGold": (COMMON, (865, 431, 80, 80), (60, 60)),
}


def brush(atlas, rect, size):
    x, y, width, height = rect
    atlas_width, atlas_height = ATLASES[atlas][0]
    return (
        "(TintColor=(SpecifiedColor=(R=1,G=1,B=1,A=1)),DrawAs=Image,"
        "ImageType=FullColor,Tiling=NoTile,Mirroring=NoMirror,"
        f"ImageSize=(X={size[0]},Y={size[1]}),Margin=(),"
        f'ResourceObject="/Script/Engine.Texture2D\'{atlas}\'",'
        f"UVRegion=(Min=(X={x/atlas_width:.12f},Y={(atlas_height-y-height)/atlas_height:.12f}),"
        f"Max=(X={(x+width)/atlas_width:.12f},Y={(atlas_height-y)/atlas_height:.12f}),bIsValid=True))"
    )


def build():
    return {
        "version": 1,
        "label": "Designer-owned city construction and harvest icon brushes",
        "root": ROOT_NAME,
        "nodes": [{
            "name": name,
            "class": "/Script/UMG.Image",
            "props": {"Visibility": "HitTestInvisible", "Brush": brush(atlas, rect, size)},
            "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
                "LayoutData": f"(Offsets=(Left={index%3*100},Top={index//3*100},Right={size[0]},Bottom={size[1]}))",
            }},
        } for index, (name, (atlas, rect, size)) in enumerate(STYLES.items())],
        "notes": [
            "Create only. Never regenerate existing construction bar, harvest bubble or feedback WBPs.",
            "The detached page is resolved once while building the city HUD and never enters its visible hierarchy.",
            "Each image owns the complete brush, including resource, UVs, ImageSize and Brush.TintColor. Existing visible WBP geometry and image ColorAndOpacity remain unchanged.",
            "C++ selects new-build/upgrade or building resource types 2/3/4/5. Timer, fill, projection, harvest normal/full backgrounds, visibility and click/server state remain unchanged.",
            "Missing style image/resource keeps its native brush; absent or malformed visible row/bubble WBP also keeps native art. -RokUiLegacy retains the native path.",
            "Authored brushes do not depend on successful loading of the native fallback atlas. No asset resolution occurs in either update loop.",
            "Unity RS.MMU_levelup refers to missing img_mmu_levelp; the previous img_mmu_levelup prefab default is intentionally retained.",
            "Unity bottom-origin coordinates are converted to top-origin Slate UVs exactly as MakeAtlasBrush. Source PNG dimensions are checked when available.",
        ],
        "references": [
            "Source/Rok/RokMainHudWidget.cpp:BuildConstructionBarPool/UpdateConstructionBars/BuildCityHarvestPops/UpdateCityHarvestPops",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/GlobalCityBuidlingMediator.cs:RefreshhudView",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/RS.cs:MMU_building/MMU_levelup",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIWorker/UI_Tip_BuildingBar.prefab:img_icon",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/GlobalResourceMediator.cs:SetImgByBuildingType",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/CurrencyProxy.cs:GeticonIdByType",
            "ref/config-tables/编译客户端配置/CSV/Currency.csv:100/101/102/103",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_map/UIMainCity/UI_Pop_IconOnBuilding.prefab",
            "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_mainmenu.png.meta",
            "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png.meta",
        ],
    }


def verify(spec):
    assert spec["version"] == 1 and spec["root"] == ROOT_NAME
    assert spec["nodes"] == build()["nodes"], "Initial spec must preserve current atlas, UV, size and tint defaults"
    assert len(spec["nodes"]) == 6
    for atlas, (x, y, width, height), _ in STYLES.values():
        atlas_width, atlas_height = ATLASES[atlas][0]
        assert 0 <= x < x + width <= atlas_width and 0 <= y < y + height <= atlas_height
    checked = skipped = 0
    for size, source in ATLASES.values():
        if not source.is_file():
            skipped += 1
            continue
        with source.open("rb") as stream:
            header = stream.read(24)
        assert header[:8] == b"\x89PNG\r\n\x1a\n" and header[12:16] == b"IHDR"
        assert struct.unpack(">II", header[16:24]) == size, f"Unexpected source atlas dimensions: {source}"
        checked += 1
    print(f"PASS: 6 initial spec brushes, UV bounds, sizes and tints; source PNG dimensions checked={checked}, unavailable={skipped} (spec/PNG checks only)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true", help="Verify the prepared initial spec without rewriting it")
    args = parser.parse_args()
    if not args.verify:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(OUTPUT)
    verify(json.loads(OUTPUT.read_text(encoding="utf-8-sig")))
    print(f"Rok.UiWbp.GeneratePage {ASSET} {KEY} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={ROOT_NAME}")


if __name__ == "__main__":
    main()
