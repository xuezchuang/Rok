import copy
import gc
import hashlib
import json
import math
import re
from collections import Counter
from pathlib import Path

from BuildRokUnityWorldMapRuntimeData import (
    EXPECTED_STATISTICS,
    FAR_LODS,
    KNOWN_EMPTY_SPRITE_NODE,
    KNOWN_EMPTY_SPRITE_TILE_TYPE,
    KNOWN_EXTRA_LOD_ADDRESS_TEMPLATE,
    LOD_MAX_DXF,
    OUTPUT_PATH,
    PROJECT_ROOT,
    SCHEMA,
    UNITY_CLASS_NAMES,
    UNITY_BUILTIN_GUIDS,
    build_runtime_manifest,
    canonical_payload_hash,
    quaternion_transform_matrix,
    serialized_matrix,
    sha256,
)


OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
VERIFY_PATH = OUTPUT_DIR / "unity_world_map_runtime_data_verify.json"
DECIMAL_FILE_ID_PATTERN = re.compile(r"^-?\d+$")
GUID_PATTERN = re.compile(r"^[0-9a-f]{32}$")


def source_record_is_current(record, context, failures):
    relative_path = record.get("path", "")
    if not relative_path:
        failures.append(f"{context} has no source path")
        return
    path = PROJECT_ROOT / relative_path
    if not path.exists():
        failures.append(f"{context} source is missing: {relative_path}")
        return
    if path.stat().st_size != record.get("size"):
        failures.append(f"{context} source size changed: {relative_path}")
    if sha256(path) != record.get("sha256"):
        failures.append(f"{context} source hash changed: {relative_path}")


def is_matrix(value):
    return (
        isinstance(value, list)
        and len(value) == 4
        and all(
            isinstance(row, list)
            and len(row) == 4
            and all(
                isinstance(item, (int, float))
                and math.isfinite(float(item))
                for item in row
            )
            for row in value
        )
    )


def matrices_nearly_equal(left, right, tolerance=1.0e-6):
    if not is_matrix(left) or not is_matrix(right):
        return False
    return all(
        math.isclose(
            float(left[row][column]),
            float(right[row][column]),
            abs_tol=tolerance,
        )
        for row in range(4)
        for column in range(4)
    )


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


def validate_file_id(value, context, failures):
    if not isinstance(value, str) or not DECIMAL_FILE_ID_PATTERN.fullmatch(
        value
    ):
        failures.append(
            f"{context} fileID is not a decimal string: {value!r}"
        )


def validate_reference(reference, context, failures, require_asset=False):
    if not isinstance(reference, dict):
        failures.append(f"{context} is not a Unity reference record")
        return
    validate_file_id(reference.get("file_id"), context, failures)
    guid = reference.get("guid")
    if not isinstance(guid, str):
        failures.append(f"{context} GUID is not a string")
        return
    if guid and not GUID_PATTERN.fullmatch(guid):
        failures.append(f"{context} has invalid lowercase GUID {guid!r}")
    if reference.get("type") is not None and not isinstance(
        reference.get("type"),
        int,
    ):
        failures.append(f"{context} reference type is not int/null")
    expected_null = reference.get("file_id") == "0" and not guid
    if reference.get("is_null") is not expected_null:
        failures.append(f"{context} null-reference flag is inconsistent")
    expected_builtin = guid in UNITY_BUILTIN_GUIDS
    if reference.get("is_unity_builtin") is not expected_builtin:
        failures.append(f"{context} Unity built-in flag is inconsistent")
    asset_path = reference.get("asset_path", "")
    if asset_path:
        if not (PROJECT_ROOT / asset_path).exists():
            failures.append(
                f"{context} referenced asset is missing: {asset_path}"
            )
    elif require_asset and guid and not expected_builtin:
        failures.append(
            f"{context} non-builtin GUID {guid} has no resolved asset path"
        )


