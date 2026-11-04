"""Read-only preflight by default; opt-in creation of city-building source assets.

Run in UE 5.8 on L_RokUnityGameAppCity with ROK_CITY_BUILDING_NATIVE_RUN set to the
verified OutputRuns child (and ROK_CITY_BUILDING_ASSETS_APPLY=1 to create). Same asset
contract as CreateRokCityResourcePresentationAssets.py (sprite, GPU mask, R/G material,
tight Sprite.mesh), written under /Game/RokUnity/City/SourceBuildings/<model>. Never
saves the map, places actors, or touches the Farm/Wood or Academy assets.
"""

import collections
import hashlib
import json
import os
from pathlib import Path

import unreal

import CreateRokCityResourcePresentationAssets as resource
import PrepareRokCityBuildingPresentationAssets as source


ROOT = source.ROOT
WORLD = resource.WORLD
CODE = resource.CODE  # Same pinned source R/G Gamma translation as the Farm/Wood slice.
RUN_ENV = "ROK_CITY_BUILDING_NATIVE_RUN"
APPLY_ENV = "ROK_CITY_BUILDING_ASSETS_APPLY"
REPORT = ROOT / "Saved/RokUnityMigration/CityBuildingPresentationAssets/ue_assets_create.json"
ACCEPTANCE = "native-source-probed;ue-runtime-render-pending"
sha = resource.sha
check_meta = resource.check_meta
set_meta = resource.set_meta
flags = resource.flags


def native_run():
    value = os.environ.get(RUN_ENV, "")
    if not value:
        raise RuntimeError(f"Set {RUN_ENV} to exact verified OutputRuns child")
    run = Path(value).resolve()
    if run.parent != source.RUNS.resolve():
        raise RuntimeError("City building native run is outside exact OutputRuns children")
    return run


def prepared_contract(run):
    catalog, summary, records = source.load_source(run)
    base = source.OUTPUT / run.name
    prepared = json.loads((base / "prepared.json").read_text(encoding="utf-8"))
    if (prepared["schema"] != "rok.city_building_source_mesh_prepared.v1" or
            Path(prepared["run"]).resolve() != run or
            prepared["source_catalog_sha256"] != sha(source.CATALOG) or
            prepared["native_verified_capture_sha256"] != sha(source.SUMMARY) or
            [m["model"] for m in prepared["models"]] != list(records)):
        raise RuntimeError("Prepared city building manifest/run identity differs")
    result = {}
    for model_id, listed in zip(records, prepared["models"]):
        record = records[model_id]
        geometry = record["native_render"]["geometry"]
        text, vertices, faces = source.obj_text(model_id, geometry)
        target = base / model_id
        manifest_path = target / "source_mesh_manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        obj_path = target / manifest["obj_file"]
        names = source.asset_names(model_id)
        package = source.PACKAGE_ROOT + model_id
        zero = source.zero_area_faces(geometry)
        checks = (
            manifest["schema"] == "rok.city_building_source_sprite_mesh.v1",
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
            manifest["gpu_mask_size"] == record["mask_size"],
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
            manifest["component_relative_transform_example"] == source.component_transform(geometry),
            manifest["ue_assets"] == {role: package + "/" + name for role, name in names.items()},
            listed["manifest_sha256"] == sha(manifest_path),
            Path(listed["manifest"]).resolve() == manifest_path.resolve(),
        )
        if not all(checks):
            raise RuntimeError(f"Prepared exact source mesh/manifest differs: {model_id}")
        result[model_id] = (record, manifest, manifest_path, obj_path)
    return catalog, summary, result


def meta_texture(model, record, role):
    item = record["catalog"][role]
    rendered = record["native_render"]
    result = {
        "RokBuildingModel": model, "RokBuildingRole": role,
        "RokBuildingCatalogSHA256": sha(source.CATALOG),
        "RokBuildingNativeVerifiedSHA256": sha(source.SUMMARY),
        "RokBuildingSourcePngSHA256": item["sha256"],
        "RokBuildingSourceMetaSHA256": item["meta_sha256"],
        "RokBuildingSourcePpu": item["importer"]["pixels_to_units"],
        "RokBuildingSourcePivot": json.dumps(item["importer"]["pivot"], separators=(",", ":")),
        "RokBuildingSourceMeshType": item["importer"]["sprite_mesh_type"],
        "RokBuildingNativeImport": json.dumps(rendered[role + "Import"], sort_keys=True, separators=(",", ":")),
        "RokBuildingAcceptance": ACCEPTANCE,
    }
    if role == "mask":
        result.update({
            "RokBuildingGpuMaskExportSHA256": record["native_verify"]["gpu_mask_export_sha256"],
            "RokBuildingGpuMaskExportMethod": rendered["maskExport"]["method"],
            "RokBuildingGpuMaskSize": f"{rendered['maskTextureWidth']}x{rendered['maskTextureHeight']}",
            "RokBuildingGpuExportControlMax": 0,
        })
    return result


