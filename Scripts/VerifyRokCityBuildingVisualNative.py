"""Verify one immutable city-building Unity source-render run; never launch Unity.

Same checks as VerifyRokCityResourceVisualNative.py, applied to the model list of
BuildRokCityBuildingVisualSources.py and its own probe/contract/summary paths.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image

from BuildRokCityBuildingVisualSources import OUT, ROOT, build
from PrepareRokCityBuildingVisualNative import PROBE, RENDERER
from PrepareRokCityResourceVisualNative import CAMERA_CSV, PROJECT_SETTINGS
from VerifyRokCityResourceVisualNative import close, control_delta, png_summary, sha, source_platforms, vector
from rok_data_paths import resolve


SUMMARY = ROOT / "Saved/RokUnityMigration/CityBuildingVisualNative/verified_capture.json"
RENDER_SCHEMA = "rok.city_building_visual_native_render.v1"
MASK_BINDING = "original Client.MaskSprite.Start MaterialPropertyBlock _Mask = mask_sprite.texture"


def verify_model(run, catalog, contract):
    model_id = contract["model"]
    model = next((item for item in catalog["models"] if item["model_id"] == model_id), None)
    if model is None:
        raise RuntimeError(f"Unexpected native model: {model_id}")
    model_dir = run / model_id
    report_path = model_dir / "native_render.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if (report["schema"] != RENDER_SCHEMA or report["model"] != model_id or
            report["sourceCatalogSha256"] != sha(OUT) or
            Path(report["runDirectory"]).resolve() != model_dir.resolve() or
            report["unityVersion"] != "2019.4.28f1c1" or report["colorSpace"] != "Gamma"):
        raise RuntimeError(f"Wrong report identity/version for {model_id}")
    if report["shaderName"] != catalog["material"]["shader_formula"]["shader_name"]:
        raise RuntimeError("Wrong source shader")
    if report["maskBinding"] != MASK_BINDING:
        raise RuntimeError("Original MaskSprite MPB binding not observed")
    if (report["spriteAsset"], report["maskAsset"], report["materialAsset"]) != (
            f"Assets/Source/{model_id}.png", f"Assets/Source/{model_id}_mask.png", "Assets/Source/map_building.mat"):
        raise RuntimeError("Native rendered a different source asset")
    for key, source in (("materialColor1", catalog["material"]["color_1"]),
                        ("materialColor2", catalog["material"]["color_2"]),
                        ("rendererColor", contract["sprite_child"]["renderer"]["color"])):
        vector(report[key], source, "rgba", key)
    if report["materialHasCityTransparencyProperty"] or report["materialGetFloatAttempted"] or \
            report["materialGetFloatAvailable"] or \
            report["materialGetFloatObservation"] != "unavailable: shader does not declare material property":
        raise RuntimeError("Source global _CityTransparency was incorrectly read as a material property")
    if not close(report["serializedSourceMaterialCityTransparency"], catalog["material"]["city_transparency"]):
        raise RuntimeError("Serialized source transparency changed")
    if (report["rendererFlipX"] != contract["sprite_child"]["renderer"]["flip_x"] or
            report["rendererFlipY"] != contract["sprite_child"]["renderer"]["flip_y"] or
            report["rendererSortingOrder"] != contract["sprite_child"]["renderer"]["sorting_order"] or
            not report["rendererEnabled"]):
        raise RuntimeError("Native SpriteRenderer state differs")
    geometry = report["geometry"]
    for prefix, node in (("root", contract["root"]), ("child", contract["sprite_child"])):
        for property_name, source_name, keys in (("LocalPosition", "local_position", "xyz"),
                                                 ("LocalScale", "local_scale", "xyz"),
                                                 ("LocalRotation", "local_rotation", "xyzw")):
            vector(geometry[prefix + property_name], node["transform"][source_name], keys,
                   prefix + property_name)
    vertex_count = len(geometry["vertices"])
    if vertex_count < 4 or len(geometry["uvs"]) != vertex_count or \
            len(geometry["worldVertices"]) != vertex_count or \
            len(geometry["screenVertices"]) != vertex_count or \
            len(geometry["triangles"]) < 6 or len(geometry["triangles"]) % 3:
        raise RuntimeError("Unity Sprite.mesh vertices/UV/triangles are incomplete")
    if not close(geometry["pixelsPerUnit"], model["sprite"]["importer"]["pixels_to_units"]):
        raise RuntimeError("Unity Sprite.mesh PPU differs from source meta")
    for role in ("sprite", "mask"):
        imported = report[role + "Import"]
        measured_size = [report[role + "TextureWidth"], report[role + "TextureHeight"]]
        if [imported["width"], imported["height"]] != measured_size or min(measured_size) <= 0:
            raise RuntimeError(f"{role} imported runtime dimensions are inconsistent")
        source_size = model[role]["size"]
        if measured_size[0] > source_size[0] or measured_size[1] > source_size[1]:
            raise RuntimeError(f"{role} imported image is larger than its source PNG")
        source_meta = source_platforms(resolve(model[role]["path"]))
        for platform, field in (("DefaultTexturePlatform", "defaultPlatform"), ("Standalone", "standalonePlatform")):
            actual, expected = imported[field], source_meta[platform]
            names = (("maxTextureSize", "maxTextureSize"), ("resizeAlgorithm", "resizeAlgorithm"),
                     ("format", "textureFormat"), ("textureCompression", "textureCompression"),
                     ("compressionQuality", "compressionQuality"))
            if actual["name"] != platform or any(actual[a] != expected[b] for a, b in names) or \
                    int(actual["overridden"]) != expected["overridden"]:
                raise RuntimeError(f"{role} import platform settings differ from source meta")
        importer = model[role]["importer"]
        if not close(report[role + "ImporterPixelsPerUnit"], importer["pixels_to_units"]) or \
                not close(report[role + "ImporterPivot"]["x"], importer["pivot"][0]) or \
                not close(report[role + "ImporterPivot"]["y"], importer["pivot"][1]) or \
                report[role + "ImporterMeshType"] != importer["sprite_mesh_type"] or \
                report[role + "ImporterSrgb"] != bool(importer["srgb"]) or \
                report[role + "ImporterAlphaIsTransparency"] != bool(importer["alpha_is_transparency"]):
            raise RuntimeError(f"{role} runtime importer differs from source meta")
    camera = report["camera"]
    expected_camera = contract["camera"]
    if any(camera[key] != expected_camera[key] for key in ("source_row", "width", "height")) or \
            any(not close(camera[key], expected_camera[key], 1e-6) for key in ("dxf", "distance", "vertical_fov")) or \
            any(not close(x, y, 1e-6) for x, y in zip(camera["forward"], expected_camera["forward"])):
        raise RuntimeError("Source city_default camera changed")
    frames = report["frames"]
    if [item["name"] for item in frames] != ["neutral", "day", "day_exported_mask_control"]:
        raise RuntimeError("Native lighting frames differ")
    frame_summaries = {}
    for frame in frames:
        if [frame["width"], frame["height"]] != [1280, 720]:
            raise RuntimeError("Rendered image dimensions differ")
        is_control = frame["name"] == "day_exported_mask_control"
        color_key = "day_sprite_color" if is_control else frame["name"] + "_sprite_color"
        vector(frame["spriteColor"], dict(zip("rgba", contract["lighting"][color_key])), "rgba", "lighting")
        if not close(frame["globalCityTransparencyAtRender"], contract["lighting"]["city_transparency"]):
            raise RuntimeError("Render-time global _CityTransparency differs")
        if frame["maskSource"] != ("gpu_exported_rgba8_reload" if is_control else "original_unity_imported"):
            raise RuntimeError("Control mask source differs")
        if Path(frame["png"]).name != frame["png"]:
            raise RuntimeError("Frame filename escapes model directory")
        clear = [frame["clearColor"][channel] for channel in "rgba"]
        frame_summaries[frame["name"]] = png_summary(model_dir / frame["png"], (1280, 720), clear)
    if frame_summaries["neutral"]["sha256"] == frame_summaries["day"]["sha256"]:
        raise RuntimeError("Neutral/day source frames are identical")
    export = report["maskExport"]
    if export["png"] != model_id + "_mask_imported_gpu_rgba.png" or \
            [export["width"], export["height"]] != [report["maskTextureWidth"], report["maskTextureHeight"]] or \
            export["renderTargetSrgb"] or \
            export["reloadFilterMode"] != report["maskImport"]["filterMode"] or \
            export["reloadWrapMode"] != report["maskImport"]["wrapMode"]:
        raise RuntimeError("GPU mask export/reload measurements are inconsistent")
    export_path = model_dir / export["png"]
    with Image.open(export_path) as image:
        image.load()
        if image.mode != "RGBA" or list(image.size) != [export["width"], export["height"]]:
            raise RuntimeError("GPU mask PNG format or dimensions differ")
        rgba = np.asarray(image, dtype=np.uint8)
        if int(np.count_nonzero(rgba[:, :, 0])) < 100:
            raise RuntimeError("GPU-decoded source mask red channel is empty")
    delta = control_delta(model_dir / frames[1]["png"], model_dir / frames[2]["png"],
                          [frames[1]["clearColor"][channel] for channel in "rgba"])
    result = {"model": model_id, "native_report_sha256": sha(report_path),
              "source_sprite_png_size": model["sprite"]["size"],
              "source_mask_png_size": model["mask"]["size"],
              "imported_sprite_size": [report["spriteTextureWidth"], report["spriteTextureHeight"]],
              "imported_mask_size": [report["maskTextureWidth"], report["maskTextureHeight"]],
              "sprite_vertex_count": vertex_count, "sprite_triangle_count": len(geometry["triangles"]) // 3,
              "gpu_mask_export_sha256": sha(export_path), "gpu_mask_export": export,
              "frames": frame_summaries, "export_control_delta": delta}
    (model_dir / "native_verify.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return result


def verify(run):
    run = run.resolve()
    if run.parent != (PROBE / "OutputRuns").resolve() or not run.is_dir():
        raise RuntimeError("Pass one exact immutable OutputRuns child directory")
    catalog = json.loads(OUT.read_text(encoding="utf-8"))
    if catalog != build(write=False):
        raise RuntimeError("Source visual catalog changed")
    contract_path = PROBE / "probe_contract.json"
    package = json.loads(contract_path.read_text(encoding="utf-8"))
    if package["schema"] != "rok.city_building_visual_native_inputs.v1" or \
            package["source_catalog_sha256"] != sha(OUT) or \
            package["renderer_script_sha256"] != sha(RENDERER) or \
            package["renderer_script_sha256"] != sha(PROBE / "Assets" / RENDERER.name) or \
            package["camera_source_sha256"] != sha(CAMERA_CSV) or \
            package["unity_project_settings_sha256"] != sha(PROJECT_SETTINGS):
        raise RuntimeError("Prepared native contract or renderer/camera/color-space source changed")
    if [item["model"] for item in package["models"]] != [m["model_id"] for m in catalog["models"]]:
        raise RuntimeError("Wrong exact source models")
    for relative, expected in catalog["sources"].items():
        if sha(resolve(relative)) != expected:
            raise RuntimeError(f"Visual catalog source changed: {relative}")
    for relative, copied in package["source_files"].items():
        origin = resolve(relative)
        copy = PROBE / copied["probe_asset"]
        if sha(origin) != copied["sha256"] or sha(copy) != copied["sha256"] or \
                sha(origin.with_name(origin.name + ".meta")) != copied["meta_sha256"] or \
                sha(copy.with_name(copy.name + ".meta")) != copied["meta_sha256"]:
            raise RuntimeError(f"Prepared source copy changed: {relative}")
    models = [verify_model(run, catalog, item) for item in package["models"]]
    result = {"schema": "rok.city_building_visual_native_verify.v1", "passed": True,
              "run_directory": str(run), "source_catalog_sha256": sha(OUT),
              "input_contract_sha256": sha(contract_path), "renderer_script_sha256": sha(RENDERER),
              "scope": "Unity 2019 Gamma source prefab subtrees only; no full-city LOD, account lighting, UE asset or parity acceptance.",
              "models": models}
    SUMMARY.parent.mkdir(parents=True, exist_ok=True)
    SUMMARY.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"CITY_BUILDING_VISUAL_NATIVE_SOURCE_PASS {SUMMARY}")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", required=True, type=Path, help="Exact new OutputRuns child directory")
    verify(parser.parse_args().run)
