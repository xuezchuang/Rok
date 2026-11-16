"""Extract exact Unity city-scene visual chains for the construction and server city presenters.

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
# Walls, towers and perimeter decorations are multi-actor city layout prefabs.
CITY_LAYOUT_TYPES = {6, 8, 99}


def texture_index():
    """Unity's SpriteRenderer and MaskSprite GUIDs are the source of truth."""
    result = {}
    for meta in sorted((ASSETS / "BundleAssets/Map/Building").rglob("*.png.meta")):
        path = meta.with_suffix("")
        key = guid(path)
        if key in result:
            raise ValueError(f"Duplicate city texture GUID: {result[key]}, {path}")
        result[key] = path
    return result


def build(write=True):
    catalog = json.loads(MODEL_CATALOG.read_text(encoding="utf-8"))
    assert catalog["schema"] == "rok.city_construction_sources.v1"
    source_models = {int(row["ID"]): row for row in rows(MODEL_CSV)}
    textures = texture_index()
    night_material = MATERIAL.with_name("city_building_night.mat")
    material_guids = {guid(MATERIAL), guid(night_material)}
    types = {int(row["type"]): row for row in rows(TYPE_CSV)}
    levels = {(int(row["type"]), int(row["level"])): row for row in rows(LEVEL_CSV)}
    common = material_contract()
    paths = {MODEL_CATALOG, MODEL_CSV, TYPE_CSV, LEVEL_CSV, MATERIAL, SHADER, MASK_SCRIPT, PARSER}
    models, unavailable = [], []
    model_groups = {}
    for row in catalog["models"]:
        model_groups.setdefault(row["model_id"], []).append(row)
    for model_id, variants in sorted(model_groups.items()):
        entry = next((row for row in variants if str(row["type"]) == model_id.split("_")[1]), variants[0])
        key = (entry["type"], entry["civilization"], entry["age"])
        raw = source_models[entry["id"]]
        assert raw["modelId"] == model_id
        if not entry["scene_prefab"] or key[0] in CITY_LAYOUT_TYPES:
            unavailable.append({"model": model_id, "keys": len(variants),
                                "reason": "missing_prefab" if not entry["scene_prefab"] else "city_layout"})
            continue
        prefab = resolve(entry["scene_prefab"])
        nodes = prefab_nodes(prefab)
        main = next((node for node in nodes if node["name"] == "sprite"), None)
        if main is None:
            unavailable.append({"model": model_id, "keys": len(variants), "reason": "nested_prefab"})
            continue
        root, = (node for node in nodes if node["transform"]["parent_transform_file_id"] == 0)
        sprite = textures[main["renderer"]["sprite_guid"]]
        mask = textures[main["mask_sprite"]["mask_sprite_guid"]]
        expected = (prefab, sprite, mask, prefab.with_name(prefab.name + ".meta"),
                    sprite.with_name(sprite.name + ".meta"), mask.with_name(mask.name + ".meta"))
        missing = [rel(path) for path in expected if not path.is_file()]
        if missing:
            raise ValueError(f"Missing city visual source for {key}: {missing}")
        paths.update(expected)
        floor = next(node for node in nodes if node["name"] == "FloorTile_delete")
        board = next(node for node in nodes if node["name"] == "UpgradeBoard")
        assert root["active"] and main["active"] and not floor["active"], model_id
        assert main["transform"]["parent_transform_file_id"] == root["transform"]["file_id"], model_id
        assert main["renderer"]["material_guid"] in material_guids, model_id
        assert main["mask_sprite"]["script_guid"] == guid(MASK_SCRIPT), model_id
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
                                                      for r in variants],
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
    paths.add(night_material)
    for path in (MATERIAL, SHADER, MASK_SCRIPT, night_material):
        paths.add(path.with_name(path.name + ".meta"))
    output = {"schema": "rok.city_building_visual_sources.v1",
              "qualification": "Unity city building body extraction across all civilization/age rows. The city light manager supplies the day material.",
              "sources": {rel(path): sha(path) for path in sorted(paths)},
              "material": common, "models": models, "unavailable": unavailable}
    if write:
        OUT.parent.mkdir(parents=True, exist_ok=True)
        OUT.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {OUT}: {len(models)} exact city building models")
    return output


if __name__ == "__main__":
    build()
