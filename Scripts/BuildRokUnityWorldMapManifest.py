import csv
import hashlib
import json
import math
import re
import struct
from collections import defaultdict
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
UNITY_ASSETS_ROOT = PROJECT_ROOT / "ref" / "client-unity" / "Assets"
ADDRESSABLE_GROUP_PATH = (
    UNITY_ASSETS_ROOT
    / "AddressableAssetsData"
    / "AssetGroups"
    / "MapData.asset"
)
ADDRESSABLE_IMPORT_SETTINGS_PATH = (
    UNITY_ASSETS_ROOT
    / "AddressableAssetsData"
    / "AddressableImportSettings.asset"
)
MAP_TILE_GROUP_PATH = (
    UNITY_ASSETS_ROOT
    / "AddressableAssetsData"
    / "AssetGroups"
    / "MapTile.asset"
)
MAP_DATA_ROOT = UNITY_ASSETS_ROOT / "BundleAssets" / "land" / "MapData"
MAP_BRIEF_PATH = MAP_DATA_ROOT / "map_4_data.bytes"
TILE_DATA_PATH = MAP_DATA_ROOT / "tile_data.bytes"
CAMERA_PARAM_PATH = (
    PROJECT_ROOT
    / "ref"
    / "config-tables"
    / "编译客户端配置"
    / "CSV"
    / "CameraParamLod.csv"
)
WORLD_SCENE_PATH = (
    UNITY_ASSETS_ROOT
    / "BundleAssets"
    / "Scenes"
    / "World.unity"
)
LIGHT_MANAGER_PATH = (
    UNITY_ASSETS_ROOT
    / "Scripts"
    / "Client"
    / "LightingManager"
    / "LightManager.cs"
)
GAME_EVENT_GLOBAL_MEDIATOR_PATH = (
    UNITY_ASSETS_ROOT
    / "Scripts"
    / "Hotfix"
    / "MVC"
    / "GlobalMediator"
    / "GameEventGlobalMediator.cs"
)
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
OUTPUT_JSON = OUTPUT_DIR / "unity_world_map.json"
OUTPUT_MD = OUTPUT_DIR / "unity_world_map.md"

MAP_BRIEF_GUID = "daf32c16c0da6254e887873a4be71320"
TILE_DATA_GUID = "ba3280ed96d34fc48a25b1a47227b268"
TILE_SIZE = 180.0
TILE_PIECES_PER_ROW = 3
DISPLAY_TO_UNITY_SCALE = 6.0
UE_UNITS_PER_UNITY_UNIT = 500.0
FOCUS_DISPLAY_X = 1146
FOCUS_DISPLAY_Y = 157
PROTOTYPE_RADIUS_TILES = 1


def rel(path):
    return str(path.relative_to(PROJECT_ROOT)).replace("\\", "/")


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


def parse_unity_yaml_objects(path):
    text = path.read_text(encoding="utf-8", errors="ignore")
    parts = re.split(r"(?m)^--- !u!(\d+) &(\d+)\s*$", text)
    objects = []
    for index in range(1, len(parts), 3):
        objects.append(
            {
                "class_id": int(parts[index]),
                "file_id": parts[index + 1],
                "body": parts[index + 2],
            }
        )
    return objects


def parse_unity_scalar(body, field_name):
    match = re.search(
        rf"(?m)^\s*{re.escape(field_name)}:\s*([-+0-9.eE]+)\s*$",
        body,
    )
    if not match:
        raise ValueError(
            f"{rel(WORLD_SCENE_PATH)} is missing scalar {field_name}"
        )
    return float(match.group(1))


def parse_unity_vector_record(body, field_name, axes):
    match = re.search(
        rf"(?m)^\s*{re.escape(field_name)}:\s*\{{([^}}]+)\}}\s*$",
        body,
    )
    if not match:
        raise ValueError(
            f"{rel(WORLD_SCENE_PATH)} is missing vector {field_name}"
        )
    values = {}
    for axis in axes:
        axis_match = re.search(
            rf"(?:^|,\s*){re.escape(axis)}:\s*([-+0-9.eE]+)",
            match.group(1),
        )
        if not axis_match:
            raise ValueError(
                f"{rel(WORLD_SCENE_PATH)} {field_name} is missing {axis}"
            )
        values[axis] = round(float(axis_match.group(1)), 8)
    return values


def parse_csharp_float_list(values_text):
    values = []
    for token in values_text.split(","):
        cleaned = token.strip().rstrip("fF")
        if cleaned:
            values.append(float(cleaned))
    return values


