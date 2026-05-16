from collections import deque
import json
import os
from pathlib import Path

from PIL import Image, ImageFilter, ImageOps


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = PROJECT_ROOT / "ref/resources/Sprite"
UNITY_GRASS_TEXTURE = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/land/Texture/ground_mask_green_02.png"
UNITY_CITY_GROUND_TEXTURE = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Map/Textures/build_ground_6_101_1.psd"
UNITY_GRASS_SPLAT_TEXTURE = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Map/Textures/Terrian_Maps/mask_map/build_GrassMap_mask_1.png"
UNITY_GRASS_TEX0 = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Map/Textures/Terrian_Maps/build_SceneMap_2_lv.tif"
UNITY_GRASS_TEX1 = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Map/Textures/Terrian_Maps/shared_terrianmap/build_SceneMap_4.tga"
UNITY_GRASS_TEX2 = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Map/Textures/Terrian_Maps/xijiecao.tga"
UNITY_GRASS_TEX3 = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Map/Textures/Terrian_Maps/nitudi.tga"
UNITY_CITY_SPLAT_MASK = PROJECT_ROOT / "ref/client-unity/Assets/T4MOBJ/Terrains/Texture/neichengmask.png"
UNITY_CITY_SPLAT_TEX0 = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Map/Textures/Terrian_Maps/build_SceneMap_1.png"
UNITY_CITY_SPLAT_TEX1 = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Map/Textures/Terrian_Maps/shared_terrianmap/build_SceneMap_4.tga"
UNITY_CITY_SPLAT_TEX2 = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Map/Textures/Terrian_Maps/shared_terrianmap/build_SceneMap_3.tga"
UNITY_CITY_BACKGROUND_TEXTURE = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Map/Env/Ground/Terrain_grass_08_3g2.png"
UNITY_CITY_SURFACE_TEXTURE = PROJECT_ROOT / "ref/client-unity/Assets/T4MOBJ/Terrains/Texture/map_incity4.png"
UNITY_CITY_ROAD_TEXTURE = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/land/Texture/shared_landtexture/dirtRoad01.png"
OUTPUT_DIR = PROJECT_ROOT / "Saved/RokUnityPlacement/clean"
UNITY_CITY_GROUND_OUTPUT = OUTPUT_DIR / "UnityCityGround_build_ground_6_101_1.png"
UNITY_GRASS_GROUND_OUTPUT = OUTPUT_DIR / "UnityGrassGround_I_TYPE_Grass_01.png"
UNITY_CITY_SPLAT_GROUND_OUTPUT = OUTPUT_DIR / "UnityCitySplatGround_city_splat_ground_alpha.png"
UNITY_CITY_BACKGROUND_OUTPUT = OUTPUT_DIR / "UnityCityBackground_Terrain_grass_08_3g2.png"
UNITY_CITY_SURFACE_OUTPUT = OUTPUT_DIR / "UnityCitySurface_map_incity4.png"
UNITY_CITY_ROAD_OUTPUT = OUTPUT_DIR / "UnityCityRoad_dirtRoad01.png"
DEFAULT_SPRITE_BASE = "Castle_6_5"


def connected_components(active, width, height):
    seen = [[False] * width for _ in range(height)]
    components = []
    for y in range(height):
        for x in range(width):
            if not active[y][x] or seen[y][x]:
                continue
            queue = deque([(x, y)])
            seen[y][x] = True
            points = []
            while queue:
                cx, cy = queue.popleft()
                points.append((cx, cy))
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if 0 <= nx < width and 0 <= ny < height and active[ny][nx] and not seen[ny][nx]:
                        seen[ny][nx] = True
                        queue.append((nx, ny))
            xs = [point[0] for point in points]
            ys = [point[1] for point in points]
            components.append(
                {
                    "area": len(points),
                    "bbox": [min(xs), min(ys), max(xs), max(ys)],
                    "points": points,
                }
            )
    return sorted(components, key=lambda item: item["area"], reverse=True)


def bbox_distance(a, b):
    ax0, ay0, ax1, ay1 = a
    bx0, by0, bx1, by1 = b
    dx = max(bx0 - ax1 - 1, ax0 - bx1 - 1, 0)
    dy = max(by0 - ay1 - 1, ay0 - by1 - 1, 0)
    return max(dx, dy)


