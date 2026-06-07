import math
import os
import re
import sys
from collections import defaultdict

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokPrototypeScene as proto
import CreateRokRuntimeCityScene as runtime


TARGET_LEVEL = os.environ.get("ROK_TESTUNITY_TARGET_LEVEL", "/Game/Maps/L_RokUnityGameAppCity")
TESTUNITY_ROOT = os.environ.get("ROK_TESTUNITY_ROOT", r"D:\ueDemo\TestUnity")
TESTUNITY_ASSETS = os.path.join(TESTUNITY_ROOT, "Assets", "RokSource")
ROK_UNITY_ROOT = os.environ.get("ROK_UNITY_ROOT", r"D:\ueDemo\Rok-unity")
ROK_UNITY_CLIENT_ASSETS = os.path.join(ROK_UNITY_ROOT, "client-unity", "Assets")
FALLBACK_UNITY_ASSETS = os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets")
SCENE_PATH = os.path.join(TESTUNITY_ROOT, "Assets", "Scenes", "RokCity2_5D_Demo.unity")
TESTUNITY_FBX_ROOT = os.path.join(TESTUNITY_ASSETS, "BundleAssets", "Map", "FBX")
TESTUNITY_SHARED_MAP_MATERIAL_ROOT = os.path.join(
    TESTUNITY_ASSETS,
    "BundleAssets",
    "Map",
    "Materials",
    "shared_mapmaterials",
)
TESTUNITY_CITY_MESH_DESTINATION = "/Game/RokPrototype/Meshes/TestUnityCity"
ROK_UNITY_MAP_FBX_ROOT = os.path.join(ROK_UNITY_CLIENT_ASSETS, "BundleAssets", "Map", "FBX")
ROK_UNITY_SHARED_MAP_MATERIAL_ROOT = os.path.join(
    ROK_UNITY_CLIENT_ASSETS,
    "BundleAssets",
    "Map",
    "Materials",
    "shared_mapmaterials",
)
ROK_UNITY_BOUNDARY_PREFABS = [
    os.path.join(
        ROK_UNITY_CLIENT_ASSETS,
        "BundleAssets",
        "Scene",
        "City_building",
        "city_building_1",
        "build_6_101_1.prefab",
    ),
    os.path.join(
        ROK_UNITY_CLIENT_ASSETS,
        "BundleAssets",
        "Scene",
        "City_building",
        "city_building_1",
        "build_8_101_1.prefab",
    ),
]
ROK_UNITY_BOUNDARY_MESH_DESTINATION = "/Game/RokPrototype/Meshes/RokUnityCityBoundary"
ROK_UNITY_CITY_WALL_OBJ = os.path.join(
    ROK_UNITY_CLIENT_ASSETS,
    "BundleAssetsTest",
    "Env",
    "City",
    "CityWall_1_1.obj",
)
ROK_UNITY_CITY_WALL_TEXTURE = os.path.join(
    ROK_UNITY_CLIENT_ASSETS,
    "BundleAssetsTest",
    "Env",
    "City",
    "CityWall_1_1_c.png",
)
ROK_UNITY_CITY_WALL_MESH_DESTINATION = "/Game/RokPrototype/Meshes/RokUnityCityWall"

UE_UNITS_PER_UNITY_UNIT = float(os.environ.get("ROK_TESTUNITY_UE_SCALE", "500.0"))
TARGET_WALL_WIDTH_UNITY = float(os.environ.get("ROK_TESTUNITY_WALL_WIDTH", "6.0"))
SPRITE_VISUAL_SCALE = float(os.environ.get("ROK_TESTUNITY_SPRITE_VISUAL_SCALE", "0.55"))
CAMERA_VIEW_CENTER = (-0.38, -0.48)
CAMERA_FORWARD_UNITY = (0.0, -0.70710678, 0.70710678)
CAMERA_VIEW_DISTANCE = 24.25
CAMERA_VERTICAL_FOV = 7.5
CAMERA_ASPECT = 16.0 / 10.0

BUILDING_ROOT = os.path.join(
    "BundleAssets",
    "Map",
    "Building",
    "Building_use",
    "shared_building",
    "Building_101",
    "city_building_1",
)

CITY_FLAG_REL = os.path.join("BundleAssets", "Map", "Materials", "shared_texture", "city_flag.png")
CITY_FLAG_MASK_REL = os.path.join("BundleAssets", "Map", "Materials", "shared_texture", "city_flag_mask.png")
UNIT_REL = os.path.join("BundleAssets", "Troop", "sprite_troop", "Arms1_3", "Arms1_3_walk", "Arms1_3_walk.png")

TESTUNITY_WALL_MESH_SPECS = {
    "wall": ("CityWall_101_1.FBX", "SM_TestUnity_CityWall_101_1", True),
    "gate": ("CityGate_101_1.FBX", "SM_TestUnity_CityGate_101_1", False),
    "tower_1": ("CityTower_101_1_1.FBX", "SM_TestUnity_CityTower_101_1_1", False),
    "tower_2": ("CityTower_101_1_2.FBX", "SM_TestUnity_CityTower_101_1_2", False),
    "tower_3": ("CityTower_101_1_3.FBX", "SM_TestUnity_CityTower_101_1_3", False),
    "tower_4": ("CityTower_101_1_4.FBX", "SM_TestUnity_CityTower_101_1_4", False),
    "tower_5": ("CityTower_101_1_5.FBX", "SM_TestUnity_CityTower_101_1_5", False),
    "tower_7": ("CityTower_101_1_7.FBX", "SM_TestUnity_CityTower_101_1_7", False),
}

TESTUNITY_WALL_TEXTURE_SPECS = {
    "wall": ("CityWall_101_1_mat.PNG", "T_TestUnity_CityWall_101_1_mat", "M_TestUnity_CityWall_101_1_mat"),
    "gate": ("CityGate_101_1_mat.png", "T_TestUnity_CityGate_101_1_mat", "M_TestUnity_CityGate_101_1_mat"),
    "tower": ("CityTower_101_1_mat.png", "T_TestUnity_CityTower_101_1_mat", "M_TestUnity_CityTower_101_1_mat"),
}

ROK_UNITY_BOUNDARY_TEXTURE_SPECS = {
    "wall": ("CityWall_101_1_mat.PNG", "T_RokUnity_CityWall_101_1_mat", "M_RokUnity_CityWall_101_1_mat"),
    "gate": ("CityGate_101_1_mat.png", "T_RokUnity_CityGate_101_1_mat", "M_RokUnity_CityGate_101_1_mat"),
    "tower": ("CityTower_101_1_mat.png", "T_RokUnity_CityTower_101_1_mat", "M_RokUnity_CityTower_101_1_mat"),
}

_ROK_UNITY_GUID_CACHE = None
_TESTUNITY_FBX_GUID_CACHE = None


def source_path(relative_path):
    for root in [TESTUNITY_ASSETS, FALLBACK_UNITY_ASSETS]:
        path = os.path.join(root, relative_path)
        if os.path.exists(path):
            return path
    return None


def parse_vector3(text, default=(0.0, 0.0, 0.0)):
    match = re.search(r"\{x: ([^,]+), y: ([^,]+), z: ([^}]+)\}", text or "")
    return tuple(float(match.group(i)) for i in range(1, 4)) if match else default


def parse_quaternion(text):
    match = re.search(r"\{x: ([^,]+), y: ([^,]+), z: ([^,]+), w: ([^}]+)\}", text or "")
    return tuple(float(match.group(i)) for i in range(1, 5)) if match else (0.0, 0.0, 0.0, 1.0)


def multiply_vec(a, b):
    return tuple(a[i] * b[i] for i in range(3))


def add_vec(a, b):
    return tuple(a[i] + b[i] for i in range(3))


