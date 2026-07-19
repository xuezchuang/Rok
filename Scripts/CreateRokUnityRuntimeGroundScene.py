import hashlib
import json
import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokUnityFreshCityScene as fresh
import CreateRokUnityRuntimeBoundaryOnlyScene as boundary


TARGET_LEVEL = os.environ.get("ROK_UNITY_RUNTIME_GROUND_LEVEL", "/Game/Maps/L_RokUnityRuntimeGround")
CITY_TAG = "RokUnityRuntimeGround"
FOLDER_ROOT = "RokUnityRuntimeGround"
ACTOR_PREFIX = "UnityRuntimeGround"
MIGRATION_ROOT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration")
UNITY_RUNTIME_GROUND_OBJ_MANIFEST = os.path.join(
    MIGRATION_ROOT, "unity_runtime_ground_obj_compensated_manifest.json"
)
UNITY_RUNTIME_GROUND_OBJ_SCHEMA = "rok.unity.runtime_ground_obj_compensated.v3"
UNITY_RUNTIME_GROUND_VERTEX_ALPHA_MANIFEST = os.path.join(
    MIGRATION_ROOT, "unity_runtime_ground_vertex_alpha.json"
)
UNITY_RUNTIME_GROUND_VERTEX_ALPHA_SCHEMA = "rok.unity.runtime_ground_vertex_alpha.v1"
UNITY_RUNTIME_GROUND_VERTEX_ALPHA_SHADER = "custom/map/cm_mask_texture_lod_alpha"
SCENE_MANIFEST = os.path.join(MIGRATION_ROOT, "unity_runtime_ground_scene_manifest.json")
GROUND_OBJ_MESH_DESTINATION = "/Game/RokUnityRuntimeGround/RuntimeObjMeshes"
GROUND_TEXTURE_DESTINATION = "/Game/RokUnityRuntimeGround/Textures"
GROUND_MATERIAL_DESTINATION = "/Game/RokUnityRuntimeGround/Materials"


def actor_tags(actor):
    return [str(tag) for tag in list(actor.tags)]


def set_tags(actor, tags):
    existing = actor_tags(actor)
    for tag in tags:
        if tag not in existing:
            existing.append(tag)
    actor.tags = existing


def load_ground_obj_manifest():
    if not os.path.exists(UNITY_RUNTIME_GROUND_OBJ_MANIFEST):
        raise RuntimeError("Missing Unity runtime ground OBJ manifest: {0}".format(UNITY_RUNTIME_GROUND_OBJ_MANIFEST))
    with open(UNITY_RUNTIME_GROUND_OBJ_MANIFEST, "r", encoding="utf-8-sig") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != UNITY_RUNTIME_GROUND_OBJ_SCHEMA:
        raise RuntimeError(
            "Unexpected Unity runtime ground OBJ schema: {0}; regenerate it with "
            "Scripts/BuildRokUnityRuntimeGroundObjImportCompensation.py".format(manifest.get("schema"))
        )
    if manifest.get("ue_actor_transform_mode") != "world_baked_identity":
        raise RuntimeError("Unity runtime ground OBJ manifest must require identity UE actors")
    if "inverse Y reflection" not in str(manifest.get("obj_import_axis_compensation", "")):
        raise RuntimeError("Unity runtime ground OBJ manifest is missing UE FbxFactory inverse-Y compensation")
    if "preserve exported faces" not in str(manifest.get("obj_face_winding_contract", "")):
        raise RuntimeError("Unity runtime ground OBJ manifest is missing face-winding compensation evidence")
    if len(manifest.get("records", [])) != 2:
        raise RuntimeError("Expected exactly two Unity runtime ground OBJ records")
    for record in manifest.get("records", []):
        triangle_count = int(record.get("triangle_count", 0))
        if int(record.get("predicted_ue_up_facing_triangle_count", -1)) != triangle_count:
            raise RuntimeError("Unity runtime ground OBJ must import entirely UE +Z-facing: {0}".format(
                record.get("ground_id", "")
            ))
        if int(record.get("predicted_ue_down_facing_triangle_count", -1)) != 0:
            raise RuntimeError("Unity runtime ground OBJ contains UE -Z-facing triangles: {0}".format(
                record.get("ground_id", "")
            ))
    return manifest


