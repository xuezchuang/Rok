import copy
import hashlib
import json
import math
import re
from collections import Counter
from pathlib import Path
from rok_data_paths import resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
UNITY_ASSETS_ROOT = PROJECT_ROOT / "ref" / "client-unity" / "Assets"
MAP_TILE_GROUP_PATH = (
    UNITY_ASSETS_ROOT
    / "AddressableAssetsData"
    / "AssetGroups"
    / "MapTile.asset"
)
WORLD_MAP_MANIFEST_PATH = (
    PROJECT_ROOT
    / "Saved"
    / "RokUnityMigration"
    / "unity_world_map.json"
)
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
OUTPUT_PATH = OUTPUT_DIR / "unity_world_map_runtime_data.json"

SCHEMA = "rok.unity_world_map_runtime_data.v1"
LOD_MAX_DXF = [1500.0, 3000.0, 8000.0, 20000.0, 42500.0, 999999.0]
FAR_LODS = (2, 3, 4, 5)
KNOWN_EXTRA_LOD_ADDRESS_TEMPLATE = "Tile-rive-HD_YT1_lod1_lod{lod}"
KNOWN_EMPTY_SPRITE_TILE_TYPE = "Tile-map-pvp_2"
KNOWN_EMPTY_SPRITE_NODE = "Rive_CO_H_plane_LOD2"
UNITY_BUILTIN_GUIDS = {
    "0000000000000000e000000000000000",
    "0000000000000000f000000000000000",
}

UNITY_CLASS_NAMES = {
    1: "GameObject",
    4: "Transform",
    23: "MeshRenderer",
    33: "MeshFilter",
    64: "MeshCollider",
    114: "MonoBehaviour",
    198: "ParticleSystem",
    199: "ParticleSystemRenderer",
    212: "SpriteRenderer",
    1001: "PrefabInstance",
}

EXPECTED_STATISTICS = {
    "map_tile_count": 1600,
    "tile_type_count": 66,
    "detailed_template_count": 66,
    "detailed_lod_binding_count": 132,
    "far_lod_recipe_count": 264,
    "far_lod_source_prefab_count": 264,
    "node_count": 6127,
    "max_node_depth": 3,
    "mesh_renderer_count": 2721,
    "mesh_filter_count": 2721,
    "sprite_renderer_count": 2009,
    "particle_system_count": 18,
    "particle_system_renderer_count": 18,
    "mesh_collider_count": 6,
    "mono_behaviour_count": 3546,
    "empty_sprite_asset_count": 4,
    "prefab_instance_count": 0,
}

STANDARD_MONO_BEHAVIOUR_FIELDS = {
    "m_ObjectHideFlags",
    "m_CorrespondingSourceObject",
    "m_PrefabInstance",
    "m_PrefabAsset",
    "m_GameObject",
    "m_Enabled",
    "m_EditorHideFlags",
    "m_Script",
    "m_Name",
    "m_EditorClassIdentifier",
}


