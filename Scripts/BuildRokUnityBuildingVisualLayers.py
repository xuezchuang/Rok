import json
from pathlib import Path

from PIL import Image, ImageFilter
from rok_data_paths import DATA_ROOT, label


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
LAYER_OUTPUT_DIR = OUTPUT_DIR / "CityBuildingVisualLayers"
OUTPUT_JSON = OUTPUT_DIR / "unity_building_visual_layers.json"
OUTPUT_MD = OUTPUT_DIR / "unity_building_visual_layers.md"
INIT_BUILDING_RESOLUTION_PATH = DATA_ROOT / "unity_gameapp_city_init_building_resolution.json"

UNITY_BUILDING_ROOT = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "BundleAssets" / "Map" / "Building" / "Building_use" / "shared_building" / "Building_101" / "city_building_1"


def rel(path):
    return label(path)


def load_json(path):
    if not path.exists():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def source_paths(sprite_name):
    color = UNITY_BUILDING_ROOT / f"{sprite_name}.png"
    mask = UNITY_BUILDING_ROOT / f"{sprite_name}_mask.png"
    return color, mask


def alpha_from_sources(color_path, mask_path):
    if mask_path.exists():
        mask = Image.open(mask_path).convert("RGBA")
        red, _green, _blue, alpha = mask.split()
        source_alpha = red.point(lambda value: 255 if value > 6 else 0)
        if source_alpha.getbbox():
            return source_alpha
        return alpha

    color = Image.open(color_path).convert("RGBA")
    return color.split()[-1]


def write_shadow(sprite_name, color_path, mask_path):
    alpha = alpha_from_sources(color_path, mask_path)
    shadow_alpha = alpha.filter(ImageFilter.GaussianBlur(radius=8)).point(lambda value: int(value * 0.34))
    shadow = Image.new("RGBA", alpha.size, (12, 14, 8, 0))
    shadow.putalpha(shadow_alpha)

    output = LAYER_OUTPUT_DIR / f"{sprite_name}_shadow.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    shadow.save(output)
    return output


def build_manifest():
    resolution = load_json(INIT_BUILDING_RESOLUTION_PATH)
    records = resolution.get("records", [])
    layers = []
    unique_generated = {}

    for record in records:
        sprite_name = record.get("sprite_name", "")
        if not sprite_name:
            continue
        color_path, mask_path = source_paths(sprite_name)
        if not color_path.exists():
            layers.append({
                "init_id": record.get("init_id", 0),
                "building_type": record.get("building_type", 0),
                "sprite_name": sprite_name,
                "source_color": rel(color_path) if color_path.exists() else str(color_path).replace("\\", "/"),
                "source_mask": rel(mask_path) if mask_path.exists() else "",
                "shadow_layer": "",
                "has_color": color_path.exists(),
                "has_mask": mask_path.exists(),
                "generated": False,
            })
            continue

        if sprite_name not in unique_generated:
            shadow_path = write_shadow(sprite_name, color_path, mask_path)
            unique_generated[sprite_name] = shadow_path
        else:
            shadow_path = unique_generated[sprite_name]

        layers.append({
            "init_id": record.get("init_id", 0),
            "building_type": record.get("building_type", 0),
            "grid_x": record.get("posX", 0),
            "grid_y": record.get("posY", 0),
            "model_id": record.get("model_id", ""),
            "sprite_name": sprite_name,
            "resolution": record.get("resolution", ""),
            "source_color": rel(color_path),
            "source_mask": rel(mask_path) if mask_path.exists() else "",
            "shadow_layer": rel(shadow_path),
            "has_color": True,
            "has_mask": mask_path.exists(),
            "generated": True,
        })

    manifest = {
        "schema": "rok.unity_building_visual_layers.v1",
        "source_resolution": rel(INIT_BUILDING_RESOLUTION_PATH),
        "ue_target_level": "/Game/Maps/L_RokUnityGameAppCity",
        "layer_role": "building_shadow",
        "record_count": len(layers),
        "generated_shadow_count": sum(1 for item in layers if item.get("generated")),
        "mask_backed_count": sum(1 for item in layers if item.get("has_mask")),
        "unique_shadow_count": len(unique_generated),
        "layers": layers,
        "notes": [
            "Shadow layers are derived from Unity building mask red channel when available, falling back to source PNG alpha.",
            "These are source-derived per-building support layers, not final Unity shader reproduction.",
        ],
    }
    return manifest


def write_markdown(manifest):
    lines = [
        "# Rok Unity Building Visual Layers",
        "",
        f"- Schema: `{manifest['schema']}`",
        f"- Records: {manifest['record_count']}",
        f"- Generated shadows: {manifest['generated_shadow_count']}",
        f"- Mask-backed records: {manifest['mask_backed_count']}",
        f"- Unique shadow textures: {manifest['unique_shadow_count']}",
        "",
        "| Init | Type | Sprite | Mask | Shadow |",
        "| ---: | ---: | --- | --- | --- |",
    ]
    for item in manifest["layers"]:
        lines.append(
            "| {init_id} | {building_type} | `{sprite_name}` | {has_mask} | `{shadow_layer}` |".format(**item)
        )
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    LAYER_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    manifest = build_manifest()
    OUTPUT_JSON.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    write_markdown(manifest)
    print(
        "Rok Unity building visual layers generated: "
        f"records={manifest['record_count']} "
        f"shadows={manifest['generated_shadow_count']} "
        f"unique={manifest['unique_shadow_count']}"
    )


if __name__ == "__main__":
    main()
