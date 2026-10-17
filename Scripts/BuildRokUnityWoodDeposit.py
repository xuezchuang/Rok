"""Extract the wood deposit's source geometry; never synthesize placement."""
import hashlib
import csv
import json
import math
import re
from pathlib import Path

from PIL import Image
from BuildRokUnityWorldMapRuntimeData import (
    parse_unity_yaml_objects, parse_scalar, parse_vector, parse_reference,
    parse_transform_children,
)

ROOT = Path(__file__).resolve().parents[1]
PREFAB = ROOT / "ref/client-unity/Assets/BundleAssets/Scene/Map_building/map_building/build_Rsswood_Big.prefab"
OUT = ROOT / "Saved/RokUnityMigration/unity_wood_deposit.json"


def rotate(q, v):
    x, y, z, w = (q[k] for k in "xyzw")
    a, b, c = v
    tx, ty, tz = 2*(y*c-z*b), 2*(z*a-x*c), 2*(x*b-y*a)
    return [a+w*tx+y*tz-z*ty, b+w*ty+z*tx-x*tz, c+w*tz+x*ty-y*tx]


def source(path):
    return {"path": path.relative_to(ROOT).as_posix(), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def curve(body):
    block = body.split("  m_scale_curve:", 1)[1].split("    m_PreInfinity:", 1)[0]
    # The first key has Infinity arrive tangent but pre-infinity is clamped.
    # Preserve that source fact without emitting invalid JSON Infinity.
    return [{"time": float(t), "value": float(v),
             "in": float(i) if math.isfinite(float(i)) else None,
             "out": float(o) if math.isfinite(float(o)) else None}
            for t, v, i, o in re.findall(r"time: (\S+)\s+value: (\S+)\s+inSlope: (\S+)\s+outSlope: (\S+)", block)]


def build():
    resource_table = ROOT / "ref/config-tables/编译客户端配置/CSV/ResourceGatherType.csv"
    with resource_table.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))[1:]
    wood_rows = [r for r in rows if r["type"] == "2"]
    assert wood_rows and all(r["modelId"] == PREFAB.stem for r in wood_rows)
    objects = parse_unity_yaml_objects(PREFAB)
    by_id = {o["file_id"]: o for o in objects}
    by_go = {}
    for o in objects:
        if o["class_id"] != 1:
            go = parse_reference(o["body"], "m_GameObject")
            if go:
                by_go.setdefault(go["file_id"], []).append(o)
    root_go = next(o for o in objects if o["class_id"] == 1 and parse_scalar(o["body"], "m_Name") == "build_Rsswood_Big")
    root_components = by_go[root_go["file_id"]]
    root_transform = next(o for o in root_components if o["class_id"] == 4)
    root_rotation = parse_vector(root_transform["body"], "m_LocalRotation", "xyzw")
    lod = next(o for o in root_components if "m_scale_curve:" in o["body"])
    manager = next(o for o in root_components if "m_size:" in o["body"])
    needed = set()
    for o in objects:
        if o["class_id"] == 212 or "m_mask_sprite:" in o["body"]:
            needed.update(re.findall(r"guid: ([a-f0-9]{32})", o["body"]))
    guids = {}
    for meta in (ROOT / "ref/client-unity/Assets/BundleAssets").rglob("*.meta"):
        match = re.search(r"(?m)^guid: (\w+)", meta.read_text(encoding="utf-8-sig"))
        if match and match[1] in needed:
            guids[match[1]] = Path(str(meta)[:-5])
    items = []
    for child in parse_transform_children(root_transform["body"]):
        transform = by_id[child]
        go = parse_reference(transform["body"], "m_GameObject")["file_id"]
        components = by_go[go]
        grid = next((o for o in components if "m_priority:" in o["body"]), None)
        if not grid:
            continue
        assert parse_scalar(grid["body"], "m_auto_registre") == "1"
        assert parse_scalar(grid["body"], "m_shift_pos") == "0"
        pos = parse_vector(transform["body"], "m_LocalPosition", "xyz")
        assert parse_vector(transform["body"], "m_LocalRotation", "xyzw") == {"x": 0, "y": 0, "z": 0, "w": 1}
        item = {"name": parse_scalar(by_id[go]["body"], "m_Name"),
                "source_transform_id": transform["file_id"],
                "position": rotate(root_rotation, [pos[k] for k in "xyz"]),
                "size": list(parse_vector(grid["body"], "size", "xy").values()),
                "priority": int(parse_scalar(grid["body"], "m_priority")), "sprites": []}
        for sprite_child in parse_transform_children(transform["body"]):
            st = by_id[sprite_child]
            sg = parse_reference(st["body"], "m_GameObject")["file_id"]
            sc = by_go[sg]
            renderer = next((o for o in sc if o["class_id"] == 212), None)
            if not renderer:
                continue
            assert all(v == 1 for v in parse_vector(renderer["body"], "m_Color", "rgba").values())
            assert parse_scalar(renderer["body"], "m_FlipX") == "0"
            assert parse_scalar(renderer["body"], "m_FlipY") == "0"
            mask_component = next(o for o in sc if "m_mask_sprite:" in o["body"])
            texture = guids[parse_reference(renderer["body"], "m_Sprite")["guid"]]
            mask = guids[parse_reference(mask_component["body"], "m_mask_sprite")["guid"]]
            material_guid = re.search(r"m_Materials:\s+- \{fileID: \d+, guid: (\w+)", renderer["body"])[1]
            material = guids[material_guid]
            mb = material.read_text(encoding="utf-8")
            assert "45d662578936c924fbe7a09119b72a9c" in mb
            meta = Path(str(texture)+".meta").read_text(encoding="utf-8")
            assert int(parse_scalar(meta, "spriteMode")) == 1
            ppm = float(parse_scalar(meta, "spritePixelsToUnits"))
            pivot = parse_vector(meta, "spritePivot", "xy")
            with Image.open(texture) as im:
                width, height = im.size
            sp = parse_vector(st["body"], "m_LocalPosition", "xyz")
            ss = parse_vector(st["body"], "m_LocalScale", "xyz")
            sq = parse_vector(st["body"], "m_LocalRotation", "xyzw")
            item["sprites"].append({
                "texture": source(texture), "mask": source(mask), "material": source(material),
                "position": rotate(root_rotation, [sp[k] for k in "xyz"]),
                "right": rotate(root_rotation, rotate(sq, [1,0,0])),
                "up": rotate(root_rotation, rotate(sq, [0,1,0])),
                "size": [width / ppm * ss["x"], height / ppm * ss["y"]],
                "pivot": list(pivot.values()),
                "color1": list(parse_vector(mb, "- _Color_1", "rgb").values()),
                "color2": list(parse_vector(mb, "- _Color_2", "rgb").values()),
            })
        assert len(item["sprites"]) == 1, item["name"]
        items.append(item)
    assert len(items) == 18
    items.sort(key=lambda i: -i["priority"])
    native_path = OUT.with_name("unity_wood_deposit_native_probe.json")
    if native_path.exists():
        native = json.loads(native_path.read_text(encoding="utf-8"))
        order = native["runs"][0]["samples"][0]["order"]
        assert len(order) == 18 and set(order) == {i["name"] for i in items}
        assert all(s["order"] == order for r in native["runs"] for s in r["samples"])
        items.sort(key=lambda i: order.index(i["name"]))
    result = {"source": source(PREFAB), "items": items, "scale_curve": curve(lod["body"]),
              "resource_table": source(resource_table), "resource_type_ids": [int(r["ID"]) for r in wood_rows],
              "max_scale": float(parse_scalar(lod["body"], "m_max_scale")),
              "bounds_size": list(parse_vector(manager["body"], "m_size", "xy").values()),
              "unity_units_to_ue": 500,
              "scope": "Initial daytime wood child composition, scale and GridCollide visibility. Dedicated material uses source day tint; night switching and deposit ground are not migrated by this slice.",
              "order_contract": "Captured native Unity 2019.4.28f1c1 Start/List.Sort order, consistent across three instances. Reproduces that reference run; Unity does not guarantee equal-priority ordering across contexts."}
    if native_path.exists():
        result["native_order_evidence"] = source(native_path)
    else:
        result["order_contract"] = "Pending isolated Unity native registration capture; extraction is not runtime ready."
    result["algorithm_sources"] = [source(ROOT/"ref/client-unity/Assets/Scripts/Client"/p) for p in (
        "TileCollideManager/GridCollideMgr.cs", "TileCollideManager/GridCollideItem.cs", "Utils/RotateSquar.cs",
        "LodManager/LevelDetailBase.cs", "LodManager/LevelDetailCastle.cs", "LodManager/LevelDetailDeposit.cs")]
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False)+"\n", encoding="utf-8")
    print(f"Extracted {len(items)} wood items: {OUT}")


if __name__ == "__main__":
    build()
