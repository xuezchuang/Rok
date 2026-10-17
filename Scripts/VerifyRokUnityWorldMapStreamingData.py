import copy
import hashlib
import json
import math
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path
from rok_data_paths import resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
COMPILER_PATH = PROJECT_ROOT / "Scripts" / "CompileRokUnityWorldMapStreamingData.py"
WORLD_PATH = MIGRATION_ROOT / "unity_world_map.json"
RUNTIME_PATH = MIGRATION_ROOT / "unity_world_map_runtime_data.json"
NEAR_ASSETS_PATH = MIGRATION_ROOT / "unity_world_map_assets.json"
NEAR_UE_PATH = MIGRATION_ROOT / "unity_world_map_ue_generation.json"
STREAMING_ASSETS_PATH = MIGRATION_ROOT / "unity_world_map_streaming_assets.json"
OUTPUT_PATH = MIGRATION_ROOT / "unity_world_map_streaming_data.json"
EVIDENCE_PATH = MIGRATION_ROOT / "unity_world_map_streaming_data_compile.json"
VERIFY_PATH = MIGRATION_ROOT / "unity_world_map_streaming_data_verify.json"
EXPECTED_TOP_LEVEL = [
    "Schema",
    "CanonicalPayloadSha256",
    "SourceManifestPath",
    "SourceManifestSha256",
    "Projection",
    "MapWidthTiles",
    "MapHeightTiles",
    "TileSizeUnityUnits",
    "LodLevels",
    "Tiles",
    "TileTypes",
    "Recipes",
    "bStaticVisualReady",
    "bRuntimeReady",
    "ReadinessBlockers",
    "PendingPolicies",
]
LOD_THRESHOLDS = [1500.0, 3000.0, 8000.0, 20000.0, 42500.0, 999999.0]
REACHABLE_MAX_DXF = 72750.0
ASPECT_RATIO = 16.0 / 10.0
EPSILON = 1.0e-6


def sha256_bytes(value):
    return hashlib.sha256(value).hexdigest()


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


def canonical_payload_hash(value):
    payload = copy.deepcopy(value)
    payload.pop("CanonicalPayloadSha256", None)
    encoded = json.dumps(
        payload,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")
    return sha256_bytes(encoded)


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


def determinant3(matrix):
    return (
        matrix[0][0]
        * (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1])
        - matrix[0][1]
        * (matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0])
        + matrix[0][2]
        * (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0])
    )


def basis_matrix(unity, translation_scale, translation_offset=(0.0, 0.0, 0.0)):
    order = (0, 2, 1)
    result = [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]
    for row in range(3):
        for column in range(3):
            result[row][column] = float(unity[order[row]][order[column]])
        result[row][3] = (
            float(unity[order[row]][3]) - float(translation_offset[row])
        ) * float(translation_scale)
    return result


def matrix_from_trs(position, rotation, scale):
    x, y, z, w = (
        float(rotation["x"]),
        float(rotation["y"]),
        float(rotation["z"]),
        float(rotation["w"]),
    )
    length = math.sqrt(x * x + y * y + z * z + w * w)
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
    result = [
        [1.0, 0.0, 0.0, float(position["x"])],
        [0.0, 1.0, 0.0, float(position["y"])],
        [0.0, 0.0, 1.0, float(position["z"])],
        [0.0, 0.0, 0.0, 1.0],
    ]
    scales = (float(scale["x"]), float(scale["y"]), float(scale["z"]))
    for row in range(3):
        for column in range(3):
            result[row][column] = rotation_matrix[row][column] * scales[column]
    return result


def transform_matrix(transform):
    rotation = transform["Rotation"]
    x = float(rotation["X"])
    y = float(rotation["Y"])
    z = float(rotation["Z"])
    w = float(rotation["W"])
    length = math.sqrt(x * x + y * y + z * z + w * w)
    if not math.isfinite(length) or abs(length - 1.0) > 2.0e-6:
        raise ValueError(f"non-unit quaternion length {length}")
    scale = transform["Scale3D"]
    sx, sy, sz = (
        float(scale["X"]),
        float(scale["Y"]),
        float(scale["Z"]),
    )
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
    translation = transform["Translation"]
    result = [[0.0] * 4 for _ in range(4)]
    scales = (sx, sy, sz)
    for row in range(3):
        for column in range(3):
            result[row][column] = rotation_matrix[row][column] * scales[column]
    result[0][3] = float(translation["X"])
    result[1][3] = float(translation["Y"])
    result[2][3] = float(translation["Z"])
    result[3][3] = 1.0
    return result


