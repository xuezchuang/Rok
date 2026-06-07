import json
import os

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
MANIFEST = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_runtime_city_all_buildings_scene_manifest.json")
OUTPUT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_runtime_building_texture_assets_inspect.json")


def load_asset(path):
    return unreal.EditorAssetLibrary.load_asset(path)


def inspect_textures():
    with open(MANIFEST, "r", encoding="utf-8-sig") as handle:
        manifest = json.load(handle)
    records = []
    for item in manifest.get("building_actors", []):
        texture_source = item.get("baked_texture_path", "")
        stem = os.path.splitext(os.path.basename(texture_source))[0]
        asset_name = "T_RokUnityRuntimeBuilding_{0}".format(stem)
        package_path = "/Game/RokUnityRuntimeCityAllBuildings/Textures/{0}.{0}".format(asset_name)
        texture = load_asset(package_path)
        record = {
            "actor": item.get("actor"),
            "source": texture_source,
            "texture_asset": package_path,
            "exists": texture is not None,
        }
        if texture:
            for prop in [
                "has_alpha_channel",
                "compression_settings",
                "srgb",
                "mip_gen_settings",
                "never_stream",
                "address_x",
                "address_y",
            ]:
                try:
                    record[prop] = str(texture.get_editor_property(prop))
                except Exception:
                    pass
            if hasattr(texture, "blueprint_get_size_x"):
                record["size"] = [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())]
        records.append(record)
    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    with open(OUTPUT, "w", encoding="utf-8") as handle:
        json.dump({"records": records}, handle, ensure_ascii=False, indent=2)
    missing_alpha = [r for r in records if r.get("exists") and r.get("has_alpha_channel") in ("False", "false", "0")]
    unreal.log("Runtime building textures inspected: {0}; missing_alpha={1}".format(len(records), len(missing_alpha)))


if __name__ == "__main__":
    inspect_textures()
