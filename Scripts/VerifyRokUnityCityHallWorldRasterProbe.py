"""Independently verify Hall Native3 world × row27 geometry UV raster inputs/output."""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import math
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityHallWorldRasterProbeV1"
CONTRACT = PROBE / "hall_world_raster_contract_v4.json"
GEOMETRY = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json"
NATIVE3 = ROOT / "Saved/UnityCityHallWorldProbeV1/OutputRuns/20261009_032009_843_25796"
PHASES = ("prefab_default_hall", "runtime_replay_hall")


def need(ok: bool, reason: str) -> None:
    if not ok:
        raise AssertionError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inside(base: Path, name: str) -> Path:
    result = (base / name).resolve()
    need(result.is_relative_to(base.resolve()), f"Path escapes root: {name}")
    return result


def f32(number: float) -> float:
    need(math.isfinite(number), "Nonfinite fit/matrix element")
    return struct.unpack("<f", struct.pack("<f", number))[0]


def unpack(raw: bytes) -> list[list[float]]:
    need(len(raw) == 64, "Expected exactly 16 float32LE matrix elements")
    values = struct.unpack("<16f", raw)
    return [list(values[i * 4:(i + 1) * 4]) for i in range(4)]


def pack(rows: list[list[float]]) -> bytes:
    return struct.pack("<16f", *(f32(x) for row in rows for x in row))


def mul(a: list[list[float]], b: list[list[float]]) -> list[list[float]]:
    return [[f32(sum(a[i][k] * b[k][j] for k in range(4))) for j in range(4)] for i in range(4)]


def transpose(a: list[list[float]]) -> list[list[float]]:
    return [[a[j][i] for j in range(4)] for i in range(4)]


def binary(spec: dict) -> bytes:
    source = inside(ROOT, spec["source_path"])
    copied = inside(PROBE, spec["probe_path"])
    raw = source.read_bytes()
    need(raw == copied.read_bytes() and len(raw) == spec["byte_count"] == spec["count"] * spec["stride_bytes"] and
         hashlib.sha256(raw).hexdigest() == spec["sha256"], "Source typed binary/copy differs")
    return raw


