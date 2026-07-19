import hashlib
import io
import json
import math
import struct
import zlib
from pathlib import Path

import numpy as np
from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
GROUND_MANIFEST_PATH = MIGRATION_ROOT / "unity_runtime_ground_obj_compensated_manifest.json"
OUTPUT_MANIFEST_PATH = MIGRATION_ROOT / "unity_runtime_ground_vertex_alpha.json"
OUTPUT_VERIFY_PATH = MIGRATION_ROOT / "unity_runtime_ground_vertex_alpha_verify.json"
SCHEMA = "rok.unity.runtime_ground_vertex_alpha.v1"
TARGET_MESH_STEM = "CityTranslucent_101_1"
TARGET_SHADER = "custom/map/cm_mask_texture_lod_alpha"
UV_MATCH_TOLERANCE = 2.0e-6


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha256_file(path):
    return sha256_bytes(Path(path).read_bytes())


def unity_asset_path_to_disk(asset_path):
    relative = str(asset_path or "").replace("/", "\\")
    return PROJECT_ROOT / "ref" / "client-unity" / relative


class BinaryFbxReader:
    def __init__(self, path):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        if not self.data.startswith(b"Kaydara FBX Binary  \x00\x1a\x00"):
            raise RuntimeError("Expected a binary FBX file: {0}".format(self.path))
        self.version = struct.unpack_from("<I", self.data, 23)[0]
        self.node_header_size = 25 if self.version >= 7500 else 13

    def read_property(self, offset):
        property_type = chr(self.data[offset])
        offset += 1
        scalar_formats = {
            "Y": "<h",
            "C": "<?",
            "I": "<i",
            "F": "<f",
            "D": "<d",
            "L": "<q",
        }
        if property_type in scalar_formats:
            value_format = scalar_formats[property_type]
            value = struct.unpack_from(value_format, self.data, offset)[0]
            return value, offset + struct.calcsize(value_format)

        if property_type in "fdlib":
            count, encoding, compressed_length = struct.unpack_from("<III", self.data, offset)
            offset += 12
            raw = self.data[offset:offset + compressed_length]
            offset += compressed_length
            if encoding == 1:
                raw = zlib.decompress(raw)
            elif encoding != 0:
                raise RuntimeError("Unsupported FBX array encoding: {0}".format(encoding))
            dtype = {
                "f": "<f4",
                "d": "<f8",
                "l": "<i8",
                "i": "<i4",
                "b": "u1",
            }[property_type]
            expected_bytes = count * np.dtype(dtype).itemsize
            if len(raw) != expected_bytes:
                raise RuntimeError(
                    "FBX array byte count mismatch for {0}: expected {1}, got {2}".format(
                        property_type, expected_bytes, len(raw)
                    )
                )
            return np.frombuffer(raw, dtype=dtype).copy(), offset

        if property_type in "SR":
            length = struct.unpack_from("<I", self.data, offset)[0]
            offset += 4
            raw = self.data[offset:offset + length]
            offset += length
            if property_type == "S":
                return raw.decode("utf-8", "replace"), offset
            return bytes(raw), offset

        raise RuntimeError("Unsupported FBX property type: {0}".format(property_type))

    def read_node(self, offset):
        if self.version >= 7500:
            end_offset, property_count, _property_bytes = struct.unpack_from("<QQQ", self.data, offset)
            name_length = self.data[offset + 24]
            offset += 25
        else:
            end_offset, property_count, _property_bytes = struct.unpack_from("<III", self.data, offset)
            name_length = self.data[offset + 12]
            offset += 13

        if end_offset == 0:
            return None, offset

        name = self.data[offset:offset + name_length].decode("utf-8", "replace")
        offset += name_length
        properties = []
        for _index in range(property_count):
            value, offset = self.read_property(offset)
            properties.append(value)

        children = []
        while offset < end_offset - self.node_header_size:
            child, next_offset = self.read_node(offset)
            if child is None:
                break
            children.append(child)
            offset = next_offset
        return {"name": name, "properties": properties, "children": children}, end_offset

    def roots(self):
        result = []
        offset = 27
        while offset + self.node_header_size <= len(self.data):
            node, next_offset = self.read_node(offset)
            if node is None:
                break
            result.append(node)
            offset = next_offset
        return result


def walk_nodes(node):
    yield node
    for child in node.get("children", []):
        yield from walk_nodes(child)


