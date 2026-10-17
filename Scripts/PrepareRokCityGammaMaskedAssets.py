"""Prepare immutable, source-guarded GPU texture inputs for city Gamma raster work.

This is offline only. It copies exact Unity 2019 GPU-exported RGBA PNGs after
checking the sealed native contract; it does not import UE assets or claim a
GameApp renderer packet. Run with --prepare, then pass its manifest to the UE
asset creator. --verify is a read-only recheck of a prepared run.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import struct
import zlib
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

import VerifyRokUnityCityGammaMaskedSpritesNative as native_verify


ROOT = Path(__file__).resolve().parents[1]
NATIVE_RUN = ROOT / "Saved/UnityCityGammaMaskedSpritesProbeV2/OutputRuns/20261008_222155_307_21444"
NATIVE_JSON = NATIVE_RUN / "native_masked_sprites.json"
NATIVE_SHA256 = "11e4e5ebf6dba9a42633a91de949a2460f2df8fc4932dfaa499039b0d4485e16"
CONTRACT = ROOT / "Saved/UnityCityGammaMaskedSpritesProbeV2/source_contract.json"
CONTRACT_SHA256 = "1673889968a47b824bc731367bd5741e1939ca9614825ac9fd31e6db559b5ecc"
OUTPUT = ROOT / "Saved/RokUnityMigration/CityGammaMaskedAssets"
DEST = "/Game/RokUnity/City/GammaMaskedInputs/V1"
SCHEMA = "rok.city_gamma_masked_inputs.prepared.v1"
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"


def need(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha_file(path: Path) -> str:
    return sha(path.read_bytes())


def read(path: Path) -> dict:
    result = json.loads(path.read_text(encoding="utf-8-sig"))
    need(isinstance(result, dict), f"JSON root differs: {path}")
    return result


def decode_rgba8_png(path: Path) -> tuple[int, int, bytes]:
    """Decode only Unity's 8-bit non-interlaced RGBA PNG, checking every CRC."""
    data = path.read_bytes()
    need(data.startswith(PNG_MAGIC), f"not PNG: {path}")
    offset = len(PNG_MAGIC)
    ihdr = None
    compressed = bytearray()
    ended = False
    while offset + 12 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        start = offset + 8
        end = start + length
        need(end + 4 <= len(data), f"truncated PNG: {path}")
        chunk = data[start:end]
        crc = struct.unpack_from(">I", data, end)[0]
        need(zlib.crc32(kind + chunk) & 0xffffffff == crc, f"PNG CRC differs: {path}")
        if kind == b"IHDR":
            need(ihdr is None and length == 13, f"PNG IHDR differs: {path}")
            ihdr = struct.unpack(">IIBBBBB", chunk)
        elif kind == b"IDAT":
            compressed.extend(chunk)
        elif kind == b"IEND":
            need(length == 0, f"PNG IEND differs: {path}")
            ended = True
            offset = end + 4
            break
        offset = end + 4
    need(ended and offset == len(data) and ihdr is not None and compressed,
         f"PNG chunks incomplete: {path}")
    width, height, depth, color, compression, filtering, interlace = ihdr
    need(width > 0 and height > 0 and (depth, color, compression, filtering, interlace) ==
         (8, 6, 0, 0, 0), f"GPU PNG is not non-interlaced RGBA8: {path}")
    pitch = width * 4
    raw = zlib.decompress(bytes(compressed))
    need(len(raw) == height * (pitch + 1), f"PNG scanline size differs: {path}")
    rgba = bytearray(height * pitch)
    previous = bytearray(pitch)
    for y in range(height):
        pos = y * (pitch + 1)
        mode = raw[pos]
        need(mode <= 4, f"PNG filter differs: {path}")
        row = bytearray(raw[pos + 1:pos + 1 + pitch])
        for x in range(pitch):
            left = row[x - 4] if x >= 4 else 0
            up = previous[x]
            upper_left = previous[x - 4] if x >= 4 else 0
            if mode == 1:
                predictor = left
            elif mode == 2:
                predictor = up
            elif mode == 3:
                predictor = (left + up) // 2
            elif mode == 4:
                p = left + up - upper_left
                distances = (abs(p - left), abs(p - up), abs(p - upper_left))
                predictor = (left, up, upper_left)[distances.index(min(distances))]
            else:
                predictor = 0
            row[x] = (row[x] + predictor) & 0xff
        rgba[y * pitch:(y + 1) * pitch] = row
        previous = row
    return width, height, bytes(rgba)


