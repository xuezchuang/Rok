"""Validate all original-window terminal geometry measurements without UE targets."""
import hashlib
import json
import math
import sys
from pathlib import Path
from rok_data_paths import resolve

from PIL import Image, ImageStat

ROOT = Path(__file__).resolve().parents[1]
ARTIFACTS = ROOT / "Saved/UnityBarracksParentWindowV5Probe/Artifacts"
SOURCE_MODEL = "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_Window_Type2.prefab"
PREDICTION = (1189.5, 40.0, 69.0, 69.0)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rect(rect):
    return tuple(float(rect[k]) for k in ("x", "y", "width", "height"))


def verify(path):
    path = Path(path).resolve()
    assert path.name == "render_evidence.json" and path.parent.parent == ARTIFACTS.resolve()
    evidence = json.loads(path.read_text(encoding="utf-8-sig"))
    inputs_path = path.parent / "ReferenceInputs.json"
    inputs = json.loads(inputs_path.read_text(encoding="utf-8-sig"))
    failures = []

    def check(ok, reason):
        if not ok:
            failures.append(reason)

    check(inputs.get("probeVersion") == 5 and inputs.get("width") == 1440 and inputs.get("height") == 810, "V5 identity/resolution")
    check(evidence.get("complete") and not evidence.get("failure") and evidence.get("playMode"), "native PlayMode completion")
    check(evidence.get("unityVersion") == "2019.4.28f1c1" and evidence.get("inputSha256") == sha(inputs_path), "Unity/input identity")
    check(evidence.get("nativeAnimatorEnabled") and evidence.get("projectionInitialized"), "original Animator/camera")
    check(evidence.get("nativeCameraClearFlags") == "Depth" and evidence.get("clearPolicy") == "GL.Clear(true,true,Color.clear)", "original camera/target clear")
    source_hashes = {entry["path"]: entry["sha256"] for entry in inputs.get("sources") or []}
    check(SOURCE_MODEL in source_hashes, "source Type2 nested prefab missing")
    for relative, expected in source_hashes.items():
        if relative.startswith("Source/Rok/"):
            continue  # UE product code may change during this independent Unity capture.
        check(sha(resolve(relative)) == expected, "source hash changed: " + relative)
    setup_path = path.parent / "setup_evidence.json"
    check(setup_path.is_file(), "source setup missing")
    if setup_path.is_file():
        setup = json.loads(setup_path.read_text(encoding="utf-8-sig"))
        check(sha(setup_path) == evidence.get("setupSha256"), "source setup hash")
        check(setup.get("fixtureTitleApplied") == inputs.get("fixtureTitle"), "Chinese fixture title")
        check(bool(setup.get("languageBindings")) and bool(setup.get("layoutFlagsApplied")), "Chinese source setup")
        check(all(abs((setup.get("rootAfterLoad") or {}).get("localScale", {}).get(k, math.inf)) < 1e-5 for k in "xyz"), "serialized zero root scale")
    samples = evidence.get("samples") or []
    check(len(samples) == 4 and evidence.get("clearCount", 0) >= 4, "four natural Open phases")
    if len(samples) == 4:
        check(all(s.get("stateName") == "Open" for s in samples), "source Open Animator state")
        check(samples[-1].get("elapsedGameTime", 0) >= 0.5 and samples[-1].get("animatorNormalizedTime", 0) >= 0.99, "natural Open terminal")
        for index, sample in enumerate(samples):
            image = path.parent / sample.get("png", "")
            check(image.is_file() and image.parent == path.parent, "PNG sample " + str(index))
            if image.is_file():
                check(sha(image) == sample.get("sha256"), "PNG hash " + str(index))
                picture = Image.open(image).convert("RGBA")
                check(picture.size == (1440, 810) and max(ImageStat.Stat(picture).stddev[:3]) > 3, "visible original image " + str(index))
        terminal_root = samples[-1].get("afterCameraRender") or {}
        check(all(math.isfinite(terminal_root.get("localScale", {}).get(k, math.inf)) for k in "xyz"), "measured root scale")
    close = evidence.get("terminalClose") or {}
    check(close.get("stateName") == "Open" and close.get("animatorNormalizedTime", 0) >= .99, "close captured at original Open terminal")
    check((close.get("cameraPixelWidth"), close.get("cameraPixelHeight")) == (1440, 810), "actual camera pixel identity")
    controls = {key: close.get(key) or {} for key in ("close", "title", "nestedModel")}
    names = {"close": "btn_close", "title": "title", "nestedModel": "UI_Model_Window_Type1"}
    geometry = {}
    for key, item in controls.items():
        check(item.get("path", "").split("/")[-1] == names[key], "actual control path " + key)
        check(item.get("activeSelf") and item.get("activeInHierarchy"), "actual control visible in hierarchy " + key)
        world, screen = item.get("worldCorners") or [], item.get("screenCorners") or []
        check(len(world) == len(screen) == 4, "actual four corners " + key)
        if len(screen) == 4:
            bounds = (min(p["x"] for p in screen), min(p["y"] for p in screen),
                      max(p["x"] for p in screen) - min(p["x"] for p in screen),
                      max(p["y"] for p in screen) - min(p["y"] for p in screen))
            actual = rect(item.get("screenRect") or {})
            check(all(abs(a - b) < .01 for a, b in zip(bounds, actual)), "rect derives from actual corners " + key)
            geometry[key] = actual
    button = controls["close"]
    check(button.get("path", "").endswith("/UI_Model_Window_Type1/title/btn_close"), "close belongs to nested source title")
    check(button.get("hasButton") and button.get("buttonEnabled") and button.get("interactable"), "actual enabled interactive Button")
    check(button.get("hasGraphic") and button.get("graphicEnabled") and button.get("raycastTarget") and not button.get("canvasRendererCull"), "actual visible raycast Graphic")
    check(bool(button.get("spriteName")) and bool(button.get("graphicType")) and bool(button.get("buttonType")), "source visual/button component identity")
    comparison = {}
    if "close" in geometry:
        observed = geometry["close"]
        delta = [a - b for a, b in zip(observed, PREDICTION)]
        comparison = {"source_derived_prediction_xywh": PREDICTION, "actual_close_screen_xywh": observed,
                      "actual_minus_prediction_xywh": delta,
                      "matches_prediction_within_1px": all(abs(value) <= 1 for value in delta)}
    inventory = evidence.get("terminalWindowGeometry") or []
    check(len(inventory) == evidence.get("terminalWindowRectTransformCount") and len(inventory) > 20,
          "complete all-RectTransform inventory")
    check(sum(bool(row.get("hasGraphic")) for row in inventory) == evidence.get("terminalWindowGraphicCount"),
          "all Graphic components represented")
    identities = [(row.get("path"), row.get("siblingIndex")) for row in inventory]
    check(len(identities) == len(set(identities)), "unique hierarchy path/sibling identity")
    path_index = {row.get("path"): row for row in inventory}
    for key in ("close", "title", "nestedModel"):
        item = controls[key]
        check(item.get("path") in path_index, "captured " + key + " in inventory")
        if item.get("path") in path_index:
            check(rect(item["screenRect"]) == rect(path_index[item["path"]]["screenRect"]),
                  "captured " + key + " matches inventory")
    for index, row in enumerate(inventory):
        check(bool(row.get("path")) and row.get("path", "").startswith("UIRoot/Container/WindowLayer/UI_Win_BuildingUpdate"),
              "source window path " + str(index))
        check(len(row.get("worldCorners") or []) == len(row.get("screenCorners") or []) == 4,
              "four measured corners " + str(index))
        for field in ("localScale", "lossyScale"):
            check(all(math.isfinite(row.get(field, {}).get(k, math.inf)) for k in "xyz"),
                  "finite " + field + " " + str(index))
        screen = row.get("screenCorners") or []
        if len(screen) == 4:
            bounds = (min(p["x"] for p in screen), min(p["y"] for p in screen),
                      max(p["x"] for p in screen) - min(p["x"] for p in screen),
                      max(p["y"] for p in screen) - min(p["y"] for p in screen))
            check(all(math.isfinite(v) for v in bounds), "finite screen quad " + str(index))
            try:
                check(all(abs(a - b) <= .01 for a, b in zip(bounds, rect(row["screenRect"]))),
                      "screen rect from corners " + str(index))
            except (KeyError, ValueError, TypeError):
                check(False, "screen rect missing " + str(index))
        if row.get("hasGraphic"):
            check(bool(row.get("graphicType")) and bool(row.get("materialName")) and bool(row.get("shaderName")),
                  "Graphic type/material identity " + str(index))
    focus = [row for row in inventory if any(token in row.get("path", "") for token in
             ("/Rect/pl_content/Left", "/Rect/pl_content/", "img_build", "btn_buildInfo", "bg", "PreviewFrame"))]
    result = {"schema": "rok.barracks_parent_window.v5_geometry_verify.v1", "passed": not failures,
              "failures": failures, "evidence_sha256": sha(path), "source_nested_prefab_sha256": source_hashes.get(SOURCE_MODEL),
              "terminal_frame": close.get("unityFrame"), "terminal_animator_normalized_time": close.get("animatorNormalizedTime"),
              "actual_geometry": geometry, "close_prediction_comparison": comparison,
              "inventory_count": len(inventory), "graphic_count": evidence.get("terminalWindowGraphicCount"),
              "focus_rows": [{"path": row.get("path"), "screenRect": row.get("screenRect"),
                              "activeInHierarchy": row.get("activeInHierarchy"),
                              "graphicType": row.get("graphicType"), "spriteName": row.get("spriteName")}
                             for row in focus],
              "close_component": {k: button.get(k) for k in ("buttonType", "buttonEnabled", "interactable", "graphicType", "graphicEnabled", "raycastTarget", "canvasRendererCull", "spriteName", "materialName", "shaderName")},
              "scope": "Every original window RectTransform at natural Open terminal. Gates cover identity and internal measurement consistency only; no UE coordinates or visual target are imposed."}
    (path.parent / "verify_v5.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return result


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence")
    args = parser.parse_args()
    result = verify(args.evidence)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    sys.exit(0 if result["passed"] else 1)