def load_ground_vertex_alpha_manifest():
    if not os.path.exists(UNITY_RUNTIME_GROUND_VERTEX_ALPHA_MANIFEST):
        raise RuntimeError(
            "Missing Unity runtime ground vertex-alpha manifest; run "
            "Scripts/PrepareRokUnityRuntimeGroundVertexAlpha.py first: {0}".format(
                UNITY_RUNTIME_GROUND_VERTEX_ALPHA_MANIFEST
            )
        )
    with open(UNITY_RUNTIME_GROUND_VERTEX_ALPHA_MANIFEST, "r", encoding="utf-8-sig") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != UNITY_RUNTIME_GROUND_VERTEX_ALPHA_SCHEMA:
        raise RuntimeError(
            "Unexpected Unity runtime ground vertex-alpha schema: {0}".format(manifest.get("schema"))
        )
    return manifest


def file_sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def same_disk_path(left, right):
    return os.path.normcase(os.path.abspath(left)) == os.path.normcase(os.path.abspath(right))


def vertex_alpha_evidence_for_record(record, manifest):
    if record.get("shader_name") != UNITY_RUNTIME_GROUND_VERTEX_ALPHA_SHADER:
        return None
    matches = [
        item for item in manifest.get("records", [])
        if item.get("mesh_stem") == record.get("mesh_stem")
    ]
    if len(matches) != 1:
        raise RuntimeError(
            "Expected one vertex-alpha evidence record for {0}, found {1}".format(
                record.get("mesh_stem"), len(matches)
            )
        )
    evidence = matches[0]
    output_path = evidence.get("output_texture_path", "")
    source_fbx = unity_asset_path_to_disk(record.get("mesh_asset_path", ""))
    source_obj = record.get("obj_path", "")
    source_baked = unity_asset_path_to_disk(record.get("baked_texture_path", ""))
    if not output_path or not os.path.exists(output_path):
        raise RuntimeError("Missing generated ground vertex-alpha texture: {0}".format(output_path))
    for actual, expected, label in [
        (source_obj, evidence.get("source_obj_path", ""), "OBJ"),
        (source_baked, evidence.get("source_baked_texture_path", ""), "baked texture"),
    ]:
        if not same_disk_path(actual, expected):
            raise RuntimeError(
                "Ground vertex-alpha {0} source path drift: expected {1}, got {2}".format(
                    label, expected, actual
                )
            )
    # Offline evidence is derived from the normalized in-repo ref snapshot while
    # the editor resolves the live Unity project. Hash equality below is the
    # compatibility boundary between those two approved source roots.
    for path, expected_hash, label in [
        (source_fbx, evidence.get("source_fbx_sha256"), "FBX"),
        (source_obj, evidence.get("source_obj_sha256"), "OBJ"),
        (source_baked, evidence.get("source_baked_texture_sha256"), "baked texture"),
        (output_path, evidence.get("output_png_sha256"), "output texture"),
    ]:
        if not expected_hash or file_sha256(path) != expected_hash:
            raise RuntimeError("Ground vertex-alpha {0} hash drift: {1}".format(label, path))
    if evidence.get("fbx_color_mapping") != "ByPolygonVertex":
        raise RuntimeError("Ground vertex-alpha FBX color mapping must be ByPolygonVertex")
    if evidence.get("fbx_color_reference") != "IndexToDirect":
        raise RuntimeError("Ground vertex-alpha FBX color reference must be IndexToDirect")
    if evidence.get("output_alpha_extrema") != [0, 255]:
        raise RuntimeError("Ground vertex-alpha output must preserve the Unity 0..1 alpha range")
    return evidence


def unity_asset_path_to_disk(asset_path):
    if not asset_path:
        return ""
    if os.path.isabs(asset_path):
        return asset_path
    return os.path.join(fresh.ROK_UNITY_CLIENT_ROOT, asset_path.replace("/", os.sep))