def validate_component(
    component,
    recipe_context,
    node_context,
    component_counts,
    script_counts,
    empty_assets,
    source_file_ids,
    failures,
):
    component_type = component.get("type")
    class_id = component.get("class_id")
    expected_type = UNITY_CLASS_NAMES.get(
        class_id,
        f"UnityClass{class_id}",
    )
    if component_type != expected_type:
        failures.append(
            f"{node_context} component type/class mismatch: "
            f"{component_type}/{class_id}"
        )
    file_id = component.get("file_id")
    validate_file_id(file_id, f"{node_context} {component_type}", failures)
    if file_id in source_file_ids:
        failures.append(
            f"{node_context} repeats Unity object fileID {file_id}"
        )
    source_file_ids.add(file_id)
    validate_file_id(
        component.get("game_object_file_id"),
        f"{node_context} {component_type} game object",
        failures,
    )
    body_hash = component.get("serialized_body_sha256", "")
    if not re.fullmatch(r"[0-9a-f]{64}", body_hash):
        failures.append(
            f"{node_context} {component_type} lacks serialized body hash"
        )
    component_counts[component_type] += 1

    materials = component.get("materials")
    if materials is not None:
        if not isinstance(materials, list):
            failures.append(
                f"{node_context} {component_type} materials is not a list"
            )
        else:
            for index, material in enumerate(materials):
                validate_reference(
                    material,
                    f"{node_context} {component_type} material[{index}]",
                    failures,
                    require_asset=True,
                )

    if component_type == "MeshFilter":
        validate_reference(
            component.get("mesh"),
            f"{node_context} MeshFilter mesh",
            failures,
            require_asset=True,
        )
    elif component_type == "SpriteRenderer":
        sprite = component.get("sprite")
        validate_reference(
            sprite,
            f"{node_context} SpriteRenderer sprite",
            failures,
            require_asset=not component.get("empty_asset", False),
        )
        expected_empty = bool(sprite and sprite.get("is_null"))
        if component.get("empty_asset") is not expected_empty:
            failures.append(
                f"{node_context} SpriteRenderer empty_asset is inconsistent"
            )
        if expected_empty:
            if (
                component.get("empty_asset_policy")
                != "explicit_source_no_asset_render_noop"
            ):
                failures.append(
                    f"{node_context} null SpriteRenderer lacks noop policy"
                )
            empty_assets.append(
                {
                    "tile_type_id": recipe_context["tile_type_id"],
                    "lod": recipe_context["lod"],
                    "node_name": node_context["name"],
                    "sprite_renderer_file_id": file_id,
                }
            )
    elif component_type == "ParticleSystem":
        if component.get("runtime_policy") != (
            "pending_niagara_or_explicit_visual_omission"
        ):
            failures.append(
                f"{node_context} ParticleSystem has invalid pending policy"
            )
        for field in (
            "length_in_seconds",
            "simulation_speed",
            "looping",
            "prewarm",
            "play_on_awake",
            "scaling_mode",
            "culling_mode",
        ):
            if component.get(field) is None:
                failures.append(
                    f"{node_context} ParticleSystem lacks {field}"
                )
    elif component_type == "ParticleSystemRenderer":
        if component.get("runtime_policy") != (
            "pending_niagara_or_explicit_visual_omission"
        ):
            failures.append(
                f"{node_context} ParticleSystemRenderer has invalid policy"
            )
        mesh = component.get("mesh")
        if mesh is not None:
            validate_reference(
                mesh,
                f"{node_context} ParticleSystemRenderer mesh",
                failures,
                require_asset=not mesh.get("is_null", False),
            )
    elif component_type == "MeshCollider":
        if component.get("runtime_policy") != (
            "pending_collision_promotion_and_query_contract"
        ):
            failures.append(
                f"{node_context} MeshCollider has invalid pending policy"
            )
        validate_reference(
            component.get("mesh"),
            f"{node_context} MeshCollider mesh",
            failures,
            require_asset=True,
        )
        material = component.get("material")
        if material is not None:
            validate_reference(
                material,
                f"{node_context} MeshCollider material",
                failures,
                require_asset=not material.get("is_null", False),
            )
    elif component_type == "MonoBehaviour":
        script = component.get("script")
        validate_reference(
            script,
            f"{node_context} MonoBehaviour script",
            failures,
            require_asset=True,
        )
        script_class = component.get("script_class", "")
        if script_class not in {"LockAngle45", "ObjectPoolItem"}:
            failures.append(
                f"{node_context} has unexpected MonoBehaviour "
                f"{script_class!r}"
            )
        else:
            script_counts[script_class] += 1
        if script_class == "LockAngle45":
            expected_policy = "LockAngle45_camera_facing"
        else:
            expected_policy = "ObjectPoolItem_source_spawn_budget"
        if component.get("runtime_policy") != expected_policy:
            failures.append(
                f"{node_context} {script_class} runtime policy mismatch"
            )
        if not isinstance(component.get("serialized_fields"), dict):
            failures.append(
                f"{node_context} {script_class} serialized fields missing"
            )
        if script_class == "ObjectPoolItem":
            expected_fields = {
                "m_max_spawn_number_high",
                "m_max_spawn_number_mid",
                "m_max_spawn_number_low",
            }
            if set(component.get("serialized_fields", {})) != expected_fields:
                failures.append(
                    f"{node_context} ObjectPoolItem fields mismatch"
                )


