import hashlib
import json
import math
import os
import re
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
MIGRATION_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
WORLD_MANIFEST_PATH = MIGRATION_DIR / "unity_world_map.json"
ASSET_MANIFEST_PATH = MIGRATION_DIR / "unity_world_map_assets.json"
GENERATION_PATH = MIGRATION_DIR / "unity_world_map_ue_generation.json"
TARGET_LEVEL = "/Game/Maps/L_RokUnityWorldMapPrototype"
PLAYER_CITY_LEVEL = "/Game/Maps/L_RokUnityGameAppCity"
TARGET_LEVEL_FILE = (
    PROJECT_ROOT
    / "Content"
    / "Maps"
    / "L_RokUnityWorldMapPrototype.umap"
)
CONTENT_ROOT = "/Game/RokUnityWorldMapPrototype"
MESH_DESTINATION = CONTENT_ROOT + "/Meshes"
MOUNTAIN_BODY_MESH_DESTINATION = MESH_DESTINATION + "/MountainBodies"
RIVER_LAYER_MESH_DESTINATION = MESH_DESTINATION + "/RiverLayers"
WATERFALL_LAYER_MESH_DESTINATION = MESH_DESTINATION + "/WaterfallLayers"
TEXTURE_DESTINATION = CONTENT_ROOT + "/Textures"
MATERIAL_DESTINATION = CONTENT_ROOT + "/Materials"
MOUNTAIN_F_DECODED_TEXTURE = (
    TEXTURE_DESTINATION
    + "/T_MountainF_UnityDecodedMips.T_MountainF_UnityDecodedMips"
)
MOUNTAIN_F_MATERIAL = (
    MATERIAL_DESTINATION
    + "/M_WorldSource_build_mountain_F.M_WorldSource_build_mountain_F"
)
MOUNTAIN_F_UPGRADE_REPORT = (
    MIGRATION_DIR / "MountainFGammaProduct" / "ue_upgrade_report.json"
)
GAME_MODE_CLASS = "/Script/Rok.RokWorldMapGameMode"
CONFIG_ACTOR_CLASS = "/Script/Rok.RokWorldMapConfigActor"
CITY_ACTOR_CLASS = "/Script/Rok.RokWorldMapCityActor"
GROUND_TRANSLUCENCY_SORT_PRIORITY = -5
PLANE_SOURCE = (
    PROJECT_ROOT
    / "ref"
    / "client-unity"
    / "Assets"
    / "BundleAssets"
    / "Map"
    / "Env"
    / "Plane"
    / "Plane60_60.FBX"
)
STALE_CITY_ASSETS = (
    (
        MATERIAL_DESTINATION
        + "/M_WorldPlayerCity_Castle_6_5"
    ),
    (
        TEXTURE_DESTINATION
        + "/T_WorldPlayerCity_Castle_6_5"
    ),
)
REUSE_EXISTING_ASSETS = (
    "-RokWorldMapReuseExistingAssets"
    in str(unreal.SystemLibrary.get_command_line())
)
TREE_SOURCE_MATERIAL_NAMES = {
    "map_tree_nomask.mat",
    "map_tree_nomask_1.mat",
}
TREE_UE_MATERIAL_NAMES = [
    "M_WorldSource_map_tree_nomask",
    "M_WorldSource_map_tree_nomask_1",
]
SPRITE_COLOR_PARAMETER = "UnitySpriteColor"
BACKGROUND_COLOR_PARAMETER = "UnityCameraBackgroundColor"
BACKGROUND_ACTOR_LABEL = "RokWorldMap_SolidColorUnderlay"
BACKGROUND_MATERIAL_NAME = "M_WorldMapCameraBackground"
BACKGROUND_TRANSLUCENCY_SORT_PRIORITY = -10
RIVER_REPRESENTATION = "exact_source_fbx_render_layers"
RIVER_FOLDER = "RokUnityWorldMap/SourceObjects/RiverLayers"
RIVER_EDGE_MATERIAL_NAME = "M_WorldRiver_Edge"
RIVER_WATER_MATERIAL_NAME = "M_WorldRiver_Water"
RIVER_EDGE_TEXTURE_NAME = "T_WorldRiver_Edge"
RIVER_WATER_TEXTURE_NAME = "T_WorldRiver_Bump"
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


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def safe_name(value):
    sanitized = re.sub(r"[^A-Za-z0-9_]+", "_", str(value)).strip("_")
    return sanitized or "Unnamed"


def object_path(destination, asset_name):
    return f"{destination}/{asset_name}.{asset_name}"


def load_asset(path):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return None
    return unreal.EditorAssetLibrary.load_asset(path)


def make_rotator(pitch=0.0, yaw=0.0, roll=0.0):
    # Unreal Python constructor order is roll, pitch, yaw.
    return unreal.Rotator(roll, pitch, yaw)


def source_sprite_plane_rotation():
    # The source world camera looks along Unity (0,-0.7,+0.7), which maps to
    # UE (0,+0.7,-0.7). Align the Plane60 local normal to that fixed source
    # direction so the city card is not viewed edge-on.
    source_normal = unreal.Vector(0.0, 0.70710678, -0.70710678)
    try:
        return unreal.MathLibrary.make_rot_from_zy(
            source_normal,
            unreal.Vector(0.0, 0.0, 1.0),
        )
    except Exception:
        return unreal.KismetMathLibrary.make_rot_from_zy(
            source_normal,
            unreal.Vector(0.0, 0.0, 1.0),
        )


def load_json(path, expected_schema):
    if not path.exists():
        raise RuntimeError(f"Missing generated input: {path}")
    payload = json.loads(path.read_text(encoding="utf-8"))
    if payload.get("schema") != expected_schema:
        raise RuntimeError(
            f"Unexpected schema in {path.name}: {payload.get('schema')}"
        )
    return payload


def configure_static_mesh_import(
    source_path,
    combine_meshes=True,
    replace_vertex_colors=False,
    preserve_node_transforms=False,
):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("automated_import_should_detect_type", False)
    try:
        options.set_editor_property(
            "mesh_type_to_import",
            unreal.FBXImportType.FBXIT_STATIC_MESH,
        )
    except Exception:
        pass
    static_data = options.get_editor_property("static_mesh_import_data")
    static_data.set_editor_property("combine_meshes", bool(combine_meshes))
    static_data.set_editor_property("generate_lightmap_u_vs", False)
    static_data.set_editor_property("auto_generate_collision", False)
    if replace_vertex_colors:
        vertex_color_enum = getattr(
            unreal,
            "VertexColorImportOption",
            None,
        )
        replace_value = (
            getattr(vertex_color_enum, "REPLACE", None)
            if vertex_color_enum
            else None
        )
        if replace_value is None:
            raise RuntimeError(
                "UE Python does not expose VertexColorImportOption.REPLACE"
            )
        static_data.set_editor_property(
            "vertex_color_import_option",
            replace_value,
        )
    if preserve_node_transforms:
        static_data.set_editor_property(
            "transform_vertex_to_absolute",
            False,
        )
        static_data.set_editor_property("bake_pivot_in_vertex", False)
    for property_name, value in (
        ("convert_scene", source_path.suffix.lower() != ".obj"),
        ("convert_scene_unit", source_path.suffix.lower() != ".obj"),
        ("force_front_x_axis", False),
        ("import_uniform_scale", 1.0),
    ):
        try:
            static_data.set_editor_property(property_name, value)
        except Exception:
            pass
    return options


