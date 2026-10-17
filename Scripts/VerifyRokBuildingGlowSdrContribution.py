"""Numerically compare one real UE SDR glow frame with source UI/Add sampling.

This is a bounded phase=.75, one-background review. It reads actual game-window
PNGs; it does not render, replace assets, or infer masks from observed deltas.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image
from rok_data_paths import DATA_ROOT

ROOT = Path(__file__).resolve().parents[1]
AUDIT_ROOT = ROOT / "Saved/RokUnityMigration/BuildingPreviewGlowAudit"
SOURCE = DATA_ROOT / "BuildingPreviewGlow/glow_data.json"
TEXTURE = ROOT / "Saved/RokUnityMigration/BuildingGlowTexture/sampled_gpu_mip0.png"
TEXTURE_NATIVE = ROOT / "Saved/RokUnityMigration/BuildingGlowTexture/native.json"
TEXTURE_VERIFY = ROOT / "Saved/RokUnityMigration/BuildingGlowTexture/verify.json"
FRAME = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_frame.png"
PREVIEW_LAYOUT = DATA_ROOT / "BuildingSourcePreview/source_preview.json"
MODAL_SOURCE = ROOT / "Source/Rok/RokUnityModalWidget.cpp"
PIXEL_NAMES = ("05_diagnostic_none", "06_diagnostic_layer0", "07_diagnostic_layer1", "08_diagnostic_both")
MATERIAL_SCRIPT = ROOT / "Scripts/CreateRokBuildingPreviewGlowMaterial.py"
WIDGET_SOURCE = ROOT / "Source/Rok/RokBuildingPreviewGlowWidget.cpp"
PARENT_SOURCE = ROOT / "Saved/UnityBarracksParentWindowProbe/Artifacts/native_20260925_111125_541/render_evidence.json"
NATIVE_V5_VERIFY = ROOT / "Saved/UnityBarracksParentWindowV5Probe/Artifacts/native_20260925_120725_828/verify_v5.json"
NATIVE_V5_EVIDENCE = ROOT / "Saved/UnityBarracksParentWindowV5Probe/Artifacts/native_20260925_120725_828/render_evidence.json"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_type9_parent_rects():
    evidence = json.loads(PARENT_SOURCE.read_text(encoding="utf-8-sig"))
    assert evidence["complete"] and not evidence["failure"] and evidence["nativeAnimatorEnabled"]
    nodes = {node["path"].split("/")[-1]: node for node in evidence["samples"][-1]["nodes"]}
    def rect(name, expected):
        item = nodes[name]["screenRect"]
        actual = (item["x"], item["y"], item["x"] + item["width"], item["y"] + item["height"])
        assert all(abs(a - b) < .03 for a, b in zip(actual, expected)), name
        return actual
    return rect("frame1", (194., 94.2, 737., 755.)), rect("frame3", (160., 63., 720., 123.))


def parent_opaque(body_rect):
    """Full four-tap opaque part of the Unity Type2BodyLeft nine-slice."""
    alpha = np.asarray(Image.open(FRAME).convert("RGBA"), dtype=np.uint8)[:, :, 3]
    yy, xx = np.indices((810, 1440))

    def mapped(v, target, source, first, last):
        return np.where(v < first, v,
                        np.where(v > target - last, source - (target - v),
                                 first + (v - first) * (source - first - last) / (target - first - last)))

    bx, by, br, bb = body_rect
    u = 1 + mapped(xx + .5 - bx, br - bx, 524, 493, 15) - .5
    v = (2048 - 822 - 659) + mapped(yy + .5 - by, bb - by, 659, 278, 291) - .5
    ix = np.clip(np.floor(u).astype(np.int32), 0, alpha.shape[1] - 2)
    iy = np.clip(np.floor(v).astype(np.int32), 0, alpha.shape[0] - 2)
    opaque = ((alpha[iy, ix] == 255) & (alpha[iy, ix + 1] == 255)
              & (alpha[iy + 1, ix] == 255) & (alpha[iy + 1, ix + 1] == 255))
    parent = (xx >= 178) & (xx < 678) & (yy >= 212) & (yy < 712)
    return opaque & parent


def source_info_rects():
    verified = json.loads(NATIVE_V5_VERIFY.read_text(encoding="utf-8-sig"))
    assert verified["passed"] and verified["inventory_count"] == 129
    assert verified["evidence_sha256"] == sha(NATIVE_V5_EVIDENCE)
    rows = {row["path"].split("/")[-1]: row for row in verified["focus_rows"]}
    button, glyph = rows["btn_BuildLevelInfo"], rows["img_polygonImage"]
    assert button["activeInHierarchy"] and glyph["activeInHierarchy"]
    assert glyph["path"] == button["path"] + "/img_polygonImage" and glyph["spriteName"] == "btn_com_1007"
    def xywh(row):
        rect = row["screenRect"]
        return [rect[key] for key in ("x", "y", "width", "height")]
    button_rect, glyph_rect = xywh(button), xywh(glyph)
    assert all(abs(a-b) < .03 for a,b in zip(button_rect, (211,227,60,60)))
    assert all(abs(a-b) < .03 for a,b in zip(glyph_rect, (222,238,38,38)))
    return button_rect, glyph_rect


def actual_rect(state, key, expected):
    item = state[key]
    xywh = [item["x"], item["y"], item["right"]-item["x"], item["bottom"]-item["y"]]
    assert all(abs(a-b) < 1 for a,b in zip(xywh, expected)), (key, xywh, expected)
    return xywh


def source_uncovered(header_bottom, layout_contract, state):
    """Exclude only known higher-Z source widgets, using pixel centers."""
    yy, xx = np.indices((810, 1440))
    x, y = xx + .5, yy + .5
    preview_layout = json.loads(PREVIEW_LAYOUT.read_text(encoding="utf-8-sig"))
    assert preview_layout["schema"] == "rok.building.source_preview.v1"
    px, py = preview_layout["layout"]["position"]
    pw, ph = preview_layout["layout"]["size"]
    assert abs(px - 248.8000335693) < .001 and py == 290.5
    assert abs(pw - 358.3999786377) < .001 and ph == 343
    if layout_contract == "type9_parent_v3_info_button":
        button_rect, glyph_rect = source_info_rects()
        modal = MODAL_SOURCE.read_text(encoding="utf-8-sig")
        assert 'MakeAtlasButton(TEXT("BarracksLevelInfoButton")' in modal
        assert 'TEXT("%sImage"), *Name.ToString()' in modal and 'Button->SetContent(MakeAtlasImage(' in modal
        assert 'PlaceOnCanvas(UpgradeContentCanvas, Glow, FVector2D::ZeroVector, FVector2D(1440, 810), 1)' in modal
        assert 'PlaceOnCanvas(UpgradeContentCanvas, SourcePreview, SourcePreview->GetSourcePosition(), SourcePreview->GetSourceSize(), 2)' in modal
        assert 'PlaceOnCanvas(UpgradeContentCanvas, Info, FVector2D(211, 227), FVector2D(60), 5)' in modal
        actual_rect(state, "source_preview_pixel_rect", (px,py,pw,ph))
        actual_rect(state, "info_button_pixel_rect", button_rect)
        actual_rect(state, "info_glyph_pixel_rect", glyph_rect)
        assert (state["glow_canvas_z"], state["source_preview_canvas_z"], state["info_button_canvas_z"]) == (1,2,5)
        assert state["info_glyph_is_button_child"]
        icon_rect = glyph_rect
    else:
        # Historical v1/v2 captures used the inert icon at this source-defined
        # rect. Rechecking them must not depend on the current modal source.
        button_rect = None
        icon_rect = [586, 223, 38, 38]
    ix, iy, iw, ih = icon_rect
    # The preview and visible icon render above the glow; the button's larger
    # transparent hit area is deliberately not removed from the numeric mask.
    preview = ((x >= px) & (x < px + pw) & (y >= py) & (y < py + ph))
    info = (x >= ix) & (x < ix + iw) & (y >= iy) & (y < iy + ih)
    return ~(preview | info | (y < header_bottom)), {"preview_rect": [px, py, pw, ph],
                                            "info_button_rect": button_rect,
                                            "info_icon_rect": icon_rect, "header_below_y": header_bottom,
                                            "preview_layout_sha256": sha(PREVIEW_LAYOUT),
                                            "modal_layout_sha256": sha(MODAL_SOURCE) if layout_contract == "type9_parent_v3_info_button" else None,
                                            "native_v5_sha256": sha(NATIVE_V5_VERIFY) if layout_contract == "type9_parent_v3_info_button" else None}


def projected_quad(state, layer):
    projection = state["projection_pixels"]
    origin = np.array([projection["origin_x"], projection["origin_y"]], dtype=np.float64)
    scale = np.array([(projection["right"] - origin[0]) / 1440,
                      (projection["bottom"] - origin[1]) / 810], dtype=np.float64)
    source = np.array([[p["x"], p["y"]] for p in layer["source_quad"]], dtype=np.float64)
    return origin + source * scale


def layer_sample(state, layer_index, source_layer, texture):
    """Pixel-center affine UV and clamp/bilinear texel sampling of one quad."""
    quad = projected_quad(state, state["layers"][layer_index])
    assert source_layer["triangles"] == [0, 1, 2, 2, 3, 0]
    assert np.max(np.abs(quad[2] - (quad[1] + quad[3] - quad[0]))) < .02
    lo = np.floor(quad.min(axis=0) - 1).astype(int)
    hi = np.ceil(quad.max(axis=0) + 1).astype(int)
    x0, y0 = np.maximum(lo, [0, 0])
    x1, y1 = np.minimum(hi, [1440, 810])
    yy, xx = np.mgrid[y0:y1, x0:x1]
    points = np.stack((xx + .5, yy + .5), axis=-1)
    basis = np.column_stack((quad[1] - quad[0], quad[3] - quad[0]))
    coords = (points - quad[0]) @ np.linalg.inv(basis).T
    s, t = coords[..., 0], coords[..., 1]
    inside = (s >= 0) & (s <= 1) & (t >= 0) & (t <= 1)
    native_uv = np.array(source_layer["uv"], dtype=np.float64)
    assert native_uv.shape == (4, 2)
    assert np.max(np.abs(native_uv[2] - (native_uv[1] + native_uv[3] - native_uv[0]))) < .001
    uv = native_uv[0] + s[..., None] * (native_uv[1] - native_uv[0]) + t[..., None] * (native_uv[3] - native_uv[0])
    uv[..., 1] = 1 - uv[..., 1]  # Unity bottom-left UV to saved PNG top-left, exactly once.
    height, width = texture.shape[:2]
    texel = uv * np.array([width, height]) - .5
    ix0 = np.floor(texel[..., 0]).astype(np.int32)
    iy0 = np.floor(texel[..., 1]).astype(np.int32)
    fx, fy = texel[..., 0] - ix0, texel[..., 1] - iy0
    ix1, iy1 = ix0 + 1, iy0 + 1
    ix0, ix1 = np.clip(ix0, 0, width - 1), np.clip(ix1, 0, width - 1)
    iy0, iy1 = np.clip(iy0, 0, height - 1), np.clip(iy1, 0, height - 1)
    sampled = (texture[iy0, ix0] * ((1 - fx) * (1 - fy))[..., None]
               + texture[iy0, ix1] * (fx * (1 - fy))[..., None]
               + texture[iy1, ix0] * ((1 - fx) * fy)[..., None]
               + texture[iy1, ix1] * (fx * fy)[..., None])
    srgb = texture[..., :3] / 255
    decoded = np.where(srgb <= .04045, srgb / 12.92, ((srgb + .055) / 1.055) ** 2.4) * 255
    decoded_sampled = (decoded[iy0, ix0] * ((1 - fx) * (1 - fy))[..., None]
                       + decoded[iy0, ix1] * (fx * (1 - fy))[..., None]
                       + decoded[iy1, ix0] * ((1 - fx) * fy)[..., None]
                       + decoded[iy1, ix1] * (fx * fy)[..., None])
    alpha = float(state["layers"][layer_index]["alpha"])
    vertex_alpha_byte = int(np.floor(np.clip(alpha, 0, 1) * 255 + .5))
    vertex_alpha = vertex_alpha_byte / 255
    # Source RGB is white-tinted; texture A and vertex A each multiply once.
    # The existing UI material supplies RGB as Color and (sampleA*vertexA)
    # as Opacity to one SrcAlpha/One additive pass. All terms stay in byte scale.
    predicted = sampled[..., :3] * (sampled[..., 3:4] / 255) * vertex_alpha
    unquantized = sampled[..., :3] * (sampled[..., 3:4] / 255) * alpha
    gamma_decoded = decoded_sampled * (sampled[..., 3:4] / 255) * vertex_alpha
    result = np.zeros((810, 1440, 3), dtype=np.float64)
    result[y0:y1, x0:x1] = predicted * inside[..., None]
    raw = np.zeros_like(result)
    raw[y0:y1, x0:x1] = unquantized * inside[..., None]
    gamma_result = np.zeros_like(result)
    gamma_result[y0:y1, x0:x1] = gamma_decoded * inside[..., None]
    quad_mask = np.zeros((810, 1440), dtype=bool)
    quad_mask[y0:y1, x0:x1] = inside
    return result, raw, gamma_result, quad_mask, {"source_alpha": alpha,
                                    "vertex_alpha_byte": vertex_alpha_byte,
                                    "vertex_alpha_unorm": vertex_alpha,
                                    "quad_pixels": int(quad_mask.sum()),
                                    "screen_quad": quad.tolist()}


def quad_edge(quad):
    """One-pixel inner border of the source quad, independent of color deltas."""
    padded = np.pad(quad, 1, constant_values=False)
    interior = quad.copy()
    for dy in range(3):
        for dx in range(3):
            interior &= padded[dy:dy + 810, dx:dx + 1440]
    return quad & ~interior


def metrics(observed, predicted, raw, gamma, base, mask, edge, vertex_alphas):
    # Keep saturation independent from opaque-parent and widget-occlusion gates.
    valid = mask[..., None] & (base + predicted < 254.5)
    valid = np.broadcast_to(valid, predicted.shape)
    predicted_clip = np.broadcast_to(mask[..., None], predicted.shape) & ~valid
    actual_final = base + observed
    clip_mismatch = predicted_clip & (actual_final != 255)
    unexpected_actual_clip = valid & (actual_final == 255)
    unexpected_clip_headroom = 255 - (base + predicted)[unexpected_actual_clip]
    signal = valid & (predicted >= .75)
    residual = observed - predicted
    quantized_final = np.floor(np.clip(base + predicted, 0, 255) + .5) - base
    sample = residual[signal]
    reference = predicted[signal]
    obs = observed[signal]
    absolute = np.abs(sample)
    positions = np.argwhere(signal)
    worst = []
    for item in np.argsort(absolute)[-5:][::-1]:
        y, x, channel = map(int, positions[item])
        worst.append({"x": x, "y": y, "channel": "RGB"[channel],
                      "observed": float(observed[y, x, channel]),
                      "predicted": float(predicted[y, x, channel]),
                      "absolute_error": float(absolute[item]),
                      "source_quad_edge_within_one_pixel": bool(edge[y, x])})
    double_alpha = predicted.copy()
    no_vertex_alpha = predicted.copy()
    for layer_prediction, vertex_alpha in vertex_alphas:
        double_alpha += layer_prediction * (vertex_alpha - 1)
        no_vertex_alpha += layer_prediction * (1 / vertex_alpha - 1)
    return {
        "opaque_uncovered_quad_pixels": int(mask.sum()),
        "unsaturated_channels": int(valid.sum()),
        "saturated_channels_excluded": int((np.broadcast_to(mask[..., None], predicted.shape) & ~valid).sum()),
        "predicted_final_unorm_255_channels": int(predicted_clip.sum()),
        "actual_255_of_predicted_clip_channels": int((predicted_clip & (actual_final == 255)).sum()),
        "predicted_clip_mismatch_channels": int(clip_mismatch.sum()),
        "unexpected_actual_255_unsaturated_channels": int(unexpected_actual_clip.sum()),
        "unexpected_actual_255_max_predicted_headroom": float(unexpected_clip_headroom.max()) if unexpected_clip_headroom.size else 0,
        "signal_channels_predicted_ge_0_75": int(signal.sum()),
        "mean_predicted_increment": float(reference.mean()) if reference.size else None,
        "mean_observed_increment": float(obs.mean()) if obs.size else None,
        "predicted_unrounded_mae": float(absolute.mean()) if sample.size else None,
        "predicted_unrounded_p95_abs": float(np.percentile(absolute, 95)) if sample.size else None,
        "predicted_unrounded_p99_abs": float(np.percentile(absolute, 99)) if sample.size else None,
        "predicted_unrounded_max_abs": float(absolute.max()) if sample.size else None,
        "residual_over_1_5_channels": int((absolute > 1.5).sum()),
        "residual_over_2_channels": int((absolute > 2).sum()),
        "worst_five_channels": worst,
        "predicted_unrounded_bias": float(sample.mean()) if sample.size else None,
        "predicted_unorm_final_mae": float(np.abs(observed[signal] - quantized_final[signal]).mean()) if sample.size else None,
        "unquantized_vertex_mae": float(np.abs(observed[signal] - raw[signal]).mean()) if sample.size else None,
        "double_vertex_alpha_mae": float(np.abs(observed[signal] - double_alpha[signal]).mean()) if sample.size else None,
        "no_vertex_alpha_mae": float(np.abs(observed[signal] - no_vertex_alpha[signal]).mean()) if sample.size else None,
        "srgb_decode_mae": float(np.abs(observed[signal] - gamma[signal]).mean()) if sample.size else None,
        "negative_observed_channels": int((obs < 0).sum()),
    }


def verify(directory, report_path=None):
    directory = directory.resolve()
    assert directory.parent == AUDIT_ROOT.resolve()
    manifest = directory / "manifest.json"
    run = json.loads(manifest.read_text(encoding="utf-8-sig"))
    layout_contract = run.get("parent_layout_contract", "legacy_type2_v1_unversioned")
    assert layout_contract in ("legacy_type2_v1_unversioned", "type9_parent_v2", "type9_parent_v3_info_button")
    body_rect, header_rect = ((177., 139.2, 720., 800.), (160., 108., 720., 168.)) if layout_contract == "legacy_type2_v1_unversioned" else source_type9_parent_rects()
    assert run["passed"] and run["complete"] and run["capture_includes_slate_ui"]
    assert run["world"] == "/Game/Maps/L_RokUnityGameAppCity"
    source = json.loads(SOURCE.read_text(encoding="utf-8-sig"))
    native = json.loads(TEXTURE_NATIVE.read_text(encoding="utf-8-sig"))
    verified = json.loads(TEXTURE_VERIFY.read_text(encoding="utf-8-sig"))
    assert verified["passed"] and verified["native_sha256"] == sha(TEXTURE_NATIVE)
    assert native["sampledPngSha256"] == sha(TEXTURE)
    assert source["schema"] == "rok.building.preview_glow.data.v1" and len(source["layers"]) == 2
    assert "ESlateDrawEffect::NoGamma" in WIDGET_SOURCE.read_text(encoding="utf-8-sig")
    assert 'unreal.BlendMode.BLEND_ADDITIVE' in MATERIAL_SCRIPT.read_text(encoding="utf-8-sig")
    texture = np.asarray(Image.open(TEXTURE).convert("RGBA"), dtype=np.float64)
    assert texture.shape == (256, 256, 4)
    captures = run["captures"][4:8]
    assert [Path(c["file"]).stem for c in captures] == list(PIXEL_NAMES)
    assert captures[0]["processed"]["elapsed"] > run["captures"][3]["processed"]["elapsed"]
    images = []
    for index, capture in enumerate(captures):
        path = Path(capture["file"]).resolve()
        assert path.parent == directory and sha(path) == capture["png_sha256"]
        assert capture["processed"]["playback_time"] == .75
        assert not capture["processed"]["auto_advance"]
        assert capture["processed"]["manual_clock"]
        assert capture["processed"]["audit_layer_mask"] == [0, 1, 2, 3][index]
        assert capture["processed"]["last_paint_layer_count"] == [0, 1, 1, 2][index]
        if layout_contract in ("type9_parent_v2", "type9_parent_v3_info_button"):
            for phase in ("request", "processed"):
                for key, expected in (("body_left_pixel_rect", body_rect), ("header_left_pixel_rect", header_rect)):
                    actual = capture[phase][key]
                    assert all(abs(actual[field] - value) < 1 for field, value in zip(("x", "y", "right", "bottom"), expected)), (index, phase, key)
                    assert abs(actual["local_width"] - (expected[2] - expected[0])) < 1
                    assert abs(actual["local_height"] - (expected[3] - expected[1])) < 1
                if layout_contract == "type9_parent_v3_info_button":
                    source_info_button, source_info_glyph = source_info_rects()
                    for key, expected in (("source_preview_pixel_rect", (248.8000335693, 290.5, 358.3999786377, 343)),
                                          ("info_button_pixel_rect", source_info_button), ("info_glyph_pixel_rect", source_info_glyph)):
                        actual_rect(capture[phase], key, expected)
                    assert (capture[phase]["glow_canvas_z"], capture[phase]["source_preview_canvas_z"], capture[phase]["info_button_canvas_z"]) == (1,2,5)
                    assert capture[phase]["info_glyph_is_button_child"]
        images.append(np.asarray(Image.open(path).convert("RGB"), dtype=np.float64))
    none, layer0, layer1, both = images
    state = captures[3]["processed"]
    parent = parent_opaque(body_rect)
    uncovered, exclusions = source_uncovered(header_rect[3], layout_contract, state)
    allowed = parent & uncovered
    predictions, raws, gammas, masks, quads, layers = [], [], [], [], [], []
    for index in range(2):
        prediction, raw, gamma, quad, detail = layer_sample(state, index, source["layers"][index], texture)
        predictions.append(prediction)
        raws.append(raw)
        gammas.append(gamma)
        masks.append(quad & allowed)
        quads.append(quad)
        detail["metrics"] = metrics(images[index + 1] - none, prediction, raw, gamma,
                                     none, masks[-1], quad_edge(quad),
                                     [(prediction, detail["vertex_alpha_unorm"])])
        layers.append(detail)
    combined = predictions[0] + predictions[1]
    combined_raw = raws[0] + raws[1]
    combined_gamma = gammas[0] + gammas[1]
    combined_mask = masks[0] | masks[1]
    both_metrics = metrics(both - none, combined, combined_raw, combined_gamma, none, combined_mask,
                           quad_edge(quads[0]) | quad_edge(quads[1]),
                           [(predictions[i], layers[i]["vertex_alpha_unorm"]) for i in range(2)])
    additive_channels = np.broadcast_to(combined_mask[..., None] & (none + combined < 254.5), both.shape)
    actual_sum_error = (both - none) - ((layer0 - none) + (layer1 - none))
    additive_abs = np.abs(actual_sum_error[additive_channels])
    actual_additivity = {
        "unsaturated_channels": int(additive_abs.size),
        "mean_absolute_integer_difference": float(additive_abs.mean()),
        "p95_absolute_integer_difference": float(np.percentile(additive_abs, 95)),
        "exact_integer_fraction": float((additive_abs == 0).mean()),
    }
    # Integer baseline/final PNG values each hide up to half an 8-bit unit;
    # hardware bilinear weights and quad coordinates add a small sub-unit
    # error. Budget 0.5 average, 1 at P95 and 1.5 worst per unsaturated
    # signal channel. This includes quad-border channels; no delta-based
    # exclusion or tint/gamma fit is allowed. Gate this capture only.
    gates = []
    for index, layer in enumerate(layers):
        m = layer["metrics"]
        gates += [
            (f"layer{index}_source_alpha_unorm", layer["vertex_alpha_byte"] == [19, 47][index]),
            (f"layer{index}_opaque_region", m["opaque_uncovered_quad_pixels"] > 1000),
            (f"layer{index}_signal", m["signal_channels_predicted_ge_0_75"] > 10000),
            (f"layer{index}_saturation", m["saturated_channels_excluded"] <= m["opaque_uncovered_quad_pixels"] * 3 * .01
             and m["predicted_clip_mismatch_channels"] == 0
             and m["unexpected_actual_255_max_predicted_headroom"] <= 1.5),
            (f"layer{index}_once_alpha", m["predicted_unrounded_mae"] <= .5
             and m["predicted_unrounded_p95_abs"] <= 1
             and m["predicted_unrounded_max_abs"] <= 1.5
             and abs(m["predicted_unrounded_bias"]) < .06
             and m["predicted_unorm_final_mae"] <= .5),
            (f"layer{index}_alternative_models_rejected",
             m["double_vertex_alpha_mae"] > 2 * m["predicted_unrounded_mae"]
             and m["no_vertex_alpha_mae"] > 2 * m["predicted_unrounded_mae"]
             and m["srgb_decode_mae"] > 2 * m["predicted_unrounded_mae"]),
            (f"layer{index}_stable_parent", m["negative_observed_channels"] == 0),
        ]
    gates += [
        ("both_opaque_region", both_metrics["opaque_uncovered_quad_pixels"] > 1000),
        ("both_signal", both_metrics["signal_channels_predicted_ge_0_75"] > 10000),
        ("both_saturation", both_metrics["saturated_channels_excluded"] <= both_metrics["opaque_uncovered_quad_pixels"] * 3 * .01
         and both_metrics["predicted_clip_mismatch_channels"] == 0
         and both_metrics["unexpected_actual_255_max_predicted_headroom"] <= 1.5),
        ("both_once_alpha", both_metrics["predicted_unrounded_mae"] <= .5
         and both_metrics["predicted_unrounded_p95_abs"] <= 1
         and both_metrics["predicted_unrounded_max_abs"] <= 1.5
         and abs(both_metrics["predicted_unrounded_bias"]) < .06
         and both_metrics["predicted_unorm_final_mae"] <= .5),
        ("both_stable_parent", both_metrics["negative_observed_channels"] == 0),
        ("two_layers_additive", actual_additivity["unsaturated_channels"] > 10000
         and actual_additivity["mean_absolute_integer_difference"] < .35
         and actual_additivity["p95_absolute_integer_difference"] <= 1),
    ]
    failed = [name for name, ok in gates if not ok]
    report = {
        "schema": "rok.building.glow_sdr_contribution_review.v1",
        "passed": not failed,
        "status": "passed_one_background_phase_0_75" if not failed else "failed_one_background_phase_0_75",
        "parent_layout_contract": layout_contract,
        "body_left_pixel_rect": body_rect,
        "parent_source_sha256": sha(PARENT_SOURCE) if layout_contract in ("type9_parent_v2", "type9_parent_v3_info_button") else None,
        "failed_gates": failed,
        "gates": {name: ok for name, ok in gates},
        "scope": "One actual 1440x810 game-window background at fixed source time .75; source-opaque, uncovered glow pixels only. No multiple-background or full-shader parity claim.",
        "run_directory": str(directory),
        "source_sha256": sha(SOURCE),
        "native_texture_sha256": sha(TEXTURE_NATIVE),
        "sampled_gpu_mip0_sha256": sha(TEXTURE),
        "source_parent_frame_sha256": sha(FRAME),
        "widget_source_sha256": sha(WIDGET_SOURCE),
        "material_script_sha256": sha(MATERIAL_SCRIPT),
        "capture_sha256": {PIXEL_NAMES[i]: captures[i]["png_sha256"] for i in range(4)},
        "exclusions": exclusions,
        "source_parent_opaque_pixels": int(parent.sum()),
        "source_parent_opaque_uncovered_pixels": int(allowed.sum()),
        "model": {"uv": "source bottom-left V flipped once to PNG top-left", "texture_filter": "clamp bilinear, texel position uv*256-0.5", "vertex_alpha": "floor(alpha*255+0.5)/255", "blend": "textureRGB*(textureA/255)*vertexAlpha, SrcAlpha/One, inherited opacity 1 after parent entry", "pixel_sample": "integer pixel centers x+0.5,y+0.5", "saturation": "per RGB channel baseline+predicted<254.5; predicted clip must actually reach 255; unexpected actual 255 must be within 1.5 of threshold", "residual_budget": "unsaturated predicted>=0.75 channels: MAE<=0.5 byte, P95<=1 byte, max<=1.5 bytes including source quad edge"},
        "layers": layers,
        "both_metrics": both_metrics,
        "actual_two_layer_additivity": actual_additivity,
    }
    output = report_path.resolve() if report_path else directory / "glow_sdr_contribution_review.json"
    assert output.parent == directory and (report_path is None or not output.exists())
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(output)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument("--report-path", type=Path, help="Write a new report beside the run without overwriting its historical review")
    args = parser.parse_args()
    review = verify(args.directory, args.report_path)
    sys.exit(0 if review["passed"] else 1)