def validate_node(
    node,
    recipe_context,
    parent_transform_id,
    parent_world_matrix,
    expected_depth,
    parent_active,
    node_counts,
    component_counts,
    script_counts,
    empty_assets,
    source_file_ids,
    failures,
):
    node_name = node.get("name", "")
    node_context = {
        "name": node_name,
        "path": node.get("node_path", ""),
    }
    context_text = (
        f"{recipe_context['recipe_id']} node {node_context['path']}"
    )
    node_counts["node_count"] += 1
    node_counts["max_depth"] = max(
        node_counts["max_depth"],
        expected_depth,
    )
    if node.get("depth") != expected_depth:
        failures.append(
            f"{context_text} depth {node.get('depth')} != {expected_depth}"
        )
    game_object_file_id = node.get("game_object_file_id")
    validate_file_id(
        game_object_file_id,
        f"{context_text} GameObject",
        failures,
    )
    if game_object_file_id in source_file_ids:
        failures.append(
            f"{context_text} repeats Unity object fileID "
            f"{game_object_file_id}"
        )
    source_file_ids.add(game_object_file_id)
    expected_active = parent_active and bool(node.get("active_self"))
    if node.get("active_in_hierarchy") is not expected_active:
        failures.append(f"{context_text} active hierarchy state mismatch")

    transform = node.get("transform", {})
    transform_file_id = transform.get("file_id")
    validate_file_id(
        transform_file_id,
        f"{context_text} Transform",
        failures,
    )
    if transform_file_id in source_file_ids:
        failures.append(
            f"{context_text} repeats Unity object fileID {transform_file_id}"
        )
    source_file_ids.add(transform_file_id)
    if transform.get("game_object_file_id") != game_object_file_id:
        failures.append(f"{context_text} Transform owner mismatch")
    if transform.get("parent_transform_file_id") != parent_transform_id:
        failures.append(f"{context_text} Transform parent mismatch")

    local_matrix = transform.get("local_matrix")
    world_matrix = transform.get("world_matrix")
    if not is_matrix(local_matrix):
        failures.append(f"{context_text} local matrix is invalid")
    if not is_matrix(world_matrix):
        failures.append(f"{context_text} world matrix is invalid")
    position = transform.get("local_position", {})
    rotation = transform.get("local_rotation_quaternion", {})
    scale = transform.get("local_scale", {})
    try:
        expected_local = serialized_matrix(
            quaternion_transform_matrix(position, rotation, scale)
        )
    except (KeyError, TypeError, ValueError) as error:
        failures.append(
            f"{context_text} cannot reconstruct local matrix: {error}"
        )
        expected_local = None
    if expected_local is not None and not matrices_nearly_equal(
        local_matrix,
        expected_local,
    ):
        failures.append(f"{context_text} local matrix mismatch")
    expected_world = (
        matrix_multiply(parent_world_matrix, local_matrix)
        if parent_world_matrix is not None and is_matrix(local_matrix)
        else local_matrix
    )
    if not matrices_nearly_equal(world_matrix, expected_world):
        failures.append(f"{context_text} composed world matrix mismatch")

    source_component_file_ids = node.get("source_component_file_ids", [])
    if not isinstance(source_component_file_ids, list):
        failures.append(f"{context_text} component order is not a list")
        source_component_file_ids = []
    for index, file_id in enumerate(source_component_file_ids):
        validate_file_id(
            file_id,
            f"{context_text} component order[{index}]",
            failures,
        )
    if source_component_file_ids.count(transform_file_id) != 1:
        failures.append(
            f"{context_text} component order lacks exactly one Transform"
        )

    component_types = []
    components = node.get("components", [])
    if not isinstance(components, list):
        failures.append(f"{context_text} components is not a list")
        components = []
    for component in components:
        if component.get("game_object_file_id") != game_object_file_id:
            failures.append(
                f"{context_text} component owner does not match GameObject"
            )
        component_types.append(component.get("type"))
        validate_component(
            component,
            recipe_context,
            node_context,
            component_counts,
            script_counts,
            empty_assets,
            source_file_ids,
            failures,
        )
    expected_non_transform_ids = [
        file_id
        for file_id in source_component_file_ids
        if file_id != transform_file_id
    ]
    actual_non_transform_ids = [
        component.get("file_id")
        for component in components
    ]
    if actual_non_transform_ids != expected_non_transform_ids:
        failures.append(
            f"{context_text} serialized component order mismatch"
        )
    if component_types.count("MeshRenderer") != component_types.count(
        "MeshFilter"
    ):
        failures.append(
            f"{context_text} MeshRenderer/MeshFilter pairing mismatch"
        )
    if component_types.count("ParticleSystem") != component_types.count(
        "ParticleSystemRenderer"
    ):
        failures.append(
            f"{context_text} ParticleSystem/Renderer pairing mismatch"
        )

    children = node.get("children", [])
    if not isinstance(children, list):
        failures.append(f"{context_text} children is not a list")
        children = []
    expected_child_ids = transform.get("child_transform_file_ids", [])
    actual_child_ids = [
        child.get("transform", {}).get("file_id")
        for child in children
    ]
    if actual_child_ids != expected_child_ids:
        failures.append(f"{context_text} child Transform order mismatch")
    for child in children:
        validate_node(
            child,
            recipe_context,
            transform_file_id,
            world_matrix,
            expected_depth + 1,
            expected_active,
            node_counts,
            component_counts,
            script_counts,
            empty_assets,
            source_file_ids,
            failures,
        )