def import_static_mesh(source_path, asset_name):
    if not source_path.exists():
        raise RuntimeError(f"Missing source mesh: {source_path}")
    asset_name = safe_name(asset_name)
    package_path = object_path(MESH_DESTINATION, asset_name)
    if REUSE_EXISTING_ASSETS:
        existing = load_asset(package_path)
        if not isinstance(existing, unreal.StaticMesh):
            raise RuntimeError(
                f"Reusable static mesh is missing: {package_path}"
            )
        return existing, package_path
    unreal.EditorAssetLibrary.make_directory(MESH_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_path))
    task.set_editor_property("destination_path", MESH_DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", configure_static_mesh_import(source_path))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = load_asset(package_path)
    if not isinstance(imported, unreal.StaticMesh):
        for imported_path in task.get_editor_property("imported_object_paths"):
            candidate = load_asset(imported_path)
            if isinstance(candidate, unreal.StaticMesh):
                imported = candidate
                package_path = candidate.get_path_name()
                break
    if not isinstance(imported, unreal.StaticMesh):
        raise RuntimeError(f"Failed to import static mesh: {source_path}")
    return imported, package_path


def purge_package_from_disk(package_path_str):
    """Fully clear a generator-owned package so a same-session re-import can
    rename onto its name. delete_asset alone leaves the memory-resident
    UPackage claiming the destination name, which makes rename_asset fail
    with "destination path already exists"; renaming the old asset to a
    unique trash name first frees the name."""
    if not unreal.EditorAssetLibrary.does_asset_exist(package_path_str):
        _unlink_package_file(package_path_str)
        return
    trash_path = f"{package_path_str}_PurgeTrash_{os.getpid()}"
    unreal.EditorAssetLibrary.rename_asset(package_path_str, trash_path)
    for path in (trash_path, package_path_str):
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            if not unreal.EditorAssetLibrary.delete_asset(path):
                raise RuntimeError(
                    f"Failed to delete generator-owned asset: {path}"
                )
        _unlink_package_file(path)


def _unlink_package_file(package_path_str):
    relative = package_path_str
    if relative.startswith("/Game/"):
        relative = relative[len("/Game/"):]
    asset_file = PROJECT_ROOT / "Content" / (relative + ".uasset")
    if asset_file.exists():
        asset_file.unlink()


def import_mountain_body_mesh(source_path, record):
    if not source_path.exists():
        raise RuntimeError(f"Missing source mountain mesh: {source_path}")
    contract = record.get("mountain_geometry_contract", {})
    if contract.get("policy") != "split_fbx_exact_body_only":
        raise RuntimeError(
            f"{source_path.name} lacks the split-body geometry contract"
        )
    if contract.get("combine_meshes") is not False:
        raise RuntimeError(
            f"{source_path.name} attempts to combine visible and collision nodes"
        )
    destination = contract.get("ue_body_mesh_folder")
    if destination != MOUNTAIN_BODY_MESH_DESTINATION:
        raise RuntimeError(
            f"{source_path.name} has unexpected mountain body destination "
            f"{destination}"
        )
    import_base_name = safe_name(contract.get("ue_import_base_name", ""))
    body_mesh_name = safe_name(contract.get("ue_body_mesh_name", ""))
    expected_path = contract.get("ue_body_mesh_asset")
    if expected_path != object_path(destination, body_mesh_name):
        raise RuntimeError(
            f"{source_path.name} body asset path is not deterministic"
        )
    if record.get("ue_mesh_name") != body_mesh_name:
        raise RuntimeError(
            f"{source_path.name} body mesh identity differs from its manifest"
        )
    body_node_name = safe_name(
        contract["body_render_node"]["game_object"]
    )
    excluded_node_names = {
        safe_name(name)
        for name in contract.get("excluded_fbx_nodes", [])
    }
    if body_node_name in excluded_node_names:
        raise RuntimeError(
            f"{source_path.name} body node is also marked excluded: "
            f"{body_node_name}"
        )
    raw_body_path = object_path(destination, body_node_name)

    if REUSE_EXISTING_ASSETS:
        body_mesh = load_asset(expected_path)
        if not isinstance(body_mesh, unreal.StaticMesh):
            raise RuntimeError(
                f"Reusable mountain body mesh is missing: {expected_path}"
            )
        imported_paths = [expected_path]
    else:
        unreal.EditorAssetLibrary.make_directory(destination)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source_path))
        task.set_editor_property("destination_path", destination)
        task.set_editor_property("destination_name", import_base_name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("replace_existing_settings", True)
        task.set_editor_property("save", True)
        task.set_editor_property(
            "options",
            configure_static_mesh_import(
                source_path,
                combine_meshes=False,
            ),
        )
        for stale_body_path in sorted({expected_path, raw_body_path}):
            purge_package_from_disk(stale_body_path)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        imported_paths = sorted(
            set(task.get_editor_property("imported_object_paths"))
        )
        body_candidates = [
            (path, asset)
            for path in imported_paths
            for asset in (load_asset(path),)
            if (
                isinstance(asset, unreal.StaticMesh)
                and safe_name(asset.get_name()) == body_node_name
            )
        ]
        if len(body_candidates) != 1:
            raise RuntimeError(
                f"{source_path.name} split import expected exactly one raw "
                f"body node {body_node_name}, found {len(body_candidates)} "
                f"in {imported_paths}"
            )
        raw_body_path, body_mesh = body_candidates[0]
        if raw_body_path != expected_path:
            raw_body_package = raw_body_path.split(".", 1)[0]
            expected_package = expected_path.split(".", 1)[0]
            if not unreal.EditorAssetLibrary.rename_asset(
                raw_body_package,
                expected_package,
            ):
                raise RuntimeError(
                    f"Failed to rename split mountain body "
                    f"{raw_body_path} to {expected_path}"
                )
            imported_paths = sorted(
                expected_path if path == raw_body_path else path
                for path in imported_paths
            )
            body_mesh = load_asset(expected_path)
            if isinstance(body_mesh, unreal.StaticMesh):
                unreal.EditorAssetLibrary.save_loaded_asset(body_mesh)
        if not isinstance(body_mesh, unreal.StaticMesh):
            raise RuntimeError(
                f"Split mountain body failed to load: {expected_path}"
            )

    static_materials = body_mesh.get_editor_property("static_materials")
    if len(static_materials) != 1:
        raise RuntimeError(
            f"{expected_path} has {len(static_materials)} material slots; "
            "the exact body node must have one"
        )
    return body_mesh, expected_path, imported_paths


def river_layer_asset_name(prefab_id, role):
    return (
        f"SM_RokWorldRiver_{safe_name(prefab_id)}_"
        f"{safe_name(role).title()}"
    )


def import_river_layer_meshes(source_path, prefab_id, record):
    if not source_path.exists():
        raise RuntimeError(f"Missing source river mesh: {source_path}")
    if sha256(source_path) != record.get("source_mesh", {}).get("sha256"):
        raise RuntimeError(
            f"{prefab_id} source FBX changed after asset preparation"
        )
    contract = record.get("river_geometry_contract", {})
    if contract.get("policy") != "split_fbx_exact_render_layers":
        raise RuntimeError(
            f"{prefab_id} lacks the split river render-layer contract"
        )
    import_contract = contract.get("ue_import_contract", {})
    if (
        contract.get("combine_meshes") is not False
        or import_contract.get("combine_meshes") is not False
        or import_contract.get("import_vertex_colors") != "replace"
        or import_contract.get("collision_nodes")
        != "excluded_from_render_and_collision_generation"
    ):
        raise RuntimeError(
            f"{prefab_id} has an invalid exact river import contract"
        )
    render_layers = contract.get("render_layers", [])
    if [layer.get("role") for layer in render_layers] != [
        "edge",
        "water",
    ]:
        raise RuntimeError(
            f"{prefab_id} must declare edge then water render layers"
        )
    expected_nodes = {
        layer["role"]: safe_name(layer["game_object"])
        for layer in render_layers
    }
    excluded_nodes = {
        safe_name(name)
        for name in contract.get("excluded_fbx_nodes", [])
    }
    if set(expected_nodes.values()) & excluded_nodes:
        raise RuntimeError(
            f"{prefab_id} marks a visible river node as excluded"
        )
    expected_paths = {
        role: object_path(
            RIVER_LAYER_MESH_DESTINATION,
            river_layer_asset_name(prefab_id, role),
        )
        for role in ("edge", "water")
    }

    if REUSE_EXISTING_ASSETS:
        layer_meshes = {
            role: load_asset(path)
            for role, path in expected_paths.items()
        }
        missing = [
            path
            for role, path in expected_paths.items()
            if not isinstance(layer_meshes[role], unreal.StaticMesh)
        ]
        if missing:
            raise RuntimeError(
                "Reusable river layer meshes are missing: "
                + ", ".join(missing)
            )
        return layer_meshes, sorted(expected_paths.values()), []

    unreal.EditorAssetLibrary.make_directory(RIVER_LAYER_MESH_DESTINATION)
    for path in expected_paths.values():
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            if not unreal.EditorAssetLibrary.delete_asset(path):
                raise RuntimeError(
                    f"Failed to replace generator-owned river mesh: {path}"
                )
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_path))
    task.set_editor_property(
        "destination_path",
        RIVER_LAYER_MESH_DESTINATION,
    )
    task.set_editor_property(
        "destination_name",
        f"RiverImport_{safe_name(prefab_id)}",
    )
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    task.set_editor_property(
        "options",
        configure_static_mesh_import(
            source_path,
            combine_meshes=False,
            replace_vertex_colors=True,
            preserve_node_transforms=True,
        ),
    )
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported_paths = sorted(
        set(task.get_editor_property("imported_object_paths"))
    )
    imported_meshes = [
        (path, asset)
        for path in imported_paths
        for asset in (load_asset(path),)
        if isinstance(asset, unreal.StaticMesh)
    ]
    by_node = {}
    for path, mesh in imported_meshes:
        node_name = safe_name(mesh.get_name())
        if node_name in by_node:
            raise RuntimeError(
                f"{prefab_id} split import duplicated node {node_name}"
            )
        by_node[node_name] = (path, mesh)
    expected_all_nodes = set(expected_nodes.values()) | excluded_nodes
    if set(by_node) != expected_all_nodes:
        # FBX Geometry nodes with empty names import as
        # "Mesh"/"Mesh_ncl_N" placeholders in FBX definition order;
        # the contract's source_fbx_geometry_model_order records that
        # order so the mapping stays source-derived.
        placeholder = re.compile(r"^Mesh(_ncl_[0-9]+)?$")
        geometry_model_order = [
            safe_name(name)
            for name in contract.get(
                "source_fbx_geometry_model_order", []
            )
        ]
        if len(by_node) == len(geometry_model_order) and all(
            placeholder.match(name) for name in by_node
        ):
            def placeholder_index(name):
                if name == "Mesh":
                    return 0
                return int(name.rsplit("_", 1)[1])

            ordered_imports = sorted(by_node, key=placeholder_index)
            by_node = {
                model_name: by_node[import_name]
                for model_name, import_name in zip(
                    geometry_model_order, ordered_imports
                )
            }
    if set(by_node) != expected_all_nodes:
        raise RuntimeError(
            f"{prefab_id} split import nodes are {sorted(by_node)}, "
            f"expected {sorted(expected_all_nodes)}"
        )

    deleted_collision_paths = []
    for node_name in sorted(excluded_nodes):
        collision_path, _ = by_node[node_name]
        collision_package = collision_path.split(".", 1)[0]
        if not unreal.EditorAssetLibrary.delete_asset(collision_package):
            raise RuntimeError(
                f"Failed to delete excluded river collision node "
                f"{collision_path}"
            )
        deleted_collision_paths.append(collision_path)

    layer_meshes = {}
    selected_paths = []
    for role in ("edge", "water"):
        raw_path, _ = by_node[expected_nodes[role]]
        expected_path = expected_paths[role]
        if raw_path != expected_path:
            if not unreal.EditorAssetLibrary.rename_asset(
                raw_path.split(".", 1)[0],
                expected_path.split(".", 1)[0],
            ):
                raise RuntimeError(
                    f"Failed to rename river {role} mesh {raw_path} "
                    f"to {expected_path}"
                )
        mesh = load_asset(expected_path)
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError(
                f"Renamed river {role} mesh is missing: {expected_path}"
            )
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        layer_meshes[role] = mesh
        selected_paths.append(expected_path)
    return layer_meshes, sorted(selected_paths), deleted_collision_paths


def import_texture(
    source_path,
    asset_name,
    no_mipmaps=False,
    clamp=False,
    max_texture_size=0,
    srgb=True,
    repeat=False,
):
    if not source_path.exists():
        raise RuntimeError(f"Missing source texture: {source_path}")
    asset_name = safe_name(asset_name)
    package_path = object_path(TEXTURE_DESTINATION, asset_name)
    if REUSE_EXISTING_ASSETS:
        existing = load_asset(package_path)
        if not isinstance(existing, unreal.Texture2D):
            raise RuntimeError(
                f"Reusable texture is missing: {package_path}"
            )
        return existing, package_path
    unreal.EditorAssetLibrary.make_directory(TEXTURE_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_path))
    task.set_editor_property("destination_path", TEXTURE_DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", unreal.TextureFactory())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = load_asset(package_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Failed to import texture: {source_path}")
    texture.set_editor_property("srgb", bool(srgb))
    if no_mipmaps:
        texture.set_editor_property(
            "mip_gen_settings",
            unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        )
    if clamp:
        texture.set_editor_property(
            "address_x",
            unreal.TextureAddress.TA_CLAMP,
        )
        texture.set_editor_property(
            "address_y",
            unreal.TextureAddress.TA_CLAMP,
        )
        texture.set_editor_property(
            "filter",
            unreal.TextureFilter.TF_BILINEAR,
        )
    if repeat:
        texture.set_editor_property(
            "address_x",
            unreal.TextureAddress.TA_WRAP,
        )
        texture.set_editor_property(
            "address_y",
            unreal.TextureAddress.TA_WRAP,
        )
    if max_texture_size > 0:
        texture.set_editor_property("max_texture_size", max_texture_size)
    try:
        texture.set_editor_property("never_stream", False)
    except Exception:
        pass
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture, package_path


def clear_material_expressions(material):
    for expression in list(
        unreal.MaterialEditingLibrary.get_material_expressions(material)
    ):
        unreal.MaterialEditingLibrary.delete_material_expression(
            material,
            expression,
        )


def color_record_to_linear(color_record):
    return unreal.LinearColor(
        float(color_record["r"]),
        float(color_record["g"]),
        float(color_record["b"]),
        float(color_record.get("a", 1.0)),
    )


def prototype_lighting_phase(world_manifest):
    runtime_lighting = world_manifest["environment"]["runtime_lighting"]
    phase_name = runtime_lighting["prototype_phase"]
    matches = [
        phase
        for phase in runtime_lighting["phases"]
        if phase["phase"] == phase_name
    ]
    if len(matches) != 1:
        raise RuntimeError(
            f"Expected one Unity runtime lighting phase {phase_name}, "
            f"found {len(matches)}"
        )
    return matches[0]


def create_textured_material(
    texture,
    material_name,
    masked=False,
    translucent=False,
    unlit=False,
    sprite_color=None,
    additive=False,
):
    if material_name == "M_WorldSource_build_mountain_A":
        if masked or translucent or additive or not unlit or sprite_color is None:
            raise RuntimeError("Mountain A source Gamma branch requires opaque unlit lighting")
        from CreateRokUnityMountainAGammaMaterial import create
        return create(sprite_color, reuse=REUSE_EXISTING_ASSETS)
    if masked and translucent:
        raise RuntimeError(
            f"Material cannot be both masked and translucent: {material_name}"
        )
    if additive and (masked or translucent):
        raise RuntimeError(
            f"Additive material cannot be masked/translucent: {material_name}"
        )
    material_name = safe_name(material_name)
    package_path = object_path(MATERIAL_DESTINATION, material_name)
    material = load_asset(package_path)
    if REUSE_EXISTING_ASSETS:
        if not isinstance(material, unreal.Material):
            raise RuntimeError(
                f"Reusable material is missing: {package_path}"
            )
        return material, package_path
    if not isinstance(material, unreal.Material):
        unreal.EditorAssetLibrary.make_directory(MATERIAL_DESTINATION)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            material_name,
            MATERIAL_DESTINATION,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"Failed to create material: {package_path}")

    clear_material_expressions(material)
    material.set_editor_property(
        "two_sided", bool(masked or translucent or additive)
    )
    material.set_editor_property(
        "blend_mode",
        unreal.BlendMode.BLEND_MASKED
        if masked
        else (
            unreal.BlendMode.BLEND_TRANSLUCENT
            if translucent
            else (
                unreal.BlendMode.BLEND_ADDITIVE
                if additive
                else unreal.BlendMode.BLEND_OPAQUE
            )
        ),
    )
    material.set_editor_property(
        "shading_model",
        unreal.MaterialShadingModel.MSM_UNLIT
        if unlit
        else unreal.MaterialShadingModel.MSM_DEFAULT_LIT,
    )
    if masked:
        material.set_editor_property("opacity_mask_clip_value", 0.28)
    texture_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSample,
        -460,
        -40,
    )
    texture_node.set_editor_property("texture", texture)
    color_output = texture_node
    color_output_pin = "RGB"
    if unlit:
        if sprite_color is None:
            raise RuntimeError(
                f"Unlit Unity material lacks {SPRITE_COLOR_PARAMETER}: "
                f"{material_name}"
            )
        sprite_color_node = (
            unreal.MaterialEditingLibrary.create_material_expression(
                material,
                unreal.MaterialExpressionVectorParameter,
                -280,
                120,
            )
        )
        sprite_color_node.set_editor_property(
            "parameter_name",
            SPRITE_COLOR_PARAMETER,
        )
        sprite_color_node.set_editor_property(
            "default_value",
            color_record_to_linear(sprite_color),
        )
        multiply_node = (
            unreal.MaterialEditingLibrary.create_material_expression(
                material,
                unreal.MaterialExpressionMultiply,
                -80,
                -20,
            )
        )
        for source, output_name, target, input_name in (
            (texture_node, "RGB", multiply_node, "A"),
            (sprite_color_node, "", multiply_node, "B"),
        ):
            if not unreal.MaterialEditingLibrary.connect_material_expressions(
                source,
                output_name,
                target,
                input_name,
            ):
                raise RuntimeError(
                    f"Failed to connect Unity sprite-color material graph "
                    f"for {package_path}"
                )
        color_output = multiply_node
        color_output_pin = ""
    unreal.MaterialEditingLibrary.connect_material_property(
        color_output,
        color_output_pin,
        unreal.MaterialProperty.MP_EMISSIVE_COLOR
        if unlit
        else unreal.MaterialProperty.MP_BASE_COLOR,
    )
    if masked:
        unreal.MaterialEditingLibrary.connect_material_property(
            texture_node,
            "A",
            unreal.MaterialProperty.MP_OPACITY_MASK,
        )
    if translucent:
        unreal.MaterialEditingLibrary.connect_material_property(
            texture_node,
            "A",
            unreal.MaterialProperty.MP_OPACITY,
        )
    if not unlit:
        roughness = unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionConstant,
            -440,
            220,
        )
        roughness.set_editor_property("r", 0.9)
        unreal.MaterialEditingLibrary.connect_material_property(
            roughness,
            "",
            unreal.MaterialProperty.MP_ROUGHNESS,
        )
    compile_errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if compile_errors:
        raise RuntimeError(
            f"Material compile failed for {package_path}: {compile_errors}"
        )
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material, package_path


