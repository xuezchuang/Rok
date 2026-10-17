"""Strict first-run Academy SpriteMesh/source-camera comparison.

The budgets below were fixed before any UE Academy presentation runtime capture.
The run is an isolated transient preview, never evidence of a constructed Academy.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
RUN_ROOT = ROOT / "Saved/RokUnityMigration/AcademyPresentationAudit"
NATIVE_ROOT = ROOT / "Saved/UnityAcademyVisualProbe/OutputRuns/20261008_153501_742_16460"
NATIVE_REPORT = NATIVE_ROOT / "native_render.json"
NATIVE_VERIFY = NATIVE_ROOT / "native_verify.json"
CATALOG = ROOT / "Saved/RokUnityMigration/AcademyVisualSource/catalog.json"
MESH_MANIFEST = ROOT / "Saved/RokUnityMigration/AcademyPresentationMesh/20261008_153501_742_16460/source_mesh_manifest.json"
MESH_IMPORT = ROOT / "Saved/RokUnityMigration/AcademyPresentationMesh/ue_mesh_create.json"
PINNED = {
    "native_render": "b00ac7d63d572c71f93ac30e0e446e88ef28a4c1e6af5df79e888d8540fda2e8",
    "native_verify": "9282c1e787cfa671218cb649ffb464f1c5f8a0019170fdef48feca4b5221bcca",
    "catalog": "ce5569c8d92584710359fafb31652b4e3864eb114cf0668984c9776942d5ef5b",
    "mesh_manifest": "d3f812c61a47f021b0209d8066df8cc1b7525bd5b0ddf35881d187c85e5771f0",
}
CAPTURE_INPUTS = (
    "Source/Rok/RokAcademyPresentationAuditSubsystem.h",
    "Source/Rok/RokAcademyPresentationAuditSubsystem.cpp",
    "Source/Rok/RokCitySpriteActor.h",
    "Source/Rok/RokCitySpriteActor.cpp",
    "Scripts/VerifyRokAcademyPresentationRuntime.py",
    "Scripts/PrepareRokAcademySourceMesh.py",
    "Scripts/CreateRokAcademyPresentationMesh.py",
)

# Immutable pre-capture acceptance budget. Do not tune after observing a UE run.
BUDGET = {
    "uv_absolute": 1e-5,
    "local_vertex_cm": .05,
    "projection_median_px": 1.0,
    "projection_max_px": 2.0,
    "bbox_max_edge_px": 2,
    "mask_iou_min": .98,
    "opaque_core_mae_rgb8": 8.0,
    "opaque_core_p95_rgb8": 16.0,
    "semi_analytic_mae_rgb8": 12.0,
    "semi_analytic_p95_rgb8": 24.0,
    "background_max_rgb8": 2,
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def image(path: Path) -> np.ndarray:
    with Image.open(path) as im:
        im.load()
        require(im.size == (1280, 720), f"Wrong image size: {path}")
        return np.asarray(im.convert("RGB"), dtype=np.uint8)


def bbox(mask: np.ndarray) -> list[int]:
    y, x = np.nonzero(mask)
    require(len(x) > 100, "Academy foreground is absent")
    return [int(x.min()), int(y.min()), int(x.max()), int(y.max())]


def erode3(mask: np.ndarray) -> np.ndarray:
    padded = np.pad(mask, 1)
    result = np.ones_like(mask)
    for dy in range(3):
        for dx in range(3):
            result &= padded[dy:dy + mask.shape[0], dx:dx + mask.shape[1]]
    return result


def stats(a: np.ndarray, b: np.ndarray, selected: np.ndarray) -> dict:
    values = np.abs(a.astype(np.float64) - b.astype(np.float64))[selected]
    require(values.size >= 100, "Too few Academy samples for color comparison")
    return {"rgb_samples": int(values.size), "pixels": int(np.count_nonzero(selected)),
            "mae": float(values.mean()), "p95": float(np.percentile(values, 95)),
            "max": float(values.max())}


def bilinear(texture: np.ndarray, u: np.ndarray, v: np.ndarray) -> np.ndarray:
    """Unity bilinear clamp sampling; source UV origin is bottom-left."""
    h, w, _ = texture.shape
    x = np.clip(u * w - .5, 0, w - 1)
    y = np.clip((1 - v) * h - .5, 0, h - 1)
    x0 = np.floor(x).astype(np.int32)
    y0 = np.floor(y).astype(np.int32)
    x1 = np.minimum(x0 + 1, w - 1)
    y1 = np.minimum(y0 + 1, h - 1)
    wx = (x - x0)[..., None]
    wy = (y - y0)[..., None]
    return ((texture[y0, x0] * (1 - wx) + texture[y0, x1] * wx) * (1 - wy)
            + (texture[y1, x0] * (1 - wx) + texture[y1, x1] * wx) * wy)


def analytic_source(native: dict, frame: dict, catalog: dict, native_pixels: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Rasterize the pinned source triangles and R/G alpha formula in Gamma RGB."""
    g = native["geometry"]
    xy = np.asarray([[v["x"], 720 - v["y"]] for v in g["screenVertices"]], dtype=np.float64)
    uv = np.asarray([[v["x"], v["y"]] for v in g["uvs"]], dtype=np.float64)
    indices = np.asarray(g["triangles"], dtype=np.int32).reshape(-1, 3)
    with Image.open(ROOT / catalog["sprite"]["path"]) as im:
        sprite = np.asarray(im.convert("RGBA"), dtype=np.float64) / 255.0
    with Image.open(NATIVE_ROOT / native["maskExport"]["png"]) as im:
        mask_texture = np.asarray(im.convert("RGBA"), dtype=np.float64) / 255.0
    source_color = np.array([frame["spriteColor"][c] for c in "rgb"], dtype=np.float64)
    renderer = np.array([native["rendererColor"][c] for c in "rgb"], dtype=np.float64)
    color1 = np.array([native["materialColor1"][c] for c in "rgb"], dtype=np.float64)
    color2 = np.array([native["materialColor2"][c] for c in "rgb"], dtype=np.float64)
    background = native_pixels[0, 0].astype(np.float64) / 255.0
    output = np.broadcast_to(background, (720, 1280, 3)).copy()
    opacity = np.zeros((720, 1280), dtype=np.float64)
    for face in indices:
        a, b, c = xy[face]
        minx = max(0, int(np.floor(min(a[0], b[0], c[0]) - 1)))
        maxx = min(1279, int(np.ceil(max(a[0], b[0], c[0]) + 1)))
        miny = max(0, int(np.floor(min(a[1], b[1], c[1]) - 1)))
        maxy = min(719, int(np.ceil(max(a[1], b[1], c[1]) + 1)))
        if minx > maxx or miny > maxy:
            continue
        xx, yy = np.meshgrid(np.arange(minx, maxx + 1) + .5,
                             np.arange(miny, maxy + 1) + .5)
        den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        require(abs(den) > 1e-8, "Degenerate source SpriteMesh triangle")
        wa = ((b[1] - c[1]) * (xx - c[0]) + (c[0] - b[0]) * (yy - c[1])) / den
        wb = ((c[1] - a[1]) * (xx - c[0]) + (a[0] - c[0]) * (yy - c[1])) / den
        wc = 1 - wa - wb
        inside = (wa >= -1e-8) & (wb >= -1e-8) & (wc >= -1e-8)
        if not np.any(inside):
            continue
        mapped_uv = wa[..., None] * uv[face[0]] + wb[..., None] * uv[face[1]] + wc[..., None] * uv[face[2]]
        main = bilinear(sprite, mapped_uv[..., 0], mapped_uv[..., 1])
        mask = bilinear(mask_texture, mapped_uv[..., 0], mapped_uv[..., 1])
        red2 = mask[..., 0] * 2
        team1 = np.clip(red2 - 1, 0, 1)
        gamma_rgb = main[..., :3] * renderer * (
            (1 - team1 - mask[..., 1])[..., None] +
            2 * color1 * team1[..., None] + 2 * color2 * mask[..., 1, None]) * source_color
        alpha = main[..., 3] * np.clip(red2, 0, 1) * frame["globalCityTransparencyAtRender"]
        blend = gamma_rgb * alpha[..., None] + background * (1 - alpha[..., None])
        segment = output[miny:maxy + 1, minx:maxx + 1]
        segment[inside] = blend[inside]
        opacity[miny:maxy + 1, minx:maxx + 1][inside] = alpha[inside]
    return np.clip(output * 255, 0, 255), opacity


