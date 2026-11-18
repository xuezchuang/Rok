"""Prepare create-only worker queue illustration brush defaults.

The detached page is read only by the existing production row WBP branch.
Visible rows retain their layout, image tint and visibility; C++ retains state.
"""
import argparse
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
KEY = "worker_queue_illustration_styles"
ASSET = "/Game/RokUI/Feature/WBP_RokWorkerQueueIllustrationStyles"
ROOT_NAME = "WorkerQueueIllustrationStylesRoot"
OUTPUT = ROOT / f"Saved/RokUi/Specs/{KEY}.json"
ATLAS = "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_worker_matrix_icon.rok_fullscreen_worker_matrix_icon"
ATLAS_SIZE = (512, 2048)
SOURCE = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_else/matrix_icon.png"
STYLES = {
    "WorkerQueueIllustrationBusy": ("matrix_icon_1004", (182, 1215, 179, 137)),
    "WorkerQueueIllustrationIdle": ("matrix_icon_1005", (182, 1354, 179, 137)),
    "WorkerQueueIllustrationLocked": ("matrix_icon_1000", (1, 659, 179, 137)),
}


def brush(rect):
    left, bottom, width, height = rect
    atlas_width, atlas_height = ATLAS_SIZE
    return (
        "(TintColor=(SpecifiedColor=(R=1,G=1,B=1,A=1)),DrawAs=Image,"
        "ImageType=FullColor,Tiling=NoTile,Mirroring=NoMirror,"
        "ImageSize=(X=179,Y=137),Margin=(),"
        f'ResourceObject="/Script/Engine.Texture2D\'{ATLAS}\'",'
        f"UVRegion=(Min=(X={left/atlas_width:.12f},Y={(atlas_height-bottom-height)/atlas_height:.12f}),"
        f"Max=(X={(left+width)/atlas_width:.12f},Y={(atlas_height-bottom)/atlas_height:.12f}),bIsValid=True))"
    )


def build():
    return {
        "version": 1,
        "label": "Designer-owned worker queue illustration brush defaults",
        "root": ROOT_NAME,
        "nodes": [{
            "name": name, "class": "/Script/UMG.Image",
            "props": {"Visibility": "HitTestInvisible", "Brush": brush(rect)},
            "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
                "LayoutData": f"(Offsets=(Left={index*190},Top=0,Right=179,Bottom=137))",
            }},
        } for index, (name, (_, rect)) in enumerate(STYLES.items())],
        "notes": [
            "Create only. Do not regenerate the existing WorkerQueue page or its PrimaryRow/SecondaryRow assets.",
            "One detached template is created per successful production WBP build; it never enters the visible hierarchy.",
            "Busy selects Busy, Idle selects Idle, every other state including Unknown retains the Locked illustration.",
            "Each image owns its full brush, including atlas, UV, image size and brush tint; existing row slot, image-level tint and visibility stay unchanged.",
            "Missing template, optional marker or marker resource uses the previous per-image native atlas fallback. -RokUiLegacy retains the native branch.",
            "No queue availability, rental, renewal, price, progress, action or server-write behavior is authored here.",
            "Unity bottom-origin coordinates are converted to top-origin UVs exactly as MakeAtlasBrush.",
        ],
        "references": [
            "Source/Rok/RokUnityFeatureScreenWidget.cpp:BuildWorkerQueue/WorkerBusyRect/WorkerIdleRect/WorkerLockedRect",
            "Scripts/ImportRokUiReferenceTextures.py:WORKER_QUEUE_ATLASES",
            str(SOURCE.relative_to(ROOT)).replace("\\", "/"),
            str(SOURCE.relative_to(ROOT)).replace("\\", "/") + ".meta",
        ],
    }


def verify(spec):
    assert spec["version"] == 1 and spec["root"] == ROOT_NAME
    assert spec["nodes"] == build()["nodes"], "Initial spec must preserve prior atlas, UV and size defaults"
    assert len(spec["nodes"]) == 3
    for _, (left, bottom, width, height) in STYLES.values():
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
        for sprite, rect in STYLES.values():
            match = re.search(r"name: " + sprite + r"\s+rect:\s+serializedVersion: \d+\s+x: (\d+)\s+y: (\d+)\s+width: (\d+)\s+height: (\d+)", data)
            assert match and tuple(map(int, match.groups())) == rect, sprite
        checked.append("Unity sprite metadata")
    print("PASS: three initial illustration brushes and UV bounds; " + ", ".join(checked) + " (static checks only)")


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