def points_bbox(points):
    xs = [point[0] for point in points]
    ys = [point[1] for point in points]
    return [min(xs), min(ys), max(xs), max(ys)]


def fill_mask_holes(points, bbox):
    x0, y0, x1, y1 = bbox
    component_points = set(points)
    outside = set()
    queue = deque()

    def enqueue_if_outside(x, y):
        if (x, y) in component_points or (x, y) in outside:
            return
        outside.add((x, y))
        queue.append((x, y))

    for x in range(x0, x1 + 1):
        enqueue_if_outside(x, y0)
        enqueue_if_outside(x, y1)
    for y in range(y0, y1 + 1):
        enqueue_if_outside(x0, y)
        enqueue_if_outside(x1, y)

    while queue:
        cx, cy = queue.popleft()
        for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
            if x0 <= nx <= x1 and y0 <= ny <= y1:
                enqueue_if_outside(nx, ny)

    filled_points = set(component_points)
    hole_count = 0
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if (x, y) not in component_points and (x, y) not in outside:
                filled_points.add((x, y))
                hole_count += 1

    return sorted(filled_points), hole_count


def is_ground_pixel(pixel):
    r, g, b, _a = pixel
    if g >= 82 and g >= r * 1.03 and g >= b * 1.04:
        return True
    if r >= 126 and g >= 98 and b >= 62 and r - b >= 26 and g - b >= 14 and r - g <= 70:
        return True
    if g >= 42 and r <= 96 and b <= 92 and g >= r * 1.02 and g >= b * 1.03:
        return True
    return False


def dilate_rgb_into_transparent(image, iterations=4):
    image = image.convert("RGBA")
    width, height = image.size
    for _ in range(iterations):
        pixels = image.load()
        updates = {}
        for y in range(height):
            for x in range(width):
                if pixels[x, y][3] > 0:
                    continue
                samples = []
                for ny in range(max(0, y - 1), min(height, y + 2)):
                    for nx in range(max(0, x - 1), min(width, x + 2)):
                        if nx == x and ny == y:
                            continue
                        sample = pixels[nx, ny]
                        if sample[3] > 0:
                            samples.append(sample)
                if samples:
                    updates[(x, y)] = (
                        int(round(sum(pixel[0] for pixel in samples) / float(len(samples)))),
                        int(round(sum(pixel[1] for pixel in samples) / float(len(samples)))),
                        int(round(sum(pixel[2] for pixel in samples) / float(len(samples)))),
                        0,
                    )
        if not updates:
            break
        pixels = image.load()
        for (x, y), value in updates.items():
            pixels[x, y] = value
    return image


def make_terrain_plate(size):
    width, height = size
    base = Image.new("RGBA", size, (112, 145, 65, 255))
    if UNITY_GRASS_TEXTURE.exists():
        texture = Image.open(UNITY_GRASS_TEXTURE).convert("RGB").resize(size, Image.Resampling.BICUBIC)
        texture = ImageOps.grayscale(texture).filter(ImageFilter.GaussianBlur(0.8))
        colored = ImageOps.colorize(texture, black=(76, 108, 49), white=(147, 170, 82)).convert("RGBA")
        base = Image.blend(base, colored, 0.38)

    alpha = Image.new("L", size, 0)
    alpha_pixels = alpha.load()
    texture_pixels = base.load()
    cx = (width - 1) * 0.5
    cy = (height - 1) * 0.5
    rx = max(1.0, width * 0.50)
    ry = max(1.0, height * 0.48)
    for y in range(height):
        for x in range(width):
            nx = abs((x - cx) / rx)
            ny = abs((y - cy) / ry)
            edge_noise = ((x * 17 + y * 29 + (x * y) % 41) % 23) / 230.0
            metric = (nx ** 1.55) + (ny ** 1.35) + edge_noise
            if metric <= 1.0:
                alpha_pixels[x, y] = 255
            elif metric <= 1.12:
                alpha_pixels[x, y] = max(0, int(255 * (1.12 - metric) / 0.12))
            else:
                texture_pixels[x, y] = (0, 0, 0, 0)
    alpha = alpha.filter(ImageFilter.GaussianBlur(0.8))
    base.putalpha(alpha)
    return base


