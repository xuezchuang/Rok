"""Read-only strict cross-check of a current UE city pose readback against Unity source occupancy."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import sys
from pathlib import Path
from rok_data_paths import DATA_ROOT


ROOT = Path(__file__).resolve().parents[1]
CATALOG = DATA_ROOT / "CityConstructionGridSources/catalog.json"
EXPECTED_PROJECT = str(ROOT.resolve())
EXPECTED_WORLD = "/Game/Maps/L_RokUnityGameAppCity.L_RokUnityGameAppCity"
MAP_FILE = ROOT / "Content/Maps/L_RokUnityGameAppCity.umap"
TOLERANCE = 1e-3
TAG_KEYS = ("UnityInitId", "UnityBuildingType", "UnityGridX", "UnityGridY",
            "UnityGridWidth", "UnityGridLength")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fail(errors: list[str], message: str) -> None:
    errors.append(message)


def parse_tags(actor: dict, errors: list[str]) -> dict[str, int]:
    parsed: dict[str, list[int]] = {key: [] for key in TAG_KEYS}
    for tag in actor.get("tags", []):
        match = re.fullmatch(r"(UnityInitId|UnityBuildingType|UnityGridX|UnityGridY|UnityGridWidth|UnityGridLength)_(-?\d+)", str(tag))
        if match:
            parsed[match.group(1)].append(int(match.group(2)))
    result = {}
    for key, values in parsed.items():
        if len(values) != 1:
            fail(errors, f"{actor.get('label', actor.get('name'))}: expected exactly one {key}_* tag, got {values}")
        else:
            result[key] = values[0]
    return result


def close_vec(actual, expected, tolerance=TOLERANCE) -> bool:
    return (isinstance(actual, list) and len(actual) == 3 and
            all(math.isfinite(float(a)) and abs(float(a) - float(e)) <= tolerance
                for a, e in zip(actual, expected)))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--readback", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    readback_path = args.readback.resolve()
    out_path = args.out.resolve()
    errors: list[str] = []

    try:
        readback = json.loads(readback_path.read_text(encoding="utf-8-sig"))
        catalog = json.loads(CATALOG.read_text(encoding="utf-8-sig"))
    except Exception as exc:
        print(f"ERROR: cannot read input: {exc}", file=sys.stderr)
        return 2

    if readback.get("project", "").replace("/", "\\").rstrip("\\").casefold() != EXPECTED_PROJECT.replace("/", "\\").rstrip("\\").casefold():
        fail(errors, f"project mismatch: {readback.get('project')!r}, expected {EXPECTED_PROJECT!r}")
    if readback.get("world") != EXPECTED_WORLD:
        fail(errors, f"world mismatch: {readback.get('world')!r}, expected {EXPECTED_WORLD!r}")
    if readback.get("map_saved") is not False:
        fail(errors, f"readback map_saved must be false, got {readback.get('map_saved')!r}")
    if not MAP_FILE.is_file():
        fail(errors, f"current map file missing: {MAP_FILE}")
        current_map_sha = None
    else:
        current_map_sha = sha256(MAP_FILE)
        if readback.get("map_sha256") != current_map_sha:
            fail(errors, f"current map SHA mismatch: readback={readback.get('map_sha256')!r}, current={current_map_sha}")

    occupancy = catalog.get("initial_occupancy")
    if not isinstance(occupancy, list):
        fail(errors, "catalog initial_occupancy missing or not a list")
        occupancy = []
    source_rows = {int(row["init_id"]): row for row in occupancy
                   if int(row.get("width", 0)) > 0 and int(row.get("length", 0)) > 0}
    actors = readback.get("source_actors")
    if not isinstance(actors, list):
        fail(errors, "readback source_actors missing or not a list")
        actors = []
    actor_rows: dict[int, tuple[dict, dict[str, int]]] = {}
    for actor in actors:
        tags = parse_tags(actor, errors)
        if len(tags) != len(TAG_KEYS):
            continue
        init_id = tags["UnityInitId"]
        if init_id in actor_rows:
            fail(errors, f"duplicate actor UnityInitId={init_id}")
            continue
        actor_rows[init_id] = (actor, tags)

    if set(actor_rows) != set(source_rows):
        fail(errors, f"source actor/init occupancy ID mismatch: actors-only={sorted(set(actor_rows)-set(source_rows))}, occupancy-only={sorted(set(source_rows)-set(actor_rows))}")

    checked = []
    for init_id in sorted(set(actor_rows) & set(source_rows)):
        actor, tags = actor_rows[init_id]
        source = source_rows[init_id]
        expected_fields = {
            "UnityBuildingType": int(source["type"]),
            "UnityGridX": int(source["pos_x"]),
            "UnityGridY": int(source["pos_y"]),
            "UnityGridWidth": int(source["width"]),
            "UnityGridLength": int(source["length"]),
        }
        for key, value in expected_fields.items():
            if tags[key] != value:
                fail(errors, f"InitId {init_id}: {key} tag {tags[key]} != source {value}")
        if actor.get("gameplay_tile") != [int(source["pos_x"]), int(source["pos_y"])]:
            fail(errors, f"InitId {init_id}: gameplay_tile {actor.get('gameplay_tile')} != source position [{source['pos_x']},{source['pos_y']}]")
        if actor.get("footprint") != [int(source["width"]), int(source["length"])]:
            fail(errors, f"InitId {init_id}: footprint {actor.get('footprint')} != source [{source['width']},{source['length']}]")

        # Keep the source steps explicit: source tile conversion, then container yaw, then Unity->UE axes/scale.
        gx, gy = int(source["pos_x"]), int(source["pos_y"])
        width, length = int(source["width"]), int(source["length"])
        local_x = gx * 0.1 + width * 0.05 - 0.05
        local_y = 0.01
        local_z = gy * 0.1 + length * 0.05 - 0.05
        yaw = math.radians(-45.0)
        rotated_x = math.cos(yaw) * local_x + math.sin(yaw) * local_z
        rotated_z = -math.sin(yaw) * local_x + math.cos(yaw) * local_z
        expected_anchor = [-rotated_x * 500.0, rotated_z * 500.0, local_y * 500.0]
        actual_anchor = actor.get("anchor")
        error = (math.dist([float(x) for x in actual_anchor], expected_anchor)
                 if isinstance(actual_anchor, list) and len(actual_anchor) == 3 else math.inf)
        if not math.isfinite(error) or error > TOLERANCE:
            fail(errors, f"InitId {init_id}: anchor {actual_anchor} != formula {expected_anchor} (error={error})")

        lod = float(actor.get("lod_scale", math.nan))
        base_offset = actor.get("base_visual_center_offset")
        visual_center = actor.get("visual_center")
        expected_center = ([float(actual_anchor[i]) + float(base_offset[i]) * lod for i in range(3)]
                           if isinstance(actual_anchor, list) and len(actual_anchor) == 3 and
                           isinstance(base_offset, list) and len(base_offset) == 3 and math.isfinite(lod)
                           else None)
        if expected_center is None or not close_vec(visual_center, expected_center):
            fail(errors, f"InitId {init_id}: visual_center {visual_center} != anchor + base_visual_center_offset * lod ({expected_center})")
        components = actor.get("components", [])
        comp_centers = [c.get("world_transform", {}).get("location") for c in components
                        if c.get("name") == "CitySpritePlane"]
        if len(comp_centers) != 1 or not close_vec(comp_centers[0], visual_center):
            fail(errors, f"InitId {init_id}: expected one CitySpritePlane world location matching visual_center, got {comp_centers}")
        checked.append({"init_id": init_id, "type": int(source["type"]),
                        "tile": [gx, gy], "footprint": [width, length],
                        "source_local": [local_x, local_y, local_z],
                        "expected_anchor_ue": expected_anchor, "actual_anchor_ue": actual_anchor,
                        "anchor_error_ue_units": error, "lod_scale": lod,
                        "visual_center_ue": visual_center,
                        "base_visual_center_offset_ue": base_offset,
                        "component_world_centers": comp_centers})

    script_sha = sha256(Path(__file__).resolve())
    catalog_sha = sha256(CATALOG)
    source_hashes = catalog.get("sources", {})
    source_hash_checks = []
    for rel, expected_sha in sorted(source_hashes.items()):
        path = ROOT / Path(rel)
        actual_sha = sha256(path) if path.is_file() else None
        source_hash_checks.append({"path": rel, "expected_sha256": expected_sha,
                                   "actual_sha256": actual_sha, "passed": actual_sha == expected_sha})
        if actual_sha != expected_sha:
            fail(errors, f"catalog source hash mismatch: {rel}; expected {expected_sha}, actual {actual_sha}")

    report = {
        "schema": "rok.current_city_source_pose_verification.v1",
        "passed": not errors,
        "project_expected": EXPECTED_PROJECT,
        "project_readback": readback.get("project"),
        "world_expected": EXPECTED_WORLD,
        "world_readback": readback.get("world"),
        "readback_path": str(readback_path),
        "readback_sha256": sha256(readback_path),
        "readback_map_sha256": readback.get("map_sha256"),
        "current_map_path": str(MAP_FILE),
        "current_map_sha256": current_map_sha,
        "map_saved": readback.get("map_saved"),
        "catalog_path": str(CATALOG),
        "catalog_sha256": catalog_sha,
        "catalog_source_hashes": source_hash_checks,
        "verifier_path": str(Path(__file__).resolve()),
        "verifier_sha256": script_sha,
        "source_formula_steps": {
            "local": "local_x=0.1*tile_x + 0.05*width - 0.05; local_y=0.01; local_z=0.1*tile_y + 0.05*length - 0.05",
            "container_yaw_degrees": -45.0,
            "unity_to_ue": "UE=(-rotated_x, rotated_z, local_y)*500",
        },
        "tolerance_ue_units": TOLERANCE,
        "source_occupied_actor_count": len(source_rows),
        "readback_source_actor_count": len(actors),
        "matched_actor_count": len(checked),
        "max_anchor_error_ue_units": max((x["anchor_error_ue_units"] for x in checked), default=None),
        "actors": checked,
        "failures": errors,
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({"passed": report["passed"], "matched_actor_count": len(checked),
                      "max_anchor_error_ue_units": report["max_anchor_error_ue_units"],
                      "failure_count": len(errors), "failures": errors,
                      "out": str(out_path)}, ensure_ascii=False, indent=2))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
