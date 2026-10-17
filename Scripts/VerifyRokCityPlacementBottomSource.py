#!/usr/bin/env python3
"""Independently verify the generated placement-bottom source manifest.

The verifier reparses Unity YAML, PNG chunks, importer metadata, material,
shader, prefab and placement source. It does not trust a generator pass flag.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import zlib
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


DEFAULT_MANIFEST = Path("Saved/RokUnityMigration/CityPlacementBottomSource/v2/placement_bottom_source.json")
DEFAULT_REPORT = Path("Saved/RokUnityMigration/CityPlacementBottomSource/v2/placement_bottom_source_verify.json")
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


class CheckSet:
    def __init__(self) -> None:
        self.rows: list[dict[str, Any]] = []

    def add(self, name: str, passed: bool, detail: str) -> None:
        self.rows.append({"name": name, "passed": bool(passed), "detail": detail})

    def equal(self, name: str, actual: Any, expected: Any, detail: str = "") -> None:
        self.add(name, actual == expected, detail or f"actual={actual!r}; expected={expected!r}")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_source_text(path: Path) -> str:
    raw = path.read_bytes()
    for encoding in ("utf-8-sig", "cp936"):
        try:
            return raw.decode(encoding).replace("\r\n", "\n").replace("\r", "\n")
        except UnicodeDecodeError:
            pass
    raise ValueError(f"unsupported source encoding: {path}")


def match(pattern: str, text: str) -> re.Match[str]:
    result = re.search(pattern, text, re.MULTILINE)
    if not result:
        raise ValueError(f"source pattern not found: {pattern}")
    return result


def parse_mesh_independently(path: Path) -> dict[str, Any]:
    text = read_source_text(path)
    mesh_file_id = int(match(r"(?m)^--- !u!43 &(\d+)\s*$", text).group(1))
    mesh_name = match(r"(?m)^  m_Name:\s*(\S.*?)\s*$", text).group(1)
    submesh = match(r"m_SubMeshes:\s*\n([\s\S]*?)(?=\n\s*m_Shapes:)", text).group(1)
    index_count = int(match(r"\bindexCount:\s*(\d+)", submesh).group(1))
    topology = int(match(r"\btopology:\s*(\d+)", submesh).group(1))
    vertex_count = int(match(r"m_VertexCount:\s*(\d+)", text).group(1))
    data_size = int(match(r"m_DataSize:\s*(\d+)", text).group(1))
    mask = int(match(r"m_CurrentChannels:\s*(\d+)", text).group(1))
    raw = bytes.fromhex(match(r"_typelessdata:\s*([0-9a-fA-F]+)", text).group(1))
    stream = bytes.fromhex(match(r"m_IndexBuffer:\s*([0-9a-fA-F]+)", text).group(1))
    if vertex_count == 0 or data_size != len(raw) or data_size % vertex_count:
        raise ValueError("raw vertex buffer length is inconsistent")
    stride = data_size // vertex_count
    block = match(r"m_Channels:\s*\n([\s\S]*?)\n\s*m_DataSize:", text).group(1)
    channels = []
    for entry in re.split(r"(?m)^\s*-\s+stream:\s*", block)[1:]:
        channels.append({
            "offset": int(match(r"\boffset:\s*(\d+)", entry).group(1)),
            "format": int(match(r"\bformat:\s*(\d+)", entry).group(1)),
            "dimension": int(match(r"\bdimension:\s*(\d+)", entry).group(1)),
        })
    if len(channels) != 8:
        raise ValueError(f"expected eight serialized channel slots, got {len(channels)}")

    def unpack(index: int, slot: int, dims: int) -> list[float]:
        row = channels[slot]
        if row["format"] != 0 or row["dimension"] != dims:
            raise ValueError(f"channel slot {slot} is not float/{dims}")
        return list(struct.unpack_from("<" + "f" * dims, raw, index * stride + row["offset"]))

    verts = []
    for i in range(vertex_count):
        pos = unpack(i, 0, 3)
        coord = unpack(i, 3, 2)
        verts.append({
            "index": i,
            "source_unity_xyz": pos,
            "source_serialized_channel_3_coord": coord,
            "source_vertex_bytes_hex": raw[i * stride:(i + 1) * stride].hex(),
            "ue_local_xyz_from_unity_minus_x_z_y_times_500": [-pos[0] * 500.0, pos[2] * 500.0, pos[1] * 500.0],
        })
    if len(stream) != index_count * 2:
        raise ValueError("index buffer length mismatches UInt16 indexCount")
    indices = list(struct.unpack("<" + "H" * index_count, stream))
    if any(index >= vertex_count for index in indices):
        raise ValueError("index buffer references an absent vertex")
    return {"subasset_file_id": mesh_file_id, "subasset_name": mesh_name,
            "vertex_count": vertex_count, "index_count": index_count,
            "topology": "triangles" if topology == 0 else f"unknown_{topology}",
            "indices_uint16_source_order": indices, "vertices": verts,
            "vertex_stride_bytes": stride, "vertex_data_size_bytes": data_size,
            "m_current_channels": mask, "serialized_channel_slots": channels,
            "coordinate_channel": {"serialized_slot": 3, "offset_bytes": channels[3]["offset"],
                                   "dimension": channels[3]["dimension"],
                                   "semantic": "unknown; source corner coordinates recorded without asserting Unity TexCoord index"},
            "coordinate_semantic_validation": "pending Unity-native Mesh.uv/triangles export probe"}


def parse_png_independently(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("bad PNG signature")
    off, chunks, ihdr, trns = 8, [], None, False
    while off + 12 <= len(data):
        size = struct.unpack_from(">I", data, off)[0]
        tag = data[off + 4:off + 8]
        stop = off + size + 12
        if stop > len(data):
            raise ValueError("PNG chunk extends beyond end")
        payload = data[off + 8:off + 8 + size]
        expected = struct.unpack_from(">I", data, off + 8 + size)[0]
        if zlib.crc32(tag + payload) & 0xFFFFFFFF != expected:
            raise ValueError(f"PNG CRC error in {tag!r}")
        chunks.append(tag.decode("ascii"))
        if tag == b"IHDR":
            if size != 13:
                raise ValueError("invalid IHDR length")
            w, h, depth, ctype, comp, filt, interlace = struct.unpack(">IIBBBBB", payload)
            ihdr = {"width": w, "height": h, "bit_depth": depth, "color_type": ctype,
                    "compression": comp, "filter": filt, "interlace": interlace}
        trns |= tag == b"tRNS"
        off = stop
        if tag == b"IEND":
            break
    if ihdr is None or not chunks or chunks[-1] != "IEND":
        raise ValueError("PNG missing required chunks")
    return {**ihdr, "has_intrinsic_or_palette_alpha": ihdr["color_type"] in (4, 6) or (ihdr["color_type"] == 3 and trns),
            "has_trns_chunk": trns, "chunks": chunks}


def run_checks(root: Path, manifest_path: Path) -> dict[str, Any]:
    checks = CheckSet()
    manifest: dict[str, Any] = {}
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    except Exception as exc:
        checks.add("manifest_readable_json", False, str(exc))
        return {"manifest": {}, "checks": checks.rows}
    checks.equal("manifest_schema", manifest.get("schema"), "rok.city_placement_bottom_source.v2")

    declared = {row.get("path"): row.get("sha256") for row in manifest.get("source_hashes", [])}
    actual_sources: dict[str, str] = {}
    for key, relative in SOURCE_FILES.items():
        path = root / relative
        exists = path.is_file()
        checks.add(f"source_exists:{key}", exists, relative.as_posix())
        if exists:
            actual = sha(path)
            actual_sources[relative.as_posix()] = actual
            checks.equal(f"source_hash:{key}", declared.get(relative.as_posix()), actual,
                         f"declared={declared.get(relative.as_posix())}; actual={actual}")

    tool_hashes = {row.get("path"): row.get("sha256") for row in manifest.get("tool_hashes", [])}
    for tool_relative in (Path("Scripts/BuildRokCityPlacementBottomSource.py"), Path("Scripts/VerifyRokCityPlacementBottomSource.py")):
        tool_path = root / tool_relative
        checks.equal(f"tool_hash:{tool_relative.name}", tool_hashes.get(tool_relative.as_posix()),
                     sha(tool_path) if tool_path.is_file() else None)

    # Independently parse all critical source evidence instead of accepting the
    # builder's generated fields as proof.
    try:
        mesh = parse_mesh_independently(root / SOURCE_FILES["mesh"])
        source_mesh = manifest.get("source", {}).get("unity_mesh", {})
        checks.equal("mesh_topology", [mesh["vertex_count"], mesh["index_count"], mesh["topology"]], [4, 6, "triangles"])
        checks.equal("mesh_source_indices", mesh["indices_uint16_source_order"], [0, 1, 2, 0, 2, 3])
        checks.equal("mesh_manifest_channels", source_mesh.get("serialized_channel_slots"), mesh["serialized_channel_slots"])
        checks.equal("mesh_coordinate_channel_is_unknown", source_mesh.get("coordinate_channel", {}).get("semantic"), mesh["coordinate_channel"]["semantic"])
        checks.equal("mesh_manifest_preserves_source_winding", source_mesh.get("indices_uint16_source_order"), mesh["indices_uint16_source_order"])
        manifest_vertices = source_mesh.get("vertices", [])
        checks.equal("mesh_vertex_count_in_manifest", len(manifest_vertices), len(mesh["vertices"]))
        for index, expected in enumerate(mesh["vertices"]):
            actual = manifest_vertices[index] if index < len(manifest_vertices) else {}
            checks.equal(f"mesh_vertex_{index}_source_raw", actual.get("source_vertex_bytes_hex"), expected["source_vertex_bytes_hex"])
            checks.equal(f"mesh_vertex_{index}_source_position", actual.get("source_unity_xyz"), expected["source_unity_xyz"])
            checks.equal(f"mesh_vertex_{index}_channel3_bytes", actual.get("source_serialized_channel_3_coord"), expected["source_serialized_channel_3_coord"])
            checks.equal(f"mesh_vertex_{index}_ue_conversion", actual.get("ue_local_xyz_from_unity_minus_x_z_y_times_500"), expected["ue_local_xyz_from_unity_minus_x_z_y_times_500"])
        checks.equal("mesh_size_metadata", [mesh["vertex_stride_bytes"], mesh["vertex_data_size_bytes"], mesh["m_current_channels"]],
                     [source_mesh.get("vertex_stride_bytes"), source_mesh.get("vertex_data_size_bytes"), source_mesh.get("m_current_channels")])
    except Exception as exc:
        checks.add("mesh_independent_parse", False, str(exc))

    try:
        prefab = read_source_text(root / SOURCE_FILES["prefab"])
        mesh_meta = read_source_text(root / SOURCE_FILES["mesh_meta"])
        material = read_source_text(root / SOURCE_FILES["material"])
        texture_meta = read_source_text(root / SOURCE_FILES["texture_meta"])
        shader = read_source_text(root / SOURCE_FILES["shader"])
        shader_meta = read_source_text(root / SOURCE_FILES["shader_meta"])
        mediator = read_source_text(root / SOURCE_FILES["city_mediator"])
        color_script = read_source_text(root / SOURCE_FILES["color_script"])
        mesh_guid = match(r"\bguid:\s*([0-9a-fA-F]{32})", mesh_meta).group(1).lower()
        meta_file_id = int(match(r"^\s*mainObjectFileID:\s*(\d+)", mesh_meta).group(1))
        checks.equal("mesh_meta_main_object_id", manifest.get("source", {}).get("unity_mesh", {}).get("subasset_file_id"), meta_file_id)
        checks.equal("mesh_manifest_source_id_name", [manifest.get("source", {}).get("unity_mesh", {}).get("subasset_file_id"),
                                                       manifest.get("source", {}).get("unity_mesh", {}).get("subasset_name")],
                     [mesh.get("subasset_file_id"), mesh.get("subasset_name")])
        texture_guid = match(r"\bguid:\s*([0-9a-fA-F]{32})", texture_meta).group(1).lower()
        prefab_mesh = match(r"m_Mesh:\s*\{fileID:\s*(\d+),\s*guid:\s*([0-9a-fA-F]{32})", prefab)
        prefab_mat = match(r"m_Materials:\s*\n\s*-\s*\{fileID:\s*(\d+),\s*guid:\s*([0-9a-fA-F]{32})", prefab)
        checks.equal("prefab_mesh_guid_chain", [prefab_mesh.group(1), prefab_mesh.group(2).lower(), mesh_guid],
                     ["4300000", "7c5e0bb563f70cd428c626f7920d38e2", "7c5e0bb563f70cd428c626f7920d38e2"])
        checks.equal("prefab_material_guid", prefab_mat.group(2).lower(), "246cfae25d324044196fe9b013ca2c1d")
        texture_section = match(r"- _MainTex:\s*\n([\s\S]*?)(?=\n\s*-\s*_\w+|\n\s*m_Floats:)", material).group(1)
        material_texture = match(r"m_Texture:.*?guid:\s*([0-9a-fA-F]{32})", texture_section).group(1).lower()
        scale = [float(v) for v in match(r"m_Scale:\s*\{x:\s*([^,]+),\s*y:\s*([^}]+)\}", texture_section).groups()]
        material_shader = match(r"m_Shader:.*?guid:\s*([0-9a-fA-F]{32})", material).group(1).lower()
        source_shader_guid = match(r"\bguid:\s*([0-9a-fA-F]{32})", shader_meta).group(1).lower()
        checks.equal("material_shader_guid_chain", [material_shader, source_shader_guid],
                     ["3b6d16a8b11aa44569752b7b1d11550f"] * 2)
        checks.equal("material_texture_guid_chain", [material_texture, texture_guid], ["6cb11efe336bf4859a4dbd8b1acdb413"] * 2)
        checks.equal("material_initial_uv_repeat", scale, [7.0, 7.0])
        base_color = [float(v) for v in match(r"(?ms)^\s*- _Color:\s*\{r:\s*([^,]+),\s*g:\s*([^,]+),\s*b:\s*([^,]+),\s*a:\s*([^}]+)\}", material).groups()]
        checks.equal("material_base_color", manifest.get("source", {}).get("material", {}).get("base_color_rgba"), base_color)
        import_settings = {
            "sRGBTexture": int(match(r"^\s*sRGBTexture:\s*(\d+)", texture_meta).group(1)),
            "enableMipMap": int(match(r"^\s*enableMipMap:\s*(\d+)", texture_meta).group(1)),
            "textureFormat": int(match(r"^\s*textureFormat:\s*(\d+)", texture_meta).group(1)),
            "wrapU": int(match(r"^\s*wrapU:\s*(\d+)", texture_meta).group(1)),
            "wrapV": int(match(r"^\s*wrapV:\s*(\d+)", texture_meta).group(1)),
            "wrapW": int(match(r"^\s*wrapW:\s*(\d+)", texture_meta).group(1)),
            "spriteMode": int(match(r"^\s*spriteMode:\s*(\d+)", texture_meta).group(1)),
            "spriteMeshType": int(match(r"^\s*spriteMeshType:\s*(\d+)", texture_meta).group(1)),
            "spritePixelsToUnits": int(match(r"^\s*spritePixelsToUnits:\s*(\d+)", texture_meta).group(1)),
            "textureType": int(match(r"^\s*textureType:\s*(\d+)", texture_meta).group(1)),
            "alphaUsage": int(match(r"^\s*alphaUsage:\s*(\d+)", texture_meta).group(1)),
            "alphaIsTransparency": int(match(r"^\s*alphaIsTransparency:\s*(\d+)", texture_meta).group(1)),
        }
        expected_import = manifest.get("source", {}).get("texture", {}).get("import_settings")
        checks.equal("texture_import_settings", expected_import, import_settings)
        expected_sampling = {"sRGBTexture": 1, "enableMipMap": 0, "wrapU": 0, "wrapV": 0, "wrapW": 0}
        checks.add("texture_repeat_semantics", all(import_settings.get(key) == value for key, value in expected_sampling.items()),
                   f"settings={import_settings}")
        checks.add("texture_alpha_import_semantics", import_settings.get("alphaUsage") == 1
                   and import_settings.get("alphaIsTransparency") == 1,
                   f"alphaUsage={import_settings.get('alphaUsage')}; alphaIsTransparency={import_settings.get('alphaIsTransparency')}")
        png = parse_png_independently(root / SOURCE_FILES["texture"])
        checks.equal("png_dimensions_and_alpha", manifest.get("source", {}).get("texture", {}).get("png"), png)
        checks.add("png_has_alpha_channel", png["has_intrinsic_or_palette_alpha"], f"color_type={png['color_type']} tRNS={png['has_trns_chunk']}")
        checks.add("shader_alpha_blend_formula", bool(re.search(r"Blend\s+SrcAlpha\s+OneMinusSrcAlpha\s*,\s*SrcAlpha\s+OneMinusSrcAlpha", shader))
                   and bool(re.search(r"\bZWrite\s+Off\b", shader))
                   and bool(re.search(r"tex2D\s*\(\s*_MainTex\s*,\s*i\.uv\s*\)\s*\*\s*_Color", shader))
                   and bool(re.search(r"return\s+col\s*;", shader)), "shader blend/tint/alpha source expressions present")
        checks.add("exact_red_green_calls", "ChangeSpriteColor.SetColor(cc, Color.red)" in mediator
                   and "ChangeSpriteColor.SetColor(cc, Color.green)" in mediator,
                   "true->red and false->green source code calls")
        checks.add("property_block_tint_path", "GetPropertyBlock(materialPropertyBlock)" in color_script
                   and "SetColor(\"_Color\", color)" in color_script
                   and "SetPropertyBlock(materialPropertyBlock)" in color_script,
                   "runtime tint writes material _Color via MaterialPropertyBlock")
        runtime_scale_call = bool(re.search(r'SetTextureScale\("_MainTex",\s*size\)', mediator))
        footprint_mesh_scale = bool(re.search(r"localScale\s*=\s*new Vector3\(size\.x,\s*0\.01f,\s*size\.y\)", mediator))
        checks.add("runtime_texture_scale_request_from_footprint", runtime_scale_call,
                   "Mediator sets the material texture-scale state from footprint size")
        checks.add("mesh_scale_from_footprint", footprint_mesh_scale,
                   "bottom quad geometry scales with footprint width and length")
        identity_uv = bool(re.search(r"\bo\.uv\s*=\s*v\.uv\s*;", shader))
        uses_st = bool(re.search(r"\b_MainTex_ST\b", shader))
        uses_transform = bool(re.search(r"\bTRANSFORM_TEX\s*\(", shader))
        shader_contract = manifest.get("source", {}).get("shader", {})
        checks.add("shader_effective_uv_is_identity", identity_uv and not uses_st and not uses_transform
                   and shader_contract.get("vertex_uv_assignment") == "o.uv = v.uv;"
                   and shader_contract.get("uses_maintex_st") is False
                   and shader_contract.get("uses_transform_tex") is False
                   and shader_contract.get("effective_sampling_uv") == "identity_mesh_uv",
                   "source shader forwards mesh UV unchanged and contains no texture-scale transform")
        checks.add("runtime_texture_scale_is_not_effective_uv_tiling", runtime_scale_call and identity_uv
                   and not uses_st and not uses_transform
                   and shader_contract.get("runtime_texture_scale_affects_sampling") is False
                   and manifest.get("source", {}).get("placement_behavior", {}).get("effective_footprint_uv_tiling") is False,
                   "SetTextureScale is recorded as runtime state; effective sampling stays identity UV")
        checks.add("follow_selected_local_position", "m_bottomBuildObj.transform.localPosition = m_selectBuild.localPosition" in mediator,
                   "bottom indicator follows snapped building root")
        checks.add("shader_contract_matches_manifest", manifest.get("source", {}).get("shader", {}).get("formula")
                   == "col = tex2D(_MainTex, uv) * _Color; return col",
                   "recorded formula is literal source behavior, with alpha from multiplied texture/tint alpha")
        transform = manifest.get("source", {}).get("prefab", {}).get("root_transform", {})
        source_transform_match = match(r"(?ms)^--- !u!4 &\d+\nTransform:\s*\n.*?m_LocalPosition:\s*\{x:\s*([^,]+),\s*y:\s*([^,]+),\s*z:\s*([^}]+)\}.*?m_LocalScale:\s*\{x:\s*([^,]+),\s*y:\s*([^,]+),\s*z:\s*([^}]+)\}.*?m_LocalEulerAnglesHint:\s*\{x:\s*([^,]+),\s*y:\s*([^,]+),\s*z:\s*([^}]+)\}", prefab)
        source_transform_values = [float(v) for v in source_transform_match.groups()]
        source_transform = {"local_position_unity": source_transform_values[0:3],
                            "local_scale_unity": source_transform_values[3:6],
                            "local_euler_degrees_unity": source_transform_values[6:9]}
        checks.equal("manifest_prefab_transform_matches_source", transform, source_transform)
        checks.equal("prefab_source_transform", transform,
                     {"local_position_unity": [0.0, 0.0, 0.0], "local_scale_unity": [1.0, 1.0, 1.0], "local_euler_degrees_unity": [0.0, 45.0, 0.0]})
        city_data = read_source_text(root / SOURCE_FILES["city_obj_data"])
        grid_size = float(match(r"CITY_GRID_SIZE\s*=\s*([0-9.]+)F", city_data).group(1))
        grid_half = float(match(r"CITY_GRID_SIZE_HALF\s*=\s*([0-9.]+)F", city_data).group(1))
        behavior = manifest.get("source", {}).get("placement_behavior", {})
        checks.equal("source_grid_metrics", [behavior.get("grid_size_unity"), behavior.get("grid_half_unity")], [grid_size, grid_half])
        checks.add("source_grid_center_formula", bool(re.search(
            r"xLogic\s*\*\s*CityObjData\.CITY_GRID_SIZE\s*\+\s*size\.x\s*\*\s*CityObjData\.CITY_GRID_SIZE_HALF\s*-\s*CityObjData\.CITY_GRID_SIZE_HALF", mediator))
            and bool(re.search(
            r"yLogic\s*\*\s*CityObjData\.CITY_GRID_SIZE\s*\+\s*size\.y\s*\*\s*CityObjData\.CITY_GRID_SIZE_HALF\s*-\s*CityObjData\.CITY_GRID_SIZE_HALF", mediator)),
            "source tile-to-local center formula exists for both grid axes")
        checks.equal("manifest_valid_invalid_colors", [behavior.get("invalid_color_rgba"), behavior.get("valid_color_rgba")],
                     [[1.0, 0.0, 0.0, 1.0], [0.0, 1.0, 0.0, 1.0]])
    except Exception as exc:
        checks.add("prefab_material_texture_shader_reparse", False, str(exc))

    conversion = manifest.get("coordinate_conversion", {})
    checks.equal("source_to_ue_conversion_definition", conversion.get("formula"), "UE = (-UnityX, UnityZ, UnityY) * 500")
    checks.equal("source_to_ue_winding_policy", conversion.get("linear_determinant"), 1,
                 "both axis swap and X reflection yield net orientation-preserving basis")
    checks.equal("no_extra_winding_flip", conversion.get("winding_policy"),
                 "preserve source index order; basis determinant is +1, so no winding flip is required")
    source_channel = manifest.get("source", {}).get("unity_mesh", {}).get("coordinate_channel", {})
    checks.equal("coordinate_semantic_not_overclaimed", source_channel.get("semantic"),
                 "unknown; source corner coordinates recorded without asserting Unity TexCoord index")

    return {"manifest": manifest, "checks": checks.rows}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--out", type=Path, default=DEFAULT_REPORT)
    args = parser.parse_args()
    root = args.root.resolve()
    manifest_path = args.manifest if args.manifest.is_absolute() else root / args.manifest
    report_path = args.out if args.out.is_absolute() else root / args.out
    results = run_checks(root, manifest_path)
    rows = results["checks"]
    failed = [row for row in rows if not row["passed"]]
    report = {
        "schema": "rok.city_placement_bottom_source_verify.v2",
        "verified_utc": datetime.now(timezone.utc).isoformat(),
        "status": "failed" if failed else "passed",
        "manifest_path": str(manifest_path),
        "manifest_sha256": sha(manifest_path) if manifest_path.is_file() else None,
        "checks_total": len(rows), "checks_passed": len(rows) - len(failed), "checks_failed": len(failed),
        "checks": rows,
        "failures": failed,
        "scope": "Independent static Unity-source/manifest verification only. No UE asset import, build, editor, or runtime rendering acceptance.",
    }
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"{report['status'].upper()}: {report['checks_passed']}/{report['checks_total']} checks; report={report_path}")
    return 0 if not failed else 1


if __name__ == "__main__":
    raise SystemExit(main())
