"""Prepare a separate, source-pinned Unity 2019 Gamma sprite-composition probe.

Only Saved/UnityCityGammaCompositeProbe is written. Unity is run by the caller.
The offsets and sorting orders below are diagnostic controls, not city layout.
"""

import csv
import hashlib
import json
import shutil
from pathlib import Path
from rok_data_paths import resolve

from BuildRokAcademyVisualSource import ROOT, OUT as ACADEMY_CATALOG, build as academy_build
from BuildRokCityResourceBuildingVisualSources import OUT as RESOURCE_CATALOG, build as resource_build


PROBE = ROOT / "Saved/UnityCityGammaCompositeProbe"
RENDERER = ROOT / "Scripts/RenderRokCityGammaCompositeNative.cs"
CAMERA_CSV = ROOT / "ref/config-tables/编译客户端配置/CSV/CameraParamLod.csv"
PROJECT_SETTINGS = ROOT / "ref/client-unity/ProjectSettings/ProjectSettings.asset"
OLD_RUN = ROOT / "Saved/UnityAcademyVisualProbe/OutputRuns/20261008_153501_742_16460"
OLD_RESOURCE_RUN = ROOT / "Saved/UnityCityResourceVisualProbe/OutputRuns/20261008_161052_827_45112"
OLD_RESOURCE_VERIFY = ROOT / "Saved/RokUnityMigration/CityResourceVisualNative/verified_capture.json"
MASK_SCRIPT = ROOT / "ref/client-unity/Assets/Scripts/Client/Compoment/MaskSprite.cs"
UNITY_VERSION = "2019.4.28f1c1"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def node(catalog, kind):
    prefab = catalog["prefab"]
    identifier = prefab["root_game_object_file_id" if kind == "root" else "main_sprite_game_object_file_id"]
    return next(n for n in prefab["nodes"] if n["file_id"] == identifier)