def validate_detailed_templates(manifest, tile_type_ids, failures):
    templates = manifest.get("detailed_templates", [])
    if len(templates) != 66:
        failures.append(
            f"Detailed template count {len(templates)} != 66"
        )
    if [item.get("tile_type_id") for item in templates] != tile_type_ids:
        failures.append("Detailed templates are not sorted by tile type")
    for template in templates:
        context = f"Detailed template {template.get('template_id')}"
        tile_type_id = template.get("tile_type_id")
        if template.get("template_id") != f"{tile_type_id}:detailed":
            failures.append(f"{context} id is not canonical")
        if template.get("applies_to_lods") != [0, 1]:
            failures.append(f"{context} does not apply to LOD0/1")
        if template.get("representation") != (
            "tile_data_detailed_instance_recipe"
        ):
            failures.append(f"{context} representation mismatch")
        instances = template.get("instances", [])
        if len(instances) != template.get("instance_count"):
            failures.append(f"{context} instance count mismatch")
        if template.get("instance_count") != (
            template.get("plane_count", 0)
            + template.get("object_count", 0)
        ):
            failures.append(f"{context} plane/object count mismatch")
        actual_kind_counts = Counter(
            item.get("kind")
            for item in instances
        )
        if actual_kind_counts["plane"] != template.get("plane_count"):
            failures.append(f"{context} plane records mismatch")
        if actual_kind_counts["object"] != template.get("object_count"):
            failures.append(f"{context} object records mismatch")
        for index, instance in enumerate(instances):
            instance_context = f"{context} instance[{index}]"
            if not is_matrix(instance.get("source_local_matrix")):
                failures.append(
                    f"{instance_context} source matrix is invalid"
                )
            if not is_matrix(instance.get("runtime_local_matrix")):
                failures.append(
                    f"{instance_context} runtime matrix is invalid"
                )
            expected_policy = (
                "position_only"
                if instance.get("kind") == "object"
                and instance.get("category") == "adorning"
                else "position_rotation_scale"
            )
            if instance.get("unity_runtime_transform_policy") != (
                expected_policy
            ):
                failures.append(
                    f"{instance_context} runtime transform policy mismatch"
                )
            if expected_policy == "position_only":
                if instance.get("unity_runtime_rotation_y") != 0.0:
                    failures.append(
                        f"{instance_context} position-only rotation is nonzero"
                    )
                if instance.get("unity_runtime_scale") != {
                    "x": 1.0,
                    "y": 1.0,
                    "z": 1.0,
                }:
                    failures.append(
                        f"{instance_context} position-only scale is non-unit"
                    )


def validate_map(manifest, tile_type_ids, failures):
    map_record = manifest.get("map", {})
    for key, expected in (
        ("tile_count", 1600),
        ("width_tiles", 40),
        ("height_tiles", 40),
        ("tile_size_unity_units", 180.0),
    ):
        if map_record.get(key) != expected:
            failures.append(
                f"Map {key} {map_record.get(key)} != {expected}"
            )
    tiles = map_record.get("tiles", [])
    if len(tiles) != 1600:
        failures.append(f"Map tile list count {len(tiles)} != 1600")
    coordinates = set()
    usage = Counter()
    for tile in tiles:
        coordinate = (tile.get("x"), tile.get("y"))
        if coordinate in coordinates:
            failures.append(f"Duplicate map tile coordinate {coordinate}")
        coordinates.add(coordinate)
        if not (
            isinstance(coordinate[0], int)
            and isinstance(coordinate[1], int)
            and 0 <= coordinate[0] < 40
            and 0 <= coordinate[1] < 40
        ):
            failures.append(f"Invalid map tile coordinate {coordinate}")
        tile_type_id = tile.get("tile_id")
        if tile_type_id not in tile_type_ids:
            failures.append(
                f"Map coordinate {coordinate} uses unknown {tile_type_id}"
            )
        usage[tile_type_id] += 1
        if not is_matrix(tile.get("template_to_world_matrix")):
            failures.append(
                f"Map coordinate {coordinate} has invalid transform matrix"
            )
    expected_coordinates = {
        (x, y)
        for y in range(40)
        for x in range(40)
    }
    if coordinates != expected_coordinates:
        failures.append("Map does not cover each 40x40 coordinate exactly once")
    expected_usage = {
        tile_type_id: usage[tile_type_id]
        for tile_type_id in tile_type_ids
    }
    if map_record.get("tile_type_usage") != expected_usage:
        failures.append("Map tile_type_usage does not match 1600 tile records")


