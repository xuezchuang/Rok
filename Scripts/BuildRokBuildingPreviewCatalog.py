"""Extract UI building art and RectTransforms from BuildingModelData.imgRes."""

import csv
import hashlib
from io import BytesIO
import json
from pathlib import Path
import re

from PIL import Image

from rok_data_paths import DATA_ROOT

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "ref/client-unity/Assets"
PREFABS = ASSETS / "BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuilding"
SOURCE_TABLE = next((ROOT / "ref/config-tables").glob("*/CSV/BuildingModelData.csv"))
OUTPUT = DATA_ROOT / "BuildingSourcePreview"


def source_file(path):
    return path.relative_to(ROOT).as_posix()


def transform(block, name):
    value = re.search(rf"^  {name}: (\{{[^}}]+\}})$", block, re.M)[1]
    return [float(number) for number in re.findall(r": ([-\d.eE+]+)", value)]


def build():
    with SOURCE_TABLE.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    guid_paths = {}
    for meta in (ASSETS / "BundleAssets/Map/Building").rglob("*.png.meta"):
        with meta.open("rb") as stream:
            guid = re.search(rb"^guid: ([a-f0-9]{32})", stream.read(160), re.M)
        if guid:
            guid_paths[guid[1].decode("ascii")] = meta.with_suffix("")

    art, missing, variants = {}, [], []
    (OUTPUT / "Images").mkdir(parents=True, exist_ok=True)
    for row in rows:
        name = row["imgRes"]
        if not row["ID"].isdigit() or not name:
            continue
        prefab = PREFABS / (name + ".prefab")
        if not prefab.is_file():
            missing.append(row["ID"])
            continue
        if name not in art:
            text = prefab.read_text(encoding="utf-8-sig")
            blocks = re.split(r"^--- !u!\d+ &", text, flags=re.M)
            image_block, = [block for block in blocks if "m_Sprite: {fileID:" in block]
            guid = re.search(r"m_Sprite: \{fileID: 21300000, guid: ([a-f0-9]{32}),", image_block)[1]
            assert "m_Material: {fileID: 0}" in image_block and "m_Type: 0" in image_block, prefab
            assert transform(image_block, "m_Color") == [1, 1, 1, 1], prefab
            game_object = re.search(r"m_GameObject: \{fileID: (-?\d+)\}", image_block)[1]
            rect, = [block for block in blocks if "RectTransform:" in block
                     and f"m_GameObject: {{fileID: {game_object}}}" in block]
            for field in ("m_AnchorMin", "m_AnchorMax", "m_Pivot"):
                assert transform(rect, field) == [.5, .5], prefab
            assert transform(rect, "m_LocalRotation") == [0, 0, 0, 1], prefab
            width, height = transform(rect, "m_SizeDelta")
            sx, sy, _ = transform(rect, "m_LocalScale")
            x, y = transform(rect, "m_AnchoredPosition")

            png = guid_paths[guid]
            source_bytes = png.read_bytes()
            with Image.open(BytesIO(source_bytes)) as image:
                rgba = image.convert("RGBA")
                bounds = rgba.getchannel("A").point(lambda value: 255 if value > 4 else 0).getbbox()
                assert bounds, png
                w, h = image.size
                if source_bytes[24] == 16:
                    # Unity imports these PNGs as sRGB sprites. UE's transient
                    # 16-bit UNORM textures cannot sample through an sRGB view.
                    # Keep the encoded color values and alpha in RGBA8 instead.
                    rgba.info.clear()
                    encoded = BytesIO()
                    rgba.save(encoded, format="PNG")
                    output_bytes = encoded.getvalue()
                else:
                    output_bytes = source_bytes
            target = OUTPUT / "Images" / (guid + ".png")
            if not target.is_file() or target.read_bytes() != output_bytes:
                target.write_bytes(output_bytes)
            art[name] = {
                "image": target.relative_to(OUTPUT).as_posix(),
                "size": [round(width * sx, 6), round(height * sy, 6)],
                "offset": [x, -y],
                "visible": [bounds[0]/w, bounds[1]/h, bounds[2]/w, bounds[3]/h],
                "source_sprite": source_file(png),
                "source_prefab": source_file(prefab),
                "sha256": hashlib.sha256(source_bytes).hexdigest(),
                "image_sha256": hashlib.sha256(output_bytes).hexdigest(),
            }
        variants.append({
            "type": int(row["type"]), "country": int(row["civType"]), "age": int(row["age"]),
            "model": row["modelId"], "art": name,
        })

    initial_file = DATA_ROOT / "unity_initial_city_state.json"
    initial = json.loads(initial_file.read_bytes())["buildings"]
    initial_models = {
        str(row["init_id"]): row["visual"]["model_id"] for row in initial
        if row.get("visual", {}).get("model_id")
    }
    data = {
        "schema": "rok.building.preview_catalog.v1",
        "source_table": source_file(SOURCE_TABLE),
        "source_table_sha256": hashlib.sha256(SOURCE_TABLE.read_bytes()).hexdigest(),
        "art": art, "variants": variants, "initial_models": initial_models, "missing_source_rows": missing,
    }
    (OUTPUT / "catalog.json").write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"{len(art)} authored previews, {len(variants)} resolved rows, {len(missing)} rows missing a source prefab")


if __name__ == "__main__":
    build()