def prepare():
    existing_runs = (PROBE / "OutputRuns").is_dir() and any((PROBE / "OutputRuns").iterdir())
    if existing_runs:
        previous_contract = PROBE / "probe_contract.json"
        if not previous_contract.is_file() or json.loads(previous_contract.read_text(encoding="utf-8"))[
                "renderer_script_sha256"] != sha(RENDERER):
            raise RuntimeError("Existing native runs pin another renderer; refusing to replace probe inputs")
    academy = json.loads(ACADEMY_CATALOG.read_text(encoding="utf-8"))
    resources = json.loads(RESOURCE_CATALOG.read_text(encoding="utf-8"))
    if academy != academy_build(write=False) or resources != resource_build(write=False):
        raise RuntimeError("Academy/resource source catalog changed")
    if academy["model_id"] != "build_13_101_2" or [m["model_id"] for m in resources["models"]] != [
        "build_2_101_1", "build_3_101_1"]:
        raise RuntimeError("Unexpected Academy/Farm/Wood source identity")
    if any(academy["material"].get(key) != resources["material"].get(key) for key in (
        "path", "guid", "shader_path", "shader_guid", "color_1", "color_2",
        "city_transparency", "shader_formula"
    )):
        raise RuntimeError("Source materials differ; one shared-shader fixture is invalid")
    for catalog in (academy, resources):
        for relative, expected in catalog["sources"].items():
            if sha(resolve(relative)) != expected:
                raise RuntimeError(f"Source changed: {relative}")
    shader = (resolve(academy["material"]["shader_path"])).read_text(encoding="utf-8-sig")
    if "_CityTransparency" in shader.split("SubShader", 1)[0] or "float _CityTransparency;" not in shader:
        raise RuntimeError("Source shader transparency-global contract changed")
    with CAMERA_CSV.open(encoding="utf-8-sig", newline="") as stream:
        camera_row = next(row for row in csv.DictReader(stream) if row["ID"] == "city_default")
    if [float(camera_row[key]) for key in ("dist", "fov", "dxf", "forwardX", "forwardY", "forwardZ")] != [
        20, 7.5, 150, 0, -0.7, 0.7]:
        raise RuntimeError("Source camera row changed")
    if "  m_ActiveColorSpace: 0" not in PROJECT_SETTINGS.read_text(encoding="utf-8-sig"):
        raise RuntimeError("Source Unity project is no longer Gamma")
    old_render = OLD_RUN / "native_render.json"
    old_verify = OLD_RUN / "native_verify.json"
    old_neutral = OLD_RUN / "academy_neutral.png"
    old_day = OLD_RUN / "academy_day.png"
    if not all(p.is_file() for p in (old_render, old_verify, old_neutral, old_day)):
        raise RuntimeError("Prior accepted Academy native control missing")
    if not json.loads(old_verify.read_text(encoding="utf-8"))["passed"]:
        raise RuntimeError("Prior Academy native control is not verified")
    old_resource_summary = json.loads(OLD_RESOURCE_VERIFY.read_text(encoding="utf-8"))
    if not old_resource_summary["passed"] or Path(old_resource_summary["run_directory"]).resolve() != OLD_RESOURCE_RUN.resolve():
        raise RuntimeError("Prior Farm/Wood native summary is not verified")
    old_resources = {}
    for label, model in (("farm", "build_2_101_1"), ("wood", "build_3_101_1")):
        directory = OLD_RESOURCE_RUN / model
        native = directory / "native_render.json"
        verified = directory / "native_verify.json"
        if not native.is_file() or not verified.is_file() or not any(
                entry["model"] == model and entry["native_report_sha256"] == sha(native)
                for entry in old_resource_summary["models"]):
            raise RuntimeError(f"Prior {label} native control missing/unverified")
        old_resources[label] = {"run": str(directory), "native_render_sha256": sha(native),
                                "native_verify_sha256": sha(verified)}

    models = [("academy", academy), ("farm", resources["models"][0]),
              ("wood", resources["models"][1])]
    records = []
    for label, model in models:
        root, child = node(model, "root"), node(model, "child")
        if not (root["active"] and child["active"] and child["renderer"]["enabled"]):
            raise RuntimeError(f"Inactive source renderer: {label}")
        if child["transform"]["parent_transform_file_id"] != root["transform"]["file_id"]:
            raise RuntimeError(f"Broken prefab parent: {label}")
        records.append({"label": label, "model": academy["model_id"] if label == "academy" else model["model_id"],
                        "source_catalog_sha256": sha(ACADEMY_CATALOG if label == "academy" else RESOURCE_CATALOG),
                        "root": root, "sprite_child": child,
                        "sprite": model["sprite"], "mask": model["mask"]})

    selected = {academy["material"]["path"], academy["material"]["shader_path"],
                MASK_SCRIPT.relative_to(ROOT).as_posix()}
    for record in records:
        selected.update((record["sprite"]["path"], record["mask"]["path"]))
    source_dir = PROBE / "Assets/Source"
    source_dir.mkdir(parents=True, exist_ok=True)
    copies = {}
    seen_names = {}
    for relative in sorted(selected):
        source = resolve(relative)
        previous = seen_names.setdefault(source.name, relative)
        if previous != relative:
            raise RuntimeError(f"Probe basename collision: {previous} / {relative}")
        target = source_dir / source.name
        meta = Path(str(source) + ".meta")
        if target.exists() and sha(target) != sha(source):
            raise RuntimeError(f"Existing isolated probe copy differs: {target}")
        if not target.exists():
            shutil.copy2(source, target)
        meta_target = Path(str(target) + ".meta")
        if meta_target.exists() and sha(meta_target) != sha(meta):
            raise RuntimeError(f"Existing isolated import metadata differs: {meta_target}")
        if not meta_target.exists():
            shutil.copy2(meta, meta_target)
        copies[relative] = {"sha256": sha(source), "meta_sha256": sha(meta),
                            "probe_asset": "Assets/Source/" + source.name}
    renderer_target = PROBE / "Assets/RenderRokCityGammaCompositeNative.cs"
    if renderer_target.exists() and sha(renderer_target) != sha(RENDERER) and existing_runs:
        raise RuntimeError("Existing native runs pin another copied renderer")
    shutil.copy2(RENDERER, renderer_target)
    (PROBE / "Assets/ProbeDependencies.cs").write_text(
        """using System;
using UnityEngine;
namespace Skyunion {
  public static class CoreUtils { public static ProbeAssetService assetService = new ProbeAssetService(); }
  public sealed class ProbeAssetService {
    public void LoadAssetAsync<T>(string path, Action<ProbeAsset> callback, GameObject owner)
      where T : UnityEngine.Object { throw new InvalidOperationException("Async source asset loading is outside composition probe"); }
  }
  public sealed class ProbeAsset { public UnityEngine.Object asset() { return null; } }
}
namespace Client {
  public sealed class NightingMask : MonoBehaviour { public void SetCanChangeLight(bool value) {} }
}
""", encoding="utf-8")
    (PROBE / "Packages").mkdir(exist_ok=True)
    (PROBE / "Packages/manifest.json").write_text(json.dumps({"dependencies": {
        "com.unity.modules.imageconversion": "1.0.0", "com.unity.modules.jsonserialize": "1.0.0",
        "com.unity.modules.ui": "1.0.0"}}, indent=2) + "\n", encoding="utf-8")
    (PROBE / "ProjectSettings").mkdir(exist_ok=True)
    (PROBE / "ProjectSettings/ProjectVersion.txt").write_text(
        "m_EditorVersion: " + UNITY_VERSION + "\n", encoding="utf-8")
    contract = {
        "schema": "rok.city_gamma_composite_native_contract.v1",
        "scope": "Isolated Unity 2019 Gamma source sprite-composition controls. Diagnostic offsets and sorting, not actual city placement or UE acceptance.",
        "source_catalogs": {"academy": sha(ACADEMY_CATALOG), "resources": sha(RESOURCE_CATALOG)},
        "source_files": copies, "renderer_script_sha256": sha(RENDERER),
        "camera_source_sha256": sha(CAMERA_CSV), "unity_project_settings_sha256": sha(PROJECT_SETTINGS),
        "old_academy_control": {"run": str(OLD_RUN), "native_render_sha256": sha(old_render),
                                "native_verify_sha256": sha(old_verify),
                                "neutral_png_sha256": sha(old_neutral), "day_png_sha256": sha(old_day)},
        "old_resource_controls": old_resources,
        "old_resource_verified_summary_sha256": sha(OLD_RESOURCE_VERIFY),
        "models": records, "material": academy["material"],
        "camera": {"source_row": "city_default", "dxf": 150.0, "distance": 20.0,
                   "vertical_fov": 7.5, "forward": [0.0, -0.7, 0.7], "width": 1280, "height": 720},
        "lighting": {"neutral_sprite_color": [1, 1, 1, 1],
                     "day_sprite_color": [1.07203974, 1.07203974, 1.07203974, 1],
                     "city_transparency": 1.0},
        "diagnostic_offsets": {"academy": [0, 0, 0], "farm": [0.12, 0, 0], "wood": [-0.10, 0, 0]},
        "fixtures": [
            {"name": "academy_neutral", "light": "neutral", "background": "flat", "layers": [{"label": "academy", "order": 0}]},
            {"name": "academy_day", "light": "day", "background": "flat", "layers": [{"label": "academy", "order": 0}]},
            {"name": "pattern_only", "light": "day", "background": "pattern", "layers": []},
            {"name": "af_academy_front_flat", "light": "day", "background": "flat", "layers": [{"label": "academy", "order": 2}, {"label": "farm", "order": 1}]},
            {"name": "af_farm_front_flat", "light": "day", "background": "flat", "layers": [{"label": "academy", "order": 1}, {"label": "farm", "order": 2}]},
            {"name": "af_academy_front_pattern", "light": "day", "background": "pattern", "layers": [{"label": "academy", "order": 2}, {"label": "farm", "order": 1}]},
            {"name": "af_farm_front_pattern", "light": "day", "background": "pattern", "layers": [{"label": "academy", "order": 1}, {"label": "farm", "order": 2}]},
            {"name": "afw_three_layers_pattern", "light": "day", "background": "pattern", "layers": [{"label": "academy", "order": 3}, {"label": "farm", "order": 2}, {"label": "wood", "order": 1}]},
        ],
        "predeclared_budgets": {"old_academy_neutral_day_max_rgb_abs": 0,
                                "pattern_only_max_rgb_abs_from_source_png": 1,
                                "pair_or_triple_overlap_pixels_min": 1000,
                                "pair_order_changed_overlap_pixels_min": 100,
                                "alpha_coverage_threshold_byte": 1},
    }
    target = PROBE / "probe_contract.json"
    if target.exists() and (PROBE / "OutputRuns").exists() and any((PROBE / "OutputRuns").iterdir()):
        if target.read_text(encoding="utf-8") != json.dumps(contract, ensure_ascii=False, indent=2) + "\n":
            raise RuntimeError("Completed probe run exists; refusing to change its contract")
    target.write_text(json.dumps(contract, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"CITY_GAMMA_COMPOSITE_PREPARED {target}")


if __name__ == "__main__":
    prepare()
