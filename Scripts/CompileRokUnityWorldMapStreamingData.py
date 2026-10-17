import copy
import hashlib
import json
import math
import re
from collections import Counter, defaultdict
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
WORLD_PATH = MIGRATION_ROOT / "unity_world_map.json"
RUNTIME_PATH = MIGRATION_ROOT / "unity_world_map_runtime_data.json"
NEAR_ASSETS_PATH = MIGRATION_ROOT / "unity_world_map_assets.json"
NEAR_UE_PATH = MIGRATION_ROOT / "unity_world_map_ue_generation.json"
STREAMING_ASSETS_PATH = (
    MIGRATION_ROOT / "unity_world_map_streaming_assets.json"
)
OUTPUT_PATH = MIGRATION_ROOT / "unity_world_map_streaming_data.json"
EVIDENCE_PATH = (
    MIGRATION_ROOT / "unity_world_map_streaming_data_compile.json"
)
SUMMARY_PATH = MIGRATION_ROOT / "unity_world_map_streaming_data.md"

SCHEMA = "rok.world_map_runtime_data_asset.v1"
EVIDENCE_SCHEMA = "rok.world_map_streaming_data_compile.v1"
EXPECTED_RUNTIME_SCHEMA = "rok.unity_world_map_runtime_data.v1"
EXPECTED_NEAR_SCHEMA = "rok.unity_world_map_assets.v1"
EXPECTED_NEAR_UE_SCHEMA = "rok.unity_world_map_ue_generation.v1"
EXPECTED_STREAMING_SCHEMA = "rok.unity_world_map_streaming_assets.v1"
LOD_THRESHOLDS = [1500.0, 3000.0, 8000.0, 20000.0, 42500.0, 999999.0]
REACHABLE_MAX_DXF = 72750.0
ASPECT_RATIO = 16.0 / 10.0
TRANSFORM_EPSILON = 1.0e-7
MATRIX_TOLERANCE = 2.0e-5


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
        "Path": rel(path),
        "Size": path.stat().st_size,
        "Sha256": sha256(path),
    }


def load_json(path, expected_schema):
    if not path.is_file():
        raise RuntimeError(f"Missing required input: {rel(path)}")
    value = json.loads(path.read_text(encoding="utf-8"))
    observed = value.get("schema")
    if observed != expected_schema:
        raise RuntimeError(
            f"{rel(path)} schema {observed!r} != {expected_schema!r}"
        )
    return value


def canonical_number(value, digits=8):
    result = round(float(value), digits)
    return 0.0 if result == 0.0 else result


def canonical_json_bytes(value):
    return json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")


def canonical_payload_hash(value):
    payload = copy.deepcopy(value)
    payload.pop("CanonicalPayloadSha256", None)
    return sha256_bytes(canonical_json_bytes(payload))


