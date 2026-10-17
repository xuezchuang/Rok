"""Offline source-pin, compile and native RGBA gates for Hall camera/material fixture."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import struct
import subprocess
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityHallCameraMaterialProbeV1"
CONTRACT = PROBE / "hall_camera_material_contract.json"
EXPORTER = PROBE / "Assets/Editor/RenderRokUnityCityHallCameraMaterialNative.cs"
SHADER = PROBE / "Assets/ProbeHallTimeWitness.shader"
PREPARE = ROOT / "Scripts/PrepareRokUnityCityHallCameraMaterialProbe.py"
UNITY_DATA = Path("C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data")
PHASES = ["day_black", "day_white", "night_black", "night_white"]


def need(value: bool, reason: str) -> None:
    if not value:
        raise AssertionError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def path_in(root: Path, relative: str) -> Path:
    need(isinstance(relative, str) and relative and not Path(relative).is_absolute() and
         ".." not in Path(relative).parts and "\\" not in relative, f"Unsafe relative path: {relative}")
    resolved = (root / relative).resolve()
    need(resolved.is_relative_to(root.resolve()), f"Path escaped: {relative}")
    return resolved


def check_contract(contract: dict) -> dict:
    need(contract["schema"] == "rok.hall_camera_material_contract.v1" and
         Path(contract["probe_root"]).resolve() == PROBE.resolve() and
         contract["qualifier"] == "source_camera_material_fixture_only", "Probe identity/qualification differs")
    need(contract["producer_sha256"] == sha(PREPARE) and
         contract["verifier_sha256"] == sha(Path(__file__)) and
         contract["exporter_sha256"] == sha(EXPORTER) and
         contract["time_shader_sha256"] == sha(SHADER), "Producer/exporter/verifier/shader hash drift")
    need(contract["source_renderer_guid"] == "c825e8897df8e424e817e4ed05eef53e" and
         contract["source_renderer_file_id"] == "212751466141171401" and
         contract["sprite_guid"] == "57ec7ddf0b743a140b2707fe14b536e2" and
         contract["mask_guid"] == "c37165d75575008419379c96242599ba", "Hall exact source identity differs")
    need(contract["day_material_asset"].endswith("/map_building.mat") and
         contract["night_material_asset"].endswith("/city_building_night.mat"), "Hall material pair differs")
    from PrepareRokUnityCityHallCameraMaterialProbe import PINNED, NATIVE_CAMERA, NATIVE3, GEOMETRY
    need(contract["source_pins"] == PINNED, "Source SHA pin set differs")
    for relative, digest in PINNED.items():
        need(sha(ROOT / relative) == digest, f"Source byte drift: {relative}")
    for key, source in (("native_camera_source", NATIVE_CAMERA), ("native3_source", NATIVE3),
                        ("geometry_source", GEOMETRY)):
        value = contract[key]
        need(path_in(ROOT, value["path"]) == source.resolve() and value["sha256"] == sha(source),
             f"Source manifest drift: {key}")
    records = contract["input_files"]
    need(len(records) == len({item["path"] for item in records}) and len(records) >= 65,
         "Prepared input file set duplicate/incomplete")
    for item in records:
        copied = path_in(PROBE, item["path"])
        need(copied.is_file() and sha(copied) == item["sha256"], f"Prepared input bytes differ: {item['path']}")
    source = path_in(PROBE, contract["source_input_path"])
    need(sha(source) == contract["source_input_sha256"], "Typed source input SHA differs")
    typed = json.loads(source.read_text(encoding="utf-8"))
    need(typed["schema"] == "rok.hall_camera_material_source_input.v1" and
         typed["qualification"] == contract["qualifier"] and
         (typed["width"], typed["height"]) == (1440, 810) and
         typed["phase_order"] == PHASES and
         (typed["fov"], typed["near_clip"], typed["far_clip"]) == (7.5, 2.0, 3350.0) and
         typed["sprite_color_rgba"] == [1.0] * 4 and
         (typed["city_transparency"], typed["expected_shader_time_y"]) == (1.0, 0.0),
         "Controlled source-camera/global/time fixture differs")
    camera = json.loads(NATIVE_CAMERA.read_text(encoding="utf-8"))
    need(camera["complete"] and camera["qualifier"] == "camera_transform_fixture_only" and
         typed["camera_position_xyz"] == camera["camera_position_xyz"] and
         typed["camera_rotation_xyzw"] == camera["camera_rotation_xyzw"],
         "Camera Native2 source pose differs")
    for name in ("camera_world_to_camera", "camera_projection", "camera_gpu_projection_rt"):
        source_matrix = next((m for m in camera["matrices"] if m["name"] == name), None)
        need(source_matrix is not None and source_matrix["byte_count"] == 64,
             f"NativeCamera2 matrix record absent: {name}")
        copied_matrix = path_in(PROBE, typed[name + "_path"])
        need(sha(copied_matrix) == source_matrix["sha256"] and copied_matrix.stat().st_size == 64,
             f"NativeCamera2 matrix bytes differ: {name}")
    native3 = json.loads(NATIVE3.read_text(encoding="utf-8"))
    source_renderer = [n for n in native3["nodes"] if n["phase"] == "runtime_replay_hall" and
                       n["source_file_id"] == "212751466141171401" and n["component_type"] == "SpriteRenderer"]
    need(len(source_renderer) == 1, "Native3 Hall renderer identity ambiguous")
    world = NATIVE3.parent / source_renderer[0]["matrix_path"]
    need(sha(path_in(PROBE, typed["hall_world_matrix_path"])) == sha(world) ==
         source_renderer[0]["matrix_sha256"], "Native3 Hall world matrix differs")
    geometry = json.loads(GEOMETRY.read_text(encoding="utf-8"))["rows"][27]
    for key, source_spec in (("positions", geometry["positions"]),
                             ("uv0", geometry["uv_channels"][0]["binary"]),
                             ("indices", geometry["indices"]["binary"])):
        original = path_in(ROOT, source_spec["path"])
        copied = path_in(PROBE, typed[key + "_path"])
        need(sha(original) == sha(copied) == source_spec["sha256"], f"Hall source {key} changed")
    return typed


def compile_exporter() -> str:
    compiler = UNITY_DATA / "Tools/Roslyn/csc.exe"
    need(compiler.is_file(), "Unity2019 Roslyn compiler absent")
    refs = list((UNITY_DATA / "Managed/UnityEngine").glob("*.dll"))
    refs += list((UNITY_DATA / "MonoBleedingEdge/lib/mono/4.7.1-api").glob("*.dll"))
    refs += list((UNITY_DATA / "MonoBleedingEdge/lib/mono/4.7.1-api/Facades").glob("*.dll"))
    refs.append(UNITY_DATA / "Managed/UnityEditor.dll")
    need(all(p.is_file() for p in refs), "Unity2019 compiler references absent")
    validation = PROBE / "Validation"
    validation.mkdir(parents=True, exist_ok=True)
    output = validation / "HallCameraMaterialSyntax.dll"
    args = ["/nologo", "/target:library", "/deterministic+", "/define:UNITY_EDITOR", "/out:" + str(output)]
    args += ["/reference:" + str(ref) for ref in refs]
    args.append(str(EXPORTER))
    response = validation / "compiler.rsp"
    response.write_text("\n".join('"' + arg + '"' for arg in args), encoding="utf-8")
    process = subprocess.run([str(compiler), "@" + str(response)], capture_output=True, check=False)
    (validation / "compiler.log").write_bytes(process.stdout + process.stderr)
    need(process.returncode == 0, "Unity2019 C# compile failed; see isolated Validation/compiler.log")
    return sha(output)


def artifact(run: Path, spec: dict, expected_size: int | None = None) -> bytes:
    path = path_in(run, spec["path"])
    need(path.is_file() and sha(path) == spec["sha256"] and
         path.stat().st_size == spec["byte_count"], f"Native artifact hash/size differs: {spec['path']}")
    if expected_size is not None:
        need(path.stat().st_size == expected_size, f"Native artifact extent differs: {spec['path']}")
    return path.read_bytes()


def verify_run(run: Path, contract: dict, typed: dict) -> dict:
    run = run.resolve()
    need(run.parent == (PROBE / "OutputRuns").resolve() and run.is_dir(), "Run outside isolated OutputRuns")
    native_path = run / "hall_camera_material_native.json"
    need(native_path.is_file(), "Native manifest absent or Unity exporter failed")
    native = json.loads(native_path.read_text(encoding="utf-8"))
    need(native["schema"] == "rok.unity_city_hall_camera_material_native.v1" and native["complete"] and
         native["contract_sha256"] == sha(CONTRACT) and native["input_sha256"] == contract["source_input_sha256"] and
         native["exporter_sha256"] == sha(EXPORTER) and native["time_shader_sha256"] == sha(SHADER),
         "Native result identity/code/input differs")
    need(native["graphics_device"] == "Direct3D11" and native["color_space"] == "Gamma" and
         native["qualifier"] == "source_camera_material_fixture_only" and
         native["target_format"] == "ARGB32 RenderTextureReadWrite.Default Gamma" and
         native["readback_row_order"] == "top_left_rgba8" and
         not any(native[k] for k in ("game_app_loaded", "source_behaviors_executed", "lod_qualified", "main_view_qualified")),
         "Native graphics or qualification differs")
    need((native["width"], native["height"], native["anti_aliasing"], native["case_count"]) == (1440, 810, 1, 4) and
         native["source_renderer_file_id"] == contract["source_renderer_file_id"] and
         native["sprite_guid"] == contract["sprite_guid"] and native["mask_guid"] == contract["mask_guid"] and
         native["renderer_world_matrix_sha256"] == contract["native3_source"]["world_matrix_sha256"],
         "Native Hall source/camera extent differs")
    need(native["camera_position_xyz"] == typed["camera_position_xyz"] and
         native["camera_rotation_xyzw"] == typed["camera_rotation_xyzw"] and
         (native["camera_fov"], native["camera_near"], native["camera_far"]) ==
         (typed["fov"], typed["near_clip"], typed["far_clip"]) and
         abs(native["camera_aspect"] - 1440/810) < 1e-6,
         "Native source-configured camera changed")
    need(native["sprite_color_global"] == [1.0] * 4 and native["city_transparency_global"] == 1.0,
         "Native globals differ from explicit controlled fixture")
    camera_source = json.loads((ROOT / contract["native_camera_source"]["path"]).read_text(encoding="utf-8"))
    for name in ("camera_world_to_camera", "camera_projection", "camera_gpu_projection_rt"):
        expected = next(m for m in camera_source["matrices"] if m["name"] == name)
        source_raw = path_in(PROBE, typed[name + "_path"]).read_bytes()
        observed = artifact(run, native[name], 64)
        a, b = struct.unpack("<16f", source_raw), struct.unpack("<16f", observed)
        need(all(abs(x-y) <= 1e-5 for x,y in zip(a,b)) and expected["sha256"] == sha(path_in(PROBE, typed[name + "_path"])),
             f"NativeCamera2 measured camera matrix changed: {name}")
    cases = native["cases"]
    need([c["phase"] for c in cases] == PHASES, "Native phase order/case set differs")
    images: dict[str, bytes] = {}
    coverage = {}
    for case in cases:
        phase = case["phase"]
        night = phase.startswith("night_")
        white = phase.endswith("white")
        need(case["background"] == ("white" if white else "black") and
             case["material_asset"] == contract["night_material_asset" if night else "day_material_asset"] and
             case["render_queue"] == (2996 if night else 2995) and
             case["property_block_mask_matches"] and case["renderer_enabled"] and case["active_in_hierarchy"] and
             (case["sprite_vertex_count"], case["sprite_triangle_count"]) == (240, 238),
             f"Native source material/renderer differs: {phase}")
        need(case["main_srgb"] and case["mask_srgb"] and
             (case["main_filter"], case["mask_filter"], case["main_wrap"], case["mask_wrap"]) ==
             ("Bilinear", "Bilinear", "Clamp", "Clamp") and
             (case["main_width"], case["main_height"], case["main_mips"],
              case["mask_width"], case["mask_height"], case["mask_mips"]) ==
             (1024, 888, 1, 512, 444, 1), f"Source importer/sampler differs: {phase}")
        need(case["sprite_color_global"] == [1.0] * 4 and case["city_transparency_global"] == 1.0 and
             case["renderer_color"] == [1.0] * 4, f"Source shader global/tint differs: {phase}")
        for side in ("before", "after"):
            raw = artifact(run, case[f"shader_time_{side}_f32le"], 16)
            values = struct.unpack("<4f", raw)
            need(tuple(case[f"gpu_shader_time_{side}"]) == values and
                 values == (0.0, 0.0, 0.0, 1.0), f"GPU _Time witness differs: {phase}/{side}")
        need(case["shader_time_before_f32le"]["sha256"] == case["shader_time_after_f32le"]["sha256"],
             f"GPU time changed across source sprite draw: {phase}")
        rgba = artifact(run, case["raw_top_left_rgba8"], 1440 * 810 * 4)
        png_path = path_in(run, case["png"]["path"])
        artifact(run, case["png"])
        with Image.open(png_path) as opened:
            need(opened.size == (1440, 810), f"PNG extent differs: {phase}")
            decoded = opened.convert("RGBA").tobytes()
        need(decoded == rgba, f"PNG decoded pixels differ from native top-left RGBA: {phase}")
        channel = 255 if white else 0
        changed = sum(1 for i in range(0, len(rgba), 4)
                      if rgba[i] != channel or rgba[i+1] != channel or rgba[i+2] != channel)
        blended = sum(1 for i in range(0, len(rgba), 4)
                      if any(0 < rgba[i+c] < 255 for c in range(3)))
        need(changed == case["changed_rgb_pixels"] and blended == case["blended_rgb_pixels"] and
             changed > 100 and blended > 10, f"Native source sprite image blank/coverage wrong: {phase}")
        images[phase] = rgba
        coverage[phase] = changed
    for mode in ("day", "night"):
        black, white = images[mode + "_black"], images[mode + "_white"]
        # Distinct opaque backgrounds must expose the same source silhouette, not two duplicate frames.
        differing = sum(1 for i in range(0, len(black), 4)
                        if black[i:i+3] != white[i:i+3])
        need(differing > 1000, f"Two-background alpha evidence absent: {mode}")
    need(images["day_black"] != images["night_black"], "Day/night source shader result identical")
    return {"schema": "rok.hall_camera_material_native_verify.v1", "passed": True,
            "run": run.relative_to(ROOT).as_posix(), "native_sha256": sha(native_path),
            "contract_sha256": sha(CONTRACT), "cases_checked": 4, "png_raw_exact_cases": 4,
            "coverage_rgb_pixels": coverage,
            "scope": "source Hall SpriteRenderer material, pinned camera/world fixture, GPU _Time.y=0 and two backgrounds; no GameApp behavior/LOD/main-view acceptance"}


def self_test(contract: dict) -> None:
    for mutate in (lambda d: d.update({"source_renderer_file_id": "0"}),
                   lambda d: d.update({"qualifier": "gameapp_accepted"}),
                   lambda d: d["input_files"][0].update({"sha256": "0" * 64})):
        invalid = copy.deepcopy(contract)
        mutate(invalid)
        try:
            check_contract(invalid)
        except AssertionError:
            continue
        raise AssertionError("Negative static source/identity mutation escaped")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prepared", action="store_true")
    parser.add_argument("--run", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    need(args.prepared or args.run is not None, "Specify --prepared or --run")
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    typed = check_contract(contract)
    if args.self_test:
        self_test(contract)
    if args.prepared:
        compiler_sha = compile_exporter()
        report = {"schema": "rok.hall_camera_material_prepared_verify.v1", "passed": True,
                  "contract_sha256": sha(CONTRACT), "exporter_sha256": sha(EXPORTER),
                  "verifier_sha256": sha(Path(__file__)), "compiler_dll_sha256": compiler_sha,
                  "negative_cases": 3 if args.self_test else 0, "unity_started": False}
        destination = PROBE / "Validation/prepared_verify.json"
        destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2))
    if args.run is not None:
        report = verify_run(args.run, contract, typed)
        destination = args.run.resolve() / "verified_camera_material.json"
        need(not destination.exists(), "Native verification output already exists")
        destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
