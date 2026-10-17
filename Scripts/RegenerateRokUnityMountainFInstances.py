"""Preflight or restore only missing Mountain F world-map body actors.

Offline: run ``python Scripts/RegenerateRokUnityMountainFInstances.py --preflight``.
This reads source/manifests/assets and prints a plan; ``--report`` additionally
writes that plan under Saved/RokUnityMigration/BatchReview20261008/MapRegeneration.
Neither option imports Unreal, changes assets, or opens/saves a map.

In a UE 5.8 editor with the exact target map loaded, call ``run_live()`` for a
read-only actor/asset inventory. After reviewing the offline plan, call
``run_live(apply=True, expected_preflight_sha256="<offline report SHA-256>")``
to spawn missing F actors only. Apply never saves the map: inspect the live
report and save the map separately when the change is accepted. The full
world-map generator remains blocked; this path never imports or overwrites F
mesh, decoded texture, or material assets.
"""

import argparse
import hashlib
import importlib.util
import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_ROOT = Path("D:/ueDemo/Rok").resolve()
SAVED = ROOT / "Saved/RokUnityMigration"
REPORT_DIR = SAVED / "BatchReview20261008/MapRegeneration"
PREFLIGHT_REPORT = REPORT_DIR / "MountainFInstancesOfflinePreflight.json"
WORLD_PATH = SAVED / "unity_world_map.json"
ASSET_PATH = SAVED / "unity_world_map_assets.json"
GENERATION_PATH = SAVED / "unity_world_map_ue_generation.json"
PRODUCT_PATH = SAVED / "MountainFGammaProduct/prepare_manifest.json"
UPGRADE_PATH = SAVED / "MountainFGammaProduct/ue_upgrade_report.json"
TARGET_LEVEL = "/Game/Maps/L_RokUnityWorldMapPrototype"
PREFAB_ID = "build_mountain_F"
MATERIAL_OBJECT = (
    "/Game/RokUnityWorldMapPrototype/Materials/"
    "M_WorldSource_build_mountain_F.M_WorldSource_build_mountain_F"
)
# Product package recorded immediately after the isolated 2026-09-25 upgrade.
# Pinning its bytes also detects parameter-GUID drift, which UE Python does not expose.
PRODUCT_MATERIAL_SHA256 = "78c3782beb4926c62be90ccf6d39b02c8c23c714c0cb87d50f2a469b44357069"
TEXTURE_OBJECT = (
    "/Game/RokUnityWorldMapPrototype/Textures/"
    "T_MountainF_UnityDecodedMips.T_MountainF_UnityDecodedMips"
)
BODY_MESH = (
    "/Game/RokUnityWorldMapPrototype/Meshes/MountainBodies/"
    "SM_RokWorldBody_build_mountain_F_build_mountain_F."
    "SM_RokWorldBody_build_mountain_F_build_mountain_F"
)
FOLDER = "RokUnityWorldMap/SourceObjects"
SOURCE_MATERIAL = "ref/client-unity/Assets/BundleAssets/land/Material/build_mountain_F.mat"
SOURCE_TEXTURE = "ref/client-unity/Assets/BundleAssets/land/Texture/build_mountain_F.png"
SOURCE_META = SOURCE_TEXTURE + ".meta"


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_json(path, schema):
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    if data.get("schema") != schema:
        raise RuntimeError(f"Unexpected schema at {path}: {data.get('schema')}")
    return data


def source_file(record):
    if not isinstance(record, dict) or not isinstance(record.get("path"), str):
        raise RuntimeError("Source file record lacks a path")
    path = (ROOT / record["path"]).resolve()
    if not path.is_relative_to(ROOT) or not path.is_file():
        raise RuntimeError(f"Source file is outside project or missing: {path}")
    if path.stat().st_size != record.get("size") or sha256(path) != record.get("sha256"):
        raise RuntimeError(f"Source file size/hash differs: {path}")
    return path


def asset_file(object_path):
    package = object_path.split(".", 1)[0]
    if not package.startswith("/Game/RokUnityWorldMapPrototype/"):
        raise RuntimeError(f"Non-F package outside expected destination: {package}")
    path = ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")
    if not path.is_file():
        raise RuntimeError(f"Existing validated F asset is missing: {path}")
    return {"object_path": object_path, "package_file": str(path.relative_to(ROOT)).replace("\\", "/"),
            "bytes": path.stat().st_size, "sha256": sha256(path)}


def same_rotation(actual, expected):
    return abs((float(actual) - float(expected) + 180.0) % 360.0 - 180.0) <= 1e-4


def same_vector(actual, expected):
    return len(actual) == len(expected) == 3 and all(
        abs(float(a) - float(b)) <= 1e-4 for a, b in zip(actual, expected)
    )


