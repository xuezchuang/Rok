"""Independently verify all 97 GPU buffer and 32 isolated frame readbacks."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
from rok_data_paths import resolve

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
BATCH = ROOT / "Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry"
RUNS = BATCH / "CitySourceGeometryAudit"
FIXTURE = ROOT / "Saved/RokUnityMigration/CitySourceGeometryGpuFixture/v5/manifest.json"
SOURCE = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json"
SOURCE_SHA = "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273"
NATIVE_SHA = "0532db0fb42c4ccac68e83845a9f95ed108385256985d1975939df31d183f935"
CONTRACT_SHA = "cd69da3bf9dbf3d7d79ca09b53b3d72371301d41b25b5faa75123a30af47a18e"
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
PIN_PATHS = (
    "Scripts/PrepareRokCitySourceGeometryGpuFixture.py",
    "Scripts/VerifyRokCitySourceGeometryGpuRuntime.py",
    "Source/Rok/RokCitySourceGeometryAuditSubsystem.h",
    "Source/Rok/RokCitySourceGeometryAuditSubsystem.cpp",
    "Source/RokRendering/Public/RokCitySourceGeometryProbe.h",
    "Source/RokRendering/Private/RokCitySourceGeometryProbe.cpp",
    "Shaders/Private/RokCitySourceGeometryProbe.usf",
    "Scripts/PrepareRokUnityCitySourceGeometryProbe.py",
    "Scripts/VerifyRokUnityCitySourceGeometryProbe.py",
    "Saved/UnityCitySourceGeometryProbeV1/Assets/Editor/ExportRokUnityCitySourceGeometryProbe.cs",
    "Saved/UnityCitySourceGeometryProbeV1/Assets/ProbeLocalUv.shader",
    "Scripts/PrepareRokCityGammaSourceGeometryInputs.py",
    "Scripts/VerifyRokCityGammaSourceGeometryInputs.py",
)


def need(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def project_file(value: str) -> Path:
    path = (resolve(value)).resolve()
    need(path.is_relative_to(ROOT.resolve()), f"Path escapes project: {value}")
    return path


def run_file(run: Path, value: str) -> Path:
    path = (run / value).resolve()
    need(path.parent == run.resolve(), f"Path escapes exact run: {value}")
    return path


def validate_fixture_contract(fixture: dict, expected_sha: str, observed_sha: str) -> None:
    need(len(expected_sha) == 64 and all(c in "0123456789abcdef" for c in expected_sha),
         "Caller must supply external 64-lowerhex fixture SHA")
    need(observed_sha == expected_sha, "Fixture bytes differ from external launch anchor")
    need(fixture["schema"] == "rok.city_source_geometry_gpu_fixture.v1"
         and fixture["fixture_revision"] == "v5"
         and fixture["source_manifest_sha256"] == SOURCE_SHA
         and fixture["native_manifest_sha256"] == NATIVE_SHA
         and fixture["native_contract_sha256"] == CONTRACT_SHA,
         "Fixture schema/source/native anchor differs")
    paths = [record["path"] for record in fixture["pinned_files"]]
    need(len(paths) == len(PIN_PATHS) and len(set(paths)) == len(PIN_PATHS)
         and set(paths) == set(PIN_PATHS), "Exact unique implementation/source pin path set differs")


def verify(run: Path, expected_fixture_sha: str) -> dict:
    run = run.resolve()
    need(run.parent == RUNS.resolve(), "Run is not a direct unique audit child")
    manifest_path = run / "manifest.json"
    runtime = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    fixture = json.loads(FIXTURE.read_text(encoding="utf-8"))
    validate_fixture_contract(fixture, expected_fixture_sha, sha(FIXTURE))
    source = json.loads(SOURCE.read_text(encoding="utf-8"))
    need(runtime["schema"] == "rok.city_source_geometry_gpu_runtime.v1"
         and runtime["fixture_path"] == FIXTURE.relative_to(ROOT).as_posix()
         and runtime["fixture_sha256"] == runtime["expected_fixture_sha256"] == expected_fixture_sha
         and runtime["source_manifest_path"] == SOURCE.relative_to(ROOT).as_posix()
         and runtime["source_manifest_sha256"] == sha(SOURCE) == SOURCE_SHA
         and fixture["source_manifest_sha256"] == SOURCE_SHA
         and len(source["rows"]) == len(fixture["cases"]) == 32,
         "Runtime fixture/sealed source identity differs")
    need(runtime["project"].replace("\\", "/").rstrip("/").lower() == "d:/uedemo/rok"
         and runtime["map"] == "/Game/Maps/L_RokUnityGameAppCity"
         and runtime["opt_in_flag"] and runtime["local_only"]
         and runtime["http_dispatch_before"] == runtime["http_dispatch_after"] == 0
         and not runtime["scene_mutated"] and not runtime["main_view_registered"]
         and not runtime["gameapp_accepted"] and not runtime["material_alpha_accepted"]
         and not runtime["world_transform_accepted"] and not runtime["input_assembler_accepted"],
         "World, HTTP, or scope boundary differs")
    need(runtime["complete"] and runtime["passed"] and not runtime["failures"]
         and runtime["expected_source_buffers"] == runtime["gpu_byte_exact_buffers"] == 97
         and runtime["expected_frames"] == runtime["strict_rgb_coverage_frames"] == 32
         and runtime["rgb_tolerance_lsb"] == 1
         and runtime["native_row_order"] == "top_left_rgba8"
         and runtime["ue_row_order"] == "GPU staging rows tight top-left BGRA8"
         and len(runtime["inputs"]) == 97 and len(runtime["frames"]) == 32,
         "Runtime claims are incomplete")
    for record in fixture["pinned_files"]:
        need(sha(project_file(record["path"])) == record["sha256"],
             f"Pinned source/helper/producer changed: {record['path']}")
    for stem in ("native_contract", "native_manifest", "native_verify"):
        need(sha(project_file(fixture[f"{stem}_path"])) == fixture[f"{stem}_sha256"],
             f"Native evidence pin changed: {stem}")
    native = json.loads(project_file(fixture["native_manifest_path"]).read_text(encoding="utf-8-sig"))
    native_verify = json.loads(project_file(fixture["native_verify_path"]).read_text(encoding="utf-8-sig"))
    need(native["schema"] == "rok.unity_city_source_geometry_native.v1"
         and fixture["native_manifest_sha256"] == NATIVE_SHA
         and fixture["native_contract_sha256"] == CONTRACT_SHA
         and native["complete"] and not native["game_app_loaded"]
         and len(native["cases"]) == 32 and native_verify["passed"]
         and native_verify["native_sha256"] == fixture["native_manifest_sha256"]
         and native_verify["cases_checked"] == 32
         and native_verify["source_typed_binary_inputs_checked"] == 97
         and native_verify["top_left_png_raw_exact_cases"] == 32,
         "Native run/verifier proof chain differs")
    expected_inputs = []
    for case in fixture["cases"]:
        expected_inputs.extend((case["index"], item["kind"], item)
                               for item in case["inputs"])
    need(len(expected_inputs) == 97, "Fixture source input count differs")
    seen_inputs: set[tuple[int, str]] = set()
    for actual, (index, kind, expected) in zip(runtime["inputs"], expected_inputs):
        identity = (index, kind)
        need(identity not in seen_inputs, "Duplicate GPU input proof")
        seen_inputs.add(identity)
        src = project_file(expected["path"])
        gpu = run_file(run, actual["gpu_file"])
        need(actual["row_index"] == index and actual["kind"] == kind
             and actual["source_key"] == fixture["cases"][index]["source_key"]
             and actual["source_path"] == expected["path"]
             and actual["source_sha256"] == expected["sha256"] == sha(src)
             and actual["gpu_sha256"] == sha(gpu) == expected["sha256"]
             and actual["byte_count"] == expected["byte_count"] == len(gpu.read_bytes())
             and gpu.read_bytes() == src.read_bytes() and actual["byte_exact"]
             and actual["ready_frame"] >= actual["copy_frame"],
             f"GPU buffer bytes do not match source: {identity}")
    max_rgb = 0
    total_coverage_mismatch = 0
    for index, (actual, case, original, native_case) in enumerate(zip(
            runtime["frames"], fixture["cases"], source["rows"], native["cases"])):
        need(actual["row_index"] == case["index"] == original["index"] == native_case["index"] == index
             and actual["source_key"] == case["source_key"] == original["source_key"] == native_case["source_key"]
             and actual["source_row_sha256"] == case["source_row_sha256"] == original["source_row_sha256"]
             and actual["matrix_sha256"] == case["matrix"]["sha256"] == native_case["matrix_sha256"]
             and actual["native_raw_sha256"] == case["native_raw_sha256"] == native_case["raw_top_left_sha256"]
             and actual["native_png_sha256"] == case["native_png_sha256"] == native_case["png_sha256"]
             and actual["ready_frame"] >= actual["copy_frame"],
             f"Frame identity/provenance differs: {index}")
        native_raw = project_file(case["native_raw_path"]).read_bytes()
        native_png = project_file(case["native_png_path"])
        gpu_raw_path = run_file(run, actual["gpu_bgra_file"])
        gpu_png_path = run_file(run, actual["gpu_png_file"])
        gpu_raw = gpu_raw_path.read_bytes()
        need(len(native_raw) == len(gpu_raw) == 128 * 128 * 4
             and sha(gpu_raw_path) == actual["gpu_bgra_sha256"]
             and sha(gpu_png_path) == actual["gpu_png_sha256"]
             and gpu_png_path.read_bytes().startswith(PNG_MAGIC)
             and native_png.read_bytes().startswith(PNG_MAGIC)
             and sha(native_png) == case["native_png_sha256"]
             and hashlib.sha256(native_raw).hexdigest() == case["native_raw_sha256"],
             f"Frame raw/PNG hash or byte count differs: {index}")
        with Image.open(native_png) as image:
            need(image.format == "PNG" and image.size == (128, 128)
                 and image.convert("RGBA").tobytes() == native_raw,
                 f"Native PNG is not the pinned top-left raw: {index}")
        with Image.open(gpu_png_path) as image:
            need(image.format == "PNG" and image.size == (128, 128)
                 and image.convert("RGBA").tobytes() == b"".join(
                     bytes((gpu_raw[p + 2], gpu_raw[p + 1], gpu_raw[p], gpu_raw[p + 3]))
                     for p in range(0, len(gpu_raw), 4)),
                 f"UE PNG is not the actual GPU BGRA bytes: {index}")
        over_one = 0
        case_max = 0
        mismatch = 0
        native_coverage = 0
        ue_coverage = 0
        for pixel in range(128 * 128):
            offset = pixel * 4
            nr, ng, nb = native_raw[offset:offset + 3]
            ub, ug, ur = gpu_raw[offset:offset + 3]
            delta = max(abs(nr - ur), abs(ng - ug), abs(nb - ub))
            case_max = max(case_max, delta)
            over_one += delta > 1
            native_drawn = nb < 122
            ue_drawn = ub < 122
            native_coverage += native_drawn
            ue_coverage += ue_drawn
            mismatch += native_drawn != ue_drawn
        need(actual["max_rgb_delta"] == case_max and actual["pixels_over_1_lsb"] == over_one
             and actual["native_coverage_pixels"] == native_coverage
             and actual["ue_coverage_pixels"] == ue_coverage
             and actual["coverage_mismatch_pixels"] == mismatch
             and actual["native_png_equals_raw"] and actual["rgb_and_coverage_passed"]
             and case_max <= 1 and over_one == mismatch == 0
             and 0 < native_coverage == ue_coverage < 128 * 128,
             f"Strict source-local RGB/coverage parity fails: {index}")
        max_rgb = max(max_rgb, case_max)
        total_coverage_mismatch += mismatch
    return {"schema": "rok.city_source_geometry_gpu_runtime_verify.v1", "passed": True,
            "runtime_sha256": sha(manifest_path), "fixture_sha256": sha(FIXTURE),
            "sealed_geometry_sha256": SOURCE_SHA,
            "native_manifest_sha256": fixture["native_manifest_sha256"],
            "native_verify_sha256": fixture["native_verify_sha256"],
            "source_buffers_gpu_byte_exact": 97, "source_uv0_full_float4_bytes_included": True,
            "actual_unity_color32_gpu_byte_exact": True,
            "source_local_frames_rgb_coverage_strict": 32,
            "max_rgb_delta": max_rgb, "coverage_mismatch_pixels": total_coverage_mismatch,
            "gameapp_mainview_material_alpha_accepted": False}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path, required=True)
    parser.add_argument("--expected-fixture-sha", required=True)
    args = parser.parse_args()
    run = args.run.resolve()
    result = verify(run, args.expected_fixture_sha.lower())
    output = run / "verified_runtime.json"
    encoded = (json.dumps(result, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if output.exists():
        need(output.read_bytes() == encoded, "Existing runtime verification differs")
    else:
        with os.fdopen(os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as stream:
            stream.write(encoded)
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
