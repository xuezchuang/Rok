"""Verify the independent game-window building glow audit; does not launch UE."""
import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from rok_data_paths import DATA_ROOT

ROOT = Path(__file__).resolve().parents[1]
AUDITS = ROOT / "Saved/RokUnityMigration/BuildingPreviewGlowAudit"
NAMES = [
    "01_selected_city", "02_natural_early", "03_natural_mid", "04_natural_loop",
    "05_diagnostic_none", "06_diagnostic_layer0", "07_diagnostic_layer1",
    "08_diagnostic_both", "09_diagnostic_both_after_loop", "10_closed",
    "11_reopened_reset", "12_final_city",
]
PARENT_SOURCE = ROOT / "Saved/UnityBarracksParentWindowProbe/Artifacts/native_20260925_111125_541/render_evidence.json"


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


def polygon(state, index):
    projection = state["projection_pixels"]
    scale_x = (projection["right"] - projection["origin_x"]) / 1440
    scale_y = (projection["bottom"] - projection["origin_y"]) / 810
    points = [
        (projection["origin_x"] + p["x"] * scale_x,
         projection["origin_y"] + p["y"] * scale_y)
        for p in state["layers"][index]["source_quad"]
    ]
    mask = Image.new("1", (1440, 810))
    ImageDraw.Draw(mask).polygon(points, fill=1)
    return np.asarray(mask, dtype=bool)


def source_parent_opaque(body_rect):
    """Source Type2BodyLeft nine-slice's fully opaque four-tap interior."""
    frame = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_frame.png"
    alpha = np.asarray(Image.open(frame).convert("RGBA"), dtype=np.uint8)[:, :, 3]
    yy, xx = np.indices((810, 1440))

    def mapped(v, target, source, first, last):
        return np.where(v < first, v,
                        np.where(v > target - last, source - (target - v),
                                 first + (v - first) * (source - first - last) / (target - first - last)))

    # BodyLeft samples the same Unity sprite; position/size come from the
    # audited layout contract, never from observed image differences.
    x, y, right, bottom = body_rect
    u = 1 + mapped(xx + .5 - x, right - x, 524, 493, 15) - .5
    v = (2048 - 822 - 659) + mapped(yy + .5 - y, bottom - y, 659, 278, 291) - .5
    ix = np.clip(np.floor(u).astype(int), 0, alpha.shape[1] - 2)
    iy = np.clip(np.floor(v).astype(int), 0, alpha.shape[0] - 2)
    opaque = ((alpha[iy, ix] == 255) & (alpha[iy, ix + 1] == 255)
              & (alpha[iy + 1, ix] == 255) & (alpha[iy + 1, ix + 1] == 255))
    parent = np.zeros((810, 1440), dtype=bool)
    parent[212:712, 178:678] = True
    return opaque & parent, sha(frame)


def angle(layer):
    a, b = layer["source_quad"][:2]
    return math.atan2(b["y"] - a["y"], b["x"] - a["x"])


def delta_angle(a, b):
    return (b - a + math.pi) % (2 * math.pi) - math.pi


def source_alpha(layer, seconds):
    phase = seconds % 2
    index = int(phase)
    u = phase - index
    a, b = layer["alpha_keys"][index:index + 2]
    return ((2*u**3 - 3*u*u + 1) * a["value"]
            + (u**3 - 2*u*u + u) * a["outSlope"]
            + (-2*u**3 + 3*u*u) * b["value"]
            + (u**3 - u*u) * b["inSlope"])


