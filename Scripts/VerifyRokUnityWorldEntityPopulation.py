import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path

from PIL import Image
from rok_data_paths import DATA_ROOT, label, resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILD_SCRIPT = (
    PROJECT_ROOT / "Scripts/BuildRokUnityWorldEntityPopulation.py"
)
MANIFEST_PATH = (
    DATA_ROOT / "unity_world_entity_population.json"
)
VERIFY_PATH = (
    PROJECT_ROOT
    / "Saved/RokUnityMigration/"
    "unity_world_entity_population_verify.json"
)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def distance(left, right):
    return math.hypot(
        float(left["x"]) - float(right["x"]),
        float(left["y"]) - float(right["y"]),
    )


def main():
    build = subprocess.run(
        [sys.executable, str(BUILD_SCRIPT)],
        cwd=PROJECT_ROOT,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    failures = []
    checks = []

    def check(name, condition, evidence):
        checks.append(
            {
                "name": name,
                "passed": bool(condition),
                "evidence": evidence,
            }
        )
        if not condition:
            failures.append(name)

    check(
        "builder_passed",
        build.returncode == 0,
        {
            "returncode": build.returncode,
            "stdout": build.stdout.strip(),
            "stderr": build.stderr.strip(),
        },
    )
    if build.returncode != 0 or not MANIFEST_PATH.exists():
        payload = {
            "schema": "rok.unity_world_entity_population_verify.v1",
            "passed": False,
            "checks": checks,
            "failures": failures or ["manifest_missing"],
        }
        VERIFY_PATH.parent.mkdir(parents=True, exist_ok=True)
        VERIFY_PATH.write_text(
            json.dumps(payload, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
        return 1

    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    entities = manifest.get("entities", [])
    summary = manifest.get("summary", {})
    region = manifest.get("reference_region", {})
    focus = region.get("focus_unity", {})
    half_size = region.get("half_size_unity", {})
    lod = manifest.get("lod_contract", {})
    sampling = manifest.get("sampling_contract", {})

    check(
        "schema",
        manifest.get("schema")
        == "rok.unity_world_entity_population.v1",
        manifest.get("schema"),
    )
    check(
        "entity_count",
        len(entities) == 10
        and summary.get("entity_count") == len(entities),
        {
            "entities": len(entities),
            "summary": summary.get("entity_count"),
        },
    )
    expected_kind_counts = {
        "food": 2,
        "wood": 2,
        "stone": 1,
        "barbarian": 3,
        "village": 2,
    }
    check(
        "kind_counts",
        summary.get("kind_counts") == expected_kind_counts,
        summary.get("kind_counts"),
    )
    check(
        "lod_thresholds",
        lod.get("close_detail_max_dxf") == 1500.0
        and lod.get("tactical_detail_max_dxf") == 3000.0
        and lod.get("far_icon_max_dxf") == 5400.0,
        lod,
    )

    unique_keys = {
        (entity.get("source_table"), entity.get("source_id"))
        for entity in entities
    }
    check(
        "stable_unique_source_identity",
        len(unique_keys) == len(entities),
        sorted(f"{table}:{source_id}" for table, source_id in unique_keys),
    )

    region_failures = []
    city_overlap = []
    city_exclusion_radius = float(
        sampling.get(
            "player_city_exclusion_radius_unity_units",
            0.0,
        )
    )
    for entity in entities:
        position = entity.get("unity_position", {})
        if (
            abs(float(position.get("x", 0)) - float(focus.get("x", 0)))
            > float(half_size.get("x", 0))
            or abs(
                float(position.get("y", 0)) - float(focus.get("y", 0))
            )
            > float(half_size.get("y", 0))
        ):
            region_failures.append(entity.get("source_id"))
        if distance(position, focus) < city_exclusion_radius:
            city_overlap.append(entity.get("source_id"))
    check(
        "positions_in_reference_region",
        not region_failures,
        region_failures,
    )
    check(
        "player_city_exclusion",
        not city_overlap,
        {
            "radius": city_exclusion_radius,
            "overlaps": city_overlap,
        },
    )

    preview_window = sampling.get(
        "reference_dxf_2479_visible_window_unity",
        {},
    )
    preview_entity_ids = []
    for entity in entities:
        position = entity.get("unity_position", {})
        offset_x = float(position.get("x", 0)) - float(focus.get("x", 0))
        offset_y = float(position.get("y", 0)) - float(focus.get("y", 0))
        if (
            float(preview_window.get("min_offset_x", 0)) <= offset_x
            <= float(preview_window.get("max_offset_x", 0))
            and float(preview_window.get("min_offset_y", 0)) <= offset_y
            <= float(preview_window.get("max_offset_y", 0))
        ):
            preview_entity_ids.append(entity.get("source_id"))
    check(
        "reference_dxf_2479_population",
        len(preview_entity_ids)
        >= int(preview_window.get("minimum_entity_count", 0)),
        {
            "visible_entity_ids": preview_entity_ids,
            "minimum": preview_window.get("minimum_entity_count"),
            "window": preview_window,
        },
    )

    visual_failures = []
    alpha_failures = []
    for entity in entities:
        for field in ("detail_visual", "far_visual"):
            visual = entity.get(field, {})
            path = resolve(visual.get("path", ""))
            if (
                not path.exists()
                or sha256(path) != visual.get("sha256")
            ):
                visual_failures.append(
                    f"{entity.get('source_id')}:{field}"
                )
                continue
            with Image.open(path) as image:
                rgba = image.convert("RGBA")
                alpha_min, alpha_max = rgba.getchannel("A").getextrema()
                if (
                    rgba.width != visual.get("width")
                    or rgba.height != visual.get("height")
                    or alpha_min != 0
                    or alpha_max < 240
                ):
                    alpha_failures.append(
                        {
                            "source_id": entity.get("source_id"),
                            "field": field,
                            "size": [rgba.width, rgba.height],
                            "alpha": [alpha_min, alpha_max],
                        }
                    )
    check(
        "runtime_visual_hashes",
        not visual_failures,
        visual_failures,
    )
    check(
        "runtime_visual_alpha",
        not alpha_failures,
        alpha_failures,
    )
    icon_failures = []
    library = manifest.get("visual_library", {})
    for entity in entities:
        far = entity["far_visual"]
        source = library.get(entity["visual_key"], {}).get("far", {})
        # Per-entity images must not bake text or resize the original icon.
        if far.get("sha256") != source.get("output", {}).get("sha256"):
            icon_failures.append(f"{entity['source_id']}: baked or changed far icon")
        if entity["kind"] == "barbarian" and not 1800 < far["world_height_cm"] < 2400:
            icon_failures.append(f"{entity['source_id']}: icon no longer uses source rect/PPU/scale")
    check("far_icon_source_size_and_separate_nameplate", not icon_failures, icon_failures)

    city_lod3 = manifest.get("player_city_lod3_visual", {})
    city_lod3_output = city_lod3.get("output", {})
    city_lod3_path = resolve(city_lod3_output.get("path", ""))
    city_lod3_failures = []
    if (city_lod3.get("pixels_per_unit") != 100.0
            or city_lod3.get("pivot") != [0.5, 0.5]
            or city_lod3.get("world_height_cm") != 2400.0
            or city_lod3.get("alpha_mode") != "source_translucent_no_dither_no_crop"):
        city_lod3_failures.append("source_size_pivot_or_alpha_contract")
    if city_lod3.get("own_city_tint") != [0.266666, 0.831372, 0.509803]:
        city_lod3_failures.append("missing_RS_green_own_city_tint")
    if (
        not city_lod3_path.exists()
        or sha256(city_lod3_path) != city_lod3_output.get("sha256")
    ):
        city_lod3_failures.append("runtime_output")
    else:
        with Image.open(city_lod3_path) as image:
            if image.size != (188, 80):
                city_lod3_failures.append("source_rect_was_cropped")
            rgba = image.convert("RGBA")
            atlas_path = resolve(city_lod3["source_atlas"]["path"])
            rect = city_lod3["source_sprite"]["rect"]
            with Image.open(atlas_path) as atlas:
                top = atlas.height - rect["y"] - rect["height"]
                source_alpha = atlas.convert("RGBA").crop((rect["x"], top,
                    rect["x"] + rect["width"], top + rect["height"])).getchannel("A")
                if rgba.getchannel("A").tobytes() != source_alpha.tobytes():
                    city_lod3_failures.append("source_alpha_changed")
            alpha_min, alpha_max = rgba.getchannel("A").getextrema()
            if alpha_min != 0 or alpha_max < 240:
                city_lod3_failures.append(
                    {
                        "alpha": [alpha_min, alpha_max],
                        "size": list(rgba.size),
                    }
                )
    if (
        city_lod3.get("source_sprite", {}).get("name")
        != "img_map_1014"
        or city_lod3.get("visible_min_dxf") != 3000.0
        or city_lod3.get("visible_max_dxf") != 5400.0
    ):
        city_lod3_failures.append("source_contract")
    check(
        "player_city_lod3_visual",
        not city_lod3_failures,
        city_lod3_failures,
    )

    source_failures = []
    for visual in manifest.get("visual_library", {}).values():
        records = [
            visual.get("detail", {}).get("source"),
            visual.get("far", {}).get("source_atlas"),
            visual.get("far", {}).get("source_meta"),
            visual.get("source_lod_prefab"),
        ]
        optional_records = [
            visual.get("detail", {}).get("source_mask"),
            visual.get("detail", {}).get("source_shader"),
        ]
        for record in records + [
            value for value in optional_records if value
        ]:
            if not record:
                source_failures.append(
                    f"{visual.get('key')}:missing_source_record"
                )
                continue
            path = resolve(record.get("path", ""))
            if (
                not path.exists()
                or path.stat().st_size != record.get("size")
                or sha256(path) != record.get("sha256")
            ):
                source_failures.append(record.get("path"))
    for record in (
        city_lod3.get("source_atlas"),
        city_lod3.get("source_meta"),
        city_lod3.get("source_lod_prefab"),
    ):
        if not record:
            source_failures.append("player_city_lod3:missing_source_record")
            continue
        path = resolve(record.get("path", ""))
        if (
            not path.exists()
            or path.stat().st_size != record.get("size")
            or sha256(path) != record.get("sha256")
        ):
            source_failures.append(record.get("path"))
    check(
        "unity_source_lineage",
        not source_failures,
        source_failures,
    )

    payload = {
        "schema": "rok.unity_world_entity_population_verify.v1",
        "passed": not failures,
        "manifest": {
            "path": label(MANIFEST_PATH),
            "sha256": sha256(MANIFEST_PATH),
        },
        "checks": checks,
        "failures": failures,
    }
    VERIFY_PATH.write_text(
        json.dumps(payload, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    print(
        "Rok world entity population verification: "
        f"passed={payload['passed']} "
        f"checks={len(checks)} failures={len(failures)}"
    )
    return 0 if payload["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
