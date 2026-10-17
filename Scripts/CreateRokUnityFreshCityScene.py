import json
import math
import os
import re
import sys

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokPrototypeScene as proto
import CreateRokRuntimeCityScene as runtime
import CreateRokTestUnityGameAppCityScene as source3d
import CreateRokUnityGameAppCityScene as gameapp
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from rok_data_paths import DATA_ROOT  # noqa: E402


TARGET_LEVEL = os.environ.get("ROK_UNITY_FRESH_CITY_LEVEL", "/Game/Maps/L_RokUnityFreshCity")
ROK_UNITY_ROOT = os.environ.get("ROK_UNITY_ROOT", r"D:\ueDemo\Rok-unity")
ROK_UNITY_CLIENT_ROOT = os.path.join(ROK_UNITY_ROOT, "client-unity")
ROK_UNITY_ASSETS = os.path.join(ROK_UNITY_CLIENT_ROOT, "Assets")
ROK_UNITY_CITY_BUILDING_PREFAB_ROOT = os.path.join(
    ROK_UNITY_ASSETS,
    "BundleAssets",
    "Scene",
    "City_building",
    "city_building_1",
)
MIGRATION_ROOT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration")
SCENE_MANIFEST = os.path.join(MIGRATION_ROOT, "unity_fresh_city_scene_manifest.json")

CITY_TAG = "RokUnityFreshCity"
UNITY_GRID_SIZE = 0.1
UNITY_GRID_SIZE_HALF = 0.05
UNITY_CITY_CONTAINER_YAW = -45.0
UE_UNITS_PER_UNITY_UNIT = float(os.environ.get("ROK_UNITY_FRESH_CITY_UE_SCALE", "500.0"))
CITY_SPRITE_SCALE = float(os.environ.get("ROK_UNITY_FRESH_CITY_SPRITE_SCALE", "0.82"))
CAMERA_ASPECT = 16.0 / 10.0
CAMERA_VERTICAL_FOV = 7.5
CAMERA_VIEW_DISTANCE = 25.5
CAMERA_VIEW_CENTER = (-0.12, -0.18)
CAMERA_FORWARD_UNITY = (0.0, -0.70710678, 0.70710678)
DEFAULT_UNITY_CITY_DXF = 271.623
UNITY_CITY_SIZE_BY_AGE = {1: 41, 2: 45, 3: 49, 4: 53, 5: 57}

SOURCE_CHAIN = {
    "GameAppScene": os.path.join(ROK_UNITY_ASSETS, "BundleAssets", "Scenes", "GameApp.unity"),
    "SwitchMainCityCmd": os.path.join(
        ROK_UNITY_ASSETS,
        "Scripts",
        "Hotfix",
        "MVC",
        "CMD",
        "SwitchMainCityCmd.cs",
    ),
    "CityGlobalMediator": os.path.join(
        ROK_UNITY_ASSETS,
        "Scripts",
        "Hotfix",
        "MVC",
        "GlobalMediator",
        "CityGlobalMediator.cs",
    ),
    "CityBuildingContainerPrefab": os.path.join(
        ROK_UNITY_ASSETS,
        "BundleAssets",
        "Scene",
        "City",
        "city_function",
        "CityBuildingContainer.prefab",
    ),
    "Map4Data": os.path.join(ROK_UNITY_ASSETS, "BundleAssets", "Map", "Data", "map_4_data.bytes"),
    "TileData": os.path.join(ROK_UNITY_ASSETS, "BundleAssets", "Map", "Data", "tile_data.bytes"),
    "CityWallPrefab": os.path.join(
        ROK_UNITY_ASSETS,
        "BundleAssets",
        "Scene",
        "City_building",
        "city_building_1",
        "build_6_101_1.prefab",
    ),
    "GuardTowerPrefab": os.path.join(
        ROK_UNITY_CITY_BUILDING_PREFAB_ROOT,
        "build_8_101_1.prefab",
    ),
    "DecorationTreePrefab": os.path.join(ROK_UNITY_CITY_BUILDING_PREFAB_ROOT, "build_52_101_1.prefab"),
    "DecorationRockPrefab": os.path.join(ROK_UNITY_CITY_BUILDING_PREFAB_ROOT, "build_53_101_1.prefab"),
}

GUID_ASSET_INDEX = None
PREFAB_SPRITE_CACHE = {}

GROUND_TEXTURES = {
    "outer": os.path.join(ROK_UNITY_ASSETS, "BundleAssets", "Map", "Env", "Ground", "Terrain_grass_08_3g2.png"),
    "city": os.path.join(ROK_UNITY_ASSETS, "T4MOBJ", "Terrains", "Texture", "map_incity4.png"),
    "road": os.path.join(
        ROK_UNITY_ASSETS,
        "BundleAssets",
        "land",
        "Texture",
        "shared_landtexture",
        "dirtRoad01.png",
    ),
}


