"""Offline V12 single-tree alias pixel coverage verifier; no Unreal writes.

The unflagged five-phase path remains the published V10 contract. This file
checks the opt-in V12 A1/A2/B/A3 capture and its real full-size PNG bytes.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path("D:/ueDemo/Rok")
HERE = ROOT / "Saved/PendingWorldAliasPixelCoverageDiagnosticV3"
V10 = ROOT / "Saved/PendingWorldInstancedVisibilityDiagnosticV10"
sys.path.insert(0, str(V10))
from rok_data_paths import resolve
import VerifyRokWorldInstancedMaterialDiagnosticV10 as base  # noqa: E402

READY = HERE / "ready_for_root_review.json"
V10_READY = V10 / "ready_for_root_review.json"
V10_READY_SHA = "f001d9dd654635a713eacb0ab8067173a5498bcca0a45c65d7db552826fcb7ba"
NATIVE_V10 = ROOT / ("Saved/RokUnityMigration/WorldInstancedMaterialDiagnosticRunsV10/"
                     "20261009_234943_10748_1386755666032/manifest.json")
NATIVE_V10_SHA = "6735691eec166c998af22714c35f1f5dc508463bfb9d602c890fb53bb3dc9e76"
RUN_FOLDER = "WorldAliasPixelCoverageDiagnosticRunsV12"
SCHEMA = "rok.world_alias_pixel_coverage_diagnostic_runtime.v12"
PHASES = ["coverage_a1", "coverage_a2", "coverage_b_target_hidden", "coverage_a3_restored"]
TARGET_COMPONENT = "WorldMapChunk_16_2_Batch_193_Gen_0"
TARGET_MESH = ("/Game/RokUnityWorldMapPrototype/Meshes/"
               "SM_RokWorld_Grove_co_tree_04_adorning.SM_RokWorld_Grove_co_tree_04_adorning")
TARGET_SOURCE = ("/Game/RokUnityWorldMapPrototype/Materials/"
                 "M_WorldSource_map_tree_nomask_1.M_WorldSource_map_tree_nomask_1")
TARGET_ALIAS = ("/Game/RokDiagnostics/WorldStreaming/InstancedMaterialVariantsV1/"
                "MIV1_M_WorldSource_map_tree_nomask_1.MIV1_M_WorldSource_map_tree_nomask_1")
FOCUS = (5810.3, 939.4)
RECT = (72, 0, 1368, 810)
DELTA = 16
MIN_CONNECTED = 64
MAX_NOISE = 2


def require(value: bool, message: str) -> None:
    if not value:
        raise AssertionError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_source_target(source: dict) -> dict:
    groups = base.expected_batches(source, 0, (16, 2), 288.31601)
    key = ((16, 2), TARGET_MESH, (TARGET_SOURCE,), 0)
    require(key in groups and len(groups[key]["component_instances"]) == 1,
            "target batch not derived uniquely from the pinned Unity streaming source")
    instances = next(iter(groups[key]["component_instances"].values()))
    require(len(instances) == 7 and all(abs(a - b) < .05 for a, b in
            zip(instances[5][:3], (-532850.0005, -1299.999, 50))),
            "target instance index/transform differs from the pinned source recipe")
    projection = source["Projection"]
    require(projection["UnityRebaseOrigin"] == {"X": 6876.0, "Y": 942.0}
            and projection["UeUnitsPerUnityUnit"] == 500.0
            and abs(6876 - (-instances[5][0]) / 500 - FOCUS[0]) < .001
            and abs(942 + instances[5][1] / 500 - FOCUS[1]) < .001,
            "registered source focus did not derive from source projection")
    return {"source_tree_chunk": [16, 2], "source_tree_batch_instances": 7,
            "source_tree_instance_index": 5, "source_tree_unity_focus": FOCUS}


def verify_stage(*, runtime: bool) -> dict:
    ready = json.loads(READY.read_text(encoding="utf-8"))
    require(ready["schema"] == "rok.world_alias_pixel_coverage_diagnostic_stage.v12"
            and ready["status"] == "READY_FOR_INDEPENDENT_STATIC_REVIEW_ONLY"
            and ready["predecessor_v10_ready_sha256"] == V10_READY_SHA
            and sha(V10_READY) == V10_READY_SHA
            and ready["native_v10_manifest_sha256"] == NATIVE_V10_SHA
            and sha(NATIVE_V10) == NATIVE_V10_SHA
            and ready["explicit_opt_in_flag"] == "-RokWorldAliasPixelCoverageDiagnostic"
            and ready["unflagged_five_phase_contract"] == [848, 1688, 1688, 0, 1688]
            and ready["target_source_focus_unity"] == list(FOCUS)
            and ready["target_component"] == TARGET_COMPONENT
            and ready["target_mesh"] == TARGET_MESH
            and ready["target_source_material"] == TARGET_SOURCE
            and ready["target_alias_material"] == TARGET_ALIAS
            and ready["coverage_rgb_delta_threshold"] == DELTA
            and ready["coverage_min_connected_pixels"] == MIN_CONNECTED
            and ready["noise_max_rgb"] == MAX_NOISE
            and ready["full_viewport_extent"] == [1440, 810]
            and ready["source_constrained_view_rect"] == list(RECT)
            and ready["alias_count"] == 24
            and ready["lod01_dependencies"] == 243
            and ready["lod2_missing_dependencies"] == 249
            and ready["ue_build_or_runtime_passed"] is False
            and ready["formal_map_lod_accepted"] is False
            and ready["full_migration_accepted"] is False,
            "frozen V12 stage contract changed")
    for row in ready["stage_files"]:
        path = resolve(row["path"])
        require(path.is_file() and path.stat().st_size == row["bytes"]
                and sha(path) == row["sha256"], "staged bytes changed: " + row["path"])
    expected_source_paths = {
        "Source/Rok/RokWorldMapCameraPawn.h",
        "Source/Rok/RokWorldMapCameraPawn.cpp",
        "Source/Rok/RokWorldMapStreamingActor.h",
        "Source/Rok/RokWorldMapStreamingActor.cpp",
    }
    require({row["path"] for row in ready["read_only_source_pins"]} == expected_source_paths,
            "four source camera/streaming pins changed")
    for row in ready["read_only_source_pins"]:
        path = resolve(row["path"])
        require(path.is_file() and path.stat().st_size == row["bytes"]
                and sha(path) == row["sha256"], "source camera/streaming drift: " + row["path"])
    if not runtime:
        require({row["path"] for row in ready["pre_integration_live_game_mode_pins"]} == {
            "Source/Rok/RokWorldInstancedMaterialDiagnosticGameMode.h",
            "Source/Rok/RokWorldInstancedMaterialDiagnosticGameMode.cpp"},
            "pre-integration V10 GameMode base pin paths changed")
        for row in ready["pre_integration_live_game_mode_pins"]:
            path = resolve(row["path"])
            require(path.is_file() and path.stat().st_size == row["bytes"]
                    and sha(path) == row["sha256"], "V10 live base drift before root integration")
    if runtime:
        for filename, live in (("RokWorldInstancedMaterialDiagnosticGameMode.h",
                                "Source/Rok/RokWorldInstancedMaterialDiagnosticGameMode.h"),
                               ("RokWorldInstancedMaterialDiagnosticGameMode.cpp",
                                "Source/Rok/RokWorldInstancedMaterialDiagnosticGameMode.cpp"),
                               ("VerifyRokWorldAliasPixelCoverageDiagnosticV12.py",
                                "Scripts/VerifyRokWorldAliasPixelCoverageDiagnosticV12.py")):
            require(sha(HERE / filename) == sha(resolve(live)),
                    "live implementation/verifier differs from frozen V12 candidate: " + live)
    return {"stage_sha256": sha(READY), "native_v10_sha256": sha(NATIVE_V10)}


def dot(a: list[float], b: list[float]) -> float:
    return sum(x * y for x, y in zip(a, b))


def transform_point(t: list[float], p: list[float]) -> list[float]:
    rotated = base.rotate(t[3:7], [p[i] * t[7 + i] for i in range(3)])
    return [t[i] + rotated[i] for i in range(3)]


def project_box(local_min: list[float], local_max: list[float], instance: list[float],
                camera: list[float], fov: float, aspect: float) -> tuple[int, int, int, int] | None:
    require(len(local_min) == len(local_max) == 3 and len(instance) == len(camera) == 10
            and all(math.isfinite(float(x)) for x in local_min + local_max + instance + camera)
            and 0 < fov < 180 and abs(aspect - 1.6) < .001,
            "invalid source mesh/camera bounds")
    q = camera[3:7]
    forward = base.rotate(q, [1, 0, 0])
    right = base.rotate(q, [0, 1, 0])
    up = base.rotate(q, [0, 0, 1])
    tan_h = math.tan(math.radians(fov) / 2)
    tan_v = tan_h / aspect
    xs, ys, depths = [], [], []
    for x in (local_min[0], local_max[0]):
        for y in (local_min[1], local_max[1]):
            for z in (local_min[2], local_max[2]):
                point = transform_point(instance, [x, y, z])
                d = [point[i] - camera[i] for i in range(3)]
                depth = dot(d, forward)
                depths.append(depth)
                if depth <= 1:
                    continue
                xs.append(RECT[0] + (1 + dot(d, right) / (depth * tan_h)) *
                          (RECT[2] - RECT[0]) / 2)
                ys.append(RECT[1] + (1 - dot(d, up) / (depth * tan_v)) *
                          (RECT[3] - RECT[1]) / 2)
    if all(depth <= 1 for depth in depths):
        return None
    require(all(depth > 1 for depth in depths), "mesh bounds straddle near plane")
    return math.floor(min(xs)), math.floor(min(ys)), math.ceil(max(xs)), math.ceil(max(ys))


def overlaps(a: tuple[int, int, int, int], b: tuple[int, int, int, int]) -> bool:
    return a[0] < b[2] and a[2] > b[0] and a[1] < b[3] and a[3] > b[1]


def verify_projection(report: dict) -> tuple[int, int, int, int]:
    require(report["target_component"] == TARGET_COMPONENT
            and report["target_mesh"] == TARGET_MESH
            and report["target_source_material"] == TARGET_SOURCE
            and report["target_alias_material"] == TARGET_ALIAS
            and report["target_source_focus_unity"] == list(FOCUS)
            and report["target_instance_index"] == 5
            and report["other_six_same_hism_instances_excluded_from_roi"] is True,
            "pre-registered source tree identity changed")
    t = report["target_instance_world_transform"]
    require(len(t) == 10 and abs(t[0] - 532850) < .1
            and abs(t[1] + 1300) < .1 and abs(t[2] - 50) < .1,
            "target world transform does not derive from Unity projection")
    bounds_min = report["target_mesh_local_bounds_min"]
    bounds_max = report["target_mesh_local_bounds_max"]
    require(all(a < b for a, b in zip(bounds_min, bounds_max)),
            "source mesh local bounds invalid")
    camera = report["registered_camera_view_transform"]
    crop = report["registered_camera_view_asymmetric_crop_fraction"]
    require(len(crop) == 4 and all(abs(float(x) - 1.0) <= .001 for x in crop),
            "source camera asymmetric crop changed after ROI registration")
    require(abs(camera[0] - 532850) < .1
            and abs(camera[1] + 10119.9079) < .2
            and abs(camera[2] - 8819.9079) < .2,
            "registered camera is not the source-derived LOD0 tree view")
    rect = project_box(bounds_min, bounds_max, t, camera,
                       report["registered_camera_view_fov"],
                       report["registered_camera_view_aspect"])
    require(rect is not None and list(rect) == report["target_projected_rect_xyxy"]
            == report["target_roi_xyxy"]
            and RECT[0] <= rect[0] < rect[2] <= RECT[2]
            and RECT[1] <= rect[1] < rect[3] <= RECT[3]
            and (rect[2] - rect[0]) * (rect[3] - rect[1]) >= MIN_CONNECTED,
            "ROI was clipped, shifted, changed or projected after screenshots")
    others = report["other_six_same_hism_instance_world_transforms"]
    require(len(others) == 6, "same HISM did not account for its other six instances")
    for other in others:
        other_rect = project_box(bounds_min, bounds_max, other, camera,
                                 report["registered_camera_view_fov"],
                                 report["registered_camera_view_aspect"])
        require(other_rect is None or not overlaps(other_rect, rect),
                "another hidden HISM instance intersects pre-registered target ROI")
    return rect


def normalized_state(state: dict) -> dict:
    result = copy.deepcopy(state)
    result["hism_visible_component_count"] = result["hism_component_count"]
    target = [c for c in result["actual_hism_components"]
              if c["name"] == TARGET_COMPONENT]
    require(len(target) == 1, "target HISM absent or ambiguous")
    target[0]["visible"] = True
    return result


def verify_restoration_flag(report: dict) -> None:
    require(report["target_restore_attempted"] is True and report["target_restored"] is True,
            "target HISM was not restored before final report")


def verify_projection_modifiers(projection: dict, report: dict) -> None:
    require(projection["view_rect_xyxy"] == list(RECT)
            and projection["camera_view_transform"] ==
                report["registered_camera_view_transform"]
            and abs(projection["fov"] - report["registered_camera_view_fov"]) < 1e-6
            and abs(projection["aspect"] - report["registered_camera_view_aspect"]) < 1e-6,
            "registered source camera projection changed")
    require(projection["off_center_projection_offset"] == [0, 0]
            and projection["overscan_resolution_fraction"] == 1
            and projection["crop_fraction"] == 1
            and len(projection["asymmetric_crop_fraction"]) == 4
            and projection["asymmetric_crop_fraction"] == [1, 1, 1, 1]
            and projection["uniform_overscan"] == 0
            and projection["private_asymmetric_overscan_matrix_neutral"] is True,
            "unsupported FMinimalViewInfo projection modifier is non-neutral")
    actual = projection["actual_projection_matrix"]
    neutral = projection["neutral_projection_matrix"]
    require(len(actual) == len(neutral) == 16
            and all(math.isfinite(x) and math.isfinite(y) and abs(x - y) <= 1e-6
                    for x, y in zip(actual, neutral)),
            "private asymmetric overscan changes the actual projection matrix")


PIN_FIELDS = {
    "candidate_header_sha256": "RokWorldInstancedMaterialDiagnosticGameMode.h",
    "candidate_cpp_sha256": "RokWorldInstancedMaterialDiagnosticGameMode.cpp",
    "candidate_verifier_sha256": "VerifyRokWorldAliasPixelCoverageDiagnosticV12.py",
    "camera_header_sha256": "Source/Rok/RokWorldMapCameraPawn.h",
    "camera_cpp_sha256": "Source/Rok/RokWorldMapCameraPawn.cpp",
    "streaming_header_sha256": "Source/Rok/RokWorldMapStreamingActor.h",
    "streaming_cpp_sha256": "Source/Rok/RokWorldMapStreamingActor.cpp",
}


def expected_capture_pins(ready: dict, report: dict) -> dict:
    stage = {Path(row["path"]).name: row["sha256"] for row in ready["stage_files"]}
    sources = {row["path"]: row["sha256"] for row in ready["read_only_source_pins"]}
    pins = {}
    for field, path in PIN_FIELDS.items():
        pins[field] = sources[path] if path.startswith("Source/") else stage[path]
    pins.update(runtime_payload_sha256=base.PAYLOAD_SHA,
                alias_input_sha256=base.ACTUAL_ALIAS_SHA,
                variants_creator_receipt_sha256=report["variants_creator_terminal_sha256"],
                river_creator_receipt_sha256=report["river_creator_terminal_sha256"],
                bridge_creator_receipt_sha256=report["bridge_creator_terminal_sha256"])
    return pins


def exact_ticks(value: object) -> int:
    require(isinstance(value, str) and re.fullmatch(r"[1-9]\d*", value) is not None,
            "capture tick must be a lossless positive decimal string")
    return int(value)


def verify_capture_provenance(report: dict, run: Path, ready: dict) -> None:
    match = re.fullmatch(r"(\d{8}_\d{6})_([1-9]\d*)_([1-9]\d*)", run.name)
    require(match is not None and report["run_id"] == run.name
            and report["process_id"] == match.group(2)
            and report["run_start_ticks"] == match.group(3),
            "run ID/PID/ticks do not match the native run directory")
    expected_pins = expected_capture_pins(ready, report)
    require(report["capture_source_pins"] == expected_pins,
            "run candidate/source/alias pins differ from frozen bytes")
    projection = report["registered_projection_modifiers"]
    verify_projection_modifiers(projection, report)
    previous_ticks = exact_ticks(report["run_start_ticks"])
    require(len(report["samples"]) == 4, "four ordered capture identities missing")
    for index, sample in enumerate(report["samples"]):
        capture_id = f"{run.name}_{index + 1:02d}"
        image_name = f"{PHASES[index]}_{capture_id}.png"
        require(sample["phase"] == PHASES[index]
                and sample["run_id"] == run.name
                and sample["process_id"] == report["process_id"]
                and sample["run_start_ticks"] == report["run_start_ticks"]
                and sample["capture_id"] == capture_id
                and type(sample["capture_seq"]) is int
                and sample["capture_seq"] == index + 1
                and sample["capture_source_pins"] == expected_pins
                and Path(sample["screenshot_path"]).name == image_name
                and Path(sample["screenshot_path"]).parent.resolve() == run.resolve()
                and len(sample["png_sha256"]) == 64
                and len(sample["png_decoded_rgba_sha256"]) == 64,
                f"capture {index}: full PNG identity, ordered sequence or source pin differs")
        ticks = [exact_ticks(sample[key]) for key in
                 ("capture_request_ticks", "capture_processed_ticks",
                  "capture_observed_ticks")]
        require(previous_ticks < ticks[0] < ticks[1] < ticks[2],
                f"capture {index}: source process clock ticks are stale or swapped")
        previous_ticks = ticks[2]
        for key in ("registered_projection_modifiers",
                    "pre_request_projection_modifiers",
                    "processed_projection_modifiers",
                    "observed_projection_modifiers"):
            require(sample[key] == projection,
                    f"capture {index}: {key} changed during renderer/fence/PNG")


def verify_state_matrix(report: dict, source: dict, alias: dict, by_source: dict) -> None:
    samples = report["samples"]
    require(len(samples) == 4 and [s["phase"] for s in samples] == PHASES,
            "A1/A2/B/A3 phases missing or reordered")
    verify_restoration_flag(report)
    baseline = None
    last_observed = -1
    serials = []
    for index, sample in enumerate(samples):
        visible = index != 2
        require(sample["coverage_opt_in_flag"] is True
                and sample["coverage_target_component"] == TARGET_COMPONENT
                and sample["coverage_target_instance_index"] == 5
                and sample["coverage_target_visible"] is visible
                and sample["coverage_roi_xyxy"] == report["target_roi_xyxy"]
                and sample["coverage_projected_rect_xyxy"] == report["target_projected_rect_xyxy"],
                f"phase {index}: target visibility or pre-registered ROI drift")
        base.verify_local_only_boundary(sample, f"coverage phase {index}")
        base.verify_render_readiness(sample, index)
        request, processed, observed = (sample[key] for key in (
            "screenshot_request_frame", "screenshot_processed_frame", "screenshot_observed_frame"))
        require(sample["game_frame"] == request <= processed < observed
                and request > last_observed and sample["stable_observations"] >= 3,
                f"phase {index}: stale capture or insufficient stable frames")
        last_observed = observed
        serials.append(sample["pre_request_render_readiness"]["alias_mutation_serial"])
        state = {key: sample[key] for key in base.VISUAL_FIELDS + ("coverage_opt_in_flag",)}
        require(sample["processed_state"] == sample["observed_state"] == state,
                f"phase {index}: camera/HISM/HTTP changed during processed screenshot")
        target = [c for c in sample["actual_hism_components"]
                  if c["name"] == TARGET_COMPONENT]
        require(len(target) == 1 and target[0]["visible"] is visible
                and target[0]["mesh"] == TARGET_MESH
                and len(target[0]["instances_local"]) == 7
                and target[0]["materials"] == [TARGET_ALIAS]
                and target[0]["material_aliases"][0]["source_object"] == TARGET_SOURCE,
                f"phase {index}: wrong source tree alias, geometry or visibility")
        local = target[0]["instances_local"][5]
        require(all(abs(a - b) < .05 for a, b in zip(local[:3],
                                                     (-532850.0005, -1299.999, 50))),
                "target instance no longer derives from exact source transform")
        require((sample["lod"], sample["center_chunk_x"], sample["center_chunk_y"],
                 sample["hism_instance_count"]) == (0, 16, 2, 1688)
                and sample["hism_visible_component_count"] ==
                    sample["hism_component_count"] - (0 if visible else 1)
                and sample["generation"] == report["target_streaming_generation"]
                and abs(sample["camera_unity_x"] - FOCUS[0]) < .001
                and abs(sample["camera_unity_y"] - FOCUS[1]) < .001
                and abs(sample["camera_dxf"] - 288.31601) < .001
                and sample["camera_is_player_pawn"] is True
                and sample["camera_is_view_target"] is True
                and sample["camera_component_active"] is True
                and sample["directional_light_count"] == 1
                and sample["sky_light_count"] == 0
                and sample["http_dispatch_attempts"] == 0,
                f"phase {index}: source camera, generation, light or HTTP drift")
        normalized = normalized_state(state)
        if baseline is None:
            baseline = normalized
        require(normalized == baseline,
                f"phase {index}: camera/streaming transforms/aliases/lighting changed across A/B")
        base.verify_actual_geometry(normalized, source, 1, by_source, alias)
    require(serials[0] == serials[1] < serials[2] < serials[3],
            "target visibility changes lack new alias serial/fence epochs")


def pixel_delta(a: bytes, b: bytes, roi: tuple[int, int, int, int],
                threshold: int) -> tuple[int, int, int]:
    require(len(a) == len(b) == 1440 * 810 * 4, "partial decoded PNG")
    x0, y0, x1, y1 = roi
    require(0 <= x0 < x1 <= 1440 and 0 <= y0 < y1 <= 810, "invalid ROI bounds")
    width, height = x1 - x0, y1 - y0
    mask = bytearray(width * height)
    maximum = count = 0
    for y in range(height):
        for x in range(width):
            offset = ((y + y0) * 1440 + x + x0) * 4
            diff = max(abs(a[offset + c] - b[offset + c]) for c in range(3))
            maximum = max(maximum, diff)
            if diff >= threshold:
                mask[y * width + x] = 1
                count += 1
    largest = 0
    for seed in range(len(mask)):
        if mask[seed] != 1:
            continue
        queue = [seed]
        mask[seed] = 2
        head = 0
        while head < len(queue):
            cur = queue[head]
            head += 1
            px, py = cur % width, cur // width
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    nx, ny = px + dx, py + dy
                    if (dx or dy) and 0 <= nx < width and 0 <= ny < height:
                        index = ny * width + nx
                        if mask[index] == 1:
                            mask[index] = 2
                            queue.append(index)
        largest = max(largest, len(queue))
    return maximum, count, largest


def verify_pixel_matrix(report: dict, pixels: list[bytes],
                        roi: tuple[int, int, int, int]) -> dict:
    require(len(pixels) == 4 and report["noise_max_rgb"] == MAX_NOISE
            and report["coverage_rgb_delta_threshold"] == DELTA
            and report["coverage_min_connected_pixels"] == MIN_CONNECTED,
            "pixel thresholds changed after capture")
    noise = pixel_delta(pixels[0], pixels[1], roi, DELTA)
    hidden = pixel_delta(pixels[0], pixels[2], roi, DELTA)
    restored = pixel_delta(pixels[0], pixels[3], roi, DELTA)
    require(noise[0] <= MAX_NOISE and restored[0] <= MAX_NOISE,
            "A1/A2 capture noise or A3 restoration differs")
    require(hidden[1] >= MIN_CONNECTED and hidden[2] >= MIN_CONNECTED,
            "nonblack view lacks >=64 connected source-tree alias pixels in registered ROI")
    require(report["a1_a2_roi_max_rgb_diff"] == noise[0]
            and report["a1_b_roi_threshold_pixels"] == hidden[1]
            and report["a1_b_roi_largest_connected_pixels"] == hidden[2]
            and report["a1_a3_roi_max_rgb_diff"] == restored[0],
            "native pixel measurements do not match full-size PNGs")
    return {"a1_a2_max": noise[0], "a1_b_threshold_pixels": hidden[1],
            "a1_b_largest_connected": hidden[2], "a1_a3_max": restored[0]}


def verify_runtime(path: Path, source: dict, alias: dict, by_source: dict,
                   ready: dict) -> dict:
    report = json.loads(path.read_text(encoding="utf-8"))
    require(report["schema"] == SCHEMA and report["passed"] is True
            and report["single_tree_alias_pixel_coverage_verified"] is True
            and report["coverage_opt_in_flag"] is True
            and report["game_mode"] == base.GAME_MODE
            and report["world"] == base.WORLD
            and report["runtime_data_path"] == base.ASSET_PATH
            and report["payload_sha256"] == base.PAYLOAD_SHA
            and report["alias_sha256"] == base.ACTUAL_ALIAS_SHA
            and report["alias_path"] == str(base.ALIAS_PATH).replace("\\", "/")
            and report["formal_map_lod_accepted"] is False
            and report["full_migration_accepted"] is False
            and report["unity_pixel_parity_accepted"] is False
            and report["unity_gamma_parity_accepted"] is False
            and report["render_proxy_material_verified"] is False
            and report["static_visual_ready"] is False
            and report["runtime_ready"] is False,
            "V12 cannot promote full world, 24 aliases or Unity parity")
    require(report["expected_viewport_width"] == 1440
            and report["expected_viewport_height"] == 810
            and report["screenshot_origin"] == "FScreenshotRequest_game_viewport",
            "full native viewport proof missing")
    base.verify_local_only_boundary(report["begin_local_boundary"], "coverage BeginPlay")
    base.verify_local_only_boundary(report, "coverage Finish")
    require(report["begin_local_boundary"]["coverage_opt_in_flag"] is True
            and report["http_dispatch_attempts"] == 0
            and report["http_suppressed_requests"] == 0,
            "explicit opt-in/local HTTP boundary missing")
    run = base.normalize_run_directory(report["run_directory"])
    require(path.resolve() == (run / "manifest.json").resolve()
            and run.parent.name == RUN_FOLDER
            and re.fullmatch(r"\d{8}_\d{6}_\d+_\d+", run.name),
            "V12 native run folder or manifest path differs")
    verify_capture_provenance(report, run, ready)
    base.compare_source_records(report["runtime_data_snapshot"],
                                base.normalized_source_data(source))
    require(report["variants_creator_terminal_sha256"] ==
            alias["creator_receipts"]["variants"]["terminal_sha256"]
            and report["river_creator_terminal_sha256"] ==
            alias["creator_receipts"]["riverwater"]["terminal_sha256"]
            and report["bridge_creator_terminal_sha256"] ==
            alias["creator_receipts"]["bridge"]["terminal_sha256"],
            "24-alias source/target creator chain changed")
    roi = verify_projection(report)
    verify_state_matrix(report, source, alias, by_source)
    images = []
    for index, sample in enumerate(report["samples"]):
        image = Path(sample["screenshot_path"])
        require(image.is_absolute() and image.parent.resolve() == run
                and image.name == PHASES[index] + "_" + sample["capture_id"] + ".png",
                "screenshot escaped run folder or phase order")
        width, height, count, digest, nonblack, pixels, decoded_sha = base.valid_png(image)
        require((width, height, count, digest, nonblack, decoded_sha) ==
                (sample["png_width"], sample["png_height"], sample["png_bytes"],
                 sample["png_sha256"], sample["png_nonblack_rgb_pixels"],
                 sample["png_decoded_rgba_sha256"])
                and sample["png_nonopaque_alpha_pixels"] == 0
                and nonblack > 1024, "full-size real PNG bytes or nonblack gate failed")
        images.append(pixels)
    metrics = verify_pixel_matrix(report, images, roi)
    return {"manifest_sha256": sha(path), "single_alias": TARGET_ALIAS,
            "roi_xyxy": roi, "pixel_metrics": metrics}


def expect_reject(label: str, call) -> str:
    try:
        call()
    except (AssertionError, KeyError, ValueError, FileNotFoundError):
        return label
    raise AssertionError("negative case was accepted: " + label)


def negative_cases() -> list[str]:
    # Same per-pixel checks as actual native PNG path; all pixels are nonblack sand.
    sand = bytes((194, 158, 113, 255)) * (1440 * 810)
    roi = (400, 300, 420, 320)
    hidden = bytearray(sand)
    for y in range(304, 314):
        for x in range(404, 414):
            off = (y * 1440 + x) * 4
            hidden[off] = 150
    good = [sand, sand, bytes(hidden), sand]
    report = {"noise_max_rgb": MAX_NOISE, "coverage_rgb_delta_threshold": DELTA,
              "coverage_min_connected_pixels": MIN_CONNECTED,
              "a1_a2_roi_max_rgb_diff": 0, "a1_b_roi_threshold_pixels": 100,
              "a1_b_roi_largest_connected_pixels": 100,
              "a1_a3_roi_max_rgb_diff": 0}
    require(verify_pixel_matrix(report, good, roi)["a1_b_largest_connected"] == 100,
            "positive synthetic pixel fixture failed")
    rejected = []
    rejected.append(expect_reject("nonblack_sand_target_zero_pixels",
                                  lambda: verify_pixel_matrix(report, [sand] * 4, roi)))
    noisy = bytearray(sand)
    noisy[(305 * 1440 + 405) * 4] += 3
    rejected.append(expect_reject("a1_a2_noise_gt2",
                                  lambda: verify_pixel_matrix(report,
                                                              [sand, bytes(noisy), bytes(hidden), sand], roi)))
    rejected.append(expect_reject("a3_not_restored",
                                  lambda: verify_pixel_matrix(report,
                                                              [sand, sand, bytes(hidden), bytes(hidden)], roi)))
    rejected.append(expect_reject("partial_decoded_png",
                                  lambda: verify_pixel_matrix(report,
                                                              [sand[:-4], sand, bytes(hidden), sand], roi)))
    with tempfile.TemporaryDirectory(dir=HERE) as temp:
        path = Path(temp) / "truncated.png"
        path.write_bytes(base.synthetic_png_for_negative_test(194)[:-11])
        rejected.append(expect_reject("truncated_png_file", lambda: base.valid_png(path)))
    # Projection checker is also the real runtime gate, not a display-only ROI.
    # For the structural negatives use a small camera-aligned synthetic mesh.
    fixture = {"target_component": TARGET_COMPONENT, "target_mesh": TARGET_MESH,
               "target_source_material": TARGET_SOURCE, "target_alias_material": TARGET_ALIAS,
               "target_source_focus_unity": list(FOCUS), "target_instance_index": 5,
               "other_six_same_hism_instances_excluded_from_roi": True,
               "target_instance_world_transform": [532850, -1300, 50, 0, 0, 0, 1, 2.273, 2.273, 2.273],
               "target_mesh_local_bounds_min": [-100, -100, -100],
               "target_mesh_local_bounds_max": [100, 100, 100],
               "registered_camera_view_transform": [532850, -10119.9079, 8819.9079,
                                                    -0.27059805, 0.27059805, .65328148, .65328148, 1, 1, 1],
               "registered_camera_view_fov": 18.3951225,
               "registered_camera_view_aspect": 1.6,
               "registered_camera_view_asymmetric_crop_fraction": [1, 1, 1, 1],
               "other_six_same_hism_instance_world_transforms": [
                   [800000 + n * 10000, -1300, 50, 0, 0, 0, 1, 2.273, 2.273, 2.273]
                   for n in range(6)]}
    computed = project_box(fixture["target_mesh_local_bounds_min"],
                           fixture["target_mesh_local_bounds_max"],
                           fixture["target_instance_world_transform"],
                           fixture["registered_camera_view_transform"],
                           fixture["registered_camera_view_fov"], 1.6)
    require(computed is not None, "synthetic projection fixture not in view")
    fixture["target_projected_rect_xyxy"] = list(computed)
    fixture["target_roi_xyxy"] = list(computed)
    verify_projection(fixture)
    moved = copy.deepcopy(fixture)
    moved["registered_camera_view_transform"][0] += 1000
    rejected.append(expect_reject("moved_camera_changes_preregistered_roi",
                                  lambda: verify_projection(moved)))
    cropped = copy.deepcopy(fixture)
    cropped["registered_camera_view_asymmetric_crop_fraction"][0] = .9
    rejected.append(expect_reject("changed_asymmetric_crop",
                                  lambda: verify_projection(cropped)))
    wrong_roi = copy.deepcopy(fixture)
    wrong_roi["target_roi_xyxy"][0] += 1
    rejected.append(expect_reject("wrong_roi_origin",
                                  lambda: verify_projection(wrong_roi)))
    wrong_alias = copy.deepcopy(fixture)
    wrong_alias["target_alias_material"] = "/Game/Foreign.Bad"
    rejected.append(expect_reject("wrong_alias", lambda: verify_projection(wrong_alias)))
    no_restore = copy.deepcopy(fixture)
    no_restore["target_instance_index"] = 4
    rejected.append(expect_reject("wrong_source_instance",
                                  lambda: verify_projection(no_restore)))
    rejected.append(expect_reject("no_restoration_flag",
                                  lambda: verify_restoration_flag({"target_restore_attempted": True,
                                                                   "target_restored": False})))
    # Reuse V10's real submitted-view/fence predicate with a concrete saved sample.
    native = json.loads(NATIVE_V10.read_text(encoding="utf-8"))["samples"][1]
    native["pre_request_render_readiness"]["world_view_submission_step"] = 1
    native["pre_request_render_readiness"]["submitted_view_fence_step"] = 1
    native["processed_render_readiness"]["world_view_submission_step"] = 1
    native["processed_render_readiness"]["submitted_view_fence_step"] = 1
    base.verify_render_readiness(native, 0)
    stale = copy.deepcopy(native)
    stale["pre_request_render_readiness"]["submitted_view_fence_alias_serial"] -= 1
    rejected.append(expect_reject("stale_fence_serial",
                                  lambda: base.verify_render_readiness(stale, 0)))
    projection = {
        "view_rect_xyxy": list(RECT),
        "camera_view_transform": fixture["registered_camera_view_transform"],
        "fov": fixture["registered_camera_view_fov"],
        "aspect": fixture["registered_camera_view_aspect"],
        "off_center_projection_offset": [0, 0],
        "overscan_resolution_fraction": 1,
        "crop_fraction": 1,
        "asymmetric_crop_fraction": [1, 1, 1, 1],
        "uniform_overscan": 0,
        "actual_projection_matrix": [1] * 16,
        "neutral_projection_matrix": [1] * 16,
        "private_asymmetric_overscan_matrix_neutral": True,
    }
    verify_projection_modifiers(projection, fixture)
    for field, value in (("off_center_projection_offset", [.01, 0]),
                         ("overscan_resolution_fraction", 1.01),
                         ("crop_fraction", .9),
                         ("asymmetric_crop_fraction", [.9, 1, 1, 1]),
                         ("uniform_overscan", .1),
                         ("actual_projection_matrix", [2] + [1] * 15)):
        altered = copy.deepcopy(projection)
        altered[field] = value
        rejected.append(expect_reject("projection_" + field,
                                      lambda p=altered: verify_projection_modifiers(p, fixture)))
    ready = json.loads(READY.read_text(encoding="utf-8"))
    run = ROOT / "Saved/RokUnityMigration/WorldAliasPixelCoverageDiagnosticRunsV12" / (
        "20261010_010203_12345_9007199254740993")
    provenance = {
        "run_id": run.name, "process_id": "12345",
        "run_start_ticks": "9007199254740993",
        "variants_creator_terminal_sha256": "a" * 64,
        "river_creator_terminal_sha256": "b" * 64,
        "bridge_creator_terminal_sha256": "c" * 64,
        "registered_projection_modifiers": projection,
        "registered_camera_view_transform": fixture["registered_camera_view_transform"],
        "registered_camera_view_fov": fixture["registered_camera_view_fov"],
        "registered_camera_view_aspect": fixture["registered_camera_view_aspect"],
    }
    provenance["capture_source_pins"] = expected_capture_pins(ready, provenance)
    provenance["samples"] = []
    for index, phase in enumerate(PHASES):
        identity = f"{run.name}_{index + 1:02d}"
        sample = {
            "phase": phase, "run_id": run.name, "process_id": "12345",
            "run_start_ticks": "9007199254740993", "capture_id": identity,
            "capture_seq": index + 1,
            "capture_request_ticks": str(9007199254741000 + index * 10),
            "capture_processed_ticks": str(9007199254741001 + index * 10),
            "capture_observed_ticks": str(9007199254741002 + index * 10),
            "capture_source_pins": copy.deepcopy(provenance["capture_source_pins"]),
            "screenshot_path": str(run / f"{phase}_{identity}.png"),
            "png_sha256": "d" * 64,
            "png_decoded_rgba_sha256": "e" * 64,
        }
        for key in ("registered_projection_modifiers",
                    "pre_request_projection_modifiers",
                    "processed_projection_modifiers",
                    "observed_projection_modifiers"):
            sample[key] = copy.deepcopy(projection)
        provenance["samples"].append(sample)
    verify_capture_provenance(provenance, run, ready)
    for label, change in (
        ("swapped_capture_identity", lambda p: p["samples"][0].update(
            capture_id=p["samples"][1]["capture_id"])),
        ("swapped_full_png_path", lambda p: p["samples"][0].update(
            screenshot_path=p["samples"][1]["screenshot_path"])),
        ("wrong_process_id", lambda p: p["samples"][2].update(process_id="9")),
        ("lossy_numeric_ticks", lambda p: p["samples"][1].update(
            capture_processed_ticks=9007199254741011)),
        ("source_pin_drift", lambda p: p["samples"][1]["capture_source_pins"].update(
            camera_cpp_sha256="f" * 64)),
        ("candidate_pin_drift", lambda p: p["capture_source_pins"].update(
            candidate_cpp_sha256="f" * 64)),
        ("capture_projection_drift", lambda p: p["samples"][2][
            "processed_projection_modifiers"].update(crop_fraction=.8)),
    ):
        bad = copy.deepcopy(provenance)
        change(bad)
        rejected.append(expect_reject(label,
                                      lambda p=bad: verify_capture_provenance(p, run, ready)))
    return rejected


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runtime", type=Path, help="native V12 coverage manifest after root build/run")
    parser.add_argument("--baseline-runtime", type=Path,
                        help="new unflagged five-phase V10 manifest from V12 code")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    result = base.verify_source()
    result.update(verify_stage(runtime=args.runtime is not None or args.baseline_runtime is not None))
    alias, by_source = base.validate_alias_evidence(base.ALIAS_PATH, base.ACTUAL_ALIAS_SHA)
    require(len(by_source) == 24 and by_source[TARGET_SOURCE]["target_object"] == TARGET_ALIAS,
            "source tree is outside the frozen 24-alias chain")
    result["alias_count"] = len(by_source)
    source = json.loads(base.SOURCE.read_text(encoding="utf-8"))
    result["source_target"] = verify_source_target(source)
    if args.baseline_runtime:
        base.verify_runtime(json.loads(args.baseline_runtime.read_text(encoding="utf-8")),
                            source, args.baseline_runtime.resolve(), alias, by_source,
                            base.ACTUAL_ALIAS_SHA)
        result["unflagged_five_phase_manifest_sha256"] = sha(args.baseline_runtime)
    if args.runtime:
        result["coverage_runtime"] = verify_runtime(args.runtime.resolve(), source,
                                                    alias, by_source,
                                                    json.loads(READY.read_text(encoding="utf-8")))
    if args.self_test:
        result["rejected_negative_cases"] = negative_cases()
    result["passed"] = True
    print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
