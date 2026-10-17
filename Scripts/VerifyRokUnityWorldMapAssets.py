import csv
import hashlib
import json
import math
import re
from pathlib import Path

import numpy as np
from PIL import Image
from PrepareRokUnityWorldMapAssets import (
    build_full_scene_object_instance_counts,
    build_guid_index,
    parse_river_core_geometry_contract,
    parse_fbx_binary_nodes,
    parse_waterfall_geometry_contract,
)


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
MANIFEST_PATH = OUTPUT_DIR / "unity_world_map_assets.json"
VERIFY_PATH = OUTPUT_DIR / "unity_world_map_assets_verify.json"

EXPECTED_GROUND_IDS = {
    *(f"I_TYPE_SceneMap_mask_{index}" for index in range(1, 10)),
    *(f"I_TYPE_TEST_guodu{index:02d}" for index in range(1, 10)),
    "I_TYPE_TEST_da",
    "I_TYPE_TEST_zhong",
}
EXPECTED_MOUNTAIN_RENDER_CONTRACT = {
    "unity_shader_name": "custom/landmap/z_cm_slg_mountain",
    "unity_render_type": "Opaque",
    "unity_blend": "opaque",
    "unity_zwrite": "On",
    "unity_fragment": "MainTex.rgb * _SpriteColor.rgb without lighting",
    "ue_import_combine_meshes": False,
    "ue_runtime_geometry": "exact_fbx_body_node_only",
    "ue_excluded_geometry": [
        "edge",
        "collision",
        "collision_da",
    ],
    "ue_blend_mode": "opaque",
    "ue_shading_model": "unlit",
    "ue_cast_shadow": False,
    "ue_affects_distance_field_lighting": False,
    "ue_affects_dynamic_indirect_lighting": False,
    "acceptance_scope": "far_stability_body_only_source_lod_pending",
}
EXPECTED_MOUNTAIN_IDS = {
    "build_mountain_A",
    "build_mountain_B",
    "build_mountain_C",
    "build_mountain_E",
    "build_mountain_F",
    "build_mountain_D",
    "build_mountain_G",
    "build_mountain_A1",
    "build_mountain_A2",
    "build_mountain_A3",
    "build_mountain_A4",
    "build_mountain_A5",
    "build_mountain_A7",
    "build_mountain_A8",
    "build_bridge_A",
}
MOUNTAIN_BODY_MESH_DESTINATION = (
    "/Game/RokUnityWorldMapPrototype/Meshes/MountainBodies"
)
EXPECTED_RIVER_CORE_IDS = {
    "Rive_CO_A",
    "Rive_CO_B",
    "Rive_CO_C",
    "Rive_CO_D",
    "Rive_CO_E",
    "Rive_CO_F",
    "Ocean_CO_A",
}
EXPECTED_RIVER_CORE_FBX_GUIDS = {
    "Rive_CO_A": "ba71fca9a83018a4ba1f9c2d0671119a",
    "Rive_CO_B": "ca69f5d5e0c0fa34c9c778e37811c898",
    "Rive_CO_C": "dd78e4a6208fddc40a090bbbe4ddb350",
    "Rive_CO_D": "c6f411a38b0ae0948bf4c852d843d92e",
    "Rive_CO_E": "f6afe36d4a33ba84bbc622fef5bed322",
    "Rive_CO_F": "870068f1ccf04aa4ba677deda15a8510",
    "Ocean_CO_A": "76e8ae8fc1193de4fa39b19f4c4a4b4c",
}
EXPECTED_RIVER_INNER_COLLISION_NODES = {
    "Rive_CO_A": "Rive_CO_A_Collision_neiw",
    "Rive_CO_B": "Rive_CO_B_Collision_nei",
    "Rive_CO_C": "Rive_CO_C_Collision_nei",
    "Rive_CO_D": "Rive_CO_D_Collision_nei",
    "Rive_CO_E": "Rive_CO_E_Collision_nei",
    "Rive_CO_F": "Rive_CO_F_Collision_nei",
    "Ocean_CO_A": "Ocean_CO_A_Collision_nei",
}
EXPECTED_RIVER_MATERIAL_GUIDS = {
    "edge": "355a14738c7d4d445b4727529ceb6234",
    "water": "536f54f1b429164408f7aba6c1911c83",
}
EXPECTED_RIVER_WATER_MATERIAL_GUID_OVERRIDES = {
    "Ocean_CO_A": "b6dfeee56d363614691bb670c328ebea",
}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify_file(record, role, failures):
    if not record:
        failures.append(f"{role} has no source record")
        return None
    relative_path = record.get("path", "")
    path = PROJECT_ROOT / relative_path
    if not path.exists():
        failures.append(f"{role} is missing: {relative_path}")
        return None
    if path.stat().st_size != record.get("size"):
        failures.append(f"{role} size changed: {relative_path}")
    if sha256(path) != record.get("sha256"):
        failures.append(f"{role} hash changed: {relative_path}")
    return path


def verify_float(observed, expected, role, failures, tolerance=1.0e-6):
    if (
        not isinstance(observed, (int, float))
        or not isinstance(expected, (int, float))
        or not math.isclose(
        float(observed),
        float(expected),
        rel_tol=0.0,
        abs_tol=tolerance,
        )
    ):
        failures.append(f"{role} is {observed!r}, expected {expected!r}")


def verify_vector(
    observed,
    expected,
    role,
    failures,
    tolerance=1.0e-6,
):
    if not isinstance(observed, list) or len(observed) != len(expected):
        failures.append(f"{role} is {observed!r}, expected {expected!r}")
        return
    for index, expected_value in enumerate(expected):
        verify_float(
            observed[index],
            expected_value,
            f"{role}[{index}]",
            failures,
            tolerance,
        )


def parse_curve_source(path, marker, failures):
    try:
        text = path.read_text(encoding="utf-8", errors="strict")
    except OSError as error:
        failures.append(f"Could not read {path}: {error}")
        return []
    matches = list(
        re.finditer(
            rf"^  {re.escape(marker)}:\s*$",
            text,
            re.M,
        )
    )
    if len(matches) != 1:
        failures.append(
            f"{path.name} expected one {marker}, found {len(matches)}"
        )
        return []
    section = text[matches[0].start() :]
    end_match = re.search(r"^    m_PreInfinity:\s*", section, re.M)
    if not end_match:
        failures.append(f"{path.name} {marker} has no terminator")
        return []
    section = section[: end_match.start()]
    keys = []
    number_pattern = r"[-+0-9.eE]+|[-+]?Infinity"
    for block in re.split(
        r"^    - serializedVersion:\s*3\s*$",
        section,
        flags=re.M,
    )[1:]:
        values = {}
        for field in (
            "time",
            "value",
            "inSlope",
            "outSlope",
            "weightedMode",
        ):
            match = re.search(
                rf"^\s+{field}:\s*({number_pattern})\s*$",
                block,
                re.M,
            )
            if not match:
                failures.append(
                    f"{path.name} {marker} key lacks {field}"
                )
                return []
            raw = match.group(1)
            values[field] = (
                int(raw)
                if field == "weightedMode"
                else float(raw)
            )
        keys.append(
            {
                "dxf": values["time"],
                "scale": values["value"],
                "in_slope": values["inSlope"],
                "out_slope": values["outSlope"],
                "weighted_mode": values["weightedMode"],
            }
        )
    return keys