def expected_instances(world, upgrade):
    historical = {row["label"]: row for row in upgrade["post_bindings"]}
    if len(historical) != 18 or len(upgrade["pre_bindings"]) != 18 or upgrade["pre_bindings"] != upgrade["post_bindings"]:
        raise RuntimeError("F upgrade does not preserve exactly 18 original bindings")
    rows = {}
    for tile in world["prototype_window"]["tiles"]:
        for instance in tile["instances"]:
            if instance["kind"] != "object" or instance["prefab_id"] != PREFAB_ID:
                continue
            index = instance["template_index"]
            label = f"RokWorldObject_Tile{tile['x']}_{tile['y']}_Object{index:02d}_{PREFAB_ID}"
            policy = instance["unity_runtime_transform_policy"]
            if policy not in ("position_only", "position_rotation_scale") or label in rows:
                raise RuntimeError(f"Unexpected Mountain F source instance: {label}")
            position = instance["ue_relative_to_focus"]
            scale = instance["ue_actor_scale_for_centimeter_asset"]
            tags = ["RokWorldMapGenerated", "RokWorldMapSourceObject", "RokWorldMapExactSource",
                    f"UnityTile_{tile['x']}_{tile['y']}", f"UnityObjectIndex_{index}",
                    f"UnityPrefab_{PREFAB_ID}",
                    "UnityTransformPolicy_PositionOnly" if policy == "position_only" else
                    "UnityTransformPolicy_PositionRotationScale", "RokWorldMapMountain"]
            row = {"label": label, "source_key": f"UnityTile_{tile['x']}_{tile['y']}/UnityObjectIndex_{index}",
                   "tile": [tile["x"], tile["y"]], "object_index": index, "source_prefab": PREFAB_ID,
                   "folder": FOLDER, "tags": tags, "mesh": BODY_MESH, "material": MATERIAL_OBJECT,
                   "location": [position[k] for k in ("x", "y", "z")],
                   "rotation": [0.0, float(instance["ue_yaw"]), 0.0],
                   "scale": [scale[k] for k in ("x", "y", "z")]}
            old = historical.get(label)
            if (not old or old["mesh"] != BODY_MESH or old["material"] != MATERIAL_OBJECT
                    or sorted(old["tags"]) != sorted(tags)
                    or not same_vector(row["location"], old["location"])
                    or not same_vector(row["scale"], old["scale"])
                    or not all(same_rotation(a, b) for a, b in zip(row["rotation"], old["rotation"]))):
                raise RuntimeError(f"Current Unity instance differs from upgraded UE F binding: {label}")
            rows[label] = row
    if len(rows) != 18 or set(rows) != set(historical) or len({r["source_key"] for r in rows.values()}) != 18:
        raise RuntimeError("Unity window and upgrade report do not identify the same 18 F instances")
    return [rows[label] for label in sorted(rows)]


def verify_dds(product):
    if (product.get("status") != "prepared-not-imported" or product.get("mip_count") != 11
            or product.get("dds_format") != "DX10 B8G8R8A8_UNORM 2D"
            or product.get("source_orientation") != "as_saved"
            or product.get("dds_extra_vertical_flip") is not False
            or product.get("dds_file") != "T_MountainF_UnityDecodedMips.dds"):
        raise RuntimeError("F decoded DDS product contract changed")
    data_path = PRODUCT_PATH.parent / product["dds_file"]
    data = data_path.read_bytes()
    if len(data) != product.get("dds_bytes") or sha256(data_path) != product.get("dds_sha256"):
        raise RuntimeError("F decoded DDS bytes/hash changed")
    if (data[:4] != b"DDS " or len(data) < 148
            or struct.unpack_from("<7I", data, 4) != (124, 0x2100F, 1024, 1024, 4096, 0, 11)
            or data[84:88] != b"DX10"
            or struct.unpack_from("<5I", data, 128) != (87, 3, 0, 1, 0)):
        raise RuntimeError("F decoded DDS header/format/mip geometry changed")
    mips = product.get("mips")
    if not isinstance(mips, list) or len(mips) != 11:
        raise RuntimeError("F decoded DDS must contain 11 declared mips")
    offset = 148
    for level, mip in enumerate(mips):
        size = max(1, 1024 >> level)
        length = size * size * 4
        if (mip.get("level") != level or mip.get("size") != [size, size]
                or mip.get("bytes") != length or
                hashlib.sha256(data[offset:offset + length]).hexdigest() != mip.get("bgra8_sha256")):
            raise RuntimeError(f"F decoded DDS mip {level} changed")
        offset += length
    if offset != len(data):
        raise RuntimeError("F decoded DDS has trailing/missing bytes")


