import hashlib
import json
import re
import struct
from collections import defaultdict
from pathlib import Path
from rok_data_paths import resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
UNITY_ASSETS_ROOT = PROJECT_ROOT / "ref" / "client-unity" / "Assets"
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
RUNTIME_DATA_PATH = MIGRATION_ROOT / "unity_world_map_runtime_data.json"
OUTPUT_PATH = MIGRATION_ROOT / "unity_world_map_streaming_assets.json"
SUMMARY_PATH = MIGRATION_ROOT / "unity_world_map_streaming_assets.md"
EXPORT_ROOT = MIGRATION_ROOT / "WorldMapStreamingAssets"
MESH_EXPORT_ROOT = EXPORT_ROOT / "Meshes"
SPRITE_MESH_EXPORT_ROOT = EXPORT_ROOT / "SpriteMeshes"
UE_MESH_ROOT = "/Game/RokUnityWorldMapStreaming/Meshes"
UE_SPRITE_MESH_ROOT = "/Game/RokUnityWorldMapStreaming/SpriteMeshes"
UE_MATERIAL_ROOT = "/Game/RokUnityWorldMapStreaming/Materials"
UE_TEXTURE_ROOT = "/Game/RokUnityWorldMapStreaming/Textures"

EXPECTED_COUNTS = {
    "mesh_assets": 232,
    "unity_mesh_assets": 218,
    "fbx_mesh_assets": 14,
    "materials": 35,
    "sprite_textures": 2,
    "particle_systems": 18,
    "mesh_colliders": 6,
    "null_sprites": 4,
}

CHANNEL_NAMES = (
    "position",
    "normal",
    "tangent",
    "color",
    "uv0",
    "uv1",
    "uv2",
    "uv3",
    "uv4",
    "uv5",
    "uv6",
    "uv7",
    "blend_weight",
    "blend_indices",
)
FORMAT_SIZES = {
    0: 4,  # Float32
    1: 2,  # Float16
    2: 1,  # UNorm8
    3: 1,  # SNorm8
    4: 2,  # UNorm16
    5: 2,  # SNorm16
    6: 1,  # UInt8
    7: 1,  # SInt8
    8: 2,  # UInt16
    9: 2,  # SInt16
    10: 4,  # UInt32
    11: 4,  # SInt32
}
FORMAT_NAMES = {
    0: "Float32",
    1: "Float16",
    2: "UNorm8",
    3: "SNorm8",
    4: "UNorm16",
    5: "SNorm16",
    6: "UInt8",
    7: "SInt8",
    8: "UInt16",
    9: "SInt16",
    10: "UInt32",
    11: "SInt32",
}
DECIMAL_ID_PATTERN = re.compile(r"^-?\d+$")


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


def source_record(path, include_meta=True):
    if not path.is_file():
        raise RuntimeError(f"Missing source file: {rel(path)}")
    result = {
        "path": rel(path),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }
    meta_path = Path(str(path) + ".meta")
    if include_meta and meta_path.is_file():
        result["meta"] = {
            "path": rel(meta_path),
            "size": meta_path.stat().st_size,
            "sha256": sha256(meta_path),
        }
    return result


def parse_ref_fields(payload):
    fields = {}
    for key, value in re.findall(
        r"(\w+):\s*([^,}]+)",
        payload,
    ):
        fields[key] = value.strip()
    file_id = fields.get("fileID", "0")
    if not DECIMAL_ID_PATTERN.fullmatch(file_id):
        raise RuntimeError(f"Non-decimal Unity fileID: {file_id!r}")
    return {
        "file_id": file_id,
        "guid": fields.get("guid", "").lower(),
        "type": int(fields["type"]) if "type" in fields else None,
    }


def build_guid_index():
    index = {}
    for meta_path in sorted(UNITY_ASSETS_ROOT.rglob("*.meta")):
        text = meta_path.read_text(encoding="utf-8", errors="ignore")
        match = re.search(
            r"(?m)^guid:\s*([0-9a-fA-F]{32})\s*$",
            text,
        )
        if not match:
            continue
        asset_path = Path(str(meta_path)[:-5])
        if not asset_path.exists():
            continue
        guid = match.group(1).lower()
        if guid in index and index[guid] != asset_path:
            raise RuntimeError(
                f"Duplicate Unity GUID {guid}: "
                f"{rel(index[guid])}, {rel(asset_path)}"
            )
        index[guid] = asset_path
    return index


def iter_nodes(node):
    yield node
    for child in node.get("children", []):
        yield from iter_nodes(child)


def require_decimal(value, label):
    value = str(value)
    if not DECIMAL_ID_PATTERN.fullmatch(value):
        raise RuntimeError(f"{label} is not a decimal string: {value!r}")
    return value


def render_state(node, renderer):
    return {
        "active_in_hierarchy": bool(node["active_in_hierarchy"]),
        "enabled": int(renderer["enabled"]),
        "rendering_layer_mask": int(renderer["rendering_layer_mask"]),
        "sorting_layer_id": int(renderer["sorting_layer_id"]),
        "sorting_order": int(renderer["sorting_order"]),
        "renderer_priority": int(renderer["renderer_priority"]),
        "cast_shadows": int(renderer["cast_shadows"]),
        "receive_shadows": int(renderer["receive_shadows"]),
    }


def state_key(state):
    return json.dumps(
        state,
        sort_keys=True,
        separators=(",", ":"),
    )


def add_use(store, key, usage):
    store[key].append(usage)


def collect_inventory(runtime_data):
    mesh_uses = defaultdict(list)
    material_uses = defaultdict(list)
    sprite_uses = defaultdict(list)
    fbx_node_bindings = defaultdict(list)

    for recipe in runtime_data["far_lod_recipes"]:
        lod = int(recipe["lod"])
        if lod not in (2, 3, 4, 5):
            raise RuntimeError(
                f"Far recipe {recipe['recipe_id']} has unexpected LOD {lod}"
            )
        for node in iter_nodes(recipe["root"]):
            components_by_type = defaultdict(list)
            for component in node.get("components", []):
                component_id = require_decimal(
                    component["file_id"],
                    f"{recipe['recipe_id']} component fileID",
                )
                if component_id != str(component["file_id"]):
                    raise RuntimeError("Unity component fileID lost precision")
                components_by_type[component["type"]].append(component)

            mesh_filters = components_by_type["MeshFilter"]
            mesh_renderers = components_by_type["MeshRenderer"]
            sprite_renderers = components_by_type["SpriteRenderer"]
            for mesh_filter in mesh_filters:
                mesh_ref = mesh_filter["mesh"]
                if mesh_ref["is_null"]:
                    raise RuntimeError(
                        f"Null MeshFilter source at {node['node_path']}"
                    )
                path = mesh_ref["asset_path"]
                file_id = require_decimal(
                    mesh_ref["file_id"],
                    f"{path} Mesh fileID",
                )
                usage = {
                    "lod": lod,
                    "recipe_id": recipe["recipe_id"],
                    "tile_type_id": recipe["tile_type_id"],
                    "node_name": node["name"],
                    "node_path": node["node_path"],
                    "game_object_file_id": require_decimal(
                        node["game_object_file_id"],
                        f"{node['node_path']} GameObject fileID",
                    ),
                    "component_file_id": require_decimal(
                        mesh_filter["file_id"],
                        f"{node['node_path']} MeshFilter fileID",
                    ),
                    "source_asset_file_id": file_id,
                    "source_guid": mesh_ref["guid"].lower(),
                }
                add_use(mesh_uses, (path, file_id), usage)
                if path.lower().endswith(".fbx"):
                    fbx_node_bindings[(path, file_id)].append(
                        {
                            **usage,
                            "mesh_filter_count_on_game_object": len(
                                mesh_filters
                            ),
                            "mesh_renderer_count_on_game_object": len(
                                mesh_renderers
                            ),
                            "enabled_mesh_renderer_count_on_game_object": sum(
                                int(item["enabled"]) == 1
                                for item in mesh_renderers
                            ),
                            "renderer_materials": [
                                {
                                    "path": item["asset_path"],
                                    "file_id": require_decimal(
                                        item["file_id"],
                                        "FBX renderer material fileID",
                                    ),
                                    "guid": item["guid"].lower(),
                                }
                                for renderer in mesh_renderers
                                if int(renderer["enabled"]) == 1
                                for item in renderer["materials"]
                                if not item["is_null"]
                            ],
                        }
                    )

            for renderer in mesh_renderers + sprite_renderers:
                renderer_type = renderer["type"]
                component_id = require_decimal(
                    renderer["file_id"],
                    f"{node['node_path']} renderer fileID",
                )
                state = render_state(node, renderer)
                for slot_index, material_ref in enumerate(
                    renderer.get("materials", [])
                ):
                    if material_ref["is_null"]:
                        raise RuntimeError(
                            f"Null {renderer_type} material at "
                            f"{node['node_path']} slot {slot_index}"
                        )
                    path = material_ref["asset_path"]
                    file_id = require_decimal(
                        material_ref["file_id"],
                        f"{path} material fileID",
                    )
                    add_use(
                        material_uses,
                        (path, file_id),
                        {
                            "lod": lod,
                            "recipe_id": recipe["recipe_id"],
                            "tile_type_id": recipe["tile_type_id"],
                            "node_path": node["node_path"],
                            "renderer_type": renderer_type,
                            "component_file_id": component_id,
                            "source_asset_file_id": file_id,
                            "source_guid": material_ref["guid"].lower(),
                            "material_slot": slot_index,
                            "render_state": state,
                        },
                    )

            for sprite_renderer in sprite_renderers:
                sprite_ref = sprite_renderer["sprite"]
                if sprite_ref["is_null"]:
                    continue
                material_refs = [
                    item
                    for item in sprite_renderer["materials"]
                    if not item["is_null"]
                ]
                if len(material_refs) != 1:
                    raise RuntimeError(
                        f"SpriteRenderer at {node['node_path']} expected "
                        f"one source material, found {len(material_refs)}"
                    )
                path = sprite_ref["asset_path"]
                file_id = require_decimal(
                    sprite_ref["file_id"],
                    f"{path} sprite fileID",
                )
                add_use(
                    sprite_uses,
                    (path, file_id),
                    {
                        "lod": lod,
                        "recipe_id": recipe["recipe_id"],
                        "tile_type_id": recipe["tile_type_id"],
                        "node_path": node["node_path"],
                        "component_file_id": require_decimal(
                            sprite_renderer["file_id"],
                            f"{node['node_path']} SpriteRenderer fileID",
                        ),
                        "source_asset_file_id": file_id,
                        "source_guid": sprite_ref["guid"].lower(),
                        "render_state": render_state(
                            node,
                            sprite_renderer,
                        ),
                        "color": sprite_renderer["color"],
                        "flip_x": int(sprite_renderer["flip_x"]),
                        "flip_y": int(sprite_renderer["flip_y"]),
                        "draw_mode": int(sprite_renderer["draw_mode"]),
                        "size": sprite_renderer["size"],
                        "source_material": {
                            "path": material_refs[0]["asset_path"],
                            "file_id": require_decimal(
                                material_refs[0]["file_id"],
                                "SpriteRenderer material fileID",
                            ),
                            "guid": material_refs[0]["guid"].lower(),
                        },
                    },
                )

    return mesh_uses, material_uses, sprite_uses, fbx_node_bindings


