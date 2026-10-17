import hashlib
import json
import math
import os
import re
import sys
import tempfile
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS_DIR = PROJECT_ROOT / "Scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

import BuildRokUnityRuntimeCitySupportLayers as source
from rok_data_paths import DATA_ROOT


UNITY_ROOT = source.UNITY_ROOT
UNITY_ASSETS = source.UNITY_ASSETS
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
OUTPUT_ROOT = MIGRATION_ROOT / "UnityRuntimeCityGateFlags"
MANIFEST_PATH = MIGRATION_ROOT / "unity_runtime_city_gate_flags.json"
VERIFY_PATH = MIGRATION_ROOT / "unity_runtime_city_gate_flags_verify.json"
INITIAL_CITY_STATE_PATH = DATA_ROOT / "unity_initial_city_state.json"

PREFAB_PATH = (
    UNITY_ASSETS
    / "BundleAssets"
    / "Scene"
    / "City_building"
    / "city_building_1"
    / "build_8_101_1.prefab"
)
COLOR_TEXTURE_PATH = (
    UNITY_ASSETS
    / "BundleAssets"
    / "Map"
    / "Materials"
    / "shared_texture"
    / "city_flag.png"
)
MASK_TEXTURE_PATH = COLOR_TEXTURE_PATH.with_name("city_flag_mask.png")
MATERIAL_PATH = (
    UNITY_ASSETS
    / "BundleAssets"
    / "Scene"
    / "City"
    / "city_function"
    / "map_building.mat"
)
SHADER_PATH = (
    UNITY_ASSETS
    / "BundleAssets"
    / "Map"
    / "Building"
    / "map_building_with_mask-5.shader"
)
MASK_SPRITE_SOURCE_PATH = UNITY_ASSETS / "Scripts" / "Client" / "Compoment" / "MaskSprite.cs"
TOWN_BUILDING_SOURCE_PATH = UNITY_ASSETS / "Scripts" / "Client" / "CityManager" / "TownBuilding.cs"
RS_SOURCE_PATH = UNITY_ASSETS / "Scripts" / "Hotfix" / "MVC" / "RS.cs"

PREFAB_GUID = "ac3cef7f718cb1041adcbfbc00b13fec"
COLOR_SPRITE_GUID = "77a823d32eb6e484aa3f0628df783914"
MASK_SPRITE_GUID = "3dbfeea42b2216b4c90e691050be89de"
MATERIAL_GUID = "8c1054cc602b94049a169c377d3fd3bd"
SHADER_GUID = "45d662578936c924fbe7a09119b72a9c"
MASK_SPRITE_SCRIPT_GUID = "f43e688d317b80c4d9560e9cb03707e9"
SPRITE_LOCAL_ID = 21300000
EXPECTED_RENDER_QUEUE = 2995
EXPECTED_VERTEX_COUNT = 13
EXPECTED_TRIANGLE_COUNT = 11
RUNTIME_BUILD_GREEN = [62.0 / 255.0, 125.0 / 255.0, 195.0 / 255.0, 1.0]
MATERIAL_COLOR_2 = [0.772549, 0.3254902, 0.2392157, 1.0]
MAIN_CITY_SPRITE_COLOR = [0.9945588, 0.8445588, 0.7370588, 1.0]

TARGETS = (
    {
        "flag_id": "city_gate_flag_sprite1",
        "object_name": "sprite1",
        "object_path": "build_8_101_1/CityGate_1/sprite1",
        "game_object_file_id": 3312056129432016268,
        "transform_file_id": 1386033310051469381,
        "sprite_renderer_file_id": 168354332547305056,
        "mask_sprite_component_file_id": 4957842544639472310,
        "expected_ue_transform_origin": [-839.182252, -1308.241482, 201.235440],
    },
    {
        "flag_id": "city_gate_flag_sprite2",
        "object_name": "sprite2",
        "object_path": "build_8_101_1/CityGate_1/sprite2",
        "game_object_file_id": 9090295385687153316,
        "transform_file_id": 3872668542940474710,
        "sprite_renderer_file_id": 5012716039405394113,
        "mask_sprite_component_file_id": 3318436056464985544,
        "expected_ue_transform_origin": [-1152.627499, -994.082778, 201.235440],
    },
)


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def json_sha256(value):
    payload = json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def project_relative(path):
    return source.project_relative(path)


def unity_relative(path):
    return source.unity_relative(path)


