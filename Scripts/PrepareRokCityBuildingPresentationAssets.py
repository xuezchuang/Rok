"""Prepare exact local city-building Unity Sprite.mesh OBJ inputs, offline only.

Same OBJ/manifest contract as PrepareRokCityResourcePresentationAssets.py, for the model
list of BuildRokCityBuildingVisualSources.py. Topology and mask size are taken from the
verified native run instead of per-model constants.
"""
import argparse
import json
import math
from pathlib import Path

from PIL import Image

from BuildRokCityBuildingVisualSources import OUT as CATALOG, ROOT, build
from PrepareRokCityBuildingVisualNative import PROBE
from PrepareRokCityResourcePresentationAssets import asset_names, canonical_sha, obj_text, sha
from VerifyRokCityBuildingVisualNative import MASK_BINDING, RENDER_SCHEMA, SUMMARY
from rok_data_paths import resolve


RUNS = PROBE / "OutputRuns"
OUTPUT = ROOT / "Saved/RokUnityMigration/CityBuildingPresentationAssets"
PACKAGE_ROOT = "/Game/RokUnity/City/SourceBuildings/"


def load_source(run):
    run = run.resolve()
    if run.parent != RUNS.resolve() or not run.is_dir():
        raise RuntimeError("City building native run must be one direct OutputRuns child")
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    if catalog != build(write=False) or catalog["schema"] != "rok.city_building_visual_sources.v1":
        raise RuntimeError("City building visual source catalog differs from current immutable sources")
    for relative, expected in catalog["sources"].items():
        if sha(resolve(relative)) != expected:
            raise RuntimeError(f"City building source hash differs: {relative}")
    summary = json.loads(SUMMARY.read_text(encoding="utf-8"))
    models = [model["model_id"] for model in catalog["models"]]
    if (summary["schema"] != "rok.city_building_visual_native_verify.v1" or not summary["passed"] or
            Path(summary["run_directory"]).resolve() != run or
            summary["source_catalog_sha256"] != sha(CATALOG) or
            summary["input_contract_sha256"] != sha(PROBE / "probe_contract.json") or
            [item["model"] for item in summary["models"]] != models):
        raise RuntimeError("City building native source verification is not pinned to this run")
    formula = catalog["material"]["shader_formula"]
    if (formula["shader_name"] != "custom/map/map_building_with_mask-5" or
            formula["blend"] != "Blend SrcAlpha OneMinusSrcAlpha, SrcAlpha OneMinusSrcAlpha" or
            formula["cull"] != "Cull Off" or formula["zwrite"] != "ZWrite Off" or
            "mask.r" not in formula["mask_1"] or "mask.g" not in formula["rgb"] or
            "mask.b" in formula["rgb"] or "mask.a" in formula["rgb"]):
        raise RuntimeError("City building shader is no longer the source R/G mask path")
    records = {}
    for catalog_model, proven in zip(catalog["models"], summary["models"]):
        model_id = catalog_model["model_id"]
        model_dir = run / model_id
        report_path = model_dir / "native_render.json"
        verify_path = model_dir / "native_verify.json"
        report = json.loads(report_path.read_text(encoding="utf-8"))
        if (json.loads(verify_path.read_text(encoding="utf-8")) != proven or
                proven["native_report_sha256"] != sha(report_path) or
                report["schema"] != RENDER_SCHEMA or
                report["model"] != model_id or report["sourceCatalogSha256"] != sha(CATALOG) or
                Path(report["runDirectory"]).resolve() != model_dir or
                report["unityVersion"] != "2019.4.28f1c1" or report["colorSpace"] != "Gamma" or
                report["shaderName"] != formula["shader_name"] or report["maskBinding"] != MASK_BINDING):
            raise RuntimeError(f"City building native report identity/binding differs: {model_id}")
        if any(proven["export_control_delta"][scope]["max"] != 0
               for scope in ("whole_frame", "fixed_union_foreground")):
            raise RuntimeError(f"Unity imported-mask/exported-mask control differs: {model_id}")
        sprite = catalog_model["sprite"]
        mask = catalog_model["mask"]
        for role, item in (("sprite", sprite), ("mask", mask)):
            source_path = resolve(item["path"])
            if sha(source_path) != item["sha256"] or \
                    sha(source_path.with_name(source_path.name + ".meta")) != item["meta_sha256"]:
                raise RuntimeError(f"City building {model_id} {role} source/hash differs")
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
                    raise RuntimeError(f"City building {model_id} {role} native import settings differ")
            if (native_import["mipmapCount"] != 1 or native_import["filterMode"] != 1 or
                    native_import["wrapMode"] != 1):
                raise RuntimeError(f"City building {model_id} {role} imported filtering/mip differs")
        # The native import preserves aspect ratio at the configured maximum,
        # including NPOT decorations whose dimensions are not powers of two.
        mask_size = proven["imported_mask_size"]
        max_size = report["maskImport"]["defaultPlatform"]["maxTextureSize"]
        mask_export = model_dir / report["maskExport"]["png"]
        if (report["maskExport"]["png"] != f"{model_id}_mask_imported_gpu_rgba.png" or
                proven["gpu_mask_export_sha256"] != sha(mask_export) or
                [report["maskTextureWidth"], report["maskTextureHeight"]] != mask_size or
                [report["maskExport"]["width"], report["maskExport"]["height"]] != mask_size or
                max(mask_size) > max_size):
            raise RuntimeError(f"City building Unity GPU-decoded mask/source dimensions differ: {model_id}")
        with Image.open(mask_export) as image:
            if list(image.size) != mask_size or image.mode != "RGBA":
                raise RuntimeError(f"City building exported mask PNG encoding/size differs: {model_id}")
        geometry = report["geometry"]
        verts, uvs, indices = geometry["vertices"], geometry["uvs"], geometry["triangles"]
        if (len(uvs) != len(verts) or len(indices) % 3 or
                proven["sprite_vertex_count"] != len(verts) or
                proven["sprite_triangle_count"] * 3 != len(indices) or
                geometry["rect"]["width"] != report["spriteTextureWidth"] or
                geometry["rect"]["height"] != report["spriteTextureHeight"] or
                geometry["pivot"] != {"x": report["spriteTextureWidth"] / 2.0,
                                       "y": report["spriteTextureHeight"] / 2.0} or
                len(geometry["worldVertices"]) != len(verts)):
            raise RuntimeError(f"City building source tight SpriteMesh geometry differs: {model_id}")
        if (any(not math.isfinite(float(v[key])) for v in verts for key in ("x", "y")) or
                any(not math.isfinite(float(uv[key])) or not -1e-6 <= float(uv[key]) <= 1.0 + 1e-6
                    for uv in uvs for key in ("x", "y")) or
                any(not isinstance(index, int) or not 0 <= index < len(verts) for index in indices)):
            raise RuntimeError(f"City building source mesh contains invalid coordinates: {model_id}")
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
            raise RuntimeError(f"City building source prefab SpriteRenderer/MaskSprite chain differs: {model_id}")
        records[model_id] = {"catalog": catalog_model, "native_verify": proven,
                             "native_render": report, "native_dir": model_dir, "mask_size": mask_size}
    return catalog, summary, records


