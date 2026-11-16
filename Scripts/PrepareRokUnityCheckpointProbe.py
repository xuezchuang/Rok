"""Build and run a small Unity 2019 checkpoint source probe, then package its witness.

All Unity writes stay under Saved/UnityCheckpointMeshProbe. The source snapshot
under ref/ is read-only. Re-running validates pinned inputs before packaging.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE_ASSETS = ROOT / "ref/client-unity/Assets"
PROJECT = ROOT / "Saved/UnityCheckpointMeshProbe"
OUTPUT = PROJECT / "Output"
PACKAGE = ROOT / "Content/RokData/WorldStructurePresentation"
UNITY = Path("C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Unity.exe")
PREFAB = SOURCE_ASSETS / "BundleAssets/Scene/Map_building/map_building/build_Checkpoint_Big.prefab"
FBX = SOURCE_ASSETS / "BundleAssets/Map/FBX/build_Checkpoint_Big.FBX"
MATERIAL = SOURCE_ASSETS / "BundleAssets/Map/Materials/shared_mapmaterials/build_Checkpoint_Big.mat"
TEXTURE = SOURCE_ASSETS / "BundleAssets/Map/Materials/shared_mapmaterials/build_Checkpoint_Big.png"
SHADER = SOURCE_ASSETS / "BundleAssets/Map/Building/customcitycity_wall.shader"
GUID_RE = re.compile(rb"guid: ([0-9a-f]{32})")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def copy_source(path: Path) -> None:
    dest = PROJECT / "Assets" / path.relative_to(SOURCE_ASSETS)
    dest.parent.mkdir(parents=True, exist_ok=True)
    for src, out in ((path, dest), (Path(str(path) + ".meta"), Path(str(dest) + ".meta"))):
        if not src.is_file():
            raise FileNotFoundError(src)
        if out.exists() and sha(out) != sha(src):
            # The exporter flips only this isolated importer's readback bit.
            if out == Path(str(PROJECT / "Assets" / FBX.relative_to(SOURCE_ASSETS)) + ".meta"):
                shutil.copy2(src, out)
            else:
                raise ValueError(f"isolated source copy differs: {out}")
        if not out.exists():
            shutil.copy2(src, out)


def prepare() -> dict:
    if not UNITY.is_file():
        raise FileNotFoundError(UNITY)
    for path in (PREFAB, FBX, MATERIAL, TEXTURE, SHADER):
        if not path.is_file() or not Path(str(path) + ".meta").is_file():
            raise FileNotFoundError(path)
    # Resolve serialized GUID references without copying the live Unity source
    # project. Missing script components are inert in this mesh/material oracle.
    guid_index = {}
    for meta in SOURCE_ASSETS.rglob("*.meta"):
        if not meta.is_file():
            continue
        match = GUID_RE.search(meta.read_bytes()[:250])
        if match:
            guid_index[match.group(1).decode()] = Path(str(meta)[:-5])
    queue = [PREFAB, FBX, MATERIAL, TEXTURE, SHADER]
    copied = set()
    while queue:
        path = queue.pop()
        if path in copied:
            continue
        if path.suffix == ".cs":
            continue
        copy_source(path)
        copied.add(path)
        if path.suffix.lower() not in (".prefab", ".mat", ".shader"):
            continue
        for guid in set(m.decode() for m in GUID_RE.findall(path.read_bytes())):
            dependent = guid_index.get(guid)
            if dependent and dependent.is_file() and dependent.suffix != ".cs":
                queue.append(dependent)
    editor = PROJECT / "Assets/Editor"
    editor.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "Scripts/ExportRokUnityCheckpointMesh.cs", editor / "ExportRokUnityCheckpointMesh.cs")
    settings = PROJECT / "ProjectSettings"
    settings.mkdir(exist_ok=True)
    version = ROOT / "Saved/UnityGoldDepositProbe/ProjectSettings/ProjectVersion.txt"
    if not version.is_file():
        version = ROOT / "Saved/UnityBuiltinShaderProbe/ProjectSettings/ProjectVersion.txt"
    shutil.copy2(version, settings / "ProjectVersion.txt")
    packages = PROJECT / "Packages"
    packages.mkdir(exist_ok=True)
    (packages / "manifest.json").write_text(json.dumps({"dependencies": {
        "com.unity.modules.imageconversion": "1.0.0",
        "com.unity.modules.jsonserialize": "1.0.0",
    }}), encoding="utf-8")
    source = {
        name: {"path": path.relative_to(ROOT).as_posix(), "sha256": sha(path),
               "meta_sha256": sha(Path(str(path) + ".meta"))}
        for name, path in (("prefab", PREFAB), ("fbx", FBX), ("material", MATERIAL),
                           ("texture", TEXTURE), ("shader", SHADER))
    }
    contract = {"schema": "rok.checkpoint_mesh_probe_contract.v1",
                "unity_version": "2019.4.28f1c1", "source": source,
                "exporter_sha256": sha(ROOT / "Scripts/ExportRokUnityCheckpointMesh.cs"),
                "copied_asset_count": len(copied),
                "copied_assets": sorted(p.relative_to(SOURCE_ASSETS).as_posix() for p in copied),
                "root_transform": {"position": [0, 0, 0], "rotation": [0, 0, 0, 1],
                                   "scale": [1, 1, 1]},
                "scope": "Source prefab and nested FBX in isolated Unity 2019.4.28f1c1; no live project or runtime WorldMgr override."}
    OUTPUT.mkdir(exist_ok=True)
    (OUTPUT / "contract.json").write_text(json.dumps(contract, indent=2) + "\n", encoding="utf-8")
    return contract


def package(contract: dict) -> None:
    native = json.loads((OUTPUT / "checkpoint_mesh_native.json").read_text(encoding="utf-8"))
    if native.get("schema") != "rok.checkpoint_mesh_native.v1" or native.get("unityVersion") != "2019.4.28f1c1":
        raise ValueError("Unexpected Unity witness identity")
    meshes = native.get("meshes", [])
    if len(meshes) != 4:
        raise ValueError(f"Expected four source FBX MeshFilters, found {len(meshes)}")
    for mesh in meshes:
        vertices = mesh["vertices"]
        if len(vertices) != mesh["vertexCount"] or len(mesh["normals"]) != len(vertices) or len(mesh["uv0"]) != len(vertices) or len(mesh["colors"]) != len(vertices):
            raise ValueError(f"Incomplete vertices on {mesh['gameObject']}")
        if len(mesh["triangles"]) != 3 * mesh["triangleCount"] or any(i < 0 or i >= len(vertices) for i in mesh["triangles"]):
            raise ValueError(f"Invalid triangles on {mesh['gameObject']}")
        if mesh["sourceMeshGuid"] != "8051e1d671e7ca64e904093aadf2f537":
            raise ValueError(f"Unexpected source FBX GUID on {mesh['gameObject']}")
        for material in mesh["materials"]:
            if material["mainTexturePath"] == "Assets/BundleAssets/Map/Materials/shared_mapmaterials/build_Checkpoint_Big.png":
                material["main_texture_export"] = "Content/RokData/WorldStructurePresentation/checkpoint_mesh_albedo.png"
            else:
                raise ValueError(f"Unexpected checkpoint mesh texture: {material['mainTexturePath']}")
    screenshot = OUTPUT / "checkpoint_mesh_near_native.png"
    if not screenshot.is_file() or screenshot.stat().st_size < 10000:
        raise ValueError("Missing rendered Unity screenshot")
    shader_text = SHADER.read_text(encoding="utf-8")
    if 'Tags { "RenderType"="Opaque" }' not in shader_text or 'tex2D(_MainTex, i.uv)*_SpriteColor' not in shader_text:
        raise ValueError("Checkpoint custom shader semantics changed")
    packaged = {
        "schema": "rok.checkpoint_mesh.v1", "source": contract["source"],
        "unity_version": native["unityVersion"],
        "coordinate_space": native["coordinateSpace"],
        "root_transform": contract["root_transform"],
        "native_witness": {"path": (OUTPUT / "checkpoint_mesh_native.json").relative_to(ROOT).as_posix(),
                           "sha256": sha(OUTPUT / "checkpoint_mesh_native.json")},
        "native_screenshot": {"path": "Content/RokData/WorldStructurePresentation/checkpoint_mesh_near_native.png",
                              "sha256": sha(screenshot), "camera": native["camera"],
                              "sprite_color_global": native["spriteColorGlobal"]},
        "shader_semantics": {
            "source_explicit_render_type": "Opaque",
            "source_explicit_fragment_rgba": "tex2D(_MainTex, uv) * _SpriteColor",
            "shaderlab_default_cull": "Back",
            "shaderlab_default_zwrite": "On",
            "shaderlab_default_ztest": "LEqual",
            "shaderlab_default_blend": "One Zero",
            "material_render_queue": 2993,
        },
        "meshes": meshes,
        "mesh_count": len(meshes), "vertex_count": sum(m["vertexCount"] for m in meshes),
        "triangle_count": sum(m["triangleCount"] for m in meshes),
        "scope": "FBX MeshFilter source topology at prefab root identity; sprite/card assembly and runtime LOD behavior are separate."
    }
    PACKAGE.mkdir(parents=True, exist_ok=True)
    target = PACKAGE / "checkpoint_mesh.json"
    target.write_text(json.dumps(packaged, indent=2) + "\n", encoding="utf-8")
    shutil.copy2(screenshot, PACKAGE / "checkpoint_mesh_near_native.png")
    shutil.copy2(TEXTURE, PACKAGE / "checkpoint_mesh_albedo.png")
    print(json.dumps({"package": str(target), "meshes": len(meshes),
                      "vertices": packaged["vertex_count"], "triangles": packaged["triangle_count"]}))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--package-only", action="store_true")
    args = parser.parse_args()
    contract = json.loads((OUTPUT / "contract.json").read_text(encoding="utf-8")) if args.package_only else prepare()
    if not args.prepare_only and not args.package_only:
        command = [str(UNITY), "-batchmode", "-quit", "-projectPath", str(PROJECT),
                   "-executeMethod", "ExportRokUnityCheckpointMesh.Run", "-logFile", str(OUTPUT / "unity_console.log")]
        result = subprocess.run(command, check=False)
        if result.returncode != 0:
            raise RuntimeError(f"Unity checkpoint export failed ({result.returncode}): {OUTPUT / 'unity_console.log'}")
    if not args.prepare_only:
        package(contract)
    else:
        print(json.dumps({"project": str(PROJECT), "assets": contract["copied_asset_count"]}))


if __name__ == "__main__":
    main()
