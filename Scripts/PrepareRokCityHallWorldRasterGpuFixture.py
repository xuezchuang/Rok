"""Join the sealed Hall pose and row27 geometry to a verified two-case Unity UV oracle.

This producer never renders or launches an engine. It refuses to write a fixture until
the native oracle and its independent verifier exist and match external SHA anchors.
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
SOURCE = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json"
SOURCE_SHA = "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273"
SOURCE_ROW_SHA = "6b2844db3726051827252fe03f5892b2d99d54675c3d9e673f78a05197cb2a5e"
HALL_NATIVE3 = ROOT / "Saved/UnityCityHallWorldProbeV1/OutputRuns/20261009_032009_843_25796/hall_world_native.json"
HALL_NATIVE3_SHA = "01b924d764395708e20766834595fa7943f1ec61c70215b6058914c2cc80ab5e"
HALL_VERIFY = ROOT / "Saved/RokUnityMigration/BatchReview20261009QuestChapterAndWorldHall/hall_native3_verified_v4.json"
HALL_CONTRACT_SHA = "60bfd58cc2c4897a49cb4440474825c641cc30685e40dc1fa6e404063fd56dce"
NATIVE_PROJECT = ROOT / "Saved/UnityCityHallWorldRasterProbeV1"
CONTRACT = NATIVE_PROJECT / "hall_world_raster_contract_v4.json"
CONTRACT_SHA = "b18e03e5caa2ac6a8e06d31756fda318cebd0c05c8e7baeb9ec23fc2b404591d"
OUTPUT = ROOT / "Saved/RokUnityMigration/CityHallWorldRasterGpuFixture/v1"
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


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sha_arg(value: str) -> str:
    need(len(value) == 64 and all(ch in "0123456789abcdef" for ch in value), "Expected lowercase SHA256")
    return value


def project_file(value: str) -> Path:
    path = (resolve(value)).resolve()
    need(path.is_relative_to(ROOT.resolve()), f"Path outside project: {value}")
    return path


def probe_file(value: str) -> Path:
    path = (NATIVE_PROJECT / value).resolve()
    need(path.is_relative_to(NATIVE_PROJECT.resolve()), f"Path outside isolated native project: {value}")
    return path


def relative(path: Path) -> str:
    path = path.resolve()
    need(path.is_relative_to(ROOT.resolve()), f"Path outside project: {path}")
    return path.relative_to(ROOT.resolve()).as_posix()


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def pin(path: Path, byte_count: int | None = None) -> dict:
    need(path.is_file(), f"Pinned file missing: {path}")
    result = {"path": relative(path), "sha256": digest(path)}
    if byte_count is not None:
        need(path.stat().st_size == byte_count, f"Pinned file size differs: {path}")
        result["byte_count"] = byte_count
    return result


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def matrix(record: dict, path_field: str = "path", *, probe: bool = False) -> list[float]:
    path = probe_file(record[path_field]) if probe else project_file(record[path_field])
    need(path.stat().st_size == 64 and digest(path) == record["sha256"], f"Matrix pin differs: {path}")
    values = struct.unpack("<16f", path.read_bytes())
    need(all(math.isfinite(v) for v in values), f"Nonfinite matrix: {path}")
    return list(values)


def matmul(a: list[float], b: list[float]) -> list[float]:
    return [f32(sum(a[r * 4 + k] * b[k * 4 + c] for k in range(4)))
            for r in range(4) for c in range(4)]


def transpose(m: list[float]) -> list[float]:
    return [m[c * 4 + r] for r in range(4) for c in range(4)]


def world_position(w: list[float], v: tuple[float, float, float]) -> tuple[float, float, float]:
    return tuple(f32(sum(w[r * 4 + k] * v[k] for k in range(3)) + w[r * 4 + 3]) for r in range(3))


def source_binary(record: dict) -> dict:
    path = project_file(record["path"])
    need(path.parent == SOURCE.parent and path.stat().st_size == record["byte_count"]
         == record["count"] * record["stride_bytes"] and digest(path) == record["sha256"],
         f"Sealed source binary differs: {path}")
    return {key: record[key] for key in ("path", "sha256", "count", "stride_bytes", "byte_count")}


def native_run_file(run: Path, value: str) -> Path:
    path = (run / value).resolve()
    need(path.parent == run, f"Native output must be direct run child: {value}")
    return path


def reference_image(raw_path: Path, png_path: Path, expected_raw: str, expected_png: str) -> None:
    raw = raw_path.read_bytes()
    png = png_path.read_bytes()
    need(len(raw) == 128 * 128 * 4 and digest(raw_path) == expected_raw, "Native top-left RGBA changed")
    need(png.startswith(PNG_MAGIC) and digest(png_path) == expected_png, "Native true PNG changed")
    with Image.open(png_path) as image:
        need(image.format == "PNG" and image.size == (128, 128), "Native PNG format/size differs")
        decoded = image.convert("RGBA").tobytes()
    need(decoded == raw, "Native decoded PNG differs from top-left raw RGBA")
    blues = raw[2::4]
    coverage = sum(blue < 122 for blue in blues)
    need(0 < coverage < 128 * 128, "Native geometry coverage empty/full")


def prepare(native_run: Path, native_sha: str, contract_sha: str) -> dict:
    native_run = native_run.resolve()
    need(native_run.parent == (NATIVE_PROJECT / "OutputRuns").resolve(), "Native run outside isolated project")
    need(digest(SOURCE) == SOURCE_SHA and digest(HALL_NATIVE3) == HALL_NATIVE3_SHA,
         "Sealed source geometry/Hall Native3 changed")
    need(digest(CONTRACT) == contract_sha == CONTRACT_SHA,
         "Sealed and externally pinned raster contract differs")
    native_path = native_run / "native_hall_world_raster.json"
    verified_path = native_run / "verified_world_raster_v4.json"
    need(native_path.is_file() and verified_path.is_file(), "Native oracle and independent verification required")
    need(digest(native_path) == native_sha, "Externally pinned native raster bytes differ")
    source, hall, hall_verify, contract, native, verified = map(
        read, (SOURCE, HALL_NATIVE3, HALL_VERIFY, CONTRACT, native_path, verified_path))
    need(source["schema"] == "rok.city_gamma_source_geometry_inputs.v1" and len(source["rows"]) == 32,
         "Sealed source geometry manifest differs")
    need(hall["schema"] == "rok.unity_city_hall_world_native.v3" and
         hall["contract_sha256"] == HALL_CONTRACT_SHA and hall_verify["passed"] and
         hall_verify["native"]["native_sha256"] == HALL_NATIVE3_SHA and
         hall_verify["native"]["source_composed_matrix_count"] == 38,
         "Accepted Hall Native3 matrix proof differs")
    need(contract["schema"] == "rok.unity_city_hall_world_raster_contract.v4" and
         native["schema"] == "rok.unity_city_hall_world_raster_native.v1" and
         native["contract_sha256"] == contract_sha and native["complete"] and
         native["case_count"] == len(native["cases"]) == len(contract["cases"]) == 2 and
         native["width"] == native["height"] == 128 and not native["target_srgb"] and
         native["anti_aliasing"] == 1 and native["readback_row_order"] == "top_left_rgba8" and
         native["graphics_device"] == "Direct3D11" and native["color_space"] == "Gamma",
         "Native raster run/render target contract differs")
    need(verified["passed"] and verified["contract_sha256"] == contract_sha and
         verified["native"]["native_sha256"] == native_sha and
         verified["native"]["cases_checked"] == verified["native"]["png_raw_exact_cases"] == 2,
         "Independent Unity raster verification differs")
    need(contract["producer_sha256"] == digest(project_file(PIN_PATHS[7])) and
         contract["verifier_sha256"] == digest(project_file(PIN_PATHS[8])) and
         contract["exporter_sha256"] == digest(project_file(PIN_PATHS[9])) and
         contract["shader_sha256"] == digest(project_file(PIN_PATHS[10])) and
         native["exporter_sha256"] == contract["exporter_sha256"] and
         native["game_app_loaded"] is False,
         "Native producer/exporter/shader or isolated scope differs")
    proof = contract["hall_native3"]
    need(proof["native_path"] == relative(HALL_NATIVE3) and proof["sha256"] == HALL_NATIVE3_SHA and
         proof["verified_path"] == relative(HALL_VERIFY) and
         proof["verified_sha256"] == digest(HALL_VERIFY) and
         proof["renderer_file_id"] == "212751466141171401" and
         proof["sprite_guid"] == "57ec7ddf0b743a140b2707fe14b536e2" and
         proof["prefab_guid"] == "c825e8897df8e424e817e4ed05eef53e",
         "Native3 selected Hall SpriteRenderer identity differs")
    geometry = contract["geometry"]
    row = source["rows"][27]
    need(geometry["manifest_path"] == relative(SOURCE) and geometry["sha256"] == SOURCE_SHA and
         geometry["row_index"] == row["index"] == 27 and
         geometry["source_key"] == row["source_key"] == "initial:5" and
         geometry["source_row_sha256"] == row["source_row_sha256"] == SOURCE_ROW_SHA and
         row["positions"]["count"] == 240 and row["positions"]["stride_bytes"] == 12 and
         row["indices"]["binary"]["count"] == 714 and
         row["uv_channels"][0]["binary"]["stride_bytes"] == 16,
         "Hall original row27 geometry identity/stride differs")
    parts = (row["positions"], row["uv_channels"][0]["binary"], row["indices"]["binary"])
    inputs = [{"kind": kind, **source_binary(record)} for kind, record in
              zip(("positions", "uv0", "indices"), parts)]
    for kind, record in zip(("positions", "uv0", "indices"), parts):
        need(geometry[kind]["source_path"] == record["path"] and
             geometry[kind]["sha256"] == record["sha256"] and
             digest(probe_file(geometry[kind]["probe_path"])) == record["sha256"],
             f"Contract {kind} source/probe bytes differ")
    positions_raw = project_file(parts[0]["path"]).read_bytes()
    vertices = list(struct.iter_unpack("<3f", positions_raw))
    indices = list(struct.iter_unpack("<I", project_file(parts[2]["path"]).read_bytes()))
    need(len(vertices) == 240 and len(indices) == 714 and max(x[0] for x in indices) < 240,
         "Hall source indices/vertices differ")
    hall_nodes = {node["phase"]: node for node in hall["nodes"] if
                  node["component_type"] == "SpriteRenderer" and
                  node["source_file_id"] == proof["renderer_file_id"] and
                  node["source_guid"] == proof["prefab_guid"]}
    need(set(hall_nodes) == set(PHASES), "Accepted Hall Native3 two world poses absent")
    fitted = contract["shared_fit"]
    need(fitted["axes"] == [0, 2], "Union projection axes differ")
    shared_pin = fitted["world_to_clip_column"]
    c = matrix(shared_pin, probe=True)
    ranges = []
    for axis in (0, 1, 2):
        values = []
        for phase in PHASES:
            node = hall_nodes[phase]
            path = HALL_NATIVE3.parent / node["matrix_path"]
            w = matrix({"path": relative(path), "sha256": node["matrix_sha256"]})
            values.extend(world_position(w, vertex)[axis] for vertex in vertices)
        ranges.append([min(values), max(values)])
    centers = [f32((ranges[axis][0] + ranges[axis][1]) / 2.0) for axis in (0, 2)]
    spans = [ranges[axis][1] - ranges[axis][0] for axis in (0, 2)]
    need(min(spans) > 0, "Union world projection degenerate")
    scale = f32(1.8 / max(spans))
    expected_c = [0.0] * 16
    expected_c[0] = scale
    expected_c[3] = f32(-f32(centers[0]) * scale)
    expected_c[6] = scale
    expected_c[7] = f32(-f32(centers[1]) * scale)
    expected_c[11] = f32(0.5)
    expected_c[15] = f32(1.0)
    need(all(abs(a - b) <= 2e-6 for a, b in zip(c, expected_c)),
         "Shared union world-to-clip matrix differs from original world positions")
    need(all(abs(a - b) <= 2e-6 for pair, source_pair in zip(fitted["world_ranges"], ranges)
             for a, b in zip(pair, source_pair)) and
         all(abs(a - b) <= 2e-6 for a, b in zip(fitted["centers"], centers)) and
         abs(fitted["uniform_scale"] - scale) <= 2e-6,
         "Shared union bounds/center/scale differ")
    cases = []
    for index, phase in enumerate(PHASES):
        contract_case, observed = contract["cases"][index], native["cases"][index]
        node = hall_nodes[phase]
        need(contract_case["index"] == observed["index"] == index and
             contract_case["phase"] == observed["phase"] == phase and
             contract_case["source_key"] == observed["source_key"] == "initial:5" and
             contract_case["renderer_file_id"] == observed["renderer_file_id"] == proof["renderer_file_id"] and
             contract_case["renderer_prefab_guid"] == proof["prefab_guid"] and
             observed["sprite_guid"] == proof["sprite_guid"] and
             observed["png_decodes_to_raw_scope"] is True and observed["opaque_alpha"] is True,
             "Native Hall raster case order/phase differs")
        world_pin = contract_case["world_matrix"]
        need(world_pin["source_path"] == relative(HALL_NATIVE3.parent / node["matrix_path"]) and
             world_pin["sha256"] == node["matrix_sha256"] and
             digest(probe_file(world_pin["probe_path"])) == node["matrix_sha256"],
             "World matrix differs from accepted Hall Native3 renderer")
        w = matrix(world_pin, "source_path")
        final_pin = contract_case["source_to_clip_row_major"]
        final = matrix(final_pin, probe=True)
        expected_final = transpose(matmul(c, w))
        need(max(abs(a - b) for a, b in zip(final, expected_final)) <= 2e-6,
             "Final row-vector matrix differs from (shared C * source W)^T")
        need(observed["world_matrix_sha256"] == world_pin["sha256"] and
             observed["source_to_clip_sha256"] == final_pin["sha256"] and
             observed["shared_fit_sha256"] == shared_pin["sha256"] and
             observed["vertex_count"] == 240 and observed["index_count"] == 714,
             "Native raster did not consume pinned source geometry/world/final matrices")
        raw = native_run_file(native_run, observed["raw_top_left"]["path"])
        png = native_run_file(native_run, observed["png"]["path"])
        reference_image(raw, png, observed["raw_top_left"]["sha256"], observed["png"]["sha256"])
        cases.append({"index": index, "phase": phase, "source_key": "initial:5",
                      "source_row_sha256": SOURCE_ROW_SHA,
                      "world_matrix": pin(project_file(world_pin["source_path"]), 64),
                      "matrix": pin(probe_file(final_pin["path"]), 64),
                      "native_raw_path": relative(raw), "native_raw_sha256": digest(raw),
                      "native_png_path": relative(png), "native_png_sha256": digest(png),
                      "native_raw_top_left": True, "width": 128, "height": 128})
    need(cases[0]["matrix"]["sha256"] != cases[1]["matrix"]["sha256"],
         "Two world poses collapsed to the same final matrix")
    pins = [pin(project_file(path)) for path in PIN_PATHS]
    fixture = {"schema": "rok.city_hall_world_raster_gpu_fixture.v1",
               "source_manifest_sha256": SOURCE_SHA,
               "source_row_sha256": SOURCE_ROW_SHA,
               "hall_native3_sha256": HALL_NATIVE3_SHA,
               "hall_contract_sha256": HALL_CONTRACT_SHA,
               "hall_native3": pin(HALL_NATIVE3), "hall_verify": pin(HALL_VERIFY),
               "native_contract": pin(CONTRACT), "native_manifest": pin(native_path),
               "native_verify": pin(verified_path),
               "shared_world_to_clip": pin(probe_file(shared_pin["path"]), 64),
               "inputs": inputs, "cases": cases, "pinned_files": pins,
               "scope": "isolated original row27 geometry times measured Native3 world matrices; no GameApp camera, active LOD, source material or main-view acceptance"}
    need(not OUTPUT.exists(), "v1 Hall GPU fixture already exists; preserve it")
    OUTPUT.mkdir(parents=True)
    path = OUTPUT / "manifest.json"
    path.write_text(json.dumps(fixture, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return {"fixture_path": relative(path), "fixture_sha256": digest(path),
            "cases": len(cases), "source_buffers": len(inputs),
            "native_oracle_sha256": native_sha, "contract_sha256": contract_sha,
            "goal_complete": False, "gameapp_mainview_accepted": False}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--native-run", type=Path, required=True)
    parser.add_argument("--native-sha", type=sha_arg, required=True)
    parser.add_argument("--contract-sha", type=sha_arg, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.native_run, args.native_sha, args.contract_sha), ensure_ascii=False))


if __name__ == "__main__":
    main()