def scaled_vec(v, s):
    return tuple(v[i] * s for i in range(3))


def quat_multiply(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    )


def quat_rotate(q, v):
    x, y, z = v
    qv = (x, y, z, 0.0)
    qx, qy, qz, qw = q
    inverse = (-qx, -qy, -qz, qw)
    result = quat_multiply(quat_multiply(q, qv), inverse)
    return result[:3]


def quat_to_matrix(q):
    x, y, z, w = q
    length = math.sqrt(x * x + y * y + z * z + w * w)
    if length < 0.000001:
        return [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]
    x, y, z, w = x / length, y / length, z / length, w / length
    xx, yy, zz = x * x, y * y, z * z
    xy, xz, yz = x * y, x * z, y * z
    wx, wy, wz = w * x, w * y, w * z
    return [
        [1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz), 2.0 * (xz + wy)],
        [2.0 * (xy + wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx)],
        [2.0 * (xz - wy), 2.0 * (yz + wx), 1.0 - 2.0 * (xx + yy)],
    ]


def mat_mul(a, b):
    return [
        [sum(a[row][k] * b[k][col] for k in range(3)) for col in range(3)]
        for row in range(3)
    ]


def unity_quaternion_to_ue_rotator(q):
    unity_matrix = quat_to_matrix(q)
    basis = [[-1.0, 0.0, 0.0], [0.0, 0.0, 1.0], [0.0, 1.0, 0.0]]
    matrix = mat_mul(mat_mul(basis, unity_matrix), basis)
    if abs(matrix[2][0]) < 0.999999:
        pitch = math.asin(-matrix[2][0])
        yaw = math.atan2(matrix[1][0], matrix[0][0])
        roll = math.atan2(matrix[2][1], matrix[2][2])
    else:
        pitch = math.pi * 0.5 if matrix[2][0] < 0.0 else -math.pi * 0.5
        yaw = math.atan2(-matrix[0][1], matrix[1][1])
        roll = 0.0
    return (math.degrees(pitch), math.degrees(yaw), math.degrees(roll))


def unity_quaternion_yaw(q):
    x, y, z, w = q
    sin_y = 2.0 * (w * y + x * z)
    cos_y = 1.0 - 2.0 * (x * x + y * y)
    return math.degrees(math.atan2(sin_y, cos_y))


def unity_guid_cache():
    global _ROK_UNITY_GUID_CACHE
    if _ROK_UNITY_GUID_CACHE is not None:
        return _ROK_UNITY_GUID_CACHE
    cache = {}
    for root, _, files in os.walk(os.path.join(ROK_UNITY_CLIENT_ASSETS, "BundleAssets")):
        for filename in files:
            if not filename.endswith(".meta"):
                continue
            meta_path = os.path.join(root, filename)
            try:
                meta_text = open(meta_path, "r", encoding="utf-8", errors="ignore").read(256)
            except Exception:
                continue
            match = re.search(r"guid:\s*([0-9a-fA-F]+)", meta_text)
            if match:
                cache[match.group(1).lower()] = meta_path[:-5]
    _ROK_UNITY_GUID_CACHE = cache
    return cache


def unity_guid_source_path(guid):
    return unity_guid_cache().get((guid or "").lower())


def testunity_fbx_guid_cache():
    global _TESTUNITY_FBX_GUID_CACHE
    if _TESTUNITY_FBX_GUID_CACHE is not None:
        return _TESTUNITY_FBX_GUID_CACHE
    cache = {}
    for root in [TESTUNITY_FBX_ROOT]:
        for current_root, _, files in os.walk(root):
            for filename in files:
                if not filename.endswith(".meta"):
                    continue
                meta_path = os.path.join(current_root, filename)
                try:
                    meta_text = open(meta_path, "r", encoding="utf-8", errors="ignore").read(256)
                except Exception:
                    continue
                match = re.search(r"guid:\s*([0-9a-fA-F]+)", meta_text)
                if match:
                    cache[match.group(1).lower()] = meta_path[:-5]
    _TESTUNITY_FBX_GUID_CACHE = cache
    return cache


def testunity_fbx_guid_source_path(guid):
    testunity_path = testunity_fbx_guid_cache().get((guid or "").lower())
    if not testunity_path:
        return unity_guid_source_path(guid)
    rok_unity_candidate = os.path.join(ROK_UNITY_MAP_FBX_ROOT, os.path.basename(testunity_path))
    if os.path.exists(rok_unity_candidate):
        return rok_unity_candidate
    return testunity_path


def parse_scene():
    text = open(SCENE_PATH, "r", encoding="utf-8").read()
    objects = {}
    transforms = {}
    mesh_filters = {}
    mesh_renderers = {}
    children = defaultdict(list)

    for block in text.split("--- !u!"):
        if not block.strip():
            continue
        match = re.match(r"(\d+) &(-?\d+)", block.splitlines()[0].strip())
        if not match:
            continue
        type_id = int(match.group(1))
        file_id = match.group(2)
        game_object = re.search(r"\n  m_GameObject: \{fileID: (-?\d+)\}", block)
        if type_id == 1:
            name = re.search(r"\n  m_Name: (.*)", block)
            active = re.search(r"\n  m_IsActive: (\d+)", block)
            objects[file_id] = {
                "name": name.group(1) if name else "",
                "active": active.group(1) == "1" if active else False,
            }
        elif type_id == 4 and game_object:
            father = re.search(r"\n  m_Father: \{fileID: (-?\d+)\}", block)
            position = re.search(r"\n  m_LocalPosition: (\{x: [^}]+\})", block)
            scale = re.search(r"\n  m_LocalScale: (\{x: [^}]+\})", block)
            rotation = re.search(r"\n  m_LocalRotation: (\{x: [^}]+\})", block)
            transforms[file_id] = {
                "game_object": game_object.group(1),
                "father": father.group(1) if father else "0",
                "position": parse_vector3(position.group(1)) if position else (0.0, 0.0, 0.0),
                "scale": parse_vector3(scale.group(1), (1.0, 1.0, 1.0)) if scale else (1.0, 1.0, 1.0),
                "rotation": parse_quaternion(rotation.group(1)) if rotation else (0.0, 0.0, 0.0, 1.0),
            }
        elif type_id == 33 and game_object:
            mesh = re.search(r"\n  m_Mesh: \{fileID: [^,]+, guid: ([0-9a-fA-F]+), type: \d+\}", block)
            if mesh:
                mesh_filters[game_object.group(1)] = mesh.group(1).lower()
        elif type_id == 23 and game_object:
            enabled = re.search(r"\n  m_Enabled: (\d+)", block)
            material_guids = re.findall(r"- \{fileID: [^,]+, guid: ([0-9a-fA-F]+), type: \d+\}", block)
            mesh_renderers[game_object.group(1)] = {
                "enabled": enabled.group(1) == "1" if enabled else True,
                "materials": [guid.lower() for guid in material_guids],
            }

    for transform_id, transform in transforms.items():
        children[transform["father"]].append(transform_id)

    transform_by_game_object = {data["game_object"]: transform_id for transform_id, data in transforms.items()}
    world_cache = {}

    def name(transform_id):
        transform = transforms.get(transform_id, {})
        return objects.get(transform.get("game_object", ""), {}).get("name", "")

    def active(transform_id):
        transform = transforms.get(transform_id, {})
        return objects.get(transform.get("game_object", ""), {}).get("active", False)

    def world_transform(transform_id):
        if transform_id in world_cache:
            return world_cache[transform_id]
        transform = transforms[transform_id]
        if transform["father"] == "0" or transform["father"] not in transforms:
            result = (transform["position"], transform["scale"], transform["rotation"])
        else:
            parent_position, parent_scale, _ = world_transform(transform["father"])
            result = (
                add_vec(parent_position, multiply_vec(transform["position"], parent_scale)),
                multiply_vec(parent_scale, transform["scale"]),
                transform["rotation"],
            )
        world_cache[transform_id] = result
        return result

    def find_first(parent_id, wanted_name):
        stack = list(children[parent_id])
        while stack:
            child_id = stack.pop(0)
            if name(child_id) == wanted_name:
                return child_id
            stack.extend(children[child_id])
        return None

    def descendants(parent_id):
        stack = list(children[parent_id])
        while stack:
            child_id = stack.pop(0)
            yield child_id
            stack.extend(children[child_id])

    dense_root = next((tid for tid in transforms if name(tid) == "Dense_Layout_Screenshot"), None)
    container_root = next((tid for tid in transforms if name(tid) == "CityBuildingContainer"), None)
    if not dense_root or not container_root:
        raise RuntimeError("TestUnity scene is missing Dense_Layout_Screenshot or CityBuildingContainer")

    return {
        "objects": objects,
        "transforms": transforms,
        "mesh_filters": mesh_filters,
        "mesh_renderers": mesh_renderers,
        "transform_by_game_object": transform_by_game_object,
        "children": children,
        "name": name,
        "active": active,
        "world_transform": world_transform,
        "find_first": find_first,
        "descendants": descendants,
        "dense_root": dense_root,
        "container_root": container_root,
    }


