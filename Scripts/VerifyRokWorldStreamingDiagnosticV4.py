"""Offline, fail-closed checks for the isolated Unity world-stream diagnostic.

No Unreal import or editor mutation. The optional runtime check consumes actual
HISM counters emitted by RokWorldStreamingDiagnosticGameMode, not JSON estimates.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import re
import struct
import tempfile
import zlib
from collections import defaultdict
from pathlib import Path


ROOT = Path("D:/ueDemo/Rok")
SOURCE = ROOT / "Saved/RokUnityMigration/unity_world_map_streaming_data.json"
ASSETS = ROOT / "Saved/RokUnityMigration/unity_world_map_streaming_assets.json"
SOURCE_MAP = ROOT / "Content/Maps/L_RokUnityWorldMapPrototype.umap"
INVENTORY = ROOT / "Saved/PendingUnityWorldStreamingDiagnosticV4/dependency_inventory.json"
INVENTORY_SHA = "32757f8cd41571dccdf53192df84407f72da3a3b361ef2844ddb863d3b63424d"
DATA_ASSET = ROOT / "Content/RokDiagnostics/WorldStreaming/V1/DA_RokWorldStreamSourceLod01.uasset"
DIAG_MAP = ROOT / "Content/Maps/L_RokWorldStreamingDiagnosticV1.umap"
PINS = {
    SOURCE: "72dd593850c582b4358a1f86f347391ea135f6f3edc25a50da7222daac2c3ba2",
    ASSETS: "29b23d360f0f8622231e3cf01a4a591cea6f126ac200dfb867abef327066b657",
    SOURCE_MAP: "2f7b32d4506d5fd1cfeca8b8c9a36440efe988a51646bc701dcb46915ab27a86",
}
PAYLOAD_SHA = "09298c671a05b3c220e24b25c7c7362c4d7f0cff1218a29d7c305e7471f86b6f"
MANIFEST_SHA = "fdce978db9b7d6be4c1fcb2da3a24f30bb49835acc16698fc54ccbfa747b7ad0"
WORLD = "/Game/Maps/L_RokWorldStreamingDiagnosticV1"
ASSET_PATH = "/Game/RokDiagnostics/WorldStreaming/V1/DA_RokWorldStreamSourceLod01.DA_RokWorldStreamSourceLod01"
PHASES = ["lod0_initial_edge", "lod0_moved_west_three_chunks", "lod1_moved",
          "lod2_refused_unloaded", "lod1_restored_after_refusal"]
EXPECTED = [(0, 19, 2, 848), (0, 16, 2, 1688), (1, 16, 2, 1688),
            (-1, 16, 2, 0), (1, 16, 2, 1688)]
VISUAL_FIELDS = ("rok_local_only_flag", "rok_world_streaming_diagnostic_flag",
                 "server_subsystem_present", "local_only", "http_dispatch_attempts",
                 "http_suppressed_requests", "lod", "center_chunk_x", "center_chunk_y",
                 "generation", "hism_component_count", "hism_visible_component_count",
                 "hism_instance_count", "registered_hism_component_count",
                 "registered_hism_instance_count", "registered_matches_active",
                 "pending_load", "has_requested_state", "camera_dxf", "camera_unity_x",
                 "camera_unity_y", "hism_component_names",
                 "registered_hism_component_names", "actual_hism_components")


def require(value: bool, message: str) -> None:
    if not value:
        raise AssertionError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def valid_png(path: Path) -> tuple[int, int, int, str]:
    data = path.read_bytes()
    require(data[:8] == b"\x89PNG\r\n\x1a\n" and len(data) > 40, "missing PNG signature")
    offset, image_data, extent, ended = 8, bytearray(), None, False
    while offset + 12 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        require(length < 100_000_000 and offset + length + 12 <= len(data), "truncated PNG chunk")
        kind = data[offset + 4:offset + 8]
        chunk = data[offset + 8:offset + 8 + length]
        crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
        require(zlib.crc32(kind + chunk) & 0xFFFFFFFF == crc, "PNG chunk CRC changed")
        if kind == b"IHDR":
            require(extent is None and length == 13, "PNG IHDR invalid")
            width, height, depth, mode, compression, filtering, interlace = struct.unpack(">IIBBBBB", chunk)
            require(width == 1440 and height == 810 and depth == 8 and mode in (2, 6)
                    and compression == filtering == interlace == 0, "PNG is not 1440x810 RGB/RGBA")
            extent = (width, height, 3 if mode == 2 else 4)
        elif kind == b"IDAT":
            image_data.extend(chunk)
        elif kind == b"IEND":
            require(length == 0, "PNG IEND invalid")
            ended = True
            offset += 12
            break
        offset += length + 12
    require(ended and offset == len(data) and extent is not None and image_data,
            "PNG missing image data or trailing bytes")
    width, height, channels = extent
    raw = zlib.decompress(image_data)
    stride = width * channels + 1
    require(len(raw) == stride * height
            and all(raw[y * stride] < 5 for y in range(height)), "PNG raster/filter invalid")
    return width, height, len(data), hashlib.sha256(data).hexdigest()


def synthetic_png_for_negative_test(gray: int) -> bytes:
    def chunk(kind: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + kind + payload
                + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))
    row = b"\x00" + bytes((gray, gray, gray, 255)) * 1440
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", 1440, 810, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(row * 810, 1))
            + chunk(b"IEND", b""))


def package_file(path: str) -> Path:
    return ROOT / "Content" / (path.split(".", 1)[0].removeprefix("/Game/") + ".uasset")


def budget(data: dict, lod: int, center: tuple[int, int]) -> tuple[int, int]:
    span = data["LodLevels"][lod]["ChunkTileSpan"]
    keys = {(center[0] + dx, center[1] + dy)
            for dx in (-1, 0, 1) for dy in (-1, 0, 1)}
    bindings = {row["TileTypeId"]: {int(b["Lod"]): b["RecipeId"]
                                    for b in row["LodBindings"]}
                for row in data["TileTypes"]}
    recipes = {row["RecipeId"]: row for row in data["Recipes"]}
    nonempty: set[tuple[int, int]] = set()
    parts = 0
    for tile in data["Tiles"]:
        chunk = (tile["TileX"] // span, tile["TileY"] // span)
        if chunk not in keys:
            continue
        nonempty.add(chunk)
        rid = bindings[tile["TileTypeId"]][lod]
        parts += sum(bool(part["Mesh"]) for part in recipes[rid]["Parts"])
    return len(nonempty), parts


def transform_values(row: dict) -> list[float]:
    p, q, s = row["Translation"], row["Rotation"], row["Scale3D"]
    return [p[k] for k in "XYZ"] + [q[k] for k in "XYZW"] + [s[k] for k in "XYZ"]


def normalized_source_data(data: dict) -> dict:
    projection = data["Projection"]
    vector = lambda name: [projection[name]["X"], projection[name]["Y"]]
    return {
        "schema": data["Schema"],
        "canonical_payload_sha256": data["CanonicalPayloadSha256"],
        "source_manifest_path": data["SourceManifestPath"],
        "source_manifest_sha256": data["SourceManifestSha256"],
        "map_width_tiles": data["MapWidthTiles"],
        "map_height_tiles": data["MapHeightTiles"],
        "tile_size_unity_units": data["TileSizeUnityUnits"],
        "static_visual_ready": False, "runtime_ready": False,
        "projection": {"unity_bounds_min": vector("UnityBoundsMin"),
                       "unity_bounds_max": vector("UnityBoundsMax"),
                       "unity_rebase_origin": vector("UnityRebaseOrigin"),
                       "ue_units_per_unity_unit": projection["UeUnitsPerUnityUnit"],
                       "ground_z": projection["GroundZ"],
                       "display_coordinate_scale": projection["DisplayCoordinateScale"]},
        "lod_levels": [[x["Lod"], x["MaxDxfInclusive"], x["ChunkTileSpan"]]
                       for x in data["LodLevels"]],
        "tiles": [[x["TileX"], x["TileY"], x["TileTypeId"],
                   transform_values(x["TemplateToWorld"])] for x in data["Tiles"]],
        "tile_types": [[x["TileTypeId"], [[b["Lod"], b["RecipeId"]]
                                          for b in x["LodBindings"]]]
                       for x in data["TileTypes"]],
        "recipes": [[x["RecipeId"], x["TileTypeId"], x["Lod"],
                     [[p["Mesh"], p["Materials"], transform_values(p["LocalTransform"]),
                       p["TranslucencySortPriority"], p["bTree"], p["RenderFlags"]]
                      for p in x["Parts"]]] for x in data["Recipes"]],
        "readiness_blockers": data["ReadinessBlockers"]
                              + ["diagnostic_lod01_only_lod2plus_assets_not_promoted"],
        "pending_policies": [[x["PolicyId"], x["Status"], x["SourceComponentCount"],
                              x["RequiredDecision"]] for x in data["PendingPolicies"]],
    }


def compare_source_records(actual, expected, path="runtime_data_snapshot") -> None:
    if isinstance(expected, (float, int)) and not isinstance(expected, bool):
        require(isinstance(actual, (float, int)) and not isinstance(actual, bool)
                and math.isfinite(actual) and math.isclose(actual, expected, rel_tol=1e-7,
                                                          abs_tol=0.002),
                f"{path}: numeric drift")
        return
    require(type(actual) is type(expected), f"{path}: type differs")
    if isinstance(expected, dict):
        require(actual.keys() == expected.keys(), f"{path}: field set differs")
        for key in expected:
            compare_source_records(actual[key], expected[key], f"{path}.{key}")
    elif isinstance(expected, list):
        require(len(actual) == len(expected), f"{path}: length differs")
        for index, (left, right) in enumerate(zip(actual, expected)):
            compare_source_records(left, right, f"{path}[{index}]")
    else:
        require(actual == expected, f"{path}: source record drift")


def quaternion_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return [aw*bx + ax*bw + ay*bz - az*by,
            aw*by - ax*bz + ay*bw + az*bx,
            aw*bz + ax*by - ay*bx + az*bw,
            aw*bw - ax*bx - ay*by - az*bz]


def rotate(q, v):
    x, y, z, w = q
    # Source quaternions are unit-length. This is the independent q*v*q^-1 form.
    uv = [y*v[2] - z*v[1], z*v[0] - x*v[2], x*v[1] - y*v[0]]
    uuv = [y*uv[2] - z*uv[1], z*uv[0] - x*uv[2], x*uv[1] - y*uv[0]]
    return [v[i] + 2*w*uv[i] + 2*uuv[i] for i in range(3)]


def tree_scale(dxf):
    keys = [(80.0, .23, -.000033767436, .00027361486),
            (250.0, .4, .0003291402, .0015314083),
            (896.1033, .8, .0006190961, .00044234627),
            (1800.3723, 1.2, .0007099115, 0.0)]
    if dxf <= keys[0][0]:
        return keys[0][1]
    for left, right in zip(keys, keys[1:]):
        if dxf > right[0]:
            continue
        duration = right[0] - left[0]
        t = (dxf-left[0])/duration
        t2, t3 = t*t, t*t*t
        return ((2*t3-3*t2+1)*left[1] + (t3-2*t2+t)*duration*left[3]
                + (-2*t3+3*t2)*right[1] + (t3-t2)*duration*right[2])
    return keys[-1][1]


def compose_part_tile(part: list[float], tile: list[float], scale_tree: float) -> list[float]:
    p, q, s = part[:3], part[3:7], part[7:]
    tp, tq, ts = tile[:3], tile[3:7], tile[7:]
    local = [ts[i]*p[i] for i in range(3)]
    offset = rotate(tq, local)
    out_q = quaternion_mul(tq, q)
    length = math.sqrt(sum(v*v for v in out_q))
    out_q = [v/length for v in out_q]
    return [tp[i]+offset[i] for i in range(3)] + out_q + [ts[i]*s[i]*scale_tree
                                                        for i in range(3)]


def expected_batches(data: dict, lod: int, center: tuple[int, int], dxf: float) -> dict:
    span = data["LodLevels"][lod]["ChunkTileSpan"]
    keys = {(center[0]+dx, center[1]+dy)
            for dx in (-1, 0, 1) for dy in (-1, 0, 1)}
    bindings = {x["TileTypeId"]: {b["Lod"]: b["RecipeId"] for b in x["LodBindings"]}
                for x in data["TileTypes"]}
    recipes = {x["RecipeId"]: x for x in data["Recipes"]}
    groups = defaultdict(lambda: {"component_instances": defaultdict(list), "instances": []})
    for tile in data["Tiles"]:
        chunk = (tile["TileX"]//span, tile["TileY"]//span)
        if chunk not in keys:
            continue
        recipe = recipes[bindings[tile["TileTypeId"]][lod]]
        for part in recipe["Parts"]:
            if not part["Mesh"]:
                continue
            broad = (chunk, part["Mesh"], tuple(part["Materials"]),
                     part["TranslucencySortPriority"])
            full = broad + (part["bTree"], part["RenderFlags"])
            group = groups[broad]
            transform = compose_part_tile(
                transform_values(part["LocalTransform"]),
                transform_values(tile["TemplateToWorld"]),
                tree_scale(dxf) if part["bTree"] else 1.0)
            group["component_instances"][full].append(transform)
            group["instances"].append(transform)
    return groups


def verify_source() -> dict:
    for path, expected in PINS.items():
        require(path.is_file() and sha(path) == expected, f"input drift: {path}")
    data = json.loads(SOURCE.read_text(encoding="utf-8"))
    require(data["Schema"] == "rok.world_map_runtime_data_asset.v1", "schema drift")
    require(data["CanonicalPayloadSha256"] == PAYLOAD_SHA
            and data["SourceManifestSha256"] == MANIFEST_SHA, "source identity drift")
    require(data["bStaticVisualReady"] is True and data["bRuntimeReady"] is False,
            "source readiness changed; re-audit diagnostic exception")
    require((len(data["Tiles"]), len(data["TileTypes"]), len(data["Recipes"]))
            == (1600, 66, 396), "source record counts changed")
    require([(x["Lod"], x["ChunkTileSpan"]) for x in data["LodLevels"][:2]]
            == [(0, 2), (1, 2)], "LOD0/1 policy drift")
    deps: set[str] = set()
    lod2_deps: set[str] = set()
    for recipe in data["Recipes"]:
        if recipe["Lod"] not in (0, 1, 2):
            continue
        target = deps if recipe["Lod"] < 2 else lod2_deps
        for part in recipe["Parts"]:
            if part["Mesh"]:
                target.add(part["Mesh"])
            target.update(x for x in part["Materials"] if x)
    require(len(deps) == 243, "LOD0/1 dependency count drift")
    require(INVENTORY.is_file() and sha(INVENTORY) == INVENTORY_SHA,
            "reviewed dependency inventory drift")
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    require({row["object_path"] for row in inventory["lod01"]} == deps
            and len(inventory["lod01"]) == 243, "reviewed dependency set drift")
    for row in inventory["lod01"]:
        path = package_file(row["object_path"])
        require(path.is_file() and sha(path) == row["uasset_sha256"],
                f"LOD0/1 dependency missing or changed: {row['object_path']}")
    missing2 = sorted(path for path in lod2_deps if not package_file(path).is_file())
    require(len(missing2) == 249 and missing2 == inventory["missing_lod2"],
            "LOD2 missing dependency set differs from reviewed 249 packages")
    computed = {"lod0_initial": budget(data, 0, (19, 2)),
                "lod0_moved": budget(data, 0, (16, 2)),
                "lod1_moved": budget(data, 1, (16, 2))}
    require(computed == {"lod0_initial": (6, 848), "lod0_moved": (9, 1688),
                         "lod1_moved": (9, 1688)}, f"source budget drift: {computed}")
    return {"schema": "rok.world_streaming_diagnostic_offline_verify.v4",
            "source_sha256": sha(SOURCE), "payload_sha256": PAYLOAD_SHA,
            "lod01_dependencies": len(deps), "lod2_missing_dependencies": len(missing2),
            "budgets_nonempty_chunks_instances": computed,
            "static_visual_ready_for_full_world": False,
            "runtime_ready_for_full_world": False}


def verify_created(path: Path) -> dict:
    report = json.loads(path.read_text(encoding="utf-8"))
    require(report["schema"] == "rok.world_streaming_diagnostic_editor_create.v4"
            and report["status"] == "created_pending_runtime_acceptance", "creation schema/status")
    require(report["creation_rok_local_only_flag"] is True
            and report["runtime_required_args"] == ["-RokWorldStreamingDiagnostic", "-RokLocalOnly"],
            "creation report missing local-only command boundary")
    require(report["source_json_sha256"] == PINS[SOURCE]
            and report["source_map_sha256_before"] == PINS[SOURCE_MAP], "creation source pins")
    require(report["runtime_data_asset"] == ASSET_PATH.split(".", 1)[0]
            and report["diagnostic_map"] == WORLD, "creation target identity")
    require(report["static_visual_ready"] is False
            and report["runtime_ready"] is False, "incorrect promotion")
    require(DATA_ASSET.is_file() and DIAG_MAP.is_file(), "created packages missing")
    require(report["dependency_inventory_sha256"] == INVENTORY_SHA
            and report["lod2_missing_dependency_count"] == 249,
            "created report dependency identity")
    require(report["data_asset_uasset_sha256"] == sha(DATA_ASSET)
            and report["diagnostic_map_umap_sha256"] == sha(DIAG_MAP),
            "created package bytes changed after recording")
    require(sha(SOURCE_MAP) == PINS[SOURCE_MAP], "protected prototype map changed")
    return {"editor_create_report_sha256": sha(path), "data_asset_sha256": sha(DATA_ASSET),
            "diagnostic_map_sha256": sha(DIAG_MAP)}


COMPONENT_NAME = re.compile(r"^WorldMapChunk_(-?\d+)_(-?\d+)_Batch_\d+_Gen_\d+(?:_\d+)?$")
COMPONENT_RELATIVE = [0, 0, 0, 0, 0, 0, 1, -1, 1, 1]


def compare_instance(actual, expected, context):
    require(len(actual) == len(expected) == 10
            and all(isinstance(v, (int, float)) and math.isfinite(v) for v in actual),
            f"{context}: invalid transform")
    require(all(abs(actual[i]-expected[i]) <= .6 for i in range(3)),
            f"{context}: source position mismatch")
    q_same = max(abs(actual[i]-expected[i]) for i in range(3, 7))
    q_negated = max(abs(actual[i]+expected[i]) for i in range(3, 7))
    require(min(q_same, q_negated) <= .001, f"{context}: source orientation mismatch")
    require(all(abs(actual[i]-expected[i]) <= .005 for i in range(7, 10)),
            f"{context}: source scale mismatch")


def verify_actual_geometry(sample, source, index):
    names = sample["hism_component_names"]
    registered = sample["registered_hism_component_names"]
    actual = sample["actual_hism_components"]
    require(sample["registered_matches_active"] is True
            and sorted(names) == sorted(registered),
            f"phase {index}: ActiveComponents does not cover all registered HISM")
    require(sample["registered_hism_component_count"] == len(registered) == len(actual)
            and sample["registered_hism_instance_count"] == sample["hism_instance_count"],
            f"phase {index}: actual registered HISM counts mismatch")
    require(sorted(x["name"] for x in actual) == sorted(registered),
            f"phase {index}: actual component snapshot differs from registry")
    if index == 3:
        require(not actual and not names and not registered,
                "LOD2 refusal left registered or active HISM components")
        return
    lod, cx, cy, _ = EXPECTED[index]
    expected_groups = expected_batches(source, lod, (cx, cy), sample["camera_dxf"])
    actual_groups = defaultdict(list)
    for component in actual:
        name = component["name"]
        match = COMPONENT_NAME.fullmatch(name)
        require(match is not None, f"phase {index}: HISM name has no source chunk identity")
        chunk = (int(match[1]), int(match[2]))
        materials = component["materials"]
        key = (chunk, component["mesh"], tuple(materials),
               component["translucency_sort_priority"])
        require(component["visible"] is True
                and component["instance_transforms_read_all"] is True
                and component["instances_local"],
                f"phase {index}: HISM hidden, unreadable or empty")
        compare_instance(component["relative_transform"], COMPONENT_RELATIVE,
                         f"phase {index} {name} component-local mirror")
        actual_groups[key].append(component)
    require(actual_groups.keys() == expected_groups.keys(),
            f"phase {index}: source chunk/mesh/material/sort batch keys differ")
    for key, expected in expected_groups.items():
        components = actual_groups[key]
        require(len(components) == len(expected["component_instances"]),
                f"phase {index}: batch count differs for {key}")
        source_instances = expected["instances"]
        actual_instances = [transform for component in components
                            for transform in component["instances_local"]]
        require(len(source_instances) == len(actual_instances),
                f"phase {index}: instance count differs for {key}")
        sorter = lambda t: tuple(round(x, 1) for x in t[:3]) + tuple(round(x, 3) for x in t[7:])
        for n, (observed, source_transform) in enumerate(zip(
                sorted(actual_instances, key=sorter), sorted(source_instances, key=sorter))):
            compare_instance(observed, source_transform, f"phase {index} {key} instance {n}")


def verify_local_only_boundary(record: dict, where: str) -> None:
    require(record["rok_world_streaming_diagnostic_flag"] is True
            and record["rok_local_only_flag"] is True
            and record["server_subsystem_present"] is True
            and record["local_only"] is True
            and type(record["http_dispatch_attempts"]) in (int, float)
            and type(record["http_suppressed_requests"]) in (int, float)
            and record["http_dispatch_attempts"] == 0
            and record["http_suppressed_requests"] == 0,
            f"{where}: local-only/server HTTP boundary failed")


def verify_runtime(report: dict, source: dict, manifest_path: Path) -> None:
    require(report["schema"] == "rok.world_streaming_diagnostic_runtime.v4"
            and report["passed"] is True, "runtime not passed")
    require(report["screenshot_origin"] == "FScreenshotRequest_game_viewport"
            and report["expected_viewport_width"] == 1440
            and report["expected_viewport_height"] == 810,
            "runtime did not request the actual 1440x810 game viewport")
    require(Path(report["run_directory"]) == manifest_path.parent
            and manifest_path.name == "manifest.json"
            and manifest_path.parent.parent.name == "WorldStreamingDiagnosticRunsV4"
            and re.fullmatch(r"\d{8}_\d{6}_\d+_\d+", manifest_path.parent.name),
            "runtime screenshots are not in one unique named PID run folder")
    verify_local_only_boundary(report["begin_local_boundary"], "BeginPlay")
    verify_local_only_boundary(report, "Finish")
    require(report["world"] == WORLD and report["runtime_data_path"] == ASSET_PATH,
            "runtime world/asset identity")
    require(report["payload_sha256"] == PAYLOAD_SHA, "runtime payload drift")
    require(report["static_visual_ready"] is False
            and report["runtime_ready"] is False, "runtime falsely promoted full world")
    compare_source_records(report["runtime_data_snapshot"], normalized_source_data(source))
    samples = report["samples"]
    require(len(samples) == 5 and [x["phase"] for x in samples] == PHASES,
            "missing or reordered runtime phases")
    generations = []
    frames = []
    observed_frames = []
    screenshot_paths = []
    for index, (sample, expected) in enumerate(zip(samples, EXPECTED)):
        verify_local_only_boundary(sample, f"phase {index}")
        require(sample["stable_observations"] >= 3, f"phase {index}: source state not stable for two later frames")
        request_frame = sample["screenshot_request_frame"]
        processed_frame = sample["screenshot_processed_frame"]
        observed_frame = sample["screenshot_observed_frame"]
        require(sample["game_frame"] == request_frame
                and request_frame <= processed_frame < observed_frame
                and (not observed_frames or request_frame > observed_frames[-1]),
                f"phase {index}: screenshot was not processed then rechecked before next camera move")
        require(sample["viewport_width"] == 1440 and sample["viewport_height"] == 810
                and sample["png_width"] == 1440 and sample["png_height"] == 810,
                f"phase {index}: actual screenshot extent differs from request")
        image_path = Path(sample["screenshot_path"])
        require(image_path.is_absolute() and image_path.parent == manifest_path.parent
                and image_path.name == PHASES[index] + ".png",
                f"phase {index}: screenshot escaped unique run folder or wrong phase name")
        require(image_path.is_file(), f"phase {index}: screenshot file missing")
        width, height, count, digest = valid_png(image_path)
        require((width, height, count, digest) ==
                (sample["png_width"], sample["png_height"], sample["png_bytes"], sample["png_sha256"]),
                f"phase {index}: PNG bytes/extent/SHA mismatch")
        screenshot_paths.append(image_path)
        request_state = {key: sample[key] for key in VISUAL_FIELDS}
        require(sample["processed_state"] == request_state
                and sample["observed_state"] == request_state,
                f"phase {index}: camera, network or actual registered HISM changed during capture")
        observed_frames.append(observed_frame)
        lod, cx, cy, instances = expected
        require((sample["lod"], sample["center_chunk_x"], sample["center_chunk_y"],
                 sample["hism_instance_count"]) == expected,
                f"phase {index}: actual HISM state differs from source budget")
        names = sample["hism_component_names"]
        require(isinstance(names, list) and len(names) == sample["hism_component_count"]
                and len(names) == len(set(names)), f"phase {index}: component names/count")
        require(sample["hism_component_count"] > 0 if instances else
                sample["hism_component_count"] == 0, f"phase {index}: HISM count")
        require(sample["hism_visible_component_count"] == sample["hism_component_count"],
                f"phase {index}: registered HISM is hidden")
        verify_actual_geometry(sample, source, index)
        require(all(name.startswith("WorldMapChunk_") and "Placeholder" not in name
                    for name in names), f"phase {index}: unexpected component name")
        require(sample["pending_load"] is False
                and sample["has_requested_state"] is bool(instances),
                f"phase {index}: unfinished or stale request")
        require(abs(sample["camera_unity_x"] - (6876 if index == 0 else 5796)) < .01
                and abs(sample["camera_unity_y"] - 942) < .01,
                f"phase {index}: focus did not move to source-derived target")
        require((lod == 0 and sample["camera_dxf"] <= 1500)
                or (lod == 1 and 1500 < sample["camera_dxf"] <= 3000)
                or (lod == -1 and 3000 < sample["camera_dxf"] <= 8000),
                f"phase {index}: Dxf/LOD mismatch")
        generations.append(sample["generation"])
        frames.append(sample["game_frame"])
    require(all(b > a for a, b in zip(generations, generations[1:])),
            "streaming generation did not advance for unload/reload")
    require(all(b > a for a, b in zip(frames, frames[1:])),
            "phase samples did not occur in later game frames")
    require(len(set(screenshot_paths)) == 5, "phase screenshots are not five distinct files")
    require(set(samples[0]["hism_component_names"]).isdisjoint(
            samples[1]["hism_component_names"]), "LOD0 previous HISM revision was retained")
    require(set(samples[2]["hism_component_names"]).isdisjoint(
            samples[4]["hism_component_names"]), "LOD1 restored stale HISM revision")


def negative_cases(source: dict) -> int:
    temp = tempfile.TemporaryDirectory(prefix="world_stream_v4_selftest_")
    manifest_path = Path(temp.name) / "WorldStreamingDiagnosticRunsV4" / "20261009_010203_1234_5678" / "manifest.json"
    manifest_path.parent.mkdir(parents=True)
    baseline = {"schema": "rok.world_streaming_diagnostic_runtime.v4", "passed": True,
                "world": WORLD, "runtime_data_path": ASSET_PATH, "payload_sha256": PAYLOAD_SHA,
                "static_visual_ready": False, "runtime_ready": False,
                "screenshot_origin": "FScreenshotRequest_game_viewport",
                "run_directory": str(manifest_path.parent),
                "expected_viewport_width": 1440, "expected_viewport_height": 810,
                "rok_world_streaming_diagnostic_flag": True, "rok_local_only_flag": True,
                "server_subsystem_present": True, "local_only": True,
                "http_dispatch_attempts": 0, "http_suppressed_requests": 0,
                "runtime_data_snapshot": normalized_source_data(source), "samples": []}
    baseline["begin_local_boundary"] = {
        "rok_world_streaming_diagnostic_flag": True, "rok_local_only_flag": True,
        "server_subsystem_present": True, "local_only": True,
        "http_dispatch_attempts": 0, "http_suppressed_requests": 0}
    for i, (phase, expected) in enumerate(zip(PHASES, EXPECTED)):
        lod, x, y, instances = expected
        dxf = [288.316, 288.316, 2000, 3001, 2000][i]
        components = []
        if instances:
            n = 0
            for broad, group in expected_batches(source, lod, (x, y), dxf).items():
                for transforms in group["component_instances"].values():
                    components.append({"name": f"WorldMapChunk_{broad[0][0]}_{broad[0][1]}_Batch_{n}_Gen_{i+1}",
                                       "mesh": broad[1], "materials": list(broad[2]),
                                       "translucency_sort_priority": broad[3],
                                       "visible": True, "relative_transform": COMPONENT_RELATIVE,
                                       "instance_transforms_read_all": True,
                                       "instances_local": transforms})
                    n += 1
        names = sorted(component["name"] for component in components)
        image_path = manifest_path.parent / (phase + ".png")
        image = synthetic_png_for_negative_test(i * 31)
        image_path.write_bytes(image)
        baseline["samples"].append({"phase": phase, "lod": lod, "center_chunk_x": x,
            "center_chunk_y": y, "hism_instance_count": instances,
            "hism_component_count": len(names), "hism_component_names": names,
            "hism_visible_component_count": len(names),
            "registered_hism_component_count": len(names),
            "registered_hism_instance_count": instances,
            "registered_hism_component_names": names,
            "registered_matches_active": True,
            "actual_hism_components": components,
            "pending_load": False, "has_requested_state": bool(instances),
            "camera_unity_x": 6876 if i == 0 else 5796, "camera_unity_y": 942,
            "camera_dxf": dxf,
            "rok_world_streaming_diagnostic_flag": True, "rok_local_only_flag": True,
            "server_subsystem_present": True, "local_only": True,
            "http_dispatch_attempts": 0, "http_suppressed_requests": 0,
            "generation": i + 1, "game_frame": (i+1)*10,
            "stable_observations": 3,
            "screenshot_path": str(image_path),
            "screenshot_request_frame": (i+1)*10,
            "screenshot_processed_frame": (i+1)*10+1,
            "screenshot_observed_frame": (i+1)*10+2,
            "viewport_width": 1440, "viewport_height": 810,
            "png_width": 1440, "png_height": 810,
            "png_bytes": len(image), "png_sha256": hashlib.sha256(image).hexdigest()})
        sample = baseline["samples"][-1]
        state = {key: sample[key] for key in VISUAL_FIELDS}
        sample["processed_state"] = copy.deepcopy(state)
        sample["observed_state"] = copy.deepcopy(state)
    verify_runtime(baseline, source, manifest_path)
    mutations = [lambda x: x.pop("begin_local_boundary"),
                 lambda x: x.update(screenshot_origin="offscreen_capture"),
                 lambda x: x["begin_local_boundary"].update(rok_local_only_flag=False),
                 lambda x: x["begin_local_boundary"].update(http_dispatch_attempts=1),
                 lambda x: x.pop("rok_local_only_flag"),
                 lambda x: x.update(rok_local_only_flag=False),
                 lambda x: x.update(server_subsystem_present=False),
                 lambda x: x.update(local_only=False),
                 lambda x: x.update(http_dispatch_attempts=1),
                 lambda x: x.update(http_suppressed_requests=1),
                 lambda x: x["samples"][2].update(rok_local_only_flag=False),
                 lambda x: x["samples"][1].update(local_only=False),
                 lambda x: x["samples"][3].update(http_dispatch_attempts=1),
                 lambda x: x["samples"][4].update(http_suppressed_requests=1),
                 lambda x: x["samples"][0].update(stable_observations=1),
                 lambda x: x["samples"][0].update(screenshot_processed_frame=9),
                 lambda x: x["samples"][1].update(screenshot_observed_frame=20),
                 lambda x: x["samples"][2].update(png_sha256="0" * 64),
                 lambda x: x["samples"][3].update(png_width=1),
                 lambda x: x["samples"][4].update(screenshot_path="/tmp/offscreen.png"),
                 lambda x: x["samples"][1]["processed_state"].update(camera_unity_x=999),
                 lambda x: x["samples"][2]["observed_state"].update(http_dispatch_attempts=1),
                 lambda x: x.update(world="/Game/Maps/L_RokUnityWorldMapPrototype"),
                 lambda x: x.update(payload_sha256="0" * 64),
                 lambda x: x.update(static_visual_ready=True),
                 lambda x: x["runtime_data_snapshot"]["tiles"][0].__setitem__(2, "wrong_type"),
                 lambda x: x["runtime_data_snapshot"]["recipes"][0][3][0][2].__setitem__(0, 999),
                 lambda x: x["samples"][1].update(hism_instance_count=0),
                 lambda x: x["samples"][3].update(hism_component_count=1),
                 lambda x: x["samples"][3].update(registered_hism_component_count=1),
                 lambda x: x["samples"][0].update(registered_matches_active=False),
                 lambda x: x["samples"][0].update(hism_visible_component_count=0),
                 lambda x: x["samples"][3].update(has_requested_state=True),
                 lambda x: x["samples"][1].update(hism_component_names=x["samples"][0]["hism_component_names"]),
                 lambda x: x["samples"][0]["actual_hism_components"][0].update(mesh="/Game/Wrong.Wrong"),
                 lambda x: x["samples"][0]["actual_hism_components"][0]["materials"].__setitem__(0, "/Game/Wrong.Wrong"),
                 lambda x: x["samples"][0]["actual_hism_components"][0]["instances_local"][0].__setitem__(0, 999),
                 lambda x: x["samples"][2].update(camera_unity_x=6876),
                 lambda x: x["samples"][4].update(game_frame=10)]
    for index, mutate in enumerate(mutations):
        candidate = copy.deepcopy(baseline)
        mutate(candidate)
        try:
            verify_runtime(candidate, source, manifest_path)
        except (AssertionError, KeyError, TypeError):
            continue
        raise AssertionError(f"negative case {index} escaped verifier")
    damaged = manifest_path.parent / (PHASES[0] + ".png")
    original = damaged.read_bytes()
    damaged.write_bytes(original[:45] + bytes((original[45] ^ 1,)) + original[46:])
    try:
        verify_runtime(baseline, source, manifest_path)
    except (AssertionError, zlib.error):
        pass
    else:
        raise AssertionError("changed PNG bytes escaped verifier")
    damaged.write_bytes(original)
    temp.cleanup()
    return len(mutations) + 1


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--created", type=Path, help="editor_create report after root-owned import")
    parser.add_argument("--runtime", type=Path, help="actual -game diagnostic manifest")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    result = verify_source()
    source = json.loads(SOURCE.read_text(encoding="utf-8"))
    if args.created:
        result.update(verify_created(args.created))
    if args.runtime:
        require(args.created is not None, "runtime acceptance requires pinned --created asset bytes")
        verify_runtime(json.loads(args.runtime.read_text(encoding="utf-8")), source,
                       args.runtime.resolve())
        result["runtime_manifest_sha256"] = sha(args.runtime)
        result["runtime_passed"] = True
    if args.self_test:
        result["rejected_negative_cases"] = negative_cases(source)
    result["passed"] = True
    print(json.dumps(result, ensure_ascii=False, sort_keys=True, indent=2))


if __name__ == "__main__":
    main()