def mesh_digest(row: dict) -> tuple[str, int]:
    vertices, uv, indices = row["sprite_vertices"], row["sprite_uv"], row["sprite_triangles"]
    need(len(vertices) == len(uv) and len(indices) % 3 == 0 and
         all(type(i) is int and 0 <= i < len(vertices) for i in indices),
         f"source mesh topology differs: {row['source_key']}")
    # Canonical SOURCE-local 2D float32 -> (x,y,0,u,v), then little-endian
    # uint32 indices. This is not a UE LocalCm packet or a newly approved model.
    payload = bytearray()
    for vertex, texel in zip(vertices, uv):
        payload.extend(struct.pack("<5f", vertex["x"], vertex["y"], 0.0,
                                   texel["x"], texel["y"]))
    for index in indices:
        payload.extend(struct.pack("<I", index))
    return sha(bytes(payload)), len(payload)


def source_rows() -> list[dict]:
    need(sha_file(NATIVE_JSON) == NATIVE_SHA256 and sha_file(CONTRACT) == CONTRACT_SHA256,
         "sealed native/contract SHA differs")
    native_verify.verify(NATIVE_RUN)  # Rechecks closure, source files, GUIDs and all 54 PNGs.
    report = read(NATIVE_JSON)
    need(report["schema"] == "rok.city_gamma_masked_sprites_native.v1" and
         report["complete"] is True and report["color_space"] == "Gamma" and
         report["game_app_loaded"] is False and len(report["rows"]) == 27 and
         Counter(row["category"] for row in report["rows"]) ==
         {"initial_source_building": 23, "city_gate_flag": 2, "dynamic_local_new": 2},
         "native identity or 27-row scope differs")
    return sorted(report["rows"], key=lambda item: item["source_key"])


