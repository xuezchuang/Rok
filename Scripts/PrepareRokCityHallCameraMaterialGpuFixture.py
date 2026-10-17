"""Prepare an immutable Hall source-camera/material GPU fixture from a verified Unity run.

No engine or asset mutation. Unity source textures are already GPU-expanded Gamma
RGBA8 with bottom-left raw rows. Channel-swapping them to BGRA8 preserves the
source UV convention; flipping rows here would mirror the sampled material.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
from pathlib import Path
from rok_data_paths import resolve

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
NATIVE_ROOT = ROOT / "Saved/UnityCityHallCameraMaterialProbeV1/OutputRuns"
SOURCE_ROOT = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json"
SOURCE_SHA = "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273"
ROW_SHA = "6b2844db3726051827252fe03f5892b2d99d54675c3d9e673f78a05197cb2a5e"
HALL = ROOT / "Saved/UnityCityHallWorldProbeV1/OutputRuns/20261009_032009_843_25796/hall_world_native.json"
HALL_SHA = "01b924d764395708e20766834595fa7943f1ec61c70215b6058914c2cc80ab5e"
CAMERA = ROOT / "Saved/UnityCityHallCameraLodProbeV1/OutputRuns/20261009_045949_616_43000/native_camera_lod.json"
CAMERA_SHA = "417eb43964bd2009879561194e4c5a75c7808f257dc6b84f8707ed4892b0d944"
TEXTURES = ROOT / "Saved/UnityCityGammaFamilyStateProbeV2/OutputRuns/20261008_234412_007_44320/native_state.json"
TEXTURES_SHA = "671f18f91275fc0a5a65107eff5e44857c59310873036293b99665f84c391b71"
CONTRACT = ROOT / "Saved/UnityCityHallCameraMaterialProbeV1/hall_camera_material_contract.json"
CONTRACT_SHA = "4bac306793ef6a94cab283ab169a3d2cd912f27649f3d093df0efb6ab260df2f"
OUTPUT = ROOT / "Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v1"
PHASES = ("day_black", "day_white", "night_black", "night_white")
PIN_CODE = (
    "Source/Rok/RokCityHallCameraMaterialAuditSubsystem.h",
    "Source/Rok/RokCityHallCameraMaterialAuditSubsystem.cpp",
    "Source/RokRendering/Public/RokCityHallCameraMaterialProbe.h",
    "Source/RokRendering/Private/RokCityHallCameraMaterialProbe.cpp",
    "Shaders/Private/RokCityHallCameraMaterial.usf",
    "Scripts/PrepareRokCityHallCameraMaterialGpuFixture.py",
    "Scripts/VerifyRokCityHallCameraMaterialGpuRuntime.py",
)


def need(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def rel(path: Path) -> str:
    path = path.resolve()
    need(path.is_relative_to(ROOT.resolve()), f"Path outside workspace: {path}")
    return path.relative_to(ROOT.resolve()).as_posix()


def pin(path: Path, *, size: int | None = None) -> dict:
    need(path.is_file(), f"Missing source pin: {path}")
    if size is not None:
        need(path.stat().st_size == size, f"Source byte count differs: {path}")
    return {"path": rel(path), "sha256": digest(path), "byte_count": path.stat().st_size}


def file_from(record: dict) -> Path:
    value = record["path"]
    need(isinstance(value, str) and not value.startswith("/") and "\\" not in value and ":" not in value,
         f"Unsafe path: {value}")
    path = (resolve(value)).resolve()
    need(path.is_relative_to(ROOT.resolve()), f"Path escaped workspace: {value}")
    return path


def checked(record: dict) -> Path:
    path = file_from(record)
    need(path.is_file() and digest(path) == record["sha256"] and
         ("byte_count" not in record or path.stat().st_size == record["byte_count"]),
         f"Pinned source differs: {path}")
    return path


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def mul(a: list[float], b: list[float]) -> list[float]:
    return [f32(sum(a[r * 4 + k] * b[k * 4 + c] for k in range(4)))
            for r in range(4) for c in range(4)]


def trans(a: list[float]) -> list[float]:
    return [a[c * 4 + r] for r in range(4) for c in range(4)]


def matrix(path: Path, sha: str) -> list[float]:
    need(path.is_file() and path.stat().st_size == 64 and digest(path) == sha,
         f"Original f32 matrix differs: {path}")
    values = list(struct.unpack("<16f", path.read_bytes()))
    need(all(math.isfinite(x) for x in values), "Nonfinite source matrix")
    return values


def colors(path: Path, names: tuple[str, ...]) -> dict:
    text = path.read_text(encoding="utf-8-sig")
    out: dict[str, list[float]] = {}
    for name in names:
        matches = re.findall(r"(?m)^\s*- " + re.escape(name) +
                             r": \{r: ([^,]+), g: ([^,]+), b: ([^,]+), a: ([^}]+)\}", text)
        need(len(matches) == 1, f"Source material color absent/ambiguous: {path} {name}")
        out[name] = [f32(float(x)) for x in matches[0]]
    return out


def texture_rgb_to_bgra(raw: bytes) -> bytes:
    result = bytearray(len(raw))
    result[0::4], result[1::4], result[2::4], result[3::4] = (
        raw[2::4], raw[1::4], raw[0::4], raw[3::4])
    return bytes(result)


def prepare(run: Path, expected_native_sha: str, refresh_owned_fixture: bool = False) -> dict:
    run = run.resolve()
    need(run.parent == NATIVE_ROOT.resolve(), "Native run must be a direct isolated OutputRuns child")
    need(re.fullmatch(r"[0-9a-f]{64}", expected_native_sha) is not None,
         "Externally pinned native SHA256 required")
    for path, sha in ((SOURCE_ROOT, SOURCE_SHA), (HALL, HALL_SHA), (CAMERA, CAMERA_SHA),
                      (TEXTURES, TEXTURES_SHA), (CONTRACT, CONTRACT_SHA)):
        need(digest(path) == sha, f"Sealed source chain differs: {path}")
    native_path = run / "hall_camera_material_native.json"
    verified_path = run / "verified_camera_material.json"
    need(digest(native_path) == expected_native_sha, "Native file differs from external SHA")
    native, verified, contract, source, hall, camera, state = map(
        read, (native_path, verified_path, CONTRACT, SOURCE_ROOT, HALL, CAMERA, TEXTURES))
    need(native["schema"] == "rok.unity_city_hall_camera_material_native.v1" and
         native["contract_sha256"] == CONTRACT_SHA and native["complete"] and
         native["graphics_device"] == "Direct3D11" and native["color_space"] == "Gamma" and
         native["width"] == 1440 and native["height"] == 810 and native["anti_aliasing"] == 1 and
         native["readback_row_order"] == "top_left_rgba8" and
         native["qualifier"] == "source_camera_material_fixture_only" and
         not native["game_app_loaded"] and not native["lod_qualified"] and
         not native["main_view_qualified"] and native["case_count"] == 4 and
         len(native["cases"]) == 4, "Native scope/target identity differs")
    need(verified["passed"] and verified["native_sha256"] == expected_native_sha and
         verified["contract_sha256"] == CONTRACT_SHA and
         verified["cases_checked"] == verified["png_raw_exact_cases"] == 4,
         "Independent Unity verification absent")
    need(source["rows"][27]["source_key"] == "initial:5" and
         source["rows"][27]["source_row_sha256"] == ROW_SHA and
         hall["schema"] == "rok.unity_city_hall_world_native.v3" and
         camera["schema"] == "rok.unity_city_hall_camera_lod_native.v1" and
         state["rows"][0]["source_key"] == "initial:5", "Sealed Hall identity differs")
    row = source["rows"][27]
    geo = (row["positions"], row["uv_channels"][0]["binary"], row["indices"]["binary"])
    need([(x["count"], x["stride_bytes"]) for x in geo] ==
         [(240, 12), (240, 16), (714, 4)], "Original Hall mesh counts/strides differ")
    world_rec = next(x for x in hall["nodes"] if x["phase"] == "runtime_replay_hall" and
                     x["component_type"] == "SpriteRenderer" and
                     x["source_file_id"] == "212751466141171401")
    need(world_rec["matrix_sha256"] == native["renderer_world_matrix_sha256"] ==
         contract["native3_source"]["world_matrix_sha256"], "Source renderer world identity differs")
    world_path = HALL.parent / world_rec["matrix_path"]
    world = matrix(world_path, world_rec["matrix_sha256"])
    lookup = {x["name"]: x for x in camera["matrices"]}
    view_rec, proj_rec = lookup["camera_world_to_camera"], lookup["camera_gpu_projection_rt"]
    view_path, proj_path = CAMERA.parent / view_rec["path"], CAMERA.parent / proj_rec["path"]
    view, projection = matrix(view_path, view_rec["sha256"]), matrix(proj_path, proj_rec["sha256"])
    source_to_clip = struct.pack("<16f", *trans(mul(mul(projection, view), world)))
    need(len(source_to_clip) == 64, "Source-camera matrix composition invalid")
    if OUTPUT.exists():
        need(refresh_owned_fixture and OUTPUT.is_dir(), "Fixture destination exists; explicit owned refresh required")
        need({p.name for p in OUTPUT.iterdir()} ==
             {"source_to_clip_row_major_f32le.bin", "sprite.source_uv_rows.bgra8.bin",
              "mask.source_uv_rows.bgra8.bin", "manifest.json"},
             "Owned fixture contents differ; refusing refresh")
        need((OUTPUT / "source_to_clip_row_major_f32le.bin").read_bytes() == source_to_clip,
             "Existing fixture matrix differs; refusing refresh")
    else:
        OUTPUT.mkdir(parents=True, exist_ok=False)
    (OUTPUT / "source_to_clip_row_major_f32le.bin").write_bytes(source_to_clip)
    inputs = []
    for kind, record in zip(("positions", "uv0", "indices"), geo):
        src = checked(record)
        inputs.append({"kind": kind, **pin(src), "count": record["count"],
                       "stride_bytes": record["stride_bytes"], "rhi_format": "ByteAddressBuffer"})
    raw_texture_root = TEXTURES.parent
    textures = state["rows"][0]["textures"]
    need(len(textures) == 2 and [x["source_guid"] for x in textures] ==
         ["57ec7ddf0b743a140b2707fe14b536e2", "c37165d75575008419379c96242599ba"],
         "Hall source texture identity differs")
    for index, (name, width, height) in enumerate((("sprite", 1024, 888), ("mask", 512, 444))):
        record = textures[index]
        need(record["width"] == width and record["height"] == height and
             record["mip_count"] == 1 and record["srgb"] and
             record["filter"] == "Bilinear" and record["wrap"] == "Clamp", "Texture import differs")
        mip = record["levels"][0]
        raw = raw_texture_root / mip["file"]
        png = raw_texture_root / mip["png"]
        need(raw.is_file() and raw.stat().st_size == width * height * 4 and digest(raw) == mip["sha256"] and
             png.is_file() and digest(png) == mip["png_sha256"], "Pinned decoded GPU texture differs")
        with Image.open(png) as image:
            top = image.convert("RGBA").tobytes()
        original = raw.read_bytes()
        need(b"".join(original[(height-1-y)*width*4:(height-y)*width*4] for y in range(height)) == top,
             "Unity raw texture bottom-row orientation differs from source PNG")
        converted = OUTPUT / f"{name}.source_uv_rows.bgra8.bin"
        gpu_bytes = texture_rgb_to_bgra(original)  # No vertical flip.
        if converted.exists():
            need(refresh_owned_fixture and converted.read_bytes() == gpu_bytes,
                 f"Existing source texture conversion differs: {converted}")
        else:
            converted.write_bytes(gpu_bytes)
        inputs.append({"kind": name, **pin(converted), "width": width, "height": height,
                       "rhi_format": "PF_B8G8R8A8", "srgb": False, "num_mips": 1,
                       "source_raw": pin(raw), "source_png": pin(png),
                       "row_order": "Unity ReadPixels bottom-left raw rows, preserved for UV.v=0"})
    day_path = ROOT / "ref/client-unity/Assets/BundleAssets/Scene/City/city_function/map_building.mat"
    night_path = ROOT / "ref/client-unity/Assets/BundleAssets/Scene/City/city_function/city_building_night.mat"
    day = colors(day_path, ("_Color_1", "_Color_2"))
    night = colors(night_path, ("_Color_1", "_Color_2", "_Color_3"))
    material_pins = [pin(day_path), pin(night_path),
                     pin(ROOT / "ref/client-unity/Assets/BundleAssets/Map/Building/map_building_with_mask-5.shader"),
                     pin(ROOT / "ref/client-unity/Assets/BundleAssets/Map/Shader/cm_city_mask_night.shader")]
    frames = []
    for index, case in enumerate(native["cases"]):
        phase = PHASES[index]
        need(case["phase"] == phase and case["render_queue"] == (2995 if index < 2 else 2996) and
             case["sprite_vertex_count"] == 240 and case["sprite_triangle_count"] == 238 and
             case["property_block_mask_matches"] and case["renderer_enabled"] and
             case["active_in_hierarchy"] and case["sprite_color_global"] == [1, 1, 1, 1] and
             case["renderer_color"] == [1, 1, 1, 1] and
             case["gpu_shader_time_before"] == case["gpu_shader_time_after"] == [0, 0, 0, 1] and
             case["city_transparency_global"] == 1 and
             case["background"] == ("black" if index % 2 == 0 else "white"),
             f"Native phase/material state differs: {phase}")
        raw = run / case["raw_top_left_rgba8"]["path"]
        png = run / case["png"]["path"]
        need(raw.parent == run and png.parent == run and digest(raw) == case["raw_top_left_rgba8"]["sha256"] and
             digest(png) == case["png"]["sha256"] and raw.stat().st_size == 1440*810*4,
             "Native frame raw/PNG pin differs")
        with Image.open(png) as image:
            need(image.format == "PNG" and image.size == (1440, 810) and
                 image.convert("RGBA").tobytes() == raw.read_bytes(), "Native PNG/raw pixel mismatch")
        need(case["changed_rgb_pixels"] > 80000 and case["blended_rgb_pixels"] > 80000,
             "Native material coverage unexpectedly empty")
        chosen = day if index < 2 else night
        frames.append({"phase": phase, "background": case["background"],
                       "material_queue": case["render_queue"], "team1": chosen["_Color_1"],
                       "team2": chosen["_Color_2"],
                       "night_color3": chosen.get("_Color_3", [0, 0, 0, 1]),
                       "native_raw": pin(raw), "native_png": pin(png),
                       "native_changed_rgb_pixels": case["changed_rgb_pixels"]})
    code_pins = [pin(resolve(item)) for item in PIN_CODE]
    native_pins = [pin(native_path), pin(verified_path), pin(CONTRACT), pin(HALL), pin(CAMERA),
                   pin(TEXTURES), pin(SOURCE_ROOT), pin(world_path), pin(view_path), pin(proj_path)]
    fixture = {"schema": "rok.city_hall_camera_material_gpu_fixture.v1", "width": 1440,
               "height": 810, "target_format": "PF_B8G8R8A8", "target_srgb": False,
               "source_key": "initial:5", "sprite_renderer_file_id": "212751466141171401",
               "source_row_sha256": ROW_SHA, "native_sha256": expected_native_sha,
               "native_contract_sha256": CONTRACT_SHA, "hall_native3_sha256": HALL_SHA,
               "native_camera2_sha256": CAMERA_SHA, "source_geometry_sha256": SOURCE_SHA,
               "source_texture_state_sha256": TEXTURES_SHA,
               "scope": "isolated source-camera/material fixture, not GameApp behaviors/LOD/main view",
               "matrix": pin(OUTPUT / "source_to_clip_row_major_f32le.bin", size=64),
               "matrix_formula": "transpose(camera_gpu_projection_rt * camera_world_to_camera * renderer_world); row-vector VS plus explicit clipY adapter",
               "texture_upload": "source GPU-expanded RGBA8 raw row0 is Unity UV.v=0; channel-swap only to BGRA8; no row flip; output staging is top-left",
               "inputs": inputs, "cases": frames, "native_pins": native_pins,
               "material_pins": material_pins, "code_pins": code_pins,
               "expected_http_dispatches": 0, "main_view_registered": False,
               "lod_qualified": False, "gameapp_accepted": False}
    manifest = OUTPUT / "manifest.json"
    manifest.write_text(json.dumps(fixture, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return {"fixture": rel(manifest), "fixture_sha256": digest(manifest),
            "native_sha256": expected_native_sha, "cases": len(frames), "inputs": len(inputs)}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--native-run", required=True, type=Path)
    parser.add_argument("--native-sha", required=True)
    parser.add_argument("--refresh-owned-fixture", action="store_true")
    args = parser.parse_args()
    print(json.dumps(prepare(args.native_run, args.native_sha, args.refresh_owned_fixture), indent=2))


if __name__ == "__main__":
    main()