def parse_runtime_lighting_contract():
    light_manager_text = LIGHT_MANAGER_PATH.read_text(
        encoding="utf-8",
        errors="ignore",
    )
    event_text = GAME_EVENT_GLOBAL_MEDIATOR_PATH.read_text(
        encoding="utf-8",
        errors="ignore",
    )
    formula = (
        "m_sprite_color = RenderSettings.ambientLight + "
        "m_direction_light.intensity * m_direction_light.color * 0.75f;"
    )
    if formula not in light_manager_text:
        raise ValueError(
            f"{rel(LIGHT_MANAGER_PATH)} lacks the audited _SpriteColor formula"
        )
    camera_fill_match = re.search(
        r"m_camera_fill_base_color\s*=\s*new Color\(([^)]+)\)",
        light_manager_text,
    )
    if not camera_fill_match:
        raise ValueError(
            f"{rel(LIGHT_MANAGER_PATH)} lacks camera fill base color"
        )
    camera_fill_values = parse_csharp_float_list(
        camera_fill_match.group(1)
    )
    if len(camera_fill_values) != 4:
        raise ValueError(
            f"{rel(LIGHT_MANAGER_PATH)} camera fill color is incomplete"
        )

    setting_block = re.search(
        r"(?ms)private Color\[\]\[\]\s+lightSetting\s*=\s*new\s*\[\]\s*"
        r"\{(.*?)^\s*\};",
        event_text,
    )
    if not setting_block:
        raise ValueError(
            f"{rel(GAME_EVENT_GLOBAL_MEDIATOR_PATH)} lacks lightSetting"
        )
    color_values = [
        parse_csharp_float_list(match)
        for match in re.findall(
            r"new Color\(([^)]+)\)",
            setting_block.group(1),
        )
    ]
    if len(color_values) != 8 or any(
        len(values) != 3 for values in color_values
    ):
        raise ValueError(
            f"{rel(GAME_EVENT_GLOBAL_MEDIATOR_PATH)} has an unexpected "
            "day/night color table"
        )
    intensity_match = re.search(
        r"m_lightSettingValue\s*=\s*new\[\]\s*\{([^}]+)\}",
        event_text,
    )
    if not intensity_match:
        raise ValueError(
            f"{rel(GAME_EVENT_GLOBAL_MEDIATOR_PATH)} lacks "
            "m_lightSettingValue"
        )
    intensities = parse_csharp_float_list(intensity_match.group(1))
    if len(intensities) != 4:
        raise ValueError(
            f"{rel(GAME_EVENT_GLOBAL_MEDIATOR_PATH)} has an unexpected "
            "day/night intensity table"
        )

    phases = []
    for index, phase_name in enumerate(("dawn", "day", "dusk", "night")):
        ambient = color_values[index * 2]
        direction = color_values[index * 2 + 1]
        intensity = intensities[index]
        sprite_color = [
            round(
                ambient[channel]
                + intensity * direction[channel] * 0.75,
                8,
            )
            for channel in range(3)
        ]
        background_color = [
            round(sprite_color[channel] * camera_fill_values[channel], 8)
            for channel in range(3)
        ]
        phases.append(
            {
                "phase": phase_name,
                "ambient_color": {
                    "r": round(ambient[0], 8),
                    "g": round(ambient[1], 8),
                    "b": round(ambient[2], 8),
                    "a": 1.0,
                },
                "direction_color": {
                    "r": round(direction[0], 8),
                    "g": round(direction[1], 8),
                    "b": round(direction[2], 8),
                    "a": 1.0,
                },
                "direction_intensity": round(intensity, 8),
                "sprite_color": {
                    "r": sprite_color[0],
                    "g": sprite_color[1],
                    "b": sprite_color[2],
                    "a": 1.0,
                },
                "camera_background_color": {
                    "r": background_color[0],
                    "g": background_color[1],
                    "b": background_color[2],
                    "a": 1.0,
                },
            }
        )

    return {
        "light_manager_source": rel(LIGHT_MANAGER_PATH),
        "day_night_source": rel(GAME_EVENT_GLOBAL_MEDIATOR_PATH),
        "sprite_color_formula": (
            "ambient + direction_intensity * direction_color * 0.75"
        ),
        "camera_background_formula": (
            "sprite_color * camera_fill_base_color"
        ),
        "camera_fill_base_color": {
            "r": round(camera_fill_values[0], 8),
            "g": round(camera_fill_values[1], 8),
            "b": round(camera_fill_values[2], 8),
            "a": round(camera_fill_values[3], 8),
        },
        "prototype_phase": "day",
        "phases": phases,
    }


