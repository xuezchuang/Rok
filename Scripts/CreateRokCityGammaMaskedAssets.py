"""UE5.8 editor-only importer for prepared city Gamma masked GPU texture inputs.

Set ROK_CITY_GAMMA_MASKED_MANIFEST to an exact prepared manifest. The default
is source/target preflight with no asset/map save. Set
ROK_CITY_GAMMA_MASKED_APPLY=1 to import only absent dedicated Texture2D assets.
No existing texture is reimported, overwritten or repaired by this script.
"""

import hashlib
import json
import os
from datetime import datetime, timezone
from pathlib import Path

import unreal

import PrepareRokCityGammaMaskedAssets as prepared


ROOT = prepared.ROOT
WORLD = "/Game/Maps/L_RokUnityGameAppCity"
MANIFEST_ENV = "ROK_CITY_GAMMA_MASKED_MANIFEST"
APPLY_ENV = "ROK_CITY_GAMMA_MASKED_APPLY"
DEST = prepared.DEST
META_PREFIX = "RokCityGammaMasked"


def need(value, message):
    if not value:
        raise RuntimeError(message)


def sha_file(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def identity():
    project = Path(unreal.Paths.project_dir()).resolve()
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = subsystem.get_editor_world() if subsystem else None
    actual = world.get_path_name().split(".", 1)[0] if world else None
    need(project == ROOT.resolve() and actual == WORLD and world.get_name() == "L_RokUnityGameAppCity",
         f"wrong Gamma input project/world: {project} / {actual}")
    return world


def manifest():
    value = os.environ.get(MANIFEST_ENV, "")
    need(value, f"set {MANIFEST_ENV} to exact prepared manifest.json")
    path = Path(value).resolve()
    plan = prepared.verify_prepared(path)
    need(plan["destination_root"] == DEST and len(plan["rows"]) == 27 and
         len(plan["resources"]) == plan["counts"]["deduplicated_texture_assets"],
         "prepared asset plan scope differs")
    return path, plan


def source_png(run, item):
    path = (run / item["png"]).resolve()
    need(path.parent == (run / "textures").resolve() and path.is_file() and
         sha_file(path) == item["png_sha256"], f"prepared PNG changed: {path}")
    return path


def package_file(asset_path):
    need(asset_path.startswith(DEST + "/") and asset_path.count("/") == DEST.count("/") + 1,
         f"target escapes dedicated directory: {asset_path}")
    return ROOT / "Content" / (asset_path.removeprefix("/Game/") + ".uasset")


def texture_flags():
    return {
        "srgb": False,
        "compression_settings": unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "filter": unreal.TextureFilter.TF_BILINEAR,
        "address_x": unreal.TextureAddress.TA_CLAMP,
        "address_y": unreal.TextureAddress.TA_CLAMP,
        "never_stream": True,
    }


def metadata(item, native_sha, prepared_sha):
    aliases = item["source_aliases"]
    need(aliases and all(alias["source_guid"] and alias["native_png_sha256"]
                         for alias in aliases), "source aliases are incomplete")
    return {
        META_PREFIX + "Scope": "GPU-prefab-default-input-only;main-view-unbound;not-visual-accepted",
        META_PREFIX + "Role": item["role"],
        META_PREFIX + "BGRA8PixelSHA256": item["bgra8_pixel_sha256"],
        META_PREFIX + "RGBA8PixelSHA256": item["rgba8_pixel_sha256"],
        META_PREFIX + "RawGpuPngSHA256": item["png_sha256"],
        META_PREFIX + "NativeManifestSHA256": native_sha,
        META_PREFIX + "PreparedManifestSHA256": prepared_sha,
        META_PREFIX + "SourceAliases": json.dumps(aliases, sort_keys=True, separators=(",", ":")),
        META_PREFIX + "SourceMethod": "Unity2019 Gamma Graphics.Blit/ReadPixels GPU export; not original PNG bytes",
        META_PREFIX + "Dimensions": f"{item['width']}x{item['height']}",
    }


def verify_texture(asset, item, png, native_sha, prepared_sha):
    need(isinstance(asset, unreal.Texture2D), f"not Texture2D: {item['asset']}")
    need(asset.get_path_name().split(".", 1)[0] == item["asset"],
         f"texture package identity differs: {item['asset']}")
    for key, expected in texture_flags().items():
        need(asset.get_editor_property(key) == expected,
             f"{item['asset']} flag differs: {key}")
    for key, expected in metadata(item, native_sha, prepared_sha).items():
        need(str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key)) == str(expected),
             f"{item['asset']} metadata differs: {key}")
    source_data = asset.get_editor_property("asset_import_data")
    need(source_data is not None and hasattr(source_data, "get_first_filename"),
         f"{item['asset']} source import data unavailable")
    need(Path(source_data.get_first_filename()).resolve() == png,
         f"{item['asset']} was imported from a different file")
    need(sha_file(png) == item["png_sha256"], f"{item['asset']} source PNG changed")
    size = (int(asset.blueprint_get_size_x()), int(asset.blueprint_get_size_y()))
    if size != (item["width"], item["height"]):
        # UE5.8 can expose a 32x32 placeholder during asynchronous texture
        # compilation. Finish derived compilation without saving any asset.
        unreal.SystemLibrary.execute_console_command(identity(),
            "Editor.AsyncTextureCompilationFinishAll")
        size = (int(asset.blueprint_get_size_x()), int(asset.blueprint_get_size_y()))
    need(size == (item["width"], item["height"]),
         f"{item['asset']} loaded/compiled size differs: {size}")
    return {"asset": item["asset"], "size": list(size),
            "source_png_sha256": item["png_sha256"],
            "uasset_sha256": sha_file(package_file(item["asset"]))}


