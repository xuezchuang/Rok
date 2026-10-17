import hashlib
import json
import math
import re
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
MIGRATION_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
WORLD_MANIFEST_PATH = MIGRATION_DIR / "unity_world_map.json"
ASSET_MANIFEST_PATH = MIGRATION_DIR / "unity_world_map_assets.json"
GENERATION_PATH = MIGRATION_DIR / "unity_world_map_ue_generation.json"
VERIFY_PATH = MIGRATION_DIR / "unity_world_map_ue_verify.json"
TARGET_LEVEL = "/Game/Maps/L_RokUnityWorldMapPrototype"
PLAYER_CITY_LEVEL = "/Game/Maps/L_RokUnityGameAppCity"
TARGET_LEVEL_FILE = (
    PROJECT_ROOT
    / "Content"
    / "Maps"
    / "L_RokUnityWorldMapPrototype.umap"
)
EXPECTED_GAME_MODE = "/Script/Rok.RokWorldMapGameMode"
EXPECTED_CONFIG_CLASS = "RokWorldMapConfigActor"
EXPECTED_CITY_CLASS = "RokWorldMapCityActor"
EXPECTED_CITY_CLASS_PATH = "/Script/Rok.RokWorldMapCityActor"
TREE_SOURCE_MATERIAL_NAMES = {
    "map_tree_nomask.mat",
    "map_tree_nomask_1.mat",
}
TREE_UE_MATERIAL_NAMES = [
    "M_WorldSource_map_tree_nomask",
    "M_WorldSource_map_tree_nomask_1",
]
MOUNTAIN_UE_MATERIAL_NAMES = [
    "M_WorldSource_build_mountain_A",
    "M_WorldSource_build_mountain_F",
]
MOUNTAIN_BODY_MESH_DESTINATION = (
    "/Game/RokUnityWorldMapPrototype/Meshes/MountainBodies"
)
RIVER_LAYER_MESH_DESTINATION = (
    "/Game/RokUnityWorldMapPrototype/Meshes/RiverLayers"
)
RIVER_REPRESENTATION = "exact_source_fbx_render_layers"
RIVER_FOLDER = "RokUnityWorldMap/SourceObjects/RiverLayers"
RIVER_EDGE_MATERIAL_ASSET = (
    "/Game/RokUnityWorldMapPrototype/Materials/"
    "M_WorldRiver_Edge.M_WorldRiver_Edge"
)
RIVER_WATER_MATERIAL_ASSET = (
    "/Game/RokUnityWorldMapPrototype/Materials/"
    "M_WorldRiver_Water.M_WorldRiver_Water"
)
RIVER_EDGE_TEXTURE_ASSET = (
    "/Game/RokUnityWorldMapPrototype/Textures/"
    "T_WorldRiver_Edge.T_WorldRiver_Edge"
)
RIVER_WATER_TEXTURE_ASSET = (
    "/Game/RokUnityWorldMapPrototype/Textures/"
    "T_WorldRiver_Bump.T_WorldRiver_Bump"
)
RIVER_PARAMETER_NAMES = {
    "ambient": "RiverEdgeAmbientColor",
    "direction_color": "RiverEdgeDirectionColor",
    "direction_intensity": "RiverEdgeDirectionIntensity",
    "light_direction": "RiverEdgeLightDirection",
    "bump_tiling": "RiverBumpTiling",
    "speed": "RiverSpeed",
    "base_color": "RiverBaseColor",
    "near": "RiverNear",
    "far": "RiverFar",
    "shininess": "RiverShininess",
    "gloss": "RiverGloss",
}
SPRITE_COLOR_PARAMETER = "UnitySpriteColor"
BACKGROUND_COLOR_PARAMETER = "UnityCameraBackgroundColor"
BACKGROUND_ACTOR_LABEL = "RokWorldMap_SolidColorUnderlay"
BACKGROUND_MATERIAL_ASSET = (
    "/Game/RokUnityWorldMapPrototype/Materials/"
    "M_WorldMapCameraBackground.M_WorldMapCameraBackground"
)
PLANE_MESH_ASSET = (
    "/Game/RokUnityWorldMapPrototype/Meshes/"
    "SM_RokWorld_Plane60.SM_RokWorld_Plane60"
)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_record(path):
    return {
        "path": str(path.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def safe_name(value):
    cleaned = re.sub(r"[^A-Za-z0-9_]+", "_", str(value)).strip("_")
    return cleaned or "Unnamed"


def load_asset(path):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return None
    return unreal.EditorAssetLibrary.load_asset(path)


def tags(actor):
    return {str(tag) for tag in actor.tags}


def vector_values(vector):
    return [float(vector.x), float(vector.y), float(vector.z)]


def verify_vector(
    observed,
    expected,
    role,
    failures,
    tolerance,
):
    if len(observed) != len(expected):
        failures.append(
            f"{role} has {len(observed)} values, "
            f"expected {len(expected)}"
        )
        return
    axes = ("x", "y", "z", "w")
    for index in range(len(expected)):
        axis = axes[index] if index < len(axes) else str(index)
        if abs(float(observed[index]) - float(expected[index])) > tolerance:
            failures.append(
                f"{role} {axis} is {observed[index]}, "
                f"expected {expected[index]}"
            )


def mesh_path(actor):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        return ""
    mesh = component.get_editor_property("static_mesh")
    return mesh.get_path_name() if mesh else ""


def material_paths(actor):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        return []
    paths = []
    for index in range(component.get_num_materials()):
        material = component.get_material(index)
        if material:
            paths.append(material.get_path_name())
    return paths


def normalized_angle_delta(observed, expected):
    return abs((float(observed) - float(expected) + 180.0) % 360.0 - 180.0)


def verify_actor_transform(
    actor,
    expected_location,
    expected_rotation,
    expected_scale,
    role,
    failures,
):
    verify_vector(
        vector_values(actor.get_actor_location()),
        [
            expected_location["x"],
            expected_location["y"],
            expected_location["z"],
        ],
        f"{role} location",
        failures,
        0.05,
    )
    rotation = actor.get_actor_rotation()
    for axis in ("pitch", "yaw", "roll"):
        if normalized_angle_delta(
            getattr(rotation, axis),
            expected_rotation.get(axis, 0.0),
        ) > 0.05:
            failures.append(
                f"{role} {axis} is {getattr(rotation, axis)}, "
                f"expected {expected_rotation.get(axis, 0.0)}"
            )
    verify_vector(
        vector_values(actor.get_actor_scale3d()),
        [
            expected_scale["x"],
            expected_scale["y"],
            expected_scale["z"],
        ],
        f"{role} scale",
        failures,
        0.0001,
    )


def verify_required_tags(actor, required_tags, role, failures):
    missing = sorted(set(required_tags) - tags(actor))
    if missing:
        failures.append(
            f"{role} lacks source tags: {', '.join(missing)}"
        )


def verify_static_mesh_binding(
    actor,
    expected_mesh,
    expected_material,
    role,
    failures,
):
    observed_mesh = mesh_path(actor)
    if observed_mesh != expected_mesh:
        failures.append(
            f"{role} mesh is {observed_mesh}, expected {expected_mesh}"
        )
    observed_materials = material_paths(actor)
    if not observed_materials or any(
        path != expected_material
        for path in observed_materials
    ):
        failures.append(
            f"{role} materials are {observed_materials}, "
            f"expected only {expected_material}"
        )


def prototype_lighting_phase(world_manifest):
    runtime_lighting = world_manifest["environment"]["runtime_lighting"]
    phase_name = runtime_lighting["prototype_phase"]
    matches = [
        phase
        for phase in runtime_lighting["phases"]
        if phase["phase"] == phase_name
    ]
    return matches[0] if len(matches) == 1 else None


def verify_linear_color(observed, expected, role, failures):
    verify_vector(
        [
            float(observed.r),
            float(observed.g),
            float(observed.b),
            float(observed.a),
        ],
        [
            float(expected["r"]),
            float(expected["g"]),
            float(expected["b"]),
            float(expected.get("a", 1.0)),
        ],
        role,
        failures,
        0.0001,
    )


def verify_unity_sprite_color_material(
    material,
    expected_sprite_color,
    role,
    failures,
):
    expressions = list(
        unreal.MaterialEditingLibrary.get_material_expressions(material)
    )
    parameters = [
        expression
        for expression in expressions
        if isinstance(
            expression,
            unreal.MaterialExpressionVectorParameter,
        )
        and str(expression.get_editor_property("parameter_name"))
        == SPRITE_COLOR_PARAMETER
    ]
    if len(parameters) != 1:
        failures.append(
            f"{role} has {len(parameters)} {SPRITE_COLOR_PARAMETER} "
            "parameters, expected one"
        )
    else:
        verify_linear_color(
            parameters[0].get_editor_property("default_value"),
            expected_sprite_color,
            f"{role} {SPRITE_COLOR_PARAMETER}",
            failures,
        )
    emissive_input = (
        unreal.MaterialEditingLibrary.get_material_property_input_node(
            material,
            unreal.MaterialProperty.MP_EMISSIVE_COLOR,
        )
    )
    if not isinstance(
        emissive_input,
        unreal.MaterialExpressionMultiply,
    ):
        failures.append(
            f"{role} does not multiply source RGB by "
            f"{SPRITE_COLOR_PARAMETER}"
        )
        return
    inputs = set(
        unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
            material,
            emissive_input,
        )
    )
    if not any(
        isinstance(item, unreal.MaterialExpressionTextureSample)
        for item in inputs
    ):
        failures.append(f"{role} sprite-color multiply lacks source texture")
    if parameters and parameters[0] not in inputs:
        failures.append(
            f"{role} sprite-color parameter is not connected to Emissive"
        )


def verify_solid_background_material(
    material,
    expected_color,
    failures,
):
    if material.get_editor_property("blend_mode") != (
        unreal.BlendMode.BLEND_OPAQUE
    ):
        failures.append("World background material is not opaque")
    if material.get_editor_property("shading_model") != (
        unreal.MaterialShadingModel.MSM_UNLIT
    ):
        failures.append("World background material is not unlit")
    emissive_input = (
        unreal.MaterialEditingLibrary.get_material_property_input_node(
            material,
            unreal.MaterialProperty.MP_EMISSIVE_COLOR,
        )
    )
    if not isinstance(
        emissive_input,
        unreal.MaterialExpressionVectorParameter,
    ):
        failures.append(
            "World background Emissive is not driven by its source color"
        )
        return
    parameter_name = str(
        emissive_input.get_editor_property("parameter_name")
    )
    if parameter_name != BACKGROUND_COLOR_PARAMETER:
        failures.append(
            f"World background parameter is {parameter_name}, expected "
            f"{BACKGROUND_COLOR_PARAMETER}"
        )
    verify_linear_color(
        emissive_input.get_editor_property("default_value"),
        expected_color,
        f"World background {BACKGROUND_COLOR_PARAMETER}",
        failures,
    )


def verify_ground_texture_clamp(material, role, failures):
    texture_samples = [
        expression
        for expression in (
            unreal.MaterialEditingLibrary.get_material_expressions(material)
        )
        if isinstance(
            expression,
            unreal.MaterialExpressionTextureSample,
        )
    ]
    if len(texture_samples) != 1:
        failures.append(
            f"{role} has {len(texture_samples)} source texture samples, "
            "expected one"
        )
        return
    texture = texture_samples[0].get_editor_property("texture")
    if not isinstance(texture, unreal.Texture2D):
        failures.append(f"{role} has no Texture2D source")
        return
    if texture.get_editor_property("address_x") != unreal.TextureAddress.TA_CLAMP:
        failures.append(f"{role} texture AddressX is not Clamp")
    if texture.get_editor_property("address_y") != unreal.TextureAddress.TA_CLAMP:
        failures.append(f"{role} texture AddressY is not Clamp")


def expression_inputs(material, expression):
    return [
        node
        for node in (
            unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
                material,
                expression,
            )
        )
        if node is not None
    ]