def parse_world_environment():
    objects = parse_unity_yaml_objects(WORLD_SCENE_PATH)
    render_settings = [
        item for item in objects if item["class_id"] == 104
    ]
    if len(render_settings) != 1:
        raise ValueError(
            f"{rel(WORLD_SCENE_PATH)} expected one RenderSettings object, "
            f"found {len(render_settings)}"
        )
    render_body = render_settings[0]["body"]

    game_objects = {}
    transforms = {}
    lights = {}
    for item in objects:
        body = item["body"]
        game_object_match = re.search(
            r"m_GameObject:\s*\{fileID:\s*(\d+)\}",
            body,
        )
        if item["class_id"] == 1:
            name_match = re.search(
                r"(?m)^\s*m_Name:\s*(.+?)\s*$",
                body,
            )
            if name_match:
                game_objects[item["file_id"]] = name_match.group(1).strip()
        elif item["class_id"] == 4 and game_object_match:
            transforms[game_object_match.group(1)] = body
        elif item["class_id"] == 108 and game_object_match:
            lights[game_object_match.group(1)] = body

    directional_ids = [
        file_id
        for file_id, name in game_objects.items()
        if name == "Directional Light"
        and file_id in transforms
        and file_id in lights
    ]
    if len(directional_ids) != 1:
        raise ValueError(
            f"{rel(WORLD_SCENE_PATH)} expected one Directional Light, "
            f"found {len(directional_ids)}"
        )
    directional_id = directional_ids[0]
    light_body = lights[directional_id]
    transform_body = transforms[directional_id]
    light_type = int(parse_unity_scalar(light_body, "m_Type"))
    if light_type != 1:
        raise ValueError(
            f"{rel(WORLD_SCENE_PATH)} Directional Light type is "
            f"{light_type}, expected 1"
        )

    skybox_match = re.search(
        r"(?m)^\s*m_SkyboxMaterial:\s*\{fileID:\s*(-?\d+),\s*"
        r"guid:\s*([0-9a-fA-F]+),\s*type:\s*(\d+)\}\s*$",
        render_body,
    )
    if not skybox_match:
        raise ValueError(
            f"{rel(WORLD_SCENE_PATH)} is missing skybox material identity"
        )
    shadow_type_match = re.search(
        r"(?ms)^\s*m_Shadows:\s*\n\s*m_Type:\s*(\d+)\s*$",
        light_body,
    )
    if not shadow_type_match:
        raise ValueError(
            f"{rel(WORLD_SCENE_PATH)} is missing directional shadow type"
        )

    return {
        "source_scene": rel(WORLD_SCENE_PATH),
        "render_settings": {
            "fog_enabled": bool(
                int(parse_unity_scalar(render_body, "m_Fog"))
            ),
            "ambient_mode": int(
                parse_unity_scalar(render_body, "m_AmbientMode")
            ),
            "ambient_intensity": parse_unity_scalar(
                render_body,
                "m_AmbientIntensity",
            ),
            "ambient_sky_color": parse_unity_vector_record(
                render_body,
                "m_AmbientSkyColor",
                ("r", "g", "b", "a"),
            ),
            "ambient_equator_color": parse_unity_vector_record(
                render_body,
                "m_AmbientEquatorColor",
                ("r", "g", "b", "a"),
            ),
            "ambient_ground_color": parse_unity_vector_record(
                render_body,
                "m_AmbientGroundColor",
                ("r", "g", "b", "a"),
            ),
            "reflection_intensity": parse_unity_scalar(
                render_body,
                "m_ReflectionIntensity",
            ),
            "skybox_material": {
                "file_id": int(skybox_match.group(1)),
                "guid": skybox_match.group(2).lower(),
                "type": int(skybox_match.group(3)),
                "is_unity_builtin_default": (
                    skybox_match.group(2).lower()
                    == "0000000000000000f000000000000000"
                ),
            },
        },
        "directional_light": {
            "name": "Directional Light",
            "type": "Directional",
            "color": parse_unity_vector_record(
                light_body,
                "m_Color",
                ("r", "g", "b", "a"),
            ),
            "intensity": parse_unity_scalar(light_body, "m_Intensity"),
            "shadow_type": int(shadow_type_match.group(1)),
            "unity_local_rotation_quaternion": parse_unity_vector_record(
                transform_body,
                "m_LocalRotation",
                ("x", "y", "z", "w"),
            ),
            "unity_local_euler_degrees": parse_unity_vector_record(
                transform_body,
                "m_LocalEulerAnglesHint",
                ("x", "y", "z"),
            ),
        },
        "runtime_lighting": parse_runtime_lighting_contract(),
    }


def parse_camera_parameters():
    with CAMERA_PARAM_PATH.open(
        "r",
        encoding="utf-8-sig",
        newline="",
    ) as handle:
        reader = csv.DictReader(handle)
        required = {
            "ID",
            "dist",
            "fov",
            "dxf",
            "forwardX",
            "forwardY",
            "forwardZ",
        }
        if not reader.fieldnames or not required.issubset(reader.fieldnames):
            raise ValueError(
                f"{rel(CAMERA_PARAM_PATH)} has unexpected columns: "
                f"{reader.fieldnames}"
            )
        records = []
        for row_number, row in enumerate(reader, start=2):
            camera_id = (row.get("ID") or "").strip()
            if not camera_id or camera_id == "string":
                continue
            try:
                distance = float(row["dist"])
                vertical_fov = float(row["fov"])
                dxf = float(row["dxf"])
                forward = vector(
                    row["forwardX"],
                    row["forwardY"],
                    row["forwardZ"],
                )
            except (TypeError, ValueError) as error:
                raise ValueError(
                    f"{rel(CAMERA_PARAM_PATH)} row {row_number} is invalid"
                ) from error
            if distance <= 0.0 or vertical_fov <= 0.0 or dxf <= 0.0:
                raise ValueError(
                    f"{rel(CAMERA_PARAM_PATH)} row {row_number} has "
                    "non-positive camera values"
                )
            if not math.isclose(
                distance * vertical_fov,
                dxf,
                rel_tol=0.0,
                abs_tol=0.01,
            ):
                raise ValueError(
                    f"{rel(CAMERA_PARAM_PATH)} row {row_number} dxf does "
                    "not match distance * fov"
                )
            records.append(
                {
                    "id": camera_id,
                    "distance_unity_units": distance,
                    "vertical_fov_degrees": vertical_fov,
                    "dxf": dxf,
                    "unity_forward": forward,
                }
            )
    camera_ids = [record["id"] for record in records]
    if len(camera_ids) != len(set(camera_ids)):
        raise ValueError(
            f"{rel(CAMERA_PARAM_PATH)} contains duplicate camera ids"
        )
    required_ids = {"limit_min", "map_tactical", "limit_max"}
    if not required_ids.issubset(camera_ids):
        raise ValueError(
            f"{rel(CAMERA_PARAM_PATH)} is missing required camera ids "
            f"{sorted(required_ids - set(camera_ids))}"
        )
    return sorted(records, key=lambda record: record["dxf"])