def rel(path):
    return str(path.relative_to(PROJECT_ROOT)).replace("\\", "/")


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_record(path):
    return {
        "path": rel(path),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def canonical_json_bytes(value):
    return json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")


def canonical_payload_hash(manifest):
    payload = copy.deepcopy(manifest)
    payload.pop("canonical_payload_sha256", None)
    return sha256_bytes(canonical_json_bytes(payload))


def canonical_number(value, digits=8):
    number = round(float(value), digits)
    if number == 0:
        return 0.0
    return number


def decimal_file_id(value):
    return str(int(str(value).strip()))


def parse_unity_yaml_objects(path):
    text = path.read_text(encoding="utf-8", errors="strict")
    parts = re.split(r"(?m)^--- !u!(\d+) &(-?\d+)\s*$", text)
    objects = []
    for index in range(1, len(parts), 3):
        objects.append(
            {
                "class_id": int(parts[index]),
                "file_id": decimal_file_id(parts[index + 1]),
                "body": parts[index + 2],
            }
        )
    if not objects:
        raise ValueError(f"No Unity YAML objects parsed from {rel(path)}")
    return objects


def parse_scalar(body, field_name, default=None, required=False):
    match = re.search(
        rf"(?m)^\s*{re.escape(field_name)}:\s*(.*?)\s*$",
        body,
    )
    if match:
        return match.group(1).strip()
    if required:
        raise ValueError(f"Missing field {field_name}")
    return default


def parse_int(body, field_name, default=None, required=False):
    value = parse_scalar(
        body,
        field_name,
        default=None,
        required=required,
    )
    if value is None:
        return default
    return int(value)


def parse_float(body, field_name, default=None, required=False):
    value = parse_scalar(
        body,
        field_name,
        default=None,
        required=required,
    )
    if value is None:
        return default
    return canonical_number(value)


def parse_inline_mapping(text):
    result = {}
    for key, value in re.findall(
        r"(?:^|,\s*)([A-Za-z_][A-Za-z0-9_]*):\s*([^,}]+)",
        text.strip().strip("{}"),
    ):
        result[key] = value.strip()
    return result


def parse_vector(body, field_name, axes, required=False):
    match = re.search(
        rf"(?m)^\s*{re.escape(field_name)}:\s*\{{([^}}]*)\}}\s*$",
        body,
    )
    if not match:
        if required:
            raise ValueError(f"Missing vector field {field_name}")
        return None
    values = parse_inline_mapping(match.group(1))
    result = {}
    for axis in axes:
        if axis not in values:
            raise ValueError(f"{field_name} is missing axis {axis}")
        result[axis] = canonical_number(values[axis])
    return result


def parse_reference_text(text, guid_index=None):
    values = parse_inline_mapping(text)
    if "fileID" not in values:
        raise ValueError(f"Unity reference is missing fileID: {text}")
    guid = values.get("guid", "").lower()
    record = {
        "file_id": decimal_file_id(values["fileID"]),
        "guid": guid,
        "type": int(values["type"]) if "type" in values else None,
    }
    if guid_index is not None:
        record["asset_path"] = guid_index.get(guid, "") if guid else ""
    record["is_null"] = record["file_id"] == "0" and not guid
    record["is_unity_builtin"] = guid in UNITY_BUILTIN_GUIDS
    return record


def parse_reference(body, field_name, guid_index=None, required=False):
    match = re.search(
        rf"(?m)^\s*{re.escape(field_name)}:\s*(\{{[^}}]*\}})",
        body,
    )
    if not match:
        if required:
            raise ValueError(f"Missing reference field {field_name}")
        return None
    return parse_reference_text(match.group(1), guid_index=guid_index)


def parse_yaml_list_block(body, field_name):
    lines = body.splitlines()
    for index, line in enumerate(lines):
        match = re.match(
            rf"^(\s*){re.escape(field_name)}:\s*$",
            line,
        )
        if not match:
            continue
        base_indent = len(match.group(1))
        result = []
        for child_line in lines[index + 1 :]:
            stripped = child_line.lstrip()
            indent = len(child_line) - len(stripped)
            if not stripped:
                result.append(child_line)
                continue
            if indent < base_indent:
                break
            if indent == base_indent and not stripped.startswith("-"):
                break
            result.append(child_line)
        return "\n".join(result)
    return ""


def parse_reference_list(body, field_name, guid_index=None):
    block = parse_yaml_list_block(body, field_name)
    return [
        parse_reference_text(match.group(0), guid_index=guid_index)
        for match in re.finditer(r"\{fileID:[^}]*\}", block)
    ]


def parse_game_object_components(body):
    block = parse_yaml_list_block(body, "m_Component")
    return [
        decimal_file_id(value)
        for value in re.findall(
            r"component:\s*\{fileID:\s*(-?\d+)\}",
            block,
        )
    ]


def parse_transform_children(body):
    block = parse_yaml_list_block(body, "m_Children")
    return [
        decimal_file_id(value)
        for value in re.findall(r"\{fileID:\s*(-?\d+)\}", block)
    ]


def parse_simple_yaml_value(value, guid_index):
    value = value.strip()
    if value.startswith("{") and value.endswith("}"):
        mapping = parse_inline_mapping(value)
        if "fileID" in mapping:
            return parse_reference_text(value, guid_index=guid_index)
        parsed = {}
        for key, item in mapping.items():
            try:
                parsed[key] = canonical_number(item)
            except ValueError:
                parsed[key] = item
        return parsed
    if value == "[]":
        return []
    if value in {"true", "false"}:
        return value == "true"
    if re.fullmatch(r"[-+]?\d+", value):
        return int(value)
    if re.fullmatch(r"[-+]?(?:\d+\.\d*|\.\d+)(?:[eE][-+]?\d+)?", value):
        return canonical_number(value)
    return value


def parse_mono_behaviour_custom_fields(body, guid_index):
    result = {}
    for line in body.splitlines():
        match = re.match(r"^  ([A-Za-z_][A-Za-z0-9_]*):\s*(.*?)\s*$", line)
        if not match:
            continue
        field_name = match.group(1)
        if field_name in STANDARD_MONO_BEHAVIOUR_FIELDS:
            continue
        value = match.group(2)
        result[field_name] = (
            parse_simple_yaml_value(value, guid_index)
            if value
            else {"nested_yaml": True}
        )
    return result


def build_guid_index():
    index = {}
    duplicate_guids = {}
    for meta_path in sorted(UNITY_ASSETS_ROOT.rglob("*.meta")):
        text = meta_path.read_text(encoding="utf-8", errors="ignore")
        match = re.search(r"(?m)^guid:\s*([0-9a-fA-F]+)\s*$", text)
        if not match:
            continue
        guid = match.group(1).lower()
        asset_path = meta_path.with_suffix("")
        if not asset_path.exists():
            continue
        relative_path = rel(asset_path)
        if guid in index and index[guid] != relative_path:
            duplicate_guids[guid] = sorted({index[guid], relative_path})
        else:
            index[guid] = relative_path
    if duplicate_guids:
        sample_guid = sorted(duplicate_guids)[0]
        raise ValueError(
            f"Duplicate Unity GUID {sample_guid}: "
            f"{duplicate_guids[sample_guid]}"
        )
    return index


def parse_addressables():
    text = MAP_TILE_GROUP_PATH.read_text(encoding="utf-8", errors="strict")
    entries = []
    for guid, address in re.findall(
        r"(?m)^\s*-\s+m_GUID:\s*([0-9a-fA-F]+)\s*$"
        r"\s*^\s*m_Address:\s*(.*?)\s*$",
        text,
    ):
        entries.append(
            {
                "address": address.strip(),
                "guid": guid.lower(),
            }
        )
    if not entries:
        raise ValueError(f"No entries parsed from {rel(MAP_TILE_GROUP_PATH)}")
    by_address = {}
    for entry in entries:
        address = entry["address"]
        if address in by_address:
            raise ValueError(f"Duplicate MapTile address: {address}")
        by_address[address] = entry
    return entries, by_address


def quaternion_transform_matrix(position, rotation, scale):
    x = float(rotation["x"])
    y = float(rotation["y"])
    z = float(rotation["z"])
    w = float(rotation["w"])
    sx = float(scale["x"])
    sy = float(scale["y"])
    sz = float(scale["z"])
    tx = float(position["x"])
    ty = float(position["y"])
    tz = float(position["z"])

    xx = x * x
    yy = y * y
    zz = z * z
    xy = x * y
    xz = x * z
    yz = y * z
    wx = w * x
    wy = w * y
    wz = w * z

    rotation_matrix = [
        [1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz), 2.0 * (xz + wy)],
        [2.0 * (xy + wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx)],
        [2.0 * (xz - wy), 2.0 * (yz + wx), 1.0 - 2.0 * (xx + yy)],
    ]
    matrix = [
        [
            rotation_matrix[0][0] * sx,
            rotation_matrix[0][1] * sy,
            rotation_matrix[0][2] * sz,
            tx,
        ],
        [
            rotation_matrix[1][0] * sx,
            rotation_matrix[1][1] * sy,
            rotation_matrix[1][2] * sz,
            ty,
        ],
        [
            rotation_matrix[2][0] * sx,
            rotation_matrix[2][1] * sy,
            rotation_matrix[2][2] * sz,
            tz,
        ],
        [0.0, 0.0, 0.0, 1.0],
    ]
    return matrix


