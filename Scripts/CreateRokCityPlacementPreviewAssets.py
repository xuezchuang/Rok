"""UE5.8 source bottom assets. Read-only preflight unless ROK_PLACEMENT_BOTTOM_ASSETS_APPLY=1.

Run only after the isolated Unity2019 bottom probe. A new import creates three
placement-only assets. The explicit source-v2 repair changes only the exact
unsealed bottom material and mesh; it never edits maps or older city assets.
"""

import collections
import hashlib
import json
import math
import os
import shutil
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[1]
WORLD = "L_RokUnityGameAppCity"
SOURCE_V1 = ROOT / "Saved/RokUnityMigration/CityPlacementBottomSource/placement_bottom_source.json"
SOURCE = ROOT / "Saved/RokUnityMigration/CityPlacementBottomSource/v2/placement_bottom_source.json"
SOURCE_VERIFY = ROOT / "Saved/RokUnityMigration/CityPlacementBottomSource/v2/placement_bottom_source_verify.json"
SOURCE_V1_SHA256 = "da956b86d55e51616740073d098d873b3c34f04105db71d7759b3cd21e69d983"
SOURCE_V2_SHA256 = "928600ef589c7e0f8fdae8660e8af6ee273197b47b5c7352dab69b2a20bad8ed"
NATIVE_SHA256 = "259592eaad2b3d782f8e66aafd7cb0ffb01cbc4e70fb6a700906cfdb55617613"
PROBE = ROOT / "Saved/UnityCityPlacementBottomProbe"
SOURCE_CONTRACT = ROOT / "Saved/RokUnityMigration/BatchReview20261009Gamma/placement_bottom_source_contract.json"
PACKAGE = "/Game/RokUnity/City/PlacementPreview"
ASSETS = {"texture": PACKAGE + "/T_buiding_bottom2_Source",
          "material": PACKAGE + "/M_building_move_bottom_SourceAlpha",
          "mesh": PACKAGE + "/SM_building_move_bottom_SourceQuad"}
APPLY = "ROK_PLACEMENT_BOTTOM_ASSETS_APPLY"
RUN = "ROK_PLACEMENT_BOTTOM_NATIVE_RUN"
EVIDENCE = ROOT / "Saved/RokUnityMigration/CityPlacementPreviewAssets"
REPAIR = "ROK_PLACEMENT_BOTTOM_MESH_REPAIR"
REPAIR_V2 = "ROK_PLACEMENT_BOTTOM_SOURCE_V2_REPAIR"
FAILED_MESH_SHA256 = "a47d72c25d7a337ab97a4770bee2c1af803982cc0c38d0e1fd45f6175ef69abe"
FAILED_LOD0_OBJ_SHA256 = "ca672f9884cca194863a9fb65df0421a8ecb20f9b2fa0bf822b40171303fdaf5"
V1_TEXTURE_SHA256 = "ce09772c81889ddb2d0fd1213712635459642ba6dae18112b63c10bd1c08878e"
V1_MATERIAL_SHA256 = "fea1ef41f9ffea36a29f063a0971ce2189731f3b0dc4184cec04150defd32da9"
V1_MESH_SHA256 = "375ee0c7019cae1c0ce908dc42d40652c83bfb23db1a0b8649cef7c9203f2994"
V1_MESH_LOD0_SHA256 = "62008ad1304a2a55a0d72c857883372cbc768cafcc9ddd45652658c8ea93d129"
V2_INTERRUPTED_MATERIAL_SHA256 = "b43adf7ca04e40c6c764d4a174f5de07197f1dbc12cdb87ad338ad51ea2405b2"
V2_INTERRUPTED_MESH_SHA256 = "790144ca044426241ffc55b3c8cb88a58cc4f2338256f6f6bc2fbc25c947024e"
V2_READONLY_PROBE = ROOT / "Saved/RokUnityMigration/BatchReview20261009Placement/placement_bottom_v2_readonly_probe_20261008_203343_543056_46712.json"
V2_READONLY_PROBE_SHA256 = "7983011a8e5f91934c08c6a0f04421a25b9ec6070bebfbf9708dba2ab597de25"
V2_READONLY_LOD0_SHA256 = "039157dc4325b24573626d6ea9a2886e8e4c8b909271b5e29f3451aa02a06760"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    candidate, number = path, 2
    while candidate.exists():
        candidate = path.with_name(f"{path.stem}_{number}{path.suffix}")
        number += 1
    return candidate


def identity():
    project = Path(unreal.Paths.project_dir()).resolve()
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world() if editor else None
    if project != ROOT.resolve() or world is None or world.get_name() != WORLD:
        raise RuntimeError(f"Wrong placement asset target: project={project}; world={world.get_name() if world else None}")


