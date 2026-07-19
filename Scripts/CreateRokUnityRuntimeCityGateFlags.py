import hashlib
import json
import os

import unreal

import CreateRokUnityFreshCityScene as fresh


PROJECT_ROOT = unreal.Paths.project_dir()
MIGRATION_ROOT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration")
UNITY_RUNTIME_CITY_GATE_FLAGS_MANIFEST = os.path.join(
    MIGRATION_ROOT,
    "unity_runtime_city_gate_flags.json",
)

CITY_TAG = "RokUnityRuntimeCityGateFlags"
FOLDER_ROOT = "RokUnityRuntimeCityGateFlags"
ACTOR_PREFIX = "UnityRuntimeCityGateFlag"
MESH_DESTINATION = "/Game/RokUnityRuntimeCityGateFlags/RuntimeObjMeshes"
TEXTURE_DESTINATION = "/Game/RokUnityRuntimeCityGateFlags/Textures"
MATERIAL_DESTINATION = "/Game/RokUnityRuntimeCityGateFlags/Materials"
EXPECTED_SCHEMA = "rok.unity_runtime_city_gate_flags.v1"
EXPECTED_RENDER_QUEUE = 2995


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
    return "".join(result).strip("_") or "CityGateFlag"


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


def validate_file(path, expected_hash, label):
    if not path or not os.path.exists(path):
        raise RuntimeError("Missing Unity CityGate flag {0}: {1}".format(label, path))
    if not expected_hash or file_sha256(path) != expected_hash:
        raise RuntimeError("Unity CityGate flag {0} hash drift: {1}".format(label, path))


def load_gate_flags_manifest():
    if not os.path.exists(UNITY_RUNTIME_CITY_GATE_FLAGS_MANIFEST):
        raise RuntimeError(
            "Missing Unity runtime CityGate flag manifest; run "
            "Scripts/BuildRokUnityRuntimeCityGateFlags.py first: {0}".format(
                UNITY_RUNTIME_CITY_GATE_FLAGS_MANIFEST
            )
        )
    with open(UNITY_RUNTIME_CITY_GATE_FLAGS_MANIFEST, "r", encoding="utf-8-sig") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != EXPECTED_SCHEMA:
        raise RuntimeError("Unexpected Unity CityGate flag schema: {0}".format(manifest.get("schema")))
    if manifest.get("failures"):
        raise RuntimeError("Unity CityGate flag manifest has failures: {0}".format(manifest.get("failures")))
    records = manifest.get("records", [])
    if len(records) != 2 or int(manifest.get("active_drawable_flag_renderer_count") or 0) != 2:
        raise RuntimeError("Unity CityGate flag manifest must contain exactly two drawable active SpriteRenderers")
    if len({record.get("flag_id") for record in records}) != 2:
        raise RuntimeError("Unity CityGate flag manifest contains duplicate flag IDs")
    if len({record.get("source_key") for record in records}) != 2:
        raise RuntimeError("Unity CityGate flag manifest contains duplicate SpriteRenderer source keys")
    for record in records:
        for path_field, hash_field, label in (
            ("obj_path", "obj_sha256", "OBJ"),
            ("source_prefab_path", "source_prefab_sha256", "prefab"),
            ("sprite_path", "sprite_sha256", "sprite texture"),
            ("mask_path", "mask_sha256", "runtime mask texture"),
            ("material_path", "material_sha256", "material"),
            ("shader_path", "shader_sha256", "shader"),
            ("baked_texture_path", "baked_texture_sha256", "baked runtime texture"),
            ("mask_sprite_source_path", "mask_sprite_source_sha256", "MaskSprite source"),
            ("town_building_source_path", "town_building_source_sha256", "TownBuilding source"),
            ("rs_source_path", "rs_source_sha256", "RS color source"),
        ):
            validate_file(record.get(path_field), record.get(hash_field), label)
        if record.get("source_mode") != "unity_active_renderer" or record.get("manual_layout"):
            raise RuntimeError("CityGate flag is not an active source Renderer: {0}".format(record.get("flag_id")))
        if not record.get("active_in_hierarchy") or not record.get("renderer_enabled"):
            raise RuntimeError("CityGate flag is inactive or disabled: {0}".format(record.get("flag_id")))
        if record.get("renderer_type") != "SpriteRenderer":
            raise RuntimeError("CityGate flag must originate from a SpriteRenderer: {0}".format(record.get("flag_id")))
        if record.get("representation") != "unity_imported_tight_sprite_world_baked_obj":
            raise RuntimeError("CityGate flag is not the imported Unity tight SpriteMesh representation")
        if int(record.get("vertex_count") or 0) != 13 or int(record.get("triangle_count") or 0) != 11:
            raise RuntimeError("CityGate flag tight SpriteMesh geometry drifted")
        if int(record.get("material_render_queue") or 0) != EXPECTED_RENDER_QUEUE:
            raise RuntimeError("CityGate flag Unity render queue drifted")
        if record.get("runtime_material_color_1") != manifest.get("runtime_build_green"):
            raise RuntimeError("CityGate flag does not use the initial own-city runtime buildGreen")
    return manifest


