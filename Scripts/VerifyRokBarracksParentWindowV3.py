"""Validate the V3 no-scale-assignment control and compare terminal output to V2."""
import hashlib
import json
import math
import sys
from pathlib import Path
from rok_data_paths import resolve

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ARTIFACTS = ROOT / "Saved/UnityBarracksParentWindowV3Probe/Artifacts"
V2 = ROOT / "Saved/UnityBarracksParentWindowProbe/Artifacts/native_20260925_111125_541/render_evidence.json"
V2_HASH = "6e794c75ec98efe9a40c7b495a813be802ff699018a09a5423cf92b81cdcc644"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rect_values(rect):
    return [float(rect[key]) for key in ("x", "y", "width", "height")]


def verify(path):
    path = Path(path).resolve()
    assert path.name == "render_evidence.json" and path.parent.parent == ARTIFACTS.resolve()
    evidence = json.loads(path.read_text(encoding="utf-8-sig"))
    inputs_path = path.parent / "ReferenceInputs.json"
    inputs = json.loads(inputs_path.read_text(encoding="utf-8-sig"))
    old = json.loads(V2.read_text(encoding="utf-8-sig"))
    old_inputs = json.loads((V2.parent / "ReferenceInputs.json").read_text(encoding="utf-8-sig"))
    failures = []

    def check(ok, reason):
        if not ok:
            failures.append(reason)

    check(digest(V2) == V2_HASH and old.get("complete") and not old.get("failure"), "v2 control identity/completion")
    check(inputs.get("probeVersion") == 3 and old_inputs.get("probeVersion") == 2, "A/B versions")
    check(inputs.get("abControl", {}).get("v2EvidenceSha256") == V2_HASH, "v2 control hash in inputs")
    for key in ("prefab", "uiRootPrefab", "width", "height", "sampleThresholdsSeconds", "texts", "fixtureTitle", "renderPolicy", "fixture", "referenceGeometry"):
        check(inputs.get(key) == old_inputs.get(key), "A/B input changed: " + key)
    old_sources = {row["path"]: row["sha256"] for row in old_inputs["sources"]}
    new_sources = {row["path"]: row["sha256"] for row in inputs["sources"]}
    excluded = {"Scripts/PrepareRokBarracksParentWindow.py", "Scripts/RenderRokBarracksParentWindow.cs"}
    check(all(new_sources.get(k) == v for k, v in old_sources.items() if k not in excluded), "A/B original source chain differs")
    for source in inputs["sources"]:
        check(digest(resolve(source["path"])) == source["sha256"], "V3 source changed: " + source["path"])
    check(evidence.get("complete") and not evidence.get("failure"), "V3 native run incomplete")
    check(evidence.get("playMode") and evidence.get("nativeAnimatorEnabled") and evidence.get("projectionInitialized"), "V3 native Animator/projection")
    check(evidence.get("unityVersion") == "2019.4.28f1c1" and evidence.get("inputSha256") == digest(inputs_path), "V3 version/input")
    check(evidence.get("clearPolicy") == "GL.Clear(true,true,Color.clear)" and evidence.get("nativeCameraClearFlags") == "Depth", "V3 v2 render policy")
    setup_path = path.parent / "setup_evidence.json"
    check(setup_path.is_file(), "V3 setup missing")
    setup = json.loads(setup_path.read_text(encoding="utf-8-sig")) if setup_path.is_file() else {}
    check(evidence.get("setupSha256") == digest(setup_path) if setup_path.is_file() else False, "V3 setup hash")
    check(setup.get("fixtureTitleApplied") == old.get("fixtureTitleApplied") == inputs.get("fixtureTitle"), "V3 Chinese fixture title")
    check(setup.get("languageBindings") == old.get("languageBindings") and setup.get("layoutFlagsApplied") == old.get("layoutFlagsApplied"), "V3 Chinese/layout setup differs")
    after_load = setup.get("rootAfterLoad") or {}
    check(after_load.get("label") == "after_prefab_load_before_force_update", "missing after-load checkpoint")
    load_scale = after_load.get("localScale") or {}
    check(all(math.isfinite(load_scale.get(axis, math.inf)) for axis in "xyz"), "after-load root scale missing")
    samples = evidence.get("samples") or []
    check(len(samples) == 4 and evidence.get("clearCount", 0) >= 4, "four V3 cleared captures")
    checkpoints = []
    for i, sample in enumerate(samples):
        if i >= len(old.get("samples") or []):
            break
        check(sample.get("label") == old["samples"][i].get("label"), "sample label " + str(i))
        check(sample.get("stateName") == "Open" and sample.get("animatorStateLength", 0) > 0, "native Open state " + str(i))
        check(sample.get("elapsedGameTime", -1) >= inputs["sampleThresholdsSeconds"][i] - 0.001, "time threshold " + str(i))
        if i:
            check(sample["elapsedGameTime"] > samples[i - 1]["elapsedGameTime"] and sample["unityFrame"] > samples[i - 1]["unityFrame"], "natural sample order " + str(i))
        for field, label in (("afterForceUpdate", "after_force_update"), ("beforeCameraRender", "before_camera_render"), ("afterCameraRender", "after_camera_render")):
            cp = sample.get(field) or {}
            check(cp.get("label") == label and cp.get("canvasRenderMode") == "ScreenSpaceCamera", "checkpoint " + field + " " + str(i))
            for key in ("screenRect", "windowLayerScreenRect"):
                try:
                    check(all(math.isfinite(v) for v in rect_values(cp[key])), "finite " + field + "/" + key)
                except (KeyError, ValueError, TypeError):
                    check(False, "missing " + field + "/" + key)
            checkpoints.append({"sample": i, "checkpoint": field, "local_scale": cp.get("localScale"),
                                "screen_rect": cp.get("screenRect"), "window_layer_screen_rect": cp.get("windowLayerScreenRect"),
                                "canvas_scale_factor": cp.get("canvasScaleFactor")})
        image = path.parent / sample.get("png", "")
        check(image.is_file() and image.parent == path.parent, "V3 PNG " + str(i))
        if image.is_file():
            check(digest(image) == sample.get("sha256") and Image.open(image).size == (1440, 810), "V3 PNG hash/dimensions " + str(i))
        check(any(node.get("path") == "UIRoot" for node in sample.get("nodes") or []), "UIRoot node " + str(i))
    comparison = {}
    if len(samples) == 4 and len(old.get("samples") or []) == 4:
        new_final, old_final = samples[-1], old["samples"][-1]
        new_nodes = {row["path"]: row for row in new_final.get("nodes") or []}
        old_nodes = {row["path"]: row for row in old_final.get("nodes") or []}
        shared = sorted(new_nodes.keys() & old_nodes.keys())
        missing = sorted(old_nodes.keys() - new_nodes.keys())
        deltas = {key: [a - b for a, b in zip(rect_values(new_nodes[key]["screenRect"]), rect_values(old_nodes[key]["screenRect"]))] for key in shared}
        maximum = max((abs(v) for values in deltas.values() for v in values), default=math.inf)
        new_image, old_image = path.parent / new_final["png"], V2.parent / old_final["png"]
        if new_image.is_file() and old_image.is_file():
            a = np.asarray(Image.open(new_image).convert("RGBA"), dtype=np.int16)
            b = np.asarray(Image.open(old_image).convert("RGBA"), dtype=np.int16)
            pixel = np.abs(a - b)
            comparison = {"same_terminal_png_bytes": digest(new_image) == digest(old_image),
                          "same_terminal_pixels": bool(np.array_equal(a, b)),
                          "terminal_rgba_mae": float(pixel.mean()), "terminal_rgba_max_difference": int(pixel.max()),
                          "terminal_changed_pixels": int(np.any(pixel != 0, axis=2).sum()),
                          "terminal_v3_nonzero_alpha_pixels": int((a[:, :, 3] > 0).sum()),
                          "terminal_v2_nonzero_alpha_pixels": int((b[:, :, 3] > 0).sum())}
        comparison.update({"shared_node_count": len(shared), "missing_v2_node_paths": missing,
                           "terminal_screen_rect_max_abs_delta_px": maximum, "terminal_node_rect_deltas": deltas,
                           "terminal_geometry_strict_equal_0p01px": not missing and maximum <= 0.01,
                           "terminal_animator_time_v3": new_final.get("animatorNormalizedTime"),
                           "terminal_animator_time_v2": old_final.get("animatorNormalizedTime")})
        comparison["converged_strictly"] = bool(comparison.get("terminal_geometry_strict_equal_0p01px") and comparison.get("same_terminal_pixels"))
    result = {"schema": "rok.barracks_parent_window.v3_ab_verify.v1", "passed": not failures,
              "failures": failures, "v3_evidence_sha256": digest(path), "v2_evidence_sha256": digest(V2),
              "after_load": after_load, "root_checkpoints": checkpoints, "terminal_ab": comparison,
              "interpretation": "Passed means the no-assignment control ran with source inputs and valid captures. Strict convergence is separately reported; a difference is a valid A/B finding."}
    (path.parent / "verify_v3.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return result


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence")
    args = parser.parse_args()
    result = verify(args.evidence)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    sys.exit(0 if result["passed"] else 1)