def with_soft_rect_alpha(image, edge_fraction=0.16, max_alpha=210):
    image = image.convert("RGBA")
    width, height = image.size
    alpha = Image.new("L", image.size, 0)
    alpha_pixels = alpha.load()
    edge_x = max(1.0, width * edge_fraction)
    edge_y = max(1.0, height * edge_fraction)
    for y in range(height):
        fy = min(float(y) / edge_y, float(height - 1 - y) / edge_y, 1.0)
        for x in range(width):
            fx = min(float(x) / edge_x, float(width - 1 - x) / edge_x, 1.0)
            alpha_pixels[x, y] = max(0, min(max_alpha, int(round(max_alpha * min(fx, fy)))))
    alpha = alpha.filter(ImageFilter.GaussianBlur(max(1.0, min(width, height) * 0.015)))
    image.putalpha(alpha)
    return image


def road_with_white_alpha(image):
    image = image.convert("RGBA")
    pixels = image.load()
    width, height = image.size
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            white_distance = max(255 - r, 255 - g, 255 - b)
            alpha = max(0, min(255, int(round(white_distance * 4.2))))
            pixels[x, y] = (r, g, b, min(a, alpha))
    return image.filter(ImageFilter.GaussianBlur(0.15))


def requested_sprite_bases():
    bases = os.environ.get("ROK_SINGLE_BUILDING_SPRITE_BASES") or os.environ.get("ROK_SINGLE_BUILDING_SPRITE_BASE")
    if not bases:
        return [DEFAULT_SPRITE_BASE]
    return [item.strip() for item in bases.replace(";", ",").split(",") if item.strip()]


def classify_owned_components(components, width, height):
    main_component = components[0]
    main_bbox = main_component["bbox"]
    margin = int(os.environ.get("ROK_SINGLE_BUILDING_COMPONENT_MARGIN", "0") or "0")
    if margin <= 0:
        margin = max(12, int(round(max(width, height) * 0.08)))

    kept = []
    dropped = []
    for index, component in enumerate(components):
        distance = bbox_distance(component["bbox"], main_bbox)
        item = {
            "index": index,
            "area": component["area"],
            "bbox": component["bbox"],
            "distance_to_main_bbox": distance,
        }
        if index == 0 or distance <= margin:
            kept.append((component, item))
        else:
            dropped.append(item)
    return kept, dropped, margin


