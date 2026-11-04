"""Prepare an isolated Unity 2019.4 source probe for locally constructible city buildings.

Mirrors PrepareRokCityResourceVisualNative.py with its own probe project, renderer and
contract so the pinned Farm/Sawmill probe stays untouched. Never launches Unity.
"""
import csv
import hashlib
import json
import shutil

from BuildRokCityBuildingVisualSources import OUT, ROOT, build
from PrepareRokCityResourceVisualNative import (
    CAMERA_CSV, MASK_SCRIPT, PROBE as RESOURCE_PROBE, PROJECT_SETTINGS, UNITY_VERSION,
)
from rok_data_paths import resolve


PROBE = ROOT / "Saved/UnityCityBuildingVisualProbe"
PREPARED = ROOT / "Saved/RokUnityMigration/CityBuildingVisualNative/prepared.json"
RENDERER = ROOT / "Scripts/RenderRokCityBuildingVisualNative.cs"
RENDERER_CLASS = "RenderRokCityBuildingVisualNative"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def unity_command():
    editor = f"C:/Program Files/Unity/Hub/Editor/{UNITY_VERSION}/Editor/Unity.exe"
    return [editor, "-batchmode", "-projectPath", str(PROBE), "-executeMethod", RENDERER_CLASS + ".Run",
            "-logFile", str(ROOT / "Saved/Logs/UnityCityBuildingVisualProbe_<UTC>.log")]


