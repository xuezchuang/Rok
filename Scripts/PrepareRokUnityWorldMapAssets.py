import csv
import hashlib
import json
import math
import re
import struct
from pathlib import Path

import numpy as np
from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
UNITY_ASSETS_ROOT = PROJECT_ROOT / "ref" / "client-unity" / "Assets"
MIGRATION_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
WORLD_MANIFEST_PATH = MIGRATION_DIR / "unity_world_map.json"
OUTPUT_ROOT = MIGRATION_DIR / "WorldMapPrototype"
TEXTURE_OUTPUT_ROOT = OUTPUT_ROOT / "Textures"
MESH_OUTPUT_ROOT = OUTPUT_ROOT / "Meshes"
OUTPUT_MANIFEST_PATH = MIGRATION_DIR / "unity_world_map_assets.json"
CITY_RUNTIME_MANIFEST_PATH = (
    MIGRATION_DIR / "unity_runtime_city_buildings_obj_manifest.json"
)
CITY_BAKED_TEXTURE_PATH = (
    MIGRATION_DIR
    / "UnityRuntimeCityBuildingsObj"
    / "03_Init_005_build_1_101_1_sprite_BakedSprite.png"
)
CITY_CONTAINER_PREFAB_PATH = (
    UNITY_ASSETS_ROOT
    / "BundleAssets"
    / "Scene"
    / "City"
    / "city_function"
    / "CityBuildingContainer.prefab"
)
CITY_LOD_CODE_PATH = (
    UNITY_ASSETS_ROOT
    / "Scripts"
    / "Client"
    / "LodManager"
    / "LevelDetailCastle.cs"
)
CITY_WORLD_INTEGRATION_PATH = (
    UNITY_ASSETS_ROOT
    / "Scripts"
    / "Hotfix"
    / "MVC"
    / "GlobalMediator"
    / "CityGlobalMediator.cs"
)
CITY_EXPORTER_PATH = (
    UNITY_ASSETS_ROOT
    / "Editor"
    / "CodexExportRuntimeCityBuildings.cs"
)
CITY_AGE_SIZE_PATH = (
    PROJECT_ROOT
    / "ref"
    / "config-tables"
    / "编译客户端配置"
    / "CSV"
    / "CityAgeSize.csv"
)
CITY_BUILDING_MODEL_PATH = (
    PROJECT_ROOT
    / "ref"
    / "config-tables"
    / "编译客户端配置"
    / "CSV"
    / "BuildingModelData.csv"
)
CITY_EXPECTED_BAKED_SHA256 = (
    "b2ee142e838faa4a692b1bde008a47108a3e20f5300f4864d57f096154988ce1"
)
CITY_INITIAL_DXF = None
CITY_BASE_MAX_LOD_SCALE = None
CITY_AGE_SIZE = None
CITY_MAX_AGE_SIZE = None
CITY_AGE_LOD_SCALE_CAP = None
CITY_SCALE_CURVE_KEYS = []
CITY_INITIAL_CURVE_SCALE = None
CITY_SOURCE_PLANE_SIZE_CM = 6000.0
CITY_CONTAINER_YAW_DEGREES = -45.0
UNITY_TO_UE_WORLD_SCALE = 500.0
BAKE_SIZE = 512