def import_flag_obj(record):
    source_path = record["obj_path"]
    asset_name = "SM_RokUnityCityGateFlag_{0}_{1}".format(
        safe_name(record["object_name"]),
        record["obj_sha256"][:10],
    )
    asset_path = package_path(MESH_DESTINATION, asset_name)
    force_reimport = os.environ.get(
        "ROK_FORCE_REIMPORT_GATE_FLAGS",
        os.environ.get("ROK_FORCE_REIMPORT", "0"),
    ) != "0"
    existing = load_asset(asset_path)
    if isinstance(existing, unreal.StaticMesh) and not force_reimport:
        return existing

    unreal.EditorAssetLibrary.make_directory(MESH_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", MESH_DESTINATION)
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

    imported = load_asset(asset_path)
    if isinstance(imported, unreal.StaticMesh):
        return imported
    for imported_path in task.get_editor_property("imported_object_paths"):
        candidate = load_asset(imported_path)
        if isinstance(candidate, unreal.StaticMesh):
            return candidate
    raise RuntimeError("Failed to import Unity CityGate flag OBJ: {0}".format(source_path))


def import_baked_texture(manifest):
    baked = manifest["shared_baked_texture"]
    source_path = baked["baked_texture_path"]
    asset_name = "T_RokUnityCityGateFlag_BuildGreen_{0}".format(baked["baked_texture_sha256"][:10])
    asset_path = package_path(TEXTURE_DESTINATION, asset_name)
    force_reimport = os.environ.get(
        "ROK_FORCE_REIMPORT_GATE_FLAG_TEXTURE",
        os.environ.get("ROK_FORCE_REIMPORT_TEXTURES", "0"),
    ) != "0"
    existing = load_asset(asset_path)
    if existing and not force_reimport:
        return existing

    unreal.EditorAssetLibrary.make_directory(TEXTURE_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", TEXTURE_DESTINATION)
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
        raise RuntimeError("Failed to import baked Unity CityGate flag texture: {0}".format(source_path))
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
    if unreal.MaterialEditingLibrary.get_material_expressions(material):
        raise RuntimeError("Failed to clear CityGate flag material expressions: {0}".format(material.get_path_name()))


def create_flag_material(manifest, texture):
    baked = manifest["shared_baked_texture"]
    material_name = "M_RokUnityCityGateFlag_BuildGreen_RQ2995_{0}".format(
        baked["baked_texture_sha256"][:10]
    )
    asset_path = package_path(MATERIAL_DESTINATION, material_name)
    material = load_asset(asset_path)
    if not material:
        unreal.EditorAssetLibrary.make_directory(MATERIAL_DESTINATION)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            material_name,
            MATERIAL_DESTINATION,
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
    if not unreal.MaterialEditingLibrary.connect_material_property(
        texture_node,
        "RGB",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    ):
        raise RuntimeError("Failed to connect CityGate flag emissive color")
    if not unreal.MaterialEditingLibrary.connect_material_property(
        texture_node,
        "A",
        unreal.MaterialProperty.MP_OPACITY,
    ):
        raise RuntimeError("Failed to connect CityGate flag opacity")
    compile_errors = [str(item) for item in unreal.MaterialEditingLibrary.recompile_material(material)]
    if compile_errors:
        raise RuntimeError("CityGate flag material compile failed: {0}".format(compile_errors))
    material.modify(True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, False):
        raise RuntimeError("Failed to save CityGate flag material: {0}".format(asset_path))
    return material


def configure_component(actor, record):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        raise RuntimeError("CityGate flag actor has no StaticMeshComponent: {0}".format(actor.get_actor_label()))
    settings = (
        ("cast_shadow", False),
        ("receive_decals", False),
        ("use_as_occluder", False),
        ("collision_enabled", unreal.CollisionEnabled.NO_COLLISION),
        ("translucency_sort_priority", int(record["material_render_queue"])),
        ("translucency_sort_distance_offset", 0.0),
        ("reverse_culling", False),
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


def actor_record(actor, source_record, material, texture):
    bounds_origin, bounds_extent = actor.get_actor_bounds(False)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    return {
        "actor": actor.get_actor_label(),
        "flag_id": source_record["flag_id"],
        "source_key": source_record["source_key"],
        "source_mode": source_record["source_mode"],
        "manual_layout": False,
        "unity_object": source_record["object_name"],
        "unity_object_path": source_record["object_path"],
        "source_prefab": source_record["source_prefab_path"],
        "source_prefab_guid": source_record["source_prefab_guid"],
        "source_prefab_sha256": source_record["source_prefab_sha256"],
        "sprite_renderer_file_id": source_record["sprite_renderer_file_id"],
        "mask_sprite_component_file_id": source_record["mask_sprite_component_file_id"],
        "source_sprite_guid": source_record["sprite_guid"],
        "source_sprite_sha256": source_record["sprite_sha256"],
        "source_mask_guid": source_record["mask_guid"],
        "source_mask_sha256": source_record["mask_sha256"],
        "source_material_guid": source_record["material_guid"],
        "source_material_sha256": source_record["material_sha256"],
        "source_shader_guid": source_record["shader_guid"],
        "source_shader_sha256": source_record["shader_sha256"],
        "runtime_material_color_1": source_record["runtime_material_color_1"],
        "main_city_sprite_color": source_record["main_city_sprite_color"],
        "baked_texture_sha256": source_record["baked_texture_sha256"],
        "obj_path": source_record["obj_path"],
        "obj_sha256": source_record["obj_sha256"],
        "representation": source_record["representation"],
        "obj_import_axis_compensation": source_record["obj_import_axis_compensation"],
        "unity_material_render_queue": source_record["material_render_queue"],
        "ue_material": material.get_path_name(),
        "ue_texture": texture.get_path_name(),
        "ue_expected_bounds_center": source_record["ue_bounds_center"],
        "ue_expected_bounds_extents": source_record["ue_bounds_extents"],
        "ue_alignment_mode": "Unity tight SpriteMesh world-baked OBJ; actor rotation/scale identity; translation only corrects importer bounds-center drift",
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


def spawn_gate_flags(manifest=None):
    manifest = manifest or load_gate_flags_manifest()
    texture = import_baked_texture(manifest)
    material = create_flag_material(manifest, texture)
    spawned_records = []
    for index, record in enumerate(manifest.get("records", [])):
        mesh = import_flag_obj(record)
        actor = fresh.spawn_static_mesh_actor(
            "{0}_{1}_{2}".format(ACTOR_PREFIX, safe_name(record["object_name"]), index),
            mesh,
            (0.0, 0.0, 0.0),
            (1.0, 1.0, 1.0),
            material,
            (0.0, 0.0, 0.0),
            "{0}/BoundaryDetails/CityGateFlags".format(FOLDER_ROOT),
        )
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        configure_component(actor, record)
        align_visible_bounds_center(actor, record["ue_bounds_center"])
        actor.tags = []
        set_tags(actor, [
            CITY_TAG,
            "RokUnityRuntimeCityGateFlag",
            "RokUnityRuntimeObjBaked",
            "UnityFlagId_{0}".format(safe_name(record["flag_id"])),
            "UnitySourceKey_{0}".format(safe_name(record["source_key"])),
            "UnitySourceMode_unity_active_renderer",
            "UnityRenderer_SpriteRenderer",
            "UnityRendererActiveInHierarchy",
            "UnityRendererEnabled",
            "UnityImportedTightSpriteMesh",
            "UnityObjImportAxisCompensation_FbxFactoryInverseY",
            "UnitySpriteRendererFileId_{0}".format(record["sprite_renderer_file_id"]),
            "UnityMaskSpriteComponentFileId_{0}".format(record["mask_sprite_component_file_id"]),
            "UnitySpriteGuid_{0}".format(record["sprite_guid"]),
            "UnityMaskGuid_{0}".format(record["mask_guid"]),
            "UnityMaterialGuid_{0}".format(record["material_guid"]),
            "UnityShaderGuid_{0}".format(record["shader_guid"]),
            "UnityRuntimeColor_RSBuildGreen_62_125_195",
            "UnityRuntimeMask_MaskSpritePropertyBlock",
            "UnityRuntimeTexture_GammaShaderBakedRGBA",
            "UnityOpacity_ColorA_SaturateMaskR2_CityTransparency",
            "UnityRenderQueue_{0}".format(record["material_render_queue"]),
            "UnityShader_custom_map_map_building_with_mask_5",
            "UnityShaderZWrite_Off",
            "UnityManualLayout_False",
            "UnityBakedOrientationScaleIdentity",
            "UnityRootRotationOverride_Identity",
            "UnityContainerYaw_Neg45",
            "UnityRendererBoundsCenterAligned",
        ])
        spawned_records.append(actor_record(actor, record, material, texture))
    return len(spawned_records), spawned_records
