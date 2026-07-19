import json
import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokUnityFreshCityScene as fresh


TARGET_LEVEL = os.environ.get("ROK_UNITY_RUNTIME_BOUNDARY_LEVEL", "/Game/Maps/L_RokUnityRuntimeBoundaryOnly")
CITY_TAG = "RokUnityRuntimeBoundaryOnly"
FOLDER_ROOT = "RokUnityRuntimeBoundaryOnly"
ACTOR_PREFIX = os.environ.get("ROK_UNITY_RUNTIME_BOUNDARY_ACTOR_PREFIX", "UnityRuntimeBoundaryOnly")
MIGRATION_ROOT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration")
SCENE_MANIFEST = os.path.join(MIGRATION_ROOT, "unity_runtime_boundary_only_scene_manifest.json")
UNITY_REFERENCE_JSON = os.path.join(MIGRATION_ROOT, "unity_runtime_boundary_reference.json")
UNITY_RUNTIME_OBJ_MANIFEST = os.path.join(MIGRATION_ROOT, "unity_runtime_boundary_obj_manifest.json")
OBJ_MESH_DESTINATION = "/Game/RokUnityRuntimeBoundaryOnly/RuntimeObjMeshes"
TOWER_VARIANT_2_TEXTURE = os.path.join(
    fresh.source3d.ROK_UNITY_SHARED_MAP_MATERIAL_ROOT,
    "CityTower_101_2.png",
)


def actor_tags(actor):
    return [str(tag) for tag in list(actor.tags)]


def set_tags(actor, tags):
    existing = actor_tags(actor)
    for tag in tags:
        if tag not in existing:
            existing.append(tag)
    actor.tags = existing