def collect_blocked_component_records(runtime_data):
    particles = []
    colliders = []
    for recipe in runtime_data["far_lod_recipes"]:
        lod = int(recipe["lod"])
        for node in iter_nodes(recipe["root"]):
            by_type = defaultdict(list)
            for component in node.get("components", []):
                by_type[component["type"]].append(component)
            particle_systems = by_type["ParticleSystem"]
            particle_renderers = by_type["ParticleSystemRenderer"]
            if particle_systems or particle_renderers:
                if (
                    len(particle_systems) != 1
                    or len(particle_renderers) != 1
                ):
                    raise RuntimeError(
                        f"Ambiguous particle component pairing at "
                        f"{node['node_path']}"
                    )
                system = particle_systems[0]
                renderer = particle_renderers[0]
                particles.append(
                    {
                        "lod": lod,
                        "recipe_id": recipe["recipe_id"],
                        "node_path": node["node_path"],
                        "game_object_file_id": require_decimal(
                            node["game_object_file_id"],
                            "particle GameObject fileID",
                        ),
                        "particle_system_file_id": require_decimal(
                            system["file_id"],
                            "ParticleSystem fileID",
                        ),
                        "particle_renderer_file_id": require_decimal(
                            renderer["file_id"],
                            "ParticleSystemRenderer fileID",
                        ),
                        "system_serialized_body_sha256": system[
                            "serialized_body_sha256"
                        ],
                        "renderer_serialized_body_sha256": renderer[
                            "serialized_body_sha256"
                        ],
                        "render_mode": int(renderer["render_mode"]),
                        "source_materials": [
                            {
                                "slot_index": slot,
                                "path": ref["asset_path"],
                                "file_id": require_decimal(
                                    ref["file_id"],
                                    "particle material fileID",
                                ),
                                "guid": ref["guid"].lower(),
                                "source": source_record(
                                    resolve(ref["asset_path"])
                                ),
                            }
                            for slot, ref in enumerate(
                                renderer["materials"]
                            )
                            if not ref["is_null"]
                        ],
                        "runtime_policy": (
                            "pending_niagara_or_explicit_visual_omission"
                        ),
                    }
                )
            for collider in by_type["MeshCollider"]:
                mesh_ref = collider["mesh"]
                if mesh_ref["is_null"]:
                    raise RuntimeError(
                        f"Null MeshCollider source at {node['node_path']}"
                    )
                colliders.append(
                    {
                        "lod": lod,
                        "recipe_id": recipe["recipe_id"],
                        "node_path": node["node_path"],
                        "game_object_file_id": require_decimal(
                            node["game_object_file_id"],
                            "collider GameObject fileID",
                        ),
                        "mesh_collider_file_id": require_decimal(
                            collider["file_id"],
                            "MeshCollider fileID",
                        ),
                        "source_mesh": {
                            "path": mesh_ref["asset_path"],
                            "file_id": require_decimal(
                                mesh_ref["file_id"],
                                "collider mesh fileID",
                            ),
                            "guid": mesh_ref["guid"].lower(),
                            "source": source_record(
                                resolve(mesh_ref["asset_path"])
                            ),
                        },
                        "enabled": int(collider["enabled"]),
                        "is_trigger": int(collider["is_trigger"]),
                        "convex": int(collider["convex"]),
                        "cooking_options": int(
                            collider["cooking_options"]
                        ),
                        "serialized_body_sha256": collider[
                            "serialized_body_sha256"
                        ],
                        "runtime_policy": (
                            "pending_collision_promotion_and_query_contract"
                        ),
                    }
                )
    return particles, colliders


def aggregate_usage(uses):
    by_lod = {}
    for lod in (2, 3, 4, 5):
        selected = [item for item in uses if item["lod"] == lod]
        state_groups = defaultdict(list)
        for item in selected:
            if "render_state" in item:
                state_groups[state_key(item["render_state"])].append(
                    item["component_file_id"]
                )
        by_lod[str(lod)] = {
            "count": len(selected),
            "component_file_ids": sorted(
                {
                    require_decimal(
                        item["component_file_id"],
                        "usage component fileID",
                    )
                    for item in selected
                },
                key=int,
            ),
            "source_asset_file_ids": sorted(
                {
                    require_decimal(
                        item["source_asset_file_id"],
                        "usage source fileID",
                    )
                    for item in selected
                },
                key=int,
            ),
            "recipe_ids": sorted(
                {item["recipe_id"] for item in selected}
            ),
        }
        if state_groups:
            by_lod[str(lod)]["render_state_groups"] = [
                {
                    "state": json.loads(key),
                    "count": len(component_ids),
                    "component_file_ids": sorted(
                        set(component_ids),
                        key=int,
                    ),
                }
                for key, component_ids in sorted(state_groups.items())
            ]
    return {
        "total_count": len(uses),
        "by_lod": by_lod,
    }