def unity_to_ue(position):
    return unreal.Vector(-position[0] * UE_UNITS_PER_UNITY_UNIT, position[2] * UE_UNITS_PER_UNITY_UNIT, position[1] * UE_UNITS_PER_UNITY_UNIT)


def unity_planar_to_ue(x, z, y=0.0):
    return unreal.Vector(-x * UE_UNITS_PER_UNITY_UNIT, z * UE_UNITS_PER_UNITY_UNIT, y * UE_UNITS_PER_UNITY_UNIT)


def unity_yaw_from_quaternion(rotation):
    _, y, _, w = rotation
    return math.degrees(2.0 * math.atan2(y, w))


def unity_yaw_to_ue_reflected(yaw):
    return 180.0 - yaw


def camera_forward_ue():
    x, y, z = CAMERA_FORWARD_UNITY
    length = math.sqrt(x * x + y * y + z * z)
    if length < 0.0001:
        return unreal.Vector(0.0, 0.70710678, -0.70710678)
    return unreal.Vector(x / length, z / length, y / length)


def camera_center_ue():
    return unity_planar_to_ue(CAMERA_VIEW_CENTER[0], CAMERA_VIEW_CENTER[1], 0.0)


def camera_location_ue():
    forward = camera_forward_ue()
    center = camera_center_ue()
    return center - forward * (CAMERA_VIEW_DISTANCE * UE_UNITS_PER_UNITY_UNIT)


def look_rotation_from_forward(forward):
    try:
        return unreal.MathLibrary.make_rot_from_x(forward)
    except Exception:
        return unreal.KismetMathLibrary.make_rot_from_x(forward)


def read_sprite_ppu(color_path):
    meta_path = color_path + ".meta"
    if not os.path.exists(meta_path):
        return 100.0
    text = open(meta_path, "r", encoding="utf-8", errors="ignore").read()
    match = re.search(r"spritePixelsToUnits: ([0-9.]+)", text)
    return float(match.group(1)) if match else 100.0


def texture_dimensions(texture):
    width = 256
    height = 256
    if hasattr(texture, "blueprint_get_size_x"):
        width = max(1, int(texture.blueprint_get_size_x()))
        height = max(1, int(texture.blueprint_get_size_y()))
    return width, height


def import_testunity_wall_meshes():
    meshes = {}
    for key, spec in TESTUNITY_WALL_MESH_SPECS.items():
        filename, asset_name, required = spec
        source = os.path.join(TESTUNITY_FBX_ROOT, filename)
        mesh = proto.import_reference_static_mesh({
            "source": source,
            "destination": TESTUNITY_CITY_MESH_DESTINATION,
            "name": asset_name,
            "required": required,
        })
        if mesh:
            meshes[key] = mesh
    return meshes


def import_testunity_wall_texture_materials():
    materials = {}
    for key, spec in TESTUNITY_WALL_TEXTURE_SPECS.items():
        filename, texture_name, material_name = spec
        source = os.path.join(TESTUNITY_SHARED_MAP_MATERIAL_ROOT, filename)
        if not os.path.exists(source):
            unreal.log_warning("Missing TestUnity wall texture: {0}".format(source))
            continue
        texture = runtime.import_runtime_texture_as(source, texture_name)
        if not texture:
            unreal.log_warning("Failed to import TestUnity wall texture: {0}".format(source))
            continue
        material = runtime.create_textured_surface_material(texture, material_name)
        if material:
            materials[key] = material
    return materials


def import_rok_unity_boundary_materials():
    materials = {}
    for key, spec in ROK_UNITY_BOUNDARY_TEXTURE_SPECS.items():
        filename, texture_name, material_name = spec
        source = os.path.join(ROK_UNITY_SHARED_MAP_MATERIAL_ROOT, filename)
        if not os.path.exists(source):
            unreal.log_warning("Missing Rok-unity boundary texture: {0}".format(source))
            continue
        texture = runtime.import_runtime_texture_as(source, texture_name)
        if not texture:
            unreal.log_warning("Failed to import Rok-unity boundary texture: {0}".format(source))
            continue
        material = runtime.create_textured_surface_material(texture, material_name)
        if material:
            materials[key] = material
    return materials


def import_rok_unity_boundary_mesh(source_path):
    if not source_path or not os.path.exists(source_path):
        return None
    stem = os.path.splitext(os.path.basename(source_path))[0]
    asset_name = "SM_RokUnity_{0}".format(re.sub(r"[^A-Za-z0-9_]+", "_", stem))
    return proto.import_reference_static_mesh({
        "source": source_path,
        "destination": ROK_UNITY_BOUNDARY_MESH_DESTINATION,
        "name": asset_name,
        "required": True,
    })


