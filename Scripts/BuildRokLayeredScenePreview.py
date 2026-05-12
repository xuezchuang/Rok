from pathlib import Path

from PIL import Image, ImageFilter, ImageOps

import RokCity2DLayout as city2d

PROJECT_ROOT = Path(r"D:\ue\Rok")
SPRITE_ROOT = PROJECT_ROOT / "ref" / "resources" / "Sprite"
CUTOUT_ROOT = PROJECT_ROOT / "Saved" / "RokDerivedSprites" / "LayeredSceneCutouts"
FALLBACK_CUTOUT_ROOT = PROJECT_ROOT / "Saved" / "RokDerivedSprites" / "StructureCutouts"
OUTPUT_PATH = PROJECT_ROOT / "Saved" / "RokLayeredScenePreview.png"
GROUND_PLATE_PATH = PROJECT_ROOT / "Saved" / "RokDerivedSprites" / "LayeredGround" / "LayeredGroundPlate.png"

IMAGE_SIZE = (1280, 760)
PREVIEW_ORIGIN = (640, 352)
PREVIEW_SCALE = 0.739
GROUND_PLATE_PREVIEW_SIZE = (
    int(round(city2d.IMAGE_SIZE[0] * PREVIEW_SCALE)),
    int(round(city2d.IMAGE_SIZE[1] * PREVIEW_SCALE)),
)

GRASS_TEXTURE_PATH = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "BundleAssets" / "land" / "Texture" / "ground_mask_green_02.png"
GRASS_COLOR = (118, 154, 62, 255)


def world_to_pixel(world_x, world_y):
    source_x, source_y = city2d.world_to_pixel(world_x, world_y)
    return (
        int(round(PREVIEW_ORIGIN[0] + (source_x - city2d.ORIGIN[0]) * PREVIEW_SCALE)),
        int(round(PREVIEW_ORIGIN[1] + (source_y - city2d.ORIGIN[1]) * PREVIEW_SCALE)),
    )


def apply_mask(image, mask_path):
    if not mask_path.exists():
        return image
    mask = Image.open(mask_path).convert("RGBA").resize(image.size)
    pixels = image.load()
    mask_pixels = mask.load()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, _a = pixels[x, y]
            mr, mg, mb, _ma = mask_pixels[x, y]
            pixels[x, y] = (r, g, b, 255 if max(mr, mg, mb) > 18 else 0)
    return image


def scaled_sprite(source_path, scale=1.0, mask_path=None):
    image = Image.open(source_path).convert("RGBA")
    if mask_path:
        image = apply_mask(image, mask_path)
    target_size = (
        max(1, int(image.width * scale * city2d.SPRITE_SCALE_PER_WORLD_UNIT * PREVIEW_SCALE)),
        max(1, int(image.height * scale * city2d.SPRITE_SCALE_PER_WORLD_UNIT * PREVIEW_SCALE)),
    )
    return image.resize(target_size, Image.Resampling.LANCZOS)


def make_grass_background():
    base = Image.new("RGBA", IMAGE_SIZE, GRASS_COLOR)
    if not GRASS_TEXTURE_PATH.exists():
        return base
    texture = Image.open(GRASS_TEXTURE_PATH).convert("RGB").resize(IMAGE_SIZE, Image.Resampling.BICUBIC)
    texture = ImageOps.grayscale(texture).filter(ImageFilter.GaussianBlur(0.8))
    colored = ImageOps.colorize(texture, black=(92, 125, 54), white=(146, 174, 78)).convert("RGBA")
    return Image.blend(base, colored, 0.22)


def paste_center(canvas, image, world_x, world_y):
    px, py = world_to_pixel(world_x, world_y)
    canvas.alpha_composite(image, (px - image.width // 2, py - image.height // 2))


def paste_center_bottom(canvas, image, world_x, world_y):
    px, py = world_to_pixel(world_x, world_y)
    canvas.alpha_composite(image, (px - image.width // 2, py - image.height))


def projected_depth(world_x, world_y):
    return city2d.projected_depth(world_x, world_y)


def draw_shadow(canvas, world_x, world_y, width, height):
    shadow = Image.new("RGBA", (max(24, int(width)), max(18, int(height))), (0, 0, 0, 0))
    alpha = Image.new("L", shadow.size, 0)
    from PIL import ImageDraw

    draw = ImageDraw.Draw(alpha)
    draw.ellipse((4, 5, shadow.width - 4, shadow.height - 5), fill=70)
    alpha = alpha.filter(ImageFilter.GaussianBlur(max(2, shadow.height // 7)))
    shadow.putalpha(alpha)
    px, py = world_to_pixel(world_x + 16, world_y - 22)
    canvas.alpha_composite(shadow, (px - shadow.width // 2, py - shadow.height // 2))


def main():
    canvas = make_grass_background()
    if GROUND_PLATE_PATH.exists():
        paste_center(
            canvas,
            Image.open(GROUND_PLATE_PATH).convert("RGBA").resize(GROUND_PLATE_PREVIEW_SIZE, Image.Resampling.LANCZOS),
            city2d.GROUND_PLATE_CENTER[0],
            city2d.GROUND_PLATE_CENTER[1],
        )
    else:
        for sprite_name, x, y, scale, _sort_priority in city2d.GROUND_TILES:
            source_path = SPRITE_ROOT / f"{sprite_name}.png"
            mask_path = SPRITE_ROOT / f"{sprite_name}_mask.png"
            if source_path.exists():
                paste_center(canvas, scaled_sprite(source_path, scale, mask_path), x, y)

    for _label, sprite_name, mask_name, x, y, scale, _sort_offset in sorted(city2d.WALL_SPRITES, key=lambda item: projected_depth(item[3], item[4])):
        source_path = CUTOUT_ROOT / sprite_name
        mask_path = None
        if not source_path.exists():
            source_path = SPRITE_ROOT / sprite_name
            mask_path = SPRITE_ROOT / mask_name if mask_name else None
        if source_path.exists():
            paste_center_bottom(canvas, scaled_sprite(source_path, scale, mask_path), x, y)

    for sprite_name, mask_name, x, y, scale in sorted(city2d.DECOR, key=lambda item: projected_depth(item[2], item[3])):
        source_path = SPRITE_ROOT / sprite_name
        mask_path = SPRITE_ROOT / mask_name if mask_name else None
        if source_path.exists():
            paste_center_bottom(canvas, scaled_sprite(source_path, scale, mask_path), x, y)

    for _building_type, sprite_base, x, y, scale in sorted(city2d.BUILDINGS, key=lambda item: projected_depth(item[2], item[3])):
        source_path = CUTOUT_ROOT / f"{sprite_base}_6_5.png"
        if not source_path.exists():
            source_path = FALLBACK_CUTOUT_ROOT / f"{sprite_base}_6_5.png"
        if not source_path.exists():
            source_path = SPRITE_ROOT / f"{sprite_base}_6_5.png"
        if not source_path.exists():
            continue
        image = scaled_sprite(source_path, scale)
        paste_center_bottom(canvas, image, x, y)

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(OUTPUT_PATH)
    print(OUTPUT_PATH)


if __name__ == "__main__":
    main()