def parse_mesh_asset(path):
    text = path.read_text(encoding="utf-8", errors="strict")
    required_patterns = {
        "vertex_count": r"(?m)^\s*m_VertexCount:\s*(\d+)\s*$",
        "index_format": r"(?m)^\s*m_IndexFormat:\s*(\d+)\s*$",
        "index_buffer": (
            r"(?m)^\s*m_IndexBuffer:\s*([0-9a-fA-F]+)\s*$"
        ),
        "vertex_data": r"(?m)^\s*_typelessdata:\s*([0-9a-fA-F]+)\s*$",
        "data_size": r"(?m)^\s*m_DataSize:\s*(\d+)\s*$",
    }
    matches = {
        key: re.search(pattern, text)
        for key, pattern in required_patterns.items()
    }
    missing = [key for key, match in matches.items() if not match]
    if missing:
        raise RuntimeError(
            f"{rel(path)} missing Unity mesh fields: {missing}"
        )
    vertex_count = int(matches["vertex_count"].group(1))
    index_format = int(matches["index_format"].group(1))
    if index_format not in (0, 1):
        raise RuntimeError(
            f"{rel(path)} has unsupported index format {index_format}"
        )
    index_buffer = bytes.fromhex(matches["index_buffer"].group(1))
    vertex_data = bytes.fromhex(matches["vertex_data"].group(1))
    if len(vertex_data) != int(matches["data_size"].group(1)):
        raise RuntimeError(f"{rel(path)} vertex data size mismatch")

    channel_start = text.find("    m_Channels:")
    channel_end = text.find("    m_DataSize:", channel_start)
    if channel_start < 0 or channel_end < 0:
        raise RuntimeError(f"{rel(path)} lacks a complete channel table")
    channel_rows = re.findall(
        r"- stream:\s*(\d+)\s*\n"
        r"\s*offset:\s*(\d+)\s*\n"
        r"\s*format:\s*(\d+)\s*\n"
        r"\s*dimension:\s*(\d+)",
        text[channel_start:channel_end],
    )
    if len(channel_rows) != len(CHANNEL_NAMES):
        raise RuntimeError(
            f"{rel(path)} has {len(channel_rows)} vertex channels; "
            f"expected {len(CHANNEL_NAMES)}"
        )
    channels = []
    for index, (stream, offset, data_format, dimension) in enumerate(
        channel_rows
    ):
        channel = {
            "semantic": CHANNEL_NAMES[index],
            "stream": int(stream),
            "offset": int(offset),
            "format": int(data_format),
            "dimension": int(dimension),
        }
        if channel["dimension"] > 0:
            if channel["stream"] != 0:
                raise RuntimeError(
                    f"{rel(path)} uses unsupported vertex stream "
                    f"{channel['stream']}"
                )
            if channel["format"] not in FORMAT_SIZES:
                raise RuntimeError(
                    f"{rel(path)} uses unknown vertex format "
                    f"{channel['format']}"
                )
        channels.append(channel)
    active = [item for item in channels if item["dimension"] > 0]
    stride = max(
        item["offset"]
        + FORMAT_SIZES[item["format"]] * item["dimension"]
        for item in active
    )
    if stride * vertex_count != len(vertex_data):
        raise RuntimeError(
            f"{rel(path)} stride audit failed: "
            f"{stride} * {vertex_count} != {len(vertex_data)}"
        )
    by_semantic = {item["semantic"]: item for item in channels}
    position = by_semantic["position"]
    normal = by_semantic["normal"]
    color = by_semantic["color"]
    uv0 = by_semantic["uv0"]
    if (
        position["format"],
        position["dimension"],
        uv0["format"],
        uv0["dimension"],
    ) != (0, 3, 0, 2):
        raise RuntimeError(
            f"{rel(path)} has unsupported position or UV0 encoding"
        )
    if normal["dimension"] and (
        normal["format"],
        normal["dimension"],
    ) != (0, 3):
        raise RuntimeError(f"{rel(path)} has unsupported normal encoding")
    if color["dimension"] and (
        color["format"],
        color["dimension"],
    ) not in ((0, 4), (2, 4)):
        raise RuntimeError(
            f"{rel(path)} has unsupported vertex color encoding"
        )
    unsupported = [
        name
        for name in ("uv1", "uv2", "uv3", "uv4", "uv5", "uv6", "uv7")
        if by_semantic[name]["dimension"]
    ]
    if unsupported:
        raise RuntimeError(
            f"{rel(path)} has additional UV channels that OBJ export "
            f"would not preserve: {unsupported}"
        )

    def unpack_float(channel, vertex_index):
        return struct.unpack_from(
            "<" + "f" * channel["dimension"],
            vertex_data,
            vertex_index * stride + channel["offset"],
        )

    positions = []
    normals = []
    colors = []
    uvs = []
    for vertex_index in range(vertex_count):
        px, py, pz = unpack_float(position, vertex_index)
        positions.append((px * 100.0, -pz * 100.0, py * 100.0))
        if normal["dimension"]:
            nx, ny, nz = unpack_float(normal, vertex_index)
            normals.append((nx, -nz, ny))
        if color["dimension"]:
            offset = vertex_index * stride + color["offset"]
            if color["format"] == 0:
                colors.append(
                    struct.unpack_from("<ffff", vertex_data, offset)
                )
            else:
                colors.append(
                    tuple(value / 255.0 for value in vertex_data[offset:offset + 4])
                )
        uvs.append(unpack_float(uv0, vertex_index))

    submesh_start = text.find("  m_SubMeshes:")
    submesh_end = text.find("  m_Shapes:", submesh_start)
    if submesh_start < 0 or submesh_end < 0:
        raise RuntimeError(f"{rel(path)} lacks submesh data")
    submeshes = []
    for block in re.split(
        r"\n\s*-\s+serializedVersion:\s*\d+",
        text[submesh_start:submesh_end],
    )[1:]:
        fields = {}
        for key in ("firstByte", "indexCount", "topology", "baseVertex"):
            match = re.search(
                rf"(?m)^\s*{key}:\s*(-?\d+)\s*$",
                block,
            )
            if not match:
                raise RuntimeError(
                    f"{rel(path)} submesh lacks {key}"
                )
            fields[key] = int(match.group(1))
        if fields["topology"] != 0:
            raise RuntimeError(
                f"{rel(path)} has non-triangle topology "
                f"{fields['topology']}"
            )
        if fields["indexCount"] % 3:
            raise RuntimeError(
                f"{rel(path)} has a partial triangle index buffer"
            )
        submeshes.append(fields)
    if not submeshes:
        raise RuntimeError(f"{rel(path)} has no submeshes")

    index_size = 2 if index_format == 0 else 4
    index_code = "<H" if index_format == 0 else "<I"
    triangles = []
    for submesh_index, submesh in enumerate(submeshes):
        end = (
            submesh["firstByte"]
            + submesh["indexCount"] * index_size
        )
        if end > len(index_buffer):
            raise RuntimeError(f"{rel(path)} index range exceeds buffer")
        indices = [
            struct.unpack_from(
                index_code,
                index_buffer,
                submesh["firstByte"] + index * index_size,
            )[0]
            + submesh["baseVertex"]
            for index in range(submesh["indexCount"])
        ]
        for index in range(0, len(indices), 3):
            triangles.append(
                (
                    submesh_index,
                    indices[index],
                    indices[index + 2],
                    indices[index + 1],
                )
            )
    if any(
        vertex < 0 or vertex >= vertex_count
        for triangle in triangles
        for vertex in triangle[1:]
    ):
        raise RuntimeError(f"{rel(path)} has out-of-range triangle indices")

    return {
        "positions": positions,
        "normals": normals,
        "colors": colors,
        "uvs": uvs,
        "triangles": triangles,
        "submesh_count": len(submeshes),
        "vertex_stride": stride,
        "channels": [
            {
                **item,
                "format_name": FORMAT_NAMES.get(item["format"]),
            }
            for item in active
        ],
        "raw_vertex_data_sha256": sha256_bytes(vertex_data),
        "raw_index_buffer_sha256": sha256_bytes(index_buffer),
    }


def number(value):
    if value == 0:
        value = 0.0
    return format(value, ".9g")


def make_obj_bytes(ue_name, source_path, parsed):
    lines = [
        f"# Unity source: {rel(source_path)}",
        "# Axis: Unity (x,y,z) -> OBJ (x,-z,y) * 100 cm",
        "# Vertex color extension: v x y z r g b a when COLOR is present",
        f"o {ue_name}",
    ]
    for index, (x, y, z) in enumerate(parsed["positions"]):
        values = [number(x), number(y), number(z)]
        if parsed["colors"]:
            values.extend(number(item) for item in parsed["colors"][index])
        lines.append("v " + " ".join(values))
    for u, v in parsed["uvs"]:
        lines.append(f"vt {number(u)} {number(v)}")
    for nx, ny, nz in parsed["normals"]:
        lines.append(f"vn {number(nx)} {number(ny)} {number(nz)}")
    current_submesh = None
    has_normals = bool(parsed["normals"])
    for submesh, a, b, c in parsed["triangles"]:
        if submesh != current_submesh:
            lines.append(f"g submesh_{submesh}")
            current_submesh = submesh
        indices = []
        for vertex in (a + 1, b + 1, c + 1):
            if has_normals:
                indices.append(f"{vertex}/{vertex}/{vertex}")
            else:
                indices.append(f"{vertex}/{vertex}")
        lines.append("f " + " ".join(indices))
    return ("\n".join(lines) + "\n").encode("utf-8")


def ue_name(prefix, path, file_id):
    stem = re.sub(r"[^A-Za-z0-9]+", "_", path.stem).strip("_")
    if not stem:
        stem = "Asset"
    stem = stem[:48]
    identity = f"{rel(path)}|{file_id}".encode("utf-8")
    suffix = hashlib.sha1(identity).hexdigest()[:10]
    return f"{prefix}_{stem}_{suffix}"


