"""Strict offline and post-Unity verification of the isolated Hall material V2 probe."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import struct
import subprocess
from pathlib import Path

from PIL import Image

from PrepareRokUnityCityHallCameraMaterialProbeV2 import (
    CONTRACT, EXPORTER, MUTABLE_META, NEW, OLD_CONTRACT, OLD_CONTRACT_SHA,
    OLD_NATIVE, OLD_NATIVE_SHA, OLD_RUN, PHASES, ROOT, sha,
)

UNITY_DATA = Path("C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data")
SHADER = NEW / "Assets/ProbeHallTimeWitness.shader"


def need(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def inside(root: Path, relative: str) -> Path:
    need(isinstance(relative, str) and relative and "\\" not in relative and
         not Path(relative).is_absolute() and ".." not in Path(relative).parts,
         f"Unsafe artifact path: {relative}")
    path = (root / relative).resolve()
    need(path.is_relative_to(root.resolve()), f"Escaped artifact path: {relative}")
    return path


def artifact(run: Path, item: dict, size: int | None = None) -> bytes:
    path = inside(run, item["path"])
    need(path.is_file() and sha(path) == item["sha256"] and path.stat().st_size == item["byte_count"],
         f"Artifact hash or length differs: {item['path']}")
    if size is not None:
        need(path.stat().st_size == size, f"Artifact expected size differs: {item['path']}")
    return path.read_bytes()


def contract_check(c: dict, prepared: bool) -> None:
    need(sha(OLD_NATIVE) == OLD_NATIVE_SHA and sha(OLD_CONTRACT) == OLD_CONTRACT_SHA,
         "Sealed V1 oracle/contract drift")
    need(c["schema"] == "rok.hall_camera_material_contract.v2" and
         Path(c["probe_root"]).resolve() == NEW.resolve() and
         c["qualifier"] == "source_camera_material_fixture_only" and
         c["producer_sha256"] == sha(ROOT / "Scripts/PrepareRokUnityCityHallCameraMaterialProbeV2.py") and
         c["verifier_sha256"] == sha(Path(__file__)) and c["exporter_sha256"] == sha(EXPORTER) and
         c["time_shader_sha256"] == sha(SHADER), "V2 identity/source code drift")
    need(Path(c["v1_native_manifest_path"]).resolve() == OLD_NATIVE.resolve() and
         c["v1_native_manifest_sha256"] == OLD_NATIVE_SHA, "V1 oracle anchor drift")
    old = json.loads(OLD_NATIVE.read_text(encoding="utf-8"))
    expected = {case["phase"]: case["raw_top_left_rgba8"]["sha256"] for case in old["cases"]}
    need([p["path"] for p in c["v1_case_raw"]] == list(PHASES) and
         {p["path"]: p["sha256"] for p in c["v1_case_raw"]} == expected,
         "V1 per-phase byte oracle drift")
    original = json.loads(OLD_CONTRACT.read_text(encoding="utf-8"))
    need(c["source_pins"] == original["source_pins"] and
         c["source_renderer_guid"] == original["source_renderer_guid"] and
         c["source_renderer_file_id"] == original["source_renderer_file_id"] and
         c["sprite_guid"] == original["sprite_guid"] and c["mask_guid"] == original["mask_guid"],
         "Hall source identity drift")
    for relative, digest in c["source_pins"].items():
        need(sha(inside(ROOT, relative)) == digest, f"Reference source drift: {relative}")
    pins = c["input_files"]
    need(len(pins) == 77 and len({p["path"] for p in pins}) == len(pins) and
         set(p["path"] for p in pins if p["path"] in MUTABLE_META) == MUTABLE_META,
         "V2 source pin set differs")
    for pin in pins:
        path = inside(NEW, pin["path"])
        need(path.is_file() and (not prepared and pin["path"] in MUTABLE_META or sha(path) == pin["sha256"]),
             f"V2 input drift: {pin['path']}")
    need(sha(inside(NEW, c["source_input_path"])) == c["source_input_sha256"], "Typed fixture drift")
    typed = json.loads(inside(NEW, c["source_input_path"]).read_text(encoding="utf-8"))
    need(typed["phase_order"] == list(PHASES) and typed["width"] == 1440 and typed["height"] == 810 and
         typed["qualification"] == "source_camera_material_fixture_only" and
         (typed["fov"], typed["near_clip"], typed["far_clip"]) == (7.5, 2, 3350) and
         typed["sprite_color_rgba"] == [1, 1, 1, 1] and
         typed["city_transparency"] == 1 and typed["expected_shader_time_y"] == 0,
         "Source camera/material controlled fixture drift")


def compile_exporter() -> str:
    compiler = UNITY_DATA / "Tools/Roslyn/csc.exe"
    refs = list((UNITY_DATA / "Managed/UnityEngine").glob("*.dll"))
    refs += list((UNITY_DATA / "MonoBleedingEdge/lib/mono/4.7.1-api").glob("*.dll"))
    refs += list((UNITY_DATA / "MonoBleedingEdge/lib/mono/4.7.1-api/Facades").glob("*.dll"))
    refs.append(UNITY_DATA / "Managed/UnityEditor.dll")
    need(compiler.is_file() and all(p.is_file() for p in refs), "Unity2019 compiler refs missing")
    validation = NEW / "Validation"
    validation.mkdir(exist_ok=True)
    output = validation / "HallMaterialV2Syntax.dll"
    args = ["/nologo", "/target:library", "/deterministic+", "/define:UNITY_EDITOR", "/out:" + str(output)]
    args += ["/reference:" + str(ref) for ref in refs]
    args.append(str(EXPORTER))
    response = validation / "compiler.rsp"
    response.write_text("\n".join('"' + item + '"' for item in args), encoding="utf-8")
    result = subprocess.run([str(compiler), "@" + str(response)], capture_output=True, check=False)
    (validation / "compiler.log").write_bytes(result.stdout + result.stderr)
    need(result.returncode == 0, "Unity2019 C# syntax compile failed; see V2 Validation/compiler.log")
    return sha(output)


def verify_run(run: Path, c: dict) -> dict:
    run = run.resolve()
    need(run.is_dir() and run.parent == (NEW / "OutputRuns").resolve(), "Run outside V2 isolated OutputRuns")
    manifest = run / "hall_camera_material_native_v2.json"
    need(manifest.is_file(), "V2 Unity manifest absent")
    n = json.loads(manifest.read_text(encoding="utf-8"))
    need(n["schema"] == "rok.unity_city_hall_camera_material_native.v2" and n["complete"] and
         n["contract_sha256"] == sha(CONTRACT) and n["exporter_sha256"] == sha(EXPORTER) and
         n["input_sha256"] == c["source_input_sha256"] and n["time_shader_sha256"] == sha(SHADER) and
         n["graphics_device"] == "Direct3D11" and n["color_space"] == "Gamma" and
         n["qualifier"] == "source_camera_material_fixture_only" and
         n["readback_row_order"] == "top_left_rgba8" and
         not any(n[k] for k in ("game_app_loaded", "source_behaviors_executed", "lod_qualified", "main_view_qualified")),
         "V2 native identity/render scope differs")
    need((n["width"], n["height"], n["anti_aliasing"], n["case_count"], n["v1_raw_exact_cases"]) ==
         (1440, 810, 1, 4, 4), "V2 four-phase native extent or oracle comparison differs")
    source_world = inside(NEW, "ProbeInputs/hall_native3_world.f32le").read_bytes()
    actual_world = artifact(run, n["actual_renderer_world_matrix"], 64)
    source_matrix, actual_matrix = struct.unpack("<16f", source_world), struct.unpack("<16f", actual_world)
    matrix_max_delta = max(abs(a - b) for a, b in zip(source_matrix, actual_matrix))
    need(matrix_max_delta <= 1e-5, "Actual Hall renderer world matrix differs beyond Native3 gate")
    need(n["renderer_world_matrix_sha256"] == hashlib.sha256(source_world).hexdigest(),
         "Native3 matrix SHA witness differs")
    old_cases = {case["phase"]: case for case in json.loads(OLD_NATIVE.read_text(encoding="utf-8"))["cases"]}
    need([case["phase"] for case in n["cases"]] == list(PHASES), "V2 phase order differs")
    for case in n["cases"]:
        phase = case["phase"]
        need(case["render_queue"] == (2996 if phase.startswith("night") else 2995) and
             case["property_block_mask_matches"] and case["renderer_enabled"] and
             case["active_in_hierarchy"] and case["sprite_vertex_count"] == 240 and
             case["sprite_triangle_count"] == 238 and case["main_texture_format"] == "DXT5" and
             case["mask_texture_format"] == "DXT1", "V2 source material/mesh/import differs: " + phase)
        rgba = artifact(run, case["raw_top_left_rgba8"], 1440 * 810 * 4)
        v1 = artifact(OLD_RUN, old_cases[phase]["raw_top_left_rgba8"], 1440 * 810 * 4)
        need(rgba == v1, "V2 RGBA differs byte-for-byte from frozen V1: " + phase)
        png = artifact(run, case["png"])
        with Image.open(inside(run, case["png"]["path"])) as image:
            need(image.size == (1440, 810) and image.convert("RGBA").tobytes() == rgba,
                 "V2 PNG decode/raw differs: " + phase)
        need(len(png) > 32, "PNG empty: " + phase)
        for side in ("before", "after"):
            witness = artifact(run, case[f"shader_time_{side}_f32le"], 16)
            need(struct.unpack("<4f", witness) == (0, 0, 0, 1), "GPU shader time fixture changed")
    original_meta = {p["path"]: p["sha256"] for p in c["input_files"] if p["path"] in MUTABLE_META}
    texture_info = {}
    for role, expected_format, expected_dims, block_bytes, suffix in (
        ("main", "DXT5", (1024, 888), 16, ""), ("mask", "DXT1", (512, 444), 8, "_mask")
    ):
        t = n[role + "_texture"]
        meta = f"Assets/Source/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_1_101_1{suffix}.png.meta"
        need(t["role"] == role and t["asset_path"] + ".meta" == meta and
             t["guid"] == c["sprite_guid" if role == "main" else "mask_guid"] and
             t["source_meta_sha256"] == original_meta[meta] and
             t["project_meta_after_sha256"] == sha(NEW / meta),
             "Original/reimported texture metadata identity differs: " + role)
        need(t["format_before"] == t["format_after"] == expected_format and
             t["mips_before"] == t["mips_after"] == 1 and
             (t["width_before"], t["height_before"]) == expected_dims and
             (t["width_after"], t["height_after"]) == expected_dims and
             (t["filter_before"], t["filter_after"], t["wrap_before"], t["wrap_after"]) ==
             ("Bilinear", "Bilinear", "Clamp", "Clamp") and
             t["srgb_before"] and t["srgb_after"] and not t["readable_before"] and t["readable_after"] and
             t["block_bytes"] == block_bytes, "Compressed importer format/sampler state changed: " + role)
        expected_bytes = ((expected_dims[0] + 3) // 4) * ((expected_dims[1] + 3) // 4) * block_bytes
        artifact(run, t["compressed_blocks"], expected_bytes)
        for state in ("before", "after"):
            encoded = artifact(run, t[f"importer_{state}_json"])
            need(b"TextureImporter" in encoded and len(encoded) > 100, "Importer serialized state missing")
        texture_info[role] = {"compressed_blocks_sha256": t["compressed_blocks"]["sha256"],
                              "compressed_bytes": expected_bytes, "format": expected_format,
                              "importer_meta_before_sha256": original_meta[meta],
                              "importer_meta_after_sha256": t["project_meta_after_sha256"]}
    return {"schema": "rok.hall_camera_material_v2_native_verify.v1", "passed": True,
            "run": run.relative_to(ROOT).as_posix(), "native_sha256": sha(manifest),
            "contract_sha256": sha(CONTRACT), "v1_native_sha256": OLD_NATIVE_SHA,
            "cases_checked": 4, "v1_rgba_byte_exact_cases": 4, "png_raw_exact_cases": 4,
            "actual_world_sha256": n["actual_renderer_world_matrix"]["sha256"],
            "native3_world_sha256": hashlib.sha256(source_world).hexdigest(),
            "actual_world_byte_exact_native3": actual_world == source_world,
            "actual_world_max_element_delta": matrix_max_delta, "textures": texture_info,
            "scope": "isolated Unity importer compression and actual Hall world-matrix witness; GameApp/main-view false"}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prepared", action="store_true")
    parser.add_argument("--run", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    need(args.prepared or args.run, "Specify --prepared or --run")
    c = json.loads(CONTRACT.read_text(encoding="utf-8"))
    contract_check(c, prepared=args.prepared)
    if args.self_test:
        for mutation in (
            lambda value: value.update(schema="fake"),
            lambda value: value.update(v1_native_manifest_sha256="0" * 64),
            lambda value: value["input_files"][0].update(sha256="0" * 64),
        ):
            test = copy.deepcopy(c)
            mutation(test)
            try:
                contract_check(test, prepared=args.prepared)
            except AssertionError:
                continue
            raise AssertionError("Negative identity/input mutation escaped")
    if args.prepared:
        dll_sha = compile_exporter()
        report = {"schema": "rok.hall_camera_material_v2_prepared_verify.v1", "passed": True,
                  "contract_sha256": sha(CONTRACT), "exporter_sha256": sha(EXPORTER),
                  "compiler_dll_sha256": dll_sha, "negative_cases": 3 if args.self_test else 0,
                  "unity_started": False}
        destination = NEW / "Validation/prepared_verify_v2_revision2.json"
    else:
        report = verify_run(args.run, c)
        destination = args.run.resolve() / "verified_camera_material_v2.json"
    need(not destination.exists(), "Verification report already exists")
    destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