def recent_runtime_evidence(product):
    found = []
    for path in (SAVED / "MountainFGammaAudit").glob("*/manifest.json"):
        try:
            evidence = json.loads(path.read_text(encoding="utf-8-sig"))
            if (evidence.get("passed") is True and evidence.get("temporary_state_restored") is True
                    and len(evidence.get("fresh_f_bodies", [])) == 18
                    and len(evidence.get("platform_mips", [])) == 11
                    and evidence.get("product_manifest_sha256") == sha256(PRODUCT_PATH)
                    and evidence.get("material") == MATERIAL_OBJECT
                    and evidence.get("texture") == TEXTURE_OBJECT
                    and [x.get("sha256") for x in evidence["platform_mips"]] ==
                    [x["bgra8_sha256"] for x in product["mips"]]):
                found.append(path)
        except (OSError, ValueError, KeyError, TypeError):
            continue
    if not found:
        raise RuntimeError("No matching 18-F / 11-compiled-mip runtime audit evidence")
    path = sorted(found)[-1]
    return {"path": str(path.relative_to(ROOT)).replace("\\", "/"), "sha256": sha256(path),
            "scope": "historical compiled-mip and bindings evidence, not current SDR viewport acceptance"}


def build_preflight():
    if ROOT != EXPECTED_ROOT or not (ROOT / "Rok.uproject").is_file():
        raise RuntimeError(f"Explicit Rok project identity failed: {ROOT}")
    world = read_json(WORLD_PATH, "rok.unity_world_map.v1")
    assets = read_json(ASSET_PATH, "rok.unity_world_map_assets.v1")
    generation = read_json(GENERATION_PATH, "rok.unity_world_map_ue_generation.v1")
    product = read_json(PRODUCT_PATH, "rok.mountain_f_gamma_product.prepare.v1")
    upgrade = read_json(UPGRADE_PATH, "rok.mountain_f_gamma_product.ue_upgrade.v1")
    if (world.get("ue_target_level") != TARGET_LEVEL or generation.get("target_level") != TARGET_LEVEL
            or assets["source_world_manifest"].get("sha256") != sha256(WORLD_PATH)
            or generation["source_world_manifest"].get("sha256") != sha256(WORLD_PATH)
            or generation["source_asset_manifest"].get("sha256") != sha256(ASSET_PATH)
            or Path(upgrade.get("project", "")).resolve() != ROOT
            or upgrade.get("world") != TARGET_LEVEL.rsplit("/", 1)[-1]
            or upgrade.get("material") != MATERIAL_OBJECT or upgrade.get("texture") != TEXTURE_OBJECT):
        raise RuntimeError("F project/map/world/asset/upgrade provenance differs")
    source = assets["object_sources"][PREFAB_ID]
    geometry = source["mountain_geometry_contract"]
    if (source["representation"] != "exact_source_fbx" or geometry["policy"] != "split_fbx_exact_body_only"
            or geometry["combine_meshes"] is not False or geometry["ue_body_mesh_asset"] != BODY_MESH
            or geometry["body_render_node"]["material_path"] != SOURCE_MATERIAL
            or geometry["edge_render_node"]["material_path"].endswith("build_mountain_F_edge.mat") is not True
            or geometry["excluded_fbx_nodes"] != ["build_mountain_F_edge"]):
        raise RuntimeError("F body-only geometry/edge exclusion contract changed")
    source_mesh = source_file(source["source_mesh"])
    fbx_meta = Path(str(source_mesh) + ".meta")
    if f"guid: {geometry['source_fbx_guid']}" not in fbx_meta.read_text(encoding="utf-8-sig"):
        raise RuntimeError("F source FBX GUID changed")
    material_records = [row for row in assets["mountain_materials"]
                        if row["source_material"]["path"] == SOURCE_MATERIAL]
    if len(material_records) != 1 or material_records[0]["source_texture"]["path"] != SOURCE_TEXTURE:
        raise RuntimeError("F material/texture variant mapping changed")
    material_source = source_file(material_records[0]["source_material"])
    texture_source = source_file(material_records[0]["source_texture"])
    material_meta = Path(str(material_source) + ".meta")
    if f"guid: {geometry['body_render_node']['material_guid']}" not in material_meta.read_text(encoding="utf-8-sig"):
        raise RuntimeError("F source body material GUID changed")
    source_meta_sha = sha256(ROOT / SOURCE_META)
    if (source_meta_sha != upgrade["source_hashes"].get("source_meta_sha256")
            or sha256(texture_source) != product["source_hashes"].get("source_png_sha256")):
        raise RuntimeError("F Unity texture/source metadata changed")
    prefab_path = (ROOT / source["source_prefab"]).resolve()
    if not prefab_path.is_relative_to(ROOT) or not prefab_path.is_file():
        raise RuntimeError("F source prefab path escaped project or is missing")
    prefab = prefab_path.read_text(encoding="utf-8-sig")
    if (f"guid: {geometry['source_fbx_guid']}" not in prefab or
            f"guid: {geometry['body_render_node']['material_guid']}" not in prefab or
            "m_Name: build_mountain_F_edge" not in prefab):
        raise RuntimeError("F source prefab body/edge/material metadata changed")
    for file_name, key in (("verify.json", "verify_sha256"),
                           ("native_render.json", "native_sha256"),
                           ("prepare.json", "prepare_sha256")):
        expected = product["source_hashes"].get(key)
        path = SAVED / "MountainFGammaV2" / file_name
        if sha256(path) != expected or upgrade["source_hashes"].get(key) != expected:
            raise RuntimeError(f"F native decode source evidence changed: {file_name}")
    if product["dds_sha256"] != upgrade["source_hashes"].get("dds_sha256"):
        raise RuntimeError("F decoded product/upgrade backup provenance changed")
    backup = Path(upgrade["backup"])
    if not backup.is_absolute():
        backup = ROOT / backup
    backup = backup.resolve()
    if not backup.is_relative_to(PRODUCT_PATH.parent / "BeforeUpgradeBackup") or not backup.is_file():
        raise RuntimeError("F original material backup path escaped dedicated product folder")
    if sha256(backup) != upgrade["backup_sha256"]:
        raise RuntimeError("F original material backup changed")
    verify_dds(product)
    rows = expected_instances(world, upgrade)
    map_file = ROOT / "Content/Maps/L_RokUnityWorldMapPrototype.umap"
    if not map_file.is_file():
        raise RuntimeError("Target world-map package missing")
    ue_assets = {name: asset_file(path) for name, path in
                 (("mesh", BODY_MESH), ("material", MATERIAL_OBJECT), ("decoded_texture", TEXTURE_OBJECT))}
    if ue_assets["material"]["sha256"] != PRODUCT_MATERIAL_SHA256:
        raise RuntimeError("F product material package differs from recorded post-upgrade SHA-256")
    return {"schema": "rok.mountain_f_instances.preflight.v1", "passed": True,
            "read_only": True, "project": str(ROOT), "target_level": TARGET_LEVEL,
            "scope": "Only the 18 source-keyed Mountain F body actors; existing mesh/material/texture are immutable dependencies.",
            "full_world_generator_safe_to_rerun": False,
            "source_inputs": {"world_manifest_sha256": sha256(WORLD_PATH),
                              "asset_manifest_sha256": sha256(ASSET_PATH),
                              "generation_manifest_sha256": sha256(GENERATION_PATH),
                              "upgrade_report_sha256": sha256(UPGRADE_PATH),
                              "prepare_manifest_sha256": sha256(PRODUCT_PATH),
                              "source_fbx": {"path": str(source_mesh.relative_to(ROOT)).replace("\\", "/"),
                                             "sha256": sha256(source_mesh)},
                              "source_prefab": {"path": str(prefab_path.relative_to(ROOT)).replace("\\", "/"),
                                                "sha256": sha256(prefab_path)},
                              "source_material_sha256": sha256(material_source),
                              "source_texture_sha256": sha256(texture_source),
                              "source_texture_meta_sha256": source_meta_sha,
                              "decoded_dds_sha256": product["dds_sha256"],
                              "decoded_dds_mip_sha256": [m["bgra8_sha256"] for m in product["mips"]]},
            "runtime_evidence": recent_runtime_evidence(product),
            "evidence_limits": [
                "The F material uasset is pinned to the recorded 2026-09-25 post-upgrade byte hash; the mesh and texture hashes below remain current disk snapshots, not approved historical baselines.",
                "The matching runtime 11-mip audit is historical; offline preflight cannot prove the current editor's compiled mip bytes or mesh geometry.",
                "Normal SDR viewport fidelity remains unaccepted; this actor-only recovery never modifies or reaccepts F product dependencies.",
            ],
            "pinned_product_material_sha256": PRODUCT_MATERIAL_SHA256,
            "target_map_file": {"path": str(map_file.relative_to(ROOT)).replace("\\", "/"),
                                "sha256": sha256(map_file)},
            "immutable_ue_dependencies": ue_assets,
            "expected_actor_count": len(rows), "expected_actors": rows,
            "apply_policy": "Fail on any unexpected/duplicate/drifted F actor. Spawn only missing source keys; never reimport assets, delete existing actors, or save the map."}


