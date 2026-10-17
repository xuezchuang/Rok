"""Read-only preflight by default; opt-in creation of dedicated Farm/Wood assets.

Run in UE 5.8 on L_RokUnityGameAppCity. This never saves the map, places actors,
reimports assets, or edits the sealed Academy assets. A source mesh export is
used only during apply/verification to prove surviving source triangles.
"""

import collections
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

import PrepareRokCityResourcePresentationAssets as source


ROOT = source.ROOT
WORLD = "L_RokUnityGameAppCity"
RUN_ENV = "ROK_CITY_RESOURCE_NATIVE_RUN"
APPLY_ENV = "ROK_CITY_RESOURCE_ASSETS_APPLY"
REPORT = ROOT / "Saved/RokUnityMigration/CityResourcePresentationAssets/ue_assets_create.json"
ACCEPTANCE = "native-source-probed;ue-runtime-render-pending"

# Pin the source R/G shader translation in this resource-building slice. A later
# Academy material revision must not silently change these dedicated assets.
CODE = """float4 col = MainSample * Renderer;
float red2 = MaskSample.r * 2.0;
float team1 = saturate(red2 - 1.0);
float3 gammaColor = col.rgb * ((1.0 - team1 - MaskSample.g)
    + 2.0 * Team1.rgb * team1 + 2.0 * Team2.rgb * MaskSample.g);
gammaColor *= Lighting.rgb;
float3 linearColor = lerp(gammaColor / 12.92,
    pow(max((gammaColor + 0.055) / 1.055, 0.0), 2.4),
    step(0.04045, gammaColor));
float opacity = col.a * saturate(red2) * Transparency;
return float4(linearColor, opacity);
"""


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def next_evidence_path(base):
    path = base
    suffix = 2
    while path.exists():
        path = base.with_name(f"{base.stem}_{suffix}{base.suffix}")
        suffix += 1
    return path


def identity():
    project = Path(unreal.Paths.project_dir()).resolve()
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = subsystem.get_editor_world() if subsystem else None
    if project != ROOT.resolve() or world is None or world.get_name() != WORLD:
        raise RuntimeError(f"Wrong resource asset target: project={project}; world={world.get_name() if world else None}")
    return world


def static_mesh_editor():
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem is None:
        raise RuntimeError("UE5.8 StaticMeshEditorSubsystem unavailable; cannot verify UV build settings")
    return subsystem


def native_run():
    value = os.environ.get(RUN_ENV, "")
    if not value:
        raise RuntimeError(f"Set {RUN_ENV} to exact verified OutputRuns child")
    run = Path(value).resolve()
    if run.parent != source.RUNS.resolve():
        raise RuntimeError("Resource native run is outside exact OutputRuns children")
    return run