def parse_internal_id_name_table(meta_text):
    match = re.search(
        r"(?ms)^\s*internalIDToNameTable:\s*(.*?)(?=^\s*\w[^:\n]*:|\Z)",
        meta_text,
    )
    if not match or match.group(1).strip() in ("", "[]"):
        return {}
    table = {}
    for class_id, file_id, name in re.findall(
        r"(?ms)-\s*first:\s*\n"
        r"\s*([0-9]+):\s*(-?[0-9]+)\s*\n"
        r"\s*second:\s*(.*?)\s*(?=\n\s*-\s*first:|\Z)",
        match.group(1),
    ):
        key = (class_id, file_id)
        if key in table:
            raise RuntimeError(f"Duplicate ModelImporter internal ID {key}")
        table[key] = name.strip()
    return table


def build_fbx_contract(path, file_id, uses, bindings):
    meta_path = Path(str(path) + ".meta")
    if not meta_path.is_file():
        raise RuntimeError(f"Missing FBX meta: {rel(meta_path)}")
    meta_text = meta_path.read_text(encoding="utf-8", errors="strict")
    guid_match = re.search(
        r"(?m)^guid:\s*([0-9a-fA-F]{32})\s*$",
        meta_text,
    )
    if not guid_match:
        raise RuntimeError(f"Missing FBX GUID: {rel(meta_path)}")
    guid = guid_match.group(1).lower()
    source_guids = {item["source_guid"] for item in uses}
    if source_guids != {guid}:
        raise RuntimeError(
            f"{rel(path)} GUID mismatch: {source_guids} != {guid}"
        )
    node_names = {item["node_name"] for item in bindings}
    if len(node_names) != 1:
        raise RuntimeError(
            f"{rel(path)} fileID {file_id} maps to ambiguous nodes: "
            f"{sorted(node_names)}"
        )
    if any(
        item["mesh_filter_count_on_game_object"] != 1
        or item["mesh_renderer_count_on_game_object"] != 1
        or item["enabled_mesh_renderer_count_on_game_object"] != 1
        for item in bindings
    ):
        raise RuntimeError(
            f"{rel(path)} fileID {file_id} is not bound to exactly one "
            "MeshFilter and one enabled MeshRenderer on every source node"
        )
    material_slots = {
        tuple(
            (item["path"], item["file_id"], item["guid"])
            for item in binding["renderer_materials"]
        )
        for binding in bindings
    }
    if len(material_slots) != 1:
        raise RuntimeError(
            f"{rel(path)} fileID {file_id} has ambiguous material slots"
        )
    slots = next(iter(material_slots))
    if len(slots) != 1:
        raise RuntimeError(
            f"{rel(path)} fileID {file_id} expected one exact submesh "
            f"material slot, found {len(slots)}"
        )
    node_name = next(iter(node_names))
    exact_occurrences = path.read_bytes().count(node_name.encode("utf-8"))
    if exact_occurrences != 1:
        raise RuntimeError(
            f"{rel(path)} exact node {node_name!r} occurs "
            f"{exact_occurrences} times in FBX binary"
        )
    internal_table = parse_internal_id_name_table(meta_text)
    meta_candidates = sorted(
        {
            name
            for (_, internal_file_id), name in internal_table.items()
            if internal_file_id == file_id
        }
    )
    if meta_candidates and meta_candidates != [node_name]:
        raise RuntimeError(
            f"{rel(meta_path)} maps {file_id} to {meta_candidates}, "
            f"not exact runtime node {node_name}"
        )
    return {
        "status": "ready_exact_split_import",
        "mapping_source": (
            "prefab_meshfilter_exact_node_plus_unique_fbx_node"
        ),
        "source_fbx_guid": guid,
        "source_mesh_file_id": file_id,
        "exact_node_name": node_name,
        "exact_node_utf8_occurrence_count": exact_occurrences,
        "model_importer_internal_id_name_table_entry": (
            node_name if meta_candidates else None
        ),
        "model_importer_internal_id_name_table_policy": (
            "exact_entry_confirmed"
            if meta_candidates
            else "empty_table_strict_prefab_plus_binary_fallback"
        ),
        "combine_meshes": False,
        "node_selection": {
            "mode": "exact_name",
            "value": node_name,
            "case_sensitive": True,
            "first_or_contains_matching_forbidden": True,
        },
        "submesh_slots": [
            {
                "slot_index": index,
                "source_material_path": slot[0],
                "source_material_file_id": slot[1],
                "source_material_guid": slot[2],
            }
            for index, slot in enumerate(slots)
        ],
        "collision_import_policy": (
            "disabled_until_dedicated_collision_policy_is_accepted"
        ),
    }


def parse_inline_xy(payload, field_name):
    match = re.search(
        rf"(?m)^\s*{re.escape(field_name)}:\s*"
        r"\{x:\s*([-+0-9.eE]+),\s*y:\s*([-+0-9.eE]+)\}\s*$",
        payload,
    )
    if not match:
        raise RuntimeError(f"Missing sprite {field_name}")
    return {"x": float(match.group(1)), "y": float(match.group(2))}


def parse_inline_xyzw(payload, field_name):
    match = re.search(
        rf"(?m)^\s*{re.escape(field_name)}:\s*"
        r"\{x:\s*([-+0-9.eE]+),\s*y:\s*([-+0-9.eE]+),\s*"
        r"z:\s*([-+0-9.eE]+),\s*w:\s*([-+0-9.eE]+)\}\s*$",
        payload,
    )
    if not match:
        raise RuntimeError(f"Missing sprite {field_name}")
    return {
        key: float(value)
        for key, value in zip("xyzw", match.groups())
    }


def parse_png_dimensions(path):
    header = path.read_bytes()[:24]
    if (
        len(header) != 24
        or header[:8] != b"\x89PNG\r\n\x1a\n"
        or header[12:16] != b"IHDR"
    ):
        raise RuntimeError(f"Not a supported PNG source: {rel(path)}")
    width, height = struct.unpack(">II", header[16:24])
    if width <= 0 or height <= 0:
        raise RuntimeError(f"Invalid PNG dimensions: {rel(path)}")
    return {"width": width, "height": height}


def parse_sprite_sheet(meta_path, texture_path):
    text = meta_path.read_text(encoding="utf-8", errors="strict")

    def scalar(name, converter=float):
        match = re.search(
            rf"(?m)^  {re.escape(name)}:\s*([-+0-9.eE]+)\s*$",
            text,
        )
        if not match:
            raise RuntimeError(f"{rel(meta_path)} lacks {name}")
        return converter(match.group(1))

    importer = {
        "sprite_pixels_to_units": scalar("spritePixelsToUnits"),
        "sprite_mesh_type": scalar("spriteMeshType", int),
        "default_alignment": scalar("alignment", int),
        "default_pivot": parse_inline_xy(text, "spritePivot"),
        "default_border": parse_inline_xyzw(text, "spriteBorder"),
        "max_texture_size": scalar("maxTextureSize", int),
        "source_texture_dimensions": parse_png_dimensions(texture_path),
        "texture_dimensions_source": (
            "PNG_IHDR; TextureImporter meta does not serialize source "
            "pixel width/height"
        ),
    }
    if importer["sprite_pixels_to_units"] <= 0:
        raise RuntimeError(f"{rel(meta_path)} has invalid PPU")
    if importer["sprite_mesh_type"] not in (0, 1):
        raise RuntimeError(
            f"{rel(meta_path)} has unknown spriteMeshType"
        )

    sheet_start = text.find("  spriteSheet:")
    if sheet_start < 0:
        raise RuntimeError(f"{rel(meta_path)} lacks spriteSheet")
    blocks = re.split(
        r"(?m)^    - serializedVersion:\s*\d+\s*$",
        text[sheet_start:],
    )[1:]
    records = {}
    for block in blocks:
        name_match = re.search(
            r"(?m)^\s*name:\s*(.*?)\s*$",
            block,
        )
        file_id_match = re.search(
            r"(?m)^\s*internalID:\s*(-?\d+)\s*$",
            block,
        )
        rect_match = re.search(
            r"(?ms)^\s*rect:\s*\n"
            r"\s*serializedVersion:\s*\d+\s*\n"
            r"\s*x:\s*([-+0-9.eE]+)\s*\n"
            r"\s*y:\s*([-+0-9.eE]+)\s*\n"
            r"\s*width:\s*([-+0-9.eE]+)\s*\n"
            r"\s*height:\s*([-+0-9.eE]+)\s*$",
            block,
        )
        alignment_match = re.search(
            r"(?m)^\s*alignment:\s*(\d+)\s*$",
            block,
        )
        if not all(
            (
                name_match,
                file_id_match,
                rect_match,
                alignment_match,
            )
        ):
            raise RuntimeError(
                f"{rel(meta_path)} has an incomplete sprite record"
            )
        file_id = require_decimal(
            file_id_match.group(1),
            "sprite internalID",
        )
        if file_id in records:
            raise RuntimeError(
                f"Duplicate sprite internalID {file_id} in {rel(meta_path)}"
            )
        rect_values = tuple(float(value) for value in rect_match.groups())
        rect = {
            key: value
            for key, value in zip(
                ("x", "y", "width", "height"),
                rect_values,
            )
        }
        if rect["width"] <= 0 or rect["height"] <= 0:
            raise RuntimeError(
                f"{rel(meta_path)} sprite {file_id} has invalid rect"
            )
        dimensions = importer["source_texture_dimensions"]
        if (
            rect["x"] < 0
            or rect["y"] < 0
            or rect["x"] + rect["width"] > dimensions["width"]
            or rect["y"] + rect["height"] > dimensions["height"]
        ):
            raise RuntimeError(
                f"{rel(meta_path)} sprite {file_id} rect exceeds atlas"
            )
        records[file_id] = {
            "name": name_match.group(1).strip(),
            "rect": rect,
            "pivot": parse_inline_xy(block, "pivot"),
            "alignment": int(alignment_match.group(1)),
            "border": parse_inline_xyzw(block, "border"),
            "sprite_mesh_type": importer["sprite_mesh_type"],
        }
    return importer, records