def verify_geometry(manifest: dict, native: dict) -> dict:
    source = native["geometry"]
    measured = manifest["mesh"]
    require(measured["vertex_count"] == 49 and measured["triangle_count"] == 47
            and measured["uv_channel_count"] == 1, "LOD0 source SpriteMesh count mismatch")
    require(len(measured["vertices"]) == 49 and len(measured["triangles"]) == 47,
            "Missing actual imported LOD0 geometry")
    expected_pos = np.asarray([[-v["x"] * 100, 0, v["y"] * 100] for v in source["vertices"]])
    actual_pos = np.asarray([v["local_cm"] for v in measured["vertices"]])
    actual_uv = np.asarray([v["uv"] for v in measured["vertices"]])
    source_uv = np.asarray([[v["x"], v["y"]] for v in source["uvs"]])
    distance = np.linalg.norm(actual_pos[:, None, :] - expected_pos[None, :, :], axis=2)
    nearest = np.argmin(distance, axis=1)
    require(len(set(map(int, nearest))) == 49 and float(distance[np.arange(49), nearest].max()) <= BUDGET["local_vertex_cm"],
            "Imported Academy local vertices differ from 49 source positions")
    uv_direct = np.max(np.abs(actual_uv - source_uv[nearest]))
    uv_flipped = np.max(np.abs(actual_uv - np.column_stack((source_uv[nearest, 0], 1 - source_uv[nearest, 1]))))
    require(min(uv_direct, uv_flipped) <= BUDGET["uv_absolute"],
            f"Imported Academy UV values differ exactly from source/OBJ importer V convention: direct={uv_direct}, flipped={uv_flipped}")
    orientation = "direct_unity_uv" if uv_direct <= uv_flipped else "importer_v_flip"
    triangles = {tuple(sorted(int(nearest[i]) for i in face)) for face in measured["triangles"]}
    source_faces = {tuple(sorted(face)) for face in np.asarray(source["triangles"]).reshape(-1, 3)}
    require(len(triangles) == 47 and triangles == source_faces, "Imported Academy triangle topology differs from source")
    source_screen = np.asarray([[v["x"], 720 - v["y"]] for v in source["screenVertices"]])
    projections = np.asarray(manifest["frames"][0]["projected_mesh_vertices_xy_top_left_depth_cm"])
    require(projections.shape == (49, 3), "Projected Academy LOD0 vertex array missing")
    error = np.linalg.norm(projections[:, :2] - source_screen[nearest], axis=1)
    median, maximum = float(np.median(error)), float(np.max(error))
    require(median <= BUDGET["projection_median_px"] and maximum <= BUDGET["projection_max_px"],
            f"Academy source camera projection exceeds locked 1/2px budget: median={median:.4f}, max={maximum:.4f}")
    return {"uv_orientation": orientation, "uv_direct_max": float(uv_direct),
            "uv_flipped_max": float(uv_flipped), "local_vertex_max_cm": float(distance[np.arange(49), nearest].max()),
            "triangle_sets_equal": True, "projection_median_px": median, "projection_max_px": maximum}


