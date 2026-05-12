import os
from collections import deque
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(os.environ.get("ROK_PROJECT_ROOT", r"D:\ue\Rok"))
SOURCE_ROOT = PROJECT_ROOT / "ref" / "resources" / "Sprite"
CUTOUT_MODE = os.environ.get("ROK_NAMED_CUTOUT_MODE", "maskmax").lower()
if CUTOUT_MODE == "edge":
    DEFAULT_OUTPUT_NAME = "EdgeCutouts"
elif CUTOUT_MODE == "softkey":
    DEFAULT_OUTPUT_NAME = "SoftKeyCutouts"
elif CUTOUT_MODE == "structure":
    DEFAULT_OUTPUT_NAME = "StructureCutouts"
else:
    DEFAULT_OUTPUT_NAME = "MaskMaxCutouts"
OUTPUT_ROOT = Path(os.environ.get("ROK_NAMED_CUTOUT_OUTPUT", PROJECT_ROOT / "Saved" / "RokDerivedSprites" / DEFAULT_OUTPUT_NAME))
NAMED_CIVILIZATION = os.environ.get("ROK_CITY_CLUSTER_NAMED_CIV", "6")

SPRITE_BASES = [
    "TownCenter",
    "Castle",
    "AllianceCenter",
    "Barracks",
    "Stable",
    "Archery",
    "SiegeWorkshop",
    "Campus",
    "Hospital",
    "Tavern",
]


def is_green_background(rgb):
    r, g, b = rgb
    return g >= 55 and g >= r * 1.02 and g > b * 1.10


def is_edge_background(rgb):
    r, g, b = rgb
    if is_green_background(rgb):
        return True
    if r < 72 and g < 92 and b < 82:
        return True
    if g >= 45 and r <= 110 and b <= 105 and g >= b * 0.95:
        return True
    return False


def edge_connected_green_mask(image):
    width, height = image.size
    pixels = image.load()
    visited = bytearray(width * height)
    queue = deque()

    def push(x, y):
        index = y * width + x
        if visited[index]:
            return
        if is_edge_background(pixels[x, y][:3]):
            visited[index] = 1
            queue.append((x, y))

    for x in range(width):
        push(x, 0)
        push(x, height - 1)
    for y in range(height):
        push(0, y)
        push(width - 1, y)

    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < width and 0 <= ny < height:
                push(nx, ny)
    return visited


def make_mask_max_cutout(source_path, mask_path, output_path):
    image = Image.open(source_path).convert("RGBA")
    mask = Image.open(mask_path).convert("RGBA").resize(image.size)
    width, height = image.size
    pixels = image.load()
    mask_pixels = mask.load()

    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            mr, mg, mb, ma = mask_pixels[x, y]
            alpha = 255 if max(mr, mg, mb) > 18 else 0
            pixels[x, y] = (r, g, b, alpha)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path)
    return output_path


def make_green_edge_cutout(source_path, output_path):
    image = Image.open(source_path).convert("RGBA")
    width, height = image.size
    pixels = image.load()
    remove_mask = edge_connected_green_mask(image)

    for y in range(height):
        for x in range(width):
            index = y * width + x
            r, g, b, a = pixels[x, y]
            pixels[x, y] = (r, g, b, 0 if remove_mask[index] else a)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path)
    return output_path


def make_soft_key_cutout(source_path, output_path):
    image = Image.open(source_path).convert("RGBA")
    width, height = image.size
    pixels = image.load()

    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            greenish = g > 70 and g > r * 1.02 and g > b * 1.08
            likely_background = greenish and r > 70 and b < 112
            if likely_background:
                dominance = min(1.0, max(0.0, (g - max(r, b) - 10.0) / 55.0))
                alpha = int(255 * (1.0 - dominance))
                if dominance > 0.65:
                    alpha = 0
                pixels[x, y] = (r, g, b, min(a, alpha))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path)
    return output_path


def is_city_terrain_color(rgb):
    r, g, b = rgb
    if g < 44:
        return False
    if g > r * 1.08 and g > b * 1.10:
        return True
    if g > r + 18 and g > b + 14 and b < 125:
        return True
    return False


def make_structure_cutout(source_path, mask_path, output_path):
    image = Image.open(source_path).convert("RGBA")
    mask = Image.open(mask_path).convert("RGBA").resize(image.size)
    width, height = image.size
    pixels = image.load()
    mask_pixels = mask.load()

    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            mr, mg, mb, ma = mask_pixels[x, y]
            mask_coverage = max(mr, mg, mb)
            keeps_structure_channel = mb > 12 or mg > 24
            keeps_non_terrain_region = mr > 28 and not is_city_terrain_color((r, g, b))
            keeps_dark_contact_shadow = mr > 92 and g < 92 and b < 92
            alpha = 255 if mask_coverage > 18 and (keeps_structure_channel or keeps_non_terrain_region or keeps_dark_contact_shadow) else 0
            pixels[x, y] = (r, g, b, min(a, alpha))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path)
    return output_path


def main():
    for base in SPRITE_BASES:
        name = f"{base}_{NAMED_CIVILIZATION}_5.png"
        source_path = SOURCE_ROOT / name
        mask_path = SOURCE_ROOT / f"{base}_{NAMED_CIVILIZATION}_5_mask.png"
        if not source_path.exists():
            print(f"skip missing {source_path}")
            continue
        output_path = OUTPUT_ROOT / name
        if CUTOUT_MODE == "edge":
            make_green_edge_cutout(source_path, output_path)
        elif CUTOUT_MODE == "softkey":
            make_soft_key_cutout(source_path, output_path)
        elif CUTOUT_MODE == "structure" and mask_path.exists():
            make_structure_cutout(source_path, mask_path, output_path)
        elif mask_path.exists():
            make_mask_max_cutout(source_path, mask_path, output_path)
        else:
            make_green_edge_cutout(source_path, output_path)
        print(f"wrote {output_path}")


if __name__ == "__main__":
    main()
