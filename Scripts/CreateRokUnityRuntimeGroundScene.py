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
UNITY_RUNTIME_GROUND_OBJ_MANIFEST = os.path.join(MIGRATION_ROOT, "unity_runtime_ground_obj_manifest.json")
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
        return json.load(handle)


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
    force_reimport = os.environ.get("ROK_FORCE_REIMPORT", "0") == "1"
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
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    imported = fresh.proto.load_asset_if_exists(package_path)
    if isinstance(imported, unreal.StaticMesh):
        return imported
    for imported_path in task.get_editor_property("imported_object_paths"):
        candidate = fresh.proto.load_asset_if_exists(imported_path)
        if isinstance(candidate, unreal.StaticMesh):
            return candidate
    raise RuntimeError("Failed to import Unity runtime ground OBJ: {0}".format(obj_path))


def import_ground_texture(source_path, asset_name):
    if not source_path or not os.path.exists(source_path):
        raise RuntimeError("Missing Unity runtime ground texture: {0}".format(source_path))

    safe_name = fresh.proto.safe_asset_name(asset_name)
    package_path = fresh.proto.asset_package_path(GROUND_TEXTURE_DESTINATION, safe_name)
    existing = fresh.proto.load_asset_if_exists(package_path)
    force_reimport = os.environ.get("ROK_FORCE_REIMPORT_TEXTURES", "0") == "1"
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
    else:
        unreal.EditorAssetLibrary.make_directory(GROUND_MATERIAL_DESTINATION)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            material_name,
            GROUND_MATERIAL_DESTINATION,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    material.set_editor_property("blend_mode", blend_mode)
    material.set_editor_property("two_sided", True)
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


def choose_ground_texture_path(record):
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


def align_visible_bounds_center(actor, expected_center):
    if not expected_center:
        return
    origin, _extent = actor.get_actor_bounds(False)
    target = unreal.Vector(float(expected_center[0]), float(expected_center[1]), float(expected_center[2]))
    actor.set_actor_location(actor.get_actor_location() + (target - origin), False, False)


def actor_record_from_ground(actor, record, texture_source, texture_source_kind):
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
        "obj_path": record.get("obj_path"),
        "ue_expected_bounds_center": record.get("ue_bounds_center"),
        "ue_expected_bounds_extents": record.get("ue_bounds_extents"),
        "ue_alignment_mode": "Unity runtime ground MeshRenderer geometry baked to OBJ in UE centimeters; visible bounds center aligned to Unity Renderer.bounds.center",
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
    for index, record in enumerate(ground_manifest.get("records", [])):
        mesh = import_runtime_ground_obj_mesh(record)
        texture_source, texture_source_kind = choose_ground_texture_path(record)
        texture_asset_name = "T_RokUnityRuntimeGround_{0}".format(
            fresh.safe_tag(os.path.splitext(os.path.basename(texture_source))[0])
        )
        texture = import_ground_texture(texture_source, texture_asset_name)
        material_name = "M_RokUnityRuntimeGround_{0}_RQ{1}_Z{2}".format(
            fresh.safe_tag(record.get("material_name", "Ground")),
            record.get("material_render_queue", "0"),
            fresh.safe_tag(record.get("shader_zwrite", "")),
        )
        material = create_ground_material(texture, material_name, material_blend_mode(record))
        label = "{0}_{1}_{2}".format(
            ACTOR_PREFIX,
            fresh.safe_tag(record.get("object_name", "Ground")),
            index,
        )
        actor = fresh.proto.spawn_mesh(
            label,
            mesh,
            (0.0, 0.0, 0.0),
            (1.0, 1.0, 1.0),
            material,
            (0.0, 0.0, 0.0),
            "{0}/Ground".format(FOLDER_ROOT),
        )
        actor.tags = []
        set_tags(actor, [
            CITY_TAG,
            "RokUnityRuntimeGroundSource",
            "RokUnityRuntimeObjBaked",
            "UnityRendererBoundsBaked",
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
        ])
        actor.set_folder_path("{0}/Ground".format(FOLDER_ROOT))
        configure_static_mesh_component(actor, int(record.get("material_render_queue") or index))
        align_visible_bounds_center(actor, record.get("ue_bounds_center"))
        records.append(actor_record_from_ground(actor, record, texture_source, texture_source_kind))
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
        "schema": "rok.unity_runtime_ground_scene.v1",
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
            "ground_transform_mode": "Unity runtime MeshRenderer geometry and shader-baked ground textures exported from Unity; UE actor visible bounds centers are aligned to Unity Renderer.bounds.center, same as boundary actors",
        },
        "unity_runtime_boundary_obj_manifest": boundary.UNITY_RUNTIME_OBJ_MANIFEST,
        "unity_runtime_ground_obj_manifest": UNITY_RUNTIME_GROUND_OBJ_MANIFEST,
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
