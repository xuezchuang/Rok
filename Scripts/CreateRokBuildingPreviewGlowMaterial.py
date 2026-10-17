"""Create the dedicated Unity GPU-decoded star003 UI glow candidate.

The original PNG remains provenance. Native Unity D3D11 Gamma sampling has
different texels after alpha dilation and DXT5 decode, so the UE texture imports
the verified pass-through sampled mip0 PNG without a second UE compression.
Run in the UE 5.8 editor on L_RokUnityGameAppCity only.
"""

from pathlib import Path
import hashlib
import json
import struct
import zlib

import unreal


PROJECT = Path("D:/ueDemo/Rok").resolve()
TEXTURE_SOURCE = Path(
    "ref/client-unity/Assets/BundleAssets/Effect/UIsprite/shared_uisprite/star003.png"
)
SOURCE_SHA256 = "9f159150cc1614e2eb38f88217433d2c68b1d28d0b73910ecd249f8f39c40388"
NATIVE_DIR = Path("Saved/RokUnityMigration/BuildingGlowTexture")
DECODED_SHA256 = "a91b4c11aa4841d7536a475e7a39f3a488c5ea582ea12401bfa5f11586e44eaf"
DESTINATION = "/Game/RokPrototype/UI/Materials"
TEXTURE_NAME = "T_RokBuildingPreviewGlowStar003Raw"
MATERIAL_NAME = "M_RokBuildingPreviewGlowUI"


def require_identity():
    actual = Path(unreal.Paths.project_dir()).resolve()
    world = unreal.EditorLevelLibrary.get_editor_world()
    if actual != PROJECT or world is None or world.get_name() != "L_RokUnityGameAppCity":
        raise RuntimeError(
            "Glow asset write refused: expected D:/ueDemo/Rok and "
            f"L_RokUnityGameAppCity; got {actual} / "
            f"{world.get_name() if world else '<no editor world>'}"
        )


def decode_png_rgba(source, expected_sha256):
    """Validate PNG bytes, CRC, 256x256 RGBA8 layout and decode raw pixels."""
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected_sha256:
        raise RuntimeError(f"Glow PNG SHA256 changed: {source}")
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError(f"Glow input is not PNG: {source}")
    chunks = []
    offset = 8
    while offset + 12 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
        if len(payload) != length or zlib.crc32(kind + payload) & 0xffffffff != crc:
            raise RuntimeError(f"Glow PNG chunk failed CRC: {source}")
        chunks.append((kind, payload))
        offset += 12 + length
        if kind == b"IEND":
            break
    if not chunks or chunks[0][0] != b"IHDR" or chunks[-1][0] != b"IEND":
        raise RuntimeError(f"Glow PNG chunks incomplete: {source}")
    width, height, depth, color, compression, filtering, interlace = struct.unpack(
        ">IIBBBBB", chunks[0][1]
    )
    if (width, height, depth, color, compression, filtering, interlace) != (256, 256, 8, 6, 0, 0, 0):
        raise RuntimeError(f"Glow PNG pixel layout changed: {source}")
    stream = zlib.decompress(b"".join(p for k, p in chunks if k == b"IDAT"))
    stride = width * 4
    if len(stream) != height * (stride + 1):
        raise RuntimeError(f"Glow PNG scanline size changed: {source}")
    previous = bytearray(stride)
    rgba = bytearray()
    for y in range(height):
        at = y * (stride + 1)
        filter_type = stream[at]
        row = bytearray(stream[at + 1:at + 1 + stride])
        if filter_type > 4:
            raise RuntimeError(f"Glow PNG filter invalid: {source}")
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
        rgba.extend(row)
        previous = row
    return data, rgba