def import_rok_unity_city_wall_obj():
    asset_name = "SM_RokUnity_CityWall_1_1_Obj"
    package_path = proto.asset_package_path(ROK_UNITY_CITY_WALL_MESH_DESTINATION, asset_name)
    existing = proto.load_asset_if_exists(package_path)
    force_reimport = os.environ.get("ROK_FORCE_REIMPORT", "0") == "1"
    if isinstance(existing, unreal.StaticMesh) and not force_reimport:
        return existing
    if not os.path.exists(ROK_UNITY_CITY_WALL_OBJ):
        unreal.log_warning("Missing Rok-unity CityWall_1_1 OBJ source: {0}".format(ROK_UNITY_CITY_WALL_OBJ))
        return None

    unreal.EditorAssetLibrary.make_directory(ROK_UNITY_CITY_WALL_MESH_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", ROK_UNITY_CITY_WALL_OBJ)
    task.set_editor_property("destination_path", ROK_UNITY_CITY_WALL_MESH_DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    imported = proto.load_asset_if_exists(package_path)
    if isinstance(imported, unreal.StaticMesh):
        return imported

    for path in task.get_editor_property("imported_object_paths"):
        candidate = proto.load_asset_if_exists(path)
        if isinstance(candidate, unreal.StaticMesh):
            return candidate

    unreal.log_warning("Failed to import Rok-unity CityWall_1_1 OBJ: {0}".format(ROK_UNITY_CITY_WALL_OBJ))
    return None


def create_lit_textured_material(texture, material_name):
    material_path = proto.asset_package_path(runtime.RUNTIME_MATERIAL_DESTINATION, material_name)
    if os.environ.get("ROK_FORCE_REIMPORT", "0") == "1" and unreal.EditorAssetLibrary.does_asset_exist(material_path):
        unreal.EditorAssetLibrary.delete_asset(material_path)
    else:
        existing = proto.load_asset_if_exists(material_path)
        if existing:
            return existing

    unreal.EditorAssetLibrary.make_directory(runtime.RUNTIME_MATERIAL_DESTINATION)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        material_name,
        runtime.RUNTIME_MATERIAL_DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        return proto.load_asset_if_exists(material_path)

    material.set_editor_property("two_sided", True)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    except Exception:
        pass

    texture_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -480, 0
    )
    texture_node.set_editor_property("texture", texture)
    unreal.MaterialEditingLibrary.connect_material_property(texture_node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    try:
        unreal.MaterialEditingLibrary.connect_material_property(texture_node, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    except Exception:
        pass

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -480, 220
    )
    roughness.set_editor_property("r", 0.86)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def import_rok_unity_city_wall_material():
    if not os.path.exists(ROK_UNITY_CITY_WALL_TEXTURE):
        unreal.log_warning("Missing Rok-unity CityWall_1_1 texture: {0}".format(ROK_UNITY_CITY_WALL_TEXTURE))
        return None
    texture = runtime.import_runtime_texture_as(ROK_UNITY_CITY_WALL_TEXTURE, "T_RokUnity_CityWall_1_1_c")
    if not texture:
        return None
    return create_lit_textured_material(texture, "M_RokUnity_CityWall_1_1_Lit")


def get_or_create_unlit_constant_material(material_name, color):
    material_path = proto.asset_package_path(runtime.RUNTIME_MATERIAL_DESTINATION, material_name)
    existing = proto.load_asset_if_exists(material_path)
    if existing:
        return existing

    unreal.EditorAssetLibrary.make_directory(runtime.RUNTIME_MATERIAL_DESTINATION)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        material_name,
        runtime.RUNTIME_MATERIAL_DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        return proto.load_asset_if_exists(material_path)

    material.set_editor_property("two_sided", True)
    try:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    color_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -360, -40
    )
    color_node.set_editor_property("constant", color)
    unreal.MaterialEditingLibrary.connect_material_property(color_node, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(color_node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def make_materials():
    materials = {
        "outer": get_or_create_unlit_constant_material("M_TestUnity_OuterDesert", unreal.LinearColor(0.52, 0.36, 0.18, 1.0)),
        "city": get_or_create_unlit_constant_material("M_TestUnity_CityGround", unreal.LinearColor(0.50, 0.60, 0.31, 1.0)),
        "road": get_or_create_unlit_constant_material("M_TestUnity_Road", unreal.LinearColor(0.54, 0.58, 0.36, 1.0)),
        "wall": get_or_create_unlit_constant_material("M_TestUnity_CityWall", unreal.LinearColor(0.93, 0.72, 0.43, 1.0)),
        "leaf": get_or_create_unlit_constant_material("M_TestUnity_PalmLeaf", unreal.LinearColor(0.08, 0.58, 0.20, 1.0)),
        "trunk": get_or_create_unlit_constant_material("M_TestUnity_PalmTrunk", unreal.LinearColor(0.48, 0.26, 0.10, 1.0)),
        "prop": get_or_create_unlit_constant_material("M_TestUnity_DecorProp", unreal.LinearColor(0.34, 0.47, 0.18, 1.0)),
    }
    wall_materials = import_testunity_wall_texture_materials()
    materials.update({
        "wall_mesh": wall_materials.get("wall", materials["wall"]),
        "gate_mesh": wall_materials.get("gate", wall_materials.get("wall", materials["wall"])),
        "tower_mesh": wall_materials.get("tower", wall_materials.get("wall", materials["wall"])),
    })
    return materials


def spawn_ground(primitives, materials):
    plane = primitives["plane"]
    proto.spawn_mesh(
        "TestUnity_Outer_Desert_Ground",
        plane,
        (0.0, 0.0, -40.0),
        (24.0 * UE_UNITS_PER_UNITY_UNIT / 100.0, 24.0 * UE_UNITS_PER_UNITY_UNIT / 100.0, 1.0),
        materials["outer"],
        (0.0, 0.0, 0.0),
        "RokUnityGameAppCity/TestUnity/Ground",
    ).tags = ["RokTestUnityLayout", "UnitySource_Outer_Desert_Ground"]
    proto.spawn_mesh(
        "TestUnity_City_Interior_Ground",
        plane,
        (0.0, 0.0, -18.0),
        (16.0 * UE_UNITS_PER_UNITY_UNIT / 100.0, 16.0 * UE_UNITS_PER_UNITY_UNIT / 100.0, 1.0),
        materials["city"],
        (0.0, 0.0, 0.0),
        "RokUnityGameAppCity/TestUnity/Ground",
    ).tags = ["RokTestUnityLayout", "UnitySource_City_Interior_Ground"]


def spawn_lighting():
    directional = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight,
        unreal.Vector(-2000.0, -3000.0, 4000.0),
        proto.make_rotator(-55.0, -35.0, 0.0),
    )
    directional.set_actor_label("TestUnity_Sun_Directional_Light")
    directional.set_folder_path("RokUnityGameAppCity/TestUnity/Lighting")
    directional.tags = ["RokTestUnityLayout", "UnitySource_Sun_Directional_Light"]
    component = directional.get_component_by_class(unreal.DirectionalLightComponent)
    if component:
        component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        component.set_intensity(5.0)
        component.set_editor_property("light_color", unreal.Color(255, 235, 199, 255))
        component.set_editor_property("indirect_lighting_intensity", 1.1)

    sky = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyLight,
        unreal.Vector(0.0, 0.0, 1000.0),
        proto.make_rotator(0.0, 0.0, 0.0),
    )
    sky.set_actor_label("TestUnity_SkyLight")
    sky.set_folder_path("RokUnityGameAppCity/TestUnity/Lighting")
    sky.tags = ["RokTestUnityLayout", "UnitySource_AmbientLight"]
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    if sky_component:
        sky_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        sky_component.set_intensity(1.4)
        sky_component.set_editor_property("lower_hemisphere_is_black", False)
        sky_component.set_editor_property("indirect_lighting_intensity", 1.0)
    return directional, sky


def spawn_road(label, transform, primitives, materials):
    position, scale, rotation = transform
    location = unity_to_ue(position)
    location.z = 2.0 + position[1] * UE_UNITS_PER_UNITY_UNIT
    width = max(20.0, scale[0] * UE_UNITS_PER_UNITY_UNIT)
    length = max(20.0, scale[2] * UE_UNITS_PER_UNITY_UNIT)
    yaw = unity_yaw_to_ue_reflected(unity_yaw_from_quaternion(rotation))
    actor = proto.spawn_mesh(
        "TestUnity_{0}".format(proto.safe_asset_name(label)),
        primitives["plane"],
        (location.x, location.y, location.z),
        (width / 100.0, length / 100.0, 1.0),
        materials["road"],
        (0.0, yaw, 0.0),
        "RokUnityGameAppCity/TestUnity/Roads",
    )
    actor.tags = ["RokTestUnityLayout", "UnitySource_{0}".format(label)]
    return actor


def building_model_from_name(name):
    match = re.search(r"_build_(\d+)_", name)
    if not match:
        return None
    building_type = int(match.group(1))
    if building_type == 51:
        return "build_51_101_1_1"
    return "build_{0}_101_1".format(building_type)


def spawn_sprite(label, color_path, mask_path, center_position, world_scale, folder, tags, material_id):
    texture = runtime.import_runtime_texture_as(color_path, "TestUnity_{0}".format(material_id))
    if not texture:
        unreal.log_warning("Failed to import TestUnity sprite: {0}".format(color_path))
        return None
    material = runtime.create_runtime_city_sprite_material(texture, "TestUnity_{0}".format(material_id), mask_path, False, False)
    tex_width, tex_height = texture_dimensions(texture)
    ppu = read_sprite_ppu(color_path)
    width = tex_width / max(1.0, ppu) * world_scale[0] * UE_UNITS_PER_UNITY_UNIT * SPRITE_VISUAL_SCALE
    height = tex_height / max(1.0, ppu) * world_scale[1] * UE_UNITS_PER_UNITY_UNIT * SPRITE_VISUAL_SCALE
    location = unity_to_ue(center_position)
    sort_priority = int((center_position[2] + 8.0) * 1000.0 + center_position[1] * 100.0)
    actor = runtime.spawn_sprite_actor(
        "TestUnity_{0}".format(proto.safe_asset_name(label)),
        material,
        location.x,
        location.y,
        width,
        height,
        sort_priority,
        folder,
        ["RokTestUnityLayout"] + tags,
        False,
        z_override=max(8.0, location.z),
        anchor_mode="center",
    )
    return actor


def spawn_building(scene, transform_id):
    name = scene["name"](transform_id)
    model_id = building_model_from_name(name)
    if not model_id:
        return None
    color_path = source_path(os.path.join(BUILDING_ROOT, "{0}.png".format(model_id)))
    mask_path = source_path(os.path.join(BUILDING_ROOT, "{0}_mask.png".format(model_id)))
    if not color_path:
        unreal.log_warning("Missing TestUnity building source for {0}: {1}".format(name, model_id))
        return None

    sprite_id = scene["find_first"](transform_id, "sprite")
    if sprite_id:
        center, world_scale, _ = scene["world_transform"](sprite_id)
    else:
        center, world_scale, _ = scene["world_transform"](transform_id)
        world_scale = scaled_vec(world_scale, 0.12)

    actor = spawn_sprite(
        name,
        color_path,
        mask_path,
        center,
        world_scale,
        "RokUnityGameAppCity/TestUnity/Buildings",
        ["UnityDenseBuilding", "UnityModel_{0}".format(model_id)],
        model_id,
    )
    if actor:
        actor.tags = list(set(list(actor.tags) + ["UnitySource_{0}".format(name)]))
    return actor


def spawn_flag(scene, transform_id):
    color_path = source_path(CITY_FLAG_REL)
    mask_path = source_path(CITY_FLAG_MASK_REL)
    if not color_path:
        return None
    position, scale, _ = scene["world_transform"](transform_id)
    center = (position[0], position[1] + 0.38 * scale[1], position[2])
    texture = runtime.import_runtime_texture_as(color_path, "TestUnity_{0}".format(scene["name"](transform_id)))
    if not texture:
        return None
    material = runtime.create_runtime_city_sprite_material(
        texture,
        "TestUnity_{0}".format(scene["name"](transform_id)),
        mask_path,
        False,
        False,
    )
    tex_width, tex_height = texture_dimensions(texture)
    width = 32.0 * max(0.65, scale[0])
    height = width * float(tex_height) / max(1.0, float(tex_width))
    location = unity_to_ue(center)
    actor = runtime.spawn_sprite_actor(
        "TestUnity_{0}".format(proto.safe_asset_name(scene["name"](transform_id))),
        material,
        location.x,
        location.y,
        width,
        height,
        int((center[2] + 8.0) * 1000.0),
        "RokUnityGameAppCity/TestUnity/Flags",
        ["RokTestUnityLayout", "UnityFlag"],
        False,
        z_override=max(8.0, location.z),
        anchor_mode="center",
    )
    return actor


def spawn_unit(scene, transform_id, primitives, materials):
    position, scale, _ = scene["world_transform"](transform_id)
    location = unity_to_ue(position)
    actor = proto.spawn_mesh(
        "TestUnity_{0}".format(proto.safe_asset_name(scene["name"](transform_id))),
        primitives["cube"],
        (location.x, location.y, 28.0),
        (0.18, 0.18, 0.36),
        materials["trunk"],
        (0.0, 0.0, 0.0),
        "RokUnityGameAppCity/TestUnity/Units",
    )
    actor.tags = ["RokTestUnityLayout", "UnityUnit"]
    return actor


def spawn_palm_at(label, position, scale, primitives, materials, folder):
    x, y, z = position
    sx, sy, sz = scale
    base = unity_planar_to_ue(x, z, y)
    trunk_height = 0.84 * sy * UE_UNITS_PER_UNITY_UNIT
    trunk_width = 0.08 * sx * UE_UNITS_PER_UNITY_UNIT
    proto.spawn_mesh(
        "TestUnity_{0}_Trunk".format(proto.safe_asset_name(label)),
        primitives["cube"],
        (base.x, base.y, base.z + trunk_height * 0.5),
        (trunk_width / 100.0, trunk_width / 100.0, trunk_height / 100.0),
        materials["trunk"],
        (0.0, 0.0, 0.0),
        folder,
    ).tags = ["RokTestUnityLayout", "UnityPalm"]

    leaf_length = 0.78 * max(sx, sz) * UE_UNITS_PER_UNITY_UNIT
    leaf_width = 0.10 * max(sx, sz) * UE_UNITS_PER_UNITY_UNIT
    leaf_height = 0.035 * sy * UE_UNITS_PER_UNITY_UNIT
    top_z = base.z + 0.78 * sy * UE_UNITS_PER_UNITY_UNIT
    for index, yaw in enumerate([0, 45, 90, 135, 180, 225, 270, 315]):
        radians = math.radians(yaw)
        leaf_x = base.x + math.cos(radians) * leaf_length * 0.18
        leaf_y = base.y + math.sin(radians) * leaf_length * 0.18
        actor = proto.spawn_mesh(
            "TestUnity_{0}_Leaf_{1:02d}".format(proto.safe_asset_name(label), index),
            primitives["cube"],
            (leaf_x, leaf_y, top_z),
            (leaf_length / 100.0, leaf_width / 100.0, leaf_height / 100.0),
            materials["leaf"],
            (0.0, yaw, 0.0),
            folder,
        )
        actor.tags = ["RokTestUnityLayout", "UnityPalm"]


def spawn_prop(label, transform, primitives, materials):
    position, scale, _ = transform
    location = unity_to_ue(position)
    size = max(35.0, max(scale) * UE_UNITS_PER_UNITY_UNIT * 3.5)
    actor = proto.spawn_mesh(
        "TestUnity_{0}".format(proto.safe_asset_name(label)),
        primitives["cube"],
        (location.x, location.y, 7.0),
        (size / 100.0, size / 100.0, 0.04),
        materials["prop"],
        (0.0, 0.0, 0.0),
        "RokUnityGameAppCity/TestUnity/Props",
    )
    actor.tags = ["RokTestUnityLayout", "UnityProp"]
    return actor


def load_static_mesh(asset_name):
    return proto.load_asset_if_exists("/Game/RokPrototype/Meshes/City/{0}.{0}".format(asset_name))


def centered_mesh_location(mesh, center, uniform_scale, yaw, bottom_z):
    bounds = mesh.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent
    local_min_z = origin.z - extent.z
    offset_x, offset_y = proto.rotate_xy(origin.x * uniform_scale, origin.y * uniform_scale, yaw)
    return (
        center.x - offset_x,
        center.y - offset_y,
        bottom_z - local_min_z * uniform_scale,
    )


def mesh_length_scale(mesh, target_length):
    bounds = mesh.get_bounds()
    size_x = max(1.0, float(bounds.box_extent.x) * 2.0)
    size_y = max(1.0, float(bounds.box_extent.y) * 2.0)
    return target_length / max(size_x, size_y)


def mesh_unity_xz_scale(mesh, target_length):
    bounds = mesh.get_bounds()
    size_x = max(1.0, float(bounds.box_extent.x) * 2.0)
    size_z = max(1.0, float(bounds.box_extent.z) * 2.0)
    return target_length / max(size_x, size_z)


def unity_obj_wall_location(mesh, center, uniform_scale, yaw):
    bounds = mesh.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent
    local_min_y = origin.y - extent.y
    offset_x, offset_y = proto.rotate_xy(-origin.x * uniform_scale, origin.z * uniform_scale, yaw)
    return (
        center.x - offset_x,
        center.y - offset_y,
        -local_min_y * uniform_scale,
    )


def parse_unity_prefab(path):
    text = open(path, "r", encoding="utf-8", errors="ignore").read()
    objects = {}
    transforms = {}
    mesh_filters = {}
    mesh_renderers = {}
    children = defaultdict(list)

    for block in text.split("--- !u!"):
        if not block.strip():
            continue
        header = block.splitlines()[0].strip()
        match = re.match(r"(\d+) &(-?\d+)", header)
        if not match:
            continue
        type_id = int(match.group(1))
        file_id = match.group(2)
        game_object = re.search(r"\n  m_GameObject: \{fileID: (-?\d+)\}", block)
        if type_id == 1:
            name = re.search(r"\n  m_Name: (.*)", block)
            active = re.search(r"\n  m_IsActive: (\d+)", block)
            objects[file_id] = {
                "name": name.group(1) if name else "",
                "active": active.group(1) == "1" if active else False,
            }
        elif type_id == 4 and game_object:
            father = re.search(r"\n  m_Father: \{fileID: (-?\d+)\}", block)
            position = re.search(r"\n  m_LocalPosition: (\{x: [^}]+\})", block)
            scale = re.search(r"\n  m_LocalScale: (\{x: [^}]+\})", block)
            rotation = re.search(r"\n  m_LocalRotation: (\{x: [^}]+\})", block)
            transforms[file_id] = {
                "game_object": game_object.group(1),
                "father": father.group(1) if father else "0",
                "position": parse_vector3(position.group(1)) if position else (0.0, 0.0, 0.0),
                "scale": parse_vector3(scale.group(1), (1.0, 1.0, 1.0)) if scale else (1.0, 1.0, 1.0),
                "rotation": parse_quaternion(rotation.group(1)) if rotation else (0.0, 0.0, 0.0, 1.0),
            }
        elif type_id == 33 and game_object:
            mesh = re.search(r"\n  m_Mesh: \{fileID: [^,]+, guid: ([0-9a-fA-F]+), type: \d+\}", block)
            if mesh:
                mesh_filters[game_object.group(1)] = mesh.group(1).lower()
        elif type_id == 23 and game_object:
            enabled = re.search(r"\n  m_Enabled: (\d+)", block)
            material_guids = re.findall(r"- \{fileID: [^,]+, guid: ([0-9a-fA-F]+), type: \d+\}", block)
            mesh_renderers[game_object.group(1)] = {
                "enabled": enabled.group(1) == "1" if enabled else True,
                "materials": [guid.lower() for guid in material_guids],
            }

    for transform_id, transform in transforms.items():
        children[transform["father"]].append(transform_id)

    transform_by_game_object = {data["game_object"]: transform_id for transform_id, data in transforms.items()}
    world_cache = {}

    def world_transform(transform_id):
        if transform_id in world_cache:
            return world_cache[transform_id]
        transform = transforms[transform_id]
        local_position = transform["position"]
        local_rotation = transform["rotation"]
        local_scale = transform["scale"]
        parent_id = transform["father"]
        if parent_id == "0" or parent_id not in transforms:
            result = (local_position, local_rotation, local_scale)
        else:
            parent_position, parent_rotation, parent_scale = world_transform(parent_id)
            scaled_position = multiply_vec(local_position, parent_scale)
            result = (
                add_vec(parent_position, quat_rotate(parent_rotation, scaled_position)),
                quat_multiply(parent_rotation, local_rotation),
                multiply_vec(parent_scale, local_scale),
            )
        world_cache[transform_id] = result
        return result

    return {
        "objects": objects,
        "transforms": transforms,
        "mesh_filters": mesh_filters,
        "mesh_renderers": mesh_renderers,
        "transform_by_game_object": transform_by_game_object,
        "world_transform": world_transform,
    }


def boundary_material_for_source(source_path, materials):
    stem = os.path.splitext(os.path.basename(source_path or ""))[0]
    if stem.startswith("CityGate_101_1"):
        return materials.get("gate")
    if stem.startswith("CityTower_101_1"):
        return materials.get("tower")
    if stem.startswith("CityWall_101_1"):
        return materials.get("wall")
    return None


def boundary_scale_multiplier_for_source(source_path):
    stem = os.path.splitext(os.path.basename(source_path or ""))[0]
    if stem.startswith("CityTower_101_1"):
        return float(os.environ.get("ROK_UNITY_BOUNDARY_TOWER_SCALE_MULT", "0.1"))
    if stem.startswith("CityGate_101_1"):
        return float(os.environ.get("ROK_UNITY_BOUNDARY_GATE_SCALE_MULT", "1.0"))
    return float(os.environ.get("ROK_UNITY_BOUNDARY_WALL_SCALE_MULT", "0.65"))


def is_boundary_source_mesh(source_path):
    stem = os.path.splitext(os.path.basename(source_path or ""))[0]
    return (
        stem.startswith("CityWall_101_1")
        or stem.startswith("CityGate_101_1")
        or stem.startswith("CityTower_101_1")
    )


def spawn_rok_unity_boundary_prefab_renderer(label, source_path, transform, material, prefab_name):
    mesh = import_rok_unity_boundary_mesh(source_path)
    if not mesh:
        return None
    position, rotation, scale = transform
    target_location = unity_to_ue(position)
    if os.environ.get("ROK_UNITY_BOUNDARY_FULL_ROTATION", "0") == "1":
        pitch, yaw, roll = unity_quaternion_to_ue_rotator(rotation)
    else:
        pitch = 0.0
        yaw = unity_yaw_to_ue_reflected(unity_quaternion_yaw(rotation))
        roll = 0.0
    scale_multiplier = (
        float(os.environ.get("ROK_UNITY_BOUNDARY_SCALE_MULT", "1.0"))
        * boundary_scale_multiplier_for_source(source_path)
    )
    fbx_centimeters = UE_UNITS_PER_UNITY_UNIT / 100.0
    actor_scale = (
        scale[0] * fbx_centimeters * scale_multiplier,
        scale[1] * fbx_centimeters * scale_multiplier,
        scale[2] * fbx_centimeters * scale_multiplier,
    )
    bounds = mesh.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent
    offset_x, offset_y = proto.rotate_xy(origin.x * actor_scale[0], origin.y * actor_scale[1], yaw)
    local_min_z = origin.z - extent.z
    location = (
        target_location.x - offset_x,
        target_location.y - offset_y,
        target_location.z - local_min_z * actor_scale[2],
    )
    actor = proto.spawn_mesh(
        label,
        mesh,
        location,
        actor_scale,
        material,
        (pitch, yaw, roll),
        "RokUnityGameAppCity/RokUnity/Walls",
    )
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if component:
        try:
            component.set_editor_property("cast_shadow", True)
            component.set_editor_property("use_as_occluder", True)
        except Exception:
            pass
    stem = os.path.splitext(os.path.basename(source_path))[0]
    actor.tags = [
        "RokTestUnityLayout",
        "UnityWallSegment",
        "RokUnityCityBoundaryPrefabSource",
        "UnityMesh_{0}".format(stem),
        "UnitySourcePrefab_{0}".format(os.path.splitext(os.path.basename(prefab_name))[0]),
    ]
    return actor


def spawn_rok_unity_city_boundary_from_prefabs(materials, include_wall_meshes=True):
    spawned = 0
    for prefab_path in ROK_UNITY_BOUNDARY_PREFABS:
        if not os.path.exists(prefab_path):
            unreal.log_warning("Missing Rok-unity city boundary prefab: {0}".format(prefab_path))
            continue
        prefab = parse_unity_prefab(prefab_path)
        for game_object_id, mesh_guid in prefab["mesh_filters"].items():
            renderer = prefab["mesh_renderers"].get(game_object_id)
            transform_id = prefab["transform_by_game_object"].get(game_object_id)
            source_path = unity_guid_source_path(mesh_guid)
            if not renderer or not renderer["enabled"] or not transform_id or not is_boundary_source_mesh(source_path):
                continue
            if not include_wall_meshes and os.path.splitext(os.path.basename(source_path))[0].startswith("CityWall_101_1"):
                continue
            transform = prefab["world_transform"](transform_id)
            object_name = prefab["objects"].get(game_object_id, {}).get("name", "Boundary")
            stem = os.path.splitext(os.path.basename(source_path))[0]
            material = boundary_material_for_source(source_path, materials)
            label = "RokUnity_{0}_{1}_{2}".format(
                os.path.splitext(os.path.basename(prefab_path))[0],
                object_name.replace(" ", "_").replace("(", "").replace(")", ""),
                spawned,
            )
            if spawn_rok_unity_boundary_prefab_renderer(label, source_path, transform, material, prefab_path):
                spawned += 1
                unreal.log("Spawned Rok-unity boundary source mesh {0} from {1}".format(stem, prefab_path))
    return spawned


def spawn_rok_unity_city_boundary_from_testunity_scene(scene, materials):
    spawned = 0
    for game_object_id, mesh_guid in scene["mesh_filters"].items():
        transform_id = scene["transform_by_game_object"].get(game_object_id)
        renderer = scene["mesh_renderers"].get(game_object_id)
        object_info = scene["objects"].get(game_object_id, {})
        object_name = object_info.get("name", "")
        source_path = testunity_fbx_guid_source_path(mesh_guid)
        if (
            not transform_id
            or not renderer
            or not renderer["enabled"]
            or not object_info.get("active", False)
            or not is_boundary_source_mesh(source_path)
        ):
            continue

        # The TestUnity demo uses this scene instance as the rendered city boundary.
        # Other prefab helper objects are layout anchors and can double-spawn oversized wall parts in UE.
        if object_name != "CityWall_1":
            continue

        stem = os.path.splitext(os.path.basename(source_path))[0]
        label = "RokUnity_TestUnityScene_{0}_{1}".format(object_name, spawned)
        position, scale, rotation = scene["world_transform"](transform_id)
        actor = spawn_rok_unity_boundary_prefab_renderer(
            label,
            source_path,
            (position, rotation, scale),
            boundary_material_for_source(source_path, materials),
            os.path.basename(SCENE_PATH),
        )
        if actor:
            actor.tags = list(actor.tags) + [
                "RokUnityTestUnitySceneBoundary",
                "UnitySourceScene_RokCity2_5D_Demo",
            ]
            spawned += 1
            unreal.log("Spawned TestUnity scene city boundary mesh {0} from {1}".format(stem, SCENE_PATH))
    return spawned


def spawn_scaled_city_mesh(label, asset_name, target_width, materials):
    mesh = load_static_mesh(asset_name)
    if not mesh:
        unreal.log_warning("Missing TestUnity city mesh asset: {0}".format(asset_name))
        return None
    bounds = mesh.get_bounds()
    size_x = max(1.0, float(bounds.box_extent.x) * 2.0)
    size_y = max(1.0, float(bounds.box_extent.y) * 2.0)
    uniform_scale = target_width / max(size_x, size_y)
    origin = bounds.origin
    local_min_z = bounds.origin.z - bounds.box_extent.z
    location = (-origin.x * uniform_scale, -origin.y * uniform_scale, -local_min_z * uniform_scale)
    actor = proto.spawn_mesh(
        label,
        mesh,
        location,
        (uniform_scale, uniform_scale, uniform_scale),
        materials["wall"],
        (0.0, 0.0, 0.0),
        "RokUnityGameAppCity/TestUnity/Walls",
    )
    actor.tags = ["RokTestUnityLayout", "UnitySource_CityWall_age1_101", "UnityMesh_{0}".format(asset_name)]
    return actor


def spawn_wall_segment(label, x, z, length, width, yaw, wall_mesh, material, primitives, materials):
    location = unity_planar_to_ue(x, z, 0.0)
    wall_yaw = unity_yaw_to_ue_reflected(yaw)

    if wall_mesh:
        target_length = length * UE_UNITS_PER_UNITY_UNIT
        uniform_scale = mesh_length_scale(wall_mesh, target_length)
        mesh_location = centered_mesh_location(wall_mesh, location, uniform_scale, wall_yaw, 0.0)
        actor = proto.spawn_mesh(
            "TestUnity_Wall_{0}".format(label),
            wall_mesh,
            mesh_location,
            (uniform_scale, uniform_scale, uniform_scale),
            material,
            (0.0, wall_yaw, 0.0),
            "RokUnityGameAppCity/TestUnity/Walls",
        )
        actor.tags = [
            "RokTestUnityLayout",
            "UnityWallSegment",
            "UnitySource_CityWall_age1_101",
            "UnityMesh_CityWall_101_1",
        ]
        return actor

    wall_height = 0.42 * UE_UNITS_PER_UNITY_UNIT
    actor = proto.spawn_mesh(
        "TestUnity_Wall_{0}".format(label),
        primitives["cube"],
        (location.x, location.y, wall_height * 0.5),
        (
            length * UE_UNITS_PER_UNITY_UNIT / 100.0,
            width * UE_UNITS_PER_UNITY_UNIT / 100.0,
            wall_height / 100.0,
        ),
        materials["wall"],
        (0.0, wall_yaw, 0.0),
        "RokUnityGameAppCity/TestUnity/Walls",
    )
    actor.tags = ["RokTestUnityLayout", "UnityWallSegment", "UnitySource_CityWall_age1_101", "FallbackCubeWall"]
    return actor


def spawn_full_wall_model(wall_mesh, material):
    if not wall_mesh:
        return None
    center = unity_planar_to_ue(0.0, 0.0, 0.0)
    wall_yaw = unity_yaw_to_ue_reflected(-45.0)
    target_width = TARGET_WALL_WIDTH_UNITY * UE_UNITS_PER_UNITY_UNIT
    uniform_scale = mesh_length_scale(wall_mesh, target_width)
    mesh_location = centered_mesh_location(wall_mesh, center, uniform_scale, wall_yaw, 0.0)
    actor = proto.spawn_mesh(
        "TestUnity_CityWall_age1_101",
        wall_mesh,
        mesh_location,
        (uniform_scale, uniform_scale, uniform_scale),
        material,
        (0.0, wall_yaw, 0.0),
        "RokUnityGameAppCity/TestUnity/Walls",
    )
    actor.tags = [
        "RokTestUnityLayout",
        "UnityWallSegment",
        "UnitySource_CityWall_age1_101",
        "UnityMesh_CityWall_101_1",
        "UnityWallFullSourceModel",
    ]
    return actor


def spawn_rok_unity_city_wall_model(wall_mesh, material):
    if not wall_mesh:
        return None
    center = unity_planar_to_ue(0.0, 0.0, 0.0)
    wall_yaw = float(os.environ.get("ROK_UNITY_CITY_WALL_YAW", "0.0"))
    target_width = TARGET_WALL_WIDTH_UNITY * UE_UNITS_PER_UNITY_UNIT
    uniform_scale = mesh_unity_xz_scale(wall_mesh, target_width)
    mesh_location = unity_obj_wall_location(wall_mesh, center, uniform_scale, wall_yaw)
    roll = float(os.environ.get("ROK_UNITY_CITY_WALL_ROLL", "90.0"))
    actor = proto.spawn_mesh(
        "RokUnity_CityWall_1_1_SourceObj",
        wall_mesh,
        mesh_location,
        (-uniform_scale, uniform_scale, -uniform_scale),
        material,
        (0.0, wall_yaw, roll),
        "RokUnityGameAppCity/RokUnity/Walls",
    )
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if component:
        try:
            component.set_editor_property("cast_shadow", True)
            component.set_editor_property("use_as_occluder", True)
        except Exception:
            pass
    actor.tags = [
        "RokTestUnityLayout",
        "UnityWallSegment",
        "RokUnityCityWallSource",
        "UnityMesh_CityWall_1_1_Obj",
        "UnitySource_BundleAssetsTest_Env_City_CityWall_1_1_obj",
    ]
    return actor


def spawn_walls(scene, primitives, materials):
    boundary_materials = import_rok_unity_boundary_materials()
    boundary_count = spawn_rok_unity_city_boundary_from_testunity_scene(scene, boundary_materials)
    if boundary_count > 0:
        return boundary_count

    boundary_count = spawn_rok_unity_city_boundary_from_prefabs(boundary_materials)
    if boundary_count > 0:
        return boundary_count

    rok_unity_wall_mesh = import_rok_unity_city_wall_obj()
    rok_unity_wall_material = import_rok_unity_city_wall_material() or materials.get("wall_mesh", materials["wall"])
    if spawn_rok_unity_city_wall_model(rok_unity_wall_mesh, rok_unity_wall_material):
        return 1

    wall_meshes = import_testunity_wall_meshes()
    wall_mesh = wall_meshes.get("wall")
    wall_material = materials.get("wall_mesh", materials["wall"])
    if spawn_full_wall_model(wall_mesh, wall_material):
        return 1

    wall_segments = [
        ("NorthWest", -1.52, 2.15, 2.05, 0.22, -18.0),
        ("NorthEast", 0.86, 2.23, 2.15, 0.22, 16.0),
        ("EastUpper", 2.65, 1.02, 2.16, 0.22, -57.0),
        ("EastLower", 2.36, -1.42, 1.92, 0.22, 57.0),
        ("SouthEast", 0.95, -2.58, 1.88, 0.22, 10.0),
        ("SouthWest", -1.25, -2.55, 1.92, 0.22, -11.0),
        ("WestLower", -2.65, -1.18, 2.05, 0.22, -58.0),
        ("WestUpper", -2.48, 1.08, 1.92, 0.22, 58.0),
    ]
    for segment in wall_segments:
        spawn_wall_segment(*segment, wall_mesh, wall_material, primitives, materials)
    return len(wall_segments)


def spawn_dense_layout(scene, primitives, materials):
    spawned = defaultdict(int)
    dense_root = scene["dense_root"]
    for transform_id in scene["children"][dense_root]:
        if not scene["active"](transform_id):
            continue
        label = scene["name"](transform_id)
        transform = scene["world_transform"](transform_id)
        lower = label.lower()
        if "_build_" in label:
            if spawn_building(scene, transform_id):
                spawned["buildings"] += 1
        elif lower.startswith("road_"):
            spawn_road(label, transform, primitives, materials)
            spawned["roads"] += 1
        elif lower.startswith("palm_"):
            position, scale, _ = transform
            spawn_palm_at(label, position, scale, primitives, materials, "RokUnityGameAppCity/TestUnity/Palms")
            spawned["palms"] += 1
        elif lower.startswith("flag_"):
            if spawn_flag(scene, transform_id):
                spawned["flags"] += 1
        elif lower.startswith("unit_"):
            if spawn_unit(scene, transform_id, primitives, materials):
                spawned["units"] += 1
        elif lower.startswith("prop_"):
            spawn_prop(label, transform, primitives, materials)
            spawned["props"] += 1
    return spawned


def spawn_outer_palms(scene, primitives, materials):
    spawned = 0
    container_root = scene["container_root"]
    for transform_id in scene["children"][container_root]:
        label = scene["name"](transform_id)
        if not label.startswith("SimplePalm_") or not scene["active"](transform_id):
            continue
        position, scale, _ = scene["world_transform"](transform_id)
        spawn_palm_at(label, position, scale, primitives, materials, "RokUnityGameAppCity/TestUnity/OuterPalms")
        spawned += 1
    return spawned


def spawn_camera():
    location = camera_location_ue()
    rotation = look_rotation_from_forward(camera_forward_ue())
    camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.CameraActor, location, rotation)
    camera.set_actor_label("Camera_RokUnityGameAppCity")
    camera.set_folder_path("RokUnityGameAppCity/TestUnity/Camera")
    component = camera.get_component_by_class(unreal.CameraComponent)
    if component:
        component.set_editor_property("projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
        horizontal_fov = 2.0 * math.degrees(math.atan(math.tan(math.radians(CAMERA_VERTICAL_FOV) * 0.5) * CAMERA_ASPECT))
        component.set_editor_property("field_of_view", horizontal_fov)
        try:
            component.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.DISABLED)
        except Exception:
            pass
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
            unreal.log_warning("Skipping TestUnity camera post-process setup: {0}".format(error))
    camera.tags = ["RokTestUnityLayout", "UnityCameraReference"]
    return camera


