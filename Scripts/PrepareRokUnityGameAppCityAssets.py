from pathlib import Path

try:
    from PIL import Image
except ImportError:
    Image = None


PROJECT_ROOT = Path(__file__).resolve().parents[1]
UNITY_PROJECT_ROOT = Path(r"D:\ueDemo\Rok-unity\client-unity")
TREE_ATLAS = UNITY_PROJECT_ROOT / "Assets/BundleAssets/Map/Textures/Tree_maps/build_tree_yezi_1.png"
OUTPUT_ROOT = PROJECT_ROOT / "Saved/RokUnityGameAppCity/Trees"

PALM_BOXES = {
    "PalmPatch_01": (5, 0, 602, 627),
    "PalmPatch_05": (712, 523, 1152, 1067),
    "PalmPatch_06": (9, 1547, 465, 1949),
    "PalmPatch_07": (1475, 1490, 1800, 1777),
}


def crop_alpha_bounds(image, padding=8):
    bounds = image.getchannel("A").getbbox()
    if not bounds:
        return image
    left, top, right, bottom = bounds
    return image.crop((
        max(0, left - padding),
        max(0, top - padding),
        min(image.width, right + padding),
        min(image.height, bottom + padding),
    ))


def main():
    expected_outputs = [OUTPUT_ROOT / "{0}.png".format(name) for name in PALM_BOXES]
    if Image is None:
        if all(path.exists() for path in expected_outputs):
            print("Pillow is not installed; reusing existing Unity GameApp tree crops.")
            for path in expected_outputs:
                print(path)
            return
        raise SystemExit("Pillow is required to crop Unity tree atlas: {0}".format(TREE_ATLAS))

    if not TREE_ATLAS.exists():
        raise SystemExit("Missing Unity tree atlas: {0}".format(TREE_ATLAS))
    atlas = Image.open(TREE_ATLAS).convert("RGBA")
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    for name, box in PALM_BOXES.items():
        crop = crop_alpha_bounds(atlas.crop(box))
        output_path = OUTPUT_ROOT / "{0}.png".format(name)
        crop.save(output_path)
        print("{0} {1}".format(output_path, crop.size))


if __name__ == "__main__":
    main()
