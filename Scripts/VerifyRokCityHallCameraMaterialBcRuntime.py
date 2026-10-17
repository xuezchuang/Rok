"""Strict Hall BC RHI audit verifier: actual GPU four-phase sampling against V1 RGBA."""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

from PIL import Image

from PrepareRokCityHallCameraMaterialBcFixture import CODE, OUT, ROOT, V2_NATIVE_SHA, V2_CONTRACT_SHA

KINDS = ("positions", "uv0", "indices", "sprite", "mask")
PHASES = ("day_black", "day_white", "night_black", "night_white")
NATIVE1 = "3e2d70c5a9c2a845f5e34b8e9e80fe95d2218142b9697a9d63236beb4d6ef451"
GEOMETRY = "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273"


def need(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pinned(item: dict) -> Path:
    name = item["path"]
    need(isinstance(name, str) and name and not name.startswith("/") and ":" not in name and
         "\\" not in name and ".." not in Path(name).parts, "Unsafe source pin")
    path = (ROOT / name).resolve()
    need(path.is_relative_to(ROOT.resolve()) and path.is_file() and
         sha(path) == item["sha256"] and path.stat().st_size == item["byte_count"],
         f"Source pin differs: {name}")
    return path


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def fixture_check(fixture: dict, fixture_path: Path, external_sha: str) -> None:
    need(sha(fixture_path) == external_sha and fixture_path in
         ((OUT / "bc/manifest.json"), (OUT / "expanded/manifest.json")), "External fixture identity differs")
    variant = "source_bc" if fixture_path.parent.name == "bc" else "expanded_rgba"
    need(fixture["schema"] == "rok.city_hall_camera_material_gpu_fixture.v3" and
         fixture["variant"] == variant and fixture["native_sha256"] == NATIVE1 and
         fixture["native_v2_sha256"] == V2_NATIVE_SHA and
         fixture["native_v2_contract_sha256"] == V2_CONTRACT_SHA and
         fixture["source_geometry_sha256"] == GEOMETRY and
         fixture["source_key"] == "initial:5" and fixture["width"] == 1440 and
         fixture["height"] == 810 and fixture["target_format"] == "PF_B8G8R8A8" and
         not fixture["target_srgb"] and not fixture["main_view_registered"] and
         not fixture["lod_qualified"] and not fixture["gameapp_accepted"] and
         fixture["expected_http_dispatches"] == 0, "Fixture scope/source changed")
    need(len(fixture["inputs"]) == 5 and len(fixture["cases"]) == 4 and
         [x["kind"] for x in fixture["inputs"]] == list(KINDS) and
         [x["phase"] for x in fixture["cases"]] == list(PHASES), "Fixture inputs/cases differ")
    for group in ("native_pins", "material_pins", "code_pins"):
        entries = fixture[group]
        need(len(entries) == len({x["path"] for x in entries}), f"Duplicate {group} pin")
        for entry in entries:
            pinned(entry)
    need({x["path"] for x in fixture["code_pins"]} == set(CODE), "Code pin set incomplete")
    need(pinned(fixture["matrix"]).stat().st_size == 64, "Matrix bytes differ")
    original = read(ROOT / "Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v1/manifest.json")
    for i in range(3):
        need(pinned(fixture["inputs"][i]).read_bytes() == pinned(original["inputs"][i]).read_bytes(),
             "Source geometry input differs")
    native2 = read(ROOT / "Saved/UnityCityHallCameraMaterialProbeV2/OutputRuns/20261009_061201_264_46812/hall_camera_material_native_v2.json")
    for i, role, expected, block in ((3, "main", "PF_DXT5", 16), (4, "mask", "PF_DXT1", 8)):
        entry = fixture["inputs"][i]
        if variant == "source_bc":
            source = native2[role + "_texture"]["compressed_blocks"]
            source_path = ROOT / "Saved/UnityCityHallCameraMaterialProbeV2/OutputRuns/20261009_061201_264_46812" / source["path"]
            need(entry["rhi_format"] == expected and not entry["srgb"] and entry["num_mips"] == 1 and
                 (entry["block_size_x"], entry["block_size_y"], entry["block_bytes"]) == (4, 4, block) and
                 pinned(entry) == source_path.resolve() and pinned(entry["source_blocks"]) == source_path.resolve() and
                 source_path.stat().st_size == ((entry["width"]+3)//4)*((entry["height"]+3)//4)*block,
                 "Original imported BC block identity/layout differs")
        else:
            need(entry == original["inputs"][i] and pinned(entry).is_file(), "Expanded source texture differs")
    for i, entry in enumerate(fixture["cases"]):
        need(entry["material_queue"] == (2995 if i < 2 else 2996) and
             entry["background"] == ("black" if i % 2 == 0 else "white"), "Case source state differs")
        with Image.open(pinned(entry["native_png"])) as image:
            need(image.size == (1440, 810) and image.convert("RGBA").tobytes() == pinned(entry["native_raw"]).read_bytes(),
                 "V1 oracle PNG/raw differs")


def metrics(gpu: bytes, source: bytes) -> dict:
    need(len(gpu) == len(source) == 1440 * 810 * 4, "Frame extent differs")
    rgb_max = alpha_max = over = coverage = nonopaque = 0
    for i in range(0, len(source), 4):
        d = max(abs(gpu[i+2]-source[i]), abs(gpu[i+1]-source[i+1]), abs(gpu[i]-source[i+2]))
        a = abs(gpu[i+3]-source[i+3])
        rgb_max = max(rgb_max, d); alpha_max = max(alpha_max, a); over += d > 2
        coverage += source[i] != source[0] or source[i+1] != source[1] or source[i+2] != source[2]
        nonopaque += source[i+3] != 255
    need(rgb_max <= 2 and alpha_max <= 2 and over == 0 and coverage > 80000 and nonopaque > 1000,
         "Actual GPU BC sampled frame differs from native RGBA over 2 LSB")
    return {"max_rgb_delta": rgb_max, "max_alpha_delta": alpha_max, "rgb_over_2_lsb": over,
            "native_coverage": coverage, "native_nonopaque": nonopaque}


def check_run(run: Path, fixture: dict, fixture_sha: str) -> dict:
    root = ROOT / "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/CityHallCameraMaterialGpuAudit"
    run = run.resolve()
    need(run.is_dir() and run.parent == root.resolve(), "Runtime outside isolated audit root")
    report_path = run / "manifest.json"
    report = read(report_path)
    need(report["schema"] == "rok.city_hall_camera_material_gpu_runtime.v2" and
         report["variant"] == fixture["variant"] and report["complete"] and report["passed"] and
         not report["failures"] and report["fixture_sha256"] == report["expected_fixture_sha256"] == fixture_sha and
         report["project"].replace("\\", "/").rstrip("/").lower() == str(ROOT).replace("\\", "/").lower() and
         report["map"] == "/Game/Maps/L_RokUnityGameAppCity" and
         report["opt_in_flag"] and report["local_only"] and
         report["http_dispatch_before"] == report["http_dispatch_after"] == 0 and
         not report["scene_mutated"] and not report["main_view_registered"] and
         not report["lod_accepted"] and not report["gameapp_accepted"] and
         report["rgb_tolerance_lsb"] == report["alpha_tolerance_lsb"] == 2 and
         len(report["inputs"]) == 5 and len(report["frames"]) == 4,
         "Runtime identity or isolation differs")
    bc = fixture["variant"] == "source_bc"
    need(report["gpu_byte_exact_inputs"] == (3 if bc else 5) and
         report["compressed_gpu_descriptor_inputs"] == (2 if bc else 0) and
         report["compressed_gpu_sampling_verified_by_frames"] == bc,
         "GPU compressed proof scope mislabeled")
    for i, item in enumerate(report["inputs"]):
        source = fixture["inputs"][i]
        need(item["kind"] == source["kind"] and item["source_sha256"] == source["sha256"],
             "Runtime source input identity differs")
        if bc and i >= 3:
            block = 16 if i == 3 else 8
            width, height = source["width"], source["height"]
            need(item["gpu_texture_format"] == source["rhi_format"] and
                 item["compressed_source_hash_verified"] and item["gpu_descriptor_verified"] and
                 not item["gpu_byte_exact"] and item["byte_count"] == source["byte_count"] and
                 item["block_row_bytes"] == ((width + 3)//4)*block and
                 item["block_rows"] == (height + 3)//4 and
                 item["initializer_stride_bytes"] >= item["block_row_bytes"],
                 "BC GPU upload/source pitch witness differs")
        else:
            gpu = run / item["gpu_file"]
            need(gpu.parent == run and gpu.read_bytes() == pinned(source).read_bytes() and
                 sha(gpu) == item["gpu_sha256"] and item["gpu_byte_exact"] and
                 item["ready_frame"] >= item["copy_frame"], "GPU buffer/expanded input byte readback differs")
    frame_results = []
    for i, item in enumerate(report["frames"]):
        source = fixture["cases"][i]
        raw, png = run / item["gpu_raw_file"], run / item["gpu_png_file"]
        need(raw.parent == run and png.parent == run and item["phase"] == source["phase"] and
             sha(raw) == item["gpu_raw_sha256"] and sha(png) == item["gpu_png_sha256"] and
             item["ready_frame"] >= item["copy_frame"] and item["rgba_passed"],
             "GPU frame hash/fence/source identity differs")
        data = raw.read_bytes()
        rgba = bytes(v for p in range(0, len(data), 4) for v in (data[p+2], data[p+1], data[p], data[p+3]))
        with Image.open(png) as image:
            need(image.size == (1440, 810) and image.convert("RGBA").tobytes() == rgba,
                 "GPU PNG decoded bytes differ from GPU BGRA readback")
        result = metrics(data, pinned(source["native_raw"]).read_bytes())
        need(item["max_rgb_delta"] == result["max_rgb_delta"] and
             item["max_alpha_delta"] == result["max_alpha_delta"] and
             item["rgb_pixels_over_2_lsb"] == result["rgb_over_2_lsb"] and
             item["native_rgb_coverage_pixels"] == result["native_coverage"],
             "Runtime pixel metric differs from independent recomputation")
        frame_results.append(result)
    return {"schema": "rok.city_hall_camera_material_bc_gpu_verify.v1", "passed": True,
            "run": run.relative_to(ROOT).as_posix(), "runtime_sha256": sha(report_path),
            "fixture_sha256": fixture_sha, "variant": fixture["variant"],
            "gpu_geometry_byte_exact": 3, "gpu_bc_sampling_rgba_frames": 4 if bc else 0,
            "frames": frame_results, "scope": "private source-camera GPU only; GameApp/main-view/LOD false"}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--variant", choices=("bc", "expanded"), required=True)
    parser.add_argument("--expected-fixture-sha", required=True)
    parser.add_argument("--run", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    need(len(args.expected_fixture_sha) == 64, "External root fixture SHA required")
    path = OUT / args.variant / "manifest.json"
    fixture = read(path)
    fixture_check(fixture, path, args.expected_fixture_sha)
    result = {"fixture_verified": True, "fixture_sha256": args.expected_fixture_sha}
    if args.self_test:
        rejected = 0
        for mutate in (lambda x: x.update(native_v2_sha256="0"*64),
                       lambda x: x["inputs"][3].update(sha256="0"*64)):
            clone = json.loads(json.dumps(fixture)); mutate(clone)
            try:
                fixture_check(clone, path, args.expected_fixture_sha)
            except AssertionError:
                rejected += 1
        need(rejected == 2, "Fixture negative cases escaped")
        result["negative_cases"] = 2
    if args.run:
        result = check_run(args.run, fixture, args.expected_fixture_sha)
        destination = args.run / "verified_bc_runtime.json"
        need(not destination.exists(), "Runtime verification report already exists")
        destination.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
