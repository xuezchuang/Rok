"""Upgrade only the existing mountain F body material from a verified 11-mip DDS.

Execute in UE 5.8 with L_RokUnityWorldMapPrototype open, after running
PrepareRokMountainFDecodedDds.py. Does not save maps or change actor transforms.
The imported texture's compiled mip count and visual parity remain runtime gates.
"""

from pathlib import Path
import hashlib
import json
import re
import shutil
import struct

import unreal


ROOT = Path("D:/ueDemo/Rok").resolve()
PRODUCT = ROOT / "Saved/RokUnityMigration/MountainFGammaProduct"
PROBE = ROOT / "Saved/RokUnityMigration/NextWorldMaterialSlice/ue_binding_probe.json"
SOURCE_META = ROOT / "ref/client-unity/Assets/BundleAssets/land/Texture/build_mountain_F.png.meta"
TEXTURE_DESTINATION = "/Game/RokUnityWorldMapPrototype/Textures"
TEXTURE_NAME = "T_MountainF_UnityDecodedMips"
TEXTURE_PATH = TEXTURE_DESTINATION + "/" + TEXTURE_NAME
MATERIAL_PATH = "/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_build_mountain_F"
MATERIAL_OBJECT = MATERIAL_PATH + ".M_WorldSource_build_mountain_F"
BODY_MESH = (
    "/Game/RokUnityWorldMapPrototype/Meshes/MountainBodies/"
    "SM_RokWorldBody_build_mountain_F_build_mountain_F."
    "SM_RokWorldBody_build_mountain_F_build_mountain_F"
)
BASELINE_UASSET_SHA256 = "2a1c3abb3667a7f93c9e63002ed31587c7e75ad68cb038f3a91a38590b943b2a"
CODE = '''float3 g = RawRGB * Lighting;
return float3(g.r <= 0.04045 ? g.r / 12.92 : pow((g.r + 0.055) / 1.055, 2.4),
              g.g <= 0.04045 ? g.g / 12.92 : pow((g.g + 0.055) / 1.055, 2.4),
              g.b <= 0.04045 ? g.b / 12.92 : pow((g.b + 0.055) / 1.055, 2.4));'''


def digest(data):
    return hashlib.sha256(data).hexdigest()


def identity():
    root = Path(unreal.Paths.project_dir()).resolve()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if root != ROOT or world is None or world.get_name() != "L_RokUnityWorldMapPrototype":
        raise RuntimeError(f"Mountain F write refused: project={root}, world={world.get_name() if world else None}")
    return world


def verified_dds():
    manifest_path = PRODUCT / "prepare_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if (manifest.get("schema") != "rok.mountain_f_gamma_product.prepare.v1"
            or manifest.get("status") != "prepared-not-imported"
            or manifest.get("mip_count") != 11
            or manifest.get("dds_format") != "DX10 B8G8R8A8_UNORM 2D"
            or manifest.get("source_orientation") != "as_saved"
            or manifest.get("dds_extra_vertical_flip") is not False
            or manifest.get("ue_asset_import_verified") is not False
            or manifest.get("dds_file") != "T_MountainF_UnityDecodedMips.dds"):
        raise RuntimeError("Mountain F product manifest missing or unverified")
    inputs = ROOT / "Saved/RokUnityMigration/MountainFGammaV2"
    hashes = manifest.get("source_hashes", {})
    for file_name, key in (("verify.json", "verify_sha256"),
                           ("native_render.json", "native_sha256"),
                           ("prepare.json", "prepare_sha256")):
        if digest((inputs / file_name).read_bytes()) != hashes.get(key):
            raise RuntimeError(f"Mountain F product source changed: {file_name}")
    if digest((ROOT / "ref/client-unity/Assets/BundleAssets/land/Texture/build_mountain_F.png").read_bytes()) != hashes.get("source_png_sha256"):
        raise RuntimeError("Mountain F original PNG changed")
    dds_path = PRODUCT / manifest["dds_file"]
    data = dds_path.read_bytes()
    if digest(data) != manifest.get("dds_sha256") or len(data) != manifest.get("dds_bytes"):
        raise RuntimeError("Mountain F DDS hash or length changed")
    if data[:4] != b"DDS " or len(data) < 148:
        raise RuntimeError("Mountain F DDS header missing")
    if struct.unpack_from("<7I", data, 4) != (124, 0x2100F, 1024, 1024, 4096, 0, 11):
        raise RuntimeError("Mountain F DDS geometry/mips changed")
    if data[84:88] != b"DX10" or struct.unpack_from("<5I", data, 128) != (87, 3, 0, 1, 0):
        raise RuntimeError("Mountain F DDS is not DX10 BGRA8 2D")
    mips = manifest.get("mips")
    if not isinstance(mips, list) or len(mips) != 11:
        raise RuntimeError("Mountain F DDS manifest requires 11 mips")
    at = 148
    for level, mip in enumerate(mips):
        size = max(1, 1024 >> level)
        count = size * size * 4
        if (mip.get("level") != level or mip.get("size") != [size, size]
                or mip.get("bytes") != count
                or mip.get("source_orientation") != "as_saved"
                or mip.get("orientation_evidence") != ("single-pixel-inherited" if size == 1 else "source-PNG-bilinear-downsample")
                or digest(data[at:at + count]) != mip.get("bgra8_sha256")):
            raise RuntimeError(f"Mountain F DDS mip {level} differs from manifest")
        at += count
    if at != len(data):
        raise RuntimeError("Mountain F DDS has unexpected trailing data")
    return dds_path, manifest