def verify(directory):
    directory = directory.resolve()
    assert directory.parent == AUDITS.resolve(), "Audit directory is outside Saved/RokUnityMigration"
    manifest = directory / "manifest.json"
    data = json.loads(manifest.read_text(encoding="utf-8-sig"))
    assert data["passed"] and data["complete"] and not data["failures"]
    assert data["capture_includes_slate_ui"] and data["owns_target_world"]
    assert Path(data["project"]).resolve() == ROOT.resolve()
    assert data["world"] == "/Game/Maps/L_RokUnityGameAppCity"
    assert len(data["captures"]) == data["expected_capture_count"] == len(NAMES)
    layout_contract = data.get("parent_layout_contract", "legacy_type2_v1_unversioned")
    assert layout_contract in ("legacy_type2_v1_unversioned", "type9_parent_v2", "type9_parent_v3_info_button")
    body_rect, header_rect = ((177., 139.2, 720., 800.), (160., 108., 720., 168.)) if layout_contract == "legacy_type2_v1_unversioned" else source_type9_parent_rects()
    assert [a["button"] for a in data["actions"]] == [
        "BuildingUpgradeButton", "Type2CloseButton", "BuildingUpgradeButton", "Type2CloseButton"
    ]
    source_path = DATA_ROOT / "BuildingPreviewGlow/glow_data.json"
    source = json.loads(source_path.read_text(encoding="utf-8-sig"))
    assert source["schema"] == "rok.building.preview_glow.data.v1" and source["duration"] == 2
    assert [layer["name"] for layer in source["layers"]] == ["img_star003_2", "img_star003_1"]

    states = []
    images = []
    artifacts = []
    for i, (name, capture) in enumerate(zip(NAMES, data["captures"])):
        path = Path(capture["file"]).resolve()
        assert path.parent == directory and path.stem == name
        assert path.stat().st_size == capture["file_bytes"] and sha(path) == capture["png_sha256"]
        image = Image.open(path).convert("RGB")
        assert image.size == (1440, 810), name
        images.append(np.asarray(image, dtype=np.int16))
        for phase in ("request", "processed"):
            state = capture[phase]
            opened = i in range(1, 9) or i == 10
            manual = i in range(4, 9)
            mask = {4: 0, 5: 1, 6: 2, 7: 3, 8: 3}.get(i, 3)
            assert state["open"] == opened and state["glow_attached"] == opened
            assert state["state_unchanged"] and state["manual_clock"] == manual
            assert state["audit_layer_mask"] == mask
            if opened:
                if layout_contract in ("type9_parent_v2", "type9_parent_v3_info_button"):
                    for key, expected in (("body_left_pixel_rect", body_rect), ("header_left_pixel_rect", header_rect)):
                        actual = state[key]
                        assert all(abs(actual[field] - value) < 1 for field, value in zip(("x", "y", "right", "bottom"), expected)), (name, phase, key)
                        assert abs(actual["local_width"] - (expected[2] - expected[0])) < 1
                        assert abs(actual["local_height"] - (expected[3] - expected[1])) < 1
                if layout_contract == "type9_parent_v3_info_button":
                    for key, expected in (("source_preview_pixel_rect", (248.8, 290.5, 607.2, 633.5)),
                                          ("info_button_pixel_rect", (211, 227, 271, 287)),
                                          ("info_glyph_pixel_rect", (222, 238, 260, 276))):
                        actual = state[key]
                        assert all(abs(actual[field] - value) < 1 for field, value in zip(("x", "y", "right", "bottom"), expected)), (name, phase, key)
                    assert (state["glow_canvas_z"], state["source_preview_canvas_z"], state["info_button_canvas_z"]) == (1,2,5)
                    assert state["info_glyph_is_button_child"]
                assert state["playback_active"] and not state["last_error"]
                assert state["auto_advance"] == (not manual)
                assert state["paint_count"] > 0
                assert state["last_paint_layer_count"] == mask.bit_count()
                assert len(state["layers"]) == 2
                assert [layer["name"] for layer in state["layers"]] == ["img_star003_2", "img_star003_1"]
                projection = state["projection_pixels"]
                assert abs(projection["origin_x"]) < 2 and abs(projection["origin_y"]) < 2
                assert abs(projection["center_x"] - 428) < 2 and abs(projection["center_y"] - 406) < 2
                assert abs(projection["right"] - 1440) < 2 and abs(projection["bottom"] - 810) < 2
                assert abs(state["source_phase"] - state["playback_time"] % 2) < 1e-5
                for j, layer in enumerate(state["layers"]):
                    assert len(layer["source_quad"]) == 4
                    assert layer["degrees_per_second"] == [4, 3][j]
                    assert 0 < layer["alpha"] < 1
                    assert abs(layer["alpha"] - source_alpha(source["layers"][j], state["playback_time"])) < 1e-6
                    assert all(math.isfinite(p[k]) for p in layer["source_quad"] for k in ("x", "y"))
            if manual:
                assert abs(state["playback_time"] - (2.75 if i == 8 else .75)) < 1e-6
                assert abs(state["source_phase"] - .75) < 1e-6
                assert abs(state["last_paint_phase"] - .75) < 1e-6
        assert capture["processed"]["frame"] >= capture["request"]["frame"]
        states.append(capture["processed"])
        artifacts.append({"name": path.name, "sha256": sha(path), "bytes": path.stat().st_size})

    early, middle, loop = states[1:4]
    assert early["playback_time"] < 1 and 1.1 <= middle["playback_time"] < 2
    assert loop["playback_time"] >= 2.3
    assert data["first_natural_time"] >= early["playback_time"]
    assert data["last_natural_time"] >= loop["playback_time"]
    assert data["last_natural_time"] - data["first_natural_time"] > 1.6
    assert loop["source_phase"] < 1, "Alpha phase did not loop after two seconds"
    rotation = []
    for j in range(2):
        observed = math.degrees(delta_angle(angle(early["layers"][j]), angle(loop["layers"][j])))
        expected = [4, 3][j] * (loop["playback_time"] - early["playback_time"])
        assert abs(abs(observed) - expected) < 1, "Continuous rotation stopped at alpha loop"
        rotation.append({"layer": j, "observed_degrees": observed, "expected_degrees": expected})
        assert abs(states[7]["layers"][j]["alpha"] - states[8]["layers"][j]["alpha"]) < 1e-6
    assert abs(states[10]["playback_time"]) < .8, "Reopened glow did not reset"
    assert states[10]["last_paint_layer_count"] == 2

    # The four frames have one fixed source time and only the two real Slate
    # draw-layer bits change. Compare actual final-window pixels in each quad.
    none, layer0, layer1, both = images[4:8]
    parent_opaque, frame_sha = source_parent_opaque(body_rect)
    masks = [polygon(states[7], j) & parent_opaque for j in range(2)]
    metrics = []
    for j, single in enumerate((layer0, layer1)):
        opaque_valid = int(masks[j].sum())
        assert opaque_valid > 1000, "Source layer has no opaque parent pixels for visual acceptance"
        changed = np.max(np.abs(single - none), axis=2) > 2
        changed_in_quad = int((changed & masks[j]).sum())
        other = layer1 if j == 0 else layer0
        contribution_in_both = int(((np.max(np.abs(both - other), axis=2) > 2) & masks[j]).sum())
        assert changed_in_quad > 100 and contribution_in_both > 100, "Source glow layer is absent in actual Slate pixels"
        metrics.append({"layer": j, "source_parent_opaque_pixels": opaque_valid,
                        "single_vs_none_pixels": changed_in_quad,
                        "both_vs_other_pixels": contribution_in_both})
    union = masks[0] | masks[1]
    rotation_pixels = int(((np.max(np.abs(images[8] - both), axis=2) > 2) & union).sum())
    assert rotation_pixels > 50, "Continuous post-loop rotation made no visible pixel change"
    assert states[7]["paint_count"] > states[4]["paint_count"]
    assert states[8]["source_phase"] == states[7]["source_phase"]

    result = {
        "passed": True,
        "parent_layout_contract": layout_contract,
        "body_left_pixel_rect": body_rect,
        "parent_source_sha256": sha(PARENT_SOURCE) if layout_contract in ("type9_parent_v2", "type9_parent_v3_info_button") else None,
        "manifest_sha256": sha(manifest),
        "source_data_sha256": sha(source_path),
        "source_parent_frame_sha256": frame_sha,
        "source_parent_opaque_pixels": int(parent_opaque.sum()),
        "natural_rotation": rotation,
        "diagnostic_clock_seconds": [.75, 2.75],
        "layer_pixel_contributions": metrics,
        "post_loop_rotation_changed_pixels": rotation_pixels,
        "artifacts": artifacts,
        "scope": "Real Init6 barracks modal/delegates, natural Slate tick through alpha loop and reset, fixed-time per-layer final-window pixels; no physical pointer hit-test or upgrade command.",
    }
    output = directory / "runtime_verify.json"
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    verify(parser.parse_args().directory)
