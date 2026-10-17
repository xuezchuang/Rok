"""Independently rebuild 32 Unity-local typed buffers and verify 99 GPU mip refs."""

from __future__ import annotations

import hashlib
import json
import math
import os
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1"
MANIFEST = PACKAGE / "manifest.json"
INVENTORY_SHA = "fcbf6c325b8235df1a6852356fe5dc52a2c522963492da46fab8dd05c54dc543"


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise AssertionError(reason)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def repo_path(value: str) -> Path:
    path = (ROOT / value).resolve()
    require(path.is_relative_to(ROOT), f"Path escapes repo: {value}")
    return path


def disk_sha(value: str) -> str:
    return sha(repo_path(value).read_bytes())


def source_row_sha(row: dict) -> str:
    return sha(json.dumps(row, sort_keys=True, separators=(",", ":"),
                          ensure_ascii=False).encode("utf-8"))


def pack_float_rows(rows: list[dict], components: tuple[str, ...]) -> tuple[bytes, list[list[float]], float]:
    require(bool(rows), "Required source float array is empty")
    packed = bytearray()
    by_column: list[list[float]] = [[] for _ in components]
    max_error = 0.0
    for row in rows:
        require(set(row) == set(components), f"Float vector schema differs: {components}")
        for i, component in enumerate(components):
            value = row[component]
            require(type(value) in (float, int) and math.isfinite(value), "Nonfinite source float")
            encoded = struct.pack("<f", value)
            max_error = max(max_error, abs(struct.unpack("<f", encoded)[0] - value))
            packed += encoded
            by_column[i].append(value)
    return bytes(packed), [[min(column), max(column)] for column in by_column], max_error


def verify_binary(entry: dict, expected: bytes, count: int, stride: int,
                  components: tuple[str, ...] | None = None,
                  ranges: list[list[float]] | None = None,
                  roundtrip: float | None = None) -> None:
    path = repo_path(entry["path"])
    require(path.parent == PACKAGE and path.name.startswith("row_") and path.suffix == ".bin",
            "Geometry binary outside dedicated package")
    actual = path.read_bytes()
    require(entry["count"] == count and entry["stride_bytes"] == stride
            and entry["byte_count"] == len(expected) == len(actual) == count * stride,
            f"Typed buffer count/stride differs: {path.name}")
    require(actual == expected and entry["sha256"] == sha(actual),
            f"Typed bytes differ from sealed Unity source: {path.name}")
    if components is not None:
        require(entry["components"] == list(components)
                and entry["component_ranges"] == ranges, f"Component layout/ranges differ: {path.name}")
    if roundtrip is not None:
        require(entry["float32_roundtrip_max_abs_error"] == roundtrip and roundtrip <= 1e-6,
                f"Float32 source representation differs: {path.name}")


