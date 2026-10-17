"""Independent source-byte/matrix and native GPU local-UV probe verification."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import struct
import subprocess
from pathlib import Path
from rok_data_paths import resolve

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCitySourceGeometryProbeV1"
CONTRACT = PROBE / "geometry_probe_contract.json"
EXPORTER = PROBE / "Assets/Editor/ExportRokUnityCitySourceGeometryProbe.cs"
SHADER = PROBE / "Assets/ProbeLocalUv.shader"
PRODUCER = ROOT / "Scripts/PrepareRokUnityCitySourceGeometryProbe.py"
GEOMETRY = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json"
GEOMETRY_SHA = "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273"
UNITY_DATA = Path("C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data")


def need(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_path(value: str) -> Path:
    path = (resolve(value)).resolve()
    need(path.is_relative_to(ROOT), f"Path escapes repository: {value}")
    return path


def project_path(value: str) -> Path:
    path = (PROBE / value).resolve()
    need(path.is_relative_to(PROBE.resolve()), f"Path escapes probe: {value}")
    return path


def f32(value: float) -> float:
    need(math.isfinite(value), "Nonfinite local matrix value")
    return struct.unpack("<f", struct.pack("<f", value))[0]


def same_f32_bits(left: float, right: float) -> bool:
    return math.isfinite(left) and math.isfinite(right) and struct.pack("<f", left) == struct.pack("<f", right)


def matrix_recipe(position: dict, raw: bytes) -> tuple[bytes, dict]:
    dims = len(position["components"])
    need(dims in (2, 3) and len(raw) == position["count"] * dims * 4,
         "Source position buffer layout differs")
    rows = [struct.unpack_from("<" + "f" * dims, raw, index * dims * 4)
            for index in range(position["count"])]
    bounds = [(min(row[axis] for row in rows), max(row[axis] for row in rows)) for axis in range(dims)]
    need([list(pair) for pair in bounds] == position["component_ranges"],
         "Source float32 position ranges differ")
    axes = [axis for axis, (lo, hi) in enumerate(bounds) if hi - lo > 1e-5]
    need(len(axes) == 2 and all(hi - lo <= 1e-5 for axis, (lo, hi) in enumerate(bounds)
                                if axis not in axes), "Source plane has wrong nonzero spans")
    a, b = axes
    ca = f32((bounds[a][0] + bounds[a][1]) * 0.5)
    cb = f32((bounds[b][0] + bounds[b][1]) * 0.5)
    scale = f32(1.8 / max(bounds[a][1] - bounds[a][0], bounds[b][1] - bounds[b][0]))
    matrix = [[0.0] * 4 for _ in range(4)]
    matrix[a][0] = scale
    matrix[b][1] = scale
    matrix[3] = [f32(-ca * scale), f32(-cb * scale), 0.5, 1.0]
    values = [f32(matrix[row][column]) for row in range(4) for column in range(4)]
    return struct.pack("<16f", *values), {
        "axis_a": a, "axis_b": b, "axis_a_name": position["components"][a],
        "axis_b_name": position["components"][b], "axis_a_center": ca,
        "axis_b_center": cb, "uniform_scale": scale,
        "source_axis_ranges": [list(bounds[a]), list(bounds[b])],
        "remaining_axes_max_span": max([bounds[i][1] - bounds[i][0]
                                        for i in range(dims) if i not in axes] or [0.0]),
        "matrix_values_row_major": values,
    }


def settings_digest(records: list[dict]) -> str:
    return hashlib.sha256(json.dumps(records, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def verify_prepared() -> dict:
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    need(contract["schema"] == "rok.unity_city_source_geometry_probe_contract.v1"
         and contract["repo_root"] == ROOT.as_posix()
         and contract["probe_root"] == PROBE.as_posix()
         and contract["unity_version"] == "2019.4.28f1c1"
         and contract["geometry_manifest_sha256"] == sha(GEOMETRY) == GEOMETRY_SHA,
         "Frozen source geometry/probe identity differs")
    need(contract["producer_sha256"] == sha(PRODUCER)
         and contract["exporter_sha256"] == sha(EXPORTER)
         and contract["shader_sha256"] == sha(SHADER),
         "Prepared source exporter/shader changed")
    need(contract["matrix_layout"].startswith("16 IEEE754 float32LE values, row-major m00..m33")
         and contract["shader_matrix_expression"].startswith("mul(input.vertex, _ProbeLocalToClip)")
         and "top-left raw" in contract["readback_row_contract"],
         "Local matrix/row-order convention differs")
    shader = SHADER.read_text(encoding="utf-8")
    for literal in ("mul(input.vertex, _ProbeLocalToClip)", "float4(input.uv.x, input.uv.y, 0.25, 1.0)",
                    "Cull Off", "ZTest Always", "ZWrite Off", "Blend Off"):
        need(literal in shader, f"Local UV diagnostic shader differs: {literal}")
    geometry = json.loads(GEOMETRY.read_text(encoding="utf-8"))
    need(len(geometry["rows"]) == len(contract["cases"]) == 32
         and len(contract["binary_files"]) == 97
         and len(contract["matrix_files"]) == 32,
         "32 cases/97 source buffers/32 matrices required")
    for folder, records, digest, count in (
        ("ProjectSettings", contract["settings_files"], contract["settings_digest_sha256"], 19),
        ("Packages", contract["packages_files"], contract["packages_digest_sha256"], 2),
    ):
        need(len(records) == count and settings_digest(records) == digest,
             f"Sealed {folder} count/digest differs")
        for record in records:
            source = source_path(record["source_path"])
            output = source_path(record["probe_path"])
            need(output.is_relative_to((PROBE / folder).resolve())
                 and sha(source) == sha(output) == record["source_sha256"],
                 f"Sealed {folder} file differs: {source}")
    source_bindings = {}
    for record in contract["binary_files"]:
        source = source_path(record["source_path"])
        copied = source_path(record["probe_path"])
        need(source.parent == GEOMETRY.parent.resolve()
             and copied.parent == (PROBE / "ProbeInputs/Geometry").resolve()
             and sha(source) == sha(copied) == record["source_sha256"]
             and source.stat().st_size == copied.stat().st_size == record["byte_count"]
             == record["count"] * record["stride_bytes"],
             f"Source/copy typed geometry differs: {record['source_path']}")
        source_bindings[source.name] = record
    need(len(source_bindings) == 97, "Source binary names duplicated")
    source_color_count = 0
    matrix_paths = set()
    for case, row in zip(contract["cases"], geometry["rows"]):
        index = row["index"]
        need(case["index"] == index and case["source_key"] == row["source_key"]
             and case["source_group"] == row["source_group"]
             and case["source_row_sha256"] == row["source_row_sha256"],
             f"Case/source row identity differs: {index}")
        source_binding = {key: row[key] for key in ("flips", "root_trs", "renderer_local_trs",
                                                      "prefab_renderer_local_to_world", "source_queue",
                                                      "source_material", "source_shader",
                                                      "source_material_state", "texture_slots")}
        binding_sha = hashlib.sha256(json.dumps(source_binding, sort_keys=True, separators=(",", ":"),
                                                ensure_ascii=False).encode("utf-8")).hexdigest()
        need(case["source_binding"] == source_binding and case["source_binding_sha256"] == binding_sha,
             f"Source binding provenance differs: {index}")
        uv0 = next(channel["binary"] for channel in row["uv_channels"] if channel["channel"] == 0)
        expected = {"positions": row["positions"], "uv0": uv0,
                    "indices": row["indices"]["binary"], "colors": row["colors"]["binary"]}
        need(case["has_source_colors"] is (expected["colors"] is not None),
             f"Source Color32 presence bit differs: {index}")
        for role, source in expected.items():
            actual = case[role]
            if source is None:
                need(actual is None, f"Missing source colors synthesized: {index}")
                continue
            need(actual["path"] == "ProbeInputs/Geometry/" + Path(source["path"]).name
                 and actual["sha256"] == source["sha256"]
                 and all(actual[field] == source[field] for field in ("count", "stride_bytes", "byte_count")),
                 f"Case source buffer binding differs: {index}/{role}")
            if role == "colors":
                source_color_count += 1
        need(case["position_components"] == len(row["positions"]["components"])
             and case["uv_components"] == len(uv0["components"])
             and row["positions"]["count"] == uv0["count"],
             f"Case source vertex/UV layout differs: {index}")
        matrix, recipe = matrix_recipe(row["positions"], source_path(row["positions"]["path"]).read_bytes())
        need(case["matrix_recipe"] == recipe
             and all(case[key] == recipe[key] for key in ("axis_a", "axis_b", "axis_a_name", "axis_b_name",
                                                       "axis_a_center", "axis_b_center", "uniform_scale")),
             f"Source-local plane/matrix recipe differs: {index}")
        matrix_file = project_path(case["matrix"]["path"])
        need(matrix_file.parent == (PROBE / "ProbeInputs/Matrices").resolve()
             and matrix_file.read_bytes() == matrix
             and case["matrix"]["sha256"] == sha(matrix_file)
             and case["matrix"]["count"] == 16
             and case["matrix"]["stride_bytes"] == 4
             and case["matrix"]["byte_count"] == 64,
             f"64B matrix bits differ: {index}")
        matrix_paths.add(matrix_file.name)
    need(source_color_count == 1 and len(matrix_paths) == 32,
         "Exactly one source Color32 array and 32 unique matrices required")
    matrix_declared = {Path(record["probe_path"]).name: record for record in contract["matrix_files"]}
    need(len(matrix_declared) == 32 and set(matrix_declared) == matrix_paths,
         "Matrix provenance file list differs")
    for record in matrix_declared.values():
        matrix_file = source_path(record["probe_path"])
        need(sha(matrix_file) == record["sha256"] and matrix_file.stat().st_size == 64,
             "Matrix source file hash/size differs")
    return contract


def compile_exporter() -> str:
    compiler = UNITY_DATA / "Tools/Roslyn/csc.exe"
    need(compiler.is_file(), "Unity 2019 Roslyn compiler absent")
    refs = list((UNITY_DATA / "Managed/UnityEngine").glob("*.dll"))
    refs += list((UNITY_DATA / "MonoBleedingEdge/lib/mono/4.7.1-api").glob("*.dll"))
    refs += list((UNITY_DATA / "MonoBleedingEdge/lib/mono/4.7.1-api/Facades").glob("*.dll"))
    refs.append(UNITY_DATA / "Managed/UnityEditor.dll")
    need(all(ref.is_file() for ref in refs), "Unity 2019 static compiler reference absent")
    output = PROBE / "Validation/ProbeSyntax.dll"
    output.parent.mkdir(parents=True, exist_ok=True)
    args = ["/nologo", "/target:library", "/deterministic+", "/define:UNITY_EDITOR",
            "/out:" + str(output)]
    args += ["/reference:" + str(ref) for ref in refs]
    args.append(str(EXPORTER))
    response = PROBE / "Validation/compiler.rsp"
    response.write_text("\n".join('"' + arg + '"' for arg in args), encoding="utf-8")
    process = subprocess.run([str(compiler), "@" + str(response)], capture_output=True, check=False)
    (PROBE / "Validation/compiler.log").write_bytes(process.stdout + process.stderr)
    need(process.returncode == 0, "Unity 2019 geometry exporter C# compile failed; see Validation/compiler.log")
    return sha(output)


def verify_run(run: Path, contract: dict) -> dict:
    run = run.resolve()
    need(run.parent == (PROBE / "OutputRuns").resolve() and run.is_dir(),
         "Native run outside isolated OutputRuns")
    need(sha(run / "geometry_probe_contract.json") == sha(CONTRACT)
         and sha(run / "ExportRokUnityCitySourceGeometryProbe.cs") == sha(EXPORTER)
         and sha(run / "ProbeLocalUv.shader") == sha(SHADER),
         "Run copied source contract/exporter/shader differs")
    native = json.loads((run / "native_geometry.json").read_text(encoding="utf-8-sig"))
    need(native["schema"] == "rok.unity_city_source_geometry_native.v1"
         and native["complete"] and not native["game_app_loaded"]
         and native["run_directory"] == run.as_posix()
         and native["contract_sha256"] == sha(CONTRACT)
         and native["exporter_sha256"] == sha(EXPORTER)
         and native["shader_sha256"] == sha(SHADER)
         and native["unity_version"] == "2019.4.28f1c1"
         and len(native["cases"]) == 32,
         "Native source geometry run identity/integrity differs")
    need(native["target_width"] == native["target_height"] == 128
         and native["target_anti_aliasing"] == 1 and not native["target_srgb"]
         and native["target_format"] == "ARGB32"
         and native["target_read_write"] == "Linear"
         and native["matrix_layout"] == contract["matrix_layout"]
         and native["vertex_shader_expression"] == "mul(input.vertex, _ProbeLocalToClip)"
         and native["clear_color"] == "0.07,0.15,0.71,1"
         and "top-left raw" in native["readback_row_order"]
         and "top-left" in native["png_row_order"],
         "Native target/matrix/readback contract differs")
    need(native["graphics_device"] == "Direct3D11" and native["color_space"] == "Gamma",
         "Native D3D11/Gamma diagnostic context differs")
    coverage = []
    for case, result in zip(contract["cases"], native["cases"]):
        need(result["index"] == case["index"] and result["source_key"] == case["source_key"]
             and result["source_group"] == case["source_group"]
             and result["source_row_sha256"] == case["source_row_sha256"]
             and result["source_binding_sha256"] == case["source_binding_sha256"]
             and result["vertex_count"] == case["positions"]["count"]
             and result["index_count"] == case["indices"]["count"]
             and result["has_source_colors"] is case["has_source_colors"]
             and result["color_count"] == (0 if not case["has_source_colors"] else case["colors"]["count"])
             and result["axis_a"] == case["axis_a"] and result["axis_b"] == case["axis_b"]
             and same_f32_bits(result["axis_a_center"], case["axis_a_center"])
             and same_f32_bits(result["axis_b_center"], case["axis_b_center"])
             and same_f32_bits(result["uniform_scale"], case["uniform_scale"])
             and result["positions_sha256"] == case["positions"]["sha256"]
             and result["uv0_sha256"] == case["uv0"]["sha256"]
             and result["indices_sha256"] == case["indices"]["sha256"]
             and result["colors_sha256"] == ("source_array_absent" if not case["has_source_colors"] else case["colors"]["sha256"])
             and result["matrix_sha256"] == case["matrix"]["sha256"]
             and result["width"] == result["height"] == 128,
             f"Native source case data differs: {case['index']}")
        raw_path = (run / result["raw_top_left"]).resolve()
        png_path = (run / result["png"]).resolve()
        need(raw_path.parent == run and png_path.parent == run
             and raw_path.suffix == ".bin" and png_path.suffix == ".png"
             and result["raw_top_left"].startswith(f"{case['index']:02d}_")
             and result["png"].startswith(f"{case['index']:02d}_"),
             f"Native output path differs: {case['index']}")
        raw = raw_path.read_bytes()
        need(len(raw) == 128 * 128 * 4 and sha(raw_path) == result["raw_top_left_sha256"]
             and sha(png_path) == result["png_sha256"]
             and png_path.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n",
             f"Raw/PNG bytes/hash/magic differ: {case['index']}")
        with Image.open(png_path) as png:
            need(png.format == "PNG" and png.mode == "RGBA" and png.size == (128, 128)
                 and png.tobytes() == raw, f"PNG decoded top-left pixels differ from raw: {case['index']}")
        row_stride = 128 * 4
        bottom = b"".join(raw[y * row_stride:(y + 1) * row_stride]
                          for y in range(127, -1, -1))
        need(hashlib.sha256(bottom).hexdigest() == result["raw_bottom_left_sha256"]
             and result["png_top_left_rgba"] == list(raw[:4])
             and result["png_bottom_left_rgba"] == list(raw[-row_stride:-row_stride + 4]),
             f"Native explicit row reversal differs: {case['index']}")
        background = bytes((18, 38, 181, 255))
        corners = (raw[:4], raw[(127 * 4):(128 * 4)], raw[-row_stride:-row_stride + 4], raw[-4:])
        need(all(corner == background for corner in corners),
             f"Linear clear RGB/alpha or expected corner margin differs: {case['index']}")
        pixels = [raw[offset:offset + 4] for offset in range(0, len(raw), 4)]
        need(all(pixel[3] == 255 for pixel in pixels),
             f"Opaque background/fragment alpha differs: {case['index']}")
        foreground = [pixel for pixel in pixels if pixel[:3] != background[:3]]
        covered = len(foreground)
        need(0 < covered < 128 * 128
             and all(abs(pixel[2] - 64) <= 1 for pixel in foreground)
             and len({pixel[0] for pixel in foreground}) > 1
             and len({pixel[1] for pixel in foreground}) > 1
             and result["matrix_set_mapping"].startswith("16 source f32LE"),
             f"Source UV RGB geometry appears empty/wrong or matrix mapping differs: {case['index']}")
        coverage.append({"source_key": case["source_key"], "non_background_pixels": covered})
    result = {"schema": "rok.unity_city_source_geometry_native_verify.v1", "passed": True,
              "native_sha256": sha(run / "native_geometry.json"), "contract_sha256": sha(CONTRACT),
              "exporter_sha256": sha(EXPORTER), "shader_sha256": sha(SHADER),
              "verifier_sha256": sha(Path(__file__)), "cases_checked": 32,
              "source_typed_binary_inputs_checked": 97, "matrix_64_byte_inputs_checked": 32,
              "top_left_png_raw_exact_cases": 32, "expected_clear_rgba8": [18, 38, 181, 255],
              "opaque_alpha_cases": 32, "nonempty_uv_rgb_coverage": coverage,
              "qualification": "source-local position/index/UV GPU diagnostic only; no source material/mips, GameApp world/LOD/mainview or alpha proof"}
    output = run / "verified_geometry.json"
    encoded = (json.dumps(result, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if output.exists():
        need(output.read_bytes() == encoded, "Existing native verification differs")
    else:
        with os.fdopen(os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as stream:
            stream.write(encoded)
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prepared", action="store_true")
    parser.add_argument("--run", type=Path)
    args = parser.parse_args()
    need(args.prepared != bool(args.run), "Specify exactly one of --prepared or --run")
    contract = verify_prepared()
    if args.prepared:
        dll_sha = compile_exporter()
        result = {"schema": "rok.unity_city_source_geometry_prepare_verify.v1", "passed": True,
                  "contract_sha256": sha(CONTRACT), "producer_sha256": sha(PRODUCER),
                  "exporter_sha256": sha(EXPORTER), "shader_sha256": sha(SHADER),
                  "verifier_sha256": sha(Path(__file__)), "static_dll_sha256": dll_sha,
                  "cases_checked": 32, "source_binary_files_checked": 97,
                  "matrix_files_checked": 32, "project_settings_checked": 19,
                  "packages_checked": 2, "unity_started": False}
        output = PROBE / "prepared_verify_revision3.json"
        encoded = (json.dumps(result, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
        if output.exists():
            need(output.read_bytes() == encoded, "Existing prepared verification differs")
        else:
            with os.fdopen(os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as stream:
                stream.write(encoded)
    else:
        result = verify_run(args.run, contract)
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
