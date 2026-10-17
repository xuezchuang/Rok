"""Verify isolated Unity Academy rendering against pinned source geometry and chain.

This checks identity, material binding, camera, image integrity and visible
coverage. It does not claim UE parity or full-city GameView equivalence.
"""

import hashlib
import argparse
import json
from pathlib import Path
from rok_data_paths import resolve

import numpy as np
from PIL import Image

from BuildRokAcademyVisualSource import ROOT, OUT, build
from PrepareRokAcademyVisualNative import PROBE


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def close(a, b, eps=1e-5):
    return abs(float(a) - float(b)) <= eps


def same_vector(actual, expected, keys, label):
    if not all(close(actual[key], expected[key]) for key in keys):
        raise RuntimeError(f"{label} changed: {actual} != {expected}")


def pixels(path, size, background):
    with Image.open(path) as source:
        source.load()
        if source.mode != "RGB" or source.size != size:
            raise RuntimeError(f"Unexpected Academy probe PNG format: {path}")
        array = np.asarray(source, dtype=np.int16)
    baseline = np.asarray(background[:3], dtype=np.float64) * 255.0
    delta = np.max(np.abs(array - baseline), axis=2)
    foreground = delta > 4.0  # Clear color quantization can differ by one byte.
    count = int(np.count_nonzero(foreground))
    if count < 100 or count > size[0] * size[1] // 4:
        raise RuntimeError(f"Unexpected Academy visible coverage: {count}")
    yy, xx = np.where(foreground)
    return {"sha256": sha(path), "non_background_pixels": count,
            "bbox_xyxy": [int(xx.min()), int(yy.min()), int(xx.max()), int(yy.max())]}


def source_platforms(path):
    text = Path(str(path) + ".meta").read_text(encoding="utf-8-sig")
    area = text.split("  platformSettings:\n", 1)[1].split("  spriteSheet:", 1)[0]
    result = {}
    for block in area.split("  - serializedVersion: 3\n"):
        if "buildTarget:" not in block:
            continue
        lines = dict(line.strip().split(": ", 1) for line in block.splitlines() if ": " in line)
        result[lines["buildTarget"]] = {key: int(lines[key]) for key in (
            "maxTextureSize", "resizeAlgorithm", "textureFormat", "textureCompression",
            "compressionQuality", "overridden")}
    return result


def compare_images(day_path, control_path, clear):
    with Image.open(day_path) as image:
        day = np.asarray(image.convert("RGB"), dtype=np.int16)
    with Image.open(control_path) as image:
        control = np.asarray(image.convert("RGB"), dtype=np.int16)
    if day.shape != control.shape:
        raise RuntimeError("Original/export-control Academy frame sizes differ")
    background = np.asarray(clear[:3], dtype=np.float64) * 255.0
    foreground = np.maximum(np.max(np.abs(day - background), axis=2),
                            np.max(np.abs(control - background), axis=2)) > 4.0
    if int(np.count_nonzero(foreground)) < 100:
        raise RuntimeError("Academy mask control has no visible source geometry")
    difference = np.abs(day.astype(np.int16) - control.astype(np.int16))
    def stats(values):
        return {"mae": float(np.mean(values)), "p95": float(np.percentile(values, 95)),
                "max": int(np.max(values)), "changed_rgb_samples": int(np.count_nonzero(values))}
    return {"whole_frame": stats(difference), "fixed_union_foreground": stats(difference[foreground]),
            "fixed_union_foreground_pixels": int(np.count_nonzero(foreground)),
            "rgb_by_channel": {key: stats(difference[foreground, index])
                               for index, key in enumerate("rgb")},
            "gate": "diagnostic-only; no predeclared pixel-parity budget for RGBA8 export control"}