def read_source_text(path):
    data = Path(path).read_bytes()
    for encoding in ("utf-8-sig", "gb18030", "utf-16"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    raise UnicodeDecodeError("source", data, 0, len(data), "unable to detect source text encoding")


def guid_from_meta(asset_path):
    meta_path = Path(str(asset_path) + ".meta")
    match = re.search(r"^guid:\s*([0-9a-fA-F]+)\s*$", read_source_text(meta_path), re.MULTILINE)
    return match.group(1).lower() if match else ""


def nearly_equal_sequence(actual, expected, tolerance=1.0e-5):
    return len(actual) == len(expected) and all(abs(float(a) - float(b)) <= tolerance for a, b in zip(actual, expected))


def resolve_python_dependencies():
    candidates = [
        os.environ.get("ROK_UNITYPY_PATH", ""),
        str(Path(tempfile.gettempdir()) / "rok_unitypy"),
    ]
    for candidate in candidates:
        if candidate and Path(candidate).is_dir() and candidate not in sys.path:
            sys.path.insert(0, candidate)
    try:
        import UnityPy  # noqa: F401
        from PIL import Image  # noqa: F401
    except ImportError as exc:
        raise RuntimeError(
            "UnityPy and Pillow are required to read Unity's imported tight SpriteMesh and bake the Gamma-space "
            "flag shader. Install them into a Python environment, or set ROK_UNITYPY_PATH to a directory "
            "containing UnityPy and PIL."
        ) from exc


def unity_cache_path(guid):
    return UNITY_ROOT / "Library" / "metadata" / guid[:2] / guid


def load_imported_sprite(guid, local_id):
    resolve_python_dependencies()
    import UnityPy
    from UnityPy.helpers.MeshHelper import MeshHandler

    cache_path = unity_cache_path(guid)
    if not cache_path.is_file():
        raise FileNotFoundError(
            "Missing Unity imported Sprite cache for {0}; open the live Unity project once so Library metadata is generated: {1}".format(
                guid,
                cache_path,
            )
        )
    environment = UnityPy.load(str(cache_path))
    candidates = [item for item in environment.objects if int(item.path_id) == int(local_id)]
    if len(candidates) != 1 or candidates[0].type.name != "Sprite":
        raise RuntimeError("Expected one Unity Sprite object {0} in {1}".format(local_id, cache_path))
    object_reader = candidates[0]
    sprite = object_reader.read()
    render_data = sprite.m_RD
    mesh = MeshHandler(render_data, object_reader.version, object_reader.endian)
    mesh.process()
    vertices = [[float(value) for value in vertex[:3]] for vertex in (mesh.m_Vertices or [])]
    indices = [int(value) for value in (mesh.m_IndexBuffer or [])]
    if len(indices) % 3:
        raise RuntimeError("Unity Sprite tight mesh has a non-triangular index count: {0}".format(len(indices)))

    texture = render_data.texture.read()
    texture_size = [int(texture.m_Width), int(texture.m_Height)]
    rect = render_data.textureRect
    texture_rect = [float(rect.x), float(rect.y), float(rect.width), float(rect.height)]
    uv_transform = [
        float(render_data.uvTransform.x),
        float(render_data.uvTransform.y),
        float(render_data.uvTransform.z),
        float(render_data.uvTransform.w),
    ]
    raw_uvs = list(mesh.m_UV0 or [])
    if raw_uvs and any(abs(float(u)) > 1.0e-8 or abs(float(v)) > 1.0e-8 for u, v in raw_uvs):
        texture_uvs = [[float(u), float(v)] for u, v in raw_uvs]
        uv_source = "Unity Sprite m_VertexData UV0"
    else:
        # Unity 2019 single sprites can serialize a zero-filled UV stream and
        # reconstruct Sprite.uv from uvTransform. This matches Sprite.uv:
        # pixel=(vertex.xy*scale + offset), then normalized by texture size.
        texture_uvs = [
            [
                (vertex[0] * uv_transform[0] + uv_transform[1]) / float(texture_size[0]),
                (vertex[1] * uv_transform[2] + uv_transform[3]) / float(texture_size[1]),
            ]
            for vertex in vertices
        ]
        uv_source = "Unity 2019 SpriteRenderData uvTransform reconstructed from imported cache"
    local_uvs = [
        [
            (uv[0] * texture_size[0] - texture_rect[0]) / max(texture_rect[2], 1.0),
            (uv[1] * texture_size[1] - texture_rect[1]) / max(texture_rect[3], 1.0),
        ]
        for uv in texture_uvs
    ]
    settings_raw = int(render_data.settingsRaw)
    return {
        "cache_path": str(cache_path.resolve()),
        "cache_sha256": sha256(cache_path),
        "unity_version": str(object_reader.version),
        "name": str(sprite.m_Name),
        "local_id": int(local_id),
        "rect": [float(sprite.m_Rect.x), float(sprite.m_Rect.y), float(sprite.m_Rect.width), float(sprite.m_Rect.height)],
        "texture_rect": texture_rect,
        "texture_size": texture_size,
        "pivot": [float(sprite.m_Pivot.x), float(sprite.m_Pivot.y)],
        "pixels_per_unit": float(sprite.m_PixelsToUnits),
        "settings_raw": settings_raw,
        "mesh_type": "Tight" if ((settings_raw >> 6) & 1) == 1 else "FullRect",
        "vertices": vertices,
        "indices": indices,
        "local_uvs": local_uvs,
        "uv_transform": uv_transform,
        "uv_source": uv_source,
        "vertex_count": len(vertices),
        "triangle_count": len(indices) // 3,
        "vertices_sha256": json_sha256(vertices),
        "indices_sha256": json_sha256(indices),
        "uv_sha256": json_sha256(local_uvs),
    }


def unity_round_to_int(value):
    return int(math.floor(float(value) + 0.5))


def crop_unity_texture_rect(image, rect):
    x = unity_round_to_int(rect[0])
    y = unity_round_to_int(rect[1])
    width = unity_round_to_int(rect[2])
    height = unity_round_to_int(rect[3])
    # Unity texture coordinates start at bottom-left; Pillow starts at top-left.
    top = image.height - (y + height)
    bottom = image.height - y
    return image.crop((x, top, x + width, bottom)).convert("RGBA")


def bake_initial_city_flag(color_sprite, mask_sprite, output_path):
    resolve_python_dependencies()
    from PIL import Image

    color_image = crop_unity_texture_rect(Image.open(COLOR_TEXTURE_PATH), color_sprite["texture_rect"])
    mask_image = crop_unity_texture_rect(Image.open(MASK_TEXTURE_PATH), mask_sprite["texture_rect"])
    if mask_image.size != color_image.size:
        mask_image = mask_image.resize(color_image.size, Image.Resampling.BILINEAR)

    output_pixels = []
    for color_byte, mask_byte in zip(color_image.getdata(), mask_image.getdata()):
        color = [float(channel) / 255.0 for channel in color_byte]
        mask = [float(channel) / 255.0 for channel in mask_byte]
        mask_r2 = mask[0] * 2.0
        owned_color = max(0.0, min(1.0, mask_r2 - 1.0))
        base_weight = (1.0 - owned_color) - mask[1]
        rgb = []
        for channel in range(3):
            value = (
                color[channel] * base_weight
                + (2.0 * color[channel]) * (RUNTIME_BUILD_GREEN[channel] * owned_color)
                + (2.0 * color[channel]) * (MATERIAL_COLOR_2[channel] * mask[1])
            )
            value = max(0.0, min(1.0, value)) * MAIN_CITY_SPRITE_COLOR[channel]
            rgb.append(max(0.0, min(1.0, value)))
        alpha = color[3] * max(0.0, min(1.0, mask_r2))
        output_pixels.append(tuple(int(max(0, min(255, math.floor(value * 255.0 + 0.5)))) for value in (*rgb, alpha)))

    output = Image.new("RGBA", color_image.size)
    output.putdata(output_pixels)
    output.save(output_path, format="PNG", optimize=False)
    alpha = output.getchannel("A")
    return {
        "baked_texture_path": str(Path(output_path).resolve()),
        "baked_texture_project_path": project_relative(output_path),
        "baked_texture_sha256": sha256(output_path),
        "baked_texture_size": [int(output.width), int(output.height)],
        "baked_alpha_bbox": list(alpha.getbbox() or (0, 0, 0, 0)),
        "baked_mode": "RGBA8",
        "bake_color_space": "Unity project Gamma (m_ActiveColorSpace=0), quantized to RGBA32",
        "shader_rgb_formula": "main.rgb*((1-owned)-mask.g) + 2*main.rgb*(buildGreen*owned) + 2*main.rgb*(Color2*mask.g), then daytime SpriteColor",
        "shader_alpha_formula": "main.a * saturate(mask.r * 2) * CityTransparency(1)",
    }


def parse_material_and_shader(failures):
    material_body = read_source_text(MATERIAL_PATH)
    shader_body = read_source_text(SHADER_PATH)
    shader_reference = source.file_reference(material_body, "m_Shader")

    def material_color(name):
        match = re.search(r"- " + re.escape(name) + r":\s*(\{[^\r\n]+\})", material_body)
        return source.parse_float_map(match.group(1), ("r", "g", "b", "a")) if match else None

    def material_float(name):
        match = re.search(r"- " + re.escape(name) + r":\s*([-+0-9.eE]+)", material_body)
        return float(match.group(1)) if match else None

    serialized_color_1 = material_color("_Color_1")
    serialized_color_2 = material_color("_Color_2")
    city_transparency = material_float("_CityTransparency")
    if shader_reference["guid"] != SHADER_GUID:
        failures.append("map_building.mat shader GUID drifted: {0}".format(shader_reference["guid"]))
    if not nearly_equal_sequence(serialized_color_2 or [], MATERIAL_COLOR_2, 1.0e-6):
        failures.append("map_building.mat _Color_2 drifted: {0}".format(serialized_color_2))
    if city_transparency is None or abs(city_transparency - 1.0) > 1.0e-6:
        failures.append("map_building.mat _CityTransparency must remain 1")
    if not re.search(r'"Queue"\s*=\s*"Transparent-5"', shader_body):
        failures.append("map_building_with_mask-5.shader queue tag drifted")
    for token in ("Cull Off", "ZWrite Off", "Blend SrcAlpha OneMinusSrcAlpha"):
        if token not in shader_body:
            failures.append("map_building_with_mask-5.shader missing source render state: {0}".format(token))
    return {
        "material_path": str(MATERIAL_PATH.resolve()),
        "material_unity_path": unity_relative(MATERIAL_PATH),
        "material_guid": MATERIAL_GUID,
        "material_sha256": sha256(MATERIAL_PATH),
        "shader_path": str(SHADER_PATH.resolve()),
        "shader_unity_path": unity_relative(SHADER_PATH),
        "shader_guid": SHADER_GUID,
        "shader_sha256": sha256(SHADER_PATH),
        "shader_name": "custom/map/map_building_with_mask-5",
        "material_custom_render_queue": int(source.scalar(material_body, "m_CustomRenderQueue", "-1")),
        "shader_queue_tag": "Transparent-5",
        "material_render_queue": EXPECTED_RENDER_QUEUE,
        "shader_cull": "Off",
        "shader_zwrite": "Off",
        "shader_blend": "SrcAlpha OneMinusSrcAlpha",
        "serialized_material_color_1": serialized_color_1,
        "runtime_material_color_1": RUNTIME_BUILD_GREEN,
        "material_color_2": serialized_color_2,
        "city_transparency": city_transparency,
        "main_city_sprite_color": MAIN_CITY_SPRITE_COLOR,
    }


def parse_sprite_renderer(prefab, target, failures):
    documents = prefab["documents"]
    renderer_document = documents.get(target["sprite_renderer_file_id"])
    if not renderer_document or renderer_document["class_id"] != 212:
        failures.append("missing expected SpriteRenderer fileID {0}".format(target["sprite_renderer_file_id"]))
        return None
    body = renderer_document["body"]
    game_object_id = source.file_reference(body, "m_GameObject")["file_id"]
    transform = prefab["transforms"].get(target["transform_file_id"])
    game_object = prefab["game_objects"].get(target["game_object_file_id"])
    if game_object_id != target["game_object_file_id"] or not transform or not game_object:
        failures.append("flag {0} GameObject/Transform source IDs drifted".format(target["flag_id"]))
        return None
    if transform["game_object_id"] != game_object_id:
        failures.append("flag {0} Transform no longer belongs to the SpriteRenderer GameObject".format(target["flag_id"]))

    sprite_reference = source.file_reference(body, "m_Sprite")
    materials_section = body.split("m_Materials:", 1)[1].split("m_StaticBatchInfo:", 1)[0]
    material_references = re.findall(r"-\s*\{fileID:\s*(-?\d+),\s*guid:\s*([0-9a-fA-F]+)", materials_section)
    if len(material_references) != 1:
        failures.append("flag {0} must have exactly one SpriteRenderer material".format(target["flag_id"]))
        material_guid = ""
    else:
        material_guid = material_references[0][1].lower()

    mask_component = documents.get(target["mask_sprite_component_file_id"])
    mask_reference = {"file_id": 0, "guid": ""}
    mask_script_guid = ""
    if not mask_component or mask_component["class_id"] != 114:
        failures.append("flag {0} is missing its MaskSprite MonoBehaviour".format(target["flag_id"]))
    else:
        mask_script_guid = source.file_reference(mask_component["body"], "m_Script")["guid"]
        mask_reference = source.file_reference(mask_component["body"], "m_mask_sprite")

    object_path = source.object_path(prefab, transform["file_id"])
    enabled = source.scalar(body, "m_Enabled", "0") == "1"
    active = source.active_in_hierarchy(prefab, transform["file_id"])
    renderer_color = source.parse_float_map(source.scalar(body, "m_Color"), ("r", "g", "b", "a"))
    checks = (
        (game_object["name"] == target["object_name"], "object name"),
        (object_path == target["object_path"], "object hierarchy path"),
        (active, "activeInHierarchy"),
        (enabled, "SpriteRenderer enabled"),
        (sprite_reference["file_id"] == SPRITE_LOCAL_ID and sprite_reference["guid"] == COLOR_SPRITE_GUID, "Sprite reference"),
        (material_guid == MATERIAL_GUID, "material GUID"),
        (mask_script_guid == MASK_SPRITE_SCRIPT_GUID, "MaskSprite script GUID"),
        (mask_reference["file_id"] == SPRITE_LOCAL_ID and mask_reference["guid"] == MASK_SPRITE_GUID, "runtime mask Sprite reference"),
        (nearly_equal_sequence(renderer_color, [1.0, 1.0, 1.0, 1.0]), "renderer color"),
        (source.scalar(body, "m_FlipX", "0") == "0" and source.scalar(body, "m_FlipY", "0") == "0", "flip state"),
        (int(source.scalar(body, "m_SortingLayerID", "0")) == 0 and int(source.scalar(body, "m_SortingOrder", "0")) == 0, "sorting layer/order"),
    )
    for passed, label in checks:
        if not passed:
            failures.append("flag {0} source {1} drifted".format(target["flag_id"], label))

    return {
        "flag_id": target["flag_id"],
        "source_key": "{0}:SR:{1}".format(PREFAB_GUID, target["sprite_renderer_file_id"]),
        "object_name": game_object["name"],
        "object_path": object_path,
        "game_object_file_id": game_object_id,
        "transform_file_id": transform["file_id"],
        "sprite_renderer_file_id": target["sprite_renderer_file_id"],
        "mask_sprite_component_file_id": target["mask_sprite_component_file_id"],
        "active_self": game_object["active_self"],
        "active_in_hierarchy": active,
        "renderer_enabled": enabled,
        "renderer_type": "SpriteRenderer",
        "renderer_color": renderer_color,
        "renderer_flip_x": source.scalar(body, "m_FlipX", "0") == "1",
        "renderer_flip_y": source.scalar(body, "m_FlipY", "0") == "1",
        "renderer_draw_mode": int(source.scalar(body, "m_DrawMode", "0")),
        "renderer_sorting_layer_id": int(source.scalar(body, "m_SortingLayerID", "0")),
        "renderer_sorting_order": int(source.scalar(body, "m_SortingOrder", "0")),
        "renderer_priority": int(source.scalar(body, "m_RendererPriority", "0")),
        "sprite_sort_point": int(source.scalar(body, "m_SpriteSortPoint", "0")),
        "sprite_guid": sprite_reference["guid"],
        "sprite_local_id": sprite_reference["file_id"],
        "mask_guid": mask_reference["guid"],
        "mask_local_id": mask_reference["file_id"],
        "material_guid": material_guid,
        "mask_sprite_script_guid": mask_script_guid,
        "local_position": transform["local_position"],
        "local_rotation_quaternion": transform["local_rotation"],
        "local_scale": transform["local_scale"],
    }


def vector_cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def vector_subtract(a, b):
    return [a[index] - b[index] for index in range(3)]


def normalized(vector):
    length = math.sqrt(sum(value * value for value in vector))
    return [value / length for value in vector] if length > 1.0e-8 else [0.0, 0.0, 1.0]


def export_sprite_obj(path, object_name, world_matrix, sprite_mesh):
    ue_vertices = [
        source.unity_to_ue(source.transform_point(world_matrix, vertex))
        for vertex in sprite_mesh["vertices"]
    ]
    # UE 5.8's legacy FbxFactory treats OBJ as right-handed input and reflects
    # its Y coordinate during StaticMesh import. Feed the inverse reflection so
    # the imported vertex positions are the already-derived UE positions. This
    # is format/import compensation only; the source hierarchy is not rotated.
    obj_vertices = [[vertex[0], -vertex[1], vertex[2]] for vertex in ue_vertices]
    uvs = sprite_mesh["local_uvs"]
    source_indices = sprite_mesh["indices"]
    # UnityX -> -UEX changes handedness, so reverse every triangle just like
    # CodexExportRuntimeCityBuildings.cs does (c,b,a).
    faces = [
        (source_indices[index + 2], source_indices[index + 1], source_indices[index])
        for index in range(0, len(source_indices), 3)
    ]
    first = faces[0]
    ue_normal = normalized(vector_cross(
        vector_subtract(ue_vertices[first[1]], ue_vertices[first[0]]),
        vector_subtract(ue_vertices[first[2]], ue_vertices[first[0]]),
    ))
    obj_normal = normalized(vector_cross(
        vector_subtract(obj_vertices[first[1]], obj_vertices[first[0]]),
        vector_subtract(obj_vertices[first[2]], obj_vertices[first[0]]),
    ))
    lines = [
        "# Unity imported tight SpriteMesh baked into UE centimeters",
        "# CityBuildingContainer yaw={0}; runtime root rotation identity; LOD scale={1}".format(
            source.CONTAINER_YAW_DEGREES,
            source.RUNTIME_LOD_SCALE,
        ),
        "o {0}".format(object_name),
    ]
    for vertex in obj_vertices:
        lines.append("v {0:.9f} {1:.9f} {2:.9f}".format(*vertex))
    for uv in uvs:
        lines.append("vt {0:.9f} {1:.9f}".format(*uv))
    lines.append("vn {0:.9f} {1:.9f} {2:.9f}".format(*obj_normal))
    for face in faces:
        values = [value + 1 for value in face]
        lines.append("f {0}/{0}/1 {1}/{1}/1 {2}/{2}/1".format(*values))
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8")

    mins = [min(vertex[axis] for vertex in ue_vertices) for axis in range(3)]
    maxs = [max(vertex[axis] for vertex in ue_vertices) for axis in range(3)]
    obj_mins = [min(vertex[axis] for vertex in obj_vertices) for axis in range(3)]
    obj_maxs = [max(vertex[axis] for vertex in obj_vertices) for axis in range(3)]
    return {
        "vertex_count": len(ue_vertices),
        "triangle_count": len(faces),
        "ue_bounds_min": mins,
        "ue_bounds_max": maxs,
        "ue_bounds_center": [(mins[axis] + maxs[axis]) * 0.5 for axis in range(3)],
        "ue_bounds_extents": [(maxs[axis] - mins[axis]) * 0.5 for axis in range(3)],
        "ue_baked_normal": ue_normal,
        "obj_import_axis_compensation": "FbxFactory OBJ inverse Y reflection: input=(UE.x,-UE.y,UE.z), imported=(UE.x,UE.y,UE.z)",
        "obj_input_bounds_min": obj_mins,
        "obj_input_bounds_max": obj_maxs,
        "obj_input_normal": obj_normal,
    }


def find_owner():
    state = json.loads(INITIAL_CITY_STATE_PATH.read_text(encoding="utf-8-sig"))
    owners = [item for item in state.get("buildings", []) if int(item.get("type_id", 0)) == 8]
    if len(owners) != 1:
        raise RuntimeError("Expected exactly one initial CityGate owner type 8, found {0}".format(len(owners)))
    return owners[0]


def build():
    required = (
        PREFAB_PATH,
        COLOR_TEXTURE_PATH,
        MASK_TEXTURE_PATH,
        MATERIAL_PATH,
        SHADER_PATH,
        MASK_SPRITE_SOURCE_PATH,
        TOWN_BUILDING_SOURCE_PATH,
        RS_SOURCE_PATH,
        INITIAL_CITY_STATE_PATH,
    )
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise FileNotFoundError("Missing Unity runtime city gate-flag source(s): {0}".format(missing))

    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    failures = []
    prefab = source.parse_prefab(PREFAB_PATH)
    owner = find_owner()
    for asset_path, expected_guid in (
        (PREFAB_PATH, PREFAB_GUID),
        (COLOR_TEXTURE_PATH, COLOR_SPRITE_GUID),
        (MASK_TEXTURE_PATH, MASK_SPRITE_GUID),
        (MATERIAL_PATH, MATERIAL_GUID),
        (SHADER_PATH, SHADER_GUID),
    ):
        actual_guid = guid_from_meta(asset_path)
        if actual_guid != expected_guid:
            failures.append("Unity asset GUID drifted for {0}: expected {1}, found {2}".format(
                unity_relative(asset_path),
                expected_guid,
                actual_guid,
            ))

    rs_body = read_source_text(RS_SOURCE_PATH)
    if not re.search(r"buildGreen\s*=\s*new Color\(62\s*/\s*255\.0f,\s*125\s*/\s*255\.0f,\s*195\s*/\s*255\.0f\)", rs_body):
        failures.append("RS.buildGreen source values drifted from (62,125,195)/255")
    if 'SetColor("_Color_1"' not in read_source_text(TOWN_BUILDING_SOURCE_PATH):
        failures.append("TownBuilding.SetColor no longer writes runtime _Color_1")
    mask_source = read_source_text(MASK_SPRITE_SOURCE_PATH)
    if 'SetTexture("_Mask"' not in mask_source or "m_mask_sprite.texture" not in mask_source:
        failures.append("MaskSprite no longer writes m_mask_sprite.texture into runtime _Mask")

    material_record = parse_material_and_shader(failures)
    color_sprite = load_imported_sprite(COLOR_SPRITE_GUID, SPRITE_LOCAL_ID)
    mask_sprite = load_imported_sprite(MASK_SPRITE_GUID, SPRITE_LOCAL_ID)
    if color_sprite["vertex_count"] != EXPECTED_VERTEX_COUNT or color_sprite["triangle_count"] != EXPECTED_TRIANGLE_COUNT:
        failures.append("city_flag imported tight SpriteMesh drifted: vertices={0}, triangles={1}".format(
            color_sprite["vertex_count"],
            color_sprite["triangle_count"],
        ))
    if color_sprite["mesh_type"] != "Tight":
        failures.append("city_flag Sprite must remain a Tight mesh")
    if color_sprite["texture_size"] != [128, 315] or not nearly_equal_sequence(color_sprite["pivot"], [0.5, 0.5]):
        failures.append("city_flag Sprite texture size or pivot drifted")
    if abs(color_sprite["pixels_per_unit"] - 100.0) > 1.0e-6:
        failures.append("city_flag Sprite PPU drifted from 100")

    baked_path = OUTPUT_ROOT / "city_flag_initial_city_buildGreen_baked.png"
    baked_record = bake_initial_city_flag(color_sprite, mask_sprite, baked_path)
    records = []
    world_cache = {}
    for target in TARGETS:
        renderer_record = parse_sprite_renderer(prefab, target, failures)
        if not renderer_record:
            continue
        world_matrix = source.runtime_world_matrix(prefab, target["transform_file_id"], world_cache)
        ue_transform_origin = source.unity_to_ue(source.transform_point(world_matrix, [0.0, 0.0, 0.0]))
        if not nearly_equal_sequence(ue_transform_origin, target["expected_ue_transform_origin"], 0.02):
            failures.append("flag {0} runtime transform origin drifted: {1}".format(target["flag_id"], ue_transform_origin))
        obj_path = OUTPUT_ROOT / (target["flag_id"] + ".obj")
        geometry = export_sprite_obj(obj_path, target["object_name"], world_matrix, color_sprite)
        records.append({
            **renderer_record,
            "owner_init_id": int(owner.get("init_id", 0)),
            "owner_type_id": int(owner.get("type_id", 0)),
            "owner_model_id": owner.get("visual", {}).get("model_id", ""),
            "source_mode": "unity_active_renderer",
            "manual_layout": False,
            "source_prefab_path": str(PREFAB_PATH.resolve()),
            "source_prefab_unity_path": unity_relative(PREFAB_PATH),
            "source_prefab_guid": PREFAB_GUID,
            "source_prefab_sha256": sha256(PREFAB_PATH),
            "sprite_path": str(COLOR_TEXTURE_PATH.resolve()),
            "sprite_unity_path": unity_relative(COLOR_TEXTURE_PATH),
            "sprite_sha256": sha256(COLOR_TEXTURE_PATH),
            "sprite_meta_sha256": sha256(str(COLOR_TEXTURE_PATH) + ".meta"),
            "mask_path": str(MASK_TEXTURE_PATH.resolve()),
            "mask_unity_path": unity_relative(MASK_TEXTURE_PATH),
            "mask_sha256": sha256(MASK_TEXTURE_PATH),
            "mask_meta_sha256": sha256(str(MASK_TEXTURE_PATH) + ".meta"),
            **material_record,
            "mask_sprite_source_path": str(MASK_SPRITE_SOURCE_PATH.resolve()),
            "mask_sprite_source_sha256": sha256(MASK_SPRITE_SOURCE_PATH),
            "town_building_source_path": str(TOWN_BUILDING_SOURCE_PATH.resolve()),
            "town_building_source_sha256": sha256(TOWN_BUILDING_SOURCE_PATH),
            "rs_source_path": str(RS_SOURCE_PATH.resolve()),
            "rs_source_sha256": sha256(RS_SOURCE_PATH),
            "runtime_world_matrix": world_matrix,
            "runtime_container_yaw": source.CONTAINER_YAW_DEGREES,
            "runtime_lod_scale": source.RUNTIME_LOD_SCALE,
            "runtime_root_transform_override": "local position zero, rotation identity, uniform LOD scale; serialized prefab root yaw ignored",
            "ue_transform_origin": ue_transform_origin,
            "representation": "unity_imported_tight_sprite_world_baked_obj",
            "obj_path": str(obj_path.resolve()),
            "obj_project_path": project_relative(obj_path),
            "obj_sha256": sha256(obj_path),
            **geometry,
            **baked_record,
        })

    if len(records) != 2:
        failures.append("expected exactly 2 drawable active CityGate flag SpriteRenderers, found {0}".format(len(records)))
    if len({record["flag_id"] for record in records}) != len(records):
        failures.append("duplicate CityGate flag IDs")
    if len({record["source_key"] for record in records}) != len(records):
        failures.append("duplicate CityGate flag source keys")

    shared_sprite_mesh = {
        key: value for key, value in color_sprite.items()
        if key not in ("vertices", "indices", "local_uvs")
    }
    shared_sprite_mesh["vertices"] = color_sprite["vertices"]
    shared_sprite_mesh["indices"] = color_sprite["indices"]
    shared_sprite_mesh["local_uvs"] = color_sprite["local_uvs"]
    manifest = {
        "schema": "rok.unity_runtime_city_gate_flags.v1",
        "scope_id": "initial_city_civ101_age1_city_default_day",
        "unity_root": str(UNITY_ROOT.resolve()),
        "source_initial_city_state": str(INITIAL_CITY_STATE_PATH.resolve()),
        "source_initial_city_state_sha256": sha256(INITIAL_CITY_STATE_PATH),
        "source_prefab": str(PREFAB_PATH.resolve()),
        "source_prefab_guid": PREFAB_GUID,
        "source_prefab_sha256": sha256(PREFAB_PATH),
        "container_yaw": source.CONTAINER_YAW_DEGREES,
        "runtime_lod_scale": source.RUNTIME_LOD_SCALE,
        "ue_units_per_unity_unit": source.UE_UNITS_PER_UNITY_UNIT,
        "runtime_build_green": RUNTIME_BUILD_GREEN,
        "main_city_sprite_color": MAIN_CITY_SPRITE_COLOR,
        "shared_sprite_mesh": shared_sprite_mesh,
        "shared_baked_texture": baked_record,
        "active_drawable_flag_renderer_count": len(records),
        "source_active_renderer_closure": {
            "build8_drawable_mesh_renderers": 9,
            "build8_drawable_flag_sprite_renderers": 2,
            "build8_drawable_renderer_total": 11,
            "source_unresolved_active_mesh_renderers": 2,
            "source_unresolved_policy": "MeshFilter.m_Mesh fileID 0; record as source-nondraw and do not fabricate geometry",
        },
        "records": records,
        "failures": failures,
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    verify = {
        "schema": "rok.unity_runtime_city_gate_flags.verify.v1",
        "manifest": str(MANIFEST_PATH.resolve()),
        "manifest_sha256": sha256(MANIFEST_PATH),
        "active_drawable_flag_renderer_count": len(records),
        "tight_sprite_vertex_count": color_sprite["vertex_count"],
        "tight_sprite_triangle_count": color_sprite["triangle_count"],
        "runtime_root_rotation_override": "identity",
        "container_yaw": source.CONTAINER_YAW_DEGREES,
        "actor_transform_policy": "world-baked OBJ; UE actor rotation and scale identity",
        "runtime_color_policy": "initial own-city RS.buildGreen via TownBuilding MPB _Color_1",
        "runtime_mask_policy": "MaskSprite MPB _Mask=city_flag_mask.png",
        "manual_layout_count": sum(1 for record in records if record.get("manual_layout")),
        "failures": failures,
    }
    VERIFY_PATH.write_text(json.dumps(verify, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Rok Unity runtime CityGate flags: records={0} tight_vertices={1} failures={2}".format(
        len(records),
        color_sprite["vertex_count"],
        len(failures),
    ))
    if failures:
        for failure in failures:
            print("FAIL: " + failure)
        raise SystemExit(1)


if __name__ == "__main__":
    build()
