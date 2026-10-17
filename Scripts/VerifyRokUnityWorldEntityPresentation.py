"""Verify the Unity world-entity presentation manifest."""

import json
import re
import sys
from pathlib import Path
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BuildRokUnityWorldEntityPresentation import (  # noqa: E402
    BADGE_SPRITES,
    IDLE_DIRECTION_INDEX,
    MIGRATION_DIR,
    OUTPUT_PATH,
    PROJECT_ROOT,
    decode_unit_sprite,
    formation_offset_to_ue,
    runtime_unit_scale_keys,
    parse_packed_sprite_geometry,
    formation_lod_contract,
    barbarian_icon_lod_contract,
)
from PrepareRokUnityWorldMapAssets import parse_unity_curve, sha256  # noqa: E402

SCHEMA = "rok.unity_world_entity_presentation.v1"
EXPECTED_FORMATION_LEVELS = {"14", "16"}
EXPECTED_UNIT_PREFABS = {
    "Monster_1000",
    "Arms2_1",
    "Arms2_3",
    "Monster_2004",
}
EXPECTED_UNITS_PER_FORMATION = 15


def bilinear(image, x, y):
    ix, iy = int(x), int(y)
    tx, ty = x - ix, y - iy
    pixels = [image.getpixel((ix + dx, iy + dy)) for dy in (0, 1) for dx in (0, 1)]
    weights = [(1-tx)*(1-ty), tx*(1-ty), (1-tx)*ty, tx*ty]
    return [sum(p[c] * w for p, w in zip(pixels, weights)) / 255 for c in range(4)]


def verify_fade_subpixels(unit):
    """Compare exported fade sampling against SpriteFrag on real atlas edges.

    Interior sprite polygons are tested; polygon rasterization equivalence
    and GPU filtering/color-space acceptance remain runtime visual gates.
    """
    sheet_path = PROJECT_ROOT / unit["sprite_sheet"]["path"]
    sheet = Image.open(sheet_path).convert("RGBA")
    geometries = parse_packed_sprite_geometry(sheet_path)
    checked = 0
    for frame, record in zip(unit["idle_frames_direction_1"], unit["frame_crops"]):
        rect = frame["rect"]
        x0, top = int(rect["x"]), int(sheet.height - rect["y"] - rect["height"])
        width, height = int(rect["width"]), int(rect["height"])
        source = sheet.crop((x0, top, x0 + width, top + height))
        mask = decode_unit_sprite(Image.new("RGBA", source.size, (255, 255, 255, 255)), geometries[frame["name"]], fade=True)
        output = Image.open(PROJECT_ROOT / record["fade_path"]).convert("RGBA")
        offset_x = round(record["pivot"]["x"] * output.width) - round(frame["pivot"]["x"] * width)
        offset_y = output.height - round(record["pivot"]["y"] * output.height) - (height - round(frame["pivot"]["y"] * height))
        edge_count = 0
        for y in range(height - 1):
            for x in range(width - 1):
                coords = [(x, y), (x+1, y), (x, y+1), (x+1, y+1)]
                alphas = [source.getpixel(p)[3] for p in coords]
                if max(alphas)-min(alphas) < 32 or any(mask.getpixel(p)[3] != 255 for p in coords):
                    continue
                for tx, ty in ((0.25, 0.3), (0.5, 0.5), (0.75, 0.6)):
                    src = bilinear(source, x+tx, y+ty)
                    dst = bilinear(output, x+offset_x+tx, y+offset_y+ty)
                    for fade in (0.1, 0.5, 0.9):
                        expected = [c * min(1, src[3]*2) * min(1, src[3]*2*fade) for c in src[:3]]
                        actual = [c * min(1, dst[3]*2) * min(1, dst[3]*2*fade) for c in dst[:3]]
                        if max(abs(a-b) for a, b in zip(expected, actual)) > 1e-9:
                            raise AssertionError(f"fade subpixel mismatch {frame['name']} at {x+tx},{y+ty}")
                        checked += 1
                edge_count += 1
                if edge_count >= 20:
                    break
            if edge_count >= 20:
                break
    if checked == 0:
        raise AssertionError("no source alpha edges exercised")
    return checked


