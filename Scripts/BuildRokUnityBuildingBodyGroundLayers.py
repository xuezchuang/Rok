import json
from pathlib import Path

from PIL import Image, ImageFilter


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
LAYER_OUTPUT_DIR = OUTPUT_DIR / "CityBuildingBodyGroundLayers"
SOURCE_MANIFEST_PATH = OUTPUT_DIR / "unity_building_visual_layers.json"
OUTPUT_JSON = OUTPUT_DIR / "unity_building_body_ground_layers.json"
OUTPUT_MD = OUTPUT_DIR / "unity_building_body_ground_layers.md"


def rel(path):
    return str(path.relative_to(PROJECT_ROOT)).replace("\\", "/")


def load_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def mask_alpha(mask_path, color):
    if mask_path and Path(mask_path).exists():
        mask = Image.open(mask_path).convert("RGBA")
        red, _green, _blue, alpha = mask.split()
        derived = red.point(lambda value: 255 if value > 6 else 0)
        if derived.getbbox():
            return derived
        return alpha
    return color.split()[-1]


def estimate_ground_alpha(color, alpha):
    rgba = color.convert("RGBA")
    pixels = rgba.load()
    alpha_pixels = alpha.load()
    out = Image.new("L", rgba.size, 0)
    out_pixels = out.load()
    width, height = rgba.size
    for y in range(height):
        for x in range(width):
            if alpha_pixels[x, y] <= 0:
                continue
            r, g, b, _a = pixels[x, y]
            if g >= 85 and r >= 75 and b <= 150 and g >= b + 16:
                out_pixels[x, y] = min(210, alpha_pixels[x, y])
    return out.filter(ImageFilter.GaussianBlur(radius=1))


def write_layer_images(layer):
    color_path = PROJECT_ROOT / layer["source_color"]
    mask_path = PROJECT_ROOT / layer["source_mask"] if layer.get("source_mask") else None
    color = Image.open(color_path).convert("RGBA")
    alpha = mask_alpha(mask_path, color)

    body = color.copy()
    body.putalpha(alpha)
    ground_alpha = estimate_ground_alpha(color, alpha)
    ground = color.copy()
    ground.putalpha(ground_alpha)

    sprite_name = layer["sprite_name"]
    body_path = LAYER_OUTPUT_DIR / f"{sprite_name}_body_clean.png"
    ground_path = LAYER_OUTPUT_DIR / f"{sprite_name}_ground_patch.png"
    body_path.parent.mkdir(parents=True, exist_ok=True)
    body.save(body_path)
    ground.save(ground_path)
    return body_path, ground_path, bool(ground_alpha.getbbox())


def build_manifest():
    source = load_json(SOURCE_MANIFEST_PATH)
    output_layers = []
    generated_by_sprite = {}

    for item in source.get("layers", []):
        if not item.get("generated"):
            continue
        sprite_name = item["sprite_name"]
        if sprite_name not in generated_by_sprite:
            body_path, ground_path, has_ground_patch = write_layer_images(item)
            generated_by_sprite[sprite_name] = {
                "body_layer": rel(body_path),
                "ground_patch_layer": rel(ground_path),
                "has_ground_patch": has_ground_patch,
            }
        generated = generated_by_sprite[sprite_name]
        output_layers.append({
            "init_id": item["init_id"],
            "building_type": item["building_type"],
            "grid_x": item.get("grid_x", 0),
            "grid_y": item.get("grid_y", 0),
            "sprite_name": sprite_name,
            "source_color": item["source_color"],
            "source_mask": item.get("source_mask", ""),
            "shadow_layer": item.get("shadow_layer", ""),
            "body_layer": generated["body_layer"],
            "ground_patch_layer": generated["ground_patch_layer"],
            "has_ground_patch": generated["has_ground_patch"],
        })

    return {
        "schema": "rok.unity_building_body_ground_layers.v1",
        "source_manifest": rel(SOURCE_MANIFEST_PATH),
        "ue_target_level": "/Game/Maps/L_RokUnityGameAppCity",
        "record_count": len(output_layers),
        "unique_sprite_count": len(generated_by_sprite),
        "body_layer_count": len(output_layers),
        "ground_patch_count": sum(1 for item in output_layers if item["has_ground_patch"]),
        "layers": output_layers,
        "notes": [
            "Body layers are Unity sprite color clipped by mask red channel when available.",
            "Ground patch layers are color-estimated support layers inside the Unity mask and remain diagnostic until material/shader rules are migrated.",
        ],
    }


def write_markdown(manifest):
    lines = [
        "# Rok Unity Building Body and Ground Layers",
        "",
        f"- Schema: `{manifest['schema']}`",
        f"- Records: {manifest['record_count']}",
        f"- Unique sprites: {manifest['unique_sprite_count']}",
        f"- Body layers: {manifest['body_layer_count']}",
        f"- Ground patches: {manifest['ground_patch_count']}",
        "",
        "| Init | Type | Sprite | Body | Ground patch |",
        "| ---: | ---: | --- | --- | --- |",
    ]
    for item in manifest["layers"]:
        lines.append(
            "| {init_id} | {building_type} | `{sprite_name}` | `{body_layer}` | `{ground_patch_layer}` |".format(**item)
        )
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    LAYER_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    manifest = build_manifest()
    OUTPUT_JSON.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    write_markdown(manifest)
    print(
        "Rok Unity building body/ground layers generated: "
        f"records={manifest['record_count']} body={manifest['body_layer_count']} ground={manifest['ground_patch_count']}"
    )


if __name__ == "__main__":
    main()