def verify() -> dict:
    package = json.loads(MANIFEST.read_text(encoding="utf-8"))
    require(package["schema"] == "rok.city_gamma_source_geometry_inputs.v1", "Package schema differs")
    require(package["producer_sha256"] == disk_sha("Scripts/PrepareRokCityGammaSourceGeometryInputs.py"),
            "Producer changed after package generation")
    require(package["counts"] == {"rows": 32, "masked_rows": 27, "family_rows": 5,
                                    "unique_texture_chains": 32, "unique_mips": 99,
                                    "logical_texture_slots": 68}, "Package counts differ")
    require("no GameApp world matrix/LOD" in package["qualification"],
            "Package overclaims runtime source state")
    for key, spec in package["source_files"].items():
        require(disk_sha(spec["path"]) == spec["sha256"], f"Pinned source changed: {key}")
    require(package["source_files"]["inventory"]["sha256"] == INVENTORY_SHA,
            "Frozen 32-row inventory differs")
    inventory = json.loads(repo_path(package["source_files"]["inventory"]["path"]).read_text(encoding="utf-8"))
    masked = json.loads(repo_path(package["source_files"]["masked_native"]["path"]).read_text(encoding="utf-8"))
    remaining = json.loads(repo_path(package["source_files"]["remaining_native"]["path"]).read_text(encoding="utf-8"))
    masked_assets = json.loads(repo_path(package["source_files"]["masked_assets"]["path"]).read_text(encoding="utf-8"))
    masked_ue = json.loads(repo_path(package["source_files"]["masked_ue_runtime"]["path"]).read_text(encoding="utf-8-sig"))
    family_input = json.loads(repo_path(package["source_files"]["family_rhi_inputs"]["path"]).read_text(encoding="utf-8"))
    family_verified = json.loads(repo_path(package["source_files"]["family_rhi_verified"]["path"]).read_text(encoding="utf-8"))
    require(family_verified["passed"] and family_verified["gpu_mips_byte_exact"] == 77,
            "77-mip GPU proof not strictly verified")
    require(len(package["rows"]) == len(inventory["rows"]) == 32
            and len(masked["rows"]) == 27 and len(remaining["rows"]) == 5,
            "32 source geometry rows differ")
    all_paths: set[str] = set()
    slot_count = 0
    exported_uv = 0
    colors_with_source = 0
    for index, entry in enumerate(package["rows"]):
        original = masked["rows"][index] if index < 27 else remaining["rows"][index - 27]
        summary = inventory["rows"][index]
        require(entry["index"] == index and entry["source_key"] == original["source_key"] == summary["source_key"],
                f"Row identity differs: {index}")
        require(entry["source_json_pointer"] == f"/rows/{index if index < 27 else index - 27}"
                and entry["source_json"] == package["source_files"]["masked_native" if index < 27 else "remaining_native"]["path"]
                and entry["source_row_sha256"] == source_row_sha(original),
                f"Source row pointer/hash differs: {index}")
        require(entry["texture_slots"] == summary["texture_slots"]
                and entry["source_queue"] == summary["material_runtime"]["queue"]
                and entry["source_material_state"] == summary["material_runtime"],
                f"Source material/texture binding changed: {index}")
        slot_count += len(entry["texture_slots"])
        mesh = original.get("mesh") if index >= 27 else None
        positions = original["sprite_vertices"] if mesh is None else mesh["vertices"]
        dims = ("x", "y") if mesh is None else ("x", "y", "z")
        data, bounds, error = pack_float_rows(positions, dims)
        verify_binary(entry["positions"], data, len(positions), 4 * len(dims), dims, bounds, error)
        all_paths.add(entry["positions"]["path"])
        expected_uv = ([{"channel": 0, "values": original["sprite_uv"], "fields": ("x", "y")}] if mesh is None
                       else [{"channel": channel["channel"], "values": channel["values"],
                              "fields": ("x", "y", "z", "w")} for channel in mesh["uv_channels"]])
        require(len(expected_uv) == len(entry["uv_channels"]), f"UV channel count differs: {index}")
        for actual, source in zip(entry["uv_channels"], expected_uv):
            require(actual["channel"] == source["channel"], f"UV channel identity differs: {index}")
            if not source["values"]:
                require(actual["status"] == "empty_in_unity_source_array" and actual["binary"] is None,
                        f"Source empty UV was synthesized: {index}")
                continue
            require(len(source["values"]) == len(positions) and actual["status"] == "exported",
                    f"Source UV count/status differs: {index}")
            data, bounds, error = pack_float_rows(source["values"], source["fields"])
            verify_binary(actual["binary"], data, len(source["values"]), 4 * len(source["fields"]),
                          source["fields"], bounds, error)
            all_paths.add(actual["binary"]["path"])
            exported_uv += 1
        colors = [] if mesh is None else mesh["colors"]
        if colors:
            require(len(colors) == len(positions) and entry["colors"]["status"] == "exported_unity_color32",
                    f"Source Color32 count/status differs: {index}")
            color_bytes = bytes(channel for color in colors for channel in
                                (color["r"], color["g"], color["b"], color["a"]))
            columns = [[color[key] for color in colors] for key in ("r", "g", "b", "a")]
            require(all(type(value) is int and 0 <= value <= 255 for col in columns for value in col),
                    f"Source Color32 invalid: {index}")
            bounds = [[min(col), max(col)] for col in columns]
            verify_binary(entry["colors"]["binary"], color_bytes, len(colors), 4,
                          ("r", "g", "b", "a"), bounds)
            all_paths.add(entry["colors"]["binary"]["path"])
            colors_with_source += 1
        else:
            require(entry["colors"]["binary"] is None and entry["colors"]["status"] == (
                "sprite_vertex_colors_not_exported" if mesh is None else "empty_in_unity_mesh_colors32_array"),
                f"Missing source Color32 was fabricated: {index}")
        source_submeshes = [original["sprite_triangles"]] if mesh is None else [s["indices"] for s in mesh["submeshes"]]
        index_bytes = bytearray()
        submesh_records = []
        for subindex, source_indices in enumerate(source_submeshes):
            require(source_indices and len(source_indices) % 3 == 0
                    and all(type(value) is int and 0 <= value < len(positions) for value in source_indices),
                    f"Triangle source range/topology invalid: {index}/{subindex}")
            offset = len(index_bytes) // 4
            index_bytes.extend(struct.pack(f"<{len(source_indices)}I", *source_indices))
            submesh_records.append({"submesh": subindex, "first_index": offset,
                                    "index_count": len(source_indices), "triangle_count": len(source_indices) // 3,
                                    "range": [min(source_indices), max(source_indices)],
                                    "source_topology": "Triangles" if mesh is None else mesh["submeshes"][subindex]["topology"]})
        require(entry["indices"]["submeshes"] == submesh_records, f"Submesh index ranges differ: {index}")
        verify_binary(entry["indices"]["binary"], bytes(index_bytes), len(index_bytes) // 4, 4)
        all_paths.add(entry["indices"]["binary"]["path"])
        require(entry["renderer_color"] == original["renderer_color"]
                and entry["prefab_renderer_local_to_world"] == original["renderer_local_to_world_in_prefab"]
                and entry["transform_qualification"].startswith("source prefab local"),
                f"Source local transform/color metadata differs: {index}")
        if mesh is None:
            require(entry["flips"] == {"x": original["flip_x"], "y": original["flip_y"]}
                    and entry["root_trs"] == {"position": original["root_local_position"],
                                              "rotation": original["root_local_rotation"],
                                              "scale": original["root_local_scale"]},
                    f"Masked local transform/flip differs: {index}")
        else:
            require(entry["flips"] is None and entry["root_trs"] == {
                "position": original["root_position"], "rotation": original["root_rotation"],
                "scale": original["root_scale"]}, f"Mesh local transform differs: {index}")
        require(entry["renderer_local_trs"] == {
            "position": original["renderer_local_position"],
            "rotation": original["renderer_local_rotation"],
            "scale": original["renderer_local_scale"]}, f"Renderer local TRS differs: {index}")
    require(slot_count == 68 and exported_uv == 32 and colors_with_source == 1,
            "68 slots, 32 present UV0 channels, one source Color32 array expected")
    refs = package["texture_chains"]
    inventory_chains = {item["source_guid"]: item for item in inventory["texture_chains"]}
    masked_by_guid = {}
    for asset in masked_assets["resources"]:
        for alias in asset["source_aliases"]:
            guid = alias["source_guid"]
            require(guid not in masked_by_guid or masked_by_guid[guid]["asset"] == asset["asset"],
                    f"Masked alias collision: {guid}")
            masked_by_guid[guid] = asset
    masked_gpu_by_asset = {item["asset"]: item for item in masked_ue["textures"]}
    family_by_guid = {item["source_guid"]: item for item in family_input["textures"]}
    family_runtime_path = repo_path(package["source_files"]["family_rhi_verified"]["path"]).parent / "manifest.json"
    require(sha(family_runtime_path.read_bytes()) == family_verified["runtime_manifest_sha256"],
            "Pinned 77-mip runtime report differs")
    family_runtime = json.loads(family_runtime_path.read_text(encoding="utf-8-sig"))
    family_gpu_by_key = {(item["texture_id"], item["level"]): item for item in family_runtime["mips"]}
    require(len(refs) == len(inventory_chains) == 32 and sum(item["mip_count"] for item in refs) == 99,
            "32 source chains / 99 unique mips differ")
    require(len(masked_by_guid) == len(masked_gpu_by_asset) == 22
            and len(family_by_guid) == 10 and len(family_gpu_by_key) == 77,
            "Pinned GPU source texture identities differ")
    mip_keys: set[tuple[str, int]] = set()
    default_filter_count = 0
    for chain in refs:
        guid = chain["source_guid"]
        source = inventory_chains[guid]
        require(chain["mip_count"] == source["mip_count"] == len(chain["levels"]),
                f"Source chain count differs: {guid}")
        if guid in masked_by_guid:
            require(chain["sampler_source"] == source["source_sampler_meta"],
                    f"Masked source sampler declaration changed: {guid}")
            if chain["sampler_source"]["serialized_filter_mode"] == -1:
                default_filter_count += 1
                require(chain["sampler_source"]["effective_filter"] == "default_filter_not_native_runtime_observed",
                        "Default source filter was silently promoted")
        else:
            require(chain["sampler_source"] == {"filter": source["sampler_filter"],
                                                 "wrap": source["sampler_wrap"]},
                    f"Family source sampler differs: {guid}")
        for level in chain["levels"]:
            key = (guid, level["level"])
            require(level["level"] in range(chain["mip_count"]) and key not in mip_keys,
                    f"Mip identity duplicate/out of range: {key}")
            mip_keys.add(key)
            if guid in masked_by_guid:
                asset = masked_by_guid[guid]
                gpu = masked_gpu_by_asset[asset["asset"]]
                expected_raw = (repo_path(package["source_files"]["masked_ue_runtime"]["path"]).parent
                                / gpu["canonical_bgra8_file"]).relative_to(ROOT).as_posix()
                require(level["level"] == 0 and level["ue_gpu_readback"] == expected_raw
                        and level["ue_gpu_readback_sha256"] == gpu["observed_bgra8_sha256"]
                        == asset["bgra8_pixel_sha256"]
                        and level["native_gpu_png"] == source["native_gpu_mip0_png"]
                        and level["native_gpu_png_sha256"] == source["native_gpu_mip0_png_sha256"]
                        and gpu["match"], f"Masked mip binding not proven: {key}")
            else:
                require(guid in family_by_guid, f"Unknown family texture GUID: {guid}")
                family = family_by_guid[guid]
                prepared = family["levels"][level["level"]]
                gpu = family_gpu_by_key[(family["texture_id"], level["level"])]
                expected_raw = (family_runtime_path.parent / gpu["gpu_readback_file"]).relative_to(ROOT).as_posix()
                require(level["prepared_bgra"] == prepared["prepared_bgra_file"]
                        and level["prepared_bgra_sha256"] == prepared["prepared_bgra_sha256"]
                        and level["ue_gpu_readback"] == expected_raw
                        and level["ue_gpu_readback_sha256"] == gpu["gpu_readback_bgra_sha256"]
                        == prepared["prepared_bgra_sha256"]
                        and gpu["source_guid"] == guid and gpu["byte_exact"],
                        f"Family mip binding not proven: {key}")
            require(disk_sha(level["ue_gpu_readback"]) == level["ue_gpu_readback_sha256"],
                    f"Proven GPU mip file changed: {key}")
            if "prepared_bgra" in level:
                require(disk_sha(level["prepared_bgra"]) == level["prepared_bgra_sha256"]
                        == level["ue_gpu_readback_sha256"], f"Family prepared/GPU mip differs: {key}")
            else:
                require(level["level"] == 0 and disk_sha(level["native_gpu_png"]) == level["native_gpu_png_sha256"],
                        f"Masked native GPU mip0 changed: {key}")
    require(len(mip_keys) == 99 and default_filter_count == 12,
            "All 99 source GPU mip refs and 12 sampler exceptions required")
    on_disk = {path.relative_to(ROOT).as_posix() for path in PACKAGE.glob("*.bin")}
    require(on_disk == all_paths, "Unexpected or missing geometry binary in dedicated package")
    return {"schema": "rok.city_gamma_source_geometry_inputs_verify.v1",
            "passed": True, "manifest_sha256": sha(MANIFEST.read_bytes()),
            "producer_sha256": package["producer_sha256"],
            "verifier_sha256": sha(Path(__file__).read_bytes()),
            "rows_checked": 32, "typed_binary_files_checked": len(all_paths),
            "logical_slots_checked": 68, "unique_gpu_mip_refs_rehashed": 99,
            "masked_effective_filter_unknown": 12,
            "qualification": "Unity-local typed geometry/source mip references only; no formal raster or actual GameApp world/LOD proof"}


def main() -> None:
    result = verify()
    encoded = (json.dumps(result, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    output = PACKAGE / "verified_geometry_inputs.json"
    if output.exists():
        require(output.read_bytes() == encoded, "Existing geometry verification output differs")
    else:
        with os.fdopen(os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as stream:
            stream.write(encoded)
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
