"""Verify the new Unity 2019 Gamma GPU composition fixtures, without UE claims."""

import argparse
import hashlib
import json
from pathlib import Path
from rok_data_paths import resolve

import numpy as np
from PIL import Image

from BuildRokAcademyVisualSource import ROOT, OUT as ACADEMY_CATALOG, build as academy_build
from BuildRokCityResourceBuildingVisualSources import OUT as RESOURCE_CATALOG, build as resource_build
from PrepareRokCityGammaCompositeNative import (
    PROBE, RENDERER, CAMERA_CSV, PROJECT_SETTINGS, OLD_RUN, OLD_RESOURCE_VERIFY)


OLD_RESOURCE_RUN = ROOT / "Saved/UnityCityResourceVisualProbe/OutputRuns/20261008_161052_827_45112"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def near(actual, expected, tolerance=1e-5):
    return abs(float(actual) - float(expected)) <= tolerance


def same_vector(actual, expected, keys, name, tolerance=1e-5):
    require(all(near(actual[key], expected[key], tolerance) for key in keys),
            f"{name} changed: {actual} != {expected}")


def image(path, mode, size):
    with Image.open(path) as source:
        source.load()
        require(source.mode == mode and source.size == size,
                f"Image mode/dimensions differ: {path}: {source.mode}/{source.size}")
        return np.asarray(source, dtype=np.uint8).copy()


def diff_stats(delta):
    return {"mae": float(np.mean(delta)), "p95": float(np.percentile(delta, 95)),
            "max": int(np.max(delta)), "changed_samples": int(np.count_nonzero(delta))}


def compare_native_geometry(actual, prior, offset, label):
    for key in ("rect", "textureRect", "pivot", "textureRectOffset", "pixelsPerUnit",
                "vertices", "uvs", "triangles", "spriteBoundsCenter", "spriteBoundsExtents",
                "childLocalPosition", "childLocalScale", "childLocalRotation"):
        require(actual[key] == prior[key], f"{label} source sprite geometry differs: {key}")
    for key in "xyz":
        require(near(actual["sourceRootLocalPosition"][key], prior["rootLocalPosition"][key]),
                f"{label} source root position changed")
        require(near(actual["sourceRootLocalScale"][key], prior["rootLocalScale"][key]),
                f"{label} source root scale changed")
        require(near(actual["actualRootPosition"][key],
                     prior["rootLocalPosition"][key] + offset["xyz".index(key)]),
                f"{label} diagnostic offset not applied as contracted")
        require(near(actual["diagnosticOffset"][key], offset["xyz".index(key)]),
                f"{label} diagnostic offset changed")
    same_vector(actual["sourceRootLocalRotation"], prior["rootLocalRotation"], "xyzw", label + " root rotation")
    require(len(actual["worldVertices"]) == len(actual["vertices"]) and
            len(actual["screenVertices"]) == len(actual["vertices"]),
            f"{label} projected full source mesh missing")
    for current, old in zip(actual["worldVertices"], prior["worldVertices"]):
        for i, key in enumerate("xyz"):
            require(near(current[key], old[key] + offset[i], 2e-5),
                    f"{label} projected source world vertex changed")


