import hashlib
import json
import os

import unreal

import CreateRokUnityFreshCityScene as fresh


PROJECT_ROOT = unreal.Paths.project_dir()
MIGRATION_ROOT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration")
UNITY_RUNTIME_CITY_SUPPORT_MANIFEST = os.path.join(
    MIGRATION_ROOT,
    "unity_runtime_city_support_layers.json",
)

CITY_TAG = "RokUnityRuntimeCitySupportLayers"
FOLDER_ROOT = "RokUnityRuntimeCitySupportLayers"
ACTOR_PREFIX = "UnityRuntimeCitySupport"
SUPPORT_OBJ_MESH_DESTINATION = "/Game/RokUnityRuntimeCitySupportLayers/RuntimeObjMeshes"
SUPPORT_TEXTURE_DESTINATION = "/Game/RokUnityRuntimeCitySupportLayers/Textures"
SUPPORT_MATERIAL_DESTINATION = "/Game/RokUnityRuntimeCitySupportLayers/Materials"
EXPECTED_SCHEMA = "rok.unity_runtime_city_support_layers.v2"
EXPECTED_OBJ_IMPORT_AXIS_COMPENSATION = (
    "UE 5.8 FbxFactory OBJ inverse-Y import compensation: "
    "OBJ input=(desired UE.x,-desired UE.y,desired UE.z)"
)


def file_sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def safe_name(value):
    result = []
    for char in str(value):
        result.append(char if char.isalnum() or char == "_" else "_")
    return "".join(result).strip("_") or "SupportLayer"


def package_path(folder, name):
    return "{0}/{1}.{1}".format(folder.rstrip("/"), name)


def load_asset(path):
    return unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None


def actor_tags(actor):
    return [str(tag) for tag in list(actor.tags)]


def set_tags(actor, tags):
    current = actor_tags(actor)
    for tag in tags:
        if tag not in current:
            current.append(tag)
    actor.tags = current


def load_support_manifest():
    if not os.path.exists(UNITY_RUNTIME_CITY_SUPPORT_MANIFEST):
        raise RuntimeError(
            "Missing Unity runtime city support-layer manifest; run "
            "Scripts/BuildRokUnityRuntimeCitySupportLayers.py first: {0}".format(
                UNITY_RUNTIME_CITY_SUPPORT_MANIFEST
            )
        )
    with open(UNITY_RUNTIME_CITY_SUPPORT_MANIFEST, "r", encoding="utf-8-sig") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != EXPECTED_SCHEMA:
        raise RuntimeError("Unexpected Unity city support-layer schema: {0}".format(manifest.get("schema")))
    if manifest.get("failures"):
        raise RuntimeError("Unity city support-layer manifest has failures: {0}".format(manifest.get("failures")))
    records = manifest.get("records", [])
    role_counts = manifest.get("role_counts", {})
    if len(records) != 2 or role_counts != {"road": 1, "authored_shadow": 1}:
        raise RuntimeError(
            "Unity city support-layer manifest must contain exactly one road and one authored shadow: "
            "records={0}, roles={1}".format(len(records), role_counts)
        )
    layer_ids = [record.get("layer_id") for record in records]
    if len(set(layer_ids)) != len(layer_ids):
        raise RuntimeError("Unity city support-layer manifest contains duplicate layer IDs")
    for record in records:
        validate_record_files(record)
        if record.get("source_mode") != "unity_active_renderer" or record.get("manual_layout"):
            raise RuntimeError("Support layer is not an active source Renderer: {0}".format(record.get("layer_id")))
        if not record.get("active_in_hierarchy") or not record.get("renderer_enabled"):
            raise RuntimeError("Support layer is inactive or disabled: {0}".format(record.get("layer_id")))
        if int(record.get("mesh_file_id") or 0) != 10209:
            raise RuntimeError("Support layer must use Unity built-in Plane 10209: {0}".format(record.get("layer_id")))
        if record.get("obj_import_axis_compensation") != EXPECTED_OBJ_IMPORT_AXIS_COMPENSATION:
            raise RuntimeError("Support layer is missing UE OBJ inverse-Y compensation: {0}".format(
                record.get("layer_id")
            ))
        if int(record.get("renderer_sorting_layer_id") or 0) != 0 or int(record.get("renderer_sorting_order") or 0) != 0:
            raise RuntimeError("Non-zero Unity sorting layer/order is not supported yet: {0}".format(record.get("layer_id")))
    return manifest


