"""Prepare exact local Farm/Sawmill Unity Sprite.mesh OBJ inputs, offline only."""

import argparse
import hashlib
import json
import math
from pathlib import Path
from rok_data_paths import resolve

from PIL import Image

from BuildRokCityResourceBuildingVisualSources import ROOT, OUT as CATALOG, build
from PrepareRokCityResourceVisualNative import PROBE


RUNS = PROBE / "OutputRuns"
SUMMARY = ROOT / "Saved/RokUnityMigration/CityResourceVisualNative/verified_capture.json"
OUTPUT = ROOT / "Saved/RokUnityMigration/CityResourcePresentationAssets"
MODELS = ("build_2_101_1", "build_3_101_1")
TOPOLOGY = {"build_2_101_1": (139, 177), "build_3_101_1": (43, 41)}
MASK_SIZES = {"build_2_101_1": (256, 256), "build_3_101_1": (256, 230)}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical_sha(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()


def asset_names(model_id):
    return {"sprite": f"T_{model_id}_SpriteRaw", "mask": f"T_{model_id}_MaskUnityGpu256",
            "material": f"M_{model_id}_SourceRG", "mesh": f"SM_{model_id}_SourceSpriteMesh"}


def load_source(run):
    run = run.resolve()
    if run.parent != RUNS.resolve() or not run.is_dir():
        raise RuntimeError("Resource native run must be one direct OutputRuns child")
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    if catalog != build(write=False) or catalog["schema"] != "rok.city_resource_building_visual_sources.v1":
        raise RuntimeError("Resource visual source catalog differs from current immutable sources")
    for relative, expected in catalog["sources"].items():
        if sha(resolve(relative)) != expected:
            raise RuntimeError(f"Resource source hash differs: {relative}")
    summary = json.loads(SUMMARY.read_text(encoding="utf-8"))
    contract_path = PROBE / "probe_contract.json"
    if (summary["schema"] != "rok.city_resource_visual_native_verify.v1" or not summary["passed"] or
            Path(summary["run_directory"]).resolve() != run or
            summary["source_catalog_sha256"] != sha(CATALOG) or
            summary["input_contract_sha256"] != sha(contract_path) or
            [item["model"] for item in summary["models"]] != list(MODELS) or
            [item["model_id"] for item in catalog["models"]] != list(MODELS)):
        raise RuntimeError("Farm/Sawmill native source verification is not pinned to this run")
    formula = catalog["material"]["shader_formula"]
    if (formula["shader_name"] != "custom/map/map_building_with_mask-5" or
            formula["blend"] != "Blend SrcAlpha OneMinusSrcAlpha, SrcAlpha OneMinusSrcAlpha" or
            formula["cull"] != "Cull Off" or formula["zwrite"] != "ZWrite Off" or
            "mask.r" not in formula["mask_1"] or "mask.g" not in formula["rgb"] or
            "mask.b" in formula["rgb"] or "mask.a" in formula["rgb"]):
        raise RuntimeError("Resource shader is no longer the source R/G mask path")
    records = {}
    for catalog_model, proven in zip(catalog["models"], summary["models"]):
        model_id = catalog_model["model_id"]
        model_dir = run / model_id
        report_path = model_dir / "native_render.json"
        verify_path = model_dir / "native_verify.json"
        report = json.loads(report_path.read_text(encoding="utf-8"))
        if (json.loads(verify_path.read_text(encoding="utf-8")) != proven or
                proven["native_report_sha256"] != sha(report_path) or
                report["schema"] != "rok.city_resource_visual_native_render.v2" or
                report["model"] != model_id or report["sourceCatalogSha256"] != sha(CATALOG) or
                Path(report["runDirectory"]).resolve() != model_dir or
                report["unityVersion"] != "2019.4.28f1c1" or report["colorSpace"] != "Gamma" or
                report["shaderName"] != formula["shader_name"] or
                report["maskBinding"] != "original Client.MaskSprite.Start MaterialPropertyBlock _Mask = mask_sprite.texture"):
            raise RuntimeError(f"Resource native report identity/binding differs: {model_id}")
        if any(proven["export_control_delta"][scope]["max"] != 0
               for scope in ("whole_frame", "fixed_union_foreground")):
            raise RuntimeError(f"Unity imported-mask/exported-mask control differs: {model_id}")
        sprite = catalog_model["sprite"]
        mask = catalog_model["mask"]
        for role, item in (("sprite", sprite), ("mask", mask)):
            if sha(resolve(item["path"])) != item["sha256"] or \
               sha(Path(str(resolve(item["path"])) + ".meta")) != item["meta_sha256"]:
                raise RuntimeError(f"Resource {model_id} {role} source/hash differs")
            native_import = report[role + "Import"]
            platform = item["platform"]["platform_settings"]
            for target, native_key in (("DefaultTexturePlatform", "defaultPlatform"),
                                       ("Standalone", "standalonePlatform")):
                src = next(p for p in platform if p["build_target"] == target)
                actual = native_import[native_key]
                if (actual["maxTextureSize"] != src["max_texture_size"] or
                        actual["resizeAlgorithm"] != src["resize_algorithm"] or
                        actual["format"] != src["texture_format"] or
                        int(actual["overridden"]) != src["overridden"]):
                    raise RuntimeError(f"Resource {model_id} {role} native import settings differ")
            if (native_import["mipmapCount"] != 1 or native_import["filterMode"] != 1 or
                    native_import["wrapMode"] != 1):
                raise RuntimeError(f"Resource {model_id} {role} imported filtering/mip differs")
        mask_export = model_dir / report["maskExport"]["png"]
        if (report["maskExport"]["png"] != f"{model_id}_mask_imported_gpu_rgba.png" or
                proven["gpu_mask_export_sha256"] != sha(mask_export) or
                (report["maskTextureWidth"], report["maskTextureHeight"]) != MASK_SIZES[model_id] or
                [report["maskExport"]["width"], report["maskExport"]["height"]] != list(MASK_SIZES[model_id]) or
                proven["imported_mask_size"] != list(MASK_SIZES[model_id]) or
                [report["spriteTextureWidth"], report["spriteTextureHeight"]] != sprite["size"] or
                report["maskImport"]["defaultPlatform"]["maxTextureSize"] != 256):
            raise RuntimeError(f"Resource Unity GPU-decoded mask/source dimensions differ: {model_id}")
        with Image.open(mask_export) as image:
            if image.size != MASK_SIZES[model_id] or image.mode != "RGBA":
                raise RuntimeError(f"Resource exported mask PNG encoding/size differs: {model_id}")
        geometry = report["geometry"]
        verts, uvs, indices = geometry["vertices"], geometry["uvs"], geometry["triangles"]
        if (len(verts) != TOPOLOGY[model_id][0] or len(uvs) != len(verts) or
                len(indices) != TOPOLOGY[model_id][1] * 3 or
                proven["sprite_vertex_count"] != len(verts) or
                proven["sprite_triangle_count"] * 3 != len(indices) or
                geometry["rect"]["width"] != sprite["size"][0] or
                geometry["rect"]["height"] != sprite["size"][1] or
                geometry["pivot"] != {"x": sprite["size"][0] / 2.0,
                                       "y": sprite["size"][1] / 2.0} or
                geometry["pixelsPerUnit"] != sprite["importer"]["pixels_to_units"] or
                len(geometry["worldVertices"]) != len(verts)):
            raise RuntimeError(f"Resource source tight SpriteMesh geometry differs: {model_id}")
        if (any(not math.isfinite(float(v[key])) for v in verts for key in ("x", "y")) or
                any(not math.isfinite(float(uv[key])) or not -1e-6 <= float(uv[key]) <= 1.0 + 1e-6
                    for uv in uvs for key in ("x", "y")) or
                any(not isinstance(index, int) or not 0 <= index < len(verts) for index in indices)):
            raise RuntimeError(f"Resource source mesh contains invalid coordinates: {model_id}")
        main = next(node for node in catalog_model["prefab"]["nodes"]
                    if node["file_id"] == catalog_model["prefab"]["main_sprite_game_object_file_id"])
        root = next(node for node in catalog_model["prefab"]["nodes"]
                    if node["file_id"] == catalog_model["prefab"]["root_game_object_file_id"])
        if (not root["active"] or not main["active"] or not main["renderer"]["enabled"] or
                main["transform"]["parent_transform_file_id"] != root["transform"]["file_id"] or
                main["renderer"]["sprite_guid"] != sprite["guid"] or
                main["mask_sprite"]["mask_sprite_guid"] != mask["guid"] or
                catalog_model["mask_binding"] != {
                    "script_guid": "f43e688d317b80c4d9560e9cb03707e9",
                    "component_file_id": main["mask_sprite"]["file_id"],
                    "property": "_Mask",
                    "mechanism": "MaterialPropertyBlock.SetTexture(_Mask, m_mask_sprite.texture)",
                }):
            raise RuntimeError(f"Resource source prefab SpriteRenderer/MaskSprite chain differs: {model_id}")
        records[model_id] = {"catalog": catalog_model, "native_verify": proven,
                             "native_render": report, "native_dir": model_dir}
    return catalog, summary, records


def obj_text(model_id, geometry):
    vertices = [[-float(v["x"]) * 100.0, 0.0, float(v["y"]) * 100.0]
                for v in geometry["vertices"]]
    indices = geometry["triangles"]
    lines = ["# Original imported Unity 2019 tight Sprite.mesh, local pivot, no prefab pose",
             "# Source model " + model_id, "o " + asset_names(model_id)["mesh"]]
    lines += [f"v {x:.9f} {y:.9f} {z:.9f}" for x, y, z in vertices]
    lines += [f"vt {float(uv['x']):.9f} {float(uv['y']):.9f}" for uv in geometry["uvs"]]
    lines.append("vn 0.000000000 1.000000000 0.000000000")
    faces = [tuple(indices[offset + i] + 1 for i in (2, 1, 0))
             for offset in range(0, len(indices), 3)]
    lines += [f"f {a}/{a}/1 {b}/{b}/1 {c}/{c}/1" for a, b, c in faces]
    return "\n".join(lines) + "\n", vertices, faces


def prepare(run):
    run = run.resolve()
    catalog, summary, records = load_source(run)
    base = OUTPUT / run.name
    reports = []
    for model_id in MODELS:
        record = records[model_id]
        source = record["catalog"]
        geometry = record["native_render"]["geometry"]
        text, vertices, faces = obj_text(model_id, geometry)
        target = base / model_id
        target.mkdir(parents=True, exist_ok=True)
        obj_path = target / (asset_names(model_id)["mesh"] + ".obj")
        obj_path.write_text(text, encoding="utf-8", newline="\n")
        areas = []
        original = geometry["vertices"]
        indices = geometry["triangles"]
        for offset in range(0, len(indices), 3):
            a, b, c = (original[indices[offset + i]] for i in range(3))
            areas.append((b["x"] - a["x"]) * (c["y"] - a["y"]) -
                         (b["y"] - a["y"]) * (c["x"] - a["x"]))
        degenerate = [i for i, area in enumerate(areas) if area == 0.0]
        child_p = geometry["childLocalPosition"]
        child_q = geometry["childLocalRotation"]
        child_s = geometry["childLocalScale"]
        if max(abs(child_s[key] - child_s["x"]) for key in ("y", "z")) > 1e-6:
            raise RuntimeError(f"Resource child source scale is no longer uniform: {model_id}")
        package = "/Game/RokUnity/City/ResourceBuildings/" + model_id
        names = asset_names(model_id)
        manifest = {
            "schema": "rok.city_resource_source_sprite_mesh.v1", "model": model_id,
            "scope": "Original local Sprite.mesh only; no runtime root/grid pose baked",
            "source_run": str(run), "source_catalog_sha256": sha(CATALOG),
            "native_verified_capture_sha256": sha(SUMMARY),
            "native_render_sha256": sha(record["native_dir"] / "native_render.json"),
            "native_verify_sha256": sha(record["native_dir"] / "native_verify.json"),
            "source_sprite_png_sha256": source["sprite"]["sha256"],
            "source_mask_png_sha256": source["mask"]["sha256"],
            "gpu_mask_export_sha256": record["native_verify"]["gpu_mask_export_sha256"],
            "source_geometry_sha256": canonical_sha({k: geometry[k] for k in
                ("vertices", "uvs", "triangles", "rect", "pivot", "pixelsPerUnit")}),
            "obj_file": obj_path.name, "obj_sha256": sha(obj_path),
            "vertex_count": len(vertices), "uv_count": len(geometry["uvs"]),
            "source_triangle_count": len(faces),
            "source_zero_area_triangle_indices": degenerate,
            "source_nonzero_area_triangle_count": len(faces) - len(degenerate),
            "unity_sprite_rect": geometry["rect"], "unity_sprite_pivot_pixels": geometry["pivot"],
            "unity_sprite_pixels_per_unit": geometry["pixelsPerUnit"],
            "ue_local_bounds_min_cm": [min(v[i] for v in vertices) for i in range(3)],
            "ue_local_bounds_max_cm": [max(v[i] for v in vertices) for i in range(3)],
            "coordinate_mapping": "Unity local (x,y,z) -> UE local (-x,z,y)*100; FbxFactory OBJ inverse Y compensation",
            "face_order": "Unity source triangle (a,b,c) -> OBJ (c,b,a); zero-area source triangles retained in OBJ",
            "uv_mapping": "Unity Sprite.uv -> OBJ vt(u,v); runtime import V orientation pending visual audit",
            "project_ue_units_per_unity_unit": 500.0, "obj_ue_units_per_unity_unit": 100.0,
            "component_relative_transform_example": {
                "translation_cm": [-child_p["x"] * 500.0, child_p["z"] * 500.0, child_p["y"] * 500.0],
                "rotation_quaternion_xyzw": [-child_q["x"], child_q["z"], child_q["y"], child_q["w"]],
                "uniform_scale": child_s["x"] * 5.0,
                "scope": "Source prefab child-local pose relative to root; runtime root/grid pose supplied separately"},
            "ue_assets": {role: package + "/" + name for role, name in names.items()},
            "runtime_visual_acceptance": "pending actual UE capture against verified Unity source",
        }
        manifest_path = target / "source_mesh_manifest.json"
        manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        reports.append({"model": model_id, "manifest": str(manifest_path),
                        "manifest_sha256": sha(manifest_path), "obj_sha256": sha(obj_path),
                        "vertex_count": len(vertices), "source_triangle_count": len(faces),
                        "source_zero_area_triangles": len(degenerate)})
    result = {"schema": "rok.city_resource_source_mesh_prepared.v1", "run": str(run),
              "source_catalog_sha256": sha(CATALOG), "native_verified_capture_sha256": sha(SUMMARY),
              "models": reports, "runtime_visual_acceptance": "pending"}
    report_path = base / "prepared.json"
    report_path.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return report_path


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path, required=True)
    args = parser.parse_args()
    print("CITY_RESOURCE_SOURCE_MESH_PREPARED", prepare(args.run))