class DotNetBinaryReader:
    def __init__(self, path):
        self.path = path
        self.data = path.read_bytes()
        self.offset = 0

    def read_exact(self, size):
        end = self.offset + size
        if end > len(self.data):
            raise ValueError(
                f"{rel(self.path)} truncated at byte {self.offset}; "
                f"requested {size}, size is {len(self.data)}"
            )
        value = self.data[self.offset:end]
        self.offset = end
        return value

    def read_int32(self):
        return struct.unpack("<i", self.read_exact(4))[0]

    def read_float(self):
        return struct.unpack("<f", self.read_exact(4))[0]

    def read_byte(self):
        return self.read_exact(1)[0]

    def read_bool(self):
        return self.read_byte() != 0

    def read_7bit_int(self):
        result = 0
        shift = 0
        for _ in range(5):
            value = self.read_byte()
            result |= (value & 0x7F) << shift
            if value & 0x80 == 0:
                return result
            shift += 7
        raise ValueError(
            f"{rel(self.path)} has invalid .NET 7-bit integer at byte {self.offset}"
        )

    def read_string(self):
        size = self.read_7bit_int()
        return self.read_exact(size).decode("utf-8")

    def assert_eof(self):
        if self.offset != len(self.data):
            raise ValueError(
                f"{rel(self.path)} has {len(self.data) - self.offset} trailing bytes"
            )


def vector(x, y, z):
    return {
        "x": round(float(x), 6),
        "y": round(float(y), 6),
        "z": round(float(z), 6),
    }


def planar(x, y):
    return {
        "x": round(float(x), 6),
        "y": round(float(y), 6),
    }


def parse_map_brief():
    reader = DotNetBinaryReader(MAP_BRIEF_PATH)
    count = reader.read_int32()
    width = reader.read_int32()
    if count <= 0 or width <= 0:
        raise ValueError(f"Invalid map header count={count} width={width}")

    tiles = []
    for index in range(count):
        tile_x = index % width
        tile_y = index // width
        tile_id = reader.read_string()
        rotation_y = reader.read_float()
        enable_bridge = reader.read_bool()
        flow_river = reader.read_bool()
        show_province = reader.read_bool()
        province_count = reader.read_byte()
        if province_count == 1:
            province = reader.read_string()
            provinces = [province] * 4
        else:
            provinces = [reader.read_string() for _ in range(4)]
        tiles.append(
            {
                "index": index,
                "x": tile_x,
                "y": tile_y,
                "tile_id": tile_id,
                "rotation_y": round(rotation_y, 6),
                "enable_bridge": enable_bridge,
                "flow_river": flow_river,
                "show_province": show_province,
                "province_count": province_count,
                "province_ids": provinces,
                "unity_center": planar(
                    (tile_x + 0.5) * TILE_SIZE,
                    (tile_y + 0.5) * TILE_SIZE,
                ),
            }
        )

    reader.assert_eof()
    height = math.ceil(count / width)
    return {
        "count": count,
        "width": width,
        "height": height,
        "tiles": tiles,
        "parsed_bytes": reader.offset,
    }


def parse_tile_templates():
    reader = DotNetBinaryReader(TILE_DATA_PATH)
    template_count = reader.read_int32()
    templates = {}
    plane_total = 0
    object_total = 0

    for _ in range(template_count):
        tile_id = reader.read_string()
        plane_count = reader.read_int32()
        planes = []
        for plane_index in range(plane_count):
            prefab_id = reader.read_string()
            rotation_y = reader.read_float()
            scale = vector(
                reader.read_float(),
                reader.read_float(),
                reader.read_float(),
            )
            piece_width = TILE_SIZE / TILE_PIECES_PER_ROW
            local_x = (
                plane_index % TILE_PIECES_PER_ROW
            ) * piece_width + piece_width * 0.5
            local_z = (
                plane_index // TILE_PIECES_PER_ROW
            ) * piece_width + piece_width * 0.5
            planes.append(
                {
                    "index": plane_index,
                    "prefab_id": prefab_id,
                    "unity_local_position": vector(local_x, 0.0, local_z),
                    "rotation_y": round(rotation_y, 6),
                    "scale": scale,
                }
            )

        object_count = reader.read_int32()
        objects = []
        for object_index in range(object_count):
            prefab_id = reader.read_string()
            stored_position = vector(
                reader.read_float(),
                reader.read_float(),
                reader.read_float(),
            )
            rotation_y = reader.read_float()
            scale = vector(
                reader.read_float(),
                reader.read_float(),
                reader.read_float(),
            )
            river_flow_direction = reader.read_byte() != 0
            objects.append(
                {
                    "index": object_index,
                    "prefab_id": prefab_id,
                    "category": "adorning" if "Grove" in prefab_id else "detail",
                    "stored_position": stored_position,
                    "unity_local_position": vector(
                        stored_position["x"] + TILE_SIZE * 0.5,
                        stored_position["y"],
                        stored_position["z"] + TILE_SIZE * 0.5,
                    ),
                    "rotation_y": round(rotation_y, 6),
                    "scale": scale,
                    "river_flow_direction": river_flow_direction,
                }
            )

        if tile_id in templates:
            raise ValueError(f"Duplicate tile template id: {tile_id}")
        templates[tile_id] = {
            "tile_id": tile_id,
            "plane_count": plane_count,
            "object_count": object_count,
            "planes": planes,
            "objects": objects,
        }
        plane_total += plane_count
        object_total += object_count

    reader.assert_eof()
    return {
        "count": template_count,
        "plane_total": plane_total,
        "object_total": object_total,
        "templates": templates,
        "parsed_bytes": reader.offset,
    }


def build_guid_index():
    index = {}
    for meta_path in UNITY_ASSETS_ROOT.rglob("*.meta"):
        try:
            text = meta_path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        match = re.search(r"^guid:\s*([0-9a-fA-F]+)\s*$", text, re.M)
        if not match:
            continue
        asset_path = meta_path.with_suffix("")
        if asset_path.exists():
            index[match.group(1).lower()] = rel(asset_path)
    return index


