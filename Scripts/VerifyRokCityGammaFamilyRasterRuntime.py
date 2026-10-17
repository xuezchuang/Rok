"""Verify every isolated five-family GPU fixture case against pinned Unity PNG RGB.

The two night family_a/b frames remain explicitly pending because the Unity
probe did not capture actual shader _Time.y. Their raw deltas are reported.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "Saved/RokUnityMigration/CityGammaFamilyRasterFixture/v2/manifest.json"
FIXTURE_SHA = "7607afededef0e985c4458020e672389b14afd6f4566d47ee10ce770babe1165"
RUN_ROOT = (ROOT / "Saved/RokUnityMigration/BatchReview20261009ScaffoldRuntimeAndGammaRhi/CityGammaFamilyRasterAudit").resolve()
CODE = ["Source/Rok/RokCityGammaFamilyRasterAuditSubsystem.h",
        "Source/Rok/RokCityGammaFamilyRasterAuditSubsystem.cpp",
        "Scripts/PrepareRokCityGammaFamilyRasterFixture.py",
        "Scripts/VerifyRokCityGammaFamilyRasterRuntime.py",
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
    check(fixture["schema"] == "rok.city_gamma_family_raster_fixture.v1"
          and len(fixture["rows"]) == 5 and len(fixture["cases"]) == 42,
          "5/42 fixture schema/count differs")
    check(runtime["schema"] == "rok.city_gamma_family_raster_runtime.v1"
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
    for relative in CODE:
        check(runtime[Path(relative).name + "_sha256"] == sha(ROOT / relative),
              f"producer/verifier changed after capture: {relative}")
    check(runtime["expected_cases"] == runtime["observed_cases"] == 42
          and runtime["input_gpu_byte_exact"] == 5
          and runtime["transport_complete42"] is True
          and runtime["strict_rgb_tolerance_lsb"] == 1
          and runtime["strict_rgb_cases_passed"] == 40
          and runtime["pending_source_time_cases"] == 2
          and runtime["all_42_rgb_parity_accepted"] is False
          and runtime["gameapp_accepted"] is False
          and runtime["mainview_accepted"] is False
          and runtime["night_b1_scope"].startswith("pending_source_time"),
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
    strict_count = pending_count = max_delta = 0
    for actual, expected in zip(runtime["cases"], fixture["cases"], strict=True):
        ident = expected["id"]
        check(actual["id"] == ident and actual["family_index"] == expected["family_index"]
              and actual["frame"] == expected["frame"]
              and actual["source_png_sha256"] == expected["source_png_sha256"],
              f"case identity differs: {ident}")
        check(sha(safe_repo(expected["source_png"])) == expected["source_png_sha256"],
              f"Unity PNG changed: {ident}")
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
            native = image.convert("RGB").tobytes()
        measured = difference(frame_rgb(raw), native)
        check(measured == (actual["max_rgb_delta"], actual["pixels_over_1_lsb"],
                           actual["sum_pixel_max_rgb_delta"]), f"runtime RGB diff differs: {ident}")
        center = 4 * (64 * 128 + 64)
        check((actual["center_r"], actual["center_g"], actual["center_b"],
               actual["center_a_ue_preserved"]) == (raw[center+2], raw[center+1], raw[center], raw[center+3]),
              f"center sample differs: {ident}")
        check(actual["row_pitch_pixels"] >= 128 and actual["staging_height"] >= 128
              and actual["ready_frame"] >= actual["copy_frame"] > 0,
              f"GPU staging evidence invalid: {ident}")
        pending = expected["comparison"] == "night_time_pending"
        check(actual["comparison"] == expected["comparison"]
              and actual["night_time_pending"] == pending and actual["case_passed"],
              f"case status differs: {ident}")
        if pending:
            check(expected["family_index"] == 0 and expected["frame"] in ("family_a", "family_b"),
                  f"unjustified pending case: {ident}")
            pending_count += 1
        else:
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
    check(strict_count == 40 and pending_count == 2, "strict/pending accounting differs")
    return {"schema": "rok.city_gamma_family_raster_verify.v1", "passed": True,
            "fixture_sha256": FIXTURE_SHA, "runtime_manifest_sha256": sha(run / "manifest.json"),
            "gpu_input_bytes_exact": 5, "gpu_frames": 42, "strict_rgb_within_1_lsb": strict_count,
            "night_time_pending": pending_count, "max_rgb_delta_including_pending": max_delta,
            "all_42_rgb_parity_accepted": False, "gameapp_accepted": False, "mainview_accepted": False,
            "alpha_scope": "UE preserved target alpha; Unity alpha not claimed equivalent"}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("run", type=Path, help="exact unique CityGammaFamilyRasterAudit child")
    run = parser.parse_args().run.resolve()
    result = verify(run)
    target = run / "verified_family_raster.json"
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