def verify_run(run):
    run = run.resolve()
    require(run.is_dir() and run.parent == (PROBE / "OutputRuns").resolve(),
            "--run must be an existing direct child of the isolated OutputRuns")
    contract_path = PROBE / "probe_contract.json"
    contract = json.loads(contract_path.read_text(encoding="utf-8"))
    report_path = run / "native_render.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    require(contract["schema"] == "rok.city_gamma_composite_native_contract.v1" and
            report["schema"] == "rok.city_gamma_composite_native_render.v1", "Wrong contract/report schema")
    require(report["runDirectory"].replace("\\", "/") == run.as_posix(), "Native output path changed")
    require(report["unityVersion"] == "2019.4.28f1c1" and report["colorSpace"] == "Gamma",
            "Native source Unity/Gamma identity changed")
    require(contract["renderer_script_sha256"] == sha(RENDERER) == sha(PROBE / "Assets/RenderRokCityGammaCompositeNative.cs"),
            "Native renderer script differs from prepared source")
    require(contract["camera_source_sha256"] == sha(CAMERA_CSV) and
            contract["unity_project_settings_sha256"] == sha(PROJECT_SETTINGS),
            "Camera/Unity color-space source changed")
    academy = json.loads(ACADEMY_CATALOG.read_text(encoding="utf-8"))
    resources = json.loads(RESOURCE_CATALOG.read_text(encoding="utf-8"))
    require(academy == academy_build(write=False) and resources == resource_build(write=False),
            "Pinned source catalogs differ from current source")
    require(contract["source_catalogs"] == {"academy": sha(ACADEMY_CATALOG),
                                            "resources": sha(RESOURCE_CATALOG)},
            "Pinned source catalog hashes changed")
    for relative, record in contract["source_files"].items():
        target = PROBE / record["probe_asset"]
        require(sha(resolve(relative)) == record["sha256"] == sha(target) and
                sha(Path(str(resolve(relative)) + ".meta")) == record["meta_sha256"] ==
                sha(Path(str(target) + ".meta")), f"Source/import copy changed: {relative}")
    old = contract["old_academy_control"]
    require(Path(old["run"]).resolve() == OLD_RUN.resolve(), "Old Academy control directory changed")
    for filename, key in (("native_render.json", "native_render_sha256"),
                          ("native_verify.json", "native_verify_sha256"),
                          ("academy_neutral.png", "neutral_png_sha256"),
                          ("academy_day.png", "day_png_sha256")):
        require(sha(OLD_RUN / filename) == old[key], "Old sealed Academy control changed: " + filename)
    require(json.loads((OLD_RUN / "native_verify.json").read_text(encoding="utf-8"))["passed"],
            "Old Academy control is not verified")
    resource_summary = json.loads(OLD_RESOURCE_VERIFY.read_text(encoding="utf-8"))
    require(contract["old_resource_verified_summary_sha256"] == sha(OLD_RESOURCE_VERIFY) and
            resource_summary["passed"], "Old resource verified summary changed")
    for label, pinned in contract["old_resource_controls"].items():
        expected_dir = OLD_RESOURCE_RUN / ("build_2_101_1" if label == "farm" else "build_3_101_1")
        require(Path(pinned["run"]).resolve() == expected_dir.resolve() and
                sha(expected_dir / "native_render.json") == pinned["native_render_sha256"] and
                sha(expected_dir / "native_verify.json") == pinned["native_verify_sha256"],
                "Old sealed resource control changed: " + label)
    require(report["shaderName"] == academy["material"]["shader_formula"]["shader_name"] and
            report["materialAsset"] == "Assets/Source/map_building.mat" and
            not report["materialHasCityTransparencyProperty"], "Source shader/material identity changed")
    same_vector(report["materialColor1"], academy["material"]["color_1"], "rgba", "material color1")
    same_vector(report["materialColor2"], academy["material"]["color_2"], "rgba", "material color2")
    require(near(contract["material"]["city_transparency"], 1), "Source transparency changed")
    source_camera, native_camera = contract["camera"], report["camera"]
    require(all(native_camera[key] == source_camera[key] for key in ("source_row", "width", "height")) and
            all(near(native_camera[key], source_camera[key], 1e-6) for key in
                ("dxf", "distance", "vertical_fov")) and
            all(near(actual, expected, 1e-6) for actual, expected in
                zip(native_camera["forward"], source_camera["forward"])),
            "Native camera differs from pinned city_default")
    size = (contract["camera"]["width"], contract["camera"]["height"])
    require(size == (1280, 720), "Unexpected source camera dimensions")

    previous_paths = {"academy": OLD_RUN / "native_render.json",
                      "farm": OLD_RESOURCE_RUN / "build_2_101_1/native_render.json",
                      "wood": OLD_RESOURCE_RUN / "build_3_101_1/native_render.json"}
    old_models = {}
    for label, path in previous_paths.items():
        verify_path = path.with_name("native_verify.json")
        require(verify_path.is_file(), f"Old {label} visual control is missing")
        old_models[label] = json.loads(path.read_text(encoding="utf-8"))
    native_models = report["models"]
    require([m["label"] for m in native_models] == ["academy", "farm", "wood"] and
            [m["label"] for m in contract["models"]] == ["academy", "farm", "wood"],
            "Model/source ordering changed")
    coverage = {}
    models_summary = {}
    for record, source in zip(native_models, contract["models"]):
        label = record["label"]
        prior = old_models[label]
        expected_model = source["model"]
        require(record["model"] == expected_model and
                record["sourceCatalogSha256"] == source["source_catalog_sha256"],
                label + " model/source identity changed")
        require(record["spriteAsset"] == "Assets/Source/" + Path(source["sprite"]["path"]).name and
                record["maskAsset"] == "Assets/Source/" + Path(source["mask"]["path"]).name and
                record["materialAsset"] == report["materialAsset"] and
                record["maskBinding"] == "original Client.MaskSprite.Start MaterialPropertyBlock _Mask = mask_sprite.texture",
                label + " source asset or mask binding changed")
        require(record["sourceSortingOrder"] == source["sprite_child"]["renderer"]["sorting_order"] and
                record["nativeSortingLayer"] == "Default" and
                record["nativeLayer"] == source["sprite_child"]["layer"] and
                record["flipX"] == source["sprite_child"]["renderer"]["flip_x"] and
                record["flipY"] == source["sprite_child"]["renderer"]["flip_y"],
                label + " native renderer changed")
        same_vector(record["rendererColor"], source["sprite_child"]["renderer"]["color"],
                    "rgba", label + " renderer color")
        for role in ("sprite", "mask"):
            actual = record[role + "Import"]
            old_import = prior[role + "Import"]
            require(all(actual[key] == old_import[key] for key in
                        ("width", "height", "mipmapCount", "format", "filterMode", "wrapMode")),
                    label + " " + role + " imported GPU texture changed")
            require(near(actual["pixelsPerUnit"], prior[role + "ImporterPixelsPerUnit"]) and
                    actual["importerMeshType"] == prior[role + "ImporterMeshType"] and
                    actual["importerSrgb"] == prior[role + "ImporterSrgb"] and
                    actual["importerAlphaIsTransparency"] == prior[role + "ImporterAlphaIsTransparency"],
                    label + " " + role + " importer changed")
            same_vector(actual["importerPivot"], prior[role + "ImporterPivot"], "xy",
                        label + " " + role + " importer pivot")
        offset = contract["diagnostic_offsets"][label]
        compare_native_geometry(record["geometry"], prior["geometry"], offset, label)
        require(record["coveragePng"] == label + "_coverage_rgba.png", label + " coverage path changed")
        rgba = image(run / record["coveragePng"], "RGBA", size)
        alpha = rgba[:, :, 3]
        # Original shader uses Blend SrcAlpha OneMinusSrcAlpha for alpha too.
        # Against transparent black the RT alpha is srcAlpha^2, subsequently
        # quantized to eight bits. This is a support-domain aid, not raw opacity.
        support = alpha > contract["predeclared_budgets"]["alpha_coverage_threshold_byte"]
        require(int(support.sum()) > 1000, label + " coverage absent")
        coverage[label] = support
        estimate = np.sqrt(alpha[support].astype(np.float64) / 255.0)
        models_summary[label] = {"source_model": expected_model, "coverage_png_sha256": sha(run / record["coveragePng"]),
                                 "postblend_alpha_support_pixels": int(support.sum()),
                                 "postblend_alpha_min_nonzero": int(alpha[support].min()),
                                 "postblend_alpha_max": int(alpha.max()),
                                 "sqrt_postblend_alpha_estimate_range": [float(estimate.min()), float(estimate.max())],
                                 "sqrt_qualification": "sqrt(a8/255) is a quantized mathematical estimate of shader alpha, not a measured texture/fragment alpha"}

    fixtures = contract["fixtures"]
    frames = report["frames"]
    require([f["name"] for f in fixtures] == [f["name"] for f in frames], "Frame fixture list/order changed")
    require(len(frames) == 8, "Eight independent fixtures required")
    arrays = {}
    frame_hashes = {}
    for planned, measured in zip(fixtures, frames):
        name = planned["name"]
        require(measured["png"] == name + ".png" and measured["light"] == planned["light"] and
                measured["background"] == planned["background"] and
                measured["layers"] == planned["layers"] and
                measured["width"] == size[0] and measured["height"] == size[1],
                name + " actual render fixture changed")
        require(not measured["cameraAllowHDR"] and not measured["cameraAllowMSAA"] and
                near(measured["globalCityTransparencyAtRender"], 1) and
                measured["targetFormatName"] == "ARGB32",
                name + " HDR/MSAA/global transparency changed")
        light = contract["lighting"][planned["light"] + "_sprite_color"]
        for key, expected in zip("rgba", light):
            require(near(measured["spriteColor"][key], expected), name + " sprite light changed")
        arrays[name] = image(run / measured["png"], "RGB", size)
        frame_hashes[name] = sha(run / measured["png"])
    require(len({(f["targetFormat"], f["targetSrgb"]) for f in frames}) == 1,
            "Target format/sRGB differs between fixtures")

    old_pixel_deltas = {}
    for name in ("academy_neutral", "academy_day"):
        difference = np.abs(arrays[name].astype(np.int16) -
                            image(OLD_RUN / (name + ".png"), "RGB", size).astype(np.int16))
        old_pixel_deltas[name] = diff_stats(difference)
        require(old_pixel_deltas[name]["max"] <= contract["predeclared_budgets"]["old_academy_neutral_day_max_rgb_abs"],
                name + " differs from old native source-pixel control")
    require(report["patternSourcePng"] == "pattern_source.png", "Pattern source PNG path changed")
    source_pattern = image(run / "pattern_source.png", "RGBA", size)
    require(np.all(source_pattern[:, :, 3] == 255), "Nonuniform background not opaque")
    source_rgb = source_pattern[:, :, :3].astype(np.int16)
    require(all(int(np.ptp(source_rgb[:, :, c])) >= 50 for c in range(3)),
            "Opaque background is too uniform")
    pattern_delta = np.abs(arrays["pattern_only"].astype(np.int16) - source_rgb)
    pattern_stats = diff_stats(pattern_delta)
    require(pattern_stats["max"] <= contract["predeclared_budgets"]["pattern_only_max_rgb_abs_from_source_png"],
            "GPU pattern-only frame differs from independently saved source bytes")

    pair_overlap = coverage["academy"] & coverage["farm"]
    triple_overlap = pair_overlap & coverage["wood"]
    counts = {"academy_farm": int(pair_overlap.sum()), "academy_farm_wood": int(triple_overlap.sum())}
    threshold = contract["predeclared_budgets"]["pair_or_triple_overlap_pixels_min"]
    require(all(value >= threshold for value in counts.values()),
            "Complete source mesh layers do not have predeclared overlap")
    order_results = {}
    for suffix in ("flat", "pattern"):
        first = arrays["af_academy_front_" + suffix].astype(np.int16)
        second = arrays["af_farm_front_" + suffix].astype(np.int16)
        pixel_changed = np.any(first != second, axis=2) & pair_overlap
        count = int(pixel_changed.sum())
        order_results[suffix] = {"changed_overlap_pixels": count,
                                 "rgb_delta_in_overlap": diff_stats(np.abs(first - second)[pair_overlap])}
        require(count >= contract["predeclared_budgets"]["pair_order_changed_overlap_pixels_min"],
                "Transparent source order has no meaningful GPU effect: " + suffix)

    return {"schema": "rok.city_gamma_composite_native_verify.v1", "passed": True,
            "qualification": "Unity 2019 Gamma isolated source-shader GPU composition with artificial offsets/order. No actual city-layout, UE, or product visual acceptance.",
            "run_directory": str(run), "contract_sha256": sha(contract_path),
            "native_render_sha256": sha(report_path), "renderer_sha256": sha(RENDERER),
            "old_control_max_deltas": old_pixel_deltas,
            "pattern_only_delta": pattern_stats, "pattern_source_png_sha256": sha(run / "pattern_source.png"),
            "models": models_summary, "frame_png_sha256": frame_hashes,
            "overlap_support_pixels": counts, "pair_order_effect": order_results,
            "predeclared_budgets": contract["predeclared_budgets"],
            "target_format_enum": frames[0]["targetFormat"], "target_srgb": frames[0]["targetSrgb"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", required=True, type=Path, help="Exact new OutputRuns child")
    args = parser.parse_args()
    if args.run.resolve().parent != (PROBE / "OutputRuns").resolve() or not args.run.is_dir():
        raise SystemExit("--run must be an existing direct child of the isolated OutputRuns")
    target = args.run / "native_verify.json"
    try:
        result = verify_run(args.run)
    except Exception as exc:
        result = {"schema": "rok.city_gamma_composite_native_verify.v1", "passed": False,
                  "run_directory": str(args.run.resolve()), "reason": str(exc)}
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(("CITY_GAMMA_COMPOSITE_NATIVE_PASS" if result["passed"] else
           "CITY_GAMMA_COMPOSITE_NATIVE_FAIL") + " " + str(target))
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