def child_node(node, name):
    matches = [child for child in node.get("children", []) if child.get("name") == name]
    if len(matches) != 1:
        raise RuntimeError("Expected one FBX child node {0}, found {1}".format(name, len(matches)))
    return matches[0]


def node_scalar(node):
    properties = node.get("properties", [])
    if len(properties) != 1:
        raise RuntimeError("Expected one property on FBX node {0}".format(node.get("name")))
    return properties[0]


def clean_fbx_name(value):
    return str(value).split("\x00", 1)[0]


def find_mesh_geometry(roots):
    geometries = []
    for root in roots:
        for node in walk_nodes(root):
            properties = node.get("properties", [])
            if node.get("name") == "Geometry" and len(properties) >= 3 and clean_fbx_name(properties[2]) == "Mesh":
                geometries.append(node)
    if len(geometries) != 1:
        raise RuntimeError("Expected exactly one mesh Geometry node, found {0}".format(len(geometries)))
    return geometries[0]


def parse_fbx_vertex_alpha(fbx_path):
    reader = BinaryFbxReader(fbx_path)
    geometry = find_mesh_geometry(reader.roots())
    vertices = np.asarray(node_scalar(child_node(geometry, "Vertices")), dtype=np.float64).reshape((-1, 3))
    polygon_vertex_indices = np.asarray(
        node_scalar(child_node(geometry, "PolygonVertexIndex")), dtype=np.int64
    )

    uv_layer = child_node(geometry, "LayerElementUV")
    uv_mapping = clean_fbx_name(node_scalar(child_node(uv_layer, "MappingInformationType")))
    uv_reference = clean_fbx_name(node_scalar(child_node(uv_layer, "ReferenceInformationType")))
    direct_uvs = np.asarray(node_scalar(child_node(uv_layer, "UV")), dtype=np.float64).reshape((-1, 2))
    uv_indices = np.asarray(node_scalar(child_node(uv_layer, "UVIndex")), dtype=np.int64)

    color_layer = child_node(geometry, "LayerElementColor")
    color_mapping = clean_fbx_name(node_scalar(child_node(color_layer, "MappingInformationType")))
    color_reference = clean_fbx_name(node_scalar(child_node(color_layer, "ReferenceInformationType")))
    direct_colors = np.asarray(node_scalar(child_node(color_layer, "Colors")), dtype=np.float64).reshape((-1, 4))
    color_indices = np.asarray(node_scalar(child_node(color_layer, "ColorIndex")), dtype=np.int64)

    if uv_mapping != "ByPolygonVertex" or uv_reference != "IndexToDirect":
        raise RuntimeError("Unexpected FBX UV mapping: {0}/{1}".format(uv_mapping, uv_reference))
    if color_mapping != "ByPolygonVertex" or color_reference != "IndexToDirect":
        raise RuntimeError("Unexpected FBX color mapping: {0}/{1}".format(color_mapping, color_reference))
    corner_count = len(polygon_vertex_indices)
    if len(uv_indices) != corner_count or len(color_indices) != corner_count:
        raise RuntimeError(
            "FBX corner attribute length mismatch: polygon={0}, uv={1}, color={2}".format(
                corner_count, len(uv_indices), len(color_indices)
            )
        )
    if np.any(uv_indices < 0) or np.any(uv_indices >= len(direct_uvs)):
        raise RuntimeError("FBX UVIndex contains an out-of-range value")
    if np.any(color_indices < 0) or np.any(color_indices >= len(direct_colors)):
        raise RuntimeError("FBX ColorIndex contains an out-of-range value")

    corner_alphas = direct_colors[color_indices, 3]
    alpha_by_uv_index = np.full(len(direct_uvs), np.nan, dtype=np.float64)
    for uv_index, alpha in zip(uv_indices, corner_alphas):
        previous = alpha_by_uv_index[uv_index]
        if math.isnan(previous):
            alpha_by_uv_index[uv_index] = alpha
        elif abs(previous - alpha) > 1.0e-7:
            raise RuntimeError(
                "The same direct UV index maps to conflicting vertex alpha values: {0} vs {1}".format(
                    previous, alpha
                )
            )
    if np.any(np.isnan(alpha_by_uv_index)):
        raise RuntimeError("At least one direct FBX UV has no vertex-alpha assignment")

    polygon_sizes = []
    current_size = 0
    for index in polygon_vertex_indices:
        current_size += 1
        if index < 0:
            polygon_sizes.append(current_size)
            current_size = 0
    if current_size:
        raise RuntimeError("FBX PolygonVertexIndex ended before a polygon terminator")

    return {
        "version": reader.version,
        "control_point_count": len(vertices),
        "polygon_count": len(polygon_sizes),
        "polygon_sizes": polygon_sizes,
        "polygon_vertex_count": corner_count,
        "direct_uvs": direct_uvs,
        "direct_color_count": len(direct_colors),
        "corner_alphas": corner_alphas,
        "alpha_by_uv_index": alpha_by_uv_index,
        "uv_mapping": uv_mapping,
        "uv_reference": uv_reference,
        "color_mapping": color_mapping,
        "color_reference": color_reference,
    }