def multiply_matrices(left, right):
    return [
        [
            sum(left[row][index] * right[index][column] for index in range(4))
            for column in range(4)
        ]
        for row in range(4)
    ]


def serialized_matrix(matrix):
    return [
        [canonical_number(value) for value in row]
        for row in matrix
    ]


def yaw_transform_matrix(position, rotation_y_degrees, scale):
    radians = math.radians(float(rotation_y_degrees))
    half = radians * 0.5
    rotation = {
        "x": 0.0,
        "y": math.sin(half),
        "z": 0.0,
        "w": math.cos(half),
    }
    return quaternion_transform_matrix(position, rotation, scale)


def tile_template_to_world_matrix(tile, tile_size):
    rotation_y = float(tile["rotation_y"])
    radians = math.radians(rotation_y)
    cosine = math.cos(radians)
    sine = math.sin(radians)
    center = tile_size * 0.5
    translation_x = tile["x"] * tile_size + center - (
        cosine * center + sine * center
    )
    translation_z = tile["y"] * tile_size + center - (
        -sine * center + cosine * center
    )
    return [
        [cosine, 0.0, sine, translation_x],
        [0.0, 1.0, 0.0, 0.0],
        [-sine, 0.0, cosine, translation_z],
        [0.0, 0.0, 0.0, 1.0],
    ]


def component_base_record(unity_object, game_object_file_id):
    body = unity_object["body"]
    return {
        "type": UNITY_CLASS_NAMES.get(
            unity_object["class_id"],
            f"UnityClass{unity_object['class_id']}",
        ),
        "class_id": unity_object["class_id"],
        "file_id": unity_object["file_id"],
        "game_object_file_id": game_object_file_id,
        "enabled": parse_int(body, "m_Enabled", default=None),
        "serialized_body_sha256": sha256_bytes(body.encode("utf-8")),
    }


def renderer_fields(body, guid_index):
    return {
        "materials": parse_reference_list(
            body,
            "m_Materials",
            guid_index=guid_index,
        ),
        "cast_shadows": parse_int(body, "m_CastShadows", default=None),
        "receive_shadows": parse_int(body, "m_ReceiveShadows", default=None),
        "rendering_layer_mask": parse_int(
            body,
            "m_RenderingLayerMask",
            default=None,
        ),
        "renderer_priority": parse_int(
            body,
            "m_RendererPriority",
            default=None,
        ),
        "sorting_layer_id": parse_int(
            body,
            "m_SortingLayerID",
            default=None,
        ),
        "sorting_order": parse_int(body, "m_SortingOrder", default=None),
    }


