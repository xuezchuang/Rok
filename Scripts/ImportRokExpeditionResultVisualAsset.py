"""Import only the sealed ui_result atlas into the verified Rok UE editor.

Read-only by default; set ROK_EXPEDITION_RESULT_IMPORT_APPLY=1 to create the
single new texture. Existing assets and maps are never saved by this script.
"""
from __future__ import annotations

import hashlib
import json
import os
from datetime import datetime, timezone
from pathlib import Path
from rok_data_paths import resolve


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "Saved/RokUnityMigration/ExpeditionVisualAssets/v2/result_visual_manifest.json"
MANIFEST_SHA256 = "7a05a0562374c05cc1de56768a12c09f05eaeaef3a2d36f3707d27398042be83"
TARGET = "/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_result_atlas"
DISK = ROOT / "Content/RokPrototype/UI/ExpeditionSource/rok_expedition_result_atlas.uasset"
REPORT_DIR = ROOT / "Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    import unreal

    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_name = world.get_name() if world else ""
    if project != ROOT.resolve() or world_name != "L_RokUnityGameAppCity":
        raise RuntimeError(f"Wrong editor identity: {project} / {world_name}")
    if digest(MANIFEST) != MANIFEST_SHA256:
        raise RuntimeError("Sealed result visual manifest changed")
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if manifest.get("schema") != "rok.expedition.result_visual_assets.v2":
        raise RuntimeError("Unexpected result source schema")
    atlas = manifest["atlas"]
    if atlas.get("name") != "ui_result" or tuple(atlas["size"]) != (512, 512):
        raise RuntimeError("Unexpected source atlas identity")
    source = resolve(atlas["source"])
    prepared = resolve(atlas["prepared"])
    expected_hash = atlas["sha256"]
    if not source.is_file() or not prepared.is_file():
        raise RuntimeError("Source or prepared result atlas missing")
    if digest(source) != expected_hash or digest(prepared) != expected_hash:
        raise RuntimeError("Result atlas bytes differ from sealed source")
    sprites = atlas.get("sprites", [])
    if len(sprites) != 1 or sprites[0].get("name") != "pl_result_1000_1" or \
            sprites[0].get("rect_png_top_left") != [1, 243, 454, 138] or \
            sprites[0].get("internal_file_id") != 2260871125762619715:
        raise RuntimeError("Result sprite identity/rect changed")
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET) or DISK.exists():
        raise RuntimeError(f"Refusing to overwrite existing asset: {TARGET}")

    if os.environ.get("ROK_EXPEDITION_RESULT_IMPORT_APPLY") != "1":
        print("READ_ONLY Expedition result atlas preflight passed:", TARGET)
        return

    task = unreal.AssetImportTask()
    task.filename = str(prepared)
    task.destination_path = TARGET.rsplit("/", 1)[0]
    task.destination_name = TARGET.rsplit("/", 1)[-1]
    task.factory = unreal.TextureFactory()
    task.automated = True
    task.replace_existing = False
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if getattr(task, "errors", None):
        raise RuntimeError(f"TextureFactory failed: {task.errors}")
    texture = unreal.EditorAssetLibrary.load_asset(TARGET)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Result atlas import did not create Texture2D")
    settings = (
        ("srgb", True),
        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
        ("never_stream", True),
        ("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON),
        ("address_x", unreal.TextureAddress.TA_CLAMP),
        ("address_y", unreal.TextureAddress.TA_CLAMP),
    )
    for key, value in settings:
        texture.set_editor_property(key, value)
        if texture.get_editor_property(key) != value:
            raise RuntimeError(f"Texture setting rejected: {key}")
    size = (int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y()))
    if size != (512, 512):
        raise RuntimeError(f"Result atlas size mismatch: {size}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture) or not DISK.is_file():
        raise RuntimeError("Result atlas save failed")
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    report_path = REPORT_DIR / ("expedition_result_ue_import_" +
        datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S_%f") + ".json")
    report = {
        "schema": "rok.expedition.result_visual_import.v2", "passed": True,
        "project": str(project), "world": world_name,
        "source_manifest": str(MANIFEST), "source_manifest_sha256": MANIFEST_SHA256,
        "source_png": str(prepared), "source_sha256": expected_hash,
        "target": TARGET, "uasset": str(DISK), "uasset_sha256": digest(DISK),
        "size": list(size), "texture_settings": {k: str(v) for k, v in settings},
        "map_saved": False, "engine_runtime_acceptance": False,
    }
    with report_path.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    print("Expedition result atlas imported:", report_path)


if __name__ == "__main__":
    main()