def main():
    failures = []
    # Regression for the packed-alpha shader: 128 is opaque neutral art,
    # not a half-transparent, half-dark barbarian. Polygon exterior is empty.
    sample = Image.new("RGBA", (4, 4), (200, 120, 80, 128))
    geometry = {"vertices": [(0, 0), (4, 0), (0, 4)], "triangles": [0, 1, 2]}
    decoded = decode_unit_sprite(sample, geometry)
    if decoded.getpixel((1, 3)) != (200, 120, 80, 255):
        failures.append("soldier shader neutral tint/encoded alpha regression")
    if decoded.getpixel((3, 0))[3] != 0:
        failures.append("polygon crop includes neighboring packed sprite")
    fade_decoded = decode_unit_sprite(sample, geometry, fade=True)
    if fade_decoded.getpixel((1, 3)) != (200, 120, 80, 128):
        failures.append("fade crop must retain source alpha before per-instance multiplication")
    faint = Image.new("RGBA", (4, 4), (255, 255, 255, 10))
    if decode_unit_sprite(faint, geometry, fade=True).getpixel((1, 3)) != (255, 255, 255, 10):
        failures.append("fade crop incorrectly applies normal shader alpha clip")
    if formation_offset_to_ue([0, 0, 1])[1] <= 0:
        failures.append("formation leader was mirrored to the south")
    manifest = json.loads(OUTPUT_PATH.read_text(encoding="utf-8"))
    source_keys = parse_unity_curve(
        PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/Scenes/GameApp.unity",
        "m_unit_scale_curve",
    )
    if manifest.get("unit_scale_curve", {}).get("keys") != runtime_unit_scale_keys(source_keys):
        failures.append("unit scale curve lost source tangents or constant LOD boundary")
    if manifest.get("schema") != SCHEMA:
        failures.append("schema changed")
    if not (PROJECT_ROOT / manifest["source_population"]).exists():
        failures.append("source population manifest missing")

    formations = manifest.get("formations", {})
    if manifest.get("barbarian_icon_lod") != barbarian_icon_lod_contract():
        failures.append("barbarian icon visibility source chain or 3000 boundary drifted")
    if "pending" in manifest.get("barbarian_icon_lod", {}):
        failures.append("barbarian Obj direct visibility must not invent a HUD fade migration requirement")
    if manifest.get("formation_lod") != formation_lod_contract(formations):
        failures.append("formation LOD contract differs from current source upper-bound comparison")
    expected_lods = [0, 0, 1, 1, 1, 2]
    if any(limits != [501.0, 900.0, 99999.0] for limits in manifest.get("formation_lod", {}).get("thresholds_by_level", {}).values()):
        failures.append("runtime formation LOD thresholds no longer match every source formation")
    if [row["lod"] for row in manifest.get("formation_lod", {}).get("source_samples", [])] != expected_lods:
        failures.append("formation 501/900 equality must remain in the lower LOD")
    if set(formations) != EXPECTED_FORMATION_LEVELS:
        failures.append(
            f"formation levels are {sorted(formations)}, expected "
            f"{sorted(EXPECTED_FORMATION_LEVELS)}"
        )
    for level, formation in formations.items():
        if formation["unit_count"] != EXPECTED_UNITS_PER_FORMATION:
            failures.append(
                f"formation level {level} has {formation['unit_count']} "
                f"units, expected {EXPECTED_UNITS_PER_FORMATION}"
            )
        hero_rows = [
            row for row in formation["rows"] if row["is_hero_row"]
        ]
        if len(hero_rows) != 1 or not hero_rows[0]["units"]:
            failures.append(f"formation level {level} hero row is empty")
        for row in formation["rows"]:
            for unit in row["units"]:
                if len(unit["ue_offset_cm"]) != 3:
                    failures.append(
                        f"formation level {level} unit offset malformed"
                    )
                if row["is_hero_row"]:
                    if "close_lod_offsets_cm" in unit:
                        failures.append(
                            f"formation level {level} hero row must not "
                            "carry close-LOD expansion"
                        )
                else:
                    variants = unit.get("close_lod_offsets_cm", [])
                    if len(variants) != 3 or any(
                        len(offset) != 3 for offset in variants
                    ):
                        failures.append(
                            f"formation level {level} unit close-LOD "
                            "expansion is malformed"
                        )
        if not (PROJECT_ROOT / formation["source_prefab"]["path"]).exists():
            failures.append(
                f"formation level {level} source prefab missing"
            )
        elif sha256(
            PROJECT_ROOT / formation["source_prefab"]["path"]
        ) != formation["source_prefab"]["sha256"]:
            failures.append(
                f"formation level {level} source prefab changed"
            )

    unit_lib = manifest.get("unit_library", {})
    if set(unit_lib) != EXPECTED_UNIT_PREFABS:
        failures.append(
            f"unit library is {sorted(unit_lib)}, expected "
            f"{sorted(EXPECTED_UNIT_PREFABS)}"
        )
    for name, unit in unit_lib.items():
        try:
            print(f"Fade subpixel source checks {name}: {verify_fade_subpixels(unit)}")
        except (AssertionError, KeyError, FileNotFoundError) as error:
            failures.append(str(error))
        prefab = (PROJECT_ROOT / unit["source_prefab"]["path"]).read_text(encoding="utf-8")
        idle_state = prefab.split("m_state:", 1)[1].split("- m_direction", 2)[1]
        expected_rate = float(re.search(r"m_update_rate:\s*([-0-9.eE]+)", idle_state).group(1))
        expected_offset = float(re.search(r"m_playing_random_offset:\s*([-0-9.eE]+)", prefab).group(1))
        if unit.get("idle_update_rate") != expected_rate or unit.get("idle_random_offset") != expected_offset:
            failures.append(f"unit {name} idle timing differs from its AnimationBase prefab")
        if unit.get("idle_direction_index") != 0:
            failures.append(f"unit {name} default dummy direction is not +Z")
        geometry_source = unit.get("sprite_geometry_source", {})
        geometry_path = PROJECT_ROOT / geometry_source.get("path", "")
        if not geometry_path.is_file() or sha256(geometry_path) != geometry_source.get("sha256"):
            failures.append(f"unit {name} source polygon geometry missing or changed")
        if len(unit["quad_size_unity"]) != 2:
            failures.append(f"unit {name} quad size malformed")
        if len(unit["idle_frames_direction_1"]) != 8:
            failures.append(
                f"unit {name} idle direction {IDLE_DIRECTION_INDEX} does "
                "not carry 8 frames"
            )
        crops = unit.get("frame_crops", [])
        for crop_record in crops:
            fade_path = PROJECT_ROOT / crop_record.get("fade_path", "")
            if not fade_path.is_file() or sha256(fade_path) != crop_record.get("fade_sha256"):
                failures.append(f"unit {name} source fade frame missing or changed")
            elif Image.open(fade_path).size != (crop_record["width"], crop_record["height"]):
                failures.append(f"unit {name} fade frame loses pivot-aligned canvas")
        if len(crops) != 8:
            failures.append(
                f"unit {name} has {len(crops)} frame crops, expected 8"
            )
        for frame_index, crop in enumerate(crops):
            if crop.get("polygon_triangle_count", 0) <= 0:
                failures.append(f"unit {name} frame {frame_index} lost packed sprite geometry")
            if crops and any(crop.get(key) != crops[0].get(key) for key in ("width", "height", "pivot")):
                failures.append(f"unit {name} frame {frame_index} shifts its pivot/size")
            crop_path = PROJECT_ROOT / crop.get("path", "")
            if not crop_path.exists():
                failures.append(f"unit {name} frame {frame_index} missing")
            elif sha256(crop_path) != crop.get("sha256"):
                failures.append(f"unit {name} frame {frame_index} changed")
            elif crop.get("width", 0) < 16 or crop.get("height", 0) < 16:
                failures.append(
                    f"unit {name} frame {frame_index} is suspiciously small"
                )
            elif Image.open(crop_path).getchannel("A").getextrema()[1] != 255:
                failures.append(f"unit {name} frame {frame_index} still has encoded half-alpha")
        if not unit.get("idle_animation_ref"):
            pass  # animation metadata lives at manifest level

    badges = manifest.get("badge_sprites", {})
    if set(badges) != set(BADGE_SPRITES):
        failures.append(f"badge sprites are {sorted(badges)}")
    for name, badge in badges.items():
        output = PROJECT_ROOT / badge["output"]["path"]
        if not output.exists():
            failures.append(f"badge {name} crop missing")
        elif sha256(output) != badge["output"]["sha256"]:
            failures.append(f"badge {name} crop changed")
        elif badge["output"]["width"] != int(badge["rect"]["width"]):
            failures.append(f"badge {name} crop width differs from atlas")

    for prefab_name, spec in manifest.get("nameplate_specs", {}).items():
        if not (PROJECT_ROOT / spec["source_prefab"]["path"]).exists():
            failures.append(f"nameplate prefab {prefab_name} missing")

    pending = manifest.get("pending", [])
    for required in (
        "uizoomcurve_nameplate_offset_and_scale_curves",
    ):
        if required not in pending:
            failures.append(f"pending policy {required} was dropped")
    animation = manifest.get("idle_animation", {})
    if (
        animation.get("frame_count") != 8
        or abs(animation.get("minimum_interval_seconds", 0.0) - 0.0416) > 1e-6
    ):
        failures.append("idle animation metadata is not source-faithful")
    animation_source = animation.get("source", {})
    animation_path = PROJECT_ROOT / animation_source.get("path", "")
    if not animation_path.is_file() or sha256(animation_path) != animation_source.get("sha256"):
        failures.append("idle AnimationBase source missing or changed")
    fade = manifest.get("spawn_fade", {})
    if fade.get("alpha_per_second") != 3.0:
        failures.append("spawn fade rate differs from AnimationBase.UpdateFade")
    for key in ("source", "shader"):
        record = fade.get(key, {})
        source_path = PROJECT_ROOT / record.get("path", "")
        if not source_path.is_file() or sha256(source_path) != record.get("sha256"):
            failures.append(f"spawn fade {key} missing or changed")

    result = {
        "schema": "rok.unity_world_entity_presentation_verify.v1",
        "failures": failures,
        "formation_levels": sorted(formations),
        "unit_library": sorted(unit_lib),
        "badge_sprites": sorted(badges),
    }
    verify_path = MIGRATION_DIR / "unity_world_entity_presentation_verify.json"
    verify_path.write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        raise SystemExit(1)
    print(
        "Rok Unity world-entity presentation verified: "
        f"formations={len(formations)} units={len(unit_lib)} "
        f"badges={len(badges)}"
    )


if __name__ == "__main__":
    main()