def build_prefab_stem_index():
    index = defaultdict(list)
    land_root = UNITY_ASSETS_ROOT / "BundleAssets" / "land"
    for prefab_path in land_root.rglob("*.prefab"):
        index[prefab_path.stem.lower()].append(prefab_path)
    return index


def build_address_index():
    text = MAP_TILE_GROUP_PATH.read_text(encoding="utf-8", errors="ignore")
    index = {}
    for guid, address in re.findall(
        r"^\s*-\s+m_GUID:\s*([0-9a-fA-F]+)\s*$"
        r"\s*^\s*m_Address:\s*(.*?)\s*$",
        text,
        re.M,
    ):
        address = address.strip()
        if address in index:
            raise ValueError(f"Duplicate MapTile Addressables address: {address}")
        index[address] = guid.lower()
    if not index:
        raise ValueError(f"No Addressables entries parsed from {rel(MAP_TILE_GROUP_PATH)}")
    return index


def prefab_sort_key(path):
    normalized = rel(path)
    return (
        "_lod" in normalized.lower(),
        "/Tile/" not in normalized,
        "/AutoMake/" not in normalized,
        len(normalized),
        normalized,
    )


def resolved_prefab_record(
    prefab_id,
    prefab_index,
    guid_index,
    address_index,
):
    candidates = sorted(
        prefab_index.get(prefab_id.lower(), []),
        key=prefab_sort_key,
    )
    address_guid = address_index.get(prefab_id, "")
    address_path = guid_index.get(address_guid, "")
    primary = PROJECT_ROOT / address_path if address_path else None
    dependencies = []
    if primary and primary.exists():
        text = primary.read_text(encoding="utf-8", errors="ignore")
        seen = set()
        for guid in re.findall(r"guid:\s*([0-9a-fA-F]+)", text):
            guid = guid.lower()
            path = guid_index.get(guid, "")
            if not path or (guid, path) in seen:
                continue
            seen.add((guid, path))
            dependencies.append({"guid": guid, "path": path})
        dependencies.sort(key=lambda item: item["path"])

    return {
        "prefab_id": prefab_id,
        "address_guid": address_guid,
        "resolution_method": "addressable_guid" if address_path else "",
        "source_prefab_path": rel(primary) if primary and primary.exists() else "",
        "candidate_paths": [rel(path) for path in candidates],
        "resolved_dependencies": dependencies,
    }


def rotate_tile_local(local_x, local_z, rotation_y):
    center = TILE_SIZE * 0.5
    radians = math.radians(rotation_y)
    x = local_x - center
    z = local_z - center
    rotated_x = x * math.cos(radians) + z * math.sin(radians)
    rotated_z = -x * math.sin(radians) + z * math.cos(radians)
    return rotated_x + center, rotated_z + center


def unity_to_ue(unity_x, unity_y, unity_z):
    return vector(
        -unity_x * UE_UNITS_PER_UNITY_UNIT,
        unity_z * UE_UNITS_PER_UNITY_UNIT,
        unity_y * UE_UNITS_PER_UNITY_UNIT,
    )


def expand_record(tile, source, kind, focus_unity):
    local = source["unity_local_position"]
    local_x, local_z = rotate_tile_local(
        local["x"],
        local["z"],
        tile["rotation_y"],
    )
    world_x = tile["x"] * TILE_SIZE + local_x
    world_y = local["y"]
    world_z = tile["y"] * TILE_SIZE + local_z
    relative_x = world_x - focus_unity["x"]
    relative_z = world_z - focus_unity["y"]
    source_rotation = source["rotation_y"] + tile["rotation_y"]
    position_only = (
        kind != "plane"
        and source.get("category") == "adorning"
    )
    runtime_rotation = 0.0 if position_only else source_rotation
    runtime_scale = (
        vector(1.0, 1.0, 1.0)
        if position_only
        else source["scale"]
    )
    record = {
        "kind": kind,
        "template_index": source["index"],
        "prefab_id": source["prefab_id"],
        "unity_local_position": source["unity_local_position"],
        "unity_world_position": vector(world_x, world_y, world_z),
        "unity_source_rotation_y": round(source_rotation, 6),
        "unity_source_scale": source["scale"],
        "unity_runtime_transform_policy": (
            "position_only"
            if position_only
            else "position_rotation_scale"
        ),
        "unity_runtime_rotation_y": round(runtime_rotation, 6),
        "unity_runtime_scale": runtime_scale,
        "ue_world_position": unity_to_ue(world_x, world_y, world_z),
        "ue_relative_to_focus": unity_to_ue(relative_x, world_y, relative_z),
        "ue_yaw": round(runtime_rotation, 6),
        "ue_actor_scale_for_centimeter_asset": vector(
            -runtime_scale["x"] * UE_UNITS_PER_UNITY_UNIT / 100.0,
            runtime_scale["z"] * UE_UNITS_PER_UNITY_UNIT / 100.0,
            runtime_scale["y"] * UE_UNITS_PER_UNITY_UNIT / 100.0,
        ),
    }
    if kind != "plane":
        record["category"] = source["category"]
        record["river_flow_direction"] = not (
            tile["flow_river"] ^ source["river_flow_direction"]
        )
    return record