def create_material_asset(material_name):
    material_name = safe_name(material_name)
    package_path = object_path(MATERIAL_DESTINATION, material_name)
    material = load_asset(package_path)
    if REUSE_EXISTING_ASSETS:
        if not isinstance(material, unreal.Material):
            raise RuntimeError(
                f"Reusable material is missing: {package_path}"
            )
        return material, package_path
    if not isinstance(material, unreal.Material):
        unreal.EditorAssetLibrary.make_directory(MATERIAL_DESTINATION)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            material_name,
            MATERIAL_DESTINATION,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"Failed to create material: {package_path}")
    clear_material_expressions(material)
    material.set_editor_property("two_sided", False)
    material.set_editor_property(
        "blend_mode",
        unreal.BlendMode.BLEND_TRANSLUCENT,
    )
    material.set_editor_property(
        "shading_model",
        unreal.MaterialShadingModel.MSM_UNLIT,
    )
    return material, package_path


def material_expression(material, expression_class, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material,
        expression_class,
        x,
        y,
    )


def connect_expression(
    source,
    output_name,
    target,
    input_name,
    role,
):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        source,
        output_name,
        target,
        input_name,
    ):
        raise RuntimeError(f"Failed to connect river material graph: {role}")


def set_expression_description(expression, description):
    try:
        expression.set_editor_property("desc", description)
    except Exception:
        pass


def vector_parameter(material, name, values, x, y):
    node = material_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        x,
        y,
    )
    node.set_editor_property("parameter_name", name)
    node.set_editor_property(
        "default_value",
        unreal.LinearColor(
            float(values[0]),
            float(values[1]),
            float(values[2]),
            float(values[3]) if len(values) > 3 else 1.0,
        ),
    )
    return node


def scalar_parameter(material, name, value, x, y):
    node = material_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        x,
        y,
    )
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", float(value))
    return node


def constant_expression(material, value, x, y):
    node = material_expression(
        material,
        unreal.MaterialExpressionConstant,
        x,
        y,
    )
    node.set_editor_property("r", float(value))
    return node


def component_mask(material, source, channels, x, y, role):
    node = material_expression(
        material,
        unreal.MaterialExpressionComponentMask,
        x,
        y,
    )
    for channel in ("r", "g", "b", "a"):
        node.set_editor_property(channel, channel in channels.lower())
    connect_expression(source, "", node, "", role)
    return node


def binary_expression(
    material,
    expression_class,
    left,
    right,
    x,
    y,
    role,
):
    node = material_expression(material, expression_class, x, y)
    connect_expression(left, "", node, "A", role + " A")
    connect_expression(right, "", node, "B", role + " B")
    return node


def normalized_vector(values):
    length = sum(float(value) ** 2 for value in values) ** 0.5
    if length <= 1.0e-8:
        raise RuntimeError("Cannot normalize a zero source vector")
    return [float(value) / length for value in values]


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


def source_light_direction_ue(world_manifest):
    quaternion = world_manifest["environment"]["directional_light"][
        "unity_local_rotation_quaternion"
    ]
    # Unity's directional-light vector is the direction from the surface
    # toward the light, i.e. negative Transform.forward.
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


def create_river_edge_material(
    texture,
    lighting_phase,
    light_direction,
):
    material, package_path = create_material_asset(
        RIVER_EDGE_MATERIAL_NAME
    )
    if REUSE_EXISTING_ASSETS:
        return material, package_path

    texture_node = material_expression(
        material,
        unreal.MaterialExpressionTextureSample,
        -1250,
        -120,
    )
    texture_node.set_editor_property("texture", texture)
    texture_node.set_editor_property(
        "sampler_type",
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
    )
    ambient = vector_parameter(
        material,
        RIVER_PARAMETER_NAMES["ambient"],
        [
            lighting_phase["ambient_color"][channel]
            for channel in ("r", "g", "b", "a")
        ],
        -1250,
        120,
    )
    direction_color = vector_parameter(
        material,
        RIVER_PARAMETER_NAMES["direction_color"],
        [
            lighting_phase["direction_color"][channel]
            for channel in ("r", "g", "b", "a")
        ],
        -1250,
        300,
    )
    direction_intensity = scalar_parameter(
        material,
        RIVER_PARAMETER_NAMES["direction_intensity"],
        lighting_phase["direction_intensity"],
        -1250,
        460,
    )
    light_direction_node = vector_parameter(
        material,
        RIVER_PARAMETER_NAMES["light_direction"],
        [*light_direction, 0.0],
        -1250,
        620,
    )
    normal = material_expression(
        material,
        unreal.MaterialExpressionVertexNormalWS,
        -1250,
        780,
    )
    normalized_normal = material_expression(
        material,
        unreal.MaterialExpressionNormalize,
        -1050,
        780,
    )
    connect_expression(
        normal,
        "",
        normalized_normal,
        "VectorInput",
        "edge normalize VertexNormalWS",
    )
    light_direction_rgb = component_mask(
        material,
        light_direction_node,
        "rgb",
        -1050,
        620,
        "edge light direction RGB",
    )
    dot_node = material_expression(
        material,
        unreal.MaterialExpressionDotProduct,
        -840,
        700,
    )
    connect_expression(
        normalized_normal,
        "",
        dot_node,
        "A",
        "edge normal dot A",
    )
    connect_expression(
        light_direction_rgb,
        "",
        dot_node,
        "B",
        "edge normal dot B",
    )
    zero = constant_expression(material, 0.0, -840, 850)
    ndotl = binary_expression(
        material,
        unreal.MaterialExpressionMax,
        dot_node,
        zero,
        -650,
        720,
        "edge max NdotL",
    )
    two = constant_expression(material, 2.0, -1040, 200)
    ambient_twice = binary_expression(
        material,
        unreal.MaterialExpressionMultiply,
        ambient,
        two,
        -820,
        180,
        "edge ambient times two",
    )
    direction_scaled = binary_expression(
        material,
        unreal.MaterialExpressionMultiply,
        direction_color,
        direction_intensity,
        -820,
        380,
        "edge directional color intensity",
    )
    direction_lit = binary_expression(
        material,
        unreal.MaterialExpressionMultiply,
        direction_scaled,
        ndotl,
        -430,
        460,
        "edge directional NdotL",
    )
    vertex_lighting = binary_expression(
        material,
        unreal.MaterialExpressionAdd,
        ambient_twice,
        direction_lit,
        -200,
        220,
        "edge source vertex lighting",
    )
    texture_rgb = component_mask(
        material,
        texture_node,
        "rgb",
        -820,
        -120,
        "edge texture RGB",
    )
    emissive = binary_expression(
        material,
        unreal.MaterialExpressionMultiply,
        texture_rgb,
        vertex_lighting,
        40,
        80,
        "edge texture times source lighting",
    )
    vertex_color = material_expression(
        material,
        unreal.MaterialExpressionVertexColor,
        -220,
        -180,
    )
    set_expression_description(
        emissive,
        (
            "RokRiverEdgeExact: TextureRGB * "
            "(Ambient*2 + DirectionColor*DirectionIntensity*"
            "max(0,dot(VertexNormalWS,LightDirection)))"
        ),
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        emissive,
        "",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        vertex_color,
        "A",
        unreal.MaterialProperty.MP_OPACITY,
    )
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    try:
        compile_errors = unreal.MaterialEditingLibrary.get_statistics(
            material
        ).get_editor_property("errors")
    except Exception:
        # UE 5.8 MaterialStatistics no longer exposes an 'errors' property;
        # recompile_material above already surfaces compile failures in the log.
        compile_errors = None
    if compile_errors:
        raise RuntimeError(
            f"River edge material compile failed for {package_path}: "
            f"{compile_errors}"
        )
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material, package_path