def identity_matrix():
    return [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def matrix_multiply(left, right):
    return [
        [
            sum(
                float(left[row][index]) * float(right[index][column])
                for index in range(4)
            )
            for column in range(4)
        ]
        for row in range(4)
    ]


def matrix_from_trs(position, rotation, scale):
    x = float(rotation["x"])
    y = float(rotation["y"])
    z = float(rotation["z"])
    w = float(rotation["w"])
    length = math.sqrt(x * x + y * y + z * z + w * w)
    if not math.isfinite(length) or length <= TRANSFORM_EPSILON:
        raise RuntimeError("Source TRS has an invalid quaternion")
    x, y, z, w = (value / length for value in (x, y, z, w))
    rotation_matrix = [
        [
            1.0 - 2.0 * (y * y + z * z),
            2.0 * (x * y - z * w),
            2.0 * (x * z + y * w),
        ],
        [
            2.0 * (x * y + z * w),
            1.0 - 2.0 * (x * x + z * z),
            2.0 * (y * z - x * w),
        ],
        [
            2.0 * (x * z - y * w),
            2.0 * (y * z + x * w),
            1.0 - 2.0 * (x * x + y * y),
        ],
    ]
    scales = (float(scale["x"]), float(scale["y"]), float(scale["z"]))
    result = identity_matrix()
    for row in range(3):
        for column in range(3):
            result[row][column] = rotation_matrix[row][column] * scales[column]
    result[0][3] = float(position["x"])
    result[1][3] = float(position["y"])
    result[2][3] = float(position["z"])
    return result


def determinant3(matrix):
    return (
        matrix[0][0]
        * (
            matrix[1][1] * matrix[2][2]
            - matrix[1][2] * matrix[2][1]
        )
        - matrix[0][1]
        * (
            matrix[1][0] * matrix[2][2]
            - matrix[1][2] * matrix[2][0]
        )
        + matrix[0][2]
        * (
            matrix[1][0] * matrix[2][1]
            - matrix[1][1] * matrix[2][0]
        )
    )


def basis_convert_matrix(
    unity_matrix,
    translation_scale,
    translation_offset=(0.0, 0.0, 0.0),
):
    # Unmirrored UE component basis: (Unity X,Y,Z) -> (X,Z,Y).
    # The shared HISM component owns the single global X mirror.
    order = (0, 2, 1)
    result = identity_matrix()
    for row in range(3):
        for column in range(3):
            result[row][column] = float(
                unity_matrix[order[row]][order[column]]
            )
        result[row][3] = (
            float(unity_matrix[order[row]][3])
            - float(translation_offset[row])
        ) * float(translation_scale)
    return result


def quaternion_from_rotation(rotation):
    trace = rotation[0][0] + rotation[1][1] + rotation[2][2]
    if trace > 0.0:
        scale = math.sqrt(trace + 1.0) * 2.0
        w = 0.25 * scale
        x = (rotation[2][1] - rotation[1][2]) / scale
        y = (rotation[0][2] - rotation[2][0]) / scale
        z = (rotation[1][0] - rotation[0][1]) / scale
    elif rotation[0][0] > rotation[1][1] and rotation[0][0] > rotation[2][2]:
        scale = math.sqrt(
            1.0 + rotation[0][0] - rotation[1][1] - rotation[2][2]
        ) * 2.0
        w = (rotation[2][1] - rotation[1][2]) / scale
        x = 0.25 * scale
        y = (rotation[0][1] + rotation[1][0]) / scale
        z = (rotation[0][2] + rotation[2][0]) / scale
    elif rotation[1][1] > rotation[2][2]:
        scale = math.sqrt(
            1.0 + rotation[1][1] - rotation[0][0] - rotation[2][2]
        ) * 2.0
        w = (rotation[0][2] - rotation[2][0]) / scale
        x = (rotation[0][1] + rotation[1][0]) / scale
        y = 0.25 * scale
        z = (rotation[1][2] + rotation[2][1]) / scale
    else:
        scale = math.sqrt(
            1.0 + rotation[2][2] - rotation[0][0] - rotation[1][1]
        ) * 2.0
        w = (rotation[1][0] - rotation[0][1]) / scale
        x = (rotation[0][2] + rotation[2][0]) / scale
        y = (rotation[1][2] + rotation[2][1]) / scale
        z = 0.25 * scale
    length = math.sqrt(x * x + y * y + z * z + w * w)
    if length <= TRANSFORM_EPSILON:
        raise RuntimeError("Matrix produced a zero-length quaternion")
    values = [x / length, y / length, z / length, w / length]
    if values[3] < 0.0:
        values = [-item for item in values]
    return values


def transform_from_matrix(matrix, mesh_scale=1.0):
    determinant = determinant3(matrix)
    if not math.isfinite(determinant) or abs(determinant) <= TRANSFORM_EPSILON:
        raise ValueError(
            f"Residual transform determinant is degenerate: {determinant}"
        )
    # Unity mirrors some ground transition tiles with a negative-scale
    # transform. Fold the reflection into a negative X scale so the rotation
    # stays a proper rotation; UE composes transforms as T * R * S and
    # reverse-culls negative-determinant instances, and the affected ground
    # materials are two-sided unlit.
    mirrored = determinant < 0.0
    scales = [
        math.sqrt(sum(matrix[row][column] ** 2 for row in range(3)))
        for column in range(3)
    ]
    if any(
        not math.isfinite(scale) or scale <= TRANSFORM_EPSILON
        for scale in scales
    ):
        raise ValueError(f"Invalid transform scales: {scales}")
    rotation = [
        [
            float(matrix[row][column]) / scales[column]
            for column in range(3)
        ]
        for row in range(3)
    ]
    if mirrored:
        for row in range(3):
            rotation[row][0] = -rotation[row][0]
    rotation_determinant = determinant3(rotation)
    if not math.isclose(
        rotation_determinant,
        1.0,
        rel_tol=0.0,
        abs_tol=MATRIX_TOLERANCE,
    ):
        raise ValueError(
            "Transform contains shear or a residual reflection: "
            f"rotation determinant {rotation_determinant}"
        )
    for left in range(3):
        for right in range(left + 1, 3):
            dot = sum(
                rotation[row][left] * rotation[row][right]
                for row in range(3)
            )
            if abs(dot) > MATRIX_TOLERANCE:
                raise ValueError(
                    f"Transform contains unsupported shear: dot={dot}"
                )
    quaternion = quaternion_from_rotation(rotation)
    return {
        "Rotation": {
            "X": canonical_number(quaternion[0]),
            "Y": canonical_number(quaternion[1]),
            "Z": canonical_number(quaternion[2]),
            "W": canonical_number(quaternion[3]),
        },
        "Translation": {
            "X": canonical_number(matrix[0][3]),
            "Y": canonical_number(matrix[1][3]),
            "Z": canonical_number(matrix[2][3]),
        },
        "Scale3D": {
            "X": canonical_number(
                (-scales[0] if mirrored else scales[0]) * mesh_scale
            ),
            "Y": canonical_number(scales[1] * mesh_scale),
            "Z": canonical_number(scales[2] * mesh_scale),
        },
    }


def vector_length(vector):
    return math.sqrt(sum(float(item) ** 2 for item in vector))


def normalize(vector):
    length = vector_length(vector)
    if length <= TRANSFORM_EPSILON:
        raise RuntimeError(f"Cannot normalize zero vector {vector}")
    return [float(item) / length for item in vector]


def cross(left, right):
    return [
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    ]


def interpolate_camera(keys, dxf):
    safe_dxf = min(max(float(dxf), keys[0]["dxf"]), REACHABLE_MAX_DXF)
    for key in keys:
        if math.isclose(
            safe_dxf,
            float(key["dxf"]),
            rel_tol=0.0,
            abs_tol=1.0e-9,
        ):
            return {
                "distance": float(key["distance_unity_units"]),
                "vertical_fov": float(key["vertical_fov_degrees"]),
                "lower_id": key["id"],
                "upper_id": key["id"],
                "interpolation_t": 0.0,
            }
    for lower, upper in zip(keys, keys[1:]):
        if float(lower["dxf"]) < safe_dxf < float(upper["dxf"]):
            dist_min = float(lower["distance_unity_units"])
            dist_max = float(upper["distance_unity_units"])
            fov_min = float(lower["vertical_fov_degrees"])
            fov_max = float(upper["vertical_fov_degrees"])
            lo = 0.0
            hi = 1.0
            chosen = None
            for _ in range(24):
                middle = (lo + hi) * 0.5
                product = (
                    dist_min + middle * (dist_max - dist_min)
                ) * (fov_min + middle * (fov_max - fov_min))
                if math.isclose(
                    product,
                    safe_dxf,
                    rel_tol=0.0,
                    abs_tol=1.0e-12,
                ):
                    chosen = middle
                    break
                if product > safe_dxf:
                    hi = middle
                else:
                    lo = middle
            if chosen is None:
                chosen = (lo + hi) * 0.5
            distance = dist_min + chosen * (dist_max - dist_min)
            vertical_fov = fov_min + chosen * (fov_max - fov_min)
            if not math.isclose(
                distance * vertical_fov,
                safe_dxf,
                rel_tol=0.0,
                abs_tol=0.02,
            ):
                raise RuntimeError(
                    f"Camera interpolation failed at dxf {safe_dxf}"
                )
            return {
                "distance": distance,
                "vertical_fov": vertical_fov,
                "lower_id": lower["id"],
                "upper_id": upper["id"],
                "interpolation_t": chosen,
            }
    raise RuntimeError(f"No CameraParam interval covers dxf {safe_dxf}")


def derive_lod_levels(world, tile_size):
    keys = sorted(world["camera_keys"], key=lambda item: float(item["dxf"]))
    forwards = {
        tuple(
            canonical_number(item["unity_forward"][axis], digits=7)
            for axis in ("x", "y", "z")
        )
        for item in keys
    }
    if len(forwards) != 1:
        raise RuntimeError(
            f"CameraParam source forward is not stable: {sorted(forwards)}"
        )
    forward = normalize(list(next(iter(forwards))))
    world_up = [0.0, 1.0, 0.0]
    right = normalize(cross(world_up, forward))
    camera_up = normalize(cross(forward, right))
    levels = []
    evidence = []
    previous_span = 1
    for lod, threshold in enumerate(LOD_THRESHOLDS):
        reachable_dxf = min(threshold, REACHABLE_MAX_DXF)
        camera = interpolate_camera(keys, reachable_dxf)
        distance = camera["distance"]
        vertical_fov = camera["vertical_fov"]
        vertical_tangent = math.tan(math.radians(vertical_fov) * 0.5)
        horizontal_tangent = vertical_tangent * ASPECT_RATIO
        origin = [-forward[index] * distance for index in range(3)]
        intersections = []
        for screen_y in (-1.0, 1.0):
            for screen_x in (-1.0, 1.0):
                ray = [
                    forward[index]
                    + right[index] * screen_x * horizontal_tangent
                    + camera_up[index] * screen_y * vertical_tangent
                    for index in range(3)
                ]
                if ray[1] >= -TRANSFORM_EPSILON:
                    raise RuntimeError(
                        f"LOD{lod} frustum corner does not hit ground"
                    )
                distance_to_ground = -origin[1] / ray[1]
                point = [
                    origin[index] + ray[index] * distance_to_ground
                    for index in range(3)
                ]
                intersections.append(
                    {
                        "ScreenX": screen_x,
                        "ScreenY": screen_y,
                        "UnityGroundX": canonical_number(point[0]),
                        "UnityGroundZ": canonical_number(point[2]),
                    }
                )
        max_abs_x = max(abs(item["UnityGroundX"]) for item in intersections)
        max_abs_z = max(abs(item["UnityGroundZ"]) for item in intersections)
        max_extent = max(max_abs_x, max_abs_z)
        footprint_tiles = max_extent / tile_size
        minimum_span = int(math.ceil(footprint_tiles - 1.0e-12)) + 1
        chunk_span = max(previous_span, minimum_span)
        previous_span = chunk_span
        levels.append(
            {
                "Lod": lod,
                "MaxDxfInclusive": threshold,
                "ChunkTileSpan": chunk_span,
            }
        )
        evidence.append(
            {
                "Lod": lod,
                "TierMaxDxf": threshold,
                "ReachableDxf": reachable_dxf,
                "CameraParamLowerId": camera["lower_id"],
                "CameraParamUpperId": camera["upper_id"],
                "CameraInterpolationT": canonical_number(
                    camera["interpolation_t"]
                ),
                "DistanceUnityUnits": canonical_number(distance),
                "VerticalFovDegrees": canonical_number(vertical_fov),
                "SourceForward": {
                    "X": canonical_number(forward[0]),
                    "Y": canonical_number(forward[1]),
                    "Z": canonical_number(forward[2]),
                },
                "AspectRatio": ASPECT_RATIO,
                "GroundIntersections": intersections,
                "MaxAbsUnityX": canonical_number(max_abs_x),
                "MaxAbsUnityZ": canonical_number(max_abs_z),
                "FootprintExtentTilesBeforeMargin": canonical_number(
                    footprint_tiles
                ),
                "SafetyMarginTiles": 1,
                "MinimumSpanForWorstCenterChunkEdge": minimum_span,
                "ChunkTileSpan": chunk_span,
                "FixedWindowChunksPerAxis": 3,
                "CoveragePass": chunk_span + 1.0e-9
                >= footprint_tiles + 1.0,
            }
        )
    return levels, evidence


def parse_sprite_meta(meta_path):
    text = meta_path.read_text(encoding="utf-8", errors="strict")
    pixels_match = re.search(r"(?m)^\s*spritePixelsToUnits:\s*(\S+)", text)
    if not pixels_match:
        raise RuntimeError(f"{rel(meta_path)} lacks spritePixelsToUnits")
    pixels_per_unit = float(pixels_match.group(1))
    result = {}
    marker = "\n    - serializedVersion: 2\n"
    for block in text.split(marker)[1:]:
        name_match = re.search(r"(?m)^\s*name:\s*(.+?)\s*$", block)
        internal_match = re.search(
            r"(?m)^\s*internalID:\s*(-?\d+)\s*$",
            block,
        )
        rect_match = re.search(
            r"(?ms)^\s*rect:\s*\n"
            r"\s*serializedVersion:\s*\d+\s*\n"
            r"\s*x:\s*([^\s]+)\s*\n"
            r"\s*y:\s*([^\s]+)\s*\n"
            r"\s*width:\s*([^\s]+)\s*\n"
            r"\s*height:\s*([^\s]+)",
            block,
        )
        pivot_match = re.search(
            r"(?m)^\s*pivot:\s*\{x:\s*([^,]+),\s*y:\s*([^}]+)\}",
            block,
        )
        if not (name_match and internal_match and rect_match and pivot_match):
            continue
        result[internal_match.group(1)] = {
            "Name": name_match.group(1).strip(),
            "RectPixels": {
                "X": canonical_number(rect_match.group(1)),
                "Y": canonical_number(rect_match.group(2)),
                "Width": canonical_number(rect_match.group(3)),
                "Height": canonical_number(rect_match.group(4)),
            },
            "Pivot": {
                "X": canonical_number(pivot_match.group(1)),
                "Y": canonical_number(pivot_match.group(2)),
            },
            "PixelsPerUnit": canonical_number(pixels_per_unit),
        }
    return result


def build_asset_indices(near_assets, near_ue, streaming_assets):
    near_mesh_paths = set(near_ue["mesh_assets"])
    near_material_paths = set(near_ue["material_assets"])
    ground_by_id = {
        item["prefab_id"]: item
        for item in near_assets["baked_ground_materials"]
    }
    object_by_id = dict(near_assets["object_sources"])

    stream_mesh = {
        (
            item["identity"]["source_path"],
            str(item["identity"]["source_file_id"]),
        ): item
        for item in streaming_assets["meshes"]
    }
    stream_material = {
        (
            item["identity"]["source_path"],
            str(item["identity"]["source_file_id"]),
        ): item
        for item in streaming_assets["materials"]
    }
    stream_sprite = {}
    for texture in streaming_assets["sprite_textures"]:
        source_path = texture["identity"]["source_path"]
        meta_records = parse_sprite_meta(
            PROJECT_ROOT / f"{source_path}.meta"
        )
        for subasset in texture["referenced_sprite_subassets"]:
            file_id = str(subasset["source_file_id"])
            if file_id not in meta_records:
                raise RuntimeError(
                    f"{source_path} sprite {file_id} lacks rect/pivot"
                )
            manifest_meta = subasset["meta"]
            parsed_meta = meta_records[file_id]
            if (
                manifest_meta["name"] != parsed_meta["Name"]
                or manifest_meta["rect"]
                != {
                    key.lower(): value
                    for key, value in parsed_meta["RectPixels"].items()
                }
                or manifest_meta["pivot"]
                != {
                    key.lower(): value
                    for key, value in parsed_meta["Pivot"].items()
                }
            ):
                raise RuntimeError(
                    f"{source_path} sprite {file_id} manifest metadata "
                    "does not match the Unity TextureImporter meta"
                )
            if subasset.get("binding_status") != "ready":
                raise RuntimeError(
                    f"{source_path} sprite {file_id} is not ready"
                )
            stream_sprite[(source_path, file_id)] = {
                "Texture": texture,
                "Subasset": subasset,
                "Metadata": parsed_meta,
                "Mesh": subasset["ue_mesh_soft_object_path"],
                "Material": subasset["ue_material_soft_object_path"],
            }
    return {
        "NearMeshPaths": near_mesh_paths,
        "NearMaterialPaths": near_material_paths,
        "GroundById": ground_by_id,
        "ObjectById": object_by_id,
        "StreamMesh": stream_mesh,
        "StreamMaterial": stream_material,
        "StreamSprite": stream_sprite,
    }


def near_material_path(source_material, near_material_paths):
    stem = Path(source_material).stem
    candidate = (
        "/Game/RokUnityWorldMapPrototype/Materials/"
        f"M_WorldSource_{stem}.M_WorldSource_{stem}"
    )
    return candidate if candidate in near_material_paths else ""


def ground_material_path(prefab_id, near_material_paths):
    candidate = (
        "/Game/RokUnityWorldMapPrototype/Materials/"
        f"M_WorldGround_{prefab_id}.M_WorldGround_{prefab_id}"
    )
    return candidate if candidate in near_material_paths else ""


def near_ground_sort_priority(ground):
    contract = ground.get("render_contract")
    if not isinstance(contract, dict) or (
        contract.get("unity_queue") != "Geometry-5"
        or contract.get("unity_zwrite") != "Off"
        or contract.get("ue_blend_mode") != "translucent"
        or type(contract.get("ue_translucency_sort_priority")) is not int
        or contract["ue_translucency_sort_priority"] != -5
    ):
        raise RuntimeError(
            f"{ground['prefab_id']} has no valid Unity ground-before-tree render contract"
        )
    return contract["ue_translucency_sort_priority"]


def compile_detailed_recipes(runtime, indices):
    recipes = []
    evidence = []
    counters = Counter()
    for template in sorted(
        runtime["detailed_templates"],
        key=lambda item: item["tile_type_id"],
    ):
        for lod in (0, 1):
            recipe_id = f"{template['tile_type_id']}:lod{lod}"
            parts = []
            for instance in template["instances"]:
                prefab_id = instance["prefab_id"]
                source = indices["ObjectById"].get(prefab_id)
                binding_status = "missing"
                mesh_path = ""
                materials = []
                source_identity = {
                    "PrefabId": prefab_id,
                    "Kind": instance["kind"],
                }
                b_tree = False
                sort_priority = 0
                if (
                    source is not None
                    and source.get("representation")
                    == "exact_source_fbx_render_layers"
                ):
                    contract = source.get("river_geometry_contract", {})
                    layers = contract.get("render_layers", [])
                    if [layer.get("role") for layer in layers] != [
                        "edge",
                        "water",
                    ]:
                        raise RuntimeError(
                            f"{prefab_id} river render layers are not "
                            "ordered edge then water"
                        )
                    for layer in layers:
                        role = layer["role"]
                        asset_name = (
                            f"SM_RokWorldRiver_{prefab_id}_{role.title()}"
                        )
                        mesh_candidate = (
                            "/Game/RokUnityWorldMapPrototype/Meshes/"
                            f"RiverLayers/{asset_name}.{asset_name}"
                        )
                        material_name = (
                            "M_WorldRiver_Edge"
                            if role == "edge"
                            else "M_WorldRiver_Water"
                        )
                        material_candidate = (
                            "/Game/RokUnityWorldMapPrototype/Materials/"
                            f"{material_name}.{material_name}"
                        )
                        layer_mesh = ""
                        layer_materials = []
                        layer_status = "missing"
                        if (
                            mesh_candidate in indices["NearMeshPaths"]
                            and material_candidate
                            in indices["NearMaterialPaths"]
                        ):
                            layer_mesh = mesh_candidate
                            layer_materials = [material_candidate]
                            layer_status = (
                                "exact_near_source_fbx_render_layer"
                            )
                        local = layer["transform"]
                        layer_matrix = matrix_from_trs(
                            local["local_position"],
                            local["local_rotation"],
                            local["local_scale"],
                        )
                        raw_matrix = matrix_multiply(
                            instance["runtime_local_matrix"],
                            layer_matrix,
                        )
                        raw_determinant = determinant3(raw_matrix)
                        converted = basis_convert_matrix(raw_matrix, 500.0)
                        transform_error = ""
                        output_transform = None
                        try:
                            output_transform = transform_from_matrix(
                                converted,
                                mesh_scale=5.0,
                            )
                        except ValueError as error:
                            transform_error = str(error)
                        layer_identity = {
                            **source_identity,
                            "Representation": source["representation"],
                            "SourcePrefab": source.get(
                                "source_prefab",
                                "",
                            ),
                            "SourceMesh": source.get(
                                "source_mesh",
                                {},
                            ).get("path", ""),
                            "SourceLayerRole": role,
                            "SourceMeshFileId": str(
                                layer["mesh_file_id"]
                            ),
                            "SourceRendererFileId": str(
                                layer["mesh_renderer_file_id"]
                            ),
                            "SourceMaterial": layer[
                                "source_material"
                            ]["path"],
                        }
                        evidence_record = {
                            "RecipeId": recipe_id,
                            "SourceKind": "detailed_instance",
                            "SourceIndex": instance["index"],
                            "SourceIdentity": layer_identity,
                            "SourceLocalMatrix": raw_matrix,
                            "RawSourceDeterminant": canonical_number(
                                raw_determinant
                            ),
                            "BindingStatus": layer_status,
                            "Mesh": layer_mesh,
                            "Materials": layer_materials,
                            "SourceSort": {
                                "SortingLayerId": 0,
                                "SortingOrder": int(
                                    layer["sort_priority"]
                                ),
                                "RendererPriority": 0,
                            },
                            "bTree": False,
                            "RenderFlags": 0,
                        }
                        if transform_error:
                            evidence_record[
                                "TransformRejection"
                            ] = transform_error
                            evidence.append(evidence_record)
                            counters[
                                f"Lod{lod}RejectedNegativeOrInvalidTransform"
                            ] += 1
                            continue
                        parts.append(
                            {
                                "Mesh": layer_mesh,
                                "Materials": layer_materials,
                                "LocalTransform": output_transform,
                                "TranslucencySortPriority": int(
                                    layer["sort_priority"]
                                ),
                                "bTree": False,
                                "RenderFlags": 0,
                            }
                        )
                        evidence_record["PartIndex"] = len(parts) - 1
                        evidence.append(evidence_record)
                        counters[f"Lod{lod}CompiledParts"] += 1
                        if layer_mesh and layer_materials:
                            counters[f"Lod{lod}ExactBindings"] += 1
                        else:
                            counters[f"Lod{lod}MissingBindings"] += 1
                    continue
                if (
                    source is not None
                    and source.get("representation")
                    == "exact_source_fbx_composite_waterfall"
                ):
                    contract = source.get("waterfall_geometry_contract", {})
                    static_parts = contract.get("static_parts", [])
                    if len(static_parts) != 6:
                        raise RuntimeError(
                            f"{prefab_id} waterfall static-part contract "
                            f"expected 6 parts, found {len(static_parts)}"
                        )
                    for part in static_parts:
                        mesh_candidate = part["ue_mesh"]
                        material_candidate = part["ue_material"]
                        part_mesh = ""
                        part_materials = []
                        part_status = "missing"
                        if (
                            mesh_candidate in indices["NearMeshPaths"]
                            and material_candidate
                            in indices["NearMaterialPaths"]
                        ):
                            part_mesh = mesh_candidate
                            part_materials = [material_candidate]
                            part_status = (
                                "exact_near_source_fbx_composite_part"
                            )
                        raw_matrix = matrix_multiply(
                            instance["runtime_local_matrix"],
                            part["local_matrix"],
                        )
                        raw_determinant = determinant3(raw_matrix)
                        converted = basis_convert_matrix(raw_matrix, 500.0)
                        transform_error = ""
                        output_transform = None
                        try:
                            output_transform = transform_from_matrix(
                                converted,
                                mesh_scale=5.0,
                            )
                        except ValueError as error:
                            transform_error = str(error)
                        part_identity = {
                            **source_identity,
                            "Representation": source["representation"],
                            "SourcePrefab": source.get("source_prefab", ""),
                            "SourcePart": part["game_object"],
                            "SourceMeshNode": part["mesh_node"],
                            "SourceMeshFileId": str(part["mesh_file_id"]),
                            "SourceMaterial": part["material_path"],
                        }
                        evidence_record = {
                            "RecipeId": recipe_id,
                            "SourceKind": "detailed_instance",
                            "SourceIndex": instance["index"],
                            "SourceIdentity": part_identity,
                            "SourceLocalMatrix": raw_matrix,
                            "RawSourceDeterminant": canonical_number(
                                raw_determinant
                            ),
                            "BindingStatus": part_status,
                            "Mesh": part_mesh,
                            "Materials": part_materials,
                            "SourceSort": {
                                "SortingLayerId": 0,
                                "SortingOrder": int(
                                    part["sort_priority"]
                                ),
                                "RendererPriority": 0,
                            },
                            "bTree": False,
                            "RenderFlags": 0,
                        }
                        if transform_error:
                            evidence_record[
                                "TransformRejection"
                            ] = transform_error
                            evidence.append(evidence_record)
                            counters[
                                f"Lod{lod}RejectedNegativeOrInvalidTransform"
                            ] += 1
                            continue
                        parts.append(
                            {
                                "Mesh": part_mesh,
                                "Materials": part_materials,
                                "LocalTransform": output_transform,
                                "TranslucencySortPriority": int(
                                    part["sort_priority"]
                                ),
                                "bTree": False,
                                "RenderFlags": 0,
                            }
                        )
                        evidence_record["PartIndex"] = len(parts) - 1
                        evidence.append(evidence_record)
                        counters[f"Lod{lod}CompiledParts"] += 1
                        if part_mesh and part_materials:
                            counters[f"Lod{lod}ExactBindings"] += 1
                        else:
                            counters[f"Lod{lod}MissingBindings"] += 1
                    continue
                if prefab_id in indices["GroundById"]:
                    ground = indices["GroundById"][prefab_id]
                    sort_priority = near_ground_sort_priority(ground)
                    mesh_candidate = (
                        "/Game/RokUnityWorldMapPrototype/Meshes/"
                        "SM_RokWorld_Plane60.SM_RokWorld_Plane60"
                    )
                    material_candidate = ground_material_path(
                        prefab_id,
                        indices["NearMaterialPaths"],
                    )
                    if (
                        mesh_candidate in indices["NearMeshPaths"]
                        and material_candidate
                    ):
                        mesh_path = mesh_candidate
                        materials = [material_candidate]
                        binding_status = "exact_near_ground"
                    source_identity["SourceMaterial"] = ground[
                        "source_material"
                    ]["path"]
                elif source is not None:
                    representation = source.get("representation", "")
                    source_identity.update(
                        {
                            "Representation": representation,
                            "SourcePrefab": source.get("source_prefab", ""),
                            "SourceMesh": source.get(
                                "source_mesh",
                                {},
                            ).get("path", ""),
                        }
                    )
                    if representation == "empty_source_prefab":
                        counters[f"Lod{lod}SourceExplicitNoOp"] += 1
                        evidence.append(
                            {
                                "RecipeId": recipe_id,
                                "SourceKind": "detailed_instance",
                                "SourceIndex": instance["index"],
                                "SourceIdentity": source_identity,
                                "BindingStatus": "source_explicit_noop",
                            }
                        )
                        continue
                    folder = source.get(
                        "ue_mesh_folder",
                        "/Game/RokUnityWorldMapPrototype/Meshes",
                    )
                    ue_name = source.get("ue_mesh_name", "")
                    mesh_candidate = (
                        f"{folder}/{ue_name}.{ue_name}" if ue_name else ""
                    )
                    source_material = source.get("source_material", "")
                    if not source_material and representation == "exact_source_fbx":
                        source_material = source.get(
                            "mountain_geometry_contract",
                            {},
                        ).get("body_render_node", {}).get(
                            "material_path",
                            "",
                        )
                    material_candidate = near_material_path(
                        source_material,
                        indices["NearMaterialPaths"],
                    )
                    b_tree = Path(source_material).name in {
                        "map_tree_nomask.mat",
                        "map_tree_nomask_1.mat",
                    }
                    if (
                        mesh_candidate in indices["NearMeshPaths"]
                        and material_candidate
                    ):
                        mesh_path = mesh_candidate
                        materials = [material_candidate]
                        binding_status = f"exact_near_{representation}"
                    source_identity["SourceMaterial"] = source_material
                raw_matrix = instance["runtime_local_matrix"]
                raw_determinant = determinant3(raw_matrix)
                converted = basis_convert_matrix(raw_matrix, 500.0)
                output_transform = None
                transform_error = ""
                try:
                    output_transform = transform_from_matrix(
                        converted,
                        mesh_scale=5.0,
                    )
                except ValueError as error:
                    transform_error = str(error)
                evidence_record = {
                    "RecipeId": recipe_id,
                    "SourceKind": "detailed_instance",
                    "SourceIndex": instance["index"],
                    "SourceIdentity": source_identity,
                    "SourceLocalMatrix": raw_matrix,
                    "RawSourceDeterminant": canonical_number(raw_determinant),
                    "BindingStatus": binding_status,
                    "Mesh": mesh_path,
                    "Materials": materials,
                    "SourceSort": {
                        "SortingLayerId": 0,
                        "SortingOrder": sort_priority,
                        "RendererPriority": 0,
                    },
                    "bTree": b_tree,
                    "RenderFlags": 0,
                }
                if transform_error:
                    evidence_record["TransformRejection"] = transform_error
                    evidence.append(evidence_record)
                    counters[f"Lod{lod}RejectedNegativeOrInvalidTransform"] += 1
                    continue
                parts.append(
                    {
                        "Mesh": mesh_path,
                        "Materials": materials,
                        "LocalTransform": output_transform,
                        "TranslucencySortPriority": sort_priority,
                        "bTree": b_tree,
                        "RenderFlags": 0,
                    }
                )
                evidence_record["PartIndex"] = len(parts) - 1
                evidence.append(evidence_record)
                counters[f"Lod{lod}CompiledParts"] += 1
                if mesh_path and materials:
                    counters[f"Lod{lod}ExactBindings"] += 1
                else:
                    counters[f"Lod{lod}MissingBindings"] += 1
            recipes.append(
                {
                    "RecipeId": recipe_id,
                    "TileTypeId": template["tile_type_id"],
                    "Lod": lod,
                    "Parts": parts,
                }
            )
    return recipes, evidence, counters


def iter_composed_nodes(root):
    stack = [(root, identity_matrix())]
    while stack:
        node, parent_matrix = stack.pop()
        if node is root:
            composed = identity_matrix()
        else:
            composed = matrix_multiply(
                parent_matrix,
                node["transform"]["local_matrix"],
            )
        yield node, composed
        for child in reversed(node.get("children", [])):
            stack.append((child, composed))


def reference_identity(reference):
    return {
        "SourcePath": reference.get("asset_path", ""),
        "SourceFileId": str(reference.get("file_id", "")),
        "SourceGuid": reference.get("guid", ""),
    }


def compile_far_recipes(runtime, indices):
    recipes = []
    evidence = []
    counters = Counter()
    for source_recipe in sorted(
        runtime["far_lod_recipes"],
        key=lambda item: (item["tile_type_id"], int(item["lod"])),
    ):
        lod = int(source_recipe["lod"])
        parts = []
        for node, composed_unity in iter_composed_nodes(source_recipe["root"]):
            components = defaultdict(list)
            for component in node.get("components", []):
                components[component["type"]].append(component)
            mesh_filters = components["MeshFilter"]
            mesh_renderers = components["MeshRenderer"]
            sprite_renderers = components["SpriteRenderer"]
            for renderer in mesh_renderers:
                if not node["active_in_hierarchy"] or int(renderer["enabled"]) != 1:
                    continue
                if len(mesh_filters) != 1 or len(mesh_renderers) != 1:
                    raise RuntimeError(
                        f"{source_recipe['recipe_id']} {node['node_path']} "
                        "does not have one MeshFilter/one MeshRenderer"
                    )
                mesh_ref = mesh_filters[0]["mesh"]
                mesh_binding = indices["StreamMesh"].get(
                    (mesh_ref["asset_path"], str(mesh_ref["file_id"]))
                )
                mesh_path = (
                    mesh_binding["ue_soft_object_path"]
                    if mesh_binding
                    else ""
                )
                materials = []
                material_identities = []
                for material_ref in renderer.get("materials", []):
                    material_identities.append(
                        reference_identity(material_ref)
                    )
                    material_binding = indices["StreamMaterial"].get(
                        (
                            material_ref["asset_path"],
                            str(material_ref["file_id"]),
                        )
                    )
                    materials.append(
                        material_binding["ue_soft_object_path"]
                        if material_binding
                        else ""
                    )
                material_names = {
                    Path(item["SourcePath"]).name
                    for item in material_identities
                }
                b_tree = bool(
                    material_names
                    & {"map_tree_nomask.mat", "map_tree_nomask_1.mat"}
                )
                compile_far_part(
                    source_recipe,
                    node,
                    composed_unity,
                    renderer,
                    "MeshRenderer",
                    mesh_path,
                    materials,
                    reference_identity(mesh_ref),
                    material_identities,
                    b_tree,
                    None,
                    parts,
                    evidence,
                    counters,
                )
            for renderer in sprite_renderers:
                if not node["active_in_hierarchy"] or int(renderer["enabled"]) != 1:
                    continue
                sprite_ref = renderer["sprite"]
                if renderer.get("empty_asset") or sprite_ref.get("is_null"):
                    counters[f"Lod{lod}NullSpriteNoOps"] += 1
                    evidence.append(
                        {
                            "RecipeId": source_recipe["recipe_id"],
                            "SourceKind": "far_renderer",
                            "RendererType": "SpriteRenderer",
                            "RendererFileId": str(renderer["file_id"]),
                            "NodePath": node["node_path"],
                            "BindingStatus": "source_explicit_null_sprite_noop",
                            "SourceIdentity": reference_identity(sprite_ref),
                        }
                    )
                    continue
                sprite_binding = indices["StreamSprite"].get(
                    (sprite_ref["asset_path"], str(sprite_ref["file_id"]))
                )
                if sprite_binding is None:
                    raise RuntimeError(
                        f"Missing sprite identity for {node['node_path']}"
                    )
                mesh_path = sprite_binding.get("Mesh", "")
                materials = []
                material_identities = []
                for material_ref in renderer.get("materials", []):
                    material_identities.append(
                        reference_identity(material_ref)
                    )
                    material_binding = indices["StreamMaterial"].get(
                        (
                            material_ref["asset_path"],
                            str(material_ref["file_id"]),
                        )
                    )
                    materials.append(
                        material_binding["ue_soft_object_path"]
                        if material_binding
                        else ""
                    )
                expected_sprite_material = sprite_binding["Material"]
                if len(materials) != 1 or materials[0] != expected_sprite_material:
                    raise RuntimeError(
                        f"{source_recipe['recipe_id']} {node['node_path']} "
                        "sprite material order/identity does not match the "
                        "generated quad binding"
                    )
                materials = [expected_sprite_material]
                b_tree = (
                    Path(sprite_ref["asset_path"]).name == "map_tree_atlas.png"
                )
                sprite_evidence = {
                    **reference_identity(sprite_ref),
                    "TextureTarget": sprite_binding["Texture"][
                        "ue_soft_object_path"
                    ],
                    "SubassetName": sprite_binding["Subasset"]["name"],
                    **sprite_binding["Metadata"],
                    "Color": renderer.get("color"),
                    "FlipX": int(renderer.get("flip_x", 0)),
                    "FlipY": int(renderer.get("flip_y", 0)),
                    "DrawMode": int(renderer.get("draw_mode", 0)),
                    "SourceSize": renderer.get("size"),
                }
                compile_far_part(
                    source_recipe,
                    node,
                    composed_unity,
                    renderer,
                    "SpriteRenderer",
                    mesh_path,
                    materials,
                    reference_identity(sprite_ref),
                    material_identities,
                    b_tree,
                    sprite_evidence,
                    parts,
                    evidence,
                    counters,
                )
        recipes.append(
            {
                "RecipeId": source_recipe["recipe_id"],
                "TileTypeId": source_recipe["tile_type_id"],
                "Lod": lod,
                "Parts": parts,
            }
        )
    return recipes, evidence, counters


def compile_far_part(
    source_recipe,
    node,
    composed_unity,
    renderer,
    renderer_type,
    mesh_path,
    materials,
    source_asset_identity,
    material_identities,
    b_tree,
    sprite_evidence,
    parts,
    evidence,
    counters,
):
    lod = int(source_recipe["lod"])
    source_determinant = determinant3(composed_unity)
    converted = basis_convert_matrix(composed_unity, 500.0)
    transform_error = ""
    output_transform = None
    try:
        output_transform = transform_from_matrix(
            converted,
            mesh_scale=5.0,
        )
    except ValueError as error:
        transform_error = str(error)
    render_flags = 1 if int(renderer.get("cast_shadows", 0)) == 1 else 0
    binding_status = (
        "exact_streaming_static_binding"
        if mesh_path and materials and all(materials)
        else (
            "sprite_static_mesh_uv_binding_missing"
            if renderer_type == "SpriteRenderer"
            else "streaming_mesh_or_material_binding_missing"
        )
    )
    evidence_record = {
        "RecipeId": source_recipe["recipe_id"],
        "SourceKind": "far_renderer",
        "RendererType": renderer_type,
        "RendererFileId": str(renderer["file_id"]),
        "NodePath": node["node_path"],
        "SourceAssetIdentity": source_asset_identity,
        "OrderedMaterialIdentities": material_identities,
        "ComposedSourceMatrixRootExcluded": composed_unity,
        "RawSourceDeterminant": canonical_number(source_determinant),
        "BindingStatus": binding_status,
        "Mesh": mesh_path,
        "Materials": materials,
        "SourceSort": {
            "SortingLayerId": int(renderer.get("sorting_layer_id", 0)),
            "SortingOrder": int(renderer.get("sorting_order", 0)),
            "RendererPriority": int(renderer.get("renderer_priority", 0)),
            "RenderingLayerMask": int(
                renderer.get("rendering_layer_mask", 0)
            ),
        },
        "SourceRender": {
            "CastShadows": int(renderer.get("cast_shadows", 0)),
            "ReceiveShadows": int(renderer.get("receive_shadows", 0)),
        },
        "bTree": b_tree,
        "RenderFlags": render_flags,
    }
    if sprite_evidence is not None:
        evidence_record["Sprite"] = sprite_evidence
    if transform_error:
        evidence_record["TransformRejection"] = transform_error
        evidence.append(evidence_record)
        counters[f"Lod{lod}RejectedNegativeOrInvalidTransform"] += 1
        return
    parts.append(
        {
            "Mesh": mesh_path,
            "Materials": materials,
            "LocalTransform": output_transform,
            "TranslucencySortPriority": int(
                renderer.get("sorting_order", 0)
            ),
            "bTree": b_tree,
            "RenderFlags": render_flags,
        }
    )
    evidence_record["PartIndex"] = len(parts) - 1
    evidence.append(evidence_record)
    counters[f"Lod{lod}CompiledParts"] += 1
    if mesh_path and materials and all(materials):
        counters[f"Lod{lod}ExactBindings"] += 1
    else:
        counters[f"Lod{lod}MissingBindings"] += 1
    counters[f"Lod{lod}{renderer_type}Parts"] += 1


def compile_tiles(runtime, projection):
    result = []
    origin = (
        projection["UnityRebaseOrigin"]["X"],
        0.0,
        projection["UnityRebaseOrigin"]["Y"],
    )
    for source in sorted(
        runtime["map"]["tiles"],
        key=lambda item: int(item["index"]),
    ):
        converted = basis_convert_matrix(
            source["template_to_world_matrix"],
            projection["UeUnitsPerUnityUnit"],
            translation_offset=(origin[0], origin[2], origin[1]),
        )
        transform = transform_from_matrix(converted)
        result.append(
            {
                "TileX": int(source["x"]),
                "TileY": int(source["y"]),
                "TileTypeId": source["tile_id"],
                "TemplateToWorld": transform,
            }
        )
    return result


def build_projection(world):
    coordinate = world["coordinate_contract"]
    focus = world["prototype_window"]["focus"]["unity_coordinate"]
    return {
        "UnityBoundsMin": {"X": 0.0, "Y": 0.0},
        "UnityBoundsMax": {
            "X": float(world["map"]["world_width_unity_units"]),
            "Y": float(world["map"]["world_height_unity_units"]),
        },
        "UnityRebaseOrigin": {
            "X": float(focus["x"]),
            "Y": float(focus["y"]),
        },
        "UeUnitsPerUnityUnit": float(
            coordinate["ue_units_per_unity_unit"]
        ),
        "GroundZ": 0.0,
        "DisplayCoordinateScale": float(
            coordinate["display_to_unity_scale"]
        ),
    }


def build_tile_types(runtime):
    result = []
    for item in sorted(runtime["tile_types"], key=lambda row: row["tile_type_id"]):
        tile_type_id = item["tile_type_id"]
        result.append(
            {
                "TileTypeId": tile_type_id,
                "LodBindings": [
                    {
                        "Lod": lod,
                        "RecipeId": f"{tile_type_id}:lod{lod}",
                    }
                    for lod in range(6)
                ],
            }
        )
    return result


def build_readiness(counters, streaming_assets):
    missing_by_lod = {
        str(lod): int(counters.get(f"Lod{lod}MissingBindings", 0))
        for lod in range(6)
    }
    rejected_by_lod = {
        str(lod): int(
            counters.get(f"Lod{lod}RejectedNegativeOrInvalidTransform", 0)
        )
        for lod in range(6)
    }
    blockers = [
        "particle_systems_pending_niagara_or_omission_policy",
        "mesh_colliders_pending_runtime_collision_policy",
    ]
    for lod in range(6):
        if missing_by_lod[str(lod)]:
            blockers.append(
                f"lod{lod}_static_asset_bindings_missing:"
                f"{missing_by_lod[str(lod)]}"
            )
        if rejected_by_lod[str(lod)]:
            blockers.append(
                f"lod{lod}_negative_or_invalid_instance_transforms_rejected:"
                f"{rejected_by_lod[str(lod)]}"
            )
    b_static = not any(missing_by_lod.values()) and not any(
        rejected_by_lod.values()
    )
    blocked = streaming_assets["blocked_policies"]
    pending = [
        {
            "PolicyId": "ParticleSystems",
            "Status": "Pending",
            "SourceComponentCount": int(
                blocked["particle_systems"]["count"]
            ),
            "RequiredDecision": blocked["particle_systems"]["policy"],
        },
        {
            "PolicyId": "MeshColliders",
            "Status": "Pending",
            "SourceComponentCount": int(
                blocked["mesh_colliders"]["count"]
            ),
            "RequiredDecision": blocked["mesh_colliders"]["policy"],
        },
        {
            "PolicyId": "NullSprites",
            "Status": "SourceExplicitNoOp",
            "SourceComponentCount": int(blocked["null_sprites"]["count"]),
            "RequiredDecision": blocked["null_sprites"]["policy"],
        },
    ]
    return b_static, blockers, pending, missing_by_lod, rejected_by_lod


def build_markdown(data, evidence):
    counts = evidence["Counts"]
    lines = [
        "# Unity World Map Streaming Data",
        "",
        f"- Static visual ready: `{data['bStaticVisualReady']}`",
        f"- Runtime ready: `{data['bRuntimeReady']}`",
        f"- Tiles: {len(data['Tiles'])}",
        f"- Tile types: {len(data['TileTypes'])}",
        f"- Recipes: {len(data['Recipes'])}",
        f"- Compiled parts: {counts['CompiledPartCount']}",
        f"- Source detailed instances: {counts['DetailedSourceInstanceCount']}",
        f"- Source far static renderers: {counts['FarStaticRendererCount']}",
        "",
        "## LOD coverage",
        "",
        "| LOD | Chunk span | Parts | Exact bindings | Missing | Rejected transform |",
        "| ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for level in data["LodLevels"]:
        lod = level["Lod"]
        lines.append(
            f"| {lod} | {level['ChunkTileSpan']} | "
            f"{counts['ByLod'][str(lod)]['CompiledParts']} | "
            f"{counts['ByLod'][str(lod)]['ExactBindings']} | "
            f"{counts['ByLod'][str(lod)]['MissingBindings']} | "
            f"{counts['ByLod'][str(lod)]['RejectedTransforms']} |"
        )
    lines.extend(
        [
            "",
            "## Readiness blockers",
            "",
        ]
    )
    lines.extend(f"- `{item}`" for item in data["ReadinessBlockers"])
    lines.extend(
        [
            "",
            "The main JSON exactly follows the reflected UE data-asset "
            "contract. Source identities, sprite rect/pivot evidence, "
            "material order, render state, and rejected transforms are kept "
            "in the sibling compile-evidence JSON.",
            "",
        ]
    )
    return "\n".join(lines)


def main():
    world = load_json(WORLD_PATH, "rok.unity_world_map.v1")
    runtime = load_json(RUNTIME_PATH, EXPECTED_RUNTIME_SCHEMA)
    near_assets = load_json(NEAR_ASSETS_PATH, EXPECTED_NEAR_SCHEMA)
    near_ue = load_json(NEAR_UE_PATH, EXPECTED_NEAR_UE_SCHEMA)
    streaming_assets = load_json(
        STREAMING_ASSETS_PATH,
        EXPECTED_STREAMING_SCHEMA,
    )
    if runtime["statistics"]["map_tile_count"] != 1600:
        raise RuntimeError("Runtime source no longer contains 1600 tiles")
    if runtime["statistics"]["tile_type_count"] != 66:
        raise RuntimeError("Runtime source no longer contains 66 tile types")

    indices = build_asset_indices(near_assets, near_ue, streaming_assets)
    projection = build_projection(world)
    tile_size = float(runtime["map"]["tile_size_unity_units"])
    lod_levels, footprint_evidence = derive_lod_levels(world, tile_size)
    detailed_recipes, detailed_evidence, detailed_counts = (
        compile_detailed_recipes(runtime, indices)
    )
    far_recipes, far_evidence, far_counts = compile_far_recipes(
        runtime,
        indices,
    )
    counters = detailed_counts + far_counts
    recipes = sorted(
        detailed_recipes + far_recipes,
        key=lambda item: (item["TileTypeId"], item["Lod"]),
    )
    b_static, blockers, pending, missing, rejected = build_readiness(
        counters,
        streaming_assets,
    )
    data = {
        "Schema": SCHEMA,
        "CanonicalPayloadSha256": "",
        "SourceManifestPath": rel(RUNTIME_PATH),
        "SourceManifestSha256": sha256(RUNTIME_PATH),
        "Projection": projection,
        "MapWidthTiles": int(runtime["map"]["width_tiles"]),
        "MapHeightTiles": int(runtime["map"]["height_tiles"]),
        "TileSizeUnityUnits": tile_size,
        "LodLevels": lod_levels,
        "Tiles": compile_tiles(runtime, projection),
        "TileTypes": build_tile_types(runtime),
        "Recipes": recipes,
        "bStaticVisualReady": b_static,
        "bRuntimeReady": False,
        "ReadinessBlockers": blockers,
        "PendingPolicies": pending,
    }
    data["CanonicalPayloadSha256"] = canonical_payload_hash(data)

    by_lod = {}
    for lod in range(6):
        by_lod[str(lod)] = {
            "CompiledParts": int(counters.get(f"Lod{lod}CompiledParts", 0)),
            "ExactBindings": int(counters.get(f"Lod{lod}ExactBindings", 0)),
            "MissingBindings": int(missing[str(lod)]),
            "RejectedTransforms": int(rejected[str(lod)]),
            "NullSpriteNoOps": int(
                counters.get(f"Lod{lod}NullSpriteNoOps", 0)
            ),
            "SourceExplicitNoOps": int(
                counters.get(f"Lod{lod}SourceExplicitNoOp", 0)
            ),
            "MeshRendererParts": int(
                counters.get(f"Lod{lod}MeshRendererParts", 0)
            ),
            "SpriteRendererParts": int(
                counters.get(f"Lod{lod}SpriteRendererParts", 0)
            ),
        }
    evidence = {
        "Schema": EVIDENCE_SCHEMA,
        "Sources": {
            "World": source_record(WORLD_PATH),
            "Runtime": source_record(RUNTIME_PATH),
            "NearAssets": source_record(NEAR_ASSETS_PATH),
            "NearUeGeneration": source_record(NEAR_UE_PATH),
            "StreamingAssets": source_record(STREAMING_ASSETS_PATH),
        },
        "OutputCanonicalPayloadSha256": data[
            "CanonicalPayloadSha256"
        ],
        "ProjectionContract": (
            "component-local (Unity X,Z,Y) * scale; one global X mirror "
            "belongs only to the HISM component"
        ),
        "PrefabRootTransformPolicy": (
            "exclude recursive prefab root TRS because MapCellLarge "
            "overwrites the root"
        ),
        "LodFootprints": footprint_evidence,
        "Counts": {
            "TileCount": len(data["Tiles"]),
            "TileTypeCount": len(data["TileTypes"]),
            "RecipeCount": len(data["Recipes"]),
            "CompiledPartCount": sum(
                len(recipe["Parts"]) for recipe in data["Recipes"]
            ),
            "DetailedSourceInstanceCount": sum(
                int(item["instance_count"])
                for item in runtime["detailed_templates"]
            ),
            "DetailedLodInstanceUseCount": 2
            * sum(
                int(item["instance_count"])
                for item in runtime["detailed_templates"]
            ),
            "DetailedFlattenedSourcePartUseCount": sum(
                1
                for item in detailed_evidence
                if item["SourceKind"] == "detailed_instance"
            ),
            "FarStaticRendererCount": int(
                runtime["statistics"]["mesh_renderer_count"]
                + runtime["statistics"]["sprite_renderer_count"]
                - runtime["statistics"]["empty_sprite_asset_count"]
            ),
            "FlattenedSourceRenderCandidateUseCount": (
                sum(
                    1
                    for item in detailed_evidence
                    if item["SourceKind"] == "detailed_instance"
                )
                + int(
                    runtime["statistics"]["mesh_renderer_count"]
                    + runtime["statistics"]["sprite_renderer_count"]
                    - runtime["statistics"]["empty_sprite_asset_count"]
                )
            ),
            "ByLod": by_lod,
        },
        "PartEvidence": detailed_evidence + far_evidence,
        "StaticReadiness": {
            "Ready": b_static,
            "MissingBindingsByLod": missing,
            "RejectedTransformsByLod": rejected,
        },
        "RuntimeReadiness": {
            "Ready": False,
            "ParticleSystemCount": int(
                runtime["statistics"]["particle_system_count"]
            ),
            "MeshColliderCount": int(
                runtime["statistics"]["mesh_collider_count"]
            ),
            "NullSpriteNoOpCount": int(
                runtime["statistics"]["empty_sprite_asset_count"]
            ),
        },
    }

    OUTPUT_PATH.write_text(
        json.dumps(
            data,
            ensure_ascii=False,
            indent=2,
            sort_keys=False,
            allow_nan=False,
        )
        + "\n",
        encoding="utf-8",
    )
    evidence["OutputRawSha256"] = sha256(OUTPUT_PATH)
    EVIDENCE_PATH.write_text(
        json.dumps(
            evidence,
            ensure_ascii=False,
            indent=2,
            sort_keys=True,
            allow_nan=False,
        )
        + "\n",
        encoding="utf-8",
    )
    SUMMARY_PATH.write_text(
        build_markdown(data, evidence),
        encoding="utf-8",
    )
    print(
        "Rok world-map streaming data compiled: "
        f"tiles={len(data['Tiles'])} "
        f"tile_types={len(data['TileTypes'])} "
        f"recipes={len(data['Recipes'])} "
        f"parts={evidence['Counts']['CompiledPartCount']} "
        f"static_ready={b_static} runtime_ready=False"
    )


if __name__ == "__main__":
    main()
