"""Extract the immutable Unity build_13_101_2 city visual source contract."""
import csv
import hashlib
import json
import re
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "ref/client-unity/Assets"
MODEL = "build_13_101_2"
PREFAB = ASSETS / f"BundleAssets/Scene/City_building/city_building_2/{MODEL}.prefab"
SPRITE = ASSETS / f"BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_2/{MODEL}.png"
MASK = SPRITE.with_name(f"{MODEL}_mask.png")
MATERIAL = ASSETS / "BundleAssets/Scene/City/city_function/map_building.mat"
SHADER = ASSETS / "BundleAssets/Map/Building/map_building_with_mask-5.shader"
MASK_SCRIPT = ASSETS / "Scripts/Client/Compoment/MaskSprite.cs"
MODEL_CSV = ROOT / "ref/config-tables/编译客户端配置/CSV/BuildingModelData.csv"
MODEL_CATALOG = ROOT / "Saved/RokUnityMigration/CityConstructionSources/catalog.json"
OUT = ROOT / "Saved/RokUnityMigration/AcademyVisualSource/catalog.json"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rel(path):
    return path.relative_to(ROOT).as_posix()


def guid(path):
    match = re.search(r"(?m)^guid: ([0-9a-f]{32})$", Path(str(path) + ".meta").read_text(encoding="utf-8-sig"))
    assert match, path
    return match.group(1)


def field(body, key):
    match = re.search(rf"(?m)^  {re.escape(key)}: (.*)$", body)
    assert match, (key, body[:70])
    return match.group(1).strip()


def vec(body, key):
    match = re.fullmatch(r"\{(.+)\}", field(body, key))
    assert match, key
    return {part.strip().split(": ")[0]: float(part.strip().split(": ")[1]) for part in match.group(1).split(",")}


def file_id(body, key):
    match = re.search(r"fileID: (-?\d+)", field(body, key))
    assert match, key
    return int(match.group(1))


def file_guid(body, key):
    match = re.search(r"guid: ([0-9a-f]{32})", field(body, key))
    assert match, key
    return match.group(1)


def yaml_objects(path):
    parts = re.split(r"(?m)^--- !u!(\d+) &(-?\d+)\s*$", path.read_text(encoding="utf-8-sig"))
    return [(int(parts[i]), int(parts[i + 1]), parts[i + 2]) for i in range(1, len(parts), 3)]


def image(path):
    with Image.open(path) as source:
        rgba = source.convert("RGBA")
        bands = rgba.split()
        stats = {name: {"min": band.getextrema()[0], "max": band.getextrema()[1],
                        "nonzero_pixels": sum(band.histogram()[1:])}
                 for name, band in zip("rgba", bands)}
        return {"path": rel(path), "guid": guid(path), "size": list(source.size),
                "mode": source.mode, "channels": stats,
                "sha256": sha(path), "meta_sha256": sha(Path(str(path) + ".meta"))}


def importer(path):
    text = Path(str(path) + ".meta").read_text(encoding="utf-8-sig")
    def number(key):
        match = re.search(rf"(?m)^\s+{re.escape(key)}: ([^\r\n]+)$", text)
        assert match, key
        return float(match.group(1)) if "." in match.group(1) else int(match.group(1))
    pivot = re.search(r"(?m)^  spritePivot: \{x: ([^,]+), y: ([^}]+)\}$", text)
    assert pivot
    return {"pixels_to_units": number("spritePixelsToUnits"), "pivot": [float(pivot[1]), float(pivot[2])],
            "alignment": number("alignment"), "sprite_mesh_type": number("spriteMeshType"),
            "srgb": number("sRGBTexture"), "alpha_is_transparency": number("alphaIsTransparency")}