def create_river_water_material(texture, shader_contract):
    material, package_path = create_material_asset(
        RIVER_WATER_MATERIAL_NAME
    )
    if REUSE_EXISTING_ASSETS:
        return material, package_path

    bump_tiling = vector_parameter(
        material,
        RIVER_PARAMETER_NAMES["bump_tiling"],
        shader_contract["bump_tiling"],
        -1800,
        -520,
    )
    speed = vector_parameter(
        material,
        RIVER_PARAMETER_NAMES["speed"],
        shader_contract["speed"],
        -1800,
        -340,
    )
    base_color = vector_parameter(
        material,
        RIVER_PARAMETER_NAMES["base_color"],
        shader_contract["base_color"],
        -1800,
        700,
    )
    near = scalar_parameter(
        material,
        RIVER_PARAMETER_NAMES["near"],
        shader_contract["near"],
        -1800,
        100,
    )
    far = scalar_parameter(
        material,
        RIVER_PARAMETER_NAMES["far"],
        shader_contract["far"],
        -1800,
        220,
    )
    shininess = scalar_parameter(
        material,
        RIVER_PARAMETER_NAMES["shininess"],
        shader_contract["shininess"],
        -1800,
        420,
    )
    gloss = scalar_parameter(
        material,
        RIVER_PARAMETER_NAMES["gloss"],
        shader_contract["gloss"],
        -1800,
        540,
    )
    local_position = material_expression(
        material,
        unreal.MaterialExpressionPreSkinnedPosition,
        -1800,
        -900,
    )
    local_xz = component_mask(
        material,
        local_position,
        "rb",
        -1600,
        -900,
        "water local position XZ",
    )
    time = material_expression(
        material,
        unreal.MaterialExpressionTime,
        -1800,
        -720,
    )
    twenty = constant_expression(material, 20.0, -1600, -700)
    time_x = binary_expression(
        material,
        unreal.MaterialExpressionDivide,
        time,
        twenty,
        -1400,
        -720,
        "water Unity _Time.x",
    )
    two = constant_expression(material, 2.0, -1400, -980)
    local_twice = binary_expression(
        material,
        unreal.MaterialExpressionMultiply,
        local_xz,
        two,
        -1190,
        -940,
        "water first local position times two",
    )
    speed_xy = component_mask(
        material,
        speed,
        "rg",
        -1600,
        -420,
        "water Speed.xy",
    )
    speed_zw = component_mask(
        material,
        speed,
        "ba",
        -1600,
        -300,
        "water Speed.zw",
    )
    tiling_xy = component_mask(
        material,
        bump_tiling,
        "rg",
        -1600,
        -540,
        "water BumpTiling.xy",
    )
    tiling_zw = component_mask(
        material,
        bump_tiling,
        "ba",
        -1600,
        -600,
        "water BumpTiling.zw",
    )
    scroll_xy = binary_expression(
        material,
        unreal.MaterialExpressionMultiply,
        speed_xy,
        time_x,
        -1170,
        -500,
        "water first scroll",
    )
    scroll_zw = binary_expression(
        material,
        unreal.MaterialExpressionMultiply,
        speed_zw,
        time_x,
        -1170,
        -320,
        "water second scroll",
    )
    uv0_unscaled = binary_expression(
        material,
        unreal.MaterialExpressionAdd,
        local_twice,
        scroll_xy,
        -920,
        -790,
        "water first UV add",
    )
    uv1_unscaled = binary_expression(
        material,
        unreal.MaterialExpressionAdd,
        local_xz,
        scroll_zw,
        -920,
        -500,
        "water second UV add",
    )
    uv0 = binary_expression(
        material,
        unreal.MaterialExpressionMultiply,
        uv0_unscaled,
        tiling_xy,
        -680,
        -790,
        "water first UV tiling",
    )
    uv1 = binary_expression(
        material,
        unreal.MaterialExpressionMultiply,
        uv1_unscaled,
        tiling_zw,
        -680,
        -500,
        "water second UV tiling",
    )
    samples = []
    for index, (uv, y) in enumerate(((uv0, -780), (uv1, -470))):
        sample = material_expression(
            material,
            unreal.MaterialExpressionTextureSample,
            -420,
            y,
        )
        sample.set_editor_property("texture", texture)
        sample.set_editor_property(
            "sampler_type",
            unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR,
        )
        connect_expression(
            uv,
            "",
            sample,
            "UVs",
            f"water bump sample {index} UV",
        )
        samples.append(sample)
    raw_two = constant_expression(material, 2.0, -190, -900)
    raw_one = constant_expression(material, 1.0, -190, -820)
    raw_samples = []
    for index, (sample, y) in enumerate(
        ((samples[0], -700), (samples[1], -420))
    ):
        rgb = component_mask(
            material,
            sample,
            "rgb",
            -180,
            y,
            f"water bump {index} RGB",
        )
        doubled = binary_expression(
            material,
            unreal.MaterialExpressionMultiply,
            rgb,
            raw_two,
            20,
            y,
            f"water bump {index} times two",
        )
        minus_one = material_expression(
            material,
            unreal.MaterialExpressionSubtract,
            220,
            y,
        )
        connect_expression(
            doubled,
            "",
            minus_one,
            "A",
            f"water bump {index} raw A",
        )
        connect_expression(
            raw_one,
            "",
            minus_one,
            "B",
            f"water bump {index} raw B",
        )
        raw_samples.append(minus_one)
    half = constant_expression(material, 2.0, 220, -300)
    second_half = binary_expression(
        material,
        unreal.MaterialExpressionDivide,
        raw_samples[1],
        half,
        430,
        -420,
        "water second raw bump half",
    )
    normal_color = binary_expression(
        material,
        unreal.MaterialExpressionAdd,
        raw_samples[0],
        second_half,
        650,
        -570,
        "water combined raw-linear bumps",
    )
    normal_y = component_mask(
        material,
        normal_color,
        "g",
        850,
        -570,
        "water combined normal Y",
    )
    camera_position = material_expression(
        material,
        unreal.MaterialExpressionCameraPositionWS,
        -800,
        60,
    )
    camera_z = component_mask(
        material,
        camera_position,
        "b",
        -600,
        60,
        "water camera Z",
    )
    camera_divisor = constant_expression(material, 35000.0, -600, 150)
    camera_height = binary_expression(
        material,
        unreal.MaterialExpressionDivide,
        camera_z,
        camera_divisor,
        -380,
        60,
        "water CameraZ over 35000",
    )
    zero = constant_expression(material, 0.0, -380, 160)
    z_dis = binary_expression(
        material,
        unreal.MaterialExpressionMax,
        camera_height,
        zero,
        -160,
        60,
        "water max camera height",
    )
    height_scale = material_expression(
        material,
        unreal.MaterialExpressionLinearInterpolate,
        60,
        120,
    )
    connect_expression(near, "", height_scale, "A", "water lerp Near")
    connect_expression(far, "", height_scale, "B", "water lerp Far")
    connect_expression(
        z_dis,
        "",
        height_scale,
        "Alpha",
        "water lerp unsaturated zDis",
    )
    scaled_normal_y = binary_expression(
        material,
        unreal.MaterialExpressionMultiply,
        normal_y,
        height_scale,
        1040,
        -300,
        "water normal Y height scale",
    )
    spec_base = binary_expression(
        material,
        unreal.MaterialExpressionMax,
        scaled_normal_y,
        zero,
        1230,
        -300,
        "water spec max zero",
    )
    power = material_expression(
        material,
        unreal.MaterialExpressionPower,
        1420,
        -250,
    )
    connect_expression(
        spec_base,
        "",
        power,
        "Base",
        "water spec power base",
    )
    connect_expression(
        shininess,
        "",
        power,
        "Exp",
        "water spec shininess",
    )
    powered_max = binary_expression(
        material,
        unreal.MaterialExpressionMax,
        power,
        zero,
        1610,
        -220,
        "water powered spec max zero",
    )
    specular = binary_expression(
        material,
        unreal.MaterialExpressionMultiply,
        powered_max,
        gloss,
        1800,
        -140,
        "water specular gloss",
    )
    color = binary_expression(
        material,
        unreal.MaterialExpressionAdd,
        base_color,
        specular,
        2000,
        100,
        "water BaseColor plus specular",
    )
    color_saturate = material_expression(
        material,
        unreal.MaterialExpressionSaturate,
        2200,
        100,
    )
    connect_expression(
        color,
        "",
        color_saturate,
        "",
        "water saturate RGB",
    )
    vertex_color = material_expression(
        material,
        unreal.MaterialExpressionVertexColor,
        1450,
        500,
    )
    base_alpha = component_mask(
        material,
        base_color,
        "a",
        1450,
        650,
        "water BaseColor alpha",
    )
    vertex_alpha = component_mask(
        material,
        vertex_color,
        "a",
        1640,
        500,
        "water VertexAlpha",
    )
    opacity_product = binary_expression(
        material,
        unreal.MaterialExpressionMultiply,
        base_alpha,
        vertex_alpha,
        1840,
        560,
        "water BaseColor alpha times VertexAlpha",
    )
    opacity_saturate = material_expression(
        material,
        unreal.MaterialExpressionSaturate,
        2050,
        560,
    )
    connect_expression(
        opacity_product,
        "",
        opacity_saturate,
        "",
        "water saturate alpha",
    )
    set_expression_description(
        color_saturate,
        (
            "RokRiverWaterExact: dual local-XZ scroll; raw-linear bump "
            "samples; zDis=max(0,CameraZ/35000); "
            "height=lerp(Near,Far,zDis) without saturating zDis; "
            "spec=max(0,pow(max(0,NormalY*height),Shininess))*Gloss; "
            "RGB=saturate(BaseColor+spec)"
        ),
    )
    set_expression_description(
        opacity_saturate,
        (
            "RokRiverWaterExactOpacity: "
            "saturate(BaseColor.a*VertexColor.a)"
        ),
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        color_saturate,
        "",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity_saturate,
        "",
        unreal.MaterialProperty.MP_OPACITY,
    )
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    try:
        compile_errors = unreal.MaterialEditingLibrary.get_statistics(
            material
        ).get_editor_property("errors")
    except Exception:
        # UE 5.8 MaterialStatistics no longer exposes an 'errors' property;
        # recompile_material above already surfaces compile failures in the log.
        compile_errors = None
    if compile_errors:
        raise RuntimeError(
            f"River water material compile failed for {package_path}: "
            f"{compile_errors}"
        )
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material, package_path


ENTITY_SPRITE_MATERIAL_NAME = "M_RokWorldEntitySprite"