def expected_bindings():
    probe = json.loads(PROBE.read_text(encoding="utf-8"))
    if (probe.get("completed") is not True or probe.get("read_only") is not True
            or Path(probe.get("project", "")).resolve() != ROOT
            or probe.get("world") != "/Game/Maps/L_RokUnityWorldMapPrototype"):
        raise RuntimeError("Historic F-body binding probe invalid")
    expected = {}
    for item in probe.get("actors", []):
        label = item.get("label", "")
        if label.endswith("_build_mountain_F"):
            if (label in expected or item.get("mesh") != BODY_MESH
                    or item.get("materials") != [MATERIAL_OBJECT]):
                raise RuntimeError(f"Historic F-body binding differs: {label}")
            expected[label] = item
    if len(expected) != 18:
        raise RuntimeError("Historic probe must contain exactly 18 F body actors")
    materials = probe.get("materials", {})
    record = materials.get(MATERIAL_OBJECT)
    if (not isinstance(record, dict) or record.get("class") != "Material"
            or record.get("blend_mode") != "<BlendMode.BLEND_OPAQUE: 0>"
            or record.get("two_sided") is not False
            or len(record.get("expressions", [])) != 3):
        raise RuntimeError("Historic F material graph contract changed")
    texture_rows = [row for row in record["expressions"] if row.get("type") == "MaterialExpressionTextureSample"]
    vector_rows = [row for row in record["expressions"] if row.get("type") == "MaterialExpressionVectorParameter"]
    if len(texture_rows) != 1 or len(vector_rows) != 1 or vector_rows[0].get("parameter_name") != "UnitySpriteColor":
        raise RuntimeError("Historic F texture/light graph missing")
    texture_match = re.search(r"'(/Game/[^']+)'", texture_rows[0].get("texture", ""))
    if not texture_match:
        raise RuntimeError("Historic F source texture path missing")
    return expected, texture_match.group(1)


def source_import_settings():
    text = SOURCE_META.read_text(encoding="utf-8-sig")
    values = {}
    for key in ("filterMode", "wrapU", "wrapV", "enableMipMap", "sRGBTexture"):
        matches = re.findall(r"^\s*" + key + r":\s*(-?\d+)\s*$", text, re.MULTILINE)
        if len(matches) != 1:
            raise RuntimeError(f"Mountain F source importer field ambiguous: {key}")
        values[key] = int(matches[0])
    if ("guid: b9d81a5ad9e692c499abcf99c1cabd63" not in text
            or values != {"filterMode": -1, "wrapU": -1, "wrapV": -1,
                          "enableMipMap": 1, "sRGBTexture": 1}):
        raise RuntimeError("Mountain F Unity import semantics changed")
    return values, digest(SOURCE_META.read_bytes())


def close_triplet(actual, expected):
    return len(actual) == len(expected) == 3 and all(abs(float(a) - float(b)) <= 1e-4
                                      for a, b in zip(actual, expected))


