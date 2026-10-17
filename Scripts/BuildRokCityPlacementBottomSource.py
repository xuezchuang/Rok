#!/usr/bin/env python3
"""Extract the Unity placement-bottom quad and its rendering contract.

This is a standalone, read-only source extractor. It reads ref/ files and emits
a traceable JSON manifest under Saved/RokUnityMigration/CityPlacementBottomSource.
It does not import UE assets or modify reference data.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SCHEMA = "rok.city_placement_bottom_source.v2"
DEFAULT_MANIFEST = Path("Saved/RokUnityMigration/CityPlacementBottomSource/v2/placement_bottom_source.json")
SOURCE_FILES = {
    "prefab": Path("ref/client-unity/Assets/BundleAssets/Scene/City/city_function/building_move_bottom.prefab"),
    "mesh": Path("ref/client-unity/Assets/BundleAssetsTest/City/Mesh/building_move_bottom.asset"),
    "mesh_meta": Path("ref/client-unity/Assets/BundleAssetsTest/City/Mesh/building_move_bottom.asset.meta"),
    "texture": Path("ref/client-unity/Assets/BundleAssetsTest/City/Texture2D/sprite_city_building/buiding_bottom2.png"),
    "texture_meta": Path("ref/client-unity/Assets/BundleAssetsTest/City/Texture2D/sprite_city_building/buiding_bottom2.png.meta"),
    "material": Path("ref/client-unity/Assets/BundleAssets/Scene/City/city_function/city_building_move_bottom.mat"),
    "shader": Path("ref/client-unity/Assets/BundleAssets/Map/Building/customcitybuilding_move_bottom-4.shader"),
    "shader_meta": Path("ref/client-unity/Assets/BundleAssets/Map/Building/customcitybuilding_move_bottom-4.shader.meta"),
    "color_script": Path("ref/client-unity/Assets/Scripts/Client/Compoment/Map/ChangeSpriteColor.cs"),
    "city_mediator": Path("ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/CityGlobalMediator.cs"),
    "city_obj_data": Path("ref/client-unity/Assets/Scripts/Hotfix/Common/Utils/CityObjData.cs"),
    "contract": Path("Saved/RokUnityMigration/BatchReview20261009Gamma/placement_bottom_source_contract.json"),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_source_text(path: Path) -> str:
    raw = path.read_bytes()
    for encoding in ("utf-8-sig", "cp936"):
        try:
            return raw.decode(encoding).replace("\r\n", "\n").replace("\r", "\n")
        except UnicodeDecodeError:
            pass
    fail(f"Unsupported source text encoding: {path}")


def fail(message: str) -> None:
    raise ValueError(message)


def require_match(pattern: str, text: str, label: str, flags: int = re.MULTILINE) -> re.Match[str]:
    match = re.search(pattern, text, flags)
    if not match:
        fail(f"Could not parse source field: {label}")
    return match


def parse_mesh(path: Path) -> dict[str, Any]:
    text = read_source_text(path)
    mesh_file_id = int(require_match(r"(?m)^--- !u!43 &(\d+)\s*$", text, "Mesh subasset fileID").group(1))
    mesh_name = require_match(r"(?m)^  m_Name:\s*(\S.*?)\s*$", text, "Mesh subasset name").group(1)
    submesh = require_match(r"m_SubMeshes:\s*\n(.*?)(?=\n\s*m_Shapes:)", text, "m_SubMeshes", re.DOTALL).group(1)
    index_count = int(require_match(r"\bindexCount:\s*(\d+)", submesh, "submesh.indexCount").group(1))
    topology = int(require_match(r"\btopology:\s*(\d+)", submesh, "submesh.topology").group(1))
    aabb_center = [float(v) for v in require_match(
        r"localAABB:\s*\n\s*m_Center:\s*\{x:\s*([^,]+),\s*y:\s*([^,]+),\s*z:\s*([^}]+)\}",
        submesh, "submesh.localAABB.center").groups()]
    aabb_extent = [float(v) for v in require_match(
        r"localAABB:\s*\n\s*m_Center:[^\n]*\n\s*m_Extent:\s*\{x:\s*([^,]+),\s*y:\s*([^,]+),\s*z:\s*([^}]+)\}",
        submesh, "submesh.localAABB.extent").groups()]
    current_channels = int(require_match(r"m_CurrentChannels:\s*(\d+)", text, "m_CurrentChannels").group(1))
    vertex_count = int(require_match(r"m_VertexCount:\s*(\d+)", text, "m_VertexCount").group(1))
    data_size = int(require_match(r"m_DataSize:\s*(\d+)", text, "m_DataSize").group(1))
    raw_hex = require_match(r"_typelessdata:\s*([0-9a-fA-F]+)", text, "_typelessdata").group(1)
    vertex_bytes = bytes.fromhex(raw_hex)
    if vertex_count <= 0 or data_size != len(vertex_bytes) or data_size % vertex_count:
        fail("Mesh vertex data size/count is inconsistent")
    stride = data_size // vertex_count

    channel_block = require_match(r"m_Channels:\s*\n(.*?)\n\s*m_DataSize:", text, "m_Channels", re.DOTALL).group(1)
    channel_rows = []
    for block in re.split(r"(?m)^\s*-\s+stream:\s*", channel_block)[1:]:
        row = {}
        for field in ("offset", "format", "dimension"):
            row[field] = int(require_match(rf"\b{field}:\s*(\d+)", block, f"channel.{field}").group(1))
        channel_rows.append(row)
    if len(channel_rows) != 8:
        fail(f"Expected 8 serialized vertex channel slots, found {len(channel_rows)}")

    def read_vec(vertex_index: int, channel_index: int, dimensions: int) -> list[float]:
        channel = channel_rows[channel_index]
        if channel["format"] != 0 or channel["dimension"] != dimensions:
            fail(f"Mesh channel slot {channel_index} format/dimension differs from expected float/{dimensions}")
        offset = vertex_index * stride + channel["offset"]
        end = offset + dimensions * 4
        if end > (vertex_index + 1) * stride:
            fail(f"Mesh channel slot {channel_index} exceeds vertex stride")
        return list(struct.unpack_from("<" + "f" * dimensions, vertex_bytes, offset))

    positions = [read_vec(i, 0, 3) for i in range(vertex_count)]
    # Unity's serialized channel slot 3 is the 2-float coordinate channel in
    # this source mesh. Keep the raw slot identity; do not rename it UV0.
    uv_channel_index = 3
    uv_values = [read_vec(i, uv_channel_index, 2) for i in range(vertex_count)]
    if vertex_count != 4 or index_count != 6 or topology != 0:
        fail("Source bottom mesh is not the expected four-vertex triangle quad")

    index_hex = require_match(r"m_IndexBuffer:\s*([0-9a-fA-F]+)", text, "m_IndexBuffer").group(1)
    index_bytes = bytes.fromhex(index_hex)
    if len(index_bytes) != index_count * 2:
        fail("Index buffer byte count does not match indexCount and UInt16 source format")
    indices = list(struct.unpack("<" + "H" * index_count, index_bytes))
    if any(index >= vertex_count for index in indices):
        fail("Source index buffer references a missing vertex")

    vertices = []
    for i, (position, uv) in enumerate(zip(positions, uv_values)):
        ue_position = [-position[0] * 500.0, position[2] * 500.0, position[1] * 500.0]
        vertices.append({
            "index": i,
            "source_unity_xyz": position,
            "source_serialized_channel_3_coord": uv,
            "source_vertex_bytes_hex": vertex_bytes[i * stride:(i + 1) * stride].hex(),
            "ue_local_xyz_from_unity_minus_x_z_y_times_500": ue_position,
        })
    return {
        "subasset_file_id": mesh_file_id,
        "subasset_name": mesh_name,
        "vertex_count": vertex_count,
        "index_count": index_count,
        "topology": "triangles" if topology == 0 else f"unknown_{topology}",
        "indices_uint16_source_order": indices,
        "vertices": vertices,
        "vertex_stride_bytes": stride,
        "vertex_data_size_bytes": data_size,
        "m_current_channels": current_channels,
        "serialized_channel_slots": channel_rows,
        "coordinate_channel": {"serialized_slot": uv_channel_index, "offset_bytes": channel_rows[uv_channel_index]["offset"], "dimension": channel_rows[uv_channel_index]["dimension"], "semantic": "unknown; source corner coordinates recorded without asserting Unity TexCoord index"},
        "coordinate_semantic_validation": "pending Unity-native Mesh.uv/triangles export probe",
        "bounds_local_center": aabb_center,
        "bounds_local_extent": aabb_extent,
    }


def parse_guid(text: str, label: str) -> str:
    return require_match(r"\bguid:\s*([0-9a-fA-F]{32})", text, label).group(1).lower()


def parse_png(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        fail("Texture source is not a PNG")
    offset = 8
    chunks = []
    ihdr = None
    has_trns = False
    while offset + 12 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        name = data[offset + 4:offset + 8]
        end = offset + 12 + length
        if end > len(data):
            fail("PNG chunk exceeds file length")
        payload = data[offset + 8:offset + 8 + length]
        crc_expected = struct.unpack_from(">I", data, offset + 8 + length)[0]
        crc_actual = zlib_crc(name + payload)
        if crc_expected != crc_actual:
            fail(f"PNG chunk CRC mismatch: {name.decode('ascii', 'replace')}")
        chunks.append(name.decode("ascii"))
        if name == b"IHDR":
            if length != 13:
                fail("PNG IHDR length is invalid")
            width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(">IIBBBBB", payload)
            ihdr = {"width": width, "height": height, "bit_depth": bit_depth, "color_type": color_type,
                    "compression": compression, "filter": filter_method, "interlace": interlace}
        if name == b"tRNS":
            has_trns = True
        offset = end
        if name == b"IEND":
            break
    if not ihdr or not chunks or chunks[-1] != "IEND":
        fail("PNG is missing IHDR or IEND")
    alpha = ihdr["color_type"] in (4, 6) or (ihdr["color_type"] == 3 and has_trns)
    return {**ihdr, "has_intrinsic_or_palette_alpha": alpha, "has_trns_chunk": has_trns, "chunks": chunks}


def zlib_crc(data: bytes) -> int:
    import zlib
    return zlib.crc32(data) & 0xFFFFFFFF


def source_contract(root: Path) -> dict[str, Any]:
    texts = {key: read_source_text(root / path)
             for key, path in SOURCE_FILES.items() if key not in ("mesh", "texture")}
    prefab = texts["prefab"]
    mesh = parse_mesh(root / SOURCE_FILES["mesh"])
    mesh_guid = parse_guid(texts["mesh_meta"], "mesh meta GUID")
    meta_main_file_id = int(require_match(r"^\s*mainObjectFileID:\s*(\d+)", texts["mesh_meta"], "mesh meta mainObjectFileID").group(1))
    if mesh["subasset_file_id"] != meta_main_file_id:
        fail("Mesh YAML object fileID differs from its meta mainObjectFileID")
    texture_guid = parse_guid(texts["texture_meta"], "texture meta GUID")
    material_text = texts["material"]
    shader_guid = require_match(r"m_Shader:.*?guid:\s*([0-9a-fA-F]{32})", material_text, "material shader GUID").group(1).lower()
    shader_meta_guid = parse_guid(texts["shader_meta"], "shader meta GUID")
    if shader_guid != shader_meta_guid:
        fail("Material shader GUID does not resolve to the supplied shader source")
    texture_section = require_match(r"- _MainTex:\s*\n(.*?)(?=\n\s*- _\w+|\n\s*m_Floats:)", material_text, "material _MainTex", re.DOTALL).group(1)
    material_texture_guid = require_match(r"m_Texture:.*?guid:\s*([0-9a-fA-F]{32})", texture_section, "material texture GUID").group(1).lower()
    material_scale = [float(v) for v in require_match(r"m_Scale:\s*\{x:\s*([^,]+),\s*y:\s*([^}]+)\}", texture_section, "material _MainTex scale").groups()]
    base_color_match = require_match(r"(?ms)^\s*- _Color:\s*\{r:\s*([^,]+),\s*g:\s*([^,]+),\s*b:\s*([^,]+),\s*a:\s*([^}]+)\}", material_text, "material base _Color")
    base_color = [float(value) for value in base_color_match.groups()]
    shader_text = texts["shader"]
    shader_semantics = {
        "alpha_blend": bool(re.search(r"Blend\s+SrcAlpha\s+OneMinusSrcAlpha\s*,\s*SrcAlpha\s+OneMinusSrcAlpha", shader_text)),
        "zwrite_off": bool(re.search(r"\bZWrite\s+Off\b", shader_text)),
        "sample_multiplied_by_color": bool(re.search(r"tex2D\s*\(\s*_MainTex\s*,\s*i\.uv\s*\)\s*\*\s*_Color", shader_text)),
        "returns_sampled_color": bool(re.search(r"return\s+col\s*;", shader_text)),
    }
    if not all(shader_semantics.values()):
        fail("Source shader semantics differ from expected alpha/tint path")
    texture_meta_text = texts["texture_meta"]
    texture_import = {
        "sRGBTexture": int(require_match(r"^\s*sRGBTexture:\s*(\d+)", texture_meta_text, "texture sRGBTexture").group(1)),
        "enableMipMap": int(require_match(r"^\s*enableMipMap:\s*(\d+)", texture_meta_text, "texture enableMipMap").group(1)),
        "textureFormat": int(require_match(r"^\s*textureFormat:\s*(\d+)", texture_meta_text, "texture textureFormat").group(1)),
        "wrapU": int(require_match(r"^\s*wrapU:\s*(\d+)", texture_meta_text, "texture wrapU").group(1)),
        "wrapV": int(require_match(r"^\s*wrapV:\s*(\d+)", texture_meta_text, "texture wrapV").group(1)),
        "wrapW": int(require_match(r"^\s*wrapW:\s*(\d+)", texture_meta_text, "texture wrapW").group(1)),
        "spriteMode": int(require_match(r"^\s*spriteMode:\s*(\d+)", texture_meta_text, "texture spriteMode").group(1)),
        "spriteMeshType": int(require_match(r"^\s*spriteMeshType:\s*(\d+)", texture_meta_text, "texture spriteMeshType").group(1)),
        "spritePixelsToUnits": int(require_match(r"^\s*spritePixelsToUnits:\s*(\d+)", texture_meta_text, "texture spritePixelsToUnits").group(1)),
        "textureType": int(require_match(r"^\s*textureType:\s*(\d+)", texture_meta_text, "texture textureType").group(1)),
        "alphaUsage": int(require_match(r"^\s*alphaUsage:\s*(\d+)", texture_meta_text, "texture alphaUsage").group(1)),
        "alphaIsTransparency": int(require_match(r"^\s*alphaIsTransparency:\s*(\d+)", texture_meta_text, "texture alphaIsTransparency").group(1)),
    }
    subasset = require_match(r"m_Mesh:\s*\{fileID:\s*(\d+),\s*guid:\s*([0-9a-fA-F]{32})", prefab, "prefab mesh reference")
    mat_ref = require_match(r"m_Materials:\s*\n\s*-\s*\{fileID:\s*(\d+),\s*guid:\s*([0-9a-fA-F]{32})", prefab, "prefab material reference")
    root_transform = require_match(r"(?ms)^--- !u!4 &\d+\nTransform:\s*\n.*?m_LocalPosition:\s*\{x:\s*([^,]+),\s*y:\s*([^,]+),\s*z:\s*([^}]+)\}.*?m_LocalScale:\s*\{x:\s*([^,]+),\s*y:\s*([^,]+),\s*z:\s*([^}]+)\}.*?m_LocalEulerAnglesHint:\s*\{x:\s*([^,]+),\s*y:\s*([^,]+),\s*z:\s*([^}]+)\}", prefab, "prefab root transform")
    root_values = [float(value) for value in root_transform.groups()]
    transform = {"local_position_unity": root_values[0:3], "local_scale_unity": root_values[3:6], "local_euler_degrees_unity": root_values[6:9]}
    medi = texts["city_mediator"]
    color_script = texts["color_script"]
    citydata = texts["city_obj_data"]
    uv_assignment = require_match(r"\bo\.uv\s*=\s*v\.uv\s*;", shader_text,
                                  "identity vertex UV assignment").group(0)
    shader_uses_maintex_st = bool(re.search(r"\b_MainTex_ST\b", shader_text))
    shader_uses_transform_tex = bool(re.search(r"\bTRANSFORM_TEX\s*\(", shader_text))
    if shader_uses_maintex_st or shader_uses_transform_tex:
        fail("Source placement-bottom shader now transforms texture UVs; review the v2 semantic contract")
    width_values = [float(v) for v in require_match(r"CITY_GRID_SIZE\s*=\s*([0-9.]+)F", citydata, "CITY_GRID_SIZE").groups()]
    half = float(require_match(r"CITY_GRID_SIZE_HALF\s*=\s*([0-9.]+)F", citydata, "CITY_GRID_SIZE_HALF").group(1))
    visual_behavior = {
        "red_invalid_call": "ChangeSpriteColor.SetColor(cc, Color.red)" in medi,
        "green_valid_call": "ChangeSpriteColor.SetColor(cc, Color.green)" in medi,
        "runtime_texture_scale_call_from_footprint": bool(re.search(r'SetTextureScale\("_MainTex",\s*size\)', medi)),
        "scale_from_footprint": bool(re.search(r"localScale\s*=\s*new Vector3\(size\.x,\s*0\.01f,\s*size\.y\)", medi)),
        "position_follows_selected_building": "m_bottomBuildObj.transform.localPosition = m_selectBuild.localPosition" in medi,
        "color_written_through_property_block": "GetPropertyBlock(materialPropertyBlock)" in color_script and "SetColor(\"_Color\", color)" in color_script and "SetPropertyBlock(materialPropertyBlock)" in color_script,
    }
    if not all(visual_behavior.values()):
        fail("Source placement feedback calls differ from expected behavior")
    if mesh_guid != "7c5e0bb563f70cd428c626f7920d38e2" or material_texture_guid != texture_guid:
        fail("Prefab/material GUID chain does not resolve to expected bottom source assets")
    if subasset.group(2).lower() != mesh_guid or mat_ref.group(2).lower() != "246cfae25d324044196fe9b013ca2c1d":
        fail("Prefab mesh or material reference GUID differs from source meta")
    png = parse_png(root / SOURCE_FILES["texture"])
    return {
        "unity_mesh": mesh,
        "mesh_guid": mesh_guid,
        "prefab": {"mesh_file_id": int(subasset.group(1)), "mesh_guid": subasset.group(2).lower(),
                   "material_file_id": int(mat_ref.group(1)), "material_guid": mat_ref.group(2).lower(),
                   "root_transform": transform},
        "material": {"shader_guid": shader_guid, "shader_meta_guid": shader_meta_guid, "main_texture_guid": material_texture_guid,
                     "main_texture_scale": material_scale, "base_color_rgba": base_color},
        "texture": {"guid": texture_guid, "png": png, "import_settings": texture_import,
                    "repeat_mode": "Repeat" if texture_import["wrapU"] == texture_import["wrapV"] == texture_import["wrapW"] == 0 else "unknown"},
        "shader": {"blend": "SrcAlpha OneMinusSrcAlpha", "zwrite": "Off", "formula": "col = tex2D(_MainTex, uv) * _Color; return col",
                   "vertex_uv_assignment": uv_assignment, "uses_maintex_st": shader_uses_maintex_st,
                   "uses_transform_tex": shader_uses_transform_tex,
                   "effective_sampling_uv": "identity_mesh_uv",
                   "runtime_texture_scale_affects_sampling": False, **shader_semantics},
        "placement_behavior": {"grid_size_unity": width_values[0], "grid_half_unity": half,
                               "invalid_color_rgba": [1.0, 0.0, 0.0, 1.0], "valid_color_rgba": [0.0, 1.0, 0.0, 1.0],
                               "root_scale_from_footprint": ["width", 0.01, "length"],
                               "runtime_texture_scale_request_from_footprint": ["width", "length"],
                               "effective_footprint_uv_tiling": False,
                               "uv_semantics_note": "Mediator calls SetTextureScale(_MainTex,size), but this shader passes v.uv to o.uv and samples i.uv without _MainTex_ST or TRANSFORM_TEX; effective sampling remains identity mesh UV.",
                               **visual_behavior},
    }


def build_manifest(root: Path) -> dict[str, Any]:
    for rel in SOURCE_FILES.values():
        if not (root / rel).is_file():
            fail(f"Required source file is missing: {rel.as_posix()}")
    source = source_contract(root)
    source_hashes = [{"path": rel.as_posix(), "sha256": sha256(root / rel)} for rel in SOURCE_FILES.values()]
    transform = source["prefab"]["root_transform"]
    # Coordinate map is a proper rotation (det=+1): source triangle winding
    # remains valid. No extra reflection compensation should be introduced.
    conversion = {"formula": "UE = (-UnityX, UnityZ, UnityY) * 500", "linear_determinant": 1,
                  "winding_policy": "preserve source index order; basis determinant is +1, so no winding flip is required",
                  "unity_prefab_root_transform": transform,
                  "ue_prefab_root_rotation_degrees": [transform["local_euler_degrees_unity"][0], transform["local_euler_degrees_unity"][1], transform["local_euler_degrees_unity"][2]],
                  "note": "Converted per-vertex coordinates are local mesh coordinates. Keep the source prefab +45 degree child yaw and let the UE city-container transform reproduce the city frame; do not bake scene placement into this quad."}
    return {
        "schema": SCHEMA,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "acceptance": "source-extracted; ue-import-and-runtime-render-pending",
        "source_hashes": source_hashes,
        "tool_hashes": [
            {"path": Path(__file__).resolve().relative_to(root).as_posix(), "sha256": sha256(Path(__file__).resolve())},
            {"path": "Scripts/VerifyRokCityPlacementBottomSource.py", "sha256": sha256(root / "Scripts/VerifyRokCityPlacementBottomSource.py")},
        ],
        "source": source,
        "coordinate_conversion": conversion,
        "ue_import_guidance": {
            "geometry": "Import the recorded four source vertices and six source indices as a dedicated source-derived mesh; do not replace with BasicShapes.",
            "texture": "Import the exact source PNG; preserve sRGB, no-mipmap, repeat sampler state, and alpha channel. The source shader uses identity mesh UV; its runtime SetTextureScale call is not consumed by shader sampling.",
            "material": "Use translucent source-alpha blend, disable depth writes, multiply sampled texture by tint, and use exact validity tint RGBA values.",
            "runtime": "Place the transient quad at the snapped footprint center and drive width/length and tint from the placement preview/grid validator.",
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--out", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--force", action="store_true", help="replace an existing generated manifest")
    args = parser.parse_args()
    root = args.root.resolve()
    out = args.out if args.out.is_absolute() else root / args.out
    if out.exists() and not args.force:
        fail(f"Output already exists; pass --force to replace generated output: {out}")
    manifest = build_manifest(root)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {out}")
    print(f"Unity quad: {manifest['source']['unity_mesh']['vertex_count']} vertices, {manifest['source']['unity_mesh']['index_count']} indices")
    print(f"Texture: {manifest['source']['texture']['png']['width']}x{manifest['source']['texture']['png']['height']}, alpha={manifest['source']['texture']['png']['has_intrinsic_or_palette_alpha']}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        raise SystemExit(f"FAIL: {exc}")
