"""Extract exact Unity city-scene visual chains for locally constructible non-resource buildings.

Same source chain as the Farm/Sawmill slice (root -> `sprite` SpriteRenderer + MaskSprite,
map_building.mat, map_building_with_mask-5), kept in its own catalog so the pinned
Farm/Sawmill catalog and its evidence stay byte-identical.
"""
import json

from BuildRokAcademyVisualSource import ASSETS, ROOT, guid, image, importer
from BuildRokCityResourceBuildingVisualSources import (
    LEVEL_CSV, MASK_SCRIPT, MATERIAL, MODEL_CATALOG, MODEL_CSV, PARSER, SHADER, TYPE_CSV,
    material_contract, meta_platform, prefab_nodes, rel, rows, sha,
)
from rok_data_paths import resolve


OUT = ROOT / "Saved/RokUnityMigration/CityBuildingVisualSources/catalog.json"
# (type, civilization, age). 18 = 统帅厅 (commander hall), civilization 101 age 1 model.
KEYS = ((18, 101, 1),)


def build(write=True):
    catalog = json.loads(MODEL_CATALOG.read_text(encoding="utf-8"))
    assert catalog["schema"] == "rok.city_construction_sources.v1"
    source_models = rows(MODEL_CSV)
    types = {int(row["type"]): row for row in rows(TYPE_CSV)}
    levels = {(int(row["type"]), int(row["level"])): row for row in rows(LEVEL_CSV)}
    common = material_contract()
    paths = {MODEL_CATALOG, MODEL_CSV, TYPE_CSV, LEVEL_CSV, MATERIAL, SHADER, MASK_SCRIPT, PARSER}
    models = []
    for key in KEYS:
        matches = [row for row in catalog["models"]
                   if (row["type"], row["civilization"], row["age"]) == key]
        assert len(matches) == 1 and matches[0]["has_building_type_config"], key
        entry = matches[0]
        raw = [row for row in source_models
               if (int(row["type"]), int(row["civType"]), int(row["age"])) == key]
        assert len(raw) == 1 and int(raw[0]["ID"]) == entry["id"] and raw[0]["modelId"] == entry["model_id"]
        model_id = entry["model_id"]
        prefab = resolve(entry["scene_prefab"])
        sprite = ASSETS / f"BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/{model_id}.png"
        mask = sprite.with_name(f"{model_id}_mask.png")
        expected = (prefab, sprite, mask, prefab.with_name(prefab.name + ".meta"),
                    sprite.with_name(sprite.name + ".meta"), mask.with_name(mask.name + ".meta"))
        missing = [rel(path) for path in expected if not path.is_file()]
        if missing:
            raise ValueError(f"Missing city visual source for {key}: {missing}")
        paths.update(expected)
        nodes = prefab_nodes(prefab)
        root = next(node for node in nodes if node["name"] == model_id)
        main = next(node for node in nodes if node["name"] == "sprite")
        floor = next(node for node in nodes if node["name"] == "FloorTile_delete")
        board = next(node for node in nodes if node["name"] == "UpgradeBoard")
        assert root["active"] and main["active"] and not floor["active"] and not board["active"]
        assert root["transform"]["parent_transform_file_id"] == 0
        assert main["transform"]["parent_transform_file_id"] == root["transform"]["file_id"]
        assert main["renderer"]["sprite_guid"] == guid(sprite)
        assert main["renderer"]["material_guid"] == guid(MATERIAL)
        assert main["mask_sprite"]["mask_sprite_guid"] == guid(mask)
        assert main["mask_sprite"]["script_guid"] == guid(MASK_SCRIPT)
        type_row, level_row = types[key[0]], levels[(key[0], 1)]
        assert (int(type_row["width"]), int(type_row["length"])) == (
            int(level_row["width"]), int(level_row["length"]))
        prerequisites = [{"type": int(level_row[f"reqType{i}"]), "level": int(level_row[f"reqLevel{i}"])}
                         for i in range(1, 4)]
        assert all((req["type"] == 0) == (req["level"] == 0) for req in prerequisites)
        models.append({"key": {"type": key[0], "civilization": key[1], "age": key[2]},
                       "model_data_id": entry["id"], "model_id": model_id,
                       "source_model_row": entry["source_row"],
                       "matching_model_keys": sorted([{"id": r["id"], "type": r["type"],
                                                       "civilization": r["civilization"], "age": r["age"]}
                                                      for r in catalog["models"] if r["model_id"] == model_id],
                                                     key=lambda item: (item["type"], item["civilization"], item["age"])),
                       "prefab": {"path": rel(prefab), "guid": guid(prefab), "nodes": nodes,
                                  "root_game_object_file_id": root["file_id"],
                                  "main_sprite_game_object_file_id": main["file_id"],
                                  "optional_floor_active": floor["active"],
                                  "upgrade_board_active": board["active"]},
                       "sprite": {**image(sprite), "importer": importer(sprite),
                                  "platform": meta_platform(sprite)},
                       "mask": {**image(mask), "importer": importer(mask),
                                "platform": meta_platform(mask)},
                       "mask_binding": {"script_guid": guid(MASK_SCRIPT), "component_file_id": main["mask_sprite"]["file_id"],
                                        "property": "_Mask", "mechanism": "MaterialPropertyBlock.SetTexture(_Mask, m_mask_sprite.texture)"},
                       "footprint": {"width": int(type_row["width"]), "length": int(type_row["length"])},
                       "level_1": {"level_data_id": int(level_row["ID"]),
                                   "cost": {name: int(level_row[name]) for name in ("food", "wood", "stone", "coin", "denar")},
                                   "duration_seconds": int(level_row["buildingTime"]),
                                   "power": int(level_row["power"]),
                                   "prerequisites": prerequisites}})
    for path in (MATERIAL, SHADER, MASK_SCRIPT):
        paths.add(path.with_name(path.name + ".meta"))
    output = {"schema": "rok.city_building_visual_sources.v1",
              "qualification": "Unity scene prefab source extraction only; civilization 101/age 1 is an explicit local visual variant, not account identity or UE visual acceptance.",
              "sources": {rel(path): sha(path) for path in sorted(paths)},
              "material": common, "models": models}
    if write:
        OUT.parent.mkdir(parents=True, exist_ok=True)
        OUT.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {OUT}: {len(models)} exact city building models")
    return output


if __name__ == "__main__":
    build()