def shader_formula(text):
    def expression(pattern):
        match = re.search(pattern, text)
        assert match, pattern
        return match.group(1).strip()
    return {
        "shader_name": expression(r'Shader "([^"]+)"'),
        "queue": expression(r'"Queue"\s*=\s*"([^"]+)"'),
        "blend": expression(r"(?m)^\s*(Blend SrcAlpha OneMinusSrcAlpha, SrcAlpha OneMinusSrcAlpha)\s*$"),
        "cull": expression(r"(?m)^\s*(Cull Off)\s*$"),
        "zwrite": expression(r"(?m)^\s*(ZWrite Off)\s*$"),
        "sample": expression(r"fixed4 col = (tex2D\(_MainTex, IN.texcoord\)\*IN.color);"),
        "mask_sample": expression(r"fixed4 mask = (tex2D\(_Mask, IN.texcoord\));"),
        "mask_1": expression(r"fixed mask_1 = (\(mask.r \* 2.0\));"),
        "recolor_weight": expression(r"tmpvar_5 = (clamp\(\(mask_1 - 1.0\), 0.0, 1.0\));"),
        "rgb": expression(r"c_2.xyz = (.+);"),
        "sprite_color_multiply": expression(r"c_2 = (\(c_2 \* _SpriteColor\));"),
        "alpha": expression(r"c_2.w = (\(col.a \* clamp\(mask_1, 0.0, 1.0\)\)\*_CityTransparency);"),
        "channel_note": "Only mask.r and mask.g are read by this shader; mask.b and mask.a are not the sprite cutout rule.",
    }


