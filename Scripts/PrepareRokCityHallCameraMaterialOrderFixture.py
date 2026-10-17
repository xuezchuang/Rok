"""Prepare immutable Hall material v5 inputs for Unity's object-then-VP VS order.

The source Unity VP constant was not exported. Its value here is explicitly a
source-derived f32 P_rt*V approximation, not an observed unity_MatrixVP byte dump.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import struct
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v5"
BASE = ROOT / "Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v3/bc/manifest.json"
UNITY_INCLUDE = Path("C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data/CGIncludes/UnityShaderUtilities.cginc")
WORLD = ROOT / "Saved/UnityCityHallWorldProbeV1/OutputRuns/20261009_032009_843_25796/matrices/023.f32le"
VIEW = ROOT / "Saved/UnityCityHallCameraLodProbeV1/OutputRuns/20261009_045949_616_43000/matrices/camera_world_to_camera.f32le"
PROJECTION = ROOT / "Saved/UnityCityHallCameraLodProbeV1/OutputRuns/20261009_045949_616_43000/matrices/camera_gpu_projection_rt.f32le"
SOURCE_HASHES = {
    WORLD: "a53abcdc0613cc25b8b16761301e163ccc15e211b5ad889d753a83fb9d43571b",
    VIEW: "81a53de6bff64236ac3b167a0d76edb5be09cd453fd462c2f67bba80a60f9dba",
    PROJECTION: "0c7014135f52baadbb60f81a124392fd7179fc0cd5fe9720c0c5e145e2b3cd3b",
}
CODE = (
    "Source/Rok/RokCityHallCameraMaterialAuditSubsystem.h",
    "Source/Rok/RokCityHallCameraMaterialAuditSubsystem.cpp",
    "Source/RokRendering/Public/RokCityHallCameraMaterialProbe.h",
    "Source/RokRendering/Private/RokCityHallCameraMaterialProbe.cpp",
    "Shaders/Private/RokCityHallCameraMaterial.usf",
    "Scripts/PrepareRokCityHallCameraMaterialOrderFixture.py",
    "Scripts/VerifyRokCityHallCameraMaterialOrderRuntime.py",
    "Scripts/PrepareRokCityHallCameraMaterialBcFixture.py",
    "Scripts/VerifyRokCityHallCameraMaterialBcRuntime.py",
)
VARIANTS = ("expanded_rgba", "source_bc", "source_bc_object_vp")


def need(value: bool, reason: str) -> None:
    if not value:
        raise AssertionError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rel(path: Path) -> str:
    resolved = path.resolve()
    need(resolved.is_relative_to(ROOT.resolve()), f"Path outside workspace: {path}")
    return resolved.relative_to(ROOT.resolve()).as_posix()


def pin(path: Path) -> dict:
    need(path.is_file(), f"Missing source: {path}")
    return {"path": rel(path), "sha256": sha(path), "byte_count": path.stat().st_size}


def once(path: Path, payload: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with os.fdopen(os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644), "wb") as stream:
        stream.write(payload)


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def multiply(a: list[float], b: list[float]) -> list[float]:
    return [f32(sum(a[4*r+k] * b[4*k+c] for k in range(4)))
            for r in range(4) for c in range(4)]


def transpose(a: list[float]) -> list[float]:
    return [a[4*c+r] for r in range(4) for c in range(4)]


def matrix(path: Path) -> list[float]:
    need(sha(path) == SOURCE_HASHES[path] and path.stat().st_size == 64,
         f"Sealed matrix differs: {path}")
    result = list(struct.unpack("<16f", path.read_bytes()))
    need(all(math.isfinite(value) for value in result), "Nonfinite source matrix")
    return result


def prepare() -> dict:
    need(not OUT.exists(), "v5 fixture already exists; never overwrite a prepared or run input")
    base = json.loads(BASE.read_text(encoding="utf-8"))
    need(base["schema"] == "rok.city_hall_camera_material_gpu_fixture.v3" and
         base["variant"] == "source_bc" and base["source_key"] == "initial:5" and
         len(base["inputs"]) == 5 and len(base["cases"]) == 4,
         "Sealed v3 source scope differs")
    include = UNITY_INCLUDE.read_bytes()
    need(b"return mul(UNITY_MATRIX_VP, mul(unity_ObjectToWorld, float4(pos, 1.0)));" in include,
         "Unity 2019 UnityObjectToClipPos source order differs")
    w, v, p = (matrix(path) for path in (WORLD, VIEW, PROJECTION))
    world_row = struct.pack("<16f", *transpose(w))
    vp_row = struct.pack("<16f", *transpose(multiply(p, v)))
    code = [pin(resolve(name)) for name in CODE]
    source_copy = OUT / "UnityShaderUtilities_2019_4_28f1c1.cginc"
    once(source_copy, include)
    world_path = OUT / "unity_object_to_world_row_f32le.bin"
    vp_path = OUT / "source_derived_gpu_rt_view_projection_row_f32le.bin"
    once(world_path, world_row)
    once(vp_path, vp_row)
    results = {}
    for variant in VARIANTS:
        fixture = json.loads(json.dumps(base))
        fixture.update(schema="rok.city_hall_camera_material_gpu_fixture.v5", variant=variant,
                       code_pins=code, unity_object_to_clip_source=pin(source_copy),
                       source_order_qualification="source-confirmed two GPU muls; VP bytes derived from sealed P_rt*V, not measured unity_MatrixVP",
                       unity_vp_bytes_measured=False,
                       world_matrix_row=pin(world_path), view_projection_matrix_row=pin(vp_path))
        if variant == "expanded_rgba":
            old = json.loads((ROOT / "Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v3/expanded/manifest.json").read_text(encoding="utf-8"))
            fixture["inputs"] = old["inputs"]
            fixture["texture_upload"] = old["texture_upload"] if "texture_upload" in old else "Expanded RGBA8 original v1 input"
        if variant == "source_bc_object_vp":
            fixture["matrix_formula"] = "row-vector local * transpose(renderer_world), then world * transpose(camera_gpu_projection_rt * camera_world_to_camera); explicit clipY adapter"
        destination = OUT / ("order" if variant == "source_bc_object_vp" else
                             "bc" if variant == "source_bc" else "expanded") / "manifest.json"
        once(destination, (json.dumps(fixture, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
        results[variant] = pin(destination)
    return {"schema": "rok.city_hall_camera_material_order_prepare.v2", "ready": True,
            "base_v3_sha256": sha(BASE), "variants": results,
            "source_order_verified_in_installed_unity_include": True,
            "unity_vp_bytes_measured": False, "engine_started": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.parse_args()
    print(json.dumps(prepare(), indent=2))