def build_sprite_quad(meta, importer):
    rect = meta["rect"]
    pivot = meta["pivot"]
    ppu = importer["sprite_pixels_to_units"]
    width = rect["width"] / ppu
    height = rect["height"] / ppu
    left = -pivot["x"] * width
    right = (1.0 - pivot["x"]) * width
    bottom = -pivot["y"] * height
    top = (1.0 - pivot["y"]) * height
    atlas = importer["source_texture_dimensions"]
    u0 = rect["x"] / atlas["width"]
    v0 = rect["y"] / atlas["height"]
    u1 = (rect["x"] + rect["width"]) / atlas["width"]
    v1 = (rect["y"] + rect["height"]) / atlas["height"]
    return {
        "unity_local_size": {"x": width, "y": height},
        "unity_local_bounds": {
            "left": left,
            "right": right,
            "bottom": bottom,
            "top": top,
        },
        "obj_positions_cm": [
            (left * 100.0, 0.0, bottom * 100.0),
            (right * 100.0, 0.0, bottom * 100.0),
            (right * 100.0, 0.0, top * 100.0),
            (left * 100.0, 0.0, top * 100.0),
        ],
        "unity_source_uvs": [
            (u0, v0),
            (u1, v0),
            (u1, v1),
            (u0, v1),
        ],
        "obj_normal": (0.0, -1.0, 0.0),
        "triangles": ((1, 2, 3), (1, 3, 4)),
    }


def make_sprite_obj_bytes(ue_name, texture_path, file_id, quad):
    lines = [
        f"# Unity sprite source: {rel(texture_path)} fileID {file_id}",
        "# Source UV: Unity normalized bottom-left atlas coordinates",
        "# Axis: Unity (x,y,z) -> OBJ (x,-z,y) * 100 cm",
        "# Axis determinant: +1; source winding is preserved",
        f"o {ue_name}",
    ]
    for x, y, z in quad["obj_positions_cm"]:
        lines.append(f"v {number(x)} {number(y)} {number(z)}")
    for u, v in quad["unity_source_uvs"]:
        lines.append(f"vt {number(u)} {number(v)}")
    nx, ny, nz = quad["obj_normal"]
    lines.append(f"vn {number(nx)} {number(ny)} {number(nz)}")
    lines.append("g sprite_quad")
    for a, b, c in quad["triangles"]:
        lines.append(f"f {a}/{a}/1 {b}/{b}/1 {c}/{c}/1")
    return ("\n".join(lines) + "\n").encode("utf-8")


def parse_material(path, guid_index):
    text = path.read_text(encoding="utf-8", errors="strict")
    shader_match = re.search(
        r"m_Shader:\s*\{([^}]*)\}",
        text,
    )
    if not shader_match:
        raise RuntimeError(f"{rel(path)} lacks m_Shader")
    shader_ref = parse_ref_fields(shader_match.group(1))
    queue_match = re.search(
        r"(?m)^\s*m_CustomRenderQueue:\s*(-?\d+)\s*$",
        text,
    )
    if not queue_match:
        raise RuntimeError(f"{rel(path)} lacks m_CustomRenderQueue")
    custom_queue = int(queue_match.group(1))
    project_shader_path = None
    project_shader_text = ""
    if shader_ref["guid"] != "0000000000000000f000000000000000":
        project_shader_path = guid_index.get(shader_ref["guid"])
        if not project_shader_path:
            raise RuntimeError(
                f"{rel(path)} has unresolved shader GUID "
                f"{shader_ref['guid']}"
            )
        project_shader_text = project_shader_path.read_text(
            encoding="utf-8",
            errors="strict",
        )

    tex_start = text.find("    m_TexEnvs:")
    float_start = text.find("    m_Floats:", tex_start)
    color_start = text.find("    m_Colors:", float_start)
    if min(tex_start, float_start, color_start) < 0:
        raise RuntimeError(f"{rel(path)} has incomplete saved properties")
    texture_properties = []
    texture_pattern = re.compile(
        r"(?ms)^\s*-\s*([^:\n]+):\s*\n"
        r"\s*m_Texture:\s*\{([^}]*)\}\s*\n"
        r"\s*m_Scale:\s*\{x:\s*([-+0-9.eE]+),\s*y:\s*([-+0-9.eE]+)\}\s*\n"
        r"\s*m_Offset:\s*\{x:\s*([-+0-9.eE]+),\s*y:\s*([-+0-9.eE]+)\}\s*$"
    )
    for name, ref_payload, sx, sy, ox, oy in texture_pattern.findall(
        text[tex_start:float_start]
    ):
        texture_ref = parse_ref_fields(ref_payload)
        record = {
            "property": name.strip(),
            "texture_ref": texture_ref,
            "scale": {"x": float(sx), "y": float(sy)},
            "offset": {"x": float(ox), "y": float(oy)},
        }
        if texture_ref["file_id"] != "0":
            guid = texture_ref["guid"]
            texture_path = guid_index.get(guid)
            if not texture_path:
                property_pattern = re.compile(
                    rf"(?<![A-Za-z0-9_]){re.escape(name.strip())}"
                    rf"(?![A-Za-z0-9_])"
                )
                if (
                    not project_shader_text
                    or property_pattern.search(project_shader_text)
                ):
                    raise RuntimeError(
                        f"{rel(path)} render-bound property {name} has "
                        f"unresolved texture GUID {guid}"
                    )
                record["resolution_status"] = (
                    "unresolved_stale_property_not_declared_by_bound_shader"
                )
                record["render_binding"] = (
                    "not_declared_or_sampled_by_exact_project_shader"
                )
            else:
                record["source_texture"] = source_record(texture_path)
                record["resolution_status"] = "resolved_source"
        texture_properties.append(record)
    if not texture_properties:
        raise RuntimeError(f"{rel(path)} has no parsed texture properties")

    floats = {}
    for name, value in re.findall(
        r"(?m)^\s*-\s*([^:\n]+):\s*([-+0-9.eE]+)\s*$",
        text[float_start:color_start],
    ):
        floats[name.strip()] = float(value)
    colors = {}
    for name, payload in re.findall(
        r"(?m)^\s*-\s*([^:\n]+):\s*\{([^}]*)\}\s*$",
        text[color_start:],
    ):
        values = {
            key: float(value)
            for key, value in re.findall(
                r"([rgba]):\s*([-+0-9.eE]+)",
                payload,
            )
        }
        if set(values) != {"r", "g", "b", "a"}:
            raise RuntimeError(
                f"{rel(path)} property {name} has incomplete color"
            )
        colors[name.strip()] = values

    if shader_ref["guid"] == "0000000000000000f000000000000000":
        shader = {
            "kind": "unity_builtin",
            "file_id": shader_ref["file_id"],
            "guid": shader_ref["guid"],
            "source_shader": None,
            "shader_name": "Unity builtin shader fileID 10721",
            "render_contract": {
                "queue": {
                    "value": custom_queue,
                    "origin": "material_custom_render_queue",
                },
                "render_type": {
                    "value": None,
                    "origin": "not_serialized_by_builtin_reference",
                },
                "blend": {
                    "value": (
                        f"material _SrcBlend={floats.get('_SrcBlend')} "
                        f"_DstBlend={floats.get('_DstBlend')}"
                    ),
                    "origin": "material_saved_properties",
                },
                "zwrite": {
                    "value": floats.get("_ZWrite"),
                    "origin": "material_saved_properties",
                },
                "cull": {
                    "value": None,
                    "origin": "not_serialized_by_builtin_reference",
                },
                "vertex_color": {
                    "semantic": "not_proven_by_available_builtin_source",
                    "alpha_semantic": "not_proven_by_available_builtin_source",
                    "evidence_lines": [],
                },
            },
        }
    else:
        shader_path = project_shader_path
        shader_text = project_shader_text
        name_match = re.search(
            r'(?m)^\ufeff?\s*Shader\s+"([^"]+)"',
            shader_text,
        )
        if not name_match:
            raise RuntimeError(
                f"{rel(shader_path)} lacks a Shader name"
            )
        tags = {}
        for block in re.findall(
            r"(?is)\bTags\s*\{([^}]*)\}",
            shader_text,
        ):
            for key, value in re.findall(
                r'"([^"]+)"\s*=\s*"([^"]+)"',
                block,
            ):
                tags[key.lower()] = value

        def commands(keyword):
            return [
                match.strip()
                for match in re.findall(
                    rf"(?im)^\s*{keyword}\s+([^\r\n/]+)",
                    shader_text,
                )
            ]

        blend = commands("Blend")
        zwrite = commands("ZWrite")
        cull = commands("Cull")
        color_declarations = [
            line.strip()
            for line in shader_text.splitlines()
            if re.search(r":\s*COLOR\d*\s*;", line)
        ]
        alpha_lines = [
            line.strip()
            for line in shader_text.splitlines()
            if re.search(r"\b(?:v|i|o)\.color\.a\b", line)
            or re.search(r"\bv\.color\.a\b", line)
        ]
        shader_name = name_match.group(1)
        if (
            shader_name == "custom/landmap/tree-16-nomask"
            and color_declarations
        ):
            vertex_semantic = (
                "COLOR0.xyz encodes per-card center; shader multiplies "
                "center by 180 and applies global tree scale/rotation"
            )
            alpha_semantic = "COLOR0.a is not sampled"
        elif alpha_lines:
            vertex_semantic = "vertex COLOR is passed to fragment stage"
            alpha_semantic = "vertex COLOR.a contributes to opacity"
        elif color_declarations:
            vertex_semantic = "vertex COLOR declaration present"
            alpha_semantic = "vertex COLOR.a use not found"
        else:
            vertex_semantic = "vertex COLOR is not declared"
            alpha_semantic = "not_used"
        shader = {
            "kind": "project_shader",
            "file_id": shader_ref["file_id"],
            "guid": shader_ref["guid"],
            "source_shader": source_record(shader_path),
            "shader_name": shader_name,
            "render_contract": {
                "queue": {
                    "value": (
                        custom_queue
                        if custom_queue >= 0
                        else tags.get("queue", "Geometry")
                    ),
                    "origin": (
                        "material_custom_render_queue"
                        if custom_queue >= 0
                        else (
                            "shader_tag"
                            if "queue" in tags
                            else "shaderlab_default"
                        )
                    ),
                },
                "render_type": {
                    "value": tags.get("rendertype"),
                    "origin": (
                        "shader_tag"
                        if "rendertype" in tags
                        else "not_declared"
                    ),
                },
                "blend": {
                    "value": blend or ["Off"],
                    "origin": (
                        "shader_command"
                        if blend
                        else "shaderlab_default"
                    ),
                },
                "zwrite": {
                    "value": zwrite or ["On"],
                    "origin": (
                        "shader_command"
                        if zwrite
                        else "shaderlab_default"
                    ),
                },
                "cull": {
                    "value": cull or ["Back"],
                    "origin": (
                        "shader_command"
                        if cull
                        else "shaderlab_default"
                    ),
                },
                "vertex_color": {
                    "semantic": vertex_semantic,
                    "alpha_semantic": alpha_semantic,
                    "evidence_lines": color_declarations + alpha_lines,
                },
                "tags": tags,
            },
        }
    return {
        "material_source": source_record(path),
        "custom_render_queue": custom_queue,
        "shader": shader,
        "texture_properties": texture_properties,
        "float_properties": floats,
        "color_properties": colors,
    }


