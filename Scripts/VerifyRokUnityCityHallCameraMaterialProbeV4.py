"""Strict source/PNG/f32 witness verification for the isolated Hall V4 probe."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "Saved/UnityCityHallCameraMaterialProbeV4"
CONTRACT = PROJECT / "hall_camera_material_contract_v4.json"
V1 = ROOT / "Saved/UnityCityHallCameraMaterialProbeV1/OutputRuns/20261009_052943_467_46956/hall_camera_material_native.json"
V1_SHA = "3e2d70c5a9c2a845f5e34b8e9e80fe95d2218142b9697a9d63236beb4d6ef451"
PHASES = ("day_black", "day_white", "night_black", "night_white")
PIXEL_MODES = (20, 21, 22, 23, 24, 25, 26, 27)
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


def check_coverage_control(measured: list[float], source: bytes) -> None:
    need(len(measured) == 4 and len(source) == 4 and sum(measured[:3]) > 0.1 and
         measured[3] > 0.5 and
         max(abs(measured[i] - source[i] / 255.0) for i in range(4)) < 0.08,
         "Float target mode0 failed source-positive Hall coverage control")


def check_signal(value: list[float], label: str) -> None:
    need(len(value) == 4 and sum(value[:3]) > 0.01,
         f"Float {label} witness clear/zero")


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
    need(contract["schema"] == "rok.hall_camera_material_contract.v4" and
         Path(contract["probe_root"]).resolve() == PROJECT.resolve() and
         contract["source_renderer_file_id"] == "212751466141171401" and
         contract["sprite_guid"] == "57ec7ddf0b743a140b2707fe14b536e2" and
         contract["mask_guid"] == "c37165d75575008419379c96242599ba" and
         contract["v1_native_manifest_sha256"] == V1_SHA and sha(V1) == V1_SHA and
         contract["qualifier"] ==
         "source_camera_material_instrumented_diagnostic_only; original and clone mode0 must byteexact V1",
         "V4 source scope/identity differs")
    need(len(contract["input_files"]) == 79 and
         len({row["path"] for row in contract["input_files"]}) == 79,
         "Project source pin set incomplete")
    for entry in contract["input_files"]:
        if not (after_run and entry["path"] in MUTABLE_META):
            pinned(PROJECT, entry)
    need(sha(PROJECT / "Assets/Editor/RenderRokUnityCityHallCameraMaterialNativeV4.cs") ==
         contract["exporter_sha256"] and
         sha(PROJECT / "Assets/ProbeHallDiagnosticDayV4.shader") ==
         contract["diagnostic_day_sha256"] and
         sha(PROJECT / "Assets/ProbeHallDiagnosticNightV4.shader") ==
         contract["diagnostic_night_sha256"] and
         sha(ROOT / "Scripts/PrepareRokUnityCityHallCameraMaterialProbeV4.py") ==
         contract["producer_sha256"] and
         sha(ROOT / "Scripts/VerifyRokUnityCityHallCameraMaterialProbeV4.py") ==
         contract["verifier_sha256"], "Producer/exporter/verifier pin differs")
    for phase in PHASES:
        need(any(row["path"] == phase for row in contract["v1_case_raw"]),
             f"Missing V1 phase pin: {phase}")
    return contract


def verify_run(run: Path, contract: dict) -> dict:
    run = run.resolve()
    need(run.parent == (PROJECT / "OutputRuns").resolve(), "Run outside V4 isolated project")
    manifest_path = run / "hall_camera_material_native_v4.json"
    native = read(manifest_path)
    need(native["schema"] == "rok.unity_city_hall_camera_material_native.v4" and
         native["complete"] and native["contract_sha256"] == sha(CONTRACT) and
         native["input_sha256"] == contract["source_input_sha256"] and
         native["exporter_sha256"] == contract["exporter_sha256"] and
         native["graphics_device"] == "Direct3D11" and native["color_space"] == "Gamma" and
         (native["width"], native["height"], native["anti_aliasing"], native["case_count"]) ==
         (1440, 810, 1, 4) and native["v1_raw_exact_cases"] == 4 and
         native["clone_raw_exact_cases"] == 4 and not native["game_app_loaded"] and
         native["float_coverage_control_count"] == 4 and native["float_camera_hdr"] and
         native["float_legacy_target_commands_removed"] and native["float_source_time_zero_bound"] and
         native["float_diagnostic_target_format"] == "ARGBFloat RenderTextureReadWrite.Linear" and
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
             "V4 isolated readable reimport changed source texture state")
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
    witnesses = native["float_witnesses"]
    need(len(witnesses) == 52, "Expected 4 coverage, 16 matrix and 32 per-pixel float GPU witnesses")
    expected = {(mode, "day_black", 598, 138) for mode in range(1, 17)}
    expected |= {(mode, phase, 598, y) for mode in PIXEL_MODES
                 for phase in ("day_black", "night_black") for y in (127, 138)}
    expected |= {(0, phase, 598, y) for phase in ("day_black", "night_black") for y in (127, 138)}
    seen = set()
    for row in witnesses:
        key = (row["mode"], row["phase"], row["x"], row["y"])
        need(key in expected and key not in seen and
             row["semantic"] == ("coverage" if row["mode"] == 0 else
                                  "matrix" if row["mode"] < 20 else "pixel"),
             "Unexpected/duplicate float witness")
        seen.add(key)
        raw = pinned(run, row["raw_rgba32f"])
        need(raw.stat().st_size == 16, "Float GPU point is not 16B")
        values = struct.unpack("<4f", raw.read_bytes())
        need(all(math.isfinite(v) for v in values) and abs(values[3]-1.0) <= 1e-6 and
             all(struct.pack("<f", v) == struct.pack("<f", row["rgba"][i])
                 for i, v in enumerate(values)), "Float point raw/JSON or coverage differs")
    need(seen == expected, "Float witness coverage incomplete")
    witness_map = {(row["mode"], row["phase"], row["x"], row["y"]): row["rgba"] for row in witnesses}
    for phase in ("day_black", "night_black"):
        source_row = next(item for item in v1["cases"] if item["phase"] == phase)
        source_raw = pinned(V1.parent, source_row["raw_top_left_rgba8"]).read_bytes()
        for y in (127, 138):
            measured = witness_map[(0, phase, 598, y)]
            source = source_raw[(y * 1440 + 598)*4:(y * 1440 + 598)*4 + 4]
            check_coverage_control(measured, source)
            for mode in (20, 21, 23, 25):
                value = witness_map[(mode, phase, 598, y)]
                check_signal(value, f"UV/Main/Mask/preblend at {phase}/{mode}/{y}")
            uv = witness_map[(20, phase, 598, y)]
            need(0.0 <= uv[0] <= 1.0 and 0.0 <= uv[1] <= 1.0 and uv[0] + uv[1] > 0.01,
                 "GPU UV witness outside source sprite range/clear")
            time = witness_map[(27, phase, 598, y)]
            need(all(abs(value) <= 1e-6 for value in time[:3]),
                 "Float GPU draw did not observe source _Time.y=0 fixture")
    mapped = { (row["mode"], row["phase"]): row["rgba"] for row in witnesses if row["mode"] <= 16 }
    for role, shift, record in (("object", 0, native["gpu_object_to_world_rows_f32le"]),
                                ("vp", 8, native["gpu_view_projection_rows_f32le"])):
        values = []
        for index in range(4):
            values.extend(mapped[(shift+1+index, "day_black")][:3])
            values.append(mapped[(shift+5+index, "day_black")][0])
        actual = pinned(run, record).read_bytes()
        need(actual == struct.pack("<16f", *values), f"GPU {role} matrix not reconstructed from pixel witnesses")
        check_matrix(values, role)
    need(math.isfinite(native["gpu_object_vs_csharp_max_abs"]) and
         math.isfinite(native["gpu_vp_vs_source_derived_max_abs"]),
         "GPU/C# matrix comparison unavailable")
    return {"schema": "rok.unity_city_hall_camera_material_v4_verify.v1", "passed": True,
            "native_sha256": sha(manifest_path), "contract_sha256": sha(CONTRACT),
            "original_rgba_exact_cases": 4, "instrumented_mode0_rgba_exact_cases": 4,
            "png_raw_exact_cases": frame_count, "float_gpu_witnesses": len(witnesses),
            "matrix_source": "instrumented clone shader draw; original untouched shader constant values not directly observed",
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
            lambda: check_coverage_control([0.0, 0.0, 0.0, 1.0], bytes((145, 149, 52, 197))),
            lambda: check_signal([0.0, 0.0, 0.0, 1.0], "all-clear fixture"),
            lambda: check_matrix([0.0] * 16, "all-zero fixture"),
            lambda: need((20, "day_black", 597, 127) == (20, "day_black", 598, 127), "wrong coordinate fixture"),
        ):
            try:
                action()
            except AssertionError:
                rejected += 1
        need(rejected == 6, "Prepared negative checks escaped")
        result["negative_cases"] = rejected
    if args.run:
        result = verify_run(args.run, contract)
        destination = args.run / "verified_hall_sampling_v4.json"
        need(not destination.exists(), "Do not overwrite a prior V4 run verifier")
        destination.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