def create_entity_sprite_material():
    """Unlit translucent two-sided card material with a 'Sprite' texture
    parameter. World-map entity cards (formation units, far map icons,
    composite fallbacks) are 45-degree tilted planes using the same source
    orientation as the runtime city sprite actors; UBillboardComponent is
    an editor sprite helper gated by the BillboardSprites show flag and
    must not back gameplay entities.

    Blend mode MUST stay BLEND_TRANSLUCENT: Unity SpriteRenderer materials
    alpha-blend the source texture, and several unit textures carry
    intentional partial alpha (Monster_1000 idle frames are natively
    capped at 50% alpha, and soldier shield/head/leg pixels sit near 50%).
    A masked clip value turns those into moth-eaten silhouettes that do
    not match the Unity reference. The on-disk asset was created as
    translucent and must keep that mode on regeneration."""
    material_name = safe_name(ENTITY_SPRITE_MATERIAL_NAME)
    package_path = object_path(MATERIAL_DESTINATION, material_name)
    material = load_asset(package_path)
    if REUSE_EXISTING_ASSETS:
        if not isinstance(material, unreal.Material):
            raise RuntimeError(
                f"Reusable material is missing: {package_path}"
            )
        return material, package_path
    if not isinstance(material, unreal.Material):
        unreal.EditorAssetLibrary.make_directory(MATERIAL_DESTINATION)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            material_name,
            MATERIAL_DESTINATION,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"Failed to create material: {package_path}")
    clear_material_expressions(material)
    material.set_editor_property("two_sided", True)
    # Translucent (not masked): Unity SpriteRenderer semantics. Several
    # source sprites carry intentional partial alpha; masking them at 0.33
    # produces moth-eaten cards. Keep in sync with the on-disk asset.
    material.set_editor_property(
        "blend_mode",
        unreal.BlendMode.BLEND_TRANSLUCENT,
    )
    material.set_editor_property(
        "shading_model",
        unreal.MaterialShadingModel.MSM_UNLIT,
    )
    sampler = material_expression(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        -460,
        -40,
    )
    sampler.set_editor_property("parameter_name", "Sprite")
    sampler.set_editor_property(
        "sampler_type",
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        sampler,
        "RGB",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    )
    # Normal decoded crops retain their existing appearance at defaults.
    # Fade crops retain RAW source alpha; the fade shader doubles it
    # before multiplying instance alpha, then the blend factor saturates.
    alpha_multiplier = material_expression(material, unreal.MaterialExpressionScalarParameter, -460, 160)
    alpha_multiplier.set_editor_property("parameter_name", "SpriteAlphaMultiplier")
    alpha_multiplier.set_editor_property("default_value", 1.0)
    fade_amount = material_expression(material, unreal.MaterialExpressionScalarParameter, -460, 260)
    fade_amount.set_editor_property("parameter_name", "FadeAmount")
    fade_amount.set_editor_property("default_value", 1.0)
    decode_alpha = material_expression(material, unreal.MaterialExpressionMultiply, -220, 100)
    fade_alpha = material_expression(material, unreal.MaterialExpressionMultiply, -60, 100)
    opacity = material_expression(material, unreal.MaterialExpressionSaturate, 100, 100)
    unreal.MaterialEditingLibrary.connect_material_expressions(sampler, "A", decode_alpha, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(alpha_multiplier, "", decode_alpha, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(decode_alpha, "", fade_alpha, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(fade_amount, "", fade_alpha, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(fade_alpha, "", opacity, "")
    unreal.MaterialEditingLibrary.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    # SpriteAlphaMultiplier is 1 for normal decoded crops, 2 for raw fade
    # crops. Decode RGB AFTER sampling, preserving bilinear edge colors.
    fade_mode = material_expression(material, unreal.MaterialExpressionSubtract, -220, 350)
    fade_mode.set_editor_property("const_b", 1.0)
    rgb_alpha = material_expression(material, unreal.MaterialExpressionSaturate, -60, 350)
    rgb_factor = material_expression(material, unreal.MaterialExpressionLinearInterpolate, 100, 350)
    rgb_factor.set_editor_property("const_a", 1.0)
    rgb_output = material_expression(material, unreal.MaterialExpressionMultiply, 270, 300)
    unreal.MaterialEditingLibrary.connect_material_expressions(alpha_multiplier, "", fade_mode, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(decode_alpha, "", rgb_alpha, "")
    unreal.MaterialEditingLibrary.connect_material_expressions(rgb_alpha, "", rgb_factor, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(fade_mode, "", rgb_factor, "Alpha")
    unreal.MaterialEditingLibrary.connect_material_expressions(sampler, "RGB", rgb_output, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(rgb_factor, "", rgb_output, "B")
    unreal.MaterialEditingLibrary.connect_material_property(rgb_output, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    compile_errors = unreal.MaterialEditingLibrary.recompile_material(
        material
    )
    if compile_errors:
        raise RuntimeError(
            f"Entity sprite material compile failed: {compile_errors}"
        )
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material, package_path


def create_solid_unlit_material(
    material_name,
    parameter_name,
    color_record,
):
    material_name = safe_name(material_name)
    package_path = object_path(MATERIAL_DESTINATION, material_name)
    material = load_asset(package_path)
    if REUSE_EXISTING_ASSETS:
        if not isinstance(material, unreal.Material):
            raise RuntimeError(
                f"Reusable material is missing: {package_path}"
            )
        return material, package_path
    if not isinstance(material, unreal.Material):
        unreal.EditorAssetLibrary.make_directory(MATERIAL_DESTINATION)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            material_name,
            MATERIAL_DESTINATION,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"Failed to create material: {package_path}")

    clear_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property(
        "blend_mode",
        unreal.BlendMode.BLEND_OPAQUE,
    )
    material.set_editor_property(
        "shading_model",
        unreal.MaterialShadingModel.MSM_UNLIT,
    )
    color_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        -240,
        -20,
    )
    color_node.set_editor_property("parameter_name", parameter_name)
    color_node.set_editor_property(
        "default_value",
        color_record_to_linear(color_record),
    )
    if not unreal.MaterialEditingLibrary.connect_material_property(
        color_node,
        "",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    ):
        raise RuntimeError(
            f"Failed to connect solid background material {package_path}"
        )
    compile_errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if compile_errors:
        raise RuntimeError(
            f"Material compile failed for {package_path}: {compile_errors}"
        )
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material, package_path


def build_material_assets(asset_manifest, world_manifest):
    lighting_phase = prototype_lighting_phase(world_manifest)
    sprite_color = lighting_phase["sprite_color"]
    ground_materials = {}
    material_asset_paths = []
    texture_asset_paths = []
    texture_cache = {}

    for record in asset_manifest["baked_ground_materials"]:
        prefab_id = record["prefab_id"]
        source_path = PROJECT_ROOT / record["output"]["path"]
        texture, texture_path = import_texture(
            source_path,
            "T_WorldGround_" + safe_name(prefab_id),
            clamp=True,
        )
        material, material_path = create_textured_material(
            texture,
            "M_WorldGround_" + safe_name(prefab_id),
            masked=False,
            translucent=True,
            unlit=True,
            sprite_color=sprite_color,
        )
        ground_materials[prefab_id] = material
        texture_asset_paths.append(texture_path)
        material_asset_paths.append(material_path)

    entity_sprite_material, entity_sprite_material_path = (
        create_entity_sprite_material()
    )
    material_asset_paths.append(entity_sprite_material_path)

    source_materials = {}
    for family in ("mountain_materials", "tree_materials"):
        is_tree = family == "tree_materials"
        for record in asset_manifest[family]:
            source_texture = PROJECT_ROOT / record["source_texture"]["path"]
            source_material = record["source_material"]["path"]
            texture_key = str(source_texture).lower()
            if texture_key not in texture_cache:
                texture_cache[texture_key] = import_texture(
                    source_texture,
                    "T_WorldSource_"
                    + safe_name(source_texture.stem)
                    + "_"
                    + sha256(source_texture)[:8],
                    no_mipmaps=is_tree,
                    clamp=is_tree,
                    max_texture_size=1024 if is_tree else 0,
                )
            texture, texture_path = texture_cache[texture_key]
            material, material_path = create_textured_material(
                texture,
                "M_WorldSource_" + safe_name(Path(source_material).stem),
                masked=False,
                translucent=is_tree,
                # Unity's z_cm_slg_mountain shader is opaque but samples only
                # MainTex * _SpriteColor. It does not evaluate lighting.
                unlit=True,
                sprite_color=sprite_color,
            )
            source_materials[source_material] = material
            texture_asset_paths.append(texture_path)
            material_asset_paths.append(material_path)

    waterfall_records = [
        record
        for record in asset_manifest["object_sources"].values()
        if record.get("representation") == WATERFALL_REPRESENTATION
    ]
    waterfall_material_paths = []
    for waterfall_record in waterfall_records:
        contract = waterfall_record.get("waterfall_geometry_contract", {})
        for ue_name, mat_record in sorted(
            contract.get("material_records", {}).items()
        ):
            render_spec = mat_record.get("ue_render", {})
            texture_source = PROJECT_ROOT / (
                mat_record["source_texture"]["path"]
            )
            texture, texture_path = import_texture(
                texture_source,
                "T_WorldSource_"
                + safe_name(texture_source.stem)
                + "_"
                + sha256(texture_source)[:8],
            )
            blend_mode = render_spec.get("blend_mode")
            material, material_path = create_textured_material(
                texture,
                ue_name,
                masked=False,
                translucent=False,
                unlit=render_spec.get("shading_model") == "unlit",
                sprite_color=sprite_color,
                additive=blend_mode == "additive",
            )
            texture_asset_paths.append(texture_path)
            material_asset_paths.append(material_path)
            waterfall_material_paths.append(material_path)

    river_records = [
        (prefab_id, record)
        for prefab_id, record in asset_manifest["object_sources"].items()
        if record.get("representation") == RIVER_REPRESENTATION
    ]
    river_materials = {}
    river_texture_paths = {}
    if river_records:
        first_prefab_id, first_record = river_records[0]
        first_layers = first_record["river_geometry_contract"][
            "render_layers"
        ]
        if [layer.get("role") for layer in first_layers] != [
            "edge",
            "water",
        ]:
            raise RuntimeError(
                f"{first_prefab_id} has invalid river material layers"
            )
        layer_by_role = {
            layer["role"]: layer
            for layer in first_layers
        }
        for prefab_id, record in river_records[1:]:
            comparison_layers = {
                layer["role"]: layer
                for layer in record["river_geometry_contract"][
                    "render_layers"
                ]
            }
            for role in ("edge", "water"):
                for field in (
                    "source_material",
                    "source_shader",
                    "shader_contract",
                    "textures",
                ):
                    if comparison_layers[role].get(field) != (
                        layer_by_role[role].get(field)
                    ):
                        raise RuntimeError(
                            f"{prefab_id} river {role} {field} differs "
                            "from the shared source contract"
                        )
        edge_texture_source = (
            PROJECT_ROOT
            / layer_by_role["edge"]["textures"]["_MainTex"][
                "source_texture"
            ]["path"]
        )
        if sha256(edge_texture_source) != (
            layer_by_role["edge"]["textures"]["_MainTex"][
                "source_texture"
            ]["sha256"]
        ):
            raise RuntimeError(
                "River edge texture changed after asset preparation"
            )
        edge_texture, edge_texture_path = import_texture(
            edge_texture_source,
            RIVER_EDGE_TEXTURE_NAME,
            srgb=True,
            repeat=True,
        )
        water_texture_source = (
            PROJECT_ROOT
            / layer_by_role["water"]["textures"]["_BumpMap"][
                "source_texture"
            ]["path"]
        )
        if sha256(water_texture_source) != (
            layer_by_role["water"]["textures"]["_BumpMap"][
                "source_texture"
            ]["sha256"]
        ):
            raise RuntimeError(
                "River water bump texture changed after asset preparation"
            )
        water_texture, water_texture_path = import_texture(
            water_texture_source,
            RIVER_WATER_TEXTURE_NAME,
            srgb=False,
            repeat=True,
        )
        light_direction = source_light_direction_ue(world_manifest)
        edge_material, edge_material_path = create_river_edge_material(
            edge_texture,
            lighting_phase,
            light_direction,
        )
        water_contract = layer_by_role["water"]["shader_contract"]
        edge_material_contract = layer_by_role["edge"]["shader_contract"]
        if (
            edge_material_contract.get("opacity")
            != "interpolated vertex color alpha"
            or water_contract.get("opacity")
            != "saturate(BaseColor.a * vertex color alpha)"
        ):
            raise RuntimeError(
                "River shader contract changed its vertex-alpha semantics"
            )
        water_material, water_material_path = (
            create_river_water_material(
                water_texture,
                water_contract,
            )
        )
        river_materials = {
            "edge": edge_material,
            "water": water_material,
        }
        river_texture_paths = {
            "edge": edge_texture_path,
            "water": water_texture_path,
        }
        texture_asset_paths.extend(
            [edge_texture_path, water_texture_path]
        )
        material_asset_paths.extend(
            [edge_material_path, water_material_path]
        )

    city_record = asset_manifest.get("player_city", {})
    city_baked_record = city_record.get("baked_texture", {})
    city_sprite_source = PROJECT_ROOT / city_baked_record.get("path", "")
    if not city_sprite_source.exists():
        raise RuntimeError(
            "Missing audited Unity world-map player-city texture; run "
            "Scripts/PrepareRokUnityWorldMapAssets.py first"
        )
    if sha256(city_sprite_source) != city_baked_record.get("sha256"):
        raise RuntimeError(
            "Unity world-map player-city texture changed after asset preparation"
        )
    city_texture, city_texture_path = import_texture(
        city_sprite_source,
        "T_WorldPlayerCity_build_1_101_1",
        no_mipmaps=True,
        clamp=True,
        max_texture_size=2048,
    )
    city_material, city_material_path = create_textured_material(
        city_texture,
        "M_WorldPlayerCity_build_1_101_1",
        masked=False,
        translucent=True,
        unlit=True,
        sprite_color=sprite_color,
    )
    texture_asset_paths.append(city_texture_path)
    material_asset_paths.append(city_material_path)
    background_material, background_material_path = (
        create_solid_unlit_material(
            BACKGROUND_MATERIAL_NAME,
            BACKGROUND_COLOR_PARAMETER,
            lighting_phase["camera_background_color"],
        )
    )
    material_asset_paths.append(background_material_path)
    return {
        "ground": ground_materials,
        "source": source_materials,
        "river": river_materials,
        "river_texture_paths": river_texture_paths,
        "city": city_material,
        "background": background_material,
        "lighting_phase": lighting_phase,
        "city_texture_path": city_texture_path,
        "city_material_path": city_material_path,
        "texture_asset_paths": sorted(set(texture_asset_paths)),
        "material_asset_paths": sorted(set(material_asset_paths)),
    }


WATERFALL_REPRESENTATION = "exact_source_fbx_composite_waterfall"


def import_waterfall_layer_meshes(asset_manifest):
    records = [
        record
        for record in asset_manifest["object_sources"].values()
        if record.get("representation") == WATERFALL_REPRESENTATION
    ]
    if not records:
        return {}, []
    if len(records) != 1:
        raise RuntimeError("Only one waterfall composite source is audited")
    record = records[0]
    contract = record.get("waterfall_geometry_contract", {})
    if contract.get("policy") != "split_fbx_exact_static_parts":
        raise RuntimeError("Waterfall composite lacks its static-part contract")
    needed = {}
    source_fbx = None
    for part in contract.get("static_parts", []):
        node = part["mesh_node"]
        if node in ("build_mountain_G_edge", "build_mountain_G_water"):
            expected_path = part["ue_mesh"]
            if not expected_path.startswith(
                WATERFALL_LAYER_MESH_DESTINATION + "/"
            ):
                raise RuntimeError(
                    f"Waterfall layer path is not in the audited destination: "
                    f"{expected_path}"
                )
            needed[node] = expected_path
            source_fbx = part["mesh_guid"]
    if set(needed) != {"build_mountain_G_edge", "build_mountain_G_water"}:
        raise RuntimeError(
            f"Waterfall layer nodes are not edge+water: {sorted(needed)}"
        )
    source_path = (
        PROJECT_ROOT
        / "ref/client-unity/Assets/BundleAssets/land/Fbx/build_mountain_G.FBX"
    )
    if not source_path.exists():
        raise RuntimeError(f"Missing waterfall source mesh: {source_path}")

    if REUSE_EXISTING_ASSETS:
        meshes = {node: load_asset(path) for node, path in needed.items()}
        missing = [
            path
            for node, path in needed.items()
            if not isinstance(meshes[node], unreal.StaticMesh)
        ]
        if missing:
            raise RuntimeError(
                "Reusable waterfall layer meshes are missing: "
                + ", ".join(missing)
            )
        return meshes, sorted(needed.values())

    unreal.EditorAssetLibrary.make_directory(WATERFALL_LAYER_MESH_DESTINATION)
    for path in needed.values():
        package = path.split(".", 1)[0]
        if unreal.EditorAssetLibrary.does_asset_exist(package):
            if not unreal.EditorAssetLibrary.delete_asset(package):
                raise RuntimeError(
                    f"Failed to replace generator-owned waterfall mesh: {path}"
                )
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_path))
    task.set_editor_property(
        "destination_path", WATERFALL_LAYER_MESH_DESTINATION
    )
    task.set_editor_property(
        "destination_name", "WaterfallImport_build_mountain_G"
    )
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    task.set_editor_property(
        "options",
        configure_static_mesh_import(source_path, combine_meshes=False),
    )
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported_paths = sorted(
        set(task.get_editor_property("imported_object_paths"))
    )
    by_node = {}
    for path in imported_paths:
        asset = load_asset(path)
        if not isinstance(asset, unreal.StaticMesh):
            continue
        node_name = safe_name(asset.get_name())
        if node_name in by_node:
            raise RuntimeError(
                f"Waterfall split import duplicated node {node_name}"
            )
        by_node[node_name] = (path, asset)
    missing_nodes = set(needed) - set(by_node)
    if missing_nodes:
        raise RuntimeError(
            f"Waterfall split import lacks nodes {sorted(missing_nodes)}, "
            f"got {sorted(by_node)}"
        )
    meshes = {}
    for node, expected_path in needed.items():
        raw_path, mesh = by_node[node]
        if raw_path != expected_path:
            if not unreal.EditorAssetLibrary.rename_asset(
                raw_path.split(".", 1)[0],
                expected_path.split(".", 1)[0],
            ):
                raise RuntimeError(
                    f"Failed to rename waterfall layer {raw_path} to "
                    f"{expected_path}"
                )
        meshes[node] = load_asset(expected_path)
    for node, (raw_path, _) in by_node.items():
        if node in needed:
            continue
        package = raw_path.split(".", 1)[0]
        if unreal.EditorAssetLibrary.does_asset_exist(package):
            unreal.EditorAssetLibrary.delete_asset(package)
    return meshes, sorted(needed.values())


def build_mesh_assets(asset_manifest):
    plane_mesh, plane_path = import_static_mesh(
        PLANE_SOURCE,
        "SM_RokWorld_Plane60",
    )
    mesh_by_prefab = {}
    mesh_asset_paths = [plane_path]
    mountain_body_imports = {}
    river_layer_imports = {}
    for prefab_id, record in asset_manifest["object_sources"].items():
        representation = record["representation"]
        if representation == "empty_source_prefab":
            mesh_by_prefab[prefab_id] = None
            continue
        if representation == WATERFALL_REPRESENTATION:
            # Static parts bind existing river/mountain meshes plus the
            # dedicated waterfall layer meshes imported below; the near
            # window never places this prefab, so no direct mesh is stored.
            mesh_by_prefab[prefab_id] = None
            continue
        if representation == "exact_source_fbx":
            source_path = PROJECT_ROOT / record["source_mesh"]["path"]
            mesh, mesh_path, imported_paths = import_mountain_body_mesh(
                source_path,
                record,
            )
            mesh_by_prefab[prefab_id] = mesh
            mesh_asset_paths.extend(imported_paths)
            mountain_body_imports[prefab_id] = {
                "source_fbx": record["source_mesh"],
                "geometry_contract": record["mountain_geometry_contract"],
                "selected_body_asset": mesh_path,
                "imported_split_assets": imported_paths,
            }
            continue
        if representation == RIVER_REPRESENTATION:
            source_path = PROJECT_ROOT / record["source_mesh"]["path"]
            layer_meshes, layer_paths, deleted_collision_paths = (
                import_river_layer_meshes(
                    source_path,
                    prefab_id,
                    record,
                )
            )
            mesh_by_prefab[prefab_id] = layer_meshes
            mesh_asset_paths.extend(layer_paths)
            river_layer_imports[prefab_id] = {
                "source_fbx": record["source_mesh"],
                "geometry_contract": record["river_geometry_contract"],
                "selected_layer_assets": {
                    role: layer_meshes[role].get_path_name()
                    for role in ("edge", "water")
                },
                "imported_render_layer_assets": layer_paths,
                "deleted_collision_node_assets": (
                    deleted_collision_paths
                ),
                "vertex_color_import_option": "Replace",
            }
            continue
        elif representation == "exact_unity_mesh_export":
            source_path = PROJECT_ROOT / record["exported_obj"]["path"]
        else:
            raise RuntimeError(
                f"Unsupported object representation for {prefab_id}: "
                f"{representation}"
            )
        mesh, mesh_path = import_static_mesh(
            source_path,
            record["ue_mesh_name"],
        )
        mesh_by_prefab[prefab_id] = mesh
        mesh_asset_paths.append(mesh_path)
    waterfall_meshes, waterfall_paths = import_waterfall_layer_meshes(
        asset_manifest
    )
    mesh_asset_paths.extend(waterfall_paths)
    return (
        plane_mesh,
        mesh_by_prefab,
        sorted(set(mesh_asset_paths)),
        mountain_body_imports,
        river_layer_imports,
        waterfall_meshes,
    )


def configure_component(
    component,
    selectable=False,
    cast_shadow=False,
    translucency_sort_priority=0,
    lighting_relevant=True,
):
    component.modify()
    component.set_collision_profile_name(
        "Custom" if selectable else "NoCollision"
    )
    component.set_collision_enabled(
        unreal.CollisionEnabled.QUERY_ONLY
        if selectable
        else unreal.CollisionEnabled.NO_COLLISION
    )
    if selectable:
        component.set_collision_response_to_all_channels(
            unreal.CollisionResponseType.ECR_IGNORE
        )
        component.set_collision_response_to_channel(
            unreal.CollisionChannel.ECC_VISIBILITY,
            unreal.CollisionResponseType.ECR_BLOCK,
        )
    component.set_editor_property("cast_shadow", cast_shadow)
    component.set_editor_property(
        "translucency_sort_priority",
        int(translucency_sort_priority),
    )
    if not lighting_relevant:
        for property_name in (
            "affect_distance_field_lighting",
            "affect_dynamic_indirect_lighting",
        ):
            try:
                component.set_editor_property(property_name, False)
            except Exception:
                unreal.log_warning(
                    f"Could not set {component.get_name()}.{property_name}"
                )
    for property_name, value in (
        ("receives_decals", False),
        ("use_as_occluder", False),
        ("can_ever_affect_navigation", False),
    ):
        try:
            component.set_editor_property(property_name, value)
        except Exception:
            unreal.log_warning(
                f"Could not set {component.get_name()}.{property_name}"
            )
    expected_collision = (
        unreal.CollisionEnabled.QUERY_ONLY
        if selectable
        else unreal.CollisionEnabled.NO_COLLISION
    )
    if component.get_collision_enabled() != expected_collision:
        raise RuntimeError(
            f"{component.get_name()} collision did not persist during "
            f"generation: observed={component.get_collision_enabled()} "
            f"expected={expected_collision}"
        )


def spawn_mesh_actor(
    label,
    mesh,
    material,
    location,
    rotation,
    scale,
    folder,
    tags,
    selectable=False,
    cast_shadow=False,
    translucency_sort_priority=0,
    lighting_relevant=True,
    require_single_material_slot=False,
):
    actor_rotation = (
        rotation
        if hasattr(rotation, "pitch")
        else make_rotator(
            rotation.get("pitch", 0.0),
            rotation.get("yaw", 0.0),
            rotation.get("roll", 0.0),
        )
    )
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(location["x"], location["y"], location["z"]),
        actor_rotation,
    )
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_actor_scale3d(
        unreal.Vector(scale["x"], scale["y"], scale["z"])
    )
    actor.tags = tags
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        raise RuntimeError(f"Spawned actor has no StaticMeshComponent: {label}")
    component.set_static_mesh(mesh)
    material_slot_count = max(component.get_num_materials(), 1)
    if require_single_material_slot and material_slot_count != 1:
        raise RuntimeError(
            f"{label} body mesh has {material_slot_count} material slots; "
            "only the exact FBX body node may be rendered"
        )
    for material_slot_index in range(material_slot_count):
        component.set_material(material_slot_index, material)
    configure_component(
        component,
        selectable,
        cast_shadow,
        translucency_sort_priority,
        lighting_relevant,
    )
    return actor


def object_material(prefab_id, record, material_assets):
    if record["representation"] == "exact_unity_mesh_export":
        source_material = record["source_material"]
        material = material_assets["source"].get(source_material)
        if not material:
            raise RuntimeError(
                f"Missing UE atlas material for {prefab_id}: {source_material}"
            )
        return material
    material_source = (
        "ref/client-unity/Assets/BundleAssets/land/Material/"
        + (
            "build_mountain_F.mat"
            if prefab_id == "build_mountain_F"
            else "build_mountain_A.mat"
        )
    )
    material = material_assets["source"].get(material_source)
    if not material:
        raise RuntimeError(
            f"Missing UE mountain material for {prefab_id}: {material_source}"
        )
    return material


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
        raise RuntimeError("River layer has a zero rotation quaternion")
    x, y, z, w = (
        x / length,
        y / length,
        z / length,
        w / length,
    )
    sin_pitch = 2.0 * (w * y - z * x)
    sin_pitch = max(-1.0, min(1.0, sin_pitch))
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

    # The imported split mesh keeps each FBX node pivot. Compose the source
    # child TRS under the existing Unity-to-UE instance transform. The local
    # translation is converted to source centimeters before parent scale.
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
    instance_quaternion = (
        0.0,
        0.0,
        math.sin(yaw_radians * 0.5),
        math.cos(yaw_radians * 0.5),
    )
    local_ue_quaternion = (
        -float(local_rotation["x"]),
        float(local_rotation["z"]),
        float(local_rotation["y"]),
        float(local_rotation["w"]),
    )
    composed_rotation = quaternion_multiply(
        instance_quaternion,
        local_ue_quaternion,
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


def spawn_manifest_world(
    world_manifest,
    asset_manifest,
    plane_mesh,
    mesh_by_prefab,
    material_assets,
):
    terrain_count = 0
    rendered_object_count = 0
    omitted_empty_count = 0
    river_logical_actor_count = 0
    river_physical_actor_count = 0
    for tile in world_manifest["prototype_window"]["tiles"]:
        for instance in tile["instances"]:
            prefab_id = instance["prefab_id"]
            if instance["kind"] == "plane":
                label = (
                    f"RokWorldGround_Tile{tile['x']}_{tile['y']}_"
                    f"Plane{instance['template_index']:02d}"
                )
                spawn_mesh_actor(
                    label,
                    plane_mesh,
                    material_assets["ground"][prefab_id],
                    instance["ue_relative_to_focus"],
                    {"yaw": instance["ue_yaw"]},
                    instance["ue_actor_scale_for_centimeter_asset"],
                    "RokUnityWorldMap/Terrain",
                    [
                        "RokWorldMapGenerated",
                        "RokWorldMapTerrain",
                        f"UnityTile_{tile['x']}_{tile['y']}",
                        f"UnityPlaneIndex_{instance['template_index']}",
                        "UnityTransformPolicy_PositionRotationScale",
                    ],
                    selectable=False,
                    cast_shadow=False,
                    translucency_sort_priority=(
                        GROUND_TRANSLUCENCY_SORT_PRIORITY
                    ),
                )
                terrain_count += 1
                continue

            source_record = asset_manifest["object_sources"][prefab_id]
            mesh = mesh_by_prefab[prefab_id]
            if mesh is None:
                omitted_empty_count += 1
                continue
            label = (
                f"RokWorldObject_Tile{tile['x']}_{tile['y']}_"
                f"Object{instance['template_index']:02d}_"
                f"{safe_name(prefab_id)}"
            )
            transform_policy = instance["unity_runtime_transform_policy"]
            policy_tag = (
                "UnityTransformPolicy_PositionOnly"
                if transform_policy == "position_only"
                else "UnityTransformPolicy_PositionRotationScale"
            )
            if source_record["representation"] == RIVER_REPRESENTATION:
                layer_by_role = {
                    layer["role"]: layer
                    for layer in source_record["river_geometry_contract"][
                        "render_layers"
                    ]
                }
                common_tags = [
                    "RokWorldMapGenerated",
                    "RokWorldMapExactSource",
                    "RokWorldMapRiver",
                    f"UnityTile_{tile['x']}_{tile['y']}",
                    f"UnityObjectIndex_{instance['template_index']}",
                    f"UnityPrefab_{safe_name(prefab_id)}",
                    policy_tag,
                ]
                for role in ("edge", "water"):
                    layer = layer_by_role[role]
                    composed = compose_river_layer_transform(
                        instance,
                        layer,
                    )
                    is_edge = role == "edge"
                    layer_label = (
                        label if is_edge else label + "_LayerWater"
                    )
                    layer_tags = list(common_tags)
                    if is_edge:
                        layer_tags.extend(
                            [
                                "RokWorldMapSourceObject",
                                "RokWorldMapRiverEdge",
                                "RokWorldMapRiverLogicalObject",
                            ]
                        )
                    else:
                        layer_tags.extend(
                            [
                                "RokWorldMapRiverRenderLayer",
                                "RokWorldMapRiverWater",
                            ]
                        )
                    spawn_mesh_actor(
                        layer_label,
                        mesh[role],
                        material_assets["river"][role],
                        composed["location"],
                        composed["rotation"],
                        composed["scale"],
                        RIVER_FOLDER,
                        layer_tags,
                        selectable=False,
                        cast_shadow=False,
                        translucency_sort_priority=int(
                            layer["sort_priority"]
                        ),
                        lighting_relevant=False,
                        require_single_material_slot=True,
                    )
                    river_physical_actor_count += 1
                river_logical_actor_count += 1
                rendered_object_count += 1
                continue
            source_material_name = Path(
                source_record.get("source_material", "")
            ).name
            is_tree_source = (
                source_material_name in TREE_SOURCE_MATERIAL_NAMES
            )
            is_mountain_source = (
                source_record["representation"] == "exact_source_fbx"
            )
            actor_tags = [
                "RokWorldMapGenerated",
                "RokWorldMapSourceObject",
                "RokWorldMapExactSource",
                f"UnityTile_{tile['x']}_{tile['y']}",
                f"UnityObjectIndex_{instance['template_index']}",
                f"UnityPrefab_{safe_name(prefab_id)}",
                policy_tag,
            ]
            if is_tree_source:
                actor_tags.append("RokWorldMapTree")
            if is_mountain_source:
                actor_tags.append("RokWorldMapMountain")
            spawn_mesh_actor(
                label,
                mesh,
                object_material(prefab_id, source_record, material_assets),
                instance["ue_relative_to_focus"],
                {"yaw": instance["ue_yaw"]},
                instance["ue_actor_scale_for_centimeter_asset"],
                "RokUnityWorldMap/SourceObjects",
                actor_tags,
                selectable=False,
                # The Unity mountain body is unlit. UE shadows create a
                # source-incompatible light interaction that visibly shimmers
                # while the tactical camera zooms.
                cast_shadow=False,
                translucency_sort_priority=(
                    6 if is_tree_source else 0
                ),
                lighting_relevant=not is_mountain_source,
                require_single_material_slot=is_mountain_source,
            )
            rendered_object_count += 1
    return (
        terrain_count,
        rendered_object_count,
        omitted_empty_count,
        river_logical_actor_count,
        river_physical_actor_count,
    )


def spawn_player_city(plane_mesh, city_material, city_record):
    city_class = unreal.load_class(None, CITY_ACTOR_CLASS)
    if not city_class:
        raise RuntimeError(
            f"World-map city actor class is unavailable: {CITY_ACTOR_CLASS}"
        )

    identity = city_record["identity"]
    presentation = city_record["ue_presentation"]
    lod_contract = city_record["lod_contract"]
    shader_contract = city_record["shader_contract"]
    logical_root = presentation["logical_world_root_cm"]
    base_scale = presentation["base_plane_scale"]
    selection_anchor = presentation["selection_anchor_offset_cm"]
    visual_center_offset = presentation["base_visual_center_offset_cm"]

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        city_class,
        unreal.Vector(*logical_root),
        unreal.Rotator(),
    )
    if not actor:
        raise RuntimeError("Failed to spawn source-correct world-map city actor")
    actor.set_actor_label("RokWorldPlayerCity_1146_157")
    actor.set_folder_path("RokUnityWorldMap/Gameplay")
    actor.tags = [
        "RokWorldMapGenerated",
        "RokWorldMapSelectable",
        "RokWorldMapPlayerCity",
        "RokWorldMapExactSource",
        "UnityDisplayX_1146",
        "UnityDisplayY_157",
        f"UnityInitId_{identity['init_id']}",
        f"UnityBuildingType_{identity['building_type']}",
        f"UnityBuildingLevel_{identity['level']}",
        f"UnityGridX_{identity['grid_x']}",
        f"UnityGridY_{identity['grid_y']}",
        f"UnityFootprint_{identity['grid_width']}x{identity['grid_length']}",
        f"UnityModel_{identity['model_id']}",
        "UnityPivot_SourceAuthored",
        "UnityLod_LevelDetailCastle",
    ]
    actor.configure_source_visual(
        plane_mesh,
        city_material,
        source_sprite_plane_rotation(),
        unreal.Vector(*base_scale),
        unreal.Vector(*selection_anchor),
        unreal.Vector(*visual_center_offset),
        float(lod_contract["initial_dxf"]),
        float(lod_contract["age_scale_cap"]),
        int(shader_contract["material_render_queue"]),
    )
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component or component.get_editor_property("static_mesh") != plane_mesh:
        raise RuntimeError(
            "World-map city actor did not accept the source Plane60 mesh"
        )
    return actor


def spawn_solid_background_underlay(
    world_manifest,
    plane_mesh,
    background_material,
):
    map_record = world_manifest["map"]
    focus = world_manifest["prototype_window"]["focus"]["unity_coordinate"]
    ue_scale = float(
        world_manifest["coordinate_contract"]["ue_units_per_unity_unit"]
    )
    center_unity_x = float(map_record["world_width_unity_units"]) * 0.5
    center_unity_y = float(map_record["world_height_unity_units"]) * 0.5
    plane_source_size_cm = 60.0 * 100.0
    coverage_cm = (
        max(
            float(map_record["world_width_unity_units"]),
            float(map_record["world_height_unity_units"]),
        )
        * ue_scale
        * 4.0
    )
    background_scale = coverage_cm / plane_source_size_cm
    return spawn_mesh_actor(
        BACKGROUND_ACTOR_LABEL,
        plane_mesh,
        background_material,
        {
            "x": -(center_unity_x - float(focus["x"])) * ue_scale,
            "y": (center_unity_y - float(focus["y"])) * ue_scale,
            "z": -10000.0,
        },
        {"yaw": 0.0},
        {
            "x": background_scale,
            "y": background_scale,
            "z": background_scale,
        },
        "RokUnityWorldMap/Environment",
        [
            "RokWorldMapGenerated",
            "RokWorldMapBackground",
            "UnityCameraClear_SolidColor",
            "UnityLightManager_CameraFill",
        ],
        selectable=False,
        cast_shadow=False,
        translucency_sort_priority=(
            BACKGROUND_TRANSLUCENCY_SORT_PRIORITY
        ),
        lighting_relevant=False,
        require_single_material_slot=True,
    )


def set_light_property(actor, property_name, value):
    component = actor.get_component_by_class(unreal.LightComponent)
    if component:
        try:
            component.set_editor_property(property_name, value)
        except Exception:
            pass


def setup_lighting(world_manifest):
    environment = world_manifest["environment"]
    source_light = environment["directional_light"]
    source_euler = source_light["unity_local_euler_degrees"]
    source_color = source_light["color"]
    sun = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 45000.0),
        make_rotator(
            -float(source_euler["x"]),
            90.0 + float(source_euler["y"]),
            -float(source_euler["z"]),
        ),
    )
    sun.set_actor_label("RokWorldMap_DirectionalLight")
    sun.set_folder_path("RokUnityWorldMap/Lighting")
    sun.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
    sun.tags = ["RokWorldMapGenerated", "RokWorldMapLighting"]
    set_light_property(sun, "intensity", float(source_light["intensity"]))
    set_light_property(
        sun,
        "light_color",
        unreal.Color(
            r=round(float(source_color["r"]) * 255.0),
            g=round(float(source_color["g"]) * 255.0),
            b=round(float(source_color["b"]) * 255.0),
            a=round(float(source_color["a"]) * 255.0),
        ),
    )
    component = sun.get_component_by_class(unreal.LightComponent)
    if component:
        try:
            component.set_editor_property(
                "mobility",
                unreal.ComponentMobility.MOVABLE,
            )
        except Exception:
            unreal.log_warning(
                "Could not make source Directional Light movable"
            )
    return sun


def configure_runtime():
    config_class = unreal.load_class(None, CONFIG_ACTOR_CLASS)
    if not config_class:
        raise RuntimeError(
            f"World-map runtime class is unavailable: {CONFIG_ACTOR_CLASS}. "
            "Build RokEditor first."
        )
    config = unreal.EditorLevelLibrary.spawn_actor_from_class(
        config_class,
        unreal.Vector(),
        unreal.Rotator(),
    )
    config.set_actor_label("RokWorldMap_SourceConfig")
    config.set_folder_path("RokUnityWorldMap/Runtime")
    config.tags = [
        "RokWorldMapGenerated",
        "RokWorldMapConfig",
        "UnityMapBounds_0_7200",
        "UnityFocus_6876_942",
        "UnityCamera_map_tactical",
    ]
    config.set_editor_property("enter_player_city_on_zoom", True)
    config.set_editor_property(
        "city_entry_threshold_camera_key_id",
        "city_bound",
    )
    config.set_editor_property(
        "city_entry_target_camera_key_id",
        "city_default",
    )
    config.set_editor_property("city_entry_transition_seconds", 0.5)

    player_start = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart,
        unreal.Vector(0.0, -18000.0, 18000.0),
        make_rotator(-45.0, 90.0, 0.0),
    )
    player_start.set_actor_label("PlayerStart_RokUnityWorldMap")
    player_start.set_folder_path("RokUnityWorldMap/Runtime")
    player_start.tags = ["RokWorldMapGenerated", "RokWorldMapPlayerStart"]

    game_mode_class = unreal.load_class(None, GAME_MODE_CLASS)
    if not game_mode_class:
        raise RuntimeError(f"World-map GameMode is unavailable: {GAME_MODE_CLASS}")
    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property(
        "default_game_mode",
        game_mode_class,
    )
    return config, player_start