def evaluate_curve(keys, dxf, role, failures):
    for left, right in zip(keys, keys[1:]):
        if not (left["dxf"] <= dxf <= right["dxf"]):
            continue
        if dxf == left["dxf"]:
            return float(left["scale"])
        if dxf == right["dxf"]:
            return float(right["scale"])
        if (
            left["weighted_mode"] != 0
            or right["weighted_mode"] != 0
            or not math.isfinite(left["out_slope"])
            or not math.isfinite(right["in_slope"])
        ):
            failures.append(
                f"{role} target interval is weighted or discontinuous"
            )
            return None
        duration = right["dxf"] - left["dxf"]
        alpha = (dxf - left["dxf"]) / duration
        alpha2 = alpha * alpha
        alpha3 = alpha2 * alpha
        return (
            (2.0 * alpha3 - 3.0 * alpha2 + 1.0) * left["scale"]
            + (alpha3 - 2.0 * alpha2 + alpha)
            * duration
            * left["out_slope"]
            + (-2.0 * alpha3 + 3.0 * alpha2) * right["scale"]
            + (alpha3 - alpha2)
            * duration
            * right["in_slope"]
        )
    failures.append(f"{role} has no interval for dxf {dxf}")
    return None


def parse_age_sizes(path, failures):
    try:
        with path.open(
            "r",
            encoding="utf-8-sig",
            newline="",
        ) as handle:
            rows = [
                {
                    "id": int(row["ID"]),
                    "age": int(row["age"]),
                    "size": float(row["size"]),
                }
                for row in csv.DictReader(handle)
                if (row.get("ID") or "").strip() != "int"
            ]
    except (KeyError, OSError, TypeError, ValueError) as error:
        failures.append(f"Could not parse {path}: {error}")
        return []
    return rows


