"""Strict source/PNG/f32 witness verification for the isolated Hall V5 probe."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "Saved/UnityCityHallCameraMaterialProbeV5"
CONTRACT = PROJECT / "hall_camera_material_contract_v5.json"
V1 = ROOT / "Saved/UnityCityHallCameraMaterialProbeV1/OutputRuns/20261009_052943_467_46956/hall_camera_material_native.json"
V1_SHA = "3e2d70c5a9c2a845f5e34b8e9e80fe95d2218142b9697a9d63236beb4d6ef451"
PHASES = ("day_black", "day_white", "night_black", "night_white")
PIXEL_MODES = tuple(range(33, 48))
MUTABLE_META = {
    "Assets/Source/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_1_101_1.png.meta",
    "Assets/Source/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_1_101_1_mask.png.meta",
}


def need(value: bool, reason: str) -> None:
    if not value:
        raise AssertionError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def check_sentinel(raw: bytes) -> None:
    need(raw == struct.pack("<f", 0.75),
         "Packed shader sentinel differs: no source-Hall draw, wrong coordinate, blend, or channel order")


def check_signal(value: float, label: str) -> None:
    need(math.isfinite(value) and value > 0.01,
         f"Packed {label} witness clear/zero")


def check_matrix(values: list[float], role: str) -> None:
    need(len(values) == 16 and all(math.isfinite(value) for value in values) and
         max(abs(value) for value in values) > 0.1,
         f"GPU {role} matrix is all clear/zero")


def pinned(base: Path, record: dict) -> Path:
    name = record["path"]
    need(isinstance(name, str) and name and not name.startswith("/") and ":" not in name and
         "\\" not in name and ".." not in Path(name).parts, "Unsafe pinned path")
    path = (base / name).resolve()
    need(path.is_relative_to(base.resolve()) and path.is_file() and sha(path) == record["sha256"] and
         ("byte_count" not in record or path.stat().st_size == record["byte_count"]),
         f"Pinned bytes differ: {name}")
    return path


def verify_contract(after_run: bool = False) -> dict:
    contract = read(CONTRACT)
    need(contract["schema"] == "rok.hall_camera_material_contract.v5" and
         Path(contract["probe_root"]).resolve() == PROJECT.resolve() and
         contract["source_renderer_file_id"] == "212751466141171401" and
         contract["sprite_guid"] == "57ec7ddf0b743a140b2707fe14b536e2" and
         contract["mask_guid"] == "c37165d75575008419379c96242599ba" and
         contract["v1_native_manifest_sha256"] == V1_SHA and sha(V1) == V1_SHA and
         contract["qualifier"] ==
         "source_camera_material_instrumented_diagnostic_only; original and clone mode0 must byteexact V1",
         "V5 source scope/identity differs")
    need(len(contract["input_files"]) == 79 and
         len({row["path"] for row in contract["input_files"]}) == 79,
         "Project source pin set incomplete")
    for entry in contract["input_files"]:
        if not (after_run and entry["path"] in MUTABLE_META):
            pinned(PROJECT, entry)
    need(sha(PROJECT / "Assets/Editor/RenderRokUnityCityHallCameraMaterialNativeV5.cs") ==
         contract["exporter_sha256"] and
         sha(PROJECT / "Assets/ProbeHallDiagnosticDayV5.shader") ==
         contract["diagnostic_day_sha256"] and
         sha(PROJECT / "Assets/ProbeHallDiagnosticNightV5.shader") ==
         contract["diagnostic_night_sha256"] and
         sha(ROOT / "Scripts/PrepareRokUnityCityHallCameraMaterialProbeV5.py") ==
         contract["producer_sha256"] and
         sha(ROOT / "Scripts/VerifyRokUnityCityHallCameraMaterialProbeV5.py") ==
         contract["verifier_sha256"], "Producer/exporter/verifier pin differs")
    for phase in PHASES:
        need(any(row["path"] == phase for row in contract["v1_case_raw"]),
             f"Missing V1 phase pin: {phase}")
    return contract


def verify_run(run: Path, contract: dict) -> dict:
    run = run.resolve()
    need(run.parent == (PROJECT / "OutputRuns").resolve(), "Run outside V5 isolated project")
    manifest_path = run / "hall_camera_material_native_v5.json"
    native = read(manifest_path)
    need(native["schema"] == "rok.unity_city_hall_camera_material_native.v5" and
         native["complete"] and native["contract_sha256"] == sha(CONTRACT) and
         native["input_sha256"] == contract["source_input_sha256"] and
         native["exporter_sha256"] == contract["exporter_sha256"] and
         native["graphics_device"] == "Direct3D11" and native["color_space"] == "Gamma" and
         (native["width"], native["height"], native["anti_aliasing"], native["case_count"]) ==
         (1440, 810, 1, 4) and native["v1_raw_exact_cases"] == 4 and
         native["clone_raw_exact_cases"] == 4 and not native["game_app_loaded"] and
         native["float_diagnostic_hdr"] and native["packed_source_time_zero_bound"] and
         native["float_diagnostic_target_format"] == "ARGBFloat RenderTextureReadWrite.Linear" and
         native["packed_target_format"] == "ARGB32 RenderTextureReadWrite.Default Gamma; Blend One Zero" and
         not native["source_behaviors_executed"] and not native["lod_qualified"] and
         not native["main_view_qualified"] and
         native["diagnostic_shader_qualification"] ==
         "instrumented mode0 four phases byteexact Native1; GPU constants are measured on clone draws, not untouched source shader draw",
         "Native run identity/qualification differs")
    v1 = read(V1)
    for role, suffix in (("main", "build_1_101_1.png.meta"),
                         ("mask", "build_1_101_1_mask.png.meta")):
        source = next(entry for entry in contract["input_files"] if entry["path"].endswith(suffix))
        state = native[role + "_texture"]
        need(state["source_meta_sha256"] == source["sha256"] and
             sha(PROJECT / source["path"]) == state["project_meta_after_sha256"] and
             state["format_before"] == state["format_after"] == ("DXT5" if role == "main" else "DXT1") and
             state["filter_before"] == state["filter_after"] == "Bilinear" and
             state["wrap_before"] == state["wrap_after"] == "Clamp" and
             state["mips_before"] == state["mips_after"] == 1,
             "V5 isolated readable reimport changed source texture state")
    need([row["phase"] for row in native["cases"]] == list(PHASES) and
         [row["phase"] for row in native["cloned_cases"]] == list(PHASES),
         "Original/clone phase order differs")
    frame_count = 0
    for group in ("cases", "cloned_cases"):
        for index, row in enumerate(native[group]):
            oracle = v1["cases"][index]
            raw = pinned(run, row["raw_top_left_rgba8"])
            png = pinned(run, row["png"])
            need(raw.stat().st_size == 1440*810*4 and
                 sha(raw) == oracle["raw_top_left_rgba8"]["sha256"],
                 f"Native1 original/clone full RGBA differs: {group}/{row['phase']}")
            with Image.open(png) as image:
                need(image.size == (1440, 810) and image.convert("RGBA").tobytes() == raw.read_bytes(),
                     "PNG does not decode to full raw RGBA")
            need(row["sprite_vertex_count"] == 240 and row["sprite_triangle_count"] == 238 and
                 row["property_block_mask_matches"] and row["renderer_enabled"] and
                 row["active_in_hierarchy"] and row["render_queue"] == (2995 if index < 2 else 2996),
                 "Source renderer/material state differs")
            frame_count += 1
    full_float = pinned(run, native["float_diagnostic_mode0_top_left_rgba32f"]).read_bytes()
    need(len(full_float) == 1440*810*16, "Float diagnostic full-frame byte count differs")
    nonclear = sum(1 for i in range(0, len(full_float), 16)
                   if any(abs(struct.unpack_from("<f", full_float, i+channel*4)[0]) > 0.01
                          for channel in range(3)))
    need(nonclear == native["float_diagnostic_nonclear_rgb_pixels"],
         "Float diagnostic full-frame coverage statistic differs")
    witnesses = native["packed_witnesses"]
    need(len(witnesses) == 96, "Expected 4 sentinel, 32 matrix and 60 pixel packed GPU witnesses")
    expected = {(mode, "day_black", 598, 138) for mode in range(1, 33)}
    expected |= {(mode, phase, 598, y) for mode in PIXEL_MODES
                 for phase in ("day_black", "night_black") for y in (127, 138)}
    expected |= {(48, phase, 598, y) for phase in ("day_black", "night_black") for y in (127, 138)}
    seen = set()
    raw_map = {}
    value_map = {}
    for row in witnesses:
        key = (row["mode"], row["phase"], row["x"], row["y"])
        need(key in expected and key not in seen and
             row["semantic"] == ("sentinel" if row["mode"] == 48 else
                                  "matrix" if row["mode"] < 33 else "pixel"),
             "Unexpected/duplicate packed witness")
        seen.add(key)
        raw = pinned(run, row["raw_rgba8"]).read_bytes()
        need(len(raw) == 4 and list(raw) == row["rgba8"], "Packed point raw/JSON channel order differs")
        value = struct.unpack("<f", raw)[0]
        need(math.isfinite(value) and struct.pack("<f", value) == struct.pack("<f", row["decoded_float"]),
             "Packed point IEEE754 float/raw JSON differs")
        raw_map[key] = raw
        value_map[key] = value
    need(seen == expected, "Packed witness coverage incomplete")
    for phase in ("day_black", "night_black"):
        for y in (127, 138):
            check_sentinel(raw_map[(48, phase, 598, y)])
            for mode in (33, 34, 35, 39, 43):
                check_signal(value_map[(mode, phase, 598, y)], f"UV/Main/Mask/preblend at {phase}/{mode}/{y}")
            u, v = value_map[(33, phase, 598, y)], value_map[(34, phase, 598, y)]
            need(0.0 <= u <= 1.0 and 0.0 <= v <= 1.0 and u + v > 0.01,
                 "GPU UV witness outside source sprite range/clear")
            need(value_map[(47, phase, 598, y)] == 0.0,
                 "Packed GPU draw did not observe source _Time.y=0 fixture")
    for role, shift, record in (("object", 0, native["gpu_object_to_world_rows_f32le"]),
                                ("vp", 16, native["gpu_view_projection_rows_f32le"])):
        raw_rows = b"".join(raw_map[(shift+index+1, "day_black", 598, 138)] for index in range(16))
        values = list(struct.unpack("<16f", raw_rows))
        actual = pinned(run, record).read_bytes()
        need(actual == raw_rows, f"GPU {role} matrix not reconstructed from packed pixel witnesses")
        check_matrix(values, role)
    need(math.isfinite(native["gpu_object_vs_csharp_max_abs"]) and
         math.isfinite(native["gpu_vp_vs_source_derived_max_abs"]),
         "GPU/C# matrix comparison unavailable")
    return {"schema": "rok.unity_city_hall_camera_material_v5_verify.v1", "passed": True,
            "native_sha256": sha(manifest_path), "contract_sha256": sha(CONTRACT),
            "original_rgba_exact_cases": 4, "instrumented_mode0_rgba_exact_cases": 4,
            "png_raw_exact_cases": frame_count, "packed_gpu_witnesses": len(witnesses),
            "float_diagnostic_nonclear_rgb_pixels": nonclear,
            "matrix_source": "source-equivalent mode0 clone, packed instrumented RGBA8 draws; untouched shader constants remain unobserved",
            "runtime_lod_accepted": False, "gameapp_main_view_accepted": False}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    contract = verify_contract(after_run=args.run is not None)
    result = {"prepared": True, "contract_sha256": sha(CONTRACT)}
    if args.self_test:
        rejected = 0
        for change in (lambda x: x.update(v1_native_manifest_sha256="0"*64),
                       lambda x: x["input_files"][0].update(sha256="0"*64)):
            clone = json.loads(json.dumps(contract)); change(clone)
            try:
                need(clone["v1_native_manifest_sha256"] == V1_SHA, "V1 mutation")
                for item in clone["input_files"]:
                    pinned(PROJECT, item)
            except AssertionError:
                rejected += 1
        for action in (
            lambda: check_sentinel(bytes((0, 0, 0, 255))),
            lambda: check_signal(0.0, "all-clear fixture"),
            lambda: check_matrix([0.0] * 16, "all-zero fixture"),
            lambda: need((33, "day_black", 597, 127) == (33, "day_black", 598, 127), "wrong coordinate fixture"),
        ):
            try:
                action()
            except AssertionError:
                rejected += 1
        need(rejected == 6, "Prepared negative checks escaped")
        result["negative_cases"] = rejected
    if args.run:
        result = verify_run(args.run, contract)
        destination = args.run / "verified_hall_sampling_v5.json"
        need(not destination.exists(), "Do not overwrite a prior V5 run verifier")
        destination.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
