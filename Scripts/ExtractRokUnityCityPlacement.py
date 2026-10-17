import csv
import hashlib
import json
import math
import os
import re
from pathlib import Path
from rok_data_paths import resolve

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
UNITY_REFERENCE_ROOT = Path(os.environ.get("ROK_UNITY_REFERENCE_ROOT", PROJECT_ROOT / "ref"))
if not UNITY_REFERENCE_ROOT.is_absolute():
    UNITY_REFERENCE_ROOT = resolve(UNITY_REFERENCE_ROOT)
UNITY_CLIENT_ROOT = UNITY_REFERENCE_ROOT / "client-unity"
UNITY_RESOURCE_ROOT = UNITY_REFERENCE_ROOT / "resources"
UNITY_SCENE_ROOT = UNITY_CLIENT_ROOT / "Assets/BundleAssets/Map/Scenes"
INIT_BUILDING_CSV = UNITY_REFERENCE_ROOT / "config-tables/编译客户端配置/CSV/InitBuilding.csv"
OUTPUT_ROOT = PROJECT_ROOT / "Saved/RokUnityPlacement"
OUTPUT_JSON = OUTPUT_ROOT / "unity_city_placement.json"
OUTPUT_CSV = OUTPUT_ROOT / "unity_city_placement.csv"
OUTPUT_GRID_CSV = OUTPUT_ROOT / "unity_init_building_grid.csv"

DEFAULT_SCENES = ["Map_TTS.unity"]
SCENE_NAMES = [
    item.strip()
    for item in os.environ.get("ROK_UNITY_PLACEMENT_SCENES", ",".join(DEFAULT_SCENES)).split(",")
    if item.strip()
]

# The extracted resource snapshot does not carry the original .meta files for
# ref/resources/Sprite. Keep the known Map_TTS object-to-resource chain explicit
# instead of guessing from visually similar files.
KNOWN_RESOURCE_SPRITES = {
    "Building_Castle_02": "Castle_6_5.png",
}

# The original Sprite .meta is absent from the extracted snapshot, but the
# exported Sprite/Texture2D pairs preserve an exact runtime-atlas relationship.
# Verify that relationship pixel-for-pixel before recording it as source
# evidence. Values below use Unity's bottom-left textureRect convention and a
# second top-left rect for image-tool reproducibility.
KNOWN_RUNTIME_ATLASES = {
    "Building_Castle_02": {
        "color_atlas": "Texture2D/city_build_6_5_0.png",
        "mask_atlas": "Texture2D/city_build_mask_6_5_0.png",
        "atlas_size": [1024, 1024],
        "texture_rect_unity_bottom_left": [513, 402, 418, 331],
        "texture_rect_top_left": [513, 291, 418, 331],
        "observed_crop_transform": "identity",
        "observed_rotation_degrees": 0,
        "observed_flip_x": False,
        "observed_flip_y": False,
        "unity_packing_metadata_status": "unresolved_in_snapshot",
    },
}

DOCUMENT_HEADER = re.compile(r"^--- !u!(?P<class_id>\d+) &(?P<file_id>-?\d+)(?: stripped)?\s*$", re.MULTILINE)
REFERENCE = re.compile(r"\{fileID:\s*(?P<file_id>-?\d+)(?:,\s*guid:\s*(?P<guid>[0-9a-f]+))?", re.IGNORECASE)
VECTOR_NUMBER = r"[-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?"


def display_path(path):
    path = Path(path)
    try:
        return path.resolve().relative_to(PROJECT_ROOT.resolve()).as_posix()
    except ValueError:
        return path.resolve().as_posix()


def pixel_sha256(image):
    return hashlib.sha256(image.convert("RGBA").tobytes()).hexdigest()