def build_prototype_window(
    map_data,
    template_data,
    prefab_index,
    guid_index,
    address_index,
):
    focus_unity = planar(
        FOCUS_DISPLAY_X * DISPLAY_TO_UNITY_SCALE,
        FOCUS_DISPLAY_Y * DISPLAY_TO_UNITY_SCALE,
    )
    focus_tile_x = int(math.floor(focus_unity["x"] / TILE_SIZE))
    focus_tile_y = int(math.floor(focus_unity["y"] / TILE_SIZE))
    min_x = max(0, focus_tile_x - PROTOTYPE_RADIUS_TILES)
    max_x = min(map_data["width"] - 1, focus_tile_x + PROTOTYPE_RADIUS_TILES)
    min_y = max(0, focus_tile_y - PROTOTYPE_RADIUS_TILES)
    max_y = min(map_data["height"] - 1, focus_tile_y + PROTOTYPE_RADIUS_TILES)

    selected_tiles = []
    selected_prefab_ids = set()
    for tile in map_data["tiles"]:
        if not (min_x <= tile["x"] <= max_x and min_y <= tile["y"] <= max_y):
            continue
        template = template_data["templates"].get(tile["tile_id"])
        if template is None:
            raise ValueError(
                f"Map tile {tile['x']},{tile['y']} references missing "
                f"template {tile['tile_id']}"
            )
        instances = []
        for plane in template["planes"]:
            instances.append(expand_record(tile, plane, "plane", focus_unity))
            selected_prefab_ids.add(plane["prefab_id"])
        for obj in template["objects"]:
            instances.append(expand_record(tile, obj, "object", focus_unity))
            selected_prefab_ids.add(obj["prefab_id"])

        tile_copy = dict(tile)
        tile_copy["ue_center_relative_to_focus"] = unity_to_ue(
            tile["unity_center"]["x"] - focus_unity["x"],
            0.0,
            tile["unity_center"]["y"] - focus_unity["y"],
        )
        tile_copy["plane_count"] = template["plane_count"]
        tile_copy["object_count"] = template["object_count"]
        tile_copy["instances"] = instances
        selected_tiles.append(tile_copy)

    source_prefabs = {
        prefab_id: resolved_prefab_record(
            prefab_id,
            prefab_index,
            guid_index,
            address_index,
        )
        for prefab_id in sorted(selected_prefab_ids)
    }
    return {
        "focus": {
            "display_coordinate": {
                "x": FOCUS_DISPLAY_X,
                "y": FOCUS_DISPLAY_Y,
            },
            "unity_coordinate": focus_unity,
            "focus_anchor_policy": "display_bucket_lower_bound",
            "unity_coordinate_bucket": {
                "min_inclusive": focus_unity,
                "max_exclusive": planar(
                    (FOCUS_DISPLAY_X + 1) * DISPLAY_TO_UNITY_SCALE,
                    (FOCUS_DISPLAY_Y + 1) * DISPLAY_TO_UNITY_SCALE,
                ),
            },
            "tile_coordinate": {"x": focus_tile_x, "y": focus_tile_y},
            "ue_world_position": unity_to_ue(
                focus_unity["x"],
                0.0,
                focus_unity["y"],
            ),
            "ue_prototype_local_origin": vector(0.0, 0.0, 0.0),
        },
        "tile_bounds": {
            "min_x": min_x,
            "max_x": max_x,
            "min_y": min_y,
            "max_y": max_y,
        },
        "tile_count": len(selected_tiles),
        "instance_count": sum(len(tile["instances"]) for tile in selected_tiles),
        "plane_instance_count": sum(
            tile["plane_count"] for tile in selected_tiles
        ),
        "object_instance_count": sum(
            tile["object_count"] for tile in selected_tiles
        ),
        "tiles": selected_tiles,
        "source_prefabs": source_prefabs,
        "unresolved_prefab_ids": [
            prefab_id
            for prefab_id, record in source_prefabs.items()
            if not record["source_prefab_path"]
        ],
    }


def build_full_scene_inventory(
    map_data,
    template_data,
    prefab_index,
    guid_index,
    address_index,
):
    ground_prefab_ids = set()
    object_prefab_ids = set()
    for template in template_data["templates"].values():
        ground_prefab_ids.update(
            record["prefab_id"]
            for record in template["planes"]
        )
        object_prefab_ids.update(
            record["prefab_id"]
            for record in template["objects"]
        )

    all_prefab_ids = ground_prefab_ids | object_prefab_ids
    source_prefabs = {
        prefab_id: resolved_prefab_record(
            prefab_id,
            prefab_index,
            guid_index,
            address_index,
        )
        for prefab_id in sorted(all_prefab_ids)
    }
    tile_usage = defaultdict(int)
    for tile in map_data["tiles"]:
        tile_usage[tile["tile_id"]] += 1
    expanded_plane_count = sum(
        tile_usage[tile_id] * template["plane_count"]
        for tile_id, template in template_data["templates"].items()
    )
    expanded_object_count = sum(
        tile_usage[tile_id] * template["object_count"]
        for tile_id, template in template_data["templates"].items()
    )
    unresolved_prefab_ids = [
        prefab_id
        for prefab_id, record in source_prefabs.items()
        if not record["source_prefab_path"]
    ]
    return {
        "tile_count": map_data["count"],
        "tile_type_count": template_data["count"],
        "expanded_plane_instance_count": expanded_plane_count,
        "expanded_object_instance_count": expanded_object_count,
        "expanded_instance_count": (
            expanded_plane_count + expanded_object_count
        ),
        "ground_prefab_ids": sorted(ground_prefab_ids),
        "object_prefab_ids": sorted(object_prefab_ids),
        "source_prefab_count": len(source_prefabs),
        "source_prefabs": source_prefabs,
        "unresolved_prefab_ids": unresolved_prefab_ids,
        "runtime_strategy": (
            "stream tiles around the camera from reusable source templates; "
            "do not flatten all instances into level actors"
        ),
    }


