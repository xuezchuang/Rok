import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
OUTPUT_JSON = OUTPUT_DIR / "unity_city_visual_layer_stack.json"
OUTPUT_MD = OUTPUT_DIR / "unity_city_visual_layer_stack.md"

LAYER_DEFS = [
    {
        "id": "terrain_background",
        "role": "terrain",
        "source": "Saved/RokUnityPlacement/clean/UnityCityBackground_Terrain_grass_08_3g2.png",
        "width": 3300.0,
        "height": 2450.0,
        "x": 0.0,
        "y": -40.0,
        "z": -22.0,
        "blend": "opaque",
    },
    {
        "id": "grassland_blend",
        "role": "terrain_blend",
        "source": "Saved/RokUnityPlacement/clean/UnityGrassGround_I_TYPE_Grass_01.png",
        "width": 3180.0,
        "height": 2320.0,
        "x": 0.0,
        "y": -40.0,
        "z": -18.0,
        "blend": "opaque",
    },
    {
        "id": "city_wall_ground",
        "role": "city_ground",
        "source": "Saved/RokUnityPlacement/clean/UnityCityGround_build_ground_6_101_1.png",
        "width": 3000.0,
        "height": 2150.0,
        "x": 0.0,
        "y": -40.0,
        "z": -14.0,
        "blend": "opaque",
    },
    {
        "id": "city_surface_soft",
        "role": "city_surface",
        "source": "Saved/RokUnityPlacement/clean/UnityCitySurface_map_incity4.png",
        "width": 2540.0,
        "height": 1680.0,
        "x": 0.0,
        "y": -160.0,
        "z": -10.0,
        "blend": "alpha",
    },
    {
        "id": "city_road_preview",
        "role": "road",
        "source": "Saved/RokUnityPlacement/clean/UnityCityRoad_dirtRoad01.png",
        "width": 1320.0,
        "height": 420.0,
        "x": -120.0,
        "y": -360.0,
        "z": -6.0,
        "blend": "alpha",
        "yaw": 0.0,
    },
]


def exists(relative_path):
    return (PROJECT_ROOT / relative_path).exists()


def build_manifest():
    layers = []
    for sort_index, layer in enumerate(LAYER_DEFS):
        enriched = dict(layer)
        enriched["exists"] = exists(layer["source"])
        enriched["sort_index"] = sort_index
        enriched["ue_actor_tag"] = f"RokUnityCityVisualLayer_{layer['id']}"
        layers.append(enriched)

    return {
        "schema": "rok.unity_city_visual_layer_stack.v1",
        "ue_target_level": "/Game/Maps/L_RokUnityGameAppCity",
        "layers": layers,
        "layer_count": len(layers),
        "existing_layer_count": sum(1 for layer in layers if layer["exists"]),
        "required_roles": ["terrain", "terrain_blend", "city_ground", "city_surface", "road"],
        "notes": [
            "Layer dimensions are UE staging values for the first source-driven visual foundation pass.",
            "Sources are Unity-derived baked diagnostics and must be replaced by tighter source extraction when mesh/mask rules are migrated.",
        ],
    }


def write_markdown(manifest):
    lines = [
        "# Rok Unity City Visual Layer Stack",
        "",
        f"- Schema: `{manifest['schema']}`",
        f"- UE target level: `{manifest['ue_target_level']}`",
        f"- Layers: {manifest['layer_count']}",
        f"- Existing layers: {manifest['existing_layer_count']}",
        "",
        "| Layer | Role | Source | Exists | Size | Z | Blend |",
        "| --- | --- | --- | --- | ---: | ---: | --- |",
    ]
    for layer in manifest["layers"]:
        lines.append(
            "| {id} | {role} | `{source}` | {exists} | {width:.0f}x{height:.0f} | {z:.1f} | {blend} |".format(**layer)
        )
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    manifest = build_manifest()
    OUTPUT_JSON.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    write_markdown(manifest)
    print(
        "Rok Unity city visual layer stack generated: "
        f"layers={manifest['layer_count']} existing={manifest['existing_layer_count']}"
    )


if __name__ == "__main__":
    main()