def validate_lod_contract(manifest, tile_type_ids, failures):
    lod_policy = manifest.get("lod_policy", {})
    if lod_policy.get("selection_semantics") != "first_max_inclusive":
        failures.append("LOD selection semantics is not first-max-inclusive")
    if lod_policy.get("dxf_thresholds") != LOD_MAX_DXF:
        failures.append(
            f"LOD thresholds {lod_policy.get('dxf_thresholds')} != "
            f"{LOD_MAX_DXF}"
        )
    levels = lod_policy.get("levels", [])
    if len(levels) != 6:
        failures.append(f"LOD level count {len(levels)} != 6")
    for lod, expected_max in enumerate(LOD_MAX_DXF):
        if lod >= len(levels):
            break
        level = levels[lod]
        if level.get("lod") != lod:
            failures.append(f"LOD policy index {lod} id mismatch")
        if level.get("max_dxf_inclusive") != expected_max:
            failures.append(f"LOD{lod} max dxf mismatch")
        expected_min = None if lod == 0 else LOD_MAX_DXF[lod - 1]
        if level.get("min_dxf_exclusive") != expected_min:
            failures.append(f"LOD{lod} min dxf mismatch")
        if level.get("selection_rule") != (
            "first_max_dxf_greater_than_or_equal"
        ):
            failures.append(f"LOD{lod} selection rule mismatch")
    coverage = lod_policy.get("far_addressable_coverage", {})
    for lod in FAR_LODS:
        record = coverage.get(str(lod), {})
        expected_extra = KNOWN_EXTRA_LOD_ADDRESS_TEMPLATE.format(lod=lod)
        if record.get("address_count") != 67:
            failures.append(f"LOD{lod} Addressable count is not 67")
        if record.get("primary_tile_address_count") != len(tile_type_ids):
            failures.append(f"LOD{lod} primary address count mismatch")
        if record.get("extra_addresses") != [expected_extra]:
            failures.append(f"LOD{lod} extra Addressable mismatch")


def validate_tile_type_bindings(manifest, tile_type_ids, failures):
    tile_types = manifest.get("tile_types", [])
    if [item.get("tile_type_id") for item in tile_types] != tile_type_ids:
        failures.append("Tile type runtime bindings are not sorted/canonical")
    for tile_type in tile_types:
        tile_type_id = tile_type.get("tile_type_id")
        bindings = tile_type.get("lod_bindings", [])
        if [item.get("lod") for item in bindings] != list(range(6)):
            failures.append(
                f"{tile_type_id} does not bind each LOD0-5 exactly once"
            )
            continue
        for lod in (0, 1):
            binding = bindings[lod]
            if binding != {
                "lod": lod,
                "representation": "detailed_template",
                "template_id": f"{tile_type_id}:detailed",
            }:
                failures.append(
                    f"{tile_type_id} LOD{lod} detailed binding mismatch"
                )
        for lod in FAR_LODS:
            binding = bindings[lod]
            if binding != {
                "lod": lod,
                "representation": "whole_tile_recursive_prefab_recipe",
                "recipe_id": f"{tile_type_id}:lod{lod}",
            }:
                failures.append(
                    f"{tile_type_id} LOD{lod} far binding mismatch"
                )