def build_component_record(unity_object, game_object_file_id, guid_index):
    body = unity_object["body"]
    class_id = unity_object["class_id"]
    record = component_base_record(unity_object, game_object_file_id)

    if class_id == 23:
        record.update(renderer_fields(body, guid_index))
    elif class_id == 33:
        record["mesh"] = parse_reference(
            body,
            "m_Mesh",
            guid_index=guid_index,
            required=True,
        )
    elif class_id == 212:
        record.update(renderer_fields(body, guid_index))
        sprite = parse_reference(
            body,
            "m_Sprite",
            guid_index=guid_index,
            required=True,
        )
        record.update(
            {
                "sprite": sprite,
                "empty_asset": sprite["is_null"],
                "color": parse_vector(
                    body,
                    "m_Color",
                    ("r", "g", "b", "a"),
                    required=True,
                ),
                "flip_x": parse_int(body, "m_FlipX", default=0),
                "flip_y": parse_int(body, "m_FlipY", default=0),
                "draw_mode": parse_int(body, "m_DrawMode", default=0),
                "size": parse_vector(body, "m_Size", ("x", "y")),
                "mask_interaction": parse_int(
                    body,
                    "m_MaskInteraction",
                    default=0,
                ),
                "sprite_sort_point": parse_int(
                    body,
                    "m_SpriteSortPoint",
                    default=0,
                ),
            }
        )
        if record["empty_asset"]:
            record["empty_asset_policy"] = (
                "explicit_source_no_asset_render_noop"
            )
    elif class_id == 198:
        record.update(
            {
                "length_in_seconds": parse_float(
                    body,
                    "lengthInSec",
                    required=True,
                ),
                "simulation_speed": parse_float(
                    body,
                    "simulationSpeed",
                    required=True,
                ),
                "looping": parse_int(body, "looping", required=True),
                "prewarm": parse_int(body, "prewarm", required=True),
                "play_on_awake": parse_int(
                    body,
                    "playOnAwake",
                    required=True,
                ),
                "scaling_mode": parse_int(
                    body,
                    "scalingMode",
                    required=True,
                ),
                "culling_mode": parse_int(
                    body,
                    "cullingMode",
                    required=True,
                ),
                "runtime_policy": (
                    "pending_niagara_or_explicit_visual_omission"
                ),
            }
        )
    elif class_id == 199:
        record.update(renderer_fields(body, guid_index))
        record.update(
            {
                "render_mode": parse_int(body, "m_RenderMode", default=None),
                "mesh": parse_reference(
                    body,
                    "m_Mesh",
                    guid_index=guid_index,
                ),
                "runtime_policy": (
                    "pending_niagara_or_explicit_visual_omission"
                ),
            }
        )
    elif class_id == 64:
        record.update(
            {
                "mesh": parse_reference(
                    body,
                    "m_Mesh",
                    guid_index=guid_index,
                    required=True,
                ),
                "material": parse_reference(
                    body,
                    "m_Material",
                    guid_index=guid_index,
                ),
                "is_trigger": parse_int(body, "m_IsTrigger", default=0),
                "convex": parse_int(body, "m_Convex", default=0),
                "cooking_options": parse_int(
                    body,
                    "m_CookingOptions",
                    default=None,
                ),
                "runtime_policy": (
                    "pending_collision_promotion_and_query_contract"
                ),
            }
        )
    elif class_id == 114:
        script = parse_reference(
            body,
            "m_Script",
            guid_index=guid_index,
            required=True,
        )
        script_path = script.get("asset_path", "")
        script_class = Path(script_path).stem if script_path else ""
        record.update(
            {
                "script": script,
                "script_class": script_class,
                "serialized_fields": parse_mono_behaviour_custom_fields(
                    body,
                    guid_index,
                ),
                "runtime_policy": (
                    "LockAngle45_camera_facing"
                    if script_class == "LockAngle45"
                    else (
                        "ObjectPoolItem_source_spawn_budget"
                        if script_class == "ObjectPoolItem"
                        else "recorded_source_behaviour_pending_adapter_review"
                    )
                ),
            }
        )
    else:
        record["runtime_policy"] = "unsupported_component_recorded"
    return record


