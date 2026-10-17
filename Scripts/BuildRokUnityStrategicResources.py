"""Export source fixed-resource cells and visual contracts, without inventing nodes."""
from pathlib import Path
import csv
import hashlib
import json
import re
import struct
import sys
from PIL import Image

root = Path(__file__).resolve().parents[1]
base = root / "ref/client-unity/Assets/BundleAssets/Scene/Map_building/map_building_lod"
table = root / "ref/config-tables/编译客户端配置/CSV/MapFixPoint.csv"
names = dict(zip(range(4, 12), ["alliance_food_lod3", "alliance_wood_lod3", "alliance_stone_lod3", "alliance_coin_lod3",
    "alliance_super_mine_food_lod3", "alliance_super_mine_wood_lod3", "alliance_super_mine_stone_lod3", "alliance_super_mine_coin_lod3"]))
owner = root / "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_LodMenu_SubView.cs"
owner_text = owner.read_text(encoding="utf-8-sig")
for type_id, name in names.items():
    assert re.search(rf'case {type_id}:\s+return "{name}";', owner_text), "Source type mapping changed"
atlas = root / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/map_icon.png"
atlas_meta = Path(str(atlas) + ".meta")
atlas_text = atlas_meta.read_text()
atlas_sprites = {}
for block in atlas_text.split("    - serializedVersion: 2\n")[1:]:
    identity = re.search(r"      internalID: (-?\d+)", block)
    if not identity:
        continue
    rect = [int(re.search(rf"        {field}: (\d+)", block)[1]) for field in ("x", "y", "width", "height")]
    atlas_sprites[identity[1]] = {"name": re.search(r"      name: (.+)", block)[1], "rect_bottom_left": rect}

def evidence(path):
    return {"path": path.relative_to(root).as_posix(), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}

with table.open(encoding="utf-8-sig", newline="") as stream:
    rows = [{key: int(value) for key, value in row.items()} for row in list(csv.DictReader(stream))[1:]]
assert len({row["ID"] for row in rows}) == len(rows), "Duplicate source ID"
points = []
cells = [[] for _ in range(24 * 24)]
for row in rows:
    if row["group"] != 2:
        continue
    assert row["type"] in names, f"Unknown resource type: {row}"
    x, y = row["posX"], row["posY"]
    assert 0 <= x < 7200 and 0 <= y < 7200
    cell = x // 300 + y // 300 * 24
    cells[cell].append(row["ID"])
    points.append({"id": row["ID"], "type": row["type"], "unity_position": [x, y],
                   "prefab": names[row["type"]], "cell": cell})
assert points
visuals = {}
crop_dir = root / "Saved/RokUnityMigration/StrategicResourceIcons"
if "--verify" not in sys.argv:
    crop_dir.mkdir(parents=True, exist_ok=True)
atlas_image = Image.open(atlas).convert("RGBA")
for type_id, name in names.items():
    prefab = base / (name + ".prefab")
    text = prefab.read_text()
    curve = text.split("  m_scale_explore_curve:", 1)[1].split("  m_PreInfinity:", 1)[0]
    keys = [[float(value) for value in key] for key in re.findall(
        r"time: (\S+)\s+value: (\S+)\s+inSlope: (\S+)\s+outSlope: (\S+)", curve)]
    assert keys and all(a[0] < b[0] for a, b in zip(keys, keys[1:]))
    assert all(mode == "0" for mode in re.findall(r"weightedMode: (\d+)", curve))
    sprite = re.search(r"m_Sprite: \{fileID: (-?\d+), guid: ([a-f0-9]+)", text)
    assert sprite, f"Missing source sprite in {prefab}"
    assert sprite[2] == "84525cbeb38f9e147ab1cfa2fb4bf034" and sprite[1] in atlas_sprites
    x, y, w, h = atlas_sprites[sprite[1]]["rect_bottom_left"]
    crop = atlas_image.crop((x, atlas_image.height-y-h, x+w, atlas_image.height-y))
    crop_path = crop_dir / (name + ".png")
    if "--verify" in sys.argv:
        actual = Image.open(crop_path).convert("RGBA")
        assert actual.size == crop.size and actual.tobytes() == crop.tobytes(), "Sprite crop differs from source RGBA"
    else:
        crop.save(crop_path)
    visuals[str(type_id)] = {"prefab": evidence(prefab), "sprite_file_id": sprite[1], "sprite_guid": sprite[2],
        "sprite": atlas_sprites[sprite[1]], "texture": evidence(crop_path), "pixels_per_unit": float(re.search(r"spritePixelsToUnits: ([\d.]+)", atlas_text)[1]), "explore_curve": keys}

texture = root / "ref/client-unity/Assets/BundleAssets/Map/Env/Ground/resource_minimap.png"
meta = Path(str(texture) + ".meta")
material = texture.with_suffix(".mat")
shader = root / "ref/client-unity/Assets/Shader/share_shader/custommapmap_lod_resource_minimap_+45.shader"
width, height = struct.unpack(">II", texture.read_bytes()[16:24])
assert (width, height) == (40, 40)
assert "spritePixelsToUnits: 40" in meta.read_text()
assert "filterMode: 0" in meta.read_text() and "enableMipMap: 0" in meta.read_text()
assert "Blend SrcAlpha OneMinusSrcAlpha" in shader.read_text() and "ZWrite Off" in shader.read_text()
assert "Transparent+45" in shader.read_text()

# Validate every source nine-cell neighborhood against an independent spatial scan.
for cy in range(24):
    for cx in range(24):
        indexed = {point for y in range(max(0, cy-1), min(24, cy+2))
                   for x in range(max(0, cx-1), min(24, cx+2)) for point in cells[x+y*24]}
        expected = {p["id"] for p in points if abs(p["unity_position"][0] // 300-cx) <= 1 and abs(p["unity_position"][1] // 300-cy) <= 1}
        assert indexed == expected
result = {"schema": "rok.unity.strategic_resources.v1", "table": evidence(table),
    "owner": evidence(owner), "point_atlas": evidence(atlas), "point_atlas_meta": evidence(atlas_meta),
    "points": points, "cells": cells, "cell_size": 300, "cells_per_axis": 24, "visuals": visuals,
    "overlay": {"prefab": evidence(base / "resource_minimap.prefab"), "texture": evidence(texture),
        "texture_meta": evidence(meta), "material": evidence(material), "shader": evidence(shader),
        "runtime_center": [3600, 0, 3600], "unity_size": [7200, 7200], "rotation_x": 90,
        "point_sampling": True, "mipmaps": False, "render_queue": 3045, "depth_write": False},
    "ue_target": "ARokStrategicResourceLayer"}
output = root / "Saved/RokUnityMigration/unity_strategic_resources.json"
if "--verify" in sys.argv:
    assert json.loads(output.read_text(encoding="utf-8")) == result, "Stale source export"
else:
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")
print(f"PASS: {len(points)} fixed resource points, {len(visuals)} source prefabs, 576 neighborhood checks, overlay source chain")