def current_bindings(expected):
    found = {}
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        label = actor.get_actor_label()
        if not label.endswith("_build_mountain_F"):
            continue
        if label not in expected or label in found:
            raise RuntimeError(f"Unexpected or duplicate live F body: {label}")
        meshes = actor.get_components_by_class(unreal.StaticMeshComponent)
        if len(meshes) != 1:
            raise RuntimeError(f"F body must have one mesh component: {label}")
        component = meshes[0]
        mesh = component.get_editor_property("static_mesh")
        material = component.get_material(0)
        tags = [str(tag) for tag in actor.get_editor_property("tags")]
        location = actor.get_actor_location()
        rotation = actor.get_actor_rotation()
        scale = actor.get_actor_scale3d()
        location_xyz = [location.x, location.y, location.z]
        rotation_pyr = [rotation.pitch, rotation.yaw, rotation.roll]
        scale_xyz = [scale.x, scale.y, scale.z]
        if (mesh is None or mesh.get_path_name() != expected[label]["mesh"]
                or component.get_num_materials() != 1
                or material is None or material.get_path_name() != MATERIAL_OBJECT
                or sorted(tags) != sorted(expected[label]["tags"])
                or not close_triplet(location_xyz, expected[label]["location"])
                or not close_triplet(rotation_pyr, expected[label]["rotation"])
                or not close_triplet(scale_xyz, expected[label]["scale"])):
            raise RuntimeError(f"Live F body binding differs: {label}")
        found[label] = {
            "label": label, "tags": tags,
            "mesh": mesh.get_path_name(), "material": material.get_path_name(),
            "location": location_xyz, "rotation": rotation_pyr, "scale": scale_xyz,
        }
    if len(found) != 18 or set(found) != set(expected):
        raise RuntimeError("Live world does not have the historic 18 F bodies")
    return [found[label] for label in sorted(found)]


def baseline_material(expected_texture):
    file_path = ROOT / "Content/RokUnityWorldMapPrototype/Materials/M_WorldSource_build_mountain_F.uasset"
    if not file_path.is_file() or digest(file_path.read_bytes()) != BASELINE_UASSET_SHA256:
        raise RuntimeError("Original F material uasset missing or disk SHA changed")
    material = unreal.load_asset(MATERIAL_OBJECT)
    if not isinstance(material, unreal.Material):
        raise RuntimeError("Original F material failed to load")
    edit = unreal.MaterialEditingLibrary
    expressions = edit.get_material_expressions(material)
    samples = [e for e in expressions if isinstance(e, unreal.MaterialExpressionTextureSample)]
    lights = [e for e in expressions if isinstance(e, unreal.MaterialExpressionVectorParameter)
              and str(e.get_editor_property("parameter_name")) == "UnitySpriteColor"]
    multiplies = [e for e in expressions if isinstance(e, unreal.MaterialExpressionMultiply)]
    old_texture = samples[0].get_editor_property("texture") if len(samples) == 1 else None
    if (len(expressions) != 3 or len(samples) != 1 or len(lights) != 1 or len(multiplies) != 1
            or not isinstance(old_texture, unreal.Texture2D)
            or old_texture.get_path_name() != expected_texture
            or edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR) != multiplies[0]
            or material.get_editor_property("blend_mode") != unreal.BlendMode.BLEND_OPAQUE
            or material.get_editor_property("two_sided")):
        raise RuntimeError("Loaded original F material differs from disk/probe")
    return material, lights[0].get_editor_property("default_value"), old_texture, file_path


def backup_original(path):
    backup_dir = PRODUCT / "BeforeUpgradeBackup"
    backup_dir.mkdir(parents=True, exist_ok=True)
    target = backup_dir / f"M_WorldSource_build_mountain_F_{BASELINE_UASSET_SHA256}.uasset"
    if target.exists():
        if digest(target.read_bytes()) != BASELINE_UASSET_SHA256:
            raise RuntimeError("Mountain F baseline backup changed")
    else:
        shutil.copyfile(path, target)
    if digest(target.read_bytes()) != BASELINE_UASSET_SHA256:
        raise RuntimeError("Mountain F backup verification failed")
    return target