def native_source():
    run_value = os.environ.get(RUN, "")
    if not run_value:
        raise RuntimeError(f"Set {RUN} to one exact OutputRuns child")
    run = Path(run_value).resolve()
    if run.parent != (PROBE / "OutputRuns").resolve():
        raise RuntimeError("Native run is outside isolated OutputRuns")
    source = json.loads(SOURCE.read_text(encoding="utf-8"))
    verified = json.loads(SOURCE_VERIFY.read_text(encoding="utf-8"))
    native_path = run / "native_bottom.json"
    native = json.loads(native_path.read_text(encoding="utf-8"))
    if (sha(SOURCE_V1) != SOURCE_V1_SHA256 or sha(native_path) != NATIVE_SHA256 or
            source["schema"] != "rok.city_placement_bottom_source.v2" or
            verified["status"] != "passed" or verified["checks_failed"] != 0 or
            verified["checks_total"] != 81 or verified["checks_passed"] != 81 or
            verified["manifest_sha256"] != sha(SOURCE) or sha(SOURCE) != SOURCE_V2_SHA256):
        raise RuntimeError("Static source extraction is unverified")
    shader = source["source"]["shader"]
    behavior = source["source"]["placement_behavior"]
    if (shader["vertex_uv_assignment"] != "o.uv = v.uv;" or
            shader["effective_sampling_uv"] != "identity_mesh_uv" or
            shader["uses_maintex_st"] or shader["uses_transform_tex"] or
            shader["runtime_texture_scale_affects_sampling"] or
            behavior["effective_footprint_uv_tiling"]):
        raise RuntimeError("Verified v2 source shader must sample identity mesh UV")
    for item in source["source_hashes"]:
        if sha(ROOT / item["path"]) != item["sha256"]:
            raise RuntimeError(f"Source changed: {item['path']}")
    expected = source["source"]["unity_mesh"]
    if (native["schema"] != "rok.city_placement_bottom_native.v1" or
            Path(native["runDirectory"]).resolve() != run or
            native["sourceContractSha256"] != sha(SOURCE_CONTRACT) or
            native["preparedProbeContractSha256"] != sha(PROBE / "source_contract.json") or
            native["unityVersion"] != "2019.4.28f1c1" or native["colorSpace"] != "Gamma" or
            native["meshSha256"] != sha(ROOT / "ref/client-unity/Assets/BundleAssetsTest/City/Mesh/building_move_bottom.asset") or
            native["textureSha256"] != sha(ROOT / "ref/client-unity/Assets/BundleAssetsTest/City/Texture2D/sprite_city_building/buiding_bottom2.png") or
            native["materialSha256"] != sha(ROOT / "ref/client-unity/Assets/BundleAssets/Scene/City/city_function/city_building_move_bottom.mat") or
            native["prefabSha256"] != sha(ROOT / "ref/client-unity/Assets/BundleAssets/Scene/City/city_function/building_move_bottom.prefab") or
            native["shaderName"] != "custom/city/building_move_bottom-4"):
        raise RuntimeError("Native placement source chain differs")
    if (native["vertexCount"] != 4 or native["subMeshCount"] != 1 or
            native["triangles"] != expected["indices_uint16_source_order"] or
            len(native["vertices"]) != 4 or len(native["uv0"]) != 4 or
            native["textureWidth"] != 256 or native["textureHeight"] != 256 or
            native["textureMips"] != 1 or not native["textureSrgb"] or
            native["textureMipmapEnabled"] or native["wrapU"] != "Repeat" or native["wrapV"] != "Repeat" or
            native["filterMode"] != "Bilinear" or native["textureFormat"] != "RGBA32" or
            native["footprintWidth"] != 5 or native["footprintLength"] != 5 or
            native["renderPngs"] != ["bottom_green_5x5.png", "bottom_red_5x5.png"]):
        raise RuntimeError("Native bottom mesh/texture/footprint settings differ")
    q = native["prefabRootLocalRotation"]
    if (any(abs(q[k] - v) > 1e-6 for k, v in
            (("x", 0.0), ("y", -0.3826834323650898), ("z", 0.0), ("w", 0.9238795325112867))) or
            any(abs(native["prefabRootLocalPosition"][k]) > 1e-7 for k in "xyz") or
            any(abs(native["prefabRootLocalScale"][k] - 1.0) > 1e-7 for k in "xyz") or
            [native["serializedMaterialTiling"][k] for k in "xy"] != [7.0, 7.0] or
            any(abs(native["serializedMaterialColor"][k] - v) > 1e-6 for k, v in
                zip("rgba", (.9528302, .9303578, .9303578, 1.0)))):
        raise RuntimeError("Native bottom prefab/material source transform differs")
    if (native["defaultPlatform"]["overridden"] or native["defaultPlatform"]["maxTextureSize"] != 2048 or
            not native["standalonePlatform"]["overridden"] or
            native["standalonePlatform"]["maxTextureSize"] != 2048 or
            native["standalonePlatform"]["format"] != 4):
        raise RuntimeError("Native bottom texture platform import settings differ")
    # Static v2 slot-3 semantics were deliberately unknown. The native API
    # establishes UV0 by matching its values to those exact serialized bytes.
    for got, item, uv in zip(native["vertices"], expected["vertices"], native["uv0"]):
        if any(abs(got[axis] - item["source_unity_xyz"][index]) > 1e-8
               for index, axis in enumerate("xyz")) or any(
                abs(uv[axis] - item["source_serialized_channel_3_coord"][index]) > 1e-8
                for index, axis in enumerate("xy")):
            raise RuntimeError("Unity Mesh.uv0 does not prove serialized channel slot 3")
    for name in native["renderPngs"]:
        data = (run / name).read_bytes()
        if not data.startswith(b"\x89PNG\r\n\x1a\n") or len(data) < 100:
            raise RuntimeError(f"Native GPU PNG missing/corrupt: {name}")
    return run, source, native_path, native


def obj_text(native):
    # FbxFactory's OBJ path flips input Y and V (shown by the failed, exported
    # LOD0 and the UE exporter source). Pre-invert OBJ Y so the actual UE render
    # vertices are canonical (-UnityX,UnityZ,UnityY)*100cm. Keep source vt:
    # importer converts native Unity bottom-origin V to UE top-origin 1-V.
    # Unity native Mesh.normals are +Y for its source index order. UE5.8
    # MeshUtilities calculates a triangle normal as AC x AB, so invert each
    # face order at the UE import boundary to keep the rendered front at +Z.
    vertices = [(-v["x"] * 100.0, v["z"] * 100.0, v["y"] * 100.0) for v in native["vertices"]]
    obj_vertices = [(x, -y, z) for x, y, z in vertices]
    lines = ["# Unity2019 Mesh.vertices/uv/triangles, no BasicShapes substitution",
             "o SM_building_move_bottom_SourceQuad"]
    lines += [f"v {x:.9f} {y:.9f} {z:.9f}" for x, y, z in obj_vertices]
    lines += [f"vt {v['x']:.9f} {v['y']:.9f}" for v in native["uv0"]]
    lines.append("vn 0.000000000 0.000000000 1.000000000")
    for i in range(0, 6, 3):
        a, b, c = [native["triangles"][i + j] + 1 for j in (0, 1, 2)]
        lines.append(f"f {a}/{a}/1 {c}/{c}/1 {b}/{b}/1")
    return "\n".join(lines) + "\n", vertices


def metadata(run, native_path, role, source_manifest=SOURCE):
    return {"RokPlacementRole": role, "RokPlacementSourceSHA256": sha(source_manifest),
            "RokPlacementNativeSHA256": sha(native_path),
            "RokPlacementNativeRun": str(run),
            "RokPlacementAcceptance": "source-native-probed;normal-world-render-pending"}


def set_meta(asset, expected):
    for key, value in expected.items():
        unreal.EditorAssetLibrary.set_metadata_tag(asset, key, str(value))


def check_meta(asset, expected):
    for key, value in expected.items():
        if str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key)) != str(value):
            raise RuntimeError(f"{asset.get_path_name()} metadata differs: {key}")


def texture_flags():
    return {"srgb": True, "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            "filter": unreal.TextureFilter.TF_BILINEAR,
            "address_x": unreal.TextureAddress.TA_WRAP,
            "address_y": unreal.TextureAddress.TA_WRAP, "never_stream": True}


def verify_texture(asset, run, native_path):
    if not isinstance(asset, unreal.Texture2D):
        raise RuntimeError("Placement source texture type differs")
    for key, value in texture_flags().items():
        if asset.get_editor_property(key) != value:
            raise RuntimeError(f"Placement source texture property differs: {key}")
    size = [int(asset.blueprint_get_size_x()), int(asset.blueprint_get_size_y())]
    if size != [256, 256]:
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), "Editor.AsyncTextureCompilationFinishAll")
        size = [int(asset.blueprint_get_size_x()), int(asset.blueprint_get_size_y())]
    if size != [256, 256]:
        raise RuntimeError(f"Placement source texture size differs: {size}")
    package_file = ROOT / "Content/RokUnity/City/PlacementPreview/T_buiding_bottom2_Source.uasset"
    legacy = package_file.is_file() and sha(package_file) == V1_TEXTURE_SHA256
    check_meta(asset, metadata(run, native_path, "source_texture", SOURCE_V1 if legacy else SOURCE))
    return {"asset_sha256": sha(package_file), "legacy_v1_metadata": legacy}