def prepared_contract(run):
    catalog, summary, records = source.load_source(run)
    base = source.OUTPUT / run.name
    prepared_path = base / "prepared.json"
    prepared = json.loads(prepared_path.read_text(encoding="utf-8"))
    if (prepared["schema"] != "rok.city_resource_source_mesh_prepared.v1" or
            Path(prepared["run"]).resolve() != run or
            prepared["source_catalog_sha256"] != sha(source.CATALOG) or
            prepared["native_verified_capture_sha256"] != sha(source.SUMMARY) or
            [m["model"] for m in prepared["models"]] != list(source.MODELS)):
        raise RuntimeError("Prepared resource manifest/run identity differs")
    result = {}
    for model_id, listed in zip(source.MODELS, prepared["models"]):
        record = records[model_id]
        geometry = record["native_render"]["geometry"]
        text, vertices, faces = source.obj_text(model_id, geometry)
        target = base / model_id
        manifest_path = target / "source_mesh_manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        obj_path = target / manifest["obj_file"]
        names = source.asset_names(model_id)
        package = "/Game/RokUnity/City/ResourceBuildings/" + model_id
        indices = geometry["triangles"]
        original = geometry["vertices"]
        zero = []
        for i in range(0, len(indices), 3):
            a, b, c = (original[indices[i + j]] for j in range(3))
            area = (b["x"] - a["x"]) * (c["y"] - a["y"]) - (b["y"] - a["y"]) * (c["x"] - a["x"])
            if area == 0.0:
                zero.append(i // 3)
        child_p = geometry["childLocalPosition"]
        child_q = geometry["childLocalRotation"]
        child_s = geometry["childLocalScale"]
        relative = {
            "translation_cm": [-child_p["x"] * 500.0, child_p["z"] * 500.0, child_p["y"] * 500.0],
            "rotation_quaternion_xyzw": [-child_q["x"], child_q["z"], child_q["y"], child_q["w"]],
            "uniform_scale": child_s["x"] * 5.0,
            "scope": "Source prefab child-local pose relative to root; runtime root/grid pose supplied separately",
        }
        checks = (
            manifest["schema"] == "rok.city_resource_source_sprite_mesh.v1",
            manifest["model"] == model_id,
            Path(manifest["source_run"]).resolve() == run,
            manifest["obj_file"] == names["mesh"] + ".obj",
            obj_path.parent.resolve() == target.resolve(),
            obj_path.read_bytes() == text.encode("utf-8"),
            manifest["obj_sha256"] == sha(obj_path) == listed["obj_sha256"],
            manifest["source_catalog_sha256"] == sha(source.CATALOG),
            manifest["native_verified_capture_sha256"] == sha(source.SUMMARY),
            manifest["native_render_sha256"] == sha(record["native_dir"] / "native_render.json"),
            manifest["native_verify_sha256"] == sha(record["native_dir"] / "native_verify.json"),
            manifest["source_geometry_sha256"] == source.canonical_sha({key: geometry[key] for key in
                ("vertices", "uvs", "triangles", "rect", "pivot", "pixelsPerUnit")}),
            manifest["source_sprite_png_sha256"] == record["catalog"]["sprite"]["sha256"],
            manifest["source_mask_png_sha256"] == record["catalog"]["mask"]["sha256"],
            manifest["gpu_mask_export_sha256"] == record["native_verify"]["gpu_mask_export_sha256"],
            manifest["vertex_count"] == len(vertices) == listed["vertex_count"],
            manifest["uv_count"] == len(vertices),
            manifest["source_triangle_count"] == len(faces) == listed["source_triangle_count"],
            manifest["source_zero_area_triangle_indices"] == zero,
            manifest["source_nonzero_area_triangle_count"] == len(faces) - len(zero),
            manifest["ue_local_bounds_min_cm"] == [min(v[i] for v in vertices) for i in range(3)],
            manifest["ue_local_bounds_max_cm"] == [max(v[i] for v in vertices) for i in range(3)],
            manifest["unity_sprite_rect"] == geometry["rect"],
            manifest["unity_sprite_pivot_pixels"] == geometry["pivot"],
            manifest["unity_sprite_pixels_per_unit"] == geometry["pixelsPerUnit"],
            manifest["component_relative_transform_example"] == relative,
            manifest["coordinate_mapping"] == "Unity local (x,y,z) -> UE local (-x,z,y)*100; FbxFactory OBJ inverse Y compensation",
            manifest["face_order"] == "Unity source triangle (a,b,c) -> OBJ (c,b,a); zero-area source triangles retained in OBJ",
            manifest["uv_mapping"] == "Unity Sprite.uv -> OBJ vt(u,v); runtime import V orientation pending visual audit",
            manifest["project_ue_units_per_unity_unit"] == 500.0,
            manifest["obj_ue_units_per_unity_unit"] == 100.0,
            manifest["ue_assets"] == {role: package + "/" + name for role, name in names.items()},
            listed["manifest_sha256"] == sha(manifest_path),
            Path(listed["manifest"]).resolve() == manifest_path.resolve(),
        )
        if not all(checks):
            raise RuntimeError(f"Prepared exact source mesh/manifest differs: {model_id}")
        if model_id == "build_2_101_1" and len(zero) != 14 or model_id == "build_3_101_1" and zero:
            raise RuntimeError(f"Source exact-zero face proof changed: {model_id}")
        result[model_id] = (record, manifest, manifest_path, obj_path)
    return catalog, summary, result


def flags():
    return {
        "srgb": False, "compression_settings": unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "filter": unreal.TextureFilter.TF_BILINEAR,
        "address_x": unreal.TextureAddress.TA_CLAMP,
        "address_y": unreal.TextureAddress.TA_CLAMP, "never_stream": True,
    }


def meta_texture(model, record, role):
    item = record["catalog"][role]
    rendered = record["native_render"]
    result = {
        "RokResourceModel": model, "RokResourceRole": role,
        "RokResourceCatalogSHA256": sha(source.CATALOG),
        "RokResourceNativeVerifiedSHA256": sha(source.SUMMARY),
        "RokResourceSourcePngSHA256": item["sha256"],
        "RokResourceSourceMetaSHA256": item["meta_sha256"],
        "RokResourceSourcePpu": item["importer"]["pixels_to_units"],
        "RokResourceSourcePivot": json.dumps(item["importer"]["pivot"], separators=(",", ":")),
        "RokResourceSourceMeshType": item["importer"]["sprite_mesh_type"],
        "RokResourceNativeImport": json.dumps(rendered[role + "Import"], sort_keys=True, separators=(",", ":")),
        "RokResourceAcceptance": ACCEPTANCE,
    }
    if role == "mask":
        result.update({
            "RokResourceGpuMaskExportSHA256": record["native_verify"]["gpu_mask_export_sha256"],
            "RokResourceGpuMaskExportMethod": rendered["maskExport"]["method"],
            "RokResourceGpuMaskSize": f"{rendered['maskTextureWidth']}x{rendered['maskTextureHeight']}",
            "RokResourceGpuExportControlMax": 0,
        })
    return result


def meta_material(model, record):
    catalog_model = record["catalog"]
    return {
        "RokResourceModel": model,
        "RokResourceCatalogSHA256": sha(source.CATALOG),
        "RokResourceNativeVerifiedSHA256": sha(source.SUMMARY),
        "RokResourceShaderSHA256": sha(ROOT / source.build(write=False)["material"]["shader_path"]),
        "RokResourceMaterialSHA256": sha(ROOT / source.build(write=False)["material"]["path"]),
        "RokResourceHlslSHA256": hashlib.sha256(CODE.encode("utf-8")).hexdigest(),
        "RokResourceMaskBinding": json.dumps(catalog_model["mask_binding"], sort_keys=True, separators=(",", ":")),
        "RokResourceGammaTransfer": "source-Gamma-arithmetic-then-sRGB-decode-to-UE-linear",
        "RokResourceAcceptance": ACCEPTANCE,
    }


def meta_mesh(model, manifest):
    return {
        "RokResourceModel": model, "RokResourceSourceGeometrySHA256": manifest["source_geometry_sha256"],
        "RokResourceObjSHA256": manifest["obj_sha256"],
        "RokResourceNativeRenderSHA256": manifest["native_render_sha256"],
        "RokResourceSourceCatalogSHA256": manifest["source_catalog_sha256"],
        "RokResourceSourceSpriteSHA256": manifest["source_sprite_png_sha256"],
        "RokResourceSourceZeroAreaFaces": json.dumps(manifest["source_zero_area_triangle_indices"], separators=(",", ":")),
        "RokResourceUvMapping": manifest["uv_mapping"],
        "RokResourceRuntimePose": "local-source-mesh-only;runtime-root-grid-pose-not-baked",
        "RokResourceAcceptance": ACCEPTANCE,
    }


def check_meta(asset, expected):
    for key, value in expected.items():
        if str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key)) != str(value):
            raise RuntimeError(f"{asset.get_path_name()} metadata differs: {key}")


