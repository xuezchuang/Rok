"""Seal GPU-measured Unity Night _Time.y into the existing five-family fixture.

The V2 40 strict frames and all five 2x2 diagnostic inputs stay byte-for-byte
identified. Only two formerly pending Night references become measured B1 A/B.
"""
from __future__ import annotations

import copy
import hashlib
import json
import math
import os
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
V2 = ROOT / "Saved/RokUnityMigration/CityGammaFamilyRasterFixture/v2/manifest.json"
V2_SHA = "7607afededef0e985c4458020e672389b14afd6f4566d47ee10ce770babe1165"
NIGHT_RUN = ROOT / "Saved/UnityCityNightTimeProbeV1/OutputRuns/20261009_005901_476_41564"
NATIVE = NIGHT_RUN / "native_night_time.json"
NATIVE_SHA = "8c228f9f01cabc3bcdabc9ef09926470d38798b4a11c09c308cb5998636d99ea"
VERIFY = NIGHT_RUN / "native_verify.json"
VERIFY_SHA = "9a5dd3c5971f284be82adcf9f29d55e7a9befb702bd4540bf70c4e3a8d9f9062"
CONTRACT = ROOT / "Saved/UnityCityNightTimeProbeV1/night_time_contract.json"
CONTRACT_SHA = "8549e7e1071658f72cc9fb93d01c5434ab5d362b652ff6168ece828c77e4a039"
EXPORTER = ROOT / "Saved/UnityCityNightTimeProbeV1/Assets/Editor/ExportRokUnityCityNightTimeProbe.cs"
TIME_SHADER = ROOT / "Saved/UnityCityNightTimeProbeV1/Assets/ProbeNightTime.shader"
SOURCE_SHADER = ROOT / "ref/client-unity/Assets/BundleAssets/Map/Shader/cm_city_mask_night.shader"
OUT = ROOT / "Saved/RokUnityMigration/CityGammaFamilyRasterFixture/v3"


def need(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def immutable(path: Path, data: bytes) -> None:
    if path.exists():
        need(path.read_bytes() == data, f"Existing fixture differs: {path}")
        return
    fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644)
    with os.fdopen(fd, "wb") as stream:
        stream.write(data)