def make_texture(run, native_path):
    path = ASSETS["texture"]
    asset = unreal.load_asset(path)
    if asset is None:
        task = unreal.AssetImportTask()
        for key, value in {"filename": str(ROOT / "ref/client-unity/Assets/BundleAssetsTest/City/Texture2D/sprite_city_building/buiding_bottom2.png"),
                           "destination_path": PACKAGE, "destination_name": path.rsplit("/", 1)[1],
                           "factory": unreal.TextureFactory(), "automated": True,
                           "replace_existing": False, "save": False}.items():
            task.set_editor_property(key, value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset = unreal.load_asset(path)
        if not isinstance(asset, unreal.Texture2D):
            raise RuntimeError("Placement texture import failed")
        for key, value in texture_flags().items():
            asset.set_editor_property(key, value)
        set_meta(asset, metadata(run, native_path, "source_texture"))
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
            raise RuntimeError("Placement source texture save failed")
    verify_texture(asset, run, native_path)
    return asset


def verify_material(asset, texture, run, native_path, legacy=False):
    if not isinstance(asset, unreal.Material):
        raise RuntimeError("Placement source material type differs")
    for key, value in (("material_domain", unreal.MaterialDomain.MD_SURFACE),
                       ("shading_model", unreal.MaterialShadingModel.MSM_UNLIT),
                       ("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT), ("two_sided", False)):
        if asset.get_editor_property(key) != value:
            raise RuntimeError(f"Placement source material property differs: {key}")
    check_meta(asset, metadata(run, native_path, "source_alpha_tint", SOURCE_V1 if legacy else SOURCE))
    edit = unreal.MaterialEditingLibrary
    nodes = list(edit.get_material_expressions(asset))
    if len(nodes) != 9:
        raise RuntimeError(f"Placement source material expression count differs: {len(nodes)}")
    def one(cls, count):
        result = [n for n in nodes if isinstance(n, cls)]
        if len(result) != count:
            raise RuntimeError(f"Placement source material {cls.__name__} count differs")
        return result
    sample = one(unreal.MaterialExpressionTextureSampleParameter2D, 1)[0]
    vectors = {str(n.get_editor_property("parameter_name")): n for n in one(unreal.MaterialExpressionVectorParameter, 2)}
    coord = one(unreal.MaterialExpressionTextureCoordinate, 1)[0]
    multiplies = one(unreal.MaterialExpressionMultiply, 2)
    masks = one(unreal.MaterialExpressionComponentMask, 3)
    if (set(vectors) != {"Tint", "Tiling"} or
            str(sample.get_editor_property("parameter_name")) != "SourceTexture" or
            sample.get_editor_property("texture") != texture or
            sample.get_editor_property("sampler_type") != unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
        raise RuntimeError("Placement material source sample/parameters differ")
    tint = vectors["Tint"].get_editor_property("default_value")
    tiling = vectors["Tiling"].get_editor_property("default_value")
    if any(abs(getattr(tint, k) - v) > 1e-5 for k, v in zip("rgba", (.9528302, .9303578, .9303578, 1))) or \
            any(abs(getattr(tiling, k) - v) > 1e-5 for k, v in zip("rgba", (7, 7, 0, 0))):
        raise RuntimeError("Placement material source defaults differ")
    emissive = edit.get_material_property_input_node(asset, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = edit.get_material_property_input_node(asset, unreal.MaterialProperty.MP_OPACITY)
    if emissive not in masks or opacity not in masks or emissive == opacity:
        raise RuntimeError("Placement material outputs differ")
    other = next(mask for mask in masks if mask != emissive and mask != opacity)
    if (not emissive.get_editor_property("r") or not emissive.get_editor_property("g") or
            not emissive.get_editor_property("b") or emissive.get_editor_property("a") or
            any(opacity.get_editor_property(k) for k in "rgb") or not opacity.get_editor_property("a") or
            not other.get_editor_property("r") or not other.get_editor_property("g") or
            other.get_editor_property("b") or other.get_editor_property("a")):
        raise RuntimeError("Placement material source RGB/alpha/UV channel masks differ")
    inputs = lambda node: [value for value in edit.get_inputs_for_material_expression(asset, node) if value is not None]
    uv = next((node for node in multiplies if inputs(node) == [coord, other]), None)
    color = next((node for node in multiplies if inputs(node) == [sample, vectors["Tint"]]), None)
    if (uv is None or color is None or uv == color or
            inputs(other) != [vectors["Tiling"]] or
            inputs(emissive) != [color] or inputs(opacity) != [color]):
        raise RuntimeError("Placement material source texture*Tint / UV*Tiling graph differs")
    if legacy:
        if inputs(sample) != [uv]:
            raise RuntimeError("Legacy placement material must sample the old UV*Tiling graph")
    elif inputs(sample) != [coord] or any(uv in inputs(node) for node in nodes):
        raise RuntimeError("Source-v2 placement material must sample TexCoord0 directly; Tiling is disconnected")
    for upstream in (sample, vectors["Tint"]):
        output = edit.get_input_node_output_name_for_material_expression(color, upstream)
        if output != "RGBA":
            raise RuntimeError(f"Placement material must multiply the full source RGBA: {output!r}")
    return {"expressions": len(nodes), "sample": sample.get_path_name(),
            "sample_uv": "UV*Tiling" if legacy else "identity_TexCoord0",
            "serialized_tiling_parameter_affects_sampling": legacy}


def make_material(texture, run, native_path):
    path = ASSETS["material"]
    asset = unreal.load_asset(path)
    if asset is None:
        edit = unreal.MaterialEditingLibrary
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            path.rsplit("/", 1)[1], PACKAGE, unreal.Material, unreal.MaterialFactoryNew())
        if not isinstance(asset, unreal.Material):
            raise RuntimeError("Placement material creation failed")
        for key, value in (("material_domain", unreal.MaterialDomain.MD_SURFACE),
                           ("shading_model", unreal.MaterialShadingModel.MSM_UNLIT),
                           ("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT), ("two_sided", False)):
            asset.set_editor_property(key, value)
        def node(cls): return edit.create_material_expression(asset, cls)
        def connect(a, output, b, input_name):
            if not edit.connect_material_expressions(a, output, b, input_name):
                raise RuntimeError(f"Placement material connection failed: {input_name}")
        coord = node(unreal.MaterialExpressionTextureCoordinate)
        tiling = node(unreal.MaterialExpressionVectorParameter)
        tiling.set_editor_property("parameter_name", "Tiling")
        tiling.set_editor_property("default_value", unreal.LinearColor(7, 7, 0, 0))
        tiling_rg = node(unreal.MaterialExpressionComponentMask)
        for key, value in (("r", True), ("g", True), ("b", False), ("a", False)):
            tiling_rg.set_editor_property(key, value)
        uv = node(unreal.MaterialExpressionMultiply)
        sample = node(unreal.MaterialExpressionTextureSampleParameter2D)
        sample.set_editor_property("parameter_name", "SourceTexture")
        sample.set_editor_property("texture", texture)
        sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        tint = node(unreal.MaterialExpressionVectorParameter)
        tint.set_editor_property("parameter_name", "Tint")
        tint.set_editor_property("default_value", unreal.LinearColor(.9528302, .9303578, .9303578, 1))
        color = node(unreal.MaterialExpressionMultiply)
        rgb = node(unreal.MaterialExpressionComponentMask)
        alpha = node(unreal.MaterialExpressionComponentMask)
        for key in "rgb": rgb.set_editor_property(key, True)
        for key in "rgb": alpha.set_editor_property(key, False)
        rgb.set_editor_property("a", False)
        alpha.set_editor_property("a", True)
        connect(tiling, "RGBA", tiling_rg, "")
        connect(coord, "", uv, "A")
        connect(tiling_rg, "", uv, "B")
        # Source shader assigns o.uv=v.uv; serialized _MainTex scale is unused.
        connect(coord, "", sample, "UVs")
        connect(sample, "RGBA", color, "A")
        connect(tint, "RGBA", color, "B")
        connect(color, "", rgb, "")
        connect(color, "", alpha, "")
        if not edit.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR) or \
                not edit.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY):
            raise RuntimeError("Placement material outputs could not be connected")
        set_meta(asset, metadata(run, native_path, "source_alpha_tint"))
        errors = [str(e) for e in edit.recompile_material(asset)]
        edit.get_statistics(asset)
        errors.extend(str(e) for e in edit.recompile_material(asset))
        edit.get_statistics(asset)
        if errors:
            raise RuntimeError(f"Placement material compile failed: {errors}")
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
            raise RuntimeError("Placement material save failed")
        edit.get_statistics(asset)
    verify_material(asset, texture, run, native_path)
    return asset


def mesh_editor():
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem is None:
        raise RuntimeError("UE5.8 StaticMeshEditorSubsystem unavailable")
    return subsystem


def export_lod0(asset, stem):
    export = unique(EVIDENCE / (stem + ".obj"))
    task = unreal.AssetExportTask()
    for key, value in {"object": asset, "filename": str(export),
                       "exporter": unreal.StaticMeshExporterOBJ(), "automated": True,
                       "prompt": False, "replace_identical": False}.items():
        task.set_editor_property(key, value)
    if not unreal.Exporter.run_asset_export_task(task) or not export.is_file():
        raise RuntimeError("Placement source quad LOD0 export failed")
    return export


def read_export_lod0(export):
    # EditorExporters.cpp:2178-2196: OBJ export is (UE.X,UE.Z,UE.Y),
    # vt=(UE.U,1-UE.V). This reverses only the exporter presentation.
    positions, uvs, faces = [], [], []
    for line in export.read_text(encoding="utf-8-sig").splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v":
            x, z, y = map(float, parts[1:4])
            positions.append((x, y, z))
        elif parts[0] == "vt":
            u, inverse_v = map(float, parts[1:3])
            uvs.append((u, 1.0 - inverse_v))
        elif parts[0] == "f":
            if len(parts) != 4:
                raise RuntimeError("Placement LOD0 exported non-triangle")
            face = []
            for token in parts[1:]:
                position_index, uv_index = [int(v) - 1 for v in token.split("/")[:2]]
                face.append((positions[position_index], uvs[uv_index]))
            faces.append(face)
    if len(positions) != 6 or len(uvs) != 6 or len(faces) != 2:
        raise RuntimeError("Placement exported LOD0 vertex/face count differs")
    return faces


def face_key(face):
    return tuple(sorted(tuple(round(v, 4) for v in position) +
                        tuple(round(v, 6) for v in uv) for position, uv in face))


def expected_faces(native, vertices, mirror_y=False, reverse_winding=False):
    faces = []
    for i in (0, 3):
        face = []
        for j in ((0, 2, 1) if reverse_winding else (0, 1, 2)):
            index = native["triangles"][i + j]
            x, y, z = vertices[index]
            uv = native["uv0"][index]
            face.append(((x, -y if mirror_y else y, z), (uv["x"], 1.0 - uv["y"])))
        faces.append(face)
    return faces


def face_normal_z(face):
    a, b, c = [point for point, _ in face]
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def ue_face_normal_z(face):
    # UE5.8 MeshUtilities.cpp CalculateTriangleTangentInternal uses
    # (B-C) ^ (A-C) == AC x AB, opposite the usual AB x AC test.
    return -face_normal_z(face)


def verify_mesh(asset, material, run, native_path, vertices, native, legacy=False):
    if not isinstance(asset, unreal.StaticMesh):
        raise RuntimeError("Placement source mesh type differs")
    if (int(asset.get_num_vertices(0)) != 4 or int(asset.get_num_triangles(0)) != 2 or
            int(asset.get_num_tex_coords(0)) != 1):
        raise RuntimeError("Placement source mesh topology differs")
    settings = mesh_editor().get_lod_build_settings(asset, 0)
    if (not settings.get_editor_property("use_full_precision_u_vs") or
            settings.get_editor_property("remove_degenerates") or
            settings.get_editor_property("generate_lightmap_u_vs")):
        raise RuntimeError("Placement source mesh UV precision/build differs")
    materials = list(asset.get_editor_property("static_materials"))
    if len(materials) != 1 or materials[0].get_editor_property("material_interface") != material:
        raise RuntimeError("Placement source mesh material binding differs")
    check_meta(asset, metadata(run, native_path, "source_quad", SOURCE_V1 if legacy else SOURCE))
    bounds = asset.get_bounds()
    expected_lo = [min(v[i] for v in vertices) for i in range(3)]
    expected_hi = [max(v[i] for v in vertices) for i in range(3)]
    actual_lo = [bounds.origin.x - bounds.box_extent.x, bounds.origin.y - bounds.box_extent.y,
                 bounds.origin.z - bounds.box_extent.z]
    actual_hi = [bounds.origin.x + bounds.box_extent.x, bounds.origin.y + bounds.box_extent.y,
                 bounds.origin.z + bounds.box_extent.z]
    if any(abs(a - b) > 1e-4 for actual, expected in ((actual_lo, expected_lo), (actual_hi, expected_hi))
           for a, b in zip(actual, expected)):
        raise RuntimeError(f"Placement source mesh bounds/pivot differ: {actual_lo}; {actual_hi}")
    # UE exporter writes (X,Z,Y) and (U,1-V). Undo its formatting, then
    # compare to exact native->UE import conversions, not raw Unity numbers.
    export = export_lod0(asset, "SM_building_move_bottom_v1_before_v2_repair" if legacy
                         else "SM_building_move_bottom_ue_lod0")
    faces = read_export_lod0(export)
    if collections.Counter(map(face_key, faces)) != collections.Counter(
            map(face_key, expected_faces(native, vertices, reverse_winding=not legacy))):
        raise RuntimeError("Placement imported LOD0 differs from exact native->UE position/UV conversion")
    for face in faces:
        normal_z = ue_face_normal_z(face)
        if (normal_z > -99.99 if legacy else normal_z < 99.99):
            raise RuntimeError(f"Placement source quad UE front-facing winding differs: {normal_z}")
    return {"vertices": 4, "triangles": 2, "uv_channels": 1,
            "bounds_min_cm": actual_lo, "bounds_max_cm": actual_hi,
            "source_face_uv_proof": True, "ue_front_normal_z": [ue_face_normal_z(face) for face in faces],
            "ue_front_facing_winding": not legacy,
            "ue_lod0_export": str(export), "ue_lod0_export_sha256": sha(export)}


def prove_failed_mesh_for_repair(asset, material, run, native_path, vertices, native):
    package_file = ROOT / "Content/RokUnity/City/PlacementPreview/SM_building_move_bottom_SourceQuad.uasset"
    if (not package_file.is_file() or sha(package_file) != FAILED_MESH_SHA256 or
            not isinstance(asset, unreal.StaticMesh) or
            int(asset.get_num_vertices(0)) != 4 or int(asset.get_num_triangles(0)) != 2 or
            int(asset.get_num_tex_coords(0)) != 1):
        raise RuntimeError("Only exact first failed placement-bottom mesh may be repaired")
    check_meta(asset, metadata(run, native_path, "source_quad"))
    old_settings = mesh_editor().get_lod_build_settings(asset, 0)
    materials = list(asset.get_editor_property("static_materials"))
    if (not old_settings.get_editor_property("use_full_precision_u_vs") or
            old_settings.get_editor_property("remove_degenerates") or
            len(materials) != 1 or materials[0].get_editor_property("material_interface") != material):
        raise RuntimeError("Failed placement-bottom mesh build/material identity differs")
    old_export = export_lod0(asset, "SM_building_move_bottom_before_axis_repair")
    if sha(old_export) != FAILED_LOD0_OBJ_SHA256:
        raise RuntimeError("Failed placement-bottom render LOD0 export hash differs")
    faces = read_export_lod0(old_export)
    if (collections.Counter(map(face_key, faces)) != collections.Counter(
            map(face_key, expected_faces(native, vertices, mirror_y=True))) or
            any(face_normal_z(face) > -99.99 for face in faces)):
        raise RuntimeError("Failed placement-bottom mesh is not the known Y-mirror/down-face signature")
    backup = unique(EVIDENCE / "SM_building_move_bottom_before_axis_repair.uasset")
    shutil.copy2(package_file, backup)
    if sha(backup) != FAILED_MESH_SHA256:
        raise RuntimeError("Placement-bottom mesh backup SHA differs")
    diagnostic = {"schema": "rok.placement_bottom_mesh_repair_before.v1",
                  "scope": "only the new unsealed placement-bottom StaticMesh package",
                  "asset_path": ASSETS["mesh"], "old_uasset_sha256": FAILED_MESH_SHA256,
                  "old_lod0_obj": str(old_export), "old_lod0_obj_sha256": sha(old_export),
                  "backup": str(backup), "backup_sha256": sha(backup),
                  "old_face_area_z": [face_normal_z(face) for face in faces],
                  "source_uv_conversion": "Unity (u,v) -> UE raw (u,1-v); OBJ importer flips V; UE OBJ exporter flips V again",
                  "old_geometry": "exact native source canonical X/Z with imported Y mirrored; both faces downward"}
    before_report = unique(EVIDENCE / "mesh_axis_repair_before.json")
    before_report.write_text(json.dumps(diagnostic, indent=2) + "\n", encoding="utf-8")
    unreal.log(f"ROK_PLACEMENT_BOTTOM_MESH_REPAIR_BACKUP {backup} {FAILED_MESH_SHA256}")
    return package_file, backup, before_report


def make_mesh(material, run, native_path, obj_path, vertices, native, repair_v2=False):
    path = ASSETS["mesh"]
    asset = unreal.load_asset(path)
    repair = asset is not None and os.environ.get(REPAIR) == "1"
    repair_evidence = prove_failed_mesh_for_repair(asset, material, run, native_path, vertices, native) if repair else None
    if asset is None or repair or repair_v2:
        task = unreal.AssetImportTask()
        for key, value in {"filename": str(obj_path), "destination_path": PACKAGE,
                           "destination_name": path.rsplit("/", 1)[1], "automated": True,
                           "replace_existing": repair or repair_v2, "replace_existing_settings": False,
                           "save": False}.items():
            task.set_editor_property(key, value)
        options = unreal.FbxImportUI()
        for key, value in (("import_mesh", True), ("import_as_skeletal", False),
                           ("import_materials", False), ("import_textures", False),
                           ("automated_import_should_detect_type", False)):
            options.set_editor_property(key, value)
        data = options.get_editor_property("static_mesh_import_data")
        for key, value in (("combine_meshes", True), ("generate_lightmap_u_vs", False),
                           ("auto_generate_collision", False), ("build_nanite", False),
                           ("remove_degenerates", False)):
            data.set_editor_property(key, value)
        task.set_editor_property("factory", unreal.FbxFactory())
        task.set_editor_property("options", options)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset = unreal.load_asset(path)
        if not isinstance(asset, unreal.StaticMesh):
            raise RuntimeError("Placement source quad import failed")
        settings = mesh_editor().get_lod_build_settings(asset, 0)
        settings.set_editor_property("use_full_precision_u_vs", True)
        settings.set_editor_property("remove_degenerates", False)
        # FbxImportUI does not guarantee the source-model build flag on reimport.
        # UE5.8 rebuilds from this LOD setting and must retain only source UV0.
        settings.set_editor_property("generate_lightmap_u_vs", False)
        mesh_editor().set_lod_build_settings(asset, 0, settings)
        materials = list(asset.get_editor_property("static_materials"))
        if len(materials) != 1:
            raise RuntimeError("Placement quad material slots differ")
        materials[0].set_editor_property("material_interface", material)
        asset.set_editor_property("static_materials", materials)
        set_meta(asset, metadata(run, native_path, "source_quad"))
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
            raise RuntimeError("Placement source mesh save failed")
    result = verify_mesh(asset, material, run, native_path, vertices, native)
    if repair_evidence:
        package_file, backup, before_report = repair_evidence
        after = {"schema": "rok.placement_bottom_mesh_repair_after.v1",
                 "asset_path": path, "before_report": str(before_report),
                 "before_report_sha256": sha(before_report),
                 "backup": str(backup), "backup_sha256": sha(backup),
                 "new_uasset_sha256": sha(package_file), "new_lod0": result}
        if after["new_uasset_sha256"] == FAILED_MESH_SHA256:
            raise RuntimeError("Placement-bottom mesh repair did not change the package")
        after_report = unique(EVIDENCE / "mesh_axis_repair_after.json")
        after_report.write_text(json.dumps(after, indent=2) + "\n", encoding="utf-8")
        result["repair_before_report"] = str(before_report)
        result["repair_after_report"] = str(after_report)
    return result


def continue_interrupted_source_v2(texture, material, mesh, run, native_path, vertices, native):
    """Finish only the pinned v2 quad whose geometry is correct but has generated UV1."""
    texture_file = ROOT / "Content/RokUnity/City/PlacementPreview/T_buiding_bottom2_Source.uasset"
    material_file = ROOT / "Content/RokUnity/City/PlacementPreview/M_building_move_bottom_SourceAlpha.uasset"
    mesh_file = ROOT / "Content/RokUnity/City/PlacementPreview/SM_building_move_bottom_SourceQuad.uasset"
    old_material_sha, old_mesh_sha = sha(material_file), sha(mesh_file)
    if (sha(texture_file) != V1_TEXTURE_SHA256 or
            old_material_sha != V2_INTERRUPTED_MATERIAL_SHA256 or
            old_mesh_sha != V2_INTERRUPTED_MESH_SHA256):
        raise RuntimeError("Only the exact interrupted source-v2 M/SM/T packages may continue")
    material_backup = EVIDENCE / "M_building_move_bottom_before_source_v2.uasset"
    mesh_backup = EVIDENCE / "SM_building_move_bottom_before_source_v2.uasset"
    if (sha(material_backup) != V1_MATERIAL_SHA256 or
            sha(mesh_backup) != V1_MESH_SHA256 or
            sha(V2_READONLY_PROBE) != V2_READONLY_PROBE_SHA256):
        raise RuntimeError("Source-v2 v1 backups or sealed read-only probe differ")
    probe = json.loads(V2_READONLY_PROBE.read_text(encoding="utf-8"))
    if (not probe.get("diagnostic_complete") or not probe.get("dirty_unchanged") or
            not probe.get("disk_unchanged") or
            probe.get("project") != str(ROOT) or probe.get("world") != WORLD or
            probe["disk_sha256_before"]["texture"] != V1_TEXTURE_SHA256 or
            probe["disk_sha256_before"]["material"] != old_material_sha or
            probe["disk_sha256_before"]["mesh"] != old_mesh_sha or
            probe["source_v2_sha256"] != SOURCE_V2_SHA256 or
            probe["native_sha256"] != sha(native_path) or
            probe["lod0_export"]["sha256"] != V2_READONLY_LOD0_SHA256 or
            probe["raw_lod0_counts_before_export"] != {"vertices": 4, "triangles": 2, "uv_channels": 2} or
            probe["raw_lod0_counts_after_export"] != {"vertices": 4, "triangles": 2, "uv_channels": 2} or
            not probe["material"]["uv_is_exact_texcoord0"] or
            probe["lod0_export"]["parsed"]["unique_position_uv_corners"] != 4 or
            probe["lod0_export"]["parsed"]["ue_front_normal_z"] != [100.0, 100.0]):
        raise RuntimeError("Sealed source-v2 forensic probe does not prove the exact interrupted asset")
    material_check = verify_material(material, texture, run, native_path)
    if (int(mesh.get_num_vertices(0)) != 4 or int(mesh.get_num_triangles(0)) != 2 or
            int(mesh.get_num_tex_coords(0)) != 2):
        raise RuntimeError("Interrupted source-v2 quad render counts differ")
    check_meta(mesh, metadata(run, native_path, "source_quad"))
    slots = list(mesh.get_editor_property("static_materials"))
    if len(slots) != 1 or slots[0].get_editor_property("material_interface") != material:
        raise RuntimeError("Interrupted source-v2 quad material slot differs")
    settings = mesh_editor().get_lod_build_settings(mesh, 0)
    if (not settings.get_editor_property("use_full_precision_u_vs") or
            settings.get_editor_property("remove_degenerates") or
            not settings.get_editor_property("generate_lightmap_u_vs")):
        raise RuntimeError("Interrupted source-v2 quad LOD0 build flags differ")
    bounds = mesh.get_bounds()
    expected_lo = [min(v[i] for v in vertices) for i in range(3)]
    expected_hi = [max(v[i] for v in vertices) for i in range(3)]
    actual_lo = [bounds.origin.x - bounds.box_extent.x, bounds.origin.y - bounds.box_extent.y,
                 bounds.origin.z - bounds.box_extent.z]
    actual_hi = [bounds.origin.x + bounds.box_extent.x, bounds.origin.y + bounds.box_extent.y,
                 bounds.origin.z + bounds.box_extent.z]
    if any(abs(a - b) > 1e-4 for actual, expected in ((actual_lo, expected_lo), (actual_hi, expected_hi))
           for a, b in zip(actual, expected)):
        raise RuntimeError("Interrupted source-v2 quad pivot/bounds differ")
    before_export = export_lod0(mesh, "SM_building_move_bottom_v2_uv1_before_continuation")
    faces = read_export_lod0(before_export)
    if (collections.Counter(map(face_key, faces)) != collections.Counter(
            map(face_key, expected_faces(native, vertices, reverse_winding=True))) or
            any(ue_face_normal_z(face) < 99.99 for face in faces)):
        raise RuntimeError("Interrupted source-v2 quad face, UV0 or winding differs")

    backup = unique(EVIDENCE / "SM_building_move_bottom_v2_generated_uv1_backup.uasset")
    shutil.copy2(mesh_file, backup)
    if sha(backup) != old_mesh_sha:
        raise RuntimeError("Interrupted source-v2 quad backup differs")
    before = {"schema": "rok.placement_bottom_source_v2_uv1_continuation_before.v1",
              "scope": "only new PlacementPreview source quad LOD0 generated lightmap UV",
              "probe": str(V2_READONLY_PROBE), "probe_sha256": V2_READONLY_PROBE_SHA256,
              "v1_material_backup_sha256": sha(material_backup), "v1_mesh_backup_sha256": sha(mesh_backup),
              "current_material_sha256": old_material_sha, "current_mesh_sha256": old_mesh_sha,
              "current_mesh_backup": str(backup), "current_mesh_backup_sha256": sha(backup),
              "source_face_uv0_and_front_winding_proved": True,
              "before_export": str(before_export), "before_export_sha256": sha(before_export),
              "old_lod0_uv_channels": 2, "old_generate_lightmap_u_vs": True,
              "planned_change": "disable LOD0 generated lightmap UV; remove only channel 1 if rebuild retains it"}
    before_report = unique(EVIDENCE / "placement_bottom_source_v2_uv1_continuation_before.json")
    before_report.write_text(json.dumps(before, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    # SetLodBuildSettings calls PostEditChange and rebuilds. Generated UV1 may
    # disappear automatically; only a retained MeshDescription UV1 is removed.
    settings.set_editor_property("generate_lightmap_u_vs", False)
    mesh_editor().set_lod_build_settings(mesh, 0, settings)
    if mesh_editor().get_lod_build_settings(mesh, 0).get_editor_property("generate_lightmap_u_vs"):
        raise RuntimeError("Source-v2 LOD0 generate-lightmap flag did not switch off")
    uv_after_build = int(mesh.get_num_tex_coords(0))
    removed_uv1 = False
    if uv_after_build == 2:
        removed_uv1 = bool(mesh_editor().remove_uv_channel(mesh, 0, 1))
        if not removed_uv1:
            raise RuntimeError("Source-v2 generated UV1 persisted and could not be removed")
    elif uv_after_build != 1:
        raise RuntimeError(f"Source-v2 UV channel count after rebuild differs: {uv_after_build}")
    if int(mesh.get_num_tex_coords(0)) != 1:
        raise RuntimeError("Source-v2 quad must have exactly source UV0 after continuation")
    mesh_check = verify_mesh(mesh, material, run, native_path, vertices, native)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mesh):
        raise RuntimeError("Source-v2 quad continuation save failed; backup is preserved")
    mesh_after_sha = sha(mesh_file)
    if (mesh_after_sha == old_mesh_sha or sha(material_file) != old_material_sha or
            sha(texture_file) != V1_TEXTURE_SHA256):
        raise RuntimeError("Source-v2 continuation altered another asset or failed to save quad")
    after = {"schema": "rok.placement_bottom_source_v2_repair_after.v1",
             "status": "interrupted_v2_uv1_continued", "source_v2_sha256": SOURCE_V2_SHA256,
             "before_report": str(before_report), "before_report_sha256": sha(before_report),
             "probe": str(V2_READONLY_PROBE), "probe_sha256": V2_READONLY_PROBE_SHA256,
             "current_mesh_backup": str(backup), "current_mesh_backup_sha256": sha(backup),
             "material_before_sha256": old_material_sha, "material_after_sha256": sha(material_file),
             "mesh_before_sha256": old_mesh_sha, "mesh_after_sha256": mesh_after_sha,
             "texture_unchanged_sha256": sha(texture_file),
             "uv_channels_after_setting_only": uv_after_build, "removed_retained_uv1": removed_uv1,
             "material_new_graph": material_check, "mesh_new_lod0": mesh_check,
             "visual_acceptance": "pending separate Root runtime; source-v2 static import is not pixel acceptance"}
    after_report = unique(EVIDENCE / "placement_bottom_source_v2_repair_after_continuation.json")
    after_report.write_text(json.dumps(after, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return {"status": "interrupted_v2_uv1_continued", "before_report": str(before_report),
            "after_report": str(after_report), "material_validation": material_check,
            "mesh_validation": mesh_check, "material_sha256": old_material_sha,
            "mesh_sha256": mesh_after_sha}


def repair_source_v2(texture, run, native_path, obj_path, vertices, native):
    """Repair only the two exact, unsealed v1 bottom packages after full preflight."""
    texture_file = ROOT / "Content/RokUnity/City/PlacementPreview/T_buiding_bottom2_Source.uasset"
    material_file = ROOT / "Content/RokUnity/City/PlacementPreview/M_building_move_bottom_SourceAlpha.uasset"
    mesh_file = ROOT / "Content/RokUnity/City/PlacementPreview/SM_building_move_bottom_SourceQuad.uasset"
    if any(not path.is_file() for path in (texture_file, material_file, mesh_file)):
        raise RuntimeError("Source-v2 repair requires all three existing placement-only assets")
    if sha(texture_file) != V1_TEXTURE_SHA256:
        raise RuntimeError("Placement source texture changed; this repair may not touch it")
    material = unreal.load_asset(ASSETS["material"])
    mesh = unreal.load_asset(ASSETS["mesh"])
    if not isinstance(material, unreal.Material) or not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Placement source-v2 repair asset classes differ")
    old_material_sha, old_mesh_sha = sha(material_file), sha(mesh_file)
    material_source_tag = str(unreal.EditorAssetLibrary.get_metadata_tag(material, "RokPlacementSourceSHA256"))
    mesh_source_tag = str(unreal.EditorAssetLibrary.get_metadata_tag(mesh, "RokPlacementSourceSHA256"))
    if material_source_tag == SOURCE_V2_SHA256 and mesh_source_tag == SOURCE_V2_SHA256:
        if old_material_sha == V2_INTERRUPTED_MATERIAL_SHA256 and old_mesh_sha == V2_INTERRUPTED_MESH_SHA256:
            return continue_interrupted_source_v2(texture, material, mesh, run, native_path, vertices, native)
        material_check = verify_material(material, texture, run, native_path)
        mesh_check = verify_mesh(mesh, material, run, native_path, vertices, native)
        prior_reports = sorted(EVIDENCE.glob("placement_bottom_source_v2_repair_after*.json"))
        matching_report = next((path for path in prior_reports if
                                (row := json.loads(path.read_text(encoding="utf-8"))).get("source_v2_sha256") == SOURCE_V2_SHA256 and
                                row.get("material_after_sha256") == old_material_sha and
                                row.get("mesh_after_sha256") == old_mesh_sha and
                                row.get("texture_unchanged_sha256") == V1_TEXTURE_SHA256), None)
        if matching_report is None:
            raise RuntimeError("Source-v2 bottom assets lack a matching sealed repair report")
        return {"status": "already_repaired_no_asset_write", "previous_report": str(matching_report),
                "material_validation": material_check, "mesh_validation": mesh_check,
                "material_sha256": old_material_sha, "mesh_sha256": old_mesh_sha}
    if material_source_tag == SOURCE_V2_SHA256 or mesh_source_tag == SOURCE_V2_SHA256:
        raise RuntimeError("Partial source-v2 placement repair; inspect backups before retrying")
    if old_material_sha != V1_MATERIAL_SHA256 or old_mesh_sha != V1_MESH_SHA256:
        raise RuntimeError("Only exact first v1 placement material/mesh packages may be repaired")

    old_material_check = verify_material(material, texture, run, native_path, legacy=True)
    old_mesh_check = verify_mesh(mesh, material, run, native_path, vertices, native, legacy=True)
    if old_mesh_check["ue_lod0_export_sha256"] != V1_MESH_LOD0_SHA256:
        raise RuntimeError("Placement v1 LOD0 hash differs despite matching asset package")
    material_backup = unique(EVIDENCE / "M_building_move_bottom_before_source_v2.uasset")
    mesh_backup = unique(EVIDENCE / "SM_building_move_bottom_before_source_v2.uasset")
    shutil.copy2(material_file, material_backup)
    shutil.copy2(mesh_file, mesh_backup)
    if sha(material_backup) != V1_MATERIAL_SHA256 or sha(mesh_backup) != V1_MESH_SHA256:
        raise RuntimeError("Placement source-v2 backup integrity differs")
    before = {"schema": "rok.placement_bottom_source_v2_repair_before.v1",
              "scope": "exact two new PlacementPreview assets only; source texture read-only; no map or older city asset write",
              "source_v2": str(SOURCE), "source_v2_sha256": SOURCE_V2_SHA256,
              "native_run": str(run), "native_sha256": sha(native_path),
              "texture_unchanged_sha256": V1_TEXTURE_SHA256,
              "material_before_sha256": old_material_sha, "mesh_before_sha256": old_mesh_sha,
              "material_backup": str(material_backup), "material_backup_sha256": sha(material_backup),
              "mesh_backup": str(mesh_backup), "mesh_backup_sha256": sha(mesh_backup),
              "material_old_graph": old_material_check, "mesh_old_lod0": old_mesh_check,
              "planned_material_change": "TextureSample.UVs: UV*Tiling -> TexCoord0; serialized Tiling branch remains disconnected",
              "planned_mesh_change": "reverse each existing triangle's index order; preserve positions, UV, pivot and material slot"}
    before_report = unique(EVIDENCE / "placement_bottom_source_v2_repair_before.json")
    before_report.write_text(json.dumps(before, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    unreal.log(f"ROK_PLACEMENT_BOTTOM_V2_BACKUPS {material_backup} {mesh_backup}")

    edit = unreal.MaterialEditingLibrary
    nodes = list(edit.get_material_expressions(material))
    coord = next(node for node in nodes if isinstance(node, unreal.MaterialExpressionTextureCoordinate))
    sample = next(node for node in nodes if isinstance(node, unreal.MaterialExpressionTextureSampleParameter2D))
    if not edit.connect_material_expressions(coord, "", sample, "UVs"):
        raise RuntimeError("Placement source-v2 material UV rewire failed; backups are preserved")
    set_meta(material, metadata(run, native_path, "source_alpha_tint"))
    errors = [str(error) for error in edit.recompile_material(material)]
    if errors:
        raise RuntimeError(f"Placement source-v2 material compile failed: {errors}; backups are preserved")
    material.modify(True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Placement source-v2 material save failed; backups are preserved")
    material_check = verify_material(material, texture, run, native_path)
    mesh_check = make_mesh(material, run, native_path, obj_path, vertices, native, repair_v2=True)
    material_after_sha, mesh_after_sha = sha(material_file), sha(mesh_file)
    if (material_after_sha == old_material_sha or mesh_after_sha == old_mesh_sha or
            sha(texture_file) != V1_TEXTURE_SHA256):
        raise RuntimeError("Placement source-v2 packages or untouched texture hash differ")
    after = {"schema": "rok.placement_bottom_source_v2_repair_after.v1",
             "source_v2_sha256": SOURCE_V2_SHA256,
             "before_report": str(before_report), "before_report_sha256": sha(before_report),
             "texture_unchanged_sha256": sha(texture_file),
             "material_before_sha256": old_material_sha, "material_after_sha256": material_after_sha,
             "mesh_before_sha256": old_mesh_sha, "mesh_after_sha256": mesh_after_sha,
             "material_backup": str(material_backup), "material_backup_sha256": sha(material_backup),
             "mesh_backup": str(mesh_backup), "mesh_backup_sha256": sha(mesh_backup),
             "material_new_graph": material_check, "mesh_new_lod0": mesh_check,
             "visual_acceptance": "pending separate Root runtime; source-v2 static import is not a pixel acceptance"}
    after_report = unique(EVIDENCE / "placement_bottom_source_v2_repair_after.json")
    after_report.write_text(json.dumps(after, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return {"status": "repaired", "before_report": str(before_report),
            "after_report": str(after_report), "material_validation": material_check,
            "mesh_validation": mesh_check, "material_sha256": material_after_sha,
            "mesh_sha256": mesh_after_sha}


def main():
    identity()
    run, source, native_path, native = native_source()
    obj, vertices = obj_text(native)
    result = {"schema": "rok.city_placement_preview_assets.v2", "project": str(ROOT),
              "world": WORLD, "native_run": str(run), "native_sha256": sha(native_path),
              "static_source_sha256": sha(SOURCE), "assets": ASSETS,
              "obj_sha256": hashlib.sha256(obj.encode("utf-8")).hexdigest(),
              "mesh_units": "OBJ input pre-inverts Y for FbxFactory; actual UE mesh canonical (-UnityX,UnityZ,UnityY)*100cm; runtime CityContainer 500UU/Unity unit and component scale 1/100 with -X bridge",
              "uv_conversion": "Native Unity (u,v) -> actual UE raw (u,1-v), matching sealed resource OBJ importer and UE top-origin texture UV convention; no relaxed UV error budget",
              "ue_front_winding": "invert source triangle order at OBJ import; UE5.8 MeshUtilities AC x AB normal must face +Z",
              "effective_texture_uv": "identity TexCoord0; source shader ignores serialized and runtime _MainTex tiling",
              "native_uv0_proves_serialized_slot3": True,
              "visual_acceptance": "pending normal-world Farm/Wood + bottom render comparison",
              "apply": os.environ.get(APPLY) == "1",
              "explicit_v2_repair": os.environ.get(REPAIR_V2) == "1"}
    if not result["apply"]:
        result["existing_assets"] = {role: unreal.load_asset(path) is not None for role, path in ASSETS.items()}
        output = unique(EVIDENCE / "readonly_preflight.json")
        output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        unreal.log(f"ROK_PLACEMENT_BOTTOM_READONLY_PREFLIGHT {output}")
        return result
    if os.environ.get(REPAIR) == "1":
        raise RuntimeError(f"Historical {REPAIR} is not a source-v2 repair route; use {REPAIR_V2}")
    if result["explicit_v2_repair"]:
        texture_file = ROOT / "Content/RokUnity/City/PlacementPreview/T_buiding_bottom2_Source.uasset"
        material_file = ROOT / "Content/RokUnity/City/PlacementPreview/M_building_move_bottom_SourceAlpha.uasset"
        mesh_file = ROOT / "Content/RokUnity/City/PlacementPreview/SM_building_move_bottom_SourceQuad.uasset"
        if (not texture_file.is_file() or sha(texture_file) != V1_TEXTURE_SHA256 or
                not material_file.is_file() or not mesh_file.is_file()):
            raise RuntimeError("Source-v2 repair requires the pinned existing texture and both new bottom assets")
    obj_path = unique(EVIDENCE / "SM_building_move_bottom_SourceQuad_v2.obj")
    obj_path.write_text(obj, encoding="utf-8")
    texture = make_texture(run, native_path)
    if result["explicit_v2_repair"]:
        result["repair_v2"] = repair_source_v2(texture, run, native_path, obj_path, vertices, native)
        result["mesh_validation"] = result["repair_v2"]["mesh_validation"]
        result["material_validation"] = result["repair_v2"]["material_validation"]
    else:
        material = make_material(texture, run, native_path)
        result["material_validation"] = verify_material(material, texture, run, native_path)
        result["mesh_validation"] = make_mesh(material, run, native_path, obj_path, vertices, native)
    result["obj_path"] = str(obj_path)
    result["asset_files_sha256"] = {
        role: sha(ROOT / "Content" / (path.removeprefix("/Game/") + ".uasset"))
        for role, path in ASSETS.items()}
    result["passed"] = True
    output = unique(EVIDENCE / "ue_assets_create.json")
    output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    unreal.log(f"ROK_PLACEMENT_BOTTOM_ASSETS_PASS {output}")
    return result


if __name__ == "__main__":
    main()
