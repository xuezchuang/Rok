"""Import the two sealed Expedition source PNGs in the verified Rok editor.

Default is read-only. ROK_EXPEDITION_VISUAL_IMPORT_APPLY=1 creates only the two
new Texture2D assets; existing assets, maps and source evidence are never saved.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
from datetime import datetime, timezone


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "Saved/RokUnityMigration/ExpeditionVisualAssets/v1/atlas_manifest.json"
MANIFEST_SHA256 = "c5baa5b4f373094dea6e1a75c8311769d8484463f49972b5c5d7f1138ed14bb1"
REPORT_DIR = ROOT / "Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry"
TARGET_DIR = "/Game/RokPrototype/UI/ExpeditionSource"
ASSETS = {
    "ui_battle.png": ("rok_expedition_battle_atlas", (2048, 256)),
    "img_battle_result_task.png": ("rok_expedition_task_panel", (676, 761)),
}


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
        raise RuntimeError("Sealed Expedition visual manifest SHA changed")
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if manifest.get("schema") != "rok.expedition.visual_assets.v1":
        raise RuntimeError("Unexpected Expedition source schema")

    selected = {}
    for row in manifest.get("atlases", []):
        source = ROOT / row["source"]
        prepared = ROOT / row["prepared"]
        if not source.is_file() or not prepared.is_file():
            raise RuntimeError(f"Source or prepared PNG missing: {row['source']}")
        if digest(source) != row["sha256"] or digest(prepared) != row["sha256"]:
            raise RuntimeError(f"Source/prepared bytes changed: {row['source']}")
        name = prepared.name
        if name in ASSETS:
            expected_name, expected_size = ASSETS[name]
            if tuple(row["size"]) != expected_size:
                raise RuntimeError(f"PNG dimensions differ from source manifest: {name}")
            target = TARGET_DIR + "/" + expected_name
            disk = ROOT / "Content/RokPrototype/UI/ExpeditionSource" / (expected_name + ".uasset")
            if unreal.EditorAssetLibrary.does_asset_exist(target) or disk.exists():
                raise RuntimeError(f"Refusing to overwrite existing asset: {target}")
            selected[name] = (prepared, target, disk, expected_size, row["sha256"])
    if set(selected) != set(ASSETS):
        raise RuntimeError(f"Prepared PNG set mismatch: {sorted(selected)}")

    if os.environ.get("ROK_EXPEDITION_VISUAL_IMPORT_APPLY") != "1":
        print("READ_ONLY Expedition visual preflight passed:", sorted(selected))
        return

    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    report_path = REPORT_DIR / ("expedition_visual_ue_import_" +
        datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S_%f") + ".json")
    results = []
    for name in sorted(selected):
        source, target, disk, expected_size, source_hash = selected[name]
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = TARGET_DIR
        task.destination_name = target.rsplit("/", 1)[-1]
        task.factory = unreal.TextureFactory()
        task.automated = True
        task.replace_existing = False
        task.save = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        if getattr(task, "errors", None):
            raise RuntimeError(f"TextureFactory failed for {name}: {task.errors}")
        texture = unreal.EditorAssetLibrary.load_asset(target)
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(f"Imported object is not Texture2D: {target}")
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
                raise RuntimeError(f"Texture setting rejected: {target} {key}")
        size = (int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y()))
        if size != expected_size:
            raise RuntimeError(f"Texture size mismatch for {target}: {size}")
        if not unreal.EditorAssetLibrary.save_loaded_asset(texture) or not disk.is_file():
            raise RuntimeError(f"Texture save failed: {target}")
        results.append({"source_png": str(source), "source_sha256": source_hash,
                        "target": target, "uasset": str(disk),
                        "uasset_sha256": digest(disk), "size": list(size),
                        "texture_settings": {key: str(value) for key, value in settings}})
    report = {"schema": "rok.expedition.visual_import.v1", "passed": True,
              "project": str(project), "world": world_name,
              "source_manifest": str(MANIFEST), "source_manifest_sha256": MANIFEST_SHA256,
              "assets": results, "map_saved": False, "engine_runtime_acceptance": False}
    with report_path.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    print("Expedition source textures imported:", report_path)


if __name__ == "__main__":
    main()