def write_report(path, record):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return sha256(path)


def inspect_material_expression_connections(unreal, material, expressions, customs):
    """Read loaded graph identity/edges; never infer an edge from node count."""
    def prop(obj, name):
        try:
            return obj.get_editor_property(name), None
        except Exception as exc:
            return None, f"{type(exc).__name__}: {exc}"

    def path(obj):
        return obj.get_path_name() if obj else None

    nodes = []
    for expression in expressions:
        row = {"path": path(expression), "type": expression.get_class().get_name()}
        try:
            row["input_nodes_from_editor_api"] = [
                path(x) for x in unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
                    material, expression)]
        except Exception as exc:
            row["input_nodes_read_error"] = f"{type(exc).__name__}: {exc}"
        if isinstance(expression, unreal.MaterialExpressionVectorParameter):
            for key in ("parameter_name", "expression_guid", "material_expression_guid",
                        "group", "use_custom_primitive_data"):
                value, error = prop(expression, key)
                row[key] = str(value) if value is not None else None
                if error:
                    row[key + "_read_error"] = error
            value, error = prop(expression, "default_value")
            row["default_value"] = ({"r": value.r, "g": value.g, "b": value.b, "a": value.a}
                                    if value is not None else None)
            if error:
                row["default_value_read_error"] = error
        nodes.append(row)

    custom_inputs = []
    custom_input_read_error = None
    if len(customs) == 1:
        inputs, custom_input_read_error = prop(customs[0], "inputs")
        if inputs is not None:
            for index, entry in enumerate(inputs):
                name, name_error = prop(entry, "input_name")
                edge, edge_error = prop(entry, "input")
                source, source_error = prop(edge, "expression") if edge is not None else (None, None)
                item = {"index": index, "input_name": str(name) if name is not None else None,
                        "source_expression": path(source)}
                for field in ("output_index", "mask", "mask_r", "mask_g", "mask_b", "mask_a"):
                    value, error = prop(edge, field) if edge is not None else (None, None)
                    item[field] = value
                    if error:
                        item[field + "_read_error"] = error
                for field, error in (("input_name", name_error), ("input", edge_error),
                                     ("source_expression", source_error)):
                    if error:
                        item[field + "_read_error"] = error
                custom_inputs.append(item)

    input_nodes = None
    input_names = None
    input_nodes_read_error = None
    input_names_read_error = None
    if len(customs) == 1:
        try:
            input_nodes = [path(x) for x in unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
                material, customs[0])]
        except Exception as exc:
            input_nodes_read_error = f"{type(exc).__name__}: {exc}"
        try:
            input_names = list(unreal.MaterialEditingLibrary.get_material_expression_input_names(customs[0]))
        except Exception as exc:
            input_names_read_error = f"{type(exc).__name__}: {exc}"
    return {"nodes": nodes, "custom_inputs": custom_inputs,
            "custom_inputs_read_error": custom_input_read_error,
            "custom_input_names_from_editor_api": input_names,
            "custom_input_names_read_error": input_names_read_error,
            "custom_input_nodes_from_editor_api": input_nodes,
            "custom_input_nodes_read_error": input_nodes_read_error,
            "historic_pre_upgrade_vector_path": MATERIAL_OBJECT + ":MaterialExpressionVectorParameter_0"}


