"""Strictly verify one opt-in UE RHI family-mip readback run.

This checks all GPU readback byte files against the pinned Unity-derived BGRA
inputs. A descriptor or a runtime `passed` flag alone is never acceptance.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
PREPARED = ROOT / "Saved/RokUnityMigration/CityGammaFamilyRhiInputs/v1/manifest.json"
PREPARED_SHA = "b02d0568745fd0c26b073202496028bf2d81c2e0ae9dffef2e18083782b101e2"
OUTPUT_ROOT = (ROOT / "Saved/RokUnityMigration/CityGammaFamilyInputAudit").resolve()
NATIVE_SHA = "671f18f91275fc0a5a65107eff5e44857c59310873036293b99665f84c391b71"
CONTRACT_SHA = "9555b7e77ff67dad29789923109cde5574358dbc4cc7072e174b0f3987f27a49"


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_sha(path: Path) -> str:
    return sha(path.read_bytes())


def check(value: bool, message: str) -> None:
    if not value:
        raise AssertionError(message)


def repo_file(relative: str) -> Path:
    check(relative.startswith(("Saved/UnityCityGammaFamilyStateProbeV2/",
                               "Saved/RokUnityMigration/CityGammaFamilyRhiInputs/v1/",
                               "ref/client-unity/")), f"not an allowlisted input: {relative}")
    path = (resolve(relative)).resolve()
    check(path.is_relative_to(ROOT), f"input escapes repo: {relative}")
    return path


def verify(run: Path) -> dict:
    check(run.is_dir() and run.parent == OUTPUT_ROOT, "run must be one unique audit child")
    check(file_sha(PREPARED) == PREPARED_SHA, "prepared manifest SHA differs")
    prepared = json.loads(PREPARED.read_text(encoding="utf-8"))
    runtime_path = run / "manifest.json"
    runtime = json.loads(runtime_path.read_text(encoding="utf-8-sig"))
    check(prepared["schema"] == "rok.city_gamma_family_rhi_inputs.v1", "input schema differs")
    check(runtime["schema"] == "rok.city_gamma_family_input_runtime.v1", "runtime schema differs")
    check(runtime["complete"] and runtime["passed"] and not runtime["failures"], "runtime not complete/passed")
    check(Path(runtime["project"]).resolve() == ROOT and runtime["map"] == "/Game/Maps/L_RokUnityGameAppCity",
          "project or map identity differs")
    check(runtime["prepared_input_path"] == "Saved/RokUnityMigration/CityGammaFamilyRhiInputs/v1/manifest.json"
          and runtime["prepared_input_sha256"] == PREPARED_SHA, "prepared identity differs")
    check(runtime["native_state_sha256"] == NATIVE_SHA and runtime["state_contract_sha256"] == CONTRACT_SHA
          and runtime["native_verify_sha256"] == prepared["native_verify_sha256"], "native source chain differs")
    for key, path in (("native_state_sha256", prepared["native_state_path"]),
                      ("state_contract_sha256", prepared["state_contract_path"]),
                      ("native_verify_sha256", prepared["native_verify_path"])):
        check(file_sha(repo_file(path)) == prepared[key], f"source chain file differs: {key}")
    for key, rel in (("audit_cpp_sha256", "Source/Rok/RokCityGammaFamilyInputAuditSubsystem.cpp"),
                     ("audit_h_sha256", "Source/Rok/RokCityGammaFamilyInputAuditSubsystem.h"),
                     ("verifier_sha256", "Scripts/VerifyRokCityGammaFamilyInputRuntime.py")):
        check(runtime[key] == file_sha(resolve(rel)), f"producer/verifier changed after capture: {rel}")
    check(runtime["opt_in_flag"] is True and runtime["local_only"] is True
          and runtime["scene_mutated"] is False and runtime["family_raster_registered"] is False,
          "opt-in/local diagnostic boundary differs")
    check(runtime["rhi_srgb"] is False and runtime["mips_generated_by_ue"] is False,
          "RHI input must be explicit non-SRGB source mips")
    check(runtime["readback_method"] == "explicit_source_mip_copy_to_single_mip_texture",
          "GPU mip selection proof is missing")
    check(runtime["http_dispatch_before"] == runtime["http_dispatch_after"] == 0,
          "HTTP dispatch attempted")
    check(runtime["expected_textures"] == runtime["observed_textures"] == 10
          and runtime["expected_mips"] == runtime["observed_mips"] == 77
          and runtime["family_slot_mip_references"] == 110,
          "10/77/110 accounting differs")
    check(len(prepared["textures"]) == len(runtime["textures"]) == 10, "texture rows differ")
    check(len(runtime["mips"]) == 77, "mip rows differ")
    texture_map = {t["texture_id"]: t for t in prepared["textures"]}
    check(len(texture_map) == 10, "prepared texture IDs not unique")
    for actual in runtime["textures"]:
        expected = texture_map.get(actual["texture_id"])
        check(expected is not None, f"unknown texture {actual['texture_id']}")
        for key in ("source_guid", "source_repo_path", "source_png_sha256", "source_filter", "source_wrap",
                    "width", "height", "mip_count"):
            check(actual[key] == expected[key], f"texture metadata mismatch: {actual['texture_id']} {key}")
        check(actual["rhi_format"] == "PF_B8G8R8A8" and actual["rhi_srgb"] is False
              and actual["mips_generated_by_ue"] is False, "texture RHI format/creation differs")
        check(file_sha(repo_file(expected["source_repo_path"])) == expected["source_png_sha256"],
              f"source PNG changed: {expected['texture_id']}")
    expected_keys = {(t["texture_id"], m["level"]) for t in prepared["textures"] for m in t["levels"]}
    check(len(expected_keys) == 77, "prepared mip identities not unique")
    actual_keys: set[tuple[str, int]] = set()
    for actual in runtime["mips"]:
        key = (actual["texture_id"], actual["level"])
        check(key in expected_keys and key not in actual_keys, f"unexpected or duplicate mip: {key}")
        actual_keys.add(key)
        texture = texture_map[key[0]]
        expected = texture["levels"][key[1]]
        for field in ("width", "height"):
            check(actual[field] == expected[field], f"GPU mip extent differs: {key} {field}")
        check(actual["source_guid"] == texture["source_guid"] and actual["byte_count"] == expected["byte_count"],
              f"GPU mip identity/bytes differ: {key}")
        check(actual["expected_bgra_sha256"] == expected["prepared_bgra_sha256"],
              f"declared expected SHA differs: {key}")
        check(file_sha(repo_file(expected["source_rgba_file"])) == expected["source_rgba_sha256"]
              and file_sha(repo_file(expected["source_png_file"])) == expected["source_png_sha256"],
              f"Unity mip source changed: {key}")
        check(actual["rhi_mip_count"] == texture["mip_count"] and actual["rhi_samples"] == 1
              and actual["rhi_srgb"] is False and actual["byte_exact"] is True,
              f"GPU mip descriptor/byte proof differs: {key}")
        check(actual["readback_method"] == "explicit_source_mip_copy_to_single_mip_texture"
              and actual["copy_source_mip_index"] == key[1]
              and actual["copy_dest_mip_index"] == actual["readback_source_slice_index"] == 0,
              f"explicit GPU source mip copy route differs: {key}")
        check(actual["staging_row_pitch_pixels"] >= actual["width"]
              and actual["staging_buffer_height"] >= actual["height"]
              and actual["gpu_ready_frame"] >= actual["gpu_copy_frame"] > 0,
              f"GPU staging evidence invalid: {key}")
        gpu_name = actual["gpu_readback_file"]
        check(gpu_name == f"{key[0]}_mip_{key[1]:02d}.readback.bgra8.bin", f"GPU file identity differs: {key}")
        gpu = (run / gpu_name).read_bytes()
        prepared_bytes = repo_file(expected["prepared_bgra_file"]).read_bytes()
        check(len(gpu) == actual["byte_count"] == len(prepared_bytes), f"readback length differs: {key}")
        check(gpu == prepared_bytes and sha(gpu) == actual["gpu_readback_bgra_sha256"]
              == expected["prepared_bgra_sha256"], f"GPU bytes differ: {key}")
    check(actual_keys == expected_keys, "not every prepared mip was GPU read back")
    references = sum(texture_map[s["texture_id"]]["mip_count"]
                     for family in prepared["families"] for s in family["textures"])
    check(references == 110 and prepared["counts"] == {
        "families": 5, "textures": 10, "unique_mips": 77, "source_row_mip_references": 110},
        "prepared family slot accounting differs")
    return {
        "schema": "rok.city_gamma_family_input_verify.v1",
        "passed": True,
        "runtime_manifest_sha256": file_sha(runtime_path),
        "prepared_manifest_sha256": PREPARED_SHA,
        "native_state_sha256": NATIVE_SHA,
        "state_contract_sha256": CONTRACT_SHA,
        "textures_verified": 10,
        "gpu_mips_byte_exact": 77,
        "family_slot_mip_references": 110,
        "qualification": "Diagnostic RHI mip upload/readback only; five-family raster and production registration remain separate",
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("run", type=Path, help="unique Saved/RokUnityMigration/CityGammaFamilyInputAudit child")
    args = parser.parse_args()
    run = args.run.resolve()
    result = verify(run)
    out = run / "verified_family_input.json"
    encoded = (json.dumps(result, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if out.exists():
        check(out.read_bytes() == encoded, "existing verification result differs")
    else:
        fd = os.open(out, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
        with os.fdopen(fd, "wb") as stream:
            stream.write(encoded)
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
