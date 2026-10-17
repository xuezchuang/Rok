"""Strict verifier for the isolated Unity 2019 placement-menu reference fixture."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved" / "UnityPlacementMenuProbeV3"
KEYS = ("dxf255_valid", "dxf255_disabled", "dxf240600006_valid", "dxf240600006_disabled")
DXFS = (255.0, 255.0, 240.600006, 240.600006)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def near(actual: float, expected: float, tolerance: float, label: str) -> None:
    assert math.isfinite(actual) and abs(actual - expected) <= tolerance, (label, actual, expected)


def rect(node: dict, expected: tuple[float, float, float, float], label: str) -> None:
    actual = node["screenRect"]
    for key, value in zip(("x", "y", "width", "height"), expected):
        near(actual[key], value, 0.75, f"{label}.{key}")


def source_crop_nontransparent(atlas: Image.Image, box: tuple[int, int, int, int]) -> int:
    return sum(1 for alpha in atlas.crop(box).getchannel("A").getdata() if alpha > 0)


def verify(artifact: Path) -> dict:
    artifact = artifact.resolve()
    assert artifact.parent == (PROBE / "Artifacts").resolve() and artifact.name.startswith("native_"), artifact
    evidence_path = artifact / "render_evidence.json"
    contract_path = artifact / "source_contract.json"
    assert evidence_path.is_file() and contract_path.is_file()
    evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
    contract = json.loads(contract_path.read_text(encoding="utf-8"))
    assert evidence["schema"] == "rok.placement_menu.native.v1" and evidence["complete"] is True
    assert evidence.get("failure") in (None, "")
    assert contract["schema"] == "rok.placement_menu_native_input.v1"
    assert evidence["inputSha256"] == sha(contract_path)
    assert contract["rendererSha256"] == sha(ROOT / "Scripts" / "RenderRokPlacementMenuNative.cs")
    assert contract["runnerSha256"] == sha(ROOT / "Scripts" / "RunRokPlacementMenuNative.ps1")
    packages = json.loads((PROBE / "Packages" / "manifest.json").read_text(encoding="utf-8"))["dependencies"]
    assert packages["com.unity.ugui"] == "1.0.0"
    assert packages["com.unity.modules.ui"] == "1.0.0" and packages["com.unity.modules.imgui"] == "1.0.0"
    assert evidence["rendererSha256"] == contract["rendererSha256"]
    assert evidence["unityVersion"] == "2019.4.28f1c1"
    assert evidence["colorSpace"] == "Gamma" and evidence["graphicsDevice"] != "Null"
    assert evidence["canvasRenderMode"] == "ScreenSpaceCamera"
    assert evidence["canvasScaleMode"] == "ScaleWithScreenSize"
    assert evidence["screenMatchMode"] == "MatchWidthOrHeight"
    near(evidence["canvasMatchWidthOrHeight"], 1, 1e-5, "canvas.matchHeight")
    near(evidence["canvasScaleFactor"], 1, 1e-5, "canvas.scale")
    assert evidence["unityScreenWidth"] == 1440 and evidence["unityScreenHeight"] == 810
    assert evidence["cameraPixelWidth"] == 1440 and evidence["cameraPixelHeight"] == 810
    assert evidence["gameViewSizeMethod"] == (
        "UnityEditor.GameView fixed 1440x810 custom size + SetMainPlayModeViewSize before PlayMode")
    rect({"screenRect": evidence["cameraPixelRect"]}, (0, 0, 1440, 810), "cameraPixelRect")
    rect({"screenRect": evidence["canvasPixelRect"]}, (0, 0, 1440, 810), "canvasPixelRect")
    near(evidence["cameraOrthographicSize"], 405, 1e-5, "camera.orthographicSize")
    assert evidence["width"] == 1440 and evidence["height"] == 810
    assert evidence["targetFormat"] == "ARGB32" and evidence["targetReadWrite"] == "Default"
    assert evidence["sourceDerivedFixture"] is True and evidence["originalPrefabInstantiated"] is False
    assert evidence["fullGameAppExecuted"] is False and evidence["sourceGrayMaterialUsed"] is True
    assert evidence["noAccountOrServerBehavior"] is True
    assert evidence["scope"] == "isolated_source_derived_ugui_fixture_not_gameapp"
    assert evidence["cameraBackground"]["a"] == 0
    near(evidence["buttonFadeSeconds"], 0.1, 1e-6, "source.buttonFadeSeconds")
    for field, rgb, alpha in (("buttonNormalColor", 1, 1),
                              ("buttonHighlightedColor", 245 / 255, 1),
                              ("buttonPressedColor", 200 / 255, 1),
                              ("buttonDisabledColor", 200 / 255, 128 / 255)):
        for channel in ("r", "g", "b"):
            near(evidence[field][channel], rgb, 1e-5, f"{field}.{channel}")
        near(evidence[field]["a"], alpha, 1e-5, f"{field}.a")

    sources = {row["path"]: row for row in contract["sourceFiles"]}
    assert len(sources) == 11
    for row in sources.values():
        original = ROOT / row["path"]
        isolated = PROBE / row["probeAsset"]
        assert original.is_file() and isolated.is_file()
        assert sha(original) == sha(isolated) == row["sha256"]
    atlas_path = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/ui_hud.png"
    assert contract["spriteAtlasSha256"] == sha(atlas_path)
    assert contract["spriteSheetSha256"] == sources["ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/ui_hud.tpsheet"]["sha256"]
    assert contract["grayShaderSha256"] == sources["ref/client-unity/Assets/BundleAssets/UI/UIShader/ImageGrayShader.shader"]["sha256"]
    assert contract["grayMaterialSha256"] == sources["ref/client-unity/Assets/BundleAssets/Default/UI_GrayWithMask.mat"]["sha256"]
    with Image.open(atlas_path) as source_image:
        atlas = source_image.convert("RGBA")
    assert atlas.size == (1024, 1024)
    # Unity SpriteRects are bottom-left: (721,575)/(1,435), both 88 square.
    assert source_crop_nontransparent(atlas, (721, 361, 809, 449)) == 6305
    assert source_crop_nontransparent(atlas, (1, 501, 89, 589)) == 6276

    samples = evidence["samples"]
    assert len(samples) == 4 and tuple(x["key"] for x in samples) == KEYS
    images: dict[str, Image.Image] = {}
    pixels: dict[str, dict] = {}
    for i, sample in enumerate(samples):
        key = KEYS[i]
        assert sample["png"] == key + ".png"
        png = artifact / sample["png"]
        assert png.is_file() and sample["sha256"] == sha(png)
        with Image.open(png) as opened:
            assert opened.mode == "RGBA" and opened.size == (1440, 810)
            image = opened.copy()
        images[key] = image
        assert image.getpixel((0, 0))[3] == 0 and image.getpixel((1439, 809))[3] == 0
        assert image.getchannel("A").getbbox() is not None, f"empty render {key}"
        scale = 172.0 / DXFS[i]
        near(sample["dxf"], DXFS[i], 1e-4, key + ".dxf")
        near(sample["uiScale"], scale, 1e-5, key + ".uiScale")
        assert sample["confirmEnabled"] == (i % 2 == 0)
        near(sample["targetScreen"]["x"], 720, 1e-5, key + ".target.x")
        near(sample["targetScreen"]["y"], 405, 1e-5, key + ".target.y")
        near(sample["popupAnchoredPosition"]["x"], 0, 1e-5, key + ".popup.anchor.x")
        near(sample["popupAnchoredPosition"]["y"], 0, 1e-5, key + ".popup.anchor.y")
        rect(sample["popup"], (720 - 236.35 * scale, 405 - 110 * scale, 472.7 * scale, 220 * scale), key + ".popup")
        # Resolved source prefab topology: root (472.7x220), FeatureBtns
        # bottom anchor +59, pl_tp21/22 top-right (-150/-250,-50), 87px images.
        for field, local_left, sprite in (("confirmImage", 279.2, "ui_hud[btn_hud_1008]"),
                                          ("cancelImage", 179.2, "ui_hud[btn_hud_1004]")):
            node = sample[field]
            assert node["sprite"] == sprite and node["active"] is True
            rect(node, (720 + (local_left - 236.35) * scale,
                        405 + (167.5 - 110) * scale, 87 * scale, 87 * scale), key + "." + field)
        assert sample["confirmImage"]["interactable"] == sample["confirmEnabled"]
        assert sample["cancelImage"]["interactable"] is True
        assert sample["cancelImage"]["material"] == "UI/Default"
        assert (sample["confirmImage"]["material"] == "UI_GrayWithMask") == (not sample["confirmEnabled"])
        for field, expected in (("confirmImage", evidence["buttonNormalColor"] if sample["confirmEnabled"] else evidence["buttonDisabledColor"]),
                                ("cancelImage", evidence["buttonNormalColor"])):
            for channel in ("r", "g", "b", "a"):
                near(sample[field]["rendererColor"][channel], expected[channel], 1e-3,
                     f"{key}.{field}.rendererColor.{channel}")
        tint = sample["confirmImage"]["buttonDisabledColor"]
        for channel in ("r", "g", "b"):
            near(tint[channel], 200 / 255, 1e-5, key + ".disabledColor." + channel)
        near(tint["a"], 128 / 255, 1e-5, key + ".disabledColor.alpha")
        box = sample["confirmImage"]["screenRect"]
        roi = image.crop((int(math.floor(box["x"])), int(math.floor(box["y"])),
                          int(math.ceil(box["x"] + box["width"])), int(math.ceil(box["y"] + box["height"]))))
        opaque = [v for v in roi.getdata() if v[3] > 16]
        assert len(opaque) > 100, f"confirm source sprite invisible: {key}"
        pixels[key] = {"nontransparent_full_frame": sum(a > 0 for a in image.getchannel("A").getdata()),
                       "confirm_nontransparent_roi": len(opaque),
                       "confirm_colored_pixels": sum(max(r,g,b)-min(r,g,b) > 2 for r,g,b,a in opaque)}
        if not sample["confirmEnabled"]:
            assert pixels[key]["confirm_colored_pixels"] < max(20, len(opaque) // 50), f"gray source material not rendered: {key}"
    for a, b in (("dxf255_valid", "dxf255_disabled"), ("dxf240600006_valid", "dxf240600006_disabled")):
        assert sha(artifact / (a + ".png")) != sha(artifact / (b + ".png"))
        assert pixels[a]["confirm_colored_pixels"] > pixels[b]["confirm_colored_pixels"] + 50
    result = {"schema": "rok.placement_menu.native_verify.v1", "complete": True,
              "artifact": str(artifact), "evidence_sha256": sha(evidence_path),
              "contract_sha256": sha(contract_path), "sample_keys": list(KEYS),
              "pixels": pixels, "scope": evidence["scope"],
              "limitations": ["source-derived UGUI fixture, not executable GameApp capture",
                              "popup animation, account eligibility, world target projection and other UI are not verified"]}
    output = artifact / "verified_capture.json"
    assert not output.exists(), f"do not overwrite existing verification: {output}"
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifact", required=True, type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(args.artifact), ensure_ascii=False, indent=2))