MOUNTAIN_MATERIAL_PATHS = (
    "ref/client-unity/Assets/BundleAssets/land/Material/build_mountain_A.mat",
    "ref/client-unity/Assets/BundleAssets/land/Material/build_mountain_F.mat",
    "ref/client-unity/Assets/BundleAssets/land/Material/build_mountain_A4.mat",
    "ref/client-unity/Assets/BundleAssets/land/Material/build_mountain_A8.mat",
    "ref/client-unity/Assets/BundleAssets/land/Material/build_bridge_A.mat",
)
MOUNTAIN_RENDER_CONTRACT = {
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
TREE_MATERIAL_PATHS = (
    "ref/client-unity/Assets/BundleAssets/land/Material/map_tree_nomask.mat",
    "ref/client-unity/Assets/BundleAssets/land/Material/map_tree_nomask_1.mat",
)
TREE_SCALE_CURVE_SOURCE = (
    "ref/client-unity/Assets/BundleAssets/Scenes/GameApp.unity"
)
TREE_SCALE_BAKE_DXF = None
TREE_VERTEX_SCALE = None
TREE_VERTEX_CENTER_SCALE = 180.0
TREE_VERTEX_ROTATION_RADIANS = 0.0
MOUNTAIN_FBX_PATHS = {
    "build_mountain_A": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_A.FBX",
    "build_mountain_B": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_B.FBX",
    "build_mountain_C": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_C.FBX",
    "build_mountain_E": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_E.FBX",
    "build_mountain_F": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_F.FBX",
    "build_mountain_D": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_D.FBX",
    "build_mountain_G": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_G.FBX",
    "build_mountain_A1": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_A1.FBX",
    "build_mountain_A2": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_A2.FBX",
    "build_mountain_A3": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_A3.FBX",
    "build_mountain_A4": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_A4.FBX",
    "build_mountain_A5": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_A5.FBX",
    "build_mountain_A7": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_A7.FBX",
    "build_mountain_A8": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_A8.FBX",
    "build_bridge_A": "ref/client-unity/Assets/BundleAssets/land/Fbx/build_bridge_A.FBX",
}
MOUNTAIN_BODY_MESH_DESTINATION = (
    "/Game/RokUnityWorldMapPrototype/Meshes/MountainBodies"
)

WATERFALL_PREFAB_ID = "Rive_CO_H"
WATERFALL_LAYER_MESH_DESTINATION = (
    "/Game/RokUnityWorldMapPrototype/Meshes/WaterfallLayers"
)
WATERFALL_MATERIAL_SOURCES = {
    "M_WorldSource_build_mountain_T": {
        "source_material": (
            "ref/client-unity/Assets/BundleAssets/land/Material/"
            "build_mountain_T.mat"
        ),
        "ue_render": {
            "blend_mode": "opaque",
            "shading_model": "unlit",
            "shader_note": "z_cm_slg_mountain_edge.shader",
        },
    },
    "M_WorldWaterfall_WaterLight": {
        "source_material": (
            "ref/client-unity/Assets/BundleAssets/land/Material/"
            "water_01_light 1.mat"
        ),
        "ue_render": {
            "blend_mode": "additive",
            "shading_model": "unlit",
            "shader_note": (
                "Unity builtin shader fileID 10721 "
                "(Particles additive family); UE static approximation"
            ),
        },
    },
}
RIVER_CORE_IDS = (
    "Rive_CO_A",
    "Rive_CO_B",
    "Rive_CO_C",
    "Rive_CO_D",
    "Rive_CO_E",
    "Rive_CO_F",
    "Ocean_CO_A",
)
RIVER_CORE_FBX_PATHS = {
    prefab_id: (
        "ref/client-unity/Assets/BundleAssets/land/Fbx/"
        f"{prefab_id}.FBX"
    )
    for prefab_id in RIVER_CORE_IDS
}
RIVER_CORE_INNER_COLLISION_NODES = {
    "Rive_CO_A": "Rive_CO_A_Collision_neiw",
    "Rive_CO_B": "Rive_CO_B_Collision_nei",
    "Rive_CO_C": "Rive_CO_C_Collision_nei",
    "Rive_CO_D": "Rive_CO_D_Collision_nei",
    "Rive_CO_E": "Rive_CO_E_Collision_nei",
    "Rive_CO_F": "Rive_CO_F_Collision_nei",
    "Ocean_CO_A": "Ocean_CO_A_Collision_nei",
}
RIVER_EXTRA_COLLISION_NODES = {
    "Ocean_CO_A": ("Ocean_CO_A_Collision_building",),
}
RIVER_WATER_MATERIAL_GUIDS = {
    "Ocean_CO_A": "b6dfeee56d363614691bb670c328ebea",
}
RIVER_DEFAULT_WATER_MATERIAL_GUID = "536f54f1b429164408f7aba6c1911c83"
RIVER_EDGE_MATERIAL_PATH = (
    "ref/client-unity/Assets/BundleAssets/land/Material/"
    "river_CO_A_edge.mat"
)
RIVER_WATER_MATERIAL_PATH = (
    "ref/client-unity/Assets/BundleAssets/land/Material/"
    "river_CO_A_water.mat"
)
RIVER_EDGE_SHADER_NAME = (
    "custom/landmap/custommap_60_map_lowland_edge"
)
RIVER_WATER_SHADER_NAME = "custom/land/-23_water"


def rel(path):
    return str(path.relative_to(PROJECT_ROOT)).replace("\\", "/")


def safe_name(value):
    # Must match CreateRokUnityWorldMapPrototype.safe_name so manifest
    # ue_mesh_name equals the imported UE asset name.
    sanitized = re.sub(r"[^A-Za-z0-9_]+", "_", str(value)).strip("_")
    return sanitized or "Unnamed"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


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
            index[match.group(1).lower()] = asset_path
    return index


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


def parse_mountain_geometry_contract(
    prefab_id,
    prefab_path,
    source_fbx_path,
    guid_index,
):
    game_objects = {}
    mesh_filters = {}
    mesh_renderers = {}
    for item in parse_unity_yaml_objects(prefab_path):
        body = item["body"]
        if item["class_id"] == 1:
            name_match = re.search(r"(?m)^\s*m_Name:\s*(.+?)\s*$", body)
            if name_match:
                game_objects[item["file_id"]] = name_match.group(1).strip()
        elif item["class_id"] == 33:
            game_object_match = re.search(
                r"m_GameObject:\s*\{fileID:\s*(\d+)\}",
                body,
            )
            mesh_match = re.search(
                r"m_Mesh:\s*\{fileID:\s*(-?\d+),\s*"
                r"guid:\s*([0-9a-fA-F]+)",
                body,
            )
            if game_object_match and mesh_match:
                mesh_filters.setdefault(
                    game_object_match.group(1),
                    [],
                ).append(
                    {
                        "component_file_id": item["file_id"],
                        "mesh_file_id": mesh_match.group(1),
                        "mesh_guid": mesh_match.group(2).lower(),
                    }
                )
        elif item["class_id"] == 23:
            game_object_match = re.search(
                r"m_GameObject:\s*\{fileID:\s*(\d+)\}",
                body,
            )
            material_match = re.search(
                r"m_Materials:\s*\n\s*-\s*\{fileID:\s*-?\d+,\s*"
                r"guid:\s*([0-9a-fA-F]+)",
                body,
            )
            enabled_match = re.search(
                r"(?m)^\s*m_Enabled:\s*(\d+)\s*$",
                body,
            )
            if game_object_match and material_match:
                mesh_renderers.setdefault(
                    game_object_match.group(1),
                    [],
                ).append(
                    {
                        "component_file_id": item["file_id"],
                        "enabled": (
                            int(enabled_match.group(1))
                            if enabled_match
                            else 1
                        ),
                        "material_guid": material_match.group(1).lower(),
                    }
                )

    def render_node(node_name):
        candidates = []
        for game_object_id, game_object_name in game_objects.items():
            if game_object_name != node_name:
                continue
            filters = mesh_filters.get(game_object_id, [])
            renderers = [
                renderer
                for renderer in mesh_renderers.get(game_object_id, [])
                if renderer["enabled"] == 1
            ]
            if len(filters) == 1 and len(renderers) == 1:
                material_path = guid_index.get(
                    renderers[0]["material_guid"]
                )
                candidates.append(
                    {
                        "game_object": node_name,
                        "game_object_file_id": game_object_id,
                        **filters[0],
                        **renderers[0],
                        "material_path": (
                            rel(material_path)
                            if material_path and material_path.exists()
                            else None
                        ),
                    }
                )
        if len(candidates) != 1:
            raise RuntimeError(
                f"{rel(prefab_path)} expected exactly one enabled render node "
                f"named {node_name}, found {len(candidates)}"
            )
        return candidates[0]

    body_node_name = prefab_id
    edge_node_name = f"{prefab_id}_edge"
    body_node = render_node(body_node_name)
    edge_node = render_node(edge_node_name)
    fbx_meta_path = Path(str(source_fbx_path) + ".meta")
    fbx_meta_text = fbx_meta_path.read_text(
        encoding="utf-8",
        errors="ignore",
    )
    fbx_guid_match = re.search(
        r"(?m)^guid:\s*([0-9a-fA-F]+)\s*$",
        fbx_meta_text,
    )
    if not fbx_guid_match:
        raise RuntimeError(f"Missing Unity GUID in {rel(fbx_meta_path)}")
    fbx_guid = fbx_guid_match.group(1).lower()
    for node in (body_node, edge_node):
        if node["mesh_guid"] != fbx_guid:
            raise RuntimeError(
                f"{rel(prefab_path)} render node {node['game_object']} does "
                "not reference the audited source FBX"
            )

    collision_nodes = sorted(
        name
        for name in game_objects.values()
        if name.startswith(f"{prefab_id}_Collision")
    )
    fbx_bytes = source_fbx_path.read_bytes()
    for node_name in [body_node_name, edge_node_name, *collision_nodes]:
        if node_name.encode("utf-8") not in fbx_bytes:
            raise RuntimeError(
                f"{rel(source_fbx_path)} lacks expected FBX node {node_name}"
            )

    import_base_name = f"SM_RokWorldBody_{prefab_id}"
    body_mesh_name = f"{import_base_name}_{body_node_name}"
    names_with_mesh = {
        game_objects[game_object_id]
        for game_object_id in mesh_filters
        if game_object_id in game_objects
    }
    names_with_enabled_renderer = {
        game_objects[game_object_id]
        for game_object_id, renderer_list in mesh_renderers.items()
        if game_object_id in game_objects
        and any(renderer["enabled"] == 1 for renderer in renderer_list)
    }
    additional_render_nodes = sorted(
        name
        for name in names_with_mesh & names_with_enabled_renderer
        if name not in {body_node_name, edge_node_name}
        and not name.startswith(f"{prefab_id}_Collision")
    )
    return {
        "policy": "split_fbx_exact_body_only",
        "combine_meshes": False,
        "source_fbx_guid": fbx_guid,
        "body_render_node": body_node,
        "edge_render_node": edge_node,
        "excluded_fbx_nodes": [
            edge_node_name,
            *collision_nodes,
        ],
        "additional_unrendered_render_nodes": additional_render_nodes,
        "ue_import_base_name": import_base_name,
        "ue_body_mesh_name": body_mesh_name,
        "ue_body_mesh_folder": MOUNTAIN_BODY_MESH_DESTINATION,
        "ue_body_mesh_asset": (
            f"{MOUNTAIN_BODY_MESH_DESTINATION}/"
            f"{body_mesh_name}.{body_mesh_name}"
        ),
        "runtime_edge_policy": "excluded_stability_scope",
        "runtime_collision_policy": "never_render_collision_nodes",
    }


def parse_inline_vector(body, field_name, dimensions):
    match = re.search(
        rf"(?m)^\s*{re.escape(field_name)}:\s*\{{([^}}]+)\}}\s*$",
        body,
    )
    if not match:
        raise RuntimeError(f"Missing Unity vector field {field_name}")
    values = {}
    for key, value in re.findall(
        r"([xyzw]):\s*([-+0-9.eE]+)",
        match.group(1),
    ):
        values[key] = float(value)
    keys = "xyzw"[:dimensions]
    if set(values) != set(keys):
        raise RuntimeError(
            f"Unity vector {field_name} has fields {sorted(values)}, "
            f"expected {list(keys)}"
        )
    return {key: values[key] for key in keys}


def parse_material_float(material_text, property_name):
    match = re.search(
        rf"(?m)^\s*-\s*{re.escape(property_name)}:\s*"
        r"([-+0-9.eE]+)\s*$",
        material_text,
    )
    if not match:
        raise RuntimeError(
            f"Missing Unity material float {property_name}"
        )
    return float(match.group(1))


def parse_shader_name(shader_path):
    shader_text = shader_path.read_text(
        encoding="utf-8-sig",
        errors="strict",
    )
    match = re.search(r'^\s*Shader\s+"([^"]+)"', shader_text, re.M)
    if not match:
        raise RuntimeError(f"Missing shader name in {rel(shader_path)}")
    return match.group(1)


def parse_river_material_contract(
    material_path,
    guid_index,
    role,
):
    material_text = material_path.read_text(
        encoding="utf-8",
        errors="strict",
    )
    shader_guid = parse_shader_guid(material_text)
    shader_path = guid_index.get(shader_guid)
    if not shader_path or not shader_path.exists():
        raise RuntimeError(
            f"{rel(material_path)} has unresolved shader {shader_guid}"
        )
    shader_name = parse_shader_name(shader_path)
    expected_shader_name = (
        RIVER_EDGE_SHADER_NAME
        if role == "edge"
        else RIVER_WATER_SHADER_NAME
    )
    if shader_name != expected_shader_name:
        raise RuntimeError(
            f"{rel(material_path)} uses {shader_name}, expected "
            f"{expected_shader_name}"
        )

    texture_properties = (
        ("_MainTex",)
        if role == "edge"
        else ("_BumpMap", "_MainTex")
    )
    textures = {}
    for property_name in texture_properties:
        environment = parse_texture_environment(
            material_text,
            property_name,
        )
        texture_path = resolve_environment(
            environment,
            guid_index,
            property_name,
        )
        textures[property_name] = {
            "guid": environment["guid"],
            "source_texture": source_record(texture_path),
            "scale": {
                "x": environment["scale"][0],
                "y": environment["scale"][1],
            },
            "offset": {
                "x": environment["offset"][0],
                "y": environment["offset"][1],
            },
            "shader_usage": (
                "sampled"
                if role == "edge" or property_name == "_BumpMap"
                else "serialized_but_not_sampled_by_source_shader"
            ),
        }

    result = {
        "source_material": source_record(material_path),
        "shader_guid": shader_guid,
        "source_shader": source_record(shader_path),
        "shader_name": shader_name,
        "textures": textures,
    }
    if role == "edge":
        result["shader_contract"] = {
            "queue": "Transparent-60",
            "render_type": "Transparent",
            "blend": "SrcAlpha OneMinusSrcAlpha",
            "zwrite": "Off",
            "rgb": (
                "_MainTex.rgb multiplied by Unity ambient plus "
                "directional-light vertex lighting"
            ),
            "opacity": "interpolated vertex color alpha",
        }
    else:
        result["shader_contract"] = {
            "queue": "Transparent-23",
            "render_type": "Transparent",
            "blend": (
                "SrcAlpha OneMinusSrcAlpha, "
                "SrcAlpha OneMinusSrcAlpha"
            ),
            "zwrite": "Off",
            "bump_tiling": list(
                parse_color(material_text, "_BumpTiling")
            ),
            "speed": list(parse_color(material_text, "_Speed")),
            "base_color": list(
                parse_color(material_text, "_BaseColor")
            ),
            "world_light_dir": list(
                parse_color(material_text, "_WorldLightDir")
            ),
            "near": parse_material_float(material_text, "_Near"),
            "far": parse_material_float(material_text, "_Far"),
            "shininess": parse_material_float(
                material_text,
                "_Shininess",
            ),
            "gloss": parse_material_float(material_text, "_Gloss"),
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
        }
    return result



FBX_BINARY_MODEL_PATTERN = re.compile(
    rb"L(.{8})S(....)((?:[\x20-\x7e]{2,128})\x00\x01Model)S",
    re.DOTALL,
)
FBX_BINARY_GEOMETRY_PATTERN = re.compile(
    rb"L(.{8})S(....)((?:[\x20-\x7e]{0,128})\x00\x01Geometry)S",
    re.DOTALL,
)
FBX_BINARY_CONNECTION_PATTERN = re.compile(
    rb"S\x02\x00\x00\x00OOL(.{8})L(.{8})",
    re.DOTALL,
)


def parse_fbx_binary_nodes(fbx_path):
    """Parse actual Model/Geometry node names from a binary FBX.

    Unity prefab GameObject names are not guaranteed to exist as FBX
    nodes (Ocean_CO_A.prefab has no plain _Collision child even though
    the FBX carries _Collision_nei/_Collision_building), and UE names
    imported static meshes after FBX Geometry nodes, which can be plain
    "Mesh" placeholders. The geometry->model map lets the UE importer
    translate those placeholder names back to real Model names.
    """
    data = fbx_path.read_bytes()
    models = {}
    model_order = []
    for match in FBX_BINARY_MODEL_PATTERN.finditer(data):
        node_id = int.from_bytes(match.group(1), "little", signed=True)
        declared = int.from_bytes(match.group(2), "little")
        raw_name = match.group(3)
        if declared != len(raw_name):
            continue
        name = raw_name[: -len(b"\x00\x01Model")].decode("ascii")
        if node_id not in models:
            models[node_id] = name
            model_order.append(name)
    geometry_ids = []
    for match in FBX_BINARY_GEOMETRY_PATTERN.finditer(data):
        node_id = int.from_bytes(match.group(1), "little", signed=True)
        declared = int.from_bytes(match.group(2), "little")
        raw_name = match.group(3)
        if declared != len(raw_name):
            continue
        geometry_ids.append(node_id)
    # Geometry node names can be empty (UE then imports them as
    # "Mesh"/"Mesh_ncl_N" placeholders), so the model mapping must be
    # order-based via OO connections instead of name-based.
    geometry_parent = {}
    for match in FBX_BINARY_CONNECTION_PATTERN.finditer(data):
        child_id = int.from_bytes(match.group(1), "little", signed=True)
        parent_id = int.from_bytes(match.group(2), "little", signed=True)
        if child_id in geometry_ids and parent_id in models:
            geometry_parent[child_id] = models[parent_id]
    geometry_model_order = [
        geometry_parent[node_id]
        for node_id in geometry_ids
        if node_id in geometry_parent
    ]
    if not model_order:
        raise RuntimeError(
            f"{rel(fbx_path)} exposes no binary FBX Model nodes"
        )
    return model_order, geometry_model_order


def parse_river_core_geometry_contract(
    prefab_id,
    prefab_path,
    source_fbx_path,
    guid_index,
):
    game_objects = {}
    transforms = {}
    mesh_filters = {}
    mesh_renderers = {}
    for item in parse_unity_yaml_objects(prefab_path):
        body = item["body"]
        if item["class_id"] == 1:
            name_match = re.search(
                r"(?m)^\s*m_Name:\s*(.+?)\s*$",
                body,
            )
            layer_match = re.search(
                r"(?m)^\s*m_Layer:\s*(\d+)\s*$",
                body,
            )
            active_match = re.search(
                r"(?m)^\s*m_IsActive:\s*(\d+)\s*$",
                body,
            )
            if name_match:
                game_objects[item["file_id"]] = {
                    "name": name_match.group(1).strip(),
                    "layer": int(layer_match.group(1))
                    if layer_match
                    else None,
                    "active": int(active_match.group(1))
                    if active_match
                    else None,
                }
        elif item["class_id"] == 4:
            game_object_match = re.search(
                r"m_GameObject:\s*\{fileID:\s*(\d+)\}",
                body,
            )
            if game_object_match:
                transforms[game_object_match.group(1)] = {
                    "transform_file_id": item["file_id"],
                    "local_position": parse_inline_vector(
                        body,
                        "m_LocalPosition",
                        3,
                    ),
                    "local_rotation": parse_inline_vector(
                        body,
                        "m_LocalRotation",
                        4,
                    ),
                    "local_scale": parse_inline_vector(
                        body,
                        "m_LocalScale",
                        3,
                    ),
                }
        elif item["class_id"] == 33:
            game_object_match = re.search(
                r"m_GameObject:\s*\{fileID:\s*(\d+)\}",
                body,
            )
            mesh_match = re.search(
                r"m_Mesh:\s*\{fileID:\s*(-?\d+),\s*"
                r"guid:\s*([0-9a-fA-F]+)",
                body,
            )
            if game_object_match and mesh_match:
                mesh_filters[game_object_match.group(1)] = {
                    "mesh_filter_file_id": item["file_id"],
                    "mesh_file_id": mesh_match.group(1),
                    "mesh_guid": mesh_match.group(2).lower(),
                }
        elif item["class_id"] == 23:
            game_object_match = re.search(
                r"m_GameObject:\s*\{fileID:\s*(\d+)\}",
                body,
            )
            material_match = re.search(
                r"m_Materials:\s*\n\s*-\s*\{fileID:\s*-?\d+,\s*"
                r"guid:\s*([0-9a-fA-F]+)",
                body,
            )
            enabled_match = re.search(
                r"(?m)^\s*m_Enabled:\s*(\d+)\s*$",
                body,
            )
            sorting_match = re.search(
                r"(?m)^\s*m_SortingOrder:\s*(-?\d+)\s*$",
                body,
            )
            if game_object_match and material_match:
                mesh_renderers[game_object_match.group(1)] = {
                    "mesh_renderer_file_id": item["file_id"],
                    "enabled": int(enabled_match.group(1))
                    if enabled_match
                    else 1,
                    "material_guid": material_match.group(1).lower(),
                    "source_sorting_order": int(
                        sorting_match.group(1)
                    )
                    if sorting_match
                    else 0,
                }

    fbx_meta_path = Path(str(source_fbx_path) + ".meta")
    fbx_meta_text = fbx_meta_path.read_text(
        encoding="utf-8",
        errors="strict",
    )
    guid_match = re.search(
        r"(?m)^guid:\s*([0-9a-fA-F]+)\s*$",
        fbx_meta_text,
    )
    if not guid_match:
        raise RuntimeError(f"Missing Unity GUID in {rel(fbx_meta_path)}")
    fbx_guid = guid_match.group(1).lower()
    inner_collision_node = RIVER_CORE_INNER_COLLISION_NODES[prefab_id]
    fbx_model_order, fbx_geometry_model_order = parse_fbx_binary_nodes(
        source_fbx_path
    )
    render_node_names = (prefab_id, f"{prefab_id}_water")
    missing_render_nodes = [
        node_name
        for node_name in render_node_names
        if node_name not in fbx_model_order
    ]
    if missing_render_nodes:
        raise RuntimeError(
            f"{rel(source_fbx_path)} lacks FBX render nodes "
            f"{missing_render_nodes}"
        )
    expected_fbx_nodes = list(fbx_model_order)
    collision_fbx_nodes = [
        node_name
        for node_name in expected_fbx_nodes
        if "_Collision" in node_name
    ]
    expected_collision_nodes = {
        inner_collision_node,
        *RIVER_EXTRA_COLLISION_NODES.get(prefab_id, ()),
    }
    if not expected_collision_nodes.issubset(set(collision_fbx_nodes)):
        raise RuntimeError(
            f"{rel(source_fbx_path)} lacks FBX collision nodes "
            f"{sorted(expected_collision_nodes - set(collision_fbx_nodes))}"
        )
    if sorted(fbx_geometry_model_order) != sorted(expected_fbx_nodes):
        raise RuntimeError(
            f"{rel(source_fbx_path)} FBX geometry/model sets differ: "
            f"{fbx_geometry_model_order} vs {expected_fbx_nodes}"
        )

    material_paths = {
        "edge": PROJECT_ROOT / RIVER_EDGE_MATERIAL_PATH,
        "water": PROJECT_ROOT / RIVER_WATER_MATERIAL_PATH,
    }
    material_contracts = {
        role: parse_river_material_contract(
            material_path,
            guid_index,
            role,
        )
        for role, material_path in material_paths.items()
    }
    render_layers = []
    for role, node_name, sort_priority in (
        ("edge", prefab_id, 0),
        ("water", f"{prefab_id}_water", 1),
    ):
        candidates = [
            game_object_id
            for game_object_id, game_object in game_objects.items()
            if game_object["name"] == node_name
            and game_object_id in transforms
            and game_object_id in mesh_filters
            and game_object_id in mesh_renderers
            and mesh_renderers[game_object_id]["enabled"] == 1
        ]
        if len(candidates) != 1:
            raise RuntimeError(
                f"{rel(prefab_path)} expected one enabled render node "
                f"{node_name}, found {len(candidates)}"
            )
        game_object_id = candidates[0]
        renderer = mesh_renderers[game_object_id]
        material = material_contracts[role]
        if mesh_filters[game_object_id]["mesh_guid"] != fbx_guid:
            raise RuntimeError(
                f"{rel(prefab_path)} {node_name} does not reference "
                "the audited FBX"
            )
        if renderer["material_guid"] != (
            "355a14738c7d4d445b4727529ceb6234"
            if role == "edge"
            else RIVER_WATER_MATERIAL_GUIDS.get(
                prefab_id, RIVER_DEFAULT_WATER_MATERIAL_GUID
            )
        ):
            raise RuntimeError(
                f"{rel(prefab_path)} {node_name} material GUID changed"
            )
        render_layers.append(
            {
                "role": role,
                "sort_priority": sort_priority,
                "game_object": node_name,
                "game_object_file_id": game_object_id,
                "game_object_layer": game_objects[game_object_id][
                    "layer"
                ],
                "game_object_active": game_objects[game_object_id][
                    "active"
                ],
                "transform": transforms[game_object_id],
                **mesh_filters[game_object_id],
                **renderer,
                **material,
            }
        )

    return {
        "policy": "split_fbx_exact_render_layers",
        "combine_meshes": False,
        "source_fbx_guid": fbx_guid,
        "source_fbx_node_count": len(expected_fbx_nodes),
        "fbx_mesh_nodes": expected_fbx_nodes,
        "excluded_fbx_nodes": collision_fbx_nodes,
        "source_fbx_geometry_model_order": fbx_geometry_model_order,
        "logical_instance_layer_count": 2,
        "render_layers": render_layers,
        "ue_import_contract": {
            "combine_meshes": False,
            "import_vertex_colors": "replace",
            "vertex_color_source_semantics": {
                "edge": "alpha contains transparent and opaque values",
                "water": "alpha is fully opaque before BaseColor.a",
            },
            "collision_nodes": (
                "excluded_from_render_and_collision_generation"
            ),
        },
        "water_dynamic_shader_contract": render_layers[1][
            "shader_contract"
        ],
        "acceptance_scope": (
            "static_source_contract_only_ue_import_map_runtime_pending"
        ),
    }


def parse_vector2(text):
    match = re.search(
        r"\{x:\s*([-+0-9.eE]+),\s*y:\s*([-+0-9.eE]+)\}",
        text,
    )
    if not match:
        return (0.0, 0.0)
    return (float(match.group(1)), float(match.group(2)))


def parse_texture_environment(material_text, property_name):
    lines = material_text.splitlines()
    needle = f"- {property_name}:"
    for index, line in enumerate(lines):
        if line.strip() != needle:
            continue
        block = "\n".join(lines[index : index + 6])
        guid_match = re.search(
            r"m_Texture:\s*\{[^}]*guid:\s*([0-9a-fA-F]+)",
            block,
        )
        scale_line = next(
            (item for item in lines[index : index + 6] if "m_Scale:" in item),
            "",
        )
        offset_line = next(
            (item for item in lines[index : index + 6] if "m_Offset:" in item),
            "",
        )
        return {
            "guid": guid_match.group(1).lower() if guid_match else "",
            "scale": parse_vector2(scale_line),
            "offset": parse_vector2(offset_line),
        }
    return {"guid": "", "scale": (1.0, 1.0), "offset": (0.0, 0.0)}


def parse_color(material_text, property_name="_Color"):
    match = re.search(
        rf"- {re.escape(property_name)}:\s*\{{r:\s*([-+0-9.eE]+),\s*"
        rf"g:\s*([-+0-9.eE]+),\s*b:\s*([-+0-9.eE]+),\s*"
        rf"a:\s*([-+0-9.eE]+)\}}",
        material_text,
    )
    if not match:
        return (1.0, 1.0, 1.0, 1.0)
    return tuple(float(match.group(index)) for index in range(1, 5))


def parse_shader_guid(material_text):
    match = re.search(
        r"m_Shader:\s*\{[^}]*guid:\s*([0-9a-fA-F]+)",
        material_text,
    )
    return match.group(1).lower() if match else ""


def source_record(path):
    return {
        "path": rel(path),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def parse_unity_curve(source_path, marker):
    text = source_path.read_text(encoding="utf-8", errors="strict")
    marker_line = f"  {marker}:"
    marker_positions = [
        match.start()
        for match in re.finditer(
            rf"^{re.escape(marker_line)}\s*$",
            text,
            re.M,
        )
    ]
    if len(marker_positions) != 1:
        raise RuntimeError(
            f"{rel(source_path)} expected one {marker}, found "
            f"{len(marker_positions)}"
        )
    section = text[marker_positions[0] :]
    end_match = re.search(r"^    m_PreInfinity:\s*", section, re.M)
    if not end_match:
        raise RuntimeError(
            f"{rel(source_path)} {marker} has no m_PreInfinity terminator"
        )
    section = section[: end_match.start()]
    key_blocks = re.split(
        r"^    - serializedVersion:\s*3\s*$",
        section,
        flags=re.M,
    )[1:]
    keys = []
    number_pattern = r"[-+0-9.eE]+|[-+]?Infinity"
    for block in key_blocks:
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
                raise RuntimeError(
                    f"{rel(source_path)} {marker} key lacks {field}"
                )
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
    if len(keys) < 2:
        raise RuntimeError(
            f"{rel(source_path)} {marker} has fewer than two keys"
        )
    if any(
        keys[index]["dxf"] >= keys[index + 1]["dxf"]
        for index in range(len(keys) - 1)
    ):
        raise RuntimeError(
            f"{rel(source_path)} {marker} keys are not strictly ordered"
        )
    return keys


def evaluate_unweighted_unity_curve(keys, dxf, role):
    if dxf <= keys[0]["dxf"]:
        return float(keys[0]["scale"])
    if dxf >= keys[-1]["dxf"]:
        return float(keys[-1]["scale"])
    for left, right in zip(keys, keys[1:]):
        if dxf > right["dxf"]:
            continue
        if dxf == left["dxf"]:
            return float(left["scale"])
        if dxf == right["dxf"]:
            return float(right["scale"])
        if (
            left["weighted_mode"] != 0
            or right["weighted_mode"] != 0
            or not math.isfinite(float(left["out_slope"]))
            or not math.isfinite(float(right["in_slope"]))
        ):
            raise RuntimeError(
                f"{role} cannot evaluate weighted or infinite-tangent "
                f"interval {left['dxf']}..{right['dxf']}"
            )
        duration = float(right["dxf"]) - float(left["dxf"])
        alpha = (float(dxf) - float(left["dxf"])) / duration
        alpha2 = alpha * alpha
        alpha3 = alpha2 * alpha
        h00 = 2.0 * alpha3 - 3.0 * alpha2 + 1.0
        h10 = alpha3 - 2.0 * alpha2 + alpha
        h01 = -2.0 * alpha3 + 3.0 * alpha2
        h11 = alpha3 - alpha2
        return (
            h00 * float(left["scale"])
            + h10 * duration * float(left["out_slope"])
            + h01 * float(right["scale"])
            + h11 * duration * float(right["in_slope"])
        )
    raise RuntimeError(f"{role} has no interval for dxf {dxf}")


def parse_city_age_sizes():
    with CITY_AGE_SIZE_PATH.open(
        "r",
        encoding="utf-8-sig",
        newline="",
    ) as handle:
        rows = []
        for row in csv.DictReader(handle):
            if (row.get("ID") or "").strip() == "int":
                continue
            rows.append(
                {
                    "id": int(row["ID"]),
                    "age": int(row["age"]),
                    "size": float(row["size"]),
                }
            )
    if not rows:
        raise RuntimeError(f"{rel(CITY_AGE_SIZE_PATH)} has no data rows")
    if len({row["id"] for row in rows}) != len(rows):
        raise RuntimeError(f"{rel(CITY_AGE_SIZE_PATH)} has duplicate ids")
    age_one = [row for row in rows if row["age"] == 1]
    if len(age_one) != 1:
        raise RuntimeError(
            f"{rel(CITY_AGE_SIZE_PATH)} expected one age-1 row"
        )
    maximum_age = max(row["age"] for row in rows)
    maximum_rows = [row for row in rows if row["age"] == maximum_age]
    if len(maximum_rows) != 1:
        raise RuntimeError(
            f"{rel(CITY_AGE_SIZE_PATH)} has ambiguous maximum age"
        )
    return rows, age_one[0]["size"], maximum_rows[0]["size"]


def derive_source_semantics(world_manifest):
    tactical_dxf = float(
        world_manifest["map_tactical_camera"]["dxf"]
    )
    city_curve = parse_unity_curve(
        CITY_CONTAINER_PREFAB_PATH,
        "m_scale_curve",
    )
    tree_curve_path = PROJECT_ROOT / TREE_SCALE_CURVE_SOURCE
    tree_curve = parse_unity_curve(
        tree_curve_path,
        "m_tree_scale_curve",
    )
    city_text = CITY_CONTAINER_PREFAB_PATH.read_text(
        encoding="utf-8",
        errors="strict",
    )
    maximum_matches = re.findall(
        r"^\s*m_max_scale:\s*([-+0-9.eE]+)\s*$",
        city_text,
        re.M,
    )
    if len(maximum_matches) != 1:
        raise RuntimeError(
            f"{rel(CITY_CONTAINER_PREFAB_PATH)} expected one m_max_scale"
        )
    base_max_scale = float(maximum_matches[0])
    age_rows, age_size, maximum_age_size = parse_city_age_sizes()
    city_curve_scale = evaluate_unweighted_unity_curve(
        city_curve,
        tactical_dxf,
        "CityBuildingContainer.m_scale_curve",
    )
    tree_scale = evaluate_unweighted_unity_curve(
        tree_curve,
        tactical_dxf,
        "GameApp.m_tree_scale_curve",
    )
    age_scale_cap = base_max_scale * age_size / maximum_age_size
    return {
        "map_tactical_dxf": tactical_dxf,
        "city": {
            "source": source_record(CITY_CONTAINER_PREFAB_PATH),
            "age_size_source": source_record(CITY_AGE_SIZE_PATH),
            "curve_keys": city_curve,
            "base_max_scale": base_max_scale,
            "age_sizes": age_rows,
            "age_size": age_size,
            "max_age_size": maximum_age_size,
            "age_scale_cap": age_scale_cap,
            "curve_scale_at_map_tactical": city_curve_scale,
            "initial_scale": min(
                max(city_curve_scale, 0.0),
                age_scale_cap,
            ),
        },
        "tree": {
            "source": source_record(tree_curve_path),
            "curve_keys": tree_curve,
            "scale_at_map_tactical": tree_scale,
            "runtime_visual_contract": (
                "exact_source_tree_shader_scale_baked_at_map_tactical_"
                "with_runtime_curve_ratio"
            ),
        },
    }


def resolve_unity_asset_path(asset_path):
    normalized = str(asset_path).replace("\\", "/")
    if not normalized.startswith("Assets/"):
        raise RuntimeError(f"Unexpected Unity asset path: {asset_path}")
    path = UNITY_ASSETS_ROOT / normalized[len("Assets/") :]
    if not path.exists():
        raise RuntimeError(f"Missing Unity player-city source: {path}")
    return path


def rotate_xy(vector, yaw_degrees):
    radians = math.radians(yaw_degrees)
    cosine = math.cos(radians)
    sine = math.sin(radians)
    return (
        vector[0] * cosine - vector[1] * sine,
        vector[0] * sine + vector[1] * cosine,
        vector[2],
    )


def unity_position_to_ue_centimeters(position):
    return (
        -float(position[0]) * UNITY_TO_UE_WORLD_SCALE,
        float(position[2]) * UNITY_TO_UE_WORLD_SCALE,
        float(position[1]) * UNITY_TO_UE_WORLD_SCALE,
    )


def build_player_city_record():
    if not CITY_RUNTIME_MANIFEST_PATH.exists():
        raise RuntimeError(
            "Missing Unity runtime city export manifest; regenerate "
            "unity_runtime_city_buildings_obj_manifest.json first"
        )
    runtime_manifest = json.loads(
        CITY_RUNTIME_MANIFEST_PATH.read_text(encoding="utf-8-sig")
    )
    if (
        runtime_manifest.get("schema")
        != "rok.unity.runtime_city_buildings_obj.v1"
    ):
        raise RuntimeError(
            "Unexpected runtime city export schema: "
            f"{runtime_manifest.get('schema')}"
        )

    candidates = [
        item
        for item in runtime_manifest.get("records", [])
        if item.get("init_id") == 5
        and item.get("building_type") == 1
        and item.get("level") == 1
        and item.get("model_id") == "build_1_101_1"
    ]
    if len(candidates) != 1:
        raise RuntimeError(
            "Expected exactly one Unity runtime TownCenter record for "
            "InitBuilding 5 / build_1_101_1"
        )
    source = candidates[0]
    if (
        runtime_manifest.get("container_yaw") != CITY_CONTAINER_YAW_DEGREES
        or runtime_manifest.get("runtime_lod_scale") != 1.121089
        or runtime_manifest.get("main_city_sprite_color")
        != [0.994559, 0.844559, 0.737059, 1.0]
    ):
        raise RuntimeError(
            "Unity runtime city export context no longer matches the audited "
            "player-city capture"
        )

    expected_identity = {
        "ue_type": "TownCenter",
        "visual_resolution": "exact",
        "renderer_type": "SpriteRenderer",
        "representation": "runtime sprite card",
        "shader_name": "custom/map/cm_city_mask_night",
        "shader_queue_tag": "Transparent-5",
        "shader_zwrite": "Off",
        "material_render_queue": 2996,
        "grid_x": -5,
        "grid_y": 5,
        "grid_width": 9,
        "grid_length": 9,
    }
    for field, expected in expected_identity.items():
        if source.get(field) != expected:
            raise RuntimeError(
                f"Unity player-city {field} is {source.get(field)!r}; "
                f"expected {expected!r}"
            )

    baked_texture_path = Path(source.get("baked_texture_path", "")).resolve()
    if baked_texture_path != CITY_BAKED_TEXTURE_PATH.resolve():
        raise RuntimeError(
            "Unity player-city export points at an unexpected baked texture: "
            f"{baked_texture_path}"
        )
    if not baked_texture_path.exists():
        raise RuntimeError(
            f"Missing Unity player-city baked texture: {baked_texture_path}"
        )
    if sha256(baked_texture_path) != CITY_EXPECTED_BAKED_SHA256:
        raise RuntimeError(
            "Unity player-city baked texture does not match the audited "
            "build_1_101_1 export"
        )
    fallback_obj_path = Path(source.get("obj_path", "")).resolve()
    if not fallback_obj_path.exists():
        raise RuntimeError(
            f"Missing Unity player-city fallback OBJ: {fallback_obj_path}"
        )

    with Image.open(baked_texture_path) as image:
        rgba = np.asarray(image.convert("RGBA"), dtype=np.uint8)
        image_size = [int(image.width), int(image.height)]
    alpha = rgba[:, :, 3]
    alpha_stats = {
        "transparent_pixel_count": int(np.count_nonzero(alpha == 0)),
        "opaque_pixel_count": int(np.count_nonzero(alpha == 255)),
        "partial_alpha_pixel_count": int(
            np.count_nonzero((alpha > 0) & (alpha < 255))
        ),
        "unique_alpha_value_count": int(np.unique(alpha).size),
    }
    if image_size != [1024, 888]:
        raise RuntimeError(
            f"Unity player-city baked texture is {image_size}, expected 1024x888"
        )
    if alpha_stats != {
        "transparent_pixel_count": 328428,
        "opaque_pixel_count": 482833,
        "partial_alpha_pixel_count": 98051,
        "unique_alpha_value_count": 256,
    }:
        raise RuntimeError(
            "Unity player-city baked texture lost its audited alpha coverage"
        )

    sprite_rect = source.get("sprite_texture_rect", {})
    renderer_scale = source.get("renderer_local_scale", [])
    if (
        sprite_rect.get("width") != 1024.0
        or sprite_rect.get("height") != 888.0
        or source.get("pixels_per_unit") != 100.0
        or renderer_scale != [0.124825, 0.124825, 0.124825]
    ):
        raise RuntimeError(
            "Unity player-city sprite geometry no longer matches the audited "
            "runtime export"
        )

    base_width_cm = (
        sprite_rect["width"]
        / source["pixels_per_unit"]
        * renderer_scale[0]
        * UNITY_TO_UE_WORLD_SCALE
    )
    base_height_cm = (
        sprite_rect["height"]
        / source["pixels_per_unit"]
        * renderer_scale[1]
        * UNITY_TO_UE_WORLD_SCALE
    )
    selection_anchor_offset = rotate_xy(
        unity_position_to_ue_centimeters(source["root_local_position"]),
        CITY_CONTAINER_YAW_DEGREES,
    )
    base_visual_center_offset = rotate_xy(
        unity_position_to_ue_centimeters(source["renderer_local_position"]),
        CITY_CONTAINER_YAW_DEGREES,
    )
    initial_lod_scale = min(
        max(CITY_INITIAL_CURVE_SCALE, 0.0),
        CITY_AGE_LOD_SCALE_CAP,
    )
    initial_visual_center = [
        selection_anchor_offset[index]
        + base_visual_center_offset[index] * initial_lod_scale
        for index in range(3)
    ]
    diagonal = math.sqrt(0.5)
    initial_visual_bounds_extent = [
        base_width_cm * initial_lod_scale * 0.5,
        base_height_cm * initial_lod_scale * 0.5 * diagonal,
        base_height_cm * initial_lod_scale * 0.5 * diagonal,
    ]

    return {
        "identity": {
            "init_id": source["init_id"],
            "building_type": source["building_type"],
            "ue_type": source["ue_type"],
            "level": source["level"],
            "grid_x": source["grid_x"],
            "grid_y": source["grid_y"],
            "grid_width": source["grid_width"],
            "grid_length": source["grid_length"],
            "model_id": source["model_id"],
            "visual_sprite_name": source["visual_sprite_name"],
            "visual_resolution": source["visual_resolution"],
            "representation": source["representation"],
        },
        "source_runtime_manifest": source_record(
            CITY_RUNTIME_MANIFEST_PATH
        ),
        "source_assets": {
            "sprite": {
                **source_record(
                    resolve_unity_asset_path(source["sprite_path"])
                ),
                "guid": source["sprite_guid"],
            },
            "mask": {
                **source_record(
                    resolve_unity_asset_path(source["mask_path"])
                ),
                "guid": source["mask_guid"],
            },
            "building_prefab": source_record(
                resolve_unity_asset_path(source["prefab_path"])
            ),
            "material": {
                **source_record(
                    resolve_unity_asset_path(source["material_path"])
                ),
                "guid": source["material_guid"],
            },
            "shader": source_record(
                resolve_unity_asset_path(source["shader_asset_path"])
            ),
            "city_container_prefab": source_record(
                CITY_CONTAINER_PREFAB_PATH
            ),
            "lod_implementation": source_record(CITY_LOD_CODE_PATH),
            "world_integration": source_record(
                CITY_WORLD_INTEGRATION_PATH
            ),
            "age_size_table": source_record(CITY_AGE_SIZE_PATH),
            "building_model_table": source_record(
                CITY_BUILDING_MODEL_PATH
            ),
            "runtime_exporter": source_record(CITY_EXPORTER_PATH),
        },
        "runtime_export_context": {
            "container_yaw": runtime_manifest["container_yaw"],
            "captured_runtime_lod_scale": runtime_manifest[
                "runtime_lod_scale"
            ],
            "main_city_sprite_color": runtime_manifest[
                "main_city_sprite_color"
            ],
            "note": (
                "captured_runtime_lod_scale is exporter evidence only; the UE "
                "world-map actor evaluates the source LevelDetailCastle curve "
                "from its own camera dxf"
            ),
        },
        "baked_texture": {
            **source_record(baked_texture_path),
            "image_size": image_size,
            "alpha_stats": alpha_stats,
        },
        "fallback_obj": {
            **source_record(fallback_obj_path),
            "vertex_count": source["sprite_vertex_count"],
            "triangle_count": source["sprite_triangle_count"],
            "usage": "evidence_only_not_default_world_city_visual",
        },
        "source_renderer": {
            "name": source["renderer_name"],
            "path": source["renderer_path"],
            "type": source["renderer_type"],
            "root_local_position": source["root_local_position"],
            "root_local_scale": source["root_local_scale"],
            "renderer_local_position": source["renderer_local_position"],
            "renderer_local_euler": source["renderer_local_euler"],
            "renderer_local_scale": source["renderer_local_scale"],
            "sprite_texture_rect": source["sprite_texture_rect"],
            "pixels_per_unit": source["pixels_per_unit"],
            "unity_bounds_center": source["unity_bounds_center"],
            "unity_bounds_extents": source["unity_bounds_extents"],
        },
        "shader_contract": {
            "shader_name": source["shader_name"],
            "queue_tag": source["shader_queue_tag"],
            "zwrite": source["shader_zwrite"],
            "material_render_queue": source["material_render_queue"],
            "ue_blend_mode": "translucent",
            "ue_shading_model": "unlit",
        },
        "lod_contract": {
            "initial_dxf": CITY_INITIAL_DXF,
            "curve_keys": [
                {
                    "dxf": float(key["dxf"]),
                    "scale": float(key["scale"]),
                }
                for key in CITY_SCALE_CURVE_KEYS
            ],
            "source_curve_keys": CITY_SCALE_CURVE_KEYS,
            "curve_scale_at_initial_dxf": CITY_INITIAL_CURVE_SCALE,
            "base_max_scale": CITY_BASE_MAX_LOD_SCALE,
            "age_size": CITY_AGE_SIZE,
            "max_age_size": CITY_MAX_AGE_SIZE,
            "age_scale_cap": CITY_AGE_LOD_SCALE_CAP,
            "initial_scale": initial_lod_scale,
            "formula": (
                "min(max(CityBuildingContainer.m_scale_curve(dxf), 0), "
                "7.8 * CityAgeSize(age1=41) / CityAgeSize(max=57))"
            ),
        },
        "ue_presentation": {
            "logical_world_root_cm": [0.0, 0.0, 0.0],
            "selection_anchor_offset_cm": list(selection_anchor_offset),
            "base_visual_center_offset_cm": list(
                base_visual_center_offset
            ),
            "base_card_size_cm": [base_width_cm, base_height_cm],
            "source_plane_size_cm": CITY_SOURCE_PLANE_SIZE_CM,
            "base_plane_scale": [
                base_width_cm / CITY_SOURCE_PLANE_SIZE_CM,
                -base_height_cm / CITY_SOURCE_PLANE_SIZE_CM,
                1.0,
            ],
            "visual_basis": {
                "u": [-1.0, 0.0, 0.0],
                "v": [0.0, diagonal, diagonal],
                "normal": [0.0, diagonal, -diagonal],
            },
            "initial_visual_center_cm": initial_visual_center,
            "initial_visual_bounds_extent_cm": (
                initial_visual_bounds_extent
            ),
            "derivation": (
                "Unity (x,y,z) -> UE (-x,z,y) at 500 cm/unit, then "
                "CityBuildingContainer yaw -45; renderer offset and card "
                "size scale with LevelDetailCastle while the logical root and "
                "selection anchor remain stable"
            ),
        },
    }


def load_rgb(path):
    with Image.open(path) as image:
        return np.asarray(image.convert("RGB"), dtype=np.float32) / 255.0


def bilinear_sample(image, sample_u, sample_v, wrap):
    height, width, _ = image.shape
    if wrap:
        u = np.mod(sample_u, 1.0)
        v = np.mod(sample_v, 1.0)
    else:
        u = np.clip(sample_u, 0.0, 1.0)
        v = np.clip(sample_v, 0.0, 1.0)

    x = u * float(width - 1)
    # Image rows run top-to-bottom while Unity texture V runs bottom-to-top.
    y = (1.0 - v) * float(height - 1)
    x0 = np.floor(x).astype(np.int32)
    y0 = np.floor(y).astype(np.int32)
    if wrap:
        x1 = (x0 + 1) % width
        y1 = (y0 + 1) % height
    else:
        x1 = np.minimum(x0 + 1, width - 1)
        y1 = np.minimum(y0 + 1, height - 1)
    dx = (x - x0)[..., None]
    dy = (y - y0)[..., None]
    return (
        image[y0, x0] * (1.0 - dx) * (1.0 - dy)
        + image[y0, x1] * dx * (1.0 - dy)
        + image[y1, x0] * (1.0 - dx) * dy
        + image[y1, x1] * dx * dy
    )


def resolve_environment(environment, guid_index, role):
    guid = environment["guid"]
    path = guid_index.get(guid)
    if not path or not path.exists():
        raise RuntimeError(f"Could not resolve Unity material {role} GUID {guid}")
    return path


def bake_splat_material(prefab_id, material_path, guid_index):
    material_text = material_path.read_text(encoding="utf-8", errors="ignore")
    shader_guid = parse_shader_guid(material_text)
    shader_path = guid_index.get(shader_guid)
    if not shader_path or shader_path.name != "cm_mask_texture_lod.shader":
        raise RuntimeError(
            f"{rel(material_path)} does not use cm_mask_texture_lod.shader"
        )

    splat_env = parse_texture_environment(material_text, "_Splat")
    layer_envs = [
        parse_texture_environment(material_text, f"_Tex{index}")
        for index in range(4)
    ]
    splat_path = resolve_environment(splat_env, guid_index, "_Splat")
    layer_paths = [
        resolve_environment(environment, guid_index, f"_Tex{index}")
        for index, environment in enumerate(layer_envs)
    ]

    output_axis = (
        np.arange(BAKE_SIZE, dtype=np.float32) + 0.5
    ) / float(BAKE_SIZE)
    mesh_u, image_y = np.meshgrid(output_axis, output_axis)
    mesh_v = 1.0 - image_y

    shader_u = 1.0 - mesh_u
    shader_v = 1.0 - mesh_v
    splat = bilinear_sample(
        load_rgb(splat_path),
        shader_u,
        shader_v,
        wrap=False,
    )
    weights = [
        splat[..., 0:1],
        splat[..., 1:2],
        splat[..., 2:3],
        1.0
        - splat[..., 0:1]
        - splat[..., 1:2]
        - splat[..., 2:3],
    ]
    output = np.zeros((BAKE_SIZE, BAKE_SIZE, 3), dtype=np.float32)
    for index, (environment, texture_path) in enumerate(
        zip(layer_envs, layer_paths)
    ):
        scale_x, scale_y = environment["scale"]
        offset_x, offset_y = environment["offset"]
        layer_u = (shader_u * scale_x + offset_x) * 60.0
        layer_v = (shader_v * scale_y + offset_y) * 60.0
        sampled = bilinear_sample(
            load_rgb(texture_path),
            layer_u,
            layer_v,
            wrap=True,
        )
        output += weights[index] * sampled

    color = parse_color(material_text)
    output *= np.asarray(color[:3], dtype=np.float32)
    output = np.clip(output, 0.0, 1.0)
    output_path = TEXTURE_OUTPUT_ROOT / f"{prefab_id}.png"
    Image.fromarray(
        np.rint(output * 255.0).astype(np.uint8),
        mode="RGB",
    ).save(output_path)
    return {
        "prefab_id": prefab_id,
        "source_material": source_record(material_path),
        "source_shader": source_record(shader_path),
        "splat": source_record(splat_path),
        "layers": [
            {
                "texture": source_record(path),
                "scale": {
                    "x": environment["scale"][0],
                    "y": environment["scale"][1],
                },
                "offset": {
                    "x": environment["offset"][0],
                    "y": environment["offset"][1],
                },
            }
            for path, environment in zip(layer_paths, layer_envs)
        ],
        "color": {
            "r": color[0],
            "g": color[1],
            "b": color[2],
            "a": color[3],
        },
        "output": source_record(output_path),
        "bake_size": [BAKE_SIZE, BAKE_SIZE],
        "shader_rule": (
            "cm_mask_texture_lod: RGB splat weights plus "
            "(1-R-G-B), 1-UV, per-layer ST multiplied by 60"
        ),
        "render_contract": {
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
        },
    }


def material_texture_record(material_relative_path, guid_index):
    material_path = PROJECT_ROOT / material_relative_path
    if not material_path.exists():
        raise RuntimeError(f"Missing source material: {material_relative_path}")
    material_text = material_path.read_text(encoding="utf-8", errors="ignore")
    main_env = parse_texture_environment(material_text, "_MainTex")
    main_path = resolve_environment(main_env, guid_index, "_MainTex")
    shader_guid = parse_shader_guid(material_text)
    shader_path = guid_index.get(shader_guid)
    return {
        "source_material": source_record(material_path),
        "source_texture": source_record(main_path),
        "source_shader": source_record(shader_path)
        if shader_path and shader_path.exists()
        else None,
        "color": {
            name: value
            for name, value in zip(
                ("r", "g", "b", "a"),
                parse_color(material_text),
            )
        },
    }


VERTEX_FORMAT_SIZE = {
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


def analyze_tree_map_tactical_projection(positions, centers, triangles):
    diagonal = math.sqrt(0.5)

    def imported_world_position(position):
        # UE OBJ import: (X,-Y,Z), then source actor scale (-5,+5,+5).
        return np.asarray(
            (
                -5.0 * position[0],
                -5.0 * position[1],
                5.0 * position[2],
            ),
            dtype=np.float64,
        )

    grouped_positions = {}
    for position, center in zip(positions, centers):
        grouped_positions.setdefault(center, []).append(
            imported_world_position(position)
        )
    screen_up = np.asarray(
        (0.0, diagonal, diagonal),
        dtype=np.float64,
    )
    card_heights = []
    for group in grouped_positions.values():
        if len(group) < 3:
            continue
        projections = [float(np.dot(item, screen_up)) for item in group]
        card_heights.append(max(projections) - min(projections))
    if not card_heights:
        raise RuntimeError("Tree mesh has no COLOR-center billboard groups")

    tactical_forward = np.asarray(
        (0.0, diagonal, -diagonal),
        dtype=np.float64,
    )
    face_alignment = []
    for _, a, b, c in triangles:
        point_a = imported_world_position(positions[a])
        point_b = imported_world_position(positions[b])
        point_c = imported_world_position(positions[c])
        normal = np.cross(point_b - point_a, point_c - point_a)
        length = float(np.linalg.norm(normal))
        if length <= 1.0e-8:
            continue
        face_alignment.append(
            abs(float(np.dot(normal / length, tactical_forward)))
        )
    if not face_alignment:
        raise RuntimeError("Tree mesh has no non-degenerate billboard faces")

    return {
        "camera_forward_ue": [0.0, diagonal, -diagonal],
        "camera_up_ue": [0.0, diagonal, diagonal],
        "card_count": len(card_heights),
        "minimum_card_screen_height_cm": min(card_heights),
        "median_card_screen_height_cm": float(np.median(card_heights)),
        "maximum_card_screen_height_cm": max(card_heights),
        "card_count_below_100_cm": sum(
            1 for height in card_heights if height < 100.0
        ),
        "minimum_abs_face_normal_dot_camera_forward": min(face_alignment),
        "formula": (
            "simulate UE OBJ (X,-Y,Z), actor scale (-5,+5,+5), "
            "then project each COLOR-center card onto tactical camera-up"
        ),
    }


def parse_unity_mesh_asset(mesh_path, reconstruct_tree_shader=False):
    text = mesh_path.read_text(encoding="utf-8", errors="ignore")
    vertex_count_match = re.search(r"^\s*m_VertexCount:\s*(\d+)\s*$", text, re.M)
    index_format_match = re.search(r"^\s*m_IndexFormat:\s*(\d+)\s*$", text, re.M)
    index_buffer_match = re.search(
        r"^\s*m_IndexBuffer:\s*([0-9a-fA-F]+)\s*$",
        text,
        re.M,
    )
    vertex_data_match = re.search(
        r"^\s*_typelessdata:\s*([0-9a-fA-F]+)\s*$",
        text,
        re.M,
    )
    data_size_match = re.search(r"^\s*m_DataSize:\s*(\d+)\s*$", text, re.M)
    if not all(
        (
            vertex_count_match,
            index_format_match,
            index_buffer_match,
            vertex_data_match,
            data_size_match,
        )
    ):
        raise RuntimeError(f"Unsupported or incomplete Unity mesh: {rel(mesh_path)}")

    vertex_count = int(vertex_count_match.group(1))
    index_format = int(index_format_match.group(1))
    index_buffer = bytes.fromhex(index_buffer_match.group(1))
    vertex_data = bytes.fromhex(vertex_data_match.group(1))
    expected_data_size = int(data_size_match.group(1))
    if len(vertex_data) != expected_data_size:
        raise RuntimeError(
            f"{rel(mesh_path)} vertex-data size mismatch: "
            f"{len(vertex_data)} != {expected_data_size}"
        )

    channel_start = text.find("    m_Channels:")
    channel_end = text.find("    m_DataSize:", channel_start)
    if channel_start < 0 or channel_end < 0:
        raise RuntimeError(f"{rel(mesh_path)} has no readable channel table")
    channel_text = text[channel_start:channel_end]
    channel_matches = re.findall(
        r"- stream:\s*(\d+)\s*\n"
        r"\s*offset:\s*(\d+)\s*\n"
        r"\s*format:\s*(\d+)\s*\n"
        r"\s*dimension:\s*(\d+)",
        channel_text,
    )
    channels = [
        {
            "stream": int(stream),
            "offset": int(offset),
            "format": int(data_format),
            "dimension": int(dimension),
        }
        for stream, offset, data_format, dimension in channel_matches
    ]
    if len(channels) < 5:
        raise RuntimeError(f"{rel(mesh_path)} has an incomplete channel table")
    if any(
        channel["stream"] != 0
        for channel in channels
        if channel["dimension"] > 0
    ):
        raise RuntimeError(
            f"{rel(mesh_path)} uses unsupported multiple vertex streams"
        )
    active_channels = [
        channel for channel in channels if channel["dimension"] > 0
    ]
    stride = max(
        channel["offset"]
        + VERTEX_FORMAT_SIZE[channel["format"]] * channel["dimension"]
        for channel in active_channels
    )
    if stride * vertex_count != len(vertex_data):
        raise RuntimeError(
            f"{rel(mesh_path)} vertex stride mismatch: "
            f"{stride}*{vertex_count} != {len(vertex_data)}"
        )
    position_channel = channels[0]
    color_channel = channels[3]
    uv_channel = channels[4]
    if (
        position_channel["format"] != 0
        or position_channel["dimension"] < 3
        or uv_channel["format"] != 0
        or uv_channel["dimension"] < 2
    ):
        raise RuntimeError(
            f"{rel(mesh_path)} uses unsupported position or UV encoding"
        )
    if reconstruct_tree_shader and (
        color_channel["format"] != 0
        or color_channel["dimension"] < 3
    ):
        raise RuntimeError(
            f"{rel(mesh_path)} uses unsupported tree-center vertex encoding"
        )

    positions = []
    centers = []
    uvs = []
    for vertex_index in range(vertex_count):
        base = vertex_index * stride
        px, py, pz = struct.unpack_from(
            "<fff",
            vertex_data,
            base + position_channel["offset"],
        )
        if reconstruct_tree_shader:
            center_r, center_g, center_b = struct.unpack_from(
                "<fff",
                vertex_data,
                base + color_channel["offset"],
            )
            center_x = center_r * TREE_VERTEX_CENTER_SCALE
            center_y = center_g * TREE_VERTEX_CENTER_SCALE
            center_z = center_b * TREE_VERTEX_CENTER_SCALE
            # customlandmaptree-16-nomask.shader expands each atlas sprite
            # around the center stored in COLOR. Unity overrides _TreeScale
            # globally from GameApp's dxf curve; bake its map_tactical value
            # and _TreeRot=0 into the deterministic phase-1 OBJ.
            px = center_x + (px - center_x) * TREE_VERTEX_SCALE
            py = center_y + (py - center_y) * TREE_VERTEX_SCALE
            pz = center_z + (pz - center_z) * TREE_VERTEX_SCALE
            centers.append((center_x, center_y, center_z))
        u, v = struct.unpack_from(
            "<ff",
            vertex_data,
            base + uv_channel["offset"],
        )
        # UE 5.8's Interchange OBJ translator applies
        # PositionToUEBasis(X,-Y,Z). Pre-negate OBJ Y so the imported mesh is
        # (Unity x,z,y)*100 rather than the camera-edge-on (x,-z,y)*100.
        # Actor scale then applies the project Unity-X mirror plus the
        # remaining 5x world scale.
        positions.append((px * 100.0, -pz * 100.0, py * 100.0))
        uvs.append((u, v))

    submesh_text_start = text.find("  m_SubMeshes:")
    submesh_text_end = text.find("  m_Shapes:", submesh_text_start)
    submesh_text = text[submesh_text_start:submesh_text_end]
    submeshes = []
    for block in re.split(r"\n\s*-\s+serializedVersion:\s*\d+", submesh_text)[1:]:
        first_byte = re.search(r"^\s*firstByte:\s*(\d+)\s*$", block, re.M)
        index_count = re.search(r"^\s*indexCount:\s*(\d+)\s*$", block, re.M)
        topology = re.search(r"^\s*topology:\s*(\d+)\s*$", block, re.M)
        base_vertex = re.search(r"^\s*baseVertex:\s*(-?\d+)\s*$", block, re.M)
        if not all((first_byte, index_count, topology, base_vertex)):
            continue
        if int(topology.group(1)) != 0:
            raise RuntimeError(
                f"{rel(mesh_path)} has unsupported non-triangle topology"
            )
        submeshes.append(
            {
                "first_byte": int(first_byte.group(1)),
                "index_count": int(index_count.group(1)),
                "base_vertex": int(base_vertex.group(1)),
            }
        )
    if not submeshes:
        raise RuntimeError(f"{rel(mesh_path)} has no readable triangle submesh")

    index_size = 2 if index_format == 0 else 4
    index_code = "<H" if index_format == 0 else "<I"
    triangles = []
    for submesh_index, submesh in enumerate(submeshes):
        if submesh["index_count"] % 3:
            raise RuntimeError(
                f"{rel(mesh_path)} submesh index count is not divisible by 3"
            )
        indices = [
            struct.unpack_from(
                index_code,
                index_buffer,
                submesh["first_byte"] + index * index_size,
            )[0]
            + submesh["base_vertex"]
            for index in range(submesh["index_count"])
        ]
        for index in range(0, len(indices), 3):
            # Swapping Unity Y/Z to UE Y/Z changes handedness; reverse winding
            # to retain front faces.
            triangles.append(
                (
                    submesh_index,
                    indices[index],
                    indices[index + 2],
                    indices[index + 1],
                )
            )
    if any(
        vertex_index < 0 or vertex_index >= vertex_count
        for triangle in triangles
        for vertex_index in triangle[1:]
    ):
        raise RuntimeError(f"{rel(mesh_path)} has out-of-range mesh indices")
    result = {
        "positions": positions,
        "uvs": uvs,
        "triangles": triangles,
        "submesh_count": len(submeshes),
    }
    if reconstruct_tree_shader:
        result["map_tactical_projection"] = (
            analyze_tree_map_tactical_projection(
                positions,
                centers,
                triangles,
            )
        )
    return result


def export_unity_mesh_to_obj(
    prefab_id,
    mesh_path,
    reconstruct_tree_shader=False,
):
    parsed = parse_unity_mesh_asset(
        mesh_path,
        reconstruct_tree_shader=reconstruct_tree_shader,
    )
    MESH_OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    output_path = MESH_OUTPUT_ROOT / f"{prefab_id}.obj"
    lines = [
        f"# Unity source: {rel(mesh_path)}",
        f"o {prefab_id}",
    ]
    for x, y, z in parsed["positions"]:
        lines.append(f"v {x:.9g} {y:.9g} {z:.9g}")
    for u, v in parsed["uvs"]:
        lines.append(f"vt {u:.9g} {v:.9g}")
    current_submesh = None
    for submesh_index, a, b, c in parsed["triangles"]:
        if submesh_index != current_submesh:
            lines.append(f"g submesh_{submesh_index}")
            current_submesh = submesh_index
        # OBJ is one-based and position/UV indices share the same source index.
        a += 1
        b += 1
        c += 1
        lines.append(f"f {a}/{a} {b}/{b} {c}/{c}")
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return {
        "source_mesh": source_record(mesh_path),
        "exported_obj": source_record(output_path),
        "vertex_count": len(parsed["positions"]),
        "triangle_count": len(parsed["triangles"]),
        "submesh_count": parsed["submesh_count"],
        "axis_conversion": (
            "(Unity x,y,z) -> OBJ (x,-z,y) * 100 cm -> UE 5.8 "
            "Interchange imported mesh (x,z,y) * 100 cm"
        ),
        "obj_import_compensation": {
            "ue_translator": "Interchange OBJ PositionToUEBasis(X,-Y,Z)",
            "obj_payload": "(Unity x,-z,y) * 100 cm",
            "imported_mesh": "(Unity x,z,y) * 100 cm",
            "actor_scale_contract": "(-5,+5,+5)",
            "final_world_vector": "500 * (-Unity x,+Unity z,+Unity y)",
        },
        "map_tactical_projection": parsed.get(
            "map_tactical_projection"
        ),
        "vertex_shader_reconstruction": (
            {
                "shader": "customlandmaptree-16-nomask.shader",
                "center_semantic": "COLOR.xyz * 180",
                "tree_scale": TREE_VERTEX_SCALE,
                "bake_dxf": TREE_SCALE_BAKE_DXF,
                "curve_source": TREE_SCALE_CURVE_SOURCE,
                "tree_rotation_radians": TREE_VERTEX_ROTATION_RADIANS,
                "formula": "center + (position - center) * tree_scale",
            }
            if reconstruct_tree_shader
            else None
        ),
    }


def quaternion_to_matrix(rotation):
    x, y, z, w = (float(v) for v in rotation)
    length = (x * x + y * y + z * z + w * w) ** 0.5
    x, y, z, w = x / length, y / length, z / length, w / length
    return [
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ]


def trs_matrix(position, rotation, scale):
    rot = quaternion_to_matrix(rotation)
    matrix = [
        [rot[r][c] * float(scale[c]) for c in range(3)]
        for r in range(3)
    ]
    for r in range(3):
        matrix[r].append(float(position[r]))
    matrix.append([0.0, 0.0, 0.0, 1.0])
    return matrix


def matrix_multiply_4(left, right):
    return [
        [
            sum(left[r][k] * right[k][c] for k in range(4))
            for c in range(4)
        ]
        for r in range(4)
    ]


def parse_waterfall_geometry_contract(prefab_path, guid_index):
    objects = parse_unity_yaml_objects(prefab_path)
    game_objects = {}
    transforms = {}
    go_transform = {}
    mesh_filters = {}
    renderers = {}
    particle_marks = set()
    particle_renderers = {}
    for item in objects:
        class_id = item["class_id"]
        body = item["body"]
        file_id = item["file_id"]
        if class_id == 1:
            name_match = re.search(r"(?m)^\s*m_Name:\s*(.+)$", body)
            active_match = re.search(r"(?m)^\s*m_IsActive:\s*(\d)", body)
            game_objects[file_id] = {
                "name": name_match.group(1).strip(),
                "active": int(active_match.group(1)) if active_match else 1,
            }
        elif class_id == 4:
            go_match = re.search(
                r"m_GameObject:\s*\{fileID:\s*(\d+)\}", body
            )
            if not go_match:
                continue
            def vec(field, names):
                match = re.search(
                    field
                    + r":\s*\{"
                    + r",\s*".join(
                        name + r":\s*([-+0-9.eE]+)" for name in names
                    )
                    + r"\}",
                    body,
                )
                if not match:
                    raise RuntimeError(
                        f"{rel(prefab_path)} transform lacks {field}"
                    )
                return [float(v) for v in match.groups()]
            father_match = re.search(
                r"m_Father:\s*\{fileID:\s*(\d+)\}", body
            )
            transforms[file_id] = {
                "game_object": go_match.group(1),
                "position": vec("m_LocalPosition", ("x", "y", "z")),
                "rotation": vec("m_LocalRotation", ("x", "y", "z", "w")),
                "scale": vec("m_LocalScale", ("x", "y", "z")),
                "father": father_match.group(1) if father_match else "0",
            }
            go_transform[go_match.group(1)] = file_id
        elif class_id == 33:
            go_match = re.search(
                r"m_GameObject:\s*\{fileID:\s*(\d+)\}", body
            )
            mesh_match = re.search(
                r"m_Mesh:\s*\{fileID:\s*(-?\d+)(?:,\s*guid:\s*([0-9a-fA-F]+))?",
                body,
            )
            if go_match and mesh_match:
                mesh_filters[go_match.group(1)] = {
                    "mesh_file_id": mesh_match.group(1),
                    "mesh_guid": (mesh_match.group(2) or "").lower(),
                }
        elif class_id == 23:
            go_match = re.search(
                r"m_GameObject:\s*\{fileID:\s*(\d+)\}", body
            )
            materials = re.findall(
                r"guid:\s*([0-9a-fA-F]+)",
                body.split("m_Materials:")[1].split("m_StaticBatchInfo")[0],
            ) if "m_Materials:" in body else []
            enabled_match = re.search(r"(?m)^\s*m_Enabled:\s*(\d)", body)
            if go_match:
                renderers[go_match.group(1)] = {
                    "material_guids": [g.lower() for g in materials],
                    "enabled": int(enabled_match.group(1)) if enabled_match else 1,
                }
        elif class_id == 198:
            go_match = re.search(
                r"m_GameObject:\s*\{fileID:\s*(\d+)\}", body
            )
            if go_match:
                particle_marks.add(go_match.group(1))
        elif class_id == 199:
            go_match = re.search(
                r"m_GameObject:\s*\{fileID:\s*(\d+)\}", body
            )
            materials = re.findall(
                r"guid:\s*([0-9a-fA-F]+)",
                body.split("m_Materials:")[1].split("m_StaticBatchInfo")[0],
            ) if "m_Materials:" in body else []
            mesh_match = re.search(
                r"m_Mesh:\s*\{fileID:\s*(-?\d+)(?:,\s*guid:\s*([0-9a-fA-F]+))?",
                body,
            )
            if go_match:
                particle_renderers[go_match.group(1)] = {
                    "material_guids": [g.lower() for g in materials],
                    "mesh_file_id": mesh_match.group(1) if mesh_match else "",
                    "mesh_guid": (mesh_match.group(2) or "").lower()
                    if mesh_match
                    else "",
                }

    def local_matrix(go_file_id):
        transform = transforms[go_transform[go_file_id]]
        return trs_matrix(
            transform["position"],
            transform["rotation"],
            transform["scale"],
        )

    def prefab_matrix(go_file_id):
        chain = []
        transform_id = go_transform[go_file_id]
        while transform_id and transform_id != "0":
            transform = transforms[transform_id]
            chain.append(transform_id)
            transform_id = transform["father"]
        matrix = None
        for link in reversed(chain):
            go_id = transforms[link]["game_object"]
            link_matrix = local_matrix(go_id)
            matrix = (
                link_matrix
                if matrix is None
                else matrix_multiply_4(matrix, link_matrix)
            )
        return matrix

    land_root = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/land"
    river_f_prefab = next(land_root.rglob("Rive_CO_F.prefab"))
    mountain_g_prefab = next(land_root.rglob("build_mountain_G.prefab"))
    river_f_contract = parse_river_core_geometry_contract(
        "Rive_CO_F",
        river_f_prefab,
        PROJECT_ROOT / RIVER_CORE_FBX_PATHS["Rive_CO_F"],
        guid_index,
    )
    mountain_g_contract = parse_mountain_geometry_contract(
        "build_mountain_G",
        mountain_g_prefab,
        PROJECT_ROOT / MOUNTAIN_FBX_PATHS["build_mountain_G"],
        guid_index,
    )
    mesh_node_by_file_id = {
        layer["mesh_file_id"]: layer["game_object"]
        for layer in river_f_contract["render_layers"]
    }
    for role in ("body_render_node", "edge_render_node"):
        node = mountain_g_contract[role]
        mesh_node_by_file_id[node["mesh_file_id"]] = node["game_object"]
    mountain_g_fbx_models, _ = parse_fbx_binary_nodes(
        PROJECT_ROOT / MOUNTAIN_FBX_PATHS["build_mountain_G"]
    )
    water_node = "build_mountain_G_water"
    if water_node not in mountain_g_fbx_models:
        raise RuntimeError("build_mountain_G.FBX lacks the water node")

    ue_mesh_by_node = {
        "Rive_CO_F": (
            "/Game/RokUnityWorldMapPrototype/Meshes/RiverLayers/"
            "SM_RokWorldRiver_Rive_CO_F_Edge.SM_RokWorldRiver_Rive_CO_F_Edge"
        ),
        "Rive_CO_F_water": (
            "/Game/RokUnityWorldMapPrototype/Meshes/RiverLayers/"
            "SM_RokWorldRiver_Rive_CO_F_Water.SM_RokWorldRiver_Rive_CO_F_Water"
        ),
        "build_mountain_G": (
            f"{mountain_g_contract['ue_body_mesh_folder']}/"
            f"{mountain_g_contract['ue_body_mesh_name']}."
            f"{mountain_g_contract['ue_body_mesh_name']}"
        ),
        "build_mountain_G_edge": (
            f"{WATERFALL_LAYER_MESH_DESTINATION}/"
            "SM_RokWorldWaterfall_build_mountain_G_Edge."
            "SM_RokWorldWaterfall_build_mountain_G_Edge"
        ),
        water_node: (
            f"{WATERFALL_LAYER_MESH_DESTINATION}/"
            "SM_RokWorldWaterfall_build_mountain_G_Water."
            "SM_RokWorldWaterfall_build_mountain_G_Water"
        ),
    }
    ue_material_by_guid = {
        "355a14738c7d4d445b4727529ceb6234": (
            "/Game/RokUnityWorldMapPrototype/Materials/"
            "M_WorldRiver_Edge.M_WorldRiver_Edge"
        ),
        "536f54f1b429164408f7aba6c1911c83": (
            "/Game/RokUnityWorldMapPrototype/Materials/"
            "M_WorldRiver_Water.M_WorldRiver_Water"
        ),
        "929fc4b646183f042b6eaaec1bef20d3": (
            "/Game/RokUnityWorldMapPrototype/Materials/"
            "M_WorldSource_build_mountain_A.M_WorldSource_build_mountain_A"
        ),
        "04b57c961a6ceb9428f51fb3b37b0b67": (
            "/Game/RokUnityWorldMapPrototype/Materials/"
            "M_WorldSource_build_mountain_T.M_WorldSource_build_mountain_T"
        ),
        "7d0ff6461f170264b8171fb71b2cbffb": (
            "/Game/RokUnityWorldMapPrototype/Materials/"
            "M_WorldWaterfall_WaterLight.M_WorldWaterfall_WaterLight"
        ),
    }
    water_like_guids = {
        "536f54f1b429164408f7aba6c1911c83",
        "7d0ff6461f170264b8171fb71b2cbffb",
    }

    static_parts = []
    for go_file_id, mesh_filter in sorted(
        mesh_filters.items(), key=lambda pair: game_objects[pair[0]]["name"]
    ):
        game_object = game_objects[go_file_id]
        renderer = renderers.get(go_file_id)
        if renderer is None or not renderer["material_guids"]:
            raise RuntimeError(
                f"Rive_CO_H part {game_object['name']} lacks a MeshRenderer"
            )
        if game_object["active"] != 1 or renderer["enabled"] != 1:
            raise RuntimeError(
                f"Rive_CO_H part {game_object['name']} is not active"
            )
        mesh_file_id = mesh_filter["mesh_file_id"]
        mesh_node = mesh_node_by_file_id.get(mesh_file_id)
        if mesh_node is None:
            if mesh_file_id == "4377289380272011037":
                mesh_node = water_node
            else:
                raise RuntimeError(
                    f"Rive_CO_H part {game_object['name']} mesh fileID "
                    f"{mesh_file_id} is not source-resolved"
                )
        material_guids = renderer["material_guids"]
        if len(material_guids) != 1:
            raise RuntimeError(
                f"Rive_CO_H part {game_object['name']} has "
                f"{len(material_guids)} materials"
            )
        material_guid = material_guids[0]
        static_parts.append(
            {
                "game_object": game_object["name"],
                "mesh_node": mesh_node,
                "mesh_file_id": mesh_file_id,
                "mesh_guid": mesh_filter["mesh_guid"],
                "material_guid": material_guid,
                "material_path": rel(guid_index[material_guid]),
                "ue_mesh": ue_mesh_by_node[mesh_node],
                "ue_material": ue_material_by_guid[material_guid],
                "local_matrix": prefab_matrix(go_file_id),
                "sort_priority": 1 if material_guid in water_like_guids else 0,
            }
        )
    if len(static_parts) != 6:
        raise RuntimeError(
            f"Rive_CO_H expected 6 static parts, found {len(static_parts)}"
        )

    effect_parts = []
    for go_file_id in sorted(
        particle_marks, key=lambda fid: game_objects[fid]["name"]
    ):
        game_object = game_objects[go_file_id]
        particle_renderer = particle_renderers.get(go_file_id, {})
        effect_parts.append(
            {
                "game_object": game_object["name"],
                "local_matrix": prefab_matrix(go_file_id),
                "material_guids": particle_renderer.get(
                    "material_guids", []
                ),
                "mesh_file_id": particle_renderer.get("mesh_file_id", ""),
                "mesh_guid": particle_renderer.get("mesh_guid", ""),
            }
        )
    if len(effect_parts) != 6:
        raise RuntimeError(
            f"Rive_CO_H expected 6 particle systems, found "
            f"{len(effect_parts)}"
        )

    material_records = {}
    for ue_material_name, spec in WATERFALL_MATERIAL_SOURCES.items():
        record = material_texture_record(
            spec["source_material"], guid_index
        )
        material_records[ue_material_name] = {
            **record,
            "ue_material_name": ue_material_name,
            "ue_render": dict(spec["ue_render"]),
        }

    return {
        "policy": "split_fbx_exact_static_parts",
        "static_parts": static_parts,
        "effect_parts": effect_parts,
        "effect_policy": (
            "particle_systems_pending_niagara_or_omission_policy"
        ),
        "material_records": material_records,
    }


def build_object_source_records(world_manifest, guid_index):
    records = {}
    pending_reasons = {}
    source_prefabs = world_manifest["full_scene"]["source_prefabs"]
    for prefab_id, record in source_prefabs.items():
        if prefab_id.startswith("I_TYPE_"):
            continue
        if prefab_id in RIVER_CORE_IDS:
            source_path = PROJECT_ROOT / RIVER_CORE_FBX_PATHS[prefab_id]
            prefab_path = PROJECT_ROOT / record["source_prefab_path"]
            geometry_contract = parse_river_core_geometry_contract(
                prefab_id,
                prefab_path,
                source_path,
                guid_index,
            )
            records[prefab_id] = {
                "representation": "exact_source_fbx_render_layers",
                "source_prefab": record["source_prefab_path"],
                "source_prefab_record": source_record(prefab_path),
                "source_mesh": source_record(source_path),
                "river_geometry_contract": geometry_contract,
                "ue_mesh_name": "",
            }
        elif prefab_id in MOUNTAIN_FBX_PATHS:
            source_path = PROJECT_ROOT / MOUNTAIN_FBX_PATHS[prefab_id]
            prefab_path = PROJECT_ROOT / record["source_prefab_path"]
            geometry_contract = parse_mountain_geometry_contract(
                prefab_id,
                prefab_path,
                source_path,
                guid_index,
            )
            records[prefab_id] = {
                "representation": "exact_source_fbx",
                "source_prefab": record["source_prefab_path"],
                "source_mesh": source_record(source_path),
                "mountain_geometry_contract": geometry_contract,
                "ue_mesh_name": geometry_contract["ue_body_mesh_name"],
                "ue_mesh_folder": geometry_contract["ue_body_mesh_folder"],
            }
        elif prefab_id == WATERFALL_PREFAB_ID:
            prefab_path = PROJECT_ROOT / record["source_prefab_path"]
            waterfall_contract = parse_waterfall_geometry_contract(
                prefab_path,
                guid_index,
            )
            records[prefab_id] = {
                "representation": "exact_source_fbx_composite_waterfall",
                "source_prefab": record["source_prefab_path"],
                "source_prefab_record": source_record(prefab_path),
                "waterfall_geometry_contract": waterfall_contract,
                "ue_mesh_name": "",
            }
        elif prefab_id in {"build_mountain_I", "build_mountain_I1"}:
            records[prefab_id] = {
                "representation": "empty_source_prefab",
                "source_prefab": record["source_prefab_path"],
                "ue_mesh_name": "",
            }
        else:
            mesh_paths = [
                PROJECT_ROOT / dependency["path"]
                for dependency in record["resolved_dependencies"]
                if dependency["path"].endswith(".asset")
            ]
            material_paths = [
                dependency["path"]
                for dependency in record["resolved_dependencies"]
                if dependency["path"].endswith(".mat")
            ]
            source_material = (
                material_paths[0]
                if len(material_paths) == 1
                else ""
            )
            reconstruct_tree_shader = (
                len(mesh_paths) == 1
                and len(material_paths) == 1
                and source_material in TREE_MATERIAL_PATHS
            )
            if not reconstruct_tree_shader:
                pending_reasons[prefab_id] = {
                    "reason": (
                        "requires_additional_source_render_pipeline"
                    ),
                    "source_prefab": record["source_prefab_path"],
                    "unity_mesh_asset_count": len(mesh_paths),
                    "unity_material_count": len(material_paths),
                    "source_materials": sorted(material_paths),
                    "resolved_dependencies": record[
                        "resolved_dependencies"
                    ],
                }
                continue
            export = export_unity_mesh_to_obj(
                prefab_id,
                mesh_paths[0],
                reconstruct_tree_shader=True,
            )
            records[prefab_id] = {
                "representation": "exact_unity_mesh_export",
                "source_prefab": record["source_prefab_path"],
                **export,
                "source_material": source_material,
                "ue_mesh_name": f"SM_RokWorld_{safe_name(prefab_id)}",
            }
    return records, pending_reasons


def build_full_scene_object_instance_counts(world_manifest):
    tile_type_counts = {}
    for tile in world_manifest["map"]["tiles"]:
        tile_id = tile["tile_id"]
        tile_type_counts[tile_id] = (
            tile_type_counts.get(tile_id, 0) + 1
        )
    counts = {}
    for template in world_manifest["tile_templates"]:
        multiplier = tile_type_counts.get(template["tile_id"], 0)
        for object_record in template["objects"]:
            prefab_id = object_record["prefab_id"]
            counts[prefab_id] = (
                counts.get(prefab_id, 0) + multiplier
            )
    expected_total = int(
        world_manifest["full_scene"]["expanded_object_instance_count"]
    )
    if sum(counts.values()) != expected_total:
        raise RuntimeError(
            "Expanded world object instance audit differs from the "
            f"world manifest: {sum(counts.values())} != {expected_total}"
        )
    return counts


def main():
    global CITY_INITIAL_DXF
    global CITY_BASE_MAX_LOD_SCALE
    global CITY_AGE_SIZE
    global CITY_MAX_AGE_SIZE
    global CITY_AGE_LOD_SCALE_CAP
    global CITY_SCALE_CURVE_KEYS
    global CITY_INITIAL_CURVE_SCALE
    global TREE_SCALE_BAKE_DXF
    global TREE_VERTEX_SCALE

    if not WORLD_MANIFEST_PATH.exists():
        raise RuntimeError(
            "Generate unity_world_map.json before preparing world-map assets"
        )
    world_manifest = json.loads(
        WORLD_MANIFEST_PATH.read_text(encoding="utf-8")
    )
    if world_manifest.get("schema") != "rok.unity_world_map.v1":
        raise RuntimeError(
            f"Unexpected world-map schema: {world_manifest.get('schema')}"
        )
    source_semantics = derive_source_semantics(world_manifest)
    CITY_INITIAL_DXF = source_semantics["map_tactical_dxf"]
    CITY_BASE_MAX_LOD_SCALE = source_semantics["city"][
        "base_max_scale"
    ]
    CITY_AGE_SIZE = source_semantics["city"]["age_size"]
    CITY_MAX_AGE_SIZE = source_semantics["city"]["max_age_size"]
    CITY_AGE_LOD_SCALE_CAP = source_semantics["city"][
        "age_scale_cap"
    ]
    CITY_SCALE_CURVE_KEYS = source_semantics["city"]["curve_keys"]
    CITY_INITIAL_CURVE_SCALE = source_semantics["city"][
        "curve_scale_at_map_tactical"
    ]
    TREE_SCALE_BAKE_DXF = source_semantics["map_tactical_dxf"]
    TREE_VERTEX_SCALE = source_semantics["tree"][
        "scale_at_map_tactical"
    ]

    TEXTURE_OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    MESH_OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    guid_index = build_guid_index()
    full_scene = world_manifest.get("full_scene", {})
    plane_ids = sorted(full_scene.get("ground_prefab_ids", []))
    if not plane_ids:
        raise RuntimeError(
            "World-map manifest has no full-scene ground prefab inventory"
        )
    baked_materials = []
    source_prefabs = full_scene.get("source_prefabs", {})
    for prefab_id in plane_ids:
        dependencies = source_prefabs[prefab_id]["resolved_dependencies"]
        material_paths = [
            PROJECT_ROOT / item["path"]
            for item in dependencies
            if item["path"].endswith(".mat")
        ]
        if len(material_paths) != 1:
            raise RuntimeError(
                f"{prefab_id} expected one source material, found "
                f"{len(material_paths)}"
            )
        baked_materials.append(
            bake_splat_material(prefab_id, material_paths[0], guid_index)
        )

    mountain_materials = [
        material_texture_record(path, guid_index)
        for path in MOUNTAIN_MATERIAL_PATHS
    ]
    for record in mountain_materials:
        record["render_contract"] = dict(MOUNTAIN_RENDER_CONTRACT)
    tree_materials = [
        material_texture_record(path, guid_index)
        for path in TREE_MATERIAL_PATHS
    ]
    (
        object_sources,
        pending_object_reasons,
    ) = build_object_source_records(world_manifest, guid_index)
    required_object_prefab_ids = set(
        full_scene.get("object_prefab_ids", [])
    )
    prepared_object_prefab_ids = set(object_sources)
    object_instance_counts = build_full_scene_object_instance_counts(
        world_manifest
    )
    required_object_instance_count = sum(
        object_instance_counts.get(prefab_id, 0)
        for prefab_id in required_object_prefab_ids
    )
    prepared_object_instance_count = sum(
        object_instance_counts.get(prefab_id, 0)
        for prefab_id in prepared_object_prefab_ids
    )
    pending_object_prefab_ids = sorted(
        required_object_prefab_ids - prepared_object_prefab_ids
    )
    pending_object_instance_count = (
        required_object_instance_count
        - prepared_object_instance_count
    )
    player_city = build_player_city_record()
    manifest = {
        "schema": "rok.unity_world_map_assets.v1",
        "source_world_manifest": {
            "path": rel(WORLD_MANIFEST_PATH),
            "sha256": sha256(WORLD_MANIFEST_PATH),
        },
        "bake_size": [BAKE_SIZE, BAKE_SIZE],
        "baked_ground_material_count": len(baked_materials),
        "baked_ground_materials": baked_materials,
        "mountain_render_contract": MOUNTAIN_RENDER_CONTRACT,
        "mountain_materials": mountain_materials,
        "tree_materials": tree_materials,
        "tree_scale_curve_source": source_record(
            PROJECT_ROOT / TREE_SCALE_CURVE_SOURCE
        ),
        "tree_scale_bake_dxf": TREE_SCALE_BAKE_DXF,
        "tree_scale_bake_value": TREE_VERTEX_SCALE,
        "source_semantics": source_semantics,
        "object_sources": object_sources,
        "pending_object_reasons": pending_object_reasons,
        "player_city": player_city,
        "object_representation_summary": {
            "exact_source_fbx": sum(
                1
                for item in object_sources.values()
                if item["representation"] == "exact_source_fbx"
            ),
            "exact_source_fbx_render_layers": sum(
                1
                for item in object_sources.values()
                if item["representation"]
                == "exact_source_fbx_render_layers"
            ),
            "exact_unity_mesh_export": sum(
                1
                for item in object_sources.values()
                if item["representation"] == "exact_unity_mesh_export"
            ),
            "empty_source_prefab": sum(
                1
                for item in object_sources.values()
                if item["representation"] == "empty_source_prefab"
            ),
        },
        "full_scene_asset_coverage": {
            "required_ground_prefab_count": len(plane_ids),
            "prepared_ground_prefab_count": len(baked_materials),
            "pending_ground_prefab_ids": sorted(
                set(plane_ids)
                - {
                    record["prefab_id"]
                    for record in baked_materials
                }
            ),
            "required_object_prefab_count": len(
                required_object_prefab_ids
            ),
            "prepared_object_prefab_count": len(
                prepared_object_prefab_ids
            ),
            "pending_object_prefab_ids": pending_object_prefab_ids,
            "required_object_instance_count": (
                required_object_instance_count
            ),
            "prepared_object_instance_count": (
                prepared_object_instance_count
            ),
            "pending_object_instance_count": (
                pending_object_instance_count
            ),
            "prepared_object_instance_percent": round(
                100.0
                * prepared_object_instance_count
                / required_object_instance_count,
                2,
            ),
            "pending_object_prefab_instance_counts": {
                prefab_id: object_instance_counts[prefab_id]
                for prefab_id in pending_object_prefab_ids
            },
        },
        "phase1_scope": [
            (
                "All ground-plane source materials required by the Unity "
                "40x40 world are baked. The current promoted UE level still "
                "places the selected 3x3 window while streaming is built."
            ),
            (
                "Direct mountain records split the exact source FBX and render "
                "only the prefab's enabled body MeshFilter. Edge and collision "
                "nodes are excluded from runtime rendering to prevent the old "
                "combined opaque depth conflict. Source distance-LOD fidelity "
                "remains outside this stability slice. Unity combined grove/"
                "adornment Mesh assets are decoded and exported to deterministic "
                "OBJ files with their original atlas UVs and the source tree "
                "shader's COLOR-center vertex expansion."
            ),
            (
                "Phase 1 bakes the source tree shader scale at map_tactical "
                "and ARokWorldMapCameraPawn applies the parsed Unity curve "
                "as a runtime ratio across the supported zoom range."
            ),
            (
                "The two build_mountain_I/I1 source prefabs are intentionally "
                "empty in Unity and therefore generate no UE geometry."
            ),
            (
                "River Core Rive_CO_A/B/C records audit the exact split FBX, "
                "preserve the edge and dynamic-water render layers as one "
                "logical object, exclude both collision nodes, and require "
                "vertex-color replacement. This is a static source contract; "
                "UE import, map generation, and runtime visuals remain pending."
            ),
            (
                "The interactive player city uses the audited Unity runtime "
                "TownCenter build_1_101_1 export, its translucent city shader "
                "semantics, and the source LevelDetailCastle age-capped LOD "
                "curve. Castle_6_5 single-building diagnostics are excluded."
            ),
        ],
    }
    OUTPUT_MANIFEST_PATH.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    summary = manifest["object_representation_summary"]
    print(
        "Rok Unity world-map assets prepared: "
        f"ground_materials={len(baked_materials)} "
        f"exact_objects={summary['exact_source_fbx']} "
        f"river_render_layers="
        f"{summary['exact_source_fbx_render_layers']} "
        f"exported_unity_meshes={summary['exact_unity_mesh_export']} "
        f"empty_sources={summary['empty_source_prefab']}"
    )


if __name__ == "__main__":
    main()
