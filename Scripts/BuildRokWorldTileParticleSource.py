"""Extract the two source bowen systems and their exact Unity asset chain, read-only."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREFAB = ROOT / "ref/client-unity/Assets/BundleAssets/land/Tile_lod2/Tile-JuDian-A2_lod2.prefab"
MATERIAL = ROOT / "ref/client-unity/Assets/BundleAssets/land/Material/Ring_06_light.mat"
TEXTURE = ROOT / "ref/client-unity/Assets/BundleAssets/land/Texture/Ring_07.png"
ADDRESS_GROUP = ROOT / "ref/client-unity/Assets/AddressableAssetsData/AssetGroups/MapTile.asset"
MAP_MANAGER = ROOT / "ref/client-unity/Assets/Scripts/Client/MapManager/MapManager.cs"
MAP_CELL = ROOT / "ref/client-unity/Assets/Scripts/Client/MapManager/Piece/MapCellDynamic.cs"
OUTPUT = ROOT / "Saved/RokUnityMigration/BatchReview20261009WorldSearchAndTileEffects/TileParticleSource/source_v1.json"
MATERIAL_GUID = "707a424589ea046458deff5a47ad40ff"
TEXTURE_GUID = "f05537e81d3b5464a985ff05e06cb464"
HEADER = re.compile(r"^--- !u!(\d+) &(\d+)\s*$", re.M)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inline(value: str):
    value = value.strip()
    if value in ("[]", "{}"):
        return [] if value == "[]" else {}
    if value.startswith("{") and value.endswith("}"):
        result = {}
        for part in value[1:-1].split(","):
            key, sep, item = part.partition(":")
            if not sep:
                raise ValueError(f"bad inline Unity map: {value}")
            result[key.strip()] = inline(item)
        return result
    if re.fullmatch(r"[+-]?\d+", value):
        return int(value)
    if re.fullmatch(r"[+-]?(?:\d+\.\d*|\.\d+)(?:[eE][+-]?\d+)?", value):
        return float(value)
    return value


def parse_lines(raw: str):
    lines = []
    pending = ""
    for line in raw.splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if pending:
            pending += " " + line.strip()
            if pending.count("{") == pending.count("}"):
                lines.append((len(pending) - len(pending.lstrip(" ")), pending.strip()))
                pending = ""
        elif line.count("{") > line.count("}"):
            pending = line
        else:
            lines.append((len(line) - len(line.lstrip(" ")), line.strip()))
    if pending:
        raise ValueError("unterminated inline Unity map")

    def block(index: int, indent: int):
        if index >= len(lines):
            return {}, index
        if lines[index][1].startswith("- "):
            values = []
            while index < len(lines) and lines[index][0] == indent and lines[index][1].startswith("- "):
                item = lines[index][1][2:]
                index += 1
                if item.startswith("{") or ":" not in item:
                    values.append(inline(item))
                else:
                    key, value = item.split(":", 1)
                    entry = {}
                    if value.strip():
                        entry[key] = inline(value)
                    elif index < len(lines) and lines[index][0] > indent + 2:
                        entry[key], index = block(index, lines[index][0])
                    else:
                        entry[key] = ""
                    if index < len(lines) and lines[index][0] > indent:
                        rest, index = block(index, lines[index][0])
                        if not isinstance(rest, dict):
                            raise ValueError(f"list-map continuation is not a map: {item}")
                        entry.update(rest)
                    values.append(entry)
            return values, index
        result = {}
        while index < len(lines) and lines[index][0] == indent and not lines[index][1].startswith("- "):
            entry = lines[index][1]
            key, sep, value = entry.partition(":")
            if not sep:
                raise ValueError(f"unsupported Unity YAML line: {entry}")
            index += 1
            if value.strip():
                result[key] = inline(value)
            elif index < len(lines) and (lines[index][0] > indent
                                           or (lines[index][0] == indent and lines[index][1].startswith("- "))):
                result[key], index = block(index, lines[index][0])
            else:
                result[key] = ""
        return result, index

    if not lines:
        return {}
    value, end = block(0, lines[0][0])
    if end != len(lines):
        raise ValueError(f"Unity YAML subset parser stopped at {end}/{len(lines)}: {lines[end]}")
    return value


def documents(path: Path):
    text = path.read_text(encoding="utf-8-sig")
    matches = list(HEADER.finditer(text))
    result = {}
    for index, match in enumerate(matches):
        raw = text[match.end():matches[index + 1].start() if index + 1 < len(matches) else len(text)]
        try:
            parsed = parse_lines(raw)
        except ValueError as exc:
            raise ValueError(f"Unity document {match.group(2)}: {exc}") from exc
        if len(parsed) != 1:
            raise ValueError(f"Unity object has multiple roots: {match.group(2)}")
        typ, data = next(iter(parsed.items()))
        result[int(match.group(2))] = {"unity_class_id": int(match.group(1)),
                                       "unity_type": typ, "data": data}
    return result


def component(objects: dict, go: dict, typ: str):
    for reference in go["m_Component"]:
        file_id = reference["component"]["fileID"]
        if objects[file_id]["unity_type"] == typ:
            return file_id, objects[file_id]["data"]
    raise ValueError(f"missing {typ} for {go['m_Name']}")


def source_file(path: Path, guid: str | None = None):
    item = {"path": path.relative_to(ROOT).as_posix(), "sha256": sha(path),
            "bytes": path.stat().st_size}
    meta = Path(str(path) + ".meta")
    if meta.exists():
        item["meta_path"] = meta.relative_to(ROOT).as_posix()
        item["meta_sha256"] = sha(meta)
        match = re.search(r"^guid: ([0-9a-f]{32})$", meta.read_text(encoding="utf-8"), re.M)
        item["guid"] = match.group(1) if match else None
        if guid and item["guid"] != guid:
            raise ValueError(f"GUID mismatch for {path}")
    return item


def ancestor_chain(objects: dict, transform: dict):
    chain = []
    current = transform
    while True:
        go_id = current["m_GameObject"]["fileID"]
        go = objects[go_id]["data"]
        chain.append({"game_object_file_id": go_id, "name": go["m_Name"],
                      "active_self": bool(go["m_IsActive"]),
                      "transform_file_id": component(objects, go, "Transform")[0],
                      "local_position": current["m_LocalPosition"],
                      "local_rotation": current["m_LocalRotation"],
                      "local_scale": current["m_LocalScale"],
                      "local_euler_hint": current["m_LocalEulerAnglesHint"],
                      "root_order": current["m_RootOrder"]})
        parent_id = current["m_Father"]["fileID"]
        if not parent_id:
            break
        current = objects[parent_id]["data"]
    return chain


def build():
    objects = documents(PREFAB)
    material = documents(MATERIAL)[2100000]["data"]
    root = [entry for entry in objects.values() if entry["unity_type"] == "GameObject"
            and entry["data"]["m_Name"] == "Tile-JuDian-A2_lod2"]
    if len(root) != 1:
        raise ValueError("LOD2 root identity ambiguous")
    systems = []
    for go_id, entry in objects.items():
        if entry["unity_type"] != "GameObject" or entry["data"]["m_Name"] not in ("bowen", "bowen (1)"):
            continue
        go = entry["data"]
        transform_id, transform = component(objects, go, "Transform")
        system_id, system = component(objects, go, "ParticleSystem")
        renderer_id, renderer = component(objects, go, "ParticleSystemRenderer")
        materials = renderer["m_Materials"]
        if not materials or any(item.get("guid") != MATERIAL_GUID for item in materials):
            raise ValueError(f"bowen renderer material identity mismatch: {go['m_Name']}")
        systems.append({"name": go["m_Name"], "game_object_file_id": go_id,
                        "transform_file_id": transform_id, "particle_system_file_id": system_id,
                        "particle_renderer_file_id": renderer_id,
                        "ancestor_chain_child_to_root": ancestor_chain(objects, transform),
                        "particle_system": system, "renderer": renderer})
    systems.sort(key=lambda entry: entry["name"])
    if [item["name"] for item in systems] != ["bowen", "bowen (1)"]:
        raise ValueError("expected exactly two bowen systems")
    main = systems[0]
    if main["particle_system"]["lengthInSec"] != 5 or main["particle_system"]["looping"] != 1:
        raise ValueError("main bowen timing drift")
    if main["ancestor_chain_child_to_root"][0]["local_position"] != {"x": -2.04478, "y": -0.03, "z": -1.8406}:
        raise ValueError("main bowen transform drift")
    main_tex = next(item["_MainTex"] for item in material["m_SavedProperties"]["m_TexEnvs"]
                    if "_MainTex" in item)
    if main_tex["m_Texture"]["guid"] != TEXTURE_GUID:
        raise ValueError("Ring_06_light texture GUID drift")
    address_text = ADDRESS_GROUP.read_text(encoding="utf-8-sig")
    if not re.search(r"m_GUID: 06e49016cd7a70648baf210c76991d88\s+"
                     r"m_Address: Tile-JuDian-A2_lod2", address_text):
        raise ValueError("LOD2 addressable GUID/name chain changed")
    manager_text = MAP_MANAGER.read_text(encoding="utf-8-sig")
    # Legacy C# comments are not UTF-8; only ASCII syntax markers are inspected.
    cell_text = MAP_CELL.read_bytes().decode("latin-1")
    for marker in ("tile_id + \"_\" + str", "lod == 2", "lod == 3", "lod == 4",
                   "GetMapDataTile()", "m_current_map_data_tile_detail"):
        if marker not in manager_text:
            raise ValueError(f"MapManager LOD selection marker missing: {marker}")
    for marker in ("CoreUtils.assetService.Instantiate(mapObjectData.m_prefab_id",
                   "gameObject.name.Contains(\"_lod2\")"):
        if marker not in cell_text:
            raise ValueError(f"MapCellDynamic runtime load marker missing: {marker}")
    return {"schema": "rok.world_tile_particle_source.v1", "status": "static_source_oracle_only",
            "source_files": {"prefab": source_file(PREFAB),
                             "material": source_file(MATERIAL, MATERIAL_GUID),
                             "texture": source_file(TEXTURE, TEXTURE_GUID),
                             "addressable_group": source_file(ADDRESS_GROUP),
                             "map_manager": source_file(MAP_MANAGER),
                             "map_cell_dynamic": source_file(MAP_CELL)},
            "material": material, "systems": systems,
            "lod_activation_source_chain": {
                "addressable_name": "Tile-JuDian-A2_lod2",
                "prefab_guid": "06e49016cd7a70648baf210c76991d88",
                "map_manager": "DoReadDataFromTileBrief builds tile_id + '_lod' + lod for LOD 2, 3, 4 detail; GetMapDataTile selects detail for 2/3/4, brief at 5; 0/1 use independent adorning/plane data.",
                "map_cell_dynamic": "Instantiates MapObjectData.m_prefab_id only inside camera-ground-rectangle; destroys outside rectangle; _lod2 branch updates LockRotate180 after tile Y rotation.",
                "source_scope_limit": "This proves asset naming/selection logic and prefab activeSelf, not that a specific live map tile is instantiated or rendered."
            },
            "static_limits": [
                "Unity built-in shader fileID alone does not establish the resolved shader name or exact blend render state.",
                "Serialized particle settings do not prove native runtime visible count, culling behavior, camera-facing geometry, final blending or LOD activation in the game map.",
                "The LOD2 suffix is a prefab asset identity; no LODGroup component is present in this prefab. Runtime selection is external and needs a separate source call-chain audit."
            ]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()
    data = build()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    payload = (json.dumps(data, ensure_ascii=False, indent=2) + "\n").replace("\n", "\r\n").encode("utf-8")
    if args.output.exists() and args.output.read_bytes() != payload:
        raise ValueError("existing generated source oracle differs; preserve it and write a revision")
    if not args.output.exists():
        args.output.write_bytes(payload)
    print(json.dumps({"output": str(args.output), "sha256": sha(args.output),
                      "systems": [item["name"] for item in data["systems"]]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
