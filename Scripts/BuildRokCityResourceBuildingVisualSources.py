"""Extract exact Unity city-scene visual chains for the local Farm/Sawmill models."""
import csv
import hashlib
import json
import re
from pathlib import Path

from BuildRokAcademyVisualSource import (
    ROOT, ASSETS, image, importer, guid, field, vec, file_id, file_guid,
    yaml_objects, shader_formula,
)
from rok_data_paths import DATA_ROOT, label, resolve


CSV_ROOT = ROOT / "ref/config-tables/编译客户端配置/CSV"
MODEL_CATALOG = DATA_ROOT / "CityConstructionSources/catalog.json"
MODEL_CSV = CSV_ROOT / "BuildingModelData.csv"
TYPE_CSV = CSV_ROOT / "BuildingTypeConfig.csv"
LEVEL_CSV = CSV_ROOT / "BuildingLevelData.csv"
PRODUCE_CSV = CSV_ROOT / "BuildingResourcesProduce.csv"
MATERIAL = ASSETS / "BundleAssets/Scene/City/city_function/map_building.mat"
SHADER = ASSETS / "BundleAssets/Map/Building/map_building_with_mask-5.shader"
MASK_SCRIPT = ASSETS / "Scripts/Client/Compoment/MaskSprite.cs"
PARSER = ROOT / "Scripts/BuildRokAcademyVisualSource.py"
OUT = ROOT / "Saved/RokUnityMigration/CityResourceBuildingVisualSources/catalog.json"
KEYS = ((2, 101, 1), (3, 101, 1))


def rel(path):
    return label(path)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        result = list(csv.DictReader(stream))
    assert result and result[0][next(iter(result[0]))] == "int", path
    return result[1:]


def meta_platform(path):
    text = Path(str(path) + ".meta").read_text(encoding="utf-8-sig")
    settings = []
    for block in re.findall(r"(?ms)^  - serializedVersion: 3\s*\n(.*?)(?=^  - serializedVersion: 3|^  spriteSheet:)", text):
        def number(key):
            match = re.search(rf"(?m)^    {key}: (-?\d+)$", block)
            assert match, (path, key)
            return int(match[1])
        target = re.search(r"(?m)^    buildTarget: (\S+)$", block)
        assert target, path
        settings.append({"build_target": target[1], "max_texture_size": number("maxTextureSize"),
                         "resize_algorithm": number("resizeAlgorithm"), "texture_format": number("textureFormat"),
                         "texture_compression": number("textureCompression"), "overridden": number("overridden")})
    assert settings, path
    npot = re.search(r"(?m)^  nPOTScale: (\d+)$", text)
    assert npot
    return {"npot_scale": int(npot[1]), "platform_settings": settings}


def prefab_nodes(path):
    objects = yaml_objects(path)
    game_objects, transforms, renderers, masks = {}, {}, {}, {}
    for class_id, fid, body in objects:
        if class_id == 1:
            game_objects[fid] = {"file_id": fid, "name": field(body, "m_Name"),
                                 "active": field(body, "m_IsActive") == "1",
                                 "layer": int(field(body, "m_Layer"))}
        elif class_id == 4:
            go = file_id(body, "m_GameObject")
            assert go not in transforms
            transforms[go] = {"file_id": fid, "parent_transform_file_id": file_id(body, "m_Father"),
                              "root_order": int(field(body, "m_RootOrder")),
                              "local_position": vec(body, "m_LocalPosition"),
                              "local_rotation": vec(body, "m_LocalRotation"),
                              "local_scale": vec(body, "m_LocalScale"),
                              "local_euler_hint": vec(body, "m_LocalEulerAnglesHint")}
        elif class_id == 212:
            go = file_id(body, "m_GameObject")
            material = re.search(r"(?m)^  - \{fileID: 2100000, guid: ([0-9a-f]{32}), type: 2\}$", body)
            sprite = re.search(r"m_Sprite: \{fileID: (-?\d+)(?:, guid: ([0-9a-f]{32}))?", body)
            assert material and sprite and go not in renderers
            renderers[go] = {"file_id": fid, "enabled": field(body, "m_Enabled") == "1",
                             "material_guid": material[1], "sprite_file_id": int(sprite[1]),
                             "sprite_guid": sprite[2], "color": vec(body, "m_Color"),
                             "flip_x": field(body, "m_FlipX") == "1", "flip_y": field(body, "m_FlipY") == "1",
                             "sorting_layer_id": int(field(body, "m_SortingLayerID")),
                             "sorting_layer": int(field(body, "m_SortingLayer")),
                             "sorting_order": int(field(body, "m_SortingOrder"))}
        elif class_id == 114 and "m_mask_sprite:" in body:
            go = file_id(body, "m_GameObject")
            sprite = re.search(r"m_mask_sprite: \{fileID: -?\d+(?:, guid: ([0-9a-f]{32}))?", body)
            assert sprite and go not in masks
            masks[go] = {"file_id": fid, "script_guid": file_guid(body, "m_Script"),
                         "mask_sprite_file_id": file_id(body, "m_mask_sprite"),
                         "mask_sprite_guid": sprite[1]}
    assert len(game_objects) == len(transforms)
    return [{**node, "transform": transforms[go], "renderer": renderers.get(go),
             "mask_sprite": masks.get(go)} for go, node in game_objects.items()]