def build_prefab_recipe(
    tile_type_id,
    lod,
    address_entry,
    guid_index,
):
    source_path_text = guid_index.get(address_entry["guid"], "")
    if not source_path_text:
        raise ValueError(
            f"Address {address_entry['address']} GUID "
            f"{address_entry['guid']} has no asset path"
        )
    source_path = resolve(source_path_text)
    if source_path.suffix.lower() != ".prefab":
        raise ValueError(
            f"Address {address_entry['address']} does not resolve to a prefab: "
            f"{source_path_text}"
        )

    objects = parse_unity_yaml_objects(source_path)
    objects_by_id = {item["file_id"]: item for item in objects}
    if len(objects_by_id) != len(objects):
        raise ValueError(f"Duplicate Unity object fileID in {source_path_text}")

    class_counts = Counter(item["class_id"] for item in objects)
    game_objects = {
        item["file_id"]: item
        for item in objects
        if item["class_id"] == 1
    }
    transforms = {
        item["file_id"]: item
        for item in objects
        if item["class_id"] == 4
    }
    if len(game_objects) != len(transforms):
        raise ValueError(
            f"{source_path_text} GameObject/Transform mismatch: "
            f"{len(game_objects)} != {len(transforms)}"
        )

    transform_to_game_object = {}
    game_object_components = {}
    for game_object_id, game_object in game_objects.items():
        component_ids = parse_game_object_components(game_object["body"])
        if not component_ids:
            raise ValueError(f"{source_path_text} GameObject {game_object_id} has no components")
        game_object_components[game_object_id] = component_ids
        transform_ids = [
            component_id
            for component_id in component_ids
            if component_id in transforms
        ]
        if len(transform_ids) != 1:
            raise ValueError(
                f"{source_path_text} GameObject {game_object_id} has "
                f"{len(transform_ids)} Transform components"
            )
        transform_to_game_object[transform_ids[0]] = game_object_id

    if len(transform_to_game_object) != len(transforms):
        raise ValueError(
            f"{source_path_text} contains unowned Transform objects"
        )

    root_transform_ids = []
    transform_records = {}
    for transform_id, transform_object in transforms.items():
        body = transform_object["body"]
        game_object_ref = parse_reference(
            body,
            "m_GameObject",
            required=True,
        )
        game_object_id = game_object_ref["file_id"]
        if transform_to_game_object.get(transform_id) != game_object_id:
            raise ValueError(
                f"{source_path_text} Transform {transform_id} ownership mismatch"
            )
        father = parse_reference(body, "m_Father", required=True)["file_id"]
        if father == "0":
            root_transform_ids.append(transform_id)
        elif father not in transforms:
            raise ValueError(
                f"{source_path_text} Transform {transform_id} has missing father "
                f"{father}"
            )
        children = parse_transform_children(body)
        if any(child not in transforms for child in children):
            raise ValueError(
                f"{source_path_text} Transform {transform_id} has missing child"
            )
        position = parse_vector(
            body,
            "m_LocalPosition",
            ("x", "y", "z"),
            required=True,
        )
        rotation = parse_vector(
            body,
            "m_LocalRotation",
            ("x", "y", "z", "w"),
            required=True,
        )
        scale = parse_vector(
            body,
            "m_LocalScale",
            ("x", "y", "z"),
            required=True,
        )
        transform_records[transform_id] = {
            "file_id": transform_id,
            "game_object_file_id": game_object_id,
            "parent_transform_file_id": father,
            "child_transform_file_ids": children,
            "root_order": parse_int(body, "m_RootOrder", default=0),
            "local_position": position,
            "local_rotation_quaternion": rotation,
            "local_scale": scale,
            "_local_matrix": quaternion_transform_matrix(
                position,
                rotation,
                scale,
            ),
        }

    if len(root_transform_ids) != 1:
        raise ValueError(
            f"{source_path_text} expected one root Transform, found "
            f"{len(root_transform_ids)}"
        )

    component_counter = Counter()
    script_counter = Counter()
    empty_assets = []
    visited_transforms = set()
    max_depth = 0

    def build_node(transform_id, parent_world_matrix, parent_path, depth, parent_active):
        nonlocal max_depth
        if transform_id in visited_transforms:
            raise ValueError(
                f"{source_path_text} Transform cycle or duplicate {transform_id}"
            )
        visited_transforms.add(transform_id)
        max_depth = max(max_depth, depth)

        transform_record = transform_records[transform_id]
        game_object_id = transform_record["game_object_file_id"]
        game_object = game_objects[game_object_id]
        game_object_body = game_object["body"]
        name = parse_scalar(
            game_object_body,
            "m_Name",
            required=True,
        )
        active_self = parse_int(
            game_object_body,
            "m_IsActive",
            required=True,
        ) != 0
        active_in_hierarchy = parent_active and active_self
        path_segment = f"{name}[{game_object_id}]"
        node_path = (
            f"{parent_path}/{path_segment}"
            if parent_path
            else path_segment
        )
        local_matrix = transform_record["_local_matrix"]
        world_matrix = (
            multiply_matrices(parent_world_matrix, local_matrix)
            if parent_world_matrix is not None
            else local_matrix
        )

        component_records = []
        component_types = []
        for component_id in game_object_components[game_object_id]:
            component_object = objects_by_id.get(component_id)
            if component_object is None:
                raise ValueError(
                    f"{source_path_text} GameObject {game_object_id} references "
                    f"missing component {component_id}"
                )
            if component_object["class_id"] == 4:
                continue
            component_record = build_component_record(
                component_object,
                game_object_id,
                guid_index,
            )
            component_records.append(component_record)
            component_type = component_record["type"]
            component_types.append(component_type)
            component_counter[component_type] += 1
            if component_type == "MonoBehaviour":
                script_counter[component_record["script_class"]] += 1
            if component_type == "SpriteRenderer" and component_record["empty_asset"]:
                empty_assets.append(
                    {
                        "tile_type_id": tile_type_id,
                        "lod": lod,
                        "node_name": name,
                        "node_path": node_path,
                        "sprite_renderer_file_id": component_record["file_id"],
                        "policy": component_record["empty_asset_policy"],
                    }
                )

        if component_types.count("MeshRenderer") != component_types.count("MeshFilter"):
            raise ValueError(
                f"{source_path_text} node {node_path} MeshRenderer/MeshFilter "
                "pairing mismatch"
            )
        if component_types.count("ParticleSystem") != component_types.count(
            "ParticleSystemRenderer"
        ):
            raise ValueError(
                f"{source_path_text} node {node_path} ParticleSystem/Renderer "
                "pairing mismatch"
            )

        node = {
            "game_object_file_id": game_object_id,
            "name": name,
            "node_path": node_path,
            "depth": depth,
            "layer": parse_int(game_object_body, "m_Layer", required=True),
            "tag": parse_scalar(
                game_object_body,
                "m_TagString",
                required=True,
            ),
            "active_self": active_self,
            "active_in_hierarchy": active_in_hierarchy,
            "source_component_file_ids": game_object_components[game_object_id],
            "transform": {
                key: value
                for key, value in transform_record.items()
                if not key.startswith("_")
            },
            "components": component_records,
            "children": [],
        }
        node["transform"]["local_matrix"] = serialized_matrix(local_matrix)
        node["transform"]["world_matrix"] = serialized_matrix(world_matrix)
        node["children"] = [
            build_node(
                child_transform_id,
                world_matrix,
                node_path,
                depth + 1,
                active_in_hierarchy,
            )
            for child_transform_id in transform_record[
                "child_transform_file_ids"
            ]
        ]
        return node

    root = build_node(
        root_transform_ids[0],
        None,
        "",
        0,
        True,
    )
    if visited_transforms != set(transforms):
        missing = sorted(set(transforms) - visited_transforms)
        raise ValueError(
            f"{source_path_text} has unreachable Transforms: {missing[:4]}"
        )

    return {
        "recipe_id": f"{tile_type_id}:lod{lod}",
        "tile_type_id": tile_type_id,
        "lod": lod,
        "representation": "whole_tile_recursive_prefab_recipe",
        "address": address_entry["address"],
        "address_guid": address_entry["guid"],
        "source_prefab": source_record(source_path),
        "source_prefab_file_id_policy": "signed_decimal_string",
        "root_count": 1,
        "node_count": len(game_objects),
        "max_node_depth": max_depth,
        "unity_object_class_counts": {
            str(class_id): count
            for class_id, count in sorted(class_counts.items())
        },
        "component_counts": dict(sorted(component_counter.items())),
        "mono_behaviour_script_counts": dict(sorted(script_counter.items())),
        "empty_assets": empty_assets,
        "root": root,
    }