def load_unity_reference():
    if not os.path.exists(UNITY_REFERENCE_JSON):
        raise RuntimeError("Missing Unity runtime boundary reference: {0}".format(UNITY_REFERENCE_JSON))
    with open(UNITY_REFERENCE_JSON, "r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def unity_vector_to_ue(values):
    return fresh.unity_to_ue_exact(values)


def unity_extent_to_ue(values):
    return [
        abs(float(values[0])) * fresh.UE_UNITS_PER_UNITY_UNIT,
        abs(float(values[2])) * fresh.UE_UNITS_PER_UNITY_UNIT,
        abs(float(values[1])) * fresh.UE_UNITS_PER_UNITY_UNIT,
    ]


def source_path_from_reference(record):
    mesh_asset_path = record.get("mesh_asset_path", "").replace("/", os.sep)
    return os.path.join(fresh.ROK_UNITY_CLIENT_ROOT, mesh_asset_path)


def source_prefab_from_reference(record):
    if record.get("mesh_name", "").startswith("CityWall_101_1"):
        return fresh.SOURCE_CHAIN["CityWallPrefab"]
    return fresh.SOURCE_CHAIN["GuardTowerPrefab"]


def source_prefab_from_mesh_stem(mesh_stem):
    if str(mesh_stem).startswith("CityWall_101_1"):
        return fresh.SOURCE_CHAIN["CityWallPrefab"]
    return fresh.SOURCE_CHAIN["GuardTowerPrefab"]


def import_runtime_boundary_materials():
    materials = fresh.source3d.import_rok_unity_boundary_materials()
    if os.path.exists(TOWER_VARIANT_2_TEXTURE):
        texture = fresh.runtime.import_runtime_texture_as(TOWER_VARIANT_2_TEXTURE, "T_RokUnity_CityTower_101_2")
        material = fresh.runtime.create_textured_surface_material(texture, "M_RokUnity_CityTower_101_2") if texture else None
        if material:
            materials["tower_2"] = material
    else:
        unreal.log_warning("Missing CityTower_101_2 runtime texture: {0}".format(TOWER_VARIANT_2_TEXTURE))
    return materials


def material_key_for_obj_record(obj_record):
    mesh_stem = obj_record.get("mesh_stem", "")
    object_name = obj_record.get("object_name", "")
    if mesh_stem == "CityTower_101_1_4" or object_name.startswith("CityTower_1_4"):
        return "tower_2"
    if mesh_stem.startswith("CityGate_101_1"):
        return "gate"
    if mesh_stem.startswith("CityWall_101_1"):
        return "wall"
    if mesh_stem.startswith("CityTower_101_1"):
        return "tower"
    return "unknown"


def material_source_texture_for_key(material_key):
    if material_key == "tower_2":
        return TOWER_VARIANT_2_TEXTURE
    specs = getattr(fresh.source3d, "ROK_UNITY_BOUNDARY_TEXTURE_SPECS", {})
    if material_key in specs:
        return os.path.join(fresh.source3d.ROK_UNITY_SHARED_MAP_MATERIAL_ROOT, specs[material_key][0])
    return ""


def align_visible_bounds_center(actor, target_center):
    origin, _extent = actor.get_actor_bounds(False)
    actor.set_actor_location(actor.get_actor_location() + (target_center - origin), False, False)


def actor_record(actor, source_record, source_path, source_prefab, object_name, initial_location, initial_rotation, initial_scale):
    bounds_origin, bounds_extent = actor.get_actor_bounds(False)
    return {
        "actor": actor.get_actor_label(),
        "source_prefab": source_prefab,
        "unity_object": object_name,
        "source_mesh": source_path,
        "source_mesh_stem": os.path.splitext(os.path.basename(source_path))[0],
        "unity_world_position": source_record.get("world_position"),
        "unity_world_rotation_quaternion": source_record.get("world_rotation"),
        "unity_world_scale": source_record.get("lossy_scale"),
        "unity_bounds_center": source_record.get("bounds_center"),
        "unity_bounds_extents": source_record.get("bounds_extents"),
        "unity_bounds_size": source_record.get("bounds_size"),
        "ue_initial_location_from_unity_transform": initial_location,
        "ue_initial_rotation_from_unity_transform": initial_rotation,
        "ue_initial_scale_from_unity_transform": initial_scale,
        "ue_mesh_import_scale_multiplier": fresh.boundary_mesh_import_scale_multiplier(source_path),
        "ue_alignment_mode": "visible bounds center aligned to Unity Renderer.bounds.center",
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


def import_runtime_obj_mesh(record):
    obj_path = record.get("obj_path")
    if not obj_path or not os.path.exists(obj_path):
        raise RuntimeError("Missing Unity runtime OBJ: {0}".format(obj_path))

    asset_name = fresh.safe_tag(os.path.splitext(os.path.basename(obj_path))[0])
    package_path = fresh.proto.asset_package_path(OBJ_MESH_DESTINATION, asset_name)
    existing = fresh.proto.load_asset_if_exists(package_path)
    force_reimport = os.environ.get("ROK_FORCE_REIMPORT", "0") == "1"
    if isinstance(existing, unreal.StaticMesh) and not force_reimport:
        return existing

    unreal.EditorAssetLibrary.make_directory(OBJ_MESH_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", obj_path)
    task.set_editor_property("destination_path", OBJ_MESH_DESTINATION)
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
    raise RuntimeError("Failed to import Unity runtime OBJ: {0}".format(obj_path))


def load_obj_manifest():
    if not os.path.exists(UNITY_RUNTIME_OBJ_MANIFEST):
        raise RuntimeError("Missing Unity runtime OBJ manifest: {0}".format(UNITY_RUNTIME_OBJ_MANIFEST))
    with open(UNITY_RUNTIME_OBJ_MANIFEST, "r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def actor_record_from_obj(actor, obj_record, source_prefab, material_key):
    bounds_origin, bounds_extent = actor.get_actor_bounds(False)
    mesh_stem = obj_record.get("mesh_stem", "")
    source_mesh = os.path.join(fresh.ROK_UNITY_CLIENT_ROOT, obj_record.get("mesh_asset_path", "").replace("/", os.sep))
    return {
        "actor": actor.get_actor_label(),
        "source_prefab": source_prefab,
        "unity_object": obj_record.get("object_name"),
        "source_mesh": source_mesh,
        "source_mesh_stem": mesh_stem,
        "unity_bounds_center": obj_record.get("unity_bounds_center"),
        "unity_bounds_extents": obj_record.get("unity_bounds_extents"),
        "ue_unity_renderer_bounds_center_evidence": obj_record.get("ue_bounds_center"),
        "ue_unity_renderer_bounds_extents_evidence": obj_record.get("ue_bounds_extents"),
        "ue_expected_bounds_center": [float(bounds_origin.x), float(bounds_origin.y), float(bounds_origin.z)],
        "ue_expected_bounds_extents": [float(bounds_extent.x), float(bounds_extent.y), float(bounds_extent.z)],
        "ue_alignment_mode": "Unity runtime MeshRenderer geometry is already world-baked into the OBJ in UE centimeters; actor location, rotation, and scale remain identity; Renderer.bounds is retained as source evidence only",
        "unity_material_key": material_key,
        "unity_material_source_texture": material_source_texture_for_key(material_key),
        "obj_path": obj_record.get("obj_path"),
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


def spawn_boundary_from_runtime_obj_manifest(obj_manifest):
    materials = import_runtime_boundary_materials()
    records = []
    for index, obj_record in enumerate(obj_manifest.get("records", [])):
        mesh_stem = obj_record.get("mesh_stem", "")
        material_key = material_key_for_obj_record(obj_record)
        source_prefab = source_prefab_from_mesh_stem(mesh_stem)
        source_mesh = os.path.join(fresh.ROK_UNITY_CLIENT_ROOT, obj_record.get("mesh_asset_path", "").replace("/", os.sep))
        mesh = import_runtime_obj_mesh(obj_record)
        material = materials.get(material_key)
        if not material:
            unreal.log_warning("Missing material key {0} for runtime boundary object {1}; falling back by source".format(
                material_key,
                obj_record.get("object_name", ""),
            ))
            material = fresh.source3d.boundary_material_for_source(source_mesh, materials)
        actor = fresh.spawn_static_mesh_actor(
            "{0}_{1}_{2}_{3}".format(
                ACTOR_PREFIX,
                os.path.splitext(os.path.basename(source_prefab))[0],
                fresh.safe_tag(obj_record.get("object_name", "Boundary")),
                index,
            ),
            mesh,
            (0.0, 0.0, 0.0),
            (1.0, 1.0, 1.0),
            material,
            (0.0, 0.0, 0.0),
            "{0}/Boundary".format(FOLDER_ROOT),
        )
        actor.tags = []
        source_tags = [
            CITY_TAG,
            "RokUnityRuntimeBoundarySource",
            "RokUnityRuntimeObjBaked",
            "UnityBakedOrientationScaleIdentity",
            "UnityWorldBakedActorIdentity",
            "UnityMesh_{0}".format(mesh_stem),
            "UnityMaterial_{0}".format(material_key),
            "UnitySourcePrefab_{0}".format(os.path.splitext(os.path.basename(source_prefab))[0]),
            "UnityPrefabObject_{0}".format(fresh.safe_tag(obj_record.get("object_name", ""))),
        ]
        set_tags(actor, source_tags)
        actor.set_folder_path("{0}/Boundary".format(FOLDER_ROOT))
        records.append(actor_record_from_obj(actor, obj_record, source_prefab, material_key))
    return len(records), records


def spawn_boundary_from_unity_reference(reference):
    materials = fresh.source3d.import_rok_unity_boundary_materials()
    records = []
    for index, source_record in enumerate(reference.get("records", [])):
        source_path = source_path_from_reference(source_record)
        source_prefab = source_prefab_from_reference(source_record)
        object_name = source_record.get("path", "").split("/")[-1] or source_record.get("mesh_name", "Boundary")
        label = "{0}_{1}_{2}_{3}".format(
            ACTOR_PREFIX,
            os.path.splitext(os.path.basename(source_prefab))[0],
            fresh.safe_tag(object_name),
            index,
        )
        actor = fresh.spawn_exact_boundary_prefab_renderer(
            label,
            source_path,
            (
                source_record.get("world_position", [0.0, 0.0, 0.0]),
                source_record.get("world_rotation", [0.0, 0.0, 0.0, 1.0]),
                source_record.get("lossy_scale", [1.0, 1.0, 1.0]),
            ),
            fresh.source3d.boundary_material_for_source(source_path, materials),
            source_prefab,
            object_name,
        )
        if not actor:
            continue

        actor.set_folder_path("{0}/Boundary".format(FOLDER_ROOT))
        actor.tags = []
        set_tags(actor, [
            CITY_TAG,
            "RokUnityRuntimeBoundarySource",
            "RokUnityCityBoundaryPrefabSource",
            "RokUnityBoundaryExactUnityRendererSource",
            "UnityRendererBoundsCenterAligned",
            "UnityMesh_{0}".format(os.path.splitext(os.path.basename(source_path))[0]),
            "UnitySourcePrefab_{0}".format(os.path.splitext(os.path.basename(source_prefab))[0]),
            "UnityPrefabObject_{0}".format(fresh.safe_tag(object_name)),
        ])

        initial_location = [
            float(actor.get_actor_location().x),
            float(actor.get_actor_location().y),
            float(actor.get_actor_location().z),
        ]
        initial_rotation = [
            float(actor.get_actor_rotation().pitch),
            float(actor.get_actor_rotation().yaw),
            float(actor.get_actor_rotation().roll),
        ]
        initial_scale = [
            float(actor.get_actor_scale3d().x),
            float(actor.get_actor_scale3d().y),
            float(actor.get_actor_scale3d().z),
        ]
        align_visible_bounds_center(actor, unity_vector_to_ue(source_record.get("bounds_center", [0.0, 0.0, 0.0])))
        records.append(actor_record(actor, source_record, source_path, source_prefab, object_name, initial_location, initial_rotation, initial_scale))
    return len(records), records


def build_scene():
    fresh.TARGET_LEVEL = TARGET_LEVEL
    fresh.SCENE_MANIFEST = SCENE_MANIFEST
    fresh.CITY_TAG = CITY_TAG
    fresh.require_sources()

    fresh.create_or_load_target_map()
    fresh.clear_current_level()

    reference = load_unity_reference()
    obj_manifest = load_obj_manifest()
    boundary_spawned, boundary_records = spawn_boundary_from_runtime_obj_manifest(obj_manifest)

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError("Failed to save runtime boundary map: {0}".format(TARGET_LEVEL))
    unreal.EditorAssetLibrary.save_asset(TARGET_LEVEL, only_if_is_dirty=False)

    summary = {
        "schema": "rok.unity_runtime_boundary_only_scene.v1",
        "target_level": TARGET_LEVEL,
        "rok_unity_root": fresh.ROK_UNITY_ROOT,
        "source_files": {
            "GameAppScene": fresh.SOURCE_CHAIN["GameAppScene"],
            "SwitchMainCityCmd": fresh.SOURCE_CHAIN["SwitchMainCityCmd"],
            "CityGlobalMediator": fresh.SOURCE_CHAIN["CityGlobalMediator"],
            "CityBuildingContainerPrefab": fresh.SOURCE_CHAIN["CityBuildingContainerPrefab"],
            "CityWallPrefab": fresh.SOURCE_CHAIN["CityWallPrefab"],
            "GuardTowerPrefab": fresh.SOURCE_CHAIN["GuardTowerPrefab"],
        },
        "unity_runtime_rules": {
            "city_container_parent": "SceneObject/land_root",
            "city_container_yaw": reference.get("container_yaw"),
            "runtime_lod_scale": reference.get("runtime_lod_scale"),
            "city_wall_and_guard_tower_local_position": "Vector3.zero from ConvertCityObjTileToLocal",
            "boundary_root_scale": "GridCollideItem.SetScale(LevelDetailCastle scale from CameraParamLod city dxf)",
        },
        "unity_reference_json": UNITY_REFERENCE_JSON,
        "unity_reference_png": os.path.join(MIGRATION_ROOT, "unity_runtime_boundary_reference.png"),
        "unity_runtime_obj_manifest": UNITY_RUNTIME_OBJ_MANIFEST,
        "boundary_transform_mode": "formal boundary-only map: Unity runtime MeshRenderer geometry is baked to OBJ in UE centimeters and imported as static meshes; no ground, buildings, roads, asset review, camera, or player start",
        "boundary_spawned": boundary_spawned,
        "boundary_tagged": boundary_spawned,
        "boundary_actors": boundary_records,
    }
    os.makedirs(os.path.dirname(SCENE_MANIFEST), exist_ok=True)
    with open(SCENE_MANIFEST, "w", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)
    unreal.log("Rok Unity runtime boundary-only map saved: {0}; boundary={1}".format(TARGET_LEVEL, boundary_spawned))


if __name__ == "__main__":
    build_scene()