def validate_native_candidate():
    native_path = PROJECT / NATIVE_DIR / "native.json"
    verify_path = PROJECT / NATIVE_DIR / "verify.json"
    native_bytes = native_path.read_bytes()
    native = json.loads(native_bytes)
    verify = json.loads(verify_path.read_bytes())
    if (native.get("schema") != "rok.building.glow_texture.native.v1"
            or verify.get("schema") != "rok.building.glow_texture.verify.v1"
            or verify.get("passed") is not True or verify.get("failures") != []
            or native.get("failure") != ""
            or hashlib.sha256(native_bytes).hexdigest() != verify.get("native_sha256")):
        raise RuntimeError("Native glow texture probe failed or verification is stale")
    if (native.get("sourceSha256") != SOURCE_SHA256
            or verify.get("source_png_sha256") != SOURCE_SHA256
            or native.get("sampledPngSha256") != DECODED_SHA256):
        raise RuntimeError("Native glow source/sample identity changed")
    expected_runtime = {
        "width": 256, "height": 256, "mipmapCount": 1,
        "format": "DXT5", "graphicsFormat": "RGBA_DXT5_UNorm",
        "filterMode": "Bilinear", "wrapMode": "Clamp",
        "wrapModeU": "Clamp", "wrapModeV": "Clamp",
        "importerSRGB": True, "alphaIsTransparency": True,
        "importerMipmaps": False, "importerCompression": "Compressed",
    }
    runtime = native.get("texture")
    if (not isinstance(runtime, dict)
            or verify.get("runtime_texture") != runtime
            or any(runtime.get(key) != value for key, value in expected_runtime.items())
            or native.get("unityVersion") != "2019.4.28f1c1"
            or native.get("colorSpace") != "Gamma"
            or native.get("graphicsAPI") != "Direct3D11"
            or native.get("passThroughGPUReadback") is not True
            or native.get("rgbWriteDisabled") is not True
            or native.get("sampledRgbaByteCount") != 256 * 256 * 4):
        raise RuntimeError("Native glow GPU sampling contract changed")
    material = native.get("material")
    if (not isinstance(material, dict) or verify.get("source_material") != material
            or material.get("shader") != "UI/Add"
            or material.get("textureSampleAdd") != "(0.000000000, 0.000000000, 0.000000000, 0.000000000)"
            or material.get("hasTextureSampleAdd") is not False):
        raise RuntimeError("Native UI/Add source material changed")
    # rawReadbackFailure is diagnostic: the verified pass-through GPU mip0
    # readback succeeded. It is not the native probe's top-level failure.
    return native, verify, hashlib.sha256(native_bytes).hexdigest()


def validate_inputs():
    _, raw_rgba = decode_png_rgba(PROJECT / TEXTURE_SOURCE, SOURCE_SHA256)
    pixels_above_alpha = sum(
        max(raw_rgba[i:i + 3]) > raw_rgba[i + 3] for i in range(0, len(raw_rgba), 4)
    )
    if pixels_above_alpha != 53209:
        raise RuntimeError("Source star003 straight-alpha pixels changed")
    native, verify, native_sha = validate_native_candidate()
    sampled_path = PROJECT / NATIVE_DIR / "sampled_gpu_mip0.png"
    decoded_bytes, decoded_rgba = decode_png_rgba(sampled_path, native["sampledPngSha256"])
    # Unity's raw readback byte rows start at the opposite vertical edge from
    # the saved PNG. Confirm the PNG texels are exactly that native sample.
    row_bytes = 256 * 4
    native_row_order = b"".join(
        decoded_rgba[i:i + row_bytes]
        for i in range(len(decoded_rgba) - row_bytes, -1, -row_bytes)
    )
    if (hashlib.sha256(decoded_bytes).hexdigest() != DECODED_SHA256
            or len(decoded_rgba) != native["sampledRgbaByteCount"]
            or hashlib.sha256(native_row_order).hexdigest() != native.get("sampledRgbaSha256")
            or verify.get("gpu_vs_png", {}).get("exact_rgba_match") is not False
            or verify.get("gpu_vs_png", {}).get("changed_rgba_pixels") != 62811
            or verify.get("gpu_vs_png", {}).get("changed_alpha_pixels") != 15000):
        raise RuntimeError("Native GPU-decoded glow candidate changed")
    return sampled_path, native_sha