def parse_obj_uv_triangles(obj_path):
    vertices = []
    uvs = []
    triangles = []
    for raw_line in Path(obj_path).read_text(encoding="utf-8-sig").splitlines():
        line = raw_line.strip()
        if line.startswith("v "):
            values = line.split()
            vertices.append(tuple(float(value) for value in values[1:4]))
        elif line.startswith("vt "):
            values = line.split()
            uvs.append(tuple(float(value) for value in values[1:3]))
        elif line.startswith("f "):
            corners = []
            for token in line.split()[1:]:
                fields = token.split("/")
                vertex_index = int(fields[0])
                uv_index = int(fields[1]) if len(fields) > 1 and fields[1] else vertex_index
                if vertex_index < 0:
                    vertex_index = len(vertices) + vertex_index + 1
                if uv_index < 0:
                    uv_index = len(uvs) + uv_index + 1
                corners.append((vertex_index - 1, uv_index - 1))
            if len(corners) < 3:
                raise RuntimeError("OBJ face has fewer than three corners")
            for index in range(1, len(corners) - 1):
                triangles.append((corners[0], corners[index], corners[index + 1]))

    if not vertices or not uvs or not triangles:
        raise RuntimeError("OBJ is missing vertices, UVs, or faces: {0}".format(obj_path))
    for triangle in triangles:
        for vertex_index, uv_index in triangle:
            if not (0 <= vertex_index < len(vertices)) or not (0 <= uv_index < len(uvs)):
                raise RuntimeError("OBJ face contains an out-of-range vertex or UV index")
    return np.asarray(vertices, dtype=np.float64), np.asarray(uvs, dtype=np.float64), triangles


def match_obj_uv_alpha(obj_uvs, direct_uvs, direct_uv_alphas):
    result = np.zeros(len(obj_uvs), dtype=np.float64)
    max_error = 0.0
    for index, obj_uv in enumerate(obj_uvs):
        squared_errors = np.sum((direct_uvs - obj_uv) ** 2, axis=1)
        source_index = int(np.argmin(squared_errors))
        error = math.sqrt(float(squared_errors[source_index]))
        max_error = max(max_error, error)
        if error > UV_MATCH_TOLERANCE:
            raise RuntimeError(
                "OBJ UV {0} has no FBX direct-UV match within tolerance; nearest error={1}".format(
                    index, error
                )
            )
        result[index] = direct_uv_alphas[source_index]
    return result, max_error


def rasterize_obj_vertex_alpha(width, height, obj_uvs, triangles, obj_uv_alphas):
    alpha = np.zeros((height, width), dtype=np.float32)
    coverage = np.zeros((height, width), dtype=np.bool_)
    overlap_max_delta = 0.0
    degenerate_triangles = 0

    for triangle in triangles:
        uv_indices = [corner[1] for corner in triangle]
        triangle_uvs = obj_uvs[uv_indices]
        triangle_alphas = obj_uv_alphas[uv_indices]
        # Raster samples live at pixel centers: u=(x+0.5)/width and
        # Unity v=1-(y+0.5)/height after conversion to PNG row order.
        px = triangle_uvs[:, 0] * width - 0.5
        py = (1.0 - triangle_uvs[:, 1]) * height - 0.5
        x0, x1, x2 = px
        y0, y1, y2 = py
        denominator = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(denominator) < 1.0e-10:
            degenerate_triangles += 1
            continue

        min_x = max(0, int(math.floor(float(np.min(px)))))
        max_x = min(width - 1, int(math.ceil(float(np.max(px)))))
        min_y = max(0, int(math.floor(float(np.min(py)))))
        max_y = min(height - 1, int(math.ceil(float(np.max(py)))))
        if min_x > max_x or min_y > max_y:
            continue

        grid_y, grid_x = np.mgrid[min_y:max_y + 1, min_x:max_x + 1]
        weight0 = ((y1 - y2) * (grid_x - x2) + (x2 - x1) * (grid_y - y2)) / denominator
        weight1 = ((y2 - y0) * (grid_x - x2) + (x0 - x2) * (grid_y - y2)) / denominator
        weight2 = 1.0 - weight0 - weight1
        inside = (weight0 >= -1.0e-5) & (weight1 >= -1.0e-5) & (weight2 >= -1.0e-5)
        if not np.any(inside):
            continue

        values = np.clip(
            weight0 * triangle_alphas[0] + weight1 * triangle_alphas[1] + weight2 * triangle_alphas[2],
            0.0,
            1.0,
        ).astype(np.float32)
        alpha_region = alpha[min_y:max_y + 1, min_x:max_x + 1]
        coverage_region = coverage[min_y:max_y + 1, min_x:max_x + 1]
        overlap = inside & coverage_region
        if np.any(overlap):
            overlap_max_delta = max(
                overlap_max_delta,
                float(np.max(np.abs(alpha_region[overlap] - values[overlap]))),
            )
        write_new = inside & ~coverage_region
        alpha_region[write_new] = values[write_new]
        alpha_region[overlap] = np.maximum(alpha_region[overlap], values[overlap])
        coverage_region[inside] = True

    if not np.any(coverage):
        raise RuntimeError("Vertex-alpha rasterization did not cover any texture pixels")
    return np.rint(alpha * 255.0).astype(np.uint8), coverage, overlap_max_delta, degenerate_triangles


