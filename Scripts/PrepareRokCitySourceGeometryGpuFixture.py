"""Join sealed 32-row geometry inputs with a verified isolated Unity UV probe.

This prepares evidence only. It never launches Unity/UE or changes source inputs.
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
PROBE = ROOT / "Saved/UnityCitySourceGeometryProbeV1"
CONTRACT = PROBE / "geometry_probe_contract.json"
OUTPUT_ROOT = ROOT / "Saved/RokUnityMigration/CitySourceGeometryGpuFixture"
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"


def need(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rel(path: Path) -> str:
    path = path.resolve()
    need(path.is_relative_to(ROOT.resolve()), f"Path escapes repository: {path}")
    return path.relative_to(ROOT.resolve()).as_posix()


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def sealed_binary(record: dict) -> dict:
    path = resolve(record["path"])
    need(path.resolve().parent == SOURCE.parent.resolve(), "Input outside sealed geometry directory")
    need(path.stat().st_size == record["byte_count"] == record["count"] * record["stride_bytes"]
         and sha(path) == record["sha256"], f"Sealed binary changed: {path}")
    return {key: record[key] for key in ("path", "sha256", "count", "stride_bytes", "byte_count")}


def pin(path: Path) -> dict:
    need(path.is_file(), f"Pinned file missing: {path}")
    return {"path": rel(path), "sha256": sha(path)}


def prepare(native_run: Path, revision: str = "v5") -> dict:
    need(revision == "v5", "This producer only writes v5; v1/v2/v3/v4 are sealed")
    native_run = native_run.resolve()
    need(native_run.parent == (PROBE / "OutputRuns").resolve(), "Native run outside isolated OutputRuns")
    native_path = native_run / "native_geometry.json"
    verify_path = native_run / "verified_geometry.json"
    need(native_path.is_file() and verify_path.is_file(), "Native run and independent verification required")
    need(sha(SOURCE) == SOURCE_SHA, "Sealed source geometry manifest changed")
    source, contract, native, verified = map(read, (SOURCE, CONTRACT, native_path, verify_path))
    need(source["schema"] == "rok.city_gamma_source_geometry_inputs.v1"
         and len(source["rows"]) == 32 and len(contract["cases"]) == 32
         and native["schema"] == "rok.unity_city_source_geometry_native.v1"
         and len(native["cases"]) == 32 and native["complete"]
         and not native["game_app_loaded"]
         and native["contract_sha256"] == sha(CONTRACT)
         and verified["passed"] and verified["native_sha256"] == sha(native_path),
         "Native 32-case source-local run/verifier identity differs")
    need(native["target_width"] == native["target_height"] == 128
         and native["target_anti_aliasing"] == 1 and not native["target_srgb"]
         and native["target_read_write"] == "Linear"
         and native["vertex_shader_expression"] == "mul(input.vertex, _ProbeLocalToClip)"
         and "top-left raw" in native["readback_row_order"], "Native target/matrix/row contract differs")
    rows: list[dict] = []
    unique_inputs: set[str] = set()
    unique_matrices: set[str] = set()
    source_color_count = 0
    for index, (src, contract_case, observed) in enumerate(zip(source["rows"], contract["cases"], native["cases"])):
        key = src["source_key"]
        need(src["index"] == contract_case["index"] == observed["index"] == index
             and key == contract_case["source_key"] == observed["source_key"]
             and src["source_group"] == contract_case["source_group"] == observed["source_group"]
             and src["source_row_sha256"] == contract_case["source_row_sha256"] == observed["source_row_sha256"],
             f"Source/native case identity differs: {index}")
        uv0 = next(ch["binary"] for ch in src["uv_channels"] if ch["channel"] == 0)
        binaries = [("positions", src["positions"]), ("uv0", uv0),
                    ("indices", src["indices"]["binary"])]
        if src["colors"]["binary"] is not None:
            need(index == 29, "Unexpected actual source Color32 array")
            binaries.append(("colors", src["colors"]["binary"]))
            source_color_count += 1
        inputs = []
        for kind, record in binaries:
            normalized = sealed_binary(record)
            need(contract_case[kind]["sha256"] == normalized["sha256"]
                 and observed[f"{kind}_sha256"] == normalized["sha256"],
                 f"Native source buffer differs: {index}/{kind}")
            need(normalized["path"] not in unique_inputs, "Duplicate sealed source binary")
            unique_inputs.add(normalized["path"])
            inputs.append({"kind": kind, **normalized})
        indices = (resolve(src["indices"]["binary"]["path"])).read_bytes()
        need(len(indices) % 12 == 0 and all(v < src["positions"]["count"]
             for (v,) in struct.iter_unpack("<I", indices)), f"Source index out of range: {index}")
        matrix_record = contract_case["matrix"]
        matrix_path = (PROBE / matrix_record["path"]).resolve()
        need(matrix_path.parent == (PROBE / "ProbeInputs/Matrices").resolve()
             and matrix_path.stat().st_size == 64 and sha(matrix_path) == matrix_record["sha256"]
             and observed["matrix_sha256"] == matrix_record["sha256"],
             f"Native/contract float32 matrix differs: {index}")
        matrix_bytes = matrix_path.read_bytes()
        need(all(math.isfinite(v) for v in struct.unpack("<16f", matrix_bytes)),
             f"Nonfinite matrix: {index}")
        need(rel(matrix_path) not in unique_matrices, "Duplicate matrix path")
        unique_matrices.add(rel(matrix_path))
        raw_path = (native_run / observed["raw_top_left"]).resolve()
        png_path = (native_run / observed["png"]).resolve()
        need(raw_path.parent == png_path.parent == native_run
             and observed["width"] == observed["height"] == 128
             and len(raw_path.read_bytes()) == 128 * 128 * 4
             and sha(raw_path) == observed["raw_top_left_sha256"]
             and png_path.read_bytes().startswith(PNG_MAGIC)
             and sha(png_path) == observed["png_sha256"],
             f"Native GPU frame files differ: {index}")
        with Image.open(png_path) as image:
            need(image.size == (128, 128) and image.convert("RGBA").tobytes() == raw_path.read_bytes(),
                 f"Native PNG/raw top-left pixels differ: {index}")
        rows.append({
            "index": index, "source_key": key, "source_group": src["source_group"],
            "source_row_sha256": src["source_row_sha256"], "inputs": inputs,
            "matrix": {"path": rel(matrix_path), "sha256": sha(matrix_path),
                       "count": 16, "stride_bytes": 4, "byte_count": 64},
            "native_raw_path": rel(raw_path), "native_raw_sha256": sha(raw_path),
            "native_png_path": rel(png_path), "native_png_sha256": sha(png_path),
            "native_raw_top_left": True, "width": 128, "height": 128,
            "axis_a": contract_case["axis_a"], "axis_b": contract_case["axis_b"],
            "source_binding_sha256": contract_case["source_binding_sha256"],
        })
    need(len(rows) == 32 and len(unique_inputs) == 97 and len(unique_matrices) == 32
         and source_color_count == 1, "32/97/32/1 source closure incomplete")
    pinned = [
        ROOT / "Scripts/PrepareRokCitySourceGeometryGpuFixture.py",
        ROOT / "Scripts/VerifyRokCitySourceGeometryGpuRuntime.py",
        ROOT / "Source/Rok/RokCitySourceGeometryAuditSubsystem.h",
        ROOT / "Source/Rok/RokCitySourceGeometryAuditSubsystem.cpp",
        ROOT / "Source/RokRendering/Public/RokCitySourceGeometryProbe.h",
        ROOT / "Source/RokRendering/Private/RokCitySourceGeometryProbe.cpp",
        ROOT / "Shaders/Private/RokCitySourceGeometryProbe.usf",
        ROOT / "Scripts/PrepareRokUnityCitySourceGeometryProbe.py",
        ROOT / "Scripts/VerifyRokUnityCitySourceGeometryProbe.py",
        PROBE / "Assets/Editor/ExportRokUnityCitySourceGeometryProbe.cs",
        PROBE / "Assets/ProbeLocalUv.shader",
        ROOT / "Scripts/PrepareRokCityGammaSourceGeometryInputs.py",
        ROOT / "Scripts/VerifyRokCityGammaSourceGeometryInputs.py",
    ]
    return {
        "schema": "rok.city_source_geometry_gpu_fixture.v1",
        "fixture_revision": revision,
        "scope": "isolated source-local indexed vertex-pulling UV0 diagnostic; no source material/alpha, main view, GameApp transform or input assembler acceptance",
        "source_manifest_path": rel(SOURCE), "source_manifest_sha256": SOURCE_SHA,
        "native_contract_path": rel(CONTRACT), "native_contract_sha256": sha(CONTRACT),
        "native_manifest_path": rel(native_path), "native_manifest_sha256": sha(native_path),
        "native_verify_path": rel(verify_path), "native_verify_sha256": sha(verify_path),
        "matrix_layout": "16 IEEE754 float32LE values in row-major m00..m33; mul(vertex,matrix)",
        "native_raw_format": "RGBA8 top-left", "ue_gpu_format": "BGRA8 top-left",
        "target": {"width": 128, "height": 128, "srgb": False, "samples": 1,
                   "clear_rgba": [0.07, 0.15, 0.71, 1.0], "shader_rgb": "UV.x,UV.y,0.25"},
        "expected_source_buffers": 97, "expected_cases": 32,
        "pinned_files": [pin(path) for path in pinned], "cases": rows,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--native-run", required=True, type=Path)
    parser.add_argument("--revision", required=True, choices=("v5",))
    args = parser.parse_args()
    fixture = prepare(args.native_run, args.revision)
    encoded = (json.dumps(fixture, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    output = OUTPUT_ROOT / args.revision / "manifest.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        need(output.read_bytes() == encoded, "Frozen fixture differs; choose a revision instead of overwrite")
    else:
        output.write_bytes(encoded)
    print(json.dumps({"fixture": rel(output), "sha256": sha(output),
                      "cases": 32, "source_buffers": 97, "unity_launched": False}, ensure_ascii=False))


if __name__ == "__main__":
    main()