def read_text(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as handle:
        return handle.read()


def require_sources():
    missing = [name for name, path in SOURCE_CHAIN.items() if not os.path.exists(path)]
    if missing:
        raise RuntimeError("Missing Rok-unity source files: {0}".format(", ".join(missing)))

    checks = [
        ("GameAppScene", "land_root"),
        ("SwitchMainCityCmd", "ReadMapBriefDataFromFile2(\"map_4_data\""),
        ("SwitchMainCityCmd", "ReadMapDataFromFile2(\"tile_data\""),
        ("CityGlobalMediator", "CityBuildingContainer"),
        ("CityGlobalMediator", "ConvertCityObjTileToLocal"),
    ]
    for key, needle in checks:
        text = read_text(SOURCE_CHAIN[key])
        if needle not in text:
            raise RuntimeError("Rok-unity source check failed: {0} missing `{1}`".format(key, needle))

    for key, path in GROUND_TEXTURES.items():
        if not os.path.exists(path):
            unreal.log_warning("Missing optional Rok-unity ground texture {0}: {1}".format(key, path))


def safe_tag(value):
    return re.sub(r"[^0-9A-Za-z_]+", "_", str(value))


def set_tags(actor, tags):
    actor.tags = list(set([str(tag) for tag in list(actor.tags)] + list(tags)))


def spawn_actor_from_class_direct(actor_class, location, rotation, scale=None):
    """Spawn directly through UWorld so UE commandlets do not require PlacementSubsystem."""
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        raise RuntimeError("Cannot spawn actor without an editor world")
    scale = scale or unreal.Vector(1.0, 1.0, 1.0)
    transform = unreal.Transform(location, rotation, scale)
    gameplay_statics = unreal.GameplayStatics.get_default_object()
    actor = gameplay_statics.call_method(
        "BeginDeferredActorSpawnFromClass",
        (
            world,
            actor_class,
            transform,
            unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,
            None,
            unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT,
        ),
    )
    if not actor:
        raise RuntimeError("BeginDeferredActorSpawnFromClass returned no actor")
    actor = gameplay_statics.call_method(
        "FinishSpawningActor",
        (actor, transform, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT),
    )
    if not actor:
        raise RuntimeError("FinishSpawningActor returned no actor")
    return actor


def spawn_static_mesh_actor(
    label,
    mesh,
    location,
    scale,
    material,
    rotation=(0.0, 0.0, 0.0),
    folder="RokUnityFreshCity",
):
    """Spawn a StaticMeshActor without the editor-only asset placement subsystem."""
    actor = spawn_actor_from_class_direct(
        unreal.StaticMeshActor,
        unreal.Vector(location[0], location[1], location[2]),
        proto.make_rotator(rotation[0], rotation[1], rotation[2]),
    )
    if not actor:
        raise RuntimeError("Failed to spawn StaticMeshActor: {0}".format(label))
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        raise RuntimeError("Spawned StaticMeshActor has no StaticMeshComponent: {0}".format(label))
    component.set_static_mesh(mesh)
    actor.set_actor_label(label)
    actor.set_actor_scale3d(unreal.Vector(scale[0], scale[1], scale[2]))
    actor.set_folder_path(folder)
    proto.assign_material(actor, material)
    for prop, value in [
        ("cast_shadow", False),
        ("receive_decals", False),
        ("use_as_occluder", False),
        ("collision_enabled", unreal.CollisionEnabled.NO_COLLISION),
    ]:
        try:
            component.set_editor_property(prop, value)
        except Exception:
            pass
    return actor


def load_initial_city_manifest():
    path = str(DATA_ROOT / "unity_initial_city_state.json")
    if not os.path.exists(path):
        raise RuntimeError("Missing generated initial city state: {0}".format(path))
    with open(path, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    if manifest.get("schema_version") != 1:
        raise RuntimeError("Unexpected initial city state schema: {0}".format(manifest.get("schema_version")))
    return manifest


def unity_yaw_rotate(x, z, yaw_degrees):
    radians = math.radians(yaw_degrees)
    cos_yaw = math.cos(radians)
    sin_yaw = math.sin(radians)
    return x * cos_yaw - z * sin_yaw, x * sin_yaw + z * cos_yaw


def unity_local_for_building(building):
    type_id = int(building.get("type_id", 0))
    grid = building.get("grid", {})
    x_logic = float(grid.get("x", 0.0))
    y_logic = float(grid.get("y", 0.0))
    if type_id in (6, 8):
        return 0.0, 0.0

    level_config = building.get("level_config", {})
    width = max(1.0, float(level_config.get("width", 1.0)))
    length = max(1.0, float(level_config.get("length", 1.0)))
    x = x_logic * UNITY_GRID_SIZE + width * UNITY_GRID_SIZE_HALF - UNITY_GRID_SIZE_HALF
    z = y_logic * UNITY_GRID_SIZE + length * UNITY_GRID_SIZE_HALF - UNITY_GRID_SIZE_HALF
    return x, z


def unity_city_local_to_ue(x, z):
    rotated_x, rotated_z = unity_yaw_rotate(x, z, UNITY_CITY_CONTAINER_YAW)
    return -rotated_x * UE_UNITS_PER_UNITY_UNIT, rotated_z * UE_UNITS_PER_UNITY_UNIT


def import_textured_ground_material(key, material_name):
    path = GROUND_TEXTURES.get(key)
    if not path or not os.path.exists(path):
        return None
    texture = runtime.import_runtime_texture_as(path, "UnityFreshCity_{0}".format(key))
    if not texture:
        return None
    return runtime.create_textured_surface_material(texture, material_name)


def spawn_ground(primitives, fallback_materials):
    plane = primitives["plane"]
    outer_material = import_textured_ground_material("outer", "M_UnityFreshCity_OuterGround")
    city_material = import_textured_ground_material("city", "M_UnityFreshCity_CityGround")
    road_material = import_textured_ground_material("road", "M_UnityFreshCity_DirtRoad")
    if not outer_material:
        outer_material = fallback_materials["M_Rok_Grass_Blockout"]
    if not city_material:
        city_material = fallback_materials["M_Rok_Plaza_Blockout"]
    if not road_material:
        road_material = fallback_materials["M_Rok_Path_Blockout"]

    spawned = []
    spawned.append(proto.spawn_mesh(
        "UnityFreshCity_OuterGround_Terrain_grass_08_3g2",
        plane,
        (0.0, 0.0, -55.0),
        (70.0, 70.0, 1.0),
        outer_material,
        (0.0, 0.0, 0.0),
        "RokUnityFreshCity/Ground",
    ))
    spawned.append(proto.spawn_mesh(
        "UnityFreshCity_InCityGround_map_incity4",
        plane,
        (0.0, 0.0, -36.0),
        (38.0, 38.0, 1.0),
        city_material,
        (0.0, -45.0, 0.0),
        "RokUnityFreshCity/Ground",
    ))
    for label, location, scale, yaw in [
        ("MainRoad_NE", (0.0, 0.0, -24.0), (36.0, 1.05, 1.0), 45.0),
        ("MainRoad_NW", (0.0, 0.0, -23.0), (36.0, 1.05, 1.0), -45.0),
        ("InnerRoad_EW", (0.0, 380.0, -22.0), (20.0, 0.62, 1.0), 0.0),
        ("InnerRoad_NS", (360.0, -160.0, -22.0), (18.0, 0.58, 1.0), 90.0),
    ]:
        spawned.append(proto.spawn_mesh(
            "UnityFreshCity_Road_{0}".format(label),
            plane,
            location,
            scale,
            road_material,
            (0.0, yaw, 0.0),
            "RokUnityFreshCity/Roads",
        ))

    for actor in spawned:
        set_tags(actor, [CITY_TAG, "RokUnityFreshGround"])
    return len(spawned)


def retag_boundary_actors():
    updated = 0
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    for actor in actors:
        tags = [str(tag) for tag in list(actor.tags)]
        if "RokUnityCityBoundaryPrefabSource" not in tags:
            continue
        actor.set_folder_path("RokUnityFreshCity/Boundary")
        actor.set_actor_label(actor.get_actor_label().replace("RokUnity_", "UnityFreshCity_"))
        set_tags(actor, [CITY_TAG, "RokUnityFreshBoundarySource"])
        updated += 1
    return updated


def unity_to_ue_exact(position):
    return unreal.Vector(
        -float(position[0]) * UE_UNITS_PER_UNITY_UNIT,
        float(position[2]) * UE_UNITS_PER_UNITY_UNIT,
        float(position[1]) * UE_UNITS_PER_UNITY_UNIT,
    )


def unity_yaw_quaternion(degrees):
    radians = math.radians(float(degrees)) * 0.5
    return (0.0, math.sin(radians), 0.0, math.cos(radians))


def find_prefab_root_transform_id(prefab):
    transforms = prefab.get("transforms", {})
    for transform_id, transform in transforms.items():
        if transform.get("father") == "0" or transform.get("father") not in transforms:
            return transform_id
    return None


def camera_dxf_from_unity_config(camera_id="city"):
    table_roots = [
        os.path.join(ROK_UNITY_ROOT, "config-tables"),
        os.path.join(ROK_UNITY_ROOT, "config-tables", "配置客户端数据表"),
    ]
    for table_root in table_roots:
        for root, _dirs, files in os.walk(table_root):
            if "CameraParamLod.csv" not in files:
                continue
            path = os.path.join(root, "CameraParamLod.csv")
            try:
                with open(path, "r", encoding="utf-8-sig", errors="ignore") as handle:
                    rows = [line.strip().strip('"').split('","') for line in handle if line.strip()]
            except OSError:
                continue
            if len(rows) < 3:
                continue
            header = rows[0]
            for row in rows[2:]:
                if len(row) != len(header):
                    continue
                record = dict(zip(header, row))
                if record.get("ID") == camera_id:
                    try:
                        return float(record.get("dxf", ""))
                    except ValueError:
                        pass
    return DEFAULT_UNITY_CITY_DXF


def city_runtime_lod_scale():
    dxf = float(os.environ.get("ROK_UNITY_FRESH_CITY_DXF", camera_dxf_from_unity_config("city")))
    # CityBuildingContainer.prefab LevelDetailCastle m_scale_curve:
    # 250->1, 1500->8, 2250->8, 2999.999->8, 3000->0.
    if dxf <= 250.0:
        scale = 1.0
    elif dxf < 1500.0:
        scale = 1.0 + (dxf - 250.0) * ((8.0 - 1.0) / (1500.0 - 250.0))
    elif dxf < 3000.0:
        scale = 8.0
    else:
        scale = 0.0

    city_age = int(float(os.environ.get("ROK_UNITY_FRESH_CITY_AGE", "1")))
    city_size = float(UNITY_CITY_SIZE_BY_AGE.get(city_age, UNITY_CITY_SIZE_BY_AGE[1]))
    max_city_size = float(max(UNITY_CITY_SIZE_BY_AGE.values()))
    max_scale = 7.8 * city_size / max_city_size
    return max(0.0, min(scale, max_scale)), dxf, max_scale


def apply_unity_city_container_runtime_transform(prefab, transform, runtime_scale):
    position, rotation, scale = transform
    root_transform_id = find_prefab_root_transform_id(prefab)
    root_position = (0.0, 0.0, 0.0)
    if root_transform_id:
        root_position = prefab["world_transform"](root_transform_id)[0]

    scaled_relative = tuple((float(position[i]) - float(root_position[i])) * runtime_scale for i in range(3))
    scaled_position = tuple(float(root_position[i]) + scaled_relative[i] for i in range(3))
    container_rotation = unity_yaw_quaternion(UNITY_CITY_CONTAINER_YAW)
    runtime_position = source3d.quat_rotate(container_rotation, scaled_position)
    runtime_rotation = source3d.quat_multiply(container_rotation, rotation)
    runtime_scale_vec = tuple(float(value) * runtime_scale for value in scale)
    return runtime_position, runtime_rotation, runtime_scale_vec


def boundary_mesh_stem(source_path):
    return os.path.splitext(os.path.basename(source_path or ""))[0]


def is_tower_source_mesh(source_path):
    return boundary_mesh_stem(source_path).startswith("CityTower_101_1")


def boundary_mesh_import_scale_multiplier(source_path):
    if is_tower_source_mesh(source_path):
        return float(os.environ.get("ROK_UNITY_FRESH_TOWER_MESH_SCALE_MULT", "0.1"))
    return 1.0


def should_normalize_tower_boundary_bounds():
    return os.environ.get("ROK_UNITY_FRESH_TOWER_NORMALIZE_BOUNDS", "0") == "1"


def align_actor_visible_bounds_to_pivot(actor, pivot_location, align_bottom=True):
    visible_origin, visible_extent = actor.get_actor_bounds(False)
    desired_origin = unreal.Vector(
        pivot_location.x,
        pivot_location.y,
        pivot_location.z + visible_extent.z if align_bottom else visible_origin.z,
    )
    actor.set_actor_location(actor.get_actor_location() + (desired_origin - visible_origin), False, False)


def spawn_exact_boundary_prefab_renderer(label, source_path, transform, material, prefab_path, object_name):
    mesh = source3d.import_rok_unity_boundary_mesh(source_path)
    if not mesh:
        return None

    position, rotation, scale = transform
    pivot_location = unity_to_ue_exact(position)
    pitch, yaw, roll = source3d.unity_quaternion_to_ue_rotator(rotation)
    fbx_centimeters = UE_UNITS_PER_UNITY_UNIT / 100.0
    mesh_scale_multiplier = boundary_mesh_import_scale_multiplier(source_path)
    actor_scale = (
        float(scale[0]) * fbx_centimeters * mesh_scale_multiplier,
        float(scale[1]) * fbx_centimeters * mesh_scale_multiplier,
        float(scale[2]) * fbx_centimeters * mesh_scale_multiplier,
    )
    actor = proto.spawn_mesh(
        label,
        mesh,
        (pivot_location.x, pivot_location.y, pivot_location.z),
        actor_scale,
        material,
        (pitch, yaw, roll),
        "RokUnityFreshCity/Boundary",
    )
    stem = boundary_mesh_stem(source_path)
    tower_bounds_normalized = is_tower_source_mesh(source_path) and should_normalize_tower_boundary_bounds()
    if tower_bounds_normalized:
        align_actor_visible_bounds_to_pivot(actor, pivot_location, align_bottom=True)
    actor.tags = [
        "RokTestUnityLayout",
        "UnityWallSegment",
        "RokUnityCityBoundaryPrefabSource",
        "RokUnityBoundaryExactPrefabTransform",
        "UnityRuntimeGridScaleApplied",
        "UnityCityContainerYawApplied",
        "UnityMeshImportScale_{0:g}".format(mesh_scale_multiplier),
        "UnityMesh_{0}".format(stem),
        "UnitySourcePrefab_{0}".format(os.path.splitext(os.path.basename(prefab_path))[0]),
        "UnityPrefabObject_{0}".format(safe_tag(object_name)),
    ]
    return actor


def spawn_boundary_from_rok_unity_prefabs():
    materials = source3d.import_rok_unity_boundary_materials()
    spawned = 0
    records = []
    runtime_scale, runtime_dxf, runtime_max_scale = city_runtime_lod_scale()
    for prefab_path in source3d.ROK_UNITY_BOUNDARY_PREFABS:
        if not os.path.exists(prefab_path):
            unreal.log_warning("Missing Rok-unity city boundary prefab: {0}".format(prefab_path))
            continue

        prefab = source3d.parse_unity_prefab(prefab_path)
        for game_object_id, mesh_guid in prefab["mesh_filters"].items():
            object_info = prefab["objects"].get(game_object_id, {})
            renderer = prefab["mesh_renderers"].get(game_object_id)
            transform_id = prefab["transform_by_game_object"].get(game_object_id)
            source_path = source3d.unity_guid_source_path(mesh_guid)
            if (
                not object_info.get("active", False)
                or not renderer
                or not renderer["enabled"]
                or not transform_id
                or not source3d.is_boundary_source_mesh(source_path)
            ):
                continue

            prefab_transform = prefab["world_transform"](transform_id)
            transform = apply_unity_city_container_runtime_transform(prefab, prefab_transform, runtime_scale)
            object_name = object_info.get("name", "Boundary")
            stem = os.path.splitext(os.path.basename(source_path))[0]
            label = "UnityFreshCity_{0}_{1}_{2}".format(
                os.path.splitext(os.path.basename(prefab_path))[0],
                safe_tag(object_name),
                spawned,
            )
            actor = spawn_exact_boundary_prefab_renderer(
                label,
                source_path,
                transform,
                source3d.boundary_material_for_source(source_path, materials),
                prefab_path,
                object_name,
            )
            if not actor:
                continue

            set_tags(actor, [CITY_TAG, "RokUnityFreshBoundarySource"])
            actor.set_folder_path("RokUnityFreshCity/Boundary")
            position, rotation, scale = transform
            bounds_origin, bounds_extent = actor.get_actor_bounds(False)
            records.append({
                "actor": actor.get_actor_label(),
                "source_prefab": prefab_path,
                "unity_object": object_name,
                "source_mesh": source_path,
                "source_mesh_stem": stem,
                "unity_prefab_position": list(prefab_transform[0]),
                "unity_prefab_rotation_quaternion": list(prefab_transform[1]),
                "unity_prefab_scale": list(prefab_transform[2]),
                "unity_world_position": list(position),
                "unity_world_rotation_quaternion": list(rotation),
                "unity_world_scale": list(scale),
                "unity_city_container_yaw": UNITY_CITY_CONTAINER_YAW,
                "unity_runtime_lod_dxf": runtime_dxf,
                "unity_runtime_lod_scale": runtime_scale,
                "unity_runtime_max_scale": runtime_max_scale,
                "ue_mesh_import_scale_multiplier": boundary_mesh_import_scale_multiplier(source_path),
                "ue_tower_bounds_normalized_to_pivot": is_tower_source_mesh(source_path) and should_normalize_tower_boundary_bounds(),
                "ue_location": [float(actor.get_actor_location().x), float(actor.get_actor_location().y), float(actor.get_actor_location().z)],
                "ue_rotation": [float(actor.get_actor_rotation().pitch), float(actor.get_actor_rotation().yaw), float(actor.get_actor_rotation().roll)],
                "ue_scale": [float(actor.get_actor_scale3d().x), float(actor.get_actor_scale3d().y), float(actor.get_actor_scale3d().z)],
                "ue_visible_bounds_origin": [float(bounds_origin.x), float(bounds_origin.y), float(bounds_origin.z)],
                "ue_visible_bounds_extent": [float(bounds_extent.x), float(bounds_extent.y), float(bounds_extent.z)],
                "mesh_bounds_compensation": "none; actor transform is converted Unity runtime transform",
            })
            spawned += 1
            unreal.log("Spawned exact Rok-unity boundary mesh {0}/{1} from {2}".format(stem, object_name, prefab_path))

    return spawned, retag_boundary_actors(), records


def boundary_asset_review_sources():
    fbx_root = os.path.join(ROK_UNITY_ASSETS, "BundleAssets", "Map", "FBX")
    sources = [
        ("CityWall_101_1", os.path.join(fbx_root, "CityWall_101_1.FBX")),
        ("CityGate_101_1", os.path.join(fbx_root, "CityGate_101_1.FBX")),
    ]
    for index in range(1, 9):
        stem = "CityTower_101_1_{0}".format(index)
        sources.append((stem, os.path.join(fbx_root, "{0}.FBX".format(stem))))
    return sources


def spawn_boundary_asset_review(materials):
    spawned = 0
    records = []
    folder = "RokUnityFreshCity/AssetReview/Boundary"
    start_x = -4200.0
    start_y = 4200.0
    step_x = 780.0
    step_y = 820.0
    columns = 5
    review_scale = float(os.environ.get("ROK_UNITY_FRESH_BOUNDARY_REVIEW_SCALE", "0.045"))
    for index, (stem, source_path) in enumerate(boundary_asset_review_sources()):
        if not os.path.exists(source_path):
            unreal.log_warning("Missing boundary review source: {0}".format(source_path))
            continue
        mesh = source3d.import_rok_unity_boundary_mesh(source_path)
        if not mesh:
            unreal.log_warning("Failed to import boundary review mesh: {0}".format(source_path))
            continue
        row = int(index / columns)
        column = index % columns
        anchor = unreal.Vector(start_x + column * step_x, start_y - row * step_y, 0.0)
        scale_multiplier = boundary_mesh_import_scale_multiplier(source_path)
        actor_scale = review_scale * scale_multiplier
        label = "UnityFreshCity_AssetReview_{0}".format(stem)
        actor = proto.spawn_mesh(
            label,
            mesh,
            (anchor.x, anchor.y, anchor.z),
            (actor_scale, actor_scale, actor_scale),
            source3d.boundary_material_for_source(source_path, materials),
            (0.0, 0.0, 90.0),
            folder,
        )
        align_actor_visible_bounds_to_pivot(actor, anchor, align_bottom=True)
        tags = [
            CITY_TAG,
            "RokUnityFreshAssetReview",
            "UnityBoundaryAssetReview",
            "UnityMesh_{0}".format(stem),
            "UnitySourceFBX_{0}".format(stem),
            "UnityMeshImportScale_{0:g}".format(scale_multiplier),
        ]
        if stem.startswith("CityTower_101_1_"):
            tags.append("UnityTowerAssetReview")
        set_tags(actor, tags)
        actor.set_folder_path(folder)
        bounds_origin, bounds_extent = actor.get_actor_bounds(False)
        records.append({
            "actor": actor.get_actor_label(),
            "source_mesh": source_path,
            "source_mesh_stem": stem,
            "ue_location": [float(actor.get_actor_location().x), float(actor.get_actor_location().y), float(actor.get_actor_location().z)],
            "ue_scale": [float(actor.get_actor_scale3d().x), float(actor.get_actor_scale3d().y), float(actor.get_actor_scale3d().z)],
            "ue_visible_bounds_origin": [float(bounds_origin.x), float(bounds_origin.y), float(bounds_origin.z)],
            "ue_visible_bounds_extent": [float(bounds_extent.x), float(bounds_extent.y), float(bounds_extent.z)],
            "purpose": "all migrated boundary FBX assets displayed for source-asset inspection",
        })
        spawned += 1
    return spawned, records


def sprite_paths(sprite_name):
    color = os.path.join(gameapp.UNITY_BUILDING_ROOT, "{0}.png".format(sprite_name))
    mask = os.path.join(gameapp.UNITY_BUILDING_ROOT, "{0}_mask.png".format(sprite_name))
    if not os.path.exists(color):
        return None, None
    return color, mask if os.path.exists(mask) else None


def build_guid_asset_index():
    global GUID_ASSET_INDEX
    if GUID_ASSET_INDEX is not None:
        return GUID_ASSET_INDEX

    index = {}
    guid_pattern = re.compile(r"^guid:\s*([0-9a-fA-F]{32})\s*$", re.MULTILINE)
    for root, _dirs, files in os.walk(ROK_UNITY_ASSETS):
        for name in files:
            if not name.endswith(".meta"):
                continue
            meta_path = os.path.join(root, name)
            try:
                text = read_text(meta_path)
            except OSError:
                continue
            match = guid_pattern.search(text)
            if not match:
                continue
            asset_path = meta_path[:-5]
            if os.path.exists(asset_path):
                index[match.group(1).lower()] = asset_path

    GUID_ASSET_INDEX = index
    unreal.log("Indexed {0} Rok-unity asset GUIDs for fresh city prefab sprite resolution.".format(len(index)))
    return GUID_ASSET_INDEX


def parse_unity_vector(text, key):
    match = re.search(
        r"{0}:\s*\{{x:\s*([^,}}]+),\s*y:\s*([^,}}]+),\s*z:\s*([^,}}]+)\}}".format(re.escape(key)),
        text,
    )
    if not match:
        return {"x": 0.0, "y": 0.0, "z": 0.0}
    return {"x": float(match.group(1)), "y": float(match.group(2)), "z": float(match.group(3))}


def parse_unity_prefab_blocks(prefab_path):
    text = read_text(prefab_path)
    parts = re.split(r"\n--- !u!(\d+) &(-?\d+)\n", "\n" + text)
    blocks = []
    for index in range(1, len(parts), 3):
        blocks.append((parts[index], parts[index + 1], parts[index + 2]))
    return blocks


def resolve_prefab_sprite_source(sprite_name):
    if sprite_name in PREFAB_SPRITE_CACHE:
        return PREFAB_SPRITE_CACHE[sprite_name]

    prefab_path = os.path.join(ROK_UNITY_CITY_BUILDING_PREFAB_ROOT, "{0}.prefab".format(sprite_name))
    if not os.path.exists(prefab_path):
        PREFAB_SPRITE_CACHE[sprite_name] = None
        return None

    guid_index = build_guid_asset_index()
    game_objects = {}
    transforms = {}
    masks_by_game_object = {}
    renderers = []

    for type_id, file_id, block in parse_unity_prefab_blocks(prefab_path):
        if type_id == "1":
            name_match = re.search(r"m_Name:\s*(.*)", block)
            active_match = re.search(r"m_IsActive:\s*([01])", block)
            game_objects[file_id] = {
                "name": name_match.group(1).strip() if name_match else "",
                "active": active_match.group(1) != "0" if active_match else True,
            }
        elif type_id == "4":
            go_match = re.search(r"m_GameObject:\s*\{fileID:\s*(-?\d+)\}", block)
            if go_match:
                transforms[go_match.group(1)] = {
                    "position": parse_unity_vector(block, "m_LocalPosition"),
                    "scale": parse_unity_vector(block, "m_LocalScale"),
                    "euler": parse_unity_vector(block, "m_LocalEulerAnglesHint"),
                }
        elif type_id == "212":
            go_match = re.search(r"m_GameObject:\s*\{fileID:\s*(-?\d+)\}", block)
            sprite_match = re.search(r"m_Sprite:\s*\{fileID:\s*[^,]+,\s*guid:\s*([0-9a-fA-F]{32}),\s*type:\s*3\}", block)
            enabled_match = re.search(r"m_Enabled:\s*([01])", block)
            if go_match and sprite_match:
                renderers.append({
                    "game_object": go_match.group(1),
                    "sprite_guid": sprite_match.group(1).lower(),
                    "enabled": enabled_match.group(1) != "0" if enabled_match else True,
                })
        elif type_id == "114":
            go_match = re.search(r"m_GameObject:\s*\{fileID:\s*(-?\d+)\}", block)
            mask_match = re.search(r"m_mask_sprite:\s*\{fileID:\s*[^,]+,\s*guid:\s*([0-9a-fA-F]{32}),\s*type:\s*3\}", block)
            if go_match and mask_match:
                masks_by_game_object[go_match.group(1)] = mask_match.group(1).lower()

    candidates = []
    for renderer in renderers:
        game_object = game_objects.get(renderer["game_object"], {})
        name = game_object.get("name", "")
        normalized = name.lower()
        if not renderer["enabled"] or not game_object.get("active", True):
            continue
        if "delete" in normalized or "upgrade" in normalized or "board" in normalized:
            continue

        color_path = guid_index.get(renderer["sprite_guid"])
        if not color_path:
            continue
        if os.path.splitext(color_path)[1].lower() not in (".png", ".tga", ".tif", ".tiff", ".psd"):
            continue

        mask_guid = masks_by_game_object.get(renderer["game_object"])
        mask_path = guid_index.get(mask_guid) if mask_guid else None
        transform = transforms.get(renderer["game_object"], {})
        score = 100 if normalized == "sprite" else 0
        score += 25 if os.path.basename(color_path).startswith("build_") else 0
        score += 5 if mask_path else 0
        candidates.append({
            "score": score,
            "prefab_path": prefab_path,
            "renderer_name": name,
            "color_path": color_path,
            "mask_path": mask_path,
            "sprite_guid": renderer["sprite_guid"],
            "mask_guid": mask_guid or "",
            "local_transform": transform,
        })

    candidates.sort(key=lambda item: item["score"], reverse=True)
    resolved = candidates[0] if candidates else None
    if resolved:
        unreal.log("Resolved prefab sprite {0} -> {1}".format(sprite_name, resolved["color_path"]))
    PREFAB_SPRITE_CACHE[sprite_name] = resolved
    return resolved


def resolve_building_visual(sprite_name):
    color_path, mask_path = sprite_paths(sprite_name)
    if color_path:
        return {
            "resolution": "direct_png",
            "source_sprite_name": sprite_name,
            "color_path": color_path,
            "mask_path": mask_path,
            "prefab_path": "",
            "renderer_name": "",
            "sprite_guid": "",
            "mask_guid": "",
        }

    prefab_resolution = resolve_prefab_sprite_source(sprite_name)
    if not prefab_resolution:
        return None

    source_name = os.path.splitext(os.path.basename(prefab_resolution["color_path"]))[0]
    return {
        "resolution": "prefab_sprite_renderer",
        "source_sprite_name": source_name,
        "color_path": prefab_resolution["color_path"],
        "mask_path": prefab_resolution.get("mask_path"),
        "prefab_path": prefab_resolution["prefab_path"],
        "renderer_name": prefab_resolution.get("renderer_name", ""),
        "sprite_guid": prefab_resolution.get("sprite_guid", ""),
        "mask_guid": prefab_resolution.get("mask_guid", ""),
    }


def spawn_init_building(building, index):
    type_id = int(building.get("type_id", 0))
    if type_id in (6, 8):
        return None, "represented_by_boundary_prefab"

    visual = building.get("visual", {})
    sprite_name = visual.get("sprite_name") or visual.get("model_id")
    if not sprite_name:
        return None, "missing_sprite_name"

    visual_source = resolve_building_visual(sprite_name)
    if not visual_source:
        return None, "missing_sprite_source:{0}".format(sprite_name)

    color_path = visual_source["color_path"]
    mask_path = visual_source.get("mask_path")
    texture = runtime.import_runtime_texture_as(color_path, "UnityFreshCity_{0}".format(sprite_name))
    if not texture:
        return None, "texture_import_failed:{0}".format(sprite_name)
    material = runtime.create_runtime_city_sprite_material(
        texture,
        "UnityFreshCity_{0}".format(sprite_name),
        mask_path,
        False,
        False,
        "R",
        0.32,
    )
    tex_width, tex_height = runtime.texture_dimensions(texture)
    level_config = building.get("level_config", {})
    footprint_width = max(1.0, float(level_config.get("width", 3.0))) * UNITY_GRID_SIZE * UE_UNITS_PER_UNITY_UNIT
    footprint_length = max(1.0, float(level_config.get("length", 3.0))) * UNITY_GRID_SIZE * UE_UNITS_PER_UNITY_UNIT
    visual_width = max(165.0, max(footprint_width, footprint_length) * 1.55) * CITY_SPRITE_SCALE
    if type_id == 1:
        visual_width *= 1.28
    visual_height = visual_width * float(tex_height) / max(1.0, float(tex_width))
    local_x, local_z = unity_local_for_building(building)
    ue_x, ue_y = unity_city_local_to_ue(local_x, local_z)
    sort_priority = int((ue_y + 4500.0) * 10.0) + index
    actor = runtime.spawn_sprite_actor(
        "UnityFreshCity_Init_{0:03d}_{1}_{2}".format(building.get("init_id", 0), building.get("ue_type", "Building"), sprite_name),
        material,
        ue_x,
        ue_y,
        visual_width,
        visual_height,
        sort_priority,
        "RokUnityFreshCity/Buildings",
        [
            CITY_TAG,
            "RokBuilding",
            "CityBuilding",
            "RokUnityFreshInitBuilding",
            "RokUnityConfigInitBuilding",
            "UnityInitId_{0}".format(building.get("init_id", 0)),
            "UnityBuildingType_{0}".format(type_id),
            "UnityBuildingLevel_{0}".format(building.get("level", 1)),
            "UnityGridX_{0}".format(building.get("grid", {}).get("x", 0)),
            "UnityGridY_{0}".format(building.get("grid", {}).get("y", 0)),
            "UnitySprite_{0}".format(safe_tag(sprite_name)),
            "UnityVisualResolution_{0}".format(visual_source["resolution"]),
            "UnityVisualSourceSprite_{0}".format(safe_tag(visual_source["source_sprite_name"])),
            "UnityRuntimePlacement_ConvertCityObjTileToLocal",
        ],
        True,
        (int(level_config.get("width", 1)), int(level_config.get("length", 1))),
        (int(building.get("grid", {}).get("x", 0)), int(building.get("grid", {}).get("y", 0))),
        z_override=0.0,
        anchor_mode="bottom",
    )
    extra_tags = []
    if visual_source.get("prefab_path"):
        extra_tags.append("UnitySourcePrefab_{0}".format(safe_tag(os.path.splitext(os.path.basename(visual_source["prefab_path"]))[0])))
    if visual_source.get("renderer_name"):
        extra_tags.append("UnityPrefabSpriteRenderer_{0}".format(safe_tag(visual_source["renderer_name"])))
    if visual_source.get("sprite_guid"):
        extra_tags.append("UnitySpriteGuid_{0}".format(visual_source["sprite_guid"]))
    if visual_source.get("mask_guid"):
        extra_tags.append("UnityMaskGuid_{0}".format(visual_source["mask_guid"]))
    if extra_tags:
        set_tags(actor, extra_tags)
    return actor, "spawned"


def spawn_initial_city_buildings(manifest):
    spawned = 0
    skipped = []
    for index, building in enumerate(manifest.get("buildings", [])):
        actor, status = spawn_init_building(building, index)
        if actor:
            spawned += 1
        else:
            skipped.append({
                "init_id": building.get("init_id"),
                "type_id": building.get("type_id"),
                "ue_type": building.get("ue_type"),
                "status": status,
            })
    return spawned, skipped


def spawn_runtime_chain_markers():
    root = spawn_actor_from_class_direct(
        unreal.StaticMeshActor,
        unreal.Vector(0.0, 0.0, 0.0),
        proto.make_rotator(0.0, 0.0, 0.0),
    )
    root.set_actor_label("UnityFreshCity_SourceChain_GameApp_Runtime")
    root.set_folder_path("RokUnityFreshCity/SourceChain")
    set_tags(root, [
        CITY_TAG,
        "UnityRuntimeChain_GameAppScene",
        "UnityRuntimeChain_SwitchMainCityCmd_map_4_data_tile_data",
        "UnityRuntimeChain_CityGlobalMediator_CityBuildingContainer",
        "UnityRuntimeChain_ConvertCityObjTileToLocal_YawMinus45",
        "UnitySource_RokUnityRoot_{0}".format(safe_tag(ROK_UNITY_ROOT)),
    ])
    return root


def camera_forward_ue():
    x, y, z = CAMERA_FORWARD_UNITY
    length = math.sqrt(x * x + y * y + z * z)
    if length < 0.0001:
        return unreal.Vector(0.0, 0.70710678, -0.70710678)
    return unreal.Vector(x / length, z / length, y / length)


def unity_planar_to_ue(x, z, y=0.0):
    return unreal.Vector(-x * UE_UNITS_PER_UNITY_UNIT, z * UE_UNITS_PER_UNITY_UNIT, y * UE_UNITS_PER_UNITY_UNIT)


def look_rotation_from_forward(forward):
    try:
        return unreal.MathLibrary.make_rot_from_x(forward)
    except Exception:
        return unreal.KismetMathLibrary.make_rot_from_x(forward)


def spawn_camera_and_player_start():
    center = unity_planar_to_ue(CAMERA_VIEW_CENTER[0], CAMERA_VIEW_CENTER[1], 0.0)
    forward = camera_forward_ue()
    location = center - forward * (CAMERA_VIEW_DISTANCE * UE_UNITS_PER_UNITY_UNIT)
    camera = spawn_actor_from_class_direct(unreal.CameraActor, location, look_rotation_from_forward(forward))
    camera.set_actor_label("Camera_RokUnityFreshCity")
    camera.set_folder_path("RokUnityFreshCity/Camera")
    set_tags(camera, [CITY_TAG, "RokUnityFreshCamera"])
    component = camera.get_component_by_class(unreal.CameraComponent)
    if component:
        component.set_editor_property("projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
        horizontal_fov = 2.0 * math.degrees(math.atan(math.tan(math.radians(CAMERA_VERTICAL_FOV) * 0.5) * CAMERA_ASPECT))
        component.set_editor_property("field_of_view", horizontal_fov)
        component.set_editor_property("aspect_ratio", CAMERA_ASPECT)
        try:
            pp = component.get_editor_property("post_process_settings")
            pp.set_editor_property("override_auto_exposure_method", True)
            pp.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_BASIC)
            pp.set_editor_property("override_auto_exposure_min_brightness", True)
            pp.set_editor_property("auto_exposure_min_brightness", 1.0)
            pp.set_editor_property("override_auto_exposure_max_brightness", True)
            pp.set_editor_property("auto_exposure_max_brightness", 1.0)
            pp.set_editor_property("override_auto_exposure_bias", True)
            pp.set_editor_property("auto_exposure_bias", 0.0)
            component.set_editor_property("post_process_settings", pp)
        except Exception as error:
            unreal.log_warning("Skipping fresh city camera post-process setup: {0}".format(error))

    player_start = spawn_actor_from_class_direct(
        unreal.PlayerStart,
        center,
        proto.make_rotator(0.0, 0.0, 0.0),
    )
    player_start.set_actor_label("PlayerStart_RokUnityFreshCity")
    player_start.set_folder_path("RokUnityFreshCity/Camera")
    set_tags(player_start, [CITY_TAG, "RokUnityFreshPlayerStart"])
    return camera, player_start


def is_target_level_loaded():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        return False
    return str(world.get_outermost().get_name()) == TARGET_LEVEL


def create_or_load_target_map():
    if is_target_level_loaded():
        return
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    elif not unreal.EditorLevelLibrary.new_level(TARGET_LEVEL):
        raise RuntimeError("Failed to create target map: {0}".format(TARGET_LEVEL))


def clear_current_level():
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        unreal.EditorLevelLibrary.destroy_actor(actor)


def write_scene_manifest(summary):
    os.makedirs(os.path.dirname(SCENE_MANIFEST), exist_ok=True)
    with open(SCENE_MANIFEST, "w", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)


def build_scene():
    require_sources()
    manifest = load_initial_city_manifest()

    create_or_load_target_map()
    clear_current_level()

    camera_target = unity_planar_to_ue(CAMERA_VIEW_CENTER[0], CAMERA_VIEW_CENTER[1], 0.0)
    camera_location = camera_target - camera_forward_ue() * (CAMERA_VIEW_DISTANCE * UE_UNITS_PER_UNITY_UNIT)
    runtime.CAMERA_LOCATION = camera_location
    runtime.CAMERA_TARGET = camera_target
    runtime.SPRITE_PLANE_MODE = "camera"
    runtime.ENGINE_PLANE_FLIP_Y = True

    proto.ensure_project_primitive_meshes()
    primitives = {name: proto.load_primitive(name) for name in proto.PRIMITIVE_PATHS}
    fallback_materials = runtime.make_runtime_materials()

    source_chain = spawn_runtime_chain_markers()
    ground_count = spawn_ground(primitives, fallback_materials)
    boundary_spawned, boundary_tagged, boundary_records = spawn_boundary_from_rok_unity_prefabs()
    boundary_review_count, boundary_review_records = spawn_boundary_asset_review(source3d.import_rok_unity_boundary_materials())
    building_count, skipped = spawn_initial_city_buildings(manifest)
    spawn_camera_and_player_start()
    runtime.configure_world_settings()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError("Failed to save fresh city map: {0}".format(TARGET_LEVEL))
    unreal.EditorAssetLibrary.save_asset(TARGET_LEVEL, only_if_is_dirty=False)

    summary = {
        "schema": "rok.unity_fresh_city_scene.v1",
        "target_level": TARGET_LEVEL,
        "rok_unity_root": ROK_UNITY_ROOT,
        "source_chain_actor": source_chain.get_actor_label(),
        "source_files": SOURCE_CHAIN,
        "unity_runtime_rules": {
            "city_container_parent": "SceneObject/land_root",
            "map_data": "map_4_data",
            "tile_data": "tile_data",
            "city_container_yaw": UNITY_CITY_CONTAINER_YAW,
            "grid_size": UNITY_GRID_SIZE,
            "special_zero_local_types": [6, 8],
            "city_wall_and_guard_tower_local_position": "Vector3.zero from ConvertCityObjTileToLocal",
            "boundary_root_scale": "GridCollideItem.SetScale(LevelDetailCastle scale from CameraParamLod city dxf)",
        },
        "boundary_transform_mode": "Unity runtime transform: prefab child transform, prefab root GridCollideItem scale, CityBuildingContainer Y=-45, then Unity-to-UE conversion; formal boundary actors keep Unity transform origin",
        "ground_actors": ground_count,
        "boundary_spawned": boundary_spawned,
        "boundary_tagged": boundary_tagged,
        "boundary_actors": boundary_records,
        "boundary_asset_review_count": boundary_review_count,
        "boundary_asset_review_actors": boundary_review_records,
        "tower_asset_review_count": sum(1 for record in boundary_review_records if record.get("source_mesh_stem", "").startswith("CityTower_101_1_")),
        "tower_asset_review_actors": [record for record in boundary_review_records if record.get("source_mesh_stem", "").startswith("CityTower_101_1_")],
        "init_building_actor_count": building_count,
        "initial_city_building_count": manifest.get("building_count"),
        "skipped_init_buildings": skipped,
    }
    write_scene_manifest(summary)
    unreal.log("Rok Unity fresh city map saved: {0}; buildings={1}; boundary={2}; asset_review={3}".format(
        TARGET_LEVEL,
        building_count,
        boundary_tagged,
        boundary_review_count,
    ))


if __name__ == "__main__":
    build_scene()
