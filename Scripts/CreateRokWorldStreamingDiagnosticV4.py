"""UE 5.8 editor-only isolated world-streaming diagnostic asset/map creator.

Default is strict read-only preflight. ROK_WORLD_STREAM_DIAG_APPLY=1 creates only
the two new target packages; it never saves the source world map or imports a
missing LOD2+ dependency. Run only after reviewing/applying the staged C++ patch.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
from pathlib import Path
from datetime import datetime, timezone

import unreal

ROOT = Path("D:/ueDemo/Rok")
SOURCE_JSON = ROOT / "Saved/RokUnityMigration/unity_world_map_streaming_data.json"
SOURCE_ASSETS = ROOT / "Saved/RokUnityMigration/unity_world_map_streaming_assets.json"
SOURCE_MAP = ROOT / "Content/Maps/L_RokUnityWorldMapPrototype.umap"
INVENTORY = ROOT / "Saved/PendingUnityWorldStreamingDiagnosticV4/dependency_inventory.json"
INVENTORY_SHA = "32757f8cd41571dccdf53192df84407f72da3a3b361ef2844ddb863d3b63424d"
DATA_PACKAGE = "/Game/RokDiagnostics/WorldStreaming/V1/DA_RokWorldStreamSourceLod01"
MAP_PACKAGE = "/Game/Maps/L_RokWorldStreamingDiagnosticV1"
SOURCE_HASHES = {
    SOURCE_JSON: "72dd593850c582b4358a1f86f347391ea135f6f3edc25a50da7222daac2c3ba2",
    SOURCE_ASSETS: "29b23d360f0f8622231e3cf01a4a591cea6f126ac200dfb867abef327066b657",
    SOURCE_MAP: "2f7b32d4506d5fd1cfeca8b8c9a36440efe988a51646bc701dcb46915ab27a86",
}
EXPECTED_DEPENDENCIES = 243
EXPECTED_BUDGET = {"lod0_initial_instances": 848,
                   "lod0_moved_instances": 1688,
                   "lod1_moved_instances": 1688}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def assign(value, property_name, property_value):
    value.set_editor_property(property_name, property_value)
    return value


def vec3(row):
    return unreal.Vector(float(row["X"]), float(row["Y"]), float(row["Z"]))


def transform(row):
    result = unreal.Transform()
    rotation = row["Rotation"]
    assign(result, "rotation", unreal.Quat(float(rotation["X"]), float(rotation["Y"]),
                                             float(rotation["Z"]), float(rotation["W"])))
    assign(result, "translation", vec3(row["Translation"]))
    assign(result, "scale3d", vec3(row["Scale3D"]))
    return result


def import_soft_references(value, mesh_path, material_paths):
    # StructBase.import_text delegates FSoftObjectProperty parsing to UE and
    # preserves paths even when the referenced packages are not loaded.
    def validate_path(path):
        if path is None:
            return "None"
        require(isinstance(path, str) and re.fullmatch(
            r"/Game(?:/[A-Za-z0-9_-]+)+\.[A-Za-z0-9_-]+", path),
            "soft object path is outside the strict /Game/Asset.Asset grammar: " + repr(path))
        package, object_name = path.rsplit("/", 1)[-1].split(".", 1)
        require(package == object_name, "soft object package/object names differ: " + path)
        return '"' + path + '"'

    mesh_text = validate_path(mesh_path)
    material_text = "(" + ",".join(validate_path(path) for path in material_paths) + ")"
    text = "(Mesh=" + mesh_text + ",Materials=" + material_text + ")"
    require(value.import_text(text), "UE reflection text import rejected soft object references")
    exported = value.export_text()
    mesh_match = re.search(r"(?:^|[,\(])Mesh=(None|\"[^\"]*\")(?=[,\)])", exported)
    materials_match = re.search(r"(?:^|[,\(])Materials=\(([^)]*)\)(?=[,\)])", exported)
    require(mesh_match is not None and materials_match is not None,
            "UE exported struct text omitted Mesh or Materials fields")
    exported_mesh = mesh_match.group(1)
    expected_mesh = "None" if mesh_path is None else '"' + mesh_path + '"'
    tokens = re.findall(r'"[^"\\]*"|None', materials_match.group(1))
    require(",".join(tokens) == materials_match.group(1),
            "UE exported material array contains unsupported text syntax")
    expected_materials = ["None" if path is None else '"' + path + '"'
                          for path in material_paths]
    require(exported_mesh == expected_mesh,
            "Mesh soft path did not roundtrip exactly: " + repr((expected_mesh, exported_mesh)))
    require(tokens == expected_materials,
            "Materials soft paths/None slots/order did not roundtrip exactly")
    return value


def package_file(package):
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def validated_inputs():
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(),
            "wrong UE project; no writes permitted")
    require(str(unreal.SystemLibrary.get_engine_version()).startswith("5.8"),
            "only reviewed UE 5.8 editor is permitted")
    for path, expected in SOURCE_HASHES.items():
        require(path.is_file() and sha(path) == expected, f"source/input SHA drift: {path}")
    require(INVENTORY.is_file() and sha(INVENTORY) == INVENTORY_SHA,
            "reviewed LOD0/1 dependency inventory drift")
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    require(not unreal.EditorAssetLibrary.does_asset_exist(DATA_PACKAGE)
            and not unreal.EditorAssetLibrary.does_asset_exist(MAP_PACKAGE)
            and not package_file(DATA_PACKAGE).exists()
            and not (ROOT / "Content/Maps/L_RokWorldStreamingDiagnosticV1.umap").exists(),
            "a diagnostic target already exists; verify it separately, never overwrite")
    for class_path in ("/Script/Rok.RokWorldStreamingDiagnosticGameMode",
                       "/Script/Rok.RokWorldMapConfigActor",
                       "/Script/Rok.RokWorldMapStreamingActor"):
        require(unreal.load_class(None, class_path),
                f"compiled diagnostic class unavailable before writes: {class_path}")
    data = json.loads(SOURCE_JSON.read_bytes())
    require(data["Schema"] == "rok.world_map_runtime_data_asset.v1"
            and data["CanonicalPayloadSha256"] ==
            "09298c671a05b3c220e24b25c7c7362c4d7f0cff1218a29d7c305e7471f86b6f"
            and data["SourceManifestSha256"] ==
            "fdce978db9b7d6be4c1fcb2da3a24f30bb49835acc16698fc54ccbfa747b7ad0"
            and data["bStaticVisualReady"] is True
            and data["bRuntimeReady"] is False
            and len(data["Tiles"]) == 1600
            and len(data["TileTypes"]) == 66
            and len(data["Recipes"]) == 396,
            "full source tile/recipe/policy identity drift")
    require([(r["Lod"], r["ChunkTileSpan"]) for r in data["LodLevels"][:2]]
            == [(0, 2), (1, 2)], "source LOD0/1 chunk span drift")
    dependencies = set()
    lod2_dependencies = set()
    for recipe in data["Recipes"]:
        if recipe["Lod"] in (0, 1, 2):
            for part in recipe["Parts"]:
                target = dependencies if recipe["Lod"] < 2 else lod2_dependencies
                require(part["Mesh"], "source static recipe has null LOD0/1/2 mesh")
                target.add(part["Mesh"])
                target.update(value for value in part["Materials"] if value)
    require(len(dependencies) == EXPECTED_DEPENDENCIES, "source LOD0/1 dependency count drift")
    require({row["object_path"] for row in inventory["lod01"]} == dependencies
            and len(inventory["lod01"]) == EXPECTED_DEPENDENCIES,
            "source LOD0/1 dependency set differs from reviewed inventory")
    inventory_by_path = {row["object_path"]: row for row in inventory["lod01"]}
    missing_lod2 = sorted(path for path in lod2_dependencies
                          if not package_file(path.split(".")[0]).is_file())
    require(len(missing_lod2) == 249
            and missing_lod2 == inventory["missing_lod2"],
            "LOD2 missing dependency set changed; review before any diagnostic import")
    for path in sorted(dependencies):
        filename = package_file(path.split(".")[0])
        require(filename.is_file(), f"LOD0/1 source dependency missing: {path}")
        require(sha(filename) == inventory_by_path[path]["uasset_sha256"],
                f"reviewed LOD0/1 dependency bytes changed: {path}")
        value = unreal.load_asset(path)
        expected_class = unreal.StaticMesh if "/Meshes/" in path else unreal.MaterialInterface
        require(value and isinstance(value, expected_class),
                f"LOD0/1 dependency did not load as expected class: {path}")
    # Build and validate every nested record before any package write. This also
    # exercises loaded, absent, mixed, None-slot, and multi-material mappings.
    assembled = assemble_source_records(data)
    probe_boolean_property_names()
    return data, sorted(dependencies), assembled


def assemble_source_records(data):
    projection = unreal.RokWorldMapProjection()
    for key, value in data["Projection"].items():
        if isinstance(value, dict):
            value = unreal.Vector2D(float(value["X"]), float(value["Y"]))
        assign(projection, key, value)
    levels = []
    for row in data["LodLevels"]:
        item = unreal.RokWorldMapLodLevel()
        assign(item, "lod", row["Lod"])
        assign(item, "max_dxf_inclusive", row["MaxDxfInclusive"])
        assign(item, "chunk_tile_span", row["ChunkTileSpan"])
        levels.append(item)
    tiles = []
    for row in data["Tiles"]:
        item = unreal.RokWorldMapTileRecord()
        assign(item, "tile_x", row["TileX"])
        assign(item, "tile_y", row["TileY"])
        assign(item, "tile_type_id", row["TileTypeId"])
        assign(item, "template_to_world", transform(row["TemplateToWorld"]))
        tiles.append(item)
    types = []
    for row in data["TileTypes"]:
        item = unreal.RokWorldMapTileTypeRecord()
        assign(item, "tile_type_id", row["TileTypeId"])
        bindings = []
        for source in row["LodBindings"]:
            binding = unreal.RokWorldMapLodBinding()
            assign(binding, "lod", source["Lod"])
            assign(binding, "recipe_id", source["RecipeId"])
            bindings.append(binding)
        assign(item, "lod_bindings", bindings)
        types.append(item)
    recipes = []
    for row in data["Recipes"]:
        item = unreal.RokWorldMapRenderRecipe()
        assign(item, "recipe_id", row["RecipeId"])
        assign(item, "tile_type_id", row["TileTypeId"])
        assign(item, "lod", row["Lod"])
        parts = []
        for source in row["Parts"]:
            part = unreal.RokWorldMapRenderPart()
            import_soft_references(part, source["Mesh"], source["Materials"])
            assign(part, "local_transform", transform(source["LocalTransform"]))
            assign(part, "translucency_sort_priority", source["TranslucencySortPriority"])
            assign(part, "tree", source["bTree"])
            assign(part, "render_flags", source["RenderFlags"])
            parts.append(part)
        assign(item, "parts", parts)
        recipes.append(item)
    policies = []
    for row in data["PendingPolicies"]:
        item = unreal.RokWorldMapPendingPolicy()
        assign(item, "policy_id", row["PolicyId"])
        status_names = {"Pending": "PENDING", "SourceExplicitNoOp": "SOURCE_EXPLICIT_NO_OP",
                        "AcceptedOmission": "ACCEPTED_OMISSION", "Promoted": "PROMOTED"}
        require(row["Status"] in status_names, "unreviewed policy enum")
        assign(item, "status", getattr(unreal.RokWorldMapPendingPolicyStatus,
                                       status_names[row["Status"]]))
        assign(item, "source_component_count", row["SourceComponentCount"])
        assign(item, "required_decision", row["RequiredDecision"])
        policies.append(item)
    # Mapping probes for each source path shape happen on temporary structs only.
    existing_mesh = next(path for path in data_paths(data) if "/Meshes/" in path)
    existing_material = next(path for path in data_paths(data) if "/Materials/" in path)
    missing_paths = json.loads(INVENTORY.read_text(encoding="utf-8"))["missing_lod2"]
    missing_mesh = next(path for path in missing_paths if "/Meshes/" in path)
    missing_material = next(path for path in missing_paths if "/Materials/" in path)
    cases = [(existing_mesh, [existing_material]),
             (missing_mesh, [missing_material]),
             (existing_mesh, [missing_material, None, existing_material]),
             (existing_mesh, [existing_material, existing_material])]
    for mesh_path, material_paths in cases:
        import_soft_references(unreal.RokWorldMapRenderPart(), mesh_path, material_paths)
    import_soft_references(unreal.RokWorldMapRenderPart(), None, [None, existing_material, None])
    for bad_path in ('/Game/A.A,Injected=True', '/Game/A".A"', '/Engine/A.A'):
        rejected = False
        try:
            import_soft_references(unreal.RokWorldMapRenderPart(), bad_path, [])
        except RuntimeError:
            rejected = True
        require(rejected, "unsafe/non-Game soft path was not rejected: " + bad_path)
    return projection, levels, tiles, types, recipes, policies


def data_paths(data):
    for recipe in data["Recipes"]:
        for part in recipe["Parts"]:
            if part["Mesh"]:
                yield part["Mesh"]
            yield from (path for path in part["Materials"] if path)


def probe_boolean_property_names():
    # Struct/transient-object setters are safe to exercise; actor CDOs remain
    # read-only, so those names are verified by getters without global mutation.
    report = {"schema": "rok.world_creator_bool_property_mapping.v1",
              "status": "in_progress", "properties": {}}

    def roundtrip(value, python_name, cpp_name):
        original = bool(value.get_editor_property(python_name))
        requested = not original
        assign(value, python_name, requested)
        observed = bool(value.get_editor_property(python_name))
        assign(value, python_name, original)
        require(observed == requested
                and bool(value.get_editor_property(python_name)) == original,
                f"bool property setter/getter mismatch: {python_name} ({cpp_name})")
        report["properties"][python_name] = {
            "cpp_field": cpp_name, "python_property": python_name,
            "set_get_passed": True, "restored_original": True}

    roundtrip(unreal.RokWorldMapRenderPart(), "tree", "bTree")
    data_asset = unreal.new_object(unreal.RokWorldMapRuntimeDataAsset)
    require(data_asset is not None, "transient runtime DataAsset construction failed")
    roundtrip(data_asset, "static_visual_ready", "bStaticVisualReady")
    roundtrip(data_asset, "runtime_ready", "bRuntimeReady")
    for class_type, python_name, cpp_name in (
            (unreal.RokWorldMapConfigActor, "enter_player_city_on_zoom", "bEnterPlayerCityOnZoom"),
            (unreal.RokWorldMapStreamingActor, "enable_scoped_diagnostic_lod01",
             "bEnableScopedDiagnosticLod01")):
        cdo = unreal.get_default_object(class_type)
        require(cdo is not None, "actor CDO unavailable: " + cpp_name)
        value = bool(cdo.get_editor_property(python_name))
        report["properties"][python_name] = {
            "cpp_field": cpp_name, "python_property": python_name,
            "cdo_read_passed": True, "cdo_value": value, "cdo_mutated": False}
    report["status"] = "passed"
    suffix = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    output = ROOT / f"Saved/PendingUnityWorldStreamingCreatorMappingV8/bool_property_probe_{suffix}.json"
    with output.open("x", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)
    return report


def make_asset(data, assembled):
    projection, levels, tiles, types, recipes, policies = assembled
    factory = unreal.DataAssetFactory()
    assign(factory, "data_asset_class", unreal.RokWorldMapRuntimeDataAsset)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        DATA_PACKAGE.rsplit("/", 1)[1], DATA_PACKAGE.rsplit("/", 1)[0],
        unreal.RokWorldMapRuntimeDataAsset, factory)
    require(asset, "DataAssetFactory could not create scoped runtime data")
    for key in ("Schema", "CanonicalPayloadSha256", "SourceManifestPath", "SourceManifestSha256",
                "MapWidthTiles", "MapHeightTiles", "TileSizeUnityUnits"):
        assign(asset, key, data[key])
    assign(asset, "projection", projection)
    assign(asset, "lod_levels", levels)
    assign(asset, "tiles", tiles)
    assign(asset, "tile_types", types)
    assign(asset, "recipes", recipes)
    assign(asset, "pending_policies", policies)
    # Full source records are retained. Static-ready is false because LOD2+ assets
    # remain unimported; only the named runtime diagnostic explicitly bypasses it.
    assign(asset, "static_visual_ready", False)
    assign(asset, "runtime_ready", False)
    assign(asset, "readiness_blockers", data["ReadinessBlockers"]
           + ["diagnostic_lod01_only_lod2plus_assets_not_promoted"])
    require(len(asset.get_editor_property("tiles")) == 1600
            and len(asset.get_editor_property("recipes")) == 396
            and not asset.get_editor_property("static_visual_ready")
            and not asset.get_editor_property("runtime_ready"),
            "DataAsset field readback failed before save")
    require(unreal.EditorAssetLibrary.save_loaded_asset(asset), "DataAsset save failed")
    return asset


def make_map(asset):
    require(unreal.EditorLevelLibrary.new_level(MAP_PACKAGE), "isolated map creation failed")
    world = unreal.EditorLevelLibrary.get_editor_world()
    require(world and str(world.get_outermost().get_name()) == MAP_PACKAGE,
            "new map identity mismatch; do not save")
    game_mode = unreal.load_class(None, "/Script/Rok.RokWorldStreamingDiagnosticGameMode")
    config_class = unreal.load_class(None, "/Script/Rok.RokWorldMapConfigActor")
    stream_class = unreal.load_class(None, "/Script/Rok.RokWorldMapStreamingActor")
    require(game_mode and config_class and stream_class, "compiled diagnostic runtime classes missing")
    world.get_world_settings().set_editor_property("default_game_mode", game_mode)
    config = unreal.EditorLevelLibrary.spawn_actor_from_class(config_class, unreal.Vector(), unreal.Rotator())
    config.set_actor_label("RokWorldMap_SourceConfig_Diagnostic")
    config.tags = ["RokWorldStreamingDiagnostic", "RokWorldMapConfig", "SourceFocus_6876_942"]
    assign(config, "enter_player_city_on_zoom", False)
    start = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(0, -18000, 18000), unreal.Rotator())
    start.set_actor_label("PlayerStart_RokWorldStreamingDiagnostic")
    stream = unreal.EditorLevelLibrary.spawn_actor_from_class(stream_class, unreal.Vector(), unreal.Rotator())
    stream.set_actor_label("RokWorldMap_SourceStreaming_Diagnostic")
    stream.tags = ["RokWorldStreamingDiagnostic", "NoPromotion"]
    assign(stream, "runtime_data", asset)
    assign(stream, "enable_scoped_diagnostic_lod01", True)
    require(unreal.EditorLevelLibrary.save_current_level(), "diagnostic map save failed")
    return world


def main():
    data, dependencies, assembled = validated_inputs()
    if os.environ.get("ROK_WORLD_STREAM_DIAG_APPLY") != "1":
        unreal.log("World stream diagnostic PRECHECK_ONLY: full source data and 243 LOD0/1 assets validated; no writes")
        return
    command_tokens = str(unreal.SystemLibrary.get_command_line()).lower().split()
    require("-roklocalonly" in command_tokens,
            "asset creation requires explicit -RokLocalOnly before any package write")
    asset = make_asset(data, assembled)
    make_map(asset)
    for path, expected in SOURCE_HASHES.items():
        require(sha(path) == expected, f"protected source changed during creation: {path}")
    require(package_file(DATA_PACKAGE).is_file()
            and (ROOT / "Content/Maps/L_RokWorldStreamingDiagnosticV1.umap").is_file(),
            "new packages were not saved on disk")
    result = {"schema": "rok.world_streaming_diagnostic_editor_create.v4",
              "status": "created_pending_runtime_acceptance", "source_json_sha256": sha(SOURCE_JSON),
              "creation_rok_local_only_flag": True,
              "runtime_required_args": ["-RokWorldStreamingDiagnostic", "-RokLocalOnly"],
              "source_map_sha256_before": SOURCE_HASHES[SOURCE_MAP],
              "runtime_data_asset": DATA_PACKAGE, "diagnostic_map": MAP_PACKAGE,
              "lod01_dependency_count": len(dependencies),
              "dependency_inventory_sha256": INVENTORY_SHA,
              "data_asset_uasset_sha256": sha(package_file(DATA_PACKAGE)),
              "diagnostic_map_umap_sha256": sha(ROOT / "Content/Maps/L_RokWorldStreamingDiagnosticV1.umap"),
              "lod2_missing_dependency_count": 249,
              "expected_budget": EXPECTED_BUDGET,
              "static_visual_ready": False, "runtime_ready": False,
              "scope": "new isolated map and source-data asset; no old map or assets saved"}
    folder = ROOT / "Saved/RokUnityMigration/WorldStreamingDiagnosticCreateV4"
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / ("editor_create_" +
                     datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ") + ".json")
    with path.open("x", encoding="utf-8") as handle:
        json.dump(result, handle, ensure_ascii=False, indent=2)
    unreal.log(f"World stream diagnostic created: {path}")


main()
