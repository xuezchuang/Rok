"""Offline, fail-closed checks for the isolated Unity world-stream diagnostic.

No Unreal import or editor mutation. The optional runtime check consumes actual
HISM counters emitted by RokWorldInstancedMaterialDiagnosticGameMode, not JSON estimates.
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
from PrepareWorldInstancedAliasContractV7 import make_template, make_actual_alias


ROOT = Path("D:/ueDemo/Rok")
HERE = ROOT / "Saved/PendingWorldInstancedVisibilityDiagnosticV10"
ALIAS_TEMPLATE = HERE / "alias_template.json"
ALIAS_PATH = ROOT / "Saved/PendingWorldInstancedVisibilityDiagnosticV7/actual_alias_v7.json"
VARIANTS_PLAN = ROOT / "Saved/PendingWorldInstancedMaterialVariantsV6/frozen_plan.json"
RIVER_CONTRACT = ROOT / "Saved/PendingWorldRiverWaterMaterialSourceV3/frozen_contract.json"
BRIDGE_PLAN = ROOT / "Saved/PendingWorldBridgeInstancedMaterialV4/frozen_plan.json"
ALIAS_TEMPLATE_SHA = "428f287389c01f71e3da45aa54b9be520c96a4d9b020aaac25a509b9f3232480"
ACTUAL_ALIAS_SHA = "0a78887c5326cdf994c491cbac3900db2bbd752be2d27289c1afca9fb49fe2fd"
VARIANTS_PLAN_SHA = "21b714660c5cd0c3f0577b3c553f4c28f687b7bbd76e030045fd524156690aae"
RIVER_CONTRACT_SHA = "c7fac29e53c4f6180b8557d00f27a206a8596b6d9f006f9b9dd7bd8529b70709"
BRIDGE_PLAN_SHA = "3825d7f164a545ba38dd55f59347edf1176bd2ebcbd5829807eb4593435370d4"
VARIANTS_READY_SHA = "f2adb9a2a5254079e83bf2088abdde8b7d56363062ce420c14df747402c98861"
RIVER_READY_SHA = "f2b5ce4a29994d906cbf9ab12c92b1db47a09b3f7f5769bf251252e3431fa01f"
BRIDGE_READY_SHA = "4d0db09de14952fd41e7527a49c1d9482173539058a7625da6a1302629bba393"
VARIANTS_REVIEW_SHA = "16bcca3124b1d23d73f4fcd985edb46376447bff0fffb80f49fe36bcb72b625e"
RIVER_V2_HISTORICAL_REVIEW_SHA = "4ae7d8437616c7e59f556bfc11b0e9b83b925d7956f7020f49e9645542d45638"
RIVER_V3_CREATOR_SHA = "6cff8120a7ee82d533386393dba70684ef2a379463364db0a854336e0370d2dc"
RIVER_V3_VERIFIER_SHA = "32a5dbd292149bafae8ad34aba03d7257ea88c61d57b00b18effa35da98ff71a"
RIVER_V2_TEXTURE_SHA = "b4ab70d2ca3e66ecd8db5cb070c2ceaf76368b4a7558d64362e58cde656d4385"
RIVER_V2_FAILED_MATERIAL_SHA = "687a65a28831cae49c54c12c2d0ac72fb5e7438d589184191a3c7ea34687d61c"
BRIDGE_REVIEW_SHA = "590a47d5e436964f5210eced3b713079e942db27ed52b3429848d72516e67027"
VARIANTS_GRAPH_SHA = "3d95ecd290dd7eab03b82176f9262dee605b1c65ff138fde2c2de9a0aa524fcc"
VARIANTS_CREATOR_SHA = "8d6a3ac9983d03fa4c2f55746fb12eb998a50652f88fa1cf93670a2ef972ac5b"
BRIDGE_CREATOR_SHA = "0250a288227f7e7b0fb7785975b334efe349e1cbdbac5efbc427a45e8a11f521"
ORIGINAL_CONTENT_PINSET_SHA = "07161b934c07ffcb221d6f330eb9ab11244578ead7c15a97cb7b70c1b97a2e7a"
BRIDGE_SOURCE = "/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_build_bridge_A.M_WorldSource_build_bridge_A"
BRIDGE_SHA = "07b79a8751ed8cc4a2567010f229b2db177420d6170c40151963b7d8953305d9"
BRIDGE_BYTES = 6806
RIVER_BLOCKED_REVIEW_SHA = "e7773ec4abfdacf5e89abc43002fbf0a9cac82af08f242fe22be862d62f48d46"
RIVER_SUPPLEMENT_HELPER_SHA = "d3db721642bc2cc5945fe3bb454fe3bc55a84f3330e5c74ad0f94c713a1b68bf"
RIVER_SUPPLEMENT_REVIEW_SHA = "5f672de51f363a3ef7415922bb1697cfd6232e6bd4051c5b9182950be63cd3c1"
RIVER_SUPPLEMENT_RESULT_SHA = "455a01f04f1e2ecd8e3422d5e2b22e43d8d1f817603a3dc6fae99bf1812fc482"
RIVER_SUPPLEMENT_BEFORE_SHA = "b67bf5eb5df7e57e34b3b4b2347eca0a053d19488eb09f7bdf20ffd45dbf014d"
RIVER_SUPPLEMENT_AFTER_SHA = "b7d1e41db909df46d244b105fc98f1abc0e5d1ccd0c73069ce0ffd5e57490eb1"
RIVER_SUPPLEMENT_TERMINAL_SHA = "3401d41b6329c7c1848b0919bde69ccdab9657a0662b285558b849aa6e3380c1"
RIVER_SUPPLEMENT_PREFLIGHT_RECEIPT_SHA = "bb8b0d45038cac6864b6404f2fce72527efe71e04593bf1ea1487c1044b623ba"
RIVER_SUPPLEMENT_MATERIAL_SHA = "880c36b5ea49bf56cc029b10fc88da730046ee2fa05ff8a95e544892eb8abac8"
RIVER_SUPPLEMENT_LOG_SHA = "18389e8c3bf5127d727952a83dc6de537d128ca3449c365cb0746abc2d552834"
RIVER_SUPPLEMENT_DIR = (ROOT / "Saved/RokUnityMigration/BatchReview20261010WorldNativeMaterialBridge/"
                        "RiverV3RootSupplementV2_20261009T192113498016Z")
GAME_MODE = "/Script/Rok.RokWorldInstancedMaterialDiagnosticGameMode"
STAGE_READY = HERE / "ready_for_root_review.json"
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
                 "rok_world_instanced_material_diagnostic_flag", "instanced_alias_loaded",
                 "instanced_alias_sha256", "directional_light_count", "sky_light_count",
                 "server_subsystem_present", "local_only", "http_dispatch_attempts",
                 "http_suppressed_requests", "lod", "center_chunk_x", "center_chunk_y",
                 "generation", "hism_component_count", "hism_visible_component_count",
                 "hism_instance_count", "registered_hism_component_count",
                 "registered_hism_instance_count", "registered_matches_active",
                 "pending_load", "has_requested_state", "camera_dxf", "camera_unity_x",
                 "camera_unity_y", "hism_component_names",
                 "camera_actor_transform", "camera_is_player_pawn", "camera_is_view_target",
                 "camera_component_active", "streaming_root_static",
                 "registered_hism_component_names", "actual_hism_components")
READINESS_FIELDS = {"alias_mutation_frame", "alias_mutation_serial",
                    "platform_allowed_to_render", "world_rendering_enabled",
                    "game_player_count", "world_view_submission_frame",
                    "world_view_submission_step", "world_view_count",
                    "world_view_rect_width", "world_view_rect_height",
                    "world_view_rect_min_x", "world_view_rect_min_y",
                    "world_view_rect_max_x", "world_view_rect_max_y",
                    "full_viewport_width", "full_viewport_height",
                    "expected_view_rect_valid", "expected_view_rect_min_x",
                    "expected_view_rect_min_y", "expected_view_rect_max_x",
                    "expected_view_rect_max_y", "camera_view_aspect_ratio",
                    "camera_view_constrain_aspect_ratio", "camera_effective_aspect_ratio",
                    "viewport_desired_aspect_ratio",
                    "world_view_rendering_showflag", "world_view_static_meshes_showflag",
                    "world_view_instanced_static_meshes_showflag", "viewport_rendered_frame",
                    "submitted_view_fence_armed", "submitted_view_fence_frame",
                    "submitted_view_fence_step", "submitted_view_fence_alias_serial",
                    "submitted_view_fence_complete", "camera_horizontal_fov",
                    "camera_aspect_ratio", "camera_perspective"}


def require(value: bool, message: str) -> None:
    if not value:
        raise AssertionError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


PNG_CACHE: dict[tuple[str, str], tuple[int, int, int, str, int, bytes, str]] = {}


def valid_png(path: Path) -> tuple[int, int, int, str, int, bytes, str]:
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    cache_key = (str(path.resolve()), digest)
    if cache_key in PNG_CACHE:
        return PNG_CACHE[cache_key]
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
            require(width == 1440 and height == 810 and depth == 8 and mode == 6
                    and compression == filtering == interlace == 0, "PNG is not 1440x810 RGBA8")
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
    previous = bytearray(width * channels)
    nonblack = 0
    rgba_pixels = bytearray()
    for y in range(height):
        start = y * stride
        mode = raw[start]
        row = bytearray(raw[start + 1:start + stride])
        for x in range(len(row)):
            left = row[x - channels] if x >= channels else 0
            above = previous[x]
            upper_left = previous[x - channels] if x >= channels else 0
            if mode == 1:
                predictor = left
            elif mode == 2:
                predictor = above
            elif mode == 3:
                predictor = (left + above) // 2
            elif mode == 4:
                p = left + above - upper_left
                distances = (abs(p - left), abs(p - above), abs(p - upper_left))
                predictor = (left, above, upper_left)[distances.index(min(distances))]
            else:
                predictor = 0
            row[x] = (row[x] + predictor) & 255
        for x in range(0, len(row), channels):
            if channels == 4:
                require(row[x + 3] == 255, "PNG has nonopaque alpha")
            nonblack += any(channel > 2 for channel in row[x:x + 3])
        rgba_pixels.extend(row)
        previous = row
    pixels = bytes(rgba_pixels)
    require(len(pixels) == width * height * 4, "decoded RGBA8 raster length differs")
    result = width, height, len(data), digest, nonblack, pixels, hashlib.sha256(pixels).hexdigest()
    PNG_CACHE[cache_key] = result
    return result


def synthetic_png_for_negative_test(gray: int, *, compression: int = 1,
                                    metadata: bytes = b"") -> bytes:
    def chunk(kind: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + kind + payload
                + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))
    row = b"\x00" + bytes((gray, gray, gray, 255)) * 1440
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", 1440, 810, 8, 6, 0, 0, 0))
            + (chunk(b"tEXt", b"diagnostic\x00" + metadata) if metadata else b"")
            + chunk(b"IDAT", zlib.compress(row * 810, compression))
            + chunk(b"IEND", b""))


PIXEL_DIFF_CACHE: dict[tuple[str, str], tuple[int, int]] = {}


def rgba_difference(active: bytes, empty: bytes) -> tuple[int, int]:
    require(len(active) == len(empty) == 1440 * 810 * 4,
            "decoded active/empty RGBA8 raster sizes differ")
    key = (hashlib.sha256(active).hexdigest(), hashlib.sha256(empty).hexdigest())
    if key in PIXEL_DIFF_CACHE:
        return PIXEL_DIFF_CACHE[key]
    changed, maximum = 0, 0
    for offset in range(0, len(active), 4):
        pixel_diff = max(abs(active[offset + channel] - empty[offset + channel])
                         for channel in range(3))
        changed += pixel_diff > 2
        maximum = max(maximum, pixel_diff)
    PIXEL_DIFF_CACHE[key] = changed, maximum
    return changed, maximum


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
    return {"schema": "rok.world_instanced_material_diagnostic_offline_verify.v10",
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


def fixed_alias_template() -> dict:
    require(sha(VARIANTS_PLAN) == VARIANTS_PLAN_SHA
            and sha(RIVER_CONTRACT) == RIVER_CONTRACT_SHA
            and sha(BRIDGE_PLAN) == BRIDGE_PLAN_SHA,
            "frozen creator source contracts drifted")
    require(ALIAS_TEMPLATE.is_file() and sha(ALIAS_TEMPLATE) == ALIAS_TEMPLATE_SHA,
            "fixed 24-pair alias template drifted")
    require(sha(ROOT / "Saved/PendingWorldMaterialRootNativeBridgeV2/ready_for_root_review.json") == VARIANTS_READY_SHA
            and sha(ROOT / "Saved/PendingWorldRiverWaterMaterialSourceV3/ready_for_root_review.json") == RIVER_READY_SHA
            and sha(ROOT / "Saved/PendingWorldBridgeInstancedMaterialV4/ready_for_root_review.json") == BRIDGE_READY_SHA
            and sha(ROOT / "Saved/PendingWorldMaterialRootNativeBridgeV2/IndependentReviewV1/review.json") == VARIANTS_REVIEW_SHA
            and sha(ROOT / "Saved/PendingWorldRiverWaterMaterialSourceV2/IndependentReviewV1/review.json") == RIVER_V2_HISTORICAL_REVIEW_SHA
            and sha(ROOT / "Saved/PendingWorldBridgeInstancedMaterialV4/IndependentReviewV1/review.json") == BRIDGE_REVIEW_SHA,
            "creator ready or historical River V2 source-review bytes drifted")
    native_ready = json.loads((ROOT / "Saved/PendingWorldMaterialRootNativeBridgeV2/ready_for_root_review.json").read_text(encoding="utf-8-sig"))
    river_ready = json.loads((ROOT / "Saved/PendingWorldRiverWaterMaterialSourceV3/ready_for_root_review.json").read_text(encoding="utf-8-sig"))
    bridge_ready = json.loads((ROOT / "Saved/PendingWorldBridgeInstancedMaterialV4/ready_for_root_review.json").read_text(encoding="utf-8-sig"))
    require(native_ready.get("stage_files", {}).get("CreateWorldInstancedMaterialVariantsNativeBridgeV2.py", {}).get("sha256") == VARIANTS_CREATOR_SHA
            and native_ready.get("stage_files", {}).get("MaterialGraphFingerprintNativeBridgeV2.py", {}).get("sha256") == VARIANTS_GRAPH_SHA
            and river_ready.get("contract_sha256") == RIVER_CONTRACT_SHA
            and river_ready.get("stage_files", {}).get("CreateWorldRiverWaterMaterialV3.py") == RIVER_V3_CREATOR_SHA
            and river_ready.get("stage_files", {}).get("VerifyWorldRiverWaterSourceV3.py") == RIVER_V3_VERIFIER_SHA
            and bridge_ready.get("stage_files", {}).get("CreateWorldBridgeInstancedMaterialV4.py", {}).get("sha256") == BRIDGE_CREATOR_SHA
            and bridge_ready.get("stage_files", {}).get("frozen_plan.json", {}).get("sha256") == BRIDGE_PLAN_SHA
            and bridge_ready.get("native_bridge_v2_ready_sha256") == VARIANTS_READY_SHA,
            "creator source/plan/native dependency pins differ from the reviewed ready reports")
    template = json.loads(ALIAS_TEMPLATE.read_text(encoding="utf-8"))
    require(template == make_template(), "24 frozen source-target mappings drifted")
    require(ALIAS_PATH.is_file() and sha(ALIAS_PATH) == ACTUAL_ALIAS_SHA
            and json.loads(ALIAS_PATH.read_text(encoding="utf-8")) == make_actual_alias(template),
            "24 actual creator aliases drifted")
    require(package_file(BRIDGE_SOURCE).is_file()
            and package_file(BRIDGE_SOURCE).stat().st_size == BRIDGE_BYTES
            and sha(package_file(BRIDGE_SOURCE)) == BRIDGE_SHA,
            "unique bridge source package drifted")
    return template


def verify_stage_pins(runtime: bool = False) -> dict:
    ready = json.loads(STAGE_READY.read_text(encoding="utf-8"))
    require(ready["schema"] == "rok.world_instanced_visibility_diagnostic_stage.v10"
            and ready["status"] == "READY_FOR_INDEPENDENT_STATIC_REVIEW_ONLY"
            and ready["alias_source_target_pairs"] == 24
            and ready["game_mode"] == GAME_MODE
            and ready["five_phase_instance_counts"] == [848, 1688, 1688, 0, 1688]
            and ready["lod01_dependency_count"] == 243
            and ready["lod2_missing_dependency_count"] == 249
            and ready["black_viewport_hard_failure"] is True
            and ready["source_camera_aspect_ratio"] == 16 / 10
            and ready["source_camera_constrain_aspect_ratio"] is True
            and ready["expected_source_constrained_view_rect"] == [72, 0, 1368, 810]
            and ready["full_viewport_extent"] == [1440, 810]
            and ready["ue_build_or_runtime_passed"] is False
            and ready["formal_map_lod_accepted"] is False
            and ready["full_migration_accepted"] is False,
            "visibility stage identity changed")
    predecessor = ROOT / "Saved/PendingWorldInstancedVisibilityDiagnosticV9/ready_for_root_review.json"
    require(ready["predecessor_ready_sha256"] ==
            "78ae47274a7d7ae7ed6670649001d72763da31367790eeb6b56648e5537506fc"
            and sha(predecessor) == ready["predecessor_ready_sha256"],
            "frozen Product9 ready changed")
    product9_runtime = ROOT / ("Saved/RokUnityMigration/WorldInstancedMaterialDiagnosticRunsV9/"
                               "20261009_232026_14456_1369177671760/manifest.json")
    require(ready["product9_runtime_failure_sha256"] ==
            "6641e219910a9ff5e8e34a80e5941dd206225cf954971f9fd56744963adec908"
            and sha(product9_runtime) == ready["product9_runtime_failure_sha256"],
            "Product9 native timeout evidence changed")
    expected_sources = {"Source/Rok/RokWorldStreamingDiagnosticGameMode.h",
                        "Source/Rok/RokWorldStreamingDiagnosticGameMode.cpp",
                        "Source/Rok/RokWorldMapStreamingActor.h",
                        "Source/Rok/RokWorldMapStreamingActor.cpp",
                        "Source/Rok/RokWorldMapCameraPawn.h",
                        "Source/Rok/RokWorldMapCameraPawn.cpp"}
    require({row["path"] for row in ready["read_only_input_pins"]} == expected_sources,
            "six read-only Source input paths differ")
    for record in ready["stage_files"] + ready["read_only_input_pins"]:
        path = ROOT / record["path"]
        require(path.is_file() and path.stat().st_size == record["bytes"]
                and sha(path) == record["sha256"],
                "visibility stage/read-only input bytes differ: " + record["path"])
    if runtime:
        copies = {
            HERE / "RokWorldInstancedMaterialDiagnosticGameMode.h":
                ROOT / "Source/Rok/RokWorldInstancedMaterialDiagnosticGameMode.h",
            HERE / "RokWorldInstancedMaterialDiagnosticGameMode.cpp":
                ROOT / "Source/Rok/RokWorldInstancedMaterialDiagnosticGameMode.cpp",
            HERE / "VerifyRokWorldInstancedMaterialDiagnosticV10.py":
                ROOT / "Scripts/VerifyRokWorldInstancedMaterialDiagnosticV10.py",
        }
        for staged, live in copies.items():
            require(live.is_file() and sha(live) == sha(staged),
                    "runtime source/verifier does not match reviewed staged byte copy: " + str(live))
    return {"visibility_stage_ready_sha256": sha(STAGE_READY)}


def verify_render_readiness(sample: dict, index: int) -> None:
    request = sample["screenshot_request_frame"]
    processed = sample["screenshot_processed_frame"]
    pre = sample["pre_request_render_readiness"]
    post = sample["processed_render_readiness"]
    for label, record in (("pre-request", pre), ("processed", post)):
        require(type(record) is dict and set(record) == READINESS_FIELDS,
                f"phase {index}: {label} render readiness field set")
        require(all(record[key] is True for key in (
            "platform_allowed_to_render", "world_rendering_enabled",
            "world_view_rendering_showflag", "world_view_static_meshes_showflag",
            "world_view_instanced_static_meshes_showflag",
            "submitted_view_fence_armed", "submitted_view_fence_complete",
            "camera_perspective", "camera_view_constrain_aspect_ratio",
            "expected_view_rect_valid")),
            f"phase {index}: {label} world view was not permitted and fenced")
        require(record["game_player_count"] == record["world_view_count"] == 1
                and record["full_viewport_width"] == 1440
                and record["full_viewport_height"] == 810
                and record["world_view_rect_width"] == 1296
                and record["world_view_rect_height"] == 810
                and record["world_view_submission_step"] == index + 1
                and record["submitted_view_fence_step"] == index + 1
                and record["submitted_view_fence_alias_serial"] ==
                    record["alias_mutation_serial"],
                f"phase {index}: {label} view/phase/alias fence differs")
        require(all(type(record[key]) in (int, float) and math.isfinite(record[key])
                    and record[key] >= 0 for key in (
                        "alias_mutation_frame", "alias_mutation_serial",
                        "world_view_submission_frame", "viewport_rendered_frame",
                        "submitted_view_fence_frame"))
                and type(record["camera_horizontal_fov"]) in (int, float)
                and math.isfinite(record["camera_horizontal_fov"])
                and 0 < record["camera_horizontal_fov"] < 180
                and type(record["camera_aspect_ratio"]) in (int, float)
                and math.isfinite(record["camera_aspect_ratio"])
                and abs(record["camera_aspect_ratio"] - 16 / 10) < .001
                and type(record["camera_view_aspect_ratio"]) in (int, float)
                and math.isfinite(record["camera_view_aspect_ratio"])
                and abs(record["camera_view_aspect_ratio"] - 16 / 10) < .001
                and type(record["camera_effective_aspect_ratio"]) in (int, float)
                and math.isfinite(record["camera_effective_aspect_ratio"])
                and abs(record["camera_effective_aspect_ratio"] - 16 / 10) < .001
                and type(record["viewport_desired_aspect_ratio"]) in (int, float)
                and math.isfinite(record["viewport_desired_aspect_ratio"])
                and abs(record["viewport_desired_aspect_ratio"] - 16 / 9) < .001,
                f"phase {index}: {label} invalid frame or perspective values")
        actual_rect = tuple(record[f"world_view_rect_{axis}"] for axis in
                            ("min_x", "min_y", "max_x", "max_y"))
        expected_rect = tuple(record[f"expected_view_rect_{axis}"] for axis in
                              ("min_x", "min_y", "max_x", "max_y"))
        require(all(type(value) in (int, float) and math.isfinite(value)
                    for value in actual_rect + expected_rect)
                and expected_rect == (72, 0, 1368, 810)
                and actual_rect == expected_rect
                and 0 <= actual_rect[0] < actual_rect[2] <= record["full_viewport_width"]
                and 0 <= actual_rect[1] < actual_rect[3] <= record["full_viewport_height"]
                and actual_rect[2] - actual_rect[0] == record["world_view_rect_width"]
                and actual_rect[3] - actual_rect[1] == record["world_view_rect_height"],
                f"phase {index}: {label} constrained view rect differs from source camera")
        require(record["alias_mutation_frame"] <= record["submitted_view_fence_frame"]
                <= record["world_view_submission_frame"]
                <= record["viewport_rendered_frame"],
                f"phase {index}: {label} view did not follow alias and fence")
    require(pre["submitted_view_fence_frame"] < request
            and pre["viewport_rendered_frame"] <= request
            and pre["alias_mutation_serial"] == post["alias_mutation_serial"]
            and pre["alias_mutation_frame"] == post["alias_mutation_frame"]
            and pre["submitted_view_fence_frame"] == post["submitted_view_fence_frame"]
            and pre["camera_horizontal_fov"] == post["camera_horizontal_fov"]
            and pre["camera_aspect_ratio"] == post["camera_aspect_ratio"]
            and pre["camera_view_aspect_ratio"] == post["camera_view_aspect_ratio"]
            and pre["camera_effective_aspect_ratio"] == post["camera_effective_aspect_ratio"]
            and pre["camera_view_constrain_aspect_ratio"] is
                post["camera_view_constrain_aspect_ratio"]
            and tuple(pre[f"expected_view_rect_{axis}"] for axis in
                      ("min_x", "min_y", "max_x", "max_y")) ==
                tuple(post[f"expected_view_rect_{axis}"] for axis in
                      ("min_x", "min_y", "max_x", "max_y"))
            and post["world_view_submission_frame"] == processed
            and post["viewport_rendered_frame"] == processed,
            f"phase {index}: screenshot frame lacks same-view submission proof")


def validate_alias_structure(alias: dict, template: dict) -> dict:
    require(set(alias) == set(template), "alias top-level fields differ")
    for key in template:
        if key not in ("entries", "creator_receipts"):
            require(alias[key] == template[key], "alias identity/acceptance differs: " + key)
    require(len(alias["entries"]) == len(template["entries"]) == 24,
            "alias must have exactly 24 frozen entries")
    sources, targets = set(), set()
    for observed, frozen in zip(alias["entries"], template["entries"]):
        require(set(observed) == set(frozen), "alias entry fields differ")
        for key in frozen:
            if key not in ("target_package_sha256", "target_package_bytes"):
                require(observed[key] == frozen[key], "frozen source-target pair differs: " + key)
        require(isinstance(observed["target_package_sha256"], str)
                and re.fullmatch(r"[0-9a-f]{64}", observed["target_package_sha256"])
                and type(observed["target_package_bytes"]) is int
                and observed["target_package_bytes"] > 0,
                "target package SHA/bytes missing")
        sources.add(observed["source_object"])
        targets.add(observed["target_object"])
    require(len(sources) == len(targets) == 24, "alias bijection differs")
    require(set(alias["creator_receipts"]) == {"variants", "riverwater", "bridge"},
            "exactly three creator proofs required")
    for key, proof in alias["creator_receipts"].items():
        require(set(proof) == set(template["creator_receipts"][key]),
                "creator proof fields differ")
        require(all(type(proof[name]) is str and proof[name]
                    for name in ("terminal_path", "terminal_sha256", "ready_path", "ready_sha256"))
                and type(proof["terminal_bytes"]) is int and proof["terminal_bytes"] > 0
                and all(re.fullmatch(r"[0-9a-f]{64}", proof[name])
                        for name in ("terminal_sha256", "ready_sha256")),
                "creator proof SHA/path/bytes missing")
        require(proof["ready_sha256"] == {"variants": VARIANTS_READY_SHA,
                                           "riverwater": RIVER_READY_SHA,
                                           "bridge": BRIDGE_READY_SHA}[key],
                "creator revision differs from frozen reviewed ready")
    return {item["source_object"]: item for item in alias["entries"]}


def validate_river_v3_terminal(receipt: dict, river_item: dict, contract: dict) -> None:
    partial = contract["v2_failed_partial"]
    created = receipt.get("created")
    after = receipt.get("after")
    require(receipt.get("schema") == "rok.world_river_water.editor_attempt.v3"
            and receipt.get("status") == "created_pending_native_gpu_comparison_and_binding"
            and receipt.get("apply_requested") is True
            and receipt.get("ready_sha256") == RIVER_READY_SHA
            and receipt.get("contract_sha256") == RIVER_CONTRACT_SHA
            and receipt.get("v2_partial_reused_texture_sha256") == partial["texture_sha256"]
            and receipt.get("v2_failed_material_protected_sha256") == partial["failed_material_sha256"]
            and type(created) is list and len(created) == 1 and type(after) is dict
            and after.get("content_before_count") == 2448
            and after.get("content_after_count") == 2449
            and after.get("actual_dll_count") == 3
            and (after.get("new_material") or {}).get("sha256") == river_item["target_package_sha256"]
            and (after.get("new_material") or {}).get("bytes") == river_item["target_package_bytes"]
            and Path((after.get("new_material") or {}).get("path", "")).resolve() ==
                package_file(river_item["target_object"]).resolve()
            and receipt.get("runtime_accepted") is False
            and receipt.get("visual_accepted") is False
            and receipt.get("full_migration_accepted") is False,
            "V3 River terminal/partial/full Content proof differs")
    record = created[0]
    require(record.get("object") == river_item["target_object"]
            and record.get("sha256") == river_item["target_package_sha256"]
            and record.get("bytes") == river_item["target_package_bytes"]
            and record.get("instanced_usage") is True
            and record.get("compile_errors") == []
            and record.get("texture_reused") == partial["texture_object"]
            and record.get("actual_rhi_format") == "pending_native_readback"
            and record.get("native_gpu_and_unity_parity_accepted") is False
            and record.get("stage_bridge_and_channel_hits") ==
                {"vertex_interpolator": 1, "ba": 2, "alpha": 2,
                 "spec": 1, "alpha_plus_spec": 1},
            "V3 River single-created material/source-stage/compile proof differs")


def pinned_rows(baseline: dict, key: str) -> dict[str, tuple[str, int]]:
    rows = baseline[key]
    require(type(rows) is list, "supplement baseline row group absent: " + key)
    mapped = {}
    for row in rows:
        require(type(row) is dict and set(row) == {"path", "sha256", "bytes"}
                and Path(row["path"]).is_absolute()
                and re.fullmatch(r"[0-9a-f]{64}", row["sha256"])
                and type(row["bytes"]) is int and row["bytes"] > 0,
                "supplement baseline malformed row: " + key)
        path = str(Path(row["path"]).resolve()).casefold()
        require(path not in mapped, "supplement baseline duplicate: " + key)
        mapped[path] = (row["sha256"], row["bytes"])
    return mapped


def validate_supplement_sets(before: dict, after: dict, material_path: Path,
                             material_sha: str, material_bytes: int) -> None:
    for key in ("source_files", "unaccepted_source_files", "accepted_asset_files",
                "actual_dll_files"):
        old = pinned_rows(before, key)
        new = pinned_rows(after, key)
        require(old and old == new, "River supplemental protected set changed: " + key)
        if key == "actual_dll_files":
            require({Path(path).name.casefold() for path in old} ==
                    {"unrealeditor-rok.dll", "unrealeditor-rok-gridvalidation.dll",
                     "unrealeditor-rokrendering.dll"},
                    "River supplemental actual Rok DLL set differs")
    old = pinned_rows(before, "all_content_files")
    new = pinned_rows(after, "all_content_files")
    target = str(material_path.resolve()).casefold()
    require(len(old) == 2448 and len(new) == 2449 and target not in old
            and new.get(target) == (material_sha, material_bytes)
            and {k: v for k, v in new.items() if k != target} == old,
            "River supplemental Content delta is not exactly the one V3 material")


def validate_river_root_supplement(terminal_path: Path, river_item: dict) -> None:
    helper = ROOT / "Saved/RokUnityMigration/BatchReview20261010WorldNativeMaterialBridge/RootRiverV3SupplementalExecutionV2.py"
    review = ROOT / "Saved/RokUnityMigration/BatchReview20261010WorldNativeMaterialBridge/RootRiverV3EnvelopeReviewV1/review.json"
    blocked = ROOT / "Saved/PendingWorldRiverWaterMaterialSourceV3/IndependentReviewV1/review.json"
    result_path = RIVER_SUPPLEMENT_DIR / "result.json"
    before_path = ROOT / "Saved/RokUnityMigration/BatchReview20261010CommanderSkillReadonlyDetail/SkillBuildV5_20261009T191525Z/postbuild_baseline.json"
    after_path = RIVER_SUPPLEMENT_DIR / "next_inputpins_v1.json"
    log_path = RIVER_SUPPLEMENT_DIR / "editor.log"
    preflight_receipt_path = terminal_path.parent / "receipt.json"
    for path, digest in ((helper, RIVER_SUPPLEMENT_HELPER_SHA),
                         (review, RIVER_SUPPLEMENT_REVIEW_SHA),
                         (blocked, RIVER_BLOCKED_REVIEW_SHA),
                         (result_path, RIVER_SUPPLEMENT_RESULT_SHA),
                         (before_path, RIVER_SUPPLEMENT_BEFORE_SHA),
                         (after_path, RIVER_SUPPLEMENT_AFTER_SHA),
                         (log_path, RIVER_SUPPLEMENT_LOG_SHA),
                         (preflight_receipt_path, RIVER_SUPPLEMENT_PREFLIGHT_RECEIPT_SHA)):
        require(path.is_file() and sha(path) == digest,
                "River root supplemental fixed evidence drifted: " + str(path))
    result = json.loads(result_path.read_text(encoding="utf-8"))
    before = json.loads(before_path.read_text(encoding="utf-8"))
    after = json.loads(after_path.read_text(encoding="utf-8"))
    material = package_file(river_item["target_object"])
    require(result["schema"] == "rok.root_river_v3_supplemental_execution.v2"
            and result["passed"] is True and result["exit_code"] == 0
            and type(result["pid"]) is int and result["pid"] > 0
            and result["runtime_accepted"] is False and result["visual_accepted"] is False
            and result["goal_complete"] is False
            and result["plan"]["blocked_upstream_review_sha256"] == RIVER_BLOCKED_REVIEW_SHA
            and Path(result["plan"]["baseline"]["path"]).resolve() == before_path.resolve()
            and result["plan"]["baseline"]["sha256"] == RIVER_SUPPLEMENT_BEFORE_SHA
            and Path(result["next_baseline"]["path"]).resolve() == after_path.resolve()
            and result["next_baseline"]["sha256"] == RIVER_SUPPLEMENT_AFTER_SHA
            and Path(result["creator_terminal"]["path"]).resolve() == terminal_path.resolve()
            and result["creator_terminal"]["sha256"] == RIVER_SUPPLEMENT_TERMINAL_SHA
            and Path(result["creator_receipt"]["path"]).resolve() == preflight_receipt_path.resolve()
            and result["creator_receipt"]["sha256"] == RIVER_SUPPLEMENT_PREFLIGHT_RECEIPT_SHA
            and Path(result["material"]["path"]).resolve() == material.resolve()
            and result["material"]["sha256"] == river_item["target_package_sha256"] == RIVER_SUPPLEMENT_MATERIAL_SHA
            and result["material"]["bytes"] == river_item["target_package_bytes"] == 36251
            and Path(result["log"]["path"]).resolve() == log_path.resolve()
            and result["log"]["sha256"] == RIVER_SUPPLEMENT_LOG_SHA
            and result["protected_after_observation"]["full_source_content_dll_sets_and_bytes_verified"] is True,
            "River root supplemental process/material/blocked-review binding differs")
    validate_supplement_sets(before, after, material,
                             river_item["target_package_sha256"], river_item["target_package_bytes"])


def validate_alias_evidence(alias_path: Path, expected_sha: str) -> tuple[dict, dict]:
    require(alias_path.resolve() == ALIAS_PATH.resolve() and expected_sha == ACTUAL_ALIAS_SHA
            and re.fullmatch(r"[0-9a-f]{64}", expected_sha)
            and alias_path.is_file() and sha(alias_path) == expected_sha,
            "external fixed alias JSON path/SHA differs")
    alias = json.loads(alias_path.read_text(encoding="utf-8"))
    by_source = validate_alias_structure(alias, fixed_alias_template())
    for item in by_source.values():
        source_file = package_file(item["source_object"])
        target_file = package_file(item["target_object"])
        require(source_file.is_file()
                and source_file.stat().st_size == item["source_package_bytes"]
                and sha(source_file) == item["source_package_sha256"],
                "frozen original source package differs")
        require(target_file.is_file()
                and target_file.stat().st_size == item["target_package_bytes"]
                and sha(target_file) == item["target_package_sha256"],
                "creator target package differs")
    expected_ready = {
        "variants": ROOT / "Saved/PendingWorldMaterialRootNativeBridgeV2/ready_for_root_review.json",
        "riverwater": ROOT / "Saved/PendingWorldRiverWaterMaterialSourceV3/ready_for_root_review.json",
        "bridge": ROOT / "Saved/PendingWorldBridgeInstancedMaterialV4/ready_for_root_review.json",
    }
    expected_run_root = {
        "variants": ROOT / "Saved/RokUnityMigration/WorldInstancedMaterialVariantsV8",
        "riverwater": ROOT / "Saved/PendingWorldRiverWaterMaterialSourceV3/ApplyRuns",
        "bridge": ROOT / "Saved/RokUnityMigration/WorldBridgeInstancedMaterialV4",
    }
    ready_sha = {"variants": VARIANTS_READY_SHA, "riverwater": RIVER_READY_SHA,
                 "bridge": BRIDGE_READY_SHA}
    target_by_object = {item["target_object"]: item for item in by_source.values()}
    for key, proof in alias["creator_receipts"].items():
        ready = Path(proof["ready_path"])
        terminal_path = Path(proof["terminal_path"])
        require(ready.is_absolute() and ready.resolve() == expected_ready[key].resolve()
                and proof["ready_sha256"] == ready_sha[key]
                and ready.is_file() and sha(ready) == proof["ready_sha256"],
                "reviewed creator ready bytes differ")
        require(terminal_path.is_absolute() and terminal_path.name == "terminal.json"
                and terminal_path.parent.parent.resolve() == expected_run_root[key].resolve()
                and terminal_path.is_file()
                and terminal_path.stat().st_size == proof["terminal_bytes"]
                and sha(terminal_path) == proof["terminal_sha256"],
                "creator terminal path/SHA/bytes differ")
        receipt = json.loads(terminal_path.read_text(encoding="utf-8"))
        variants = key == "variants"
        river = key == "riverwater"
        require(receipt["schema"] == {"variants": "rok.world_instanced_material_variants_editor_run.native_bridge_v2",
                                      "riverwater": "rok.world_river_water.editor_attempt.v3",
                                      "bridge": "rok.world_bridge_instanced_material_editor_run.v4"}[key]
                and receipt["status"] == {"variants": "created_pending_runtime_validation",
                                         "riverwater": "created_pending_native_gpu_comparison_and_binding",
                                         "bridge": "created_pending_native_runtime_and_visual_validation"}[key]
                and receipt["plan_sha256" if not river else "contract_sha256"] ==
                (RIVER_CONTRACT_SHA if river else VARIANTS_PLAN_SHA if variants else BRIDGE_PLAN_SHA)
                and receipt.get("apply_requested") is True
                and len(receipt["created"]) == (22 if variants else 1),
                "creator status/schema/plan/created count differs")
        if river:
            river_contract = json.loads(RIVER_CONTRACT.read_text(encoding="utf-8"))
            river_item = next(item for item in by_source.values()
                              if item["creator_key"] == "riverwater")
            validate_river_v3_terminal(receipt, river_item, river_contract)
            validate_river_root_supplement(terminal_path, river_item)
            partial = river_contract["v2_failed_partial"]
            for obj, digest, size in ((partial["texture_object"], partial["texture_sha256"], partial["texture_bytes"]),
                                      (partial["failed_material_object"], partial["failed_material_sha256"], partial["failed_material_bytes"])):
                protected = package_file(obj)
                require(protected.is_file() and protected.stat().st_size == size
                        and sha(protected) == digest,
                        "V2 failed partial package was changed or bound as the V3 target")
            require(river_item["target_object"] != partial["failed_material_object"]
                    and river_item["target_object"] != partial["texture_object"],
                    "failed V2 partial cannot be a runtime alias target")
        elif variants:
            require(receipt["graph_fingerprint_module_sha256"] == VARIANTS_GRAPH_SHA
                    and receipt["protected_content_files_checked_before"] == 2423
                    and receipt["protected_content_files_checked_after"] == 2423,
                    "variant creator graph/protected Content proof differs")
        else:
            root_pins = Path(receipt["root_inputpins_path"])
            require(receipt["source_object"] == BRIDGE_SOURCE
                    and receipt["source_package_sha256"] == BRIDGE_SHA
                    and receipt["graph_module_sha256"] == VARIANTS_GRAPH_SHA
                    and root_pins.is_absolute()
                    and root_pins.resolve().is_relative_to(
                        (ROOT / "Saved/RokUnityMigration").resolve())
                    and root_pins.is_file()
                    and re.fullmatch(r"[0-9a-f]{64}", receipt["root_inputpins_sha256"])
                    and sha(root_pins) == receipt["root_inputpins_sha256"]
                    and receipt["protected_content_files_checked_before"] >= 2423
                    and receipt["protected_content_files_checked_after"] ==
                    receipt["protected_content_files_checked_before"],
                    "Bridge creator source/graph/full current Content proof differs")
        proven = set()
        for record in receipt["created"]:
            target = record["object" if river else "target_object"]
            require(target in target_by_object and target not in proven
                    and target_by_object[target]["creator_key"] == key
                    and record["sha256" if river else "target_sha256"] ==
                    target_by_object[target]["target_package_sha256"]
                    and record["bytes" if river else "target_bytes"] ==
                    target_by_object[target]["target_package_bytes"]
                    and record["instanced_usage" if river else "instanced_static_mesh_usage"] is True,
                    "creator target package or usage proof differs")
            if variants:
                require(record["source_object"] ==
                        next(source for source, item in by_source.items()
                             if item["target_object"] == target)
                        and record["same_captured_reflected_subset_except_instancing_flag"] is True
                        and all(re.fullmatch(r"[0-9a-f]{64}", record[name]) for name in
                                ("source_graph_sha256", "variant_graph_sha256",
                                 "semantic_comparison_sha256")),
                        "creator original source/graph pairing differs")
            elif river:
                require(record["compile_errors"] == [],
                        "RiverWater material compile proof differs")
            else:
                require(record["same_captured_reflected_subset_except_instancing_flag"] is True
                        and re.fullmatch(r"[0-9a-f]{64}", record["semantic_comparison_sha256"])
                        and re.fullmatch(r"[0-9a-f]{64}", record["source_graph_file_sha256"])
                        and re.fullmatch(r"[0-9a-f]{64}", record["target_graph_file_sha256"])
                        and sha(terminal_path.parent / "source_graph.json") ==
                        record["source_graph_file_sha256"]
                        and sha(terminal_path.parent / "target_graph.json") ==
                        record["target_graph_file_sha256"],
                        "Bridge material graph/usage proof differs")
            proven.add(target)
        require(len(proven) == (22 if variants else 1),
                "creator receipt misses a reviewed alias target")
    return alias, by_source


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


def verify_actual_geometry(sample, source, index, by_source, alias):
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
        aliases = component["material_aliases"]
        require(len(materials) == len(aliases),
                f"phase {index}: material alias slot count differs")
        source_materials = []
        for slot, (material, record) in enumerate(zip(materials, aliases)):
            source_object = record["source_object"]
            expected = by_source.get(source_object)
            require(record["slot"] == slot and record["source_object"] == source_object
                    and record["actual_object"] == material
                    and record["source_passthrough"] is False
                    and record["render_proxy_material_verified"] is False
                    and record["base_instanced_static_mesh_usage"] is True
                    and record["native_shader_map_available"] is True,
                    f"phase {index}: actual slot/native usage or shader map differs")
            require(expected is not None and material == expected["target_object"]
                    and record["target_object"] == expected["target_object"]
                    and record["target_package_sha256"] == expected["target_package_sha256"]
                    and record["target_package_bytes"] == expected["target_package_bytes"]
                    and record["creator_terminal_sha256"] ==
                    alias["creator_receipts"][expected["creator_key"]]["terminal_sha256"],
                    f"phase {index}: source-to-target alias or creator proof differs")
            require(record["observed_before_alias"] in (source_object, material)
                    and record["base_material"] == material
                    and component[f"material_diagnostic_{slot}"]["path"] == material
                    and component[f"material_diagnostic_{slot}"]["base_instanced_static_mesh_usage"] is True
                    and component[f"material_diagnostic_{slot}"]["base_material"] == record["base_material"],
                    f"phase {index}: material slot observed identity/usage differs")
            source_materials.append(source_object)
        key = (chunk, component["mesh"], tuple(source_materials),
               component["translucency_sort_priority"])
        require(component["visible"] is True
                and component["instance_transforms_read_all"] is True
                and component["instances_local"]
                and component["attached_to_streaming_root"] is True
                and component["render_state_created"] is True
                and component["nonzero_world_bounds"] is True,
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
            and record["rok_world_instanced_material_diagnostic_flag"] is True
            and record["rok_local_only_flag"] is True
            and record["server_subsystem_present"] is True
            and record["local_only"] is True
            and type(record["http_dispatch_attempts"]) in (int, float)
            and type(record["http_suppressed_requests"]) in (int, float)
            and record["http_dispatch_attempts"] == 0
            and record["http_suppressed_requests"] == 0,
            f"{where}: local-only/server HTTP boundary failed")


def normalize_run_directory(value: str) -> Path:
    candidate = Path(value)
    if not candidate.is_absolute():
        candidate = ROOT / candidate
    resolved = candidate.resolve()
    require(resolved == ROOT or ROOT in resolved.parents,
            f"runtime run directory escapes project root: {value}")
    require(resolved.is_dir(), f"runtime run directory missing: {value}")
    return resolved


def verify_runtime(report: dict, source: dict, manifest_path: Path,
                   alias: dict, by_source: dict, alias_sha: str,
                   run_root: Path | None = None) -> None:
    require(report["schema"] == "rok.world_instanced_material_diagnostic_runtime.v10"
            and report["passed"] is True, "runtime not passed")
    require(report["game_mode"] == GAME_MODE
            and report["alias_path"] == str(ALIAS_PATH).replace("\\", "/")
            and report["alias_sha256"] == alias_sha
            and report["variants_creator_terminal_sha256"] ==
                alias["creator_receipts"]["variants"]["terminal_sha256"]
            and report["river_creator_terminal_sha256"] ==
                alias["creator_receipts"]["riverwater"]["terminal_sha256"]
            and report["bridge_creator_terminal_sha256"] ==
                alias["creator_receipts"]["bridge"]["terminal_sha256"]
            and report["diagnostic_directional_light_only"] is True
            and report["render_proxy_material_verified"] is False
            and report["formal_map_lod_accepted"] is False
            and report["unity_pixel_parity_accepted"] is False
            and report["unity_gamma_parity_accepted"] is False
            and report["full_migration_accepted"] is False,
            "diagnostic-only alias/proof scope differs")
    require(report["screenshot_origin"] == "FScreenshotRequest_game_viewport"
            and report["expected_viewport_width"] == 1440
            and report["expected_viewport_height"] == 810,
            "runtime did not request the actual 1440x810 game viewport")
    reported_run = normalize_run_directory(report["run_directory"])
    expected_run_root = (run_root or ROOT / "Saved/RokUnityMigration/WorldInstancedMaterialDiagnosticRunsV10").resolve()
    require(reported_run == manifest_path.parent.resolve()
            and manifest_path.name == "manifest.json"
            and manifest_path.parent.parent.resolve() == expected_run_root
            and expected_run_root.name == "WorldInstancedMaterialDiagnosticRunsV10"
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
    decoded_pixels = []
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
        verify_render_readiness(sample, index)
        require(sample["viewport_width"] == 1440 and sample["viewport_height"] == 810
                and sample["png_width"] == 1440 and sample["png_height"] == 810,
                f"phase {index}: actual screenshot extent differs from request")
        image_path = Path(sample["screenshot_path"])
        require(image_path.is_absolute() and image_path.parent == manifest_path.parent
                and image_path.name == PHASES[index] + ".png",
                f"phase {index}: screenshot escaped unique run folder or wrong phase name")
        require(image_path.is_file(), f"phase {index}: screenshot file missing")
        width, height, count, digest, nonblack, pixels, pixels_sha = valid_png(image_path)
        require((width, height, count, digest, nonblack) ==
                (sample["png_width"], sample["png_height"], sample["png_bytes"],
                 sample["png_sha256"], sample["png_nonblack_rgb_pixels"])
                and sample["png_nonopaque_alpha_pixels"] == 0
                and sample["png_decoded_rgba_sha256"] == pixels_sha,
                f"phase {index}: PNG bytes/extent/SHA mismatch")
        require(nonblack > 1024 if expected[3] else True,
                f"phase {index}: streamed viewport is effectively black")
        screenshot_paths.append(image_path)
        decoded_pixels.append(pixels)
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
        require(sample["instanced_alias_loaded"] is True
                and sample["instanced_alias_sha256"] == alias_sha
                and sample["directional_light_count"] == 1
                and sample["sky_light_count"] == 0
                and sample["camera_is_player_pawn"] is True
                and sample["camera_is_view_target"] is True
                and sample["camera_component_active"] is True
                and sample["streaming_root_static"] is True
                and len(sample["camera_actor_transform"]) == 10
                and all(isinstance(value, (int, float)) and math.isfinite(value)
                        for value in sample["camera_actor_transform"]),
                f"phase {index}: camera, alias or diagnostic light state differs")
        verify_actual_geometry(sample, source, index, by_source, alias)
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
    for index, sample in enumerate(samples):
        changed, maximum = rgba_difference(decoded_pixels[index], decoded_pixels[3])
        require(sample["png_changed_rgb_pixels_vs_empty"] == changed
                and sample["png_max_rgb_diff_vs_empty"] == maximum,
                f"phase {index}: reported decoded RGB difference differs from PNG pixels")
        if index != 3:
            require(changed > 1024 and maximum > 2,
                    f"phase {index}: active viewport lacks source-visible RGB difference from empty LOD2")
    require(set(samples[0]["hism_component_names"]).isdisjoint(
            samples[1]["hism_component_names"]), "LOD0 previous HISM revision was retained")
    require(set(samples[2]["hism_component_names"]).isdisjoint(
            samples[4]["hism_component_names"]), "LOD1 restored stale HISM revision")


def negative_cases(source: dict) -> dict:
    template = fixed_alias_template()
    alias = copy.deepcopy(template)
    for item in alias["entries"]:
        item["target_package_sha256"] = "1" * 64
        item["target_package_bytes"] = 100
    for key, proof in alias["creator_receipts"].items():
        proof.update(terminal_path="D:/ueDemo/Rok/Saved/fake/terminal.json",
                     terminal_sha256="2" * 64, terminal_bytes=100,
                     ready_path="D:/ueDemo/Rok/Saved/fake/ready.json",
                     ready_sha256={"variants": VARIANTS_READY_SHA,
                                   "riverwater": RIVER_READY_SHA,
                                   "bridge": BRIDGE_READY_SHA}[key])
    by_source = validate_alias_structure(alias, template)
    alias_mutations = {
        "foreign_target": lambda x: x["entries"][0].update(target_object="/Game/Foreign.Bad"),
        "wrong_source": lambda x: x["entries"][0].update(source_object="/Game/Foreign.Bad"),
        "wrong_source_sha": lambda x: x["entries"][0].update(source_package_sha256="0" * 64),
        "wrong_target_sha": lambda x: x["entries"][0].update(target_package_sha256="bad"),
        "duplicate_pair": lambda x: x["entries"].__setitem__(1, copy.deepcopy(x["entries"][0])),
        "missing_pair": lambda x: x["entries"].pop(),
        "false_ready": lambda x: x.update(runtime_accepted=True),
        "bad_creator_proof": lambda x: x["creator_receipts"]["variants"].update(terminal_sha256="bad"),
        "foreign_alias_path": lambda x: x.update(alias_path="D:/elsewhere/alias.json"),
        "bridge_old_source_passthrough": lambda x: next(
            item for item in x["entries"] if item["creator_key"] == "bridge").update(
                target_object=BRIDGE_SOURCE),
        "missing_bridge_receipt": lambda x: x["creator_receipts"].pop("bridge"),
        "wrong_bridge_creator_revision": lambda x: x["creator_receipts"]["bridge"].update(
            ready_sha256="0" * 64),
        "wrong_river_creator_revision": lambda x: x["creator_receipts"]["riverwater"].update(
            ready_sha256="0" * 64),
        "failed_v2_river_material_as_alias": lambda x: next(
            item for item in x["entries"] if item["creator_key"] == "riverwater").update(
                target_object="/Game/RokDiagnostics/WorldStreaming/RiverWaterSourceV2/M_WorldRiver_Water_SourceFixedV2.M_WorldRiver_Water_SourceFixedV2"),
    }
    for label, mutate in alias_mutations.items():
        candidate = copy.deepcopy(alias)
        mutate(candidate)
        try:
            validate_alias_structure(candidate, template)
        except (AssertionError, KeyError, TypeError, ValueError):
            continue
        raise AssertionError("alias negative accepted: " + label)
    river_contract = json.loads(RIVER_CONTRACT.read_text(encoding="utf-8"))
    partial = river_contract["v2_failed_partial"]
    river_item = next(item for item in alias["entries"] if item["creator_key"] == "riverwater")
    terminal = {"schema": "rok.world_river_water.editor_attempt.v3",
                "status": "created_pending_native_gpu_comparison_and_binding",
                "apply_requested": True, "ready_sha256": RIVER_READY_SHA,
                "contract_sha256": RIVER_CONTRACT_SHA,
                "v2_partial_reused_texture_sha256": partial["texture_sha256"],
                "v2_failed_material_protected_sha256": partial["failed_material_sha256"],
                "after": {"content_before_count": 2448, "content_after_count": 2449,
                          "actual_dll_count": 3,
                          "new_material": {"sha256": river_item["target_package_sha256"],
                                           "bytes": river_item["target_package_bytes"],
                                           "path": str(package_file(river_item["target_object"]))}},
                "created": [{"object": river_item["target_object"],
                             "sha256": river_item["target_package_sha256"],
                             "bytes": river_item["target_package_bytes"],
                             "instanced_usage": True, "compile_errors": [],
                             "texture_reused": partial["texture_object"],
                             "actual_rhi_format": "pending_native_readback",
                             "native_gpu_and_unity_parity_accepted": False,
                             "stage_bridge_and_channel_hits": {
                                 "vertex_interpolator": 1, "ba": 2, "alpha": 2,
                                 "spec": 1, "alpha_plus_spec": 1}}],
                "runtime_accepted": False, "visual_accepted": False,
                "full_migration_accepted": False}
    validate_river_v3_terminal(terminal, river_item, river_contract)
    river_mutations = {
        "v2_receipt_schema": lambda x: x.update(schema="rok.world_river_water.editor_attempt.v2"),
        "failed_partial_status": lambda x: x.update(status="failed_partial_do_not_replay"),
        "two_created_assets": lambda x: x["created"].append(copy.deepcopy(x["created"][0])),
        "wrong_v2_texture_pin": lambda x: x.update(v2_partial_reused_texture_sha256="0" * 64),
        "wrong_v2_failed_material_pin": lambda x: x.update(v2_failed_material_protected_sha256="0" * 64),
        "wrong_full_content_count": lambda x: x["after"].update(content_after_count=2448),
        "wrong_target_hash": lambda x: x["created"][0].update(sha256="0" * 64),
        "wrong_after_target_path": lambda x: x["after"]["new_material"].update(path="D:/elsewhere/foreign.uasset"),
        "shader_compile_error": lambda x: x["created"][0].update(compile_errors=["error"]),
    }
    for label, mutate in river_mutations.items():
        candidate = copy.deepcopy(terminal)
        mutate(candidate)
        try:
            validate_river_v3_terminal(candidate, river_item, river_contract)
        except (AssertionError, KeyError, TypeError, ValueError):
            continue
        raise AssertionError("V3 river terminal negative accepted: " + label)
    actual_river_item = next(item for item in json.loads(ALIAS_PATH.read_text(encoding="utf-8"))["entries"]
                             if item["creator_key"] == "riverwater")
    supplement_before = json.loads((ROOT / "Saved/RokUnityMigration/BatchReview20261010CommanderSkillReadonlyDetail/"
                                    "SkillBuildV5_20261009T191525Z/postbuild_baseline.json").read_text(encoding="utf-8"))
    supplement_after = json.loads((RIVER_SUPPLEMENT_DIR / "next_inputpins_v1.json").read_text(encoding="utf-8"))
    material_path = package_file(actual_river_item["target_object"])
    validate_supplement_sets(supplement_before, supplement_after, material_path,
                             actual_river_item["target_package_sha256"],
                             actual_river_item["target_package_bytes"])
    supplement_mutations = {
        "extra_actual_dll": lambda x: x["actual_dll_files"].append({
            "path": "D:/ueDemo/Rok/Binaries/Win64/UnrealEditor-Rok-999.dll",
            "sha256": "0" * 64, "bytes": 1}),
        "changed_actual_dll": lambda x: x["actual_dll_files"][0].update(sha256="0" * 64),
        "changed_source": lambda x: x["source_files"][0].update(sha256="0" * 64),
        "changed_accepted_asset": lambda x: x["accepted_asset_files"][0].update(sha256="0" * 64),
        "extra_content": lambda x: x["all_content_files"].append({
            "path": "D:/ueDemo/Rok/Content/Foreign.uasset", "sha256": "0" * 64, "bytes": 1}),
        "changed_old_content": lambda x: x["all_content_files"][0].update(sha256="0" * 64),
        "missing_v3_material": lambda x: x["all_content_files"].__setitem__(
            slice(None), [r for r in x["all_content_files"]
                          if Path(r["path"]).resolve() != material_path.resolve()]),
    }
    for label, mutate in supplement_mutations.items():
        bad_after = copy.deepcopy(supplement_after)
        mutate(bad_after)
        try:
            validate_supplement_sets(supplement_before, bad_after, material_path,
                                     actual_river_item["target_package_sha256"],
                                     actual_river_item["target_package_bytes"])
        except (AssertionError, KeyError, TypeError, ValueError):
            continue
        raise AssertionError("River root supplemental negative accepted: " + label)
    temp = tempfile.TemporaryDirectory(prefix="world_instanced_v2_selftest_", dir=HERE)
    manifest_path = (Path(temp.name) / "WorldInstancedMaterialDiagnosticRunsV10" /
                     "20261009_010203_1234_5678" / "manifest.json")
    manifest_path.parent.mkdir(parents=True)
    alias_sha = "a" * 64
    boundary = {"rok_world_streaming_diagnostic_flag": True,
                "rok_world_instanced_material_diagnostic_flag": True,
                "rok_local_only_flag": True, "server_subsystem_present": True,
                "local_only": True, "http_dispatch_attempts": 0,
                "http_suppressed_requests": 0}
    baseline = {"schema": "rok.world_instanced_material_diagnostic_runtime.v10",
                "passed": True, "game_mode": GAME_MODE,
                "alias_path": str(ALIAS_PATH).replace("\\", "/"),
                "alias_sha256": alias_sha,
                "variants_creator_terminal_sha256": "2" * 64,
                "river_creator_terminal_sha256": "2" * 64,
                "bridge_creator_terminal_sha256": "2" * 64,
                "diagnostic_directional_light_only": True,
                "render_proxy_material_verified": False,
                "formal_map_lod_accepted": False,
                "unity_pixel_parity_accepted": False,
                "unity_gamma_parity_accepted": False,
                "full_migration_accepted": False,
                "world": WORLD, "runtime_data_path": ASSET_PATH,
                "payload_sha256": PAYLOAD_SHA,
                "static_visual_ready": False, "runtime_ready": False,
                "screenshot_origin": "FScreenshotRequest_game_viewport",
                "run_directory": str(manifest_path.parent),
                "expected_viewport_width": 1440, "expected_viewport_height": 810,
                "runtime_data_snapshot": normalized_source_data(source),
                "begin_local_boundary": copy.deepcopy(boundary), "samples": [], **boundary}
    for i, (phase, expected) in enumerate(zip(PHASES, EXPECTED)):
        lod, x, y, instances = expected
        dxf = [288.316, 288.316, 2000, 3001, 2000][i]
        components = []
        if instances:
            n = 0
            for broad, group in expected_batches(source, lod, (x, y), dxf).items():
                for transforms in group["component_instances"].values():
                    actual_materials, alias_records, diagnostics = [], [], {}
                    for slot, original in enumerate(broad[2]):
                        entry = by_source.get(original)
                        require(entry is not None, "self-test unexpected source material")
                        target = entry["target_object"]
                        sha_value = entry["target_package_sha256"]
                        bytes_value = entry["target_package_bytes"]
                        creator_sha = alias["creator_receipts"][entry["creator_key"]]["terminal_sha256"]
                        actual_materials.append(target)
                        alias_records.append({"slot": slot, "source_object": original,
                            "observed_before_alias": original, "target_object": target,
                            "actual_object": target, "target_package_sha256": sha_value,
                            "target_package_bytes": bytes_value,
                            "creator_terminal_sha256": creator_sha,
                            "source_passthrough": False,
                            "render_proxy_material_verified": False,
                            "base_instanced_static_mesh_usage": True,
                            "native_shader_map_available": True,
                            "base_material": target})
                        diagnostics[f"material_diagnostic_{slot}"] = {
                            "path": target, "base_instanced_static_mesh_usage": True,
                            "base_material": target}
                    components.append({"name": f"WorldMapChunk_{broad[0][0]}_{broad[0][1]}_Batch_{n}_Gen_{i+1}",
                        "mesh": broad[1], "materials": actual_materials,
                        "material_aliases": alias_records,
                        "translucency_sort_priority": broad[3], "visible": True,
                        "attached_to_streaming_root": True, "render_state_created": True,
                        "nonzero_world_bounds": True,
                        "relative_transform": COMPONENT_RELATIVE,
                        "instance_transforms_read_all": True,
                        "instances_local": transforms, **diagnostics})
                    n += 1
        names = sorted(component["name"] for component in components)
        image_path = manifest_path.parent / (phase + ".png")
        image = synthetic_png_for_negative_test([30, 40, 50, 20, 60][i])
        image_path.write_bytes(image)
        _, _, image_bytes, image_sha, nonblack, _, pixel_sha = valid_png(image_path)
        sample = {"phase": phase, "lod": lod, "center_chunk_x": x,
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
            "camera_dxf": dxf, "camera_actor_transform": [0, 0, 0, 0, 0, 0, 1, 1, 1, 1],
            "camera_is_player_pawn": True, "camera_is_view_target": True,
            "camera_component_active": True, "streaming_root_static": True,
            "instanced_alias_loaded": True, "instanced_alias_sha256": alias_sha,
            "directional_light_count": 1, "sky_light_count": 0,
            "generation": i + 1, "game_frame": (i + 1) * 10,
            "stable_observations": 3, "screenshot_path": str(image_path),
            "screenshot_request_frame": (i + 1) * 10,
            "screenshot_processed_frame": (i + 1) * 10 + 1,
            "screenshot_observed_frame": (i + 1) * 10 + 2,
            "viewport_width": 1440, "viewport_height": 810,
            "png_width": 1440, "png_height": 810,
            "png_bytes": image_bytes, "png_sha256": image_sha,
            "png_decoded_rgba_sha256": pixel_sha,
            "png_nonblack_rgb_pixels": nonblack,
            "png_nonopaque_alpha_pixels": 0, **boundary}
        request_frame = sample["screenshot_request_frame"]
        readiness = {"alias_mutation_frame": request_frame - 5,
            "alias_mutation_serial": i + 1,
            "platform_allowed_to_render": True, "world_rendering_enabled": True,
            "game_player_count": 1, "world_view_submission_frame": request_frame - 1,
            "world_view_submission_step": i + 1, "world_view_count": 1,
            "world_view_rect_width": 1296, "world_view_rect_height": 810,
            "world_view_rect_min_x": 72, "world_view_rect_min_y": 0,
            "world_view_rect_max_x": 1368, "world_view_rect_max_y": 810,
            "full_viewport_width": 1440, "full_viewport_height": 810,
            "expected_view_rect_valid": True,
            "expected_view_rect_min_x": 72, "expected_view_rect_min_y": 0,
            "expected_view_rect_max_x": 1368, "expected_view_rect_max_y": 810,
            "camera_view_aspect_ratio": 16 / 10,
            "camera_view_constrain_aspect_ratio": True,
            "camera_effective_aspect_ratio": 16 / 10,
            "viewport_desired_aspect_ratio": 16 / 9,
            "world_view_rendering_showflag": True,
            "world_view_static_meshes_showflag": True,
            "world_view_instanced_static_meshes_showflag": True,
            "viewport_rendered_frame": request_frame - 1,
            "submitted_view_fence_armed": True,
            "submitted_view_fence_frame": request_frame - 2,
            "submitted_view_fence_step": i + 1,
            "submitted_view_fence_alias_serial": i + 1,
            "submitted_view_fence_complete": True,
            "camera_horizontal_fov": 18.395122528076172,
            "camera_aspect_ratio": 16 / 10,
            "camera_perspective": True}
        sample["pre_request_render_readiness"] = copy.deepcopy(readiness)
        readiness["world_view_submission_frame"] = sample["screenshot_processed_frame"]
        readiness["viewport_rendered_frame"] = sample["screenshot_processed_frame"]
        sample["processed_render_readiness"] = readiness
        state = {key: sample[key] for key in VISUAL_FIELDS}
        sample["processed_state"] = copy.deepcopy(state)
        sample["observed_state"] = copy.deepcopy(state)
        baseline["samples"].append(sample)
    empty_pixels = valid_png(manifest_path.parent / (PHASES[3] + ".png"))[5]
    for sample in baseline["samples"]:
        current_pixels = valid_png(Path(sample["screenshot_path"]))[5]
        changed, maximum = rgba_difference(current_pixels, empty_pixels)
        sample["png_changed_rgb_pixels_vs_empty"] = changed
        sample["png_max_rgb_diff_vs_empty"] = maximum
    verify_runtime(baseline, source, manifest_path, alias, by_source, alias_sha,
                   manifest_path.parent.parent)
    aliased_component = next(i for i, component in enumerate(
        baseline["samples"][0]["actual_hism_components"])
        if component["material_aliases"] and
        component["material_aliases"][0]["source_passthrough"] is False)
    direct_slot_mutations = {
        "old_original_material": lambda c: c["materials"].__setitem__(0,
            c["material_aliases"][0]["source_object"]),
        "old_usage": lambda c: c["material_aliases"][0].update(base_instanced_static_mesh_usage=False),
        "default_material_fallback": lambda c: c["material_diagnostic_0"].update(
            path="/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"),
        "uncompiled_shader_map": lambda c: c["material_aliases"][0].update(
            native_shader_map_available=False),
        "forged_receipt": lambda c: c["material_aliases"][0].update(
            creator_terminal_sha256="0" * 64),
        "wrong_source_key": lambda c: c["material_aliases"][0].update(
            source_object="/Game/Other.Other"),
    }
    for label, mutate in direct_slot_mutations.items():
        sample = copy.deepcopy(baseline["samples"][0])
        mutate(sample["actual_hism_components"][aliased_component])
        try:
            verify_actual_geometry(sample, source, 0, by_source, alias)
        except (AssertionError, KeyError, TypeError, ValueError):
            continue
        raise AssertionError("direct material-slot negative accepted: " + label)
    bridge_sample = copy.deepcopy(baseline["samples"][1])
    bridge_component = next(component for component in bridge_sample["actual_hism_components"]
                            if any(slot["source_object"] == BRIDGE_SOURCE
                                   for slot in component["material_aliases"]))
    bridge_slot = next(slot["slot"] for slot in bridge_component["material_aliases"]
                       if slot["source_object"] == BRIDGE_SOURCE)
    bridge_component["materials"][bridge_slot] = BRIDGE_SOURCE
    bridge_component["material_aliases"][bridge_slot]["actual_object"] = BRIDGE_SOURCE
    bridge_component["material_aliases"][bridge_slot]["source_passthrough"] = True
    try:
        verify_actual_geometry(bridge_sample, source, 1, by_source, alias)
    except (AssertionError, KeyError, TypeError, ValueError):
        pass
    else:
        raise AssertionError("actual bridge old-source passthrough accepted")
    mutations = {
        "wrong_game_mode": lambda x: x.update(game_mode="/Script/Rok.Other"),
        "bad_alias_sha": lambda x: x.update(alias_sha256="0" * 64),
        "foreign_run_directory": lambda x: x.update(run_directory="D:/ueDemo/Rok/Saved/OtherRun"),
        "bad_proof": lambda x: x.update(variants_creator_terminal_sha256="0" * 64),
        "missing_bridge_third_proof": lambda x: x.update(bridge_creator_terminal_sha256=""),
        "old_source_material": lambda x: x["samples"][0]["actual_hism_components"][0]["materials"].__setitem__(0,
            x["samples"][0]["actual_hism_components"][0]["material_aliases"][0]["source_object"]),
        "old_usage": lambda x: x["samples"][0]["actual_hism_components"][0]["material_aliases"][0].update(base_instanced_static_mesh_usage=False),
        "fallback": lambda x: x["samples"][0]["actual_hism_components"][0]["material_diagnostic_0"].update(path="/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"),
        "missing_shader_map": lambda x: x["samples"][0]["actual_hism_components"][0]["material_aliases"][0].update(native_shader_map_available=False),
        "wrong_source_recipe": lambda x: x["runtime_data_snapshot"]["recipes"][0][3][0][2].__setitem__(0, 999),
        "wrong_transform": lambda x: x["samples"][0]["actual_hism_components"][0]["instances_local"][0].__setitem__(0, 999),
        "no_light": lambda x: x["samples"][0].update(directional_light_count=0),
        "http": lambda x: x["samples"][1].update(http_dispatch_attempts=1),
        "wrong_camera": lambda x: x["samples"][2].update(camera_is_view_target=False),
        "lod2_not_empty": lambda x: x["samples"][3].update(hism_instance_count=1),
        "fake_proxy": lambda x: x.update(render_proxy_material_verified=True),
        "platform_render_disallowed": lambda x: x["samples"][0]["pre_request_render_readiness"].update(platform_allowed_to_render=False),
        "world_render_disabled": lambda x: x["samples"][0]["pre_request_render_readiness"].update(world_rendering_enabled=False),
        "zero_player_view": lambda x: x["samples"][0]["pre_request_render_readiness"].update(world_view_count=0),
        "instancing_showflag_off": lambda x: x["samples"][0]["processed_render_readiness"].update(world_view_instanced_static_meshes_showflag=False),
        "alias_after_fence": lambda x: x["samples"][0]["pre_request_render_readiness"].update(alias_mutation_frame=10),
        "fence_incomplete": lambda x: x["samples"][0]["pre_request_render_readiness"].update(submitted_view_fence_complete=False),
        "fence_wrong_phase": lambda x: x["samples"][1]["pre_request_render_readiness"].update(submitted_view_fence_step=1),
        "viewport_callback_without_submit": lambda x: x["samples"][0]["processed_render_readiness"].update(world_view_submission_frame=9),
        "late_callback_forgery": lambda x: x["samples"][0]["processed_render_readiness"].update(viewport_rendered_frame=12),
        "not_perspective": lambda x: x["samples"][0]["pre_request_render_readiness"].update(camera_perspective=False),
        "wrong_rect_origin": lambda x: x["samples"][0]["pre_request_render_readiness"].update(world_view_rect_min_x=0, world_view_rect_max_x=1296),
        "wrong_expected_rect_origin": lambda x: x["samples"][0]["pre_request_render_readiness"].update(expected_view_rect_min_x=0, expected_view_rect_max_x=1296),
        "wrong_rect_width": lambda x: x["samples"][0]["pre_request_render_readiness"].update(world_view_rect_width=1440),
        "rect_out_of_bounds": lambda x: x["samples"][0]["processed_render_readiness"].update(world_view_rect_max_x=1441),
        "wrong_component_aspect": lambda x: x["samples"][0]["pre_request_render_readiness"].update(camera_aspect_ratio=16 / 9),
        "wrong_view_aspect": lambda x: x["samples"][0]["pre_request_render_readiness"].update(camera_view_aspect_ratio=16 / 9),
        "wrong_effective_aspect": lambda x: x["samples"][0]["pre_request_render_readiness"].update(camera_effective_aspect_ratio=16 / 9),
        "unconstrained_camera": lambda x: x["samples"][0]["pre_request_render_readiness"].update(camera_view_constrain_aspect_ratio=False),
        "invalid_expected_rect": lambda x: x["samples"][0]["pre_request_render_readiness"].update(expected_view_rect_valid=False),
        "wrong_full_viewport": lambda x: x["samples"][0]["pre_request_render_readiness"].update(full_viewport_width=1296),
        "wrong_desired_aspect": lambda x: x["samples"][0]["pre_request_render_readiness"].update(viewport_desired_aspect_ratio=16 / 10),
        "wrong_fence_alias_serial": lambda x: x["samples"][0]["pre_request_render_readiness"].update(submitted_view_fence_alias_serial=999),
    }
    for label, mutate in mutations.items():
        candidate = copy.deepcopy(baseline)
        mutate(candidate)
        try:
            verify_runtime(candidate, source, manifest_path, alias, by_source, alias_sha,
                           manifest_path.parent.parent)
        except (AssertionError, KeyError, TypeError, ValueError):
            continue
        raise AssertionError("runtime negative accepted: " + label)
    active = manifest_path.parent / (PHASES[0] + ".png")
    original = active.read_bytes()
    black = synthetic_png_for_negative_test(0)
    active.write_bytes(black)
    bad = copy.deepcopy(baseline)
    bad_sample = bad["samples"][0]
    bad_sample.update(png_bytes=len(black), png_sha256=hashlib.sha256(black).hexdigest(),
                      png_nonblack_rgb_pixels=0)
    try:
        verify_runtime(bad, source, manifest_path, alias, by_source, alias_sha,
                       manifest_path.parent.parent)
    except (AssertionError, KeyError, TypeError, ValueError):
        pass
    else:
        raise AssertionError("black active viewport accepted")
    active.write_bytes(original)
    for label, replacement in {
        "same_pixels_different_zlib": synthetic_png_for_negative_test(20, compression=9),
        "same_pixels_different_metadata": synthetic_png_for_negative_test(
            20, metadata=b"identical pixels with different ancillary text"),
    }.items():
        active.write_bytes(replacement)
        _, _, image_bytes, image_sha, nonblack, pixels, pixel_sha = valid_png(active)
        require(image_sha != baseline["samples"][3]["png_sha256"]
                and pixels == empty_pixels and nonblack > 1024,
                "self-test did not construct identical nonblack pixels in different PNG bytes")
        bad = copy.deepcopy(baseline)
        bad["samples"][0].update(
            png_bytes=image_bytes, png_sha256=image_sha,
            png_decoded_rgba_sha256=pixel_sha,
            png_nonblack_rgb_pixels=nonblack,
            png_changed_rgb_pixels_vs_empty=0, png_max_rgb_diff_vs_empty=0)
        try:
            verify_runtime(bad, source, manifest_path, alias, by_source, alias_sha,
                           manifest_path.parent.parent)
        except (AssertionError, KeyError, TypeError, ValueError):
            pass
        else:
            raise AssertionError("same decoded pixels with different PNG bytes accepted: " + label)
    active.write_bytes(original)
    temp.cleanup()
    return {"alias_negative_rejected": len(alias_mutations),
            "runtime_negative_rejected": len(mutations) + 3,
            "direct_slot_negative_rejected": len(direct_slot_mutations) + 1,
            "river_terminal_negative_rejected": len(river_mutations),
            "river_supplement_negative_rejected": len(supplement_mutations)}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--created", type=Path, help="editor_create report after root-owned import")
    parser.add_argument("--runtime", type=Path, help="actual -game diagnostic manifest")
    parser.add_argument("--alias", type=Path, help="fixed Saved-only exact 24-pair alias JSON")
    parser.add_argument("--expected-alias-sha", help="external pinned alias JSON SHA256")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    result = verify_source()
    result.update(verify_stage_pins(runtime=args.runtime is not None))
    template = fixed_alias_template()
    result["fixed_alias_template_sha256"] = sha(ALIAS_TEMPLATE)
    result["fixed_alias_pairs"] = len(template["entries"])
    result["actual_alias_sha256"] = sha(ALIAS_PATH)
    actual_alias, actual_by_source = validate_alias_evidence(ALIAS_PATH, ACTUAL_ALIAS_SHA)
    require(len(actual_by_source) == 24 and actual_alias["creator_receipts"]["riverwater"]["terminal_sha256"]
            == RIVER_SUPPLEMENT_TERMINAL_SHA, "actual 24-alias proof differs")
    result["actual_creator_receipts_validated"] = True
    source = json.loads(SOURCE.read_text(encoding="utf-8"))
    if args.created:
        result.update(verify_created(args.created))
    alias = by_source = None
    if args.alias is not None:
        require(args.expected_alias_sha is not None, "external alias SHA required")
        alias, by_source = validate_alias_evidence(args.alias, args.expected_alias_sha)
        result["alias_sha256"] = sha(args.alias)
    if args.runtime:
        require(args.created is not None and alias is not None and by_source is not None,
                "runtime acceptance requires pinned --created and --alias evidence")
        verify_runtime(json.loads(args.runtime.read_text(encoding="utf-8")), source,
                       args.runtime.resolve(), alias, by_source, result["alias_sha256"])
        result["runtime_manifest_sha256"] = sha(args.runtime)
        result["runtime_passed"] = True
    if args.self_test:
        result["rejected_negative_cases"] = negative_cases(source)
    result["passed"] = True
    print(json.dumps(result, ensure_ascii=False, sort_keys=True, indent=2))


if __name__ == "__main__":
    main()