def meta_material(model, record, catalog):
    return {
        "RokBuildingModel": model,
        "RokBuildingCatalogSHA256": sha(source.CATALOG),
        "RokBuildingNativeVerifiedSHA256": sha(source.SUMMARY),
        "RokBuildingShaderSHA256": sha(ROOT / catalog["material"]["shader_path"]),
        "RokBuildingMaterialSHA256": sha(ROOT / catalog["material"]["path"]),
        "RokBuildingHlslSHA256": hashlib.sha256(CODE.encode("utf-8")).hexdigest(),
        "RokBuildingMaskBinding": json.dumps(record["catalog"]["mask_binding"], sort_keys=True, separators=(",", ":")),
        "RokBuildingGammaTransfer": "source-Gamma-arithmetic-then-sRGB-decode-to-UE-linear",
        "RokBuildingAcceptance": ACCEPTANCE,
    }


def meta_mesh(model, manifest):
    return {
        "RokBuildingModel": model, "RokBuildingSourceGeometrySHA256": manifest["source_geometry_sha256"],
        "RokBuildingObjSHA256": manifest["obj_sha256"],
        "RokBuildingNativeRenderSHA256": manifest["native_render_sha256"],
        "RokBuildingSourceCatalogSHA256": manifest["source_catalog_sha256"],
        "RokBuildingSourceSpriteSHA256": manifest["source_sprite_png_sha256"],
        "RokBuildingSourceZeroAreaFaces": json.dumps(manifest["source_zero_area_triangle_indices"], separators=(",", ":")),
        "RokBuildingUvMapping": manifest["uv_mapping"],
        "RokBuildingRuntimePose": "local-source-mesh-only;runtime-root-grid-pose-not-baked",
        "RokBuildingAcceptance": ACCEPTANCE,
    }


def texture_source(record, role):
    if role == "sprite":
        return ROOT / record["catalog"]["sprite"]["path"]
    return record["native_dir"] / record["native_render"]["maskExport"]["png"]


def verify_texture(asset, model, record, role):
    if not isinstance(asset, unreal.Texture2D):
        raise RuntimeError(f"Building {model}/{role} is not Texture2D")
    for key, value in flags().items():
        if asset.get_editor_property(key) != value:
            raise RuntimeError(f"Building {model}/{role} texture flag differs: {key}")
    size = record["catalog"]["sprite"]["size"] if role == "sprite" else list(record["mask_size"])
    actual_size = [int(asset.blueprint_get_size_x()), int(asset.blueprint_get_size_y())]
    if actual_size != size:
        # UE5.8 reports placeholder PlatformData sizes during async texture compilation.
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = subsystem.get_editor_world() if subsystem else None
        if world is None or world.get_name() != WORLD:
            raise RuntimeError(f"Building {model}/{role} cannot finish texture compile outside {WORLD}")
        unreal.SystemLibrary.execute_console_command(world, "Editor.AsyncTextureCompilationFinishAll")
        actual_size = [int(asset.blueprint_get_size_x()), int(asset.blueprint_get_size_y())]
    if actual_size != size:
        raise RuntimeError(f"Building {model}/{role} texture size differs: {actual_size} != {size}")
    check_meta(asset, meta_texture(model, record, role))


