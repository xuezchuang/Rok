"""Build the Unity world-entity presentation manifest.

Source chains (all under ref/client-unity):
- Formation: Assets/BundleAssets/Barbarian/barbarian_formation/
  BarbarianFormation_{modelId}.prefab. Rows come from the Barbarian
  component's m_squareRows order; each row's BarbarianConfig carries
  serialized m_UnitDummys with unitPrefab + unitDummy transforms.
  WorldMgrMediator.InitBarbarianObj parents the formation under
  SceneObject/Troops_root and calls Troops.InitFormationS(des, Color.gray).
- Units: Assets/BundleAssets/Barbarian/barbarian_unit/Monster_1000.prefab
  and Assets/BundleAssets/Troop/troop_unit_t2/Arms2_*.prefab. Each is a
  SpriteRenderer (native rect/PPU, cm_slg_soldiers tint encoding) plus
  AnimationBase m_state[0] (IDLE) with 5 direction arrays of 8 frames.
- Level badge: UI/UIPrefabs/ui_prefabs_map/UIWorldObjectPop/
  UI_Tip_WorldObjectRssLevel.prefab (img_bg sprite pl_map_levelBg from
  ui_res_map/ui_map.png, 60x33, label font 18 bold).
- City nameplate: UI_Tip_WorldObjectPlayerLevel.prefab (bg0 name +
  pl_map_levelBg level plate + img_map_state_back icon).

Outputs under Saved/RokUnityMigration/WorldEntityPresentation/ plus
unity_world_entity_presentation.json/.md and a matching Verify script.
Idle animation and MatrixRow close-LOD expansion are applied by UE.
Pending: UIZoomCurve nameplate offset/scale curves.
"""

import csv
import json
import math
import re
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
from PrepareRokUnityWorldMapAssets import (  # noqa: E402
    PROJECT_ROOT,
    build_guid_index,
    parse_unity_curve,
    parse_unity_yaml_objects,
    sha256,
)
from PrepareRokUnityWorldMapStreamingAssets import (  # noqa: E402
    parse_sprite_sheet,
)

MIGRATION_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
OUTPUT_DIR = MIGRATION_DIR / "WorldEntityPresentation"
POPULATION_PATH = MIGRATION_DIR / "unity_world_entity_population.json"
OUTPUT_PATH = MIGRATION_DIR / "unity_world_entity_presentation.json"
MARKDOWN_PATH = MIGRATION_DIR / "unity_world_entity_presentation.md"

FORMATION_DIR = (
    "ref/client-unity/Assets/BundleAssets/Barbarian/barbarian_formation"
)
UNIT_PREFAB_DIR = (
    "ref/client-unity/Assets/BundleAssets/Barbarian/barbarian_unit"
)
TROOP_UNIT_DIR = "ref/client-unity/Assets/BundleAssets/Troop/troop_unit_t2"
UI_MAP_ATLAS = (
    "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/"
    "ui_map.png"
)
NAMEPLATE_PREFAB_DIR = (
    "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_map/"
    "UIWorldObjectPop"
)
MONSTER_TABLE = "ref/config-tables/编译客户端配置/CSV/Monster.csv"

# Identity unit_dummy faces Unity +Z. Cell.ChangeMoveState -> SetDirection(0)
# selects m_direction[0], i.e. the atlas's numbered direction _1_, not _2_.
IDLE_DIRECTION_INDEX = 0
IDLE_FRAME_INDEX = 0
BADGE_SPRITES = (
    "pl_map_levelBg",
    "pl_map_buildingNameBg",
    "img_map_state_back",
)
LEVEL_LABEL_COLOR = (0.184, 0.176, 0.153)


def rel(path):
    return str(Path(path).relative_to(PROJECT_ROOT)).replace("\\", "/")