def verify_addressable_evidence():
    group_text = ADDRESSABLE_GROUP_PATH.read_text(
        encoding="utf-8",
        errors="ignore",
    )
    import_text = ADDRESSABLE_IMPORT_SETTINGS_PATH.read_text(
        encoding="utf-8",
        errors="ignore",
    )
    missing = []
    for guid in (MAP_BRIEF_GUID, TILE_DATA_GUID):
        if guid not in group_text:
            missing.append(f"{rel(ADDRESSABLE_GROUP_PATH)} missing {guid}")
    if "Assets/BundleAssets/land/MapData" not in import_text:
        missing.append(
            f"{rel(ADDRESSABLE_IMPORT_SETTINGS_PATH)} missing land/MapData rule"
        )
    if missing:
        raise ValueError("; ".join(missing))


def build_manifest():
    verify_addressable_evidence()
    map_data = parse_map_brief()
    environment = parse_world_environment()
    camera_keys = parse_camera_parameters()
    camera_by_id = {
        record["id"]: record
        for record in camera_keys
    }
    tactical_camera = dict(camera_by_id["map_tactical"])
    tactical_vertical_fov = tactical_camera["vertical_fov_degrees"]
    tactical_camera["horizontal_fov_degrees_at_16_10"] = round(
        math.degrees(
            2.0
            * math.atan(
                math.tan(math.radians(tactical_vertical_fov) * 0.5)
                * 1.6
            )
        ),
        6,
    )
    # CameraParamLod has no lod column. This is the phase-one runtime bucket,
    # not a value claimed to come from the CSV.
    tactical_camera["prototype_lod_bucket"] = 1
    tactical_camera["source_table"] = rel(CAMERA_PARAM_PATH)
    tactical_camera["source_world"] = rel(WORLD_SCENE_PATH)
    template_data = parse_tile_templates()
    guid_index = build_guid_index()
    prefab_index = build_prefab_stem_index()
    address_index = build_address_index()
    prototype = build_prototype_window(
        map_data,
        template_data,
        prefab_index,
        guid_index,
        address_index,
    )

    tile_usage = defaultdict(int)
    for tile in map_data["tiles"]:
        tile_usage[tile["tile_id"]] += 1

    templates = []
    for tile_id in sorted(template_data["templates"]):
        template = dict(template_data["templates"][tile_id])
        template["map_usage_count"] = tile_usage[tile_id]
        templates.append(template)
    full_scene = build_full_scene_inventory(
        map_data,
        template_data,
        prefab_index,
        guid_index,
        address_index,
    )

    return {
        "schema": "rok.unity_world_map.v1",
        "sources": {
            "map_brief": source_record(MAP_BRIEF_PATH),
            "tile_templates": source_record(TILE_DATA_PATH),
            "addressable_group": source_record(ADDRESSABLE_GROUP_PATH),
            "addressable_import_settings": source_record(
                ADDRESSABLE_IMPORT_SETTINGS_PATH
            ),
            "map_tile_group": source_record(MAP_TILE_GROUP_PATH),
            "camera_parameters": source_record(CAMERA_PARAM_PATH),
            "world_scene": source_record(WORLD_SCENE_PATH),
            "light_manager": source_record(LIGHT_MANAGER_PATH),
            "day_night_logic": source_record(
                GAME_EVENT_GLOBAL_MEDIATOR_PATH
            ),
        },
        "addressable_resolution": {
            "map_brief_guid": MAP_BRIEF_GUID,
            "map_brief_path": rel(MAP_BRIEF_PATH),
            "tile_data_guid": TILE_DATA_GUID,
            "tile_data_path": rel(TILE_DATA_PATH),
            "rule": (
                "Assets/BundleAssets/land/MapData/(?<asset>.*)\\.bytes"
            ),
            "rejected_legacy_root": (
                "ref/client-unity/Assets/BundleAssets/Map/Data"
            ),
            "prefab_resolution": (
                "MapTile address -> GUID -> .meta asset path"
            ),
        },
        "coordinate_contract": {
            "tile_size_unity_units": TILE_SIZE,
            "display_to_unity_scale": DISPLAY_TO_UNITY_SCALE,
            "ue_units_per_unity_unit": UE_UNITS_PER_UNITY_UNIT,
            "display_to_unity": (
                "unity_x = display_x * 6; unity_z = display_y * 6"
            ),
            "unity_to_ue": (
                "ue_x = -unity_x * 500; ue_y = unity_z * 500; "
                "ue_z = unity_y * 500"
            ),
            "prototype_origin": (
                "UE prototype actors are rebased relative to focus Unity "
                "coordinate to keep phase-1 transforms numerically stable."
            ),
        },
        "environment": environment,
        "camera_keys": camera_keys,
        "map_tactical_camera": tactical_camera,
        "map": {
            "tile_count": map_data["count"],
            "width_tiles": map_data["width"],
            "height_tiles": map_data["height"],
            "world_width_unity_units": map_data["width"] * TILE_SIZE,
            "world_height_unity_units": map_data["height"] * TILE_SIZE,
            "parsed_bytes": map_data["parsed_bytes"],
            "tile_type_usage": dict(sorted(tile_usage.items())),
            "tiles": map_data["tiles"],
        },
        "tile_template_summary": {
            "tile_type_count": template_data["count"],
            "plane_record_count": template_data["plane_total"],
            "object_record_count": template_data["object_total"],
            "parsed_bytes": template_data["parsed_bytes"],
        },
        "tile_templates": templates,
        "full_scene": full_scene,
        "prototype_window": prototype,
        "ue_target_level": "/Game/Maps/L_RokUnityWorldMapPrototype",
        "acceptance_notes": [
            (
                "The full_scene inventory resolves every source prefab used "
                "by the authoritative 40x40 map and records the expanded "
                "instance counts required by the streaming runtime."
            ),
            (
                "The phase-1 UE map must be generated from prototype_window; "
                "editor placement is not source of truth."
            ),
            (
                "The full 40x40 map remains manifest data. Only the bounded "
                "3x3 focus window is promoted in phase 1."
            ),
            (
                "Ground, mountain, tree and transition visuals must resolve "
                "to Unity source assets; generic BasicShapes are not accepted."
            ),
        ],
    }