def matrices_close(left, right, tolerance=5.0e-4):
    return all(
        math.isclose(
            float(left[row][column]),
            float(right[row][column]),
            rel_tol=0.0,
            abs_tol=tolerance,
        )
        for row in range(4)
        for column in range(4)
    )


def vec_length(value):
    return math.sqrt(sum(float(item) ** 2 for item in value))


def normalize(value):
    length = vec_length(value)
    return [float(item) / length for item in value]


def cross(left, right):
    return [
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    ]


def camera_at_dxf(keys, requested):
    dxf = min(max(float(requested), float(keys[0]["dxf"])), REACHABLE_MAX_DXF)
    for key in keys:
        if abs(float(key["dxf"]) - dxf) <= 1.0e-9:
            return float(key["distance_unity_units"]), float(key["vertical_fov_degrees"])
    for lower, upper in zip(keys, keys[1:]):
        if float(lower["dxf"]) < dxf < float(upper["dxf"]):
            lo, hi = 0.0, 1.0
            distance0 = float(lower["distance_unity_units"])
            distance1 = float(upper["distance_unity_units"])
            fov0 = float(lower["vertical_fov_degrees"])
            fov1 = float(upper["vertical_fov_degrees"])
            for _ in range(24):
                middle = (lo + hi) * 0.5
                product = (distance0 + middle * (distance1 - distance0)) * (
                    fov0 + middle * (fov1 - fov0)
                )
                if product > dxf:
                    hi = middle
                else:
                    lo = middle
            t = (lo + hi) * 0.5
            return (
                distance0 + t * (distance1 - distance0),
                fov0 + t * (fov1 - fov0),
            )
    raise ValueError(f"camera range does not cover {dxf}")


def derive_chunk_spans(world, tile_size):
    keys = sorted(world["camera_keys"], key=lambda item: float(item["dxf"]))
    forward = normalize(
        [
            float(keys[0]["unity_forward"]["x"]),
            float(keys[0]["unity_forward"]["y"]),
            float(keys[0]["unity_forward"]["z"]),
        ]
    )
    right = normalize(cross([0.0, 1.0, 0.0], forward))
    camera_up = normalize(cross(forward, right))
    spans = []
    previous = 1
    for threshold in LOD_THRESHOLDS:
        distance, fov = camera_at_dxf(keys, min(threshold, REACHABLE_MAX_DXF))
        vt = math.tan(math.radians(fov) * 0.5)
        ht = vt * ASPECT_RATIO
        origin = [-component * distance for component in forward]
        extent = 0.0
        for screen_y in (-1.0, 1.0):
            for screen_x in (-1.0, 1.0):
                ray = [
                    forward[index]
                    + right[index] * screen_x * ht
                    + camera_up[index] * screen_y * vt
                    for index in range(3)
                ]
                hit_distance = -origin[1] / ray[1]
                hit = [
                    origin[index] + ray[index] * hit_distance
                    for index in range(3)
                ]
                extent = max(extent, abs(hit[0]), abs(hit[2]))
        minimum = int(math.ceil(extent / tile_size - 1.0e-12)) + 1
        previous = max(previous, minimum)
        spans.append(previous)
    return spans


def parse_unity_sprite_meta(path):
    text = path.read_text(encoding="utf-8")
    pixels = re.search(r"(?m)^\s*spritePixelsToUnits:\s*(\S+)", text)
    result = {}
    for block in text.split("\n    - serializedVersion: 2\n")[1:]:
        name = re.search(r"(?m)^\s*name:\s*(.+?)\s*$", block)
        file_id = re.search(r"(?m)^\s*internalID:\s*(-?\d+)\s*$", block)
        rect = re.search(
            r"(?ms)^\s*rect:\s*\n"
            r"\s*serializedVersion:\s*\d+\s*\n"
            r"\s*x:\s*([^\s]+)\s*\n"
            r"\s*y:\s*([^\s]+)\s*\n"
            r"\s*width:\s*([^\s]+)\s*\n"
            r"\s*height:\s*([^\s]+)",
            block,
        )
        pivot = re.search(
            r"(?m)^\s*pivot:\s*\{x:\s*([^,]+),\s*y:\s*([^}]+)\}",
            block,
        )
        if name and file_id and rect and pivot:
            result[file_id.group(1)] = {
                "name": name.group(1).strip(),
                "rect": {
                    "x": float(rect.group(1)),
                    "y": float(rect.group(2)),
                    "width": float(rect.group(3)),
                    "height": float(rect.group(4)),
                },
                "pivot": {
                    "x": float(pivot.group(1)),
                    "y": float(pivot.group(2)),
                },
                "pixels_per_unit": float(pixels.group(1)) if pixels else None,
            }
    return result