def verify(run: Path) -> dict:
    run = run.resolve()
    require(run.parent == RUN_ROOT.resolve() and run.is_dir(), "Run must be a direct AcademyPresentationAudit child")
    manifest_path = run / "manifest.json"
    manifest = read(manifest_path)
    require(manifest["schema"] == "rok.academy_presentation_runtime.v1" and
            manifest["complete"] and manifest["passed"] and manifest["temporary_state_restored"],
            "UE Academy capture incomplete or temporary state not restored")
    require(Path(manifest["project"]).resolve() == ROOT and
            manifest["map"] == "/Game/Maps/L_RokUnityGameAppCity", "Wrong actual GameApp project/map")
    require(not manifest["map_saved"] and not manifest["construction_ledger_touched"] and
            not manifest["source_gameapp_context_accepted"],
            "Diagnostic incorrectly claims a placed/constructed Academy")
    require(manifest["fixed_pre_capture_budget"] == BUDGET,
            "Runtime capture did not pin the exact pre-capture visual budget")
    require(manifest["economy_before"] and manifest["economy_after"] == manifest["economy_before"],
            "Academy diagnostic changed resources or construction/upgrade queues")
    for key, path in (("native_render", NATIVE_REPORT), ("native_verify", NATIVE_VERIFY),
                      ("catalog", CATALOG), ("mesh_manifest", MESH_MANIFEST)):
        require(sha(path) == PINNED[key], f"Pinned Academy {key} changed")
    require(manifest["native_render_sha256"] == PINNED["native_render"] and
            manifest["native_verify_sha256"] == PINNED["native_verify"] and
            manifest["source_catalog_sha256"] == PINNED["catalog"] and
            manifest["mesh_manifest_sha256"] == PINNED["mesh_manifest"], "Runtime source hash chain differs")
    hashes = manifest.get("input_hashes")
    require(isinstance(hashes, dict) and set(hashes) == set(CAPTURE_INPUTS),
            "Capture did not lock the exact Academy audit, actor, verifier, and mesh scripts")
    for relative in CAPTURE_INPUTS:
        require(hashes[relative] == sha(ROOT / relative), f"Captured source changed: {relative}")
    source_catalog = read(CATALOG)
    for relative, expected in source_catalog["sources"].items():
        require((relative.startswith("ref/") or relative == "Saved/RokUnityMigration/CityConstructionSources/catalog.json")
                and ".." not in Path(relative).parts and
                sha(ROOT / relative) == expected, f"Source changed: {relative}")
    native = read(NATIVE_REPORT)
    native_verified = read(NATIVE_VERIFY)
    source_mesh = read(MESH_MANIFEST)
    imported = read(MESH_IMPORT)
    require(native_verified["passed"] and native_verified["native_report_sha256"] == PINNED["native_render"] and
            source_mesh["native_render_sha256"] == PINNED["native_render"] and
            imported["source_mesh_manifest_sha256"] == PINNED["mesh_manifest"] and
            imported["topology"]["vertex_count_lod0"] == 49 and
            imported["topology"]["triangle_count_lod0"] == 47,
            "Native/mesh import verification chain broken")
    mesh_asset = ROOT / "Content/RokUnity/City/Academy/build_13_101_2/SM_build_13_101_2_SourceSpriteMesh.uasset"
    material_asset = ROOT / "Content/RokUnity/City/Academy/build_13_101_2/M_build_13_101_2_SourceRG.uasset"
    background_asset = ROOT / "Content/RokDiagnostics/Academy/M_AcademyAuditBackground.uasset"
    require(manifest["mesh_asset_sha256"] == sha(mesh_asset) and
            manifest["material_asset_sha256"] == sha(material_asset) and
            manifest["background_material_asset_sha256"] == sha(background_asset),
            "Captured Academy mesh, sprite material, or GPU background material bytes changed")
    require(manifest["temporary_actor_count_before"] == 0 and
            manifest["temporary_actor_count_after_restore"] == 0 and
            len(manifest["frames"]) == 2 and [f["name"] for f in manifest["frames"]] == ["neutral", "day"],
            "Temporary actor lifecycle or two-frame control changed")
    geometry = verify_geometry(manifest, native)
    summaries = {}
    for source_frame, frame in zip(native["frames"][:2], manifest["frames"]):
        ue_path = Path(frame["png"])
        raw_path = Path(frame["raw_bgra8"])
        require(ue_path.parent.resolve() == run and raw_path.parent.resolve() == run and
                sha(ue_path) == frame["png_sha256"] and sha(raw_path) == frame["raw_sha256"] and
                frame["extent"] == [1280, 720] and frame["temporary_actor_count_at_capture"] == 1,
                "UE RHI/PNG provenance or temporary actor count differs")
        require("ReadPixels RHI" in frame["capture_api"], "Frame lacks real UE RHI readback")
        require(frame["capture_source"] ==
                "SCS_FinalColorLDR with postprocess GammaOnly; no Tonemapper/AA/EyeAdaptation/MotionBlur" and
                frame["background_method"] ==
                "GPU unlit opaque plane, ShowOnly with Academy SpriteMesh; no CPU compositing" and
                frame["background_material"] ==
                "/Game/RokDiagnostics/Academy/M_AcademyAuditBackground.M_AcademyAuditBackground" and
                frame["show_only_component_count"] == 2 and
                frame["post_processing_enabled"] and not frame["tonemapper_enabled"] and
                frame["render_target_force_linear_gamma"] and
                not frame["render_target_srgb"] and
                abs(frame["render_target_display_gamma"] - 2.2) < .001 and
                frame["source_mesh_registered_visible"] and
                frame["background_plane_registered_visible"] and
                np.allclose(frame["background_plane_world_scale"], [40.0] * 3, atol=1e-4) and
                np.allclose(frame["background_plane_world_location_cm"],
                            np.asarray(frame["camera_forward_ue"]) * 2000.0, atol=.01),
                "Source-camera GPU background/GammaOnly configuration changed")
        ue = image(ue_path)
        raw = np.frombuffer(raw_path.read_bytes(), dtype=np.uint8)
        require(raw.size == 1280 * 720 * 4 and
                np.array_equal(raw.reshape(720, 1280, 4)[..., :3][..., ::-1], ue),
                "UE PNG differs from captured same-frame raw RHI BGRA8 bytes")
        require(frame["rhi_corner_bgra8"] == raw[:4].tolist(),
                "Recorded GPU corner sample differs from same-frame raw RHI bytes")
        source_clear = np.asarray([source_frame["clearColor"][key] for key in "rgba"])
        source_sprite = np.asarray([source_frame["spriteColor"][key] for key in "rgba"])
        require(np.allclose(frame["source_clear_color_gamma"], source_clear, atol=1e-6) and
                np.allclose(frame["source_sprite_color_gamma"], source_sprite, atol=1e-6),
                "UE isolated frame changed native clear color or sprite lighting")
        quantized_srgb = np.rint(source_clear[:3] * 255.0) / 255.0
        linear_clear = np.where(quantized_srgb <= .04045, quantized_srgb / 12.92,
                                ((quantized_srgb + .055) / 1.055) ** 2.4)
        require(np.allclose(frame["background_material_linear_color"][:3], linear_clear, atol=1e-4) and
                np.allclose(frame["capture_clear_color_linear"][:3], linear_clear, atol=1e-4),
                "GPU background plane parameter/target clear differs from quantized source sRGB")
        native_pixels = image(NATIVE_ROOT / source_frame["png"])
        source_bg = native_pixels[0, 0].astype(np.int16)
        ue_bg = ue[0, 0].astype(np.int16)
        bg_error = int(np.max(np.abs(source_bg - ue_bg)))
        require(bg_error <= BUDGET["background_max_rgb8"],
                f"Academy source/UE clear background differs by {bg_error} RGB8")
        native_mask = np.max(np.abs(native_pixels.astype(np.int16) - source_bg), axis=2) > 4
        ue_mask = np.max(np.abs(ue.astype(np.int16) - ue_bg), axis=2) > 4
        native_box, ue_box = bbox(native_mask), bbox(ue_mask)
        edge_error = max(abs(a - b) for a, b in zip(native_box, ue_box))
        union = np.count_nonzero(native_mask | ue_mask)
        iou = float(np.count_nonzero(native_mask & ue_mask) / union)
        require(edge_error <= BUDGET["bbox_max_edge_px"] and iou >= BUDGET["mask_iou_min"],
                f"Academy silhouette exceeds locked bbox/IoU budget: edge={edge_error}px, IoU={iou:.6f}")
        core = erode3(erode3(native_mask & ue_mask))
        predicted, opacity = analytic_source(native, source_frame, source_catalog, native_pixels)
        opaque = core & (opacity >= .98)
        semi = core & (opacity >= .05) & (opacity <= .95)
        opaque_stat = stats(ue, native_pixels, opaque)
        semi_stat = stats(ue.astype(np.float64), predicted, semi)
        native_model_stat = stats(native_pixels.astype(np.float64), predicted, semi)
        require(opaque_stat["mae"] <= BUDGET["opaque_core_mae_rgb8"] and
                opaque_stat["p95"] <= BUDGET["opaque_core_p95_rgb8"],
                f"Academy opaque core color exceeds pre-locked budget: {opaque_stat}")
        require(semi_stat["mae"] <= BUDGET["semi_analytic_mae_rgb8"] and
                semi_stat["p95"] <= BUDGET["semi_analytic_p95_rgb8"] and
                native_model_stat["mae"] <= BUDGET["semi_analytic_mae_rgb8"] and
                native_model_stat["p95"] <= BUDGET["semi_analytic_p95_rgb8"],
                f"Academy semi-transparent analytical source blend exceeds pre-locked budget: ue={semi_stat}, native={native_model_stat}")
        summaries[frame["name"]] = {"source_bbox": native_box, "ue_bbox": ue_box,
                                    "bbox_max_edge_px": edge_error, "mask_iou": iou,
                                    "background_max_rgb8": bg_error, "opaque_core": opaque_stat,
                                    "semi_analytic_ue": semi_stat, "semi_analytic_native_calibration": native_model_stat,
                                    "native_png_sha256": sha(NATIVE_ROOT / source_frame["png"]),
                                    "ue_png_sha256": frame["png_sha256"]}
    context = manifest.get("gameapp_temporary_context")
    require(isinstance(context, dict) and
            manifest["source_gameapp_context_status"] == context.get("status") and
            context["status"] == "captured_temporary_source_age_one_grid" and
            context["source_camera_background_hidden_for_city"] and
            not context["constructed_building"] and not context["map_saved"],
            "Actual GameApp source-grid context was not captured as a temporary preview")
    require(context["age"] == 1 and context["model_key"] == 131011 and
            context["first_free_radius"] == 4 and context["footprint_origin"] == [-2, -7] and
            context["footprint_size"] == [6, 6] and context["source_cell_count"] == 36 and
            context["active_construction_count"] == 0,
            "Context grid is not the deterministic age-one first-free Academy footprint")
    local_x = .1 * -2 + .05 * 6 - .05
    local_z = .1 * -7 + .05 * 6 - .05
    expected_anchor = np.array([-(local_x - local_z) * 500 / np.sqrt(2),
                                (local_x + local_z) * 500 / np.sqrt(2), 5.0])
    require(np.max(np.abs(np.asarray(context["actor_world_anchor_cm"]) - expected_anchor)) <= 1e-4,
            "Age-one grid anchor differs from source city yaw/500UU conversion")
    map_asset = ROOT / "Content/Maps/L_RokUnityGameAppCity.umap"
    pose_dir = ROOT / "Saved/RokUnityMigration/BatchReview20261009"
    pose_path = pose_dir / "root_city_pose_crosscheck.json"
    readback_path = pose_dir / "current_city_pose_readback.json"
    pose = read(pose_path)
    require(context["map_sha256"] == sha(map_asset) ==
            "26609d28ca4f42f3c9d0d7cb735c88a71862d1df1fd00f362da6693111b0ddc2" and
            context["pose_crosscheck_sha256"] == sha(pose_path) and
            context["pose_readback_sha256"] == sha(readback_path) == pose["source_readback_sha256"] and
            pose["schema"] == "rok.current_city_anchor_source_grid_crosscheck.v1" and pose["passed"] and
            len(pose["actors"]) == 24 and max(a["error"] for a in pose["actors"]) < 1e-8,
            "Current GameApp source-pose/map evidence changed")
    require(context["construction_models_sha256"] == sha(ROOT / "Saved/RokUnityMigration/CityConstructionSources/catalog.json") and
            context["grid_catalog_sha256"] == sha(ROOT / "Saved/RokUnityMigration/CityConstructionGridSources/catalog.json") ==
                pose["source_grid_catalog_sha256"],
            "Context building model/grid source catalog changed")
    context_png = Path(context["png"])
    require(context_png.parent.resolve() == run and context_png.name == "02_gameapp_temporary_age1_grid.png" and
            sha(context_png) == context["png_sha256"], "GameApp diagnostic context screenshot missing or changed")
    image(context_png)
    return {"schema": "rok.academy_presentation_runtime_verify.v1", "passed": True,
            "scope": "Isolated transient Academy SpriteMesh/camera/color source comparison plus temporary age-one GameApp grid context; no constructed Academy or full-city pixel equivalence.",
            "capture_manifest_sha256": sha(manifest_path), "source_hashes": PINNED,
            "fixed_pre_capture_budget": BUDGET, "geometry": geometry, "frames": summaries,
            "context": {"status": context["status"], "grid_origin": context["footprint_origin"],
                        "actor_world_anchor_cm": context["actor_world_anchor_cm"],
                        "png_sha256": context["png_sha256"], "full_city_pixel_parity": "not claimed"}}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("run", type=Path)
    args = parser.parse_args()
    run = args.run.resolve()
    try:
        report = verify(run)
    except Exception as exc:
        report = {"schema": "rok.academy_presentation_runtime_verify.v1", "passed": False,
                  "fixed_pre_capture_budget": BUDGET, "error": f"{type(exc).__name__}: {exc}"}
        if run.parent == RUN_ROOT.resolve() and run.is_dir():
            (run / "runtime_verify.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        raise
    (run / "runtime_verify.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": True, "run": str(run), "geometry": report["geometry"],
                      "frames": report["frames"]}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
