"""Independently verify one opt-in GameApp 22-texture GPU readback audit.

The raw `.bgra8` files, not the runtime manifest's pass flag, are the pixel
evidence. This validates inputs only; it does not accept any city rendering.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from rok_data_paths import resolve


ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / "Saved/RokUnityMigration/CityGammaInputsRuntime"
SOURCE = ROOT / "Saved/RokUnityMigration/CityGammaMaskedAssets/20261008T223046Z_11852/manifest.json"
SOURCE_SHA256 = "77df4b0aeb6ed4cc512eeea9b79b9037c95369ab4999afc5e8014b17b0f2ef62"
ASSET_ROOT = "/Game/RokUnity/City/GammaMaskedInputs/V1/"
EXPECTED = 22


def need(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            h.update(chunk)
    return h.hexdigest()


def read(path: Path) -> dict:
    need(path.is_file(), f"missing JSON: {path}")
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    need(isinstance(value, dict), f"invalid JSON root: {path}")
    return value


def verify(run: Path) -> None:
    run = run.resolve()
    need(run.is_dir() and run.parent == RUNS.resolve(), "run is outside the audit directory")
    need(digest(SOURCE) == SOURCE_SHA256, "sealed prepared input manifest SHA differs")
    source = read(SOURCE)
    need(source["schema"] == "rok.city_gamma_masked_inputs.prepared.v1" and
         source["destination_root"] == ASSET_ROOT[:-1] and
         source["counts"]["deduplicated_texture_assets"] == EXPECTED and
         len(source["resources"]) == EXPECTED,
         "prepared 22-texture input identity differs")
    resources = {item["resource_id"]: item for item in source["resources"]}
    need(len(resources) == EXPECTED, "prepared texture IDs are not unique")

    result = read(run / "manifest.json")
    need(result["schema"] == "rok.city_gamma_inputs.runtime_audit.v1" and
         Path(result["project"]).resolve() == ROOT.resolve() and
         result["map"] == "/Game/Maps/L_RokUnityGameAppCity" and
         result["input_manifest_sha256"] == SOURCE_SHA256 and
         result["input_manifest_relative"] ==
         "RokUnityMigration/CityGammaMaskedAssets/20261008T223046Z_11852/manifest.json" and
         result["local_only"] is True and result["raster_registered"] is False and
         result["scene_components_modified"] is False,
         "runtime project/map/input/read-only scope differs")
    for field, relative in (("audit_cpp_sha256", "Source/Rok/RokCityGammaInputAuditSubsystem.cpp"),
                            ("audit_h_sha256", "Source/Rok/RokCityGammaInputAuditSubsystem.h"),
                            ("verifier_sha256", "Scripts/VerifyRokCityGammaInputsRuntime.py")):
        need(result[field] == digest(resolve(relative)), f"runtime source changed: {relative}")
    need(result["expected_textures"] == EXPECTED and result["observed_textures"] == EXPECTED and
         result["complete"] is True and result["passed"] is True and result["failures"] == [] and
         len(result["textures"]) == EXPECTED,
         "runtime did not complete all 22 texture readbacks")
    observed = {item["resource_id"]: item for item in result["textures"]}
    need(len(observed) == EXPECTED and set(observed) == set(resources),
         "GPU readback resource set differs")
    need(result["finish_game_frame"] >= result["start_game_frame"],
         "runtime frame sequence differs")
    total_bytes = 0
    for key, item in observed.items():
        expected = resources[key]
        w, h = expected["width"], expected["height"]
        asset = ASSET_ROOT + key
        name = key + ".bgra8"
        need(item["asset"] == asset == expected["asset"] and
             item["role"] == expected["role"] and item["width"] == w and item["height"] == h and
             item["rhi_format"] == "PF_B8G8R8A8" and item["rhi_format_enum"] == 2 and
             item["rhi_mips"] == item["rhi_samples"] == 1 and item["rhi_srgb"] is False and
             item["staging_row_pitch_pixels"] >= w and item["staging_buffer_height"] >= h and
             item["canonical_file_row_stride_bytes"] == w * 4 and
             item["canonical_bytes"] == w * h * 4 and
             item["canonical_bgra8_file"] == name and item["prepared_png_sha256"] == expected["png_sha256"] and
             item["expected_bgra8_sha256"] == expected["bgra8_pixel_sha256"] and
             item["match"] is True,
             f"RHI descriptor or source contract differs: {key}")
        need(item["gpu_ready_render_frame"] >= item["gpu_copy_render_frame"] and
             item["gpu_ready_seconds"] >= item["gpu_copy_seconds"],
             f"GPU readiness chronology differs: {key}")
        path = (run / name).resolve()
        need(path.parent == run and path.is_file() and path.stat().st_size == w * h * 4,
             f"canonical BGRA8 file missing or wrong size: {key}")
        actual = digest(path)
        need(actual == item["observed_bgra8_sha256"] == expected["bgra8_pixel_sha256"],
             f"actual GPU BGRA8 bytes differ: {key}")
        uasset = ROOT / "Content" / (asset[len("/Game/"):] + ".uasset")
        need(uasset.is_file() and digest(uasset) == item["asset_uasset_sha256"],
             f"UE texture asset changed since readback: {key}")
        total_bytes += path.stat().st_size
    print(f"PASS: {EXPECTED} GameApp GPU BGRA8 input readbacks; {total_bytes} canonical bytes; run={run}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("run", type=Path)
    args = parser.parse_args()
    verify(args.run)


if __name__ == "__main__":
    main()