def prepare_sprite(sprite_base):
    source_color = SOURCE_DIR / "{0}.png".format(sprite_base)
    source_mask = SOURCE_DIR / "{0}_mask.png".format(sprite_base)
    if not source_color.exists():
        raise RuntimeError("Missing source sprite: {0}".format(source_color))
    if not source_mask.exists():
        raise RuntimeError("Missing source sprite mask: {0}".format(source_mask))

    output_color = OUTPUT_DIR / "{0}_main_clean.png".format(sprite_base)
    output_cropped = OUTPUT_DIR / "{0}_main_clean_cropped.png".format(sprite_base)
    output_body = OUTPUT_DIR / "{0}_body_clean.png".format(sprite_base)
    output_body_cropped = OUTPUT_DIR / "{0}_body_clean_cropped.png".format(sprite_base)
    output_ground = OUTPUT_DIR / "{0}_ground_patch.png".format(sprite_base)
    output_terrain = OUTPUT_DIR / "{0}_terrain_plate.png".format(sprite_base)
    output_meta = OUTPUT_DIR / "{0}_main_clean_meta.json".format(sprite_base)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    color = Image.open(source_color).convert("RGBA")
    mask = Image.open(source_mask).convert("RGBA")
    width, height = color.size
    mask_pixels = mask.load()
    active = [[False] * width for _ in range(height)]
    for y in range(height):
        for x in range(width):
            active[y][x] = mask_pixels[x, y][0] >= 128

    components = connected_components(active, width, height)
    if not components:
        raise RuntimeError("No alpha components found in {0}".format(source_mask))

    kept_components, dropped_components, margin = classify_owned_components(components, width, height)
    owned_points = []
    for component, _ in kept_components:
        owned_points.extend(component["points"])
    owned_bbox = points_bbox(owned_points)
    filled_points, filled_hole_area = fill_mask_holes(owned_points, owned_bbox)
    x0, y0, x1, y1 = points_bbox(filled_points)
    pad = 2
    crop = [
        max(0, x0 - pad),
        max(0, y0 - pad),
        min(width - 1, x1 + pad),
        min(height - 1, y1 + pad),
    ]

    clean = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    body = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    ground = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    color_pixels = color.load()
    clean_pixels = clean.load()
    body_pixels = body.load()
    ground_pixels = ground.load()
    ground_area = 0
    for x, y in filled_points:
        r, g, b, _ = color_pixels[x, y]
        clean_pixels[x, y] = (r, g, b, 255)
        if is_ground_pixel((r, g, b, 255)):
            ground_pixels[x, y] = (r, g, b, 255)
            ground_area += 1
        else:
            body_pixels[x, y] = (r, g, b, 255)

    padding_iterations = 4
    clean = dilate_rgb_into_transparent(clean, padding_iterations)
    body = dilate_rgb_into_transparent(body, padding_iterations)
    ground = dilate_rgb_into_transparent(ground, padding_iterations)
    cropped = clean.crop((crop[0], crop[1], crop[2] + 1, crop[3] + 1))
    body_cropped = body.crop((crop[0], crop[1], crop[2] + 1, crop[3] + 1))
    ground_cropped = ground.crop((crop[0], crop[1], crop[2] + 1, crop[3] + 1))
    terrain_size = (
        max(32, int(round(cropped.size[0] * 1.45))),
        max(32, int(round(cropped.size[1] * 1.28))),
    )
    terrain_plate = make_terrain_plate(terrain_size)
    clean.save(output_color)
    cropped.save(output_cropped)
    body.save(output_body)
    body_cropped.save(output_body_cropped)
    ground_cropped.save(output_ground)
    terrain_plate.save(output_terrain)

    meta = {
        "sprite_base": sprite_base,
        "source_color": str(source_color.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "source_mask": str(source_mask.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "clean_color": str(output_color.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "clean_cropped": str(output_cropped.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "body_clean": str(output_body.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "body_clean_cropped": str(output_body_cropped.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "ground_patch": str(output_ground.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "terrain_plate": str(output_terrain.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "source_size": [width, height],
        "component_count": len(components),
        "component_keep_rule": "keep largest mask component plus components within margin pixels of the largest component bbox",
        "component_margin": margin,
        "kept_component_count": len(kept_components),
        "kept_components": [item for _, item in kept_components[:16]],
        "dropped_component_count": len(dropped_components),
        "dropped_components": dropped_components[:16],
        "main_area": components[0]["area"],
        "filled_area": len(filled_points),
        "filled_hole_area": filled_hole_area,
        "ground_area": ground_area,
        "ground_rule": "terrain-colored pixels inside the owned visual island; split into a lower layer when ROK_SINGLE_BUILDING_SPLIT_GROUND=1",
        "rgb_padding_iterations": padding_iterations,
        "rgb_padding_rule": "copy neighboring opaque RGB into transparent pixels while keeping alpha zero to reduce mip/filter edge bleed",
        "terrain_plate_size": [terrain_plate.size[0], terrain_plate.size[1]],
        "terrain_plate_rule": "diagnostic Unity grass texture plate under the single-building sprite",
        "main_bbox": components[0]["bbox"],
        "owned_bbox": owned_bbox,
        "crop_box": crop,
        "crop_size": [cropped.size[0], cropped.size[1]],
        "removed_component_count": len(dropped_components),
        "removed_area": sum(component["area"] for component in components) - sum(item["area"] for _, item in kept_components),
    }
    output_meta.write_text(json.dumps(meta, indent=2), encoding="utf-8")
    return meta


def prepare_city_ground_texture():
    if not UNITY_CITY_GROUND_TEXTURE.exists():
        return None
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    Image.open(UNITY_CITY_GROUND_TEXTURE).convert("RGBA").save(UNITY_CITY_GROUND_OUTPUT)
    return {
        "source": str(UNITY_CITY_GROUND_TEXTURE.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "output": str(UNITY_CITY_GROUND_OUTPUT.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "rule": "Unity Map_TTS CityWall_Ground_1_1_0 material texture cache for UE import",
    }


def sample_tiled(texture, u, v):
    width, height = texture.size
    x = (u % 1.0) * float(width - 1)
    y = (v % 1.0) * float(height - 1)
    x0 = int(x)
    y0 = int(y)
    x1 = (x0 + 1) % width
    y1 = (y0 + 1) % height
    dx = x - float(x0)
    dy = y - float(y0)
    p00 = texture.getpixel((x0, y0))[:3]
    p10 = texture.getpixel((x1, y0))[:3]
    p01 = texture.getpixel((x0, y1))[:3]
    p11 = texture.getpixel((x1, y1))[:3]
    return tuple(
        p00[channel] * (1.0 - dx) * (1.0 - dy)
        + p10[channel] * dx * (1.0 - dy)
        + p01[channel] * (1.0 - dx) * dy
        + p11[channel] * dx * dy
        for channel in range(3)
    )


def prepare_grass_ground_texture(size=1024):
    required = [UNITY_GRASS_SPLAT_TEXTURE, UNITY_GRASS_TEX0, UNITY_GRASS_TEX1, UNITY_GRASS_TEX2, UNITY_GRASS_TEX3]
    if not all(path.exists() for path in required):
        return None

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    splat = Image.open(UNITY_GRASS_SPLAT_TEXTURE).convert("RGB").resize((size, size), Image.Resampling.BILINEAR)
    textures = [
        Image.open(UNITY_GRASS_TEX0).convert("RGB"),
        Image.open(UNITY_GRASS_TEX1).convert("RGB"),
        Image.open(UNITY_GRASS_TEX2).convert("RGB"),
        Image.open(UNITY_GRASS_TEX3).convert("RGB"),
    ]
    scales = [0.06666, 0.15, 0.15, 0.3]
    output = Image.new("RGBA", (size, size), (0, 0, 0, 255))
    out_pixels = output.load()
    splat_pixels = splat.load()
    color = (0.8066038, 0.8946353, 1.0)

    for y in range(size):
        v = 1.0 - (float(y) + 0.5) / float(size)
        for x in range(size):
            u = 1.0 - (float(x) + 0.5) / float(size)
            sr, sg, sb = splat_pixels[x, y]
            weights = [sr / 255.0, sg / 255.0, sb / 255.0]
            weights.append(max(0.0, 1.0 - weights[0] - weights[1] - weights[2]))
            total = max(0.0001, sum(weights))
            weights = [weight / total for weight in weights]
            rgb = [0.0, 0.0, 0.0]
            for index, texture in enumerate(textures):
                sample = sample_tiled(texture, u * scales[index] * 60.0, v * scales[index] * 60.0)
                for channel in range(3):
                    rgb[channel] += weights[index] * sample[channel]
            out_pixels[x, y] = (
                max(0, min(255, int(round(rgb[0] * color[0])))),
                max(0, min(255, int(round(rgb[1] * color[1])))),
                max(0, min(255, int(round(rgb[2] * color[2])))),
                255,
            )

    output.save(UNITY_GRASS_GROUND_OUTPUT)
    return {
        "source_material": "ref/client-unity/Assets/BundleAssets/Map/Textures/Materials/mask_map/I_TYPE_Grass_01.mat",
        "splat": str(UNITY_GRASS_SPLAT_TEXTURE.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "textures": [str(path.relative_to(PROJECT_ROOT)).replace("\\", "/") for path in required[1:]],
        "output": str(UNITY_GRASS_GROUND_OUTPUT.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "rule": "Baked from Unity I_TYPE_Grass_01 using cm_mask_texture_lod splat blend",
    }


def prepare_city_splat_ground_texture(size=1024):
    required = [UNITY_CITY_SPLAT_MASK, UNITY_CITY_SPLAT_TEX0, UNITY_CITY_SPLAT_TEX1, UNITY_CITY_SPLAT_TEX2]
    if not all(path.exists() for path in required):
        return None

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    splat = Image.open(UNITY_CITY_SPLAT_MASK).convert("RGB").resize((size, size), Image.Resampling.BILINEAR)
    textures = [
        Image.open(UNITY_CITY_SPLAT_TEX0).convert("RGB"),
        Image.open(UNITY_CITY_SPLAT_TEX1).convert("RGB"),
        Image.open(UNITY_CITY_SPLAT_TEX2).convert("RGB"),
    ]
    scales = [0.083, 0.08, 0.05]
    output = Image.new("RGBA", (size, size), (0, 0, 0, 255))
    out_pixels = output.load()
    splat_pixels = splat.load()

    for y in range(size):
        v = (float(y) + 0.5) / float(size)
        for x in range(size):
            u = (float(x) + 0.5) / float(size)
            sr, sg, sb = splat_pixels[x, y]
            weights = [sr / 255.0, sg / 255.0, sb / 255.0]
            rgb = [0.0, 0.0, 0.0]
            for index, texture in enumerate(textures):
                sample = sample_tiled(texture, u * scales[index], v * scales[index])
                for channel in range(3):
                    rgb[channel] += weights[index] * sample[channel]
            out_pixels[x, y] = (
                max(0, min(255, int(round(rgb[0])))),
                max(0, min(255, int(round(rgb[1])))),
                max(0, min(255, int(round(rgb[2])))),
                255,
            )

    output.save(UNITY_CITY_SPLAT_GROUND_OUTPUT)
    return {
        "source_material": "ref/client-unity/Assets/BundleAssets/Map/Building/city_splat_ground_alpha.mat",
        "splat": str(UNITY_CITY_SPLAT_MASK.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "textures": [str(path.relative_to(PROJECT_ROOT)).replace("\\", "/") for path in required[1:]],
        "output": str(UNITY_CITY_SPLAT_GROUND_OUTPUT.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "rule": "Baked from Unity city_splat_ground_alpha using custom/city/city_splat_ground splat blend without normalizing splat weights",
    }


def prepare_city_background_texture():
    if not UNITY_CITY_BACKGROUND_TEXTURE.exists():
        return None
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    Image.open(UNITY_CITY_BACKGROUND_TEXTURE).convert("RGBA").save(UNITY_CITY_BACKGROUND_OUTPUT)
    return {
        "source_material": "ref/client-unity/Assets/BundleAssets/Map/Building/city_splat_ground_alpha.mat",
        "source": str(UNITY_CITY_BACKGROUND_TEXTURE.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "output": str(UNITY_CITY_BACKGROUND_OUTPUT.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "rule": "Unity city_splat_ground_alpha _MainTex / CitySurface base ground texture for the soft in-city grass background",
    }


def prepare_city_surface_texture():
    if not UNITY_CITY_SURFACE_TEXTURE.exists():
        return None
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    with_soft_rect_alpha(Image.open(UNITY_CITY_SURFACE_TEXTURE), 0.18, 172).save(UNITY_CITY_SURFACE_OUTPUT)
    return {
        "source_material": "ref/client-unity/Assets/BundleAssets/Map/Building/shared_mapbuild/CitySurface_101_1_mat.mat",
        "source": str(UNITY_CITY_SURFACE_TEXTURE.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "output": str(UNITY_CITY_SURFACE_OUTPUT.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "rule": "Unity CitySurface_101_1 green in-city surface texture for building-yard diagnostics",
    }


def prepare_city_road_texture():
    if not UNITY_CITY_ROAD_TEXTURE.exists():
        return None
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    road_with_white_alpha(Image.open(UNITY_CITY_ROAD_TEXTURE)).save(UNITY_CITY_ROAD_OUTPUT)
    return {
        "source_material": "ref/client-unity/Assets/BundleAssets/land/Material/shared_material/dryDoad01.mat",
        "source": str(UNITY_CITY_ROAD_TEXTURE.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "output": str(UNITY_CITY_ROAD_OUTPUT.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "rule": "Unity dryDoad01 road texture used by city-building prefab road mesh diagnostics",
    }


def main():
    results = []
    for sprite_base in requested_sprite_bases():
        results.append(prepare_sprite(sprite_base))
    city_ground = prepare_city_ground_texture()
    grass_ground = prepare_grass_ground_texture()
    city_splat_ground = prepare_city_splat_ground_texture()
    city_background_ground = prepare_city_background_texture()
    city_surface_ground = prepare_city_surface_texture()
    city_road_ground = prepare_city_road_texture()
    payload = results[0] if len(results) == 1 else results
    if city_ground or grass_ground or city_splat_ground or city_background_ground or city_surface_ground or city_road_ground:
        grounds = {}
        if city_ground:
            grounds["city_sand"] = city_ground
        if grass_ground:
            grounds["grass"] = grass_ground
        if city_splat_ground:
            grounds["city_splat"] = city_splat_ground
        if city_background_ground:
            grounds["city_background"] = city_background_ground
        if city_surface_ground:
            grounds["city_surface"] = city_surface_ground
        if city_road_ground:
            grounds["city_road"] = city_road_ground
        if isinstance(payload, list):
            payload = {"sprites": payload, "grounds": grounds}
        else:
            payload["grounds"] = grounds
    print(json.dumps(payload, indent=2))


if __name__ == "__main__":
    main()