def existing_asset(item):
    path = item["asset"]
    loaded = unreal.load_asset(path)
    if loaded is None:
        need(not unreal.EditorAssetLibrary.does_asset_exist(path) and
             not package_file(path).exists(), f"unloadable existing texture: {path}")
    return loaded


def import_missing(item, png, native_sha, prepared_sha):
    path = item["asset"]
    need(existing_asset(item) is None, f"creation target already exists: {path}")
    task = unreal.AssetImportTask()
    values = {
        "filename": str(png), "destination_path": DEST,
        "destination_name": item["resource_id"], "factory": unreal.TextureFactory(),
        "automated": True, "replace_existing": False, "save": False,
    }
    for key, value in values.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(path)
    need(isinstance(asset, unreal.Texture2D), f"TextureFactory import did not create {path}")
    for key, value in texture_flags().items():
        asset.set_editor_property(key, value)
    for key, value in metadata(item, native_sha, prepared_sha).items():
        unreal.EditorAssetLibrary.set_metadata_tag(asset, key, str(value))
    need(unreal.EditorAssetLibrary.save_loaded_asset(asset), f"save failed: {path}")
    return asset


def run():
    identity()
    prepared_path, plan = manifest()
    prepared_sha = sha_file(prepared_path)
    native_sha = plan["source_native_sha256"]
    input_dir = prepared_path.parent
    expected_assets = {item["asset"] for item in plan["resources"]}
    need(len(expected_assets) == len(plan["resources"]), "duplicate target asset")
    # Reject foreign assets under the dedicated V1 directory, including a failed
    # partial import with an unexpected name, before performing any write.
    for asset in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True, include_folder=False):
        need(str(asset).split(".", 1)[0] in expected_assets,
             f"foreign asset in dedicated input folder: {asset}")
    checked = []
    missing = []
    for item in plan["resources"]:
        png = source_png(input_dir, item)
        asset = existing_asset(item)
        if asset is None:
            missing.append(item)
        else:
            checked.append(verify_texture(asset, item, png, native_sha, prepared_sha))
    if os.environ.get(APPLY_ENV) != "1":
        unreal.log(f"ROK_CITY_GAMMA_MASKED_READONLY_PREFLIGHT_PASS "
                   f"existing={len(checked)} missing={len(missing)} manifest={prepared_path}")
        return
    created = []
    for item in missing:
        png = source_png(input_dir, item)
        asset = import_missing(item, png, native_sha, prepared_sha)
        row = verify_texture(asset, item, png, native_sha, prepared_sha)
        row["created"] = True
        created.append(row)
    for row in checked:
        row["created"] = False
    report = {
        "schema": "rok.city_gamma_masked_inputs.ue_create.v1", "passed": True,
        "project": str(ROOT), "world": WORLD, "prepared_manifest": str(prepared_path),
        "prepared_manifest_sha256": prepared_sha, "native_manifest_sha256": native_sha,
        "asset_count": len(plan["resources"]), "created_count": len(created),
        "existing_verified_count": len(checked),
        "scope": "Texture2D GPU source inputs only; no mesh, material, actor, map, renderer registration or main-view visual acceptance",
        "rhi_limit": "Editor flags/size/import-source verified; actual PF_B8G8R8A8 RHI descriptor and GPU pixels require later runtime packet verification",
        "assets": sorted(checked + created, key=lambda value: value["asset"]),
    }
    name = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S_%f") + f"_{os.getpid()}_ue_create.json"
    path = input_dir / name
    with path.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    unreal.log(f"ROK_CITY_GAMMA_MASKED_ASSETS_READY {path}")


if __name__ == "__main__":
    run()