def validate_far_recipes(manifest, tile_type_ids, failures):
    recipes = manifest.get("far_lod_recipes", [])
    expected_keys = [
        (tile_type_id, lod)
        for tile_type_id in tile_type_ids
        for lod in FAR_LODS
    ]
    actual_keys = [
        (item.get("tile_type_id"), item.get("lod"))
        for item in recipes
    ]
    if actual_keys != expected_keys:
        failures.append(
            "Far LOD recipes are not exactly 66 tile types x LOD2-5"
        )

    aggregate_node_counts = Counter()
    aggregate_component_counts = Counter()
    aggregate_script_counts = Counter()
    aggregate_empty_assets = []
    source_prefab_paths = set()

    for recipe in recipes:
        tile_type_id = recipe.get("tile_type_id")
        lod = recipe.get("lod")
        recipe_id = recipe.get("recipe_id")
        context = {
            "tile_type_id": tile_type_id,
            "lod": lod,
            "recipe_id": recipe_id,
        }
        expected_recipe_id = f"{tile_type_id}:lod{lod}"
        if recipe_id != expected_recipe_id:
            failures.append(f"Recipe {recipe_id} id mismatch")
        if recipe.get("representation") != (
            "whole_tile_recursive_prefab_recipe"
        ):
            failures.append(f"{recipe_id} representation mismatch")
        if recipe.get("address") != f"{tile_type_id}_lod{lod}":
            failures.append(f"{recipe_id} Addressable address mismatch")
        guid = recipe.get("address_guid", "")
        if not GUID_PATTERN.fullmatch(guid):
            failures.append(f"{recipe_id} Addressable GUID is invalid")
        source_prefab = recipe.get("source_prefab", {})
        source_record_is_current(
            source_prefab,
            f"{recipe_id} prefab",
            failures,
        )
        source_path = source_prefab.get("path", "")
        if source_path in source_prefab_paths:
            failures.append(
                f"{recipe_id} reuses source prefab {source_path}"
            )
        source_prefab_paths.add(source_path)
        if recipe.get("source_prefab_file_id_policy") != (
            "signed_decimal_string"
        ):
            failures.append(f"{recipe_id} fileID policy mismatch")
        if recipe.get("root_count") != 1:
            failures.append(f"{recipe_id} does not have exactly one root")
        class_counts = recipe.get("unity_object_class_counts", {})
        if class_counts.get("1001", 0) != 0:
            failures.append(f"{recipe_id} contains PrefabInstance objects")

        node_counts = Counter(node_count=0, max_depth=0)
        component_counts = Counter()
        script_counts = Counter()
        empty_assets = []
        source_file_ids = set()
        validate_node(
            recipe.get("root", {}),
            context,
            "0",
            None,
            0,
            True,
            node_counts,
            component_counts,
            script_counts,
            empty_assets,
            source_file_ids,
            failures,
        )
        if node_counts["node_count"] != recipe.get("node_count"):
            failures.append(f"{recipe_id} node count mismatch")
        if node_counts["max_depth"] != recipe.get("max_node_depth"):
            failures.append(f"{recipe_id} max depth mismatch")
        if dict(sorted(component_counts.items())) != recipe.get(
            "component_counts"
        ):
            failures.append(f"{recipe_id} component summary mismatch")
        if dict(sorted(script_counts.items())) != recipe.get(
            "mono_behaviour_script_counts"
        ):
            failures.append(f"{recipe_id} script summary mismatch")
        expected_source_object_count = sum(
            int(count)
            for count in class_counts.values()
        )
        if len(source_file_ids) != expected_source_object_count:
            failures.append(
                f"{recipe_id} traversed object count "
                f"{len(source_file_ids)} != {expected_source_object_count}"
            )

        recipe_empty_assets = [
            {
                "tile_type_id": item.get("tile_type_id"),
                "lod": item.get("lod"),
                "node_name": item.get("node_name"),
                "sprite_renderer_file_id": item.get(
                    "sprite_renderer_file_id"
                ),
            }
            for item in recipe.get("empty_assets", [])
        ]
        if recipe_empty_assets != empty_assets:
            failures.append(f"{recipe_id} empty asset summary mismatch")

        aggregate_node_counts["node_count"] += node_counts["node_count"]
        aggregate_node_counts["max_depth"] = max(
            aggregate_node_counts["max_depth"],
            node_counts["max_depth"],
        )
        aggregate_component_counts.update(component_counts)
        aggregate_script_counts.update(script_counts)
        aggregate_empty_assets.extend(empty_assets)

    return {
        "node_count": aggregate_node_counts["node_count"],
        "max_node_depth": aggregate_node_counts["max_depth"],
        "mesh_renderer_count": aggregate_component_counts["MeshRenderer"],
        "mesh_filter_count": aggregate_component_counts["MeshFilter"],
        "sprite_renderer_count": aggregate_component_counts["SpriteRenderer"],
        "particle_system_count": aggregate_component_counts["ParticleSystem"],
        "particle_system_renderer_count": aggregate_component_counts[
            "ParticleSystemRenderer"
        ],
        "mesh_collider_count": aggregate_component_counts["MeshCollider"],
        "mono_behaviour_count": aggregate_component_counts["MonoBehaviour"],
        "empty_sprite_asset_count": len(aggregate_empty_assets),
        "mono_behaviour_script_counts": dict(
            sorted(aggregate_script_counts.items())
        ),
        "empty_assets": aggregate_empty_assets,
        "source_prefab_count": len(source_prefab_paths),
    }