def material_contract():
    material = MATERIAL.read_text(encoding="utf-8-sig")
    shader = SHADER.read_text(encoding="utf-8-sig")
    assert file_guid(material, "m_Shader") == guid(SHADER)
    assert 'propertyBlock.SetTexture("_Mask", m_mask_sprite.texture)' in MASK_SCRIPT.read_text(encoding="latin1")
    def color(key):
        match = re.search(rf"(?m)^    - {re.escape(key)}: \{{([^\r\n]+)\}}$", material)
        assert match, key
        return {part.split(": ")[0]: float(part.split(": ")[1]) for part in match[1].split(", ")}
    transparency = re.search(r"(?m)^    - _CityTransparency: ([^\r\n]+)$", material)
    default_mask = re.search(r"(?s)- _Mask:\s*\n\s*m_Texture: \{fileID: 2800000, guid: ([0-9a-f]{32})", material)
    assert transparency and default_mask
    return {"path": rel(MATERIAL), "guid": guid(MATERIAL), "shader_path": rel(SHADER),
            "shader_guid": guid(SHADER), "color_1": color("_Color_1"), "color_2": color("_Color_2"),
            "city_transparency": float(transparency[1]), "default_mask_guid": default_mask[1],
            "shader_formula": shader_formula(shader)}


def build(write=True):
    catalog = json.loads(MODEL_CATALOG.read_text(encoding="utf-8"))
    assert catalog["schema"] == "rok.city_construction_sources.v1"
    source_models = rows(MODEL_CSV)
    types = {int(row["type"]): row for row in rows(TYPE_CSV)}
    levels = {(int(row["type"]), int(row["level"])): row for row in rows(LEVEL_CSV)}
    production = {(int(row["type"]), int(row["level"])): row for row in rows(PRODUCE_CSV)}
    common = material_contract()
    paths = {MODEL_CATALOG, MODEL_CSV, TYPE_CSV, LEVEL_CSV, PRODUCE_CSV,
             MATERIAL, SHADER, MASK_SCRIPT, PARSER}
    models = []
    for key in KEYS:
        matches = [row for row in catalog["models"] if
                   (row["type"], row["civilization"], row["age"]) == key]
        assert len(matches) == 1 and matches[0]["has_building_type_config"]
        entry = matches[0]
        raw = [row for row in source_models if
               (int(row["type"]), int(row["civType"]), int(row["age"])) == key]
        assert len(raw) == 1 and int(raw[0]["ID"]) == entry["id"] and raw[0]["modelId"] == entry["model_id"]
        model_id = entry["model_id"]
        prefab = resolve(entry["scene_prefab"])
        sprite = ASSETS / f"BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/{model_id}.png"
        mask = sprite.with_name(f"{model_id}_mask.png")
        expected = (prefab, sprite, mask, Path(str(prefab) + ".meta"),
                    Path(str(sprite) + ".meta"), Path(str(mask) + ".meta"))
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
        assert floor["renderer"]["sprite_file_id"] == 0
        assert board["renderer"]["sprite_guid"] != guid(sprite)
        type_row, level_row, produce_row = types[key[0]], levels[(key[0], 1)], production[(key[0], 1)]
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
                       "missing_sources": missing,
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
                                   "prerequisites": prerequisites,
                                   "produce_data_id": int(produce_row["ID"]),
                                   "produce_per_hour": int(produce_row["produceSpeed"]),
                                   "gather_min": int(produce_row["gatherMin"]),
                                   "gather_max": int(produce_row["gatherMax"])}})
    for path in (MATERIAL, SHADER, MASK_SCRIPT):
        paths.add(Path(str(path) + ".meta"))
    output = {"schema": "rok.city_resource_building_visual_sources.v1",
              "qualification": "Unity scene prefab source extraction only; civilization 101/age 1 is an explicit local visual variant, not account identity or UE visual acceptance.",
              "sources": {rel(path): sha(path) for path in sorted(paths)},
              "material": common, "models": models}
    if write:
        OUT.parent.mkdir(parents=True, exist_ok=True)
        OUT.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {OUT}: {len(models)} exact city models")
    return output


if __name__ == "__main__":
    build()