def file_sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def runtime_atlas_evidence(object_name, sprite_path, mask_path):
    specification = KNOWN_RUNTIME_ATLASES.get(normalize_name(object_name))
    if not specification:
        return None
    if not sprite_path or not mask_path:
        raise RuntimeError("Known runtime-atlas source is missing its Sprite or mask: {0}".format(object_name))

    color_atlas_path = UNITY_RESOURCE_ROOT / specification["color_atlas"]
    mask_atlas_path = UNITY_RESOURCE_ROOT / specification["mask_atlas"]
    for path in (color_atlas_path, mask_atlas_path):
        if not path.exists():
            raise RuntimeError("Missing known Unity runtime atlas: {0}".format(path))

    rect = specification["texture_rect_top_left"]
    x, y, width, height = [int(value) for value in rect]
    evidence = dict(specification)
    evidence.update({
        "color_atlas_path": display_path(color_atlas_path),
        "mask_atlas_path": display_path(mask_atlas_path),
        "sprite_path": display_path(sprite_path),
        "mask_path": display_path(mask_path),
        "coordinate_conventions": {
            "texture_rect_unity_bottom_left": "origin at atlas bottom-left",
            "texture_rect_top_left": "origin at decoded image top-left",
        },
        "source_sprite_meta_status": "unresolved_in_snapshot",
    })

    pairs = (
        ("color", Path(sprite_path), color_atlas_path),
        ("mask", Path(mask_path), mask_atlas_path),
    )
    for key, source_path, atlas_path in pairs:
        with Image.open(source_path) as source_image, Image.open(atlas_path) as atlas_image:
            source_rgba = source_image.convert("RGBA")
            atlas_rgba = atlas_image.convert("RGBA")
            if list(source_rgba.size) != [width, height]:
                raise RuntimeError("Unexpected {0} Sprite size for {1}: {2}".format(key, object_name, source_rgba.size))
            if list(atlas_rgba.size) != specification["atlas_size"]:
                raise RuntimeError("Unexpected {0} atlas size for {1}: {2}".format(key, object_name, atlas_rgba.size))
            crop = atlas_rgba.crop((x, y, x + width, y + height))
            exact = source_rgba.tobytes() == crop.tobytes()
            if not exact:
                raise RuntimeError("Known {0} Sprite is no longer an exact atlas crop for {1}".format(key, object_name))
            evidence["{0}_pixel_match".format(key)] = True
            evidence["{0}_pixel_sha256".format(key)] = pixel_sha256(source_rgba)
            evidence["{0}_source_file_sha256".format(key)] = file_sha256(source_path)
            evidence["{0}_atlas_file_sha256".format(key)] = file_sha256(atlas_path)
    return evidence


def parse_documents(path):
    text = Path(path).read_text(encoding="utf-8-sig")
    matches = list(DOCUMENT_HEADER.finditer(text))
    documents = {}
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        documents[int(match.group("file_id"))] = {
            "class_id": int(match.group("class_id")),
            "file_id": int(match.group("file_id")),
            "body": text[match.end():end],
        }
    return documents


def scalar(body, field, default=""):
    match = re.search(r"^\s*{0}:\s*(.*?)\s*$".format(re.escape(field)), body, re.MULTILINE)
    return match.group(1).strip() if match else default


def int_scalar(body, field, default=0):
    try:
        return int(float(scalar(body, field, default)))
    except (TypeError, ValueError):
        return default


def float_scalar(body, field, default=0.0):
    try:
        return float(scalar(body, field, default))
    except (TypeError, ValueError):
        return default


def reference(body, field):
    match = re.search(r"^\s*{0}:\s*(\{{.*?\}})\s*$".format(re.escape(field)), body, re.MULTILINE)
    if not match:
        return {"file_id": 0, "guid": ""}
    parsed = REFERENCE.search(match.group(1))
    if not parsed:
        return {"file_id": 0, "guid": ""}
    return {
        "file_id": int(parsed.group("file_id")),
        "guid": (parsed.group("guid") or "").lower(),
    }


def vector(body, field, dimensions=3, default=None):
    names = ("x", "y", "z", "w")[:dimensions]
    match = re.search(r"^\s*{0}:\s*\{{(?P<values>.*?)\}}\s*$".format(re.escape(field)), body, re.MULTILINE)
    if not match:
        return list(default or ([0.0] * dimensions))
    values = match.group("values")
    result = []
    for name in names:
        component = re.search(r"(?:^|,)\s*{0}:\s*({1})".format(name, VECTOR_NUMBER), values)
        result.append(float(component.group(1)) if component else 0.0)
    return result


def color(body, field, default=None):
    names = ("r", "g", "b", "a")
    match = re.search(r"^\s*{0}:\s*\{{(?P<values>.*?)\}}\s*$".format(re.escape(field)), body, re.MULTILINE)
    if not match:
        return list(default or [0.0, 0.0, 0.0, 0.0])
    values = match.group("values")
    result = []
    for name in names:
        component = re.search(r"(?:^|,)\s*{0}:\s*({1})".format(name, VECTOR_NUMBER), values)
        result.append(float(component.group(1)) if component else 0.0)
    return result