def set_meta(asset, expected):
    for key, value in expected.items():
        unreal.EditorAssetLibrary.set_metadata_tag(asset, key, str(value))


def texture_source(record, role):
    if role == "sprite":
        return ROOT / record["catalog"]["sprite"]["path"]
    return record["native_dir"] / record["native_render"]["maskExport"]["png"]


def verify_texture(asset, model, record, role):
    if not isinstance(asset, unreal.Texture2D):
        raise RuntimeError(f"Resource {model}/{role} is not Texture2D")
    for key, value in flags().items():
        if asset.get_editor_property(key) != value:
            raise RuntimeError(f"Resource {model}/{role} texture flag differs: {key}")
    size = record["catalog"]["sprite"]["size"] if role == "sprite" else list(source.MASK_SIZES[model])
    actual_size = [int(asset.blueprint_get_size_x()), int(asset.blueprint_get_size_y())]
    if actual_size != size:
        # UE5.8 GetSizeX/Y returns placeholder PlatformData dimensions during
        # async texture compilation. The engine's public texture finish-all
        # console command blocks that work without saving/modifying assets.
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = subsystem.get_editor_world() if subsystem else None
        if world is None or world.get_name() != WORLD:
            raise RuntimeError(f"Resource {model}/{role} cannot finish texture compile outside {WORLD}")
        unreal.SystemLibrary.execute_console_command(world, "Editor.AsyncTextureCompilationFinishAll")
        after = [int(asset.blueprint_get_size_x()), int(asset.blueprint_get_size_y())]
        unreal.log(f"ROK_RESOURCE_TEXTURE_COMPILE_SIZE {model}/{role} before={actual_size} after={after}")
        actual_size = after
    if actual_size != size:
        raise RuntimeError(f"Resource {model}/{role} texture size differs")
    check_meta(asset, meta_texture(model, record, role))