def payload(directory: Path) -> dict:
    rows = source_rows()
    resources: dict[tuple[str, int, int, str], dict] = {}
    outputs = []
    for row in rows:
        digest, byte_count = mesh_digest(row)
        bindings = {}
        for role in ("sprite", "mask"):
            source_name = row[role + "_gpu_png"]
            source = NATIVE_RUN / source_name
            width, height, rgba = decode_rgba8_png(source)
            need([width, height] == [row[role + "_texture_width"], row[role + "_texture_height"]],
                 f"GPU PNG size differs: {row['source_key']}:{role}")
            bgra = bytearray(rgba)
            bgra[0::4], bgra[2::4] = rgba[2::4], rgba[0::4]
            content_sha = sha(bytes(bgra))
            key = (role, width, height, content_sha)
            if key not in resources:
                token = sha(f"{role}:{width}x{height}:{content_sha}".encode("ascii"))[:20]
                name = f"T_GammaMasked_{role.title()}_{token}"
                resources[key] = {
                    "role": role, "width": width, "height": height,
                    "bgra8_pixel_sha256": content_sha, "rgba8_pixel_sha256": sha(rgba),
                    "resource_id": name, "asset": f"{DEST}/{name}",
                    "png": f"textures/{name}.png", "png_sha256": sha_file(source),
                    "representative_native_png": source_name, "source_aliases": [],
                }
            item = resources[key]
            need(item["png_sha256"] == sha_file(source) and
                 item["rgba8_pixel_sha256"] == sha(rgba),
                 f"same pixels use different PNG encoding; require explicit representative proof: {source_name}")
            item["source_aliases"].append({
                "source_key": row["source_key"], "source_guid": row[role + "_guid"],
                "native_png": source_name, "native_png_sha256": row[role + "_gpu_png_sha256"],
                "source_texture_path": row["renderer_sprite_path"] if role == "sprite"
                    else row["mask_input_path"],
            })
            bindings[role] = name
        outputs.append({
            "source_key": row["source_key"], "category": row["category"],
            "model_id": row["model_id"], "source_queue": row["source_queue"],
            "renderer_file_id_decimal": row["renderer_file_id_decimal"],
            "texture_bindings": bindings,
            "mesh_encoding": "little-endian float32 source-local (x,y,0,u,v) per vertex then uint32 source triangle indices; not UE LocalCm",
            "mesh_sha256": digest, "mesh_bytes": byte_count,
            "mesh_vertex_count": len(row["sprite_vertices"]),
            "mesh_triangle_count": len(row["sprite_triangles"]) // 3,
            "sprite_vertices": row["sprite_vertices"], "sprite_uv": row["sprite_uv"],
            "sprite_triangles": row["sprite_triangles"],
            "native_row": row,  # Preserve pose, pivots/texture dimensions, GUIDs and material inputs losslessly.
        })
    ordered = sorted(resources.values(), key=lambda item: item["resource_id"])
    for item in ordered:
        item["source_aliases"].sort(key=lambda alias: alias["source_key"])
    return {
        "schema": SCHEMA, "run_directory": str(directory.resolve()),
        "source_native_manifest": str(NATIVE_JSON), "source_native_sha256": NATIVE_SHA256,
        "source_contract": str(CONTRACT), "source_contract_sha256": CONTRACT_SHA256,
        "destination_root": DEST, "rows": outputs, "resources": ordered,
        "counts": {"source_rows": 27, "source_png_bindings": 54,
                   "deduplicated_texture_assets": len(ordered)},
        "scope": "GPU-exported prefab/default sprite and mask inputs only; not GameApp PropertyBlock, equal-queue order, renderer packet, main-view Gamma acceptance or map write",
        "texture_policy": "RGBA8 PNG bytes imported with UE TextureFactory as PF_B8G8R8A8 intended RHI, SRGB=false, TC_VectorDisplacementmap, one mip, bilinear clamp, never_stream; actual RHI awaits runtime proof",
    }


def verify_prepared(manifest_path: Path) -> dict:
    path = manifest_path.resolve()
    need(path.name == "manifest.json" and path.parent.parent == OUTPUT.resolve(),
         "prepared manifest is outside exact output child")
    actual = read(path)
    expected = payload(path.parent)
    need(actual == expected, f"prepared plan/source differs: {path}")
    for item in actual["resources"]:
        target = (path.parent / item["png"]).resolve()
        need(target.parent == (path.parent / "textures").resolve() and
             target.is_file() and sha_file(target) == item["png_sha256"],
             f"prepared PNG differs: {target}")
        width, height, rgba = decode_rgba8_png(target)
        bgra = bytearray(rgba)
        bgra[0::4], bgra[2::4] = rgba[2::4], rgba[0::4]
        need((width, height) == (item["width"], item["height"]) and
             sha(rgba) == item["rgba8_pixel_sha256"] and
             sha(bytes(bgra)) == item["bgra8_pixel_sha256"],
             f"prepared decoded pixels differ: {target}")
    return actual


def prepare() -> Path:
    run = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + f"_{os.getpid()}"
    directory = OUTPUT / run
    need(not directory.exists(), f"run exists: {directory}")
    plan = payload(directory)
    directory.mkdir(parents=True, exist_ok=False)
    (directory / "textures").mkdir()
    for item in plan["resources"]:
        shutil.copyfile(NATIVE_RUN / item["representative_native_png"], directory / item["png"])
    manifest = directory / "manifest.json"
    with manifest.open("x", encoding="utf-8") as stream:
        json.dump(plan, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    verify_prepared(manifest)
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--prepare", action="store_true")
    mode.add_argument("--verify", type=Path)
    args = parser.parse_args()
    if args.prepare:
        result = prepare()
        print(f"READY {result} sha256={sha_file(result)}")
    else:
        result = verify_prepared(args.verify)
        print(f"PASS {args.verify}: {result['counts']}")


if __name__ == "__main__":
    main()
