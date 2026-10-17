"""Prepare and verify a separate Unity 2019 GPU state/mip probe for five city families."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path
from rok_data_paths import resolve

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityGammaFamilyStateProbeV2"
BASE = ROOT / "Saved/UnityCityGammaRemainingFamiliesProbe/source_contract.json"
BASE_SHA = "accbc3065e32c0489795036cc5e8defda438cb0b749cd7df85b2005fe6846be3"
BASE_NATIVE = ROOT / "Saved/UnityCityGammaRemainingFamiliesProbe/OutputRuns/20261008_230313_337_7496/native_families.json"
BASE_NATIVE_SHA = "77bb28561050d01f078e41ccf6839b88d6530b28e422a9f1ba51db5693660b1b"
EXPORTER = ROOT / "Scripts/ExportRokCityGammaFamilyStateProbe.cs"
RUNNER = ROOT / "Scripts/RunRokCityGammaFamilyStateProbe.ps1"
CONTRACT_SCHEMA = "rok.city_gamma_family_state_contract.v2"
NATIVE_SCHEMA = "rok.city_gamma_family_state_native.v2"
KEYS = ("initial:5", "ground:ground_1", "ground:ground_2", "support:boundary_support_road", "support:boundary_support_wall_shadow")
SCRIPTS = (
    "ref/client-unity/Assets/Scripts/Client/Compoment/MaskSprite.cs",
    "ref/client-unity/Assets/Scripts/Client/LightingManager/NightingMask.cs",
    "ref/client-unity/Assets/Scripts/Client/LightingManager/NightingObject.cs",
)
STUB = """// Isolation-only compile shims for source MaskSprite dependencies unreachable in this probe.
using System;
using UnityEngine;
namespace Skyunion {
    public sealed class ProbeAsset { public UnityEngine.Object asset() { return null; } }
    public sealed class ProbeAssetService {
        public void LoadAssetAsync<T>(string path, Action<ProbeAsset> callback, GameObject owner)
            where T : UnityEngine.Object { throw new InvalidOperationException(\"GameApp asset service unavailable in isolated probe\"); }
    }
    public static class CoreUtils { public static readonly ProbeAssetService assetService = new ProbeAssetService(); }
}
namespace Client {
    public sealed class ProbeLightingManager {
        public bool isNight = false;
        public void RegisterNightObject(NightingObject value) { }
    }
    public static class ClientUtils { public static readonly ProbeLightingManager lightingManager = new ProbeLightingManager(); }
}
"""
MIP_SHADER = """Shader "Hidden/RokCityFamilyMipReadback" {
    Properties { _MainTex ("Source", 2D) = "white" {} _MipLevel ("Exact Mip", Float) = 0 }
    SubShader { Tags { "RenderType"="Opaque" "Queue"="Overlay" }
        Pass { Cull Off ZWrite Off ZTest Always Blend Off
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment frag
            #pragma target 3.0
            #include "UnityCG.cginc"
            sampler2D _MainTex;
            float _MipLevel;
            float4 frag(v2f_img i) : SV_Target {
                return tex2Dlod(_MainTex, float4(i.uv, 0, _MipLevel));
            }
            ENDCG
        }
    }
    Fallback Off
}
"""


def need(ok: bool, why: str) -> None:
    if not ok:
        raise ValueError(why)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path: Path) -> dict:
    need(path.is_file(), f"missing {path}")
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    need(isinstance(data, dict), f"bad JSON root {path}")
    return data


def contract() -> dict:
    need(sha(BASE) == BASE_SHA and sha(BASE_NATIVE) == BASE_NATIVE_SHA, "sealed five-family input evidence changed")
    base = read(BASE)
    native = read(BASE_NATIVE)
    need([r["source_key"] for r in base["rows"]] == list(KEYS) and
         [r["source_key"] for r in native["rows"]] == list(KEYS) and native["complete"],
         "five-family source/input proof differs")
    files = list(base["source_files"])
    for rel in SCRIPTS:
        path = resolve(rel)
        meta = Path(str(path) + ".meta")
        need(path.is_file() and meta.is_file(), f"source script/.meta absent: {rel}")
        match = re.search(r"(?m)^guid:\s*([0-9a-fA-F]{32})\s*$", meta.read_text(encoding="utf-8-sig"))
        need(match is not None, f"source script GUID absent: {rel}")
        files.append({"source_path": rel, "source_sha256": sha(path), "meta_sha256": sha(meta),
                      "source_guid": match.group(1).lower(),
                      "probe_path": "Assets/Source/" + rel.removeprefix("ref/client-unity/Assets/")})
    need(len({f["source_guid"] for f in files}) == len(files), "source GUID collision")
    need(EXPORTER.is_file() and RUNNER.is_file(), "new exporter/runner missing")
    return {"schema": CONTRACT_SCHEMA, "repo_root": ROOT.as_posix(), "probe_root": PROBE.as_posix(),
            "unity_version": "2019.4.28f1c1", "source_color_space": "Gamma",
            "base_contract_sha256": BASE_SHA, "base_native_sha256": BASE_NATIVE_SHA,
            "exporter_sha256": sha(EXPORTER), "runner_sha256": sha(RUNNER),
            "stub_sha256": hashlib.sha256(STUB.encode("utf-8")).hexdigest(),
            "mip_shader_sha256": hashlib.sha256(MIP_SHADER.encode("utf-8")).hexdigest(),
            "source_files": files, "rows": base["rows"],
            "scope": "isolated source material GPU state and explicit-LOD shader-expanded mip readbacks; no GameApp globals, runtime sort or fixed shader time"}


def prepare() -> None:
    c = contract()
    path = PROBE / "state_contract.json"
    runs = PROBE / "OutputRuns"
    has_runs = runs.is_dir() and any(runs.iterdir())
    if path.exists():
        need(read(path) == c or not has_runs, "state contract differs after native capture")
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or read(path) != c:
        path.write_text(json.dumps(c, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for item in c["source_files"]:
        src = resolve(item["source_path"])
        dst = PROBE / item["probe_path"]
        dst.parent.mkdir(parents=True, exist_ok=True)
        for a, b, digest in ((src, dst, item["source_sha256"]),
                             (Path(str(src) + ".meta"), Path(str(dst) + ".meta"), item["meta_sha256"])):
            need(a.is_file() and sha(a) == digest, f"source changed: {a}")
            if b.exists():
                need(sha(b) == digest, f"isolated copy changed: {b}")
            else:
                shutil.copyfile(a, b)
                need(sha(b) == digest, f"source copy failed: {b}")
    generated = (
        (PROBE / "Assets/Editor/ExportRokCityGammaFamilyStateProbe.cs", EXPORTER.read_bytes()),
        (PROBE / "Assets/ProbeDependencyStubs.cs", STUB.encode("utf-8")),
        (PROBE / "Assets/ProbeMipReadback.shader", MIP_SHADER.encode("utf-8")),
        (PROBE / "Packages/manifest.json", b'{"dependencies":{"com.unity.modules.imageconversion":"1.0.0","com.unity.modules.jsonserialize":"1.0.0","com.unity.modules.ui":"1.0.0"}}\n'),
        (PROBE / "ProjectSettings/ProjectVersion.txt", b"m_EditorVersion: 2019.4.28f1c1\n"),
    )
    for dst, content in generated:
        dst.parent.mkdir(parents=True, exist_ok=True)
        if dst.exists():
            need(dst.read_bytes() == content or not has_runs, f"generated isolated file differs after run: {dst}")
        if not dst.exists() or dst.read_bytes() != content:
            dst.write_bytes(content)
    print(f"PREPARED {PROBE}: five rows, {len(c['source_files'])} original source assets/scripts and metadata; Unity not started")


def verify(run: Path) -> dict:
    run = run.resolve()
    need(run.is_dir() and run.parent == (PROBE / "OutputRuns").resolve(), "run outside isolated probe")
    c = contract()
    contract_path = PROBE / "state_contract.json"
    need(read(contract_path) == c and
         sha(PROBE / "Assets/ProbeMipReadback.shader") == c["mip_shader_sha256"] and
         sha(PROBE / "Assets/Editor/ExportRokCityGammaFamilyStateProbe.cs") == c["exporter_sha256"],
         "source/state contract or GPU shader drift")
    report = read(run / "native_state.json")
    need(report.get("schema") == NATIVE_SCHEMA and report.get("complete") is True and
         report.get("source_contract_sha256") == sha(contract_path) and
         report.get("exporter_sha256") == c["exporter_sha256"], "native identity/incomplete")
    need(report.get("unity_version") == "2019.4.28f1c1" and report.get("color_space") == "Gamma" and
         report.get("game_app_loaded") is False and report.get("scope") == "isolated_controlled_gpu_state_only",
         "native execution scope differs")
    rows = report.get("rows", [])
    need([row.get("source_key") for row in rows] == list(KEYS), "five native family rows differ")
    source_rows = {r["source_key"]: r for r in read(BASE_NATIVE)["rows"]}
    mip_files = 0
    mip0_reference_pixels = 0
    raw_png_row_orders: set[str] = set()
    frame_files = 0
    unique_mip_files: set[str] = set()
    classifications = {}
    for row in rows:
        key = row["source_key"]
        source = source_rows[key]
        need(row.get("source_material_queue") == source["source_material_queue"] and
             row.get("renderer_file_id_decimal") == source["renderer_file_id_decimal"] and
             row.get("source_material_guid") == source["source_material_guid"] and
             row.get("source_mesh_file_id_decimal") == source["mesh"]["local_file_id_decimal"],
             f"source renderer/material/mesh identity differs: {key}")
        need(row.get("source_vertex_count") == len(source["mesh"]["vertices"]) and
             row.get("source_triangle_count") == sum(len(s["indices"]) for s in source["mesh"]["submeshes"]) // 3 and
             row.get("source_vertex_color_count") == len(source["mesh"]["colors"]),
             f"native geometry/count differs: {key}")
        texture_names = {t["source_asset_path"] for t in source["textures"] if t.get("gpu_png") and t.get("source_asset_path")}
        mip_textures = row.get("textures", [])
        need({t["source_asset_path"] for t in mip_textures} == texture_names,
             f"imported source texture set differs: {key}")
        for texture in mip_textures:
            match = next(t for t in source["textures"] if t.get("source_asset_path") == texture["source_asset_path"] and t.get("gpu_png"))
            need(texture["width"] == match["width"] and texture["height"] == match["height"] and
                 texture["mip_count"] == match["mip_count"] and texture["filter"] == match["filter"] and
                 texture["wrap"] == match["wrap"] and texture["srgb"] == match["srgb"],
                 f"source importer/texture dimensions drift: {key}")
            levels = texture.get("levels", [])
            need(len(levels) == texture["mip_count"], f"mip-chain incomplete: {key}")
            for index, level in enumerate(levels):
                w, h = max(1, texture["width"] >> index), max(1, texture["height"] >> index)
                blob = run / level["file"]
                png = run / level["png"]
                need(blob.is_file() and blob.name == level["file"] and sha(blob) == level["sha256"] and
                     level["level"] == index and level["width"] == w and level["height"] == h and
                     level["byte_count"] == w * h * 4 and blob.stat().st_size == level["byte_count"] and
                     level["readback_format"] == "RGBA32", f"mip bytes/layout differ: {key} mip{index}")
                need(png.is_file() and png.name == level["png"] and sha(png) == level["png_sha256"] and
                     level["gpu_source_asset_path"] == texture["source_asset_path"] and
                     level["gpu_source_guid"] == texture["source_guid"] and
                     level["gpu_sample_method"] == "Graphics.Blit plus explicit tex2Dlod mip into ARGB32 RT; Texture2D.ReadPixels RGBA32" and
                     "pixel centers" in level["pixel_center_uv_scope"] and
                     "Gamma" in level["transfer_scope"], f"GPU mip provenance differs: {key} mip{index}")
                with Image.open(png) as image:
                    need(image.mode == "RGBA" and image.size == (w, h), f"GPU mip PNG layout differs: {key} mip{index}")
                    raw = blob.read_bytes()
                    top_down = image.tobytes()
                    bottom_up = b"".join(top_down[y * w * 4:(y + 1) * w * 4] for y in range(h - 1, -1, -1))
                    row_order = "top_down" if raw == top_down else "bottom_up" if raw == bottom_up else "unmatched"
                    need(row_order != "unmatched", f"GPU raw bytes/PNG pixels differ: {key} mip{index}")
                    raw_png_row_orders.add(row_order)
                    if index == 0:
                        reference = BASE_NATIVE.parent / match["gpu_png"]
                        need(reference.is_file() and sha(reference) == match["gpu_png_sha256"],
                             f"sealed source mip0 GPU PNG differs: {key}")
                        with Image.open(reference) as original:
                            need(original.mode == "RGBA" and original.size == image.size and
                                 original.tobytes() == top_down,
                                 f"explicit-LOD GPU mip0 differs from sealed source GPU PNG: {key}/{texture['source_asset_path']}")
                        mip0_reference_pixels += w * h
                mip_files += 1
                unique_mip_files.add(level["file"])
        frames = row.get("frames", [])
        need(len(frames) >= 8 and len({f["name"] for f in frames}) == len(frames), f"GPU control frames incomplete: {key}")
        pixels = {}
        for frame in frames:
            image_path = run / frame["png"]
            need(image_path.is_file() and image_path.name == frame["png"] and sha(image_path) == frame["sha256"],
                 f"control PNG hash differs: {key}/{frame['name']}")
            with Image.open(image_path) as image:
                need(image.size == (128, 128) and image.mode == "RGBA", f"control PNG dimensions/mode differ: {key}")
                sample = list(image.getpixel((64, 64)))
            need(sample == frame["center_rgba8"], f"reported GPU sample differs: {key}/{frame['name']}")
            pixels[frame["name"]] = sample
            frame_files += 1
        need(all(name in pixels for name in ("background_a", "background_b", "family_a", "family_b",
                                             "front_only", "front_occluder", "late_only", "late_behind")),
             f"required GPU control frame missing: {key}")
        def rgb_delta(a: list[int], b: list[int]) -> int:
            return max(abs(a[i] - b[i]) for i in range(3))
        need(rgb_delta(pixels["background_a"], pixels["background_b"]) >= 50,
             f"background controls not distinguishable: {key}")
        need(rgb_delta(pixels["family_a"], pixels["background_a"]) >= 3,
             f"family material had no measured color effect: {key}")
        need(rgb_delta(pixels["front_only"], pixels["background_a"]) >= 3 and
             rgb_delta(pixels["late_only"], pixels["background_a"]) >= 3 and
             rgb_delta(pixels["front_occluder"], pixels["front_only"]) <= 2,
             f"front depth control did not protect known foreground: {key}")
        background_dependency = rgb_delta(pixels["family_a"], pixels["family_b"])
        late_vs_only = rgb_delta(pixels["late_behind"], pixels["late_only"])
        late_vs_family = rgb_delta(pixels["late_behind"], pixels["family_a"])
        observed_blend = ("overwrite_background_independent" if background_dependency <= 2 else
                          "background_dependent_blend" if background_dependency >= 20 else "ambiguous")
        observed_zwrite = ("no_depth_write_at_center" if late_vs_only <= 2 else
                           "depth_written_at_center" if late_vs_family <= 2 else "ambiguous")
        if key == "ground:ground_1":
            need(observed_blend == "overwrite_background_independent", "ground_1 GPU no-blend overwrite not observed")
        elif key != "support:boundary_support_wall_shadow":
            need(observed_blend == "background_dependent_blend", f"custom shader source-alpha blend not observed: {key}")
        if key != "support:boundary_support_wall_shadow":
            need(observed_zwrite == "no_depth_write_at_center", f"custom shader ZWrite Off not observed: {key}")
        classifications[key] = {
            "background_to_family_max_rgb8": rgb_delta(pixels["family_a"], pixels["background_a"]),
            "background_dependency_max_rgb8": background_dependency,
            "late_behind_to_late_only_max_rgb8": late_vs_only,
            "late_behind_to_family_max_rgb8": late_vs_family,
            "front_occluder_to_front_only_max_rgb8": rgb_delta(pixels["front_occluder"], pixels["front_only"]),
            "observed_blend_class_at_control_pixel": observed_blend,
            "observed_zwrite_class_at_control_pixel": observed_zwrite,
        }
        if key == "support:boundary_support_road":
            need(row.get("source_material_mask_unbound") is True,
                 "road source material mask unexpectedly bound")
            need("road_explicit_white" in pixels and rgb_delta(pixels["family_a"], pixels["road_explicit_white"]) <= 1,
                 "road default _Mask is not proven white at control pixel")
        if key == "initial:5":
            roles = {item["role"]: item["probe_path"] for item in c["rows"][0]["roles"]}
            need(row.get("source_mask_sprite_bound") is True and row.get("source_mask_property_block_matches") is True and
                 row.get("source_material_mask_unbound") is True and
                 row.get("source_mask_sprite_asset_path") == roles["raw_mask"] and "night_mask_b_zero" in pixels,
                 "source MaskSprite/MPB or mask-B control missing")
            need(row.get("night_light_time_formula") == "clamp(abs(sin(_Time.y*0.5))+0.5,0,1)" and
                 row.get("night_light_multiplier_min") == 0.5 and row.get("night_light_multiplier_max") == 1.0 and
                 row.get("actual_shader_time_observed") is False,
                 "source night time bounds or observed-time provenance differs")
            need(rgb_delta(pixels["family_a"], pixels["night_mask_b_zero"]) >= 2,
                 "night mask-B GPU contribution unobserved")
        if key == "ground:ground_2":
            need(row.get("source_vertex_alpha_min") == 0 and row.get("source_vertex_alpha_max") == 255,
                 "ground_2 native vertex alpha lost")
    output = {"schema": "rok.city_gamma_family_state_native_verify.v2", "passed": True,
              "run": str(run), "source_contract_sha256": sha(contract_path), "native_sha256": sha(run / "native_state.json"),
              "frame_pngs_checked": frame_files, "mip_references_checked": mip_files,
              "unique_mip_rgba8_files_checked": len(unique_mip_files),
              "mip0_source_reference_pixels_exact": mip0_reference_pixels,
              "raw_to_png_row_orders_observed": sorted(raw_png_row_orders),
              "control_pixel_differences": classifications,
              "scope": "isolated GPU material control and explicit-LOD shader-expanded mip readback; not GameApp runtime acceptance",
              "unknowns": ["GameApp globals/property blocks", "actual live sorting", "actual shader _Time.y", "full RGBA main-view acceptance"]}
    destination = run / "native_verify.json"
    need(not destination.exists(), f"verification result exists: {destination}")
    destination.write_text(json.dumps(output, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"PASS five controlled GPU family rows; {frame_files} frames, {mip_files} mip files: {run}")
    return output


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--prepare", action="store_true")
    group.add_argument("--run", type=Path)
    args = ap.parse_args()
    if args.prepare:
        prepare()
    else:
        verify(args.run)
