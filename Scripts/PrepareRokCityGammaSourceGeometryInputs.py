"""Export the 32 sealed Unity-local geometry rows without inventing GameApp transforms.

Binary convention: little-endian IEEE754 float32 positions/UV, uint8 Color32,
uint32 triangle indices. Empty/missing Unity color arrays remain absent. Texture
bytes are referenced by their existing, independently GPU-verified paths.
"""

from __future__ import annotations

import hashlib
import json
import math
import os
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "Saved/RokUnityMigration/BatchReview20261009NightTimeAndExpedition/all32_source_rhi_admission_inventory.json"
INVENTORY_SHA = "fcbf6c325b8235df1a6852356fe5dc52a2c522963492da46fab8dd05c54dc543"
OUT = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1"
MASKED = ROOT / "Saved/UnityCityGammaMaskedSpritesProbeV2/OutputRuns/20261008_222155_307_21444/native_masked_sprites.json"
REMAINING = ROOT / "Saved/UnityCityGammaRemainingFamiliesProbe/OutputRuns/20261008_230313_337_7496/native_families.json"
STATE = ROOT / "Saved/UnityCityGammaFamilyStateProbeV2/OutputRuns/20261008_234412_007_44320/native_state.json"
MASKED_ASSETS = ROOT / "Saved/RokUnityMigration/CityGammaMaskedAssets/20261008T223046Z_11852/manifest.json"
MASKED_UE = ROOT / "Saved/RokUnityMigration/CityGammaInputsRuntime/20261008_224725_fff_9804_94417F61424EBCCEFE8009A01161AA0D/manifest.json"
FAMILY_INPUT = ROOT / "Saved/RokUnityMigration/CityGammaFamilyRhiInputs/v1/manifest.json"
FAMILY_VERIFIED = ROOT / "Saved/RokUnityMigration/CityGammaFamilyInputAudit/20261009_004518_fff_42116_19E8E161405730A0EF1C76A8B2832653/verified_family_input.json"


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_sha(path: Path) -> str:
    return sha(path.read_bytes())


def canonical(value: object) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def fail_if(condition: bool, message: str) -> None:
    if condition:
        raise ValueError(message)


def immutable(path: Path, content: bytes) -> None:
    if path.exists():
        fail_if(path.read_bytes() != content, f"Prepared output differs: {path}")
        return
    with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as stream:
        stream.write(content)


def relative(path: Path) -> str:
    path = path.resolve()
    fail_if(not path.is_relative_to(ROOT), f"Outside repo: {path}")
    return path.relative_to(ROOT).as_posix()


def validate_source(path: Path, expected_sha: str) -> dict:
    actual = file_sha(path)
    fail_if(actual != expected_sha, f"Pinned source SHA differs: {path}")
    return {"path": relative(path), "sha256": actual}


def f32_buffer(vectors: list[dict], components: tuple[str, ...]) -> tuple[bytes, list[list[float]], float]:
    fail_if(not vectors, "Required source vector array is empty")
    output = bytearray()
    ranges = [[] for _ in components]
    max_error = 0.0
    for vector in vectors:
        fail_if(set(vector) != set(components), "Source vector component names differ")
        for index, component in enumerate(components):
            value = vector[component]
            fail_if(isinstance(value, bool) or not isinstance(value, (int, float))
                    or not math.isfinite(value), "Source vector is nonfinite/non-numeric")
            encoded = struct.pack("<f", value)
            decoded = struct.unpack("<f", encoded)[0]
            max_error = max(max_error, abs(decoded - value))
            ranges[index].append(value)
            output.extend(encoded)
    fail_if(max_error > 1e-6, "Source vector cannot be represented as its Unity float32 value")
    return bytes(output), [[min(values), max(values)] for values in ranges], max_error


def binary_record(row_index: int, kind: str, data: bytes, count: int, stride: int,
                  components: tuple[str, ...] | None = None,
                  ranges: list[list[float]] | None = None, max_error: float | None = None) -> dict:
    fail_if(len(data) != count * stride, f"Binary stride/count mismatch: {row_index} {kind}")
    name = f"row_{row_index:02d}_{kind}.bin"
    path = OUT / name
    immutable(path, data)
    record = {"path": relative(path), "sha256": sha(data), "count": count,
              "stride_bytes": stride, "byte_count": len(data)}
    if components is not None:
        record["components"] = list(components)
        record["component_ranges"] = ranges
        if max_error is not None:
            record["float32_roundtrip_max_abs_error"] = max_error
    return record