def check_live_assets(unreal, plan, diagnostic=None):
    diagnostic = diagnostic if diagnostic is not None else {}
    mismatches = []
    mesh = unreal.load_asset(BODY_MESH)
    material = unreal.load_asset(MATERIAL_OBJECT)
    texture = unreal.load_asset(TEXTURE_OBJECT)
    diagnostic["loaded_assets"] = {
        name: {"expected_path": path, "actual_path": asset.get_path_name() if asset else None,
               "actual_class": asset.get_class().get_name() if asset else None}
        for name, path, asset in (("mesh", BODY_MESH, mesh), ("material", MATERIAL_OBJECT, material),
                                  ("decoded_texture", TEXTURE_OBJECT, texture))}
    for name, asset, expected_class in (("mesh", mesh, unreal.StaticMesh),
                                        ("material", material, unreal.Material),
                                        ("decoded_texture", texture, unreal.Texture2D)):
        if not isinstance(asset, expected_class):
            mismatches.append(f"{name} class: expected {expected_class.__name__}, actual {type(asset).__name__}")
    if mismatches:
        diagnostic["mismatches"] = mismatches
        raise RuntimeError("F immutable dependency class/path differs: " + "; ".join(mismatches))
    mesh_slots = len(mesh.get_editor_property("static_materials"))
    diagnostic["mesh_material_slots"] = {"expected": 1, "actual": mesh_slots}
    if mesh_slots != 1:
        mismatches.append(f"mesh material slots: expected 1, actual {mesh_slots}")
    dds_sha = plan["source_inputs"]["decoded_dds_sha256"]
    metadata = {}
    for name, asset in (("material", material), ("decoded_texture", texture)):
        actual = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, "RokMountainFProductDDS"))
        metadata[name] = {"expected": dds_sha, "actual": actual}
        if actual != dds_sha:
            mismatches.append(f"{name} product DDS metadata differs")
    diagnostic["product_dds_metadata"] = metadata
    upgrade = read_json(UPGRADE_PATH, "rok.mountain_f_gamma_product.ue_upgrade.v1")
    flags = {}
    for key, expected in upgrade["imported_texture_flags"].items():
        actual = str(texture.get_editor_property(key))
        flags[key] = {"expected": expected, "actual": actual}
        if actual != expected:
            mismatches.append(f"decoded texture setting differs: {key}")
    diagnostic["decoded_texture_flags"] = flags
    upgrade_spec = importlib.util.spec_from_file_location(
        "rok_f_original_upgrade", ROOT / "Scripts/CreateRokUnityMountainFGammaMaterial.py")
    original_upgrade = importlib.util.module_from_spec(upgrade_spec)
    upgrade_spec.loader.exec_module(original_upgrade)
    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    connection_diagnostic = inspect_material_expression_connections(
        unreal, material, expressions,
        [x for x in expressions if isinstance(x, unreal.MaterialExpressionCustom)])
    samples = [x for x in expressions if isinstance(x, unreal.MaterialExpressionTextureSample)]
    colors = [x for x in expressions if isinstance(x, unreal.MaterialExpressionVectorParameter)
              and str(x.get_editor_property("parameter_name")) == "UnitySpriteColor"]
    customs = [x for x in expressions if isinstance(x, unreal.MaterialExpressionCustom)]
    sample_texture = samples[0].get_editor_property("texture") if len(samples) == 1 else None
    sample_sampler = samples[0].get_editor_property("sampler_type") if len(samples) == 1 else None
    actual_code = customs[0].get_editor_property("code") if len(customs) == 1 else None
    emissive = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    blend = material.get_editor_property("blend_mode")
    shading = material.get_editor_property("shading_model")
    two_sided = material.get_editor_property("two_sided")
    actual_code_text = str(actual_code) if actual_code is not None else None
    source_world = read_json(WORLD_PATH, "rok.unity_world_map.v1")
    source_lighting = source_world["environment"]["runtime_lighting"]
    source_phases = [phase for phase in source_lighting["phases"]
                     if phase["phase"] == source_lighting["prototype_phase"]]
    if len(source_phases) != 1:
        raise RuntimeError("F source lighting phase is ambiguous")
    source_color = source_phases[0]["sprite_color"]
    expected_color = {key: struct.unpack("<f", struct.pack("<f", float(source_color[key])))[0]
                      for key in ("r", "g", "b", "a")}
    expected_nodes = {
        MATERIAL_OBJECT + ":MaterialExpressionVectorParameter_0": "MaterialExpressionVectorParameter",
        MATERIAL_OBJECT + ":MaterialExpressionTextureSample_0": "MaterialExpressionTextureSample",
        MATERIAL_OBJECT + ":MaterialExpressionVectorParameter_1": "MaterialExpressionVectorParameter",
        MATERIAL_OBJECT + ":MaterialExpressionCustom_0": "MaterialExpressionCustom",
    }
    actual_nodes = {row["path"]: row["type"] for row in connection_diagnostic["nodes"]}
    vector_rows = [row for row in connection_diagnostic["nodes"]
                   if row["type"] == "MaterialExpressionVectorParameter"]
    node_by_path = {row["path"]: row for row in connection_diagnostic["nodes"]}
    legacy_path = MATERIAL_OBJECT + ":MaterialExpressionVectorParameter_0"
    sample_path = MATERIAL_OBJECT + ":MaterialExpressionTextureSample_0"
    active_path = MATERIAL_OBJECT + ":MaterialExpressionVectorParameter_1"
    legacy = node_by_path.get(legacy_path, {})
    sample = node_by_path.get(sample_path, {})
    custom_source_nodes = connection_diagnostic["custom_input_nodes_from_editor_api"]
    legacy_unused = (legacy.get("input_nodes_from_editor_api") == []
                     and sample.get("input_nodes_from_editor_api") is not None
                     and all(x is None for x in sample["input_nodes_from_editor_api"])
                     and custom_source_nodes is not None
                     and legacy_path not in custom_source_nodes)
    checks = {
        "exact_known_node_set": len(expressions) == 4 and actual_nodes == expected_nodes,
        "texture_sample_count": len(samples) == 1,
        "known_legacy_vector_unused": legacy_unused,
        "unity_sprite_color_count": len(colors) == 2,
        "unity_sprite_color_source_defaults": (
            len(vector_rows) == 2 and all(row.get("parameter_name") == "UnitySpriteColor"
                                          and row.get("default_value") == expected_color
                                          and row.get("use_custom_primitive_data") == "False"
                                          for row in vector_rows)),
        "custom_expression_count": len(customs) == 1,
        "custom_named_input_connections": (
            connection_diagnostic["custom_input_names_from_editor_api"] == ["RawRGB", "Lighting"]
            and connection_diagnostic["custom_input_nodes_from_editor_api"] == [sample_path, active_path]
            and [entry["input_name"] for entry in connection_diagnostic["custom_inputs"]]
            == ["RawRGB", "Lighting"]),
        "sample_texture": sample_texture == texture,
        "sample_sampler": sample_sampler == unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR,
        "custom_code_exact": actual_code == original_upgrade.CODE,
        "emissive_custom_connection": len(customs) == 1 and emissive == customs[0],
        "blend_mode": blend == unreal.BlendMode.BLEND_OPAQUE,
        "shading_model": shading == unreal.MaterialShadingModel.MSM_UNLIT,
        "two_sided": not bool(two_sided),
    }
    diagnostic["material_graph"] = {
        "expression_count": {"expected": 4, "actual": len(expressions),
                             "basis": "Exact known three-node upgrade graph plus the pinned legacy disconnected VectorParameter_0"},
        "expression_types": [x.get_class().get_name() for x in expressions],
        "connection_diagnostic": connection_diagnostic,
        "source_color_float32": expected_color,
        "expected_node_paths_and_types": expected_nodes,
        "texture_samples": [{"texture": (x.get_editor_property("texture").get_path_name()
                                           if x.get_editor_property("texture") else None),
                              "sampler": str(x.get_editor_property("sampler_type"))} for x in samples],
        "unity_sprite_color_count": len(colors),
        "custom_expression_count": len(customs),
        "custom_code": {"expected_sha256": hashlib.sha256(original_upgrade.CODE.encode("utf-8")).hexdigest(),
                        "expected_length": len(original_upgrade.CODE),
                        "actual_sha256": (hashlib.sha256(actual_code_text.encode("utf-8")).hexdigest()
                                          if actual_code_text is not None else None),
                        "actual_length": len(actual_code_text) if actual_code_text is not None else None,
                        "actual_text": actual_code_text},
        "emissive_input": {"expected": customs[0].get_path_name() if len(customs) == 1 else None,
                           "actual": emissive.get_path_name() if emissive else None},
        "blend_mode": {"expected": str(unreal.BlendMode.BLEND_OPAQUE), "actual": str(blend)},
        "shading_model": {"expected": str(unreal.MaterialShadingModel.MSM_UNLIT), "actual": str(shading)},
        "two_sided": {"expected": False, "actual": bool(two_sided)},
        "checks": checks,
    }
    mismatches.extend(f"material graph {name}" for name, passed in checks.items() if not passed)
    diagnostic["mismatches"] = mismatches
    if mismatches:
        raise RuntimeError("F migrated material/dependency graph differs: " + "; ".join(mismatches))
    return mesh, material