def build_detailed_template(template):
    tile_type_id = template["tile_id"]
    records = []

    for kind, source_records in (
        ("plane", template["planes"]),
        ("object", template["objects"]),
    ):
        for source in source_records:
            source_copy = copy.deepcopy(source)
            position = source_copy["unity_local_position"]
            source_scale = source_copy["scale"]
            source_rotation = source_copy["rotation_y"]
            transform_policy = (
                "position_only"
                if kind == "object"
                and source_copy.get("category") == "adorning"
                else "position_rotation_scale"
            )
            runtime_rotation = (
                0.0 if transform_policy == "position_only" else source_rotation
            )
            runtime_scale = (
                {"x": 1.0, "y": 1.0, "z": 1.0}
                if transform_policy == "position_only"
                else source_scale
            )
            source_copy.update(
                {
                    "kind": kind,
                    "unity_runtime_transform_policy": transform_policy,
                    "unity_runtime_rotation_y": runtime_rotation,
                    "unity_runtime_scale": runtime_scale,
                    "source_local_matrix": serialized_matrix(
                        yaw_transform_matrix(
                            position,
                            source_rotation,
                            source_scale,
                        )
                    ),
                    "runtime_local_matrix": serialized_matrix(
                        yaw_transform_matrix(
                            position,
                            runtime_rotation,
                            runtime_scale,
                        )
                    ),
                }
            )
            records.append(source_copy)

    return {
        "template_id": f"{tile_type_id}:detailed",
        "tile_type_id": tile_type_id,
        "representation": "tile_data_detailed_instance_recipe",
        "applies_to_lods": [0, 1],
        "map_usage_count": template["map_usage_count"],
        "plane_count": template["plane_count"],
        "object_count": template["object_count"],
        "instance_count": template["plane_count"] + template["object_count"],
        "instances": records,
    }


def build_map_record(world_manifest):
    source_map = world_manifest["map"]
    tile_size = world_manifest["coordinate_contract"][
        "tile_size_unity_units"
    ]
    tiles = []
    for tile in source_map["tiles"]:
        tile_copy = copy.deepcopy(tile)
        tile_copy["template_to_world_matrix"] = serialized_matrix(
            tile_template_to_world_matrix(tile, tile_size)
        )
        tiles.append(tile_copy)
    return {
        "tile_count": source_map["tile_count"],
        "width_tiles": source_map["width_tiles"],
        "height_tiles": source_map["height_tiles"],
        "tile_size_unity_units": tile_size,
        "tile_type_usage": copy.deepcopy(source_map["tile_type_usage"]),
        "tiles": tiles,
    }


