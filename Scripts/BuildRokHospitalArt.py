"""Export the two bed sprites from UI_Win_Hospital's Unity atlas."""
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ATLAS = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_build/ui_hospital.tpsheet"
OUTPUT = ROOT / "Content/RokData/Hospital"
SPRITES = {"ui_hospital_bedEmpty": "bed_empty.png", "ui_hospital_bedFull": "bed_full.png"}


if __name__ == "__main__":
    with Image.open(ATLAS.with_suffix(".png")) as atlas:
        OUTPUT.mkdir(parents=True, exist_ok=True)
        found = set()
        for line in ATLAS.read_text(encoding="utf-8-sig").splitlines():
            fields = line.split(";")
            if fields[0] not in SPRITES:
                continue
            x, y, width, height = map(int, fields[1:5])
            top = atlas.height - y - height
            atlas.crop((x, top, x + width, top + height)).save(OUTPUT / SPRITES[fields[0]])
            found.add(fields[0])
        if found != SPRITES.keys():
            raise ValueError("Unity hospital atlas does not contain both bed sprites")