def upgrade():
    world = identity()
    dds_path, manifest = verified_dds()
    expected, source_texture = expected_bindings()
    before = current_bindings(expected)
    import_settings, source_meta_sha = source_import_settings()
    material, sprite_color, old_texture, uasset = baseline_material(source_texture)
    # Unity's -1 U/V are inherited importer settings, not explicit Clamp.
    # Preserve the current UE source texture's actual address modes deliberately.
    address_x = old_texture.get_editor_property("address_x")
    address_y = old_texture.get_editor_property("address_y")
    if unreal.load_asset(TEXTURE_PATH) is not None:
        raise RuntimeError("Dedicated F decoded-mip texture already exists; refusing overwrite")
    backup = backup_original(uasset)

    # All source, identity, actor and original-asset checks precede mutation.
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(dds_path))
    task.set_editor_property("destination_path", TEXTURE_DESTINATION)
    task.set_editor_property("destination_name", TEXTURE_NAME)
    for key, value in dict(automated=True, replace_existing=False, save=False,
                           factory=unreal.TextureFactory()).items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(TEXTURE_PATH)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("F decoded-mip DDS did not import as Texture2D")
    flags = {
        "srgb": False,
        "compression_settings": unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_LEAVE_EXISTING_MIPS,
        "filter": unreal.TextureFilter.TF_BILINEAR,
        "address_x": address_x,
        "address_y": address_y,
        "max_texture_size": 1024,
        "never_stream": True,
    }
    for key, value in flags.items():
        texture.set_editor_property(key, value)
    if any(texture.get_editor_property(key) != value for key, value in flags.items()):
        raise RuntimeError("F decoded texture settings changed before save")
    unreal.EditorAssetLibrary.set_metadata_tag(texture, "RokMountainFSourceStatus", "native-gpu-decoded-11mip-candidate")
    unreal.EditorAssetLibrary.set_metadata_tag(texture, "RokMountainFProductDDS", manifest["dds_sha256"])
    unreal.EditorAssetLibrary.set_metadata_tag(texture, "RokMountainFRuntimeGate", "compiled-11mip-and-visual-pending")
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError("F decoded texture save failed")

    edit = unreal.MaterialEditingLibrary
    edit.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", False)
    sample = edit.create_material_expression(material, unreal.MaterialExpressionTextureSample)
    sample.set_editor_property("texture", texture)
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    light = edit.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
    light.set_editor_property("parameter_name", "UnitySpriteColor")
    light.set_editor_property("default_value", sprite_color)
    shader = edit.create_material_expression(material, unreal.MaterialExpressionCustom)
    shader.set_editor_property("code", CODE)
    shader.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in ("RawRGB", "Lighting"):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        inputs.append(entry)
    shader.set_editor_property("inputs", inputs)
    for source, output, input_name in ((sample, "RGB", "RawRGB"), (light, "RGB", "Lighting")):
        if not edit.connect_material_expressions(source, output, shader, input_name):
            raise RuntimeError(f"F material graph connection failed: {input_name}")
    if not edit.connect_material_property(shader, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("F material emissive connection failed")
    unreal.EditorAssetLibrary.set_metadata_tag(material, "RokMountainFSourceStatus", "native-gpu-decoded-11mip-candidate")
    unreal.EditorAssetLibrary.set_metadata_tag(material, "RokMountainFProductDDS", manifest["dds_sha256"])
    unreal.EditorAssetLibrary.set_metadata_tag(material, "RokMountainFBackup", str(backup))
    edit.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("F material save failed")
    if (edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR) != shader
            or sample.get_editor_property("sampler_type") != unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
            or sample.get_editor_property("texture") != texture
            or light.get_editor_property("default_value") != sprite_color):
        raise RuntimeError("F material graph changed after save")
    after = current_bindings(expected)
    if after != before:
        raise RuntimeError("Mountain F actors changed during isolated material upgrade")
    post_world = identity()
    report = {
        "schema": "rok.mountain_f_gamma_product.ue_upgrade.v1",
        "project": str(ROOT), "world": world.get_name(),
        "scope": "Only existing F body material and dedicated decoded-mip texture; no map save or actor mutation.",
        "pre_identity": {"project": str(ROOT), "world": world.get_name(), "f_body_count": len(before)},
        "post_identity": {"project": str(ROOT), "world": post_world.get_name(), "f_body_count": len(after)},
        "pre_bindings": before, "post_bindings": after,
        "material": MATERIAL_OBJECT, "texture": TEXTURE_PATH + "." + TEXTURE_NAME,
        "source_hashes": {**manifest["source_hashes"],
                          "source_meta_sha256": source_meta_sha,
                          "dds_sha256": manifest["dds_sha256"]},
        "source_import_settings": import_settings,
        "source_ue_texture_addresses": {"x": str(address_x), "y": str(address_y)},
        "unity_runtime_wrap_verified": False,
        "imported_texture_flags": {key: str(texture.get_editor_property(key)) for key in flags},
        "backup": str(backup), "backup_sha256": digest(backup.read_bytes()),
        "ue_compiled_mip_count_verified": False,
        "ue_visual_parity_verified": False,
    }
    (PRODUCT / "ue_upgrade_report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    unreal.log("ROK_MOUNTAIN_F_GAMMA_CANDIDATE backup=" + str(backup)
               + " texture=" + TEXTURE_PATH + " material=" + MATERIAL_OBJECT
               + " compiled mip count and visual parity pending")


if __name__ == "__main__":
    upgrade()