def create_texture(path, model, record, role):
    existing = unreal.load_asset(path)
    if existing is not None:
        verify_texture(existing, model, record, role)
        return existing, False
    task = unreal.AssetImportTask()
    for key, value in {"filename": str(texture_source(record, role)), "destination_path": path.rsplit("/", 1)[0],
                       "destination_name": path.rsplit("/", 1)[1], "factory": unreal.TextureFactory(),
                       "automated": True, "replace_existing": False, "save": False}.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.Texture2D):
        raise RuntimeError(f"Building {model}/{role} PNG import did not create target")
    for key, value in flags().items():
        asset.set_editor_property(key, value)
    set_meta(asset, meta_texture(model, record, role))
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"Building {model}/{role} texture save failed")
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
        raise RuntimeError(f"Building {model} material is not Material")
    for key, value in (("material_domain", unreal.MaterialDomain.MD_SURFACE),
                       ("shading_model", unreal.MaterialShadingModel.MSM_UNLIT),
                       ("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT), ("two_sided", True)):
        if asset.get_editor_property(key) != value:
            raise RuntimeError(f"Building {model} material property differs: {key}")
    check_meta(asset, meta_material(model, record, catalog))
    edit = unreal.MaterialEditingLibrary
    expressions = list(edit.get_material_expressions(asset))
    if len(expressions) != 10:
        raise RuntimeError(f"Building {model} material has {len(expressions)} expressions, expected 10")
    def only(cls, count):
        result = [n for n in expressions if isinstance(n, cls)]
        if len(result) != count:
            raise RuntimeError(f"Building {model} material {cls.__name__} count differs")
        return result
    samples = {str(n.get_editor_property("parameter_name")): n
               for n in only(unreal.MaterialExpressionTextureSampleParameter2D, 2)}
    vectors = {str(n.get_editor_property("parameter_name")): n
               for n in only(unreal.MaterialExpressionVectorParameter, 4)}
    scalar = only(unreal.MaterialExpressionScalarParameter, 1)[0]
    custom = only(unreal.MaterialExpressionCustom, 1)[0]
    masks = only(unreal.MaterialExpressionComponentMask, 2)
    if set(samples) != {"Sprite", "Mask"} or set(vectors) != set(expected_colors(catalog, record)):
        raise RuntimeError(f"Building {model} material parameters differ")
    for role, texture in (("Sprite", sprite), ("Mask", mask)):
        if samples[role].get_editor_property("texture") != texture or \
           samples[role].get_editor_property("sampler_type") != unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR:
            raise RuntimeError(f"Building {model} {role} texture sample differs")
    for name, expected in expected_colors(catalog, record).items():
        actual = vectors[name].get_editor_property("default_value")
        if any(abs(getattr(actual, key) - expected[key]) > 1e-5 for key in "rgba"):
            raise RuntimeError(f"Building {model} color differs: {name}")
    if str(scalar.get_editor_property("parameter_name")) != "CityTransparency" or \
       abs(scalar.get_editor_property("default_value") - catalog["material"]["city_transparency"]) > 1e-5:
        raise RuntimeError(f"Building {model} transparency differs")
    names = [str(n.get_editor_property("input_name")) for n in custom.get_editor_property("inputs")]
    if (custom.get_editor_property("code") != CODE or
            custom.get_editor_property("output_type") != unreal.CustomMaterialOutputType.CMOT_FLOAT4 or
            names != ["MainSample", "MaskSample", "Team1", "Team2", "Renderer", "Lighting", "Transparency"] or
            list(edit.get_inputs_for_material_expression(asset, custom)) !=
            [samples["Sprite"], samples["Mask"], vectors["Color1"], vectors["Color2"],
             vectors["RendererColor"], vectors["SpriteColor"], scalar]):
        raise RuntimeError(f"Building {model} R/G shader code/inputs differ")
    emissive = edit.get_material_property_input_node(asset, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = edit.get_material_property_input_node(asset, unreal.MaterialProperty.MP_OPACITY)
    if (emissive not in masks or opacity not in masks or emissive == opacity or
            not all(bool(emissive.get_editor_property(k)) for k in "rgb") or
            bool(emissive.get_editor_property("a")) or
            any(bool(opacity.get_editor_property(k)) for k in "rgb") or
            not bool(opacity.get_editor_property("a")) or
            any(list(edit.get_inputs_for_material_expression(asset, node)) != [custom]
                for node in (emissive, opacity))):
        raise RuntimeError(f"Building {model} material output topology differs")
    return {"expression_count": 10, "shader_sha256": hashlib.sha256(CODE.encode("utf-8")).hexdigest()}


def create_material(path, model, catalog, record, sprite, mask):
    existing = unreal.load_asset(path)
    if existing is not None:
        verify_material(existing, model, catalog, record, sprite, mask)
        return existing, False
    edit = unreal.MaterialEditingLibrary
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        path.rsplit("/", 1)[1], path.rsplit("/", 1)[0], unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(asset, unreal.Material):
        raise RuntimeError(f"Building {model} material creation failed")
    for key, value in (("material_domain", unreal.MaterialDomain.MD_SURFACE),
                       ("shading_model", unreal.MaterialShadingModel.MSM_UNLIT),
                       ("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT), ("two_sided", True)):
        asset.set_editor_property(key, value)
    def node(cls):
        return edit.create_material_expression(asset, cls)
    def connect(a, output, b, input_name):
        if not edit.connect_material_expressions(a, output, b, input_name):
            raise RuntimeError(f"Building {model} material connection failed: {input_name}")
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
    custom.set_editor_property("description", "Unity city building source R/G Gamma arithmetic then sRGB decode")
    inputs = []
    for name in ("MainSample", "MaskSample", "Team1", "Team2", "Renderer", "Lighting", "Transparency"):
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
        raise RuntimeError(f"Building {model} material output connection failed")
    set_meta(asset, meta_material(model, record, catalog))
    errors = [str(e) for e in edit.recompile_material(asset)]
    if errors:
        raise RuntimeError(f"Building {model} material compile failed: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"Building {model} material save failed")
    verify_material(asset, model, catalog, record, sprite, mask)
    return asset, True


def verify_mesh(asset, model, manifest, obj_path, material, export_faces=False):
    if not isinstance(asset, unreal.StaticMesh):
        raise RuntimeError(f"Building {model} mesh is not StaticMesh")
    source_count = manifest["source_triangle_count"]
    zero_count = len(manifest["source_zero_area_triangle_indices"])
    source_used, source_unique = resource._nonzero_source_vertex_counts(
        obj_path, set(manifest["source_zero_area_triangle_indices"]))
    actual = {"vertex_count_lod0": int(asset.get_num_vertices(0)),
              "triangle_count_lod0": int(asset.get_num_triangles(0)),
              "uv_channel_count_lod0": int(asset.get_num_tex_coords(0))}
    build = resource.static_mesh_editor().get_lod_build_settings(asset, 0)
    if not build.get_editor_property("use_full_precision_u_vs") or build.get_editor_property("remove_degenerates"):
        raise RuntimeError(f"Building {model} mesh UV precision/degenerate import flags differ")
    if (not source_unique <= actual["vertex_count_lod0"] <= source_used or
            actual["uv_channel_count_lod0"] != 1 or
            not source_count - zero_count <= actual["triangle_count_lod0"] <= source_count):
        raise RuntimeError(f"Building {model} mesh counts differ: {actual}")
    actual["source_used_vertex_indices_nonzero_faces"] = source_used
    actual["source_unique_position_uv_nonzero_faces"] = source_unique
    if asset.get_editor_property("static_materials")[0].get_editor_property("material_interface") != material:
        raise RuntimeError(f"Building {model} mesh material slot differs")
    bounds = asset.get_bounds()
    lo = [bounds.origin.x - bounds.box_extent.x, bounds.origin.y - bounds.box_extent.y,
          bounds.origin.z - bounds.box_extent.z]
    hi = [bounds.origin.x + bounds.box_extent.x, bounds.origin.y + bounds.box_extent.y,
          bounds.origin.z + bounds.box_extent.z]
    if any(abs(value - target) > 0.05 for measured, pinned in
           ((lo, manifest["ue_local_bounds_min_cm"]), (hi, manifest["ue_local_bounds_max_cm"]))
           for value, target in zip(measured, pinned)):
        raise RuntimeError(f"Building {model} mesh pivot/bounds differ: {lo}; {hi}")
    check_meta(asset, meta_mesh(model, manifest))
    if export_faces:
        # Editor export goes to Saved only. Never infer face equivalence from counts.
        output = resource.next_evidence_path(
            source.OUTPUT / Path(manifest["source_run"]).name / model / "ue_imported_lod0_verify.obj")
        task = unreal.AssetExportTask()
        task.set_editor_property("object", asset)
        task.set_editor_property("filename", str(output))
        task.set_editor_property("exporter", unreal.StaticMeshExporterOBJ())
        task.set_editor_property("automated", True)
        task.set_editor_property("prompt", False)
        task.set_editor_property("replace_identical", False)
        if not unreal.Exporter.run_asset_export_task(task) or not output.is_file():
            raise RuntimeError(f"Building {model} mesh face export failed; cannot prove source UV topology")
        _, _, expected, source_faces = resource._face_keys(obj_path, manifest, source_axes=True)
        _, _, observed, _ = resource._face_keys(output, manifest, source_axes=False)
        missing = expected - observed
        extra = observed - expected
        zero_allowed = collections.Counter(
            source_faces[i] for i in manifest["source_zero_area_triangle_indices"])
        if extra or missing - zero_allowed:
            raise RuntimeError(f"Building {model} imported face positions/UV differ: missing={sum(missing.values())}, extra={sum(extra.values())}")
        if sum(missing.values()) != source_count - actual["triangle_count_lod0"]:
            raise RuntimeError(f"Building {model} face export vs LOD0 count differs")
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
        raise RuntimeError(f"Building {model} source OBJ import failed")
    editor = resource.static_mesh_editor()
    build = editor.get_lod_build_settings(asset, 0)
    build.set_editor_property("use_full_precision_u_vs", True)
    build.set_editor_property("remove_degenerates", False)
    editor.set_lod_build_settings(asset, 0, build)
    if not editor.get_lod_build_settings(asset, 0).get_editor_property("use_full_precision_u_vs"):
        raise RuntimeError(f"Building {model} full-precision UV build setting was not applied")
    materials = list(asset.get_editor_property("static_materials"))
    if len(materials) != 1:
        raise RuntimeError(f"Building {model} imported mesh material slots differ: {len(materials)}")
    materials[0].set_editor_property("material_interface", material)
    asset.set_editor_property("static_materials", materials)
    set_meta(asset, meta_mesh(model, manifest))
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"Building {model} source mesh save failed")
    return asset, True


def preflight():
    resource.identity()
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
                raise RuntimeError(f"Building {model} existing material lacks pinned textures")
            verify_material(existing["material"], model, catalog, record, existing["sprite"], existing["mask"])
        mesh_proof = None
        if existing["mesh"] is not None:
            if existing["material"] is None:
                raise RuntimeError(f"Building {model} existing mesh lacks pinned material")
            mesh_proof = verify_mesh(existing["mesh"], model, manifest, obj_path,
                                     existing["material"], export_faces=True)
        plan.append({"model": model, "assets": paths,
                     "create_roles": [role for role, value in existing.items() if value is None],
                     "source_zero_area_faces": len(manifest["source_zero_area_triangle_indices"]),
                     "existing_mesh_proof": mesh_proof})
    return catalog, models, plan


def apply():
    catalog, models, plan = preflight()  # Finish identity/source/existing checks before the first write.
    if os.environ.get(APPLY_ENV) != "1":
        raise RuntimeError(f"City building asset creation requires {APPLY_ENV}=1")
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
                        "component_relative_transform": manifest["component_relative_transform_example"],
                        "mesh_topology": topology})
    report = {"schema": "rok.city_building_presentation_assets.ue_create.v1",
              "project": str(ROOT), "world": WORLD, "native_run": str(native_run()),
              "source_catalog_sha256": sha(source.CATALOG),
              "native_verified_capture_sha256": sha(source.SUMMARY),
              "scope": "City building source texture/mask/material/tightmesh asset sets only; no actors or map save",
              "plan": plan, "models": details, "runtime_visual_acceptance": "pending",
              "map_saved": False, "resource_or_academy_assets_modified": False}
    report_path = resource.next_evidence_path(REPORT)
    report["report_path"] = str(report_path)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    with report_path.open("x", encoding="utf-8") as output:
        output.write(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    return report


def main():
    if os.environ.get(APPLY_ENV) == "1":
        report = apply()
        unreal.log("ROK_CITY_BUILDING_ASSETS_CREATED " + report["report_path"] + " runtime visual pending")
        return report
    _, _, plan = preflight()
    unreal.log("ROK_CITY_BUILDING_ASSETS_READONLY_PREFLIGHT_PASS " + json.dumps(plan))
    return plan


if __name__ == "__main__":
    main()
