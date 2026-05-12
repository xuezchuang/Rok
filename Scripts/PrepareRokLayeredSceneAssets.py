from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(r"D:\ue\Rok")
SOURCE_ROOT = PROJECT_ROOT / "Saved" / "RokDerivedSprites" / "StructureCutouts"
FALLBACK_ROOT = PROJECT_ROOT / "ref" / "resources" / "Sprite"
OUTPUT_ROOT = PROJECT_ROOT / "Saved" / "RokDerivedSprites" / "LayeredSceneCutouts"

SPRITES = [
    "TownCenter_6_5",
    "Tavern_6_5",
    "Barracks_6_5",
    "Stable_6_5",
    "Archery_6_5",
]

MASKED_SPRITES = [
    ("CityWallUI_1_1", 8),
    ("CityWallUI_1_5", 8),
]


def alpha_bounds(image):
    alpha = image.getchannel("A")
    return alpha.getbbox()


def save_cutout_with_mask(cropped, output_path):
    pixels = cropped.load()
    for y in range(cropped.height):
        for x in range(cropped.width):
            r, g, b, a = pixels[x, y]
            if a <= 18:
                pixels[x, y] = (122, 152, 96, 0)
    cropped.save(output_path)
    mask = cropped.getchannel("A").point(lambda value: 255 if value > 18 else 0)
    mask.convert("RGBA").save(output_path.with_name(f"{output_path.stem}_mask.png"))


def remove_green_screen_pixels(image):
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = pixels[x, y]
            if a <= 0:
                continue
            is_key_green = g - r >= 8 and g - b >= 14 and r < 150 and b < 140
            if is_key_green:
                pixels[x, y] = (r, g, b, 0)


def crop_sprite(sprite_name):
    source_path = SOURCE_ROOT / f"{sprite_name}.png"
    if not source_path.exists():
        source_path = FALLBACK_ROOT / f"{sprite_name}.png"
    if not source_path.exists():
        print(f"missing {sprite_name}")
        return False

    image = Image.open(source_path).convert("RGBA")
    mask_path = FALLBACK_ROOT / f"{sprite_name}_mask.png"
    if mask_path.exists():
        mask = Image.open(mask_path).convert("RGBA").resize(image.size)
        pixels = image.load()
        mask_pixels = mask.load()
        for y in range(image.height):
            for x in range(image.width):
                r, g, b, a = pixels[x, y]
                mr, mg, mb, ma = mask_pixels[x, y]
                alpha = 255 if max(mr, mg, mb) > 55 and max(r, g, b) > 24 else 0
                pixels[x, y] = (r, g, b, min(a, alpha))
    remove_green_screen_pixels(image)
    bounds = alpha_bounds(image)
    if not bounds:
        print(f"empty alpha {sprite_name}")
        return False

    left, top, right, bottom = bounds
    padding = 6
    left = max(0, left - padding)
    top = max(0, top - padding)
    right = min(image.width, right + padding)
    bottom = min(image.height, bottom + padding)
    cropped = image.crop((left, top, right, bottom))

    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_ROOT / f"{sprite_name}.png"
    save_cutout_with_mask(cropped, output_path)
    print(f"{output_path} {image.size}->{cropped.size}")
    return True


def make_masked_sprite(sprite_name, padding):
    source_path = FALLBACK_ROOT / f"{sprite_name}.png"
    mask_path = FALLBACK_ROOT / f"{sprite_name}_mask.png"
    if not source_path.exists() or not mask_path.exists():
        print(f"missing masked sprite {sprite_name}")
        return False

    image = Image.open(source_path).convert("RGBA")
    mask = Image.open(mask_path).convert("RGBA").resize(image.size)
    pixels = image.load()
    mask_pixels = mask.load()

    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = pixels[x, y]
            mr, mg, mb, ma = mask_pixels[x, y]
            alpha = 255 if max(mr, mg, mb) > 18 and max(r, g, b) > 24 else 0
            pixels[x, y] = (r, g, b, min(a, alpha))

    bounds = alpha_bounds(image)
    if not bounds:
        print(f"empty masked sprite {sprite_name}")
        return False

    left, top, right, bottom = bounds
    left = max(0, left - padding)
    top = max(0, top - padding)
    right = min(image.width, right + padding)
    bottom = min(image.height, bottom + padding)
    cropped = image.crop((left, top, right, bottom))

    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_ROOT / f"{sprite_name}.png"
    save_cutout_with_mask(cropped, output_path)
    print(f"{output_path} {image.size}->{cropped.size}")
    return True


def main():
    ok = True
    for sprite_name in SPRITES:
        ok = crop_sprite(sprite_name) and ok
    for sprite_name, padding in MASKED_SPRITES:
        ok = make_masked_sprite(sprite_name, padding) and ok
    if not ok:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