def build_lod_policy(address_entries, tile_type_ids):
    address_names = {entry["address"] for entry in address_entries}
    levels = []
    previous_max = None
    for lod, max_dxf in enumerate(LOD_MAX_DXF):
        levels.append(
            {
                "lod": lod,
                "min_dxf_exclusive": previous_max,
                "max_dxf_inclusive": max_dxf,
                "selection_rule": "first_max_dxf_greater_than_or_equal",
                "representation": (
                    "detailed_template"
                    if lod in (0, 1)
                    else "whole_tile_recursive_prefab_recipe"
                ),
            }
        )
        previous_max = max_dxf

    far_addressable_coverage = {}
    for lod in FAR_LODS:
        suffix = f"_lod{lod}"
        lod_addresses = sorted(
            address
            for address in address_names
            if address.endswith(suffix)
        )
        expected_primary = {
            f"{tile_type_id}{suffix}"
            for tile_type_id in tile_type_ids
        }
        missing = sorted(expected_primary - set(lod_addresses))
        extras = sorted(set(lod_addresses) - expected_primary)
        expected_extra = KNOWN_EXTRA_LOD_ADDRESS_TEMPLATE.format(lod=lod)
        if missing:
            raise ValueError(
                f"LOD{lod} Addressables missing tile addresses: {missing}"
            )
        if extras != [expected_extra]:
            raise ValueError(
                f"LOD{lod} Addressables extras {extras} != "
                f"[{expected_extra}]"
            )
        far_addressable_coverage[str(lod)] = {
            "address_count": len(lod_addresses),
            "primary_tile_address_count": len(expected_primary),
            "extra_addresses": extras,
        }

    return {
        "selection_semantics": "first_max_inclusive",
        "dxf_thresholds": LOD_MAX_DXF,
        "levels": levels,
        "detailed_template_lods": [0, 1],
        "far_whole_tile_prefab_lods": list(FAR_LODS),
        "far_addressable_coverage": far_addressable_coverage,
    }


def load_world_manifest():
    if not WORLD_MAP_MANIFEST_PATH.exists():
        raise ValueError(
            f"Missing source world-map manifest: {WORLD_MAP_MANIFEST_PATH}"
        )
    manifest = json.loads(
        WORLD_MAP_MANIFEST_PATH.read_text(encoding="utf-8")
    )
    if manifest.get("schema") != "rok.unity_world_map.v1":
        raise ValueError(
            f"Unexpected source world-map schema: {manifest.get('schema')}"
        )
    for role, record in manifest.get("sources", {}).items():
        source_path = resolve(record.get("path", ""))
        if not source_path.exists():
            raise ValueError(
                f"World-map source {role} is missing: {record.get('path')}"
            )
        if source_path.stat().st_size != record.get("size"):
            raise ValueError(
                f"World-map source {role} size changed: {record.get('path')}"
            )
        if sha256(source_path) != record.get("sha256"):
            raise ValueError(
                f"World-map source {role} hash changed: {record.get('path')}"
            )
    return manifest


