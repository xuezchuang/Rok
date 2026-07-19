import hashlib
import json
import math
import os
import re
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
LIVE_UNITY_ROOT = Path(r"D:\ueDemo\Rok-unity\client-unity")
SNAPSHOT_UNITY_ROOT = PROJECT_ROOT / "ref" / "client-unity"
UNITY_ROOT = Path(os.environ.get(
    "ROK_UNITY_ROOT",
    str(LIVE_UNITY_ROOT if LIVE_UNITY_ROOT.is_dir() else SNAPSHOT_UNITY_ROOT),
))
UNITY_ASSETS = UNITY_ROOT / "Assets"
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
OUTPUT_ROOT = MIGRATION_ROOT / "UnityRuntimeCitySupportLayers"
MANIFEST_PATH = MIGRATION_ROOT / "unity_runtime_city_support_layers.json"
VERIFY_PATH = MIGRATION_ROOT / "unity_runtime_city_support_layers_verify.json"
INITIAL_CITY_STATE_PATH = MIGRATION_ROOT / "unity_initial_city_state.json"

RUNTIME_LOD_SCALE = 1.1210888
CONTAINER_YAW_DEGREES = -45.0
UE_UNITS_PER_UNITY_UNIT = 500.0
UNITY_BUILTIN_PLANE_FILE_ID = 10209
UNITY_BUILTIN_GUID = "0000000000000000e000000000000000"
MAIN_CITY_SPRITE_COLOR = [0.994559, 0.844559, 0.737059, 1.0]
SCHEMA = "rok.unity_runtime_city_support_layers.v2"
OBJ_IMPORT_AXIS_COMPENSATION = (
    "UE 5.8 FbxFactory OBJ inverse-Y import compensation: "
    "OBJ input=(desired UE.x,-desired UE.y,desired UE.z)"
)

CITY_WALL_PREFAB = (
    UNITY_ASSETS
    / "BundleAssets"
    / "Scene"
    / "City_building"
    / "city_building_1"
    / "build_6_101_1.prefab"
)

TARGETS = (
    {
        "layer_id": "boundary_support_road",
        "role": "road",
        "object_name": "road",
        "expected_material_guid": "cdaecaffccd495d44842ac8984697f2f",
        "expected_texture_guid": "7d6b379fb057f51478ce1b8bc529722e",
        "expected_shader_guid": "9e820fbfb342119429ef89056af56c1a",
        "expected_render_queue": 2992,
    },
    {
        "layer_id": "boundary_support_wall_shadow",
        "role": "authored_shadow",
        "object_name": "build_6_101_1_shadow",
        "expected_material_guid": "59fd34ad3fc10624991a82306c6fa02f",
        "expected_texture_guid": "77023f20efab2e549abbf2395d7d25db",
        "expected_render_queue": 2993,
    },
)

DOCUMENT_HEADER = re.compile(r"^--- !u!(\d+) &(-?\d+)\s*$", re.MULTILINE)


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def project_relative(path):
    path = Path(path).resolve()
    try:
        return str(path.relative_to(PROJECT_ROOT)).replace("\\", "/")
    except ValueError:
        return str(path).replace("\\", "/")


def unity_relative(path):
    return str(Path(path).resolve().relative_to(UNITY_ROOT.resolve())).replace("\\", "/")


def parse_documents(path):
    text = Path(path).read_text(encoding="utf-8-sig")
    matches = list(DOCUMENT_HEADER.finditer(text))
    documents = {}
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        file_id = int(match.group(2))
        documents[file_id] = {
            "class_id": int(match.group(1)),
            "file_id": file_id,
            "body": text[match.end():end],
        }
    return documents


def scalar(body, key, default=None):
    match = re.search(r"^\s*" + re.escape(key) + r":\s*(.*?)\s*$", body, re.MULTILINE)
    return match.group(1) if match else default