def write_markdown(manifest):
    map_record = manifest["map"]
    template_record = manifest["tile_template_summary"]
    full_scene = manifest["full_scene"]
    prototype = manifest["prototype_window"]
    focus = prototype["focus"]
    environment = manifest["environment"]
    directional_light = environment["directional_light"]
    lines = [
        "# Rok Unity World Map",
        "",
        f"- Schema: `{manifest['schema']}`",
        (
            f"- Authoritative map: `{map_record['width_tiles']} × "
            f"{map_record['height_tiles']}` tiles, "
            f"`{map_record['world_width_unity_units']:.0f} × "
            f"{map_record['world_height_unity_units']:.0f}` Unity units"
        ),
        (
            f"- Tile templates: `{template_record['tile_type_count']}`, "
            f"planes `{template_record['plane_record_count']}`, "
            f"objects `{template_record['object_record_count']}`"
        ),
        (
            f"- Focus display coordinate: "
            f"`X:{focus['display_coordinate']['x']} "
            f"Y:{focus['display_coordinate']['y']}`"
        ),
        (
            f"- Focus Unity coordinate: "
            f"`{focus['unity_coordinate']['x']:.0f}, "
            f"{focus['unity_coordinate']['y']:.0f}`"
        ),
        (
            f"- Focus tile: "
            f"`{focus['tile_coordinate']['x']},"
            f"{focus['tile_coordinate']['y']}`"
        ),
        (
            f"- Prototype window: `{prototype['tile_count']}` tiles, "
            f"`{prototype['plane_instance_count']}` plane instances, "
            f"`{prototype['object_instance_count']}` object instances"
        ),
        (
            f"- Full scene expansion: `{full_scene['tile_count']}` tiles, "
            f"`{full_scene['expanded_plane_instance_count']}` plane "
            f"instances, `{full_scene['expanded_object_instance_count']}` "
            "object instances"
        ),
        (
            "- Unity world environment: fog "
            f"`{environment['render_settings']['fog_enabled']}`, "
            "directional light intensity "
            f"`{directional_light['intensity']:.3f}`"
        ),
        "",
        "## Authoritative Sources",
        "",
        "| Role | Path | SHA-256 |",
        "| --- | --- | --- |",
    ]
    for role, source in manifest["sources"].items():
        lines.append(
            f"| {role} | `{source['path']}` | `{source['sha256']}` |"
        )
    lines.extend(
        [
            "",
            "## 3×3 Prototype Tiles",
            "",
            "| Tile | Template | Rotation | Province | Planes | Objects |",
            "| --- | --- | ---: | --- | ---: | ---: |",
        ]
    )
    for tile in prototype["tiles"]:
        lines.append(
            f"| `{tile['x']},{tile['y']}` | `{tile['tile_id']}` | "
            f"{tile['rotation_y']:.1f} | "
            f"`{'/'.join(tile['province_ids'])}` | "
            f"{tile['plane_count']} | {tile['object_count']} |"
        )
    lines.extend(
        [
            "",
            "## Source Resolution",
            "",
            (
                f"- Unique selected prefab ids: "
                f"`{len(prototype['source_prefabs'])}`"
            ),
            (
                f"- Unresolved prefab ids: "
                f"`{len(prototype['unresolved_prefab_ids'])}`"
            ),
            (
                f"- Full-scene prefab ids: "
                f"`{full_scene['source_prefab_count']}`"
            ),
            (
                f"- Full-scene unresolved prefab ids: "
                f"`{len(full_scene['unresolved_prefab_ids'])}`"
            ),
        ]
    )
    for prefab_id in prototype["unresolved_prefab_ids"]:
        lines.append(f"  - `{prefab_id}`")
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    manifest = build_manifest()
    OUTPUT_JSON.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    write_markdown(manifest)
    prototype = manifest["prototype_window"]
    full_scene = manifest["full_scene"]
    print(
        "Rok Unity world-map manifest generated: "
        f"map={manifest['map']['width_tiles']}x"
        f"{manifest['map']['height_tiles']} "
        f"templates={manifest['tile_template_summary']['tile_type_count']} "
        f"prototype_tiles={prototype['tile_count']} "
        f"prototype_instances={prototype['instance_count']} "
        f"full_instances={full_scene['expanded_instance_count']} "
        f"full_prefabs={full_scene['source_prefab_count']} "
        f"unresolved_prefabs={len(full_scene['unresolved_prefab_ids'])}"
    )


if __name__ == "__main__":
    main()