def reachable_material_expressions(material, properties):
    pending = []
    for material_property in properties:
        node = (
            unreal.MaterialEditingLibrary.get_material_property_input_node(
                material,
                material_property,
            )
        )
        if node:
            pending.append(node)
    visited = set()
    ordered = []
    while pending:
        expression = pending.pop()
        expression_id = expression.get_path_name()
        if expression_id in visited:
            continue
        visited.add(expression_id)
        ordered.append(expression)
        pending.extend(expression_inputs(material, expression))
    return ordered


def expression_class_counts(expressions):
    counts = {}
    for expression in expressions:
        class_name = expression.get_class().get_name()
        counts[class_name] = counts.get(class_name, 0) + 1
    return counts


def expression_description(expression):
    try:
        return str(expression.get_editor_property("desc"))
    except Exception:
        return ""


def material_parameters(expressions, expression_class):
    return {
        str(expression.get_editor_property("parameter_name")): expression
        for expression in expressions
        if isinstance(expression, expression_class)
    }


def verify_linear_color_list(
    observed,
    expected,
    role,
    failures,
):
    verify_vector(
        [
            float(observed.r),
            float(observed.g),
            float(observed.b),
            float(observed.a),
        ],
        [float(value) for value in expected],
        role,
        failures,
        0.0001,
    )


def verify_texture_asset(
    texture,
    expected_path,
    expected_srgb,
    role,
    failures,
):
    if not isinstance(texture, unreal.Texture2D):
        failures.append(f"{role} has no Texture2D")
        return
    if texture.get_path_name() != expected_path:
        failures.append(
            f"{role} texture is {texture.get_path_name()}, "
            f"expected {expected_path}"
        )
    if bool(texture.get_editor_property("srgb")) != bool(expected_srgb):
        failures.append(
            f"{role} texture sRGB is not {bool(expected_srgb)}"
        )
    for property_name in ("address_x", "address_y"):
        if texture.get_editor_property(property_name) != (
            unreal.TextureAddress.TA_WRAP
        ):
            failures.append(
                f"{role} texture {property_name} is not Repeat/Wrap"
            )


def rotate_vector_by_quaternion(vector, quaternion):
    qx = float(quaternion["x"])
    qy = float(quaternion["y"])
    qz = float(quaternion["z"])
    qw = float(quaternion["w"])
    vx, vy, vz = (float(value) for value in vector)
    tx = 2.0 * (qy * vz - qz * vy)
    ty = 2.0 * (qz * vx - qx * vz)
    tz = 2.0 * (qx * vy - qy * vx)
    return [
        vx + qw * tx + (qy * tz - qz * ty),
        vy + qw * ty + (qz * tx - qx * tz),
        vz + qw * tz + (qx * ty - qy * tx),
    ]


def normalized_vector(values):
    length = sum(float(value) ** 2 for value in values) ** 0.5
    if length <= 1.0e-8:
        return []
    return [float(value) / length for value in values]


def source_light_direction_ue(world_manifest):
    quaternion = world_manifest["environment"]["directional_light"][
        "unity_local_rotation_quaternion"
    ]
    unity_direction = rotate_vector_by_quaternion(
        [0.0, 0.0, -1.0],
        quaternion,
    )
    return normalized_vector(
        [
            -unity_direction[0],
            unity_direction[2],
            unity_direction[1],
        ]
    )


def verify_river_edge_material(
    material,
    lighting_phase,
    light_direction,
    failures,
):
    role = "River edge material"
    if not isinstance(material, unreal.Material):
        failures.append(f"{role} is missing")
        return
    if material.get_path_name() != RIVER_EDGE_MATERIAL_ASSET:
        failures.append(
            f"{role} path is {material.get_path_name()}, "
            f"expected {RIVER_EDGE_MATERIAL_ASSET}"
        )
    if material.get_editor_property("blend_mode") != (
        unreal.BlendMode.BLEND_TRANSLUCENT
    ):
        failures.append(f"{role} is not translucent")
    if material.get_editor_property("shading_model") != (
        unreal.MaterialShadingModel.MSM_UNLIT
    ):
        failures.append(f"{role} is not unlit")
    if bool(material.get_editor_property("two_sided")):
        failures.append(f"{role} is unexpectedly two-sided")
    emissive = (
        unreal.MaterialEditingLibrary.get_material_property_input_node(
            material,
            unreal.MaterialProperty.MP_EMISSIVE_COLOR,
        )
    )
    opacity = (
        unreal.MaterialEditingLibrary.get_material_property_input_node(
            material,
            unreal.MaterialProperty.MP_OPACITY,
        )
    )
    if not isinstance(emissive, unreal.MaterialExpressionMultiply):
        failures.append(
            f"{role} Emissive is not the final texture-lighting multiply"
        )
    elif "RokRiverEdgeExact:" not in expression_description(emissive):
        failures.append(f"{role} lacks its exact formula evidence marker")
    if not isinstance(opacity, unreal.MaterialExpressionVertexColor):
        failures.append(f"{role} Opacity is not VertexColor.A")
    expressions = reachable_material_expressions(
        material,
        (
            unreal.MaterialProperty.MP_EMISSIVE_COLOR,
            unreal.MaterialProperty.MP_OPACITY,
        ),
    )
    expected_counts = {
        "MaterialExpressionTextureSample": 1,
        "MaterialExpressionVectorParameter": 3,
        "MaterialExpressionScalarParameter": 1,
        "MaterialExpressionVertexNormalWS": 1,
        "MaterialExpressionNormalize": 1,
        "MaterialExpressionDotProduct": 1,
        "MaterialExpressionMax": 1,
        "MaterialExpressionVertexColor": 1,
        "MaterialExpressionComponentMask": 2,
        "MaterialExpressionConstant": 2,
        "MaterialExpressionMultiply": 4,
        "MaterialExpressionAdd": 1,
    }
    counts = expression_class_counts(expressions)
    if set(counts) != set(expected_counts):
        failures.append(
            f"{role} expression classes are {sorted(counts)}, "
            f"expected {sorted(expected_counts)}"
        )
    for class_name, expected_count in expected_counts.items():
        if counts.get(class_name, 0) != expected_count:
            failures.append(
                f"{role} has {counts.get(class_name, 0)} {class_name}, "
                f"expected {expected_count}"
            )
    vector_parameters = material_parameters(
        expressions,
        unreal.MaterialExpressionVectorParameter,
    )
    scalar_parameters = material_parameters(
        expressions,
        unreal.MaterialExpressionScalarParameter,
    )
    expected_vectors = {
        RIVER_PARAMETER_NAMES["ambient"]: [
            lighting_phase["ambient_color"][channel]
            for channel in ("r", "g", "b", "a")
        ],
        RIVER_PARAMETER_NAMES["direction_color"]: [
            lighting_phase["direction_color"][channel]
            for channel in ("r", "g", "b", "a")
        ],
        RIVER_PARAMETER_NAMES["light_direction"]: [
            *light_direction,
            0.0,
        ],
    }
    if set(vector_parameters) != set(expected_vectors):
        failures.append(
            f"{role} vector parameters are "
            f"{sorted(vector_parameters)}, expected "
            f"{sorted(expected_vectors)}"
        )
    for name, expected in expected_vectors.items():
        if name in vector_parameters:
            verify_linear_color_list(
                vector_parameters[name].get_editor_property(
                    "default_value"
                ),
                expected,
                f"{role} {name}",
                failures,
            )
    expected_scalar_name = RIVER_PARAMETER_NAMES[
        "direction_intensity"
    ]
    if set(scalar_parameters) != {expected_scalar_name}:
        failures.append(
            f"{role} scalar parameters are "
            f"{sorted(scalar_parameters)}, expected "
            f"{expected_scalar_name}"
        )
    elif not math.isclose(
        float(
            scalar_parameters[expected_scalar_name].get_editor_property(
                "default_value"
            )
        ),
        float(lighting_phase["direction_intensity"]),
        rel_tol=0.0,
        abs_tol=0.0001,
    ):
        failures.append(
            f"{role} direction intensity differs from the source phase"
        )
    texture_samples = [
        expression
        for expression in expressions
        if isinstance(expression, unreal.MaterialExpressionTextureSample)
    ]
    if len(texture_samples) == 1:
        verify_texture_asset(
            texture_samples[0].get_editor_property("texture"),
            RIVER_EDGE_TEXTURE_ASSET,
            True,
            role,
            failures,
        )


def verify_river_water_material(material, shader_contract, failures):
    role = "River water material"
    if not isinstance(material, unreal.Material):
        failures.append(f"{role} is missing")
        return
    if material.get_path_name() != RIVER_WATER_MATERIAL_ASSET:
        failures.append(
            f"{role} path is {material.get_path_name()}, "
            f"expected {RIVER_WATER_MATERIAL_ASSET}"
        )
    if material.get_editor_property("blend_mode") != (
        unreal.BlendMode.BLEND_TRANSLUCENT
    ):
        failures.append(f"{role} is not translucent")
    if material.get_editor_property("shading_model") != (
        unreal.MaterialShadingModel.MSM_UNLIT
    ):
        failures.append(f"{role} is not unlit")
    if bool(material.get_editor_property("two_sided")):
        failures.append(f"{role} is unexpectedly two-sided")
    emissive = (
        unreal.MaterialEditingLibrary.get_material_property_input_node(
            material,
            unreal.MaterialProperty.MP_EMISSIVE_COLOR,
        )
    )
    opacity = (
        unreal.MaterialEditingLibrary.get_material_property_input_node(
            material,
            unreal.MaterialProperty.MP_OPACITY,
        )
    )
    if not isinstance(emissive, unreal.MaterialExpressionSaturate):
        failures.append(f"{role} RGB is not explicitly saturated")
    elif "RokRiverWaterExact:" not in expression_description(emissive):
        failures.append(f"{role} lacks its exact RGB formula marker")
    if not isinstance(opacity, unreal.MaterialExpressionSaturate):
        failures.append(f"{role} Alpha is not explicitly saturated")
    elif (
        "RokRiverWaterExactOpacity:"
        not in expression_description(opacity)
    ):
        failures.append(f"{role} lacks its exact alpha formula marker")
    expressions = reachable_material_expressions(
        material,
        (
            unreal.MaterialProperty.MP_EMISSIVE_COLOR,
            unreal.MaterialProperty.MP_OPACITY,
        ),
    )
    expected_counts = {
        "MaterialExpressionTextureSample": 2,
        "MaterialExpressionVectorParameter": 3,
        "MaterialExpressionScalarParameter": 4,
        "MaterialExpressionPreSkinnedPosition": 1,
        "MaterialExpressionTime": 1,
        "MaterialExpressionCameraPositionWS": 1,
        "MaterialExpressionVertexColor": 1,
        "MaterialExpressionLinearInterpolate": 1,
        "MaterialExpressionPower": 1,
        "MaterialExpressionSaturate": 2,
        "MaterialExpressionSubtract": 2,
        "MaterialExpressionDivide": 3,
        "MaterialExpressionComponentMask": 11,
        "MaterialExpressionConstant": 7,
        "MaterialExpressionMultiply": 10,
        "MaterialExpressionAdd": 4,
        "MaterialExpressionMax": 3,
    }
    counts = expression_class_counts(expressions)
    if set(counts) != set(expected_counts):
        failures.append(
            f"{role} expression classes are {sorted(counts)}, "
            f"expected {sorted(expected_counts)}"
        )
    for class_name, expected_count in expected_counts.items():
        if counts.get(class_name, 0) != expected_count:
            failures.append(
                f"{role} has {counts.get(class_name, 0)} {class_name}, "
                f"expected {expected_count}"
            )
    vector_parameters = material_parameters(
        expressions,
        unreal.MaterialExpressionVectorParameter,
    )
    scalar_parameters = material_parameters(
        expressions,
        unreal.MaterialExpressionScalarParameter,
    )
    expected_vectors = {
        RIVER_PARAMETER_NAMES["bump_tiling"]: shader_contract[
            "bump_tiling"
        ],
        RIVER_PARAMETER_NAMES["speed"]: shader_contract["speed"],
        RIVER_PARAMETER_NAMES["base_color"]: shader_contract[
            "base_color"
        ],
    }
    expected_scalars = {
        RIVER_PARAMETER_NAMES["near"]: shader_contract["near"],
        RIVER_PARAMETER_NAMES["far"]: shader_contract["far"],
        RIVER_PARAMETER_NAMES["shininess"]: shader_contract[
            "shininess"
        ],
        RIVER_PARAMETER_NAMES["gloss"]: shader_contract["gloss"],
    }
    if set(vector_parameters) != set(expected_vectors):
        failures.append(
            f"{role} vector parameters are "
            f"{sorted(vector_parameters)}, expected "
            f"{sorted(expected_vectors)}"
        )
    if set(scalar_parameters) != set(expected_scalars):
        failures.append(
            f"{role} scalar parameters are "
            f"{sorted(scalar_parameters)}, expected "
            f"{sorted(expected_scalars)}"
        )
    for name, expected in expected_vectors.items():
        if name in vector_parameters:
            verify_linear_color_list(
                vector_parameters[name].get_editor_property(
                    "default_value"
                ),
                expected,
                f"{role} {name}",
                failures,
            )
    for name, expected in expected_scalars.items():
        if name in scalar_parameters and not math.isclose(
            float(
                scalar_parameters[name].get_editor_property(
                    "default_value"
                )
            ),
            float(expected),
            rel_tol=0.0,
            abs_tol=0.0001,
        ):
            failures.append(
                f"{role} {name} differs from the source material"
            )
    texture_samples = [
        expression
        for expression in expressions
        if isinstance(expression, unreal.MaterialExpressionTextureSample)
    ]
    for sample_index, sample in enumerate(texture_samples):
        verify_texture_asset(
            sample.get_editor_property("texture"),
            RIVER_WATER_TEXTURE_ASSET,
            False,
            f"{role} sample {sample_index}",
            failures,
        )
        if sample.get_editor_property("sampler_type") != (
            unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
        ):
            failures.append(
                f"{role} sample {sample_index} is not raw linear color"
            )