def parse_float_map(value, keys):
    if value is None:
        raise ValueError("Missing Unity inline map")
    result = []
    for key in keys:
        match = re.search(r"(?:^|[,\s{])" + re.escape(key) + r":\s*([-+0-9.eE]+)", value)
        if not match:
            raise ValueError("Missing {0} in Unity inline map: {1}".format(key, value))
        result.append(float(match.group(1)))
    return result


def file_reference(body, key):
    value = scalar(body, key, "")
    file_match = re.search(r"fileID:\s*(-?\d+)", value)
    guid_match = re.search(r"guid:\s*([0-9a-fA-F]+)", value)
    return {
        "file_id": int(file_match.group(1)) if file_match else 0,
        "guid": guid_match.group(1).lower() if guid_match else "",
    }


def parse_prefab(path):
    documents = parse_documents(path)
    game_objects = {}
    transforms = {}
    mesh_filters = {}
    mesh_renderers = {}

    for document in documents.values():
        body = document["body"]
        class_id = document["class_id"]
        if class_id == 1:
            component_ids = [int(value) for value in re.findall(r"- component:\s*\{fileID:\s*(-?\d+)\}", body)]
            game_objects[document["file_id"]] = {
                "file_id": document["file_id"],
                "name": scalar(body, "m_Name", ""),
                "active_self": scalar(body, "m_IsActive", "0") == "1",
                "component_ids": component_ids,
            }
        elif class_id == 4:
            game_object = file_reference(body, "m_GameObject")["file_id"]
            transforms[document["file_id"]] = {
                "file_id": document["file_id"],
                "game_object_id": game_object,
                "local_position": parse_float_map(scalar(body, "m_LocalPosition"), ("x", "y", "z")),
                "local_rotation": parse_float_map(scalar(body, "m_LocalRotation"), ("x", "y", "z", "w")),
                "local_scale": parse_float_map(scalar(body, "m_LocalScale"), ("x", "y", "z")),
                "father_id": file_reference(body, "m_Father")["file_id"],
            }
        elif class_id == 33:
            game_object = file_reference(body, "m_GameObject")["file_id"]
            mesh_filters[game_object] = {
                "file_id": document["file_id"],
                "mesh": file_reference(body, "m_Mesh"),
            }
        elif class_id == 23:
            game_object = file_reference(body, "m_GameObject")["file_id"]
            materials_section = body.split("m_Materials:", 1)[1].split("m_StaticBatchInfo:", 1)[0]
            materials = []
            for file_id, guid in re.findall(
                r"-\s*\{fileID:\s*(-?\d+),\s*guid:\s*([0-9a-fA-F]+)",
                materials_section,
            ):
                materials.append({"file_id": int(file_id), "guid": guid.lower()})
            mesh_renderers[game_object] = {
                "file_id": document["file_id"],
                "enabled": scalar(body, "m_Enabled", "0") == "1",
                "sorting_layer_id": int(scalar(body, "m_SortingLayerID", "0")),
                "sorting_order": int(scalar(body, "m_SortingOrder", "0")),
                "renderer_priority": int(scalar(body, "m_RendererPriority", "0")),
                "materials": materials,
            }

    transform_by_game_object = {item["game_object_id"]: item for item in transforms.values()}
    root_transforms = [item for item in transforms.values() if item["father_id"] == 0]
    if len(root_transforms) != 1:
        raise RuntimeError("Expected exactly one prefab root transform in {0}, found {1}".format(path, len(root_transforms)))

    return {
        "path": Path(path),
        "documents": documents,
        "game_objects": game_objects,
        "transforms": transforms,
        "transform_by_game_object": transform_by_game_object,
        "mesh_filters": mesh_filters,
        "mesh_renderers": mesh_renderers,
        "root_transform_id": root_transforms[0]["file_id"],
    }


