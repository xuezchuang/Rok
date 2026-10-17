"""Pin Unity Gamma family GPU mip bytes for an explicit UE RHI upload audit.

The source RGBA32 files are Unity shader-expanded ReadPixels results, not
compressed Unity texture bits. Each level is oriented against its pinned PNG
before converting top-down RGBA rows to non-sRGB PF_B8G8R8A8 input bytes.
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
from rok_data_paths import resolve

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityGammaFamilyStateProbeV2"
RUN = PROBE / "OutputRuns/20261008_234412_007_44320"
CONTRACT = PROBE / "state_contract.json"
NATIVE = RUN / "native_state.json"
NATIVE_VERIFY = RUN / "native_verify.json"
OUT = ROOT / "Saved/RokUnityMigration/CityGammaFamilyRhiInputs/v1"
CONTRACT_SHA = "9555b7e77ff67dad29789923109cde5574358dbc4cc7072e174b0f3987f27a49"
NATIVE_SHA = "671f18f91275fc0a5a65107eff5e44857c59310873036293b99665f84c391b71"
FAMILIES = [
    "initial:5",
    "ground:ground_1",
    "ground:ground_2",
    "support:boundary_support_road",
    "support:boundary_support_wall_shadow",
]


def fail_if(condition: bool, message: str) -> None:
    if condition:
        raise ValueError(message)


def sha_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha(path: Path) -> str:
    return sha_bytes(path.read_bytes())


def canonical_json(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def write_immutable(path: Path, data: bytes) -> None:
    if path.exists():
        fail_if(path.read_bytes() != data, f"Existing prepared output differs: {path}")
        return
    fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644)
    with os.fdopen(fd, "wb") as stream:
        stream.write(data)


def normalize_mip(raw: bytes, image: Image.Image, width: int, height: int) -> tuple[bytes, str]:
    fail_if(image.size != (width, height), "Pinned mip PNG extent differs")
    png_rgba = image.convert("RGBA").tobytes()
    stride = width * 4
    fail_if(len(raw) != stride * height, "Pinned mip RGBA byte count differs")
    reversed_rows = b"".join(raw[(height - 1 - y) * stride:(height - y) * stride]
                             for y in range(height))
    top = raw == png_rgba
    bottom = reversed_rows == png_rgba
    fail_if(not (top or bottom), "Pinned RGBA rows do not equal pinned PNG in either orientation")
    if top and bottom:
        return png_rgba, "both_orders_identical_pixels"
    return png_rgba, "top_down" if top else "bottom_up"


def rgba_to_bgra(data: bytes) -> bytes:
    output = bytearray(len(data))
    output[0::4] = data[2::4]
    output[1::4] = data[1::4]
    output[2::4] = data[0::4]
    output[3::4] = data[3::4]
    return bytes(output)


def main() -> None:
    fail_if(sha(CONTRACT) != CONTRACT_SHA or sha(NATIVE) != NATIVE_SHA,
            "Frozen contract/native SHA mismatch")
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    native = json.loads(NATIVE.read_text(encoding="utf-8"))
    native_verify = json.loads(NATIVE_VERIFY.read_text(encoding="utf-8"))
    fail_if(contract["schema"] != "rok.city_gamma_family_state_contract.v2"
            or native["schema"] != "rok.city_gamma_family_state_native.v2"
            or not native["complete"] or native["source_contract_sha256"] != CONTRACT_SHA
            or not native_verify["passed"] or native_verify["native_sha256"] != NATIVE_SHA
            or native_verify["source_contract_sha256"] != CONTRACT_SHA,
            "Frozen native family proof not qualified")
    fail_if([r["source_key"] for r in native["rows"]] != FAMILIES
            or [r["source_key"] for r in contract["rows"]] != FAMILIES,
            "Unexpected family order")
    source_by_probe = {}
    for item in contract["source_files"]:
        source = resolve(item["source_path"])
        fail_if(not source.is_file() or sha(source) != item["source_sha256"],
                f"Source file hash differs: {item['source_path']}")
        source_by_probe[item["probe_path"]] = item
    OUT.mkdir(parents=True, exist_ok=True)
    textures: list[dict] = []
    ids_by_guid: dict[str, str] = {}
    family_rows: list[dict] = []
    source_mip_ref_count = 0
    for native_row, contract_row in zip(native["rows"], contract["rows"], strict=True):
        fail_if(native_row["source_key"] != contract_row["source_key"], "Family key mismatch")
        refs = []
        for index, native_tex in enumerate(native_row["textures"]):
            guid = native_tex["source_guid"]
            source_path = native_tex["source_asset_path"]
            source = source_by_probe.get(source_path)
            fail_if(source is None or source["source_guid"] != guid,
                    f"Texture GUID/source chain differs: {source_path}")
            roles = [role["role"] for role in contract_row["roles"]
                     if role.get("probe_path") == source_path]
            fail_if(not roles, f"No source material role for {source_path}")
            if guid not in ids_by_guid:
                tid = f"texture_{len(textures):02d}"
                ids_by_guid[guid] = tid
                fail_if(native_tex["mip_count"] != len(native_tex["levels"])
                        or native_tex["mip_count"] < 1 or native_tex["mip_count"] > 12,
                        f"Mip count: {tid}")
                levels = []
                for level_index, level in enumerate(native_tex["levels"]):
                    raw_path = RUN / level["file"]
                    png_path = RUN / level["png"]
                    fail_if(level["level"] != level_index or not raw_path.is_file()
                            or not png_path.is_file(), f"Mip level/file missing: {tid}/{level_index}")
                    raw = raw_path.read_bytes()
                    fail_if(sha_bytes(raw) != level["sha256"] or sha(png_path) != level["png_sha256"]
                            or len(raw) != level["byte_count"], f"Mip hash/count: {tid}/{level_index}")
                    width, height = int(level["width"]), int(level["height"])
                    fail_if(width != max(1, native_tex["width"] >> level_index)
                            or height != max(1, native_tex["height"] >> level_index),
                            f"Mip extent: {tid}/{level_index}")
                    with Image.open(png_path) as image:
                        top_rgba, observed_order = normalize_mip(raw, image, width, height)
                    bgra = rgba_to_bgra(top_rgba)
                    output_name = f"{tid}_mip_{level_index:02d}.bgra8.bin"
                    write_immutable(OUT / output_name, bgra)
                    levels.append({
                        "level": level_index, "width": width, "height": height,
                        "byte_count": len(bgra),
                        "source_rgba_file": str(raw_path.relative_to(ROOT)).replace("\\", "/"),
                        "source_rgba_sha256": level["sha256"],
                        "source_png_file": str(png_path.relative_to(ROOT)).replace("\\", "/"),
                        "source_png_sha256": level["png_sha256"],
                        "source_rgba_row_order_vs_png": observed_order,
                        "prepared_bgra_file": str((OUT / output_name).relative_to(ROOT)).replace("\\", "/"),
                        "prepared_bgra_sha256": sha_bytes(bgra),
                        "prepared_row_order": "top_down",
                    })
                textures.append({
                    "texture_id": tid, "source_guid": guid,
                    "source_asset_path": source_path,
                    "source_repo_path": source["source_path"],
                    "source_png_sha256": source["source_sha256"],
                    "source_meta_sha256": source["meta_sha256"],
                    "source_srgb_importer": native_tex["srgb"],
                    "source_filter": native_tex["filter"],
                    "source_wrap": native_tex["wrap"],
                    "width": native_tex["width"], "height": native_tex["height"],
                    "mip_count": native_tex["mip_count"], "levels": levels,
                })
            else:
                existing = textures[int(ids_by_guid[guid].split("_")[-1])]
                for key in ("source_asset_path", "source_filter", "source_wrap", "width", "height", "mip_count"):
                    source_key = {"source_filter": "filter", "source_wrap": "wrap"}.get(key, key)
                    fail_if(existing[key] != native_tex[source_key], f"Duplicate texture differs: {guid}/{key}")
                fail_if([m["source_rgba_sha256"] for m in existing["levels"]]
                        != [m["sha256"] for m in native_tex["levels"]],
                        f"Duplicate texture mip bytes differ: {guid}")
            source_mip_ref_count += native_tex["mip_count"]
            refs.append({"native_texture_index": index, "texture_id": ids_by_guid[guid],
                         "source_roles": roles, "source_guid": guid,
                         "mip_count": native_tex["mip_count"]})
        family_rows.append({"source_key": native_row["source_key"],
                            "shader_name": native_row["shader_name"],
                            "textures": refs})
    fail_if(len(textures) != 10 or sum(t["mip_count"] for t in textures) != 77
            or source_mip_ref_count != 110, "Expected 10 textures / 77 unique mips / 110 row-mip refs")
    manifest = {
        "schema": "rok.city_gamma_family_rhi_inputs.v1",
        "qualification": "Unity2019 Gamma shader-expanded GPU sample bytes; explicit UE diagnostic RHI upload only",
        "native_state_path": str(NATIVE.relative_to(ROOT)).replace("\\", "/"),
        "native_state_sha256": NATIVE_SHA,
        "state_contract_path": str(CONTRACT.relative_to(ROOT)).replace("\\", "/"),
        "state_contract_sha256": CONTRACT_SHA,
        "native_verify_path": str(NATIVE_VERIFY.relative_to(ROOT)).replace("\\", "/"),
        "native_verify_sha256": sha(NATIVE_VERIFY),
        "rhi_target": {"format": "PF_B8G8R8A8", "srgb": False,
                       "flags": ["ShaderResource"], "generate_mips": False,
                       "upload_row_order": "top_down"},
        "counts": {"families": 5, "textures": 10, "unique_mips": 77,
                   "source_row_mip_references": 110},
        "families": family_rows,
        "textures": textures,
    }
    manifest_path = OUT / "manifest.json"
    write_immutable(manifest_path, canonical_json(manifest))
    print(json.dumps({"manifest": str(manifest_path), "sha256": sha(manifest_path),
                      "counts": manifest["counts"]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