def is_target_level_loaded():
    world = unreal.EditorLevelLibrary.get_editor_world()
    return bool(
        world
        and str(world.get_outermost().get_name()) == TARGET_LEVEL
    )


def ensure_target_level():
    if is_target_level_loaded():
        return
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    elif not unreal.EditorLevelLibrary.new_level(TARGET_LEVEL):
        raise RuntimeError(f"Failed to create target level: {TARGET_LEVEL}")


def delete_stale_generated_city_assets():
    if REUSE_EXISTING_ASSETS:
        return
    for asset_path in STALE_CITY_ASSETS:
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            if not unreal.EditorAssetLibrary.delete_asset(asset_path):
                raise RuntimeError(
                    f"Failed to delete stale generated city asset: {asset_path}"
                )


def guard_migrated_mountain_f():
    expected_project = Path("D:/ueDemo/Rok").resolve()
    if PROJECT_ROOT.resolve() != expected_project:
        raise RuntimeError(
            f"World-map generator project mismatch: expected {expected_project}, "
            f"got {PROJECT_ROOT.resolve()}"
        )

    decoded_texture_file = (
        PROJECT_ROOT
        / "Content"
        / "RokUnityWorldMapPrototype"
        / "Textures"
        / "T_MountainF_UnityDecodedMips.uasset"
    )
    decoded_texture_exists = decoded_texture_file.is_file() or (
        unreal.EditorAssetLibrary.does_asset_exist(MOUNTAIN_F_DECODED_TEXTURE)
    )
    if decoded_texture_exists:
        raise RuntimeError(
            "Full world-map generation is blocked: the migrated Mountain F "
            f"decoded texture exists at {MOUNTAIN_F_DECODED_TEXTURE}. "
            "Use the dedicated Mountain F rebuild path. "
            "-RokWorldMapReuseExistingAssets still rebuilds and saves the map."
        )

    if MOUNTAIN_F_UPGRADE_REPORT.is_file():
        try:
            report = json.loads(MOUNTAIN_F_UPGRADE_REPORT.read_text(encoding="utf-8"))
            report_valid = (
                report.get("schema") == "rok.mountain_f_gamma_product.ue_upgrade.v1"
                and bool(report.get("project"))
                and Path(report.get("project", "")).resolve() == expected_project
                and report.get("world") == "L_RokUnityWorldMapPrototype"
                and report.get("material") == MOUNTAIN_F_MATERIAL
                and report.get("texture") == MOUNTAIN_F_DECODED_TEXTURE
                and report.get("post_identity", {}).get("f_body_count") == 18
                and bool(report.get("source_hashes", {}).get("dds_sha256"))
            )
        except (OSError, ValueError, TypeError, AttributeError):
            report_valid = False
        raise RuntimeError(
            "Full world-map generation is blocked: the Mountain F upgrade "
            f"report at {MOUNTAIN_F_UPGRADE_REPORT} is "
            f"{'valid' if report_valid else 'present but invalid; migration state is ambiguous'}. "
            "Use the dedicated Mountain F rebuild path. "
            "-RokWorldMapReuseExistingAssets still rebuilds and saves the map."
        )


