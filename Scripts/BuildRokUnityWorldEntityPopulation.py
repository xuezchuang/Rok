import hashlib
import json
import math
import re
from pathlib import Path

from PIL import Image, ImageChops
from PrepareRokUnityWorldMapAssets import parse_unity_curve
from BuildRokUnityWorldEntityPresentation import decode_unit_sprite, parse_packed_sprite_geometry
from rok_data_paths import DATA_ROOT, label, resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
CONFIG_DIR = DATA_ROOT / "ConfigTables"
OUTPUT_DIR = DATA_ROOT / "WorldEntityPopulation"
MANIFEST_PATH = DATA_ROOT / "unity_world_entity_population.json"
REPORT_PATH = MIGRATION_DIR / "unity_world_entity_population.md"

FOCUS_UNITY = (6876, 942)
REGION_HALF_SIZE = (450, 320)
DISPLAY_SCALE = 6.0
UE_UNITS_PER_UNITY_UNIT = 500.0
PLAYER_CITY_EXCLUSION_RADIUS = 20.0

MAP_ICON_TEXTURE = (
    PROJECT_ROOT
    / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/"
    "ui_res_map/map_icon.png"
)
MAP_ICON_META = Path(str(MAP_ICON_TEXTURE) + ".meta")
MAIN_MENU_TEXTURE = (
    PROJECT_ROOT
    / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/"
    "ui_res_common/ui_mainmenu.png"
)
MAIN_MENU_META = Path(str(MAIN_MENU_TEXTURE) + ".meta")
CITY_LOD3_TEXTURE = (
    PROJECT_ROOT
    / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/"
    "ui_res_map/ui_map.png"
)
CITY_LOD3_META = Path(str(CITY_LOD3_TEXTURE) + ".meta")
CITY_LOD3_PREFAB = (
    PROJECT_ROOT
    / "ref/client-unity/Assets/BundleAssets/Scene/Map_building/"
    "map_building_lod/city_lod3.prefab"
)
WILD_BUILDING_DIR = (
    PROJECT_ROOT
    / "ref/client-unity/Assets/BundleAssets/Map/Building/Buildingwild_use"
)
MONSTER_IDLE_TEXTURE = (
    PROJECT_ROOT
    / "ref/client-unity/Assets/BundleAssets/Monster/Monster_1000/"
    "Monster_1000_idle/monster_1000_idle.png"
)
MONSTER_IDLE_META = Path(str(MONSTER_IDLE_TEXTURE) + ".meta")
WORLD_BUILDING_SHADER = (
    PROJECT_ROOT
    / "ref/client-unity/Assets/BundleAssets/Map/Building/"
    "map_building_with_mask-5.shader"
)

SOURCE_LOD_PREFABS = {
    "food": (
        "ref/client-unity/Assets/BundleAssets/Scene/Map_building/"
        "map_building_lod/food_lod3.prefab"
    ),
    "wood": (
        "ref/client-unity/Assets/BundleAssets/Scene/Map_building/"
        "map_building_lod/wood_lod3.prefab"
    ),
    "stone": (
        "ref/client-unity/Assets/BundleAssets/Scene/Map_building/"
        "map_building_lod/stone_lod3.prefab"
    ),
    "barbarian": (
        "ref/client-unity/Assets/BundleAssets/Scene/Map_building/"
        "map_building_lod/barbarian_lod3.prefab"
    ),
    "village": (
        "ref/client-unity/Assets/BundleAssets/Scene/Map_building/"
        "map_building_lod/village_lod3.prefab"
    ),
}

