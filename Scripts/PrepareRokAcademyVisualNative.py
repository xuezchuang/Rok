"""Prepare a source-pinned, isolated Unity 2019.4 Academy sprite probe.

This only writes Saved/UnityAcademyVisualProbe. It never opens Unity or edits ref.
The original prefab is parsed by BuildRokAcademyVisualSource; the probe recreates
its rendering root/child hierarchy because unrelated gameplay scripts are absent.
"""

import hashlib
import json
import shutil
import csv
from pathlib import Path
from rok_data_paths import resolve

from BuildRokAcademyVisualSource import ROOT, OUT, build


PROBE = ROOT / "Saved/UnityAcademyVisualProbe"
UNITY_VERSION = "2019.4.28f1c1"
SOURCE_FILES = ("sprite", "mask")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare():
    catalog = json.loads(OUT.read_text(encoding="utf-8"))
    if catalog != build(write=False):
        raise RuntimeError("Academy source catalog is stale or differs from source")
    for relative, expected in catalog["sources"].items():
        source = resolve(relative)
        if not source.is_file() or digest(source) != expected:
            raise RuntimeError(f"Academy source hash mismatch: {relative}")
    if catalog["model_id"] != "build_13_101_2":
        raise RuntimeError("Unexpected Academy model")
    shader_text = (resolve(catalog["material"]["shader_path"])).read_text(encoding="utf-8-sig")
    if "_CityTransparency" in shader_text.split("SubShader", 1)[0] or "float _CityTransparency;" not in shader_text:
        raise RuntimeError("Academy shader global CityTransparency contract changed")
    nodes = catalog["prefab"]["nodes"]
    root = next(n for n in nodes if n["file_id"] == catalog["prefab"]["root_game_object_file_id"])
    child = next(n for n in nodes if n["file_id"] == catalog["prefab"]["main_sprite_game_object_file_id"])
    if not (root["active"] and child["active"] and child["renderer"]["enabled"]):
        raise RuntimeError("Source Academy sprite is inactive")
    if child["transform"]["parent_transform_file_id"] != root["transform"]["file_id"]:
        raise RuntimeError("Source Academy parent link changed")
    camera_csv = ROOT / "ref/config-tables/编译客户端配置/CSV/CameraParamLod.csv"
    with camera_csv.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    camera_row = next(r for r in rows if r["ID"] == "city_default")
    if [float(camera_row[k]) for k in ("dist", "fov", "dxf", "forwardX", "forwardY", "forwardZ")] != [20.0, 7.5, 150.0, 0.0, -0.7, 0.7]:
        raise RuntimeError("Source city_default camera row changed")
    project_settings = ROOT / "ref/client-unity/ProjectSettings/ProjectSettings.asset"
    if "  m_ActiveColorSpace: 0" not in project_settings.read_text(encoding="utf-8-sig"):
        raise RuntimeError("Source Unity project no longer uses Gamma color space")

    source_dir = PROBE / "Assets/Source"
    source_dir.mkdir(parents=True, exist_ok=True)
    selected = [catalog[key]["path"] for key in SOURCE_FILES]
    selected.extend((catalog["material"]["path"], catalog["material"]["shader_path"]))
    selected.append("ref/client-unity/Assets/Scripts/Client/Compoment/MaskSprite.cs")
    copies = {}
    for relative in selected:
        source = resolve(relative)
        target = source_dir / source.name
        shutil.copy2(source, target)
        meta_target = Path(str(target) + ".meta")
        shutil.copy2(Path(str(source) + ".meta"), meta_target)
        copies[relative] = {"sha256": digest(target), "meta_sha256": digest(meta_target),
                            "probe_asset": "Assets/Source/" + target.name}
    renderer_script = ROOT / "Scripts/RenderRokAcademyVisualNative.cs"
    shutil.copy2(renderer_script, PROBE / "Assets/RenderRokAcademyVisualNative.cs")
    # Compile the original MaskSprite unchanged. Its unrelated async material
    # replacement and night-light references are inert in this probe.
    (PROBE / "Assets/ProbeDependencies.cs").write_text(
        """using System;
using UnityEngine;
namespace Skyunion {
  public static class CoreUtils { public static ProbeAssetService assetService = new ProbeAssetService(); }
  public sealed class ProbeAssetService {
    public void LoadAssetAsync<T>(string path, Action<ProbeAsset> callback, GameObject owner)
      where T : UnityEngine.Object { throw new InvalidOperationException("Async asset load is outside Academy probe"); }
  }
  public sealed class ProbeAsset { public UnityEngine.Object asset() { return null; } }
}
namespace Client {
  public sealed class NightingMask : MonoBehaviour { public void SetCanChangeLight(bool value) {} }
}
""",
        encoding="utf-8",
    )
    (PROBE / "Packages").mkdir(exist_ok=True)
    (PROBE / "Packages/manifest.json").write_text(
        json.dumps({"dependencies": {"com.unity.modules.imageconversion": "1.0.0",
                                      "com.unity.modules.jsonserialize": "1.0.0",
                                      "com.unity.modules.ui": "1.0.0"}}, indent=2) + "\n",
        encoding="utf-8",
    )
    (PROBE / "ProjectSettings").mkdir(exist_ok=True)
    (PROBE / "ProjectSettings/ProjectVersion.txt").write_text(
        "m_EditorVersion: " + UNITY_VERSION + "\n", encoding="utf-8"
    )
    # JsonUtility cannot read the whole source catalog's dictionary-shaped
    # evidence; this contract contains only fields used by the native renderer.
    contract = {
        "schema": "rok.academy_visual_native_probe.v1",
        "model": catalog["model_id"],
        "source_catalog_sha256": digest(OUT),
        "renderer_script_sha256": digest(renderer_script),
        "source_files": copies,
        "root": root,
        "sprite_child": child,
        "material": catalog["material"],
        "camera": {"source_row": "city_default", "dxf": 150.0, "distance": 20.0,
                   "vertical_fov": 7.5, "forward": [0.0, -0.7, 0.7],
                   "width": 1280, "height": 720},
        "camera_source_sha256": digest(camera_csv),
        "unity_project_settings_sha256": digest(project_settings),
        "lighting": {"neutral_sprite_color": [1.0, 1.0, 1.0, 1.0],
                     "day_sprite_color": [1.07203974, 1.07203974, 1.07203974, 1.0],
                     "city_transparency": 1.0,
                     "day_control_note": "Explicit 1.07203974 prior isolated source-derived day control; not measured live Academy GameView lighting."},
        "qualification": "Isolated source Unity SpriteRenderer and original shader/material/MaskSprite; fixed camera and explicit globals, not full city GameView. Source .mat serializes CityTransparency=1 but shader omits it from Properties and reads Shader global. If HasProperty=false, GetFloat is unavailable and not called because Unity logs Error; render-time global is measured separately.",
    }
    path = PROBE / "probe_contract.json"
    path.write_text(json.dumps(contract, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(path)


if __name__ == "__main__":
    prepare()