def build_scene():
    guard_migrated_mountain_f()
    world_manifest = load_json(
        WORLD_MANIFEST_PATH,
        "rok.unity_world_map.v1",
    )
    asset_manifest = load_json(
        ASSET_MANIFEST_PATH,
        "rok.unity_world_map_assets.v1",
    )
    if world_manifest.get("ue_target_level") != TARGET_LEVEL:
        raise RuntimeError(
            f"Manifest target {world_manifest.get('ue_target_level')} "
            f"does not match generator target {TARGET_LEVEL}"
        )
    if asset_manifest["source_world_manifest"]["sha256"] != sha256(
        WORLD_MANIFEST_PATH
    ):
        raise RuntimeError(
            "Asset preparation is stale relative to unity_world_map.json"
        )

    ensure_target_level()
    if not is_target_level_loaded():
        current_world = unreal.EditorLevelLibrary.get_editor_world()
        current_package = (
            str(current_world.get_outermost().get_name())
            if current_world
            else "<no editor world>"
        )
        raise RuntimeError(
            "Refusing to destroy actors because the target map did not "
            f"become current: expected {TARGET_LEVEL}, got {current_package}"
        )
    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        unreal.EditorLevelLibrary.destroy_actor(actor)
    delete_stale_generated_city_assets()

    material_assets = build_material_assets(asset_manifest, world_manifest)
    (
        plane_mesh,
        mesh_by_prefab,
        mesh_asset_paths,
        mountain_body_imports,
        river_layer_imports,
        waterfall_meshes,
    ) = build_mesh_assets(asset_manifest)
    (
        terrain_count,
        rendered_object_count,
        omitted_empty_count,
        river_logical_actor_count,
        river_physical_actor_count,
    ) = spawn_manifest_world(
        world_manifest,
        asset_manifest,
        plane_mesh,
        mesh_by_prefab,
        material_assets,
    )
    background = spawn_solid_background_underlay(
        world_manifest,
        plane_mesh,
        material_assets["background"],
    )
    player_city = spawn_player_city(
        plane_mesh,
        material_assets["city"],
        asset_manifest["player_city"],
    )
    sun = setup_lighting(world_manifest)
    config, player_start = configure_runtime()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_LEVEL):
        raise RuntimeError(f"Failed to save target map: {TARGET_LEVEL}")
    unreal.EditorAssetLibrary.save_asset(
        TARGET_LEVEL,
        only_if_is_dirty=False,
    )
    if not TARGET_LEVEL_FILE.exists():
        raise RuntimeError(
            f"Saved target map package is missing: {TARGET_LEVEL_FILE}"
        )

    generation = {
        "schema": "rok.unity_world_map_ue_generation.v1",
        "target_level": TARGET_LEVEL,
        "asset_build_mode": (
            "reused_existing_phase1_assets"
            if REUSE_EXISTING_ASSETS
            else "rebuilt_from_current_source_payloads"
        ),
        "target_level_package": {
            "path": str(
                TARGET_LEVEL_FILE.relative_to(PROJECT_ROOT)
            ).replace("\\", "/"),
            "size": TARGET_LEVEL_FILE.stat().st_size,
            "sha256": sha256(TARGET_LEVEL_FILE),
        },
        "source_world_manifest": {
            "path": str(
                WORLD_MANIFEST_PATH.relative_to(PROJECT_ROOT)
            ).replace("\\", "/"),
            "sha256": sha256(WORLD_MANIFEST_PATH),
        },
        "source_asset_manifest": {
            "path": str(
                ASSET_MANIFEST_PATH.relative_to(PROJECT_ROOT)
            ).replace("\\", "/"),
            "sha256": sha256(ASSET_MANIFEST_PATH),
        },
        "runtime_classes": {
            "game_mode": GAME_MODE_CLASS,
            "config_actor": CONFIG_ACTOR_CLASS,
            "camera_pawn": "/Script/Rok.RokWorldMapCameraPawn",
            "player_controller": "/Script/Rok.RokWorldMapPlayerController",
            "hud": "/Script/Rok.RokWorldMapHud",
            "city_actor": CITY_ACTOR_CLASS,
        },
        "environment_policy": {
            "source": world_manifest["environment"],
            "prototype_phase": material_assets["lighting_phase"]["phase"],
            "sprite_color_parameter": SPRITE_COLOR_PARAMETER,
            "sprite_color": material_assets["lighting_phase"][
                "sprite_color"
            ],
            "background_color_parameter": BACKGROUND_COLOR_PARAMETER,
            "background_color": material_assets["lighting_phase"][
                "camera_background_color"
            ],
            "background_actor_label": background.get_actor_label(),
            "background_material": material_assets[
                "background"
            ].get_path_name(),
            "directional_light_actor_label": sun.get_actor_label(),
            "directional_light_ue_rotation": {
                "pitch": -float(
                    world_manifest["environment"]["directional_light"][
                        "unity_local_euler_degrees"
                    ]["x"]
                ),
                "yaw": 90.0
                + float(
                    world_manifest["environment"]["directional_light"][
                        "unity_local_euler_degrees"
                    ]["y"]
                ),
                "roll": -float(
                    world_manifest["environment"]["directional_light"][
                        "unity_local_euler_degrees"
                    ]["z"]
                ),
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
        },
        "runtime_collision_policy": {
            "owner": GAME_MODE_CLASS,
            "nonselectable_tags": [
                "RokWorldMapTerrain",
                "RokWorldMapSourceObject",
            ],
            "selectable_tag": "RokWorldMapSelectable",
            "nonselectable_collision": "NoCollision",
            "selectable_visual_collision": "NoCollision",
            "selectable_collision": "QueryOnly",
            "selection_component_tag": (
                "RokWorldMapSelectionCollider"
            ),
            "selectable_trace_channel": "Visibility",
            "runtime_log_marker": (
                "Rok world-map collision policy applied:"
            ),
        },
        "runtime_tree_lod_policy": {
            "owner": "/Script/Rok.RokWorldMapCameraPawn",
            "source_actor_tag": "RokWorldMapSourceObject",
            "tree_actor_tag": "RokWorldMapTree",
            "legacy_tree_material_names": TREE_UE_MATERIAL_NAMES,
            "tree_translucency_sort_priority": 6,
            "bake_dxf": asset_manifest["tree_scale_bake_dxf"],
            "bake_scale": asset_manifest["tree_scale_bake_value"],
            "curve_source_sha256": asset_manifest[
                "source_semantics"
            ]["tree"]["source"]["sha256"],
            "evaluation": "source_curve_scale_over_baked_scale",
            "runtime_log_marker": (
                "Rok world-map tree LOD initialized:"
            ),
        },
        "runtime_player_city_entry_policy": {
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
        },
        "mountain_render_policy": {
            "source_actor_tag": "RokWorldMapMountain",
            "source_contract": asset_manifest["mountain_render_contract"],
            "geometry_policy": "split_fbx_exact_body_only",
            "body_mesh_destination": MOUNTAIN_BODY_MESH_DESTINATION,
            "visible_material_slot_count": 1,
            "excluded_runtime_nodes": [
                "edge",
                "collision",
                "collision_da",
            ],
            "ue_material_names": [
                "M_WorldSource_build_mountain_A",
                "M_WorldSource_build_mountain_F",
            ],
            "component_cast_shadow": False,
            "component_affects_distance_field_lighting": False,
            "component_affects_dynamic_indirect_lighting": False,
        },
        "mountain_body_imports": mountain_body_imports,
        "river_render_policy": {
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
                material_assets["lighting_phase"]["phase"]
                if river_layer_imports
                else None
            ),
            "edge_ambient_color": (
                material_assets["lighting_phase"]["ambient_color"]
                if river_layer_imports
                else None
            ),
            "edge_direction_color": (
                material_assets["lighting_phase"]["direction_color"]
                if river_layer_imports
                else None
            ),
            "edge_direction_intensity": (
                material_assets["lighting_phase"][
                    "direction_intensity"
                ]
                if river_layer_imports
                else None
            ),
            "edge_light_direction_ue": (
                source_light_direction_ue(world_manifest)
                if river_layer_imports
                else None
            ),
            "edge_material": (
                object_path(
                    MATERIAL_DESTINATION,
                    RIVER_EDGE_MATERIAL_NAME,
                )
                if river_layer_imports
                else None
            ),
            "water_material": (
                object_path(
                    MATERIAL_DESTINATION,
                    RIVER_WATER_MATERIAL_NAME,
                )
                if river_layer_imports
                else None
            ),
            "edge_texture": material_assets[
                "river_texture_paths"
            ].get("edge"),
            "water_texture": material_assets[
                "river_texture_paths"
            ].get("water"),
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
                next(iter(river_layer_imports.values()))[
                    "geometry_contract"
                ]["water_dynamic_shader_contract"]
                if river_layer_imports
                else None
            ),
            "transform_policy": (
                "source_layer_local_trs_composed_with_logical_instance"
            ),
            "imported_prefab_count": len(river_layer_imports),
            "imported_render_layer_mesh_count": sum(
                len(record["imported_render_layer_assets"])
                for record in river_layer_imports.values()
            ),
            "deleted_collision_node_asset_count": sum(
                len(record["deleted_collision_node_assets"])
                for record in river_layer_imports.values()
            ),
            "logical_actor_count": river_logical_actor_count,
            "physical_render_layer_actor_count": (
                river_physical_actor_count
            ),
        },
        "river_layer_imports": river_layer_imports,
        "waterfall_layer_imports": {
            node: mesh.get_path_name()
            for node, mesh in waterfall_meshes.items()
        },
        "terrain_actor_count": terrain_count,
        "source_object_record_count": world_manifest[
            "prototype_window"
        ]["object_instance_count"],
        "rendered_source_object_actor_count": rendered_object_count,
        "physical_source_render_actor_count": (
            rendered_object_count + river_logical_actor_count
        ),
        "river_logical_object_actor_count": (
            river_logical_actor_count
        ),
        "river_physical_render_layer_actor_count": (
            river_physical_actor_count
        ),
        "omitted_empty_source_record_count": omitted_empty_count,
        "gameplay_actor_count": 1,
        "config_actor_label": config.get_actor_label(),
        "player_start_label": player_start.get_actor_label(),
        "player_city_label": player_city.get_actor_label(),
        "player_city_source": {
            **asset_manifest["player_city"],
            "ue_actor_class": CITY_ACTOR_CLASS,
            "ue_mesh_asset": plane_mesh.get_path_name(),
            "ue_texture_asset": material_assets["city_texture_path"],
            "ue_material_asset": material_assets["city_material_path"],
        },
        "mesh_assets": mesh_asset_paths,
        "texture_assets": material_assets["texture_asset_paths"],
        "material_assets": material_assets["material_asset_paths"],
        "forbidden_content": {
            "engine_basic_shapes": 0,
            "blockout_materials": 0,
            "source_derived_proxies": 0,
        },
    }
    GENERATION_PATH.write_text(
        json.dumps(generation, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    unreal.log(
        "Rok Unity world-map prototype saved: "
        f"terrain={terrain_count} "
        f"source_objects={rendered_object_count} "
        f"empty_sources={omitted_empty_count} "
        f"level={TARGET_LEVEL}"
    )


if __name__ == "__main__":
    build_scene()
