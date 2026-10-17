"""Strict external review of an opt-in live renderer audit; never equates it to Unity pixel parity."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
from pathlib import Path

from PIL import Image
from rok_data_paths import DATA_ROOT, resolve


ROOT = Path("D:/ueDemo/Rok")
PENDING = ROOT / "Saved/PendingUnityTileRippleRendererV14"
ASSET_SOURCE = DATA_ROOT / "TileRippleRendererV8"
READY = PENDING / "ready_for_root_review.json"
ASSET_MANIFEST = ASSET_SOURCE / "asset_import_manifest.json"
GEOMETRY = ASSET_SOURCE / "geometry_contract.json"
CPU = DATA_ROOT / "TileRippleSimulationV2/tile_ripple_source_config.json"
TIMES = [2.5, 4.9, 5.1, 7.5]
MAX_LAG = [0.08, 0.08, 0.08, 0.10]
PRIVATE_NAMES = ["baseline", "bowen_only", "bowen1_only", "combined"]
PRIVATE_EXTENT = 512
TEXTURE = "/Game/RokUnityWorldMapPrototype/TileRipple/T_Ring_07_Source.T_Ring_07_Source"
MATERIAL = "/Game/RokUnityWorldMapPrototype/TileRipple/M_RokTileRippleSourceAlpha.M_RokTileRippleSourceAlpha"
PNG_MAGIC = bytes.fromhex("89504e470d0a1a0a")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def exact_keys(value: dict, keys: set[str], label: str) -> None:
    require(set(value) == keys, f"{label} keys differ")


def verify_asset_manifest(expected_sha: str) -> dict:
    require(len(expected_sha) == 64 and sha(ASSET_MANIFEST) == expected_sha,
            "externally locked asset manifest SHA differs")
    data = json.loads(ASSET_MANIFEST.read_text(encoding="utf-8"))
    require(data.get("schema") == "rok.tile_ripple.diagnostic_assets.v1", "asset manifest schema")
    require(Path(data.get("project", "")).resolve() == ROOT.resolve(), "asset manifest project")
    require(data.get("map") == "/Game/Maps/L_RokUnityWorldMapPrototype", "asset manifest map")
    require(data.get("source_png_sha256") == "1112dbcab4e82c35afb4686153aff77b0ae1397695bc466f99db24ccea7d1237",
            "source Ring_07 PNG pin")
    require(data.get("source_meta_sha256") == "d2676b0686754f92d55e4d2b9b39cee0850b6597f4e9bf4e8f9cdd3d4d176a6e",
            "source Ring_07 importer meta pin")
    require(sha(ROOT / "Scripts/CreateRokTileRippleDiagnosticAssets.py") == data.get("creation_script_sha256"),
            "asset creation script changed")
    expected = ["Content/RokUnityWorldMapPrototype/TileRipple/T_Ring_07_Source.uasset",
                "Content/RokUnityWorldMapPrototype/TileRipple/M_RokTileRippleSourceAlpha.uasset"]
    packages = data.get("packages")
    require(isinstance(packages, list) and len(packages) == 2, "exactly two imported assets")
    for item, relative in zip(packages, expected, strict=True):
        require(item.get("path") == relative, "unexpected imported package")
        path = resolve(relative)
        require(path.is_file() and path.stat().st_size == item.get("byte_count")
                and sha(path) == item.get("sha256"), f"imported package changed: {relative}")
    return data


def verify_state(state: dict, previous_sim: float = -1.0) -> float:
    require(state.get("actor_ready") is True and state.get("http_zero") is True,
            "actor/local HTTP boundary")
    require(state.get("map") == "/Game/Maps/L_RokUnityWorldMapPrototype", "world map")
    require(state.get("actor_center_on_screen") is True, "diagnostic center off screen")
    require(state.get("texture_rhi_ready") is True and state.get("texture_path") == TEXTURE,
            "source texture RHI")
    require(state.get("texture_width") == 128 and state.get("texture_height") == 128
            and state.get("rhi_width") == 128 and state.get("rhi_height") == 128
            and state.get("texture_srgb") is True and isinstance(state.get("rhi_format"), int)
            and state.get("rhi_mips", 0) >= 1, "source texture dimensions/descriptor")
    require(state.get("viewport_width", 0) >= 512 and state.get("viewport_height", 0) >= 256,
            "viewport size")
    emitters = state.get("emitters")
    require(isinstance(emitters, list) and len(emitters) == 2, "two emitter states")
    for i, emitter in enumerate(emitters):
        n = emitter.get("live_count", 0)
        require(emitter.get("index") == i and emitter.get("source_name") ==
                ("bowen" if i == 0 else "bowen (1)"), "emitter source identity")
        require(type(n) is int and 0 < n <= 1000 and emitter.get("vertex_count") == 4 * n
                and emitter.get("index_count") == 6 * n and emitter.get("render_state_created") is True
                and emitter.get("visible") is True and emitter.get("material_path") == MATERIAL
                and isinstance(emitter.get("component_path"), str)
                and emitter["component_path"].endswith("BowenSourceMesh" if i == 0 else "Bowen1SourceMesh")
                and isinstance(emitter.get("owner_path"), str)
                and emitter["owner_path"] in emitter["component_path"],
                f"emitter {i} live procedural mesh")
    sim = state.get("sim_seconds")
    require(type(sim) in (float, int) and sim >= previous_sim and sim < 50, "simulation time")
    return sim


def verify_scope(data: dict) -> None:
    require(data.get("schema") == "rok.tile_ripple.renderer_diagnostic.v3", "runtime schema")
    require(data.get("complete") is True and data.get("passed") is True and data.get("failure") == "",
            "runtime did not complete")
    require(all(data.get(flag) is False for flag in
                ("unity_pixel_parity_accepted", "map_lod_accepted", "unity_rng_fixedstep_accepted")),
            "diagnostic scope overstated")


def pixel_metrics(frames: list[bytes]) -> dict[str, int]:
    require(len(frames) == 4 and all(len(frame) == PRIVATE_EXTENT * PRIVATE_EXTENT * 4
                                     for frame in frames), "four complete private BGRA frames")
    baseline = frames[0]
    spread = max(abs(baseline[i + channel] - baseline[channel])
                 for i in range(4, len(baseline), 4) for channel in range(3))

    def changed(a: bytes, b: bytes) -> int:
        return sum(max(abs(a[i + channel] - b[i + channel]) for channel in range(3)) >= 8
                   for i in range(0, len(a), 4))

    return {
        "baseline_rgb_spread": spread,
        "bowen_vs_baseline_pixels_over7": changed(frames[0], frames[1]),
        "bowen1_vs_baseline_pixels_over7": changed(frames[0], frames[2]),
        "between_emitters_pixels_over7": changed(frames[1], frames[2]),
        "combined_vs_bowen_pixels_over7": changed(frames[1], frames[3]),
        "combined_vs_bowen1_pixels_over7": changed(frames[2], frames[3]),
    }


def require_visible_metrics(metrics: dict[str, int]) -> None:
    require(metrics["baseline_rgb_spread"] <= 2
            and metrics["bowen_vs_baseline_pixels_over7"] >= 64
            and metrics["bowen1_vs_baseline_pixels_over7"] >= 64
            and metrics["between_emitters_pixels_over7"] >= 64
            and metrics["combined_vs_bowen_pixels_over7"] >= 32
            and metrics["combined_vs_bowen1_pixels_over7"] >= 32,
            "private GPU isolated emitter visibility gate")


def require_sample_window(target: float, lag: float, requested: float, processed: float) -> None:
    require(target <= requested <= target + lag and
            requested <= processed <= target + lag,
            "natural sample outside exact time window")


def verify_private_capture_identity(entry: dict, expected_paths: list[str], owner: str,
                                    stable_camera: dict | None) -> dict:
    require(entry.get("show_only_component_paths") == expected_paths
            and entry.get("show_only_component_count") == len(expected_paths)
            and entry.get("show_only_actor_count") == 0,
            "private ShowOnly identity/list differs")
    require(entry.get("capture_owner_path") == owner
            and isinstance(entry.get("capture_component_path"), str)
            and entry["capture_component_path"].startswith(owner)
            and entry.get("capture_projection") == "Orthographic"
            and entry.get("capture_source") == "FinalColorLDR"
            and entry.get("primitive_render_mode") == "UseShowOnlyList"
            and entry.get("target_rhi_extent") == {"x": PRIVATE_EXTENT, "y": PRIVATE_EXTENT},
            "private capture component/camera/target identity differs")
    flags = entry.get("capture_show_flags")
    require(isinstance(flags, dict) and set(flags) == {
            "post_processing", "tonemapper", "grain", "vignette",
            "anti_aliasing", "temporal_aa", "atmosphere"}
            and all(type(v) is bool for v in flags.values())
            and flags["post_processing"] is False and flags["tonemapper"] is True
            and type(entry.get("postprocess_blend_weight")) in (int, float)
            and entry["postprocess_blend_weight"] == 0
            and entry.get("inherits_main_view_postprocess") is False,
            "private capture postprocess isolation/provenance differs")
    camera_keys = ("capture_show_flags", "postprocess_blend_weight",
                   "inherits_main_view_postprocess",
                   "capture_world_location", "capture_world_rotation", "capture_ortho_width",
                   "source_bounds_center", "source_bounds_size", "target_rhi_extent",
                   "capture_component_path", "capture_owner_path", "capture_projection", "capture_source")
    camera = {key: entry.get(key) for key in camera_keys}
    require(all(camera[key] is not None for key in camera_keys), "private camera metadata incomplete")
    for key in ("capture_world_location", "capture_world_rotation", "source_bounds_center",
                "source_bounds_size"):
        require(isinstance(camera[key], dict) and set(camera[key]) == {"x", "y", "z"}
                and all(type(v) in (int, float) and math.isfinite(v) for v in camera[key].values()),
                f"private {key} invalid")
    bounds = camera["source_bounds_size"]
    center = camera["source_bounds_center"]
    location = camera["capture_world_location"]
    rotation = camera["capture_world_rotation"]
    width = camera["capture_ortho_width"]
    require(all(v >= 0 for v in bounds.values()) and max(bounds.values()) > 1
            and max(bounds.values()) < 100000
            and type(width) in (int, float) and math.isfinite(width)
            and abs(width - max(600, max(bounds["x"], bounds["y"]) * 1.4)) < .05
            and abs(location["x"] - center["x"]) < .01
            and abs(location["y"] - center["y"]) < .01
            and abs(location["z"] - center["z"] - 1000) < .01
            and abs(rotation["x"] + 90) < .01
            and abs(rotation["y"]) < .01 and abs(rotation["z"]) < .01,
            "private camera transform/orthographic extent not source-bounds-derived")
    if stable_camera is not None:
        require(camera == stable_camera, "private variants used different camera/target")
    return camera


def verify_private_target_diagnostic(value: dict) -> None:
    require(isinstance(value, dict), "private target diagnostic absent")
    require(value.get("requested_extent") == {"x": PRIVATE_EXTENT, "y": PRIVATE_EXTENT}
            and value.get("requested_format") == 2 and value.get("requested_samples") == 1,
            "private target request changed")
    require(value.get("synchronized_after_update") is True
            and value.get("resource_present") is True and value.get("rhi_valid") is True,
            "private target resource/synchronization absent")
    require(value.get("actual_extent") == {"x": PRIVATE_EXTENT, "y": PRIVATE_EXTENT}
            and value.get("actual_format") == 2 and value.get("actual_samples") == 1,
            "private target actual RHI descriptor differs")


def verify_private_gpu(data: dict, run: Path) -> dict[str, int]:
    verify_private_target_diagnostic(data.get("private_target_diagnostic"))
    entries = data.get("private_gpu_frames")
    require(isinstance(entries, list) and len(entries) == 4, "four private GPU readbacks")
    frames: list[bytes] = []
    frozen_time = None
    emitters = data["final_state"]["emitters"]
    component_paths = [row["component_path"] for row in emitters]
    require(len(component_paths) == 2 and component_paths[0] != component_paths[1]
            and emitters[0]["owner_path"] == emitters[1]["owner_path"],
            "two distinct live components must share one diagnostic actor")
    owner = emitters[0]["owner_path"]
    stable_camera = None
    for index, entry in enumerate(entries):
        name = PRIVATE_NAMES[index]
        require(entry.get("variant") == name and entry.get("width") == PRIVATE_EXTENT
                and entry.get("height") == PRIVATE_EXTENT and entry.get("rhi_format") == 2
                and entry.get("rhi_resource_ready") is True
                and entry.get("gpu_readpixels_succeeded") is True, f"private GPU descriptor: {name}")
        expected_paths = [] if index == 0 else [component_paths[0]] if index == 1 else [component_paths[1]] if index == 2 else component_paths
        stable_camera = verify_private_capture_identity(entry, expected_paths, owner, stable_camera)
        time = entry.get("frozen_sim_seconds")
        require(type(time) in (float, int) and 7.5 <= time <= 7.6,
                f"private frozen simulation time: {name}")
        if frozen_time is None:
            frozen_time = time
        require(time == frozen_time, "private variants used different simulation state")
        raw = Path(entry.get("raw_bgra8", ""))
        png = Path(entry.get("png", ""))
        require(raw.parent.resolve() == run.resolve() and raw.name == f"{name}.gpu.bgra8.bin"
                and png.parent.resolve() == run.resolve() and png.name == f"{name}.gpu.png",
                f"private output path: {name}")
        require(raw.is_file() and png.is_file()
                and raw.stat().st_size == PRIVATE_EXTENT * PRIVATE_EXTENT * 4
                and raw.stat().st_size == entry.get("raw_bytes")
                and png.stat().st_size == entry.get("png_bytes")
                and sha(raw) == entry.get("raw_sha256") and sha(png) == entry.get("png_sha256")
                and png.read_bytes()[:8] == PNG_MAGIC, f"private output bytes/hash: {name}")
        bgra = raw.read_bytes()
        with Image.open(png) as image:
            require(image.format == "PNG" and image.size == (PRIVATE_EXTENT, PRIVATE_EXTENT),
                    f"private PNG format/dimensions: {name}")
            rgba = image.convert("RGBA").tobytes()
        require(all(bgra[i:i + 4] == bytes((rgba[i + 2], rgba[i + 1], rgba[i], rgba[i + 3]))
                    for i in range(0, len(bgra), 4)), f"private raw/PNG pixel mismatch: {name}")
        frames.append(bgra)
    metrics = pixel_metrics(frames)
    require(data.get("private_gpu_metrics") == metrics, "private GPU metrics differ from raw")
    require_visible_metrics(metrics)
    require("no GPU vertex-buffer readback" in data.get("private_gpu_proof_scope", ""),
            "private GPU proof scope overstated")
    return metrics


def verify_manifest(data: dict, run: Path, ready: dict, expected_asset_sha: str,
                    expected_ready_sha: str,
                    *, check_png: bool = True) -> dict:
    verify_scope(data)
    require(Path(data.get("project", "")).resolve() == ROOT.resolve()
            and type(data.get("pid")) is int and data["pid"] > 0, "runtime project/PID")
    require(data.get("world_map") == "/Game/Maps/L_RokUnityWorldMapPrototype", "runtime map")
    require(data.get("geometry_fixture_sha256") == sha(GEOMETRY) == ready["geometry_contract_sha256"],
            "geometry fixture SHA")
    require(data.get("cpu_fixture_sha256") == sha(CPU) == ready["cpu_fixture_sha256"],
            "CPU source fixture SHA")
    require(data.get("expected_asset_manifest_sha256") == expected_asset_sha ==
            data.get("asset_manifest_sha256"), "externally supplied asset SHA")
    require(data.get("expected_ready_sha256") == expected_ready_sha ==
            data.get("ready_sha256") == sha(READY), "externally supplied ready SHA")
    pins = ready["staged_files"]
    for relative in ("Source/Rok/RokUnityTileRippleRenderer.cpp",
                     "Source/Rok/RokUnityTileRippleRendererAuditSubsystem.cpp"):
        name = "renderer_cpp_sha256" if "Audit" not in relative else "audit_cpp_sha256"
        require(data.get(name) == pins[relative] == sha(resolve(relative)), f"runtime code pin: {relative}")
    captures = data.get("captures")
    require(isinstance(captures, list) and len(captures) == 4, "four natural screenshot captures")
    previous_frame, previous_sim = -1, -1.0
    pngs = []
    for index, item in enumerate(captures):
        require(item.get("index") == index and item.get("target_sim_seconds") == TIMES[index],
                "capture order/time")
        require(item.get("max_lag_seconds") == MAX_LAG[index], "capture upper lag contract")
        require(item.get("mesh_render_fence_complete") is True, "render fence not completed")
        requested = item.get("requested_frame")
        processed = item.get("processed_frame")
        require(type(requested) in (int, float) and type(processed) in (int, float) and
                requested > previous_frame and processed >= requested, "screenshot frame ordering")
        req_sim = verify_state(item["requested_state"], previous_sim)
        pro_sim = verify_state(item["processed_state"], req_sim)
        require_sample_window(TIMES[index], MAX_LAG[index], req_sim, pro_sim)
        verify_state(item["next_frame_state"], pro_sim)
        previous_frame, previous_sim = processed, pro_sim
        png = Path(item.get("png", ""))
        require(png.parent.resolve() == run.resolve() and png.name == f"{index:02d}_tile_ripple.png",
                "PNG output path outside unique run")
        if check_png:
            require(png.is_file() and png.read_bytes()[:8] == PNG_MAGIC and
                    sha(png) == item.get("png_sha256") and png.stat().st_size == item.get("png_bytes"),
                    "PNG bytes/hash")
            with Image.open(png) as image:
                image.verify()
                require(image.width >= 512 and image.height >= 256, "PNG dimensions")
        pngs.append(str(png))
    verify_state(data["final_state"], previous_sim)
    metrics = verify_private_gpu(data, run)
    return {"passed": True, "scope": "real two-emitter UE procedural mesh and screenshot diagnostic",
            "captures": 4, "private_gpu_readbacks": 4, "private_gpu_metrics": metrics,
            "pngs": pngs, "unity_pixel_parity_accepted": False,
            "map_lod_accepted": False, "unity_rng_fixedstep_accepted": False}


def verify_run(run: Path, expected_asset_sha: str, expected_ready_sha: str) -> dict:
    require(len(expected_ready_sha) == 64 and sha(READY) == expected_ready_sha,
            "externally locked ready report SHA differs")
    ready = json.loads(READY.read_text(encoding="utf-8"))
    require(ready.get("schema") == "rok.tile_ripple.renderer_pending_review.v14", "ready schema")
    for relative, expected in ready["staged_files"].items():
        require(sha(resolve(relative)) == expected, f"applied code differs: {relative}")
    verify_asset_manifest(expected_asset_sha)
    data = json.loads((run / "manifest.json").read_text(encoding="utf-8"))
    return verify_manifest(data, run, ready, expected_asset_sha, expected_ready_sha)


def self_test() -> list[str]:
    fixture = {
        "schema": "rok.tile_ripple.renderer_diagnostic.v3", "complete": True, "passed": True,
        "failure": "", "unity_pixel_parity_accepted": False, "map_lod_accepted": False,
        "unity_rng_fixedstep_accepted": False,
    }
    state = {
        "actor_ready": True, "http_zero": True, "map": "/Game/Maps/L_RokUnityWorldMapPrototype",
        "actor_center_on_screen": True, "texture_rhi_ready": True, "texture_path": TEXTURE,
        "texture_width": 128, "texture_height": 128, "rhi_width": 128, "rhi_height": 128,
        "texture_srgb": True, "rhi_format": 2, "rhi_mips": 8,
        "viewport_width": 1440, "viewport_height": 810, "sim_seconds": 2.5,
        "emitters": [{"index": i, "source_name": "bowen" if i == 0 else "bowen (1)",
                      "live_count": 2, "vertex_count": 8, "index_count": 12,
                      "render_state_created": True, "visible": True, "material_path": MATERIAL,
                      "owner_path": "/Game/Maps/DiagnosticActor_0",
                      "component_path": "/Game/Maps/DiagnosticActor_0." +
                                        ("BowenSourceMesh" if i == 0 else "Bowen1SourceMesh")}
                     for i in range(2)],
    }
    verify_scope(fixture)
    verify_state(state)
    require_sample_window(4.9, 0.08, 4.91, 4.96)
    baseline = bytes((0, 0, 0, 255)) * (PRIVATE_EXTENT * PRIVATE_EXTENT)
    a = bytearray(baseline)
    b = bytearray(baseline)
    both = bytearray(baseline)
    for pixel in range(96):
        a[pixel * 4] = 80
        both[pixel * 4] = 80
    for pixel in range(96, 192):
        b[pixel * 4 + 1] = 90
        both[pixel * 4 + 1] = 90
    good = pixel_metrics([baseline, bytes(a), bytes(b), bytes(both)])
    require(good["baseline_rgb_spread"] == 0 and
            good["bowen_vs_baseline_pixels_over7"] == 96 and
            good["bowen1_vs_baseline_pixels_over7"] == 96 and
            good["combined_vs_bowen_pixels_over7"] == 96 and
            good["combined_vs_bowen1_pixels_over7"] == 96,
            "private GPU pixel self-test expected counts")
    require_visible_metrics(good)
    camera = {
        "capture_component_path": "/Game/Maps/DiagnosticActor_0.SceneCaptureComponent2D_0",
        "capture_owner_path": "/Game/Maps/DiagnosticActor_0",
        "capture_projection": "Orthographic", "capture_source": "FinalColorLDR",
        "primitive_render_mode": "UseShowOnlyList", "target_rhi_extent": {"x": 512, "y": 512},
        "capture_show_flags": {"post_processing": False, "tonemapper": True,
                               "grain": True, "vignette": True,
                               "anti_aliasing": True, "temporal_aa": True,
                               "atmosphere": True},
        "postprocess_blend_weight": 0.0, "inherits_main_view_postprocess": False,
        "capture_world_location": {"x": 10, "y": 20, "z": 1030},
        "capture_world_rotation": {"x": -90, "y": 0, "z": 0},
        "capture_ortho_width": 600,
        "source_bounds_center": {"x": 10, "y": 20, "z": 30},
        "source_bounds_size": {"x": 20, "y": 30, "z": 5},
        "show_only_component_paths": [state["emitters"][0]["component_path"]],
        "show_only_component_count": 1, "show_only_actor_count": 0,
    }
    verify_private_capture_identity(camera, camera["show_only_component_paths"],
                                    camera["capture_owner_path"], None)
    rejected: list[str] = []
    for label, mutation in (("false_passing", {"passed": False}),
                            ("false_scope", {"unity_pixel_parity_accepted": True}),
                            ("wrong_schema", {"schema": "other"})):
        candidate = copy.deepcopy(fixture)
        candidate.update(mutation)
        try:
            verify_scope(candidate)
        except ValueError:
            rejected.append(label)
    for label, mutation in (("no_texture_rhi", {"texture_rhi_ready": False}),
                            ("bad_vertex_count", {"emitters": [dict(state["emitters"][0], vertex_count=7),
                                                              state["emitters"][1]]}),
                            ("wrong_material", {"emitters": [dict(state["emitters"][0], material_path="/Game/Other"),
                                                              state["emitters"][1]]})):
        candidate = copy.deepcopy(state)
        candidate.update(mutation)
        try:
            verify_state(candidate)
        except ValueError:
            rejected.append(label)
    require(len(rejected) == 6, "negative self-test")
    for label, frames in (("all_clear_private_frame", [baseline] * 4),
                          ("same_isolated_emitters", [baseline, bytes(a), bytes(a), bytes(a)])):
        try:
            require_visible_metrics(pixel_metrics(frames))
        except ValueError:
            rejected.append(label)
    for label, target, lag, requested, processed in (
            ("late_request", 4.9, 0.08, 4.99, 5.0),
            ("late_processed", 5.1, 0.08, 5.11, 5.19),
            ("early_request", 2.5, 0.08, 2.49, 2.52)):
        try:
            require_sample_window(target, lag, requested, processed)
        except ValueError:
            rejected.append(label)
    require(len(rejected) == 11, "time/private GPU negative self-test")
    for label, update in (
        ("wrong_show_only_identity", {"show_only_component_paths": [state["emitters"][1]["component_path"]]}),
        ("extra_show_only_actor", {"show_only_actor_count": 1}),
        ("camera_drift", {"capture_world_location": {"x": 11, "y": 20, "z": 1030}}),
        ("projection_drift", {"capture_projection": "Perspective"}),
        ("extent_drift", {"target_rhi_extent": {"x": 256, "y": 512}}),
        ("postprocessing_reenabled", {"capture_show_flags": dict(camera["capture_show_flags"], post_processing=True)}),
        ("tonemapper_disabled", {"capture_show_flags": dict(camera["capture_show_flags"], tonemapper=False)}),
        ("postprocess_blend_enabled", {"postprocess_blend_weight": 1.0}),
        ("main_view_postprocess_inherited", {"inherits_main_view_postprocess": True}),
    ):
        bad = copy.deepcopy(camera)
        bad.update(update)
        try:
            verify_private_capture_identity(bad, camera["show_only_component_paths"],
                                            camera["capture_owner_path"], None)
        except ValueError:
            rejected.append(label)
    require(len(rejected) == 20, "private identity/camera negative self-test")
    different_camera = copy.deepcopy(camera)
    different_camera["capture_component_path"] += "_other"
    try:
        verify_private_capture_identity(different_camera, camera["show_only_component_paths"],
                                        camera["capture_owner_path"],
                                        {key: camera[key] for key in (
                                            "capture_world_location", "capture_world_rotation",
                                            "capture_ortho_width", "source_bounds_center", "source_bounds_size",
                                            "target_rhi_extent", "capture_component_path", "capture_owner_path",
                                            "capture_projection", "capture_source",
                                            "capture_show_flags", "postprocess_blend_weight",
                                            "inherits_main_view_postprocess")})
    except ValueError:
        rejected.append("cross_variant_camera_identity")
    require(len(rejected) == 21, "cross-variant camera negative self-test")
    target = {
        "requested_extent": {"x": 512, "y": 512}, "requested_format": 2,
        "requested_samples": 1, "synchronized_after_update": True,
        "resource_present": True, "rhi_valid": True,
        "actual_extent": {"x": 512, "y": 512}, "actual_format": 2,
        "actual_samples": 1,
    }
    verify_private_target_diagnostic(target)
    try:
        verify_private_target_diagnostic(None)
    except ValueError:
        rejected.append("target_diagnostic_missing")
    require(len(rejected) == 22, "missing private target diagnostic negative self-test")
    for label, field, bad_value in (
        ("target_missing_resource", "resource_present", False),
        ("target_missing_rhi", "rhi_valid", False),
        ("target_unsynchronized", "synchronized_after_update", False),
        ("target_wrong_extent", "actual_extent", {"x": 256, "y": 512}),
        ("target_wrong_format", "actual_format", 7),
        ("target_multisampled", "actual_samples", 4),
    ):
        candidate = copy.deepcopy(target)
        candidate[field] = bad_value
        try:
            verify_private_target_diagnostic(candidate)
        except ValueError:
            rejected.append(label)
    require(len(rejected) == 28, "private target descriptor negative self-test")
    return rejected


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path)
    parser.add_argument("--expected-asset-manifest-sha")
    parser.add_argument("--expected-ready-sha")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    result: dict = {"self_test_rejected": self_test()} if args.self_test else {}
    if args.run is not None:
        require(args.expected_asset_manifest_sha is not None,
                "--expected-asset-manifest-sha must come from external root launch pin")
        require(args.expected_ready_sha is not None,
                "--expected-ready-sha must come from external root launch pin")
        result.update(verify_run(args.run.resolve(), args.expected_asset_manifest_sha,
                                 args.expected_ready_sha.lower()))
    require(args.run is not None or args.self_test, "provide --run or --self-test")
    if args.output:
        args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