def create_texture(path, model, record, role):
    existing = unreal.load_asset(path)
    if existing is not None:
        verify_texture(existing, model, record, role)
        return existing, False
    filename = texture_source(record, role)
    task = unreal.AssetImportTask()
    for key, value in {"filename": str(filename), "destination_path": path.rsplit("/", 1)[0],
                       "destination_name": path.rsplit("/", 1)[1], "factory": unreal.TextureFactory(),
                       "automated": True, "replace_existing": False, "save": False}.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.Texture2D):
        raise RuntimeError(f"Resource {model}/{role} PNG import did not create target")
    for key, value in flags().items():
        asset.set_editor_property(key, value)
    set_meta(asset, meta_texture(model, record, role))
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"Resource {model}/{role} texture save failed")
    verify_texture(asset, model, record, role)
    return asset, True


def expected_colors(catalog, record):
    model = record["catalog"]
    main = next(n for n in model["prefab"]["nodes"] if n["file_id"] == model["prefab"]["main_sprite_game_object_file_id"])
    return {"Color1": catalog["material"]["color_1"], "Color2": catalog["material"]["color_2"],
            "RendererColor": main["renderer"]["color"],
            "SpriteColor": dict(zip("rgba", [1.07203974, 1.07203974, 1.07203974, 1.0]))}


