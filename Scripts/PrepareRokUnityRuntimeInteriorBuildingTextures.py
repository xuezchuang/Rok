import hashlib
import json
from pathlib import Path

from PIL import Image, ImageChops


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = (
    PROJECT_ROOT
    / "ref"
    / "client-unity"
    / "Assets"
    / "BundleAssets"
    / "Map"
    / "Building"
    / "Building_use"
    / "shared_building"
    / "Building_101"
    / "city_building_1"
)
OUTPUT_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "runtime_interior_baked_alpha"
METADATA_PATH = OUTPUT_ROOT / "runtime_interior_baked_alpha.json"

MODEL_IDS = [
    "build_17_101_1",
    "build_16_101_1",
    "build_18_101_1",
    "build_30_101_1",
    "build_28_101_1",
    "build_11_101_1",
    "build_2_101_1",
    "build_3_101_1",
    "build_51_101_1_1",
    "build_51_101_1_2",
    "build_51_101_1_3",
]


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def prepare():
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    records = []
    for model_id in MODEL_IDS:
        color_path = SOURCE_ROOT / (model_id + ".png")
        mask_path = SOURCE_ROOT / (model_id + "_mask.png")
        if not color_path.is_file() or not mask_path.is_file():
            raise FileNotFoundError("Missing Unity building sprite or mask: {0}, {1}".format(color_path, mask_path))

        color = Image.open(color_path).convert("RGBA")
        mask = Image.open(mask_path).convert("RGBA")
        if color.size != mask.size:
            raise ValueError("Unity building sprite/mask size mismatch for {0}".format(model_id))

        # Unity project color space is Gamma (m_ActiveColorSpace: 0), so the shader
        # consumes the stored mask byte directly: color.a * saturate(mask.r * 2.0).
        mask_alpha = mask.getchannel("R").point(lambda value: min(255, value * 2))
        baked_alpha = ImageChops.multiply(color.getchannel("A"), mask_alpha)
        color.putalpha(baked_alpha)

        output_path = OUTPUT_ROOT / (model_id + "_unity_alpha.png")
        color.save(output_path, format="PNG", optimize=True)
        records.append(
            {
                "model_id": model_id,
                "color_source": str(color_path.relative_to(PROJECT_ROOT)).replace("\\", "/"),
                "mask_source": str(mask_path.relative_to(PROJECT_ROOT)).replace("\\", "/"),
                "output": str(output_path.relative_to(PROJECT_ROOT)).replace("\\", "/"),
                "size": list(color.size),
                "alpha_extrema": list(baked_alpha.getextrema()),
                "output_sha256": sha256(output_path),
                "alpha_formula": "color.a * saturate(mask.r * 2.0) * 1.0",
                "unity_color_space": "Gamma",
            }
        )

    METADATA_PATH.write_text(
        json.dumps({"records": records}, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    print("prepared_runtime_interior_baked_alpha={0}".format(len(records)))


if __name__ == "__main__":
    prepare()
