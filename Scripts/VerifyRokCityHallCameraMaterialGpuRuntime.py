"""Strict independent readback check for the isolated Hall material RHI audit.

The black/white pair measures both RGB composition and transparency; alpha is
checked independently against Unity's nontrivial RGBA render-target channel.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path
from rok_data_paths import resolve

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v1/manifest.json"
PHASES = ("day_black", "day_white", "night_black", "night_white")
KINDS = ("positions", "uv0", "indices", "sprite", "mask")
SHA_NATIVE = "3e2d70c5a9c2a845f5e34b8e9e80fe95d2218142b9697a9d63236beb4d6ef451"
SHA_GEOMETRY = "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273"
SHA_HALL = "01b924d764395708e20766834595fa7943f1ec61c70215b6058914c2cc80ab5e"
SHA_CAMERA = "417eb43964bd2009879561194e4c5a75c7808f257dc6b84f8707ed4892b0d944"
SHA_TEXTURES = "671f18f91275fc0a5a65107eff5e44857c59310873036293b99665f84c391b71"
SHA_CONTRACT = "4bac306793ef6a94cab283ab169a3d2cd912f27649f3d093df0efb6ab260df2f"
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"


def need(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def safe(record: dict) -> Path:
    value = record["path"]
    need(isinstance(value, str) and not value.startswith("/") and "\\" not in value and ":" not in value,
         "Unsafe pinned path")
    path = (resolve(value)).resolve()
    need(path.is_relative_to(ROOT.resolve()) and path.is_file() and
         sha(path) == record["sha256"] and path.stat().st_size == record["byte_count"],
         f"Pinned file differs: {value}")
    return path


def f32(x: float) -> float:
    return struct.unpack("<f", struct.pack("<f", x))[0]


def product(a: list[float], b: list[float]) -> list[float]:
    return [f32(sum(a[r*4+k] * b[k*4+c] for k in range(4)))
            for r in range(4) for c in range(4)]


def transpose(a: list[float]) -> list[float]:
    return [a[c*4+r] for r in range(4) for c in range(4)]


def validate_fixture(fixture: dict, expected_sha: str) -> None:
    need(sha(FIXTURE) == expected_sha, "Externally pinned fixture hash differs")
    need(fixture["schema"] == "rok.city_hall_camera_material_gpu_fixture.v1" and
         fixture["width"] == 1440 and fixture["height"] == 810 and
         fixture["target_format"] == "PF_B8G8R8A8" and not fixture["target_srgb"] and
         fixture["source_key"] == "initial:5" and
         fixture["sprite_renderer_file_id"] == "212751466141171401" and
         fixture["source_row_sha256"] == "6b2844db3726051827252fe03f5892b2d99d54675c3d9e673f78a05197cb2a5e" and
         fixture["native_sha256"] == SHA_NATIVE and
         fixture["native_contract_sha256"] == SHA_CONTRACT and
         fixture["hall_native3_sha256"] == SHA_HALL and
         fixture["native_camera2_sha256"] == SHA_CAMERA and
         fixture["source_geometry_sha256"] == SHA_GEOMETRY and
         fixture["source_texture_state_sha256"] == SHA_TEXTURES and
         fixture["expected_http_dispatches"] == 0 and
         not fixture["main_view_registered"] and not fixture["lod_qualified"] and
         not fixture["gameapp_accepted"] and len(fixture["inputs"]) == 5 and
         len(fixture["cases"]) == 4, "Fixture schema/scope/source chain differs")
    for group in ("native_pins", "material_pins", "code_pins"):
        names = [record["path"] for record in fixture[group]]
        need(len(names) == len(set(names)), f"Duplicate fixture {group} path")
        for record in fixture[group]:
            safe(record)
    pins = {record["path"]: record["sha256"] for record in fixture["native_pins"]}
    need(pins["Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json"] == SHA_GEOMETRY and
         pins["Saved/UnityCityHallWorldProbeV1/OutputRuns/20261009_032009_843_25796/hall_world_native.json"] == SHA_HALL and
         pins["Saved/UnityCityHallCameraLodProbeV1/OutputRuns/20261009_045949_616_43000/native_camera_lod.json"] == SHA_CAMERA and
         pins["Saved/UnityCityGammaFamilyStateProbeV2/OutputRuns/20261008_234412_007_44320/native_state.json"] == SHA_TEXTURES and
         pins["Saved/UnityCityHallCameraMaterialProbeV1/OutputRuns/20261009_052943_467_46956/hall_camera_material_native.json"] == SHA_NATIVE,
         "Sealed Unity native source path/hash chain differs")
    matrix = safe(fixture["matrix"])
    need(matrix.stat().st_size == 64, "Source-camera composed matrix must be 64B")
    hall = ROOT / "Saved/UnityCityHallWorldProbeV1/OutputRuns/20261009_032009_843_25796/matrices/023.f32le"
    camera = ROOT / "Saved/UnityCityHallCameraLodProbeV1/OutputRuns/20261009_045949_616_43000/matrices"
    w = list(struct.unpack("<16f", hall.read_bytes()))
    v = list(struct.unpack("<16f", (camera / "camera_world_to_camera.f32le").read_bytes()))
    p = list(struct.unpack("<16f", (camera / "camera_gpu_projection_rt.f32le").read_bytes()))
    expected_matrix = struct.pack("<16f", *transpose(product(product(p, v), w)))
    need(expected_matrix == matrix.read_bytes(), "GPU row-vector matrix does not recompute from original Unity matrices")
    source = read(ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json")["rows"][27]
    refs = (source["positions"], source["uv_channels"][0]["binary"], source["indices"]["binary"])
    for index, (kind, record) in enumerate(zip(KINDS, fixture["inputs"])):
        path = safe(record)
        need(record["kind"] == kind, "GPU input order differs")
        if index < 3:
            need(path.read_bytes() == safe(refs[index]).read_bytes() and
                 record["count"] == refs[index]["count"] and
                 record["stride_bytes"] == refs[index]["stride_bytes"],
                 f"Source row27 geometry bytes differ: {kind}")
        else:
            need(record["rhi_format"] == "PF_B8G8R8A8" and not record["srgb"] and
                 record["num_mips"] == 1 and
                 (record["width"], record["height"]) ==
                 ((1024, 888) if index == 3 else (512, 444)),
                 f"Hall {kind} GPU input format/dimensions differ")
            raw = safe(record["source_raw"]).read_bytes()
            data = path.read_bytes()
            need(data[0::4] == raw[2::4] and data[1::4] == raw[1::4] and
                 data[2::4] == raw[0::4] and data[3::4] == raw[3::4],
                 f"Hall {kind} GPU upload is not channel-swap-only source row order")
            with Image.open(safe(record["source_png"])) as image:
                top = image.convert("RGBA").tobytes()
            width, height = record["width"], record["height"]
            need(top == b"".join(raw[(height-1-y)*width*4:(height-y)*width*4] for y in range(height)),
                 f"Hall {kind} source texture PNG/raw row orientation changed")
    native = read(ROOT / "Saved/UnityCityHallCameraMaterialProbeV1/OutputRuns/20261009_052943_467_46956/hall_camera_material_native.json")
    for index, frame in enumerate(fixture["cases"]):
        source_case = native["cases"][index]
        need(frame["phase"] == source_case["phase"] == PHASES[index] and
             frame["background"] == source_case["background"] == ("white" if index % 2 else "black") and
             frame["material_queue"] == source_case["render_queue"] == (2995 if index < 2 else 2996),
             "Case phase/background/material identity differs")
        raw_path, png_path = safe(frame["native_raw"]), safe(frame["native_png"])
        need(frame["native_raw"]["sha256"] == source_case["raw_top_left_rgba8"]["sha256"] and
             frame["native_png"]["sha256"] == source_case["png"]["sha256"],
             "Case Unity oracle differs")
        with Image.open(png_path) as image:
            need(image.format == "PNG" and image.size == (1440, 810) and
                 image.convert("RGBA").tobytes() == raw_path.read_bytes(),
                 "Native PNG decoded pixels differ from raw top-left RGBA")


def frame_compare(gpu: bytes, unity: bytes) -> dict:
    need(len(gpu) == len(unity) == 1440 * 810 * 4, "GPU/native frame length differs")
    max_rgb = max_alpha = over = coverage = nonopaque = 0
    for i in range(0, len(gpu), 4):
        rgb = max(abs(gpu[i+2]-unity[i]), abs(gpu[i+1]-unity[i+1]),
                  abs(gpu[i]-unity[i+2]))
        alpha = abs(gpu[i+3]-unity[i+3])
        max_rgb = max(max_rgb, rgb)
        max_alpha = max(max_alpha, alpha)
        over += rgb > 2
        nonopaque += unity[i+3] != 255
        coverage += unity[i] != unity[0] or unity[i+1] != unity[1] or unity[i+2] != unity[2]
    need(max_rgb <= 2 and max_alpha <= 2 and over == 0 and coverage > 80000 and nonopaque > 1000,
         "Source material RGBA/coverage mismatch")
    return {"max_rgb_delta": max_rgb, "max_alpha_delta": max_alpha,
            "pixels_over_2_lsb": over, "coverage_pixels": coverage,
            "nonopaque_source_pixels": nonopaque}


def pair_alpha_compare(black_gpu: bytes, white_gpu: bytes,
                       black_native: bytes, white_native: bytes) -> int:
    # Opaque black/white paired backdrops expose effective opacity in RGB difference.
    # Also compare render-target alpha directly in frame_compare; neither check replaces the other.
    max_delta = 0
    semi = 0
    for i in range(0, len(black_gpu), 4):
        for j, src in ((2, 0), (1, 1), (0, 2)):
            a = white_native[i+src] - black_native[i+src]
            b = white_gpu[i+j] - black_gpu[i+j]
            max_delta = max(max_delta, abs(a-b))
            semi += 0 < a < 255
    need(semi > 10000 and max_delta <= 4,
         "Black/white GPU pair fails native effective-opacity constraint")
    return max_delta


def validate_runtime(run: Path, expected_fixture_sha: str) -> dict:
    run = run.resolve()
    allowed = ROOT / "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/CityHallCameraMaterialGpuAudit"
    need(run.parent == allowed.resolve(), "Runtime outside dedicated Hall material audit directory")
    fixture = read(FIXTURE)
    validate_fixture(fixture, expected_fixture_sha)
    report_path = run / "manifest.json"
    report = read(report_path)
    need(report["schema"] == "rok.city_hall_camera_material_gpu_runtime.v1" and
         report["project"].replace("\\", "/").rstrip("/").lower() == str(ROOT).replace("\\", "/").lower() and
         report["map"] == "/Game/Maps/L_RokUnityGameAppCity" and
         report["fixture_sha256"] == report["expected_fixture_sha256"] == expected_fixture_sha and
         report["native_sha256"] == SHA_NATIVE and report["source_geometry_sha256"] == SHA_GEOMETRY and
         report["opt_in_flag"] and report["local_only"] and
         not report["scene_mutated"] and not report["main_view_registered"] and
         not report["gameapp_accepted"] and not report["lod_accepted"] and
         report["http_dispatch_before"] == report["http_dispatch_after"] == 0 and
         report["rgb_tolerance_lsb"] == 2 and report["alpha_tolerance_lsb"] == 2 and
         report["complete"] and report["passed"] and not report["failures"] and
         len(report["inputs"]) == 5 and len(report["frames"]) == 4,
         "Runtime identity, boundary or acceptance state differs")
    for index, item in enumerate(report["inputs"]):
        source = fixture["inputs"][index]
        file = run / item["gpu_file"]
        need(file.parent == run and item["kind"] == source["kind"] and
             file.read_bytes() == safe(source).read_bytes() and
             sha(file) == item["gpu_sha256"] == source["sha256"] and
             item["source_sha256"] == source["sha256"] and item["gpu_byte_exact"] and
             item["ready_frame"] >= item["copy_frame"],
             f"GPU input {index} readback not original-byte exact")
    native_bytes = []
    gpu_bytes = []
    results = []
    for index, frame in enumerate(report["frames"]):
        case = fixture["cases"][index]
        raw, png = run / frame["gpu_raw_file"], run / frame["gpu_png_file"]
        need(raw.parent == run and png.parent == run and frame["phase"] == case["phase"] and
             sha(raw) == frame["gpu_raw_sha256"] and sha(png) == frame["gpu_png_sha256"] and
             png.read_bytes().startswith(PNG_MAGIC) and frame["ready_frame"] >= frame["copy_frame"] and
             frame["rgba_passed"], "GPU frame identity, hash or fence differs")
        data = raw.read_bytes()
        with Image.open(png) as image:
            need(image.format == "PNG" and image.size == (1440, 810) and
                 image.convert("RGBA").tobytes() ==
                 bytes(v for i in range(0, len(data), 4) for v in (data[i+2],data[i+1],data[i],data[i+3])),
                 "UE GPU PNG is not decoded raw BGRA output")
        original = safe(case["native_raw"]).read_bytes()
        measures = frame_compare(data, original)
        need(frame["max_rgb_delta"] == measures["max_rgb_delta"] and
             frame["max_alpha_delta"] == measures["max_alpha_delta"] and
             frame["rgb_pixels_over_2_lsb"] == measures["pixels_over_2_lsb"] and
             frame["native_rgb_coverage_pixels"] == measures["coverage_pixels"],
             "Runtime comparison metrics disagree with independent pixel recomputation")
        native_bytes.append(original); gpu_bytes.append(data); results.append(measures)
    pair = {"day": pair_alpha_compare(gpu_bytes[0],gpu_bytes[1],native_bytes[0],native_bytes[1]),
            "night": pair_alpha_compare(gpu_bytes[2],gpu_bytes[3],native_bytes[2],native_bytes[3])}
    return {"schema": "rok.city_hall_camera_material_gpu_runtime_verify.v1", "passed": True,
            "run": run.relative_to(ROOT).as_posix(), "runtime_sha256": sha(report_path),
            "fixture_sha256": expected_fixture_sha, "native_sha256": SHA_NATIVE,
            "input_byte_exact": 5, "rgba_frames": 4,
            "black_white_effective_alpha_max_delta": pair, "frames": results,
            "scope": "isolated source-camera/material GPU only; GameApp behavior/LOD/main view unqualified"}


def self_test() -> dict:
    sample_gpu = bytes([0, 0, 0, 255] * (1440*810))
    sample_native = bytes([0, 0, 0, 255] * (1440*810))
    rejects = 0
    try:
        frame_compare(sample_gpu, sample_native)
    except AssertionError:
        rejects += 1
    try:
        pair_alpha_compare(sample_gpu, sample_gpu, sample_native, sample_native)
    except AssertionError:
        rejects += 1
    # 100001 nonopaque source pixels with exact BGRA composition are a positive
    # control. Mutate only RGB and then only alpha; both must fail independently.
    source = bytearray(sample_native)
    gpu = bytearray(sample_gpu)
    for i in range(100001):
        source[4*i:4*i+4] = bytes((40, 80, 120, 200))
        gpu[4*i:4*i+4] = bytes((120, 80, 40, 200))
    frame_compare(bytes(gpu), bytes(source))
    gpu[0] = 125
    try:
        frame_compare(bytes(gpu), bytes(source))
    except AssertionError:
        rejects += 1
    gpu[0] = 120
    gpu[3] = 205
    try:
        frame_compare(bytes(gpu), bytes(source))
    except AssertionError:
        rejects += 1
    need(rejects == 4, "Negative self-test did not reject empty/zero-pair/RGB/alpha faults")
    return {"negative_rejections": rejects}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path)
    parser.add_argument("--expected-fixture-sha", required=True)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    need(len(args.expected_fixture_sha) == 64, "Explicit external fixture SHA required")
    result = validate_runtime(args.run, args.expected_fixture_sha) if args.run else {}
    if args.self_test:
        result["self_test"] = self_test()
    if args.run:
        path = args.run / "verified_runtime.json"
        need(not path.exists(), "Verified runtime report already exists")
        path.write_text(json.dumps(result, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
