"""Extract close world-structure sprite assemblies from their Unity prefabs.

No card cutouts or replacement art are generated. The exported PNGs contain
the entire source image in RGBA, including the source mask channels.
"""

import hashlib
import json
import math
import re
from pathlib import Path

from PIL import Image

from BuildRokUnityWorldMapRuntimeData import (
    parse_reference, parse_scalar, parse_transform_children, parse_vector,
)
from PrepareRokUnityWorldMapAssets import build_guid_index
from rok_data_paths import DATA_ROOT, label, resolve


ROOT = Path(__file__).resolve().parents[1]
PREFAB_DIR = ROOT / "ref/client-unity/Assets/BundleAssets/Scene/Map_building/map_building"
ASSET_DIR = DATA_ROOT / "WorldStructurePresentation"
OUT = DATA_ROOT / "world_structure_presentation.json"
MODELS = (
    "build_BarbarianCitadel_Big", "build_alliance_1003", "build_alliance_1004",
    "build_alliance_1005", "build_alliance_1006", "build_Checkpoint_Big",
    "build_HolyLand1_Big", "build_HolyLand2_Big", "build_Rssgold_Big",
)
EXPECTED_SPRITES = {
    "build_BarbarianCitadel_Big": 1, "build_alliance_1003": 1,
    "build_alliance_1004": 1, "build_alliance_1005": 1,
    "build_alliance_1006": 1, "build_Checkpoint_Big": 4,
    "build_HolyLand1_Big": 1, "build_HolyLand2_Big": 1,
    "build_Rssgold_Big": 13,
}
SUPPORTED_SHADER = "45d662578936c924fbe7a09119b72a9c"
BUILTIN_SHADER = "0000000000000000f000000000000000"