def validate_expected_counts(
    mesh_uses,
    material_uses,
    sprite_uses,
    runtime_data,
):
    mesh_paths = {key[0] for key in mesh_uses}
    material_paths = {key[0] for key in material_uses}
    sprite_paths = {key[0] for key in sprite_uses}
    unity_mesh_count = sum(
        path.lower().endswith(".asset") for path in mesh_paths
    )
    fbx_count = sum(path.lower().endswith(".fbx") for path in mesh_paths)
    actual = {
        "mesh_assets": len(mesh_paths),
        "unity_mesh_assets": unity_mesh_count,
        "fbx_mesh_assets": fbx_count,
        "materials": len(material_paths),
        "sprite_textures": len(sprite_paths),
        "particle_systems": int(
            runtime_data["statistics"]["particle_system_count"]
        ),
        "mesh_colliders": int(
            runtime_data["statistics"]["mesh_collider_count"]
        ),
        "null_sprites": int(
            runtime_data["statistics"]["empty_sprite_asset_count"]
        ),
    }
    if actual != EXPECTED_COUNTS:
        raise RuntimeError(
            "Streaming source inventory changed; fail-closed expected "
            f"{EXPECTED_COUNTS}, found {actual}"
        )
    if len(mesh_uses) != EXPECTED_COUNTS["mesh_assets"]:
        raise RuntimeError(
            "One mesh path resolves to multiple source fileIDs"
        )
    if len(material_uses) != EXPECTED_COUNTS["materials"]:
        raise RuntimeError(
            "One material path resolves to multiple source fileIDs"
        )
    return actual


def build_markdown(manifest):
    counts = manifest["statistics"]
    lod = manifest["usage_counts_by_lod"]
    lines = [
        "# Unity World Map Streaming Assets",
        "",
        "Deterministic compiler inventory for far LOD2-5 render assets.",
        "",
        "## Source coverage",
        "",
        f"- Mesh assets: {counts['mesh_asset_count']} "
        f"({counts['unity_mesh_asset_count']} Unity `.asset` OBJ exports, "
        f"{counts['fbx_mesh_asset_count']} exact-node FBX contracts)",
        f"- Materials: {counts['material_count']}",
        f"- Sprite textures: {counts['sprite_texture_count']} "
        f"({counts['sprite_subasset_count']} referenced sprite subassets)",
        f"- Cookable sprite quad meshes: "
        f"{counts['sprite_mesh_count']} "
        f"({counts['sprite_renderer_bound_count']} renderer bindings)",
        f"- Generated OBJ files: {counts['generated_obj_count']}",
        "",
        "## Per-LOD source usage",
        "",
        "| LOD | MeshFilter uses | Material slot uses | SpriteRenderer uses |",
        "| ---: | ---: | ---: | ---: |",
    ]
    for level in ("2", "3", "4", "5"):
        item = lod[level]
        lines.append(
            f"| {level} | {item['mesh_filter_uses']} | "
            f"{item['material_slot_uses']} | "
            f"{item['sprite_renderer_uses']} |"
        )
    lines.extend(
        [
            "",
            "## Explicit blocked policies",
            "",
            "- 18 ParticleSystems remain Niagara migration work; no proxy "
            "effects are generated.",
            "- 6 MeshColliders remain blocked until a cooked collision/query "
            "policy is accepted; render meshes are not reused implicitly.",
            "- 4 null SpriteRenderer assets remain source-explicit render "
            "no-ops; no substitute sprite is generated.",
            "- `sprite_renderer_static_mesh_uv_bindings_missing` is cleared: "
            "16 source-rect/pivot/PPU quad meshes bind all 2005 non-null "
            "SpriteRenderers.",
            "- Unity bottom-left atlas UVs are preserved without a guessed "
            "V flip. UE runtime visual acceptance remains pending.",
            "",
            "FBX split contracts use exact, case-sensitive node names. "
            "First-match and substring matching are forbidden.",
            "",
        ]
    )
    return "\n".join(lines)


def canonical_hash(payload):
    return sha256_bytes(
        json.dumps(
            payload,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        ).encode("utf-8")
    )