def color_buffer(colors: list[dict]) -> tuple[bytes, list[list[int]]]:
    output = bytearray()
    ranges = [[] for _ in range(4)]
    for color in colors:
        fail_if(set(color) != {"r", "g", "b", "a"}, "Unity Color32 component names differ")
        for index, name in enumerate(("r", "g", "b", "a")):
            value = color[name]
            fail_if(type(value) is not int or not 0 <= value <= 255, "Unity Color32 component invalid")
            output.append(value)
            ranges[index].append(value)
    return bytes(output), [[min(a), max(a)] for a in ranges]


def index_buffer(indices: list[int], vertices: int) -> tuple[bytes, list[int]]:
    fail_if(not indices or len(indices) % 3 != 0, "Source triangles missing or nontriangular")
    fail_if(any(type(index) is not int or index < 0 or index >= vertices for index in indices),
            "Source triangle index outside vertex range")
    return b"".join(struct.pack("<I", index) for index in indices), [min(indices), max(indices)]


def mip_references(inventory: dict, masked_assets: dict, masked_ue: dict,
                   family_input: dict, family_verified: dict) -> list[dict]:
    alias_to_asset = {}
    for asset in masked_assets["resources"]:
        for alias in asset["source_aliases"]:
            guid = alias["source_guid"]
            fail_if(guid in alias_to_asset and alias_to_asset[guid]["asset"] != asset["asset"],
                    f"Masked source GUID maps to multiple UE assets: {guid}")
            alias_to_asset[guid] = asset
    ue_by_asset = {item["asset"]: item for item in masked_ue["textures"]}
    fail_if(len(alias_to_asset) != 22 or len(ue_by_asset) != 22,
            "Masked source/UE chain count differs")
    family_runtime_path = FAMILY_VERIFIED.parent / "manifest.json"
    fail_if(file_sha(family_runtime_path) != family_verified["runtime_manifest_sha256"],
            "77-mip runtime manifest SHA differs from strict verification")
    family_runtime = json.loads(family_runtime_path.read_text(encoding="utf-8-sig"))
    fail_if(not family_runtime["passed"] or len(family_runtime["mips"]) != 77,
            "Family RHI runtime did not complete 77 GPU mips")
    gpu_levels = {(item["texture_id"], item["level"]): item for item in family_runtime["mips"]}
    fail_if(len(gpu_levels) != 77, "Family GPU mip keys duplicated")
    family_by_guid = {item["source_guid"]: item for item in family_input["textures"]}
    fail_if(len(family_by_guid) != 10, "Family source GUIDs duplicated")
    result = []
    for chain in inventory["texture_chains"]:
        guid = chain["source_guid"]
        if guid in alias_to_asset:
            resource = alias_to_asset[guid]
            gpu = ue_by_asset[resource["asset"]]
            raw = MASKED_UE.parent / gpu["canonical_bgra8_file"]
            fail_if(not gpu["match"] or file_sha(raw) != resource["bgra8_pixel_sha256"]
                    or gpu["observed_bgra8_sha256"] != resource["bgra8_pixel_sha256"],
                    f"Masked source GPU mip0 unproven: {guid}")
            refs = [{"level": 0, "ue_gpu_readback": relative(raw),
                     "ue_gpu_readback_sha256": file_sha(raw),
                     "native_gpu_png": chain["native_gpu_mip0_png"],
                     "native_gpu_png_sha256": chain["native_gpu_mip0_png_sha256"]}]
        else:
            fail_if(guid not in family_by_guid, f"Unknown texture chain: {guid}")
            source = family_by_guid[guid]
            refs = []
            for level in source["levels"]:
                gpu = gpu_levels[(source["texture_id"], level["level"])]
                raw = FAMILY_VERIFIED.parent / gpu["gpu_readback_file"]
                prepared = ROOT / level["prepared_bgra_file"]
                fail_if(not gpu["byte_exact"] or file_sha(raw) != level["prepared_bgra_sha256"]
                        or file_sha(prepared) != level["prepared_bgra_sha256"],
                        f"Family source mip GPU bytes differ: {guid}/{level['level']}")
                refs.append({"level": level["level"], "prepared_bgra": level["prepared_bgra_file"],
                             "prepared_bgra_sha256": level["prepared_bgra_sha256"],
                             "ue_gpu_readback": relative(raw),
                             "ue_gpu_readback_sha256": file_sha(raw)})
        fail_if(len(refs) != chain["mip_count"], f"Texture chain mip count differs: {guid}")
        result.append({"source_guid": guid, "mip_count": len(refs), "levels": refs,
                       "sampler_source": chain.get("source_sampler_meta", {
                           "filter": chain.get("sampler_filter"), "wrap": chain.get("sampler_wrap")})})
    fail_if(len(result) != 32 or sum(item["mip_count"] for item in result) != 99,
            "32 source chains / 99 unique GPU mip references differ")
    return result