def build(write=True):
    objects = yaml_objects(PREFAB)
    game_objects, transforms, renderers, mask_components = {}, {}, {}, {}
    for class_id, fid, body in objects:
        if class_id == 1:
            game_objects[fid] = {"file_id": fid, "name": field(body, "m_Name"),
                                 "active": field(body, "m_IsActive") == "1",
                                 "layer": int(field(body, "m_Layer")), "tag": field(body, "m_TagString")}
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
            material_guid = re.search(r"(?m)^  - \{fileID: 2100000, guid: ([0-9a-f]{32}), type: 2\}$", body)
            sprite_guid = re.search(r"m_Sprite: \{fileID: (-?\d+)(?:, guid: ([0-9a-f]{32}))?", body)
            assert material_guid and sprite_guid and go not in renderers
            renderers[go] = {"file_id": fid, "enabled": field(body, "m_Enabled") == "1",
                             "material_guid": material_guid[1], "sprite_file_id": int(sprite_guid[1]),
                             "sprite_guid": sprite_guid[2], "color": vec(body, "m_Color"),
                             "flip_x": field(body, "m_FlipX") == "1", "flip_y": field(body, "m_FlipY") == "1",
                             "sorting_layer_id": int(field(body, "m_SortingLayerID")),
                             "sorting_layer": int(field(body, "m_SortingLayer")),
                             "sorting_order": int(field(body, "m_SortingOrder")),
                             "sprite_sort_point": int(field(body, "m_SpriteSortPoint"))}
        elif class_id == 114 and "m_mask_sprite:" in body:
            go = file_id(body, "m_GameObject")
            mask_components[go] = {"file_id": fid, "script_guid": file_guid(body, "m_Script"),
                                   "mask_sprite_file_id": file_id(body, "m_mask_sprite"),
                                   "mask_sprite_guid": re.search(r"m_mask_sprite: \{fileID: -?\d+(?:, guid: ([0-9a-f]{32}))?", body)[1]}
    assert len(game_objects) == len(transforms)
    nodes = []
    for go, node in game_objects.items():
        nodes.append({**node, "transform": transforms[go], "renderer": renderers.get(go),
                      "mask_sprite": mask_components.get(go)})
    root = next(n for n in nodes if n["name"] == MODEL)
    main = next(n for n in nodes if n["name"] == "sprite")
    floor = next(n for n in nodes if n["name"] == "FloorTile_delete")
    board = next(n for n in nodes if n["name"] == "UpgradeBoard")
    assert root["transform"]["parent_transform_file_id"] == 0
    assert main["transform"]["parent_transform_file_id"] == root["transform"]["file_id"]
    assert main["renderer"]["sprite_guid"] == guid(SPRITE)
    assert main["renderer"]["material_guid"] == guid(MATERIAL)
    assert main["mask_sprite"]["script_guid"] == guid(MASK_SCRIPT)
    assert main["mask_sprite"]["mask_sprite_guid"] == guid(MASK)
    assert not floor["active"] and floor["renderer"]["sprite_file_id"] == 0
    assert not board["active"] and board["renderer"]["sprite_guid"] != guid(SPRITE)
    material_text = MATERIAL.read_text(encoding="utf-8-sig")
    shader_text = SHADER.read_text(encoding="utf-8-sig")
    assert file_guid(material_text, "m_Shader") == guid(SHADER)
    def mat_color(key):
        match = re.search(rf"(?m)^    - {re.escape(key)}: (\{{[^\r\n]+\}})$", material_text)
        assert match, key
        return {part.split(": ")[0]: float(part.split(": ")[1]) for part in match[1][1:-1].split(", ")}
    def mat_float(key):
        match = re.search(rf"(?m)^    - {re.escape(key)}: ([^\r\n]+)$", material_text)
        assert match, key
        return float(match[1])
    assert 'propertyBlock.SetTexture("_Mask", m_mask_sprite.texture)' in MASK_SCRIPT.read_text(encoding="latin1")
    with MODEL_CSV.open(encoding="utf-8-sig", newline="") as stream:
        model_rows = list(csv.DictReader(stream))[1:]
    matches = [{"id": int(r["ID"]), "type": int(r["type"]), "civilization": int(r["civType"]),
                "age": int(r["age"])} for r in model_rows if r["modelId"] == MODEL]
    matches.sort(key=lambda r: (r["type"], r["civilization"], r["age"]))
    catalog = json.loads(MODEL_CATALOG.read_text(encoding="utf-8"))
    assert matches == sorted([{"id": r["id"], "type": r["type"], "civilization": r["civilization"], "age": r["age"]}
                              for r in catalog["models"] if r["model_id"] == MODEL],
                             key=lambda r: (r["type"], r["civilization"], r["age"]))
    source_paths = [PREFAB, SPRITE, MASK, MATERIAL, SHADER, MASK_SCRIPT, MODEL_CSV, MODEL_CATALOG]
    source_paths += [Path(str(path) + ".meta") for path in (PREFAB, SPRITE, MASK, MATERIAL, SHADER, MASK_SCRIPT)]
    sprite = image(SPRITE); mask = image(MASK)
    assert sprite["size"] == mask["size"] == [512, 426]
    output = {"schema": "rok.academy_visual_source.v1", "model_id": MODEL,
              "qualification": "Unity source extraction only; no UE asset, city placement, runtime BuildCity, or visual parity claim.",
              "sources": {rel(p): sha(p) for p in source_paths}, "matching_model_keys": matches,
              "prefab": {"path": rel(PREFAB), "guid": guid(PREFAB), "nodes": nodes,
                         "main_sprite_game_object_file_id": main["file_id"],
                         "root_game_object_file_id": root["file_id"],
                         "optional_floor_active": floor["active"], "upgrade_board_active": board["active"]},
              "sprite": {**sprite, "importer": importer(SPRITE)},
              "mask": {**mask, "importer": importer(MASK)},
              "mask_binding": {"component_file_id": main["mask_sprite"]["file_id"],
                               "script_guid": guid(MASK_SCRIPT), "property": "_Mask",
                               "mechanism": "MaterialPropertyBlock.GetPropertyBlock -> SetTexture(_Mask, m_mask_sprite.texture) -> SetPropertyBlock",
                               "material_default_mask_guid":
                               re.search(r"(?s)- _Mask:\s*\n\s*m_Texture: \{fileID: 2800000, guid: ([0-9a-f]{32})", material_text)[1]},
              "material": {"path": rel(MATERIAL), "guid": guid(MATERIAL), "shader_path": rel(SHADER),
                           "shader_guid": guid(SHADER), "color_1": mat_color("_Color_1"),
                           "color_2": mat_color("_Color_2"), "city_transparency": mat_float("_CityTransparency"),
                           "shader_formula": shader_formula(shader_text)}}
    if write:
        OUT.parent.mkdir(parents=True, exist_ok=True)
        OUT.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {OUT}: {len(nodes)} prefab nodes, {len(matches)} model keys")
    return output


if __name__ == "__main__":
    build()