def verify_material(asset, model, catalog, record, sprite, mask):
    if not isinstance(asset, unreal.Material):
        raise RuntimeError(f"Resource {model} material is not Material")
    for key, value in (("material_domain", unreal.MaterialDomain.MD_SURFACE),
                       ("shading_model", unreal.MaterialShadingModel.MSM_UNLIT),
                       ("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT), ("two_sided", True)):
        if asset.get_editor_property(key) != value:
            raise RuntimeError(f"Resource {model} material property differs: {key}")
    check_meta(asset, meta_material(model, record))
    edit = unreal.MaterialEditingLibrary
    expressions = list(edit.get_material_expressions(asset))
    if len(expressions) != 10:
        raise RuntimeError(f"Resource {model} material has {len(expressions)} expressions, expected 10")
    def only(cls, count):
        result = [n for n in expressions if isinstance(n, cls)]
        if len(result) != count:
            raise RuntimeError(f"Resource {model} material {cls.__name__} count differs")
        return result
    samples = only(unreal.MaterialExpressionTextureSampleParameter2D, 2)
    vectors = only(unreal.MaterialExpressionVectorParameter, 4)
    scalars = only(unreal.MaterialExpressionScalarParameter, 1)
    customs = only(unreal.MaterialExpressionCustom, 1)
    masks = only(unreal.MaterialExpressionComponentMask, 2)
    samples = {str(n.get_editor_property("parameter_name")): n for n in samples}
    vectors = {str(n.get_editor_property("parameter_name")): n for n in vectors}
    if set(samples) != {"Sprite", "Mask"} or set(vectors) != set(expected_colors(catalog, record)):
        raise RuntimeError(f"Resource {model} material parameters differ")
    for role, texture in (("Sprite", sprite), ("Mask", mask)):
        if samples[role].get_editor_property("texture") != texture or \
           samples[role].get_editor_property("sampler_type") != unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR:
            raise RuntimeError(f"Resource {model} {role} texture sample differs")
    for name, expected in expected_colors(catalog, record).items():
        actual = vectors[name].get_editor_property("default_value")
        if any(abs(getattr(actual, key) - expected[key]) > 1e-5 for key in "rgba"):
            raise RuntimeError(f"Resource {model} color differs: {name}")
    scalar = scalars[0]
    if str(scalar.get_editor_property("parameter_name")) != "CityTransparency" or \
       abs(scalar.get_editor_property("default_value") - catalog["material"]["city_transparency"]) > 1e-5:
        raise RuntimeError(f"Resource {model} transparency differs")
    custom = customs[0]
    names = [str(n.get_editor_property("input_name")) for n in custom.get_editor_property("inputs")]
    if (custom.get_editor_property("code") != CODE or
            custom.get_editor_property("output_type") != unreal.CustomMaterialOutputType.CMOT_FLOAT4 or
            names != ["MainSample", "MaskSample", "Team1", "Team2", "Renderer", "Lighting", "Transparency"] or
            list(edit.get_inputs_for_material_expression(asset, custom)) !=
            [samples["Sprite"], samples["Mask"], vectors["Color1"], vectors["Color2"],
             vectors["RendererColor"], vectors["SpriteColor"], scalar]):
        raise RuntimeError(f"Resource {model} R/G shader code/inputs differ")
    emissive = edit.get_material_property_input_node(asset, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = edit.get_material_property_input_node(asset, unreal.MaterialProperty.MP_OPACITY)
    if (emissive not in masks or opacity not in masks or emissive == opacity or
            not all(bool(emissive.get_editor_property(k)) for k in "rgb") or
            bool(emissive.get_editor_property("a")) or
            any(bool(opacity.get_editor_property(k)) for k in "rgb") or
            not bool(opacity.get_editor_property("a")) or
            any(list(edit.get_inputs_for_material_expression(asset, node)) != [custom]
                for node in (emissive, opacity))):
        raise RuntimeError(f"Resource {model} material output topology differs")
    return {"expression_count": 10, "shader_sha256": hashlib.sha256(CODE.encode("utf-8")).hexdigest()}


def create_material(path, model, catalog, record, sprite, mask):
    existing = unreal.load_asset(path)
    if existing is not None:
        verify_material(existing, model, catalog, record, sprite, mask)
        return existing, False
    name = path.rsplit("/", 1)[1]
    parent = path.rsplit("/", 1)[0]
    edit = unreal.MaterialEditingLibrary
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, parent, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(asset, unreal.Material):
        raise RuntimeError(f"Resource {model} material creation failed")
    for key, value in (("material_domain", unreal.MaterialDomain.MD_SURFACE),
                       ("shading_model", unreal.MaterialShadingModel.MSM_UNLIT),
                       ("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT), ("two_sided", True)):
        asset.set_editor_property(key, value)
    def node(cls):
        return edit.create_material_expression(asset, cls)
    def connect(a, output, b, input_name):
        if not edit.connect_material_expressions(a, output, b, input_name):
            raise RuntimeError(f"Resource {model} material connection failed: {input_name}")
    samples = {}
    for role, texture in (("Sprite", sprite), ("Mask", mask)):
        n = node(unreal.MaterialExpressionTextureSampleParameter2D)
        n.set_editor_property("parameter_name", role)
        n.set_editor_property("texture", texture)
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        samples[role] = n
    colors = {}
    for name, rgba in expected_colors(catalog, record).items():
        n = node(unreal.MaterialExpressionVectorParameter)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", unreal.LinearColor(*(rgba[k] for k in "rgba")))
        colors[name] = n
    scalar = node(unreal.MaterialExpressionScalarParameter)
    scalar.set_editor_property("parameter_name", "CityTransparency")
    scalar.set_editor_property("default_value", catalog["material"]["city_transparency"])
    custom = node(unreal.MaterialExpressionCustom)
    custom.set_editor_property("code", CODE)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property("description", "Unity resource source R/G Gamma arithmetic then sRGB decode")
    names = ("MainSample", "MaskSample", "Team1", "Team2", "Renderer", "Lighting", "Transparency")
    inputs = []
    for name in names:
        item = unreal.CustomInput()
        item.set_editor_property("input_name", name)
        inputs.append(item)
    custom.set_editor_property("inputs", inputs)
    for upstream, output, name in ((samples["Sprite"], "RGBA", "MainSample"),
                                   (samples["Mask"], "RGBA", "MaskSample"),
                                   (colors["Color1"], "RGBA", "Team1"),
                                   (colors["Color2"], "RGBA", "Team2"),
                                   (colors["RendererColor"], "RGBA", "Renderer"),
                                   (colors["SpriteColor"], "RGBA", "Lighting"),
                                   (scalar, "", "Transparency")):
        connect(upstream, output, custom, name)
    rgb = node(unreal.MaterialExpressionComponentMask)
    alpha = node(unreal.MaterialExpressionComponentMask)
    for k in "rgb":
        rgb.set_editor_property(k, True)
        alpha.set_editor_property(k, False)
    rgb.set_editor_property("a", False)
    alpha.set_editor_property("a", True)
    connect(custom, "", rgb, "")
    connect(custom, "", alpha, "")
    if not edit.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR) or \
       not edit.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError(f"Resource {model} material output connection failed")
    set_meta(asset, meta_material(model, record))
    errors = [str(e) for e in edit.recompile_material(asset)]
    if errors:
        raise RuntimeError(f"Resource {model} material compile failed: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"Resource {model} material save failed")
    verify_material(asset, model, catalog, record, sprite, mask)
    return asset, True


def _canonical_obj_position(position, manifest, source_axes):
    # FbxFactory maps source OBJ (x, 0, z) to UE (x, z, 0); Unreal's OBJ
    # exporter then writes UE axes. Unity Sprite.mesh vertices lie on the
    # source pixel lattice defined by rect/pivot/PPU. Validate floating-point
    # roundtrip residue before recovering those exact pixel-cell coordinates.
    x, y, z = position
    if source_axes:
        y, z = z, y
    pivot = manifest["unity_sprite_pivot_pixels"]
    lattice_x = pivot["x"] - round(pivot["x"] - x)
    lattice_y = round(y + pivot["y"]) - pivot["y"]
    def float32_roundtrip_bound(value):
        def ulp32(number):
            return 2.0 ** (math.floor(math.log2(max(abs(number), 1e-30))) - 23)
        return max(1e-6, 100.0 * ulp32(value / 100.0) + ulp32(value))
    if (abs(x - lattice_x) > float32_roundtrip_bound(x) or
            abs(y - lattice_y) > float32_roundtrip_bound(y) or
            abs(z) > float32_roundtrip_bound(z)):
        raise RuntimeError("Resource OBJ vertex is outside source Sprite pixel lattice/float32 roundtrip")
    return (lattice_x, lattice_y, 0.0)


def _face_keys(path, manifest, source_axes):
    vertices, uvs, faces = [], [], []
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v":
            vertices.append(tuple(float(v) for v in parts[1:4]))
        elif parts[0] == "vt":
            uvs.append(tuple(float(v) for v in parts[1:3]))
        elif parts[0] == "f":
            if len(parts) != 4:
                raise RuntimeError("Exported resource OBJ contains nontriangle face")
            corners = []
            for token in parts[1:]:
                fields = token.split("/")
                if len(fields) < 2 or not fields[1]:
                    raise RuntimeError("Exported resource OBJ face lacks UV")
                v = _canonical_obj_position(vertices[int(fields[0]) - 1], manifest, source_axes)
                uv = uvs[int(fields[1]) - 1]
                corners.append(tuple(round(x, 5) for x in v) + tuple(round(x, 6) for x in uv))
            faces.append(tuple(sorted(corners)))
    return vertices, uvs, collections.Counter(faces), faces


def _nonzero_source_vertex_counts(obj_path, zero_indices):
    vertices, uvs, faces = [], [], []
    for line in obj_path.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v":
            vertices.append(tuple(float(value) for value in parts[1:4]))
        elif parts[0] == "vt":
            uvs.append(tuple(float(value) for value in parts[1:3]))
        elif parts[0] == "f":
            faces.append(parts[1:])
    used_indices, position_uv = set(), set()
    for face_index, face in enumerate(faces):
        if face_index in zero_indices:
            continue
        if len(face) != 3:
            raise RuntimeError("Resource source OBJ has a nontriangle face")
        for corner in face:
            fields = corner.split("/")
            if len(fields) < 2 or not fields[1]:
                raise RuntimeError("Resource source OBJ face lacks source UV")
            vertex_index, uv_index = int(fields[0]) - 1, int(fields[1]) - 1
            used_indices.add(vertex_index)
            position_uv.add((vertices[vertex_index], uvs[uv_index]))
    return len(used_indices), len(position_uv)


def verify_mesh(asset, model, manifest, obj_path, material, export_faces=False):
    if not isinstance(asset, unreal.StaticMesh):
        raise RuntimeError(f"Resource {model} mesh is not StaticMesh")
    source_count = manifest["source_triangle_count"]
    zero_count = len(manifest["source_zero_area_triangle_indices"])
    source_used, source_unique = _nonzero_source_vertex_counts(
        obj_path, set(manifest["source_zero_area_triangle_indices"]))
    actual = {"vertex_count_lod0": int(asset.get_num_vertices(0)),
              "triangle_count_lod0": int(asset.get_num_triangles(0)),
              "uv_channel_count_lod0": int(asset.get_num_tex_coords(0))}
    build = static_mesh_editor().get_lod_build_settings(asset, 0)
    if not build.get_editor_property("use_full_precision_u_vs") or build.get_editor_property("remove_degenerates"):
        raise RuntimeError(f"Resource {model} mesh UV precision/degenerate import flags differ")
    if (not source_unique <= actual["vertex_count_lod0"] <= source_used or
            actual["uv_channel_count_lod0"] != 1 or
            not source_count - zero_count <= actual["triangle_count_lod0"] <= source_count):
        raise RuntimeError(f"Resource {model} mesh counts differ: {actual}")
    actual["source_used_vertex_indices_nonzero_faces"] = source_used
    actual["source_unique_position_uv_nonzero_faces"] = source_unique
    if asset.get_editor_property("static_materials")[0].get_editor_property("material_interface") != material:
        raise RuntimeError(f"Resource {model} mesh material slot differs")
    bounds = asset.get_bounds()
    lo = [bounds.origin.x - bounds.box_extent.x, bounds.origin.y - bounds.box_extent.y,
          bounds.origin.z - bounds.box_extent.z]
    hi = [bounds.origin.x + bounds.box_extent.x, bounds.origin.y + bounds.box_extent.y,
          bounds.origin.z + bounds.box_extent.z]
    if any(abs(value - target) > 0.05 for measured, pinned in
           ((lo, manifest["ue_local_bounds_min_cm"]), (hi, manifest["ue_local_bounds_max_cm"]))
           for value, target in zip(measured, pinned)):
        raise RuntimeError(f"Resource {model} mesh pivot/bounds differ: {lo}; {hi}")
    check_meta(asset, meta_mesh(model, manifest))
    if export_faces:
        # Editor export is to Saved only. Never infer face equivalence from counts.
        export_dir = source.OUTPUT / Path(manifest["source_run"]).name / model
        output = next_evidence_path(export_dir / "ue_imported_lod0_verify.obj")
        task = unreal.AssetExportTask()
        task.set_editor_property("object", asset)
        task.set_editor_property("filename", str(output))
        task.set_editor_property("exporter", unreal.StaticMeshExporterOBJ())
        task.set_editor_property("automated", True)
        task.set_editor_property("prompt", False)
        task.set_editor_property("replace_identical", False)
        if not unreal.Exporter.run_asset_export_task(task) or not output.is_file():
            raise RuntimeError(f"Resource {model} mesh face export failed; cannot prove source UV topology")
        _, _, expected, source_faces = _face_keys(obj_path, manifest, source_axes=True)
        _, _, observed, _ = _face_keys(output, manifest, source_axes=False)
        missing = expected - observed
        extra = observed - expected
        # Farm's exact zero-area source faces may be removed by FbxFactory.
        # Each missing signature must be one of those exact source faces.
        zero_allowed = collections.Counter(
            source_faces[i] for i in manifest["source_zero_area_triangle_indices"])
        if extra or missing - zero_allowed:
            raise RuntimeError(f"Resource {model} imported face positions/UV differ: missing={sum(missing.values())}, extra={sum(extra.values())}")
        if sum(missing.values()) != source_count - actual["triangle_count_lod0"]:
            raise RuntimeError(f"Resource {model} face export vs LOD0 count differs")
        actual["source_zero_area_omitted"] = sum(missing.values())
        actual["source_nonzero_faces_proved"] = source_count - zero_count
        actual["ue_export_obj_path"] = str(output)
        actual["ue_export_obj_sha256"] = sha(output)
    actual["bounds_min_cm"], actual["bounds_max_cm"] = lo, hi
    return actual


def create_mesh(path, model, manifest, obj_path, material):
    existing = unreal.load_asset(path)
    if existing is not None:
        return existing, False
    task = unreal.AssetImportTask()
    for key, value in {"filename": str(obj_path), "destination_path": path.rsplit("/", 1)[0],
                       "destination_name": path.rsplit("/", 1)[1], "automated": True,
                       "replace_existing": False, "replace_existing_settings": False, "save": False}.items():
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
        raise RuntimeError(f"Resource {model} source OBJ import failed")
    build = static_mesh_editor().get_lod_build_settings(asset, 0)
    build.set_editor_property("use_full_precision_u_vs", True)
    build.set_editor_property("remove_degenerates", False)
    static_mesh_editor().set_lod_build_settings(asset, 0, build)
    if not static_mesh_editor().get_lod_build_settings(asset, 0).get_editor_property("use_full_precision_u_vs"):
        raise RuntimeError(f"Resource {model} full-precision UV build setting was not applied")
    materials = list(asset.get_editor_property("static_materials"))
    if len(materials) != 1:
        raise RuntimeError(f"Resource {model} imported mesh material slots differ: {len(materials)}")
    materials[0].set_editor_property("material_interface", material)
    asset.set_editor_property("static_materials", materials)
    set_meta(asset, meta_mesh(model, manifest))
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"Resource {model} source mesh save failed")
    return asset, True


def preflight():
    identity()
    run = native_run()
    catalog, _, models = prepared_contract(run)
    plan = []
    for model, (record, manifest, _, obj_path) in models.items():
        paths = manifest["ue_assets"]
        existing = {role: unreal.load_asset(path) for role, path in paths.items()}
        if existing["sprite"] is not None:
            verify_texture(existing["sprite"], model, record, "sprite")
        if existing["mask"] is not None:
            verify_texture(existing["mask"], model, record, "mask")
        if existing["material"] is not None:
            if existing["sprite"] is None or existing["mask"] is None:
                raise RuntimeError(f"Resource {model} existing material lacks pinned textures")
            verify_material(existing["material"], model, catalog, record,
                            existing["sprite"], existing["mask"])
        mesh_proof = None
        if existing["mesh"] is not None:
            if existing["material"] is None:
                raise RuntimeError(f"Resource {model} existing mesh lacks pinned material")
            mesh_proof = verify_mesh(existing["mesh"], model, manifest, obj_path,
                                     existing["material"], export_faces=True)
        plan.append({"model": model, "assets": paths,
                     "create_roles": [role for role, value in existing.items() if value is None],
                     "source_zero_area_faces": len(manifest["source_zero_area_triangle_indices"]),
                     "existing_mesh_proof": mesh_proof})
    return catalog, models, plan


def apply():
    catalog, models, plan = preflight()  # Must finish identity/source/existing checks before first write.
    if os.environ.get(APPLY_ENV) != "1":
        raise RuntimeError(f"Resource asset creation requires {APPLY_ENV}=1")
    details = []
    for model, (record, manifest, manifest_path, obj_path) in models.items():
        paths = manifest["ue_assets"]
        unreal.EditorAssetLibrary.make_directory(paths["sprite"].rsplit("/", 1)[0])
        sprite, made_sprite = create_texture(paths["sprite"], model, record, "sprite")
        mask, made_mask = create_texture(paths["mask"], model, record, "mask")
        material, made_material = create_material(paths["material"], model, catalog, record, sprite, mask)
        mesh, made_mesh = create_mesh(paths["mesh"], model, manifest, obj_path, material)
        topology = verify_mesh(mesh, model, manifest, obj_path, material, export_faces=True)
        details.append({"model": model, "assets": paths,
                        "created": {"sprite": made_sprite, "mask": made_mask,
                                    "material": made_material, "mesh": made_mesh},
                        "mesh_manifest": str(manifest_path), "mesh_manifest_sha256": sha(manifest_path),
                        "mesh_topology": topology})
    report = {"schema": "rok.city_resource_presentation_assets.ue_create.v1",
              "project": str(ROOT), "world": WORLD, "native_run": str(native_run()),
              "source_catalog_sha256": sha(source.CATALOG),
              "native_verified_capture_sha256": sha(source.SUMMARY),
              "scope": "Two source texture/mask/material/tightmesh asset sets only; no actors or map save",
              "plan": plan, "models": details, "runtime_visual_acceptance": "pending",
              "map_saved": False, "sealed_academy_assets_modified": False}
    report_path = next_evidence_path(REPORT)
    report["report_path"] = str(report_path)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    with report_path.open("x", encoding="utf-8") as output:
        output.write(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    return report


def main():
    if os.environ.get(APPLY_ENV) == "1":
        report = apply()
        unreal.log("ROK_CITY_RESOURCE_ASSETS_CREATED " + report["report_path"] + " runtime visual pending")
        return report
    _, _, plan = preflight()
    unreal.log("ROK_CITY_RESOURCE_ASSETS_READONLY_PREFLIGHT_PASS " + json.dumps(plan))
    return plan


if __name__ == "__main__":
    main()