def quaternion_multiply(left, right):
    lx, ly, lz, lw = left
    rx, ry, rz, rw = right
    return (
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
        lw * rw - lx * rx - ly * ry - lz * rz,
    )


def quaternion_to_rotator_record(quaternion):
    x, y, z, w = quaternion
    length = (x * x + y * y + z * z + w * w) ** 0.5
    if length <= 1.0e-12:
        return {"pitch": 0.0, "yaw": 0.0, "roll": 0.0}
    x, y, z, w = (
        x / length,
        y / length,
        z / length,
        w / length,
    )
    sin_pitch = max(
        -1.0,
        min(1.0, 2.0 * (w * y - z * x)),
    )
    return {
        "pitch": math.degrees(math.asin(sin_pitch)),
        "yaw": math.degrees(
            math.atan2(
                2.0 * (w * z + x * y),
                1.0 - 2.0 * (y * y + z * z),
            )
        ),
        "roll": math.degrees(
            math.atan2(
                2.0 * (w * x + y * z),
                1.0 - 2.0 * (x * x + y * y),
            )
        ),
    }


def compose_river_layer_transform(instance, layer):
    local = layer["transform"]
    local_position = local["local_position"]
    local_scale = local["local_scale"]
    local_rotation = local["local_rotation"]
    instance_location = instance["ue_relative_to_focus"]
    instance_scale = instance[
        "ue_actor_scale_for_centimeter_asset"
    ]
    yaw_radians = math.radians(float(instance["ue_yaw"]))
    local_ue_cm = (
        -float(local_position["x"]) * 100.0,
        float(local_position["z"]) * 100.0,
        float(local_position["y"]) * 100.0,
    )
    scaled_local = (
        local_ue_cm[0] * float(instance_scale["x"]),
        local_ue_cm[1] * float(instance_scale["y"]),
        local_ue_cm[2] * float(instance_scale["z"]),
    )
    cosine = math.cos(yaw_radians)
    sine = math.sin(yaw_radians)
    rotated_local = (
        scaled_local[0] * cosine - scaled_local[1] * sine,
        scaled_local[0] * sine + scaled_local[1] * cosine,
        scaled_local[2],
    )
    composed_rotation = quaternion_multiply(
        (
            0.0,
            0.0,
            math.sin(yaw_radians * 0.5),
            math.cos(yaw_radians * 0.5),
        ),
        (
            -float(local_rotation["x"]),
            float(local_rotation["z"]),
            float(local_rotation["y"]),
            float(local_rotation["w"]),
        ),
    )
    return {
        "location": {
            "x": float(instance_location["x"]) + rotated_local[0],
            "y": float(instance_location["y"]) + rotated_local[1],
            "z": float(instance_location["z"]) + rotated_local[2],
        },
        "rotation": quaternion_to_rotator_record(composed_rotation),
        "scale": {
            "x": float(instance_scale["x"])
            * float(local_scale["x"]),
            "y": float(instance_scale["y"])
            * float(local_scale["z"]),
            "z": float(instance_scale["z"])
            * float(local_scale["y"]),
        },
    }


def verify_river_mesh_import_settings(mesh, role, failures):
    if not isinstance(mesh, unreal.StaticMesh):
        failures.append(f"{role} has no StaticMesh")
        return
    try:
        import_data = mesh.get_editor_property("asset_import_data")
        vertex_color_option = import_data.get_editor_property(
            "vertex_color_import_option"
        )
        if "replace" not in str(vertex_color_option).lower():
            failures.append(
                f"{role} vertex-color import is {vertex_color_option}, "
                "expected Replace"
            )
    except Exception as error:
        failures.append(
            f"{role} vertex-color import settings are unreadable: {error}"
        )


def verify_collision(
    component,
    selectable,
    role,
    failures,
):
    try:
        observed_enabled = component.get_collision_enabled()
        expected_enabled = (
            unreal.CollisionEnabled.QUERY_ONLY
            if selectable
            else unreal.CollisionEnabled.NO_COLLISION
        )
        if observed_enabled != expected_enabled:
            failures.append(
                f"{role} collision is {observed_enabled}, "
                f"expected {expected_enabled}"
            )
        if selectable:
            response = component.get_collision_response_to_channel(
                unreal.CollisionChannel.ECC_VISIBILITY
            )
            if response != unreal.CollisionResponseType.ECR_BLOCK:
                failures.append(
                    f"{role} does not block Visibility selection traces"
                )
    except Exception as error:
        failures.append(f"{role} collision could not be read: {error}")