def validate_actor(unreal, actor, row):
    label = row["label"]
    if not isinstance(actor, unreal.StaticMeshActor) or actor.get_actor_label() != label:
        raise RuntimeError(f"F actor class/label drift: {label}")
    components = actor.get_components_by_class(unreal.StaticMeshComponent)
    if len(components) != 1:
        raise RuntimeError(f"F actor must have one mesh component: {label}")
    component = components[0]
    mesh = component.get_editor_property("static_mesh")
    material = component.get_material(0)
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    if (sorted(str(x) for x in actor.get_editor_property("tags")) != sorted(row["tags"])
            or str(actor.get_folder_path()) != row["folder"]
            or mesh is None or mesh.get_path_name() != row["mesh"]
            or component.get_num_materials() != 1
            or material is None or material.get_path_name() != row["material"]
            or not same_vector([location.x, location.y, location.z], row["location"])
            or not all(same_rotation(a, b) for a, b in zip(
                [rotation.pitch, rotation.yaw, rotation.roll], row["rotation"]))
            or not same_vector([scale.x, scale.y, scale.z], row["scale"])
            or component.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
            or component.get_editor_property("cast_shadow") is not False
            or component.get_editor_property("translucency_sort_priority") != 0):
        raise RuntimeError(f"Existing F actor differs from source-bound immutable plan: {label}")
    return {"label": label, "source_key": row["source_key"], "actor_path": actor.get_path_name(),
            "mesh": mesh.get_path_name(), "material": material.get_path_name()}