def load_target_record():
    if not GROUND_MANIFEST_PATH.exists():
        raise RuntimeError("Missing Unity runtime ground manifest: {0}".format(GROUND_MANIFEST_PATH))
    manifest = json.loads(GROUND_MANIFEST_PATH.read_text(encoding="utf-8-sig"))
    matches = [record for record in manifest.get("records", []) if record.get("mesh_stem") == TARGET_MESH_STEM]
    if len(matches) != 1:
        raise RuntimeError(
            "Expected one {0} runtime ground record, found {1}".format(TARGET_MESH_STEM, len(matches))
        )
    record = matches[0]
    if record.get("shader_name") != TARGET_SHADER:
        raise RuntimeError(
            "Unexpected Unity shader for {0}: {1}".format(TARGET_MESH_STEM, record.get("shader_name"))
        )
    return manifest, record


def build_vertex_alpha_record():
    source_manifest, source_record = load_target_record()
    fbx_path = unity_asset_path_to_disk(source_record.get("mesh_asset_path"))
    shader_path = unity_asset_path_to_disk(source_record.get("shader_asset_path"))
    obj_path = Path(source_record.get("obj_path", ""))
    baked_texture_path = Path(source_record.get("baked_texture_path", ""))
    for path, label in [
        (fbx_path, "source FBX"),
        (shader_path, "source shader"),
        (obj_path, "runtime OBJ"),
        (baked_texture_path, "shader-baked texture"),
    ]:
        if not path.exists():
            raise RuntimeError("Missing {0}: {1}".format(label, path))

    shader_text = shader_path.read_text(encoding="utf-8-sig")
    if "col.a = i.color.a" not in shader_text:
        raise RuntimeError("Unity source shader no longer assigns interpolated vertex alpha to col.a")

    fbx = parse_fbx_vertex_alpha(fbx_path)
    obj_vertices, obj_uvs, triangles = parse_obj_uv_triangles(obj_path)
    obj_uv_alphas, max_uv_match_error = match_obj_uv_alpha(
        obj_uvs, fbx["direct_uvs"], fbx["alpha_by_uv_index"]
    )

    source_image = Image.open(baked_texture_path).convert("RGBA")
    width, height = source_image.size
    output_alpha, coverage, overlap_max_delta, degenerate_triangles = rasterize_obj_vertex_alpha(
        width, height, obj_uvs, triangles, obj_uv_alphas
    )
    output_image = source_image.copy()
    output_image.putalpha(Image.fromarray(output_alpha, mode="L"))
    output_buffer = io.BytesIO()
    output_image.save(output_buffer, format="PNG", optimize=False, compress_level=6)
    output_png_bytes = output_buffer.getvalue()
    output_path = baked_texture_path.with_name(baked_texture_path.stem + "_VertexAlpha.png")

    corner_alphas = fbx["corner_alphas"]
    unique_corner_alphas = np.unique(np.round(corner_alphas, decimals=9))
    output_unique = np.unique(output_alpha)
    source_alpha = np.asarray(source_image.getchannel("A"), dtype=np.uint8)
    polygon_size_counts = {
        str(size): int(fbx["polygon_sizes"].count(size)) for size in sorted(set(fbx["polygon_sizes"]))
    }
    record = {
        "object_name": source_record.get("object_name"),
        "mesh_stem": source_record.get("mesh_stem"),
        "unity_shader_name": source_record.get("shader_name"),
        "unity_shader_path": str(shader_path),
        "unity_shader_alpha_formula": "col.a = interpolated vertex color alpha",
        "source_ground_manifest": str(GROUND_MANIFEST_PATH),
        "source_ground_manifest_schema": source_manifest.get("schema"),
        "source_fbx_path": str(fbx_path),
        "source_fbx_sha256": sha256_file(fbx_path),
        "source_fbx_version": fbx["version"],
        "source_obj_path": str(obj_path),
        "source_obj_sha256": sha256_file(obj_path),
        "source_baked_texture_path": str(baked_texture_path),
        "source_baked_texture_sha256": sha256_file(baked_texture_path),
        "source_baked_texture_pixel_sha256": sha256_bytes(source_image.tobytes()),
        "source_baked_texture_size": [width, height],
        "source_baked_texture_alpha_extrema": [int(source_alpha.min()), int(source_alpha.max())],
        "fbx_control_point_count": int(fbx["control_point_count"]),
        "fbx_polygon_count": int(fbx["polygon_count"]),
        "fbx_polygon_size_counts": polygon_size_counts,
        "fbx_polygon_vertex_count": int(fbx["polygon_vertex_count"]),
        "fbx_uv_mapping": fbx["uv_mapping"],
        "fbx_uv_reference": fbx["uv_reference"],
        "fbx_direct_uv_count": int(len(fbx["direct_uvs"])),
        "fbx_color_mapping": fbx["color_mapping"],
        "fbx_color_reference": fbx["color_reference"],
        "fbx_direct_color_count": int(fbx["direct_color_count"]),
        "fbx_corner_alpha_extrema": [float(corner_alphas.min()), float(corner_alphas.max())],
        "fbx_corner_alpha_unique_count": int(len(unique_corner_alphas)),
        "fbx_corner_alpha_zero_count": int(np.count_nonzero(np.isclose(corner_alphas, 0.0, atol=1.0e-9))),
        "fbx_corner_alpha_one_count": int(np.count_nonzero(np.isclose(corner_alphas, 1.0, atol=1.0e-9))),
        "obj_vertex_count": int(len(obj_vertices)),
        "obj_uv_count": int(len(obj_uvs)),
        "obj_triangle_count": int(len(triangles)),
        "obj_to_fbx_uv_match_tolerance": UV_MATCH_TOLERANCE,
        "obj_to_fbx_uv_max_error": float(max_uv_match_error),
        "raster_coordinate_rule": "PNG pixel center uses u=(x+0.5)/width and Unity v=1-(y+0.5)/height",
        "raster_covered_pixel_count": int(np.count_nonzero(coverage)),
        "raster_coverage_fraction": float(np.count_nonzero(coverage) / coverage.size),
        "raster_shared_edge_max_alpha_delta": float(overlap_max_delta),
        "raster_degenerate_triangle_count": int(degenerate_triangles),
        "output_texture_path": str(output_path),
        "output_texture_size": [width, height],
        "output_alpha_extrema": [int(output_alpha.min()), int(output_alpha.max())],
        "output_alpha_unique_count": int(len(output_unique)),
        "output_alpha_zero_pixel_count": int(np.count_nonzero(output_alpha == 0)),
        "output_alpha_opaque_pixel_count": int(np.count_nonzero(output_alpha == 255)),
        "output_pixel_sha256": sha256_bytes(output_image.tobytes()),
        "output_png_sha256": sha256_bytes(output_png_bytes),
    }
    return record, output_png_bytes


def expected_manifest():
    record, output_png_bytes = build_vertex_alpha_record()
    return {"schema": SCHEMA, "records": [record]}, output_png_bytes


def main():
    manifest, output_png_bytes = expected_manifest()
    record = manifest["records"][0]
    output_path = Path(record["output_texture_path"])
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(output_png_bytes)
    OUTPUT_MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_MANIFEST_PATH.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(
        "prepared_runtime_ground_vertex_alpha mesh={0} corners={1} triangles={2} alpha={3} failures=0".format(
            record["mesh_stem"],
            record["fbx_polygon_vertex_count"],
            record["obj_triangle_count"],
            record["output_alpha_extrema"],
        )
    )


if __name__ == "__main__":
    main()