def build_runtime_manifest():
    world_manifest = load_world_manifest()
    address_entries, address_by_name = parse_addressables()
    guid_index = build_guid_index()
    source_templates = sorted(
        world_manifest["tile_templates"],
        key=lambda item: item["tile_id"],
    )
    tile_type_ids = [item["tile_id"] for item in source_templates]
    if len(tile_type_ids) != len(set(tile_type_ids)):
        raise ValueError("Duplicate source tile template ids")

    lod_policy = build_lod_policy(address_entries, tile_type_ids)
    detailed_templates = [
        build_detailed_template(template)
        for template in source_templates
    ]

    far_lod_recipes = []
    for tile_type_id in tile_type_ids:
        for lod in FAR_LODS:
            address = f"{tile_type_id}_lod{lod}"
            address_entry = address_by_name.get(address)
            if not address_entry:
                raise ValueError(f"Missing MapTile address {address}")
            far_lod_recipes.append(
                build_prefab_recipe(
                    tile_type_id,
                    lod,
                    address_entry,
                    guid_index,
                )
            )

    detailed_by_tile = {
        item["tile_type_id"]: item
        for item in detailed_templates
    }
    recipe_by_key = {
        (item["tile_type_id"], item["lod"]): item
        for item in far_lod_recipes
    }
    tile_types = []
    for tile_type_id in tile_type_ids:
        detailed = detailed_by_tile[tile_type_id]
        bindings = [
            {
                "lod": lod,
                "representation": "detailed_template",
                "template_id": detailed["template_id"],
            }
            for lod in (0, 1)
        ]
        bindings.extend(
            {
                "lod": lod,
                "representation": "whole_tile_recursive_prefab_recipe",
                "recipe_id": recipe_by_key[(tile_type_id, lod)][
                    "recipe_id"
                ],
            }
            for lod in FAR_LODS
        )
        tile_types.append(
            {
                "tile_type_id": tile_type_id,
                "map_usage_count": detailed["map_usage_count"],
                "lod_bindings": bindings,
            }
        )

    aggregate_class_counts = Counter()
    aggregate_component_counts = Counter()
    aggregate_script_counts = Counter()
    empty_assets = []
    node_count = 0
    max_node_depth = 0
    for recipe in far_lod_recipes:
        node_count += recipe["node_count"]
        max_node_depth = max(max_node_depth, recipe["max_node_depth"])
        aggregate_class_counts.update(
            {
                int(class_id): count
                for class_id, count in recipe[
                    "unity_object_class_counts"
                ].items()
            }
        )
        aggregate_component_counts.update(recipe["component_counts"])
        aggregate_script_counts.update(
            recipe["mono_behaviour_script_counts"]
        )
        empty_assets.extend(recipe["empty_assets"])

    statistics = {
        "map_tile_count": world_manifest["map"]["tile_count"],
        "tile_type_count": len(tile_type_ids),
        "detailed_template_count": len(detailed_templates),
        "detailed_lod_binding_count": len(detailed_templates) * 2,
        "far_lod_recipe_count": len(far_lod_recipes),
        "far_lod_source_prefab_count": len(
            {item["source_prefab"]["path"] for item in far_lod_recipes}
        ),
        "node_count": node_count,
        "max_node_depth": max_node_depth,
        "mesh_renderer_count": aggregate_component_counts["MeshRenderer"],
        "mesh_filter_count": aggregate_component_counts["MeshFilter"],
        "sprite_renderer_count": aggregate_component_counts["SpriteRenderer"],
        "particle_system_count": aggregate_component_counts["ParticleSystem"],
        "particle_system_renderer_count": aggregate_component_counts[
            "ParticleSystemRenderer"
        ],
        "mesh_collider_count": aggregate_component_counts["MeshCollider"],
        "mono_behaviour_count": aggregate_component_counts["MonoBehaviour"],
        "empty_sprite_asset_count": len(empty_assets),
        "prefab_instance_count": aggregate_class_counts[1001],
        "unity_object_class_counts": {
            str(class_id): count
            for class_id, count in sorted(aggregate_class_counts.items())
        },
        "mono_behaviour_script_counts": dict(
            sorted(aggregate_script_counts.items())
        ),
        "far_lod_addressable_count_by_lod": {
            str(lod): lod_policy["far_addressable_coverage"][str(lod)][
                "address_count"
            ]
            for lod in FAR_LODS
        },
    }

    for key, expected in EXPECTED_STATISTICS.items():
        if statistics.get(key) != expected:
            raise ValueError(
                f"Unexpected {key}: {statistics.get(key)} != {expected}"
            )

    expected_empty_assets = [
        {
            "tile_type_id": KNOWN_EMPTY_SPRITE_TILE_TYPE,
            "lod": lod,
            "node_name": KNOWN_EMPTY_SPRITE_NODE,
        }
        for lod in FAR_LODS
    ]
    actual_empty_assets = [
        {
            "tile_type_id": item["tile_type_id"],
            "lod": item["lod"],
            "node_name": item["node_name"],
        }
        for item in empty_assets
    ]
    if actual_empty_assets != expected_empty_assets:
        raise ValueError(
            f"Unexpected empty SpriteRenderer assets: {actual_empty_assets}"
        )

    pending_policies = {
        "particle_systems": {
            "status": "pending",
            "component_count": statistics["particle_system_count"],
            "renderer_count": statistics[
                "particle_system_renderer_count"
            ],
            "required_decision": (
                "promote source effects to Niagara or explicitly accept "
                "visual omission per recipe"
            ),
        },
        "mesh_colliders": {
            "status": "pending",
            "component_count": statistics["mesh_collider_count"],
            "required_decision": (
                "define cooked runtime collision/query policy before "
                "streaming promotion"
            ),
        },
        "empty_sprite_assets": {
            "status": "source_explicit_noop",
            "component_count": statistics["empty_sprite_asset_count"],
            "known_source_contract": (
                "Tile-map-pvp_2/Rive_CO_H_plane_LOD2 has a null sprite "
                "in each LOD2-5 prefab"
            ),
            "records": empty_assets,
        },
    }

    manifest = {
        "schema": SCHEMA,
        "sources": {
            "world_map_manifest": source_record(WORLD_MAP_MANIFEST_PATH),
            "map_tile_addressables": source_record(MAP_TILE_GROUP_PATH),
        },
        "coordinate_contract": copy.deepcopy(
            world_manifest["coordinate_contract"]
        ),
        "lod_policy": lod_policy,
        "map": build_map_record(world_manifest),
        "detailed_templates": detailed_templates,
        "far_lod_recipes": far_lod_recipes,
        "tile_types": tile_types,
        "statistics": statistics,
        "pending_policies": pending_policies,
        "runtime_readiness": {
            "runtime_ready": False,
            "blockers": [
                "particle_systems_pending_niagara_or_omission_policy",
                "mesh_colliders_pending_runtime_collision_policy",
                "cookable_primary_data_asset_not_generated_by_this_script",
            ],
            "accepted_static_scope": (
                "deterministic Unity source extraction and static recipe "
                "verification only"
            ),
        },
    }
    manifest["canonical_payload_sha256"] = canonical_payload_hash(manifest)
    return manifest


def write_manifest(manifest):
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(
        json.dumps(
            manifest,
            ensure_ascii=False,
            sort_keys=True,
            indent=2,
            allow_nan=False,
        )
        + "\n",
        encoding="utf-8",
    )


def main():
    manifest = build_runtime_manifest()
    write_manifest(manifest)
    statistics = manifest["statistics"]
    print(
        "Rok Unity world-map runtime data built: "
        f"tiles={statistics['map_tile_count']} "
        f"tile_types={statistics['tile_type_count']} "
        f"detailed={statistics['detailed_template_count']} "
        f"far_recipes={statistics['far_lod_recipe_count']} "
        f"nodes={statistics['node_count']} "
        f"particles={statistics['particle_system_count']} "
        f"colliders={statistics['mesh_collider_count']} "
        f"runtime_ready={str(manifest['runtime_readiness']['runtime_ready']).lower()} "
        f"hash={manifest['canonical_payload_sha256']}"
    )


if __name__ == "__main__":
    main()
