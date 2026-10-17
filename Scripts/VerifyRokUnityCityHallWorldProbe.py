"""Strict static and optional native-run verification of the isolated Hall fixture."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityHallWorldProbeV1"
CONTRACT = PROBE / "hall_world_contract_v4.json"
SOURCE_SCENE = ROOT / "ref/client-unity/Assets/BundleAssets/Scenes/GameApp.unity"
SOURCE_HALL = ROOT / "ref/client-unity/Assets/BundleAssets/Scene/City_building/city_building_1/build_1_101_1.prefab"
SOURCE_CONTAINER = ROOT / "ref/client-unity/Assets/BundleAssets/Scene/City/city_function/CityBuildingContainer.prefab"
FLOAT_TOLERANCE = 3.0e-5  # Unity float32 TRS/matrix multiplication and decimal JsonUtility output.


def need(ok: bool, reason: str) -> None:
    if not ok:
        raise AssertionError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inside(base: Path, name: str) -> Path:
    result = (base / name).resolve()
    need(result.is_relative_to(base.resolve()), f"Path escapes expected directory: {name}")
    return result


def f32(value: float) -> bytes:
    return struct.pack("<f", float(value))


def near(actual: float, expected: float) -> bool:
    return math.isfinite(actual) and abs(actual - expected) <= FLOAT_TOLERANCE * max(1.0, abs(expected))


def vector(source: str, field: str, names: tuple[str, ...]) -> list[float]:
    match = re.search(rf"  {field}: \{{([^}}]+)\}}", source)
    need(match is not None, f"Source TRS field missing: {field}")
    values = dict((key.strip(), float(value.strip())) for key, value in
                  (part.split(":", 1) for part in match.group(1).split(",")))
    need(set(values) == set(names), f"Source TRS components differ: {field}")
    return [values[key] for key in names]


def source_prefab(path: Path) -> dict:
    text = path.read_text(encoding="utf-8-sig")
    docs = {}
    for match in re.finditer(r"--- !u!(\d+) &(-?\d+)(?: stripped)?\n([^\n]+):\n(.*?)(?=--- !u!|\Z)", text, re.S):
        docs[match.group(2)] = (int(match.group(1)), match.group(4))
    gos, transforms, renderers = {}, {}, {}
    for fid, (kind, body) in docs.items():
        if kind == 1:
            name = re.search(r"  m_Name: (.*)", body)
            active = re.search(r"  m_IsActive: ([01])", body)
            need(name is not None and active is not None, "Prefab GameObject source fields absent")
            gos[fid] = {"name": name.group(1), "active": active.group(1) == "1"}
        elif kind in (4, 224):
            go = re.search(r"  m_GameObject: \{fileID: (-?\d+)\}", body)
            parent = re.search(r"  m_Father: \{fileID: (-?\d+)\}", body)
            children = re.search(r"  m_Children:(?: \[\])?\n(.*?)  m_Father:", body, re.S)
            need(go is not None and parent is not None and children is not None, "Prefab Transform hierarchy absent")
            transforms[fid] = {"go": go.group(1), "parent": parent.group(1), "component_kind": kind,
                               "children": re.findall(r"- \{fileID: (-?\d+)\}", children.group(1)),
                               "position": vector(body, "m_LocalPosition", ("x", "y", "z")),
                               "rotation": vector(body, "m_LocalRotation", ("x", "y", "z", "w")),
                               "scale": vector(body, "m_LocalScale", ("x", "y", "z"))}
            if kind == 224:
                transforms[fid]["anchored_position"] = vector(body, "m_AnchoredPosition", ("x", "y"))
                transforms[fid]["anchor_min"] = vector(body, "m_AnchorMin", ("x", "y"))
                transforms[fid]["anchor_max"] = vector(body, "m_AnchorMax", ("x", "y"))
        elif kind == 212:
            go = re.search(r"  m_GameObject: \{fileID: (-?\d+)\}", body)
            enabled = re.search(r"  m_Enabled: ([01])", body)
            need(go is not None and enabled is not None, "Prefab SpriteRenderer source fields absent")
            renderers[fid] = {"go": go.group(1), "enabled": enabled.group(1) == "1"}
    roots = [fid for fid, spec in transforms.items() if spec["parent"] == "0"]
    need(len(roots) == 1, "Prefab root count differs")
    for fid, t in transforms.items():
        need(t["go"] in gos and all(ch in transforms for ch in t["children"]),
             f"Prefab hierarchy source missing at {fid}: go={t['go']} children={t['children']}")
    meta = Path(str(path) + ".meta").read_text(encoding="utf-8")
    guid = re.search(r"guid: ([0-9a-f]{32})", meta)
    need(guid is not None, "Prefab source GUID absent")
    return {"root": roots[0], "gos": gos, "transforms": transforms, "renderers": renderers,
            "guid": guid.group(1)}


def matmul(a: list[float], b: list[float]) -> list[float]:
    return [sum(a[row * 4 + k] * b[k * 4 + col] for k in range(4))
            for row in range(4) for col in range(4)]


def trs(position: list[float], quaternion: list[float], scale: list[float]) -> list[float]:
    x, y, z, w = quaternion
    # Unity localToWorld uses column vectors: ParentWorld * T * R(quaternion) * S.
    rot = [[1 - 2 * (y*y + z*z), 2 * (x*y - z*w), 2 * (x*z + y*w)],
           [2 * (x*y + z*w), 1 - 2 * (x*x + z*z), 2 * (y*z - x*w)],
           [2 * (x*z - y*w), 2 * (y*z + x*w), 1 - 2 * (x*x + y*y)]]
    return [rot[row][col] * scale[col] if col < 3 else position[row]
            for row in range(3) for col in range(4)] + [0.0, 0.0, 0.0, 1.0]


def yaw(degrees: float) -> list[float]:
    radians = math.radians(degrees) * 0.5
    return [0.0, math.sin(radians), 0.0, math.cos(radians)]


def expected_prefab_nodes(prefab: dict, phase: str, parent_world: list[float],
                          root_override: dict | None = None) -> dict[tuple[str, str, str, str], dict]:
    expected = {}
    def walk(fid: str, prefix: str, parent: list[float], inherited_active: bool, sibling: int) -> None:
        spec = prefab["transforms"][fid]
        go = prefab["gos"][spec["go"]]
        path = go["name"] if not prefix else prefix + "/" + go["name"] + f"[{sibling}]"
        position = root_override["position"] if prefix == "" and root_override else spec["position"]
        if spec["component_kind"] == 224:
            # Unity imports the sole Ground RectTransform under a plain Transform.
            # Its serialized m_LocalPosition.xy is stale; anchoredPosition resolves the
            # actual Transform.localPosition.xy. Keep the source Z component.
            parent_spec = prefab["transforms"][spec["parent"]]
            need(parent_spec["component_kind"] == 4 and spec["anchor_min"] == [0.5, 0.5] and
                 spec["anchor_max"] == [0.5, 0.5], "RectTransform anchor/parent source rule differs")
            position = [*spec["anchored_position"], spec["position"][2]]
        rotation = root_override["rotation"] if prefix == "" and root_override else spec["rotation"]
        scale = spec["scale"]
        world = matmul(parent, trs(position, rotation, scale))
        active = inherited_active and go["active"]
        common = {"matrix": world, "position": position, "rotation": rotation, "scale": scale,
                  "active_self": go["active"], "active_in_hierarchy": active, "guid": prefab["guid"]}
        expected[(phase, path, "Transform", fid)] = {**common, "enabled": True}
        matching = [(rid, renderer) for rid, renderer in prefab["renderers"].items() if renderer["go"] == spec["go"]]
        for rid, renderer in matching:
            expected[(phase, path, "SpriteRenderer", rid)] = {**common, "enabled": renderer["enabled"]}
        for index, child in enumerate(spec["children"]):
            walk(child, path, world, active, index)
    # AssetDatabase.LoadAssetAtPath returns persistent prefab assets outside any
    # loaded scene. Their activeSelf is serialized, but activeInHierarchy is false.
    walk(prefab["root"], "", parent_world, not phase.startswith("prefab_default_"), 0)
    return expected


def verify_contract() -> dict:
    c = json.loads(CONTRACT.read_text(encoding="utf-8"))
    need(c["schema"] == "rok.unity_city_hall_world_probe_contract.v4", "Contract schema differs")
    need(Path(c["probe_root"]).resolve() == PROBE.resolve(), "Contract project path differs")
    need(c["qualification"] == "transform_fixture_only" and c["lod_qualified"] is False, "LOD scope differs")
    need(c["unity_version"] == "2019.4.28f1c1", "Unity version differs")
    need(sha(ROOT / c["producer_path"]) == c["producer_sha256"], "Producer hash differs")
    need(sha(ROOT / c["exporter_path"]) == c["exporter_sha256"], "Exporter hash differs")
    for path, digest in c["source_paths_sha256"].items():
        need(sha(inside(ROOT, path)) == digest, f"Pinned source differs: {path}")
    scene = SOURCE_SCENE.read_text(encoding="utf-8-sig")
    for root in c["scene_roots"]:
        goid, tid, parent, name = (root[key] for key in ("game_object_file_id", "transform_file_id", "parent_transform_file_id", "name"))
        go = scene.split(f"--- !u!1 &{goid}\n", 1)[1].split("--- !u!", 1)[0]
        transform = scene.split(f"--- !u!4 &{tid}\n", 1)[1].split("--- !u!", 1)[0]
        need(f"m_Name: {name}" in go and "m_IsActive: 1" in go, "Source root identity/active differs")
        need(f"m_Father: {{fileID: {parent}}}" in transform, "Source parent differs")
        need("m_LocalPosition: {x: 0, y: 0, z: 0}" in transform and
             "m_LocalRotation: {x: 0, y: 0, z: 0, w: 1}" in transform and
             "m_LocalScale: {x: 1, y: 1, z: 1}" in transform, "Source root TRS differs")
    for item in c["settings_files"]:
        need(sha(inside(ROOT, item["source_path"])) == item["sha256"], "Sealed setting source differs")
        need(sha(inside(ROOT, item["probe_path"])) == item["sha256"], "Copied setting differs")
    need(len(c["settings_files"]) == 21, "Settings/package count differs")
    seen = set()
    for item in c["source_files"]:
        source = inside(ROOT, item["source_path"])
        need(item["source_path"] not in seen and item["copied"] is True, "Copied source identity duplicate")
        seen.add(item["source_path"])
        need(sha(source) == item["sha256"], "Source asset differs")
        asset = inside(PROBE, item["probe_asset_path"])
        need(sha(asset) == item["sha256"], "Probe asset differs")
        need(sha(inside(ROOT, item["meta_path"])) == item["meta_sha256"] == sha(Path(str(asset) + ".meta")), "Source meta differs")
    h = c["hall"]
    need((h["init_id"], h["type_id"], h["level"], h["grid_x"], h["grid_y"], h["footprint_width"], h["footprint_length"], h["visual_civilization"], h["age"]) == (5, 1, 1, -5, 5, 9, 9, 101, 1), "Hall fixture identity differs")
    need(h["city_position_x"] == h["city_position_y"] == 0, "Isolated city position differs")
    need(h["expected_renderer_file_id"] == "212751466141171401", "Source renderer fileID differs")
    hall = source_prefab(SOURCE_HALL)
    container = source_prefab(SOURCE_CONTAINER)
    need(h["expected_hall_guid"] == hall["guid"] == "c825e8897df8e424e817e4ed05eef53e" and
         h["expected_container_guid"] == container["guid"] == "db18574bd8f02dd49a79776c62b2d55b", "Exact source prefab GUID differs")
    need(h["expected_renderer_file_id"] in hall["renderers"], "Exact source renderer fileID absent")
    ground = hall["transforms"]["993110904887454094"]
    need(ground["component_kind"] == 224 and ground["parent"] == hall["root"] and
         ground["anchor_min"] == ground["anchor_max"] == [0.5, 0.5] and
         all(f32(a) == f32(b) for a, b in zip(ground["anchored_position"], [-0.012, 0])) and
         f32(ground["position"][2]) == f32(-0.002),
         "Source Ground RectTransform imported position rule differs")
    need(len(container["transforms"][container["root"]]["children"]) == 0,
         "Container prefab source children changed; dynamic attachment contract must be reconsidered")
    need(c["dynamic_attach_contract"].startswith("Container source children retain their original order; one new Hall prefab child is appended"),
         "Dynamic Hall attach contract differs")
    need(c["prefab_asset_active_contract"] == "AssetDatabase persistent prefab asset retains serialized activeSelf but activeInHierarchy is false; runtime instances inherit scene parent activity",
         "Persistent prefab active-state contract differs")
    need(c["rect_transform_contract"] == "Ground RectTransform under ordinary Hall Transform has anchorMin=anchorMax=(0.5,0.5); Unity imported localPosition.xy resolves from serialized anchoredPosition.xy=(-0.012,0), retaining localPosition.z=-0.002",
         "Ground RectTransform imported position contract differs")
    need(near(hall["transforms"][hall["root"]]["rotation"][1], -0.38268343) and
         near(hall["transforms"][hall["root"]]["rotation"][3], 0.92387956), "Source prefab default Hall yaw differs")
    need(all(f32(x) == f32(y) for x, y in zip(h["hall_local_position_xyz"], [-0.1, 0.01, 0.9])), "Source grid formula differs")
    need(h["container_local_euler_xyz"] == [0, -45, 0] and h["hall_local_euler_xyz"] == [0, 0, 0], "Source runtime rotations differ")
    return c


def verify_run(c: dict, run: Path) -> dict:
    run = run.resolve()
    need(run.is_relative_to((PROBE / "OutputRuns").resolve()), "Run outside isolated project")
    need(run.parent == (PROBE / "OutputRuns").resolve(), "Run must be a unique direct child")
    need(sha(run / "hall_world_contract_v4.json") == sha(CONTRACT), "Run contract differs")
    need(sha(run / "exporter.cs") == c["exporter_sha256"], "Run exporter differs")
    n = json.loads((run / "hall_world_native.json").read_text(encoding="utf-8"))
    need(n["schema"] == "rok.unity_city_hall_world_native.v3", "Native schema differs")
    need(n["contract_sha256"] == sha(CONTRACT), "Native contract hash differs")
    need(n["qualification"] == "transform_fixture_only" and n["lod_qualified"] is False, "Native LOD claim differs")
    need(n["source_renderer_found"] is True and n["prefab_default_rotation_overridden"] is True, "Source renderer/override absent")
    need(n["prefab_default_is_persistent_asset"] is True, "Default prefab asset qualification differs")
    need(n["dynamic_hall_attach_verified"] is True and n["container_source_child_count"] == 0 and
         n["container_runtime_child_count"] == 1 and n["dynamic_hall_child_index"] == 0 and
         n["dynamic_hall_parent_path"] == "SceneObject/land_root[0]/CityBuildingContainer[0]",
         "Explicit single dynamic Hall attachment differs")
    need((n["init_id"], n["type_id"], n["level"], n["grid_x"], n["grid_y"], n["footprint_width"], n["footprint_length"], n["visual_civilization"], n["age"]) == (5, 1, 1, -5, 5, 9, 9, 101, 1), "Native Hall fixture differs")
    hall = source_prefab(SOURCE_HALL)
    container = source_prefab(SOURCE_CONTAINER)
    identity = trs([0, 0, 0], [0, 0, 0, 1], [1, 1, 1])
    h = c["hall"]
    scene = c["scene_roots"]
    scene_world = matmul(identity, trs(scene[0]["local_position"], scene[0]["local_rotation_xyzw"], scene[0]["local_scale"]))
    land_world = matmul(scene_world, trs(scene[1]["local_position"], scene[1]["local_rotation_xyzw"], scene[1]["local_scale"]))
    container_runtime_world = matmul(land_world, trs([0.0, 0.0, 0.0], yaw(-45),
                                                      container["transforms"][container["root"]]["scale"]))
    expected = {}
    expected.update(expected_prefab_nodes(container, "prefab_default_container", identity))
    expected.update(expected_prefab_nodes(hall, "prefab_default_hall", identity))
    expected.update(expected_prefab_nodes(container, "runtime_replay_container", land_world,
                                         {"position": [0.0, 0.0, 0.0], "rotation": yaw(-45)}))
    expected.update(expected_prefab_nodes(hall, "runtime_replay_hall", container_runtime_world,
                                         {"position": [-0.1, 0.01, 0.9], "rotation": [0.0, 0.0, 0.0, 1.0]}))
    expected[("runtime_replay", "SceneObject", "Transform", scene[0]["transform_file_id"])] = {
        "matrix": scene_world, "position": scene[0]["local_position"], "rotation": scene[0]["local_rotation_xyzw"],
        "scale": scene[0]["local_scale"], "active_self": True, "active_in_hierarchy": True,
        "enabled": True, "guid": "GameApp.unity"}
    expected[("runtime_replay", "SceneObject/land_root[0]", "Transform", scene[1]["transform_file_id"])] = {
        "matrix": land_world, "position": scene[1]["local_position"], "rotation": scene[1]["local_rotation_xyzw"],
        "scale": scene[1]["local_scale"], "active_self": True, "active_in_hierarchy": True,
        "enabled": True, "guid": "GameApp.unity"}
    seen = set()
    phase_counts = {}
    source_renderer = None
    for node in n["nodes"]:
        matrix_path = inside(run, node["matrix_path"])
        raw = matrix_path.read_bytes()
        need(len(raw) == 64 and sha(matrix_path) == node["matrix_sha256"], "Native matrix byte/hash differs")
        matrix = struct.unpack("<16f", raw)
        need(all(math.isfinite(v) for v in matrix), "Native matrix nonfinite")
        need(f32(matrix[15]) == f32(1), "Affine homogeneous matrix invalid")
        key = (node["phase"], node["path"], node["component_type"], node["source_file_id"])
        need(key not in seen, "Duplicate native node identity")
        need(key in expected, f"Unexpected native source node: {key}")
        seen.add(key)
        oracle = expected[key]
        need(node["source_guid"] == oracle["guid"], f"Native source GUID differs: {key}")
        need(all(near(actual, source) for actual, source in zip(matrix, oracle["matrix"])),
             f"Native source-composed world matrix differs: {key}")
        for name, field in (("local_position", "position"), ("local_rotation_xyzw", "rotation"),
                            ("local_scale", "scale")):
            need(len(node[name]) == len(oracle[field]) and
                 all(near(actual, source) for actual, source in zip(node[name], oracle[field])),
                 f"Native source TRS differs: {key}/{name}")
        for name in ("active_self", "active_in_hierarchy", "enabled"):
            need(node[name] is oracle[name], f"Native source state differs: {key}/{name}")
        phase_counts[node["phase"]] = phase_counts.get(node["phase"], 0) + 1
        if node["phase"] == "runtime_replay_hall" and node["component_type"] == "SpriteRenderer" and node["source_file_id"] == c["hall"]["expected_renderer_file_id"]:
            source_renderer = node
    need(seen == set(expected), "Native source hierarchy omitted nodes")
    need(all(phase_counts.get(p, 0) > 0 for p in ("prefab_default_container", "prefab_default_hall", "runtime_replay", "runtime_replay_container", "runtime_replay_hall")), "Native prefab/runtime phases missing")
    need(source_renderer is not None and source_renderer["active_self"] and source_renderer["active_in_hierarchy"] and source_renderer["enabled"], "Exact source renderer not visible in transform fixture")
    hall_roots = [x for x in n["nodes"] if x["phase"] == "runtime_replay_hall" and x["path"] == "build_1_101_1" and x["component_type"] == "Transform"]
    need(len(hall_roots) == 1, "Runtime Hall root missing")
    need(all(f32(x) == f32(y) for x, y in zip(hall_roots[0]["local_position"], c["hall"]["hall_local_position_xyz"])), "Runtime Hall local position differs")
    need(all(abs(v) < 1e-6 for v in hall_roots[0]["local_rotation_xyzw"][:3]) and abs(hall_roots[0]["local_rotation_xyzw"][3] - 1) < 1e-6, "Hall prefab yaw not cleared")
    return {"run_path": run.relative_to(ROOT).as_posix(), "native_sha256": sha(run / "hall_world_native.json"), "node_count": len(n["nodes"]), "source_composed_matrix_count": len(expected), "matrix_tolerance": FLOAT_TOLERANCE, "phase_counts": phase_counts, "missing_source_behaviour_count": len(n["missing_source_behaviours"]), "qualification": n["qualification"], "lod_qualified": False}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    c = verify_contract()
    result = {"schema": "rok.unity_city_hall_world_probe_verify.v1", "passed": True,
              "contract_sha256": sha(CONTRACT), "copied_source_assets": len(c["source_files"]),
              "omitted_dependencies": len(c["omitted_dependencies"]), "unity_started": args.run is not None,
              "native": verify_run(c, args.run) if args.run else None}
    if args.output:
        need(not args.output.exists(), "Verification report already exists")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