def verify_config_actor(config, world_manifest, failures):
    verify_actor_transform(
        config,
        {"x": 0.0, "y": 0.0, "z": 0.0},
        {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
        {"x": 1.0, "y": 1.0, "z": 1.0},
        "World-map config",
        failures,
    )
    verify_required_tags(
        config,
        {
            "RokWorldMapGenerated",
            "RokWorldMapConfig",
            "UnityMapBounds_0_7200",
            "UnityFocus_6876_942",
            "UnityCamera_map_tactical",
        },
        "World-map config",
        failures,
    )
    projection = config.get_editor_property("projection")
    projection_vectors = (
        ("unity_bounds_min", [0.0, 0.0]),
        ("unity_bounds_max", [7200.0, 7200.0]),
        ("unity_rebase_origin", [6876.0, 942.0]),
    )
    for property_name, expected in projection_vectors:
        observed = projection.get_editor_property(property_name)
        verify_vector(
            [float(observed.x), float(observed.y)],
            expected,
            f"Projection {property_name}",
            failures,
            0.001,
        )
    projection_scalars = (
        ("ue_units_per_unity_unit", 500.0),
        ("ground_z", 0.0),
        ("display_coordinate_scale", 6.0),
    )
    for property_name, expected in projection_scalars:
        observed = projection.get_editor_property(property_name)
        if not math.isclose(
            float(observed),
            expected,
            rel_tol=0.0,
            abs_tol=0.001,
        ):
            failures.append(
                f"Projection {property_name} is {observed}, "
                f"expected {expected}"
            )
    default_focus = config.get_editor_property("default_focus_unity")
    verify_vector(
        [float(default_focus.x), float(default_focus.y)],
        [6876.0, 942.0],
        "World-map default focus",
        failures,
        0.001,
    )
    camera_forward = config.get_editor_property("unity_camera_forward")
    expected_forward = world_manifest["map_tactical_camera"][
        "unity_forward"
    ]
    verify_vector(
        vector_values(camera_forward),
        [
            expected_forward["x"],
            expected_forward["y"],
            expected_forward["z"],
        ],
        "World-map Unity camera forward",
        failures,
        0.0001,
    )
    key_ids = (
        ("minimum_camera_key_id", "limit_min"),
        ("maximum_camera_key_id", "limit_max"),
        ("default_camera_key_id", "map_tactical"),
        ("city_entry_threshold_camera_key_id", "city_bound"),
        ("city_entry_target_camera_key_id", "city_default"),
    )
    for property_name, expected in key_ids:
        observed = str(config.get_editor_property(property_name))
        if observed != expected:
            failures.append(
                f"Config {property_name} is {observed}, expected {expected}"
            )
    if not bool(
        config.get_editor_property("enter_player_city_on_zoom")
    ):
        failures.append("Config disables player-city entry on zoom")
    transition_seconds = float(
        config.get_editor_property("city_entry_transition_seconds")
    )
    if not math.isclose(
        transition_seconds,
        0.5,
        rel_tol=0.0,
        abs_tol=0.001,
    ):
        failures.append(
            "Config city_entry_transition_seconds is "
            f"{transition_seconds}, expected 0.5"
        )
    player_city_level = config.get_editor_property("player_city_level")
    if hasattr(player_city_level, "get_path_name"):
        player_city_level_text = player_city_level.get_path_name()
    else:
        player_city_level_text = str(player_city_level)
    if PLAYER_CITY_LEVEL not in player_city_level_text:
        failures.append(
            "Config player_city_level is "
            f"{player_city_level_text}, expected {PLAYER_CITY_LEVEL}"
        )
    observed_keys = config.get_editor_property("camera_keys")
    expected_keys = world_manifest.get("camera_keys", [])
    if len(observed_keys) != len(expected_keys):
        failures.append(
            f"Config has {len(observed_keys)} camera keys, "
            f"expected {len(expected_keys)}"
        )
        return
    for index, (observed, expected) in enumerate(
        zip(observed_keys, expected_keys)
    ):
        if str(observed.get_editor_property("id")) != expected["id"]:
            failures.append(
                f"Config camera key {index} id differs from source"
            )
        for property_name, source_name in (
            ("view_distance", "distance_unity_units"),
            ("vertical_fov", "vertical_fov_degrees"),
            ("dxf", "dxf"),
        ):
            actual_value = observed.get_editor_property(property_name)
            if not math.isclose(
                float(actual_value),
                float(expected[source_name]),
                rel_tol=0.0,
                abs_tol=0.001,
            ):
                failures.append(
                    f"Config camera key {expected['id']} "
                    f"{property_name} differs from CameraParamLod.csv"
                )


def load_json(path, failures):
    if not path.exists():
        failures.append(f"Missing generated evidence: {path.name}")
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def ensure_target_level(failures):
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world and str(world.get_outermost().get_name()) == TARGET_LEVEL:
        return world
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        failures.append(f"Missing target level asset: {TARGET_LEVEL}")
        return world
    unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world or str(world.get_outermost().get_name()) != TARGET_LEVEL:
        failures.append(f"Could not load target level: {TARGET_LEVEL}")
    return world


def main():
    failures = []
    world_manifest = load_json(WORLD_MANIFEST_PATH, failures)
    asset_manifest = load_json(ASSET_MANIFEST_PATH, failures)
    generation = load_json(GENERATION_PATH, failures)
    world = ensure_target_level(failures)
    expected_collision_policy = {
        "owner": EXPECTED_GAME_MODE,
        "nonselectable_tags": [
            "RokWorldMapTerrain",
            "RokWorldMapSourceObject",
        ],
        "selectable_tag": "RokWorldMapSelectable",
        "nonselectable_collision": "NoCollision",
        "selectable_visual_collision": "NoCollision",
        "selectable_collision": "QueryOnly",
        "selection_component_tag": "RokWorldMapSelectionCollider",
        "selectable_trace_channel": "Visibility",
        "runtime_log_marker": "Rok world-map collision policy applied:",
    }
    collision_policy_valid = (
        generation.get("runtime_collision_policy")
        == expected_collision_policy
    )
    if not collision_policy_valid:
        failures.append(
            "Generation evidence lacks the exact runtime collision policy"
        )
    expected_tree_lod_policy = {
        "owner": "/Script/Rok.RokWorldMapCameraPawn",
        "source_actor_tag": "RokWorldMapSourceObject",
        "tree_actor_tag": "RokWorldMapTree",
        "legacy_tree_material_names": TREE_UE_MATERIAL_NAMES,
        "tree_translucency_sort_priority": 6,
        "bake_dxf": asset_manifest.get("tree_scale_bake_dxf"),
        "bake_scale": asset_manifest.get("tree_scale_bake_value"),
        "curve_source_sha256": asset_manifest.get(
            "source_semantics",
            {},
        ).get("tree", {}).get("source", {}).get("sha256"),
        "evaluation": "source_curve_scale_over_baked_scale",
        "runtime_log_marker": "Rok world-map tree LOD initialized:",
    }
    if generation.get("runtime_tree_lod_policy") != (
        expected_tree_lod_policy
    ):
        failures.append(
            "Generation evidence lacks the exact runtime tree LOD policy"
        )
    expected_player_city_entry_policy = {
        "owner": "/Script/Rok.RokWorldMapCameraPawn",
        "travel_owner": "/Script/Rok.RokWorldMapPlayerController",
        "player_city_tag": "RokWorldMapPlayerCity",
        "trigger": (
            "zoom_in_at_or_below_named_dxf_while_player_city_visible"
        ),
        "threshold_camera_key": "city_bound",
        "target_camera_key": "city_default",
        "transition_seconds": 0.5,
        "target_level": PLAYER_CITY_LEVEL,
        "entry_url_option": "RokCityEntryDxf",
        "runtime_log_markers": [
            "Rok world-map player-city entry started:",
            "Rok player-city level travel requested:",
            "Rok city entry camera initialized:",
        ],
    }
    if generation.get("runtime_player_city_entry_policy") != (
        expected_player_city_entry_policy
    ):
        failures.append(
            "Generation evidence lacks the exact player-city entry policy"
        )
    expected_mountain_render_policy = {
        "source_actor_tag": "RokWorldMapMountain",
        "source_contract": asset_manifest.get("mountain_render_contract"),
        "geometry_policy": "split_fbx_exact_body_only",
        "body_mesh_destination": MOUNTAIN_BODY_MESH_DESTINATION,
        "visible_material_slot_count": 1,
        "excluded_runtime_nodes": [
            "edge",
            "collision",
            "collision_da",
        ],
        "ue_material_names": MOUNTAIN_UE_MATERIAL_NAMES,
        "component_cast_shadow": False,
        "component_affects_distance_field_lighting": False,
        "component_affects_dynamic_indirect_lighting": False,
    }
    if generation.get("mountain_render_policy") != (
        expected_mountain_render_policy
    ):
        failures.append(
            "Generation evidence lacks the source-equivalent mountain "
            "render policy"
        )
    observed_mountain_body_imports = generation.get(
        "mountain_body_imports",
        {},
    )
    expected_mountain_prefabs = {
        prefab_id
        for prefab_id, record in asset_manifest.get(
            "object_sources",
            {},
        ).items()
        if record.get("representation") == "exact_source_fbx"
    }
    if set(observed_mountain_body_imports) != expected_mountain_prefabs:
        failures.append(
            "Generation evidence does not cover every split mountain body"
        )
    for prefab_id in expected_mountain_prefabs:
        source_record_for_mountain = asset_manifest["object_sources"][
            prefab_id
        ]
        expected_body_asset = source_record_for_mountain.get(
            "mountain_geometry_contract",
            {},
        ).get("ue_body_mesh_asset")
        observed_import = observed_mountain_body_imports.get(prefab_id, {})
        if observed_import.get("selected_body_asset") != expected_body_asset:
            failures.append(
                f"{prefab_id} generation evidence selects the wrong body asset"
            )
    river_source_records = {
        prefab_id: record
        for prefab_id, record in asset_manifest.get(
            "object_sources",
            {},
        ).items()
        if record.get("representation") == RIVER_REPRESENTATION
    }
    expected_river_logical_actor_count = sum(
        1
        for tile in world_manifest.get("prototype_window", {}).get(
            "tiles",
            [],
        )
        for instance in tile.get("instances", [])
        if instance.get("kind") == "object"
        and instance.get("prefab_id") in river_source_records
    )
    expected_river_physical_actor_count = (
        expected_river_logical_actor_count * 2
    )
    expected_deleted_collision_count = (
        0
        if generation.get("asset_build_mode")
        == "reused_existing_phase1_assets"
        else sum(
            len(
                record["river_geometry_contract"].get(
                    "excluded_fbx_nodes",
                    [],
                )
            )
            for record in river_source_records.values()
        )
    )
    river_lighting_phase = prototype_lighting_phase(world_manifest)
    first_river_source_record = (
        next(iter(river_source_records.values()))
        if river_source_records
        else None
    )
    expected_river_render_policy = {
        "representation": RIVER_REPRESENTATION,
        "geometry_policy": "split_fbx_exact_render_layers",
        "mesh_destination": RIVER_LAYER_MESH_DESTINATION,
        "combine_meshes": False,
        "vertex_color_import_option": "Replace",
        "collision_nodes": (
            "excluded_from_render_and_collision_generation"
        ),
        "logical_source_actor_tag": "RokWorldMapSourceObject",
        "logical_source_actor_role": "edge",
        "water_render_layer_actor_tag": (
            "RokWorldMapRiverRenderLayer"
        ),
        "actor_folder": RIVER_FOLDER,
        "edge_actor_label_policy": "logical_object_base_label",
        "water_actor_label_suffix": "_LayerWater",
        "render_layer_sort_priorities": {
            "edge": 0,
            "water": 1,
        },
        "component_collision": "NoCollision",
        "component_cast_shadow": False,
        "component_affects_distance_field_lighting": False,
        "component_affects_dynamic_indirect_lighting": False,
        "edge_texture_srgb": True,
        "water_texture_srgb": False,
        "texture_addressing": "Repeat",
        "edge_lighting_phase": (
            river_lighting_phase["phase"]
            if river_source_records and river_lighting_phase
            else None
        ),
        "edge_ambient_color": (
            river_lighting_phase["ambient_color"]
            if river_source_records and river_lighting_phase
            else None
        ),
        "edge_direction_color": (
            river_lighting_phase["direction_color"]
            if river_source_records and river_lighting_phase
            else None
        ),
        "edge_direction_intensity": (
            river_lighting_phase["direction_intensity"]
            if river_source_records and river_lighting_phase
            else None
        ),
        "edge_light_direction_ue": (
            source_light_direction_ue(world_manifest)
            if river_source_records
            else None
        ),
        "edge_material": (
            RIVER_EDGE_MATERIAL_ASSET
            if river_source_records
            else None
        ),
        "water_material": (
            RIVER_WATER_MATERIAL_ASSET
            if river_source_records
            else None
        ),
        "edge_texture": (
            RIVER_EDGE_TEXTURE_ASSET if river_source_records else None
        ),
        "water_texture": (
            RIVER_WATER_TEXTURE_ASSET if river_source_records else None
        ),
        "edge_formula": (
            "TextureRGB*(AmbientColor*2+DirectionColor*"
            "DirectionIntensity*max(0,dot(normalize("
            "VertexNormalWS),LightDirection)));"
            "Opacity=VertexColor.A"
        ),
        "water_formula": (
            "_Time.x=Time/20; dual local-position XZ scroll; "
            "Normal=(Sample0*2-1)+(Sample1*2-1)/2; "
            "zDis=max(0,CameraPositionWS.Z/35000); "
            "heightScale=lerp(Near,Far,zDis) without saturating "
            "zDis; normalY=Normal.G*heightScale; "
            "spec=max(0,pow(max(0,normalY),Shininess))*Gloss; "
            "RGB=saturate(BaseColor.rgb+spec); "
            "Alpha=saturate(BaseColor.a*VertexColor.a)"
        ),
        "water_dynamic_shader_contract": (
            first_river_source_record["river_geometry_contract"][
                "water_dynamic_shader_contract"
            ]
            if first_river_source_record
            else None
        ),
        "transform_policy": (
            "source_layer_local_trs_composed_with_logical_instance"
        ),
        "imported_prefab_count": len(river_source_records),
        "imported_render_layer_mesh_count": (
            len(river_source_records) * 2
        ),
        "deleted_collision_node_asset_count": (
            expected_deleted_collision_count
        ),
        "logical_actor_count": expected_river_logical_actor_count,
        "physical_render_layer_actor_count": (
            expected_river_physical_actor_count
        ),
    }
    if generation.get("river_render_policy") != (
        expected_river_render_policy
    ):
        failures.append(
            "Generation evidence lacks the exact river render policy"
        )
    observed_river_imports = generation.get(
        "river_layer_imports",
        {},
    )
    if set(observed_river_imports) != set(river_source_records):
        failures.append(
            "Generation evidence does not cover every exact river split "
            "import"
        )
    for prefab_id, source_record_for_river in (
        river_source_records.items()
    ):
        observed_import = observed_river_imports.get(prefab_id, {})
        contract = source_record_for_river["river_geometry_contract"]
        expected_layer_assets = {
            role: (
                f"{RIVER_LAYER_MESH_DESTINATION}/"
                f"SM_RokWorldRiver_{safe_name(prefab_id)}_"
                f"{safe_name(role).title()}."
                f"SM_RokWorldRiver_{safe_name(prefab_id)}_"
                f"{safe_name(role).title()}"
            )
            for role in ("edge", "water")
        }
        if observed_import.get("source_fbx") != (
            source_record_for_river.get("source_mesh")
        ):
            failures.append(
                f"{prefab_id} river import evidence changed its source FBX"
            )
        if observed_import.get("geometry_contract") != contract:
            failures.append(
                f"{prefab_id} river import evidence changed its contract"
            )
        if observed_import.get("selected_layer_assets") != (
            expected_layer_assets
        ):
            failures.append(
                f"{prefab_id} river import selected wrong layer assets"
            )
        if set(
            observed_import.get("imported_render_layer_assets", [])
        ) != set(expected_layer_assets.values()):
            failures.append(
                f"{prefab_id} river import evidence lacks both layer assets"
            )
        deleted_assets = observed_import.get(
            "deleted_collision_node_assets",
            [],
        )
        expected_deleted = (
            0
            if generation.get("asset_build_mode")
            == "reused_existing_phase1_assets"
            else len(contract.get("excluded_fbx_nodes", []))
        )
        if len(deleted_assets) != expected_deleted:
            failures.append(
                f"{prefab_id} river import deleted {len(deleted_assets)} "
                f"collision assets, expected {expected_deleted}"
            )
        if observed_import.get("vertex_color_import_option") != "Replace":
            failures.append(
                f"{prefab_id} river import did not preserve vertex colors"
            )
        for excluded_node in contract.get("excluded_fbx_nodes", []):
            excluded_package = (
                f"{RIVER_LAYER_MESH_DESTINATION}/"
                f"{safe_name(excluded_node)}"
            )
            if unreal.EditorAssetLibrary.does_asset_exist(
                excluded_package
            ):
                failures.append(
                    f"{prefab_id} excluded collision node still exists as "
                    f"{excluded_package}"
                )
        for layer_asset in expected_layer_assets.values():
            if not unreal.EditorAssetLibrary.does_asset_exist(
                layer_asset.split(".", 1)[0]
            ):
                failures.append(
                    f"{prefab_id} deterministic river layer asset is "
                    f"missing: {layer_asset}"
                )
    runtime_collision_normalization_labels = []
    tree_source_actor_count = 0
    mountain_source_actor_count = 0
    mountain_actor_audit = []
    river_logical_actor_count = 0
    river_physical_actor_count = 0
    river_actor_audit = []
    selectable_collision_component_count = 0

    if world_manifest.get("schema") != "rok.unity_world_map.v1":
        failures.append(
            f"Unexpected world manifest schema: {world_manifest.get('schema')}"
        )
    if asset_manifest.get("schema") != "rok.unity_world_map_assets.v1":
        failures.append(
            f"Unexpected asset manifest schema: {asset_manifest.get('schema')}"
        )
    if generation.get("schema") != "rok.unity_world_map_ue_generation.v1":
        failures.append(
            f"Unexpected generation schema: {generation.get('schema')}"
        )
    if generation.get("target_level") != TARGET_LEVEL:
        failures.append(
            f"Generation target is {generation.get('target_level')}, "
            f"expected {TARGET_LEVEL}"
        )
    if generation.get("source_world_manifest", {}).get("sha256") != sha256(
        WORLD_MANIFEST_PATH
    ):
        failures.append("UE generation uses a stale world manifest")
    if generation.get("source_asset_manifest", {}).get("sha256") != sha256(
        ASSET_MANIFEST_PATH
    ):
        failures.append("UE generation uses a stale asset manifest")
    if not TARGET_LEVEL_FILE.exists():
        failures.append(f"Missing target map package: {TARGET_LEVEL_FILE}")
    else:
        target_record = generation.get("target_level_package", {})
        if (
            target_record.get("size") != TARGET_LEVEL_FILE.stat().st_size
            or target_record.get("sha256") != sha256(TARGET_LEVEL_FILE)
        ):
            failures.append(
                "UE generation does not bind the current target map package"
            )

    actors = (
        list(unreal.EditorLevelLibrary.get_all_level_actors())
        if world
        else []
    )
    ground_actors = [
        actor for actor in actors if "RokWorldMapTerrain" in tags(actor)
    ]
    source_actors = [
        actor for actor in actors if "RokWorldMapSourceObject" in tags(actor)
    ]
    river_layer_actors = [
        actor
        for actor in actors
        if "RokWorldMapRiverRenderLayer" in tags(actor)
    ]
    selectable_actors = [
        actor for actor in actors if "RokWorldMapSelectable" in tags(actor)
    ]
    config_actors = [
        actor
        for actor in actors
        if actor.get_class().get_name() == EXPECTED_CONFIG_CLASS
    ]
    player_starts = [
        actor
        for actor in actors
        if isinstance(actor, unreal.PlayerStart)
        and "RokWorldMapPlayerStart" in tags(actor)
    ]

    expected_ground = world_manifest.get("prototype_window", {}).get(
        "plane_instance_count",
        0,
    )
    if len(ground_actors) != expected_ground or expected_ground != 81:
        failures.append(
            f"Ground actor count is {len(ground_actors)}, expected 81"
        )
    object_sources = asset_manifest.get("object_sources", {})
    expected_rendered = sum(
        1
        for tile in world_manifest.get("prototype_window", {}).get(
            "tiles",
            [],
        )
        for instance in tile.get("instances", [])
        if instance.get("kind") == "object"
        and object_sources.get(instance.get("prefab_id"), {}).get(
            "representation"
        )
        != "empty_source_prefab"
    )
    expected_empty = (
        world_manifest.get("prototype_window", {}).get(
            "object_instance_count",
            0,
        )
        - expected_rendered
    )
    actor_by_label = {}
    for actor in actors:
        label = actor.get_actor_label()
        if label in actor_by_label:
            failures.append(f"Duplicate actor label in target map: {label}")
        actor_by_label[label] = actor

    expected_generated_labels = {
        "RokWorldPlayerCity_1146_157",
        "RokWorldMap_DirectionalLight",
        BACKGROUND_ACTOR_LABEL,
        "RokWorldMap_SourceConfig",
        "PlayerStart_RokUnityWorldMap",
    }
    for tile in world_manifest.get("prototype_window", {}).get(
        "tiles",
        [],
    ):
        tile_x = int(tile["x"])
        tile_y = int(tile["y"])
        for instance in tile.get("instances", []):
            prefab_id = instance["prefab_id"]
            template_index = int(instance["template_index"])
            if instance["kind"] == "plane":
                label = (
                    f"RokWorldGround_Tile{tile_x}_{tile_y}_"
                    f"Plane{template_index:02d}"
                )
                expected_generated_labels.add(label)
                actor = actor_by_label.get(label)
                if not actor:
                    failures.append(f"Missing source ground actor: {label}")
                    continue
                if not isinstance(actor, unreal.StaticMeshActor):
                    failures.append(f"{label} is not a StaticMeshActor")
                verify_actor_transform(
                    actor,
                    instance["ue_relative_to_focus"],
                    {"pitch": 0.0, "yaw": instance["ue_yaw"], "roll": 0.0},
                    instance[
                        "ue_actor_scale_for_centimeter_asset"
                    ],
                    label,
                    failures,
                )
                verify_required_tags(
                    actor,
                    {
                        "RokWorldMapGenerated",
                        "RokWorldMapTerrain",
                        f"UnityTile_{tile_x}_{tile_y}",
                        f"UnityPlaneIndex_{template_index}",
                        "UnityTransformPolicy_PositionRotationScale",
                    },
                    label,
                    failures,
                )
                if str(actor.get_folder_path()) != (
                    "RokUnityWorldMap/Terrain"
                ):
                    failures.append(f"{label} is in the wrong actor folder")
                mesh_asset = (
                    "/Game/RokUnityWorldMapPrototype/Meshes/"
                    "SM_RokWorld_Plane60.SM_RokWorld_Plane60"
                )
                material_name = (
                    "M_WorldGround_" + safe_name(prefab_id)
                )
                material_asset = (
                    "/Game/RokUnityWorldMapPrototype/Materials/"
                    f"{material_name}.{material_name}"
                )
                verify_static_mesh_binding(
                    actor,
                    mesh_asset,
                    material_asset,
                    label,
                    failures,
                )
                component = actor.get_component_by_class(
                    unreal.StaticMeshComponent
                )
                if component:
                    verify_collision(
                        component,
                        False,
                        label,
                        failures,
                    )
                    if component.get_editor_property("cast_shadow"):
                        failures.append(
                            f"{label} unexpectedly casts shadows"
                        )
                continue

            object_source_record = object_sources.get(prefab_id, {})
            label = (
                f"RokWorldObject_Tile{tile_x}_{tile_y}_"
                f"Object{template_index:02d}_{safe_name(prefab_id)}"
            )
            if object_source_record.get(
                "representation"
            ) == "empty_source_prefab":
                if label in actor_by_label:
                    failures.append(
                        f"Empty Unity prefab unexpectedly rendered: {label}"
                    )
                continue
            if object_source_record.get(
                "representation"
            ) == RIVER_REPRESENTATION:
                layer_by_role = {
                    layer["role"]: layer
                    for layer in object_source_record[
                        "river_geometry_contract"
                    ]["render_layers"]
                }
                policy_tag = (
                    "UnityTransformPolicy_PositionOnly"
                    if instance["unity_runtime_transform_policy"]
                    == "position_only"
                    else "UnityTransformPolicy_PositionRotationScale"
                )
                for role in ("edge", "water"):
                    is_edge = role == "edge"
                    layer_label = (
                        label if is_edge else label + "_LayerWater"
                    )
                    expected_generated_labels.add(layer_label)
                    actor = actor_by_label.get(layer_label)
                    if not actor:
                        failures.append(
                            f"Missing river {role} actor: {layer_label}"
                        )
                        continue
                    if not isinstance(actor, unreal.StaticMeshActor):
                        failures.append(
                            f"{layer_label} is not a StaticMeshActor"
                        )
                    layer = layer_by_role[role]
                    composed = compose_river_layer_transform(
                        instance,
                        layer,
                    )
                    verify_actor_transform(
                        actor,
                        composed["location"],
                        composed["rotation"],
                        composed["scale"],
                        layer_label,
                        failures,
                    )
                    required_tags = {
                        "RokWorldMapGenerated",
                        "RokWorldMapExactSource",
                        "RokWorldMapRiver",
                        f"UnityTile_{tile_x}_{tile_y}",
                        f"UnityObjectIndex_{template_index}",
                        f"UnityPrefab_{safe_name(prefab_id)}",
                        policy_tag,
                    }
                    if is_edge:
                        required_tags.update(
                            {
                                "RokWorldMapSourceObject",
                                "RokWorldMapRiverEdge",
                                "RokWorldMapRiverLogicalObject",
                            }
                        )
                    else:
                        required_tags.update(
                            {
                                "RokWorldMapRiverRenderLayer",
                                "RokWorldMapRiverWater",
                            }
                        )
                    verify_required_tags(
                        actor,
                        required_tags,
                        layer_label,
                        failures,
                    )
                    actor_tags = tags(actor)
                    if is_edge and (
                        "RokWorldMapRiverRenderLayer" in actor_tags
                    ):
                        failures.append(
                            f"{layer_label} edge is incorrectly tagged as "
                            "a secondary physical layer"
                        )
                    if not is_edge and (
                        "RokWorldMapSourceObject" in actor_tags
                        or "RokWorldMapRiverLogicalObject" in actor_tags
                    ):
                        failures.append(
                            f"{layer_label} water incorrectly increments "
                            "the logical source-object count"
                        )
                    if str(actor.get_folder_path()) != RIVER_FOLDER:
                        failures.append(
                            f"{layer_label} is in the wrong actor folder"
                        )
                    mesh_name = (
                        f"SM_RokWorldRiver_{safe_name(prefab_id)}_"
                        f"{safe_name(role).title()}"
                    )
                    expected_mesh = (
                        f"{RIVER_LAYER_MESH_DESTINATION}/"
                        f"{mesh_name}.{mesh_name}"
                    )
                    expected_material = (
                        RIVER_EDGE_MATERIAL_ASSET
                        if is_edge
                        else RIVER_WATER_MATERIAL_ASSET
                    )
                    verify_static_mesh_binding(
                        actor,
                        expected_mesh,
                        expected_material,
                        layer_label,
                        failures,
                    )
                    component = actor.get_component_by_class(
                        unreal.StaticMeshComponent
                    )
                    if not component:
                        failures.append(
                            f"{layer_label} has no StaticMeshComponent"
                        )
                        continue
                    verify_collision(
                        component,
                        False,
                        layer_label,
                        failures,
                    )
                    if bool(
                        component.get_editor_property("cast_shadow")
                    ):
                        failures.append(
                            f"{layer_label} unexpectedly casts shadows"
                        )
                    for property_name in (
                        "affect_distance_field_lighting",
                        "affect_dynamic_indirect_lighting",
                    ):
                        if bool(
                            component.get_editor_property(property_name)
                        ):
                            failures.append(
                                f"{layer_label} still participates in "
                                f"{property_name}"
                            )
                    if int(
                        component.get_editor_property(
                            "translucency_sort_priority"
                        )
                    ) != int(layer["sort_priority"]):
                        failures.append(
                            f"{layer_label} sort priority differs from "
                            f"source layer {layer['sort_priority']}"
                        )
                    if component.get_num_materials() != 1:
                        failures.append(
                            f"{layer_label} has "
                            f"{component.get_num_materials()} material slots, "
                            "expected one"
                        )
                    mesh = component.get_editor_property("static_mesh")
                    verify_river_mesh_import_settings(
                        mesh,
                        layer_label,
                        failures,
                    )
                    actor_location = actor.get_actor_location()
                    actor_rotation = actor.get_actor_rotation()
                    actor_scale = actor.get_actor_scale3d()
                    river_actor_audit.append(
                        {
                            "label": layer_label,
                            "prefab_id": prefab_id,
                            "role": role,
                            "logical_source_actor": is_edge,
                            "mesh_asset": mesh_path(actor),
                            "material_assets": material_paths(actor),
                            "sort_priority": int(
                                component.get_editor_property(
                                    "translucency_sort_priority"
                                )
                            ),
                            "collision": str(
                                component.get_collision_enabled()
                            ),
                            "cast_shadow": bool(
                                component.get_editor_property("cast_shadow")
                            ),
                            "location": {
                                "x": float(actor_location.x),
                                "y": float(actor_location.y),
                                "z": float(actor_location.z),
                            },
                            "rotation": {
                                "pitch": float(actor_rotation.pitch),
                                "yaw": float(actor_rotation.yaw),
                                "roll": float(actor_rotation.roll),
                            },
                            "scale": {
                                "x": float(actor_scale.x),
                                "y": float(actor_scale.y),
                                "z": float(actor_scale.z),
                            },
                        }
                    )
                    river_physical_actor_count += 1
                river_logical_actor_count += 1
                continue
            expected_generated_labels.add(label)
            actor = actor_by_label.get(label)
            if not actor:
                failures.append(f"Missing source object actor: {label}")
                continue
            if not isinstance(actor, unreal.StaticMeshActor):
                failures.append(f"{label} is not a StaticMeshActor")
            verify_actor_transform(
                actor,
                instance["ue_relative_to_focus"],
                {"pitch": 0.0, "yaw": instance["ue_yaw"], "roll": 0.0},
                instance["ue_actor_scale_for_centimeter_asset"],
                label,
                failures,
            )
            policy_tag = (
                "UnityTransformPolicy_PositionOnly"
                if instance["unity_runtime_transform_policy"]
                == "position_only"
                else "UnityTransformPolicy_PositionRotationScale"
            )
            is_tree_source = (
                Path(
                    object_source_record.get("source_material", "")
                ).name
                in TREE_SOURCE_MATERIAL_NAMES
            )
            is_mountain_source = (
                object_source_record.get("representation")
                == "exact_source_fbx"
            )
            expected_actor_tags = {
                "RokWorldMapGenerated",
                "RokWorldMapSourceObject",
                "RokWorldMapExactSource",
                f"UnityTile_{tile_x}_{tile_y}",
                f"UnityObjectIndex_{template_index}",
                f"UnityPrefab_{safe_name(prefab_id)}",
                policy_tag,
            }
            if is_tree_source:
                expected_actor_tags.add("RokWorldMapTree")
            if is_mountain_source:
                expected_actor_tags.add("RokWorldMapMountain")
            verify_required_tags(
                actor,
                expected_actor_tags,
                label,
                failures,
            )
            if str(actor.get_folder_path()) != (
                "RokUnityWorldMap/SourceObjects"
            ):
                failures.append(f"{label} is in the wrong actor folder")
            mesh_name = safe_name(
                object_source_record.get("ue_mesh_name", "")
            )
            mesh_folder = object_source_record.get(
                "ue_mesh_folder",
                "/Game/RokUnityWorldMapPrototype/Meshes",
            )
            mesh_asset = (
                f"{mesh_folder}/"
                f"{mesh_name}.{mesh_name}"
            )
            if (
                object_source_record.get("representation")
                == "exact_unity_mesh_export"
            ):
                source_material = Path(
                    object_source_record["source_material"]
                ).stem
            else:
                source_material = (
                    "build_mountain_F"
                    if prefab_id == "build_mountain_F"
                    else "build_mountain_A"
                )
            material_name = (
                "M_WorldSource_" + safe_name(source_material)
            )
            material_asset = (
                "/Game/RokUnityWorldMapPrototype/Materials/"
                f"{material_name}.{material_name}"
            )
            verify_static_mesh_binding(
                actor,
                mesh_asset,
                material_asset,
                label,
                failures,
            )
            component = actor.get_component_by_class(
                unreal.StaticMeshComponent
            )
            if component:
                verify_collision(
                    component,
                    False,
                    label,
                    failures,
                )
                expected_cast_shadow = False
                if bool(
                    component.get_editor_property("cast_shadow")
                ) != expected_cast_shadow:
                    failures.append(
                        f"{label} cast-shadow state differs from source policy"
                    )
                if is_mountain_source:
                    mountain_source_actor_count += 1
                    geometry_contract = object_source_record.get(
                        "mountain_geometry_contract",
                        {},
                    )
                    expected_body_asset = geometry_contract.get(
                        "ue_body_mesh_asset"
                    )
                    observed_mesh_asset = mesh_path(actor) or ""
                    if expected_body_asset != mesh_asset:
                        failures.append(
                            f"{label} manifest body path differs from the "
                            "deterministic split import"
                        )
                    if observed_mesh_asset != expected_body_asset:
                        failures.append(
                            f"{label} does not render the exact split body asset"
                        )
                    if (
                        observed_mesh_asset
                        and "/MountainBodies/" not in observed_mesh_asset
                    ):
                        failures.append(
                            f"{label} still references the old combined mesh"
                        )
                    observed_asset_name = observed_mesh_asset.rsplit(
                        "/",
                        1,
                    )[-1].split(".", 1)[0]
                    forbidden_mesh_fragments = (
                        "_edge",
                        "_Collision",
                        "_Collision_da",
                    )
                    if any(
                        fragment.lower() in observed_asset_name.lower()
                        for fragment in forbidden_mesh_fragments
                    ):
                        failures.append(
                            f"{label} renders an excluded FBX node asset"
                        )
                    material_slot_count = component.get_num_materials()
                    if material_slot_count != 1:
                        failures.append(
                            f"{label} has {material_slot_count} visible "
                            "material slots; expected the body slot only"
                        )
                    for property_name in (
                        "affect_distance_field_lighting",
                        "affect_dynamic_indirect_lighting",
                    ):
                        if bool(
                            component.get_editor_property(property_name)
                        ):
                            failures.append(
                                f"{label} still participates in "
                                f"{property_name}"
                            )
                    for material_index in range(material_slot_count):
                        material = component.get_material(material_index)
                        if not isinstance(material, unreal.Material):
                            failures.append(
                                f"{label} mountain slot {material_index} "
                                "does not use a generated base material"
                            )
                            continue
                        if material.get_editor_property("blend_mode") != (
                            unreal.BlendMode.BLEND_OPAQUE
                        ):
                            failures.append(
                                f"{label} mountain slot {material_index} "
                                "is not opaque"
                            )
                        if material.get_editor_property("shading_model") != (
                            unreal.MaterialShadingModel.MSM_UNLIT
                        ):
                            failures.append(
                                f"{label} mountain slot {material_index} "
                                "still evaluates UE lighting"
                            )
                        if bool(
                            material.get_editor_property("two_sided")
                        ):
                            failures.append(
                                f"{label} mountain slot {material_index} "
                                "is unexpectedly two-sided"
                            )
                    actor_location = actor.get_actor_location()
                    actor_rotation = actor.get_actor_rotation()
                    actor_scale = actor.get_actor_scale3d()
                    mountain_actor_audit.append(
                        {
                            "label": label,
                            "prefab_id": prefab_id,
                            "mesh_asset": observed_mesh_asset,
                            "expected_body_asset": expected_body_asset,
                            "material_assets": material_paths(actor),
                            "visible_material_slot_count": (
                                material_slot_count
                            ),
                            "cast_shadow": bool(
                                component.get_editor_property("cast_shadow")
                            ),
                            "affects_distance_field_lighting": bool(
                                component.get_editor_property(
                                    "affect_distance_field_lighting"
                                )
                            ),
                            "affects_dynamic_indirect_lighting": bool(
                                component.get_editor_property(
                                    "affect_dynamic_indirect_lighting"
                                )
                            ),
                            "location": {
                                "x": float(actor_location.x),
                                "y": float(actor_location.y),
                                "z": float(actor_location.z),
                            },
                            "rotation": {
                                "pitch": float(actor_rotation.pitch),
                                "yaw": float(actor_rotation.yaw),
                                "roll": float(actor_rotation.roll),
                            },
                            "scale": {
                                "x": float(actor_scale.x),
                                "y": float(actor_scale.y),
                                "z": float(actor_scale.z),
                            },
                        }
                    )
                expected_sort_priority = (
                    6 if is_tree_source else 0
                )
                if is_tree_source:
                    tree_source_actor_count += 1
                if component.get_editor_property(
                    "translucency_sort_priority"
                ) != expected_sort_priority:
                    failures.append(
                        f"{label} translucency order differs from source policy"
                    )

    lighting_phase = prototype_lighting_phase(world_manifest)
    if not lighting_phase:
        failures.append(
            "World manifest has no unique prototype runtime lighting phase"
        )
    else:
        verified_material_paths = set()
        for actor in ground_actors + source_actors:
            if "RokWorldMapRiver" in tags(actor):
                continue
            for material_index, material in enumerate(
                [
                    actor.get_component_by_class(
                        unreal.StaticMeshComponent
                    ).get_material(index)
                    for index in range(
                        actor.get_component_by_class(
                            unreal.StaticMeshComponent
                        ).get_num_materials()
                    )
                ]
                if actor.get_component_by_class(
                    unreal.StaticMeshComponent
                )
                else []
            ):
                if not isinstance(material, unreal.Material):
                    continue
                material_path = material.get_path_name()
                if material_path in verified_material_paths:
                    continue
                verified_material_paths.add(material_path)
                verify_unity_sprite_color_material(
                    material,
                    lighting_phase["sprite_color"],
                    f"World source material {material_path}",
                    failures,
                )

        if river_source_records:
            light_direction = source_light_direction_ue(world_manifest)
            verify_river_edge_material(
                load_asset(RIVER_EDGE_MATERIAL_ASSET),
                lighting_phase,
                light_direction,
                failures,
            )
            first_river_record = next(
                iter(river_source_records.values())
            )
            water_layers = [
                layer
                for layer in first_river_record[
                    "river_geometry_contract"
                ]["render_layers"]
                if layer.get("role") == "water"
            ]
            if len(water_layers) != 1:
                failures.append(
                    "River source contract has no unique water layer"
                )
            else:
                verify_river_water_material(
                    load_asset(RIVER_WATER_MATERIAL_ASSET),
                    water_layers[0]["shader_contract"],
                    failures,
                )

        verified_ground_material_paths = set()
        for actor in ground_actors:
            component = actor.get_component_by_class(
                unreal.StaticMeshComponent
            )
            material = component.get_material(0) if component else None
            if not isinstance(material, unreal.Material):
                continue
            material_path = material.get_path_name()
            if material_path in verified_ground_material_paths:
                continue
            verified_ground_material_paths.add(material_path)
            verify_ground_texture_clamp(
                material,
                f"World ground material {material_path}",
                failures,
            )

        background = actor_by_label.get(BACKGROUND_ACTOR_LABEL)
        if not isinstance(background, unreal.StaticMeshActor):
            failures.append("Missing source camera solid-color background")
        else:
            map_record = world_manifest["map"]
            focus = world_manifest["prototype_window"]["focus"][
                "unity_coordinate"
            ]
            ue_scale = float(
                world_manifest["coordinate_contract"][
                    "ue_units_per_unity_unit"
                ]
            )
            center_unity_x = (
                float(map_record["world_width_unity_units"]) * 0.5
            )
            center_unity_y = (
                float(map_record["world_height_unity_units"]) * 0.5
            )
            background_scale = (
                max(
                    float(map_record["world_width_unity_units"]),
                    float(map_record["world_height_unity_units"]),
                )
                * ue_scale
                * 4.0
                / 6000.0
            )
            verify_actor_transform(
                background,
                {
                    "x": -(center_unity_x - float(focus["x"])) * ue_scale,
                    "y": (center_unity_y - float(focus["y"])) * ue_scale,
                    "z": -10000.0,
                },
                {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
                {
                    "x": background_scale,
                    "y": background_scale,
                    "z": background_scale,
                },
                "World solid-color background",
                failures,
            )
            verify_required_tags(
                background,
                {
                    "RokWorldMapGenerated",
                    "RokWorldMapBackground",
                    "UnityCameraClear_SolidColor",
                    "UnityLightManager_CameraFill",
                },
                "World solid-color background",
                failures,
            )
            if str(background.get_folder_path()) != (
                "RokUnityWorldMap/Environment"
            ):
                failures.append(
                    "World solid-color background is in the wrong folder"
                )
            verify_static_mesh_binding(
                background,
                PLANE_MESH_ASSET,
                BACKGROUND_MATERIAL_ASSET,
                "World solid-color background",
                failures,
            )
            background_component = background.get_component_by_class(
                unreal.StaticMeshComponent
            )
            if background_component:
                verify_collision(
                    background_component,
                    False,
                    "World solid-color background",
                    failures,
                )
                if background_component.get_editor_property("cast_shadow"):
                    failures.append(
                        "World solid-color background unexpectedly casts "
                        "shadows"
                    )
                if background_component.get_editor_property(
                    "translucency_sort_priority"
                ) != -10:
                    failures.append(
                        "World solid-color background has the wrong sort order"
                    )
                background_material = background_component.get_material(0)
                if isinstance(background_material, unreal.Material):
                    verify_solid_background_material(
                        background_material,
                        lighting_phase["camera_background_color"],
                        failures,
                    )
                else:
                    failures.append(
                        "World solid-color background has no generated "
                        "material"
                    )

    source_light = world_manifest["environment"]["directional_light"]
    sun = actor_by_label.get("RokWorldMap_DirectionalLight")
    if not isinstance(sun, unreal.DirectionalLight):
        failures.append("Missing source Directional Light")
    else:
        source_euler = source_light["unity_local_euler_degrees"]
        verify_actor_transform(
            sun,
            {"x": 0.0, "y": 0.0, "z": 45000.0},
            {
                "pitch": -float(source_euler["x"]),
                "yaw": 90.0 + float(source_euler["y"]),
                "roll": -float(source_euler["z"]),
            },
            {"x": 1.0, "y": 1.0, "z": 1.0},
            "World Directional Light",
            failures,
        )
        light_component = sun.get_component_by_class(unreal.LightComponent)
        if not light_component:
            failures.append("World Directional Light has no LightComponent")
        else:
            if not math.isclose(
                float(light_component.get_editor_property("intensity")),
                float(source_light["intensity"]),
                rel_tol=0.0,
                abs_tol=0.0001,
            ):
                failures.append(
                    "World Directional Light intensity is not the "
                    "source bootstrap value"
                )
            observed_color = light_component.get_editor_property(
                "light_color"
            )
            expected_color = source_light["color"]
            for channel in ("r", "g", "b", "a"):
                expected_value = round(
                    float(expected_color[channel]) * 255.0
                )
                if int(getattr(observed_color, channel)) != expected_value:
                    failures.append(
                        f"World Directional Light color {channel} is "
                        f"{getattr(observed_color, channel)}, expected "
                        f"{expected_value}"
                    )
            if light_component.get_editor_property("mobility") != (
                unreal.ComponentMobility.MOVABLE
            ):
                failures.append(
                    "World Directional Light is not movable"
                )
    forbidden_environment_actors = [
        actor
        for actor in actors
        if isinstance(
            actor,
            (
                unreal.SkyLight,
                unreal.SkyAtmosphere,
                unreal.ExponentialHeightFog,
            ),
        )
    ]
    if forbidden_environment_actors:
        failures.append(
            "World map contains source-incompatible sky/fog actors: "
            + ", ".join(
                actor.get_actor_label()
                for actor in forbidden_environment_actors
            )
        )

    observed_generated_labels = {
        actor.get_actor_label()
        for actor in actors
        if "RokWorldMapGenerated" in tags(actor)
    }
    missing_generated = sorted(
        expected_generated_labels - observed_generated_labels
    )
    unexpected_generated = sorted(
        observed_generated_labels - expected_generated_labels
    )
    if missing_generated:
        failures.append(
            "Missing generated actors: " + ", ".join(missing_generated)
        )
    if unexpected_generated:
        failures.append(
            "Unexpected generated actors: "
            + ", ".join(unexpected_generated)
        )
    if len(source_actors) != expected_rendered:
        failures.append(
            f"Rendered source object count is {len(source_actors)}, "
            f"expected {expected_rendered}"
        )
    if river_logical_actor_count != expected_river_logical_actor_count:
        failures.append(
            f"River logical actor count is {river_logical_actor_count}, "
            f"expected {expected_river_logical_actor_count}"
        )
    if (
        len(river_layer_actors)
        != expected_river_logical_actor_count
    ):
        failures.append(
            f"River secondary layer actor count is "
            f"{len(river_layer_actors)}, expected "
            f"{expected_river_logical_actor_count}"
        )
    if river_physical_actor_count != (
        expected_river_physical_actor_count
    ):
        failures.append(
            f"River physical render-layer actor count is "
            f"{river_physical_actor_count}, expected "
            f"{expected_river_physical_actor_count}"
        )
    if generation.get("rendered_source_object_actor_count") != (
        expected_rendered
    ):
        failures.append(
            "Generation changed the logical rendered-source count"
        )
    if generation.get("physical_source_render_actor_count") != (
        expected_rendered + expected_river_logical_actor_count
    ):
        failures.append(
            "Generation did not distinguish logical and physical source "
            "actors"
        )
    if generation.get("river_logical_object_actor_count") != (
        expected_river_logical_actor_count
    ):
        failures.append(
            "Generation has the wrong river logical actor count"
        )
    if generation.get("river_physical_render_layer_actor_count") != (
        expected_river_physical_actor_count
    ):
        failures.append(
            "Generation has the wrong river physical actor count"
        )
    if generation.get("omitted_empty_source_record_count") != expected_empty:
        failures.append(
            "Generation did not record the exact empty Unity prefab count"
        )
    if len(config_actors) != 1:
        failures.append(
            f"Expected one RokWorldMapConfigActor, found {len(config_actors)}"
        )
    else:
        verify_config_actor(
            config_actors[0],
            world_manifest,
            failures,
        )
    if len(player_starts) != 1:
        failures.append(
            f"Expected one world-map PlayerStart, found {len(player_starts)}"
        )
    else:
        player_start = player_starts[0]
        verify_actor_transform(
            player_start,
            {"x": 0.0, "y": -18000.0, "z": 18000.0},
            {"pitch": -45.0, "yaw": 90.0, "roll": 0.0},
            {"x": 1.0, "y": 1.0, "z": 1.0},
            "World-map PlayerStart",
            failures,
        )
    if len(selectable_actors) != 1:
        failures.append(
            f"Expected one phase-1 selectable city, found {len(selectable_actors)}"
        )
    elif selectable_actors[0].get_actor_label() != "RokWorldPlayerCity_1146_157":
        failures.append("Phase-1 selectable actor is not the focus player city")
    else:
        player_city = selectable_actors[0]
        if player_city.get_class().get_name() != EXPECTED_CITY_CLASS:
            failures.append(
                "Phase-1 player city does not use RokWorldMapCityActor"
            )
        city_source = asset_manifest.get("player_city", {})
        presentation = city_source.get("ue_presentation", {})
        lod_contract = city_source.get("lod_contract", {})
        shader_contract = city_source.get("shader_contract", {})
        expected_root = presentation.get("logical_world_root_cm", [])
        if len(expected_root) != 3:
            failures.append("Player-city source record has no logical root")
        else:
            verify_vector(
                vector_values(player_city.get_actor_location()),
                expected_root,
                "Player-city logical root",
                failures,
                0.01,
            )
        verify_vector(
            vector_values(player_city.get_actor_scale3d()),
            [1.0, 1.0, 1.0],
            "Player-city actor scale",
            failures,
            0.0001,
        )

        city_component = player_city.get_visual_mesh_component()
        if not city_component:
            failures.append("Player city has no source visual mesh component")
        else:
            verify_collision(
                city_component,
                False,
                "Player-city visual",
                failures,
            )
            city_mesh = city_component.get_editor_property("static_mesh")
            city_mesh_path = city_mesh.get_path_name() if city_mesh else ""
            if city_mesh_path != (
                "/Game/RokUnityWorldMapPrototype/Meshes/"
                "SM_RokWorld_Plane60.SM_RokWorld_Plane60"
            ):
                failures.append(
                    f"Player city uses unexpected source plane: {city_mesh_path}"
                )
            if city_component.get_editor_property(
                "translucency_sort_priority"
            ) != shader_contract.get("material_render_queue"):
                failures.append(
                    "Player-city translucency priority does not preserve "
                    "Unity render queue 2996"
                )
            city_material = city_component.get_material(0)
            if not isinstance(city_material, unreal.Material):
                failures.append(
                    "Player-city visual does not use a generated base material"
                )
            else:
                if city_material.get_editor_property("blend_mode") != (
                    unreal.BlendMode.BLEND_TRANSLUCENT
                ):
                    failures.append(
                        "Player-city UE material is not translucent"
                    )
                if city_material.get_editor_property("shading_model") != (
                    unreal.MaterialShadingModel.MSM_UNLIT
                ):
                    failures.append("Player-city UE material is not unlit")
                if lighting_phase:
                    verify_unity_sprite_color_material(
                        city_material,
                        lighting_phase["sprite_color"],
                        "Player-city UE material",
                        failures,
                    )

        selection_component = (
            player_city.get_selection_collision_component()
        )
        if not selection_component:
            failures.append(
                "Player city has no dedicated selection collision component"
            )
        else:
            selection_tags = {
                str(tag)
                for tag in selection_component.get_editor_property(
                    "component_tags"
                )
            }
            if "RokWorldMapSelectionCollider" not in selection_tags:
                failures.append(
                    "Player-city selection box lacks its dedicated "
                    "selection-collider tag"
                )
            verify_collision(
                selection_component,
                True,
                "Player-city selection box",
                failures,
            )
            selectable_collision_component_count += 1
            box_extent = selection_component.get_unscaled_box_extent()
            if min(
                float(box_extent.x),
                float(box_extent.y),
                float(box_extent.z),
            ) <= 0.0:
                failures.append(
                    "Player-city selection box has invalid bounds"
                )

        expected_initial_scale = lod_contract.get("initial_scale")
        if (
            expected_initial_scale is None
            or abs(
                float(player_city.get_current_lod_scale())
                - float(expected_initial_scale)
            )
            > 0.0001
        ):
            failures.append(
                "Player-city initial LOD scale does not match "
                "LevelDetailCastle at map_tactical dxf"
            )
        expected_cap = lod_contract.get("age_scale_cap")
        if (
            expected_cap is None
            or abs(
                float(player_city.get_age_lod_scale_cap())
                - float(expected_cap)
            )
            > 0.0001
        ):
            failures.append(
                "Player-city age LOD cap does not match CityAgeSize 41/57"
            )

        expected_anchor = presentation.get(
            "selection_anchor_offset_cm",
            [],
        )
        expected_center = presentation.get(
            "initial_visual_center_cm",
            [],
        )
        expected_extent = presentation.get(
            "initial_visual_bounds_extent_cm",
            [],
        )
        if len(expected_anchor) == 3:
            verify_vector(
                vector_values(
                    player_city.get_selection_anchor_world_position()
                ),
                expected_anchor,
                "Player-city selection anchor",
                failures,
                0.05,
            )
        else:
            failures.append("Player-city source record has no selection anchor")
        if len(expected_center) == 3:
            verify_vector(
                vector_values(player_city.get_visual_center_world_position()),
                expected_center,
                "Player-city visual center",
                failures,
                2.0,
            )
        else:
            failures.append(
                "Player-city source record has no initial visual center"
            )
        if len(expected_extent) == 3:
            verify_vector(
                vector_values(player_city.get_visual_bounds_extent()),
                expected_extent,
                "Player-city visual bounds extent",
                failures,
                2.0,
            )
        else:
            failures.append(
                "Player-city source record has no initial visual bounds"
            )

        generated_city = generation.get("player_city_source", {})
        if (
            generated_city.get("identity")
            != city_source.get("identity")
            or generated_city.get("baked_texture")
            != city_source.get("baked_texture")
            or generated_city.get("lod_contract")
            != city_source.get("lod_contract")
            or generated_city.get("ue_actor_class")
            != EXPECTED_CITY_CLASS_PATH
        ):
            failures.append(
                "Generation evidence does not preserve the authoritative "
                "Unity player-city source record"
            )
        if generated_city.get("ue_texture_asset") != (
            "/Game/RokUnityWorldMapPrototype/Textures/"
            "T_WorldPlayerCity_build_1_101_1."
            "T_WorldPlayerCity_build_1_101_1"
        ):
            failures.append(
                "Generated player-city texture is not build_1_101_1"
            )
        if generated_city.get("ue_material_asset") != (
            "/Game/RokUnityWorldMapPrototype/Materials/"
            "M_WorldPlayerCity_build_1_101_1."
            "M_WorldPlayerCity_build_1_101_1"
        ):
            failures.append(
                "Generated player-city material is not build_1_101_1"
            )

    if world:
        game_mode = world.get_world_settings().get_editor_property(
            "default_game_mode"
        )
        game_mode_path = (
            game_mode.get_path_name()
            if game_mode
            else ""
        )
        if game_mode_path != EXPECTED_GAME_MODE:
            failures.append(
                f"World GameMode is {game_mode_path}, "
                f"expected {EXPECTED_GAME_MODE}"
            )

    for actor in (
        ground_actors
        + source_actors
        + river_layer_actors
        + selectable_actors
    ):
        static_mesh_path = mesh_path(actor)
        if not static_mesh_path:
            failures.append(f"{actor.get_actor_label()} has no static mesh")
        if static_mesh_path.startswith("/Engine/BasicShapes"):
            failures.append(
                f"{actor.get_actor_label()} uses forbidden BasicShapes mesh"
            )
        for material_path in material_paths(actor):
            if "Blockout" in material_path:
                failures.append(
                    f"{actor.get_actor_label()} uses blockout material "
                    f"{material_path}"
                )
            if not material_path.startswith(
                "/Game/RokUnityWorldMapPrototype/Materials/"
            ):
                failures.append(
                    f"{actor.get_actor_label()} uses out-of-slice material "
                    f"{material_path}"
                )

    for actor in source_actors:
        actor_tags = tags(actor)
        if "RokWorldMapExactSource" not in actor_tags:
            failures.append(
                f"{actor.get_actor_label()} is not tagged exact-source"
            )
        if not any(
            value.startswith("UnityTransformPolicy_")
            for value in actor_tags
        ):
            failures.append(
                f"{actor.get_actor_label()} has no runtime transform policy"
            )
        if "RokWorldMapSelectable" in actor_tags:
            failures.append(
                f"Terrain source actor is unexpectedly selectable: "
                f"{actor.get_actor_label()}"
            )

    for actor in ground_actors:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component:
            continue
        if component.get_editor_property(
            "translucency_sort_priority"
        ) != -5:
            failures.append(
                f"{actor.get_actor_label()} does not preserve terrain "
                "Geometry-5 ordering"
            )
        material = component.get_material(0)
        if not isinstance(material, unreal.Material):
            failures.append(
                f"{actor.get_actor_label()} has no generated ground material"
            )
            continue
        if material.get_editor_property("blend_mode") != (
            unreal.BlendMode.BLEND_TRANSLUCENT
        ):
            failures.append(
                f"{actor.get_actor_label()} terrain material writes depth"
            )
        if material.get_editor_property("shading_model") != (
            unreal.MaterialShadingModel.MSM_UNLIT
        ):
            failures.append(
                f"{actor.get_actor_label()} terrain material is not unlit"
            )

    for actor in selectable_actors:
        actor_tags = tags(actor)
        for forbidden_tag in ("CityBuilding", "RokBuilding"):
            if forbidden_tag in actor_tags:
                failures.append(
                    f"World selectable actor has city-only tag {forbidden_tag}"
                )
        for forbidden_fragment in (
            "Castle_6_5",
            "PivotCompensation_BottomOnGround",
        ):
            if any(
                forbidden_fragment in value
                for value in actor_tags
            ):
                failures.append(
                    "World player city still carries diagnostic tag "
                    f"{forbidden_fragment}"
                )
        for required_tag in (
            "RokWorldMapExactSource",
            "UnityModel_build_1_101_1",
            "UnityPivot_SourceAuthored",
            "UnityLod_LevelDetailCastle",
        ):
            if required_tag not in actor_tags:
                failures.append(
                    f"World player city lacks source tag {required_tag}"
                )

    if lighting_phase:
        source_euler = world_manifest["environment"][
            "directional_light"
        ]["unity_local_euler_degrees"]
        expected_environment_policy = {
            "source": world_manifest["environment"],
            "prototype_phase": lighting_phase["phase"],
            "sprite_color_parameter": SPRITE_COLOR_PARAMETER,
            "sprite_color": lighting_phase["sprite_color"],
            "background_color_parameter": BACKGROUND_COLOR_PARAMETER,
            "background_color": lighting_phase[
                "camera_background_color"
            ],
            "background_actor_label": BACKGROUND_ACTOR_LABEL,
            "background_material": BACKGROUND_MATERIAL_ASSET,
            "directional_light_actor_label": (
                "RokWorldMap_DirectionalLight"
            ),
            "directional_light_ue_rotation": {
                "pitch": -float(source_euler["x"]),
                "yaw": 90.0 + float(source_euler["y"]),
                "roll": -float(source_euler["z"]),
            },
            "sky_light_actor_count": 0,
            "sky_atmosphere_actor_count": 0,
            "exponential_height_fog_actor_count": 0,
            "ground_texture_addressing": "Clamp",
            "source_fog_enabled": False,
            "directional_intensity_note": (
                "Unity bootstrap numeric intensity is preserved for "
                "source traceability; it is not claimed as a photometric "
                "lux equivalence."
            ),
        }
        if generation.get("environment_policy") != (
            expected_environment_policy
        ):
            failures.append(
                "Generation environment policy does not preserve Unity "
                "lighting, solid camera background, and fog-disabled rules"
            )

    for asset_path in (
        generation.get("mesh_assets", [])
        + generation.get("texture_assets", [])
        + generation.get("material_assets", [])
    ):
        package_path = asset_path.split(".", 1)[0]
        if not unreal.EditorAssetLibrary.does_asset_exist(package_path):
            failures.append(f"Missing generated UE asset: {asset_path}")

    if any(
        "proxy" in record.get("representation", "")
        for record in object_sources.values()
    ):
        failures.append("Asset manifest still contains visual proxy records")
    if generation.get("forbidden_content") != {
        "engine_basic_shapes": 0,
        "blockout_materials": 0,
        "source_derived_proxies": 0,
    }:
        failures.append("Generation evidence does not reject forbidden content")
    if generation.get("runtime_classes", {}).get(
        "city_actor"
    ) != EXPECTED_CITY_CLASS_PATH:
        failures.append(
            "Generation evidence does not name RokWorldMapCityActor"
        )

    result = {
        "schema": "rok.unity_world_map_ue_verify.v1",
        "target_level": TARGET_LEVEL,
        "source_world_manifest": source_record(WORLD_MANIFEST_PATH),
        "source_asset_manifest": source_record(ASSET_MANIFEST_PATH),
        "source_ue_generation": source_record(GENERATION_PATH),
        "target_level_package": source_record(TARGET_LEVEL_FILE),
        "failure_count": len(failures),
        "failures": failures,
        "ground_actor_count": len(ground_actors),
        "source_object_actor_count": len(source_actors),
        "physical_source_render_actor_count": (
            len(source_actors) + len(river_layer_actors)
        ),
        "river_logical_object_actor_count": river_logical_actor_count,
        "river_secondary_layer_actor_count": len(
            river_layer_actors
        ),
        "river_physical_render_layer_actor_count": (
            river_physical_actor_count
        ),
        "river_actor_audit": river_actor_audit,
        "river_render_policy": generation.get("river_render_policy"),
        "river_layer_imports": generation.get("river_layer_imports"),
        "omitted_empty_source_record_count": expected_empty,
        "selectable_actor_count": len(selectable_actors),
        "selectable_collision_component_count": (
            selectable_collision_component_count
        ),
        "config_actor_count": len(config_actors),
        "player_start_count": len(player_starts),
        "game_mode": generation.get("runtime_classes", {}).get("game_mode"),
        "mesh_asset_count": len(generation.get("mesh_assets", [])),
        "texture_asset_count": len(generation.get("texture_assets", [])),
        "material_asset_count": len(generation.get("material_assets", [])),
        "saved_no_collision_component_count": (
            len(ground_actors)
            + len(source_actors)
            + len(river_layer_actors)
        ),
        "runtime_collision_normalization_required_count": len(
            runtime_collision_normalization_labels
        ),
        "runtime_collision_normalization_required_labels": (
            runtime_collision_normalization_labels
        ),
        "runtime_collision_policy": generation.get(
            "runtime_collision_policy"
        ),
        "tree_source_actor_count": tree_source_actor_count,
        "runtime_tree_lod_policy": generation.get(
            "runtime_tree_lod_policy"
        ),
        "mountain_source_actor_count": mountain_source_actor_count,
        "mountain_actor_audit": mountain_actor_audit,
        "mountain_render_policy": generation.get(
            "mountain_render_policy"
        ),
    }
    VERIFY_PATH.write_text(
        json.dumps(result, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    if failures:
        unreal.log_error("Rok Unity world-map UE verification FAILED")
        for failure in failures:
            unreal.log_error(f"- {failure}")
        raise RuntimeError(
            f"World-map UE verification failed with {len(failures)} issue(s)"
        )
    unreal.log(
        "Rok Unity world-map UE verification passed: "
        f"ground={len(ground_actors)} "
        f"source_objects={len(source_actors)} "
        f"selectable={len(selectable_actors)} "
        f"meshes={result['mesh_asset_count']} "
        f"materials={result['material_asset_count']}"
    )


if __name__ == "__main__":
    main()