def iter_nodes(root):
    identity = [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]
    stack = [(root, identity)]
    while stack:
        node, parent = stack.pop()
        composed = identity if node is root else matrix_multiply(
            parent,
            node["transform"]["local_matrix"],
        )
        yield node, composed
        for child in reversed(node.get("children", [])):
            stack.append((child, composed))


def run_compiler_twice(failures):
    hashes = []
    for run_index in range(2):
        result = subprocess.run(
            [sys.executable, str(COMPILER_PATH)],
            cwd=PROJECT_ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        if result.returncode:
            failures.append(
                f"compiler run {run_index + 1} failed: "
                f"{result.stderr.strip() or result.stdout.strip()}"
            )
            break
        hashes.append(sha256(OUTPUT_PATH))
    if len(hashes) == 2 and hashes[0] != hashes[1]:
        failures.append(
            f"non-deterministic raw output hashes: {hashes[0]} != {hashes[1]}"
        )
    return hashes


def main():
    failures = []
    double_compile_hashes = run_compiler_twice(failures)
    if failures:
        payload = {
            "schema": "rok.world_map_streaming_data_verify.v1",
            "failure_count": len(failures),
            "failures": failures,
            "double_compile_raw_sha256": double_compile_hashes,
        }
        VERIFY_PATH.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        raise SystemExit(1)

    world = load(WORLD_PATH)
    runtime = load(RUNTIME_PATH)
    near_assets = load(NEAR_ASSETS_PATH)
    near_ue = load(NEAR_UE_PATH)
    streaming = load(STREAMING_ASSETS_PATH)
    data = load(OUTPUT_PATH)
    evidence = load(EVIDENCE_PATH)

    if list(data) != EXPECTED_TOP_LEVEL:
        failures.append(f"top-level reflected field order/shape changed: {list(data)}")
    if data.get("Schema") != "rok.world_map_runtime_data_asset.v1":
        failures.append(f"unexpected output schema {data.get('Schema')!r}")
    if canonical_payload_hash(data) != data.get("CanonicalPayloadSha256"):
        failures.append("canonical payload SHA-256 does not match output content")
    if data.get("SourceManifestSha256") != sha256(RUNTIME_PATH):
        failures.append("source manifest SHA-256 is stale")
    if evidence.get("OutputRawSha256") != sha256(OUTPUT_PATH):
        failures.append("compile evidence raw output SHA-256 is stale")
    if evidence.get("OutputCanonicalPayloadSha256") != data.get(
        "CanonicalPayloadSha256"
    ):
        failures.append("compile evidence canonical hash differs from output")
    for record in evidence.get("Sources", {}).values():
        path = resolve(record["Path"])
        if (
            not path.is_file()
            or path.stat().st_size != record["Size"]
            or sha256(path) != record["Sha256"]
        ):
            failures.append(f"stale compile source identity: {record.get('Path')}")

    tiles = data.get("Tiles", [])
    if len(tiles) != 1600 or runtime["map"]["tile_count"] != 1600:
        failures.append(f"tile count is not 1600: output={len(tiles)}")
    if data.get("MapWidthTiles") != 40 or data.get("MapHeightTiles") != 40:
        failures.append("map dimensions are not 40 x 40")
    if {(tile["TileX"], tile["TileY"]) for tile in tiles} != {
        (x, y) for y in range(40) for x in range(40)
    }:
        failures.append("tile coordinate coverage is not exactly 0..39 x 0..39")

    projection = data["Projection"]
    origin = projection["UnityRebaseOrigin"]
    source_tiles = sorted(runtime["map"]["tiles"], key=lambda item: int(item["index"]))
    for index, (source, output) in enumerate(zip(source_tiles, tiles)):
        expected = basis_matrix(
            source["template_to_world_matrix"],
            projection["UeUnitsPerUnityUnit"],
            (float(origin["X"]), float(origin["Y"]), 0.0),
        )
        try:
            observed = transform_matrix(output["TemplateToWorld"])
        except (KeyError, TypeError, ValueError) as error:
            failures.append(f"tile {index} invalid FTransform: {error}")
            continue
        if not matrices_close(expected, observed):
            failures.append(f"tile {index} TemplateToWorld differs from source")
        if determinant3(observed) <= EPSILON:
            failures.append(f"tile {index} has non-positive determinant")
        if output["TileTypeId"] != source["tile_id"]:
            failures.append(f"tile {index} type identity differs from source")

    tile_types = data.get("TileTypes", [])
    recipes = data.get("Recipes", [])
    if len(tile_types) != 66 or len(recipes) != 396:
        failures.append(
            f"expected 66 tile types and 396 recipes; got "
            f"{len(tile_types)} and {len(recipes)}"
        )
    recipe_by_id = {recipe["RecipeId"]: recipe for recipe in recipes}
    if len(recipe_by_id) != len(recipes):
        failures.append("recipe IDs are not unique")
    for tile_type in tile_types:
        bindings = tile_type.get("LodBindings", [])
        if [binding.get("Lod") for binding in bindings] != list(range(6)):
            failures.append(f"{tile_type['TileTypeId']} does not bind all six LODs")
        for binding in bindings:
            recipe = recipe_by_id.get(binding.get("RecipeId"))
            if (
                recipe is None
                or recipe.get("TileTypeId") != tile_type["TileTypeId"]
                or recipe.get("Lod") != binding.get("Lod")
            ):
                failures.append(
                    f"{tile_type['TileTypeId']} LOD{binding.get('Lod')} "
                    "binding does not resolve exactly"
                )

    expected_spans = derive_chunk_spans(
        world,
        float(runtime["map"]["tile_size_unity_units"]),
    )
    observed_spans = [level["ChunkTileSpan"] for level in data["LodLevels"]]
    if observed_spans != expected_spans:
        failures.append(
            f"chunk spans {observed_spans} != source-derived {expected_spans}"
        )
    if [level["MaxDxfInclusive"] for level in data["LodLevels"]] != LOD_THRESHOLDS:
        failures.append("LOD inclusive thresholds differ from runtime source")

    part_evidence = evidence.get("PartEvidence", [])
    evidence_by_detailed = defaultdict(list)
    for item in part_evidence:
        if item.get("SourceKind") != "detailed_instance":
            continue
        identity = item.get("SourceIdentity", {})
        evidence_by_detailed[
            (
                item["RecipeId"],
                int(item["SourceIndex"]),
                identity.get("PrefabId"),
                identity.get("Kind"),
            )
        ].append(item)
    evidence_by_far = {
        (
            item["RecipeId"],
            item.get("RendererType"),
            str(item.get("RendererFileId")),
        ): item
        for item in part_evidence
        if item.get("SourceKind") == "far_renderer"
    }
    expected_counts = Counter()
    near_mesh_paths = set(near_ue["mesh_assets"])
    near_material_paths = set(near_ue["material_assets"])
    ground_by_id = {
        item["prefab_id"]: item
        for item in near_assets["baked_ground_materials"]
    }
    object_by_id = near_assets["object_sources"]

    for template in runtime["detailed_templates"]:
        for lod in (0, 1):
            recipe_id = f"{template['tile_type_id']}:lod{lod}"
            recipe = recipe_by_id[recipe_id]
            for instance in template["instances"]:
                key = (
                    recipe_id,
                    int(instance["index"]),
                    instance["prefab_id"],
                    instance["kind"],
                )
                matches = evidence_by_detailed.get(key, [])
                prefab_id = instance["prefab_id"]
                source_asset = object_by_id.get(prefab_id)
                if (
                    source_asset is not None
                    and source_asset.get("representation") == "empty_source_prefab"
                ):
                    item = matches.pop(0) if matches else None
                    expected_counts[f"lod{lod}_noop"] += 1
                    if (
                        item is None
                        or item.get("BindingStatus") != "source_explicit_noop"
                    ):
                        failures.append(f"{key} empty source is not exact no-op")
                    continue

                variants = []
                if (
                    source_asset is not None
                    and source_asset.get("representation")
                    == "exact_source_fbx_render_layers"
                ):
                    layers = source_asset["river_geometry_contract"][
                        "render_layers"
                    ]
                    if [layer["role"] for layer in layers] != [
                        "edge",
                        "water",
                    ]:
                        failures.append(f"{key} river layer order is not edge/water")
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
                        ready = (
                            mesh_candidate in near_mesh_paths
                            and material_candidate in near_material_paths
                        )
                        local = layer["transform"]
                        variants.append(
                            {
                                "Role": role,
                                "Matrix": matrix_multiply(
                                    instance["runtime_local_matrix"],
                                    matrix_from_trs(
                                        local["local_position"],
                                        local["local_rotation"],
                                        local["local_scale"],
                                    ),
                                ),
                                "Mesh": mesh_candidate if ready else "",
                                "Materials": (
                                    [material_candidate] if ready else []
                                ),
                                "Status": (
                                    "exact_near_source_fbx_render_layer"
                                    if ready
                                    else "missing"
                                ),
                                "Tree": False,
                                "Sort": int(layer["sort_priority"]),
                            }
                        )
                elif (
                    source_asset is not None
                    and source_asset.get("representation")
                    == "exact_source_fbx_composite_waterfall"
                ):
                    static_parts = source_asset["waterfall_geometry_contract"][
                        "static_parts"
                    ]
                    for part in static_parts:
                        mesh_candidate = part["ue_mesh"]
                        material_candidate = part["ue_material"]
                        ready = (
                            mesh_candidate in near_mesh_paths
                            and material_candidate in near_material_paths
                        )
                        variants.append(
                            {
                                "Role": None,
                                "Part": part["game_object"],
                                "Matrix": matrix_multiply(
                                    instance["runtime_local_matrix"],
                                    part["local_matrix"],
                                ),
                                "Mesh": mesh_candidate if ready else "",
                                "Materials": (
                                    [material_candidate] if ready else []
                                ),
                                "Status": (
                                    "exact_near_source_fbx_composite_part"
                                    if ready
                                    else "missing"
                                ),
                                "Tree": False,
                                "Sort": int(part["sort_priority"]),
                            }
                        )
                elif prefab_id in ground_by_id:
                    mesh_candidate = (
                        "/Game/RokUnityWorldMapPrototype/Meshes/"
                        "SM_RokWorld_Plane60.SM_RokWorld_Plane60"
                    )
                    material_candidate = (
                        "/Game/RokUnityWorldMapPrototype/Materials/"
                        f"M_WorldGround_{prefab_id}.M_WorldGround_{prefab_id}"
                    )
                    if (
                        mesh_candidate in near_mesh_paths
                        and material_candidate in near_material_paths
                    ):
                        expected_mesh = mesh_candidate
                        expected_materials = [material_candidate]
                        expected_status = "exact_near_ground"
                    else:
                        expected_mesh = ""
                        expected_materials = []
                        expected_status = "missing"
                    variants.append(
                        {
                            "Role": None,
                            "Matrix": instance["runtime_local_matrix"],
                            "Mesh": expected_mesh,
                            "Materials": expected_materials,
                            "Status": expected_status,
                            "Tree": False,
                            "Sort": 0,
                        }
                    )
                elif source_asset is not None:
                    representation = source_asset.get("representation", "")
                    folder = source_asset.get(
                        "ue_mesh_folder",
                        "/Game/RokUnityWorldMapPrototype/Meshes",
                    )
                    ue_name = source_asset.get("ue_mesh_name", "")
                    mesh_candidate = (
                        f"{folder}/{ue_name}.{ue_name}" if ue_name else ""
                    )
                    source_material = source_asset.get("source_material", "")
                    if (
                        not source_material
                        and representation == "exact_source_fbx"
                    ):
                        source_material = source_asset.get(
                            "mountain_geometry_contract",
                            {},
                        ).get("body_render_node", {}).get("material_path", "")
                    material_stem = Path(source_material).stem
                    material_candidate = (
                        "/Game/RokUnityWorldMapPrototype/Materials/"
                        f"M_WorldSource_{material_stem}."
                        f"M_WorldSource_{material_stem}"
                    )
                    expected_tree = Path(source_material).name in {
                        "map_tree_nomask.mat",
                        "map_tree_nomask_1.mat",
                    }
                    if (
                        mesh_candidate in near_mesh_paths
                        and material_candidate in near_material_paths
                    ):
                        expected_mesh = mesh_candidate
                        expected_materials = [material_candidate]
                        expected_status = f"exact_near_{representation}"
                    else:
                        expected_mesh = ""
                        expected_materials = []
                        expected_status = "missing"
                    variants.append(
                        {
                            "Role": None,
                            "Matrix": instance["runtime_local_matrix"],
                            "Mesh": expected_mesh,
                            "Materials": expected_materials,
                            "Status": expected_status,
                            "Tree": expected_tree,
                            "Sort": 0,
                        }
                    )
                else:
                    variants.append(
                        {
                            "Role": None,
                            "Matrix": instance["runtime_local_matrix"],
                            "Mesh": "",
                            "Materials": [],
                            "Status": "missing",
                            "Tree": False,
                            "Sort": 0,
                        }
                    )

                for variant in variants:
                    item = matches.pop(0) if matches else None
                    variant_key = key + (variant["Role"],)
                    if item is None:
                        failures.append(f"missing detailed evidence {variant_key}")
                        continue
                    if (
                        item.get("SourceIdentity", {}).get("SourceLayerRole")
                        != variant["Role"]
                        or item.get("SourceIdentity", {}).get("SourcePart")
                        != variant.get("Part")
                        or item.get("Mesh") != variant["Mesh"]
                        or item.get("Materials") != variant["Materials"]
                        or item.get("BindingStatus") != variant["Status"]
                        or item.get("bTree") != variant["Tree"]
                        or item.get("SourceSort", {}).get("SortingOrder")
                        != variant["Sort"]
                    ):
                        failures.append(
                            f"{variant_key} detailed binding is not source-derived"
                        )
                    source_matrix = variant["Matrix"]
                    source_det = determinant3(source_matrix)
                    if abs(source_det) <= EPSILON:
                        expected_counts[f"lod{lod}_rejected"] += 1
                        if "PartIndex" in item or "TransformRejection" not in item:
                            failures.append(
                                f"{variant_key} degenerate transform was not rejected"
                            )
                        continue
                    expected_counts[f"lod{lod}_parts"] += 1
                    part_index = item.get("PartIndex")
                    if (
                        not isinstance(part_index, int)
                        or part_index >= len(recipe["Parts"])
                    ):
                        failures.append(f"{variant_key} invalid detailed PartIndex")
                        continue
                    part = recipe["Parts"][part_index]
                    expected_matrix = basis_matrix(source_matrix, 500.0)
                    for row in range(3):
                        for column in range(3):
                            expected_matrix[row][column] *= 5.0
                    observed_matrix = transform_matrix(part["LocalTransform"])
                    if not matrices_close(expected_matrix, observed_matrix):
                        failures.append(
                            f"{variant_key} detailed transform differs from source"
                        )
                    if abs(determinant3(observed_matrix)) <= EPSILON:
                        failures.append(
                            f"{variant_key} emitted a degenerate transform"
                        )
                    if (
                        part["Mesh"] != item.get("Mesh")
                        or part["Materials"] != item.get("Materials")
                        or part["bTree"] != item.get("bTree")
                        or part["TranslucencySortPriority"] != variant["Sort"]
                    ):
                        failures.append(
                            f"{variant_key} detailed asset/render state differs"
                        )
                    expected_counts[
                        f"lod{lod}_{'exact' if variant['Status'].startswith('exact_') else 'missing'}"
                    ] += 1

    streaming_meshes = {
        (item["identity"]["source_path"], str(item["identity"]["source_file_id"])): item
        for item in streaming["meshes"]
    }
    streaming_materials = {
        (item["identity"]["source_path"], str(item["identity"]["source_file_id"])): item
        for item in streaming["materials"]
    }
    streaming_sprites = {}
    for texture in streaming["sprite_textures"]:
        source_path = texture["identity"]["source_path"]
        parsed_meta = parse_unity_sprite_meta(
            PROJECT_ROOT / f"{source_path}.meta"
        )
        for subasset in texture["referenced_sprite_subassets"]:
            file_id = str(subasset["source_file_id"])
            parsed = parsed_meta.get(file_id)
            if parsed is None:
                failures.append(
                    f"{source_path} sprite {file_id} is absent from Unity meta"
                )
            elif (
                parsed["name"] != subasset["meta"]["name"]
                or parsed["rect"] != subasset["meta"]["rect"]
                or parsed["pivot"] != subasset["meta"]["pivot"]
            ):
                failures.append(
                    f"{source_path} sprite {file_id} rect/pivot differs from "
                    "Unity TextureImporter meta"
                )
            importer_record = subasset["texture_importer_meta"]
            importer_path = resolve(importer_record["path"])
            if (
                not importer_path.is_file()
                or importer_path.stat().st_size != importer_record["size"]
                or sha256(importer_path) != importer_record["sha256"]
            ):
                failures.append(
                    f"{source_path} sprite {file_id} importer identity is stale"
                )
            exported = subasset["exported_quad_obj"]
            exported_path = resolve(exported["path"])
            if (
                not exported_path.is_file()
                or exported_path.stat().st_size != exported["size"]
                or sha256(exported_path) != exported["sha256"]
            ):
                failures.append(
                    f"{source_path} sprite {file_id} quad OBJ identity is stale"
                )
            streaming_sprites[
                (source_path, file_id)
            ] = (texture, subasset, parsed)

    for source_recipe in runtime["far_lod_recipes"]:
        lod = int(source_recipe["lod"])
        recipe = recipe_by_id[source_recipe["recipe_id"]]
        for node, composed in iter_nodes(source_recipe["root"]):
            components = defaultdict(list)
            for component in node.get("components", []):
                components[component["type"]].append(component)
            for renderer_type in ("MeshRenderer", "SpriteRenderer"):
                for renderer in components[renderer_type]:
                    if not node["active_in_hierarchy"] or int(renderer["enabled"]) != 1:
                        continue
                    key = (
                        source_recipe["recipe_id"],
                        renderer_type,
                        str(renderer["file_id"]),
                    )
                    item = evidence_by_far.get(key)
                    if item is None:
                        failures.append(f"missing far evidence {key}")
                        continue
                    if renderer_type == "SpriteRenderer":
                        sprite_ref = renderer["sprite"]
                        if renderer.get("empty_asset") or sprite_ref.get("is_null"):
                            expected_counts[f"lod{lod}_noop"] += 1
                            if item.get("BindingStatus") != (
                                "source_explicit_null_sprite_noop"
                            ):
                                failures.append(f"{key} null sprite is not exact no-op")
                            continue
                        sprite_record = streaming_sprites.get(
                            (sprite_ref["asset_path"], str(sprite_ref["file_id"]))
                        )
                        sprite_binding = (
                            sprite_record[1] if sprite_record is not None else None
                        )
                        if (
                            sprite_binding is None
                            or sprite_binding.get("binding_status") != "ready"
                            or item.get("Mesh")
                            != sprite_binding.get("ue_mesh_soft_object_path")
                            or item.get("Materials")
                            != [sprite_binding.get("ue_material_soft_object_path")]
                        ):
                            failures.append(f"{key} sprite quad binding is not exact")
                        elif item.get("Sprite") != {
                            "SourcePath": sprite_ref["asset_path"],
                            "SourceFileId": str(sprite_ref["file_id"]),
                            "SourceGuid": sprite_ref.get("guid", ""),
                            "TextureTarget": sprite_binding[
                                "ue_texture_soft_object_path"
                            ],
                            "SubassetName": sprite_binding["name"],
                            "Name": sprite_record[2]["name"],
                            "RectPixels": {
                                key.title(): value
                                for key, value in sprite_record[2]["rect"].items()
                            },
                            "Pivot": {
                                key.title(): value
                                for key, value in sprite_record[2]["pivot"].items()
                            },
                            "PixelsPerUnit": sprite_record[2]["pixels_per_unit"],
                            "Color": renderer.get("color"),
                            "FlipX": int(renderer.get("flip_x", 0)),
                            "FlipY": int(renderer.get("flip_y", 0)),
                            "DrawMode": int(renderer.get("draw_mode", 0)),
                            "SourceSize": renderer.get("size"),
                        }:
                            failures.append(
                                f"{key} sprite source rect/pivot/render evidence differs"
                            )
                    else:
                        mesh_ref = components["MeshFilter"][0]["mesh"]
                        mesh_binding = streaming_meshes.get(
                            (mesh_ref["asset_path"], str(mesh_ref["file_id"]))
                        )
                        expected_mesh = (
                            mesh_binding.get("ue_soft_object_path")
                            if mesh_binding is not None
                            else ""
                        )
                        if item.get("Mesh") != expected_mesh:
                            failures.append(f"{key} mesh identity binding differs")
                    expected_materials = []
                    for material_ref in renderer.get("materials", []):
                        binding = streaming_materials.get(
                            (
                                material_ref["asset_path"],
                                str(material_ref["file_id"]),
                            )
                        )
                        expected_materials.append(
                            binding.get("ue_soft_object_path") if binding else ""
                        )
                    if renderer_type == "MeshRenderer" and item.get(
                        "Materials"
                    ) != expected_materials:
                        failures.append(f"{key} ordered material bindings differ")
                    source_det = determinant3(composed)
                    if abs(source_det) <= EPSILON:
                        expected_counts[f"lod{lod}_rejected"] += 1
                        if "PartIndex" in item or "TransformRejection" not in item:
                            failures.append(f"{key} degenerate transform was not rejected")
                        continue
                    expected_counts[f"lod{lod}_parts"] += 1
                    part_index = item.get("PartIndex")
                    if not isinstance(part_index, int) or part_index >= len(
                        recipe["Parts"]
                    ):
                        failures.append(f"{key} invalid far PartIndex")
                        continue
                    part = recipe["Parts"][part_index]
                    expected_matrix = basis_matrix(composed, 500.0)
                    for row in range(3):
                        for column in range(3):
                            expected_matrix[row][column] *= 5.0
                    observed_matrix = transform_matrix(part["LocalTransform"])
                    if not matrices_close(expected_matrix, observed_matrix):
                        failures.append(f"{key} recursive root-excluded transform differs")
                    if abs(determinant3(observed_matrix)) <= EPSILON:
                        failures.append(f"{key} emitted a degenerate transform")
                    if (
                        part["Mesh"] != item.get("Mesh")
                        or part["Materials"] != item.get("Materials")
                        or part["TranslucencySortPriority"]
                        != item["SourceSort"]["SortingOrder"]
                        or part["RenderFlags"] != item["RenderFlags"]
                        or part["bTree"] != item["bTree"]
                    ):
                        failures.append(f"{key} render/sort/tree state differs")
                    expected_counts[
                        f"lod{lod}_{'exact' if item['BindingStatus'].startswith('exact_') else 'missing'}"
                    ] += 1

    by_lod = evidence["Counts"]["ByLod"]
    for lod in range(6):
        observed = by_lod[str(lod)]
        checks = {
            "CompiledParts": expected_counts[f"lod{lod}_parts"],
            "ExactBindings": expected_counts[f"lod{lod}_exact"],
            "MissingBindings": expected_counts[f"lod{lod}_missing"],
            "RejectedTransforms": expected_counts[f"lod{lod}_rejected"],
        }
        for key, expected in checks.items():
            if observed[key] != expected:
                failures.append(
                    f"LOD{lod} {key} {observed[key]} != recomputed {expected}"
                )
        if sum(len(recipe["Parts"]) for recipe in recipes if recipe["Lod"] == lod) != (
            expected_counts[f"lod{lod}_parts"]
        ):
            failures.append(f"LOD{lod} flattened recipe part total differs")

    if evidence["Counts"]["DetailedSourceInstanceCount"] != 2503:
        failures.append("detailed source instance count is not 2503")
    if evidence["Counts"]["DetailedLodInstanceUseCount"] != 5006:
        failures.append("detailed LOD instance use count is not 5006")
    recomputed_detailed_flattened = sum(
        expected_counts[f"lod{lod}_parts"]
        + expected_counts[f"lod{lod}_rejected"]
        + expected_counts[f"lod{lod}_noop"]
        for lod in (0, 1)
    )
    if (
        evidence["Counts"]["DetailedFlattenedSourcePartUseCount"]
        != recomputed_detailed_flattened
    ):
        failures.append("detailed flattened source part use count differs")
    if evidence["Counts"]["FarStaticRendererCount"] != 4726:
        failures.append("far static renderer count is not 4726")
    if (
        evidence["Counts"]["FlattenedSourceRenderCandidateUseCount"]
        != recomputed_detailed_flattened + 4726
    ):
        failures.append("flattened source render candidate count differs")
    if evidence["Counts"]["CompiledPartCount"] != sum(
        len(recipe["Parts"]) for recipe in recipes
    ):
        failures.append("flattened compiled part count is inconsistent")

    pending = {item["PolicyId"]: item for item in data["PendingPolicies"]}
    if (
        data["bRuntimeReady"] is not False
        or pending.get("ParticleSystems", {}).get("Status") != "Pending"
        or pending.get("ParticleSystems", {}).get("SourceComponentCount") != 18
        or pending.get("MeshColliders", {}).get("Status") != "Pending"
        or pending.get("MeshColliders", {}).get("SourceComponentCount") != 6
        or pending.get("NullSprites", {}).get("Status") != "SourceExplicitNoOp"
        or pending.get("NullSprites", {}).get("SourceComponentCount") != 4
    ):
        failures.append("runtime readiness/pending policy counts are not exact")
    any_static_failure = any(
        expected_counts[f"lod{lod}_missing"]
        or expected_counts[f"lod{lod}_rejected"]
        for lod in range(6)
    )
    if data["bStaticVisualReady"] is not (not any_static_failure):
        failures.append("bStaticVisualReady is not derived from exact bindings/transforms")
    if any_static_failure and not data["ReadinessBlockers"]:
        failures.append("static readiness failure has no blockers")

    result = {
        "schema": "rok.world_map_streaming_data_verify.v1",
        "failure_count": len(failures),
        "failures": failures,
        "double_compile_raw_sha256": double_compile_hashes,
        "output_raw_sha256": sha256(OUTPUT_PATH),
        "canonical_payload_sha256": data["CanonicalPayloadSha256"],
        "streaming_assets_raw_sha256": sha256(STREAMING_ASSETS_PATH),
        "counts": evidence["Counts"],
        "chunk_tile_spans": observed_spans,
        "all_emitted_transforms_non_degenerate": not any(
            "degenerate" in failure for failure in failures
        ),
        "global_x_mirror_location": "HISM component only",
        "static_visual_ready": data["bStaticVisualReady"],
        "runtime_ready": data["bRuntimeReady"],
    }
    VERIFY_PATH.write_text(
        json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(
        "Rok world-map streaming data verification: "
        f"failures={len(failures)} "
        f"parts={evidence['Counts']['CompiledPartCount']} "
        f"raw_sha256={result['output_raw_sha256']}"
    )
    if failures:
        for failure in failures[:40]:
            print(f"FAIL: {failure}")
        raise SystemExit(1)


if __name__ == "__main__":
    main()