def verify_river_core_record(
    prefab_id,
    record,
    world_manifest,
    guid_index,
    failures,
):
    expected_prefab_path = (
        "ref/client-unity/Assets/BundleAssets/land/Tile/AutoMake/"
        f"Mountain/{prefab_id}.prefab"
    )
    expected_fbx_path = (
        "ref/client-unity/Assets/BundleAssets/land/Fbx/"
        f"{prefab_id}.FBX"
    )
    if record.get("source_prefab") != expected_prefab_path:
        failures.append(
            f"{prefab_id} source prefab path is not exact"
        )
    source_prefab_path = verify_file(
        record.get("source_prefab_record"),
        f"{prefab_id} source prefab",
        failures,
    )
    source_fbx_path = verify_file(
        record.get("source_mesh"),
        f"{prefab_id} source FBX",
        failures,
    )
    if (
        record.get("source_mesh", {}).get("path")
        != expected_fbx_path
    ):
        failures.append(f"{prefab_id} source FBX path is not exact")
    if not source_prefab_path or not source_fbx_path:
        return

    expected_world_record = (
        world_manifest.get("full_scene", {})
        .get("source_prefabs", {})
        .get(prefab_id, {})
    )
    if expected_world_record.get("source_prefab_path") != (
        expected_prefab_path
    ):
        failures.append(
            f"{prefab_id} world-manifest prefab source changed"
        )
    expected_dependencies = {
        dependency.get("path")
        for dependency in expected_world_record.get(
            "resolved_dependencies",
            [],
        )
    }
    if expected_fbx_path not in expected_dependencies:
        failures.append(
            f"{prefab_id} FBX is not a resolved world dependency"
        )

    try:
        expected_contract = parse_river_core_geometry_contract(
            prefab_id,
            source_prefab_path,
            source_fbx_path,
            guid_index,
        )
    except Exception as error:
        failures.append(
            f"{prefab_id} source contract could not be rebuilt: {error}"
        )
        return
    contract = record.get("river_geometry_contract", {})
    if contract != expected_contract:
        failures.append(
            f"{prefab_id} river contract differs from current source"
        )

    expected_inner = EXPECTED_RIVER_INNER_COLLISION_NODES[prefab_id]
    fbx_model_order, fbx_geometry_model_order = (
        parse_fbx_binary_nodes(source_fbx_path)
    )
    expected_nodes = list(fbx_model_order)
    expected_collision_nodes = [
        node_name
        for node_name in expected_nodes
        if "_Collision" in node_name
    ]
    if expected_inner not in expected_collision_nodes:
        failures.append(
            f"{prefab_id} inner collision node missing from source FBX"
        )
    if not {prefab_id, f"{prefab_id}_water"}.issubset(expected_nodes):
        failures.append(
            f"{prefab_id} render nodes missing from source FBX"
        )
    if contract.get("policy") != "split_fbx_exact_render_layers":
        failures.append(
            f"{prefab_id} does not split exact river render layers"
        )
    if contract.get("combine_meshes") is not False:
        failures.append(f"{prefab_id} still combines FBX nodes")
    if contract.get("source_fbx_guid") != (
        EXPECTED_RIVER_CORE_FBX_GUIDS[prefab_id]
    ):
        failures.append(f"{prefab_id} source FBX GUID changed")
    if contract.get("source_fbx_node_count") != len(expected_nodes):
        failures.append(f"{prefab_id} FBX node audit count changed")
    if contract.get("fbx_mesh_nodes") != expected_nodes:
        failures.append(f"{prefab_id} FBX node inventory is not exact")
    if contract.get("excluded_fbx_nodes") != expected_collision_nodes:
        failures.append(
            f"{prefab_id} collision-node exclusion is incomplete"
        )
    if contract.get("source_fbx_geometry_model_order") != (
        fbx_geometry_model_order
    ):
        failures.append(
            f"{prefab_id} FBX geometry import order is not audited"
        )
    if contract.get("logical_instance_layer_count") != 2:
        failures.append(
            f"{prefab_id} is not one logical object with two layers"
        )


    layers = contract.get("render_layers", [])
    if not isinstance(layers, list) or len(layers) != 2:
        failures.append(
            f"{prefab_id} does not contain exactly two render layers"
        )
        return
    for index, (role, node_name, sort_priority) in enumerate(
        (
            ("edge", prefab_id, 0),
            ("water", f"{prefab_id}_water", 1),
        )
    ):
        layer = layers[index]
        if layer.get("role") != role:
            failures.append(
                f"{prefab_id} render layer {index} role is not {role}"
            )
        if layer.get("game_object") != node_name:
            failures.append(
                f"{prefab_id} {role} render node identity changed"
            )
        if layer.get("sort_priority") != sort_priority:
            failures.append(
                f"{prefab_id} {role} sort priority changed"
            )
        if layer.get("source_sorting_order") != 0:
            failures.append(
                f"{prefab_id} {role} source sorting order changed"
            )
        if layer.get("enabled") != 1:
            failures.append(f"{prefab_id} {role} layer is disabled")
        if layer.get("game_object_layer") != 0:
            failures.append(
                f"{prefab_id} {role} no longer uses Unity layer 0"
            )
        if layer.get("game_object_active") != 1:
            failures.append(
                f"{prefab_id} {role} GameObject is inactive"
            )
        for field in (
            "game_object_file_id",
            "mesh_filter_file_id",
            "mesh_file_id",
            "mesh_renderer_file_id",
        ):
            if not str(layer.get(field, "")):
                failures.append(
                    f"{prefab_id} {role} lacks {field}"
                )
        if layer.get("mesh_guid") != (
            EXPECTED_RIVER_CORE_FBX_GUIDS[prefab_id]
        ):
            failures.append(
                f"{prefab_id} {role} MeshFilter FBX GUID changed"
            )
        if layer.get("material_guid") != (
            EXPECTED_RIVER_WATER_MATERIAL_GUID_OVERRIDES.get(
                prefab_id, EXPECTED_RIVER_MATERIAL_GUIDS[role]
            )
            if role == "water"
            else EXPECTED_RIVER_MATERIAL_GUIDS[role]
        ):
            failures.append(
                f"{prefab_id} {role} material GUID changed"
            )
        material_path = verify_file(
            layer.get("source_material"),
            f"{prefab_id} {role} source material",
            failures,
        )
        shader_path = verify_file(
            layer.get("source_shader"),
            f"{prefab_id} {role} source shader",
            failures,
        )
        expected_names = (
            (
                "river_CO_A_edge.mat",
                "custommap-60_map_lowland_edge 1.shader",
                "custom/landmap/custommap_60_map_lowland_edge",
            )
            if role == "edge"
            else (
                "river_CO_A_water.mat",
                "customland-23_water.shader",
                "custom/land/-23_water",
            )
        )
        if material_path and material_path.name != expected_names[0]:
            failures.append(
                f"{prefab_id} {role} material source changed"
            )
        if shader_path and shader_path.name != expected_names[1]:
            failures.append(
                f"{prefab_id} {role} shader source changed"
            )
        if layer.get("shader_name") != expected_names[2]:
            failures.append(
                f"{prefab_id} {role} shader name changed"
            )
        for property_name, texture in layer.get(
            "textures",
            {},
        ).items():
            verify_file(
                texture.get("source_texture"),
                f"{prefab_id} {role} {property_name} texture",
                failures,
            )

    edge_shader = layers[0].get("shader_contract", {})
    if edge_shader != {
        "queue": "Transparent-60",
        "render_type": "Transparent",
        "blend": "SrcAlpha OneMinusSrcAlpha",
        "zwrite": "Off",
        "rgb": (
            "_MainTex.rgb multiplied by Unity ambient plus "
            "directional-light vertex lighting"
        ),
        "opacity": "interpolated vertex color alpha",
    }:
        failures.append(
            f"{prefab_id} edge shader contract is incomplete"
        )
    water_shader = contract.get("water_dynamic_shader_contract", {})
    if water_shader != {
        "queue": "Transparent-23",
        "render_type": "Transparent",
        "blend": (
            "SrcAlpha OneMinusSrcAlpha, "
            "SrcAlpha OneMinusSrcAlpha"
        ),
        "zwrite": "Off",
        "bump_tiling": [0.2, 0.2, 0.35, 0.1],
        "speed": [0.0, 0.0, -2.0, 0.0],
        "base_color": [-1.31, 1.25, 8.5, 0.009],
        "world_light_dir": [-0.14, 0.56, -1.32, 0.0],
        "near": 0.3,
        "far": 3.5,
        "shininess": 150.0,
        "gloss": 7.0,
        "uv_scroll": (
            "uv0=((vertex.xz*2)+(Speed.xy*Time.x))*"
            "BumpTiling.xy; uv1=(vertex.xz+"
            "(Speed.zw*Time.x))*BumpTiling.zw"
        ),
        "camera_height_factor_unity": (
            "max(0, _WorldSpaceCameraPos.y / 70)"
        ),
        "camera_height_factor_ue_world_scale_500": (
            "max(0, CameraZ / 35000)"
        ),
        "near_far_usage": (
            "height_scale=lerp(Near,Far,max(0,CameraZ/35000)); "
            "normalY=((Normal0*2-1)+(Normal1*2-1)/2).y*"
            "height_scale; specular=max(0,pow(max(0,normalY),"
            "Shininess))*Gloss; rgb=saturate(BaseColor.rgb+specular)"
        ),
        "fragment_formula": (
            "zDis=max(0,CameraZ/35000) without upper clamp; "
            "factor=lerp(Near,Far,zDis); "
            "normalColor=(Normal0*2-1)+(Normal1*2-1)/2; "
            "normalY=normalColor.y*factor; "
            "spec=max(0,pow(max(0,normalY),Shininess))*Gloss; "
            "col=BaseColor+spec; col.a*=VertexColor.a; "
            "col.rgb=saturate(col.rgb); col.a=saturate(col.a)"
        ),
        "opacity": "saturate(BaseColor.a * vertex color alpha)",
    }:
        failures.append(
            f"{prefab_id} dynamic water shader parameters changed"
        )
    if contract.get("ue_import_contract") != {
        "combine_meshes": False,
        "import_vertex_colors": "replace",
        "vertex_color_source_semantics": {
            "edge": "alpha contains transparent and opaque values",
            "water": "alpha is fully opaque before BaseColor.a",
        },
        "collision_nodes": (
            "excluded_from_render_and_collision_generation"
        ),
    }:
        failures.append(
            f"{prefab_id} vertex-color/collision import policy changed"
        )
    if contract.get("acceptance_scope") != (
        "static_source_contract_only_ue_import_map_runtime_pending"
    ):
        failures.append(
            f"{prefab_id} overclaims UE import or runtime acceptance"
        )


