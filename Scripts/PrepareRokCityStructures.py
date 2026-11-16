"""Stage the original boundary and construction shelf prefabs for Unity export."""
import json
import re
import shutil
from pathlib import Path

from rok_data_paths import PROJECT_ROOT as ROOT, resolve


ASSETS = ROOT / "ref/client-unity/Assets"
PROBE = ROOT / "Saved/UnityCityStructuresProbe"
OUTPUT = ROOT / "Saved/RokUnityMigration/CityStructures"
CITY = ASSETS / "BundleAssets/Scene/City_building"
GUID = re.compile(rb"(?m)^guid: ([a-f0-9]{32})\s*$")
REFERENCE = re.compile(rb"guid: ([a-f0-9]{32})")
SERIALIZED = {".prefab", ".mat", ".anim", ".asset", ".controller", ".overridecontroller"}


def prepare():
    catalog = json.loads((ROOT / "Content/RokData/CityConstructionSources/catalog.json").read_text(encoding="utf-8"))
    rows = [row for row in catalog["models"] if row["type"] in (6, 8)]
    boundary = {row["model_id"]: resolve(row["scene_prefab"]) for row in rows if row["scene_prefab"]}
    shelves = {path.stem: path for path in CITY.rglob("Build_shelf_*.prefab") if path.stem.startswith("Build_shelf_")}
    models = {**boundary, **shelves}
    index = {}
    for meta in ASSETS.rglob("*.meta"):
        path = meta.with_suffix("")
        if not path.is_file():
            continue
        match = GUID.search(meta.read_bytes())
        if match:
            index[match[1]] = path
    selected, todo, missing = set(), list(models.values()), set()
    while todo:
        path = todo.pop()
        if path in selected or path.suffix.lower() in {".cs", ".dll", ".js"}:
            continue
        selected.add(path)
        if path.suffix.lower() in SERIALIZED:
            for guid in REFERENCE.findall(path.read_bytes()):
                if guid in index:
                    todo.append(index[guid])
                elif not guid.startswith(b"0000000000000000"):
                    missing.add(guid.decode())
    for path in sorted(selected):
        destination = PROBE / "Assets/Source" / path.relative_to(ASSETS)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
        shutil.copy2(str(path) + ".meta", str(destination) + ".meta")
    settings = PROBE / "ProjectSettings"
    if not settings.is_dir():
        shutil.copytree(ROOT / "Saved/UnityCityBuildingVisualProbe/ProjectSettings", settings)
    packages = PROBE / "Packages"
    packages.mkdir(exist_ok=True)
    (packages / "manifest.json").write_text(json.dumps({"dependencies": {
        "com.unity.modules.imageconversion": "1.0.0", "com.unity.modules.jsonserialize": "1.0.0",
        "com.unity.modules.animation": "1.0.0", "com.unity.modules.particlesystem": "1.0.0",
        "com.unity.modules.physics": "1.0.0"}}), encoding="utf-8")
    editor = PROBE / "Assets/Editor"
    editor.mkdir(exist_ok=True)
    shutil.copy2(ROOT / "Scripts/ExportRokCityStructures.cs", editor)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    contract = {"output": str(OUTPUT), "models": [
        {"id": model, "path": "Assets/Source/" + path.relative_to(ASSETS).as_posix()}
        for model, path in sorted(models.items())]}
    (PROBE / "structures.json").write_text(json.dumps(contract, indent=2), encoding="utf-8")
    (OUTPUT / "source_inventory.json").write_text(json.dumps({
        "boundary": rows, "shelves": sorted(shelves), "dependency_count": len(selected),
        "unresolved_guids": sorted(missing)}, indent=2), encoding="utf-8")
    print(f"{len(boundary)} boundary prefabs, {len(shelves)} shelf prefabs, {len(selected)} source dependencies")


if __name__ == "__main__":
    prepare()