def source(path):
    return {"path": label(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def objects(path):
    text = path.read_text(encoding="utf-8")
    parts = re.split(r"(?m)^--- !u!(\d+) &(-?\d+)(?: stripped)?\s*$", text)
    return [{"class_id": int(parts[i]), "file_id": parts[i + 1], "body": parts[i + 2]}
            for i in range(1, len(parts), 3)]


def vec(body, name, axes):
    return [float(v) for v in parse_vector(body, name, axes, required=True).values()]


def quat_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return [aw*bx + ax*bw + ay*bz - az*by,
            aw*by - ax*bz + ay*bw + az*bx,
            aw*bz + ax*by - ay*bx + az*bw,
            aw*bw - ax*bx - ay*by - az*bz]


def rotate(q, v):
    x, y, z, w = q
    a, b, c = v
    tx, ty, tz = 2*(y*c-z*b), 2*(z*a-x*c), 2*(x*b-y*a)
    return [a+w*tx+y*tz-z*ty, b+w*ty+z*tx-x*tz, c+w*tz+x*ty-y*tx]


def add(a, b):
    return [x+y for x, y in zip(a, b)]


def mul(a, b):
    return [x*y for x, y in zip(a, b)]


def curve(body):
    block = body.split("  m_scale_curve:", 1)[1].split("    m_PreInfinity:", 1)[0]
    keys = []
    for t, v, ins, outs in re.findall(
            r"time: (\S+)\s+value: (\S+)\s+inSlope: (\S+)\s+outSlope: (\S+)", block):
        keys.append({"time": float(t), "value": float(v),
                     "in": float(ins) if math.isfinite(float(ins)) else None,
                     "out": float(outs) if math.isfinite(float(outs)) else None})
    assert keys
    return keys


def lod_distances(body):
    block = body.split("  m_lod_array:", 1)[1].split("  m_scale_curve:", 1)[0]
    return [float(v) for v in re.findall(r"(?m)^  - (\S+)$", block)]


def overrides(instance):
    """Unity PrefabInstance property paths keyed by source object fileID."""
    result = {}
    block = instance["body"].split("    m_Modifications:", 1)[1].split(
        "    m_RemovedComponents:", 1)[0]
    chunks = re.split(r"(?m)^    - target:", block)[1:]
    for chunk in chunks:
        fid = re.search(r"fileID:\s*(-?\d+)", chunk)
        prop = re.search(r"(?m)^      propertyPath: (.+)$", chunk)
        val = re.search(r"(?m)^      value:\s*(.*?)\s*$", chunk)
        ref = re.search(r"(?ms)^      objectReference:\s*(\{.*?\})", chunk)
        if not (fid and prop and val and ref):
            raise ValueError("Unparsed prefab override")
        result.setdefault(fid[1], {})[prop[1]] = (val[1], ref[1])
    return result


def resolved_scalar(obj, field, mods):
    return mods.get(obj["file_id"], {}).get(field, (parse_scalar(obj["body"], field), None))[0]


def resolved_vector(obj, field, axes, mods):
    values = parse_vector(obj["body"], field, axes, required=True)
    for axis in axes:
        override = mods.get(obj["file_id"], {}).get(field + "." + axis)
        if override:
            values[axis] = float(override[0])
    return [float(values[axis]) for axis in axes]


def resolved_ref(obj, field, mods):
    override = mods.get(obj["file_id"], {}).get(field)
    if override:
        return parse_ref_text(override[1])
    return parse_reference(obj["body"], field)


def parse_ref_text(text):
    fid = re.search(r"fileID:\s*(-?\d+)", text)
    guid = re.search(r"guid:\s*([0-9a-f]+)", text)
    return {"file_id": fid[1], "guid": guid[1] if guid else "",
            "is_null": fid[1] == "0" and not guid}


def png_asset(path, image_cache):
    if path.suffix.lower() != ".png":
        raise ValueError(f"Source art is not PNG: {path}")
    info = source(path)
    if info["sha256"] not in image_cache:
        target = ASSET_DIR / f'{info["sha256"][:16]}_{path.stem}.png'
        with Image.open(path) as image:
            rgba = image.convert("RGBA")
            if not target.exists():
                target.parent.mkdir(parents=True, exist_ok=True)
                rgba.save(target)
            else:
                with Image.open(target) as existing:
                    assert existing.mode == "RGBA" and existing.tobytes() == rgba.tobytes()
            info.update({"export": label(target), "width": rgba.width,
                         "height": rgba.height, "mode": "RGBA"})
        image_cache[info["sha256"]] = info
    return image_cache[info["sha256"]]


def extract(model_id, guid_index, image_cache):
    path = PREFAB_DIR / (model_id + ".prefab")
    direct = objects(path)
    by_id = {o["file_id"]: o for o in direct}
    root = next(o for o in direct if o["class_id"] == 1 and
                parse_scalar(o["body"], "m_Name") == model_id)
    root_transform = next(o for o in direct if o["class_id"] == 4 and
                          parse_reference(o["body"], "m_GameObject")["file_id"] == root["file_id"])
    lod = next(o for o in direct if "m_scale_curve:" in o["body"])
    bounds = next(o for o in direct if "  m_size:" in o["body"])
    root_position = vec(root_transform["body"], "m_LocalPosition", "xyz")
    root_rotation = vec(root_transform["body"], "m_LocalRotation", "xyzw")
    root_scale = vec(root_transform["body"], "m_LocalScale", "xyz")
    root_key = (label(path), root_transform["file_id"])
    nodes = {}
    components = {}
    model_sources = [source(path)]
    nested_mods = {}
    nested_root_parent = {}
    nested_names = {}

    def ingest(src, src_objects, mods):
        prefix = label(src)
        local_by_id = {o["file_id"]: o for o in src_objects}
        for obj in src_objects:
            if obj["class_id"] not in (1, 4, 212, 114):
                continue
            go_ref = parse_reference(obj["body"], "m_GameObject")
            if go_ref and go_ref["file_id"] != "0":
                components.setdefault((prefix, go_ref["file_id"]), []).append(obj)
            if obj["class_id"] == 4 and go_ref:
                parent_ref = parse_reference(obj["body"], "m_Father")
                parent = (prefix, parent_ref["file_id"]) if parent_ref and parent_ref["file_id"] != "0" else None
                nodes[(prefix, obj["file_id"])] = {
                    "obj": obj, "source": src, "mods": mods, "parent": parent,
                    "go": (prefix, go_ref["file_id"]),
                    "sibling_index": int(resolved_scalar(obj, "m_RootOrder", mods) or 0),
                }
        return local_by_id

    ingest(path, direct, {})
    for instance in [o for o in direct if o["class_id"] == 1001]:
        prefab_ref = parse_reference(instance["body"], "m_SourcePrefab")
        nested = guid_index.get(prefab_ref["guid"])
        if not nested or nested.suffix.lower() != ".prefab":
            # The checkpoint FBX is a separate MeshRenderer assembly. Its
            # exact source is recorded as unsupported; never make a card for it.
            mods = overrides(instance)
            parent_ref = parse_reference(instance["body"], "m_TransformParent")
            parent_key = (label(path), parent_ref["file_id"])
            nested_names[instance["file_id"]] = {
                "source": source(nested) if nested else {"guid": prefab_ref["guid"], "missing": True},
                "source_meta": source(Path(str(nested) + ".meta")) if nested else None,
                "parent_transform_id": parent_ref["file_id"],
                "modifications": [
                    {"source_file_id": fid, "property_path": prop,
                     "value": val, "object_reference": ref}
                    for fid, fields in sorted(mods.items()) for prop, (val, ref) in sorted(fields.items())
                ],
            }
            for stripped in direct:
                if stripped["class_id"] != 4 or parse_reference(stripped["body"], "m_GameObject"):
                    continue
                inst_ref = parse_reference(stripped["body"], "m_PrefabInstance")
                if not inst_ref or inst_ref["file_id"] != instance["file_id"]:
                    continue
                src_ref = parse_reference(stripped["body"], "m_CorrespondingSourceObject")
                smod = mods.get(src_ref["file_id"], {})
                required = [f"m_LocalPosition.{axis}" for axis in "xyz"] + [f"m_LocalRotation.{axis}" for axis in "xyzw"] + [f"m_LocalScale.{axis}" for axis in "xyz"]
                if not all(field in smod for field in required):
                    raise ValueError(f"Incomplete FBX root transform override: {stripped['file_id']}")
                def values(name, axes):
                    return "{" + ", ".join(f"{axis}: {smod[name+'.'+axis][0]}" for axis in axes) + "}"
                pseudo = {"file_id": stripped["file_id"], "body":
                          f"  m_LocalPosition: {values('m_LocalPosition', 'xyz')}\n"
                          f"  m_LocalRotation: {values('m_LocalRotation', 'xyzw')}\n"
                          f"  m_LocalScale: {values('m_LocalScale', 'xyz')}\n"}
                nodes[(label(path), stripped["file_id"])] = {
                    "obj": pseudo, "source": nested or path, "mods": {}, "parent": parent_key,
                    "go": None, "sibling_index": int(smod.get("m_RootOrder", (0,))[0]),
                }
                nested_names[instance["file_id"]]["root_transform_id"] = stripped["file_id"]
            continue
        mods = overrides(instance)
        ingest(nested, objects(nested), mods)
        model_sources.append(source(nested))
        parent_ref = parse_reference(instance["body"], "m_TransformParent")
        parent_key = (label(path), parent_ref["file_id"])
        roots = [key for key, node in nodes.items() if key[0] == label(nested) and node["parent"] is None]
        assert len(roots) == 1, (nested, roots)
        nodes[roots[0]]["parent"] = parent_key
        nested_root_parent[roots[0]] = parent_key
        nested_mods[label(nested)] = mods

    def world(key):
        node = nodes[key]
        if "world" in node:
            return node["world"]
        obj, mods = node["obj"], node["mods"]
        pos = resolved_vector(obj, "m_LocalPosition", "xyz", mods)
        q = resolved_vector(obj, "m_LocalRotation", "xyzw", mods)
        scale = resolved_vector(obj, "m_LocalScale", "xyz", mods)
        if key == root_key:
            # Unity's scene placement owns the root translation. Keep its
            # rotation and authored scale in the model-relative geometry.
            runtime_q = [0.0, 0.0, 0.0, 1.0] if model_id == "build_Checkpoint_Big" else q
            result = ([0.0, 0.0, 0.0], runtime_q, [1.0, 1.0, 1.0])
        else:
            if node["parent"] not in nodes:
                raise ValueError(f"Unknown transform parent: {key}")
            pp, pq, ps = world(node["parent"])
            is_grid = any(o["class_id"] == 114 and "  m_priority:" in o["body"]
                          for o in components.get(node["go"], []))
            # LevelDetailCastle/GridCollideMgr.SetScale replaces a group
            # transform's authored localScale with the curve value.
            effective_scale = [1.0, 1.0, 1.0] if is_grid else scale
            result = (add(pp, rotate(pq, mul(ps, pos))), quat_mul(pq, q), mul(ps, effective_scale))
        node["world"] = result
        return result

    def go(key):
        return next(o for o in (direct if key[0] == label(path) else objects(Path(ROOT / key[0])))
                    if o["class_id"] == 1 and o["file_id"] == key[1])

    groups = []
    group_by_key = {}
    for key, node in nodes.items():
        for component in components.get(node["go"], []):
            if component["class_id"] != 114 or "  m_priority:" not in component["body"]:
                continue
            mods = node["mods"]
            group = {
                "id": f"{key[0]}#{key[1]}", "name": resolved_scalar(go(node["go"]), "m_Name", mods),
                "source_transform_id": key[1], "source_component_id": component["file_id"],
                "source": source(node["source"]), "sibling_index": node["sibling_index"],
                "position": world(key)[0], "size": resolved_vector(component, "size", "xy", mods),
                "authored_local_scale": resolved_vector(node["obj"], "m_LocalScale", "xyz", mods),
                "shift_origin": world(node["parent"])[0] if node["parent"] else [0.0, 0.0, 0.0],
                "priority": int(resolved_scalar(component, "m_priority", mods)),
                "shift_pos": int(resolved_scalar(component, "m_shift_pos", mods)),
                "shift_position": int(resolved_scalar(component, "m_shift_pos", mods)) == 1,
                "shift_start_scale": float(resolved_scalar(component, "m_shift_pos_start_scale", mods)),
                "shift_end_scale": float(resolved_scalar(component, "m_shift_pos_end_scale", mods)),
                "auto_register": int(resolved_scalar(component, "m_auto_registre", mods)) == 1,
                "active": int(resolved_scalar(component, "m_is_active", mods)) == 1,
                "enabled": int(resolved_scalar(component, "m_Enabled", mods)) == 1,
            }
            groups.append(group)
            group_by_key[key] = group

    items, unsupported = [], []
    for key, node in nodes.items():
        objs = components.get(node["go"], [])
        renderer = next((o for o in objs if o["class_id"] == 212), None)
        if not renderer:
            continue
        mods = node["mods"]
        sprite_ref = resolved_ref(renderer, "m_Sprite", mods)
        if not sprite_ref or sprite_ref["is_null"]:
            continue
        if int(resolved_scalar(renderer, "m_Enabled", mods)) != 1:
            continue
        mask_component = next((o for o in objs if "  m_mask_sprite:" in o["body"]), None)
        mask_ref = resolved_ref(mask_component, "m_mask_sprite", mods) if mask_component else None
        materials_block = renderer["body"].split("  m_Materials:", 1)[1]
        material_ref = parse_ref_text(re.search(r"(?m)^  - (\{.*?\})", materials_block)[1])
        override_mat = mods.get(renderer["file_id"], {}).get("m_Materials.Array.data[0]")
        if override_mat:
            material_ref = parse_ref_text(override_mat[1])
        tex_path = guid_index.get(sprite_ref["guid"])
        mask_path = guid_index.get(mask_ref["guid"]) if mask_ref and not mask_ref["is_null"] else None
        mat_path = guid_index.get(material_ref["guid"])
        if not (tex_path and mat_path):
            unsupported.append({"renderer": renderer["file_id"], "reason": "missing texture, mask, or material",
                                "sprite_guid": sprite_ref["guid"], "mask_guid": mask_ref["guid"] if mask_ref else None,
                                "material_guid": material_ref["guid"]})
            continue
        mat_body = mat_path.read_text(encoding="utf-8")
        shader_ref = parse_reference(mat_body, "m_Shader")
        shader_path = guid_index.get(shader_ref["guid"])
        masked = shader_ref["guid"] == SUPPORTED_SHADER and shader_path and mask_path
        builtin = (shader_ref["guid"] == BUILTIN_SHADER and shader_ref["file_id"] == "10750"
                   and not mask_path)
        if not (masked or builtin):
            unsupported.append({"renderer": renderer["file_id"], "reason": "unsupported shader",
                                "shader_guid": shader_ref["guid"], "material": source(mat_path)})
            continue
        meta = Path(str(tex_path) + ".meta").read_text(encoding="utf-8")
        if int(parse_scalar(meta, "spriteMode")) != 1:
            raise ValueError(f"Sprite sheet needs explicit rect extraction: {tex_path}")
        ppm = float(parse_scalar(meta, "spritePixelsToUnits"))
        pivot = vec(meta, "spritePivot", "xy")
        texture = png_asset(tex_path, image_cache)
        mask = png_asset(mask_path, image_cache) if masked else None
        if mask and (texture["width"], texture["height"]) != (mask["width"], mask["height"]):
            raise ValueError(f"Texture/mask dimensions differ: {tex_path}, {mask_path}")
        group_key = key
        while group_key not in group_by_key and group_key != root_key:
            group_key = nodes[group_key]["parent"]
        group = group_by_key.get(group_key)
        wp, wq, ws = world(key)
        anchor = group["position"] if group else [0.0, 0.0, 0.0]
        lineage = []
        cursor = key
        while cursor:
            ancestor = nodes[cursor]
            if ancestor["go"]:
                lineage.append(int(resolved_scalar(go(ancestor["go"]), "m_IsActive", ancestor["mods"])) == 1)
            cursor = ancestor["parent"]
        item = {
            "source_id": f"{key[0]}#{renderer['file_id']}",
            "source_renderer_id": renderer["file_id"], "source": source(node["source"]),
            "name": resolved_scalar(go(node["go"]), "m_Name", mods),
            "group_id": group["id"] if group else None,
            "enabled": True, "active_in_hierarchy": all(lineage),
            "ground_lod0_only": False,
            "position": anchor, "sprite_position": [wp[i] - anchor[i] for i in range(3)],
            "right": rotate(wq, [1, 0, 0]),
            "up": rotate(wq, [0, 1, 0]),
            "size": [texture["width"] / ppm * ws[0], texture["height"] / ppm * ws[1]],
            "pivot": pivot, "texture": texture, "mask": mask,
            "material": source(mat_path), "shader": source(shader_path) if shader_path else None,
            "shader_guid": shader_ref["guid"],
            "shader_file_id": shader_ref["file_id"],
            "shader_mode": "custom_map_building_mask" if masked else "unity_builtin_10750",
            "shader_name": "custom/map/map_building_with_mask-5" if masked else "Unlit/Transparent",
            "material_custom_render_queue": int(parse_scalar(mat_body, "m_CustomRenderQueue")),
            "material_keywords": parse_scalar(mat_body, "m_ShaderKeywords").split(),
            "color1": vec(mat_body, "- _Color_1", "rgba") if masked else [0.5, 0.5, 0.5, 1.0],
            "color2": vec(mat_body, "- _Color_2", "rgba") if masked else [0.5, 0.5, 0.5, 1.0],
            "renderer_color": resolved_vector(renderer, "m_Color", "rgba", mods),
            "sort_layer_id": int(resolved_scalar(renderer, "m_SortingLayerID", mods)),
            "sort_layer_order": 0,  # TagManager.asset has only Default/uniqueID 0.
            "sort_order": int(resolved_scalar(renderer, "m_SortingOrder", mods)),
            "flip_x": int(resolved_scalar(renderer, "m_FlipX", mods)) == 1,
            "flip_y": int(resolved_scalar(renderer, "m_FlipY", mods)) == 1,
            "sibling_index": node["sibling_index"],
        }
        items.append(item)

    for iid, evidence in nested_names.items():
        root_key_nested = (label(path), evidence["root_transform_id"])
        evidence["runtime_root_transform"] = {
            "position": world(root_key_nested)[0],
            "rotation": world(root_key_nested)[1],
            "scale": world(root_key_nested)[2],
        }
        unsupported.append({"prefab_instance_id": iid, "reason": "nested non-prefab renderer assembly",
                            **evidence})
    ground_ref = parse_reference(lod["body"], "m_ground_object")
    ground_path = guid_index.get(ground_ref["guid"]) if ground_ref and ground_ref["guid"] else None
    ground_local_go = by_id.get(ground_ref["file_id"]) if ground_ref and not ground_ref["guid"] else None
    if ground_local_go and ground_local_go["class_id"] != 1:
        raise ValueError(f"Ground reference is not a GameObject: {model_id}")
    ground_local_name = parse_scalar(ground_local_go["body"], "m_Name") if ground_local_go else None
    ground_direct_renderers = [o for o in direct if o["class_id"] == 212 and
                               ground_ref and parse_reference(o["body"], "m_GameObject")["file_id"] == ground_ref["file_id"]]
    ground_has_direct_sprite = any(not parse_reference(o["body"], "m_Sprite")["is_null"] for o in ground_direct_renderers)
    if ground_ref and ground_ref["guid"]:
        ground_kind = "external_asset_reference" if ground_path else "missing_external_asset"
    elif ground_ref and ground_ref["file_id"] != "0":
        ground_kind = "local_root_game_object" if ground_ref["file_id"] == root["file_id"] else "local_game_object"
    else:
        ground_kind = "null"
    result = {
        "source": source(path), "source_chain": model_sources,
        "root_position_in_prefab": root_position, "root_rotation": root_rotation,
        "root_scale": root_scale, "runtime_root_scale": [1.0, 1.0, 1.0],
        "runtime_root_rotation": "strongHoldDataDefine.posTo replaces root Euler Y" if model_id == "build_Checkpoint_Big" else "prefab root rotation retained",
        "runtime_root_source": source(ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/WorldMgrMediator.cs"),
        "scale_curve": curve(lod["body"]),
        "lod_distances": lod_distances(lod["body"]),
        "max_scale": float(parse_scalar(lod["body"], "m_max_scale")),
        "fade_in_time": float(parse_scalar(lod["body"], "m_fade_in_time")),
        "bounds_size": vec(bounds["body"], "m_size", "xy"),
        "ground_lod0": {"reference": ground_ref, "source": source(ground_path) if ground_path else (source(path) if ground_kind.startswith("local_") else None),
                        "kind": ground_kind, "local_game_object_name": ground_local_name,
                        "has_direct_sprite": ground_has_direct_sprite,
                        "is_child_visual": ground_has_direct_sprite,
                        "relation": "LevelDetailDeposit toggles this GameObject reference on LOD0/LOD1. External prefab asset references are not Transform children; no runtime instantiation was found, so no extra visible sprites are inferred."},
        "groups": groups, "items": items, "unsupported": unsupported,
        "grid_order": [g["id"] for g in groups] if len(groups) < 2 else None,
        "order_contract": "Source sibling order only. Equal-priority GridCollide registration order needs native Unity evidence.",
    }
    if model_id == "build_Rssgold_Big":
        native_path = ROOT / "Saved/RokUnityMigration/unity_gold_deposit_native_probe.json"
        if native_path.exists():
            native = json.loads(native_path.read_text(encoding="utf-8"))
            assert native["unityVersion"] == "2019.4.28f1c1"
            order = native["runs"][0]["samples"][0]["order"]
            assert len(order) == 13 and set(order) == {g["source_transform_id"] for g in groups}
            assert all(sample["order"] == order for run in native["runs"] for sample in run["samples"])
            mapped = {m["sourceTransformId"]: m for m in native["mappings"]}
            assert len(mapped) == 13
            for g in groups:
                mapping = mapped[g["source_transform_id"]]
                assert mapping["sourceName"] == g["name"]
                assert mapping["siblingIndex"] == g["sibling_index"]
                associated = [i for i in items if i["group_id"] == g["id"]]
                assert len(associated) == 1
                assert mapping["sourceRendererId"] == associated[0]["source_renderer_id"]
            result["grid_order"] = [next(g["id"] for g in groups if g["source_transform_id"] == fid) for fid in order]
            result["native_order_evidence"] = source(native_path)
            result["order_contract"] = "Observed Unity 2019.4.28f1c1 GridCollideMgr.Add/List.Sort order across three isolated instances; equal-priority sorting is not guaranteed across runtimes."
        render_path = ROOT / "Saved/RokUnityMigration/GoldReference/native_render.json"
        if render_path.exists():
            render = json.loads(render_path.read_text(encoding="utf-8"))
            assert render["unityVersion"] == "2019.4.28f1c1"
            assert render["graphicsDevice"] != "Null Device"
            assert [f["dxf"] for f in render["frames"]] == [500, 2500]
            assert [len(f["active"]) for f in render["frames"]] == [13, 1]
            result["native_render_evidence"] = source(render_path)
            result["native_render_frames"] = [source(Path(f["path"])) for f in render["frames"]]
    if model_id == "build_Checkpoint_Big":
        native_path = ROOT / "Saved/RokUnityMigration/unity_checkpoint_material_native_probe.json"
        if native_path.exists():
            native = json.loads(native_path.read_text(encoding="utf-8"))
            assert native["unityVersion"] == "2019.4.28f1c1"
            states = native["renderers"]
            assert len(states) == len(items) == 4
            assert sum(s["shader"] == "Unlit/Transparent" for s in states) == 2
            assert sum(s["shader"] == "custom/map/map_building_with_mask-5" for s in states) == 2
            assert all(s["enabled"] and s["activeInHierarchy"] for s in states)
            assert all(s["renderQueue"] == (2994 if s["shader"] == "Unlit/Transparent" else 2995) for s in states)
            result["native_material_evidence"] = source(native_path)
        mesh_path = ASSET_DIR / "checkpoint_mesh.json"
        if mesh_path.exists():
            mesh = json.loads(mesh_path.read_text(encoding="utf-8"))
            assert mesh["schema"] == "rok.checkpoint_mesh.v1"
            assert mesh["mesh_count"] == len(mesh["meshes"]) == 4
            assert mesh["vertex_count"] == 8724 and mesh["triangle_count"] == 5299
            assert mesh["source"]["prefab"]["sha256"] == result["source"]["sha256"]
            assert len(result["unsupported"]) == 1
            pending = result["unsupported"][0]
            assert mesh["source"]["fbx"]["sha256"] == pending["source"]["sha256"]
            assert all(m["sourceMeshGuid"] == "8051e1d671e7ca64e904093aadf2f537" for m in mesh["meshes"])
            assert all(m["activeInHierarchy"] and m["rendererEnabled"] and not m["groundLod0Only"] for m in mesh["meshes"])
            result["mesh_asset"] = {"export": "Content/RokData/WorldStructurePresentation/checkpoint_mesh.json",
                                    "sha256": hashlib.sha256(mesh_path.read_bytes()).hexdigest()}
            result["mesh_count"] = mesh["mesh_count"]
            result["mesh_vertex_count"] = mesh["vertex_count"]
            result["mesh_triangle_count"] = mesh["triangle_count"]
            result["mesh_source"] = mesh["source"]
            result["unsupported"] = []
    if model_id in ("build_BarbarianCitadel_Big", "build_HolyLand1_Big", "build_HolyLand2_Big"):
        native_path = ROOT / "Saved/RokUnityMigration/unity_structure_activity_native_probe.json"
        if native_path.exists():
            native = json.loads(native_path.read_text(encoding="utf-8"))
            samples = [s for s in native["samples"] if s["model"] == model_id]
            assert native["unityVersion"] == "2019.4.28f1c1"
            assert [s["dxf"] for s in samples] == [480, 900, 2500, 900, 480]
            assert [s["gridFlag"] for s in samples] == [False, False, False, True, True]
            assert all(s["groupGameObjectActive"] and s["spriteRendererEnabled"] and s["spriteGameObjectActive"] for s in samples)
            result["native_activity_evidence"] = source(native_path)
            result["activity_contract"] = "Serialized GridCollideItem.m_is_active is false, but the GameObject and SpriteRenderer remain active/visible at initial and increasing dxf; on decreasing dxf Activate flips the flag true."
    if len(items) != EXPECTED_SPRITES[model_id]:
        raise ValueError(f"{model_id}: expected {EXPECTED_SPRITES[model_id]} visible sprites, got {len(items)}; {unsupported}")
    return result


def validate(result):
    assert result["version"] == 1 and len(result["models"]) == 9
    for model_id, model in result["models"].items():
        assert len(model["items"]) == EXPECTED_SPRITES[model_id]
        group_ids = {g["id"] for g in model["groups"]}
        assert len(group_ids) == len(model["groups"])
        assert len({i["source_id"] for i in model["items"]}) == len(model["items"])
        for item in model["items"]:
            assert item["group_id"] is None or item["group_id"] in group_ids
            assert item["shader_mode"] in ("custom_map_building_mask", "unity_builtin_10750")
            assert item["sort_layer_id"] == 0 and item["sort_layer_order"] == 0
            for key in ("texture", "mask"):
                if item[key] is None:
                    assert key == "mask" and item["shader_mode"] == "unity_builtin_10750"
                    continue
                exported = resolve(item[key]["export"])
                with Image.open(exported) as image:
                    assert image.mode == "RGBA"
                    assert image.size == (item[key]["width"], item[key]["height"])
            assert all(math.isfinite(x) for field in ("position", "sprite_position", "right", "up", "size", "pivot") for x in item[field])
    return True


def build():
    guid_index = build_guid_index()
    image_cache = {}
    tag_manager = ROOT / "ref/client-unity/ProjectSettings/TagManager.asset"
    assert re.findall(r"(?m)^  - name: (.+)$\s*^    uniqueID: (\d+)$", tag_manager.read_text(encoding="utf-8")) == [("Default", "0")]
    result = {"version": 1, "coordinate_space": "unity_native",
              "unity_units_to_ue_cm": 500, "material_contract": "M_RokVillagePresentationDay",
              "sorting_layers_source": source(tag_manager),
              "models": {name: extract(name, guid_index, image_cache) for name in MODELS}}
    validate(result)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(f"Extracted {len(result['models'])} models, {sum(len(m['items']) for m in result['models'].values())} sprites: {OUT}")


if __name__ == "__main__":
    build()