def validate_record_files(record):
    checks = (
        (record.get("obj_path"), record.get("obj_sha256"), "OBJ"),
        (record.get("source_prefab_path"), record.get("source_prefab_sha256"), "prefab"),
        (record.get("material_path"), record.get("material_sha256"), "material"),
        (record.get("source_texture_path"), record.get("source_texture_sha256"), "texture"),
    )
    for path, expected_hash, label in checks:
        if not path or not os.path.exists(path):
            raise RuntimeError("Missing Unity support-layer {0}: {1}".format(label, path))
        if not expected_hash or file_sha256(path) != expected_hash:
            raise RuntimeError("Unity support-layer {0} hash drift: {1}".format(label, path))


def import_support_obj(record):
    source_path = record["obj_path"]
    asset_name = safe_name(os.path.splitext(os.path.basename(source_path))[0])
    asset_path = package_path(SUPPORT_OBJ_MESH_DESTINATION, asset_name)
    # Existing mesh assets are reused by default. Forced synchronous OBJ
    # reimport belongs in the standalone commandlet path; doing it from a live
    # MCP callback can re-enter UE's import task graph.
    force_reimport = os.environ.get(
        "ROK_FORCE_REIMPORT_SUPPORT_MESHES",
        os.environ.get("ROK_FORCE_REIMPORT", "0"),
    ) != "0"
    existing = load_asset(asset_path)
    if isinstance(existing, unreal.StaticMesh) and not force_reimport:
        return existing

    unreal.EditorAssetLibrary.make_directory(SUPPORT_OBJ_MESH_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", SUPPORT_OBJ_MESH_DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    # AssetTools without an explicit factory routes OBJ through Interchange in
    # UE 5.8. Importing that way from an MCP callback can re-enter the task graph
    # while Interchange waits for completion. Force the legacy FBX/OBJ factory,
    # matching the project's stable static-mesh import path.
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("automated_import_should_detect_type", False)
    static_data = options.get_editor_property("static_mesh_import_data")
    static_data.set_editor_property("combine_meshes", True)
    static_data.set_editor_property("generate_lightmap_u_vs", False)
    static_data.set_editor_property("auto_generate_collision", False)
    task.set_editor_property("factory", unreal.FbxFactory())
    task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    imported = load_asset(asset_path)
    if isinstance(imported, unreal.StaticMesh):
        return imported
    for imported_path in task.get_editor_property("imported_object_paths"):
        candidate = load_asset(imported_path)
        if isinstance(candidate, unreal.StaticMesh):
            return candidate
    raise RuntimeError("Failed to import Unity support-layer OBJ: {0}".format(source_path))


def import_support_texture(record):
    source_path = record["source_texture_path"]
    asset_name = "T_RokUnitySupport_{0}_{1}".format(
        safe_name(record["role"]),
        record["source_texture_sha256"][:10],
    )
    asset_path = package_path(SUPPORT_TEXTURE_DESTINATION, asset_name)
    force_reimport = os.environ.get("ROK_FORCE_REIMPORT_TEXTURES", "0") != "0"
    existing = load_asset(asset_path)
    if existing and not force_reimport:
        return existing

    unreal.EditorAssetLibrary.make_directory(SUPPORT_TEXTURE_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", SUPPORT_TEXTURE_DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("factory", unreal.TextureFactory())
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", False)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = load_asset(asset_path)
    if not texture:
        for imported_path in task.get_editor_property("imported_object_paths"):
            texture = load_asset(imported_path)
            if texture:
                break
    if not texture:
        raise RuntimeError("Failed to import Unity support-layer texture: {0}".format(source_path))
    try:
        texture.set_editor_property("srgb", True)
        texture.set_editor_property("never_stream", True)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    except Exception:
        pass
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def clear_material_expressions(material):
    for expression in list(unreal.MaterialEditingLibrary.get_material_expressions(material)):
        unreal.MaterialEditingLibrary.delete_material_expression(material, expression)
    remaining = unreal.MaterialEditingLibrary.get_material_expressions(material)
    if remaining:
        raise RuntimeError("Failed to clear support material expressions: {0}".format(material.get_path_name()))


def create_support_material(record, texture):
    material_name = "M_RokUnitySupport_{0}_RQ{1}_{2}".format(
        safe_name(record["role"]),
        int(record["material_render_queue"]),
        record["source_texture_sha256"][:10],
    )
    asset_path = package_path(SUPPORT_MATERIAL_DESTINATION, material_name)
    material = load_asset(asset_path)
    if not material:
        unreal.EditorAssetLibrary.make_directory(SUPPORT_MATERIAL_DESTINATION)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            material_name,
            SUPPORT_MATERIAL_DESTINATION,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    clear_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property("disable_depth_test", False)
    except Exception:
        pass

    texture_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSample,
        -420,
        0,
    )
    texture_node.set_editor_property("texture", texture)
    try:
        texture_node.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    except Exception:
        pass
    rgb_multiplier = record.get("emissive_rgb_multiplier", [1.0, 1.0, 1.0])
    if len(rgb_multiplier) != 3:
        raise RuntimeError("Invalid support-layer RGB multiplier: {0}".format(rgb_multiplier))
    multiplier_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant3Vector,
        -420,
        180,
    )
    multiplier_node.set_editor_property(
        "constant",
        unreal.LinearColor(
            float(rgb_multiplier[0]),
            float(rgb_multiplier[1]),
            float(rgb_multiplier[2]),
            1.0,
        ),
    )
    multiply_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionMultiply,
        -120,
        0,
    )
    for from_expression, from_output, to_expression, to_input in (
        (texture_node, "RGB", multiply_node, "A"),
        (multiplier_node, "", multiply_node, "B"),
    ):
        if not unreal.MaterialEditingLibrary.connect_material_expressions(
            from_expression,
            from_output,
            to_expression,
            to_input,
        ):
            raise RuntimeError("Failed to reproduce Unity support-layer RGB semantics for {0}".format(
                record["layer_id"]
            ))
    unreal.MaterialEditingLibrary.connect_material_property(
        multiply_node,
        "",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        texture_node,
        "A",
        unreal.MaterialProperty.MP_OPACITY,
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def configure_component(actor, record):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        raise RuntimeError("Support-layer actor has no StaticMeshComponent: {0}".format(actor.get_actor_label()))
    settings = (
        ("cast_shadow", False),
        ("receive_decals", False),
        ("use_as_occluder", False),
        ("collision_enabled", unreal.CollisionEnabled.NO_COLLISION),
        ("translucency_sort_priority", int(record["material_render_queue"])),
        ("translucency_sort_distance_offset", 0.0),
    )
    for prop, value in settings:
        try:
            component.set_editor_property(prop, value)
        except Exception:
            pass


def align_visible_bounds_center(actor, expected_center):
    origin, _extent = actor.get_actor_bounds(False)
    target = unreal.Vector(float(expected_center[0]), float(expected_center[1]), float(expected_center[2]))
    actor.set_actor_location(actor.get_actor_location() + (target - origin), False, False)


def actor_record(actor, source_record):
    bounds_origin, bounds_extent = actor.get_actor_bounds(False)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    return {
        "actor": actor.get_actor_label(),
        "layer_id": source_record["layer_id"],
        "role": source_record["role"],
        "source_mode": source_record["source_mode"],
        "manual_layout": False,
        "unity_object": source_record["object_name"],
        "unity_object_path": source_record["object_path"],
        "source_prefab": source_record["source_prefab_path"],
        "source_prefab_sha256": source_record["source_prefab_sha256"],
        "source_material": source_record["material_path"],
        "source_material_guid": source_record["material_guid"],
        "source_material_sha256": source_record["material_sha256"],
        "source_texture": source_record["source_texture_path"],
        "source_texture_guid": source_record["texture_guid"],
        "source_texture_sha256": source_record["source_texture_sha256"],
        "source_alpha_policy": source_record["source_alpha_policy"],
        "source_rgb_policy": source_record["source_rgb_policy"],
        "emissive_rgb_multiplier": source_record["emissive_rgb_multiplier"],
        "obj_path": source_record["obj_path"],
        "obj_sha256": source_record["obj_sha256"],
        "obj_import_axis_compensation": source_record["obj_import_axis_compensation"],
        "unity_material_render_queue": source_record["material_render_queue"],
        "ue_expected_bounds_center": source_record["ue_bounds_center"],
        "ue_expected_bounds_extents": source_record["ue_bounds_extents"],
        "ue_alignment_mode": "Unity world-baked Plane OBJ; actor rotation/scale identity; translation only corrects OBJ importer bounds-center drift",
        "ue_location": [
            float(actor.get_actor_location().x),
            float(actor.get_actor_location().y),
            float(actor.get_actor_location().z),
        ],
        "ue_rotation": [
            float(actor.get_actor_rotation().pitch),
            float(actor.get_actor_rotation().yaw),
            float(actor.get_actor_rotation().roll),
        ],
        "ue_scale": [
            float(actor.get_actor_scale3d().x),
            float(actor.get_actor_scale3d().y),
            float(actor.get_actor_scale3d().z),
        ],
        "ue_visible_bounds_origin": [float(bounds_origin.x), float(bounds_origin.y), float(bounds_origin.z)],
        "ue_visible_bounds_extent": [float(bounds_extent.x), float(bounds_extent.y), float(bounds_extent.z)],
        "ue_translucency_sort_priority": int(component.get_editor_property("translucency_sort_priority")),
        "ue_translucency_sort_distance_offset": float(component.get_editor_property("translucency_sort_distance_offset")),
    }


def spawn_support_layers(manifest=None):
    manifest = manifest or load_support_manifest()
    spawned_records = []
    for index, record in enumerate(manifest.get("records", [])):
        mesh = import_support_obj(record)
        texture = import_support_texture(record)
        material = create_support_material(record, texture)
        actor = fresh.spawn_static_mesh_actor(
            "{0}_{1}_{2}".format(ACTOR_PREFIX, safe_name(record["role"]), index),
            mesh,
            (0.0, 0.0, 0.0),
            (1.0, 1.0, 1.0),
            material,
            (0.0, 0.0, 0.0),
            "{0}/SupportLayers".format(FOLDER_ROOT),
        )
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        configure_component(actor, record)
        align_visible_bounds_center(actor, record["ue_bounds_center"])
        actor.tags = []
        set_tags(actor, [
            CITY_TAG,
            "RokUnityRuntimeCitySupportLayer",
            "RokUnityRuntimeObjBaked",
            "UnityLayerId_{0}".format(safe_name(record["layer_id"])),
            "UnitySupportRole_{0}".format(safe_name(record["role"])),
            "UnitySourceMode_unity_active_renderer",
            "UnityRendererActiveInHierarchy",
            "UnityRendererEnabled",
            "UnityMesh_BuiltinPlane10209",
            "UnityMaterialGuid_{0}".format(record["material_guid"]),
            "UnityTextureGuid_{0}".format(record["texture_guid"]),
            "UnityRenderQueue_{0}".format(record["material_render_queue"]),
            "UnityAuthoredRGBA",
            "UnitySourceRgbSemanticsReproduced",
            "UnityNoWhiteToAlpha",
            "UnityNoGeneratedBlur",
            "UnityManualLayout_False",
            "UnityBakedOrientationScaleIdentity",
            "UnityRendererBoundsCenterAligned",
        ])
        spawned_records.append(actor_record(actor, record))
    return len(spawned_records), spawned_records
