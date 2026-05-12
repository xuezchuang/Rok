from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageOps

import RokCity2DLayout as city2d

PROJECT_ROOT = Path(r"D:\ue\Rok")
SPRITE_ROOT = PROJECT_ROOT / "ref" / "resources" / "Sprite"
LAND_TEXTURE_ROOT = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "BundleAssets" / "land" / "Texture"
GRASS_TEXTURE_PATH = LAND_TEXTURE_ROOT / "ground_mask_green_02.png"
OUTPUT_PATH = PROJECT_ROOT / "Saved" / "RokDerivedSprites" / "LayeredGround" / "LayeredGroundPlate.png"
OUTPUT_MASK_PATH = PROJECT_ROOT / "Saved" / "RokDerivedSprites" / "LayeredGround" / "LayeredGroundPlate_mask.png"
GRASS_FILL_OUTPUT_PATH = PROJECT_ROOT / "Saved" / "RokDerivedSprites" / "LayeredGround" / "LayeredGrassFill.png"


def world_to_pixel(world_x, world_y):
    return city2d.world_to_pixel(world_x, world_y)


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


def scaled_sprite(sprite_name, scale):
    source_path = SPRITE_ROOT / f"{sprite_name}.png"
    mask_path = SPRITE_ROOT / f"{sprite_name}_mask.png"
    image = Image.open(source_path).convert("RGBA")
    image = apply_mask(image, mask_path)
    if sprite_name.startswith("FloorTile"):
        image.putalpha(image.getchannel("A").point(lambda value: min(210, int(value * 0.82))))
    elif sprite_name.startswith("Road_"):
        image.putalpha(image.getchannel("A").point(lambda value: min(190, int(value * 0.78))))
    target_size = (
        max(1, int(image.width * scale * city2d.SPRITE_SCALE_PER_WORLD_UNIT)),
        max(1, int(image.height * scale * city2d.SPRITE_SCALE_PER_WORLD_UNIT)),
    )
    return image.resize(target_size, Image.Resampling.LANCZOS)


def paste_center(canvas, image, world_x, world_y):
    px, py = world_to_pixel(world_x, world_y)
    canvas.alpha_composite(image, (px - image.width // 2, py - image.height // 2))


def paste_center_rotated(canvas, image, world_x, world_y, angle):
    rotated = image.rotate(angle, resample=Image.Resampling.BICUBIC, expand=True)
    paste_center(canvas, rotated, world_x, world_y)


def draw_soft_ground_detail(canvas):
    detail = Image.new("RGBA", city2d.IMAGE_SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(detail)
    for box, fill in [
        ((170, 120, 560, 355), (72, 105, 39, 9)),
        ((835, 195, 1240, 410), (159, 188, 83, 7)),
        ((570, 570, 980, 820), (63, 96, 43, 7)),
        ((95, 620, 405, 900), (143, 116, 65, 6)),
    ]:
        draw.ellipse(box, fill=fill)
    detail = detail.filter(ImageFilter.GaussianBlur(24))
    canvas.alpha_composite(detail)


def create_grass_base():
    base = Image.new("RGBA", city2d.IMAGE_SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(base)
    for y in range(0, city2d.IMAGE_SIZE[1], 34):
        shade = 88 + (y // 34) % 8
        draw.line((0, y, city2d.IMAGE_SIZE[0], y - 85), fill=(47, shade, 34, 4), width=1)
    return base.filter(ImageFilter.GaussianBlur(0.7))


def create_grass_fill():
    base = Image.new("RGBA", city2d.IMAGE_SIZE, (112, 148, 82, 255))
    if not GRASS_TEXTURE_PATH.exists():
        return base
    texture = Image.open(GRASS_TEXTURE_PATH).convert("RGB").resize(city2d.IMAGE_SIZE, Image.Resampling.BICUBIC)
    gray = ImageOps.grayscale(texture).filter(ImageFilter.GaussianBlur(1.2))
    colored = ImageOps.colorize(gray, black=(84, 120, 61), white=(147, 174, 99)).convert("RGBA")
    return Image.blend(base, colored, 0.32)


def draw_path_underlay(canvas):
    underlay = Image.new("RGBA", city2d.IMAGE_SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(underlay)
    center = city2d.grid_to_world(0, 0)
    path_points = [
        [world_to_pixel(*center), world_to_pixel(*city2d.grid_to_world(1, -1))],
        [world_to_pixel(*center), world_to_pixel(*city2d.grid_to_world(1, 1))],
        [world_to_pixel(*center), world_to_pixel(*city2d.grid_to_world(-1, 1))],
        [world_to_pixel(*center), world_to_pixel(*city2d.grid_to_world(-1, -1))],
    ]
    for p0, p1 in path_points:
        draw.line([p0, p1], fill=(118, 94, 55, 52), width=18)
        draw.line([p0, p1], fill=(188, 162, 103, 44), width=10)
    underlay = underlay.filter(ImageFilter.GaussianBlur(1.4))
    canvas.alpha_composite(underlay)


def draw_contact_shadows(canvas):
    layer = Image.new("RGBA", city2d.IMAGE_SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    for x, y, width, height, angle in city2d.CONTACT_SHADOWS:
        px, py = world_to_pixel(x + 24, y - 32)
        shadow = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        shadow_draw = ImageDraw.Draw(shadow)
        shadow_draw.ellipse((8, 8, width - 8, height - 8), fill=(25, 35, 25, 34))
        shadow = shadow.filter(ImageFilter.GaussianBlur(15))
        shadow = shadow.rotate(angle, resample=Image.Resampling.BICUBIC, expand=True)
        layer.alpha_composite(shadow, (px - shadow.width // 2, py - shadow.height // 2))
    canvas.alpha_composite(layer)


def apply_soft_plate_alpha(canvas):
    width, height = canvas.size
    existing_alpha = canvas.getchannel("A")
    alpha = Image.new("L", canvas.size, 0)
    pixels = alpha.load()
    existing_pixels = existing_alpha.load()
    fade_width = 145
    for y in range(height):
        edge_y = min(y, height - 1 - y)
        for x in range(width):
            edge = min(x, width - 1 - x, edge_y)
            value = 255 if edge >= fade_width else int(255 * (edge / float(fade_width)) ** 0.65)
            pixels[x, y] = int(existing_pixels[x, y] * value / 255.0)
    alpha = alpha.filter(ImageFilter.GaussianBlur(5))
    canvas.putalpha(alpha)


def main():
    canvas = create_grass_base()
    draw_soft_ground_detail(canvas)
    draw_path_underlay(canvas)
    draw_contact_shadows(canvas)
    for sprite_name, x, y, scale, _sort_priority in city2d.GROUND_TILES:
        paste_center(canvas, scaled_sprite(sprite_name, scale), x, y)
    for sprite_name, x, y, scale, angle in city2d.GROUND_ROADS:
        paste_center_rotated(canvas, scaled_sprite(sprite_name, scale), x, y, angle)

    apply_soft_plate_alpha(canvas)
    pixels = canvas.load()
    for y in range(canvas.height):
        for x in range(canvas.width):
            r, g, b, a = pixels[x, y]
            if a <= 3:
                pixels[x, y] = (122, 152, 96, 0)
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    create_grass_fill().save(GRASS_FILL_OUTPUT_PATH)
    canvas.save(OUTPUT_PATH)
    mask = canvas.getchannel("A")
    mask.convert("RGBA").save(OUTPUT_MASK_PATH)
    print(GRASS_FILL_OUTPUT_PATH)
    print(OUTPUT_PATH)


if __name__ == "__main__":
    main()