def live_inventory(unreal, rows):
    expected = {row["label"]: row for row in rows}
    found = {}
    unrelated = set()
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        label = actor.get_actor_label()
        tags = {str(x) for x in actor.get_editor_property("tags")}
        components = actor.get_components_by_class(unreal.StaticMeshComponent)
        has_f_mesh = any((mesh := component.get_editor_property("static_mesh")) is not None
                         and mesh.get_path_name() == BODY_MESH for component in components)
        is_f = (label in expected or label.endswith("_build_mountain_F")
                or f"UnityPrefab_{PREFAB_ID}" in tags or has_f_mesh)
        if not is_f:
            unrelated.add(actor.get_path_name())
            continue
        if label not in expected or label in found:
            raise RuntimeError(f"Unexpected or duplicate F actor in target map: {label}")
        found[label] = validate_actor(unreal, actor, expected[label])
    return {"existing": [found[k] for k in sorted(found)],
            "missing_source_keys": [expected[k]["source_key"] for k in sorted(expected) if k not in found],
            "missing_labels": [k for k in sorted(expected) if k not in found],
            "unrelated_actor_paths": unrelated}


def spawn_missing_actor(unreal, row, mesh, material):
    location = row["location"]
    rotation = row["rotation"]
    scale = row["scale"]
    # Unreal Python Rotator constructor order is roll, pitch, yaw.
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location),
        unreal.Rotator(rotation[2], rotation[0], rotation[1]))
    if actor is None:
        raise RuntimeError(f"Failed to spawn missing F actor: {row['label']}")
    try:
        actor.set_actor_label(row["label"])
        actor.set_folder_path(row["folder"])
        actor.set_actor_scale3d(unreal.Vector(*scale))
        actor.tags = row["tags"]
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component is None:
            raise RuntimeError(f"Spawned F actor lacks mesh component: {row['label']}")
        component.set_static_mesh(mesh)
        if component.get_num_materials() != 1:
            raise RuntimeError(f"Spawned F body lacks exact one-slot mesh: {row['label']}")
        component.set_material(0, material)
        component.set_collision_profile_name("NoCollision")
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("cast_shadow", False)
        component.set_editor_property("translucency_sort_priority", 0)
        for name in ("affect_distance_field_lighting", "affect_dynamic_indirect_lighting",
                     "receives_decals", "use_as_occluder", "can_ever_affect_navigation"):
            try:
                component.set_editor_property(name, False)
            except Exception:
                unreal.log_warning(f"Optional F component flag unavailable: {name}")
    except Exception:
        # Registering with the outer rollback happens only on return. Remove
        # this new actor here if any configuration step failed before return.
        unreal.EditorLevelLibrary.destroy_actor(actor)
        raise
    return actor


