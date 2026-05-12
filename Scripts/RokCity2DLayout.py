IMAGE_SIZE = (1500, 980)
ORIGIN = (750, 490)
ISO_X_PER_WORLD_UNIT = 0.46
ISO_Y_PER_WORLD_UNIT = 0.26
SPRITE_SCALE_PER_WORLD_UNIT = 0.62

GRID_SCREEN_ORIGIN = (760, 545)
GRID_SCREEN_STEP_X = 170
GRID_SCREEN_STEP_Y = 100

GROUND_PLATE_CENTER = (-120.0, -210.0)
GROUND_PLATE_WORLD_SIZE = (1850.0, 1209.0)


def pixel_to_world(pixel_x, pixel_y):
    screen_x = float(pixel_x) - float(ORIGIN[0])
    screen_y = float(pixel_y) - float(ORIGIN[1])
    iso_x_minus_y = screen_x / ISO_X_PER_WORLD_UNIT
    iso_x_plus_y = -screen_y / ISO_Y_PER_WORLD_UNIT
    return (
        (iso_x_minus_y + iso_x_plus_y) * 0.5,
        (iso_x_plus_y - iso_x_minus_y) * 0.5,
    )


def grid_to_world(col, row):
    pixel_x = GRID_SCREEN_ORIGIN[0] + float(col) * GRID_SCREEN_STEP_X
    pixel_y = GRID_SCREEN_ORIGIN[1] + float(row) * GRID_SCREEN_STEP_Y
    return pixel_to_world(pixel_x, pixel_y)


def building_slot(building_type, display_name, col, row, relative_scale):
    world_x, world_y = grid_to_world(col, row)
    return (building_type, display_name, world_x, world_y, relative_scale)


def wall_slot(label, sprite_name, mask_name, col, row, relative_scale, sort_offset):
    world_x, world_y = grid_to_world(col, row)
    return (label, sprite_name, mask_name, world_x, world_y, relative_scale, sort_offset)


GROUND_TILES = []

GROUND_ROADS = []

CONTACT_SHADOWS = [
    (*grid_to_world(0, 0), 230, 105, -10),
    (*grid_to_world(1, -1), 150, 72, -10),
    (*grid_to_world(1, 1), 158, 76, -10),
    (*grid_to_world(-1, 1), 172, 80, -10),
    (*grid_to_world(-1, -1), 156, 74, -10),
]

WALL_SPRITES = [
    wall_slot("RokLayeredWall_BackGate", "CityWallUI_1_5.png", "CityWallUI_1_5_mask.png", 0.15, -1.45, 0.40, -1800),
    wall_slot("RokLayeredWall_LeftCorner", "CityWallUI_1_1.png", "CityWallUI_1_1_mask.png", -1.15, -0.95, 0.34, -1810),
]

BUILDINGS = [
    building_slot(1, "TownCenter", 0, 0, 0.80),
    building_slot(18, "Tavern", 1, -1, 0.66),
    building_slot(9, "Barracks", 1, 1, 0.68),
    building_slot(10, "Stable", -1, 1, 0.70),
    building_slot(11, "Archery", -1, -1, 0.68),
]

DECOR = [
    ("DecorationFountain1_1_5.png", "DecorationFountain1_1_5_mask.png", *grid_to_world(0.1, 0.85), 0.26),
    ("DecorationLion_1_2.png", "DecorationLion_1_2_mask.png", *grid_to_world(-0.5, -0.15), 0.30),
    ("DecorationLion_1_2.png", "DecorationLion_1_2_mask.png", *grid_to_world(0.45, -0.15), 0.30),
    ("bush01.png", None, *grid_to_world(-1.2, -0.45), 0.28),
    ("bush02.png", None, *grid_to_world(1.25, -0.45), 0.28),
    ("bush03.png", None, *grid_to_world(-1.2, 1.45), 0.30),
    ("bush04.png", None, *grid_to_world(1.25, 1.45), 0.30),
    ("DecorationBrazier_1_1.png", "DecorationBrazier_1_1_mask.png", *grid_to_world(0.0, 1.35), 0.28),
]


def world_to_pixel(world_x, world_y):
    return (
        int(round(ORIGIN[0] + (world_x - world_y) * ISO_X_PER_WORLD_UNIT)),
        int(round(ORIGIN[1] - (world_x + world_y) * ISO_Y_PER_WORLD_UNIT)),
    )


def projected_depth(world_x, world_y):
    return world_to_pixel(world_x, world_y)[1]


def layered_sort_priority(world_x, world_y, offset=0):
    return int((6000.0 - (float(world_x) + float(world_y))) * 10.0) + int(offset)