def source_record(path):
    path = Path(path)
    return {
        "path": rel(path),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


# FRokWorldMapProjection.UeUnitsPerUnityUnit: the world map renders at
# 500 UE cm per Unity unit (the composite cluster card at 1150 cm for a
# ~2.3-unit formation confirms it). Formation prefab offsets and sprite
# sizes share this scale.
UE_CENTIMETERS_PER_UNITY_UNIT = 500.0


def unity_offset_to_ue(position):
    # Project conversion: (Unity x, y, z) -> (UE -x, z, y).
    return [
        round(-float(position[0]) * UE_CENTIMETERS_PER_UNITY_UNIT, 4),
        round(float(position[2]) * UE_CENTIMETERS_PER_UNITY_UNIT, 4),
        round(float(position[1]) * UE_CENTIMETERS_PER_UNITY_UNIT, 4),
    ]


def formation_offset_to_ue(position):
    # Keep the same handedness as the world. The +z leader row is north
    # (screen top), followed by soldiers and the rear siege carts.
    return unity_offset_to_ue(position)


def parse_transforms(objects):
    transforms = {}
    go_transform = {}
    for item in objects:
        if item["class_id"] != 4:
            continue
        body = item["body"]
        go_match = re.search(
            r"m_GameObject:\s*\{fileID:\s*(\d+)\}", body
        )
        if not go_match:
            continue
        position = re.search(
            r"m_LocalPosition:\s*\{x:\s*([-0-9.eE]+),\s*y:\s*([-0-9.eE]+),"
            r"\s*z:\s*([-0-9.eE]+)\}",
            body,
        )
        if not position:
            continue
        transforms[item["file_id"]] = position.groups()
        go_transform[go_match.group(1)] = item["file_id"]
    return transforms, go_transform


def parse_game_objects(objects):
    names = {}
    for item in objects:
        if item["class_id"] != 1:
            continue
        name = re.search(r"(?m)^\s*m_Name:\s*(.*?)\s*$", item["body"])
        if name:
            names[item["file_id"]] = name.group(1)
    return names


def parse_formation(prefab_id):
    prefab_path = PROJECT_ROOT / FORMATION_DIR / f"{prefab_id}.prefab"
    objects = parse_unity_yaml_objects(prefab_path)
    names = parse_game_objects(objects)
    transforms, go_transform = parse_transforms(objects)

    barbarian_block = None
    config_blocks = {}
    for item in objects:
        if item["class_id"] != 114:
            continue
        body = item["body"]
        if "m_squareRows" in body:
            barbarian_block = body
        if "m_UnitDummys" in body:
            config_blocks[item["file_id"]] = body
    if barbarian_block is None:
        raise RuntimeError(f"{prefab_id} lacks the Barbarian component")

    rows_section = barbarian_block.split("m_squareRows")[1]
    row_config_ids = re.findall(
        r"-\s*\{fileID:\s*(\d+)\}", rows_section
    )
    # MatrixRow.UpdateUnitPositionBarabarianLod expands every non-hero
    # row at LOD<=1 with m_expansivity = 0.18 (Unity units): the base
    # dummy shifts +-0.18 on z and two clones land diagonally from the
    # original position.
    EXPANSIVITY = 0.18

    def close_lod_offsets(unity_offset, unit_index):
        ox, oy, oz = unity_offset
        diagonal = EXPANSIVITY * 1.414
        if unit_index % 2 == 0:
            base = (ox, oy, oz + EXPANSIVITY)
            clone_a = (ox - diagonal, oy, oz - diagonal)
            clone_b = (ox + diagonal, oy, oz - diagonal)
        else:
            base = (ox, oy, oz - EXPANSIVITY)
            clone_a = (ox - diagonal, oy, oz + diagonal)
            clone_b = (ox + diagonal, oy, oz + diagonal)
        return [
            formation_offset_to_ue(base),
            formation_offset_to_ue(clone_a),
            formation_offset_to_ue(clone_b),
        ]

    rows = []
    total_units = 0
    for row_index, config_id in enumerate(row_config_ids):
        body = config_blocks.get(config_id)
        if body is None:
            raise RuntimeError(
                f"{prefab_id} row {row_index} config {config_id} missing"
            )
        go_match = re.search(
            r"m_GameObject:\s*\{fileID:\s*(\d+)\}", body
        )
        row_go = go_match.group(1)
        row_transform = transforms.get(go_transform.get(row_go))
        row_position = (
            [float(v) for v in row_transform] if row_transform else [0, 0, 0]
        )
        dummy_section = body.split("m_UnitDummys")[1]
        entries = re.findall(
            r"(?m)^\s*- unitDummy:\s*\{fileID:\s*(\d+)\}\n\s*unitPrefab:\s*(\S*)",
            dummy_section,
        )
        units = []
        for dummy_id, unit_prefab in entries:
            if not unit_prefab:
                continue
            dummy_transform = transforms.get(dummy_id)
            if dummy_transform is None:
                raise RuntimeError(
                    f"{prefab_id} dummy {dummy_id} lacks a transform"
                )
            local = [float(v) for v in dummy_transform]
            combined = [
                row_position[i] + local[i] for i in range(3)
            ]
            entry = {
                "unit_prefab": unit_prefab,
                "dummy_transform_file_id": dummy_id,
                "unity_offset": combined,
                "ue_offset_cm": formation_offset_to_ue(combined),
            }
            if row_index != 0:
                entry["close_lod_offsets_cm"] = close_lod_offsets(
                    combined,
                    len(units),
                )
            units.append(entry)
            total_units += 1
        rows.append(
            {
                "row_index": row_index,
                "is_hero_row": row_index == 0,
                "unity_row_position": row_position,
                "units": units,
            }
        )
    return {
        "prefab_id": prefab_id,
        "source_prefab": source_record(prefab_path),
        "rows": rows,
        "unit_count": total_units,
    }


def parse_unit_prefab(prefab_name, guid_index):
    for folder in (UNIT_PREFAB_DIR, TROOP_UNIT_DIR):
        candidate = PROJECT_ROOT / folder / f"{prefab_name}.prefab"
        if candidate.exists():
            prefab_path = candidate
            break
    else:
        raise RuntimeError(f"Unit prefab missing: {prefab_name}")
    objects = parse_unity_yaml_objects(prefab_path)
    sprite_size = None
    sprite_color = None
    object_names = parse_game_objects(objects)
    idle_direction_arrays = None
    idle_update_rate = None
    idle_random_offset = None
    for item in objects:
        if item["class_id"] == 212 and sprite_size is None:
            body = item["body"]
            owner = re.search(r"m_GameObject:\s*\{fileID:\s*(\d+)\}", body)
            if not owner or object_names.get(owner.group(1)) != prefab_name:
                continue  # shadow_lod1 is serialized before the body renderer
            size = re.search(
                r"m_Size:\s*\{x:\s*([-0-9.eE]+),\s*y:\s*([-0-9.eE]+)\}",
                body,
            )
            color = re.search(
                r"m_Color:\s*\{r:\s*([-0-9.eE]+),\s*g:\s*([-0-9.eE]+),"
                r"\s*b:\s*([-0-9.eE]+),\s*a:\s*([-0-9.eE]+)\}",
                body,
            )
            if size:
                sprite_size = [float(size.group(1)), float(size.group(2))]
            if color:
                sprite_color = [float(v) for v in color.groups()]
        if item["class_id"] == 114 and idle_direction_arrays is None:
            body = item["body"]
            if "m_state:" not in body or "m_direction" not in body:
                continue
            state_section = body.split("m_state:")[1]
            first_state = state_section.split("- m_direction")[1]
            idle_update_rate = float(re.search(r"m_update_rate:\s*([-0-9.eE]+)", first_state).group(1))
            idle_random_offset = float(re.search(r"m_playing_random_offset:\s*([-0-9.eE]+)", body).group(1))
            if not math.isfinite(idle_update_rate) or idle_update_rate < 0 or not math.isfinite(idle_random_offset) or idle_random_offset < 0:
                raise RuntimeError(f"{prefab_name} has invalid idle timing")
            direction_section = ("- m_direction" + first_state).split(
                "m_update_rate"
            )[0]
            idle_direction_arrays = re.findall(
                r"(?s)m_sprite_array:(.*?)(?=m_sprite_array:|\Z)",
                direction_section,
            )
    if sprite_size is None or idle_direction_arrays is None:
        raise RuntimeError(
            f"{prefab_name} lacks SpriteRenderer size or idle arrays"
        )
    if len(idle_direction_arrays) < 5:
        raise RuntimeError(
            f"{prefab_name} expected 5 idle directions, found "
            f"{len(idle_direction_arrays)}"
        )
    direction = idle_direction_arrays[IDLE_DIRECTION_INDEX]
    frames = re.findall(
        r"\{fileID:\s*(-?\d+),\s*guid:\s*([0-9a-fA-F]+),\s*type:\s*3\}",
        direction,
    )
    if len(frames) != 8:
        raise RuntimeError(
            f"{prefab_name} idle direction 1 expected 8 frames, found "
            f"{len(frames)}"
        )
    sheet_guid = frames[0][1].lower()
    sheet_path = guid_index.get(sheet_guid)
    if sheet_path is None:
        raise RuntimeError(
            f"{prefab_name} idle sprite sheet guid unresolved: {sheet_guid}"
        )
    sheet_importer, sheet_records = parse_sprite_sheet(
        Path(str(sheet_path) + ".meta"), sheet_path
    )
    frame_records = []
    for frame_file_id, _ in frames:
        meta = sheet_records.get(frame_file_id)
        if meta is None:
            raise RuntimeError(
                f"{prefab_name} frame {frame_file_id} missing from "
                f"{rel(sheet_path)}.meta"
            )
        frame_records.append(
            {
                "file_id": frame_file_id,
                "name": meta["name"],
                "rect": dict(meta["rect"]),
                "pivot": dict(meta["pivot"]),
            }
        )
    return {
        "prefab_name": prefab_name,
        "source_prefab": source_record(prefab_path),
        "sprite_sheet": source_record(sheet_path),
        "pixels_to_units": sheet_importer["sprite_pixels_to_units"],
        "quad_size_unity": sprite_size,
        "prefab_tint": sprite_color,
        "runtime_tint": [0.5, 0.5, 0.5, 1.0],
        "runtime_tint_source": "InitBarbarianObj -> Color.gray (neutral in cm_slg_soldiers)",
        "idle_direction_index": IDLE_DIRECTION_INDEX,
        "idle_update_rate": idle_update_rate,
        "idle_random_offset": idle_random_offset,
        "idle_frames_direction_1": frame_records,
    }


IDLE_FRAME_COUNT = 8


def parse_packed_sprite_geometry(sheet_path):
    """TexturePacker 40300 polygons, not overlapping sprite bounding boxes."""
    path = sheet_path.with_suffix(".tpsheet")
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    if ":format=40300" not in lines:
        raise RuntimeError(f"Unsupported polygon sheet format: {path}")
    result = {}
    for line in lines:
        if not line or line.startswith(("#", ":")):
            continue
        fields = [field.strip() for field in line.split(";")]
        count = int(fields[11])
        vertices = [
            (float(fields[12 + 2 * i]), float(fields[13 + 2 * i]))
            for i in range(count)
        ]
        triangle_start = 12 + count * 2
        triangle_count = int(fields[triangle_start])
        indices = [int(v) for v in fields[triangle_start + 1:]]
        if len(indices) != triangle_count * 3 or any(
            index < 0 or index >= count for index in indices
        ):
            raise RuntimeError(f"Malformed sprite triangles: {fields[0]}")
        result[fields[0]] = {
            "rect": [int(v) for v in fields[1:5]],
            "vertices": vertices,
            "triangles": indices,
        }
    return result


def decode_unit_sprite(crop, geometry, fade=False):
    # cm_slg_soldiers.shader SpriteFrag: alpha*2 encodes opacity and the
    # team-color mask. Color.gray*2 == 1, so barbarians keep the original
    # RGB, NOT RGB*0.5. The shader premultiplies RGB before SrcAlpha blend.
    r, g, b, raw_alpha = crop.split()
    alpha = raw_alpha.point(lambda value: min(value * 2, 255))
    if not fade:
        alpha = alpha.point(lambda value: value if value >= 26 else 0)
    mask = Image.new("L", crop.size, 0)
    draw = ImageDraw.Draw(mask)
    vertices = [(x, crop.height - y) for x, y in geometry["vertices"]]
    indices = geometry["triangles"]
    for start in range(0, len(indices), 3):
        draw.polygon([vertices[i] for i in indices[start:start + 3]], fill=255)
    alpha = ImageChops.multiply(alpha, mask)
    if fade:
        # Decode after bilinear texture sampling in the UE material, as
        # SpriteFrag does. Pre-multiplication here darkens mixed texels.
        return Image.merge("RGBA", (r, g, b, ImageChops.multiply(raw_alpha, mask)))
    return Image.merge("RGBA", (
        ImageChops.multiply(r, alpha),
        ImageChops.multiply(g, alpha),
        ImageChops.multiply(b, alpha),
        ImageChops.multiply(raw_alpha, mask) if fade else alpha,
    ))


def crop_idle_frames(unit):
    sheet_path = PROJECT_ROOT / unit["sprite_sheet"]["path"]
    image = Image.open(sheet_path).convert("RGBA")
    packed_geometry = parse_packed_sprite_geometry(sheet_path)
    unit["sprite_geometry_source"] = source_record(sheet_path.with_suffix(".tpsheet"))
    frames = unit["idle_frames_direction_1"][:IDLE_FRAME_COUNT]
    # Trimmed animation frames have different rectangles/pivots. Repack
    # to one pivot-aligned canvas so changing texture never stretches the
    # unit or shifts its feet from the formation dummy.
    pivots = [(round(f["rect"]["width"] * f["pivot"]["x"]),
               round(f["rect"]["height"] * f["pivot"]["y"])) for f in frames]
    left = max(p[0] for p in pivots)
    bottom = max(p[1] for p in pivots)
    right = max(round(f["rect"]["width"]) - p[0] for f, p in zip(frames, pivots))
    top_extent = max(round(f["rect"]["height"]) - p[1] for f, p in zip(frames, pivots))
    canvas_size = (left + right, bottom + top_extent)
    canvas_pivot = {"x": left / canvas_size[0], "y": bottom / canvas_size[1]}
    crops = []
    for frame_index, frame in enumerate(frames):
        rect = frame["rect"]
        # Unity TextureImporter rects use a bottom-left origin; PIL crops
        # top-left first.
        top = image.height - rect["y"] - rect["height"]
        box = (
            rect["x"],
            top,
            rect["x"] + rect["width"],
            top + rect["height"],
        )
        geometry = packed_geometry[frame["name"]]
        if geometry["rect"] != [rect[k] for k in ("x", "y", "width", "height")]:
            raise RuntimeError(f"Sprite metadata/geometry disagree: {frame['name']}")
        crop = decode_unit_sprite(image.crop(box), geometry)
        fade_crop = decode_unit_sprite(image.crop(box), geometry, fade=True)
        pivot = pivots[frame_index]
        canvas = Image.new("RGBA", canvas_size, (0, 0, 0, 0))
        canvas.paste(crop, (left - pivot[0], top_extent - (crop.height - pivot[1])))
        crop = canvas
        output_path = (
            OUTPUT_DIR
            / f"{unit['prefab_name']}_idle_d1_f{frame_index}.png"
        )
        crop.save(output_path)
        fade_canvas = Image.new("RGBA", canvas_size, (0, 0, 0, 0))
        fade_canvas.paste(fade_crop, (left - pivot[0], top_extent - (fade_crop.height - pivot[1])))
        fade_path = output_path.with_name(output_path.stem + "_fade.png")
        fade_canvas.save(fade_path)
        # SpriteRenderer drawMode is Simple (0): the serialized m_Size is
        # ignored at runtime and each frame renders at its NATIVE size
        # (pixels / spritePixelsToUnits). The LOD unit-scale curve
        # (m_unit_scale_curve, applied by the actor) multiplies on top.
        # Using m_Size made the units ~2.5x too wide; using native size
        # WITHOUT the curve made them too small and scattered. The
        # Unity-reference-verified combination is native size x curve.
        pixels_to_units = unit["pixels_to_units"]
        crops.append(
            {
                "path": rel(output_path),
                "fade_path": rel(fade_path),
                "fade_sha256": sha256(fade_path),
                "sha256": sha256(output_path),
                "width": crop.width,
                "height": crop.height,
                "pivot": canvas_pivot,
                "source_sprite_name": frame["name"],
                "polygon_triangle_count": len(geometry["triangles"]) // 3,
                "world_size_cm": {
                    "width": round(
                        crop.width / pixels_to_units
                        * UE_CENTIMETERS_PER_UNITY_UNIT,
                        3,
                    ),
                    "height": round(
                        crop.height / pixels_to_units
                        * UE_CENTIMETERS_PER_UNITY_UNIT,
                        3,
                    ),
                },
            }
        )
    return crops


def crop_badge_sprites(guid_index):
    atlas_path = PROJECT_ROOT / UI_MAP_ATLAS
    meta_path = Path(str(atlas_path) + ".meta")
    _, records = parse_sprite_sheet(meta_path, atlas_path)
    by_name = {
        meta["name"]: (file_id, meta)
        for file_id, meta in records.items()
    }
    image = Image.open(atlas_path).convert("RGBA")
    crops = {}
    for sprite_name in BADGE_SPRITES:
        if sprite_name not in by_name:
            raise RuntimeError(f"ui_map.png lacks sprite {sprite_name}")
        file_id, meta = by_name[sprite_name]
        rect = dict(meta["rect"])
        top = image.height - rect["y"] - rect["height"]
        crop = image.crop(
            (
                rect["x"],
                top,
                rect["x"] + rect["width"],
                top + rect["height"],
            )
        )
        output_path = OUTPUT_DIR / f"{sprite_name}.png"
        crop.save(output_path)
        crops[sprite_name] = {
            "source_file_id": file_id,
            "atlas": source_record(atlas_path),
            "rect": rect,
            "output": {
                "path": rel(output_path),
                "sha256": sha256(output_path),
                "width": crop.width,
                "height": crop.height,
            },
        }
    return crops


def parse_nameplate_specs():
    specs = {}
    for prefab_name in (
        "UI_Tip_WorldObjectRssLevel",
        "UI_Tip_WorldObjectPlayerLevel",
    ):
        prefab_path = (
            PROJECT_ROOT / NAMEPLATE_PREFAB_DIR / f"{prefab_name}.prefab"
        )
        text = prefab_path.read_text(encoding="utf-8", errors="ignore")
        elements = []
        for match in re.finditer(
            r"(?ms)m_Name:\s*(\S+).*?m_AnchoredPosition:\s*\{x:\s*"
            r"([-0-9.eE]+),\s*y:\s*([-0-9.eE]+)\}.*?"
            r"m_SizeDelta:\s*\{x:\s*([-0-9.eE]+),\s*y:\s*([-0-9.eE]+)\}",
            text,
        ):
            elements.append(
                {
                    "name": match.group(1),
                    "anchored_position": [
                        float(match.group(2)),
                        float(match.group(3)),
                    ],
                    "size": [
                        float(match.group(4)),
                        float(match.group(5)),
                    ],
                }
            )
        specs[prefab_name] = {
            "source_prefab": source_record(prefab_path),
            "elements": elements,
        }
    return specs


def resolve_monsters(population):
    barbarian_levels = sorted(
        {
            entity["level"]
            for entity in population["entities"]
            if entity["kind"] == "barbarian"
        }
    )
    with open(PROJECT_ROOT / MONSTER_TABLE, encoding="utf-8-sig") as handle:
        rows = list(csv.reader(handle))
    header = {name: index for index, name in enumerate(rows[0])}
    monsters = {}
    for row in rows[2:]:
        if row[header["type"]] != "1":
            continue
        level = int(row[header["level"]])
        if level in barbarian_levels:
            monsters[level] = {
                "monster_id": int(row[header["ID"]]),
                "model_id": row[header["modelId"]],
                "radius": float(row[header["radius"]]),
            }
    missing = [level for level in barbarian_levels if level not in monsters]
    if missing:
        raise RuntimeError(f"Monster table lacks barbarian levels {missing}")
    return barbarian_levels, monsters


UNIT_SCALE_CURVE_KEYS = []


def runtime_unit_scale_keys(source_keys):
    """Keep Hermite slopes; Infinity is Unity's step, not JSON Infinity."""
    result = []
    for key in source_keys:
        constant = not math.isfinite(key["out_slope"])
        if key["weighted_mode"] and not constant:
            raise RuntimeError("Unit scale curve gained an unsupported weighted tangent")
        result.append({
            "dxf": key["dxf"], "scale": key["scale"],
            "in_slope": key["in_slope"] if math.isfinite(key["in_slope"]) else 0.0,
            "out_slope": key["out_slope"] if not constant else 0.0,
            "interpolation": "constant" if constant else "cubic",
        })
    return result


def formation_lod_contract(formations):
    source_path = PROJECT_ROOT / "ref/client-unity/Assets/Scripts/Client/LodManager/LevelDetailBase.cs"
    source_text = source_path.read_text(encoding="utf-8", errors="replace")
    if "lodDistance <= this.m_lod_array[i]" not in source_text:
        raise RuntimeError("Reinspect source formation LOD comparison before regenerating")
    thresholds = {}
    for level, formation in formations.items():
        text = (PROJECT_ROOT / formation["source_prefab"]["path"]).read_text(encoding="utf-8")
        matches = re.findall(r"  m_lod_array:\s*\n((?:  - [^\n]+\n)+)", text)
        if len(matches) != 1:
            raise RuntimeError(f"Ambiguous formation {level} LOD array")
        thresholds[level] = [float(line.strip()[2:]) for line in matches[0].splitlines()]
    return {
        "source": source_record(source_path),
        "upper_bound_comparison": "less_than_or_equal",
        "thresholds_by_level": thresholds,
        "source_samples": [
            {"dxf": dxf, "lod": next((i for i, limit in enumerate(next(iter(thresholds.values()))) if dxf <= limit), 2)}
            for dxf in (500.9, 501.0, 501.01, 899.9, 900.0, 900.01)
        ],
    }


def barbarian_icon_lod_contract():
    camera_path = PROJECT_ROOT / "ref/client-unity/Assets/Scripts/Client/LodManager/LevelDetailCamera.cs"
    owner_path = PROJECT_ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/GlobalViewLevelMediator.cs"
    camera = camera_path.read_text(encoding="utf-8", errors="replace")
    owner = owner_path.read_text(encoding="utf-8", errors="replace")
    scene_path = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Scenes/GameApp.unity"
    camera_guid = re.search(r"guid:\s*(\w+)", Path(str(camera_path) + ".meta").read_text(encoding="utf-8")).group(1)
    camera_arrays = []
    for item in parse_unity_yaml_objects(scene_path):
        if item["class_id"] == 114 and f"guid: {camera_guid}" in item["body"]:
            block = re.search(r"  m_lod_array:\s*\n((?:  - [^\n]+\n)+)", item["body"])
            camera_arrays.append([float(line.strip()[2:]) for line in block.group(1).splitlines()])
    if not camera_arrays or any(values[:2] != [1500.0, 3000.0] for values in camera_arrays):
        raise RuntimeError("GameApp camera thresholds changed; reevaluate icon boundary")
    config = owner.split("var BarbarianObj = new LodPopViewConfig", 1)[1].split("};", 1)[0]
    object_refresh = owner.split("private void RefreshObjElementView", 1)[1].split("private void RefreshHUDElementView", 1)[0]
    if ("dxf <= this.m_lod_array[i]" not in camera
            or "minLodLevel = MapViewLevel.TacticsToStrategy_1" not in config
            or "popType = PopViewType.Obj" not in config
            or "var visible = cur >= min && cur <= max;" not in object_refresh
            or "obj.gameObject.SetActive(visible);" not in object_refresh):
        raise RuntimeError("Reinspect barbarian icon source visibility threshold")
    return {
        "camera_source": source_record(camera_path),
        "icon_owner_source": source_record(owner_path),
        "camera_scene": source_record(scene_path),
        "camera_lod_arrays": camera_arrays,
        "icon_prefab": "barbarian_lod3",
        "enable_rule": "CalcMapViewLevel > Tactical; BarbarianObj is PopViewType.Obj with minLodLevel=TacticsToStrategy_1; RefreshObjElementView directly SetActive(cur>=min && cur<=max)",
        "samples": [{"dxf": 3000.0, "enable_far_icon": False}, {"dxf": 3000.01, "enable_far_icon": True}],
        "exact_3000": "source unit scale curve is zero; detail component state does not prove visible geometry",
        "transition": "immediate SetActive on creation callback and subsequent LOD refresh; no time-based icon fade",
        "component_behavior": "LevelDetailScale evaluates camera-distance scale curve; MapElementUI CanvasGroup fade is a separate HUD-only path",
    }


def main():
    global UNIT_SCALE_CURVE_KEYS
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    guid_index = build_guid_index()
    UNIT_SCALE_CURVE_KEYS = parse_unity_curve(
        PROJECT_ROOT
        / "ref/client-unity/Assets/BundleAssets/Scenes/GameApp.unity",
        "m_unit_scale_curve",
    )
    if len(UNIT_SCALE_CURVE_KEYS) < 5:
        raise RuntimeError("unit scale curve extraction came back thin")
    population = json.loads(
        POPULATION_PATH.read_text(encoding="utf-8")
    )
    levels, monsters = resolve_monsters(population)

    formations = {}
    unit_lib = {}
    for level in levels:
        model_id = monsters[level]["model_id"]
        formation = parse_formation(model_id)
        formations[str(level)] = {
            "monster": monsters[level],
            **formation,
        }
        for row in formation["rows"]:
            for unit in row["units"]:
                unit_lib.setdefault(unit["unit_prefab"], None)
    for prefab_name in unit_lib:
        unit_lib[prefab_name] = parse_unit_prefab(prefab_name, guid_index)
        unit_lib[prefab_name]["frame_crops"] = crop_idle_frames(
            unit_lib[prefab_name]
        )

    manifest = {
        "schema": "rok.unity_world_entity_presentation.v1",
        "source_population": rel(POPULATION_PATH),
        "formations": formations,
        "formation_lod": formation_lod_contract(formations),
        "barbarian_icon_lod": barbarian_icon_lod_contract(),
        "unit_library": unit_lib,
        "badge_sprites": crop_badge_sprites(guid_index),
        "nameplate_specs": parse_nameplate_specs(),
        "level_label": {
            "font_size": 18,
            "bold": True,
            "color_rgb": list(LEVEL_LABEL_COLOR),
            "source": "UI_Tip_WorldObjectRssLevel lbl_level",
        },
        "idle_animation": {
            "frame_count": IDLE_FRAME_COUNT,
            "timing_fields": "unit_library idle_update_rate and idle_random_offset",
            "source": source_record(PROJECT_ROOT / "ref/client-unity/Assets/Scripts/Client/Compoment/SpriteAnimations/AnimationBase.cs"),
            "start_frame": "ObjectPoolItem OnSpawn resets frame to zero after OnEnable; independent random timer phase per unit and clone",
            "minimum_interval_seconds": 0.0416,
            "advance": "elapsed > interval; skip elapsed whole frames; overflow frame resets to zero",
        },
        "spawn_fade": {
            "source": source_record(PROJECT_ROOT / "ref/client-unity/Assets/Scripts/Client/Compoment/SpriteAnimations/AnimationBase.cs"),
            "shader": source_record(PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Map/Building/customspriteunit_fade_out-2.shader"),
            "alpha_per_second": 3.0,
            "fade_crop": "raw source RGB/A; polygon exterior alpha zero; no normal alpha clip or RGB premultiplication",
            "fade_rgb": "sample raw texture bilinearly, then RGB *= saturate(sampled A * 2)",
            "opacity": "saturate(fade crop A * 2 * FadeAmount)",
            "normal_crop": "existing decoded RGB/A and 0.1 alpha clip; selected when fade completes",
            "scope": "barbarian main and clone spawn FadeIn; LOD ResetUnit destroys old cards without FadeOut",
        },
        "unit_scale_curve": {
            "source": (
                "ref/client-unity/Assets/BundleAssets/Scenes/GameApp.unity "
                "LevelDetailScalerManager m_unit_scale_curve"
            ),
            "keys": runtime_unit_scale_keys(UNIT_SCALE_CURVE_KEYS),
        },
        "unit_quad_orientation": {
            "unity_euler": [45.0, 0.0, 0.0],
            "source": "Cell.InitDummy",
            "ue_note": (
                "world-map camera pitch is fixed, so camera-facing "
                "billboards are equivalent to the 45-degree tilted quads"
            ),
        },
        "sprite_shader": {
            "source": source_record(PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Map/Shader/cm_slg_soldiers.shader"),
            "opacity": "clamp(texture_alpha * 2, 0, 1); clip below 0.1",
            "barbarian_color": "Color.gray is neutral team tint; RGB multiplied by decoded alpha",
            "geometry": "TexturePacker 40300 source triangles remove neighboring packed sprites",
        },
        "pending": [
            "uizoomcurve_nameplate_offset_and_scale_curves",
        ],
    }
    OUTPUT_PATH.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    lines = [
        "# Unity World Entity Presentation",
        "",
        f"- formations: {len(formations)}",
    ]
    for level, formation in formations.items():
        lines.append(
            f"  - level {level}: {formation['prefab_id']} "
            f"({formation['unit_count']} units)"
        )
    for name, unit in unit_lib.items():
        lines.append(
            f"- unit {name}: quad {unit['quad_size_unity']}, "
            f"idle frames {len(unit['idle_frames_direction_1'])}"
        )
    MARKDOWN_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(
        "Rok Unity world-entity presentation built: "
        f"formations={len(formations)} units={len(unit_lib)} "
        f"badges={len(BADGE_SPRITES)}"
    )


if __name__ == "__main__":
    main()
