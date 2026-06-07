import json
import os
import re

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
ROK_UNITY_ROOT = os.environ.get("ROK_UNITY_ROOT", r"D:\ueDemo\Rok-unity")
FBX_ROOT = os.path.join(ROK_UNITY_ROOT, "client-unity", "Assets", "BundleAssets", "Map", "FBX")
DESTINATION = "/Game/RokPrototype/Meshes/RokUnityBoundarySplitProbe"
OUTPUT_PATH = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "boundary_fbx_split_probe.json")


def safe_name(value):
    return re.sub(r"[^A-Za-z0-9_]+", "_", value).strip("_") or "Asset"


def configure_split_import_options():
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("automated_import_should_detect_type", False)
    static_data = options.get_editor_property("static_mesh_import_data")
    static_data.set_editor_property("combine_meshes", False)
    static_data.set_editor_property("transform_vertex_to_absolute", False)
    static_data.set_editor_property("bake_pivot_in_vertex", False)
    static_data.set_editor_property("generate_lightmap_u_vs", True)
    static_data.set_editor_property("auto_generate_collision", True)
    return options


def import_split_probe(filename):
    source_path = os.path.join(FBX_ROOT, filename)
    if not os.path.exists(source_path):
        return {"filename": filename, "missing": True}

    unreal.EditorAssetLibrary.make_directory(DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", DESTINATION)
    task.set_editor_property("destination_name", "Probe_{0}".format(safe_name(os.path.splitext(filename)[0])))
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", configure_split_import_options())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    imported = []
    for path in task.get_editor_property("imported_object_paths"):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            bounds = asset.get_bounds()
            imported.append(
                {
                    "path": path,
                    "name": asset.get_name(),
                    "origin": [bounds.origin.x, bounds.origin.y, bounds.origin.z],
                    "extent": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z],
                }
            )
    return {"filename": filename, "imported": imported}


results = [
    import_split_probe("CityWall_101_1.FBX"),
    import_split_probe("CityGate_101_1.FBX"),
    import_split_probe("CityTower_101_1_1.FBX"),
    import_split_probe("CityTower_101_1_2.FBX"),
    import_split_probe("CityTower_101_1_3.FBX"),
    import_split_probe("CityTower_101_1_4.FBX"),
    import_split_probe("CityTower_101_1_5.FBX"),
    import_split_probe("CityTower_101_1_7.FBX"),
    import_split_probe("CityTower_101_1_8.FBX"),
]

os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
with open(OUTPUT_PATH, "w", encoding="utf-8") as handle:
    json.dump(results, handle, ensure_ascii=False, indent=2)

unreal.log("Wrote Rok Unity boundary FBX split probe to {0}".format(OUTPUT_PATH))
