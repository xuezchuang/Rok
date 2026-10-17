"""Produce new isolated expanded/BC Hall RHI fixtures; never modify sealed v1."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
OLD = ROOT / "Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v1/manifest.json"
OUT = ROOT / "Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v3"
V1_NATIVE = "3e2d70c5a9c2a845f5e34b8e9e80fe95d2218142b9697a9d63236beb4d6ef451"
V2_RUN = ROOT / "Saved/UnityCityHallCameraMaterialProbeV2/OutputRuns/20261009_061201_264_46812"
V2_NATIVE = V2_RUN / "hall_camera_material_native_v2.json"
V2_NATIVE_SHA = "d70a6dfb256f5a3a95f7c81ddf2def925b7ab46ea9d187b53a9a0946dcf94f9d"
V2_VERIFIED = V2_RUN / "verified_camera_material_v2.json"
V2_CONTRACT = ROOT / "Saved/UnityCityHallCameraMaterialProbeV2/hall_camera_material_contract_v2.json"
V2_CONTRACT_SHA = "ac20566eb179f620cb3932baa9a095cb6961ac269ce6d7755c6aa737f4e509e3"
VERIFY = ROOT / "Scripts/VerifyRokCityHallCameraMaterialBcRuntime.py"
CODE = (
    "Source/Rok/RokCityHallCameraMaterialAuditSubsystem.h",
    "Source/Rok/RokCityHallCameraMaterialAuditSubsystem.cpp",
    "Source/RokRendering/Public/RokCityHallCameraMaterialProbe.h",
    "Source/RokRendering/Private/RokCityHallCameraMaterialProbe.cpp",
    "Shaders/Private/RokCityHallCameraMaterial.usf",
    "Scripts/PrepareRokCityHallCameraMaterialBcFixture.py",
    "Scripts/VerifyRokCityHallCameraMaterialBcRuntime.py",
)


def need(ok: bool, reason: str) -> None:
    if not ok:
        raise AssertionError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rel(path: Path) -> str:
    path = path.resolve()
    need(path.is_relative_to(ROOT.resolve()), f"Outside workspace: {path}")
    return path.relative_to(ROOT.resolve()).as_posix()


def pin(path: Path) -> dict:
    need(path.is_file(), f"Pinned source absent: {path}")
    return {"path": rel(path), "sha256": sha(path), "byte_count": path.stat().st_size}


def write_once(path: Path, data: bytes, refresh: bool) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        need(refresh and path.parent.parent == OUT and
             json.loads(path.read_text(encoding="utf-8"))["schema"] ==
             "rok.city_hall_camera_material_gpu_fixture.v3",
             f"Fixture already exists outside explicit pre-runtime owned refresh: {path}")
        need(path.read_bytes() != data, "Owned fixture refresh has no byte change")
        path.write_bytes(data)
        return
    with os.fdopen(os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644), "wb") as stream:
        stream.write(data)


def prepare(refresh: bool = False) -> dict:
    need(OLD.is_file() and sha(V2_NATIVE) == V2_NATIVE_SHA and sha(V2_CONTRACT) == V2_CONTRACT_SHA,
         "Sealed Hall native source differs")
    verified = json.loads(V2_VERIFIED.read_text(encoding="utf-8"))
    native = json.loads(V2_NATIVE.read_text(encoding="utf-8"))
    need(verified["passed"] and verified["v1_rgba_byte_exact_cases"] == 4 and
         verified["actual_world_byte_exact_native3"] and verified["native_sha256"] == V2_NATIVE_SHA and
         verified["contract_sha256"] == V2_CONTRACT_SHA and native["complete"] and
         native["v1_raw_exact_cases"] == 4 and native["graphics_device"] == "Direct3D11",
         "V2 compressed/native source qualification differs")
    base = json.loads(OLD.read_text(encoding="utf-8"))
    need(base["native_sha256"] == V1_NATIVE and base["source_key"] == "initial:5" and
         [row["kind"] for row in base["inputs"]] == ["positions", "uv0", "indices", "sprite", "mask"],
         "Sealed expanded Hall fixture differs")
    audit_root = ROOT / "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/CityHallCameraMaterialGpuAudit"
    v2_runs = [p for p in audit_root.glob("*/manifest.json") if
               json.loads(p.read_text(encoding="utf-8")).get("schema") ==
               "rok.city_hall_camera_material_gpu_runtime.v2"]
    need(not OUT.exists() or (refresh and not v2_runs),
         "Fixture v3 already exists or runtime has begun; refusing replacement")
    code = [pin(resolve(item)) for item in CODE]
    extended_native = list(base["native_pins"])
    for source in (V2_NATIVE, V2_VERIFIED, V2_CONTRACT,
                   V2_RUN / native["actual_renderer_world_matrix"]["path"]):
        extended_native.append(pin(source))
    for role in ("main", "mask"):
        record = native[role + "_texture"]
        for key in ("compressed_blocks", "importer_before_json", "importer_after_json"):
            extended_native.append(pin(V2_RUN / record[key]["path"]))
    need(len({item["path"] for item in extended_native}) == len(extended_native),
         "Native pin duplicate")
    results = {}
    for variant in ("expanded_rgba", "source_bc"):
        fixture = json.loads(json.dumps(base))
        fixture["schema"] = "rok.city_hall_camera_material_gpu_fixture.v3"
        fixture["variant"] = variant
        fixture["native_v2_sha256"] = V2_NATIVE_SHA
        fixture["native_v2_contract_sha256"] = V2_CONTRACT_SHA
        fixture["native_pins"] = extended_native
        fixture["code_pins"] = code
        fixture["scope"] = "isolated Hall source-camera/material GPU " + variant + "; no GameApp/LOD/main-view acceptance"
        if variant == "source_bc":
            fixture["texture_upload"] = ("Original imported DXT5/DXT1 block rows, UV.v=0 source order; "
                                         "RHI initializer pitch in bytes per 4-pixel block row, no vertical flip; "
                                         "GPU source sampling requires four native RGBA frames")
            for index, role, fmt, block_size in ((3, "main", "PF_DXT5", 16), (4, "mask", "PF_DXT1", 8)):
                source = native[role + "_texture"]
                record = fixture["inputs"][index]
                compressed = V2_RUN / source["compressed_blocks"]["path"]
                need(sha(compressed) == source["compressed_blocks"]["sha256"] and
                     source["format_before"] == source["format_after"] == ("DXT5" if index == 3 else "DXT1") and
                     source["mips_before"] == source["mips_after"] == 1 and
                     source["filter_before"] == source["filter_after"] == "Bilinear" and
                     source["wrap_before"] == source["wrap_after"] == "Clamp" and
                     source["srgb_before"] and source["srgb_after"],
                     f"Source importer changed: {role}")
                record.update(pin(compressed))
                record.update(rhi_format=fmt, block_size_x=4, block_size_y=4, block_bytes=block_size,
                              source_blocks=pin(compressed), original_import_format=source["format_before"],
                              row_order="Original Unity Texture2D.GetRawTextureData compressed block rows; no flip")
                record.pop("source_raw", None)
                record.pop("source_png", None)
                need(record["byte_count"] == ((record["width"] + 3)//4)*((record["height"] + 3)//4)*block_size,
                     "BC byte length differs from block dimensions")
        destination = OUT / ("bc" if variant == "source_bc" else "expanded") / "manifest.json"
        write_once(destination, (json.dumps(fixture, indent=2, ensure_ascii=False) + "\n").encode("utf-8"), refresh)
        results[variant] = {"path": rel(destination), "sha256": sha(destination),
                            "inputs": 5, "cases": 4}
    return {"schema": "rok.city_hall_camera_material_gpu_fixture_v3_prepare.v1", "ready": True,
            "native_v2_sha256": V2_NATIVE_SHA, "native_v1_sha256": V1_NATIVE,
            "variants": results, "engine_started": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--refresh-owned-pre-runtime", action="store_true")
    arguments = parser.parse_args()
    print(json.dumps(prepare(arguments.refresh_owned_pre_runtime), indent=2))