def run_live(apply=False, expected_preflight_sha256=None):
    """Inventory or restore missing F actors inside the exact UE editor/map only.

    This intentionally has no map save. A failed apply rolls back only actors
    spawned by this call, preserving all pre-existing actors and F assets.
    """
    import unreal

    plan = build_preflight()
    report_path = REPORT_DIR / ("MountainFInstancesApply.json" if apply else "MountainFInstancesLivePreflight.json")
    report = {"schema": "rok.mountain_f_instances.live.v1", "passed": False,
              "apply_requested": bool(apply), "map_saved": False,
              "project": str(ROOT), "target_level": TARGET_LEVEL,
              "expected_actor_count": 18, "spawned_labels": [], "failures": []}
    spawned = []
    try:
        project = Path(unreal.Paths.project_dir()).resolve()
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        actual_level = str(world.get_outermost().get_name()) if world else None
        if project != EXPECTED_ROOT or actual_level != TARGET_LEVEL:
            raise RuntimeError(f"UE identity mismatch: project={project}, level={actual_level}")
        report["actual_project"] = str(project)
        report["actual_level"] = actual_level
        if apply:
            if (not expected_preflight_sha256 or not PREFLIGHT_REPORT.is_file()
                    or sha256(PREFLIGHT_REPORT) != expected_preflight_sha256
                    or read_json(PREFLIGHT_REPORT, "rok.mountain_f_instances.preflight.v1") != plan):
                raise RuntimeError("Apply requires reviewed exact offline preflight SHA and unchanged plan")
            report["reviewed_preflight_sha256"] = expected_preflight_sha256
        report["live_asset_diagnostic"] = {}
        mesh, material = check_live_assets(unreal, plan, report["live_asset_diagnostic"])
        before = live_inventory(unreal, plan["expected_actors"])
        report["existing_before"] = before["existing"]
        report["missing_before"] = before["missing_source_keys"]
        report["immutable_ue_dependencies"] = plan["immutable_ue_dependencies"]
        report["target_map_file_before"] = plan["target_map_file"]
        if apply:
            rows = {row["label"]: row for row in plan["expected_actors"]}
            for label in before["missing_labels"]:
                spawned.append(spawn_missing_actor(unreal, rows[label], mesh, material))
                report["spawned_labels"].append(label)
        after = live_inventory(unreal, plan["expected_actors"])
        if (after["unrelated_actor_paths"] != before["unrelated_actor_paths"]
                or after["missing_labels"] != ([] if apply else before["missing_labels"])):
            raise RuntimeError("F-only actor scope or post-inventory changed unexpectedly")
        report["existing_after"] = after["existing"]
        report["missing_after"] = after["missing_source_keys"]
        if build_preflight() != plan:
            raise RuntimeError("F source, product, map, or immutable asset hash changed during live operation")
        report["passed"] = True
        report["status"] = "spawned_in_editor_unsaved" if spawned else "no_op_or_read_only"
        report["next_step"] = ("Review live actor bindings and save target map separately"
                               if spawned else "No map save needed from this operation")
    except Exception as error:
        for actor in reversed(spawned):
            unreal.EditorLevelLibrary.destroy_actor(actor)
        report["failures"].append(str(error))
        report["rolled_back_spawned_count"] = len(spawned)
        write_report(report_path, report)
        raise
    write_report(report_path, report)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preflight", action="store_true", help="Pure offline, read-only source and asset preflight")
    parser.add_argument("--report", action="store_true", help="Also write a generated Saved report")
    args = parser.parse_args()
    if not args.preflight:
        parser.error("Explicit --preflight required; live operations use run_live() inside the verified UE editor")
    try:
        report = build_preflight()
    except Exception as error:
        report = {"schema": "rok.mountain_f_instances.preflight.v1", "passed": False,
                  "read_only": True, "project": str(ROOT), "target_level": TARGET_LEVEL,
                  "failures": [str(error)]}
        if args.report:
            write_report(PREFLIGHT_REPORT, report)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        raise SystemExit(1)
    if args.report:
        report["report_sha256"] = write_report(PREFLIGHT_REPORT, report)
    print(json.dumps({"passed": report["passed"], "read_only": report["read_only"],
                      "expected_actor_count": report["expected_actor_count"],
                      "runtime_evidence": report["runtime_evidence"],
                      "report": str(PREFLIGHT_REPORT) if args.report else None,
                      "report_sha256": report.get("report_sha256")}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