def verify_inputs() -> tuple[dict, list[dict]]:
    c = json.loads(CONTRACT.read_text(encoding="utf-8"))
    need(c["schema"] == "rok.unity_city_hall_world_raster_contract.v4" and
         Path(c["probe_root"]).resolve() == PROBE.resolve() and c["unity_version"] == "2019.4.28f1c1",
         "Probe identity/version differs")
    need(sha(ROOT / "Scripts/PrepareRokUnityCityHallWorldRasterProbe.py") == c["producer_sha256"] and
         sha(Path(__file__)) == c["verifier_sha256"] and
         sha(PROBE / "Assets/Editor/RenderRokUnityCityHallWorldRasterNative.cs") == c["exporter_sha256"],
         "Producer/verifier/exporter changed")
    need(sha(inside(ROOT, c["shader_source_path"])) == c["shader_sha256"] ==
         sha(PROBE / "Assets/ProbeLocalUv.shader"), "Source shader no longer byte-identical")
    need(len(c["settings_files"]) == 21, "Sealed Unity settings/package count differs")
    for item in c["settings_files"]:
        need(sha(inside(ROOT, item["source_path"])) == item["sha256"] ==
             sha(inside(ROOT, item["probe_path"])), "Sealed Unity setting differs")
    g = c["geometry"]
    need(g["manifest_path"] == GEOMETRY.relative_to(ROOT).as_posix() and
         sha(GEOMETRY) == g["sha256"] == "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273",
         "Source geometry manifest differs")
    source_row = json.loads(GEOMETRY.read_text(encoding="utf-8"))["rows"][27]
    need(g["row_index"] == 27 and g["source_key"] == source_row["source_key"] == "initial:5" and
         g["source_row_sha256"] == source_row["source_row_sha256"] and
         source_row["source_json_pointer"] == "/rows/0", "Row27 source identity differs")
    for name, source_spec in (("positions", source_row["positions"]),
                              ("indices", source_row["indices"]["binary"]),
                              ("uv0", next(x["binary"] for x in source_row["uv_channels"] if x["channel"] == 0))):
        need(g[name]["source_path"] == source_spec["path"] and g[name]["sha256"] == source_spec["sha256"],
             f"Source row27 {name} binding differs")
        binary(g[name])
    need((g["positions"]["count"], g["positions"]["stride_bytes"],
          g["uv0"]["count"], g["uv0"]["stride_bytes"],
          g["indices"]["count"], g["indices"]["stride_bytes"]) == (240, 12, 240, 16, 714, 4),
         "Source Hall full UV/index layout differs")
    h = c["hall_native3"]
    need(inside(ROOT, h["native_path"]) == NATIVE3 / "hall_world_native.json" and
         sha(NATIVE3 / "hall_world_native.json") == h["sha256"] ==
         "01b924d764395708e20766834595fa7943f1ec61c70215b6058914c2cc80ab5e" and
         sha(inside(ROOT, h["verified_path"])) == h["verified_sha256"], "Accepted Hall Native3 proof differs")
    native3 = json.loads((NATIVE3 / "hall_world_native.json").read_text(encoding="utf-8-sig"))
    verified3 = json.loads(inside(ROOT, h["verified_path"]).read_text(encoding="utf-8-sig"))
    need(native3["schema"] == "rok.unity_city_hall_world_native.v3" and len(native3["nodes"]) == 38 and
         verified3["passed"] is True and verified3["native"]["native_sha256"] == h["sha256"],
         "Accepted Hall Native3 verification differs")
    need(h["renderer_file_id"] == "212751466141171401" and
         h["prefab_guid"] == "c825e8897df8e424e817e4ed05eef53e" and
         h["sprite_guid"] == "57ec7ddf0b743a140b2707fe14b536e2" and
         sha(inside(ROOT, h["source_prefab_path"])) == h["source_prefab_sha256"],
         "Hall renderer/prefab/sprite identity differs")
    family = json.loads(inside(ROOT, h["source_families_path"]).read_text(encoding="utf-8-sig"))["rows"][0]
    need(sha(inside(ROOT, h["source_families_path"])) == h["source_families_sha256"] and
         family["source_key"] == "initial:5" and family["renderer_file_id_decimal"] == h["renderer_file_id"] and
         family["prefab_guid"] == h["prefab_guid"], "Row27 family source binding differs")
    need(len(c["cases"]) == 2 and len({item["phase"] for item in c["cases"]}) == 2,
         "Exactly two Hall poses required")
    positions = list(struct.iter_unpack("<3f", binary(g["positions"])))
    indices = list(struct.iter_unpack("<I", binary(g["indices"])))
    need(len(positions) == 240 and len(indices) == 714 and all(value[0] < 240 for value in indices),
         "Source triangle index outside Hall mesh")
    all_world = []
    cases = []
    for index, case in enumerate(c["cases"]):
        need(case["index"] == index and case["phase"] == PHASES[index] and
             case["source_key"] == "initial:5" and case["renderer_file_id"] == h["renderer_file_id"] and
             case["renderer_prefab_guid"] == h["prefab_guid"], "Pose source identity differs")
        matches = [node for node in native3["nodes"] if node["phase"] == case["phase"] and
                   node["component_type"] == "SpriteRenderer" and node["path"] == "build_1_101_1/sprite[0]" and
                   node["source_file_id"] == h["renderer_file_id"] and node["source_guid"] == h["prefab_guid"]]
        need(len(matches) == 1, "Native3 exact pose renderer absent")
        source = NATIVE3 / matches[0]["matrix_path"]
        spec = case["world_matrix"]
        need(spec["source_path"] == source.relative_to(ROOT).as_posix() and spec["byte_count"] == 64 and
             spec["count"] == 16 and spec["stride_bytes"] == 4 and
             sha(source) == spec["sha256"] == matches[0]["matrix_sha256"], "Native3 exact world matrix differs")
        world = unpack(binary({**spec, "count": 16, "stride_bytes": 4}))
        transformed = [tuple(f32(sum(world[axis][j] * vertex[j] for j in range(3)) + world[axis][3])
                             for axis in range(3)) for vertex in positions]
        all_world.extend(transformed)
        need(case["world_vertex_ranges"] == [[min(v[axis] for v in transformed), max(v[axis] for v in transformed)]
                                              for axis in range(3)], "Pose world ranges differ")
        cases.append({"spec": case, "world": world})
    ranges = [[min(vertex[axis] for vertex in all_world), max(vertex[axis] for vertex in all_world)]
              for axis in range(3)]
    pairs = [(a, b) for a in range(3) for b in range(a + 1, 3)]
    axes = max(pairs, key=lambda pair: ((ranges[pair[0]][1] - ranges[pair[0]][0]) *
                                        (ranges[pair[1]][1] - ranges[pair[1]][0]), -pair[0], -pair[1]))
    need(axes == (0, 2), "Hall world plane selection differs")
    centers = [f32((ranges[a][0] + ranges[a][1]) * .5) for a in axes]
    scale = f32(1.8 / max(ranges[axes[0]][1] - ranges[axes[0]][0],
                          ranges[axes[1]][1] - ranges[axes[1]][0]))
    fit = c["shared_fit"]
    need(fit["qualification"] == "diagnostic_union_world_AABB_not_GameApp_camera" and
         fit["axes"] == list(axes) and fit["world_ranges"] == ranges and
         all(struct.pack("<f", x) == struct.pack("<f", y) for x, y in zip(fit["centers"], centers)) and
         struct.pack("<f", fit["uniform_scale"]) == struct.pack("<f", scale), "Independent shared fit differs")
    column = [[0.0] * 4 for _ in range(4)]
    column[0][axes[0]], column[1][axes[1]] = scale, scale
    column[0][3], column[1][3] = f32(-centers[0] * scale), f32(-centers[1] * scale)
    column[2][3], column[3][3] = .5, 1.0
    fit_raw = pack(column)
    fit_spec = fit["world_to_clip_column"]
    need(len(fit_raw) == fit_spec["byte_count"] == 64 and fit_spec["count"] == 16 and
         fit_spec["stride_bytes"] == 4 and fit_spec["path"] == fit_spec["probe_path"] and
         fit_raw == inside(PROBE, fit_spec["path"]).read_bytes() and
         hashlib.sha256(fit_raw).hexdigest() == fit_spec["sha256"], "World-to-clip matrix differs")
    for item in cases:
        final = pack(transpose(mul(column, item["world"])))
        spec = item["spec"]["source_to_clip_row_major"]
        need(len(final) == spec["byte_count"] == 64 and spec["count"] == 16 and
             spec["stride_bytes"] == 4 and spec["path"] == spec["probe_path"] and
             final == inside(PROBE, spec["path"]).read_bytes() and
             hashlib.sha256(final).hexdigest() == spec["sha256"], "Independent source-to-clip matrix differs")
    need(c["target"]["width"] == c["target"]["height"] == 128 and
         c["target"]["read_write"] == "Linear" and c["target"]["anti_aliasing"] == 1 and
         c["target"]["filter"] == "Point" and c["target"]["mips"] is False and
         c["target"]["clear_rgba"] == [.07, .15, .71, 1], "Target contract differs")
    return c, cases


