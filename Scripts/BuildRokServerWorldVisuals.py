"""Crop source MapIcon sprites for live AOI types without migrated close models.

The PNGs are deliberately icon-only at every LOD. The manifest pins each
config row's model and mapIcon so runtime cannot reuse a plausible wrong icon.
"""

import hashlib
import json
from pathlib import Path

from BuildRokUnityWorldEntityPopulation import crop_unity_sprite
from rok_data_paths import DATA_ROOT, PROJECT_ROOT


OUTPUT = DATA_ROOT / "ServerWorldVisuals"
MANIFEST = DATA_ROOT / "server_world_visuals.json"
ATLAS_DIR = PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map"
# Full close formations for 1014/1016 are already in the population manifest.
FULL_FORMATION_IDS = {1014, 1016}
STRONGHOLD_FAMILIES = {
    10: (25, "checkpoint", "build_Checkpoint_Big", "map_icon[map_icon_build1]"),
    1: (26, "holy_land", "build_HolyLand1_Big", "map_icon[map_icon_build2]"),
    2: (26, "holy_land", "build_HolyLand2_Big", "map_icon[map_icon_build3]"),
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source(path):
    return {
        "path": path.relative_to(PROJECT_ROOT).as_posix(),
        "sha256": sha256(path),
    }


def config_rows(name, key="ID"):
    path = DATA_ROOT / "ConfigTables" / f"{name}.json"
    payload = json.loads(path.read_text(encoding="utf-8"))
    assert payload["table_name"] == name
    return path, {int(row[key]): row for row in payload["rows"]}


def main():
    monster_path, monsters = config_rows("Monster")
    resource_path, resources = config_rows("ResourceGatherType")
    alliance_path, alliances = config_rows("AllianceBuildingType", "type")
    stronghold_path, strongholds = config_rows("StrongHoldType")
    specs = []
    for config_id, row in sorted(monsters.items()):
        if config_id in FULL_FORMATION_IDS or row["type"] != 1:
            continue
        if row["modelId"] != f"BarbarianFormation_{row['level']}" or row["mapIcon"] != "map_icon[map_icon_1001]":
            continue
        specs.append((2, config_id, 0, "barbarian", row, row["mapIcon"], "Monster"))
    for config_id, row in sorted(monsters.items()):
        if row["type"] != 2 or row["modelId"] != "build_BarbarianCitadel_Big" or row["mapIcon"] != "ui_map[img_map_war]":
            continue
        specs.append((28, config_id, 0, "barbarian_citadel", row, row["mapIcon"], "Monster"))
    gold_baseline = resources[40003]
    gold_visual_fields = ("modelId", "modelIdLod", "modelIdLod2", "mapIconRes")
    for config_id, row in sorted(resources.items()):
        if row["type"] != 4 or any(row[field] != gold_baseline[field] for field in gold_visual_fields):
            continue
        specs.append((7, config_id, 0, "gold", row, row["mapIconRes"], "ResourceGatherType"))
    # AllianceProxy.typeMap[16..19] = AllianceBuildingType 4..7.
    for object_type, config_id, kind in (
        (16, 4, "guild_food"), (17, 5, "guild_wood"),
        (18, 6, "guild_stone"), (19, 7, "guild_gold"),
    ):
        alliance = alliances[config_id]
        assert alliance["modelId"] == f"build_alliance_{999 + config_id}"
        assert alliance["iconImg"] == f"map_icon[map_icon_aliCur{config_id - 3}]"
        specs.append((object_type, config_id, 0, kind, alliance, alliance["iconImg"], "AllianceBuildingType"))
    for group, (object_type, kind, model, icon) in STRONGHOLD_FAMILIES.items():
        family = [row for row in strongholds.values() if row["group"] == group]
        assert family and all(row["modelId"] == model and row["iconImg"] == icon for row in family)
        specs.append((object_type, 0, group, kind, family[0], icon, "StrongHoldType"))

    OUTPUT.mkdir(parents=True, exist_ok=True)
    crops = {}
    entries = []
    for object_type, config_id, source_group, kind, row, icon_ref, source_table in specs:
        atlas_name, sprite_name = icon_ref[:-1].split("[", 1)
        assert atlas_name in ("map_icon", "ui_map")
        if sprite_name not in crops:
            atlas = ATLAS_DIR / f"{atlas_name}.png"
            meta = Path(str(atlas) + ".meta")
            image, sprite = crop_unity_sprite(atlas, meta, sprite_name)
            output = OUTPUT / f"{sprite_name}.png"
            image.save(output)
            crops[sprite_name] = {
                "path": output.relative_to(PROJECT_ROOT).as_posix(),
                "sha256": sha256(output),
                "source_atlas": source(atlas),
                "source_meta": source(meta),
                "sprite": sprite,
            }
        entries.append({
            "object_type": object_type,
            "config_id": config_id,
            "source_group": source_group,
            "kind": kind,
            "source_table": source_table,
            "model_id": row["modelId"],
            "map_icon": icon_ref,
            "sprite": sprite_name,
            "path": crops[sprite_name]["path"],
            "detail_world_height_cm": 900.0,
            "far_world_height_cm": 900.0 if object_type == 28 else 1815.0,
            "presentation": "source_map_icon_at_all_lods_no_close_model",
        })
    payload = {
        "schema": "rok.server_world_visuals.v1",
        "config_sources": {
            "Monster": source(monster_path),
            "ResourceGatherType": source(resource_path),
            "AllianceBuildingType": source(alliance_path),
            "StrongHoldType": source(stronghold_path),
        },
        "crops": crops,
        "entries": entries,
    }
    MANIFEST.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {len(entries)} icon-only entries and {len(crops)} exact atlas crops")


if __name__ == "__main__":
    main()