def verify(output):
    output = output.resolve()
    if output.parent != (PROBE / "OutputRuns").resolve() or not output.is_dir():
        raise RuntimeError("Academy native run must be an existing direct OutputRuns child")
    catalog = json.loads(OUT.read_text(encoding="utf-8"))
    if catalog != build(write=False):
        raise RuntimeError("Academy source catalog changed")
    contract_path = PROBE / "probe_contract.json"
    contract = json.loads(contract_path.read_text(encoding="utf-8"))
    if contract["schema"] != "rok.academy_visual_native_probe.v1" or contract["source_catalog_sha256"] != sha(OUT):
        raise RuntimeError("Wrong native probe contract/catalog")
    if contract["renderer_script_sha256"] != sha(ROOT / "Scripts/RenderRokAcademyVisualNative.cs") or \
       contract["renderer_script_sha256"] != sha(PROBE / "Assets/RenderRokAcademyVisualNative.cs"):
        raise RuntimeError("Native renderer script differs from prepared copy")
    if contract["camera_source_sha256"] != sha(ROOT / "ref/config-tables/编译客户端配置/CSV/CameraParamLod.csv") or \
       contract["unity_project_settings_sha256"] != sha(ROOT / "ref/client-unity/ProjectSettings/ProjectSettings.asset"):
        raise RuntimeError("Native camera or Unity color-space source changed")
    for relative, expected in catalog["sources"].items():
        if sha(resolve(relative)) != expected:
            raise RuntimeError(f"Source changed: {relative}")
    for relative, copied in contract["source_files"].items():
        if sha(resolve(relative)) != copied["sha256"] or sha(PROBE / copied["probe_asset"]) != copied["sha256"]:
            raise RuntimeError(f"Native probe source copy changed: {relative}")
        if sha(Path(str(resolve(relative)) + ".meta")) != copied["meta_sha256"] or \
           sha(Path(str(PROBE / copied["probe_asset"]) + ".meta")) != copied["meta_sha256"]:
            raise RuntimeError(f"Native probe import metadata changed: {relative}")

    report_path = output / "native_render.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if report["schema"] != "rok.academy_visual_native_render.v2" or report["unityVersion"] != "2019.4.28f1c1" or \
       Path(report["runDirectory"]).resolve() != output:
        raise RuntimeError("Unexpected native renderer/version")
    if report["colorSpace"] != "Gamma" or report["shaderName"] != catalog["material"]["shader_formula"]["shader_name"]:
        raise RuntimeError("Unexpected native color space or shader")
    if report["maskBinding"] != "original Client.MaskSprite.Start MaterialPropertyBlock _Mask = mask_sprite.texture":
        raise RuntimeError("Original MaskSprite mask binding was not proven")
    if (report["spriteAsset"], report["maskAsset"], report["materialAsset"]) != (
        "Assets/Source/build_13_101_2.png", "Assets/Source/build_13_101_2_mask.png", "Assets/Source/map_building.mat"
    ):
        raise RuntimeError("Wrong native source assets")
    for key, source in (("materialColor1", catalog["material"]["color_1"]),
                        ("materialColor2", catalog["material"]["color_2"]),
                        ("rendererColor", contract["sprite_child"]["renderer"]["color"])):
        same_vector(report[key], source, "rgba", key)
    if not close(report["serializedSourceMaterialCityTransparency"], catalog["material"]["city_transparency"]):
        raise RuntimeError("Serialized source material CityTransparency changed")
    if report["materialHasCityTransparencyProperty"]:
        raise RuntimeError("Source shader unexpectedly declares CityTransparency as a material property")
    if report["materialGetFloatAttempted"] or report["materialGetFloatAvailable"] or \
       report["materialGetFloatObservation"] != "unavailable: shader does not declare material property" or \
       not isinstance(report["materialGetFloatCityTransparencyObserved"], (int, float)) or \
       not isinstance(report["globalCityTransparencyBeforeOverride"], (int, float)):
        raise RuntimeError("Source material/global transparency observations missing")
    if (report["rendererFlipX"] or report["rendererFlipY"] or not report["rendererEnabled"] or
            report["rendererSortingOrder"] != contract["sprite_child"]["renderer"]["sorting_order"]):
        raise RuntimeError("Native SpriteRenderer state differs")
    if [report["spriteTextureWidth"], report["spriteTextureHeight"]] != catalog["sprite"]["size"] or \
       [report["maskTextureWidth"], report["maskTextureHeight"]] != [256, 213]:
        raise RuntimeError("Native imported sprite/mask dimensions differ")
    for role in ("sprite", "mask"):
        imported = report[role + "Import"]
        if [imported["width"], imported["height"]] != [report[role + "TextureWidth"], report[role + "TextureHeight"]] or \
           imported["mipmapCount"] != 1:
            raise RuntimeError(role + " imported GPU size/mip differs")
        platforms = source_platforms(resolve(catalog[role]["path"]))
        for key, actual_key in (("DefaultTexturePlatform", "defaultPlatform"), ("Standalone", "standalonePlatform")):
            actual = imported[actual_key]
            expected = platforms[key]
            fields = {"maxTextureSize": "maxTextureSize", "resizeAlgorithm": "resizeAlgorithm",
                      "format": "textureFormat", "textureCompression": "textureCompression",
                      "compressionQuality": "compressionQuality"}
            if actual["name"] != key or any(actual[measured] != expected[source]
                                               for measured, source in fields.items()) or \
               int(actual["overridden"]) != expected["overridden"]:
                raise RuntimeError(role + " platform import settings differ from source .meta")
    if report["spriteImport"]["standalonePlatform"]["maxTextureSize"] != 2048 or \
       report["maskImport"]["standalonePlatform"]["maxTextureSize"] != 256:
        raise RuntimeError("Source Standalone import-size cause changed")
    for prefix in ("sprite", "mask"):
        importer = catalog[prefix]["importer"]
        if not close(report[prefix + "ImporterPixelsPerUnit"], importer["pixels_to_units"]) or \
           not close(report[prefix + "ImporterPivot"]["x"], importer["pivot"][0]) or \
           not close(report[prefix + "ImporterPivot"]["y"], importer["pivot"][1]) or \
           report[prefix + "ImporterMeshType"] != importer["sprite_mesh_type"] or \
           report[prefix + "ImporterSrgb"] != bool(importer["srgb"]) or \
           report[prefix + "ImporterAlphaIsTransparency"] != bool(importer["alpha_is_transparency"]):
            raise RuntimeError(prefix + " native importer differs from source")

    geometry = report["geometry"]
    # textureRect is measured by Unity rather than assumed from PNG size.
    if not close(geometry["pixelsPerUnit"], catalog["sprite"]["importer"]["pixels_to_units"]):
        raise RuntimeError("Native sprite PPU differs")
    if not close(geometry["rect"]["width"], catalog["sprite"]["size"][0]) or \
       not close(geometry["rect"]["height"], catalog["sprite"]["size"][1]):
        raise RuntimeError("Source sprite rect differs from importer dimensions")
    for key, source_node in (("root", contract["root"]), ("child", contract["sprite_child"])):
        prefix = "root" if key == "root" else "child"
        same_vector(geometry[prefix + "LocalPosition"], source_node["transform"]["local_position"], "xyz", prefix + " position")
        same_vector(geometry[prefix + "LocalScale"], source_node["transform"]["local_scale"], "xyz", prefix + " scale")
        same_vector(geometry[prefix + "LocalRotation"], source_node["transform"]["local_rotation"], "xyzw", prefix + " rotation")
    if len(geometry["vertices"]) < 4 or len(geometry["uvs"]) != len(geometry["vertices"]) or \
       len(geometry["worldVertices"]) != len(geometry["vertices"]) or \
       len(geometry["screenVertices"]) != len(geometry["vertices"]) or len(geometry["triangles"]) < 6:
        raise RuntimeError("Native sprite mesh/UV geometry missing")
    camera = report["camera"]
    source_camera = contract["camera"]
    if any(camera[key] != source_camera[key] for key in ("source_row", "width", "height")) or \
       any(not close(camera[key], source_camera[key], 1e-6) for key in ("dxf", "distance", "vertical_fov")) or \
       len(camera["forward"]) != len(source_camera["forward"]) or \
       any(not close(actual, expected, 1e-6)
           for actual, expected in zip(camera["forward"], source_camera["forward"])):
        raise RuntimeError("Native camera differs from pinned city_default")

    frames = report["frames"]
    if [f["name"] for f in frames] != ["neutral", "day", "day_exported_mask_control"]:
        raise RuntimeError("Native probe lighting controls differ")
    export = report["maskExport"]
    if export["png"] != "academy_mask_imported_gpu_rgba.png" or \
       [export["width"], export["height"]] != [256, 213] or \
       export["renderTargetSrgb"] or export["reloadFilterMode"] != report["maskImport"]["filterMode"] or \
       export["reloadWrapMode"] != report["maskImport"]["wrapMode"]:
        raise RuntimeError("Native imported-mask GPU export settings differ")
    export_path = output / export["png"]
    with Image.open(export_path) as mask_image:
        mask_image.load()
        if mask_image.mode != "RGBA" or mask_image.size != (256, 213):
            raise RuntimeError("GPU-decoded Academy mask PNG format differs")
        mask_array = np.asarray(mask_image, dtype=np.uint8)
        if int(np.count_nonzero(mask_array[:, :, 0])) < 100 or int(np.count_nonzero(mask_array[:, :, 3])) < 100:
            raise RuntimeError("GPU-decoded Academy mask channels are empty")
    summaries = {}
    for frame in frames:
        if (frame["width"], frame["height"]) != (contract["camera"]["width"], contract["camera"]["height"]):
            raise RuntimeError("Native render dimensions differ")
        control = frame["name"] == "day_exported_mask_control"
        expected_color = contract["lighting"]["day_sprite_color" if control else frame["name"] + "_sprite_color"]
        same_vector(frame["spriteColor"], dict(zip("rgba", expected_color)), "rgba", frame["name"] + " sprite color")
        if not close(frame["globalCityTransparencyAtRender"], contract["lighting"]["city_transparency"]):
            raise RuntimeError("Native global CityTransparency at render differs")
        if frame["maskSource"] != ("gpu_exported_rgba8_reload" if control else "original_unity_imported"):
            raise RuntimeError("Native frame mask binding changed")
        if Path(frame["png"]).name != frame["png"]:
            raise RuntimeError("Native frame path escapes output directory")
        summaries[frame["name"]] = pixels(output / frame["png"], (frame["width"], frame["height"]),
                                          [frame["clearColor"][key] for key in "rgba"])
    if summaries["neutral"]["sha256"] == summaries["day"]["sha256"]:
        raise RuntimeError("Native lighting-control frames are identical")
    day_clear = [frames[1]["clearColor"][key] for key in "rgba"]
    control_delta = compare_images(output / frames[1]["png"], output / frames[2]["png"], day_clear)
    result = {"schema": "rok.academy_visual_native_verify.v2", "passed": True,
              "run_directory": str(output),
              "scope": "Isolated Unity source sprite/shader/MaskSprite/geometry only; not UE or full city parity.",
              "catalog_sha256": sha(OUT), "contract_sha256": sha(contract_path),
              "native_report_sha256": sha(report_path), "frames": summaries,
              "gpu_decoded_mask_export_sha256": sha(export_path),
              "gpu_decoded_mask_export": export,
              "source_mask_png_size": catalog["mask"]["size"],
              "native_imported_mask_size": [report["maskTextureWidth"], report["maskTextureHeight"]],
              "export_control_delta": control_delta,
              "sprite_vertex_count": len(geometry["vertices"]),
              "sprite_triangle_count": len(geometry["triangles"]) // 3,
              "sprite_rect": geometry["rect"], "sprite_pivot_pixels": geometry["pivot"],
              "sprite_pixels_per_unit": geometry["pixelsPerUnit"]}
    path = output / "native_verify.json"
    path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"ACADEMY_NATIVE_SOURCE_PASS {path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", required=True, type=Path, help="Exact new Academy OutputRuns child directory")
    verify(parser.parse_args().run)
