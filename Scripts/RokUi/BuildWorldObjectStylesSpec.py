"""Prepare create-only resource, barbarian quality and fixed portrait brushes.

The detached page never enters the visible hierarchy. Existing popup/reward WBPs
retain geometry and image-level tint; C++ retains item icons, reward data and IDs.
"""
import argparse
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]
KEY = "world_object_styles"
ASSET = "/Game/RokUI/Hud/WBP_RokWorldObjectStyles"
ROOT_NAME = "WorldObjectStylesRoot"
OUTPUT = ROOT / f"Saved/RokUi/Specs/{KEY}.json"
COMMON = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"
MAP_ICON = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_map_icon.rok_hud_atlas_map_icon"
ITEM0 = "/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item0.rok_modal_atlas_item0"
SOURCE = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic"
ATLASES = {
    COMMON: ((2048, 1024), SOURCE / "ui_res_common/ui_common.png"),
    MAP_ICON: ((1024, 2048), SOURCE / "ui_res_map/map_icon.png"),
    ITEM0: ((512, 512), SOURCE / "ui_res_item/item0.png"),
}
STYLES = {}
for resource, rect in (
    ("Food", (865, 349, 945, 429)),
    ("Wood", (1085, 489, 1165, 569)),
    ("Stone", (947, 431, 1027, 511)),
    ("Gold", (865, 431, 945, 511)),
):
    for variant, size in (("Info", (100, 100)), ("Progress", (60, 60))):
        STYLES[f"WorldResource{resource}{variant}"] = COMMON, rect, size
for quality, rect in (
    ("Common", (137, 219, 271, 353)),
    ("Blue", (295, 377, 429, 511)),
    ("Purple", (159, 377, 293, 511)),
    ("Epic", (1, 83, 135, 217)),
    ("Legendary", (1, 219, 135, 353)),
):
    STYLES[f"WorldRewardQuality{quality}"] = ITEM0, rect, (74, 74)
STYLES["WorldBarbarianPortraitCommon"] = COMMON, (960, 731, 1094, 883), (164, 176)
STYLES["WorldBarbarianPortraitMap"] = MAP_ICON, (547, 1847, 727, 2047), (164, 176)


def brush(atlas, rect, size):
    left, bottom, right, top = rect
    width, height = ATLASES[atlas][0]
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
        "label": "Designer-owned world resource and barbarian fixed brush defaults",
        "root": ROOT_NAME,
        "nodes": [{
            "name": name, "class": "/Script/UMG.Image",
            "props": {
                "Visibility": "HitTestInvisible",
                "Brush": brush(atlas, rect, size),
            },
            "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
                "LayoutData": f"(Offsets=(Left={index%5*190},Top={index//5*190},Right={size[0]},Bottom={size[1]}))",
            }},
        } for index, (name, (atlas, rect, size)) in enumerate(STYLES.items())],
        "notes": [
            "Create only. Never regenerate existing resource popup, barbarian popup or reward row WBPs.",
            "Detached page is resolved once per resource selection or reward-group rebuild; no persistent mutable style cache or per-tick creation.",
            "Images own the entire copied brush, including ImageSize and Brush.TintColor. Visible target slots, image ColorAndOpacity and visibility remain authored in their existing WBPs.",
            "Defaults retain the native 100/60 resource brush sizes, 74 quality size, 164x176 portrait size and white brush tint.",
            "Food/wood/stone matching remains case-insensitive; all other resource kinds retain the previous gold brush default.",
            "Reward quality keeps exact 2/3/4/5 cases, with common for all other values. Reward icons, icon atlas/UVs, quantities, text and server data stay dynamic in C++.",
            "The existing HeadIcon comparison still selects exactly the two prior fixed portraits; this template does not expand portrait selection or reward refresh behavior.",
            "Missing page, missing image/resource and -RokUiLegacy keep the previous per-image native fallback.",
            "Unity bottom-origin sprite coordinates are converted to top-origin UVs exactly as MakeAtlasBrush; fixed atlas bounds are checked, and source PNG dimensions are checked when available.",
        ],
        "references": [
            "Source/Rok/RokWorldMapHudWidget.cpp:ShowWorldObjectInfo",
            "Source/Rok/RokWorldMapGatherPanel.cpp:RefreshBarbarianInfo",
            "Source/Rok/RokUnityFeatureScreenWidget.cpp:ResolveBagQualityRect/MakeRewardQualityBrush",
            "Scripts/ImportRokUiReferenceTextures.py:MAIN_HUD_ATLASES/RESOURCE_SEARCH_ATLASES/MODAL_ATLASES",
            "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png",
            "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/map_icon.png",
            "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_item/item0.png",
        ],
    }


def verify(spec):
    assert spec["version"] == 1 and spec["root"] == ROOT_NAME
    assert spec["nodes"] == build()["nodes"], "Initial template must preserve previous atlas, UV and size defaults"
    assert len(spec["nodes"]) == 15
    for atlas, (left, bottom, right, top), _ in STYLES.values():
        width, height = ATLASES[atlas][0]
        assert 0 <= left < right <= width and 0 <= bottom < top <= height
    checked = skipped = 0
    for size, source in ATLASES.values():
        if not source.is_file():
            skipped += 1
            continue
        with source.open("rb") as stream:
            header = stream.read(24)
        assert header[:8] == b"\x89PNG\r\n\x1a\n" and header[12:16] == b"IHDR"
        assert struct.unpack(">II", header[16:24]) == size, f"Unexpected atlas dimensions: {source}"
        checked += 1
    print(f"PASS: 15 initial spec brushes, UV bounds and image sizes; source PNG dimensions checked={checked}, unavailable={skipped} (spec/PNG checks only)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true", help="Check the prepared initial spec without rewriting it")
    args = parser.parse_args()
    if not args.verify:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(OUTPUT)
    verify(json.loads(OUTPUT.read_text(encoding="utf-8-sig")))
    print(f"Rok.UiWbp.GeneratePage {ASSET} {KEY} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={ROOT_NAME}")


if __name__ == "__main__":
    main()
