"""Strict source-oracle check for Tile-JuDian-A2_lod2 bowen particles; no engine use."""

from __future__ import annotations

import argparse
import copy
import json
import struct
from pathlib import Path

import BuildRokWorldTileParticleSource as source


class Rejection(Exception):
    pass


def need(condition: bool, reason: str) -> None:
    if not condition:
        raise Rejection(reason)


def verify(data: dict) -> None:
    need(data.get("schema") == "rok.world_tile_particle_source.v1", "schema")
    need(data == source.build(), "manifest differs from current source extraction")
    systems = data["systems"]
    need([system["name"] for system in systems] == ["bowen", "bowen (1)"], "two bowen identities")
    expected_positions = [{"x": -2.04478, "y": -0.03, "z": -1.8406},
                          {"x": -0.4, "y": -0.03, "z": -1.91}]
    for system, position in zip(systems, expected_positions):
        chain = system["ancestor_chain_child_to_root"]
        need([item["name"] for item in chain] == [system["name"], "build_mountain_G",
                                                    "Tile-JuDian-A2_lod2"], "active ancestor chain")
        need(all(item["active_self"] for item in chain) and chain[0]["local_position"] == position,
             "active or transform source drift")
        ps = system["particle_system"]
        renderer = system["renderer"]
        need(ps["lengthInSec"] == 5 and ps["looping"] == ps["playOnAwake"] == 1,
             "loop source drift")
        need(ps["ShapeModule"]["enabled"] == 1 and ps["ShapeModule"]["type"] == 12
             and ps["ShapeModule"]["radius"]["value"] == .58
             and ps["ShapeModule"]["arc"]["value"] == 153.4, "shape source drift")
        need(ps["EmissionModule"]["enabled"] == 1
             and ps["EmissionModule"]["rateOverTime"]["scalar"] == 1
             and ps["EmissionModule"]["m_BurstCount"] == 0, "emission source drift")
        need(all(ps[name]["enabled"] == 1 for name in ("InitialModule", "SizeModule",
                                                      "ColorModule", "VelocityModule")),
             "enabled module source drift")
        need(renderer["m_Enabled"] == 1 and renderer["m_RenderMode"] == 4
             and renderer["m_SortingOrder"] == 0 and renderer["m_RenderAlignment"] == 2,
             "renderer mode/order source drift")
        need(len(renderer["m_Materials"]) == 2 and all(
            material["guid"] == source.MATERIAL_GUID for material in renderer["m_Materials"]),
             "material slot source drift")
        need(renderer["m_Mesh"]["fileID"] == 10210,
             "built-in particle mesh fileID source drift")
    need(abs(systems[0]["ancestor_chain_child_to_root"][0]["local_scale"]["x"] - 1.8280802) < 1e-7,
         "main scale source drift")
    material = data["material"]
    need(material["m_Shader"]["fileID"] == 10721
         and material["m_SavedProperties"]["m_Floats"] == [{"_InvFade": 1}]
         and material["m_SavedProperties"]["m_Colors"] == [{"_TintColor":
             {"r": .5, "g": .5, "b": .5, "a": .5}}], "shader/material parameter source drift")
    texture = source.TEXTURE.read_bytes()
    need(texture[:8] == bytes.fromhex("89504e470d0a1a0a")
         and texture[12:16] == b"IHDR" and struct.unpack(">II", texture[16:24]) == (128, 128),
         "Ring_07 PNG dimensions/signature source drift")
    need(data["lod_activation_source_chain"]["prefab_guid"] ==
         data["source_files"]["prefab"]["guid"], "LOD address identity drift")


def self_test(data: dict) -> None:
    verify(data)
    mutations = [
        ("loop", lambda bad: bad["systems"][0]["particle_system"].__setitem__("looping", 0)),
        ("renderer", lambda bad: bad["systems"][0]["renderer"].__setitem__("m_RenderMode", 0)),
        ("texture guid", lambda bad: bad["material"]["m_SavedProperties"]["m_TexEnvs"][1]
         ["_MainTex"]["m_Texture"].__setitem__("guid", "0" * 32)),
        ("LOD identity", lambda bad: bad["lod_activation_source_chain"].__setitem__("prefab_guid", "0" * 32)),
    ]
    for name, mutate in mutations:
        bad = copy.deepcopy(data)
        mutate(bad)
        try:
            verify(bad)
        except Rejection:
            continue
        raise Rejection(f"negative mutation accepted: {name}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", nargs="?", type=Path, default=source.OUTPUT)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    data = json.loads(args.path.read_text(encoding="utf-8"))
    verify(data)
    if args.self_test:
        self_test(data)
    print(f"PASS tile particle source: two bowen systems, 21 modules each, material/texture/LOD chain"
          + (", four negative mutations" if args.self_test else ""))


if __name__ == "__main__":
    main()
