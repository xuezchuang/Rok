import hashlib
import json
import re
import struct
from collections import defaultdict
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
RUNTIME_PATH = MIGRATION_ROOT / "unity_world_map_runtime_data.json"
MANIFEST_PATH = MIGRATION_ROOT / "unity_world_map_streaming_assets.json"
VERIFY_PATH = (
    MIGRATION_ROOT / "unity_world_map_streaming_assets_verify.json"
)
EXPECTED = {
    "mesh_asset_count": 232,
    "unity_mesh_asset_count": 218,
    "fbx_mesh_asset_count": 14,
    "material_count": 35,
    "sprite_texture_count": 2,
    "particle_system_count": 18,
    "mesh_collider_count": 6,
    "null_sprite_count": 4,
}
DECIMAL_ID = re.compile(r"^-?\d+$")
SOFT_PATH = re.compile(
    r"^/Game/RokUnityWorldMapStreaming/"
    r"(?:Meshes|SpriteMeshes|Materials|Textures)/"
    r"([A-Za-z0-9_]+)\.\1$"
)


def rel(path):
    return str(path.relative_to(PROJECT_ROOT)).replace("\\", "/")


def sha256_bytes(payload):
    return hashlib.sha256(payload).hexdigest()


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def canonical_hash(payload):
    return sha256_bytes(
        json.dumps(
            payload,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        ).encode("utf-8")
    )


def iter_nodes(node):
    yield node
    for child in node.get("children", []):
        yield from iter_nodes(child)


def decimal(value, label):
    value = str(value)
    if not DECIMAL_ID.fullmatch(value):
        raise RuntimeError(f"{label} is not a decimal string: {value!r}")
    return value


def record_use(store, key, lod, component_id, source_id, **extra):
    store[key].append(
        {
            "lod": int(lod),
            "component_file_id": decimal(
                component_id,
                "component fileID",
            ),
            "source_asset_file_id": decimal(
                source_id,
                "source asset fileID",
            ),
            **extra,
        }
    )


def collect_runtime_inventory(runtime):
    mesh = defaultdict(list)
    materials = defaultdict(list)
    sprites = defaultdict(list)
    fbx_bindings = defaultdict(list)
    for recipe in runtime["far_lod_recipes"]:
        lod = int(recipe["lod"])
        if lod not in (2, 3, 4, 5):
            raise RuntimeError(f"Unexpected far LOD {lod}")
        for node in iter_nodes(recipe["root"]):
            by_type = defaultdict(list)
            for component in node.get("components", []):
                decimal(component["file_id"], "runtime component fileID")
                by_type[component["type"]].append(component)
            for mesh_filter in by_type["MeshFilter"]:
                source = mesh_filter["mesh"]
                if source["is_null"]:
                    raise RuntimeError(
                        f"Null MeshFilter at {node['node_path']}"
                    )
                key = (
                    source["asset_path"],
                    decimal(source["file_id"], "mesh fileID"),
                )
                record_use(
                    mesh,
                    key,
                    lod,
                    mesh_filter["file_id"],
                    source["file_id"],
                    guid=source["guid"].lower(),
                    node_name=node["name"],
                    node_path=node["node_path"],
                )
                if source["asset_path"].lower().endswith(".fbx"):
                    renderers = by_type["MeshRenderer"]
                    fbx_bindings[key].append(
                        {
                            "node_name": node["name"],
                            "mesh_filter_count": len(
                                by_type["MeshFilter"]
                            ),
                            "mesh_renderer_count": len(renderers),
                            "enabled_renderer_count": sum(
                                int(item["enabled"]) == 1
                                for item in renderers
                            ),
                            "material_slots": [
                                (
                                    ref["asset_path"],
                                    decimal(
                                        ref["file_id"],
                                        "FBX material fileID",
                                    ),
                                    ref["guid"].lower(),
                                )
                                for renderer in renderers
                                if int(renderer["enabled"]) == 1
                                for ref in renderer["materials"]
                                if not ref["is_null"]
                            ],
                        }
                    )
            for renderer in (
                by_type["MeshRenderer"] + by_type["SpriteRenderer"]
            ):
                for ref in renderer["materials"]:
                    if ref["is_null"]:
                        raise RuntimeError(
                            f"Null renderer material at "
                            f"{node['node_path']}"
                        )
                    key = (
                        ref["asset_path"],
                        decimal(ref["file_id"], "material fileID"),
                    )
                    record_use(
                        materials,
                        key,
                        lod,
                        renderer["file_id"],
                        ref["file_id"],
                        guid=ref["guid"].lower(),
                    )
            for renderer in by_type["SpriteRenderer"]:
                ref = renderer["sprite"]
                if ref["is_null"]:
                    continue
                material_refs = [
                    item
                    for item in renderer["materials"]
                    if not item["is_null"]
                ]
                if len(material_refs) != 1:
                    raise RuntimeError(
                        f"SpriteRenderer at {node['node_path']} has "
                        f"{len(material_refs)} source materials"
                    )
                key = (
                    ref["asset_path"],
                    decimal(ref["file_id"], "sprite fileID"),
                )
                record_use(
                    sprites,
                    key,
                    lod,
                    renderer["file_id"],
                    ref["file_id"],
                    guid=ref["guid"].lower(),
                    draw_mode=int(renderer["draw_mode"]),
                    flip_x=int(renderer["flip_x"]),
                    flip_y=int(renderer["flip_y"]),
                    color={
                        channel: float(renderer["color"][channel])
                        for channel in "rgba"
                    },
                    size={
                        "x": float(renderer["size"]["x"]),
                        "y": float(renderer["size"]["y"]),
                    },
                    source_material=(
                        material_refs[0]["asset_path"],
                        decimal(
                            material_refs[0]["file_id"],
                            "sprite material fileID",
                        ),
                        material_refs[0]["guid"].lower(),
                    ),
                )
    return mesh, materials, sprites, fbx_bindings


