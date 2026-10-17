"""Import the prepared BuildCity atlas in the explicitly verified Rok UE editor.

This script is intentionally fail-closed: it refuses an ambiguous manifest, wrong
project/world, modified source copy, or any pre-existing destination asset.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_PROJECT = ROOT.resolve()
EXPECTED_WORLD = "L_RokUnityGameAppCity"
TARGET = "/Game/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud"
TARGET_NAME = "rok_placement_ui_hud"
EXPECTED_SPRITES = {
    "btn_hud_1008": (721, 575, 88, 88),
    "btn_hud_1004": (1, 435, 88, 88),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def get_manifest_path() -> Path:
    value = os.environ.get("ROK_PLACEMENT_UI_ATLAS_MANIFEST")
    if value:
        return Path(value).resolve()
    if "--manifest" in sys.argv:
        index = sys.argv.index("--manifest")
        if index + 1 < len(sys.argv):
            return Path(sys.argv[index + 1]).resolve()
    raise RuntimeError("Set ROK_PLACEMENT_UI_ATLAS_MANIFEST or pass --manifest <atlas_manifest.json>")


def main() -> None:
    import unreal

    project_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_name = world.get_name() if world else ""
    print("Placement atlas import identity:", project_dir, world_name)
    if project_dir != EXPECTED_PROJECT or world_name != EXPECTED_WORLD:
        raise RuntimeError("Wrong project/editor world; expected D:/ueDemo/Rok and L_RokUnityGameAppCity")

    manifest_path = get_manifest_path()
    if not manifest_path.is_file() or not manifest_path.resolve().is_relative_to(
        (ROOT / "Saved/RokUnityMigration/PlacementUiAtlas").resolve()
    ):
        raise RuntimeError("Manifest must be an existing prepared run inside Saved/RokUnityMigration/PlacementUiAtlas")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    if manifest.get("schema") != "rok.placement_ui_atlas_source.v1":
        raise RuntimeError("Unexpected placement atlas manifest schema")
    if manifest.get("ue_target") != TARGET or manifest.get("atlas_size") != [1024, 1024]:
        raise RuntimeError("Placement atlas target/size differs from the source contract")

    rects = {}
    for row in manifest.get("sprites", []):
        rect = row.get("source_rect_bottom_left", {})
        rects[row.get("name")] = tuple(rect.get(k) for k in ("x", "y", "width", "height"))
    if rects != EXPECTED_SPRITES:
        raise RuntimeError(f"Sprite rectangles differ from source contract: {rects}")

    png_copy = None
    for source in manifest.get("sources", []):
        original = ROOT / source["source_path"]
        copy = ROOT / source["copy_path"]
        if not original.is_file() or not copy.is_file():
            raise RuntimeError(f"Source/copy missing: {source.get('role')}")
        if sha256(original) != source["source_sha256"] or sha256(copy) != source["copy_sha256"]:
            raise RuntimeError(f"Source or prepared copy hash changed: {source.get('role')}")
        if source.get("role") == "atlas_png":
            png_copy = copy
    if png_copy is None:
        raise RuntimeError("Prepared source manifest has no atlas_png copy")

    report_path = manifest_path.parent / "ue_import_report.json"
    if report_path.exists():
        raise RuntimeError(f"Refusing to overwrite prior import evidence: {report_path}")
    content_file = ROOT / "Content/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud.uasset"
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET) or content_file.exists():
        raise RuntimeError(f"Refusing to overwrite existing destination: {TARGET}")

    task = unreal.AssetImportTask()
    task.filename = str(png_copy)
    task.destination_path = "/Game/RokPrototype/UI/PlacementAtlases"
    task.destination_name = TARGET_NAME
    task.automated = True
    task.replace_existing = False
    task.save = True
    task.factory = unreal.TextureFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if getattr(task, "errors", None):
        raise RuntimeError("Texture import reported errors: " + "; ".join(task.errors))
    imported_paths = list(getattr(task, "imported_object_paths", []))
    imported_packages = {str(item).split(".", 1)[0] for item in imported_paths}
    if imported_paths and imported_packages != {TARGET}:
        raise RuntimeError(f"TextureFactory imported an unexpected asset: {imported_paths}")

    texture = unreal.EditorAssetLibrary.load_asset(TARGET)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Imported placement atlas is missing or is not Texture2D")
    settings = (
        ("srgb", True),
        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
        ("never_stream", True),
        ("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON),
        ("address_x", unreal.TextureAddress.TA_CLAMP),
        ("address_y", unreal.TextureAddress.TA_CLAMP),
    )
    actual_settings = {}
    for property_name, expected in settings:
        texture.set_editor_property(property_name, expected)
        actual = texture.get_editor_property(property_name)
        if actual != expected:
            raise RuntimeError(f"Texture setting {property_name} mismatch: expected {expected}, got {actual}")
        actual_settings[property_name] = str(actual)
    if int(texture.blueprint_get_size_x()) != 1024:
        raise RuntimeError("Imported atlas width is not 1024")
    if int(texture.blueprint_get_size_y()) != 1024:
        raise RuntimeError("Imported atlas height is not 1024")
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError("Failed to save imported placement atlas")
    if not content_file.is_file():
        raise RuntimeError("Expected destination .uasset was not created")

    report = {
        "schema": "rok.placement_ui_atlas_import.v1",
        "passed": True,
        "project": str(project_dir),
        "world": world_name,
        "manifest_path": str(manifest_path),
        "manifest_sha256": sha256(manifest_path),
        "source_atlas_sha256": sha256(png_copy),
        "asset": TARGET,
        "asset_uasset_path": str(content_file),
        "asset_uasset_sha256": sha256(content_file),
        "size": [1024, 1024],
        "texture_settings": actual_settings,
        "sprites": manifest["sprites"],
        "imported_object_paths": imported_paths,
        "map_saved": False,
    }
    with report_path.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps({"imported": TARGET, "report": str(report_path), "assetSha256": report["asset_uasset_sha256"]}))


if __name__ == "__main__":
    main()