def zero_area_faces(geometry):
    original = geometry["vertices"]
    indices = geometry["triangles"]
    result = []
    for offset in range(0, len(indices), 3):
        a, b, c = (original[indices[offset + i]] for i in range(3))
        if (b["x"] - a["x"]) * (c["y"] - a["y"]) - (b["y"] - a["y"]) * (c["x"] - a["x"]) == 0.0:
            result.append(offset // 3)
    return result


def component_transform(geometry):
    child_p = geometry["childLocalPosition"]
    child_q = geometry["childLocalRotation"]
    child_s = geometry["childLocalScale"]
    x, y, z = (child_s[axis] for axis in ("x", "y", "z"))
    if not all(map(math.isfinite, (x, y, z))) or x <= 0.0 or y <= 0.0 or z < 0.0:
        raise RuntimeError("Invalid city building source sprite scale")
    # The sprite is planar. A collapsed Unity normal axis has no effect on
    # its vertices, so retain a nonzero UE normal scale for collision.
    scale = {"uniform_scale": x * 5.0} if x == y == z else {
        "scale_xyz": [x * 5.0, (z or x) * 5.0, y * 5.0],
    }
    return {
        "translation_cm": [-child_p["x"] * 500.0, child_p["z"] * 500.0, child_p["y"] * 500.0],
        "rotation_quaternion_xyzw": [-child_q["x"], child_q["z"], child_q["y"], child_q["w"]],
        **scale,
        "scope": "Source prefab child-local pose relative to root; runtime root/grid pose supplied separately",
    }


def prepare(run):
    run = run.resolve()
    catalog, summary, records = load_source(run)
    base = OUTPUT / run.name
    reports = []
    for model_id, record in records.items():
        source = record["catalog"]
        geometry = record["native_render"]["geometry"]
        text, vertices, faces = obj_text(model_id, geometry)
        target = base / model_id
        target.mkdir(parents=True, exist_ok=True)
        obj_path = target / (asset_names(model_id)["mesh"] + ".obj")
        obj_path.write_text(text, encoding="utf-8", newline="\n")
        degenerate = zero_area_faces(geometry)
        package = PACKAGE_ROOT + model_id
        names = asset_names(model_id)
        manifest = {
            "schema": "rok.city_building_source_sprite_mesh.v1", "model": model_id,
            "scope": "Original local Sprite.mesh only; no runtime root/grid pose baked",
            "source_run": str(run), "source_catalog_sha256": sha(CATALOG),
            "native_verified_capture_sha256": sha(SUMMARY),
            "native_render_sha256": sha(record["native_dir"] / "native_render.json"),
            "native_verify_sha256": sha(record["native_dir"] / "native_verify.json"),
            "source_sprite_png_sha256": source["sprite"]["sha256"],
            "source_mask_png_sha256": source["mask"]["sha256"],
            "gpu_mask_export_sha256": record["native_verify"]["gpu_mask_export_sha256"],
            "gpu_mask_size": record["mask_size"],
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
            "component_relative_transform_example": component_transform(geometry),
            "ue_assets": {role: package + "/" + name for role, name in names.items()},
            "runtime_visual_acceptance": "pending actual UE capture against verified Unity source",
        }
        manifest_path = target / "source_mesh_manifest.json"
        manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        reports.append({"model": model_id, "manifest": str(manifest_path),
                        "manifest_sha256": sha(manifest_path), "obj_sha256": sha(obj_path),
                        "vertex_count": len(vertices), "source_triangle_count": len(faces),
                        "source_zero_area_triangles": len(degenerate)})
    result = {"schema": "rok.city_building_source_mesh_prepared.v1", "run": str(run),
              "source_catalog_sha256": sha(CATALOG), "native_verified_capture_sha256": sha(SUMMARY),
              "models": reports, "runtime_visual_acceptance": "pending"}
    report_path = base / "prepared.json"
    report_path.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return report_path


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path, required=True)
    args = parser.parse_args()
    print("CITY_BUILDING_SOURCE_MESH_PREPARED", prepare(args.run))
