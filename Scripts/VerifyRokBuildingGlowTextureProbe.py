"""Validate the isolated native texture evidence and measure imported-vs-source RGBA."""
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Saved/RokUnityMigration/BuildingGlowTexture"
SOURCE = ROOT / "ref/client-unity/Assets/BundleAssets/Effect/UIsprite/shared_uisprite/star003.png"
EXPECTED = "9f159150cc1614e2eb38f88217433d2c68b1d28d0b73910ecd249f8f39c40388"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    report = json.loads((OUT / "native.json").read_text(encoding="utf-8-sig"))
    failures = []

    def check(value, label):
        if not value:
            failures.append(label)

    check(report.get("schema") == "rok.building.glow_texture.native.v1", "native schema")
    check(report.get("unityVersion") == "2019.4.28f1c1", "Unity version")
    check(report.get("colorSpace") == "Gamma", "Gamma project")
    check(report.get("graphicsAPI") != "Null", "real GPU")
    check(not report.get("failure"), "native runner failed: " + str(report.get("failure")))
    check(sha(SOURCE) == report.get("sourceSha256") == EXPECTED, "source PNG hash")
    texture = report.get("texture") or {}
    material = report.get("material") or {}
    check((texture.get("width"), texture.get("height")) == (256, 256), "original dimensions")
    check(texture.get("importerSRGB") is True and texture.get("alphaIsTransparency") is True, "original importer flags")
    check(texture.get("importerMipmaps") is False and texture.get("mipmapCount") == 1, "original no-mip state")
    check(material.get("shader") == "UI/Add", "original UI/Add material")
    check(bool(material.get("textureSampleAdd")) and bool(material.get("globalTextureSampleAdd")), "observed UI/Add _TextureSampleAdd")
    check(report.get("passThroughGPUReadback") and report.get("rgbWriteDisabled"), "real controlled GPU decoded readback")
    metrics = {}
    raw_path = OUT / "imported_gpu_mip0.bin"
    png_path = OUT / "sampled_gpu_mip0.png"
    rgba_path = OUT / "sampled_rgba_bottom_up.bin"
    required = [(png_path, "sampledPngSha256", None),
                (rgba_path, "sampledRgbaSha256", "sampledRgbaByteCount")]
    if report.get("actualGPUTextureReadback"):
        required.append((raw_path, "importedRawSha256", "importedRawByteCount"))
    for path, key, count in required:
        check(path.is_file(), "missing " + path.name)
        if path.is_file():
            check(sha(path) == report.get(key), "hash " + path.name)
            if count:
                check(path.stat().st_size == report.get(count), "byte count " + path.name)
    if png_path.is_file() and rgba_path.is_file():
        original = np.asarray(Image.open(SOURCE).convert("RGBA"), dtype=np.int16)
        sampled = np.asarray(Image.open(png_path).convert("RGBA"), dtype=np.int16)
        check(sampled.shape == original.shape, "sampled RGBA dimensions")
        if sampled.shape == original.shape:
            rgba = np.frombuffer(rgba_path.read_bytes(), dtype=np.uint8)
            check(rgba.size == 256 * 256 * 4, "sampled RGBA raw size")
            if rgba.size == 256 * 256 * 4:
                decoded = np.flipud(rgba.reshape(256, 256, 4)).astype(np.int16)
                check(np.array_equal(decoded, sampled), "PNG and raw GPU readback agree")
            directions = {"as_saved": sampled, "vertical_flip": sampled[::-1],
                          "horizontal_flip": sampled[:, ::-1], "both_flips": sampled[::-1, ::-1]}
            name = min(directions, key=lambda n: np.abs(directions[n] - original).mean())
            delta = np.abs(directions[name] - original)
            transparent = original[:, :, 3] == 0
            partial = (original[:, :, 3] > 0) & (original[:, :, 3] < 255)
            metrics = {"best_orientation": name, "mean_absolute_rgba_error": float(delta.mean()),
                       "max_absolute_rgba_error": int(delta.max()),
                       "changed_rgba_pixels": int(np.any(delta != 0, axis=2).sum()),
                       "changed_alpha_pixels": int((delta[:, :, 3] != 0).sum()),
                       "transparent_source_pixels": int(transparent.sum()),
                       "transparent_rgb_changed_pixels": int(np.any(delta[:, :, :3] != 0, axis=2)[transparent].sum()),
                       "partial_source_pixels": int(partial.sum()),
                       "partial_rgba_changed_pixels": int(np.any(delta != 0, axis=2)[partial].sum()),
                       "rgb_mae": [float(delta[:, :, i].mean()) for i in range(3)],
                       "alpha_mae": float(delta[:, :, 3].mean()),
                       "exact_rgba_match": bool(np.array_equal(directions[name], original))}
    result = {"schema": "rok.building.glow_texture.verify.v1", "passed": not failures,
              "failures": failures, "native_sha256": sha(OUT / "native.json"),
              "source_png_sha256": sha(SOURCE), "runtime_texture": texture,
              "source_material": material, "gpu_vs_png": metrics,
              "raw_gpu_available": bool(report.get("actualGPUTextureReadback")),
              "raw_gpu_failure": report.get("rawReadbackFailure"),
              "scope": "Native Unity 2019 Gamma importer/runtime/GPU texture evidence. Pixel metrics are measured, not forced to match; UE rendering parity requires separate runtime validation."}
    (OUT / "verify.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