def main() -> None:
    fail_if(file_sha(INVENTORY) != INVENTORY_SHA, "Frozen 32-row inventory SHA differs")
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    fail_if(inventory["schema"] != "rok.all32_source_rhi_admission_inventory.v1"
            or inventory["counts"]["all_unique_mips"] != 99,
            "Frozen inventory schema/count differs")
    fail_if(sum(t.get("source_sampler_meta", {}).get("serialized_filter_mode") == -1
                for t in inventory["texture_chains"]) != 12,
            "Twelve masked default-filter exceptions not preserved")
    source_files = {"inventory": validate_source(INVENTORY, INVENTORY_SHA)}
    for key, path in (("masked_native", MASKED), ("remaining_native", REMAINING),
                      ("family_state_native", STATE), ("masked_assets", MASKED_ASSETS),
                      ("masked_ue_runtime", MASKED_UE), ("family_rhi_inputs", FAMILY_INPUT),
                      ("family_rhi_verified", FAMILY_VERIFIED)):
        source_files[key] = validate_source(path, inventory["sources"][relative(path)])
    masked = json.loads(MASKED.read_text(encoding="utf-8"))
    remaining = json.loads(REMAINING.read_text(encoding="utf-8"))
    family_state = json.loads(STATE.read_text(encoding="utf-8"))
    fail_if(not masked["complete"] or not remaining["complete"] or not family_state["complete"]
            or len(masked["rows"]) != 27 or len(remaining["rows"]) != len(family_state["rows"]) != 5,
            "32 sealed Unity geometry rows unavailable")
    fail_if([r["source_key"] for r in remaining["rows"]]
            != [r["source_key"] for r in family_state["rows"]],
            "Remaining family geometry/state keys differ")
    OUT.mkdir(parents=True, exist_ok=True)
    rows = []
    for index, summary in enumerate(inventory["rows"]):
        source = masked["rows"][index] if index < 27 else remaining["rows"][index - 27]
        fail_if(summary["index"] != index or summary["source_key"] != source["source_key"]
                or summary["source_identity"]["source_native_row_sha256"] != sha(canonical(source)),
                f"Native geometry row identity differs: {index}")
        mesh = source["mesh"] if index >= 27 else None
        positions = source["sprite_vertices"] if mesh is None else mesh["vertices"]
        dimensions = ("x", "y") if mesh is None else ("x", "y", "z")
        data, bounds, quant = f32_buffer(positions, dimensions)
        record = {"index": index, "source_key": source["source_key"],
                  "source_group": "masked_sprite" if mesh is None else "remaining_family_mesh",
                  "source_json": relative(MASKED if mesh is None else REMAINING),
                  "source_json_pointer": f"/rows/{index if mesh is None else index - 27}",
                  "source_row_sha256": sha(canonical(source)),
                  "positions": binary_record(index, "positions_f32le", data, len(positions),
                                             len(dimensions) * 4, dimensions, bounds, quant),
                  "uv_channels": [], "colors": None, "indices": None,
                  "renderer_color": source["renderer_color"],
                  "flips": {"x": source["flip_x"], "y": source["flip_y"]} if mesh is None else None,
                  "root_trs": {"position": source["root_local_position"],
                               "rotation": source["root_local_rotation"],
                               "scale": source["root_local_scale"]} if mesh is None else {
                               "position": source["root_position"], "rotation": source["root_rotation"],
                               "scale": source["root_scale"]},
                  "renderer_local_trs": {"position": source["renderer_local_position"],
                                         "rotation": source["renderer_local_rotation"],
                                         "scale": source["renderer_local_scale"]},
                  "prefab_renderer_local_to_world": source["renderer_local_to_world_in_prefab"],
                  "transform_qualification": "source prefab local only; actual GameApp matrix and LOD unobserved",
                  "source_queue": summary["material_runtime"]["queue"],
                  "source_material": summary["source_identity"]["material"],
                  "source_shader": summary["source_identity"]["shader"],
                  "source_material_state": summary["material_runtime"],
                  "texture_slots": summary["texture_slots"]}
        if mesh is None:
            uv_source = [{"channel": 0, "values": source["sprite_uv"], "components": ("x", "y")}]
        else:
            uv_source = [{"channel": u["channel"], "values": u["values"],
                          "components": ("x", "y", "z", "w")} for u in mesh["uv_channels"]]
        for channel in uv_source:
            values = channel["values"]
            fail_if(values and len(values) != len(positions),
                    f"UV channel count differs from vertices: {index}/{channel['channel']}")
            if not values:
                record["uv_channels"].append({"channel": channel["channel"],
                                               "status": "empty_in_unity_source_array", "binary": None})
                continue
            uv_bytes, uv_bounds, uv_quant = f32_buffer(values, channel["components"])
            record["uv_channels"].append({"channel": channel["channel"], "status": "exported",
                "binary": binary_record(index, f"uv{channel['channel']}_f32le", uv_bytes,
                                        len(values), len(channel["components"]) * 4,
                                        channel["components"], uv_bounds, uv_quant)})
        colors = [] if mesh is None else mesh["colors"]
        if colors:
            fail_if(len(colors) != len(positions), f"Unity Color32 count differs: {index}")
            color_bytes, color_range = color_buffer(colors)
            record["colors"] = {"status": "exported_unity_color32", "binary": binary_record(
                index, "colors_u8x4", color_bytes, len(colors), 4, ("r", "g", "b", "a"), color_range)}
        else:
            record["colors"] = {"status": "sprite_vertex_colors_not_exported" if mesh is None
                                  else "empty_in_unity_mesh_colors32_array", "binary": None}
        submeshes = [source["sprite_triangles"]] if mesh is None else [s["indices"] for s in mesh["submeshes"]]
        fail_if(not submeshes, f"No source triangles: {index}")
        packed = bytearray()
        ranges = []
        for subindex, indices in enumerate(submeshes):
            encoded, index_range = index_buffer(indices, len(positions))
            offset = len(packed) // 4
            packed.extend(encoded)
            ranges.append({"submesh": subindex, "first_index": offset,
                           "index_count": len(indices), "triangle_count": len(indices) // 3,
                           "range": index_range,
                           "source_topology": "Triangles" if mesh is None else mesh["submeshes"][subindex]["topology"]})
        record["indices"] = {"binary": binary_record(index, "indices_u32le", bytes(packed),
                                                      len(packed) // 4, 4), "submeshes": ranges}
        rows.append(record)
    masked_assets = json.loads(MASKED_ASSETS.read_text(encoding="utf-8"))
    masked_ue = json.loads(MASKED_UE.read_text(encoding="utf-8-sig"))
    family_input = json.loads(FAMILY_INPUT.read_text(encoding="utf-8"))
    family_verified = json.loads(FAMILY_VERIFIED.read_text(encoding="utf-8"))
    refs = mip_references(inventory, masked_assets, masked_ue, family_input, family_verified)
    fail_if({r["source_guid"] for r in refs}
            != {t["source_guid"] for t in inventory["texture_chains"]},
            "Texture chain GUID coverage differs")
    fail_if(sum(len(r["texture_slots"]) for r in rows) != 68,
            "68 logical texture slots not preserved")
    manifest = {"schema": "rok.city_gamma_source_geometry_inputs.v1",
                "qualification": "Unity-local source arrays and independently proven input mip references only; no GameApp world matrix/LOD, formal Raster registration or visual acceptance",
                "producer_sha256": file_sha(Path(__file__)),
                "source_files": source_files,
                "counts": {"rows": 32, "masked_rows": 27, "family_rows": 5,
                           "unique_texture_chains": 32, "unique_mips": 99,
                           "logical_texture_slots": 68},
                "binary_convention": {"positions_uv": "IEEE754 float32 little-endian, Unity local components untransformed",
                                      "colors": "Unity Color32 uint8 RGBA, only when source array exists",
                                      "indices": "uint32 little-endian, original order; source index width unobserved",
                                      "storage": "tight arrays with no header; stride/count in each row"},
                "rows": rows, "texture_chains": refs,
                "known_unqualified": ["Twelve masked source metas use filterMode=-1, so effective Unity sampler filter is unobserved despite UE Bilinear policy.",
                                      "27 SpriteRenderer rows lack an exported per-vertex color array; renderer tint is preserved separately and no white vertices were synthesized.",
                                      "Four of five remaining meshes have empty Unity Color32 arrays; no replacement color data was generated.",
                                      "Unity built-in mesh/shader source files for three references cannot be hashed from readonly ref; native extracted arrays are pinned instead.",
                                      "All transforms are prefab-local; no actual GameApp matrix, camera, sorting/LOD frame or UE production caller is qualified."]}
    immutable(OUT / "manifest.json", (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    print(json.dumps({"manifest": relative(OUT / "manifest.json"),
                      "sha256": file_sha(OUT / "manifest.json"), "counts": manifest["counts"]}))


if __name__ == "__main__":
    main()