VISUAL_SOURCES = {
    "food": {
        "detail": WILD_BUILDING_DIR / "farm_3.png",
        "detail_mask": WILD_BUILDING_DIR / "farm_3_mask.png",
        "far_atlas": MAP_ICON_TEXTURE,
        "far_meta": MAP_ICON_META,
        "far_sprite": "map_icon_food1",
        "detail_world_height_cm": 1000.0,
    },
    "wood": {
        "detail": WILD_BUILDING_DIR / "wood_3.png",
        "detail_mask": WILD_BUILDING_DIR / "wood_3_mask.png",
        "far_atlas": MAP_ICON_TEXTURE,
        "far_meta": MAP_ICON_META,
        "far_sprite": "map_icon_wood1",
        "detail_world_height_cm": 1100.0,
    },
    "stone": {
        "detail": WILD_BUILDING_DIR / "stone_3.png",
        "detail_mask": WILD_BUILDING_DIR / "stone_3_mask.png",
        "far_atlas": MAP_ICON_TEXTURE,
        "far_meta": MAP_ICON_META,
        "far_sprite": "map_icon_stone1",
        "detail_world_height_cm": 950.0,
    },
    "barbarian": {
        "detail": MONSTER_IDLE_TEXTURE,
        "detail_meta": MONSTER_IDLE_META,
        "detail_sprite": "monster_1000_idle_1_00",
        "far_atlas": MAP_ICON_TEXTURE,
        "far_meta": MAP_ICON_META,
        "far_sprite": "map_icon_1001",
        "detail_world_height_cm": 1150.0,
    },
    "village": {
        "detail": WILD_BUILDING_DIR / "village_1.png",
        "detail_mask": WILD_BUILDING_DIR / "village_1_mask.png",
        "far_atlas": MAIN_MENU_TEXTURE,
        "far_meta": MAIN_MENU_META,
        "far_sprite": "icon_mmu_1004_1",
        "detail_world_height_cm": 1150.0,
    },
}


