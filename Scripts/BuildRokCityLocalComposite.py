from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


PROJECT_ROOT = Path(r"D:\ue\Rok")
SPRITE_ROOT = PROJECT_ROOT / "ref" / "resources" / "Sprite"
CUTOUT_ROOT = PROJECT_ROOT / "Saved" / "RokDerivedSprites" / "StructureCutouts"
OUTPUT_PATH = PROJECT_ROOT / "Saved" / "RokCityLocalCompositeCiv6.png"

CANVAS_SIZE = (1600, 950)
ORIGIN = (800, 425)
VISUAL_SCALE = 0.74


BUILDINGS = [
    ("TownCenter_6_5", 70, 110, 0.82),
    ("Castle_6_5", -220, 400, 0.56),
    ("AllianceCenter_6_5", 390, 360, 0.54),
    ("Campus_6_5", -390, 120, 0.56),
    ("Tavern_6_5", 430, 95, 0.54),
    ("Barracks_6_5", -490, -135, 0.58),
    ("Stable_6_5", 520, -125, 0.58),
    ("Archery_6_5", -285, -390, 0.55),
    ("SiegeWorkshop_6_5", 275, -390, 0.55),
    ("Hospital_6_5", 0, -560, 0.52),
    ("Barracks_6_5", -600, -390, 0.48),
    ("Stable_6_5", 640, -365, 0.48),
    ("Archery_6_5", -570, 335, 0.47),
    ("SiegeWorkshop_6_5", 620, 310, 0.47),
    ("Campus_6_5", -425, -640, 0.44),
    ("Tavern_6_5", 420, -640, 0.44),
    ("Hospital_6_5", 60, 610, 0.43),
]

TILES = [
    ("FloorTile_1_5_9", 0, 10, 1.12),
    ("FloorTile_1_5_9", -350, 0, 0.90),
    ("FloorTile_1_5_9", 360, 0, 0.90),
    ("FloorTile_1_5_9", -330, -340, 0.90),
    ("FloorTile_1_5_9", 340, -340, 0.90),
    ("FloorTile_1_5_9", 0, -570, 0.76),
    ("Road_1_1_5", -220, 205, 2.9),
    ("Road_1_1_5", 220, 205, 2.9),
    ("Road_1_1_5", -260, -120, 2.9),
    ("Road_1_1_5", 260, -120, 2.9),
    ("Road_1_1_5", -180, -465, 2.6),
    ("Road_1_1_5", 185, -465, 2.6),
]


def load_scaled(name, scale):
    source_path = CUTOUT_ROOT / f"{name}.png"
    if not source_path.exists():
        source_path = SPRITE_ROOT / f"{name}.png"
    image = Image.open(source_path).convert("RGBA")
    width = max(1, int(image.width * VISUAL_SCALE * scale))
    height = max(1, int(image.height * VISUAL_SCALE * scale))
    return image.resize((width, height), Image.Resampling.LANCZOS)


def paste_center_bottom(canvas, image, world_x, world_y):
    screen_x = int(ORIGIN[0] + world_x * VISUAL_SCALE)
    screen_y = int(ORIGIN[1] - world_y * VISUAL_SCALE)
    x = screen_x - image.width // 2
    y = screen_y - image.height
    canvas.alpha_composite(image, (x, y))


def paste_center(canvas, image, world_x, world_y):
    screen_x = int(ORIGIN[0] + world_x * VISUAL_SCALE)
    screen_y = int(ORIGIN[1] - world_y * VISUAL_SCALE)
    x = screen_x - image.width // 2
    y = screen_y - image.height // 2
    canvas.alpha_composite(image, (x, y))


def main():
    canvas = Image.new("RGBA", CANVAS_SIZE, (128, 161, 62, 255))
    grass = Image.new("RGBA", CANVAS_SIZE, (128, 161, 62, 255))
    grass = grass.filter(ImageFilter.GaussianBlur(0.2))
    canvas.alpha_composite(grass)

    for name, x, y, scale in TILES:
        paste_center(canvas, load_scaled(name, scale), x, y)

    for name, x, y, scale in sorted(BUILDINGS, key=lambda item: item[2], reverse=True):
        paste_center_bottom(canvas, load_scaled(name, scale), x, y)

    # Crop off the desktop-like emptiness while keeping the target aspect.
    draw = ImageDraw.Draw(canvas)
    draw.rectangle((0, 0, CANVAS_SIZE[0] - 1, CANVAS_SIZE[1] - 1), outline=(128, 161, 62, 255))
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(OUTPUT_PATH, quality=95)
    print(OUTPUT_PATH)


if __name__ == "__main__":
    main()