def collect_blocked_runtime_records(runtime):
    particles = {}
    colliders = {}
    for recipe in runtime["far_lod_recipes"]:
        lod = int(recipe["lod"])
        for node in iter_nodes(recipe["root"]):
            by_type = defaultdict(list)
            for component in node.get("components", []):
                by_type[component["type"]].append(component)
            systems = by_type["ParticleSystem"]
            renderers = by_type["ParticleSystemRenderer"]
            if systems or renderers:
                if len(systems) != 1 or len(renderers) != 1:
                    raise RuntimeError(
                        f"Ambiguous particle pairing: {node['node_path']}"
                    )
                system = systems[0]
                renderer = renderers[0]
                system_id = decimal(
                    system["file_id"],
                    "ParticleSystem fileID",
                )
                particles[system_id] = {
                    "lod": lod,
                    "recipe_id": recipe["recipe_id"],
                    "node_path": node["node_path"],
                    "game_object_file_id": decimal(
                        node["game_object_file_id"],
                        "particle GameObject fileID",
                    ),
                    "particle_system_file_id": system_id,
                    "particle_renderer_file_id": decimal(
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
                    "material_slots": [
                        (
                            slot,
                            ref["asset_path"],
                            decimal(
                                ref["file_id"],
                                "particle material fileID",
                            ),
                            ref["guid"].lower(),
                        )
                        for slot, ref in enumerate(renderer["materials"])
                        if not ref["is_null"]
                    ],
                }
            for collider in by_type["MeshCollider"]:
                collider_id = decimal(
                    collider["file_id"],
                    "MeshCollider fileID",
                )
                source = collider["mesh"]
                colliders[collider_id] = {
                    "lod": lod,
                    "recipe_id": recipe["recipe_id"],
                    "node_path": node["node_path"],
                    "game_object_file_id": decimal(
                        node["game_object_file_id"],
                        "collider GameObject fileID",
                    ),
                    "mesh_collider_file_id": collider_id,
                    "source_path": source["asset_path"],
                    "source_file_id": decimal(
                        source["file_id"],
                        "collider mesh fileID",
                    ),
                    "source_guid": source["guid"].lower(),
                    "enabled": int(collider["enabled"]),
                    "is_trigger": int(collider["is_trigger"]),
                    "convex": int(collider["convex"]),
                    "cooking_options": int(
                        collider["cooking_options"]
                    ),
                    "serialized_body_sha256": collider[
                        "serialized_body_sha256"
                    ],
                }
    return particles, colliders


def collect_null_sprite_records(runtime):
    records = []
    for recipe in runtime["far_lod_recipes"]:
        for node in iter_nodes(recipe["root"]):
            for component in node.get("components", []):
                if component["type"] != "SpriteRenderer":
                    continue
                if not component["sprite"]["is_null"]:
                    continue
                records.append(
                    {
                        "lod": int(recipe["lod"]),
                        "node_name": node["name"],
                        "node_path": node["node_path"],
                        "policy": "explicit_source_no_asset_render_noop",
                        "sprite_renderer_file_id": decimal(
                            component["file_id"],
                            "null SpriteRenderer fileID",
                        ),
                        "tile_type_id": recipe["tile_type_id"],
                    }
                )
    return sorted(
        records,
        key=lambda item: (
            item["lod"],
            int(item["sprite_renderer_file_id"]),
        ),
    )


def expected_usage(uses):
    result = {"total_count": len(uses), "by_lod": {}}
    for lod in (2, 3, 4, 5):
        selected = [item for item in uses if item["lod"] == lod]
        result["by_lod"][str(lod)] = {
            "count": len(selected),
            "component_file_ids": sorted(
                {item["component_file_id"] for item in selected},
                key=int,
            ),
            "source_asset_file_ids": sorted(
                {
                    item["source_asset_file_id"]
                    for item in selected
                },
                key=int,
            ),
        }
    return result


def verify_usage(actual, uses, label):
    expected = expected_usage(uses)
    if actual["total_count"] != expected["total_count"]:
        raise RuntimeError(f"{label} total usage count changed")
    for lod in ("2", "3", "4", "5"):
        for field in (
            "count",
            "component_file_ids",
            "source_asset_file_ids",
        ):
            if actual["by_lod"][lod][field] != expected["by_lod"][lod][field]:
                raise RuntimeError(
                    f"{label} LOD{lod} {field} differs from runtime"
                )
        for file_id in actual["by_lod"][lod]["component_file_ids"]:
            decimal(file_id, f"{label} component fileID")
        for file_id in actual["by_lod"][lod]["source_asset_file_ids"]:
            decimal(file_id, f"{label} source fileID")


def verify_source_record(record):
    path = PROJECT_ROOT / record["path"]
    if not path.is_file():
        raise RuntimeError(f"Missing recorded source: {record['path']}")
    if path.stat().st_size != int(record["size"]):
        raise RuntimeError(f"Source size changed: {record['path']}")
    if sha256(path) != record["sha256"]:
        raise RuntimeError(f"Source hash changed: {record['path']}")
    meta = record.get("meta")
    if meta:
        meta_path = PROJECT_ROOT / meta["path"]
        if not meta_path.is_file():
            raise RuntimeError(f"Missing source meta: {meta['path']}")
        if meta_path.stat().st_size != int(meta["size"]):
            raise RuntimeError(f"Meta size changed: {meta['path']}")
        if sha256(meta_path) != meta["sha256"]:
            raise RuntimeError(f"Meta hash changed: {meta['path']}")


def mesh_source_facts(path):
    text = path.read_text(encoding="utf-8", errors="strict")
    vertex_match = re.search(
        r"(?m)^\s*m_VertexCount:\s*(\d+)\s*$",
        text,
    )
    index_match = re.search(
        r"(?m)^\s*m_IndexBuffer:\s*([0-9a-fA-F]+)\s*$",
        text,
    )
    vertex_data_match = re.search(
        r"(?m)^\s*_typelessdata:\s*([0-9a-fA-F]+)\s*$",
        text,
    )
    if not all((vertex_match, index_match, vertex_data_match)):
        raise RuntimeError(f"Incomplete Unity mesh source: {rel(path)}")
    channel_start = text.find("    m_Channels:")
    channel_end = text.find("    m_DataSize:", channel_start)
    rows = re.findall(
        r"- stream:\s*(\d+)\s*\n"
        r"\s*offset:\s*(\d+)\s*\n"
        r"\s*format:\s*(\d+)\s*\n"
        r"\s*dimension:\s*(\d+)",
        text[channel_start:channel_end],
    )
    if len(rows) != 14:
        raise RuntimeError(f"Unexpected channel table: {rel(path)}")
    normal_count = (
        int(vertex_match.group(1)) if int(rows[1][3]) else 0
    )
    color_count = (
        int(vertex_match.group(1)) if int(rows[3][3]) else 0
    )
    submesh_start = text.find("  m_SubMeshes:")
    submesh_end = text.find("  m_Shapes:", submesh_start)
    blocks = re.split(
        r"\n\s*-\s+serializedVersion:\s*\d+",
        text[submesh_start:submesh_end],
    )[1:]
    triangle_count = 0
    for block in blocks:
        count_match = re.search(
            r"(?m)^\s*indexCount:\s*(\d+)\s*$",
            block,
        )
        topology_match = re.search(
            r"(?m)^\s*topology:\s*(\d+)\s*$",
            block,
        )
        if (
            not count_match
            or not topology_match
            or int(topology_match.group(1)) != 0
            or int(count_match.group(1)) % 3
        ):
            raise RuntimeError(f"Unsupported submesh: {rel(path)}")
        triangle_count += int(count_match.group(1)) // 3
    return {
        "vertex_count": int(vertex_match.group(1)),
        "normal_count": normal_count,
        "color_count": color_count,
        "triangle_count": triangle_count,
        "submesh_count": len(blocks),
        "raw_vertex_data_sha256": sha256_bytes(
            bytes.fromhex(vertex_data_match.group(1))
        ),
        "raw_index_buffer_sha256": sha256_bytes(
            bytes.fromhex(index_match.group(1))
        ),
    }


def verify_obj(record):
    obj_record = record["exported_obj"]
    verify_source_record(obj_record)
    obj_path = PROJECT_ROOT / obj_record["path"]
    if obj_path.parent != (
        MIGRATION_ROOT / "WorldMapStreamingAssets" / "Meshes"
    ):
        raise RuntimeError("OBJ escaped dedicated generated mesh folder")
    if obj_path.name != f"{record['ue_name']}.obj":
        raise RuntimeError("OBJ filename differs from unique UE name")
    lines = obj_path.read_text(encoding="utf-8").splitlines()
    vertices = [line for line in lines if line.startswith("v ")]
    uvs = [line for line in lines if line.startswith("vt ")]
    normals = [line for line in lines if line.startswith("vn ")]
    faces = [line for line in lines if line.startswith("f ")]
    groups = [line for line in lines if line.startswith("g submesh_")]
    expected_vertex_fields = (
        8 if record["vertex_color_count"] else 4
    )
    if any(len(line.split()) != expected_vertex_fields for line in vertices):
        raise RuntimeError(
            f"OBJ vertex attribute loss: {obj_record['path']}"
        )
    actual = {
        "vertex_count": len(vertices),
        "normal_count": len(normals),
        "uv0_count": len(uvs),
        "triangle_count": len(faces),
        "submesh_count": len(groups),
    }
    for field, value in actual.items():
        if int(record[field]) != value:
            raise RuntimeError(
                f"OBJ {field} differs: {obj_record['path']}"
            )
    for line in faces:
        fields = line.split()[1:]
        if len(fields) != 3:
            raise RuntimeError(f"Non-triangle OBJ face: {obj_record['path']}")
        separator_count = 2 if normals else 1
        if any(item.count("/") != separator_count for item in fields):
            raise RuntimeError(
                f"OBJ index tuple lost attributes: {obj_record['path']}"
            )


def parse_meta_guid(meta_path):
    text = meta_path.read_text(encoding="utf-8", errors="strict")
    match = re.search(
        r"(?m)^guid:\s*([0-9a-fA-F]{32})\s*$",
        text,
    )
    if not match:
        raise RuntimeError(f"Missing GUID: {rel(meta_path)}")
    return match.group(1).lower()


def png_dimensions(path):
    header = path.read_bytes()[:24]
    if (
        len(header) != 24
        or header[:8] != b"\x89PNG\r\n\x1a\n"
        or header[12:16] != b"IHDR"
    ):
        raise RuntimeError(f"Invalid PNG source: {rel(path)}")
    width, height = struct.unpack(">II", header[16:24])
    return {"width": width, "height": height}


def inline_xy(payload, name):
    match = re.search(
        rf"(?m)^\s*{re.escape(name)}:\s*"
        r"\{x:\s*([-+0-9.eE]+),\s*y:\s*([-+0-9.eE]+)\}\s*$",
        payload,
    )
    if not match:
        raise RuntimeError(f"Missing sprite {name}")
    return {"x": float(match.group(1)), "y": float(match.group(2))}


def inline_xyzw(payload, name):
    match = re.search(
        rf"(?m)^\s*{re.escape(name)}:\s*"
        r"\{x:\s*([-+0-9.eE]+),\s*y:\s*([-+0-9.eE]+),\s*"
        r"z:\s*([-+0-9.eE]+),\s*w:\s*([-+0-9.eE]+)\}\s*$",
        payload,
    )
    if not match:
        raise RuntimeError(f"Missing sprite {name}")
    return {
        channel: float(value)
        for channel, value in zip("xyzw", match.groups())
    }


def parse_texture_importer(meta_path, texture_path):
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
        "default_pivot": inline_xy(text, "spritePivot"),
        "default_border": inline_xyzw(text, "spriteBorder"),
        "max_texture_size": scalar("maxTextureSize", int),
        "source_texture_dimensions": png_dimensions(texture_path),
        "texture_dimensions_source": (
            "PNG_IHDR; TextureImporter meta does not serialize source "
            "pixel width/height"
        ),
    }
    start = text.find("  spriteSheet:")
    blocks = re.split(
        r"(?m)^    - serializedVersion:\s*\d+\s*$",
        text[start:],
    )[1:]
    records = {}
    for block in blocks:
        name_match = re.search(
            r"(?m)^\s*name:\s*(.*?)\s*$",
            block,
        )
        id_match = re.search(
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
        if not all((name_match, id_match, rect_match, alignment_match)):
            raise RuntimeError(
                f"Incomplete sprite metadata: {rel(meta_path)}"
            )
        file_id = decimal(id_match.group(1), "sprite internalID")
        records[file_id] = {
            "name": name_match.group(1).strip(),
            "rect": {
                key: float(value)
                for key, value in zip(
                    ("x", "y", "width", "height"),
                    rect_match.groups(),
                )
            },
            "pivot": inline_xy(block, "pivot"),
            "alignment": int(alignment_match.group(1)),
            "border": inline_xyzw(block, "border"),
            "sprite_mesh_type": importer["sprite_mesh_type"],
        }
    return importer, records


def expected_sprite_quad(meta, importer):
    rect = meta["rect"]
    pivot = meta["pivot"]
    ppu = importer["sprite_pixels_to_units"]
    width = rect["width"] / ppu
    height = rect["height"] / ppu
    left = -pivot["x"] * width
    right = (1.0 - pivot["x"]) * width
    bottom = -pivot["y"] * height
    top = (1.0 - pivot["y"]) * height
    dimensions = importer["source_texture_dimensions"]
    u0 = rect["x"] / dimensions["width"]
    v0 = rect["y"] / dimensions["height"]
    u1 = (rect["x"] + rect["width"]) / dimensions["width"]
    v1 = (rect["y"] + rect["height"]) / dimensions["height"]
    return {
        "positions": [
            (left * 100.0, 0.0, bottom * 100.0),
            (right * 100.0, 0.0, bottom * 100.0),
            (right * 100.0, 0.0, top * 100.0),
            (left * 100.0, 0.0, top * 100.0),
        ],
        "uvs": [(u0, v0), (u1, v0), (u1, v1), (u0, v1)],
        "normal": (0.0, -1.0, 0.0),
        "faces": [
            ("1/1/1", "2/2/1", "3/3/1"),
            ("1/1/1", "3/3/1", "4/4/1"),
        ],
        "size": {"x": width, "y": height},
        "bounds": {
            "left": left,
            "right": right,
            "bottom": bottom,
            "top": top,
        },
    }


def close_tuple(actual, expected, tolerance=1e-6):
    return len(actual) == len(expected) and all(
        abs(left - right) <= tolerance
        for left, right in zip(actual, expected)
    )


def verify_sprite_obj(record, expected):
    obj_record = record["exported_quad_obj"]
    verify_source_record(obj_record)
    path = PROJECT_ROOT / obj_record["path"]
    expected_root = (
        MIGRATION_ROOT / "WorldMapStreamingAssets" / "SpriteMeshes"
    )
    if path.parent != expected_root:
        raise RuntimeError("Sprite OBJ escaped dedicated export folder")
    if path.name != f"{record['ue_mesh_name']}.obj":
        raise RuntimeError("Sprite OBJ filename differs from UE name")
    lines = path.read_text(encoding="utf-8").splitlines()
    vertices = [
        tuple(float(value) for value in line.split()[1:])
        for line in lines
        if line.startswith("v ")
    ]
    uvs = [
        tuple(float(value) for value in line.split()[1:])
        for line in lines
        if line.startswith("vt ")
    ]
    normals = [
        tuple(float(value) for value in line.split()[1:])
        for line in lines
        if line.startswith("vn ")
    ]
    faces = [
        tuple(line.split()[1:])
        for line in lines
        if line.startswith("f ")
    ]
    if len(vertices) != 4 or any(
        not close_tuple(actual, wanted, tolerance=1e-5)
        for actual, wanted in zip(vertices, expected["positions"])
    ):
        raise RuntimeError(f"Sprite OBJ positions differ: {rel(path)}")
    if len(uvs) != 4 or any(
        not close_tuple(actual, wanted, tolerance=1e-8)
        for actual, wanted in zip(uvs, expected["uvs"])
    ):
        raise RuntimeError(f"Sprite OBJ source UVs differ: {rel(path)}")
    if normals != [expected["normal"]]:
        raise RuntimeError(f"Sprite OBJ normal differs: {rel(path)}")
    if faces != expected["faces"]:
        raise RuntimeError(f"Sprite OBJ winding differs: {rel(path)}")
    edge_a = tuple(
        vertices[1][index] - vertices[0][index]
        for index in range(3)
    )
    edge_b = tuple(
        vertices[2][index] - vertices[0][index]
        for index in range(3)
    )
    cross = (
        edge_a[1] * edge_b[2] - edge_a[2] * edge_b[1],
        edge_a[2] * edge_b[0] - edge_a[0] * edge_b[2],
        edge_a[0] * edge_b[1] - edge_a[1] * edge_b[0],
    )
    if cross[1] >= 0:
        raise RuntimeError(
            f"Sprite OBJ winding does not produce -Y normal: {rel(path)}"
        )


def parse_material_refs(path):
    text = path.read_text(encoding="utf-8", errors="strict")
    shader_match = re.search(r"m_Shader:\s*\{([^}]*)\}", text)
    if not shader_match:
        raise RuntimeError(f"Missing material shader: {rel(path)}")

    def fields(payload):
        result = {
            key: value.strip()
            for key, value in re.findall(
                r"(\w+):\s*([^,}]+)",
                payload,
            )
        }
        return {
            "file_id": decimal(result.get("fileID", "0"), "material ref"),
            "guid": result.get("guid", "").lower(),
            "type": int(result["type"]) if "type" in result else None,
        }

    tex_start = text.find("    m_TexEnvs:")
    float_start = text.find("    m_Floats:", tex_start)
    textures = {}
    for name, payload in re.findall(
        r"(?ms)^\s*-\s*([^:\n]+):\s*\n"
        r"\s*m_Texture:\s*\{([^}]*)\}",
        text[tex_start:float_start],
    ):
        textures[name.strip()] = fields(payload)
    return fields(shader_match.group(1)), textures


def main():
    checks = []
    failures = []
    try:
        runtime = json.loads(RUNTIME_PATH.read_text(encoding="utf-8"))
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        if runtime.get("schema") != "rok.unity_world_map_runtime_data.v1":
            raise RuntimeError("Unexpected runtime schema")
        if manifest.get("schema") != (
            "rok.unity_world_map_streaming_assets.v1"
        ):
            raise RuntimeError("Unexpected streaming asset schema")
        payload = dict(manifest)
        recorded_canonical = payload.pop("canonical_payload_sha256")
        if canonical_hash(payload) != recorded_canonical:
            raise RuntimeError("Manifest canonical payload hash differs")
        checks.append("canonical_payload_hash")
        verify_source_record(manifest["source_runtime_data"])
        if manifest["source_runtime_data"]["path"] != rel(RUNTIME_PATH):
            raise RuntimeError("Manifest points to a different runtime source")
        checks.append("runtime_source_hash")

        mesh, materials, sprites, fbx_bindings = (
            collect_runtime_inventory(runtime)
        )
        mesh_paths = {key[0] for key in mesh}
        material_paths = {key[0] for key in materials}
        sprite_paths = {key[0] for key in sprites}
        recomputed_counts = {
            "mesh_asset_count": len(mesh_paths),
            "unity_mesh_asset_count": sum(
                path.lower().endswith(".asset") for path in mesh_paths
            ),
            "fbx_mesh_asset_count": sum(
                path.lower().endswith(".fbx") for path in mesh_paths
            ),
            "material_count": len(material_paths),
            "sprite_texture_count": len(sprite_paths),
            "particle_system_count": int(
                runtime["statistics"]["particle_system_count"]
            ),
            "mesh_collider_count": int(
                runtime["statistics"]["mesh_collider_count"]
            ),
            "null_sprite_count": int(
                runtime["statistics"]["empty_sprite_asset_count"]
            ),
        }
        for field, expected in EXPECTED.items():
            if recomputed_counts[field] != expected:
                raise RuntimeError(
                    f"Runtime {field} changed: "
                    f"{recomputed_counts[field]} != {expected}"
                )
            if int(manifest["statistics"][field]) != expected:
                raise RuntimeError(f"Manifest {field} changed")
        checks.append("independent_unique_source_counts")

        manifest_mesh = {
            (
                item["identity"]["source_path"],
                decimal(
                    item["identity"]["source_file_id"],
                    "manifest mesh fileID",
                ),
            ): item
            for item in manifest["meshes"]
        }
        if set(manifest_mesh) != set(mesh):
            raise RuntimeError("Manifest mesh identities differ from runtime")
        soft_paths = set()
        generated_objs = set()
        for key, item in manifest_mesh.items():
            verify_source_record(item["source"])
            verify_usage(item["usage"], mesh[key], f"mesh {key}")
            if item["identity"]["source_guid"] != mesh[key][0]["guid"]:
                raise RuntimeError(f"Mesh GUID differs: {key}")
            soft_path = item["ue_soft_object_path"]
            if not SOFT_PATH.fullmatch(soft_path):
                raise RuntimeError(f"Invalid mesh soft path: {soft_path}")
            if soft_path in soft_paths:
                raise RuntimeError(f"Duplicate UE soft path: {soft_path}")
            soft_paths.add(soft_path)
            source_path = PROJECT_ROOT / key[0]
            if item["kind"] == "unity_serialized_mesh":
                if item["export_policy"] != (
                    "deterministic_exact_source_attributes_obj"
                ):
                    raise RuntimeError(f"Mesh proxy policy found: {key}")
                source_facts = mesh_source_facts(source_path)
                for field, value in source_facts.items():
                    manifest_field = (
                        "vertex_color_count"
                        if field == "color_count"
                        else field
                    )
                    if item[manifest_field] != value:
                        raise RuntimeError(
                            f"Mesh source {field} differs: {key}"
                        )
                verify_obj(item)
                generated_objs.add(item["exported_obj"]["path"])
            elif item["kind"] == "fbx_split_import":
                contract = item["split_import_contract"]
                if contract["status"] != "ready_exact_split_import":
                    raise RuntimeError(f"FBX is not exact-node ready: {key}")
                if contract["mapping_source"] != (
                    "prefab_meshfilter_exact_node_plus_unique_fbx_node"
                ):
                    raise RuntimeError(f"FBX mapping broadened: {key}")
                selection = contract["node_selection"]
                if (
                    selection["mode"] != "exact_name"
                    or not selection["case_sensitive"]
                    or not selection[
                        "first_or_contains_matching_forbidden"
                    ]
                ):
                    raise RuntimeError(f"FBX selector is ambiguous: {key}")
                bindings = fbx_bindings[key]
                node_names = {value["node_name"] for value in bindings}
                if node_names != {contract["exact_node_name"]}:
                    raise RuntimeError(f"FBX node differs: {key}")
                if any(
                    value["mesh_filter_count"] != 1
                    or value["mesh_renderer_count"] != 1
                    or value["enabled_renderer_count"] != 1
                    for value in bindings
                ):
                    raise RuntimeError(
                        f"FBX source node binding is ambiguous: {key}"
                    )
                slot_sets = {
                    tuple(value["material_slots"])
                    for value in bindings
                }
                expected_slots = [
                    (
                        slot["source_material_path"],
                        decimal(
                            slot["source_material_file_id"],
                            "FBX slot material fileID",
                        ),
                        slot["source_material_guid"],
                    )
                    for slot in contract["submesh_slots"]
                ]
                if slot_sets != {tuple(expected_slots)}:
                    raise RuntimeError(f"FBX submesh slots differ: {key}")
                if len(expected_slots) != 1:
                    raise RuntimeError(f"FBX submesh is ambiguous: {key}")
                node_bytes = contract["exact_node_name"].encode("utf-8")
                occurrence_count = source_path.read_bytes().count(node_bytes)
                if occurrence_count != 1 or occurrence_count != int(
                    contract["exact_node_utf8_occurrence_count"]
                ):
                    raise RuntimeError(
                        f"FBX node is not byte-unique: {key}"
                    )
                meta_path = Path(str(source_path) + ".meta")
                if parse_meta_guid(meta_path) != contract[
                    "source_fbx_guid"
                ]:
                    raise RuntimeError(f"FBX meta GUID differs: {key}")
            else:
                raise RuntimeError(f"Unknown mesh kind: {item['kind']}")
        expected_obj_files = {
            rel(path)
            for path in (
                MIGRATION_ROOT
                / "WorldMapStreamingAssets"
                / "Meshes"
            ).glob("*.obj")
        }
        if expected_obj_files != generated_objs:
            raise RuntimeError("Generated OBJ folder contains stale/missing files")
        checks.append("mesh_hash_attribute_and_exact_fbx_contract_audit")

        manifest_materials = {
            (
                item["identity"]["source_path"],
                decimal(
                    item["identity"]["source_file_id"],
                    "manifest material fileID",
                ),
            ): item
            for item in manifest["materials"]
        }
        if set(manifest_materials) != set(materials):
            raise RuntimeError(
                "Manifest material identities differ from runtime"
            )
        for key, item in manifest_materials.items():
            verify_source_record(item["material_source"])
            verify_usage(item["usage"], materials[key], f"material {key}")
            if item["identity"]["source_guid"] != materials[key][0]["guid"]:
                raise RuntimeError(f"Material GUID differs: {key}")
            soft_path = item["ue_soft_object_path"]
            if not SOFT_PATH.fullmatch(soft_path) or soft_path in soft_paths:
                raise RuntimeError(
                    f"Invalid/duplicate material soft path: {soft_path}"
                )
            soft_paths.add(soft_path)
            shader_ref, texture_refs = parse_material_refs(
                PROJECT_ROOT / key[0]
            )
            shader = item["shader"]
            if (
                shader["file_id"] != shader_ref["file_id"]
                or shader["guid"] != shader_ref["guid"]
            ):
                raise RuntimeError(f"Material shader ref differs: {key}")
            shader_text = ""
            if shader["source_shader"]:
                verify_source_record(shader["source_shader"])
                shader_path = PROJECT_ROOT / shader["source_shader"]["path"]
                shader_text = shader_path.read_text(
                    encoding="utf-8",
                    errors="strict",
                )
                if parse_meta_guid(Path(str(shader_path) + ".meta")) != (
                    shader["guid"]
                ):
                    raise RuntimeError(f"Shader GUID differs: {key}")
            recorded_properties = {
                value["property"]: value
                for value in item["texture_properties"]
            }
            if set(recorded_properties) != set(texture_refs):
                raise RuntimeError(
                    f"Material texture property chain differs: {key}"
                )
            for name, source_ref in texture_refs.items():
                recorded = recorded_properties[name]
                if recorded["texture_ref"] != source_ref:
                    raise RuntimeError(
                        f"Material texture ref differs: {key} {name}"
                    )
                if source_ref["file_id"] == "0":
                    if "source_texture" in recorded:
                        raise RuntimeError(
                            f"Null texture has source: {key} {name}"
                        )
                elif "source_texture" in recorded:
                    verify_source_record(recorded["source_texture"])
                    texture_path = (
                        PROJECT_ROOT / recorded["source_texture"]["path"]
                    )
                    if parse_meta_guid(Path(str(texture_path) + ".meta")) != (
                        source_ref["guid"]
                    ):
                        raise RuntimeError(
                            f"Texture GUID differs: {key} {name}"
                        )
                else:
                    if recorded.get("resolution_status") != (
                        "unresolved_stale_property_not_declared_by_bound_shader"
                    ):
                        raise RuntimeError(
                            f"Missing texture source is not fail-closed: "
                            f"{key} {name}"
                        )
                    if not shader_text or re.search(
                        rf"(?<![A-Za-z0-9_]){re.escape(name)}"
                        rf"(?![A-Za-z0-9_])",
                        shader_text,
                    ):
                        raise RuntimeError(
                            f"Missing render-bound texture source: "
                            f"{key} {name}"
                        )
        checks.append("material_shader_texture_hash_chain")

        manifest_sprites = {
            item["identity"]["source_path"]: item
            for item in manifest["sprite_textures"]
        }
        if set(manifest_sprites) != sprite_paths:
            raise RuntimeError("Sprite texture paths differ from runtime")
        sprite_use_by_path = defaultdict(list)
        for (path_text, _file_id), uses in sprites.items():
            sprite_use_by_path[path_text].extend(uses)
        generated_sprite_objs = set()
        total_bound_sprite_renderers = 0
        binding_ambiguities = 0
        sprite_mesh_count = 0
        for path_text, item in manifest_sprites.items():
            verify_source_record(item["source"])
            verify_usage(
                item["usage"],
                sprite_use_by_path[path_text],
                f"sprite texture {path_text}",
            )
            soft_path = item["ue_soft_object_path"]
            if not SOFT_PATH.fullmatch(soft_path) or soft_path in soft_paths:
                raise RuntimeError(
                    f"Invalid/duplicate texture soft path: {soft_path}"
                )
            soft_paths.add(soft_path)
            source_path = PROJECT_ROOT / path_text
            importer, sprite_ids = parse_texture_importer(
                Path(str(source_path) + ".meta"),
                source_path,
            )
            if item["texture_importer"] != importer:
                raise RuntimeError(
                    f"TextureImporter contract differs: {path_text}"
                )
            recorded_ids = {
                decimal(
                    record["source_file_id"],
                    "sprite subasset fileID",
                ): record
                for record in item["referenced_sprite_subassets"]
            }
            expected_ids = {
                key[1]
                for key in sprites
                if key[0] == path_text
            }
            if set(recorded_ids) != expected_ids:
                raise RuntimeError(
                    f"Sprite subasset IDs differ: {path_text}"
                )
            for file_id, record in recorded_ids.items():
                source_meta = sprite_ids.get(file_id)
                if source_meta != record["meta"]:
                    raise RuntimeError(
                        f"Sprite metadata differs: "
                        f"{path_text} {file_id}"
                    )
                if record["name"] != source_meta["name"]:
                    raise RuntimeError(
                        f"Sprite name differs: {path_text} {file_id}"
                    )
                verify_source_record(record["texture_importer_meta"])
                if (
                    record["texture_importer_meta"]["path"]
                    != item["source"]["meta"]["path"]
                    or record["texture_importer_meta"]["sha256"]
                    != item["source"]["meta"]["sha256"]
                    or record["source_texture_identity"]
                    != {
                        "path": path_text,
                        "guid": item["identity"]["source_guid"],
                    }
                    or record["ue_texture_soft_object_path"]
                    != item["ue_soft_object_path"]
                ):
                    raise RuntimeError(
                        f"Sprite texture/meta binding differs: "
                        f"{path_text} {file_id}"
                    )
                uses = sprites[(path_text, file_id)]
                verify_usage(
                    record["usage"],
                    uses,
                    f"sprite subasset {path_text} {file_id}",
                )
                total_bound_sprite_renderers += len(uses)
                if any(
                    use["draw_mode"] != 0
                    or use["flip_x"] != 0
                    or use["flip_y"] != 0
                    or any(
                        abs(use["color"][channel] - 1.0) > 1e-8
                        for channel in "rgba"
                    )
                    for use in uses
                ):
                    raise RuntimeError(
                        f"Sprite renderer source contract differs: "
                        f"{path_text} {file_id}"
                    )
                expected_quad = expected_sprite_quad(
                    source_meta,
                    importer,
                )
                if any(
                    abs(use["size"][axis] - expected_quad["size"][axis])
                    > 1e-5
                    for use in uses
                    for axis in ("x", "y")
                ):
                    raise RuntimeError(
                        f"Sprite renderer size differs from rect/PPU: "
                        f"{path_text} {file_id}"
                    )
                validation = record["renderer_source_validation"]
                if (
                    validation["validated_instance_count"] != len(uses)
                    or validation["draw_mode"] != 0
                    or validation["flip_x"] != 0
                    or validation["flip_y"] != 0
                    or not validation[
                        "size_matches_rect_divided_by_ppu"
                    ]
                    or any(
                        abs(
                            validation["unity_local_size"][axis]
                            - expected_quad["size"][axis]
                        )
                        > 1e-10
                        for axis in ("x", "y")
                    )
                ):
                    raise RuntimeError(
                        f"Sprite renderer validation differs: "
                        f"{path_text} {file_id}"
                    )
                material_identities = {
                    use["source_material"] for use in uses
                }
                if len(material_identities) != 1:
                    binding_ambiguities += 1
                    raise RuntimeError(
                        f"Sprite material binding is ambiguous: "
                        f"{path_text} {file_id}"
                    )
                source_material = next(iter(material_identities))
                recorded_material = record["source_material_identity"]
                if (
                    recorded_material["path"],
                    decimal(
                        recorded_material["file_id"],
                        "sprite material fileID",
                    ),
                    recorded_material["guid"],
                ) != source_material:
                    raise RuntimeError(
                        f"Sprite material identity differs: "
                        f"{path_text} {file_id}"
                    )
                material_record = manifest_materials.get(
                    (source_material[0], source_material[1])
                )
                if (
                    not material_record
                    or material_record["identity"]["source_guid"]
                    != source_material[2]
                    or record["ue_material_soft_object_path"]
                    != material_record["ue_soft_object_path"]
                ):
                    raise RuntimeError(
                        f"Sprite material target differs: "
                        f"{path_text} {file_id}"
                    )
                sprite_soft_path = record["ue_mesh_soft_object_path"]
                if (
                    not SOFT_PATH.fullmatch(sprite_soft_path)
                    or sprite_soft_path in soft_paths
                ):
                    raise RuntimeError(
                        f"Invalid/duplicate sprite mesh path: "
                        f"{sprite_soft_path}"
                    )
                soft_paths.add(sprite_soft_path)
                if record["binding_status"] != "ready":
                    raise RuntimeError(
                        f"Sprite binding is not ready: "
                        f"{path_text} {file_id}"
                    )
                geometry = record["quad_geometry"]
                if (
                    geometry["axis_determinant"] != 1
                    or geometry["vertex_count"] != 4
                    or geometry["uv_count"] != 4
                    or geometry["triangle_count"] != 2
                    or geometry["obj_normal"]
                    != {"x": 0.0, "y": -1.0, "z": 0.0}
                ):
                    raise RuntimeError(
                        f"Sprite quad geometry contract differs: "
                        f"{path_text} {file_id}"
                    )
                for axis in ("left", "right", "bottom", "top"):
                    if abs(
                        geometry["unity_local_bounds"][axis]
                        - expected_quad["bounds"][axis]
                    ) > 1e-10:
                        raise RuntimeError(
                            f"Sprite pivot bounds differ: "
                            f"{path_text} {file_id}"
                        )
                recorded_uvs = [
                    (entry["u"], entry["v"])
                    for entry in geometry["source_unity_uvs"]
                ]
                if any(
                    not close_tuple(actual, expected, tolerance=1e-12)
                    for actual, expected in zip(
                        recorded_uvs,
                        expected_quad["uvs"],
                    )
                ):
                    raise RuntimeError(
                        f"Sprite source UV record differs: "
                        f"{path_text} {file_id}"
                    )
                visual = record["visual_acceptance"]
                if (
                    visual["status"] != "pending"
                    or visual["uv_v_flip_applied"]
                ):
                    raise RuntimeError(
                        f"Sprite UV visual acceptance was guessed: "
                        f"{path_text} {file_id}"
                    )
                verify_sprite_obj(record, expected_quad)
                generated_sprite_objs.add(
                    record["exported_quad_obj"]["path"]
                )
                sprite_mesh_count += 1
        actual_sprite_obj_files = {
            rel(path)
            for path in (
                MIGRATION_ROOT
                / "WorldMapStreamingAssets"
                / "SpriteMeshes"
            ).glob("*.obj")
        }
        if actual_sprite_obj_files != generated_sprite_objs:
            raise RuntimeError(
                "Sprite OBJ folder contains stale or missing files"
            )
        if (
            sprite_mesh_count != 16
            or total_bound_sprite_renderers != 2005
            or binding_ambiguities != 0
            or int(manifest["statistics"]["sprite_mesh_count"]) != 16
            or int(
                manifest["statistics"]["sprite_renderer_bound_count"]
            )
            != 2005
            or int(
                manifest["statistics"]["sprite_renderer_noop_count"]
            )
            != 4
            or int(
                manifest["statistics"][
                    "sprite_binding_ambiguity_count"
                ]
            )
            != 0
        ):
            raise RuntimeError("Sprite binding coverage differs")
        if manifest["cleared_blockers"] != [
            "sprite_renderer_static_mesh_uv_bindings_missing"
        ]:
            raise RuntimeError("Static sprite binding blocker not cleared")
        if (
            not manifest["acceptance"][
                "sprite_static_mesh_uv_bindings_ready"
            ]
            or manifest["acceptance"][
                "sprite_runtime_visual_acceptance_complete"
            ]
        ):
            raise RuntimeError("Sprite acceptance status is incorrect")
        checks.append(
            "sprite_meta_quad_uv_material_usage_and_obj_binding"
        )

        blocked = manifest["blocked_policies"]
        if (
            blocked["particle_systems"]["count"] != 18
            or blocked["particle_systems"]["status"] != "blocked"
            or blocked["mesh_colliders"]["count"] != 6
            or blocked["mesh_colliders"]["status"] != "blocked"
            or blocked["null_sprites"]["count"] != 4
            or blocked["null_sprites"]["status"]
            != "source_explicit_render_noop"
        ):
            raise RuntimeError("Blocked source policies were broadened")
        recorded_null_sprites = sorted(
            blocked["null_sprites"]["records"],
            key=lambda item: (
                int(item["lod"]),
                int(item["sprite_renderer_file_id"]),
            ),
        )
        if recorded_null_sprites != collect_null_sprite_records(runtime):
            raise RuntimeError(
                "Null SpriteRenderer no-op records differ from runtime"
            )
        if (
            manifest["acceptance"]["particle_system_runtime_ready"]
            or manifest["acceptance"]["collision_runtime_ready"]
            or manifest["acceptance"]["ue_assets_imported"]
            or manifest["acceptance"]["ue_runtime_streaming_ready"]
        ):
            raise RuntimeError("Manifest fabricates runtime acceptance")
        expected_particles, expected_colliders = (
            collect_blocked_runtime_records(runtime)
        )
        recorded_particles = {
            decimal(
                record["particle_system_file_id"],
                "manifest ParticleSystem fileID",
            ): record
            for record in blocked["particle_systems"]["records"]
        }
        if set(recorded_particles) != set(expected_particles):
            raise RuntimeError(
                "ParticleSystem blocked identities differ from runtime"
            )
        for file_id, record in recorded_particles.items():
            expected = expected_particles[file_id]
            for field in (
                "lod",
                "recipe_id",
                "node_path",
                "game_object_file_id",
                "particle_system_file_id",
                "particle_renderer_file_id",
                "system_serialized_body_sha256",
                "renderer_serialized_body_sha256",
                "render_mode",
            ):
                if record[field] != expected[field]:
                    raise RuntimeError(
                        f"ParticleSystem {file_id} {field} differs"
                    )
            recorded_slots = []
            for slot in record["source_materials"]:
                verify_source_record(slot["source"])
                source_path = PROJECT_ROOT / slot["path"]
                if parse_meta_guid(Path(str(source_path) + ".meta")) != (
                    slot["guid"]
                ):
                    raise RuntimeError(
                        f"Particle material GUID differs: {file_id}"
                    )
                recorded_slots.append(
                    (
                        int(slot["slot_index"]),
                        slot["path"],
                        decimal(
                            slot["file_id"],
                            "particle material fileID",
                        ),
                        slot["guid"],
                    )
                )
            if recorded_slots != expected["material_slots"]:
                raise RuntimeError(
                    f"Particle material slots differ: {file_id}"
                )
        recorded_colliders = {
            decimal(
                record["mesh_collider_file_id"],
                "manifest MeshCollider fileID",
            ): record
            for record in blocked["mesh_colliders"]["records"]
        }
        if set(recorded_colliders) != set(expected_colliders):
            raise RuntimeError(
                "MeshCollider blocked identities differ from runtime"
            )
        for file_id, record in recorded_colliders.items():
            expected = expected_colliders[file_id]
            for field in (
                "lod",
                "recipe_id",
                "node_path",
                "game_object_file_id",
                "mesh_collider_file_id",
                "enabled",
                "is_trigger",
                "convex",
                "cooking_options",
                "serialized_body_sha256",
            ):
                if record[field] != expected[field]:
                    raise RuntimeError(
                        f"MeshCollider {file_id} {field} differs"
                    )
            source_mesh = record["source_mesh"]
            verify_source_record(source_mesh["source"])
            if (
                source_mesh["path"] != expected["source_path"]
                or decimal(
                    source_mesh["file_id"],
                    "collider source mesh fileID",
                )
                != expected["source_file_id"]
                or source_mesh["guid"] != expected["source_guid"]
            ):
                raise RuntimeError(
                    f"MeshCollider source differs: {file_id}"
                )
            source_path = PROJECT_ROOT / source_mesh["path"]
            if parse_meta_guid(Path(str(source_path) + ".meta")) != (
                source_mesh["guid"]
            ):
                raise RuntimeError(
                    f"MeshCollider source GUID differs: {file_id}"
                )
        stale_unbound_count = sum(
            property_record.get("resolution_status")
            == "unresolved_stale_property_not_declared_by_bound_shader"
            for material in manifest["materials"]
            for property_record in material["texture_properties"]
        )
        if int(
            manifest["statistics"][
                "unresolved_stale_unbound_texture_property_count"
            ]
        ) != stale_unbound_count:
            raise RuntimeError(
                "Stale unbound material property count differs"
            )
        if int(
            manifest["statistics"]["missing_render_bound_source_count"]
        ) != 0:
            raise RuntimeError("Manifest has a missing render-bound source")
        checks.append("explicit_particle_collider_null_sprite_policies")
    except Exception as exc:
        failures.append(str(exc))

    result = {
        "schema": "rok.unity_world_map_streaming_assets_verify.v1",
        "source_manifest": {
            "path": rel(MANIFEST_PATH),
            "sha256": sha256(MANIFEST_PATH)
            if MANIFEST_PATH.is_file()
            else None,
        },
        "checks": checks,
        "failure_count": len(failures),
        "failures": failures,
        "verified": not failures,
    }
    VERIFY_PATH.write_text(
        json.dumps(
            result,
            indent=2,
            ensure_ascii=False,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    if failures:
        raise RuntimeError("; ".join(failures))
    print(
        "Rok Unity world-map streaming assets verified: "
        f"checks={len(checks)} failures=0"
    )


if __name__ == "__main__":
    main()