def create_assets():
    require_identity()
    sampled_path, native_sha = validate_inputs()
    texture_path = DESTINATION + "/" + TEXTURE_NAME
    material_path = DESTINATION + "/" + MATERIAL_NAME
    for path, expected in ((texture_path, unreal.Texture2D), (material_path, unreal.Material)):
        existing = unreal.load_asset(path)
        if existing is not None and not isinstance(existing, expected):
            raise RuntimeError(f"Dedicated glow asset type changed: {path}")

    unreal.EditorAssetLibrary.make_directory(DESTINATION)
    task = unreal.AssetImportTask()
    task.filename = str(sampled_path)
    task.destination_path = DESTINATION
    task.destination_name = TEXTURE_NAME
    task.automated = True
    task.replace_existing = True  # Reimport only this asset from verified native sampled bytes.
    task.save = False
    task.factory = unreal.TextureFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(texture_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Dedicated GPU-decoded glow texture import failed")
    properties = (
        ("srgb", False),
        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
        ("never_stream", True),
        ("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON),
        ("filter", unreal.TextureFilter.TF_BILINEAR),
        ("address_x", unreal.TextureAddress.TA_CLAMP),
        ("address_y", unreal.TextureAddress.TA_CLAMP),
    )
    for key, value in properties:
        texture.set_editor_property(key, value)
    # Both PNG IHDRs were checked as 256 x 256 above. UE's GetSize can still
    # expose a 32 x 32 default resource while this texture compiles; the widget
    # completes only this texture before validating its compiled dimensions.
    for key, value in properties:
        if texture.get_editor_property(key) != value:
            raise RuntimeError(f"Imported native-sampled star003 flag changed: {key}")
    unreal.EditorAssetLibrary.set_metadata_tag(
        texture, "RokGlowSourceStatus", "native-gpu-decoded-candidate"
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        texture, "RokGlowSourceSha256", SOURCE_SHA256
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        texture, "RokGlowNativePngSha256", DECODED_SHA256
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        texture, "RokGlowNativeReportSha256", native_sha
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        texture, "RokGlowParityGate", "UE-SDR-paint-pending"
    )
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError("Dedicated raw glow texture save failed")

    material = unreal.load_asset(material_path)
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            MATERIAL_NAME, DESTINATION, unreal.Material, unreal.MaterialFactoryNew()
        )
    if not isinstance(material, unreal.Material):
        raise RuntimeError("Dedicated glow material creation failed")
    edit = unreal.MaterialEditingLibrary
    edit.delete_all_material_expressions(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("two_sided", True)

    def node(kind):
        return edit.create_material_expression(material, kind)

    def connect(source_node, output, target_node, input_name):
        if not edit.connect_material_expressions(source_node, output, target_node, input_name):
            raise RuntimeError(f"Glow material connection failed: {input_name}")

    sample = node(unreal.MaterialExpressionTextureSampleParameter2D)
    sample.set_editor_property("parameter_name", "Sprite")
    sample.set_editor_property("texture", texture)
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    vertex = node(unreal.MaterialExpressionVertexColor)
    rgb = node(unreal.MaterialExpressionMultiply)
    opacity = node(unreal.MaterialExpressionMultiply)
    connect(sample, "RGB", rgb, "A")
    connect(vertex, "", rgb, "B")
    connect(sample, "A", opacity, "A")
    connect(vertex, "A", opacity, "B")
    if not edit.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Glow legacy UI color connection failed")
    if not edit.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError("Glow legacy UI opacity connection failed")
    ui = node(unreal.MaterialExpressionSubstrateUI)
    connect(rgb, "", ui, "Color")
    connect(opacity, "", ui, "Opacity")
    if not edit.connect_material_property(ui, "", unreal.MaterialProperty.MP_FRONT_MATERIAL):
        raise RuntimeError("Glow Substrate UI connection failed")
    unreal.EditorAssetLibrary.set_metadata_tag(
        material, "RokGlowSourceStatus", "native-gpu-decoded-candidate"
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        material, "RokGlowNativePngSha256", DECODED_SHA256
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        material, "RokGlowNativeReportSha256", native_sha
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        material, "RokGlowParityGate", "UE-SDR-paint-pending"
    )
    edit.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Dedicated glow material save failed")
    if (material.get_editor_property("material_domain") != unreal.MaterialDomain.MD_UI
            or material.get_editor_property("blend_mode") != unreal.BlendMode.BLEND_ADDITIVE
            or sample.get_editor_property("sampler_type") != unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
            or edit.get_material_property_input_node(
                material, unreal.MaterialProperty.MP_FRONT_MATERIAL
            ) != ui):
        raise RuntimeError("Dedicated glow material properties changed after save")
    for asset in (texture, material):
        if unreal.EditorAssetLibrary.get_metadata_tag(asset, "RokGlowSourceStatus") != "native-gpu-decoded-candidate":
            raise RuntimeError("Dedicated glow candidate metadata failed")
    unreal.log(
        "ROK_BUILDING_PREVIEW_GLOW_ASSETS native-gpu-decoded-candidate "
        + texture_path + " " + material_path
        + " UE SDR paint parity pending"
    )


create_assets()
