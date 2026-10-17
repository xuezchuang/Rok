"""Package verified Unity GPU-decoded mountain F mip PNGs as a DX10 BGRA8 DDS.

No Unreal import or existing asset write occurs here. --check validates all
inputs and computes the product in memory without writing files.
"""

from pathlib import Path
import argparse
import hashlib
import json
import struct
import zlib
from PIL import Image, ImageChops, ImageOps, ImageStat


ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / "Saved/RokUnityMigration/MountainFGammaV2"
OUTPUT = ROOT / "Saved/RokUnityMigration/MountainFGammaProduct"
DDS_NAME = "T_MountainF_UnityDecodedMips.dds"
MANIFEST_NAME = "prepare_manifest.json"
SOURCE_PNG = ROOT / "ref/client-unity/Assets/BundleAssets/land/Texture/build_mountain_F.png"
SOURCE_PNG_SHA256 = "a08cb9a962ca757ac1981bcf57e6b450a0264b56e65075be8a0d21b3e6b3a505"


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def decode_rgba_png(path, expected_sha, width, height):
    data = path.read_bytes()
    if sha256(data) != expected_sha or data[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError(f"Mountain F PNG identity changed: {path}")
    chunks = []
    at = 8
    while at + 12 <= len(data):
        size = struct.unpack_from(">I", data, at)[0]
        kind = data[at + 4:at + 8]
        chunk = data[at + 8:at + 8 + size]
        if at + 12 + size > len(data) or zlib.crc32(kind + chunk) != struct.unpack_from(">I", data, at + 8 + size)[0]:
            raise RuntimeError(f"Mountain F PNG CRC failed: {path}")
        chunks.append((kind, chunk))
        at += size + 12
        if kind == b"IEND":
            break
    if not chunks or chunks[0][0] != b"IHDR" or chunks[-1][0] != b"IEND" or at != len(data):
        raise RuntimeError(f"Mountain F PNG chunks incomplete: {path}")
    header = struct.unpack(">IIBBBBB", chunks[0][1])
    if header != (width, height, 8, 6, 0, 0, 0):
        raise RuntimeError(f"Mountain F PNG is not expected RGBA8 mip: {path}")
    scanlines = zlib.decompress(b"".join(chunk for kind, chunk in chunks if kind == b"IDAT"))
    stride = width * 4
    if len(scanlines) != height * (stride + 1):
        raise RuntimeError(f"Mountain F PNG scanline length changed: {path}")
    previous = bytearray(stride)
    pixels = bytearray()
    for y in range(height):
        offset = y * (stride + 1)
        filter_type = scanlines[offset]
        row = bytearray(scanlines[offset + 1:offset + 1 + stride])
        if filter_type > 4:
            raise RuntimeError(f"Mountain F PNG filter invalid: {path}")
        for x in range(stride):
            left = row[x - 4] if x >= 4 else 0
            above = previous[x]
            upper_left = previous[x - 4] if x >= 4 else 0
            if filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            elif filter_type == 4:
                p = left + above - upper_left
                distances = (abs(p - left), abs(p - above), abs(p - upper_left))
                predictor = (left, above, upper_left)[distances.index(min(distances))]
            else:
                predictor = 0
            row[x] = (row[x] + predictor) & 255
        pixels.extend(row)
        previous = row
    if pixels[3::4] != b"\xff" * (width * height):
        raise RuntimeError(f"Mountain F mip alpha is not opaque: {path}")
    return bytes(pixels)


def read_verified_mips():
    verify_path = INPUT / "verify.json"
    native_path = INPUT / "native_render.json"
    prepare_path = INPUT / "prepare.json"
    verify = read_json(verify_path)
    native = read_json(native_path)
    preparation = read_json(prepare_path)
    if (verify.get("schema") != "rok.mountain_f_gamma_v2.verify.v1"
            or verify.get("passed") is not True
            or verify.get("prepared") is not True
            or verify.get("native_checked") is not True
            or verify.get("product_modified") is not False
            or preparation.get("schema") != "rok.mountain_f_gamma_v2.prepare.v1"
            or preparation.get("source_files_match_old_manifest") is not True
            or preparation.get("product_modified") is not False):
        raise RuntimeError("Mountain F strict V2 verification is not passed")
    if (native.get("unityVersion") != "2019.4.28f1c1"
            or native.get("colorSpace") != "Gamma"
            or native.get("texturePath") != "Assets/BundleAssets/land/Texture/build_mountain_F.png"
            or native.get("textureFormat") != "DXT1"
            or native.get("graphicsFormat") != "RGBA_DXT1_UNorm"
            or native.get("sourceShader") != "custom/landmap/z_cm_slg_mountain"
            or native.get("filterMode") != "Bilinear"
            or native.get("width") != 1024 or native.get("height") != 1024
            or native.get("mipCount") != 11
            or native.get("materialMainTextureIsSource") is not True):
        raise RuntimeError("Mountain F native Gamma DXT1 contract changed")
    checks = verify.get("mips")
    sources = native.get("mips")
    if not isinstance(checks, list) or not isinstance(sources, list) or len(checks) != 11 or len(sources) != 11:
        raise RuntimeError("Mountain F must supply exactly 11 native mips")
    if sha256(SOURCE_PNG.read_bytes()) != SOURCE_PNG_SHA256:
        raise RuntimeError("Original mountain F PNG changed")
    source_image = Image.open(SOURCE_PNG).convert("RGB")
    if source_image.size != (2048, 2048):
        raise RuntimeError("Original mountain F PNG dimensions changed")
    mips = []
    manifest_mips = []
    for level in range(11):
        expected = max(1, 1024 >> level)
        check, source = checks[level], sources[level]
        png_path = INPUT / f"native_mip{level}.png"
        bc1_path = INPUT / f"native_mip{level}.bc1"
        if (check.get("level") != level or source.get("level") != level
                or check.get("size") != [expected, expected]
                or source.get("width") != expected or source.get("height") != expected
                or check.get("alpha_opaque") != expected * expected
                or check.get("alpha_zero") != 0
                or check.get("orientation") != ("as_is" if expected == 1 else "flip_y")
                or check.get("max_bc1_rgb_error", 2) > 1
                or source.get("rgbaBytes") != expected * expected * 4
                or Path(source.get("rgbaPath", "")).resolve() != png_path.resolve()
                or Path(source.get("bc1Path", "")).resolve() != bc1_path.resolve()
                or sha256(bc1_path.read_bytes()) != check.get("bc1_sha256")):
            raise RuntimeError(f"Mountain F mip {level} source/oracle changed")
        rgba = decode_rgba_png(png_path, check.get("png_sha256"), expected, expected)
        # The V2 BC1 oracle's flip_y describes raw BC1 block row order versus
        # the saved GPU PNG. It does not imply a flip versus the original art.
        # Compare the PNG directly to the source art at this mip size. Preserve
        # its saved top-to-bottom orientation for the DDS and existing mesh UVs.
        source_small = source_image.resize((expected, expected), Image.Resampling.BILINEAR)
        native_image = Image.frombytes("RGBA", (expected, expected), rgba).convert("RGB")
        def mae(image):
            return sum(ImageStat.Stat(ImageChops.difference(source_small, image)).mean) / 3
        as_saved_mae = mae(native_image)
        flip_y_mae = mae(ImageOps.flip(native_image))
        if expected > 1 and not (as_saved_mae + 0.5 < flip_y_mae and as_saved_mae < 40):
            raise RuntimeError(f"Mountain F mip {level} source orientation is ambiguous or flipped")
        if expected == 1 and abs(as_saved_mae - flip_y_mae) > 1e-9:
            raise RuntimeError("Mountain F 1x1 orientation must be degenerate")
        bgra = bytearray(len(rgba))
        bgra[0::4], bgra[1::4], bgra[2::4], bgra[3::4] = (
            rgba[2::4], rgba[1::4], rgba[0::4], rgba[3::4]
        )
        mips.append(bytes(bgra))
        manifest_mips.append({
            "level": level, "size": [expected, expected],
            "source_png_sha256": check["png_sha256"],
            "source_bc1_sha256": check["bc1_sha256"],
            "source_orientation": "as_saved",
            "orientation_evidence": "single-pixel-inherited" if expected == 1 else "source-PNG-bilinear-downsample",
            "source_mae_as_saved": as_saved_mae,
            "source_mae_flip_y": flip_y_mae,
            "bc1_to_png_orientation": check.get("orientation"),
            "bgra8_sha256": sha256(bgra), "bytes": len(bgra),
        })
    return mips, manifest_mips, {
        "verify_sha256": sha256(verify_path.read_bytes()),
        "native_sha256": sha256(native_path.read_bytes()),
        "prepare_sha256": sha256(prepare_path.read_bytes()),
        "source_png_sha256": SOURCE_PNG_SHA256,
    }


def pack_dds(mips):
    # DDS_HEADER: 124 bytes, DDS_PIXELFORMAT FourCC DX10; DX10 header BGRA8
    # UNORM 2D arraySize1. UE5.8 ImportDDS copies all 11 source mip images.
    header = struct.pack("<7I", 124, 0x2100F, 1024, 1024, 4096, 0, 11)
    header += struct.pack("<11I", *([0] * 11))
    header += struct.pack("<II4s5I", 32, 4, b"DX10", 0, 0, 0, 0, 0)
    header += struct.pack("<5I", 0x401008, 0, 0, 0, 0)
    if len(header) != 124 or len(mips) != 11:
        raise RuntimeError("Mountain F DDS header/mip count invalid")
    dx10 = struct.pack("<5I", 87, 3, 0, 1, 0)  # B8G8R8A8_UNORM, Texture2D
    return b"DDS " + header + dx10 + b"".join(mips)


def build(check_only=False):
    mips, manifest_mips, source_hashes = read_verified_mips()
    dds = pack_dds(mips)
    manifest = {
        "schema": "rok.mountain_f_gamma_product.prepare.v1",
        "status": "prepared-not-imported" if not check_only else "read-only-check",
        "source": "Unity 2019 Gamma NVIDIA DXT1 GPU-decoded mip PNGs",
        "source_hashes": source_hashes,
        "dds_file": DDS_NAME,
        "dds_sha256": sha256(dds),
        "dds_bytes": len(dds),
        "dds_format": "DX10 B8G8R8A8_UNORM 2D",
        "source_orientation": "as_saved",
        "dds_extra_vertical_flip": False,
        "mip_count": 11,
        "mips": manifest_mips,
        "ue_asset_import_verified": False,
    }
    if check_only:
        print(json.dumps({"check": "passed", "dds_sha256": manifest["dds_sha256"], "mips": 11}))
        return manifest
    OUTPUT.mkdir(parents=True, exist_ok=True)
    dds_path = OUTPUT / DDS_NAME
    manifest_path = OUTPUT / MANIFEST_NAME
    body = (json.dumps(manifest, indent=2) + "\n").encode("utf-8")
    for path, content in ((dds_path, dds), (manifest_path, body)):
        if path.exists() and path.read_bytes() != content:
            raise RuntimeError(f"Existing Mountain F product differs; refusing overwrite: {path}")
    if not dds_path.exists():
        dds_path.write_bytes(dds)
    if not manifest_path.exists():
        manifest_path.write_bytes(body)
    print(json.dumps({"prepared": str(dds_path), "sha256": manifest["dds_sha256"], "mips": 11}))
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="validate and hash without writing")
    build(check_only=parser.parse_args().check)