def quat_multiply(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return [
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    ]


def quat_rotate(q, value):
    x, y, z = value
    rotated = quat_multiply(quat_multiply(q, [x, y, z, 0.0]), [-q[0], -q[1], -q[2], q[3]])
    return rotated[:3]


def multiply3(a, b):
    return [float(a[index]) * float(b[index]) for index in range(3)]


def add3(a, b):
    return [float(a[index]) + float(b[index]) for index in range(3)]


def normalize_name(name):
    return re.sub(r" \(\d+\)$", "", name or "")


def category_for_name(name):
    normalized = normalize_name(name)
    if normalized.startswith("Building_"):
        return "city_building"
    if normalized.startswith("World_") or "CityWall" in normalized:
        return "city_boundary"
    if normalized.startswith("zhedang"):
        return "occluder"
    return "scene_sprite"


def build_guid_index(required_guids):
    unresolved = set(guid for guid in required_guids if guid)
    result = {}
    if not unresolved:
        return result
    asset_root = UNITY_CLIENT_ROOT / "Assets"
    for meta_path in asset_root.rglob("*.meta"):
        try:
            head = meta_path.read_text(encoding="utf-8-sig", errors="ignore")[:1024]
        except OSError:
            continue
        match = re.search(r"^guid:\s*([0-9a-f]+)\s*$", head, re.MULTILINE | re.IGNORECASE)
        if not match:
            continue
        guid = match.group(1).lower()
        if guid not in unresolved:
            continue
        source_path = Path(str(meta_path)[:-5])
        if source_path.exists():
            result[guid] = source_path
        unresolved.remove(guid)
        if not unresolved:
            break
    return result


def component_ids(game_object_body):
    block = re.search(r"m_Component:\s*\n(?P<items>(?:\s*- component:.*\n)+)", game_object_body)
    if not block:
        return []
    return [int(value) for value in re.findall(r"fileID:\s*(-?\d+)", block.group("items"))]


def parse_prefab_modifications(body):
    starts = list(re.finditer(r"^\s*- target:\s*\{fileID:", body, re.MULTILINE))
    result = []
    for index, start in enumerate(starts):
        end = starts[index + 1].start() if index + 1 < len(starts) else body.find("m_RemovedComponents", start.start())
        if end < 0:
            end = len(body)
        chunk = body[start.start():end]
        target = REFERENCE.search(chunk)
        if not target:
            continue
        object_reference = re.search(r"objectReference:\s*(\{.*?\})", chunk, re.DOTALL)
        object_match = REFERENCE.search(object_reference.group(1)) if object_reference else None
        result.append({
            "target_file_id": int(target.group("file_id")),
            "target_guid": (target.group("guid") or "").lower(),
            "property": scalar(chunk, "propertyPath"),
            "value": scalar(chunk, "value"),
            "object_file_id": int(object_match.group("file_id")) if object_match else 0,
            "object_guid": (object_match.group("guid") or "").lower() if object_match else "",
        })
    return result


def parse_scene(scene_path):
    documents = parse_documents(scene_path)
    game_objects = {}
    transforms = {}
    sprite_renderers = []
    cameras = []
    prefab_documents = []

    for file_id, document in documents.items():
        body = document["body"]
        if document["class_id"] == 1:
            game_objects[file_id] = {
                "file_id": file_id,
                "name": scalar(body, "m_Name"),
                "active": int_scalar(body, "m_IsActive", 1) != 0,
                "components": component_ids(body),
            }
        elif document["class_id"] == 4:
            transforms[file_id] = {
                "file_id": file_id,
                "game_object_file_id": reference(body, "m_GameObject")["file_id"],
                "parent_transform_file_id": reference(body, "m_Father")["file_id"],
                "local_position": vector(body, "m_LocalPosition", 3),
                "local_rotation": vector(body, "m_LocalRotation", 4, [0.0, 0.0, 0.0, 1.0]),
                "local_scale": vector(body, "m_LocalScale", 3, [1.0, 1.0, 1.0]),
                "local_euler": vector(body, "m_LocalEulerAnglesHint", 3),
            }
        elif document["class_id"] == 212:
            sprite = reference(body, "m_Sprite")
            material_match = re.search(r"m_Materials:\s*\n\s*-\s*(\{.*?\})", body)
            material_ref = REFERENCE.search(material_match.group(1)) if material_match else None
            sprite_renderers.append({
                "renderer_file_id": file_id,
                "game_object_file_id": reference(body, "m_GameObject")["file_id"],
                "enabled": int_scalar(body, "m_Enabled", 1) != 0,
                "sprite_file_id": sprite["file_id"],
                "sprite_guid": sprite["guid"],
                "color": color(body, "m_Color", [1.0, 1.0, 1.0, 1.0]),
                "draw_mode": int_scalar(body, "m_DrawMode"),
                "sprite_size": vector(body, "m_Size", 2),
                "flip_x": int_scalar(body, "m_FlipX") != 0,
                "flip_y": int_scalar(body, "m_FlipY") != 0,
                "mask_interaction": int_scalar(body, "m_MaskInteraction"),
                "sprite_sort_point": int_scalar(body, "m_SpriteSortPoint"),
                "sorting_layer_id": int_scalar(body, "m_SortingLayerID"),
                "sorting_layer": int_scalar(body, "m_SortingLayer"),
                "sorting_order": int_scalar(body, "m_SortingOrder"),
                "material_file_id": int(material_ref.group("file_id")) if material_ref else 0,
                "material_guid": (material_ref.group("guid") or "").lower() if material_ref else "",
            })
        elif document["class_id"] == 20:
            cameras.append({
                "camera_file_id": file_id,
                "game_object_file_id": reference(body, "m_GameObject")["file_id"],
                "enabled": int_scalar(body, "m_Enabled", 1) != 0,
                "vertical_fov": float_scalar(body, "field of view"),
                "fov_axis_mode": int_scalar(body, "m_FOVAxisMode"),
                "orthographic": int_scalar(body, "orthographic") != 0,
                "orthographic_size": float_scalar(body, "orthographic size"),
                "near_clip": float_scalar(body, "near clip plane"),
                "far_clip": float_scalar(body, "far clip plane"),
                "clear_flags": int_scalar(body, "m_ClearFlags"),
                "background_color": color(body, "m_BackGroundColor"),
                "hdr": int_scalar(body, "m_HDR") != 0,
                "msaa": int_scalar(body, "m_AllowMSAA") != 0,
                "occlusion_culling": int_scalar(body, "m_OcclusionCulling") != 0,
            })
        elif document["class_id"] == 1001:
            prefab_documents.append(document)

    transform_by_game_object = {
        transform["game_object_file_id"]: transform
        for transform in transforms.values()
        if transform["game_object_file_id"]
    }
    world_cache = {}

    def world_transform(transform_file_id):
        if transform_file_id in world_cache:
            return world_cache[transform_file_id]
        transform = transforms.get(transform_file_id)
        if not transform:
            return {
                "position": [0.0, 0.0, 0.0],
                "rotation": [0.0, 0.0, 0.0, 1.0],
                "scale": [1.0, 1.0, 1.0],
            }
        parent_id = transform["parent_transform_file_id"]
        if parent_id and parent_id in transforms:
            parent = world_transform(parent_id)
            scaled_local = multiply3(parent["scale"], transform["local_position"])
            result = {
                "position": add3(parent["position"], quat_rotate(parent["rotation"], scaled_local)),
                "rotation": quat_multiply(parent["rotation"], transform["local_rotation"]),
                "scale": multiply3(parent["scale"], transform["local_scale"]),
            }
        else:
            result = {
                "position": list(transform["local_position"]),
                "rotation": list(transform["local_rotation"]),
                "scale": list(transform["local_scale"]),
            }
        world_cache[transform_file_id] = result
        return result

    def hierarchy_path(game_object_file_id):
        names = []
        current_game_object_id = game_object_file_id
        seen = set()
        while current_game_object_id and current_game_object_id not in seen:
            seen.add(current_game_object_id)
            game_object = game_objects.get(current_game_object_id)
            if not game_object:
                break
            names.append(game_object["name"])
            transform = transform_by_game_object.get(current_game_object_id)
            if not transform:
                break
            parent = transforms.get(transform["parent_transform_file_id"])
            current_game_object_id = parent["game_object_file_id"] if parent else 0
        return "/".join(reversed(names))

    def effective_active(game_object_file_id):
        current_game_object_id = game_object_file_id
        seen = set()
        while current_game_object_id and current_game_object_id not in seen:
            seen.add(current_game_object_id)
            game_object = game_objects.get(current_game_object_id)
            if game_object and not game_object["active"]:
                return False
            transform = transform_by_game_object.get(current_game_object_id)
            parent = transforms.get(transform["parent_transform_file_id"]) if transform else None
            current_game_object_id = parent["game_object_file_id"] if parent else 0
        return True

    parsed_prefabs = []
    for document in prefab_documents:
        modifications = parse_prefab_modifications(document["body"])
        source_prefab = reference(document["body"], "m_SourcePrefab")
        parsed_prefabs.append((document, modifications, source_prefab))

    required_guids = {record["sprite_guid"] for record in sprite_renderers}
    required_guids.update(record["material_guid"] for record in sprite_renderers)
    required_guids.update(source_prefab["guid"] for _document, _modifications, source_prefab in parsed_prefabs)
    required_guids.update(
        item["object_guid"]
        for _document, modifications, _source_prefab in parsed_prefabs
        for item in modifications
        if item["object_guid"]
    )
    guid_index = build_guid_index(required_guids)

    placement_records = []
    for renderer in sprite_renderers:
        game_object = game_objects.get(renderer["game_object_file_id"], {})
        transform = transform_by_game_object.get(renderer["game_object_file_id"])
        if not transform:
            continue
        world = world_transform(transform["file_id"])
        name = game_object.get("name", "")
        sprite_path = guid_index.get(renderer["sprite_guid"])
        mapping_source = "guid_meta" if sprite_path else ""
        if not sprite_path:
            known_name = normalize_name(name)
            filename = KNOWN_RESOURCE_SPRITES.get(known_name)
            candidate = UNITY_RESOURCE_ROOT / "Sprite" / filename if filename else None
            if candidate and candidate.exists():
                sprite_path = candidate
                mapping_source = "known_object_name_map"
        mask_path = None
        if sprite_path:
            mask_candidate = sprite_path.with_name(sprite_path.stem + "_mask" + sprite_path.suffix)
            if mask_candidate.exists():
                mask_path = mask_candidate
        atlas_evidence = runtime_atlas_evidence(name, sprite_path, mask_path)
        material_path = guid_index.get(renderer["material_guid"])
        placement_records.append({
            "scene": scene_path.name,
            "source_scene_path": display_path(scene_path),
            "category": category_for_name(name),
            "active": bool(renderer["enabled"] and effective_active(renderer["game_object_file_id"])),
            "name": name,
            "hierarchy_path": hierarchy_path(renderer["game_object_file_id"]),
            "game_object_file_id": renderer["game_object_file_id"],
            "transform_file_id": transform["file_id"],
            "renderer_file_id": renderer["renderer_file_id"],
            "local_position": transform["local_position"],
            "local_rotation": transform["local_rotation"],
            "local_euler": transform["local_euler"],
            "local_scale": transform["local_scale"],
            "world_position": world["position"],
            "world_rotation": world["rotation"],
            "world_scale": world["scale"],
            "sprite_file_id": renderer["sprite_file_id"],
            "sprite_guid": renderer["sprite_guid"],
            "renderer_enabled": renderer["enabled"],
            "renderer_color": renderer["color"],
            "draw_mode": renderer["draw_mode"],
            "sprite_size": renderer["sprite_size"],
            "mapped_sprite_path": display_path(sprite_path) if sprite_path else "",
            "mapped_mask_path": display_path(mask_path) if mask_path else "",
            "mapping_source": mapping_source or "unresolved",
            "runtime_atlas": atlas_evidence,
            "source_sprite_meta_status": (
                atlas_evidence.get("source_sprite_meta_status")
                if atlas_evidence
                else ("resolved_by_guid_meta" if mapping_source == "guid_meta" else "unresolved_in_snapshot")
            ),
            "material_file_id": renderer["material_file_id"],
            "material_guid": renderer["material_guid"],
            "material_path": display_path(material_path) if material_path else "builtin_or_unresolved",
            "flip_x": renderer["flip_x"],
            "flip_y": renderer["flip_y"],
            "mask_interaction": renderer["mask_interaction"],
            "sprite_sort_point": renderer["sprite_sort_point"],
            "sorting_layer_id": renderer["sorting_layer_id"],
            "sorting_layer": renderer["sorting_layer"],
            "sorting_order": renderer["sorting_order"],
        })

    camera_records = []
    for camera in cameras:
        game_object = game_objects.get(camera["game_object_file_id"], {})
        transform = transform_by_game_object.get(camera["game_object_file_id"])
        if not transform:
            continue
        world = world_transform(transform["file_id"])
        camera_records.append({
            **camera,
            "scene": scene_path.name,
            "source_scene_path": display_path(scene_path),
            "name": game_object.get("name", ""),
            "active": bool(camera["enabled"] and effective_active(camera["game_object_file_id"])),
            "transform_file_id": transform["file_id"],
            "local_position": transform["local_position"],
            "local_rotation": transform["local_rotation"],
            "local_euler": transform["local_euler"],
            "world_position": world["position"],
            "world_rotation": world["rotation"],
        })

    prefab_records = []
    for document, modifications, source_prefab in parsed_prefabs:
        body = document["body"]
        parent_transform_id = reference(body, "m_TransformParent")["file_id"]
        parent_transform = transforms.get(parent_transform_id)
        parent_game_object_id = parent_transform["game_object_file_id"] if parent_transform else 0
        parent_active = effective_active(parent_game_object_id) if parent_game_object_id else True
        parent_path = hierarchy_path(parent_game_object_id) if parent_game_object_id else ""
        by_target = {}
        for modification in modifications:
            by_target.setdefault(modification["target_file_id"], {})[modification["property"]] = modification
        names = [item for item in modifications if item["property"] == "m_Name" and item["value"]]
        for name_modification in names:
            name = name_modification["value"]
            if "Ground" not in name and "ground" not in name:
                continue
            transform_target_id, transform_group = next(
                ((target_id, group) for target_id, group in by_target.items() if "m_LocalPosition.x" in group),
                (0, {}),
            )
            material_modification = next(
                (item for item in modifications if "m_Materials.Array.data" in item["property"] and item["object_guid"]),
                None,
            )
            material_path = guid_index.get(material_modification["object_guid"]) if material_modification else None
            source_prefab_path = guid_index.get(source_prefab["guid"])

            def modified_float(field, default):
                try:
                    return float(transform_group[field]["value"])
                except (KeyError, TypeError, ValueError):
                    return default

            active_modification = by_target.get(name_modification["target_file_id"], {}).get("m_IsActive")
            source_object_active = active_modification is None or active_modification["value"] != "0"
            explicit_scale_components = [
                axis
                for axis in ("x", "y", "z")
                if "m_LocalScale.{0}".format(axis) in transform_group
            ]
            active_target_file_ids = sorted(
                item["target_file_id"]
                for item in modifications
                if item["property"] == "m_IsActive" and item["value"] != "0"
            )
            inactive_target_file_ids = sorted(
                item["target_file_id"]
                for item in modifications
                if item["property"] == "m_IsActive" and item["value"] == "0"
            )
            prefab_records.append({
                "scene": scene_path.name,
                "source_scene_path": display_path(scene_path),
                "instance_file_id": document["file_id"],
                "name": name,
                "active": bool(source_object_active and parent_active),
                "source_object_active": source_object_active,
                "parent_active": parent_active,
                "parent_transform_file_id": parent_transform_id,
                "parent_hierarchy_path": parent_path,
                "source_object_file_id": name_modification["target_file_id"],
                "transform_target_file_id": transform_target_id,
                "active_target_file_ids": active_target_file_ids,
                "inactive_target_file_ids": inactive_target_file_ids,
                "source_prefab_guid": source_prefab["guid"],
                "source_prefab_path": display_path(source_prefab_path) if source_prefab_path else "unresolved_in_snapshot",
                "source_prefab_geometry_resolved": bool(source_prefab_path),
                "local_position": [
                    modified_float("m_LocalPosition.x", 0.0),
                    modified_float("m_LocalPosition.y", 0.0),
                    modified_float("m_LocalPosition.z", 0.0),
                ],
                "local_rotation": [
                    modified_float("m_LocalRotation.x", 0.0),
                    modified_float("m_LocalRotation.y", 0.0),
                    modified_float("m_LocalRotation.z", 0.0),
                    modified_float("m_LocalRotation.w", 1.0),
                ],
                "local_scale": [
                    modified_float("m_LocalScale.x", 1.0),
                    modified_float("m_LocalScale.y", 1.0),
                    modified_float("m_LocalScale.z", 1.0),
                ],
                "explicit_scale_components": explicit_scale_components,
                "inferred_scale_components": [axis for axis in ("x", "y", "z") if axis not in explicit_scale_components],
                "material_target_file_id": material_modification["target_file_id"] if material_modification else 0,
                "material_guid": material_modification["object_guid"] if material_modification else "",
                "material_path": display_path(material_path) if material_path else "unresolved_in_snapshot",
            })

    return placement_records, camera_records, prefab_records


def write_placement_csv(records):
    fields = [
        "scene", "source_scene_path", "category", "active", "name", "hierarchy_path",
        "game_object_file_id", "transform_file_id", "renderer_file_id",
        "local_position", "local_rotation", "local_euler", "local_scale",
        "world_position", "world_rotation", "world_scale",
        "sprite_file_id", "sprite_guid", "renderer_enabled", "renderer_color", "draw_mode", "sprite_size", "mapped_sprite_path", "mapped_mask_path",
        "mapping_source", "runtime_atlas", "source_sprite_meta_status", "material_file_id", "material_guid", "material_path",
        "flip_x", "flip_y", "mask_interaction", "sprite_sort_point", "sorting_layer_id", "sorting_layer", "sorting_order",
    ]
    with OUTPUT_CSV.open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for record in records:
            row = dict(record)
            for field in ("local_position", "local_rotation", "local_euler", "local_scale", "world_position", "world_rotation", "world_scale", "renderer_color", "sprite_size", "runtime_atlas"):
                row[field] = json.dumps(row[field], ensure_ascii=False, separators=(",", ":"))
            writer.writerow({field: row.get(field, "") for field in fields})


def write_init_building_grid():
    if not INIT_BUILDING_CSV.exists():
        raise RuntimeError("Missing Unity InitBuilding CSV: {0}".format(INIT_BUILDING_CSV))
    with INIT_BUILDING_CSV.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if rows and rows[0].get("ID") == "int":
        rows = rows[1:]
    fields = ["ID", "type", "level", "posX", "posY", "source_csv"]
    with OUTPUT_GRID_CSV.open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({
                "ID": row.get("ID", ""),
                "type": row.get("type", ""),
                "level": row.get("level", ""),
                "posX": row.get("posX", ""),
                "posY": row.get("posY", ""),
                "source_csv": display_path(INIT_BUILDING_CSV),
            })
    return len(rows)


def main():
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    placement_records = []
    camera_records = []
    prefab_records = []
    source_scenes = []
    for scene_name in SCENE_NAMES:
        scene_path = UNITY_SCENE_ROOT / scene_name
        if not scene_path.exists():
            raise RuntimeError("Missing Unity scene: {0}".format(scene_path))
        source_scenes.append(display_path(scene_path))
        placements, cameras, prefabs = parse_scene(scene_path)
        placement_records.extend(placements)
        camera_records.extend(cameras)
        prefab_records.extend(prefabs)

    placement_records.sort(key=lambda item: (item["scene"], item["hierarchy_path"], item["renderer_file_id"]))
    write_placement_csv(placement_records)
    init_grid_count = write_init_building_grid()
    manifest = {
        "schema": "rok.unity_city_placement.v3",
        "source_scenes": source_scenes,
        "source_init_building_csv": display_path(INIT_BUILDING_CSV),
        "placement_record_count": len(placement_records),
        "mapped_placement_count": sum(1 for item in placement_records if item["mapped_sprite_path"]),
        "unresolved_placement_count": sum(1 for item in placement_records if not item["mapped_sprite_path"]),
        "camera_record_count": len(camera_records),
        "prefab_instance_record_count": len(prefab_records),
        "init_grid_count": init_grid_count,
        "placements": placement_records,
        "cameras": camera_records,
        "prefab_instances": prefab_records,
        "outputs": {
            "placement_csv": display_path(OUTPUT_CSV),
            "init_building_grid_csv": display_path(OUTPUT_GRID_CSV),
        },
    }
    OUTPUT_JSON.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "output": display_path(OUTPUT_JSON),
        "placements": len(placement_records),
        "mapped": manifest["mapped_placement_count"],
        "cameras": len(camera_records),
        "prefab_instances": len(prefab_records),
        "init_grid": init_grid_count,
    }, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