def spawn_player_start():
    player_start = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart,
        unity_planar_to_ue(CAMERA_VIEW_CENTER[0], CAMERA_VIEW_CENTER[1], 0.12),
        proto.make_rotator(0.0, 0.0, 0.0),
    )
    player_start.set_actor_label("PlayerStart_RokUnityGameAppCity")
    player_start.set_folder_path("RokUnityGameAppCity/TestUnity/Camera")
    player_start.tags = ["RokTestUnityLayout", "RokUnityGameAppPlayerStart"]
    return player_start


def is_target_level_loaded():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        return False
    path = world.get_outer().get_path_name()
    return path == TARGET_LEVEL or path.endswith(TARGET_LEVEL)


def build_scene():
    if not os.path.exists(SCENE_PATH):
        raise RuntimeError("Missing TestUnity source scene: {0}".format(SCENE_PATH))

    scene = parse_scene()
    if is_target_level_loaded():
        unreal.log("TestUnity target map already loaded: {0}".format(TARGET_LEVEL))
    elif unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    else:
        unreal.EditorLevelLibrary.new_level(TARGET_LEVEL)

    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        unreal.EditorLevelLibrary.destroy_actor(actor)

    runtime.CAMERA_LOCATION = camera_location_ue()
    runtime.CAMERA_TARGET = camera_center_ue()
    runtime.SPRITE_PLANE_MODE = "camera"
    runtime.ENGINE_PLANE_FLIP_Y = True

    proto.ensure_project_primitive_meshes()
    primitives = {name: proto.load_primitive(name) for name in proto.PRIMITIVE_PATHS}
    materials = make_materials()

    spawn_ground(primitives, materials)
    spawn_lighting()
    wall_count = spawn_walls(scene, primitives, materials)
    outer_palm_count = spawn_outer_palms(scene, primitives, materials)
    counts = spawn_dense_layout(scene, primitives, materials)
    spawn_player_start()
    spawn_camera()
    runtime.configure_world_settings()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError("Failed to save TestUnity GameApp city map: {0}".format(TARGET_LEVEL))
    unreal.EditorAssetLibrary.save_asset(TARGET_LEVEL, only_if_is_dirty=False)
    unreal.log(
        "TestUnity GameApp city map saved: {0}; walls={1}; outer_palms={2}; dense={3}".format(
            TARGET_LEVEL,
            wall_count,
            outer_palm_count,
            dict(counts),
        )
    )


if __name__ == "__main__":
    build_scene()