def validate_pending_and_readiness(manifest, aggregate, failures):
    pending = manifest.get("pending_policies", {})
    particle = pending.get("particle_systems", {})
    collider = pending.get("mesh_colliders", {})
    empty = pending.get("empty_sprite_assets", {})
    if particle.get("status") != "pending":
        failures.append("Particle policy is not pending")
    if particle.get("component_count") != aggregate[
        "particle_system_count"
    ]:
        failures.append("Particle pending count mismatch")
    if particle.get("renderer_count") != aggregate[
        "particle_system_renderer_count"
    ]:
        failures.append("Particle renderer pending count mismatch")
    if collider.get("status") != "pending":
        failures.append("MeshCollider policy is not pending")
    if collider.get("component_count") != aggregate[
        "mesh_collider_count"
    ]:
        failures.append("MeshCollider pending count mismatch")
    if empty.get("status") != "source_explicit_noop":
        failures.append("Null SpriteRenderer policy is not source noop")
    if empty.get("component_count") != aggregate[
        "empty_sprite_asset_count"
    ]:
        failures.append("Null SpriteRenderer pending count mismatch")
    expected_empty = [
        {
            "tile_type_id": KNOWN_EMPTY_SPRITE_TILE_TYPE,
            "lod": lod,
            "node_name": KNOWN_EMPTY_SPRITE_NODE,
        }
        for lod in FAR_LODS
    ]
    actual_empty = [
        {
            "tile_type_id": item["tile_type_id"],
            "lod": item["lod"],
            "node_name": item["node_name"],
        }
        for item in aggregate["empty_assets"]
    ]
    if actual_empty != expected_empty:
        failures.append(
            f"Known null SpriteRenderer set mismatch: {actual_empty}"
        )

    readiness = manifest.get("runtime_readiness", {})
    if readiness.get("runtime_ready") is not False:
        failures.append("Runtime data incorrectly claims runtime_ready=true")
    expected_blockers = {
        "particle_systems_pending_niagara_or_omission_policy",
        "mesh_colliders_pending_runtime_collision_policy",
        "cookable_primary_data_asset_not_generated_by_this_script",
    }
    if set(readiness.get("blockers", [])) != expected_blockers:
        failures.append("Runtime readiness blockers mismatch")


def build_result(failures, manifest, rebuilt_hashes):
    statistics = manifest.get("statistics", {})
    return {
        "manifest": str(OUTPUT_PATH.relative_to(PROJECT_ROOT)).replace(
            "\\",
            "/",
        ),
        "manifest_file_sha256": (
            sha256(OUTPUT_PATH)
            if OUTPUT_PATH.exists()
            else ""
        ),
        "canonical_payload_sha256": manifest.get(
            "canonical_payload_sha256",
            "",
        ),
        "deterministic_rebuild_hashes": rebuilt_hashes,
        "failure_count": len(failures),
        "failures": failures,
        "runtime_ready": manifest.get(
            "runtime_readiness",
            {},
        ).get("runtime_ready"),
        "statistics": statistics,
        "pending_summary": {
            "particle_system_count": statistics.get(
                "particle_system_count",
                0,
            ),
            "mesh_collider_count": statistics.get(
                "mesh_collider_count",
                0,
            ),
            "empty_sprite_asset_count": statistics.get(
                "empty_sprite_asset_count",
                0,
            ),
        },
    }


