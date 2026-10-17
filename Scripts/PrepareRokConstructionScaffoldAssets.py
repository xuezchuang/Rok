"""Prepare the eight original Build_shelf_5 tight Sprite meshes and raw PNG inputs.

Offline only. The output has no material or runtime actor and is deliberately
not a rectangular sprite card. Existing prepared evidence is verified, never
replaced. Unity reference files are read-only.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / "Saved/UnityConstructionScaffoldProbe/OutputRuns/20261008_223208_818_37556/native_scaffold_render.json"
CONTRACT = ROOT / "Saved/UnityConstructionScaffoldProbe/source_contract.json"
PLAN = ROOT / "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/scaffold_ue_next_slice_plan.json"
OUT = ROOT / "Saved/RokUnityMigration/ConstructionScaffoldAssets"
PREPARED = OUT / "PreparedV1"
NATIVE_SHA = "b0855cba2b8f68fd620fa6c00e9417672af3faa7e5de25d35a75a3955d7c9e0b"
CONTRACT_SHA = "04e3d29acfef05aa791d262b62ff4193954a5edf2629c823aa53907a31b17013"
PACKAGE = "/Game/RokUnity/City/ConstructionScaffold/V1"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical_sha(value) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def identity():
    if sha(NATIVE) != NATIVE_SHA or sha(CONTRACT) != CONTRACT_SHA:
        raise RuntimeError("Pinned native scaffold capture/contract changed")
    native, contract, plan = read_json(NATIVE), read_json(CONTRACT), read_json(PLAN)
    if (not native["passed"] or native["failures"] or native["source_contract_sha256"] != CONTRACT_SHA
            or plan["pinned_evidence"]["native_sha256"] != NATIVE_SHA
            or plan["pinned_evidence"]["contract_sha256"] != CONTRACT_SHA
            or len(native["layers"]) != len(native["source_mask_bindings"]) != 8
            or len(contract["mask_bindings"]) != 8):
        raise RuntimeError("Scaffold native/contract source chain differs")
    # The sealed contract covers the prefab, material/shader, scripts, all
    # eight sprites/masks and four clips. Pin the current read-only ref tree,
    # not merely the historical native JSON's recorded paths.
    for item in contract["source_files"]:
        path = (ROOT / item["source_path"]).resolve()
        meta = Path(str(path) + ".meta")
        if (not path.is_relative_to((ROOT / "ref/client-unity").resolve())
                or not path.is_file() or not meta.is_file()
                or sha(path) != item["source_sha256"]
                or sha(meta) != item["meta_sha256"]):
            raise RuntimeError(f"Pinned scaffold Unity source changed: {item['source_path']}")
    return native, contract


def checked_source(item: dict, expected_file_id: int):
    relative = item["source_path"]
    path = (ROOT / relative).resolve()
    if not path.is_relative_to((ROOT / "ref/client-unity").resolve()):
        raise RuntimeError("Scaffold source escaped Unity reference tree")
    meta = Path(str(path) + ".meta")
    if (not path.is_file() or not meta.is_file() or sha(path) != item["sha256"]
            or sha(meta) != item["meta_sha256"] or item["file_id"] != expected_file_id):
        raise RuntimeError(f"Scaffold source file/hash/fileID differs: {relative}")
    m = re.search(r"^guid: ([0-9a-f]{32})$", meta.read_text(encoding="utf-8-sig"), re.M)
    if not m or m.group(1) != item["guid"]:
        raise RuntimeError(f"Scaffold source GUID differs: {relative}")
    return path, meta


def import_settings(meta: Path, size: tuple[int, int]):
    text = meta.read_text(encoding="utf-8-sig")
    def value(name):
        m = re.search(r"^\s*" + re.escape(name) + r":\s*([^\s#]+)", text, re.M)
        if not m:
            raise RuntimeError(f"Missing source TextureImporter {name}: {meta}")
        return m.group(1)
    result = {name: value(name) for name in (
        "enableMipMap", "sRGBTexture", "maxTextureSize", "spriteMode", "spriteMeshType",
        "spritePixelsToUnits", "filterMode", "wrapU", "wrapV", "alphaIsTransparency")}
    if (result["enableMipMap"] != "0" or result["spriteMode"] != "1"
            or result["spritePixelsToUnits"] != "100"
            or int(result["maxTextureSize"]) < max(size)):
        raise RuntimeError(f"Unexpected source texture import contract: {meta}")
    return result


def png_size(path: Path):
    header = path.read_bytes()[:24]
    if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise RuntimeError(f"Scaffold source is not PNG: {path}")
    return struct.unpack(">II", header[16:24])


def obj_text(layer: dict, name: str):
    verts, uvs, indices = layer["vertices"], layer["uv"], layer["triangles"]
    if (len(verts) != len(uvs) or len(verts) < 3 or len(indices) < 3
            or len(indices) % 3 or any(not isinstance(i, int) or i < 0 or i >= len(verts) for i in indices)):
        raise RuntimeError(f"Bad source mesh topology: {name}")
    for pair in verts + uvs:
        if any(not math.isfinite(pair[k]) for k in ("x", "y")):
            raise RuntimeError(f"Nonfinite source mesh coordinate: {name}")
    # Preserve the same verified Unity->UE local Sprite convention as the
    # Farm/Wood source mesh importer. Prefab child transforms remain separate.
    lines = ["# Unity2019 Sprite.mesh tight polygon; pivot-local; no prefab pose", "o " + name]
    lines += [f"v {-v['x']*100:.9f} 0.000000000 {v['y']*100:.9f}" for v in verts]
    lines += [f"vt {v['x']:.9f} {v['y']:.9f}" for v in uvs]
    lines.append("vn 0.000000000 1.000000000 0.000000000")
    for off in range(0, len(indices), 3):
        a, b, c = (indices[off + k] + 1 for k in (2, 1, 0))
        lines.append(f"f {a}/{a}/1 {b}/{b}/1 {c}/{c}/1")
    return "\n".join(lines) + "\n"


def write_once(path: Path, content: bytes):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if path.read_bytes() != content:
            raise RuntimeError(f"Refusing to overwrite existing scaffold preparation: {path}")
        return
    with path.open("xb") as stream:
        stream.write(content)


def prepare():
    native, contract = identity()
    bindings = {(x["scaffold"], x["sprite_node"]): x for x in contract["mask_bindings"]}
    rows = []
    for layer in native["layers"]:
        parts = layer["node"].split("/")
        scaffold = parts[-2].split("[")[0]
        node = parts[-1].split("[")[0]
        binding = bindings.get((scaffold, node))
        if not binding:
            raise RuntimeError(f"Unbound source renderer: {layer['node']}")
        sprite, mask = layer["sprite"], layer["mask_sprite"]
        if (sprite["guid"] != binding["sprite_guid"] or mask["guid"] != binding["mask_guid"]
                or sprite["file_id"] != binding["sprite_file_id"]
                or mask["file_id"] != binding["mask_file_id"]
                or sprite["sha256"] != binding["sprite_sha256"]
                or mask["sha256"] != binding["mask_sha256"]
                or layer["mask_texture_sha256"] != mask["sha256"]
                or not layer["mask_property_block_verified"]):
            raise RuntimeError(f"Source sprite/per-renderer MaskSprite binding differs: {layer['node']}")
        name = Path(sprite["source_path"]).stem
        dst = PREPARED / name
        sources = {}
        for role, item in (("sprite", sprite), ("mask", mask)):
            source_path, meta = checked_source(item, 21300000)
            size = png_size(source_path)
            settings = import_settings(meta, size)
            copy = dst / source_path.name
            write_once(copy, source_path.read_bytes())
            sources[role] = {"path": item["source_path"], "guid": item["guid"],
                             "file_id": item["file_id"], "sha256": sha(source_path),
                             "meta_sha256": sha(meta), "prepared_file": copy.relative_to(OUT).as_posix(),
                             "size": list(size), "unity_import": settings,
                             "ue_asset": f"{PACKAGE}/T_{name}_{role.capitalize()}Raw"}
        rect, pivot = layer["sprite_rect"], layer["pivot"]
        if (rect["width"] != sources["sprite"]["size"][0] or
                rect["height"] != sources["sprite"]["size"][1] or
                abs(pivot["x"] - rect["width"]/2) > 1e-5 or
                abs(pivot["y"] - rect["height"]/2) > 1e-5 or
                layer["pixels_per_unit"] != 100):
            raise RuntimeError(f"Unexpected source Sprite rect/pivot/PPU: {name}")
        obj_name = f"SM_{name}_SourceSpriteMesh"
        obj = dst / (obj_name + ".obj")
        write_once(obj, obj_text(layer, obj_name).encode())
        geom = {k: layer[k] for k in ("sprite_rect", "texture_rect", "pivot",
                                     "pixels_per_unit", "vertices", "uv", "triangles")}
        areas = []
        v = layer["vertices"]
        for i in range(0, len(layer["triangles"]), 3):
            a,b,c = [v[layer["triangles"][i+j]] for j in range(3)]
            areas.append((b["x"]-a["x"])*(c["y"]-a["y"])-(b["y"]-a["y"])*(c["x"]-a["x"]))
        rows.append({"name": name, "node": layer["node"], "scaffold": scaffold,
                     "sprite_node": node, "sources": sources, "geometry": geom,
                     "geometry_sha256": canonical_sha(geom), "obj_file": obj.relative_to(OUT).as_posix(),
                     "obj_sha256": sha(obj), "vertex_count": len(v),
                     "triangle_count": len(areas),
                     "zero_area_triangles": [i for i,a in enumerate(areas) if a == 0],
                     "sorting_layer_id": layer["sorting_layer_id"],
                     "sorting_order": layer["sorting_order"], "renderer_color": layer["color"],
                     "ue_mesh_asset": f"{PACKAGE}/{obj_name}"})
    if len(rows) != 8 or len({r["name"] for r in rows}) != 8:
        raise RuntimeError("Expected eight distinct source Sprite.mesh layers")
    manifest = {"schema": "rok.construction_scaffold.assets_prepared.v1",
                "scope": "Eight source polygon meshes and 16 raw sprite/mask inputs only; no material, actor or world edit",
                "native_path": NATIVE.relative_to(ROOT).as_posix(), "native_sha256": NATIVE_SHA,
                "contract_path": CONTRACT.relative_to(ROOT).as_posix(), "contract_sha256": CONTRACT_SHA,
                "plan_sha256": sha(PLAN), "ue_root": PACKAGE,
                "coordinate_mapping": "Unity local Sprite (x,y) -> UE local (-x,0,y)*100; OBJ faces (c,b,a); prefab transform separate",
                "rendering_limit": "Raw input textures/meshes are not source Gamma map_building_with_mask-5 compositing; no main-view or particle acceptance",
                "layers": rows}
    target = OUT / "prepared_manifest.json"
    write_once(target, (json.dumps(manifest,ensure_ascii=False,indent=2)+"\n").encode())
    return target


if __name__ == "__main__":
    path = prepare()
    print(json.dumps({"prepared": str(path), "sha256": sha(path), "layers": 8}))
