"""Prepare the detached locked Research frame (resource and UV defaults only).

Run with ``py -3.12 -B Scripts/RokUi/BuildResearchLockedFrameStylesSpec.py``;
then run CreateResearchLockedFrameStyles.py in the UE 5.8 headless editor.
Existing research WBPs and the source-gray base material are never regenerated.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
KEY = "research_locked_frame_styles"
ASSET = "/Game/RokUI/Research/WBP_RokResearchLockedFrameStyles"
MATERIAL = "/Game/RokUI/Research/MI_RokResearchLockedFrameGray"
BASE = "/Game/RokPrototype/UI/Research/M_RokResearchGrayUI"
ATLAS = "/Game/RokPrototype/UI/ResearchAtlases/rok_research_ui_research"
ROOT_NAME = "ResearchLockedFrameStylesRoot"
IMAGE_NAME = "ResearchLockedIconFrame"
OUTPUT = ROOT / f"Saved/RokUi/Specs/{KEY}.json"
MANIFEST = ROOT / "Content/RokData/ResearchNative/atlas_manifest.json"


def build():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    sprite = next(row for row in manifest["sprites"]
                  if row["reference"] == "ui_research[img_research_iconBg]")
    assert sprite["ueAssetPath"] == ATLAS
    assert sprite["rect"] == {"x": 471, "y": 1824, "width": 223, "height": 223}
    assert sprite["atlasSize"] == {"width": 1024, "height": 2048}
    uv = sprite["slateUV"]
    assert tuple(uv[key] for key in ("u0", "v0", "u1", "v1")) == (
        .4599609375, .00048828125, .677734375, .109375)
    resource = MATERIAL + "." + MATERIAL.rsplit("/", 1)[1]
    brush = (
        "(TintColor=(SpecifiedColor=(R=1,G=1,B=1,A=1)),DrawAs=Image,"
        "ImageType=FullColor,Tiling=NoTile,Mirroring=NoMirror,"
        "ImageSize=(X=104,Y=104),Margin=(),"
        f'ResourceObject="/Script/Engine.MaterialInstanceConstant\'{resource}\'",'
        f'UVRegion=(Min=(X={uv["u0"]},Y={uv["v0"]}),'
        f'Max=(X={uv["u1"]},Y={uv["v1"]}),bIsValid=True))')
    return {
        "version": 1, "label": "Research locked icon frame resource and UV defaults",
        "root": ROOT_NAME,
        "nodes": [{"name": IMAGE_NAME, "class": "/Script/UMG.Image",
                   "props": {"Visibility": "HitTestInvisible", "Brush": brush},
                   "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
                       "LayoutData": "(Offsets=(Right=104,Bottom=104))"}}}],
        "notes": [
            "Create only this detached template and its dedicated grayscale material instance.",
            "Runtime copies ResourceObject and UVRegion only; visible frame slot, brush tint/size/draw settings and image tint remain unchanged.",
            "SourceAtlas is the existing ui_research atlas. Parent is the existing source-gray UI material; no UV transform or grayscale rebake.",
            "104x104 is the Unity img_iconBg preview size, not a runtime layout override.",
            "Tech icons, prerequisites and visibility remain C++; unsupported/missing resource or invalid UV retains ApplySprite fallback.",
        ],
        "references": [
            "Source/Rok/RokResearchScreenWidget.cpp:BindNodePresentation/ApplySprite",
            "Content/RokData/ResearchNative/atlas_manifest.json:ui_research[img_research_iconBg]",
            "Scripts/CreateRokResearchGrayMaterial.py:GRAY_CODE/uvHandling",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIResearch/UI_ResearchSubItem.prefab:img_iconBg",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_ResearchSubItem_SubView.cs:cp.Gray",
            "ref/client-unity/Assets/BundleAssets/UI/UIShader/ImageGrayShader.shader",
        ],
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    spec = build()
    if not args.verify:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    assert json.loads(OUTPUT.read_text(encoding="utf-8-sig")) == spec
    print("PASS: Research locked frame atlas, source-gray instance reference and exact UV defaults")
    print(OUTPUT)


if __name__ == "__main__":
    main()
