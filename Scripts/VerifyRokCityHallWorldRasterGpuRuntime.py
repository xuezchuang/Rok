"""Verify two isolated UE Hall world-pose UV frames against the sealed Unity GPU oracle."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
BATCH = ROOT / "Saved/RokUnityMigration/BatchReview20261009QuestChapterAndWorldHall"
RUNS = BATCH / "CityHallWorldRasterAudit"
FIXTURE = ROOT / "Saved/RokUnityMigration/CityHallWorldRasterGpuFixture/v1/manifest.json"
SOURCE = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json"
SOURCE_SHA = "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273"
ROW_SHA = "6b2844db3726051827252fe03f5892b2d99d54675c3d9e673f78a05197cb2a5e"
HALL_SHA = "01b924d764395708e20766834595fa7943f1ec61c70215b6058914c2cc80ab5e"
HALL_CONTRACT_SHA = "60bfd58cc2c4897a49cb4440474825c641cc30685e40dc1fa6e404063fd56dce"
NATIVE_PROJECT = ROOT / "Saved/UnityCityHallWorldRasterProbeV1"
PHASES = ("prefab_default_hall", "runtime_replay_hall")
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
PIN_PATHS = (
    "Source/Rok/RokCityHallWorldRasterAuditSubsystem.h",
    "Source/Rok/RokCityHallWorldRasterAuditSubsystem.cpp",
    "Scripts/PrepareRokCityHallWorldRasterGpuFixture.py",
    "Scripts/VerifyRokCityHallWorldRasterGpuRuntime.py",
    "Source/RokRendering/Public/RokCitySourceGeometryProbe.h",
    "Source/RokRendering/Private/RokCitySourceGeometryProbe.cpp",
    "Shaders/Private/RokCitySourceGeometryProbe.usf",
    "Scripts/PrepareRokUnityCityHallWorldRasterProbe.py",
    "Scripts/VerifyRokUnityCityHallWorldRasterProbe.py",
    "Saved/UnityCityHallWorldRasterProbeV1/Assets/Editor/RenderRokUnityCityHallWorldRasterNative.cs",
    "Saved/UnityCityHallWorldRasterProbeV1/Assets/ProbeLocalUv.shader",
)


def need(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def project_file(value: str) -> Path:
    path = (ROOT / value).resolve()
    need(path.is_relative_to(ROOT.resolve()), f"Path outside project: {value}")
    return path


def run_file(run: Path, value: str) -> Path:
    path = (run / value).resolve()
    need(path.parent == run, f"Path outside exact run: {value}")
    return path


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def load64(record: dict, *, native_project_relative: bool = False) -> list[float]:
    path = ((NATIVE_PROJECT / record["path"]).resolve() if native_project_relative else
            project_file(record["path"]))
    need(path.is_relative_to(NATIVE_PROJECT.resolve()) if native_project_relative else
         path.is_relative_to(ROOT.resolve()), "Matrix path escapes")
    need(path.stat().st_size == record["byte_count"] == 64 and sha(path) == record["sha256"],
         f"Pinned matrix differs: {path}")
    values = struct.unpack("<16f", path.read_bytes())
    need(all(math.isfinite(x) for x in values), "Matrix has nonfinite member")
    return list(values)


def verify(run: Path, expected_fixture_sha: str) -> dict:
    run = run.resolve()
    need(run.parent == RUNS.resolve(), "UE run is not a direct unique audit child")
    need(len(expected_fixture_sha) == 64 and all(x in "0123456789abcdef" for x in expected_fixture_sha),
         "Explicit lowercase external fixture SHA required")
    need(sha(FIXTURE) == expected_fixture_sha, "Fixture bytes differ from launch anchor")
    fixture, runtime, source = read(FIXTURE), read(run / "manifest.json"), read(SOURCE)
    need(fixture["schema"] == "rok.city_hall_world_raster_gpu_fixture.v1" and
         fixture["source_manifest_sha256"] == sha(SOURCE) == SOURCE_SHA and
         fixture["source_row_sha256"] == ROW_SHA and
         fixture["hall_native3_sha256"] == HALL_SHA and
         fixture["hall_contract_sha256"] == HALL_CONTRACT_SHA and
         len(fixture["inputs"]) == 3 and len(fixture["cases"]) == 2,
         "Source Hall raster fixture closure differs")
    need(runtime["schema"] == "rok.city_hall_world_raster_gpu_runtime.v1" and
         runtime["fixture_path"] == FIXTURE.relative_to(ROOT).as_posix() and
         runtime["fixture_sha256"] == runtime["expected_fixture_sha256"] == expected_fixture_sha and
         runtime["source_manifest_sha256"] == SOURCE_SHA and
         runtime["map"] == "/Game/Maps/L_RokUnityGameAppCity" and
         runtime["project"].replace("\\", "/").rstrip("/").lower() == "d:/uedemo/rok" and
         runtime["opt_in_flag"] and runtime["local_only"] and
         runtime["http_dispatch_before"] == runtime["http_dispatch_after"] == 0 and
         runtime["scene_mutated"] is False and runtime["main_view_registered"] is False and
         runtime["gameapp_accepted"] is False and runtime["material_alpha_accepted"] is False and
         runtime["live_world_actor_accepted"] is False and runtime["lod_accepted"] is False and
         runtime["camera_accepted"] is False and runtime["input_assembler_accepted"] is False and
         runtime["complete"] and runtime["passed"] and runtime["failures"] == [],
         "Runtime opt-in/local/isolated acceptance boundary differs")
    need(runtime["expected_source_buffers"] == 3 and runtime["expected_matrix_buffers"] == 2 and
         runtime["gpu_byte_exact_buffers"] == 5 and
         runtime["gpu_byte_exact_source_buffers"] == 3 and
         runtime["gpu_byte_exact_matrix_buffers"] == 2 and
         runtime["expected_frames"] == 2 and
         runtime["strict_rgb_coverage_frames"] == 2 and runtime["rgb_tolerance_lsb"] == 1 and
         runtime["native_row_order"] == "top_left_rgba8" and
         runtime["ue_row_order"] == "GPU staging rows tight top-left BGRA8" and
         len(runtime["inputs"]) == 5 and len(runtime["frames"]) == 2,
         "Runtime five-buffer/two-frame counts differ")
    need([x["path"] for x in fixture["pinned_files"]] == list(PIN_PATHS),
         "Implementation/native producer pin path set or order differs")
    for record in fixture["pinned_files"]:
        need(sha(project_file(record["path"])) == record["sha256"],
             f"Implementation/native producer pin changed: {record['path']}")
    for path in PIN_PATHS[:7]:
        key = Path(path).name + "_sha256"
        need(runtime[key] == sha(project_file(path)), f"Runtime code pin differs: {path}")
    for name in ("hall_native3", "hall_verify", "native_contract", "native_manifest", "native_verify"):
        pin = fixture[name]
        need(sha(project_file(pin["path"])) == pin["sha256"] == runtime[name + "_sha256"],
             f"Runtime source/oracle pin differs: {name}")
    hall, hall_verified = read(project_file(fixture["hall_native3"]["path"])), read(project_file(fixture["hall_verify"]["path"]))
    contract, native, verified = (read(project_file(fixture[key]["path"]))
                                  for key in ("native_contract", "native_manifest", "native_verify"))
    need(sha(project_file(fixture["hall_native3"]["path"])) == HALL_SHA and
         hall["contract_sha256"] == HALL_CONTRACT_SHA and hall_verified["passed"] and
         hall_verified["native"]["source_composed_matrix_count"] == 38 and
         contract["schema"] == "rok.unity_city_hall_world_raster_contract.v4" and
         native["schema"] == "rok.unity_city_hall_world_raster_native.v1" and native["complete"] and
         native["contract_sha256"] == fixture["native_contract"]["sha256"] and
         verified["passed"] and verified["native"]["native_sha256"] == fixture["native_manifest"]["sha256"] and
         verified["native"]["cases_checked"] == verified["native"]["png_raw_exact_cases"] == 2,
         "Hall Native3/new raster oracle proof differs")
    row = source["rows"][27]
    need(row["source_key"] == "initial:5" and row["source_row_sha256"] == ROW_SHA,
         "Source geometry row27 differs")
    source_parts = (row["positions"], row["uv_channels"][0]["binary"], row["indices"]["binary"])
    source_inputs = []
    for kind, record, pinned in zip(("positions", "uv0", "indices"), source_parts, fixture["inputs"]):
        need(pinned["kind"] == kind and all(pinned[field] == record[field]
             for field in ("path", "sha256", "count", "stride_bytes", "byte_count")),
             f"Fixture typed source buffer differs: {kind}")
        need(sha(project_file(record["path"])) == record["sha256"], f"Source binary changed: {kind}")
        source_inputs.append(record)
    vertices = list(struct.iter_unpack("<3f", project_file(source_parts[0]["path"]).read_bytes()))
    need(len(vertices) == 240, "Hall vertex count differs")
    c = load64(fixture["shared_world_to_clip"])
    need(fixture["shared_world_to_clip"]["sha256"] ==
         contract["shared_fit"]["world_to_clip_column"]["sha256"] and
         contract["shared_fit"]["axes"] == [0, 2], "Shared union clip fit differs")
    world_matrices = []
    for phase, case in zip(PHASES, fixture["cases"]):
        need(case["phase"] == phase and case["source_key"] == "initial:5" and
             case["source_row_sha256"] == ROW_SHA, "Case phase/source identity differs")
        world = load64(case["world_matrix"])
        final = load64(case["matrix"])
        corresponding = next(x for x in hall["nodes"] if x["phase"] == phase and
                             x["component_type"] == "SpriteRenderer" and
                             x["source_file_id"] == "212751466141171401" and
                             x["source_guid"] == "c825e8897df8e424e817e4ed05eef53e")
        need(case["world_matrix"]["sha256"] == corresponding["matrix_sha256"] and
             case["matrix"]["sha256"] == contract["cases"][case["index"]]
             ["source_to_clip_row_major"]["sha256"],
             "World/final matrix provenance differs")
        product = [f32(sum(c[r * 4 + k] * world[k * 4 + col] for k in range(4)))
                   for r in range(4) for col in range(4)]
        expected_final = [product[col * 4 + r] for r in range(4) for col in range(4)]
        need(max(abs(x - y) for x, y in zip(final, expected_final)) <= 2e-6,
             "Final shader matrix differs from source world composition")
        world_matrices.append(world)
    need(fixture["cases"][0]["matrix"]["sha256"] != fixture["cases"][1]["matrix"]["sha256"],
         "Two pose matrices collapsed")
    samples = [[], [], []]
    for world in world_matrices:
        for vertex in vertices:
            for axis in range(3):
                samples[axis].append(f32(sum(world[axis * 4 + k] * vertex[k]
                                             for k in range(3)) + world[axis * 4 + 3]))
    ranges = [[min(values), max(values)] for values in samples]
    fit = contract["shared_fit"]
    need(all(abs(a - b) <= 2e-6 for pair, expected in zip(fit["world_ranges"], ranges)
             for a, b in zip(pair, expected)), "Union world AABB differs")
    center_x = f32(sum(ranges[0]) / 2)
    center_z = f32(sum(ranges[2]) / 2)
    scale = f32(1.8 / max(ranges[0][1] - ranges[0][0], ranges[2][1] - ranges[2][0]))
    expected_c = [0.0] * 16
    expected_c[0], expected_c[3], expected_c[6], expected_c[7] = (
        scale, f32(-center_x * scale), scale, f32(-center_z * scale))
    expected_c[11], expected_c[15] = 0.5, 1.0
    need(max(abs(a - b) for a, b in zip(c, expected_c)) <= 2e-6 and
         all(abs(a - b) <= 2e-6 for a, b in zip(fit["centers"], [center_x, center_z])) and
         abs(fit["uniform_scale"] - scale) <= 2e-6,
         "Union world-to-clip matrix is not original two-pose fit")
    exact = 0
    for slot, record in enumerate([*fixture["inputs"],
                                   fixture["cases"][0]["matrix"], fixture["cases"][1]["matrix"]]):
        observed = runtime["inputs"][slot]
        kind = record["kind"] if slot < 3 else "matrix_" + PHASES[slot - 3]
        gpu = run_file(run, observed["gpu_file"])
        need(observed["kind"] == kind and observed["source_path"] == record["path"] and
             observed["source_sha256"] == record["sha256"] and
             observed["byte_exact"] and observed["byte_count"] == len(gpu.read_bytes()) and
             observed["gpu_sha256"] == sha(gpu) and gpu.read_bytes() == project_file(record["path"]).read_bytes() and
             observed["ready_frame"] >= observed["copy_frame"],
             f"GPU typed source/matrix byte readback differs: {kind}")
        exact += 1
    strict = 0
    for index, (phase, case, native_case, frame) in enumerate(zip(PHASES, fixture["cases"],
                                                                  native["cases"], runtime["frames"])):
        need(case["index"] == native_case["index"] == frame["case_index"] == index and
             case["phase"] == native_case["phase"] == frame["phase"] == phase and
             native_case["source_key"] == "initial:5" and
             native_case["renderer_file_id"] == "212751466141171401" and
             native_case["sprite_guid"] == "57ec7ddf0b743a140b2707fe14b536e2" and
             native_case["world_matrix_sha256"] == case["world_matrix"]["sha256"] and
             native_case["source_to_clip_sha256"] == case["matrix"]["sha256"] and
             native_case["shared_fit_sha256"] == fixture["shared_world_to_clip"]["sha256"] and
             native_case["png_decodes_to_raw_scope"] and native_case["opaque_alpha"] and
             frame["world_matrix_sha256"] == case["world_matrix"]["sha256"] and
             frame["matrix_sha256"] == case["matrix"]["sha256"],
             "Frame pose/matrix identity differs")
        native_raw = project_file(case["native_raw_path"])
        native_png = project_file(case["native_png_path"])
        need(sha(native_raw) == case["native_raw_sha256"] == native_case["raw_top_left"]["sha256"] and
             sha(native_png) == case["native_png_sha256"] == native_case["png"]["sha256"] and
             native_png.read_bytes().startswith(PNG_MAGIC), "Native raw/PNG pin differs")
        with Image.open(native_png) as image:
            need(image.format == "PNG" and image.size == (128, 128), "Native true PNG differs")
            need(image.convert("RGBA").tobytes() == native_raw.read_bytes(),
                 "Native PNG decoded pixels differ from source raw")
        gpu = run_file(run, frame["gpu_bgra_file"])
        png = run_file(run, frame["gpu_png_file"])
        need(len(gpu.read_bytes()) == 128 * 128 * 4 and sha(gpu) == frame["gpu_bgra_sha256"] and
             png.read_bytes().startswith(PNG_MAGIC) and sha(png) == frame["gpu_png_sha256"],
             "UE GPU raw/PNG file hash/format differs")
        with Image.open(png) as image:
            need(image.format == "PNG" and image.size == (128, 128), "UE output is not a true PNG")
            decoded = image.convert("RGBA").tobytes()
        bgra = gpu.read_bytes()
        rgba = bytes(channel for i in range(0, len(bgra), 4)
                     for channel in (bgra[i + 2], bgra[i + 1], bgra[i], bgra[i + 3]))
        need(decoded == rgba, "UE PNG decode differs from exact GPU BGRA swizzle")
        reference = native_raw.read_bytes()
        max_rgb = max(abs(reference[i + k] - rgba[i + k])
                      for i in range(0, len(rgba), 4) for k in range(3))
        over_one = sum(any(abs(reference[i + k] - rgba[i + k]) > 1 for k in range(3))
                       for i in range(0, len(rgba), 4))
        native_cover = [reference[i + 2] < 122 for i in range(0, len(rgba), 4)]
        ue_cover = [rgba[i + 2] < 122 for i in range(0, len(rgba), 4)]
        mismatch = sum(a != b for a, b in zip(native_cover, ue_cover))
        need(max_rgb == frame["max_rgb_delta"] and over_one == frame["pixels_over_1_lsb"] and
             mismatch == frame["coverage_mismatch_pixels"] and
             sum(native_cover) == frame["native_coverage_pixels"] and
             sum(ue_cover) == frame["ue_coverage_pixels"] and
             frame["native_png_equals_raw"] and frame["rgb_and_coverage_passed"] and
             max_rgb <= 1 and over_one == mismatch == 0 and
             0 < sum(native_cover) == sum(ue_cover) < 128 * 128 and
             frame["ready_frame"] >= frame["copy_frame"],
             f"Strict RGB/coverage parity differs: {phase}")
        strict += 1
    return {"schema": "rok.city_hall_world_raster_gpu_runtime_verify.v1", "passed": True,
            "fixture_sha256": expected_fixture_sha, "runtime_manifest_sha256": sha(run / "manifest.json"),
            "native_manifest_sha256": fixture["native_manifest"]["sha256"],
            "gpu_byte_exact_source_buffers": 3, "gpu_byte_exact_matrix_buffers": 2,
            "strict_rgb_coverage_frames": strict, "rgb_tolerance_lsb": 1,
            "transform_fixture_only": True, "live_world_actor_accepted": False,
            "gameapp_mainview_accepted": False, "source_material_alpha_lod_accepted": False,
            "goal_complete": False}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path, required=True)
    parser.add_argument("--fixture-sha", required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = verify(args.run, args.fixture_sha)
    if args.output:
        need(not args.output.exists(), "Verification report already exists")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