def main():
    failures = []
    if not MANIFEST_PATH.exists():
        failures.append(f"Missing generated asset manifest: {MANIFEST_PATH}")
        manifest = {}
    else:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    if manifest.get("schema") != "rok.unity_world_map_assets.v1":
        failures.append(f"Unexpected schema: {manifest.get('schema')}")

    world_source = manifest.get("source_world_manifest", {})
    world_path = PROJECT_ROOT / world_source.get("path", "")
    world_manifest = {}
    if not world_path.exists():
        failures.append("World-map source manifest is missing")
    elif sha256(world_path) != world_source.get("sha256"):
        failures.append("World-map source manifest changed after asset preparation")
    else:
        world_manifest = json.loads(
            world_path.read_text(encoding="utf-8")
        )

    baked = manifest.get("baked_ground_materials", [])
    if manifest.get("baked_ground_material_count") != 20 or len(baked) != 20:
        failures.append(
            f"Expected 20 baked ground materials, found {len(baked)}"
        )
    baked_ids = {item.get("prefab_id") for item in baked}
    missing_ids = sorted(EXPECTED_GROUND_IDS - baked_ids)
    unexpected_ids = sorted(baked_ids - EXPECTED_GROUND_IDS)
    if missing_ids:
        failures.append("Missing ground bakes: " + ", ".join(missing_ids))
    if unexpected_ids:
        failures.append("Unexpected ground bakes: " + ", ".join(unexpected_ids))

    for record in baked:
        prefab_id = record.get("prefab_id", "<unknown>")
        verify_file(record.get("source_material"), f"{prefab_id} material", failures)
        shader_path = verify_file(
            record.get("source_shader"),
            f"{prefab_id} shader",
            failures,
        )
        if shader_path and shader_path.name != "cm_mask_texture_lod.shader":
            failures.append(f"{prefab_id} uses the wrong Unity shader")
        if record.get("render_contract") != {
            "unity_queue": "Geometry-5",
            "unity_blend": "opaque",
            "unity_zwrite": "Off",
            "ue_blend_mode": "translucent",
            "ue_shading_model": "unlit",
            "ue_translucency_sort_priority": -5,
            "ue_emulation": (
                "alpha-one translucent material preserves Unity ZWrite Off "
                "so later tree and city cards are not clipped by terrain"
            ),
        }:
            failures.append(
                f"{prefab_id} does not preserve Unity terrain ZWrite Off"
            )
        verify_file(record.get("splat"), f"{prefab_id} splat", failures)
        if len(record.get("layers", [])) != 4:
            failures.append(f"{prefab_id} does not have four source layers")
        for index, layer in enumerate(record.get("layers", [])):
            verify_file(
                layer.get("texture"),
                f"{prefab_id} layer {index}",
                failures,
            )
        output_path = verify_file(
            record.get("output"),
            f"{prefab_id} baked output",
            failures,
        )
        if output_path:
            with Image.open(output_path) as image:
                if image.size != (512, 512):
                    failures.append(
                        f"{prefab_id} output size is {image.size}, expected 512x512"
                    )
                extrema = image.convert("RGB").getextrema()
                if all(low == high for low, high in extrema):
                    failures.append(f"{prefab_id} output is a flat-color image")

    if manifest.get("mountain_render_contract") != (
        EXPECTED_MOUNTAIN_RENDER_CONTRACT
    ):
        failures.append(
            "Mountain render contract does not preserve the Unity unlit "
            "opaque shader"
        )

    expected_family_counts = {
        "mountain_materials": 5,
        "tree_materials": 2,
    }
    for family in ("mountain_materials", "tree_materials"):
        records = manifest.get(family, [])
        if len(records) != expected_family_counts[family]:
            failures.append(
                f"{family} expected {expected_family_counts[family]} "
                f"records, found {len(records)}"
            )
        for index, record in enumerate(records):
            verify_file(
                record.get("source_material"),
                f"{family}[{index}] material",
                failures,
            )
            verify_file(
                record.get("source_texture"),
                f"{family}[{index}] texture",
                failures,
            )
            shader_path = verify_file(
                record.get("source_shader"),
                f"{family}[{index}] shader",
                failures,
            )
            if family == "mountain_materials":
                if record.get("render_contract") != (
                    EXPECTED_MOUNTAIN_RENDER_CONTRACT
                ):
                    failures.append(
                        f"{family}[{index}] does not preserve the source "
                        "mountain render contract"
                    )
                if (
                    shader_path
                    and shader_path.name != "z_cm_slg_mountain.shader"
                ):
                    failures.append(
                        f"{family}[{index}] uses the wrong Unity shader"
                    )
                if shader_path:
                    shader_text = shader_path.read_text(
                        encoding="utf-8",
                        errors="ignore",
                    )
                    required_markers = (
                        '"RenderType"="Opaque"',
                        "tex2D(_MainTex",
                        "color.rgb *= _SpriteColor",
                    )
                    if any(
                        marker not in shader_text
                        for marker in required_markers
                    ):
                        failures.append(
                            f"{family}[{index}] shader no longer has the "
                            "accepted opaque texture-color path"
                        )
                    forbidden_lighting_markers = (
                        "UnityObjectToWorldNormal",
                        "_WorldSpaceLightPos0",
                    )
                    if any(
                        marker in shader_text
                        for marker in forbidden_lighting_markers
                    ):
                        failures.append(
                            f"{family}[{index}] shader unexpectedly computes "
                            "runtime lighting"
                        )

    object_sources = manifest.get("object_sources", {})
    if not object_sources:
        failures.append("No selected Unity object sources were classified")
    guid_index = build_guid_index()
    for prefab_id, record in object_sources.items():
        source_prefab = PROJECT_ROOT / record.get("source_prefab", "")
        if not source_prefab.exists():
            failures.append(f"{prefab_id} source prefab is missing")
        if record.get("source_mesh"):
            verify_file(
                record["source_mesh"],
                f"{prefab_id} source mesh",
                failures,
            )
        if record.get("exported_obj"):
            output_path = verify_file(
                record["exported_obj"],
                f"{prefab_id} exported OBJ",
                failures,
            )
            if output_path and (
                record.get("vertex_count", 0) <= 0
                or record.get("triangle_count", 0) <= 0
            ):
                failures.append(f"{prefab_id} exported mesh is empty")
        representation = record.get("representation")
        if representation not in {
            "exact_source_fbx",
            "exact_source_fbx_render_layers",
            "exact_source_fbx_composite_waterfall",
            "exact_unity_mesh_export",
            "empty_source_prefab",
        }:
            failures.append(
                f"{prefab_id} has invalid representation {representation}"
            )
        if representation == "exact_source_fbx_render_layers":
            if prefab_id not in EXPECTED_RIVER_CORE_IDS:
                failures.append(
                    f"{prefab_id} unexpectedly uses the River Core "
                    "multi-layer contract"
                )
            else:
                verify_river_core_record(
                    prefab_id,
                    record,
                    world_manifest,
                    guid_index,
                    failures,
                )
        if representation == "exact_source_fbx":
            geometry = record.get("mountain_geometry_contract", {})
            expected_body_name = prefab_id
            expected_edge_name = f"{prefab_id}_edge"
            source_prefab_for_nodes = PROJECT_ROOT / record.get(
                "source_prefab", ""
            )
            prefab_collision_nodes = sorted(
                set(
                    re.findall(
                        rf"(?m)^\s*m_Name:\s*"
                        rf"({re.escape(prefab_id)}_Collision\S*)\s*$",
                        source_prefab_for_nodes.read_text(
                            encoding="utf-8", errors="ignore"
                        ),
                    )
                )
            )
            expected_excluded = [
                expected_edge_name,
                *prefab_collision_nodes,
            ]
            expected_import_base = f"SM_RokWorldBody_{prefab_id}"
            expected_body_asset_name = (
                f"{expected_import_base}_{expected_body_name}"
            )
            expected_body_asset = (
                f"{MOUNTAIN_BODY_MESH_DESTINATION}/"
                f"{expected_body_asset_name}.{expected_body_asset_name}"
            )
            if geometry.get("policy") != "split_fbx_exact_body_only":
                failures.append(
                    f"{prefab_id} does not split the source FBX by node"
                )
            if geometry.get("combine_meshes") is not False:
                failures.append(
                    f"{prefab_id} still combines visible and collision meshes"
                )
            if geometry.get("excluded_fbx_nodes") != expected_excluded:
                failures.append(
                    f"{prefab_id} has an incomplete excluded-node contract"
                )
            if geometry.get("ue_import_base_name") != expected_import_base:
                failures.append(
                    f"{prefab_id} has a non-deterministic split import name"
                )
            if geometry.get("ue_body_mesh_name") != (
                expected_body_asset_name
            ):
                failures.append(
                    f"{prefab_id} has the wrong UE body mesh name"
                )
            if geometry.get("ue_body_mesh_folder") != (
                MOUNTAIN_BODY_MESH_DESTINATION
            ):
                failures.append(
                    f"{prefab_id} has the wrong UE body mesh folder"
                )
            if geometry.get("ue_body_mesh_asset") != expected_body_asset:
                failures.append(
                    f"{prefab_id} has the wrong UE body mesh asset path"
                )
            if record.get("ue_mesh_name") != expected_body_asset_name:
                failures.append(
                    f"{prefab_id} still points at the old combined mesh"
                )
            if record.get("ue_mesh_folder") != (
                MOUNTAIN_BODY_MESH_DESTINATION
            ):
                failures.append(
                    f"{prefab_id} does not bind the mountain-body folder"
                )

            body_node = geometry.get("body_render_node", {})
            edge_node = geometry.get("edge_render_node", {})
            if body_node.get("game_object") != expected_body_name:
                failures.append(
                    f"{prefab_id} body node identity is not exact"
                )
            if edge_node.get("game_object") != expected_edge_name:
                failures.append(
                    f"{prefab_id} edge node identity is not exact"
                )
            if not body_node.get("mesh_file_id"):
                failures.append(
                    f"{prefab_id} lacks the Unity body MeshFilter fileID"
                )
            if not edge_node.get("mesh_file_id"):
                failures.append(
                    f"{prefab_id} lacks the Unity edge MeshFilter fileID"
                )
            if (
                body_node.get("mesh_guid")
                != geometry.get("source_fbx_guid")
                or edge_node.get("mesh_guid")
                != geometry.get("source_fbx_guid")
            ):
                failures.append(
                    f"{prefab_id} render nodes do not share the source FBX GUID"
                )
            expected_body_material = {
                "build_mountain_A": "build_mountain_A.mat",
                "build_mountain_B": "build_mountain_A.mat",
                "build_mountain_C": "build_mountain_A.mat",
                "build_mountain_E": "build_mountain_A.mat",
                "build_mountain_F": "build_mountain_F.mat",
                "build_mountain_D": "build_mountain_A.mat",
                "build_mountain_G": "build_mountain_A.mat",
                "build_mountain_A1": "build_mountain_F.mat",
                "build_mountain_A2": "build_mountain_F.mat",
                "build_mountain_A3": "build_mountain_F.mat",
                "build_mountain_A4": "build_mountain_A4.mat",
                "build_mountain_A5": "build_mountain_A4.mat",
                "build_mountain_A7": "build_mountain_A4.mat",
                "build_mountain_A8": "build_mountain_A8.mat",
                "build_bridge_A": "build_bridge_A.mat",
            }.get(prefab_id)
            if Path(body_node.get("material_path", "")).name != (
                expected_body_material
            ):
                failures.append(
                    f"{prefab_id} body material is not source-equivalent"
                )
            if geometry.get("runtime_edge_policy") != (
                "excluded_stability_scope"
            ):
                failures.append(
                    f"{prefab_id} edge visibility policy is ambiguous"
                )
            if geometry.get("runtime_collision_policy") != (
                "never_render_collision_nodes"
            ):
                failures.append(
                    f"{prefab_id} collision nodes may still render"
                )

            if source_prefab.exists():
                prefab_text = source_prefab.read_text(
                    encoding="utf-8",
                    errors="ignore",
                )
                for node in (body_node, edge_node):
                    for field in (
                        "game_object_file_id",
                        "mesh_file_id",
                        "mesh_guid",
                        "material_guid",
                    ):
                        value = str(node.get(field, ""))
                        if not value or value not in prefab_text:
                            failures.append(
                                f"{prefab_id} {node.get('game_object')} "
                                f"{field} is not present in the source prefab"
                            )
            source_mesh_record = record.get("source_mesh", {})
            source_mesh_path = PROJECT_ROOT / source_mesh_record.get(
                "path",
                "",
            )
            if source_mesh_path.exists():
                source_bytes = source_mesh_path.read_bytes()
                for node_name in [
                    expected_body_name,
                    *expected_excluded,
                ]:
                    if node_name.encode("utf-8") not in source_bytes:
                        failures.append(
                            f"{prefab_id} source FBX lacks node {node_name}"
                        )
        if representation == "exact_unity_mesh_export":
            reconstruction = record.get("vertex_shader_reconstruction", {})
            expected_reconstruction = {
                "shader": "customlandmaptree-16-nomask.shader",
                "center_semantic": "COLOR.xyz * 180",
                "curve_source": (
                    "ref/client-unity/Assets/BundleAssets/Scenes/"
                    "GameApp.unity"
                ),
                "tree_rotation_radians": 0.0,
                "formula": (
                    "center + (position - center) * tree_scale"
                ),
            }
            observed_reconstruction = dict(reconstruction)
            observed_tree_scale = observed_reconstruction.pop(
                "tree_scale",
                None,
            )
            observed_bake_dxf = observed_reconstruction.pop(
                "bake_dxf",
                None,
            )
            if observed_reconstruction != expected_reconstruction:
                failures.append(
                    f"{prefab_id} lacks exact Unity tree vertex reconstruction"
                )
            verify_float(
                observed_tree_scale,
                manifest.get("tree_scale_bake_value"),
                f"{prefab_id} reconstructed tree scale",
                failures,
            )
            verify_float(
                observed_bake_dxf,
                manifest.get("tree_scale_bake_dxf"),
                f"{prefab_id} reconstructed tree dxf",
                failures,
            )
            expected_axis_conversion = (
                "(Unity x,y,z) -> OBJ (x,-z,y) * 100 cm -> "
                "UE 5.8 Interchange imported mesh (x,z,y) * 100 cm"
            )
            if record.get("axis_conversion") != expected_axis_conversion:
                failures.append(
                    f"{prefab_id} does not compensate for the UE OBJ basis"
                )
            if record.get("obj_import_compensation") != {
                "ue_translator": (
                    "Interchange OBJ PositionToUEBasis(X,-Y,Z)"
                ),
                "obj_payload": "(Unity x,-z,y) * 100 cm",
                "imported_mesh": "(Unity x,z,y) * 100 cm",
                "actor_scale_contract": "(-5,+5,+5)",
                "final_world_vector": (
                    "500 * (-Unity x,+Unity z,+Unity y)"
                ),
            }:
                failures.append(
                    f"{prefab_id} has a stale OBJ import compensation contract"
                )

            projection = record.get("map_tactical_projection", {})
            verify_vector(
                projection.get("camera_forward_ue"),
                [0.0, math.sqrt(0.5), -math.sqrt(0.5)],
                f"{prefab_id} tactical camera forward",
                failures,
            )
            verify_vector(
                projection.get("camera_up_ue"),
                [0.0, math.sqrt(0.5), math.sqrt(0.5)],
                f"{prefab_id} tactical camera up",
                failures,
            )
            if projection.get("card_count", 0) <= 0:
                failures.append(
                    f"{prefab_id} has no reconstructed Unity tree cards"
                )
            if projection.get("card_count_below_100_cm") != 0:
                failures.append(
                    f"{prefab_id} still contains edge-on tactical tree cards"
                )
            minimum_height = projection.get(
                "minimum_card_screen_height_cm"
            )
            median_height = projection.get(
                "median_card_screen_height_cm"
            )
            if not isinstance(minimum_height, (int, float)) or minimum_height <= 100.0:
                failures.append(
                    f"{prefab_id} tactical tree height is below 100 cm"
                )
            if not isinstance(median_height, (int, float)) or median_height <= 100.0:
                failures.append(
                    f"{prefab_id} median tactical tree height is below 100 cm"
                )
            minimum_normal_dot = projection.get(
                "minimum_abs_face_normal_dot_camera_forward"
            )
            if (
                not isinstance(minimum_normal_dot, (int, float))
                or minimum_normal_dot < 0.7
            ):
                failures.append(
                    f"{prefab_id} tactical tree faces remain edge-on"
                )
    observed_mountain_ids = {
        prefab_id
        for prefab_id, record in object_sources.items()
        if record.get("representation") == "exact_source_fbx"
    }
    if observed_mountain_ids != EXPECTED_MOUNTAIN_IDS:
        failures.append(
            "Split mountain body coverage differs from the five audited "
            f"source prefabs: {sorted(observed_mountain_ids)}"
        )
    observed_river_core_ids = {
        prefab_id
        for prefab_id, record in object_sources.items()
        if record.get("representation")
        == "exact_source_fbx_render_layers"
    }
    if observed_river_core_ids != EXPECTED_RIVER_CORE_IDS:
        failures.append(
            "River Core A/B/C static contract coverage differs from the "
            f"audited set: {sorted(observed_river_core_ids)}"
        )
    waterfall_record = object_sources.get("Rive_CO_H", {})
    if (
        waterfall_record.get("representation")
        == "exact_source_fbx_composite_waterfall"
    ):
        try:
            expected_waterfall = parse_waterfall_geometry_contract(
                PROJECT_ROOT / waterfall_record["source_prefab"],
                guid_index,
            )
        except Exception as error:
            failures.append(
                f"Rive_CO_H waterfall contract could not be rebuilt: {error}"
            )
        else:
            if (
                waterfall_record.get("waterfall_geometry_contract")
                != expected_waterfall
            ):
                failures.append(
                    "Rive_CO_H waterfall contract differs from current source"
                )
    else:
        failures.append("Rive_CO_H lost its exact waterfall representation")

    coverage = manifest.get("full_scene_asset_coverage", {})
    full_scene = world_manifest.get("full_scene", {})
    expected_full_ground_ids = set(
        full_scene.get("ground_prefab_ids", [])
    )
    expected_full_object_ids = set(
        full_scene.get("object_prefab_ids", [])
    )
    if expected_full_ground_ids != EXPECTED_GROUND_IDS:
        failures.append(
            "World manifest full-scene ground ids differ from the accepted "
            "20-source terrain inventory"
        )
    if coverage.get("required_ground_prefab_count") != len(
        expected_full_ground_ids
    ):
        failures.append("Full-scene required ground count is inconsistent")
    if coverage.get("prepared_ground_prefab_count") != len(baked_ids):
        failures.append("Full-scene prepared ground count is inconsistent")
    if coverage.get("pending_ground_prefab_ids") != []:
        failures.append("Full-scene terrain still has pending source materials")
    if coverage.get("required_object_prefab_count") != len(
        expected_full_object_ids
    ):
        failures.append("Full-scene required object count is inconsistent")
    if coverage.get("prepared_object_prefab_count") != len(object_sources):
        failures.append("Full-scene prepared object count is inconsistent")
    expected_pending_object_ids = sorted(
        expected_full_object_ids - set(object_sources)
    )
    if coverage.get("pending_object_prefab_ids") != (
        expected_pending_object_ids
    ):
        failures.append(
            "Full-scene pending object inventory is not source-derived"
        )
    try:
        expected_instance_counts = (
            build_full_scene_object_instance_counts(world_manifest)
        )
    except Exception as error:
        failures.append(
            f"Could not rebuild full-scene object instance counts: {error}"
        )
        expected_instance_counts = {}
    expected_required_instances = sum(
        expected_instance_counts.get(prefab_id, 0)
        for prefab_id in expected_full_object_ids
    )
    expected_prepared_instances = sum(
        expected_instance_counts.get(prefab_id, 0)
        for prefab_id in object_sources
    )
    expected_pending_instances = (
        expected_required_instances - expected_prepared_instances
    )
    if coverage.get("required_object_instance_count") != (
        expected_required_instances
    ):
        failures.append(
            "Full-scene required object instance count is inconsistent"
        )
    if coverage.get("prepared_object_instance_count") != (
        expected_prepared_instances
    ):
        failures.append(
            "Full-scene prepared object instance count is inconsistent"
        )
    if coverage.get("pending_object_instance_count") != (
        expected_pending_instances
    ):
        failures.append(
            "Full-scene pending object instance count is inconsistent"
        )
    expected_percent = round(
        100.0
        * expected_prepared_instances
        / expected_required_instances,
        2,
    )
    verify_float(
        coverage.get("prepared_object_instance_percent"),
        expected_percent,
        "full-scene prepared object instance percent",
        failures,
        tolerance=0.0,
    )
    expected_pending_instance_counts = {
        prefab_id: expected_instance_counts[prefab_id]
        for prefab_id in expected_pending_object_ids
    }
    if coverage.get("pending_object_prefab_instance_counts") != (
        expected_pending_instance_counts
    ):
        failures.append(
            "Pending object instance statistics are not source-derived"
        )
    if (
        len(object_sources) != 205
        or expected_prepared_instances != 48096
        or len(expected_pending_object_ids) != 0
        or expected_pending_instances != 0
        or expected_percent != 100.0
    ):
        failures.append(
            "Full-scene coverage gate changed: expected 205/205 prefab "
            "sources and 48096/48096 instances (100.0%), with 0 pending "
            "prefab ids / 0 instances"
        )
    pending_object_reasons = manifest.get(
        "pending_object_reasons",
        {},
    )
    if sorted(pending_object_reasons) != expected_pending_object_ids:
        failures.append(
            "Pending object reasons do not cover the exact pending inventory"
        )
    for prefab_id, reason in pending_object_reasons.items():
        source_prefab = PROJECT_ROOT / reason.get(
            "source_prefab",
            "",
        )
        if not source_prefab.exists():
            failures.append(
                f"{prefab_id} pending source prefab is missing"
            )
        if reason.get("reason") != (
            "requires_additional_source_render_pipeline"
        ):
            failures.append(
                f"{prefab_id} has an ambiguous pending reason"
            )
        if reason.get("resolved_dependencies") != (
            full_scene.get("source_prefabs", {})
            .get(prefab_id, {})
            .get("resolved_dependencies", [])
        ):
            failures.append(
                f"{prefab_id} pending dependencies are not source-derived"
            )
    curve_source = verify_file(
        manifest.get("tree_scale_curve_source"),
        "tree-scale curve source",
        failures,
    )
    if curve_source and curve_source.name != "GameApp.unity":
        failures.append("Tree-scale curve does not come from GameApp.unity")
    semantics = manifest.get("source_semantics", {})
    tactical_dxf = semantics.get("map_tactical_dxf")
    expected_tactical_dxf = world_manifest.get(
        "map_tactical_camera",
        {},
    ).get("dxf")
    verify_float(
        tactical_dxf,
        expected_tactical_dxf,
        "source-derived map_tactical dxf",
        failures,
    )
    tree_semantics = semantics.get("tree", {})
    parsed_tree_keys = (
        parse_curve_source(
            curve_source,
            "m_tree_scale_curve",
            failures,
        )
        if curve_source
        else []
    )
    if tree_semantics.get("curve_keys") != parsed_tree_keys:
        failures.append(
            "Tree scale keys do not match GameApp.unity"
        )
    expected_tree_scale = (
        evaluate_curve(
            parsed_tree_keys,
            float(tactical_dxf),
            "tree scale curve",
            failures,
        )
        if parsed_tree_keys and isinstance(tactical_dxf, (int, float))
        else None
    )
    verify_float(
        manifest.get("tree_scale_bake_dxf"),
        tactical_dxf,
        "tree-scale bake dxf",
        failures,
    )
    verify_float(
        manifest.get("tree_scale_bake_value"),
        expected_tree_scale,
        "tree-scale bake value",
        failures,
    )
    if tree_semantics.get("runtime_visual_contract") != (
        "exact_source_tree_shader_scale_baked_at_map_tactical_"
        "with_runtime_curve_ratio"
    ):
        failures.append(
            "Tree scale contract does not require runtime curve rescaling"
        )

    player_city = manifest.get("player_city", {})
    expected_identity = {
        "init_id": 5,
        "building_type": 1,
        "ue_type": "TownCenter",
        "level": 1,
        "grid_x": -5,
        "grid_y": 5,
        "grid_width": 9,
        "grid_length": 9,
        "model_id": "build_1_101_1",
        "visual_sprite_name": "build_1_101_1",
        "visual_resolution": "exact",
        "representation": "runtime sprite card",
    }
    if player_city.get("identity") != expected_identity:
        failures.append(
            "Player city is not the exact Unity runtime build_1_101_1 "
            "TownCenter record"
        )
    verify_file(
        player_city.get("source_runtime_manifest"),
        "player-city runtime export manifest",
        failures,
    )
    source_assets = player_city.get("source_assets", {})
    expected_source_names = {
        "sprite": "build_1_101_1.png",
        "mask": "build_1_101_1_mask.png",
        "building_prefab": "build_1_101_1.prefab",
        "material": "city_building_night.mat",
        "shader": "cm_city_mask_night.shader",
        "city_container_prefab": "CityBuildingContainer.prefab",
        "lod_implementation": "LevelDetailCastle.cs",
        "world_integration": "CityGlobalMediator.cs",
        "age_size_table": "CityAgeSize.csv",
        "building_model_table": "BuildingModelData.csv",
        "runtime_exporter": "CodexExportRuntimeCityBuildings.cs",
    }
    for role, expected_name in expected_source_names.items():
        path = verify_file(
            source_assets.get(role),
            f"player-city {role}",
            failures,
        )
        if path and path.name != expected_name:
            failures.append(
                f"Player-city {role} is {path.name}, expected {expected_name}"
            )
    expected_guids = {
        "sprite": "57ec7ddf0b743a140b2707fe14b536e2",
        "mask": "c37165d75575008419379c96242599ba",
        "material": "4e9db9e5983dd8e46bcbc881f14fe50c",
    }
    for role, expected_guid in expected_guids.items():
        if source_assets.get(role, {}).get("guid") != expected_guid:
            failures.append(
                f"Player-city {role} GUID no longer matches Unity source"
            )

    if player_city.get("runtime_export_context") != {
        "container_yaw": -45.0,
        "captured_runtime_lod_scale": 1.121089,
        "main_city_sprite_color": [0.994559, 0.844559, 0.737059, 1.0],
        "note": (
            "captured_runtime_lod_scale is exporter evidence only; the UE "
            "world-map actor evaluates the source LevelDetailCastle curve "
            "from its own camera dxf"
        ),
    }:
        failures.append(
            "Player-city runtime export context is incomplete or stale"
        )
    fallback_obj_path = verify_file(
        player_city.get("fallback_obj"),
        "player-city fallback OBJ",
        failures,
    )
    fallback_obj = player_city.get("fallback_obj", {})
    if fallback_obj_path and (
        fallback_obj.get("vertex_count") != 240
        or fallback_obj.get("triangle_count") != 238
        or fallback_obj.get("usage")
        != "evidence_only_not_default_world_city_visual"
    ):
        failures.append(
            "Player-city fallback OBJ evidence is incomplete or used as "
            "the default visual"
        )

    baked_city_path = verify_file(
        player_city.get("baked_texture"),
        "player-city baked texture",
        failures,
    )
    baked_city_record = player_city.get("baked_texture", {})
    if (
        baked_city_record.get("sha256")
        != "b2ee142e838faa4a692b1bde008a47108a3e20f5300f4864d57f096154988ce1"
    ):
        failures.append(
            "Player-city baked texture is not the audited build_1_101_1 export"
        )
    if baked_city_path:
        with Image.open(baked_city_path) as image:
            rgba = np.asarray(image.convert("RGBA"), dtype=np.uint8)
            observed_size = [int(image.width), int(image.height)]
        alpha = rgba[:, :, 3]
        observed_alpha_stats = {
            "transparent_pixel_count": int(np.count_nonzero(alpha == 0)),
            "opaque_pixel_count": int(np.count_nonzero(alpha == 255)),
            "partial_alpha_pixel_count": int(
                np.count_nonzero((alpha > 0) & (alpha < 255))
            ),
            "unique_alpha_value_count": int(np.unique(alpha).size),
        }
        if observed_size != [1024, 888]:
            failures.append(
                f"Player-city baked texture is {observed_size}, expected "
                "1024x888"
            )
        if observed_size != baked_city_record.get("image_size"):
            failures.append(
                "Player-city baked texture dimensions differ from its manifest"
            )
        if observed_alpha_stats != {
            "transparent_pixel_count": 328428,
            "opaque_pixel_count": 482833,
            "partial_alpha_pixel_count": 98051,
            "unique_alpha_value_count": 256,
        }:
            failures.append(
                "Player-city baked texture alpha coverage is no longer exact"
            )
        if observed_alpha_stats != baked_city_record.get("alpha_stats"):
            failures.append(
                "Player-city alpha statistics differ from its manifest"
            )

    shader_contract = player_city.get("shader_contract", {})
    if shader_contract != {
        "shader_name": "custom/map/cm_city_mask_night",
        "queue_tag": "Transparent-5",
        "zwrite": "Off",
        "material_render_queue": 2996,
        "ue_blend_mode": "translucent",
        "ue_shading_model": "unlit",
    }:
        failures.append(
            "Player-city UE material contract does not preserve the Unity "
            "transparent city shader"
        )

    lod_contract = player_city.get("lod_contract", {})
    city_semantics = semantics.get("city", {})
    city_source_path = verify_file(
        city_semantics.get("source"),
        "city LOD source",
        failures,
    )
    age_source_path = verify_file(
        city_semantics.get("age_size_source"),
        "city age-size source",
        failures,
    )
    parsed_city_keys = (
        parse_curve_source(
            city_source_path,
            "m_scale_curve",
            failures,
        )
        if city_source_path
        else []
    )
    if city_semantics.get("curve_keys") != parsed_city_keys:
        failures.append(
            "City LOD keys do not match CityBuildingContainer.prefab"
        )
    city_text = (
        city_source_path.read_text(encoding="utf-8")
        if city_source_path
        else ""
    )
    maximum_matches = re.findall(
        r"^\s*m_max_scale:\s*([-+0-9.eE]+)\s*$",
        city_text,
        re.M,
    )
    expected_base_max = (
        float(maximum_matches[0])
        if len(maximum_matches) == 1
        else None
    )
    if len(maximum_matches) != 1:
        failures.append("City LOD source has no unique m_max_scale")
    age_rows = (
        parse_age_sizes(age_source_path, failures)
        if age_source_path
        else []
    )
    age_one_rows = [row for row in age_rows if row["age"] == 1]
    maximum_age_rows = [
        row
        for row in age_rows
        if row["age"] == max(
            (item["age"] for item in age_rows),
            default=-1,
        )
    ]
    expected_age_size = (
        age_one_rows[0]["size"]
        if len(age_one_rows) == 1
        else None
    )
    expected_max_age_size = (
        maximum_age_rows[0]["size"]
        if len(maximum_age_rows) == 1
        else None
    )
    if expected_age_size is None or expected_max_age_size is None:
        failures.append("CityAgeSize.csv has ambiguous age rows")
    expected_city_curve_scale = (
        evaluate_curve(
            parsed_city_keys,
            float(tactical_dxf),
            "city LOD curve",
            failures,
        )
        if parsed_city_keys and isinstance(tactical_dxf, (int, float))
        else None
    )
    verify_float(
        lod_contract.get("initial_dxf"),
        tactical_dxf,
        "player-city initial dxf",
        failures,
    )
    verify_float(
        lod_contract.get("base_max_scale"),
        expected_base_max,
        "player-city base max scale",
        failures,
    )
    verify_float(
        lod_contract.get("age_size"),
        expected_age_size,
        "player-city age-1 size",
        failures,
    )
    verify_float(
        lod_contract.get("max_age_size"),
        expected_max_age_size,
        "player-city max age size",
        failures,
    )
    expected_age_scale_cap = (
        expected_base_max * expected_age_size / expected_max_age_size
        if all(
            isinstance(value, (int, float))
            for value in (
                expected_base_max,
                expected_age_size,
                expected_max_age_size,
            )
        )
        else None
    )
    verify_float(
        lod_contract.get("age_scale_cap"),
        expected_age_scale_cap,
        "player-city age LOD cap",
        failures,
    )
    verify_float(
        lod_contract.get("initial_scale"),
        min(expected_city_curve_scale, expected_age_scale_cap)
        if all(
            isinstance(value, (int, float))
            for value in (
                expected_city_curve_scale,
                expected_age_scale_cap,
            )
        )
        else None,
        "player-city initial LOD scale",
        failures,
    )
    expected_curve_keys = [
        {
            "dxf": float(key["dxf"]),
            "scale": float(key["scale"]),
        }
        for key in parsed_city_keys
    ]
    if lod_contract.get("curve_keys") != expected_curve_keys:
        failures.append(
            "Player-city LOD keys do not match CityBuildingContainer.prefab"
        )

    presentation = player_city.get("ue_presentation", {})
    verify_vector(
        presentation.get("logical_world_root_cm"),
        [0.0, 0.0, 0.0],
        "player-city logical root",
        failures,
    )
    verify_vector(
        presentation.get("selection_anchor_offset_cm"),
        [353.5533905932737, 282.842712474619, 5.0],
        "player-city selection anchor",
        failures,
    )
    verify_vector(
        presentation.get("base_visual_center_offset_cm"),
        [-1.4142135623730994, 60.81118318204309, 61.0],
        "player-city base visual center",
        failures,
    )
    verify_vector(
        presentation.get("base_card_size_cm"),
        [639.104, 554.223],
        "player-city base card size",
        failures,
    )
    verify_vector(
        presentation.get("base_plane_scale"),
        [639.104 / 6000.0, -554.223 / 6000.0, 1.0],
        "player-city source plane scale",
        failures,
    )
    initial_scale = expected_age_scale_cap
    expected_center = [
        353.5533905932737 + -1.4142135623730994 * initial_scale,
        282.842712474619 + 60.81118318204309 * initial_scale,
        5.0 + 61.0 * initial_scale,
    ]
    verify_vector(
        presentation.get("initial_visual_center_cm"),
        expected_center,
        "player-city initial visual center",
        failures,
    )
    diagonal = math.sqrt(0.5)
    verify_vector(
        presentation.get("initial_visual_bounds_extent_cm"),
        [
            639.104 * initial_scale * 0.5,
            554.223 * initial_scale * 0.5 * diagonal,
            554.223 * initial_scale * 0.5 * diagonal,
        ],
        "player-city initial visual bounds extent",
        failures,
    )
    flattened_city = json.dumps(player_city, ensure_ascii=False)
    if "Castle_6_5" in flattened_city:
        failures.append(
            "Player-city source chain still references Castle_6_5 diagnostics"
        )

    proxy_count = sum(
        1
        for record in object_sources.values()
        if "proxy" in record.get("representation", "")
    )
    if proxy_count:
        failures.append(
            f"World-map object sources still contain {proxy_count} phase-1 proxies"
        )

    result = {
        "manifest": str(MANIFEST_PATH.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "failure_count": len(failures),
        "failures": failures,
        "baked_ground_material_count": len(baked),
        "object_source_count": len(object_sources),
        "object_representation_summary": manifest.get(
            "object_representation_summary",
            {},
        ),
        "full_scene_asset_coverage": manifest.get(
            "full_scene_asset_coverage",
            {},
        ),
    }
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    VERIFY_PATH.write_text(
        json.dumps(result, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    if failures:
        print("Rok Unity world-map asset verification FAILED")
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)
    print(
        "Rok Unity world-map asset verification passed: "
        f"ground_materials={result['baked_ground_material_count']} "
        f"object_sources={result['object_source_count']}"
    )


if __name__ == "__main__":
    main()