def main():
    failures = []
    rebuilt_hashes = []
    if not OUTPUT_PATH.exists():
        manifest = {}
        failures.append(f"Missing generated runtime data: {OUTPUT_PATH}")
    else:
        try:
            manifest = json.loads(OUTPUT_PATH.read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            manifest = {}
            failures.append(f"Could not load generated runtime data: {error}")

    if manifest.get("schema") != SCHEMA:
        failures.append(
            f"Unexpected runtime-data schema: {manifest.get('schema')}"
        )
    stored_payload_hash = manifest.get("canonical_payload_sha256", "")
    try:
        actual_payload_hash = canonical_payload_hash(manifest)
    except (TypeError, ValueError) as error:
        actual_payload_hash = ""
        failures.append(f"Could not hash runtime payload: {error}")
    if stored_payload_hash != actual_payload_hash:
        failures.append(
            "Stored canonical payload hash does not match generated content"
        )

    for role, record in manifest.get("sources", {}).items():
        source_record_is_current(record, f"Top-level {role}", failures)

    validate_lod_contract(
        manifest,
        sorted(
            item.get("tile_type_id", "")
            for item in manifest.get("tile_types", [])
        ),
        failures,
    )
    tile_type_ids = sorted(
        item.get("tile_type_id", "")
        for item in manifest.get("tile_types", [])
    )
    if len(tile_type_ids) != 66 or len(set(tile_type_ids)) != 66:
        failures.append("Runtime data does not contain 66 unique tile types")
    validate_map(manifest, tile_type_ids, failures)
    validate_detailed_templates(manifest, tile_type_ids, failures)
    validate_tile_type_bindings(manifest, tile_type_ids, failures)
    aggregate = validate_far_recipes(
        manifest,
        tile_type_ids,
        failures,
    )

    statistics = manifest.get("statistics", {})
    for key, expected in EXPECTED_STATISTICS.items():
        if statistics.get(key) != expected:
            failures.append(
                f"Statistics {key} {statistics.get(key)} != {expected}"
            )
    aggregate_to_statistics = {
        "node_count": "node_count",
        "max_node_depth": "max_node_depth",
        "mesh_renderer_count": "mesh_renderer_count",
        "mesh_filter_count": "mesh_filter_count",
        "sprite_renderer_count": "sprite_renderer_count",
        "particle_system_count": "particle_system_count",
        "particle_system_renderer_count": "particle_system_renderer_count",
        "mesh_collider_count": "mesh_collider_count",
        "mono_behaviour_count": "mono_behaviour_count",
        "empty_sprite_asset_count": "empty_sprite_asset_count",
        "source_prefab_count": "far_lod_source_prefab_count",
    }
    for aggregate_key, statistics_key in aggregate_to_statistics.items():
        if aggregate.get(aggregate_key) != statistics.get(statistics_key):
            failures.append(
                f"Aggregate {aggregate_key} {aggregate.get(aggregate_key)} "
                f"!= statistics {statistics_key} "
                f"{statistics.get(statistics_key)}"
            )
    if aggregate.get("mono_behaviour_script_counts") != statistics.get(
        "mono_behaviour_script_counts"
    ):
        failures.append("Aggregate MonoBehaviour script counts mismatch")
    if statistics.get("mono_behaviour_script_counts") != {
        "LockAngle45": 1433,
        "ObjectPoolItem": 2113,
    }:
        failures.append("Unexpected LockAngle45/ObjectPoolItem counts")
    validate_pending_and_readiness(manifest, aggregate, failures)

    try:
        rebuilt = build_runtime_manifest()
        rebuilt_hashes.append(rebuilt["canonical_payload_sha256"])
        if manifest != rebuilt:
            failures.append(
                "Generated runtime data is not the current deterministic "
                "source extraction"
            )
        del rebuilt
        gc.collect()
        rebuilt_again = build_runtime_manifest()
        rebuilt_hashes.append(
            rebuilt_again["canonical_payload_sha256"]
        )
        del rebuilt_again
        gc.collect()
        if len(set(rebuilt_hashes)) != 1:
            failures.append(
                f"Two in-memory rebuild hashes differ: {rebuilt_hashes}"
            )
        if rebuilt_hashes and rebuilt_hashes[0] != stored_payload_hash:
            failures.append(
                "Deterministic rebuild hash differs from generated payload"
            )
    except Exception as error:
        failures.append(
            f"Could not deterministically rebuild runtime data: "
            f"{type(error).__name__}: {error}"
        )

    result = build_result(failures, manifest, rebuilt_hashes)
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    VERIFY_PATH.write_text(
        json.dumps(
            result,
            ensure_ascii=False,
            sort_keys=True,
            indent=2,
            allow_nan=False,
        )
        + "\n",
        encoding="utf-8",
    )

    if failures:
        print("Rok Unity world-map runtime data verification FAILED")
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)

    print(
        "Rok Unity world-map runtime data verification passed: "
        f"tiles={statistics['map_tile_count']} "
        f"tile_types={statistics['tile_type_count']} "
        f"detailed={statistics['detailed_template_count']} "
        f"far_recipes={statistics['far_lod_recipe_count']} "
        f"nodes={statistics['node_count']} "
        f"mesh_pairs={statistics['mesh_renderer_count']} "
        f"sprites={statistics['sprite_renderer_count']} "
        f"particles={statistics['particle_system_count']} "
        f"colliders={statistics['mesh_collider_count']} "
        f"runtime_ready=false "
        f"hash={stored_payload_hash}"
    )


if __name__ == "__main__":
    main()