def main():
    if not RUNTIME_DATA_PATH.is_file():
        raise RuntimeError(
            "Generate unity_world_map_runtime_data.json first"
        )
    runtime_data = json.loads(
        RUNTIME_DATA_PATH.read_text(encoding="utf-8")
    )
    if runtime_data.get("schema") != "rok.unity_world_map_runtime_data.v1":
        raise RuntimeError(
            f"Unexpected runtime data schema: "
            f"{runtime_data.get('schema')}"
        )
    (
        mesh_uses,
        material_uses,
        sprite_uses,
        fbx_node_bindings,
    ) = collect_inventory(runtime_data)
    particle_records, collider_records = (
        collect_blocked_component_records(runtime_data)
    )
    actual_counts = validate_expected_counts(
        mesh_uses,
        material_uses,
        sprite_uses,
        runtime_data,
    )
    if len(particle_records) != actual_counts["particle_systems"]:
        raise RuntimeError(
            "ParticleSystem blocked-record count differs from source"
        )
    if len(collider_records) != actual_counts["mesh_colliders"]:
        raise RuntimeError(
            "MeshCollider blocked-record count differs from source"
        )
    guid_index = build_guid_index()

    MESH_EXPORT_ROOT.mkdir(parents=True, exist_ok=True)
    SPRITE_MESH_EXPORT_ROOT.mkdir(parents=True, exist_ok=True)
    mesh_records = []
    expected_obj_names = set()
    ue_names = set()
    for (path_text, file_id), uses in sorted(mesh_uses.items()):
        path = resolve(path_text)
        if not path.is_file():
            raise RuntimeError(f"Missing mesh source: {path_text}")
        extension = path.suffix.lower()
        if extension not in (".asset", ".fbx"):
            raise RuntimeError(
                f"Unsupported streaming mesh source: {path_text}"
            )
        name = ue_name("SM_RokStream", path, file_id)
        if name in ue_names:
            raise RuntimeError(f"Duplicate generated UE name: {name}")
        ue_names.add(name)
        record = {
            "identity": {
                "source_path": path_text,
                "source_file_id": file_id,
                "source_guid": uses[0]["source_guid"],
            },
            "source": source_record(path),
            "ue_name": name,
            "ue_soft_object_path": (
                f"{UE_MESH_ROOT}/{name}.{name}"
            ),
            "usage": aggregate_usage(uses),
        }
        if extension == ".asset":
            parsed = parse_mesh_asset(path)
            obj_bytes = make_obj_bytes(name, path, parsed)
            obj_path = MESH_EXPORT_ROOT / f"{name}.obj"
            obj_path.write_bytes(obj_bytes)
            expected_obj_names.add(obj_path.name)
            record.update(
                {
                    "kind": "unity_serialized_mesh",
                    "export_policy": (
                        "deterministic_exact_source_attributes_obj"
                    ),
                    "exported_obj": source_record(
                        obj_path,
                        include_meta=False,
                    ),
                    "vertex_count": len(parsed["positions"]),
                    "triangle_count": len(parsed["triangles"]),
                    "submesh_count": parsed["submesh_count"],
                    "normal_count": len(parsed["normals"]),
                    "uv0_count": len(parsed["uvs"]),
                    "vertex_color_count": len(parsed["colors"]),
                    "vertex_stride": parsed["vertex_stride"],
                    "source_channels": parsed["channels"],
                    "raw_vertex_data_sha256": parsed[
                        "raw_vertex_data_sha256"
                    ],
                    "raw_index_buffer_sha256": parsed[
                        "raw_index_buffer_sha256"
                    ],
                    "axis_contract": {
                        "obj_payload": (
                            "(Unity x,y,z) -> (x,-z,y) * 100 cm"
                        ),
                        "normal_payload": (
                            "(Unity nx,ny,nz) -> (nx,-nz,ny)"
                        ),
                        "winding": (
                            "triangle index 1/2 swapped for handedness"
                        ),
                        "vertex_color_extension": (
                            "OBJ v x y z r g b a"
                        ),
                    },
                }
            )
        else:
            record.update(
                {
                    "kind": "fbx_split_import",
                    "split_import_contract": build_fbx_contract(
                        path,
                        file_id,
                        uses,
                        fbx_node_bindings[(path_text, file_id)],
                    ),
                }
            )
        mesh_records.append(record)

    for stale_path in sorted(MESH_EXPORT_ROOT.glob("*.obj")):
        if stale_path.name not in expected_obj_names:
            stale_path.unlink()

    material_records = []
    material_names = set()
    for (path_text, file_id), uses in sorted(material_uses.items()):
        path = resolve(path_text)
        name = ue_name("MI_RokStream", path, file_id)
        if name in material_names:
            raise RuntimeError(f"Duplicate material UE name: {name}")
        material_names.add(name)
        source_guids = {item["source_guid"] for item in uses}
        if len(source_guids) != 1:
            raise RuntimeError(f"Ambiguous material GUID: {path_text}")
        material_records.append(
            {
                "identity": {
                    "source_path": path_text,
                    "source_file_id": file_id,
                    "source_guid": next(iter(source_guids)),
                },
                "ue_name": name,
                "ue_soft_object_path": (
                    f"{UE_MATERIAL_ROOT}/{name}.{name}"
                ),
                "usage": aggregate_usage(uses),
                **parse_material(path, guid_index),
            }
        )
    unresolved_stale_property_count = sum(
        property_record.get("resolution_status")
        == "unresolved_stale_property_not_declared_by_bound_shader"
        for material_record in material_records
        for property_record in material_record["texture_properties"]
    )
    material_records_by_identity = {
        (
            record["identity"]["source_path"],
            record["identity"]["source_file_id"],
            record["identity"]["source_guid"],
        ): record
        for record in material_records
    }

    sprite_path_uses = defaultdict(list)
    for (path_text, _file_id), uses in sprite_uses.items():
        sprite_path_uses[path_text].extend(uses)
    sprite_records = []
    sprite_names = set()
    sprite_mesh_names = set()
    expected_sprite_obj_names = set()
    sprite_binding_ambiguity_count = 0
    for path_text, uses in sorted(sprite_path_uses.items()):
        path = resolve(path_text)
        meta_path = Path(str(path) + ".meta")
        importer, sprite_sheet = parse_sprite_sheet(meta_path, path)
        referenced_ids = sorted(
            {item["source_asset_file_id"] for item in uses},
            key=int,
        )
        missing_ids = [
            file_id
            for file_id in referenced_ids
            if file_id not in sprite_sheet
        ]
        if missing_ids:
            raise RuntimeError(
                f"{path_text} has unresolved sprite subassets {missing_ids}"
            )
        name = ue_name("T_RokStream", path, "2800000")
        if name in sprite_names:
            raise RuntimeError(f"Duplicate sprite texture UE name: {name}")
        sprite_names.add(name)
        subasset_records = []
        for file_id in referenced_ids:
            subasset_uses = [
                item
                for item in uses
                if item["source_asset_file_id"] == file_id
            ]
            meta = sprite_sheet[file_id]
            expected_size = {
                "x": (
                    meta["rect"]["width"]
                    / importer["sprite_pixels_to_units"]
                ),
                "y": (
                    meta["rect"]["height"]
                    / importer["sprite_pixels_to_units"]
                ),
            }
            for use in subasset_uses:
                if int(use["draw_mode"]) != 0:
                    raise RuntimeError(
                        f"{path_text} sprite {file_id} uses non-simple "
                        "SpriteRenderer draw mode"
                    )
                if int(use["flip_x"]) != 0 or int(use["flip_y"]) != 0:
                    raise RuntimeError(
                        f"{path_text} sprite {file_id} uses source flip"
                    )
                if any(
                    abs(float(use["color"][channel]) - 1.0) > 1e-8
                    for channel in "rgba"
                ):
                    raise RuntimeError(
                        f"{path_text} sprite {file_id} is not white"
                    )
                if (
                    abs(float(use["size"]["x"]) - expected_size["x"])
                    > 1e-5
                    or abs(
                        float(use["size"]["y"]) - expected_size["y"]
                    )
                    > 1e-5
                ):
                    raise RuntimeError(
                        f"{path_text} sprite {file_id} renderer size "
                        "differs from rect/PPU"
                    )
            material_identities = {
                (
                    use["source_material"]["path"],
                    use["source_material"]["file_id"],
                    use["source_material"]["guid"],
                )
                for use in subasset_uses
            }
            if len(material_identities) != 1:
                sprite_binding_ambiguity_count += 1
                raise RuntimeError(
                    f"{path_text} sprite {file_id} has ambiguous "
                    f"materials: {sorted(material_identities)}"
                )
            material_identity = next(iter(material_identities))
            material_record = material_records_by_identity.get(
                material_identity
            )
            if not material_record:
                raise RuntimeError(
                    f"{path_text} sprite {file_id} material is absent "
                    "from the render material inventory"
                )
            sprite_mesh_name = ue_name(
                "SM_RokStreamSprite",
                path,
                file_id,
            )
            if sprite_mesh_name in sprite_mesh_names:
                raise RuntimeError(
                    f"Duplicate sprite mesh UE name {sprite_mesh_name}"
                )
            sprite_mesh_names.add(sprite_mesh_name)
            quad = build_sprite_quad(meta, importer)
            obj_bytes = make_sprite_obj_bytes(
                sprite_mesh_name,
                path,
                file_id,
                quad,
            )
            obj_path = (
                SPRITE_MESH_EXPORT_ROOT / f"{sprite_mesh_name}.obj"
            )
            obj_path.write_bytes(obj_bytes)
            expected_sprite_obj_names.add(obj_path.name)
            subasset_records.append(
                {
                    "source_file_id": file_id,
                    "name": meta["name"],
                    "meta": meta,
                    "texture_importer_meta": source_record(
                        meta_path,
                        include_meta=False,
                    ),
                    "source_texture_identity": {
                        "path": path_text,
                        "guid": uses[0]["source_guid"],
                    },
                    "ue_texture_soft_object_path": (
                        f"{UE_TEXTURE_ROOT}/{name}.{name}"
                    ),
                    "usage": aggregate_usage(subasset_uses),
                    "renderer_source_validation": {
                        "validated_instance_count": len(subasset_uses),
                        "draw_mode": 0,
                        "flip_x": 0,
                        "flip_y": 0,
                        "color": {
                            "r": 1.0,
                            "g": 1.0,
                            "b": 1.0,
                            "a": 1.0,
                        },
                        "size_matches_rect_divided_by_ppu": True,
                        "size_tolerance": 1e-5,
                        "unity_local_size": expected_size,
                    },
                    "quad_geometry": {
                        "source_space": (
                            "Unity Sprite local XY with pivot at origin"
                        ),
                        "unity_local_bounds": quad[
                            "unity_local_bounds"
                        ],
                        "unity_local_size": quad[
                            "unity_local_size"
                        ],
                        "source_unity_uvs": [
                            {"u": uv[0], "v": uv[1]}
                            for uv in quad["unity_source_uvs"]
                        ],
                        "axis_conversion": (
                            "Unity (x,y,z) -> OBJ (x,-z,y) * 100 cm"
                        ),
                        "axis_determinant": 1,
                        "winding": (
                            "source counter-clockwise winding preserved"
                        ),
                        "obj_normal": {
                            "x": quad["obj_normal"][0],
                            "y": quad["obj_normal"][1],
                            "z": quad["obj_normal"][2],
                        },
                        "triangle_count": 2,
                        "vertex_count": 4,
                        "uv_count": 4,
                    },
                    "exported_quad_obj": source_record(
                        obj_path,
                        include_meta=False,
                    ),
                    "ue_mesh_name": sprite_mesh_name,
                    "ue_mesh_soft_object_path": (
                        f"{UE_SPRITE_MESH_ROOT}/"
                        f"{sprite_mesh_name}.{sprite_mesh_name}"
                    ),
                    "source_material_identity": {
                        "path": material_identity[0],
                        "file_id": material_identity[1],
                        "guid": material_identity[2],
                    },
                    "ue_material_soft_object_path": material_record[
                        "ue_soft_object_path"
                    ],
                    "binding_status": "ready",
                    "visual_acceptance": {
                        "status": "pending",
                        "reason": (
                            "Unity bottom-left source UV is preserved "
                            "exactly; UE Interchange OBJ V presentation "
                            "direction requires runtime visual acceptance"
                        ),
                        "uv_v_flip_applied": False,
                    },
                }
            )
        sprite_records.append(
            {
                "identity": {
                    "source_path": path_text,
                    "source_guid": uses[0]["source_guid"],
                },
                "source": source_record(path),
                "ue_name": name,
                "ue_soft_object_path": (
                    f"{UE_TEXTURE_ROOT}/{name}.{name}"
                ),
                "usage": aggregate_usage(uses),
                "texture_importer": importer,
                "referenced_sprite_subassets": subasset_records,
            }
        )
    for stale_path in sorted(SPRITE_MESH_EXPORT_ROOT.glob("*.obj")):
        if stale_path.name not in expected_sprite_obj_names:
            stale_path.unlink()

    usage_counts_by_lod = {}
    all_mesh_uses = [
        item for values in mesh_uses.values() for item in values
    ]
    all_material_uses = [
        item for values in material_uses.values() for item in values
    ]
    all_sprite_uses = [
        item for values in sprite_uses.values() for item in values
    ]
    for lod in (2, 3, 4, 5):
        usage_counts_by_lod[str(lod)] = {
            "mesh_filter_uses": sum(
                item["lod"] == lod for item in all_mesh_uses
            ),
            "material_slot_uses": sum(
                item["lod"] == lod for item in all_material_uses
            ),
            "sprite_renderer_uses": sum(
                item["lod"] == lod for item in all_sprite_uses
            ),
        }

    blocked_policies = {
        "particle_systems": {
            "count": actual_counts["particle_systems"],
            "status": "blocked",
            "policy": (
                "never_generate_proxy; migrate exact source effect to "
                "Niagara or accept omission per recipe"
            ),
            "records": particle_records,
        },
        "mesh_colliders": {
            "count": actual_counts["mesh_colliders"],
            "status": "blocked",
            "policy": (
                "never_reuse_render_mesh_implicitly; define cooked "
                "collision/query contract first"
            ),
            "records": collider_records,
        },
        "null_sprites": {
            "count": actual_counts["null_sprites"],
            "status": "source_explicit_render_noop",
            "policy": "never_substitute_or_fake_a_sprite",
            "records": runtime_data["pending_policies"][
                "empty_sprite_assets"
            ]["records"],
        },
    }
    payload = {
        "schema": "rok.unity_world_map_streaming_assets.v1",
        "source_runtime_data": source_record(
            RUNTIME_DATA_PATH,
            include_meta=False,
        ),
        "lod_scope": [2, 3, 4, 5],
        "mesh_exports_root": rel(MESH_EXPORT_ROOT),
        "sprite_mesh_exports_root": rel(SPRITE_MESH_EXPORT_ROOT),
        "statistics": {
            "mesh_asset_count": len(mesh_records),
            "unity_mesh_asset_count": sum(
                item["kind"] == "unity_serialized_mesh"
                for item in mesh_records
            ),
            "fbx_mesh_asset_count": sum(
                item["kind"] == "fbx_split_import"
                for item in mesh_records
            ),
            "material_count": len(material_records),
            "sprite_texture_count": len(sprite_records),
            "sprite_subasset_count": sum(
                len(item["referenced_sprite_subassets"])
                for item in sprite_records
            ),
            "sprite_mesh_count": len(expected_sprite_obj_names),
            "sprite_renderer_bound_count": len(all_sprite_uses),
            "sprite_renderer_noop_count": actual_counts[
                "null_sprites"
            ],
            "sprite_binding_ambiguity_count": (
                sprite_binding_ambiguity_count
            ),
            "generated_obj_count": len(expected_obj_names),
            "generated_sprite_obj_count": len(
                expected_sprite_obj_names
            ),
            "unresolved_stale_unbound_texture_property_count": (
                unresolved_stale_property_count
            ),
            "missing_render_bound_source_count": 0,
            "particle_system_count": actual_counts[
                "particle_systems"
            ],
            "mesh_collider_count": actual_counts["mesh_colliders"],
            "null_sprite_count": actual_counts["null_sprites"],
        },
        "usage_counts_by_lod": usage_counts_by_lod,
        "meshes": mesh_records,
        "materials": material_records,
        "sprite_textures": sprite_records,
        "blocked_policies": blocked_policies,
        "cleared_blockers": [
            "sprite_renderer_static_mesh_uv_bindings_missing"
        ],
        "visual_acceptance_pending": [
            {
                "scope": "sprite_quad_obj_uv_v_direction",
                "reason": (
                    "Source Unity normalized bottom-left UVs are exact; "
                    "UE Interchange OBJ V presentation direction has not "
                    "been accepted visually"
                ),
                "source_uv_preserved": True,
                "uv_v_flip_applied": False,
            }
        ],
        "acceptance": {
            "asset_manifest_complete": True,
            "mesh_exports_exact_source_data": True,
            "fbx_import_contracts_exact_node_only": True,
            "particle_system_runtime_ready": False,
            "collision_runtime_ready": False,
            "null_sprite_policy_preserved": True,
            "sprite_static_mesh_uv_bindings_ready": True,
            "sprite_runtime_visual_acceptance_complete": False,
            "ue_assets_imported": False,
            "ue_runtime_streaming_ready": False,
        },
        "forbidden_policies": [
            "first_mesh_match",
            "substring_node_match",
            "contains_node_match",
            "proxy_mesh",
            "proxy_particle",
            "implicit_render_mesh_collision",
            "null_sprite_substitution",
            "cropped_sprite_texture_proxy",
            "guessed_sprite_uv_v_flip",
        ],
    }
    manifest = {
        "canonical_payload_sha256": canonical_hash(payload),
        **payload,
    }
    OUTPUT_PATH.write_text(
        json.dumps(
            manifest,
            indent=2,
            ensure_ascii=False,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    SUMMARY_PATH.write_text(
        build_markdown(manifest),
        encoding="utf-8",
    )
    print(
        "Rok Unity world-map streaming assets prepared: "
        f"meshes={len(mesh_records)} "
        f"obj={len(expected_obj_names)} "
        f"fbx_contracts="
        f"{sum(item['kind'] == 'fbx_split_import' for item in mesh_records)} "
        f"materials={len(material_records)} "
        f"sprite_textures={len(sprite_records)}"
    )


if __name__ == "__main__":
    main()