def verify_run(c: dict, run: Path) -> dict:
    run = run.resolve()
    need(run.parent == (PROBE / "OutputRuns").resolve(), "Native run outside unique isolated OutputRuns")
    need(sha(run / "hall_world_raster_contract_v4.json") == sha(CONTRACT) and
         sha(run / "RenderRokUnityCityHallWorldRasterNative.cs") == c["exporter_sha256"] and
         sha(run / "ProbeLocalUv.shader") == c["shader_sha256"], "Native run pinned inputs differ")
    native_path = run / "native_hall_world_raster.json"
    n = json.loads(native_path.read_text(encoding="utf-8-sig"))
    need(n["schema"] == "rok.unity_city_hall_world_raster_native.v1" and n["complete"] is True and
         n["contract_sha256"] == sha(CONTRACT) and n["exporter_sha256"] == c["exporter_sha256"] and
         n["shader_sha256"] == c["shader_sha256"] and n["hall_native3_sha256"] == c["hall_native3"]["sha256"],
         "Native run schema/provenance differs")
    need(n["width"] == n["height"] == 128 and n["target_format"] == "ARGB32_Linear" and n["target_srgb"] is False and
         n["anti_aliasing"] == 1 and n["case_count"] == 2 and len(n["cases"]) == 2 and
         n["readback_row_order"] == "top_left_rgba8" and n["graphics_device"] == "Direct3D11" and
         n["color_space"] == "Gamma" and n["game_app_loaded"] is False and n["scope"] == c["scope"],
         "Native GPU/row/scope contract differs")
    rows = []
    raw_images = []
    for index, (case, spec) in enumerate(zip(n["cases"], c["cases"])):
        need(case["index"] == spec["index"] == index and case["phase"] == spec["phase"] == PHASES[index] and
             case["source_key"] == "initial:5" and case["renderer_file_id"] == c["hall_native3"]["renderer_file_id"] and
             case["sprite_guid"] == c["hall_native3"]["sprite_guid"] and case["vertex_count"] == 240 and
             case["index_count"] == 714 and case["world_matrix_sha256"] == spec["world_matrix"]["sha256"] and
             case["source_to_clip_sha256"] == spec["source_to_clip_row_major"]["sha256"] and
             case["shared_fit_sha256"] == c["shared_fit"]["world_to_clip_column"]["sha256"],
             "Native pose geometry/matrix identity differs")
        raw_path = inside(run, case["raw_top_left"]["path"])
        png_path = inside(run, case["png"]["path"])
        raw = raw_path.read_bytes()
        need(len(raw) == 128 * 128 * 4 and sha(raw_path) == case["raw_top_left"]["sha256"] and
             sha(png_path) == case["png"]["sha256"], "Native raw/PNG hash or extent differs")
        with Image.open(io.BytesIO(png_path.read_bytes())) as image:
            need(image.format == "PNG" and image.size == (128, 128), "Native PNG format/size differs")
            decoded = image.convert("RGBA").tobytes()
        need(decoded == raw and case["png_decodes_to_raw_scope"] is True, "Native PNG decoded pixels differ from top-left raw")
        diagnostic_path = inside(run, case["png_decoder_diagnostic"]["path"])
        need(sha(diagnostic_path) == case["png_decoder_diagnostic"]["sha256"],
             "Unity PNG decoder diagnostic hash differs")
        diagnostic = json.loads(diagnostic_path.read_text(encoding="utf-8-sig"))
        direct = case["unity_png_decoder_direct_mismatches"]
        flipped = case["unity_png_decoder_flipped_mismatches"]
        need(isinstance(direct, int) and isinstance(flipped, int) and 0 <= direct <= len(raw) and
             0 <= flipped <= len(raw) and (direct == 0) != (flipped == 0) and
             case["unity_png_decoder_raw_row_order"] == ("top_left" if direct == 0 else "bottom_left") and
             case["unity_png_decoder_texture_format"] == diagnostic["texture_format"] == "ARGB32" and
             case["unity_png_decoder_channel_basis"] == diagnostic["channel_basis"] ==
             "GetPixels32_explicit_rgba" and
             diagnostic["phase"] == spec["phase"] and diagnostic["byte_count"] == len(raw) and
             diagnostic["raw_top_left_sha256"] == case["raw_top_left"]["sha256"] and
             diagnostic["png_sha256"] == case["png"]["sha256"] and
             diagnostic["direct_mismatches"] == direct and diagnostic["flipped_mismatches"] == flipped and
             diagnostic["decoded_raw_row_order"] == case["unity_png_decoder_raw_row_order"],
             "Unity PNG decode row-order self-check differs")
        need(all(raw[i] == 255 for i in range(3, len(raw), 4)) and case["opaque_alpha"] is True,
             "Native diagnostic alpha differs")
        background = (18, 38, 181)
        need(tuple(raw[:3]) == background, "Native diagnostic background differs")
        coverage = sum(tuple(raw[i:i + 3]) != background for i in range(0, len(raw), 4))
        need(coverage == case["coverage_pixels"] and coverage > 100, "Native pose has no credible source raster coverage")
        raw_images.append(raw)
        rows.append({"phase": spec["phase"], "raw_sha256": sha(raw_path), "png_sha256": sha(png_path),
                     "coverage_pixels": coverage})
    need(raw_images[0] != raw_images[1], "Shared fit erased Hall world pose difference")
    difference = sum(raw_images[0][i:i+3] != raw_images[1][i:i+3] for i in range(0, len(raw_images[0]), 4))
    need(difference > 100, "Two source-world Hall poses rasterize almost identically")
    return {"native_path": native_path.relative_to(ROOT).as_posix(), "native_sha256": sha(native_path),
            "cases_checked": 2, "png_raw_exact_cases": 2, "different_rgb_pixels": difference,
            "rows": rows, "scope": c["scope"]}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    c, cases = verify_inputs()
    native = verify_run(c, args.run) if args.run else None
    report = {"schema": "rok.unity_city_hall_world_raster_verify.v1", "passed": True,
              "verifier_sha256": sha(Path(__file__)),
              "contract_sha256": sha(CONTRACT), "hall_native3_sha256": c["hall_native3"]["sha256"],
              "geometry_sha256": c["geometry"]["sha256"], "cases_prepared": len(cases),
              "unity_started": args.run is not None, "native": native,
              "full_migration_accepted": False, "gameapp_camera_accepted": False,
              "material_alpha_accepted": False, "lod_accepted": False, "main_view_accepted": False}
    if args.output:
        need(not args.output.exists(), "Verification report already exists")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
