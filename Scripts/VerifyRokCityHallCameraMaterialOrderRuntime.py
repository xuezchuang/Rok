"""Independently verify v5 Hall object-then-VP fixture and private GPU frames."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

from PIL import Image

from PrepareRokCityHallCameraMaterialOrderFixture import (
    BASE, CODE, OUT, PROJECTION, ROOT, SOURCE_HASHES, UNITY_INCLUDE,
    VARIANTS, VIEW, WORLD, f32, matrix, multiply, pin, sha, transpose,
)
from VerifyRokCityHallCameraMaterialBcRuntime import metrics, pinned

KINDS = ("positions", "uv0", "indices", "sprite", "mask")
PHASES = ("day_black", "day_white", "night_black", "night_white")
AUDIT_ROOT = ROOT / "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/CityHallCameraMaterialGpuAudit"


def need(value: bool, reason: str) -> None:
    if not value:
        raise AssertionError(reason)


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def verify_fixture(path: Path, expected_sha: str) -> dict:
    need(path.resolve() == (OUT / "order/manifest.json").resolve() and
         sha(path) == expected_sha and len(expected_sha) == 64,
         "External v5 order fixture identity differs")
    fixture = read(path)
    base = read(BASE)
    need(fixture["schema"] == "rok.city_hall_camera_material_gpu_fixture.v5" and
         fixture["variant"] == "source_bc_object_vp" and
         fixture["source_key"] == "initial:5" and
         fixture["sprite_renderer_file_id"] == "212751466141171401" and
         fixture["native_sha256"] == "3e2d70c5a9c2a845f5e34b8e9e80fe95d2218142b9697a9d63236beb4d6ef451" and
         fixture["native_v2_sha256"] == "d70a6dfb256f5a3a95f7c81ddf2def925b7ab46ea9d187b53a9a0946dcf94f9d" and
         fixture["hall_native3_sha256"] == "01b924d764395708e20766834595fa7943f1ec61c70215b6058914c2cc80ab5e" and
         fixture["native_camera2_sha256"] == "417eb43964bd2009879561194e4c5a75c7808f257dc6b84f8707ed4892b0d944" and
         (fixture["width"], fixture["height"], fixture["target_format"], fixture["target_srgb"]) ==
         (1440, 810, "PF_B8G8R8A8", False) and
         not fixture["main_view_registered"] and not fixture["lod_qualified"] and
         not fixture["gameapp_accepted"] and not fixture["unity_vp_bytes_measured"] and
         fixture["expected_http_dispatches"] == 0, "v5 fixture source/scope differs")
    need(fixture["matrix_formula"] ==
         "row-vector local * transpose(renderer_world), then world * transpose(camera_gpu_projection_rt * camera_world_to_camera); explicit clipY adapter" and
         fixture["source_order_qualification"] ==
         "source-confirmed two GPU muls; VP bytes derived from sealed P_rt*V, not measured unity_MatrixVP",
         "Source order/VP qualification differs")
    include = pinned(fixture["unity_object_to_clip_source"])
    need(include.read_bytes() == UNITY_INCLUDE.read_bytes() and
         b"return mul(UNITY_MATRIX_VP, mul(unity_ObjectToWorld, float4(pos, 1.0)));" in include.read_bytes(),
         "Unity 2019 source macro differs")
    need(pinned(fixture["world_matrix_row"]).read_bytes() ==
         struct.pack("<16f", *transpose(matrix(WORLD))),
         "Object-to-world row matrix differs from Native3")
    need(pinned(fixture["view_projection_matrix_row"]).read_bytes() ==
         struct.pack("<16f", *transpose(multiply(matrix(PROJECTION), matrix(VIEW)))),
         "Source-derived VP row matrix differs from NativeCamera2")
    need(len(fixture["inputs"]) == 5 and len(fixture["cases"]) == 4 and
         [row["kind"] for row in fixture["inputs"]] == list(KINDS) and
         [row["phase"] for row in fixture["cases"]] == list(PHASES),
         "Source input/frame set differs")
    for group in ("native_pins", "material_pins", "code_pins"):
        rows = fixture[group]
        need(len(rows) == len({row["path"] for row in rows}), f"Duplicate {group} pin")
        for row in rows:
            pinned(row)
    need({row["path"] for row in fixture["code_pins"]} == set(CODE),
         "Exact v5 code pin set differs")
    for index in range(5):
        source = fixture["inputs"][index]
        reference = base["inputs"][index]
        need(source == reference and pinned(source).read_bytes() == pinned(reference).read_bytes(),
             "BC or geometry bytes differ from v3")
    for index, row in enumerate(fixture["cases"]):
        need(row == base["cases"][index] and row["phase"] == PHASES[index],
             "Native1 source frame state differs")
        with Image.open(pinned(row["native_png"])) as image:
            need(image.size == (1440, 810) and
                 image.convert("RGBA").tobytes() == pinned(row["native_raw"]).read_bytes(),
                 "Native1 PNG/raw oracle differs")
    return fixture


def verify_run(run: Path, fixture: dict, fixture_sha: str) -> dict:
    run = run.resolve()
    need(run.parent == AUDIT_ROOT.resolve(), "Run outside isolated Hall GPU audit root")
    report_path = run / "manifest.json"
    report = read(report_path)
    need(report["schema"] == "rok.city_hall_camera_material_gpu_runtime.v3" and
         report["variant"] == "source_bc_object_vp" and report["complete"] and
         report["passed"] and not report["failures"] and
         report["fixture_sha256"] == report["expected_fixture_sha256"] == fixture_sha and
         report["project"].replace("\\", "/").rstrip("/").lower() == str(ROOT).replace("\\", "/").lower() and
         report["map"] == "/Game/Maps/L_RokUnityGameAppCity" and
         report["opt_in_flag"] and report["local_only"] and
         report["http_dispatch_before"] == report["http_dispatch_after"] == 0 and
         report["gpu_byte_exact_inputs"] == 3 and
         report["compressed_gpu_descriptor_inputs"] == 2 and
         report["compressed_gpu_sampling_verified_by_frames"] and
         report["two_stage_object_vp"] and not report["unity_vp_bytes_measured"] and
         report["rgb_tolerance_lsb"] == report["alpha_tolerance_lsb"] == 2 and
         not report["scene_mutated"] and not report["main_view_registered"] and
         not report["lod_accepted"] and not report["gameapp_accepted"] and
         len(report["inputs"]) == 5 and len(report["frames"]) == 4,
         "GPU runtime identity/qualification differs")
    for index, item in enumerate(report["inputs"]):
        source = fixture["inputs"][index]
        need(item["kind"] == source["kind"] and item["source_sha256"] == source["sha256"],
             "GPU source input identity differs")
        if index < 3:
            file = run / item["gpu_file"]
            need(file.parent == run and file.read_bytes() == pinned(source).read_bytes() and
                 sha(file) == item["gpu_sha256"] and item["gpu_byte_exact"],
                 "GPU geometry readback differs")
        else:
            need(item["compressed_source_hash_verified"] and item["gpu_descriptor_verified"] and
                 not item["gpu_byte_exact"] and item["gpu_texture_format"] == source["rhi_format"] and
                 item["byte_count"] == source["byte_count"],
                 "BC source/descriptor witness differs")
    result = []
    for index, item in enumerate(report["frames"]):
        source = fixture["cases"][index]
        raw, png = run / item["gpu_raw_file"], run / item["gpu_png_file"]
        need(raw.parent == png.parent == run and item["phase"] == source["phase"] and
             sha(raw) == item["gpu_raw_sha256"] and sha(png) == item["gpu_png_sha256"] and
             item["rgba_passed"] and item["ready_frame"] >= item["copy_frame"],
             "GPU frame evidence differs")
        data = raw.read_bytes()
        rgba = bytes(channel for p in range(0, len(data), 4)
                     for channel in (data[p+2], data[p+1], data[p], data[p+3]))
        with Image.open(png) as image:
            need(image.size == (1440, 810) and image.convert("RGBA").tobytes() == rgba,
                 "GPU PNG does not decode to raw BGRA")
        measured = metrics(data, pinned(source["native_raw"]).read_bytes())
        need(item["max_rgb_delta"] == measured["max_rgb_delta"] and
             item["max_alpha_delta"] == measured["max_alpha_delta"] and
             item["rgb_pixels_over_2_lsb"] == measured["rgb_over_2_lsb"] and
             item["native_rgb_coverage_pixels"] == measured["native_coverage"],
             "GPU pixel metrics differ")
        result.append(measured)
    return {"schema": "rok.city_hall_camera_material_order_gpu_verify.v1", "passed": True,
            "run": run.relative_to(ROOT).as_posix(), "runtime_sha256": sha(report_path),
            "fixture_sha256": fixture_sha, "geometry_gpu_byte_exact": 3,
            "source_bc_rgba_frames": 4, "unity_vp_bytes_measured": False, "frames": result}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--expected-fixture-sha", required=True)
    parser.add_argument("--run", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    fixture = verify_fixture(OUT / "order/manifest.json", args.expected_fixture_sha)
    result: dict = {"fixture_verified": True, "fixture_sha256": args.expected_fixture_sha}
    if args.self_test:
        rejected = 0
        for change in (lambda x: x["world_matrix_row"].update(sha256="0"*64),
                       lambda x: x.update(unity_vp_bytes_measured=True),
                       lambda x: x["inputs"][3].update(sha256="0"*64)):
            clone = json.loads(json.dumps(fixture))
            change(clone)
            try:
                verify_fixture_dict(clone, args.expected_fixture_sha)
            except AssertionError:
                rejected += 1
        need(rejected == 3, "Negative fixture mutations escaped")
        result["negative_cases"] = rejected
    if args.run:
        result = verify_run(args.run, fixture, args.expected_fixture_sha)
        destination = args.run / "verified_order_runtime.json"
        need(not destination.exists(), "Do not overwrite runtime verification")
        destination.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


def verify_fixture_dict(fixture: dict, expected_sha: str) -> None:
    """Only mutation-focused checks; full source verification occurs in verify_fixture."""
    need(fixture["schema"] == "rok.city_hall_camera_material_gpu_fixture.v5" and
         fixture["variant"] == "source_bc_object_vp" and
         not fixture["unity_vp_bytes_measured"], "Mutated scope escaped")
    pinned(fixture["world_matrix_row"])
    pinned(fixture["view_projection_matrix_row"])
    for row in fixture["inputs"]:
        pinned(row)


if __name__ == "__main__":
    main()
