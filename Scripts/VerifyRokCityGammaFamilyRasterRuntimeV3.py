"""Verify all 42 isolated GPU RGB cases at one native GPU-measured Night time."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "Saved/RokUnityMigration/CityGammaFamilyRasterFixture/v3/manifest.json"
FIXTURE_SHA = "5734e6c8b7efeee276ec9bb712d2a03ffcc3c3430548f8cb8751f02b3d17ac1c"
SEALED_V2_SHA = "7607afededef0e985c4458020e672389b14afd6f4566d47ee10ce770babe1165"
NIGHT_NATIVE_SHA = "8c228f9f01cabc3bcdabc9ef09926470d38798b4a11c09c308cb5998636d99ea"
RUN_ROOT = (ROOT / "Saved/RokUnityMigration/BatchReview20261009NightTimeAndExpedition/CityGammaFamilyRasterAudit").resolve()
CODE = ["Source/Rok/RokCityGammaFamilyRasterAuditSubsystem.h",
        "Source/Rok/RokCityGammaFamilyRasterAuditSubsystem.cpp",
        "Scripts/PrepareRokCityGammaFamilyRasterFixtureV3.py",
        "Scripts/VerifyRokCityGammaFamilyRasterRuntimeV3.py",
        "Source/RokRendering/Public/RokCityGammaRaster.h",
        "Source/RokRendering/Private/RokCityGammaRaster.cpp",
        "Shaders/Private/RokCityGammaRaster.usf"]


def check(ok: bool, reason: str) -> None:
    if not ok:
        raise AssertionError(reason)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha(path: Path) -> str:
    return digest(path.read_bytes())


def safe_repo(relative: str) -> Path:
    check(relative.startswith(("Saved/UnityCityGammaFamilyStateProbeV2/",
                               "Saved/UnityCityGammaRemainingFamiliesProbe/",
                               "Saved/RokUnityMigration/CityGammaFamilyRasterFixture/v2/",
                               "Saved/RokUnityMigration/CityGammaFamilyRasterFixture/v3/",
                               "Saved/UnityCityNightTimeProbeV1/",
                               "ref/client-unity/Assets/BundleAssets/Map/Shader/cm_city_mask_night.shader",
                               "Scripts/ExportRokCityGammaFamilyStateProbe.cs")),
          f"input not allowlisted: {relative}")
    target = (ROOT / relative).resolve()
    check(target.is_relative_to(ROOT), f"input escapes repo: {relative}")
    return target


def frame_rgb(data: bytes) -> bytes:
    check(len(data) == 128 * 128 * 4, "GPU tight BGRA extent differs")
    rgb = bytearray(128 * 128 * 3)
    rgb[0::3] = data[2::4]
    rgb[1::3] = data[1::4]
    rgb[2::3] = data[0::4]
    return bytes(rgb)


def difference(a: bytes, b: bytes) -> tuple[int, int, int]:
    check(len(a) == len(b) == 128 * 128 * 3, "RGB extent differs")
    deltas = [max(abs(a[i+j] - b[i+j]) for j in range(3))
              for i in range(0, len(a), 3)]
    return max(deltas), sum(d > 1 for d in deltas), sum(deltas)


def verify(run: Path) -> dict:
    check(run.is_dir() and run.parent == RUN_ROOT, "run must be a unique direct audit child")
    check(sha(FIXTURE) == FIXTURE_SHA, "fixture changed")
    fixture = json.loads(FIXTURE.read_text(encoding="utf-8"))
    runtime = json.loads((run / "manifest.json").read_text(encoding="utf-8-sig"))
    check(fixture["schema"] == "rok.city_gamma_family_raster_fixture.v3"
          and len(fixture["rows"]) == 5 and len(fixture["cases"]) == 42,
          "5/42 fixture schema/count differs")
    check(runtime["schema"] == "rok.city_gamma_family_raster_runtime.v3"
          and runtime["complete"] and runtime["passed"] and not runtime["failures"],
          "runtime manifest incomplete/failed")
    check(Path(runtime["project"]).resolve() == ROOT
          and runtime["map"] == "/Game/Maps/L_RokUnityGameAppCity"
          and runtime["opt_in_flag"] and runtime["local_only"]
          and not runtime["scene_mutated"] and not runtime["family_main_view_registered"]
          and runtime["http_dispatch_before"] == runtime["http_dispatch_after"] == 0,
          "project/map/local-only/HTTP boundary differs")
    check(runtime["fixture_path"] == FIXTURE.relative_to(ROOT).as_posix()
          and runtime["fixture_sha256"] == FIXTURE_SHA,
          "prepared fixture identity differs")
    for field in ("native_state", "material_probe", "source_contract", "exporter"):
        check(sha(safe_repo(fixture[f"{field}_path"])) == fixture[f"{field}_sha256"],
              f"source chain changed: {field}")
    check(fixture["sealed_v2_fixture_sha256"] == runtime["sealed_v2_fixture_sha256"] == SEALED_V2_SHA
          and sha(safe_repo(fixture["sealed_v2_fixture_path"])) == SEALED_V2_SHA,
          "sealed V2 base changed")
    check(fixture["diagnostic"]["time_y"] == "one_time_gpu_measured_0_from_native_same_command_buffer"
          and fixture["diagnostic"]["sealed_v2_night_time_scope"] ==
          "old V2 Night B1 frames had no GPU shader time proof", "V3 diagnostic time scope differs")
    sealed = json.loads(safe_repo(fixture["sealed_v2_fixture_path"]).read_text(encoding="utf-8"))
    night = fixture["night_gpu_time"]
    for stem in ("native", "native_verify", "contract", "exporter", "time_shader",
                 "source_shader", "time_before", "time_after"):
        check(sha(safe_repo(night[f"{stem}_path"])) == night[f"{stem}_sha256"],
              f"Night time chain changed: {stem}")
    raw_before = safe_repo(night["time_before_path"]).read_bytes()
    raw_after = safe_repo(night["time_after_path"]).read_bytes()
    check(len(raw_before) == len(raw_after) == 16 and raw_before == raw_after
          and night["before_after_float4_raw_equal"] is True
          and raw_before[:4].hex() == night["gpu_shader_time_y_bits_hex"]
          and struct.unpack("<4f", raw_before) == (0.0, 0.0, 0.0, 1.0)
          and night["gpu_shader_time_y_f32"] == runtime["night_gpu_shader_time_y_f32"] == 0.0,
          "same-buffer GPU _Time.y raw float4 proof differs")
    native = json.loads(safe_repo(night["native_path"]).read_text(encoding="utf-8"))
    native_verify = json.loads(safe_repo(night["native_verify_path"]).read_text(encoding="utf-8"))
    check(night["native_sha256"] == runtime["night_native_sha256"] == NIGHT_NATIVE_SHA
          and native["source_contract_sha256"] == night["contract_sha256"]
          and native["time_before_sha256"] == night["time_before_sha256"]
          and native["time_after_sha256"] == night["time_after_sha256"]
          and native["source_material_guid"] == fixture["rows"][0]["source_material_guid"]
          and native["source_renderer_file_id"] == fixture["rows"][0]["renderer_file_id"]
          and native_verify["passed"] is True and native_verify["before_after_float4_raw_equal"] is True
          and native_verify["native_sha256"] == NIGHT_NATIVE_SHA
          and runtime["night_gpu_time_before_sha256"] == night["time_before_sha256"]
          and runtime["night_gpu_time_after_sha256"] == night["time_after_sha256"],
          "Night native/metadata/verifier binding differs")
    for relative in CODE:
        check(runtime[Path(relative).name + "_sha256"] == sha(ROOT / relative),
              f"producer/verifier changed after capture: {relative}")
    check(runtime["expected_cases"] == runtime["observed_cases"] == 42
          and runtime["input_gpu_byte_exact"] == 5
          and runtime["transport_complete42"] is True
          and runtime["strict_rgb_tolerance_lsb"] == 1
          and runtime["strict_rgb_cases_passed"] == 42
          and runtime["pending_source_time_cases"] == 0
          and runtime["all_42_rgb_parity_accepted"] is True
          and runtime["all_42_controlled_rgb_parity_accepted"] is True
          and runtime["gameapp_accepted"] is False
          and runtime["mainview_accepted"] is False
          and runtime["live_gameapp_dynamic_time_accepted"] is False
          and runtime["night_b1_scope"].startswith("one-time isolated native GPU shader"),
          "runtime accounting differs")
    check(len(runtime["inputs"]) == len(fixture["textures"]) == 5, "input count differs")
    for actual, expected in zip(runtime["inputs"], fixture["textures"], strict=True):
        check(actual["name"] == expected["name"] and actual["byte_exact"], "input identity/proof differs")
        check(actual["gpu_file"] == expected["name"] + ".gpu.bgra8.bin", "input filename differs")
        input_data = (run / actual["gpu_file"]).read_bytes()
        check(len(input_data) == 16 and input_data == safe_repo(expected["file"]).read_bytes()
              and digest(input_data) == actual["gpu_sha256"] == actual["expected_sha256"] == expected["sha256"],
              f"2x2 GPU input readback differs: {expected['name']}")
        check(actual["row_pitch_pixels"] >= 2 and actual["staging_height"] >= 2
              and actual["ready_frame"] >= actual["copy_frame"] > 0,
              "input GPU staging evidence invalid")
    check(len(runtime["cases"]) == 42, "case count differs")
    strict_count = max_delta = replacements = 0
    for actual, expected, old in zip(runtime["cases"], fixture["cases"], sealed["cases"], strict=True):
        ident = expected["id"]
        check(actual["id"] == ident and actual["family_index"] == expected["family_index"]
              and actual["frame"] == expected["frame"]
              and actual["source_png_sha256"] == expected["source_png_sha256"],
              f"case identity differs: {ident}")
        check(sha(safe_repo(expected["source_png"])) == expected["source_png_sha256"],
              f"Unity PNG changed: {ident}")
        check(expected["id"] == old["id"] and expected["comparison"] == "rgb_max_1_lsb"
              and expected["controlled_time_y_f32"] == 0.0
              and expected["controlled_time_proof_sha256"] == night["time_before_sha256"],
              f"V3 case/time pin differs: {ident}")
        is_new_night = expected["family_index"] == 0 and expected["frame"] in ("family_a", "family_b")
        if is_new_night:
            prefix = "b1" if expected["frame"] == "family_a" else "b1_b"
            native_ref = safe_repo(night["native_path"]).parent / native[prefix + "_png"]
            check(safe_repo(expected["source_png"]) == native_ref
                  and expected["source_png_sha256"] == native[prefix + "_sha256"]
                  and expected["night_gpu_time_source"] == "same_buffer_before_after_raw_float4",
                  f"new Night B1 A/B source differs: {ident}")
            replacements += 1
        else:
            check(expected["source_png"] == old["source_png"]
                  and expected["source_png_sha256"] == old["source_png_sha256"],
                  f"one of 40 sealed V2 references changed: {ident}")
        check(actual["gpu_bgra_file"] == ident + ".gpu.bgra8.bin"
              and actual["gpu_png_file"] == ident + ".gpu.png", f"GPU filename differs: {ident}")
        raw = (run / actual["gpu_bgra_file"]).read_bytes()
        check(digest(raw) == actual["gpu_bgra_sha256"], f"raw GPU hash differs: {ident}")
        png_bytes = (run / actual["gpu_png_file"]).read_bytes()
        check(png_bytes.startswith(b"\x89PNG\r\n\x1a\n")
              and digest(png_bytes) == actual["gpu_png_sha256"],
              f"GPU PNG hash differs: {ident}")
        with Image.open(run / actual["gpu_png_file"]) as image:
            check(image.format == "PNG" and image.size == (128, 128)
                  and image.convert("RGB").tobytes() == frame_rgb(raw),
                  f"PNG does not encode raw GPU RGB: {ident}")
        with Image.open(safe_repo(expected["source_png"])) as image:
            source_rgb = image.convert("RGB").tobytes()
        measured = difference(frame_rgb(raw), source_rgb)
        check(measured == (actual["max_rgb_delta"], actual["pixels_over_1_lsb"],
                           actual["sum_pixel_max_rgb_delta"]), f"runtime RGB diff differs: {ident}")
        center = 4 * (64 * 128 + 64)
        check((actual["center_r"], actual["center_g"], actual["center_b"],
               actual["center_a_ue_preserved"]) == (raw[center+2], raw[center+1], raw[center], raw[center+3]),
              f"center sample differs: {ident}")
        check(actual["row_pitch_pixels"] >= 128 and actual["staging_height"] >= 128
              and actual["ready_frame"] >= actual["copy_frame"] > 0,
              f"GPU staging evidence invalid: {ident}")
        check(actual["comparison"] == expected["comparison"]
              and actual["night_time_pending"] is False and actual["case_passed"]
              and actual["controlled_time_y_f32"] == expected["controlled_time_y_f32"]
              and actual["controlled_time_proof_sha256"] == night["time_before_sha256"],
              f"case status differs: {ident}")
        check(measured[1] == 0 and actual["rgb_within_1_lsb"],
              f"strict RGB comparison failed: {ident}, max={measured[0]}, over={measured[1]}")
        strict_count += 1
        max_delta = max(max_delta, measured[0])
    # The explicit white road mask has the same GPU result as Unity's proven
    # default white mask. It is a controlled semantics check, not a new slot.
    road = [c for c in fixture["cases"] if c["family_index"] == 3]
    by_name = {c["frame"]: c for c in road}
    with Image.open(safe_repo(by_name["family_a"]["source_png"])) as a, Image.open(
            safe_repo(by_name["road_explicit_white"]["source_png"])) as white:
        check(a.convert("RGB").tobytes() == white.convert("RGB").tobytes(),
              "Unity road default/explicit white semantics differ")
    check(strict_count == 42 and replacements == 2, "42 strict/two-new-Night accounting differs")
    return {"schema": "rok.city_gamma_family_raster_verify.v3", "passed": True,
            "fixture_sha256": FIXTURE_SHA, "runtime_manifest_sha256": sha(run / "manifest.json"),
            "gpu_input_bytes_exact": 5, "gpu_frames": 42, "strict_rgb_within_1_lsb": strict_count,
            "new_measured_night_b1_frames": replacements, "night_gpu_shader_time_y_f32": 0.0,
            "max_rgb_delta": max_delta, "all_42_controlled_rgb_parity_accepted": True,
            "gameapp_accepted": False, "mainview_accepted": False,
            "live_gameapp_dynamic_time_accepted": False,
            "alpha_scope": "UE preserved target alpha; Unity alpha not claimed equivalent"}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("run", type=Path, help="exact unique CityGammaFamilyRasterAudit child")
    run = parser.parse_args().run.resolve()
    result = verify(run)
    target = run / "verified_family_raster_v3.json"
    encoded = (json.dumps(result, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if target.exists():
        check(target.read_bytes() == encoded, "existing verifier result differs")
    else:
        fd = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
        with os.fdopen(fd, "wb") as stream:
            stream.write(encoded)
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