def prepare():
    catalog = json.loads(OUT.read_text(encoding="utf-8"))
    if catalog != build(write=False):
        raise RuntimeError("City building visual source catalog is stale")
    for relative, expected in catalog["sources"].items():
        source = resolve(relative)
        if not source.is_file() or digest(source) != expected:
            raise RuntimeError(f"Source hash mismatch: {relative}")
    models = catalog["models"]
    if not models or len({m["model_id"] for m in models}) != len(models):
        raise RuntimeError("City building visual model list is empty or repeats a model")
    shader = resolve(catalog["material"]["shader_path"]).read_text(encoding="utf-8-sig")
    if "_CityTransparency" in shader.split("SubShader", 1)[0] or "float _CityTransparency;" not in shader:
        raise RuntimeError("Source shader global transparency contract changed")
    with CAMERA_CSV.open(encoding="utf-8-sig", newline="") as stream:
        camera = next(row for row in csv.DictReader(stream) if row["ID"] == "city_default")
    if [float(camera[k]) for k in ("dist", "fov", "dxf", "forwardX", "forwardY", "forwardZ")] != [20, 7.5, 150, 0, -0.7, 0.7]:
        raise RuntimeError("city_default camera source row changed")
    if "  m_ActiveColorSpace: 0" not in PROJECT_SETTINGS.read_text(encoding="utf-8-sig"):
        raise RuntimeError("Unity source is no longer Gamma")

    # Reuse the resource probe's Unity-generated project settings (Gamma, same player
    # defaults) so both probes render under the same editor configuration.
    settings = PROBE / "ProjectSettings"
    if not settings.is_dir():
        shutil.copytree(RESOURCE_PROBE / "ProjectSettings", settings)
    if "  m_ActiveColorSpace: 0" not in (settings / "ProjectSettings.asset").read_text(encoding="utf-8-sig"):
        raise RuntimeError("Probe project is not Gamma")
    (settings / "ProjectVersion.txt").write_text("m_EditorVersion: " + UNITY_VERSION + "\n", encoding="utf-8")

    source_dir = PROBE / "Assets/Source"
    source_dir.mkdir(parents=True, exist_ok=True)
    selected = [catalog["material"]["path"], catalog["material"]["shader_path"],
                MASK_SCRIPT.relative_to(ROOT).as_posix()]
    for model in models:
        nodes = model["prefab"]["nodes"]
        root = next(n for n in nodes if n["file_id"] == model["prefab"]["root_game_object_file_id"])
        child = next(n for n in nodes if n["file_id"] == model["prefab"]["main_sprite_game_object_file_id"])
        if not root["active"] or not child["active"] or not child["renderer"]["enabled"]:
            raise RuntimeError(f"Inactive source rendering subtree: {model['model_id']}")
        if child["transform"]["parent_transform_file_id"] != root["transform"]["file_id"]:
            raise RuntimeError("Main sprite parent transform changed")
        selected.extend([model["sprite"]["path"], model["mask"]["path"]])
    copies = {}
    for relative in selected:
        source = resolve(relative)
        target = source_dir / source.name
        if target.is_file() and digest(target) != digest(source):
            raise RuntimeError(f"Conflicting probe basename: {source.name}")
        shutil.copy2(source, target)
        source_meta = source.with_name(source.name + ".meta")
        target_meta = target.with_name(target.name + ".meta")
        shutil.copy2(source_meta, target_meta)
        copies[relative] = {"sha256": digest(source), "meta_sha256": digest(source_meta),
                            "probe_asset": "Assets/Source/" + source.name}
    shutil.copy2(RENDERER, PROBE / "Assets" / RENDERER.name)
    (PROBE / "Assets/ProbeDependencies.cs").write_text(
        """using System;
using UnityEngine;
namespace Skyunion {
  public static class CoreUtils { public static ProbeAssetService assetService = new ProbeAssetService(); }
  public sealed class ProbeAssetService {
    public void LoadAssetAsync<T>(string path, Action<ProbeAsset> callback, GameObject owner)
      where T : UnityEngine.Object { throw new InvalidOperationException("Async source asset loading is outside building visual probe"); }
  }
  public sealed class ProbeAsset { public UnityEngine.Object asset() { return null; } }
}
namespace Client {
  public sealed class NightingMask : MonoBehaviour { public void SetCanChangeLight(bool value) {} }
}
""", encoding="utf-8")
    (PROBE / "Packages").mkdir(exist_ok=True)
    (PROBE / "Packages/manifest.json").write_text(
        json.dumps({"dependencies": {"com.unity.modules.imageconversion": "1.0.0",
                                     "com.unity.modules.jsonserialize": "1.0.0",
                                     "com.unity.modules.ui": "1.0.0"}}, indent=2) + "\n", encoding="utf-8")
    (PROBE / "OutputRuns").mkdir(exist_ok=True)
    contracts = []
    for model in models:
        nodes = model["prefab"]["nodes"]
        root = next(n for n in nodes if n["file_id"] == model["prefab"]["root_game_object_file_id"])
        child = next(n for n in nodes if n["file_id"] == model["prefab"]["main_sprite_game_object_file_id"])
        contracts.append({
            "schema": "rok.city_building_visual_native_probe.v1",
            "model": model["model_id"], "source_catalog_sha256": digest(OUT),
            "renderer_script_sha256": digest(RENDERER), "root": root, "sprite_child": child,
            "material": catalog["material"],
            "camera": {"source_row": "city_default", "dxf": 150.0, "distance": 20.0,
                       "vertical_fov": 7.5, "forward": [0.0, -0.7, 0.7], "width": 1280, "height": 720},
            "lighting": {"neutral_sprite_color": [1.0, 1.0, 1.0, 1.0],
                         "day_sprite_color": [1.07203974, 1.07203974, 1.07203974, 1.0],
                         "city_transparency": 1.0},
            "scope": "Source prefab root/main SpriteRenderer subtree, original MaskSprite/material/shader. Explicit neutral/day controls, not full-city LOD or account lighting.",
        })
    package = {"schema": "rok.city_building_visual_native_inputs.v1",
               "source_catalog_sha256": digest(OUT), "renderer_script_sha256": digest(RENDERER),
               "camera_source_sha256": digest(CAMERA_CSV),
               "unity_project_settings_sha256": digest(PROJECT_SETTINGS),
               "source_files": copies, "models": contracts,
               "qualification": "Unity 2019.4 Gamma source prefab subtree probe; civilization 101/age 1 is local visual variant, not account identity. No GameApp, login, network or server state."}
    path = PROBE / "probe_contract.json"
    path.write_text(json.dumps(package, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    PREPARED.parent.mkdir(parents=True, exist_ok=True)
    PREPARED.write_text(json.dumps({
        "schema": "rok.city_building_visual_native_prepared.v1",
        "status": "prepared_only_no_unity_run",
        "probe_project": PROBE.relative_to(ROOT).as_posix(),
        "input_contract_path": path.relative_to(ROOT).as_posix(),
        "input_contract_sha256": digest(path),
        "source_catalog_sha256": digest(OUT),
        "renderer_script_sha256": digest(RENDERER),
        "source_copy_count": len(copies),
        "models": [model["model"] for model in contracts],
        "unity_command": unity_command(),
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"prepared {path}: {len(contracts)} exact city building models")
    return package


if __name__ == "__main__":
    prepare()