def import_runtime_ground_obj_mesh(record):
    obj_path = record.get("obj_path")
    if not obj_path or not os.path.exists(obj_path):
        raise RuntimeError("Missing Unity runtime ground OBJ: {0}".format(obj_path))

    asset_name = fresh.safe_tag(os.path.splitext(os.path.basename(obj_path))[0])
    package_path = fresh.proto.asset_package_path(GROUND_OBJ_MESH_DESTINATION, asset_name)
    existing = fresh.proto.load_asset_if_exists(package_path)
    force_reimport = (
        os.environ.get("ROK_FORCE_REIMPORT_GROUND", "0") == "1"
        or os.environ.get("ROK_FORCE_REIMPORT", "0") == "1"
    )
    if isinstance(existing, unreal.StaticMesh) and not force_reimport:
        return existing

    unreal.EditorAssetLibrary.make_directory(GROUND_OBJ_MESH_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", obj_path)
    task.set_editor_property("destination_path", GROUND_OBJ_MESH_DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
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

    imported = fresh.proto.load_asset_if_exists(package_path)
    if isinstance(imported, unreal.StaticMesh):
        return imported
    for imported_path in task.get_editor_property("imported_object_paths"):
        candidate = fresh.proto.load_asset_if_exists(imported_path)
        if isinstance(candidate, unreal.StaticMesh):
            return candidate
    raise RuntimeError("Failed to import Unity runtime ground OBJ: {0}".format(obj_path))


def import_ground_texture(source_path, asset_name, force_reimport=False):
    if not source_path or not os.path.exists(source_path):
        raise RuntimeError("Missing Unity runtime ground texture: {0}".format(source_path))

    safe_name = fresh.proto.safe_asset_name(asset_name)
    package_path = fresh.proto.asset_package_path(GROUND_TEXTURE_DESTINATION, safe_name)
    existing = fresh.proto.load_asset_if_exists(package_path)
    force_reimport = force_reimport or os.environ.get("ROK_FORCE_REIMPORT_TEXTURES", "0") == "1"
    if existing and not force_reimport:
        return existing

    unreal.EditorAssetLibrary.make_directory(GROUND_TEXTURE_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", GROUND_TEXTURE_DESTINATION)
    task.set_editor_property("destination_name", safe_name)
    task.set_editor_property("factory", unreal.TextureFactory())
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", False)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = fresh.proto.load_asset_if_exists(package_path)
    if not texture:
        for imported_path in task.get_editor_property("imported_object_paths"):
            texture = fresh.proto.load_asset_if_exists(imported_path)
            if texture:
                break
    if not texture:
        raise RuntimeError("Failed to import Unity runtime ground texture: {0}".format(source_path))

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


def create_ground_material(texture, material_name, blend_mode):
    material_path = fresh.proto.asset_package_path(GROUND_MATERIAL_DESTINATION, material_name)
    existing = fresh.proto.load_asset_if_exists(material_path)
    update_existing = os.environ.get("ROK_UPDATE_EXISTING_MATERIALS", "0") == "1"
    if existing and not update_existing:
        return existing
    if existing:
        material = existing
        for expression in list(unreal.MaterialEditingLibrary.get_material_expressions(material)):
            unreal.MaterialEditingLibrary.delete_material_expression(material, expression)
        if unreal.MaterialEditingLibrary.get_material_expressions(material):
            raise RuntimeError("Failed to clear existing runtime ground material expressions: {0}".format(
                material_path
            ))
    else:
        unreal.EditorAssetLibrary.make_directory(GROUND_MATERIAL_DESTINATION)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            material_name,
            GROUND_MATERIAL_DESTINATION,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    material.set_editor_property("blend_mode", blend_mode)
    # Unity ground shaders omit Cull, so Unity uses the default back-face culling.
    # Correct OBJ winding is required; a two-sided material would only hide bad topology.
    material.set_editor_property("two_sided", False)
    if blend_mode == unreal.BlendMode.BLEND_TRANSLUCENT:
        try:
            material.set_editor_property("disable_depth_test", False)
        except Exception:
            pass
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    texture_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -480, 0
    )
    texture_node.set_editor_property("texture", texture)
    unreal.MaterialEditingLibrary.connect_material_property(texture_node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(texture_node, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if blend_mode == unreal.BlendMode.BLEND_TRANSLUCENT:
        unreal.MaterialEditingLibrary.connect_material_property(texture_node, "A", unreal.MaterialProperty.MP_OPACITY)
    elif blend_mode == unreal.BlendMode.BLEND_MASKED:
        material.set_editor_property("opacity_mask_clip_value", 0.08)
        unreal.MaterialEditingLibrary.connect_material_property(texture_node, "A", unreal.MaterialProperty.MP_OPACITY_MASK)

    roughness = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -480, 220)
    roughness.set_editor_property("r", 0.94)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def choose_ground_texture_path(record, vertex_alpha_evidence=None):
    if record.get("shader_name") == UNITY_RUNTIME_GROUND_VERTEX_ALPHA_SHADER:
        if not vertex_alpha_evidence:
            raise RuntimeError(
                "Unity ground shader {0} requires FBX vertex-alpha evidence".format(
                    UNITY_RUNTIME_GROUND_VERTEX_ALPHA_SHADER
                )
            )
        return vertex_alpha_evidence["output_texture_path"], "vertex_alpha_baked_texture_path"
    for key in ("baked_texture_path", "visual_texture_path", "main_texture_path"):
        source_path = unity_asset_path_to_disk(record.get(key, ""))
        if source_path and os.path.exists(source_path):
            return source_path, key

    texture_properties = record.get("texture_properties") or {}
    for key in ("_Tex3", "_Tex0", "_MainTex", "_Splat"):
        source_path = unity_asset_path_to_disk(texture_properties.get(key, ""))
        if source_path and os.path.exists(source_path):
            return source_path, "texture_properties.{0}".format(key)

    raise RuntimeError("No usable ground texture source for {0}".format(record.get("object_name", "")))


def material_blend_mode(record):
    shader_name = record.get("shader_name", "")
    material_name = record.get("material_name", "")
    render_queue = int(record.get("material_render_queue") or 0)
    zwrite = str(record.get("shader_zwrite", ""))
    if zwrite.lower() == "off" or render_queue >= 2500:
        return unreal.BlendMode.BLEND_TRANSLUCENT
    if "alpha" in shader_name.lower() or "translucent" in material_name.lower():
        return unreal.BlendMode.BLEND_TRANSLUCENT
    return unreal.BlendMode.BLEND_OPAQUE


def configure_static_mesh_component(actor, sort_priority):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        return
    for prop, value in [
        ("cast_shadow", False),
        ("receive_decals", False),
        ("use_as_occluder", False),
        ("collision_enabled", unreal.CollisionEnabled.NO_COLLISION),
        ("translucency_sort_priority", sort_priority),
    ]:
        try:
            component.set_editor_property(prop, value)
        except Exception:
            pass


def actor_record_from_ground(actor, record, texture_source, texture_source_kind, vertex_alpha_evidence=None):
    bounds_origin, bounds_extent = actor.get_actor_bounds(False)
    return {
        "actor": actor.get_actor_label(),
        "unity_object": record.get("object_name"),
        "unity_object_path": record.get("object_path"),
        "source_mesh": unity_asset_path_to_disk(record.get("mesh_asset_path", "")),
        "source_mesh_stem": record.get("mesh_stem"),
        "unity_material_name": record.get("material_name"),
        "unity_material_path": unity_asset_path_to_disk(record.get("material_asset_path", "")),
        "unity_material_guid": record.get("material_guid"),
        "unity_shader_name": record.get("shader_name"),
        "unity_shader_asset_path": unity_asset_path_to_disk(record.get("shader_asset_path", "")),
        "unity_shader_queue_tag": record.get("shader_queue_tag"),
        "unity_shader_zwrite": record.get("shader_zwrite"),
        "unity_material_render_queue": record.get("material_render_queue"),
        "unity_texture_properties": record.get("texture_properties"),
        "unity_baked_texture": record.get("baked_texture_path"),
        "ue_texture_source": texture_source,
        "ue_texture_source_kind": texture_source_kind,
        "unity_vertex_alpha_evidence": vertex_alpha_evidence or {},
        "obj_path": record.get("obj_path"),
        "ground_id": record.get("ground_id"),
        "source_fbx_sha256": record.get("source_fbx_sha256"),
        "source_obj_sha256": record.get("source_obj_sha256"),
        "vertex_count": record.get("vertex_count"),
        "triangle_count": record.get("triangle_count"),
        "obj_face_winding_compensation": record.get("obj_face_winding_compensation"),
        "predicted_ue_up_facing_triangle_count": record.get("predicted_ue_up_facing_triangle_count"),
        "predicted_ue_down_facing_triangle_count": record.get("predicted_ue_down_facing_triangle_count"),
        "predicted_ue_degenerate_triangle_count": record.get("predicted_ue_degenerate_triangle_count"),
        "ue_expected_bounds_center": record.get("ue_world_vertex_bounds_center"),
        "ue_expected_bounds_extents": record.get("ue_world_vertex_bounds_extents"),
        "ue_renderer_bounds_center_evidence": record.get("ue_renderer_bounds_center_evidence"),
        "ue_renderer_bounds_extents_evidence": record.get("ue_renderer_bounds_extents_evidence"),
        "ue_axis_probe_vertex": record.get("ue_axis_probe_vertex"),
        "obj_input_axis_probe_vertex": record.get("obj_input_axis_probe_vertex"),
        "ue_alignment_mode": "Unity runtime ground vertices are world-baked in UE centimeters with inverse-Y OBJ import compensation; actor location, rotation, and scale remain identity; Renderer.bounds is evidence only",
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
    }


def spawn_ground_from_runtime_obj_manifest(ground_manifest):
    records = []
    vertex_alpha_manifest = load_ground_vertex_alpha_manifest()
    for index, record in enumerate(ground_manifest.get("records", [])):
        mesh = import_runtime_ground_obj_mesh(record)
        vertex_alpha_evidence = vertex_alpha_evidence_for_record(record, vertex_alpha_manifest)
        texture_source, texture_source_kind = choose_ground_texture_path(record, vertex_alpha_evidence)
        texture_asset_name = "T_RokUnityRuntimeGround_{0}".format(
            fresh.safe_tag(os.path.splitext(os.path.basename(texture_source))[0])
        )
        texture = import_ground_texture(
            texture_source,
            texture_asset_name,
            force_reimport=bool(vertex_alpha_evidence),
        )
        material_name = "M_RokUnityRuntimeGround_{0}_RQ{1}_Z{2}".format(
            fresh.safe_tag(record.get("material_name", "Ground")),
            record.get("material_render_queue", "0"),
            fresh.safe_tag(record.get("shader_zwrite", "")),
        )
        if vertex_alpha_evidence:
            material_name += "_VertexAlphaV1"
        material = create_ground_material(texture, material_name, material_blend_mode(record))
        label = "{0}_{1}_{2}".format(
            ACTOR_PREFIX,
            fresh.safe_tag(record.get("object_name", "Ground")),
            index,
        )
        actor = fresh.spawn_static_mesh_actor(
            label,
            mesh,
            (0.0, 0.0, 0.0),
            (1.0, 1.0, 1.0),
            material,
            (0.0, 0.0, 0.0),
            "{0}/Ground".format(FOLDER_ROOT),
        )
        actor.tags = []
        tags = [
            CITY_TAG,
            "RokUnityRuntimeGroundSource",
            "RokUnityRuntimeObjBaked",
            "UnityWorldBakedActorIdentity",
            "UnityBakedOrientationScaleIdentity",
            "UnityObjImportAxisCompensation_FbxFactoryInverseY",
            "UnityObjImportFaceWindingCompensation_Preserved",
            "UnityShaderCull_BackDefault",
            "UnityRendererBoundsEvidenceOnly",
            "UnityGroundId_{0}".format(fresh.safe_tag(record.get("ground_id", ""))),
            "UnityGroundSourceFbxSha256_{0}".format(record.get("source_fbx_sha256", "")),
            "UnityGroundSourceObjSha256_{0}".format(record.get("source_obj_sha256", "")),
            "UnityGroundObject_{0}".format(fresh.safe_tag(record.get("object_name", ""))),
            "UnityMesh_{0}".format(record.get("mesh_stem", "")),
            "UnityMaterial_{0}".format(fresh.safe_tag(record.get("material_name", ""))),
            "UnityMaterialGuid_{0}".format(record.get("material_guid", "")),
            "UnityShader_{0}".format(fresh.safe_tag(record.get("shader_name", ""))),
            "UnityShaderQueue_{0}".format(fresh.safe_tag(record.get("shader_queue_tag", ""))),
            "UnityShaderZWrite_{0}".format(fresh.safe_tag(record.get("shader_zwrite", ""))),
            "UnityRenderQueue_{0}".format(record.get("material_render_queue", "")),
            "UnityTextureSource_{0}".format(fresh.safe_tag(texture_source_kind)),
            "UnityVisualTexture_{0}".format(fresh.safe_tag(os.path.basename(texture_source))),
        ]
        if vertex_alpha_evidence:
            tags.extend([
                "UnityGroundVertexAlphaBaked",
                "UnityGroundVertexAlphaSourceFBX_{0}".format(record.get("mesh_stem", "")),
                "UnityGroundVertexAlphaMapping_ByPolygonVertex_IndexToDirect",
                "UnityGroundVertexAlphaRange_0_255",
            ])
        set_tags(actor, tags)
        actor.set_folder_path("{0}/Ground".format(FOLDER_ROOT))
        configure_static_mesh_component(actor, int(record.get("material_render_queue") or index))
        records.append(actor_record_from_ground(
            actor,
            record,
            texture_source,
            texture_source_kind,
            vertex_alpha_evidence,
        ))
    return len(records), records


def configure_boundary_module_for_ground_map():
    boundary.CITY_TAG = CITY_TAG
    boundary.FOLDER_ROOT = FOLDER_ROOT
    boundary.ACTOR_PREFIX = ACTOR_PREFIX


def build_scene():
    fresh.TARGET_LEVEL = TARGET_LEVEL
    fresh.CITY_TAG = CITY_TAG
    fresh.require_sources()

    fresh.create_or_load_target_map()
    fresh.clear_current_level()

    configure_boundary_module_for_ground_map()
    boundary_manifest = boundary.load_obj_manifest()
    boundary_spawned, boundary_records = boundary.spawn_boundary_from_runtime_obj_manifest(boundary_manifest)

    ground_manifest = load_ground_obj_manifest()
    ground_spawned, ground_records = spawn_ground_from_runtime_obj_manifest(ground_manifest)

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError("Failed to save runtime ground map: {0}".format(TARGET_LEVEL))
    unreal.EditorAssetLibrary.save_asset(TARGET_LEVEL, only_if_is_dirty=False)

    summary = {
        "schema": "rok.unity_runtime_ground_scene.v2",
        "target_level": TARGET_LEVEL,
        "rok_unity_root": fresh.ROK_UNITY_ROOT,
        "source_files": {
            "GameAppScene": fresh.SOURCE_CHAIN["GameAppScene"],
            "SwitchMainCityCmd": fresh.SOURCE_CHAIN["SwitchMainCityCmd"],
            "CityGlobalMediator": fresh.SOURCE_CHAIN["CityGlobalMediator"],
            "CityBuildingContainerPrefab": fresh.SOURCE_CHAIN["CityBuildingContainerPrefab"],
            "CityWallPrefab": fresh.SOURCE_CHAIN["CityWallPrefab"],
            "GuardTowerPrefab": fresh.SOURCE_CHAIN["GuardTowerPrefab"],
            "UnityGroundExportScene": ground_manifest.get("scene"),
        },
        "unity_runtime_rules": {
            "city_container_parent": "SceneObject/land_root",
            "city_container_yaw": ground_manifest.get("container_yaw"),
            "runtime_lod_scale": ground_manifest.get("runtime_lod_scale"),
            "ground_transform_mode": "Unity runtime MeshRenderer vertices world-baked in UE centimeters; inverse-Y OBJ import and source-aware face-winding compensation; identity UE actors; Renderer.bounds retained only as culling evidence",
            "ground_face_winding_contract": ground_manifest.get("obj_face_winding_contract"),
            "city_translucent_alpha": "cm_mask_texture_lod_alpha.shader replaces texture alpha with interpolated mesh vertex color alpha; CityTranslucent_101_1 FBX ByPolygonVertex alpha is reproducibly rasterized into the UE texture",
        },
        "unity_runtime_boundary_obj_manifest": boundary.UNITY_RUNTIME_OBJ_MANIFEST,
        "unity_runtime_ground_obj_manifest": UNITY_RUNTIME_GROUND_OBJ_MANIFEST,
        "unity_runtime_ground_vertex_alpha_manifest": UNITY_RUNTIME_GROUND_VERTEX_ALPHA_MANIFEST,
        "boundary_spawned": boundary_spawned,
        "ground_spawned": ground_spawned,
        "boundary_actors": boundary_records,
        "ground_actors": ground_records,
        "exclusions": {
            "buildings": "not spawned",
            "roads": "not spawned",
            "cameras": "not spawned",
            "player_starts": "not spawned",
        },
    }
    os.makedirs(os.path.dirname(SCENE_MANIFEST), exist_ok=True)
    with open(SCENE_MANIFEST, "w", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)
    unreal.log("Rok Unity runtime ground map saved: {0}; boundary={1}, ground={2}".format(
        TARGET_LEVEL,
        boundary_spawned,
        ground_spawned,
    ))


if __name__ == "__main__":
    build_scene()