def relative(path):
    return label(path)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def source_record(path):
    if not path.exists():
        raise RuntimeError(f"Missing Unity source: {relative(path)}")
    return {
        "path": relative(path),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def load_table(name):
    path = CONFIG_DIR / f"{name}.json"
    payload = json.loads(path.read_text(encoding="utf-8"))
    if payload.get("table_name") != name:
        raise RuntimeError(f"Unexpected table payload: {path}")
    return path, payload["rows"]


def parse_sprite_rect(meta_path, sprite_name):
    text = meta_path.read_text(encoding="utf-8")
    blocks = re.split(
        r"(?m)^\s+- serializedVersion:\s*\d+\s*$",
        text[text.find("  spriteSheet:") :],
    )[1:]
    for block in blocks:
        name_match = re.search(r"(?m)^\s*name:\s*(.*?)\s*$", block)
        if not name_match or name_match.group(1).strip() != sprite_name:
            continue
        rect_match = re.search(
            r"(?ms)^\s*rect:\s*\n"
            r"\s*serializedVersion:\s*\d+\s*\n"
            r"\s*x:\s*([-+0-9.eE]+)\s*\n"
            r"\s*y:\s*([-+0-9.eE]+)\s*\n"
            r"\s*width:\s*([-+0-9.eE]+)\s*\n"
            r"\s*height:\s*([-+0-9.eE]+)\s*$",
            block,
        )
        file_id_match = re.search(
            r"(?m)^\s*internalID:\s*(-?\d+)\s*$",
            block,
        )
        if not rect_match or not file_id_match:
            raise RuntimeError(
                f"Incomplete sprite metadata for {sprite_name}"
            )
        x, y, width, height = (
            int(round(float(value))) for value in rect_match.groups()
        )
        return {
            "name": sprite_name,
            "file_id": int(file_id_match.group(1)),
            "rect": {
                "x": x,
                "y": y,
                "width": width,
                "height": height,
            },
        }
    raise RuntimeError(
        f"Sprite {sprite_name} was not found in {relative(meta_path)}"
    )


def crop_unity_sprite(texture_path, meta_path, sprite_name):
    metadata = parse_sprite_rect(meta_path, sprite_name)
    atlas = Image.open(texture_path).convert("RGBA")
    rect = metadata["rect"]
    top = atlas.height - rect["y"] - rect["height"]
    crop = atlas.crop(
        (
            rect["x"],
            top,
            rect["x"] + rect["width"],
            top + rect["height"],
        )
    )
    return crop, metadata


def tight_alpha_crop(image, padding=4):
    image = image.convert("RGBA")
    bounds = image.getchannel("A").getbbox()
    if not bounds:
        raise RuntimeError("Unity visual has no visible alpha pixels")
    left = max(0, bounds[0] - padding)
    top = max(0, bounds[1] - padding)
    right = min(image.width, bounds[2] + padding)
    bottom = min(image.height, bounds[3] + padding)
    return image.crop((left, top, right, bottom))


def dither_alpha_for_masked_billboard(image):
    """Preserve Unity soft alpha when UE renders the PNG as a masked sprite."""
    image = image.convert("RGBA")
    source_alpha = image.getchannel("A")
    output_alpha = Image.new("L", image.size, 0)
    source_pixels = source_alpha.load()
    output_pixels = output_alpha.load()
    bayer_4x4 = (
        (0, 8, 2, 10),
        (12, 4, 14, 6),
        (3, 11, 1, 9),
        (15, 7, 13, 5),
    )
    for y in range(image.height):
        threshold_row = bayer_4x4[y % 4]
        for x in range(image.width):
            threshold = (threshold_row[x % 4] + 0.5) * 255.0 / 16.0
            output_pixels[x, y] = (
                255 if source_pixels[x, y] >= threshold else 0
            )
    image.putalpha(output_alpha)
    return image


def remove_dark_low_alpha_pixels(image, max_alpha=160, max_rgb=24):
    """Drop soft black padding that cannot blend in UE's masked billboard."""
    image = image.convert("RGBA")
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            red, green, blue, alpha = pixels[x, y]
            if alpha <= max_alpha and max(red, green, blue) <= max_rgb:
                pixels[x, y] = (red, green, blue, 0)
    return image


def apply_unity_world_mask(color, mask_path):
    color = color.convert("RGBA")
    mask = Image.open(mask_path).convert("RGBA")
    if mask.size != color.size:
        mask = mask.resize(color.size, Image.Resampling.BILINEAR)
    mask_red = mask.getchannel("R")
    _, mask_max = mask_red.getextrema()
    if mask_max <= 0:
        raise RuntimeError(
            f"Unity world mask has no red-channel coverage: "
            f"{relative(mask_path)}"
        )
    # custom/map/map_building_with_mask-5.shader:
    # alpha = MainTex.a * clamp(Mask.r * 2, 0, 1)
    normalized = mask_red.point(lambda value: min(255, value * 2))
    source_alpha = color.getchannel("A")
    combined_alpha = ImageChops.multiply(source_alpha, normalized)
    color.putalpha(combined_alpha)
    return dither_alpha_for_masked_billboard(color)


def build_barbarian_formation(frame):
    frame = tight_alpha_crop(frame, 1)
    canvas = Image.new("RGBA", (280, 190), (0, 0, 0, 0))
    placements = [
        (98, 4, 0.82),
        (44, 31, 0.78),
        (151, 31, 0.78),
        (14, 72, 0.74),
        (74, 72, 0.78),
        (133, 72, 0.78),
        (191, 72, 0.74),
    ]
    for x, y, scale in placements:
        resized = frame.resize(
            (
                max(1, round(frame.width * scale)),
                max(1, round(frame.height * scale)),
            ),
            Image.Resampling.LANCZOS,
        )
        canvas.alpha_composite(resized, (x, y))
    return tight_alpha_crop(canvas, 3)




def prepare_visual_library():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    library = {}
    for key, source in VISUAL_SOURCES.items():
        if source.get("detail_sprite"):
            detail, detail_sprite = crop_unity_sprite(
                source["detail"],
                source["detail_meta"],
                source["detail_sprite"],
            )
            if key == "barbarian":
                detail = decode_unit_sprite(detail, parse_packed_sprite_geometry(
                    source["detail"]
                )[source["detail_sprite"]])
                detail = build_barbarian_formation(detail)
        else:
            detail = Image.open(source["detail"]).convert("RGBA")
            if source.get("detail_mask"):
                detail = apply_unity_world_mask(
                    detail,
                    source["detail_mask"],
                )
            detail = tight_alpha_crop(
                detail
            )
            detail_sprite = None
        far, far_sprite = crop_unity_sprite(
            source["far_atlas"],
            source["far_meta"],
            source["far_sprite"],
        )
        far = tight_alpha_crop(far, 2)
        far_scale_keys = [key for key in parse_unity_curve(
            resolve(SOURCE_LOD_PREFABS[key]), "m_scale_curve"
        ) if key["scale"] > 0 and 3000 <= key["dxf"] <= 5400]
        if len(far_scale_keys) != 2:
            raise RuntimeError(f"Unexpected far-icon scale curve: {key}")
        pixels_to_units = float(re.search(
            r"spritePixelsToUnits:\s*([0-9.]+)",
            source["far_meta"].read_text(encoding="utf-8"),
        ).group(1))
        detail_path = OUTPUT_DIR / f"{key}_detail.png"
        far_path = OUTPUT_DIR / f"{key}_far.png"
        detail.save(detail_path)
        far.save(far_path)
        record = {
            "key": key,
            "detail": {
                "output": source_record(detail_path),
                "source": source_record(source["detail"]),
                "source_sprite": detail_sprite,
                "source_mask": (
                    source_record(source["detail_mask"])
                    if source.get("detail_mask")
                    else None
                ),
                "source_shader": (
                    source_record(WORLD_BUILDING_SHADER)
                    if source.get("detail_mask")
                    else None
                ),
                "composition": (
                    "seven_unit_cluster_from_exact_unity_idle_frame"
                    if key == "barbarian"
                    else (
                        "exact_source_shader_alpha_with_masked_billboard_dither"
                        if source.get("detail_mask")
                        else "exact_source_rgba_alpha_crop"
                    )
                ),
                "world_height_cm": source["detail_world_height_cm"],
            },
            "far": {
                "output": source_record(far_path),
                "source_atlas": source_record(source["far_atlas"]),
                "source_meta": source_record(source["far_meta"]),
                "source_sprite": far_sprite,
                "composition": "exact_unity_lod3_atlas_sprite",
                "world_height_cm": far.height / pixels_to_units * UE_UNITS_PER_UNITY_UNIT * far_scale_keys[0]["scale"],
                "end_scale_multiplier": far_scale_keys[-1]["scale"] / far_scale_keys[0]["scale"],
                "source_scale_keys": far_scale_keys,
            },
            "source_lod_prefab": source_record(
                resolve(SOURCE_LOD_PREFABS[key])
            ),
        }
        if key == "food":
            record["detail"]["runtime_composition"] = {
                "component": "URokFoodDepositComponent",
                "source_manifest": source_record(DATA_ROOT / "unity_food_deposit.json"),
                "note": "Nine source prefab children replace the legacy composite at runtime; its dimensions remain selection bounds only.",
            }
        library[key] = record
    return library


def prepare_player_city_lod3():
    image, sprite = crop_unity_sprite(
        CITY_LOD3_TEXTURE,
        CITY_LOD3_META,
        "img_map_1014",
    )
    # GlobalViewLevelMediator.SetCityObj -> ChangeSpriteColor ->
    # map_lod3_icon+35.shader: multiply the own-city sprite by RS.green.
    # This output is specifically the local player's city, not a shared
    # neutral sprite for allied/enemy cities.
    own_city_color = (0.266666, 0.831372, 0.509803)
    channels = image.convert("RGBA").split()
    image = Image.merge("RGBA", tuple(
        channels[index].point(lambda value, tint=tint: round(value * tint))
        for index, tint in enumerate(own_city_color)
    ) + (channels[3],))
    output_path = OUTPUT_DIR / "player_city_lod3.png"
    image.save(output_path)
    return {
        "output": source_record(output_path),
        "source_atlas": source_record(CITY_LOD3_TEXTURE),
        "source_meta": source_record(CITY_LOD3_META),
        "source_sprite": sprite,
        "source_lod_prefab": source_record(CITY_LOD3_PREFAB),
        "world_height_cm": sprite["rect"]["height"] / 100.0 * UE_UNITS_PER_UNITY_UNIT * 6.0,
        "pixels_per_unit": 100.0,
        "pivot": [0.5, 0.5],
        "alpha_mode": "source_translucent_no_dither_no_crop",
        "visible_min_dxf": 3000.0,
        "visible_max_dxf": 5400.0,
        "composition": "unity_city_lod3_own_city_tint",
        "own_city_tint": list(own_city_color),
        "tint_source": "ref/client-unity/Assets/Scripts/Hotfix/MVC/RS.cs:green",
    }


def within_region(row):
    return (
        abs(int(row["posX"]) - FOCUS_UNITY[0]) <= REGION_HALF_SIZE[0]
        and abs(int(row["posY"]) - FOCUS_UNITY[1])
        <= REGION_HALF_SIZE[1]
    )


def squared_distance(left, right):
    return (
        (float(left[0]) - float(right[0])) ** 2
        + (float(left[1]) - float(right[1])) ** 2
    )


def choose_nearest(candidates, target, occupied, minimum_spacing):
    ranked = sorted(
        candidates,
        key=lambda row: (
            squared_distance(
                (int(row["posX"]), int(row["posY"])),
                target,
            ),
            int(row["ID"]),
        ),
    )
    for row in ranked:
        position = (int(row["posX"]), int(row["posY"]))
        if all(
            squared_distance(position, other)
            >= minimum_spacing * minimum_spacing
            for other in occupied
        ):
            occupied.append(position)
            candidates.remove(row)
            return row
    raise RuntimeError(f"No source point satisfies spacing at {target}")


def build_entity_record(
    row,
    kind,
    display_name,
    level,
    visual,
    source_table,
):
    detail_source = Path(
        resolve(visual["detail"]["output"]["path"])
    )
    far_source = Path(resolve(visual["far"]["output"]["path"]))
    # Level text belongs to the runtime UMG nameplate. Baking it into the
    # sprite duplicates the label and changes the icon's world-size ratio.
    detail_image = Image.open(detail_source).convert("RGBA")
    far_image = Image.open(far_source).convert("RGBA")
    source_id = int(row["ID"])
    detail_path = OUTPUT_DIR / f"{kind}_{source_id}_detail.png"
    far_path = OUTPUT_DIR / f"{kind}_{source_id}_far.png"
    detail_image.save(detail_path)
    far_image.save(far_path)
    x = int(row["posX"])
    y = int(row["posY"])
    return {
        "source_id": source_id,
        "source_table": source_table,
        "source_row": row,
        "kind": kind,
        "display_name": display_name,
        "level": level,
        "unity_position": {"x": x, "y": y},
        "display_position": {
            "x": x / DISPLAY_SCALE,
            "y": y / DISPLAY_SCALE,
        },
        "visual_key": visual["key"],
        "detail_visual": {
            "path": relative(detail_path),
            "sha256": sha256(detail_path),
            "width": detail_image.width,
            "height": detail_image.height,
            "world_height_cm": visual["detail"]["world_height_cm"],
        },
        "far_visual": {
            "path": relative(far_path),
            "sha256": sha256(far_path),
            "width": far_image.width,
            "height": far_image.height,
            "world_height_cm": visual["far"]["world_height_cm"],
            "end_scale_multiplier": visual["far"]["end_scale_multiplier"],
        },
        "selectable": True,
    }


def build_population(visual_library):
    resource_path, resource_rows = load_table("ResourceGatherPoint")
    monster_path, monster_rows = load_table("MonsterPoint")
    fixed_path, fixed_rows = load_table("MapFixPoint")

    resources = [
        row
        for row in resource_rows
        if within_region(row)
        and squared_distance(
            (int(row["posX"]), int(row["posY"])),
            FOCUS_UNITY,
        )
        >= PLAYER_CITY_EXCLUSION_RADIUS**2
    ]
    monsters = [
        row
        for row in monster_rows
        if within_region(row)
        and squared_distance(
            (int(row["posX"]), int(row["posY"])),
            FOCUS_UNITY,
        )
        >= PLAYER_CITY_EXCLUSION_RADIUS**2
    ]
    villages = [
        row
        for row in fixed_rows
        if within_region(row) and int(row["type"]) == 70001
    ]

    occupied = [FOCUS_UNITY]
    entities = []
    village_specs = [(-24, 6), (138, -54)]
    for offset in village_specs:
        target = (
            FOCUS_UNITY[0] + offset[0],
            FOCUS_UNITY[1] + offset[1],
        )
        row = choose_nearest(villages, target, occupied, 20.0)
        entities.append(
            build_entity_record(
                row,
                "village",
                "村庄",
                0,
                visual_library["village"],
                "MapFixPoint",
            )
        )

    resource_specs = [
        ((-29, -8), "food", "农田", 3),
        ((22, -8), "wood", "木材点", 3),
        ((19, 13), "stone", "石料点", 3),
        ((-5, -35), "food", "农田", 3),
        ((-4, 36), "wood", "木材点", 3),
    ]
    for offset, kind, name, level in resource_specs:
        target = (
            FOCUS_UNITY[0] + offset[0],
            FOCUS_UNITY[1] + offset[1],
        )
        row = choose_nearest(resources, target, occupied, 10.0)
        entities.append(
            build_entity_record(
                row,
                kind,
                name,
                level,
                visual_library[kind],
                "ResourceGatherPoint",
            )
        )

    monster_specs = [
        ((12, -20), 14),
        ((8, 27), 16),
        ((-14, 29), 14),
    ]
    for offset, level in monster_specs:
        target = (
            FOCUS_UNITY[0] + offset[0],
            FOCUS_UNITY[1] + offset[1],
        )
        row = choose_nearest(monsters, target, occupied, 10.0)
        entities.append(
            build_entity_record(
                row,
                "barbarian",
                "野蛮人",
                level,
                visual_library["barbarian"],
                "MonsterPoint",
            )
        )

    source_tables = {}
    for name, path in (
        ("ResourceGatherPoint", resource_path),
        ("MonsterPoint", monster_path),
        ("MapFixPoint", fixed_path),
    ):
        source_tables[name] = source_record(path)
    return entities, source_tables, {
        "resource_candidates_in_region": len(
            [row for row in resource_rows if within_region(row)]
        ),
        "monster_candidates_in_region": len(
            [row for row in monster_rows if within_region(row)]
        ),
        "fixed_candidates_in_region": len(
            [row for row in fixed_rows if within_region(row)]
        ),
    }


def write_report(payload):
    lines = [
        "# Rok Unity world entity population v1",
        "",
        (
            f"- Reference focus: Unity `{FOCUS_UNITY[0]},"
            f"{FOCUS_UNITY[1]}` / display "
            f"`{FOCUS_UNITY[0] / DISPLAY_SCALE:.0f},"
            f"{FOCUS_UNITY[1] / DISPLAY_SCALE:.0f}`"
        ),
        f"- Selected runtime entities: `{len(payload['entities'])}`",
        "- LOD bands: detail `< 3000`, source lod3 icon `3000–5400`",
        "- Selection is deterministic and keeps original Unity table IDs.",
        "",
        "| Kind | Unity ID | Position | Level | Detail source | Far source |",
        "|---|---:|---:|---:|---|---|",
    ]
    for entity in payload["entities"]:
        visual = payload["visual_library"][entity["visual_key"]]
        detail_source = visual["detail"]["source"]["path"]
        far_source = visual["far"]["source_sprite"]["name"]
        position = entity["unity_position"]
        lines.append(
            "| {kind} | {source_id} | {x},{y} | {level} | "
            "`{detail}` | `{far}` |".format(
                kind=entity["kind"],
                source_id=entity["source_id"],
                x=position["x"],
                y=position["y"],
                level=entity["level"],
                detail=detail_source,
                far=far_source,
            )
        )
    REPORT_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    visual_library = prepare_visual_library()
    player_city_lod3_visual = prepare_player_city_lod3()
    entities, source_tables, candidate_counts = build_population(
        visual_library
    )
    kind_counts = {}
    for entity in entities:
        kind_counts[entity["kind"]] = kind_counts.get(
            entity["kind"], 0
        ) + 1
    payload = {
        "schema": "rok.unity_world_entity_population.v1",
        "reference_region": {
            "focus_unity": {
                "x": FOCUS_UNITY[0],
                "y": FOCUS_UNITY[1],
            },
            "focus_display": {
                "x": FOCUS_UNITY[0] / DISPLAY_SCALE,
                "y": FOCUS_UNITY[1] / DISPLAY_SCALE,
            },
            "half_size_unity": {
                "x": REGION_HALF_SIZE[0],
                "y": REGION_HALF_SIZE[1],
            },
            "display_scale": DISPLAY_SCALE,
            "ue_units_per_unity_unit": UE_UNITS_PER_UNITY_UNIT,
        },
        "source_tables": source_tables,
        "candidate_counts": candidate_counts,
        "sampling_contract": {
            "policy": "deterministic_reference_region_targets",
            "spawn_all_candidates": False,
            "minimum_spacing_unity_units": {
                "resource": 10.0,
                "monster": 10.0,
                "fixed": 20.0,
            },
            "player_city_exclusion_radius_unity_units": (
                PLAYER_CITY_EXCLUSION_RADIUS
            ),
            "reference_dxf_2479_visible_window_unity": {
                "min_offset_x": -35.0,
                "max_offset_x": 35.0,
                "min_offset_y": -38.0,
                "max_offset_y": 38.0,
                "minimum_entity_count": 9,
            },
        },
        "lod_contract": {
            "owner": "/Script/Rok.RokWorldMapEntityActor",
            "source_lod_array_dxf": [
                1500.0,
                3000.0,
                8000.0,
                20000.0,
                42500.0,
            ],
            "close_detail_max_dxf": 1500.0,
            "tactical_detail_max_dxf": 3000.0,
            "far_icon_max_dxf": 5400.0,
            "behavior": {
                "close": "exact Unity detail visual at world scale",
                "tactical": (
                    "exact Unity detail visual with dxf scale compensation"
                ),
                "far": "exact Unity lod3 atlas icon",
                "beyond": "hidden for this reference-region prototype",
            },
        },
        "visual_library": visual_library,
        "player_city_lod3_visual": player_city_lod3_visual,
        "entities": entities,
        "summary": {
            "entity_count": len(entities),
            "kind_counts": kind_counts,
            "selectable_count": sum(
                1 for entity in entities if entity["selectable"]
            ),
            "exact_unity_detail_source_count": len(visual_library),
            "exact_unity_lod3_source_count": len(visual_library),
        },
    }
    MANIFEST_PATH.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    write_report(payload)
    print(
        "Rok world entity population built: "
        f"entities={len(entities)} "
        f"kinds={kind_counts} "
        f"manifest={relative(MANIFEST_PATH)}"
    )


if __name__ == "__main__":
    main()