def identity_matrix():
    return [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def multiply_matrix(a, b):
    return [
        [sum(a[row][index] * b[index][column] for index in range(4)) for column in range(4)]
        for row in range(4)
    ]


def quaternion_matrix(rotation):
    x, y, z, w = rotation
    xx, yy, zz = x * x, y * y, z * z
    xy, xz, yz = x * y, x * z, y * z
    wx, wy, wz = w * x, w * y, w * z
    return [
        [1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz), 2.0 * (xz + wy), 0.0],
        [2.0 * (xy + wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx), 0.0],
        [2.0 * (xz - wy), 2.0 * (yz + wx), 1.0 - 2.0 * (xx + yy), 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def trs_matrix(position, rotation, scale):
    matrix = quaternion_matrix(rotation)
    for row in range(3):
        matrix[row][0] *= scale[0]
        matrix[row][1] *= scale[1]
        matrix[row][2] *= scale[2]
        matrix[row][3] = position[row]
    return matrix


def yaw_quaternion(degrees):
    half = math.radians(degrees) * 0.5
    return [0.0, math.sin(half), 0.0, math.cos(half)]


def transform_point(matrix, point):
    value = [point[0], point[1], point[2], 1.0]
    return [sum(matrix[row][index] * value[index] for index in range(4)) for row in range(3)]


def runtime_world_matrix(prefab, transform_id, cache):
    if transform_id in cache:
        return cache[transform_id]
    transform = prefab["transforms"][transform_id]
    if transform_id == prefab["root_transform_id"]:
        container = trs_matrix(
            [0.0, 0.0, 0.0],
            yaw_quaternion(CONTAINER_YAW_DEGREES),
            [1.0, 1.0, 1.0],
        )
        instance_override = trs_matrix(
            [0.0, 0.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
            [RUNTIME_LOD_SCALE, RUNTIME_LOD_SCALE, RUNTIME_LOD_SCALE],
        )
        result = multiply_matrix(container, instance_override)
    else:
        parent = runtime_world_matrix(prefab, transform["father_id"], cache)
        local = trs_matrix(
            transform["local_position"],
            transform["local_rotation"],
            transform["local_scale"],
        )
        result = multiply_matrix(parent, local)
    cache[transform_id] = result
    return result


def active_in_hierarchy(prefab, transform_id):
    current = transform_id
    while current:
        transform = prefab["transforms"][current]
        game_object = prefab["game_objects"][transform["game_object_id"]]
        if not game_object["active_self"]:
            return False
        current = transform["father_id"]
    return True


def object_path(prefab, transform_id):
    names = []
    current = transform_id
    while current:
        transform = prefab["transforms"][current]
        names.append(prefab["game_objects"][transform["game_object_id"]]["name"])
        current = transform["father_id"]
    return "/".join(reversed(names))


def build_guid_index():
    index = {}
    for root, _, files in os.walk(UNITY_ASSETS):
        for name in files:
            if not name.endswith(".meta"):
                continue
            meta_path = Path(root) / name
            with meta_path.open("r", encoding="utf-8-sig", errors="ignore") as handle:
                for _ in range(8):
                    line = handle.readline()
                    if not line:
                        break
                    if line.startswith("guid:"):
                        guid = line.split(":", 1)[1].strip().lower()
                        index[guid] = Path(str(meta_path)[:-5])
                        break
    return index


def material_record(material_guid, guid_index):
    material_path = guid_index.get(material_guid)
    if not material_path or not material_path.is_file():
        raise RuntimeError("Unable to resolve Unity material GUID: {0}".format(material_guid))
    body = material_path.read_text(encoding="utf-8-sig")
    shader = file_reference(body, "m_Shader")
    render_queue = int(scalar(body, "m_CustomRenderQueue", "-1"))
    main_texture_match = re.search(
        r"- _MainTex:\s*\r?\n\s*m_Texture:\s*\{fileID:\s*-?\d+,\s*guid:\s*([0-9a-fA-F]+)",
        body,
    )
    if not main_texture_match:
        raise RuntimeError("Unity material has no authored _MainTex: {0}".format(material_path))
    texture_guid = main_texture_match.group(1).lower()
    texture_path = guid_index.get(texture_guid)
    if not texture_path or not texture_path.is_file():
        raise RuntimeError("Unable to resolve Unity texture GUID {0} from {1}".format(texture_guid, material_path))
    shader_path = guid_index.get(shader["guid"]) if shader["guid"] and shader["guid"] != UNITY_BUILTIN_GUID else None

    def saved_color(name):
        match = re.search(r"- " + re.escape(name) + r":\s*(\{[^\r\n]+\})", body)
        return parse_float_map(match.group(1), ("r", "g", "b", "a")) if match else None

    mask_match = re.search(
        r"- _Mask:\s*\r?\n\s*m_Texture:\s*\{fileID:\s*(-?\d+)(?:,\s*guid:\s*([0-9a-fA-F]+))?",
        body,
    )
    return {
        "material_guid": material_guid,
        "material_path": str(material_path.resolve()),
        "material_unity_path": unity_relative(material_path),
        "material_sha256": sha256(material_path),
        "material_render_queue": render_queue,
        "shader_file_id": shader["file_id"],
        "shader_guid": shader["guid"],
        "shader_path": str(shader_path.resolve()) if shader_path else "builtin",
        "shader_unity_path": unity_relative(shader_path) if shader_path else "builtin",
        "shader_sha256": sha256(shader_path) if shader_path else "builtin",
        "texture_guid": texture_guid,
        "source_texture_path": str(texture_path.resolve()),
        "source_texture_unity_path": unity_relative(texture_path),
        "source_texture_sha256": sha256(texture_path),
        "mask_texture_file_id": int(mask_match.group(1)) if mask_match else 0,
        "mask_texture_guid": (mask_match.group(2) or "").lower() if mask_match else "",
        "material_color_1": saved_color("_Color_1"),
        "material_color_2": saved_color("_Color_2"),
    }


def source_rgb_semantics(target, material, failures):
    if target["role"] != "road":
        return {
            "source_rgb_policy": "authored texture RGB through Unity built-in transparent material",
            "emissive_rgb_multiplier": [1.0, 1.0, 1.0],
        }

    if material.get("shader_guid") != target.get("expected_shader_guid"):
        failures.append("Unity road shader GUID drifted: {0}".format(material.get("shader_guid")))
    if material.get("mask_texture_file_id") or material.get("mask_texture_guid"):
        failures.append("Unity road unexpectedly has an authored _Mask texture")
    color_1 = material.get("material_color_1")
    color_2 = material.get("material_color_2")
    if not color_1 or not color_2:
        failures.append("Unity road cm_city_mask material colors are missing")
        return {
            "source_rgb_policy": "invalid missing cm_city_mask colors",
            "emissive_rgb_multiplier": [1.0, 1.0, 1.0],
        }

    # cm_city_mask samples the default white _Mask when no mask texture is
    # assigned. That makes tmpvar_5=1 and mask.g=1, so the RGB term becomes
    # MainTex * (-1 + 2*Color_1 + 2*Color_2) * the daytime _SpriteColor.
    multiplier = [
        (-1.0 + 2.0 * color_1[index] + 2.0 * color_2[index]) * MAIN_CITY_SPRITE_COLOR[index]
        for index in range(3)
    ]
    return {
        "source_rgb_policy": "cm_city_mask default-white mask, material _Color_1/_Color_2, daytime _SpriteColor",
        "emissive_rgb_multiplier": multiplier,
    }


def unity_to_ue(point):
    return [
        -point[0] * UE_UNITS_PER_UNITY_UNIT,
        point[2] * UE_UNITS_PER_UNITY_UNIT,
        point[1] * UE_UNITS_PER_UNITY_UNIT,
    ]


def export_unity_plane_obj(path, object_name, world_matrix):
    desired_ue_vertices = []
    uvs = []
    for z_index in range(11):
        for x_index in range(11):
            local = [-5.0 + float(x_index), 0.0, -5.0 + float(z_index)]
            desired_ue_vertices.append(unity_to_ue(transform_point(world_matrix, local)))
            uvs.append([float(x_index) / 10.0, float(z_index) / 10.0])

    # UE 5.8's legacy FbxFactory reflects the OBJ Y axis while importing.  The
    # previous support-layer exporter wrote final UE coordinates directly, so
    # the imported road and authored wall-shadow silhouettes were mirrored in
    # place after their bounds centers were aligned.  Pre-reflect the OBJ input
    # so the imported vertices land at the Unity-derived UE world positions.
    obj_input_vertices = [
        [vertex[0], -vertex[1], vertex[2]] for vertex in desired_ue_vertices
    ]

    faces = []
    for z_index in range(10):
        for x_index in range(10):
            lower_left = z_index * 11 + x_index
            upper_left = (z_index + 1) * 11 + x_index
            upper_right = upper_left + 1
            lower_right = lower_left + 1
            faces.append((lower_left, upper_left, upper_right))
            faces.append((lower_left, upper_right, lower_right))

    lines = [
        "# Unity built-in Plane 10209 baked into desired UE centimeters",
        "# {0}".format(OBJ_IMPORT_AXIS_COMPENSATION),
        "# Runtime CityBuildingContainer yaw={0} runtime_lod_scale={1}".format(
            CONTAINER_YAW_DEGREES,
            RUNTIME_LOD_SCALE,
        ),
        "o {0}".format(object_name),
    ]
    for vertex in obj_input_vertices:
        lines.append("v {0:.9f} {1:.9f} {2:.9f}".format(*vertex))
    for uv in uvs:
        lines.append("vt {0:.9f} {1:.9f}".format(*uv))
    lines.append("vn 0.000000000 0.000000000 1.000000000")
    for face in faces:
        values = [index + 1 for index in face]
        lines.append("f {0}/{0}/1 {1}/{1}/1 {2}/{2}/1".format(*values))
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8")

    mins = [min(vertex[axis] for vertex in desired_ue_vertices) for axis in range(3)]
    maxs = [max(vertex[axis] for vertex in desired_ue_vertices) for axis in range(3)]
    center = [(mins[axis] + maxs[axis]) * 0.5 for axis in range(3)]
    extents = [(maxs[axis] - mins[axis]) * 0.5 for axis in range(3)]
    obj_mins = [min(vertex[axis] for vertex in obj_input_vertices) for axis in range(3)]
    obj_maxs = [max(vertex[axis] for vertex in obj_input_vertices) for axis in range(3)]
    return {
        "vertex_count": len(desired_ue_vertices),
        "triangle_count": len(faces),
        "ue_bounds_center": center,
        "ue_bounds_extents": extents,
        "ue_bounds_min": mins,
        "ue_bounds_max": maxs,
        "obj_input_bounds_center": [
            (obj_mins[axis] + obj_maxs[axis]) * 0.5 for axis in range(3)
        ],
        "obj_input_bounds_extents": [
            (obj_maxs[axis] - obj_mins[axis]) * 0.5 for axis in range(3)
        ],
        "obj_import_axis_compensation": OBJ_IMPORT_AXIS_COMPENSATION,
    }


def find_boundary_owner():
    state = json.loads(INITIAL_CITY_STATE_PATH.read_text(encoding="utf-8-sig"))
    owners = [item for item in state.get("buildings", []) if int(item.get("type_id", 0)) == 6]
    if len(owners) != 1:
        raise RuntimeError("Expected exactly one type 6 InitBuilding owner, found {0}".format(len(owners)))
    return owners[0]


def build():
    required = (CITY_WALL_PREFAB, INITIAL_CITY_STATE_PATH)
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise FileNotFoundError("Missing Unity runtime support-layer source(s): {0}".format(missing))

    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    prefab = parse_prefab(CITY_WALL_PREFAB)
    guid_index = build_guid_index()
    owner = find_boundary_owner()
    world_cache = {}
    records = []
    failures = []

    game_objects_by_name = {}
    for game_object in prefab["game_objects"].values():
        game_objects_by_name.setdefault(game_object["name"], []).append(game_object)

    for target in TARGETS:
        candidates = game_objects_by_name.get(target["object_name"], [])
        if len(candidates) != 1:
            failures.append("expected exactly one Unity object named {0}, found {1}".format(
                target["object_name"],
                len(candidates),
            ))
            continue

        game_object = candidates[0]
        transform = prefab["transform_by_game_object"].get(game_object["file_id"])
        mesh_filter = prefab["mesh_filters"].get(game_object["file_id"])
        renderer = prefab["mesh_renderers"].get(game_object["file_id"])
        if not transform or not mesh_filter or not renderer:
            failures.append("Unity object {0} is missing Transform/MeshFilter/MeshRenderer".format(target["object_name"]))
            continue

        mesh = mesh_filter["mesh"]
        if mesh["file_id"] != UNITY_BUILTIN_PLANE_FILE_ID or mesh["guid"] != UNITY_BUILTIN_GUID:
            failures.append("Unity object {0} must use built-in Plane 10209".format(target["object_name"]))
        if not active_in_hierarchy(prefab, transform["file_id"]):
            failures.append("Unity object {0} is not active in hierarchy".format(target["object_name"]))
        if not renderer["enabled"]:
            failures.append("Unity object {0} MeshRenderer is disabled".format(target["object_name"]))
        if len(renderer["materials"]) != 1:
            failures.append("Unity object {0} must have exactly one material".format(target["object_name"]))
            continue

        material_guid = renderer["materials"][0]["guid"]
        material = material_record(material_guid, guid_index)
        rgb_semantics = source_rgb_semantics(target, material, failures)
        if material_guid != target["expected_material_guid"]:
            failures.append("Unity object {0} material GUID drifted: {1}".format(target["object_name"], material_guid))
        if material["texture_guid"] != target["expected_texture_guid"]:
            failures.append("Unity object {0} texture GUID drifted: {1}".format(
                target["object_name"],
                material["texture_guid"],
            ))
        if material["material_render_queue"] != target["expected_render_queue"]:
            failures.append("Unity object {0} render queue drifted: {1}".format(
                target["object_name"],
                material["material_render_queue"],
            ))
        if renderer["sorting_layer_id"] != 0 or renderer["sorting_order"] != 0:
            failures.append("Unity object {0} has unsupported non-zero sorting layer/order".format(target["object_name"]))

        world_matrix = runtime_world_matrix(prefab, transform["file_id"], world_cache)
        obj_path = OUTPUT_ROOT / (target["layer_id"] + ".obj")
        geometry = export_unity_plane_obj(obj_path, target["object_name"], world_matrix)
        record = {
            "layer_id": target["layer_id"],
            "role": target["role"],
            "owner_init_id": int(owner.get("init_id", 0)),
            "owner_type_id": int(owner.get("type_id", 0)),
            "owner_model_id": owner.get("visual", {}).get("model_id", ""),
            "source_mode": "unity_active_renderer",
            "manual_layout": False,
            "source_prefab_path": str(CITY_WALL_PREFAB.resolve()),
            "source_prefab_unity_path": unity_relative(CITY_WALL_PREFAB),
            "source_prefab_sha256": sha256(CITY_WALL_PREFAB),
            "object_name": target["object_name"],
            "object_path": object_path(prefab, transform["file_id"]),
            "game_object_file_id": game_object["file_id"],
            "transform_file_id": transform["file_id"],
            "mesh_filter_file_id": mesh_filter["file_id"],
            "mesh_renderer_file_id": renderer["file_id"],
            "active_self": game_object["active_self"],
            "active_in_hierarchy": active_in_hierarchy(prefab, transform["file_id"]),
            "renderer_enabled": renderer["enabled"],
            "renderer_type": "MeshRenderer",
            "renderer_sorting_layer_id": renderer["sorting_layer_id"],
            "renderer_sorting_order": renderer["sorting_order"],
            "renderer_priority": renderer["renderer_priority"],
            "mesh_file_id": mesh["file_id"],
            "mesh_guid": mesh["guid"],
            "mesh_name": "Unity Builtin Plane",
            "local_position": transform["local_position"],
            "local_rotation_quaternion": transform["local_rotation"],
            "local_scale": transform["local_scale"],
            "runtime_world_matrix": world_matrix,
            "runtime_container_yaw": CONTAINER_YAW_DEGREES,
            "runtime_lod_scale": RUNTIME_LOD_SCALE,
            "runtime_root_transform_override": "local position zero, rotation identity, uniform LOD scale",
            "representation": "world_baked_unity_builtin_plane_obj",
            "obj_import_axis_compensation": OBJ_IMPORT_AXIS_COMPENSATION,
            "obj_path": str(obj_path.resolve()),
            "obj_project_path": project_relative(obj_path),
            "obj_sha256": sha256(obj_path),
            "source_alpha_policy": "preserve authored RGBA; no white-to-alpha or generated blur",
            **rgb_semantics,
            **material,
            **geometry,
        }
        records.append(record)

    role_counts = {}
    for record in records:
        role_counts[record["role"]] = role_counts.get(record["role"], 0) + 1

    if len(records) != 2:
        failures.append("expected exactly 2 active source support layers, found {0}".format(len(records)))
    if role_counts != {"road": 1, "authored_shadow": 1}:
        failures.append("unexpected active support-layer role counts: {0}".format(role_counts))
    if len({record["layer_id"] for record in records}) != len(records):
        failures.append("duplicate support-layer IDs")

    manifest = {
        "schema": SCHEMA,
        "scope_id": "initial_city_civ101_age1_city_default",
        "unity_root": str(UNITY_ROOT.resolve()),
        "source_initial_city_state": str(INITIAL_CITY_STATE_PATH.resolve()),
        "source_initial_city_state_sha256": sha256(INITIAL_CITY_STATE_PATH),
        "source_prefab": str(CITY_WALL_PREFAB.resolve()),
        "source_prefab_sha256": sha256(CITY_WALL_PREFAB),
        "container_yaw": CONTAINER_YAW_DEGREES,
        "runtime_lod_scale": RUNTIME_LOD_SCALE,
        "ue_units_per_unity_unit": UE_UNITS_PER_UNITY_UNIT,
        "main_city_sprite_color": MAIN_CITY_SPRITE_COLOR,
        "active_renderer_count": len(records),
        "role_counts": role_counts,
        "unclassified_active_renderer_count": 0,
        "records": records,
        "failures": failures,
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    verify = {
        "schema": "rok.unity_runtime_city_support_layers.verify.v2",
        "manifest": str(MANIFEST_PATH.resolve()),
        "manifest_sha256": sha256(MANIFEST_PATH),
        "active_renderer_count": len(records),
        "role_counts": role_counts,
        "manual_layout_count": sum(1 for record in records if record.get("manual_layout")),
        "unclassified_active_renderer_count": 0,
        "source_texture_policy": "authored RGBA preserved; source shader RGB semantics recorded",
        "obj_import_axis_compensation": OBJ_IMPORT_AXIS_COMPENSATION,
        "failures": failures,
    }
    VERIFY_PATH.write_text(json.dumps(verify, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Rok Unity runtime city support layers: records={0} failures={1}".format(len(records), len(failures)))
    if failures:
        for failure in failures:
            print("FAIL: " + failure)
        raise SystemExit(1)


if __name__ == "__main__":
    build()