def main() -> None:
    for path, expected in ((V2, V2_SHA), (NATIVE, NATIVE_SHA), (VERIFY, VERIFY_SHA),
                           (CONTRACT, CONTRACT_SHA)):
        need(sha(path) == expected, f"Sealed input differs: {path}")
    v2 = json.loads(V2.read_text(encoding="utf-8"))
    native = json.loads(NATIVE.read_text(encoding="utf-8"))
    verified = json.loads(VERIFY.read_text(encoding="utf-8"))
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    need(v2["schema"] == "rok.city_gamma_family_raster_fixture.v1"
         and len(v2["rows"]) == 5 and len(v2["cases"]) == 42
         and native["schema"] == "rok.unity_city_night_time_native.v1"
         and native["complete"] and not native["failure"] and not native["game_app_loaded"]
         and verified["schema"] == "rok.unity_city_night_time_verify.v1" and verified["passed"]
         and verified["native_sha256"] == NATIVE_SHA and verified["contract_sha256"] == CONTRACT_SHA
         and verified["before_after_float4_raw_equal"] is True
         and contract["schema"] == "rok.unity_city_night_time_contract.v1"
         and native["source_contract_sha256"] == CONTRACT_SHA,
         "Night native/contract/verifier identity differs")
    for path, key in ((EXPORTER, "exporter_sha256"), (TIME_SHADER, "time_shader_sha256")):
        need(sha(path) == contract[key] == native[key], f"Night exporter/time shader differs: {path}")
    source_shader_entry = next((s for s in contract["source_files"]
                                if s["source_path"] == relative(SOURCE_SHADER)), None)
    need(source_shader_entry is not None and sha(SOURCE_SHADER) == source_shader_entry["source_sha256"],
         "Night source shader changed")
    row = v2["rows"][0]
    need(native["source_material_guid"] == row["source_material_guid"]
         and native["source_renderer_file_id"] == row["renderer_file_id"]
         and native["source_shader_name"] == row["source_shader"]
         and native["source_queue"] == row["source_queue"]
         and native["source_prefab_guid"] == "c825e8897df8e424e817e4ed05eef53e"
         and native["source_mask_sprite_path"] == "Assets/Source/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_1_101_1_mask.png"
         and native["source_mask_sprite_file_id"] == "21300000"
         and native["source_mask_property_block_matches"] is True
         and native["source_sprite_vertices"] == 240 and native["source_sprite_triangles"] == 238,
         "Night renderer/material/source mesh metadata differs")
    need(native["draw_order"] == "same CameraEvent.AfterEverything CommandBuffer: vertex _Time.y before; background A, Night B1a, Night B0a; background B, Night B1b, Night B0b; vertex _Time.y after"
         and "GPU vertex-stage" in native["time_source_scope"]
         and native["time_readback_format"] == "Texture2D RGBAFloat 1x1 little-endian raw float32"
         and native["time_target_format"] == "ARGBFloat RenderTextureReadWrite.Linear"
         and native["target_format"] == "ARGB32 RenderTextureReadWrite.Default Gamma"
         and native["color_space"] == "Gamma" and native["graphics_device"] == "Direct3D11"
         and native["unity_version"] == "2019.4.28f1c1", "Same-buffer GPU time provenance differs")
    raw = []
    for stage in ("before", "after"):
        path = NIGHT_RUN / native[f"time_{stage}_bin"]
        need(path.is_file() and len(path.read_bytes()) == 16
             and sha(path) == native[f"time_{stage}_sha256"], f"GPU time {stage} bytes differ")
        value = struct.unpack("<4f", path.read_bytes())
        need(all(math.isfinite(x) for x in value)
             and value[0] == native[f"shader_time_{stage}"]
             and value[1:] == (0.0, 0.0, 1.0), f"GPU time {stage} float4 differs")
        raw.append(path.read_bytes())
    need(raw[0] == raw[1] and struct.unpack("<f", raw[0][:4])[0] == verified["gpu_shader_time_y_f32"] == 0.0,
         "GPU shader _Time.y before/after or verifier differs")
    manifest = copy.deepcopy(v2)
    manifest["schema"] = "rok.city_gamma_family_raster_fixture.v3"
    manifest["sealed_v2_fixture_path"] = relative(V2)
    manifest["sealed_v2_fixture_sha256"] = V2_SHA
    manifest["scope"] = "isolated Unity diagnostic quad; 42 controlled RGB cases at measured one-time GPU shader _Time.y=0, not GameApp"
    manifest["diagnostic"]["time_y"] = "one_time_gpu_measured_0_from_native_same_command_buffer"
    manifest["diagnostic"]["sealed_v2_night_time_scope"] = "old V2 Night B1 frames had no GPU shader time proof"
    manifest["night_gpu_time"] = {
        "native_path": relative(NATIVE), "native_sha256": NATIVE_SHA,
        "native_verify_path": relative(VERIFY), "native_verify_sha256": VERIFY_SHA,
        "contract_path": relative(CONTRACT), "contract_sha256": CONTRACT_SHA,
        "exporter_path": relative(EXPORTER), "exporter_sha256": sha(EXPORTER),
        "time_shader_path": relative(TIME_SHADER), "time_shader_sha256": sha(TIME_SHADER),
        "source_shader_path": relative(SOURCE_SHADER), "source_shader_sha256": sha(SOURCE_SHADER),
        "time_before_path": relative(NIGHT_RUN / native["time_before_bin"]),
        "time_before_sha256": native["time_before_sha256"],
        "time_after_path": relative(NIGHT_RUN / native["time_after_bin"]),
        "time_after_sha256": native["time_after_sha256"],
        "before_after_float4_raw_equal": True,
        "gpu_shader_time_y_f32": verified["gpu_shader_time_y_f32"],
        "gpu_shader_time_y_bits_hex": raw[0][:4].hex(),
        "time_source_scope": native["time_source_scope"],
        "draw_order": native["draw_order"],
        "source_material_guid": native["source_material_guid"],
        "source_renderer_file_id": native["source_renderer_file_id"],
        "source_shader_name": native["source_shader_name"],
        "source_queue": native["source_queue"],
        "source_prefab_guid": native["source_prefab_guid"],
        "source_mask_sprite_path": native["source_mask_sprite_path"],
        "source_mask_sprite_file_id": native["source_mask_sprite_file_id"],
        "source_mask_property_block_matches": native["source_mask_property_block_matches"],
        "source_sprite_vertices": native["source_sprite_vertices"],
        "source_sprite_triangles": native["source_sprite_triangles"],
        "game_app_loaded": False,
    }
    replacements = {"family_a": ("b1", "night_mask_b1.png"),
                    "family_b": ("b1_b", "night_mask_b1_background_b.png")}
    replaced = 0
    for case in manifest["cases"]:
        case["controlled_time_y_f32"] = verified["gpu_shader_time_y_f32"]
        case["controlled_time_proof_sha256"] = native["time_before_sha256"]
        if case["family_index"] == 0 and case["frame"] in replacements:
            key, filename = replacements[case["frame"]]
            path = NIGHT_RUN / filename
            need(sha(path) == native[f"{key}_sha256"] and native[f"{key}_png"] == filename,
                 f"New Night B1 {key} source PNG differs")
            with Image.open(path) as image:
                need(image.format == "PNG" and image.mode == "RGBA" and image.size == (128, 128),
                     f"New Night B1 {key} PNG format/extent differs")
                case["source_center_rgba8"] = list(image.getpixel((64, 64)))
            case["source_png"] = relative(path)
            case["source_png_sha256"] = sha(path)
            case["comparison"] = "rgb_max_1_lsb"
            case["night_gpu_time_source"] = "same_buffer_before_after_raw_float4"
            replaced += 1
        else:
            old = next(c for c in v2["cases"] if c["id"] == case["id"])
            need(case["source_png"] == old["source_png"]
                 and case["source_png_sha256"] == old["source_png_sha256"]
                 and case["comparison"] == "rgb_max_1_lsb", "Sealed V2 strict case changed")
    need(replaced == 2 and len(manifest["cases"]) == 42
         and all(c["comparison"] == "rgb_max_1_lsb" for c in manifest["cases"]),
         "V3 42 strict case replacement count differs")
    # V2 blobs are referenced by hash/path; they are not copied or regenerated.
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "manifest.json"
    immutable(path, (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    print(json.dumps({"manifest": str(path), "sha256": sha(path), "strict_cases": 42}))


if __name__ == "__main__":
    main()
