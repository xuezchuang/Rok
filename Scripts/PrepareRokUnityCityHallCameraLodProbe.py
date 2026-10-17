"""Prepare a source-pinned, isolated Unity 2019 Hall camera/LOD measurement."""
from __future__ import annotations

import csv
import hashlib
import json
import os
import re
import shutil
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityHallCameraLodProbeV1"
SETTINGS = ROOT / "Saved/UnityCityGammaFamilyStateProbeV2"
REPORT = ROOT / "Saved/RokUnityMigration/BatchReview20261009QuestChapterArtAndHallCamera/hall_camera_lod_native_prepare_v2.json"
CONTRACT = PROBE / "hall_camera_lod_contract_v2.json"
INPUT = PROBE / "ProbeInputs/source_camera_lod_input_v1.json"
EXPORTER = PROBE / "Assets/Editor/RenderRokUnityCityHallCameraLodNative.cs"
VERIFY = ROOT / "Scripts/VerifyRokUnityCityHallCameraLodProbe.py"
SOURCES = {
    "scene": "ref/client-unity/Assets/BundleAssets/Scenes/GameApp.unity",
    "container": "ref/client-unity/Assets/BundleAssets/Scene/City/city_function/CityBuildingContainer.prefab",
    "hall": "ref/client-unity/Assets/BundleAssets/Scene/City_building/city_building_1/build_1_101_1.prefab",
    "camera_table": "ref/config-tables/编译客户端配置/CSV/CameraParamLod.csv",
    "age_table": "ref/config-tables/编译客户端配置/CSV/CityAgeSize.csv",
    "native3": "Saved/UnityCityHallWorldProbeV1/OutputRuns/20261009_032009_843_25796/hall_world_native.json",
    "native3_contract": "Saved/UnityCityHallWorldProbeV1/hall_world_contract_v4.json",
    "geometry": "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json",
}
PINNED = {
    "scene": "34c0185bef408f1b0563be0185a35de6f72d99f4d5988dc8d6904e70ccd513c7",
    "container": "fc608b1af95af05e8eac25a9b2a35bb4a09a35ea1bdd40b9a80ffe69e14c39fa",
    "hall": "498032280f6bba9414bc61e7be9a3df61e7aa66358a663ba476d8032ac715c9a",
    "camera_table": "6c7163d7e6ca333a244a59ecf5a2ac37e6ad469320cc9cdac5a75bb47f6cf02c",
    "age_table": "36dfe1f46a4c1f3b005202c81692f75a90774125c401915ba1031f1d94232aff",
    "native3": "01b924d764395708e20766834595fa7943f1ec61c70215b6058914c2cc80ab5e",
    "native3_contract": "60bfd58cc2c4897a49cb4440474825c641cc30685e40dc1fa6e404063fd56dce",
    "geometry": "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273",
}


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise AssertionError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rel(path: Path) -> str:
    return path.resolve().relative_to(ROOT).as_posix()


def create(path: Path, data: bytes) -> None:
    require(path.resolve().is_relative_to(PROBE.resolve()), "Output escapes isolated probe")
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        require(path.read_bytes() == data, f"Existing probe input changed: {path}")
    else:
        with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as out:
            out.write(data)


def block(text: str, tag: int, file_id: str) -> str:
    match = re.search(rf"(?m)^--- !u!{tag} &{re.escape(file_id)}\s*\n(.*?)(?=^--- !u!|\Z)", text, re.S | re.M)
    require(match is not None, f"Unity YAML object missing: {tag}/{file_id}")
    return match.group(1)


def field(text: str, name: str) -> str:
    match = re.search(rf"(?m)^  {re.escape(name)}: ([^\r\n]+)", text)
    require(match is not None, f"Unity field missing: {name}")
    return match.group(1).strip()


def vector(text: str, name: str, components: tuple[str, ...]) -> list[float]:
    raw = field(text, name)
    return [float(re.search(rf"\b{component}: ([^,}}]+)", raw).group(1)) for component in components]


def component_block(text: str, guid: str) -> str:
    sections = re.findall(r"(?m)^--- !u!114 &[^\r\n]+\r?\n.*?(?=^--- !u!|\Z)", text, re.S | re.M)
    matches = [section for section in sections if f"guid: {guid}" in section]
    require(len(matches) == 1, f"Expected one component {guid}, found {len(matches)}")
    return matches[0]


def numeric_rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    require(len(rows) >= 2, f"Empty source CSV: {path}")
    return rows[1:]


def source_state() -> tuple[dict, dict]:
    scene = (resolve(SOURCES["scene"])).read_text(encoding="utf-8-sig")
    parent = block(scene, 4, "425647252")
    camera_transform = block(scene, 4, "105374290")
    camera_component = block(scene, 20, "105374294")
    require(vector(parent, "m_LocalPosition", ("x", "y", "z")) == [0, 0, 0] and
            vector(parent, "m_LocalRotation", ("x", "y", "z", "w")) == [0, 0, 0, 1] and
            vector(parent, "m_LocalScale", ("x", "y", "z")) == [1, 1, 1], "SceneObject parent TRS changed")
    require(field(camera_transform, "m_Father") == "{fileID: 425647252}", "WorldCamera source parent changed")
    quat = vector(camera_transform, "m_LocalRotation", ("x", "y", "z", "w"))
    scene_position = vector(camera_transform, "m_LocalPosition", ("x", "y", "z"))
    require(abs(quat[0] - .38268343) < 1e-8 and abs(quat[3] - .92387956) < 1e-8 and
            quat[1] == quat[2] == 0 and scene_position == [3600, 12.870208, 3600],
            "GameApp serialized camera source changed")
    scene_fov = float(field(camera_component, "field of view"))
    near = float(field(camera_component, "near clip plane"))
    far = float(field(camera_component, "far clip plane"))
    require((scene_fov, near, far) == (30, 2, 3350) and field(camera_component, "orthographic") == "0" and
            field(camera_component, "m_TargetTexture") == "{fileID: 0}", "GameApp Camera component changed")
    lod_camera = component_block(scene, "4bae9f026d873e04b88d09d48706b1a4")
    scene_thresholds = [int(v) for v in re.findall(r"(?m)^  - (\d+)\s*$", lod_camera.split("  m_map_piece_width_curve:")[0])]
    require(scene_thresholds == [1500, 3000, 8000, 20000, 42500, 999999], "GameApp camera LOD thresholds changed")
    container = (resolve(SOURCES["container"])).read_text(encoding="utf-8-sig")
    castle = component_block(container, "7c3728ec13555fb4d8d560e7ec13d2a7")
    curve_body = castle.split("  m_scale_curve:", 1)[1].split("  m_max_scale:", 1)[0]
    key_blocks = re.findall(r"(?m)^    - serializedVersion: 3\s*\n(.*?)(?=^    - serializedVersion: 3|^    m_PreInfinity:)", curve_body, re.S | re.M)
    keys = []
    for part in key_blocks:
        def key_field(name: str) -> str:
            found = re.search(rf"(?m)^      {name}: ([^\r\n]+)", part)
            require(found is not None, f"Source curve key missing {name}")
            return found.group(1).strip()
        keys.append({"time": float(key_field("time")), "value": float(key_field("value")),
                     "in_slope": key_field("inSlope"), "out_slope": key_field("outSlope"),
                     "in_weight": float(key_field("inWeight")), "out_weight": float(key_field("outWeight")),
                     "weighted_mode": int(key_field("weightedMode"))})
    require([(k["time"], k["value"]) for k in keys] ==
            [(1, 1), (250, 1), (1500, 8), (2250, 8), (2999.999, 8), (3000, 0)],
            "City Hall source LOD curve changed")
    max_scale = float(field(castle, "m_max_scale"))
    require(max_scale == 7.8, "City Hall LOD max scale changed")
    container_thresholds = [int(v) for v in re.findall(r"(?m)^  - (\d+)\s*$", castle.split("  m_scale_curve:")[0])]
    require(container_thresholds == [1500, 3000, 8000, 20000, 42500], "Container LOD thresholds changed")
    camera_rows = numeric_rows(resolve(SOURCES["camera_table"]))
    selected = [r for r in camera_rows if r["ID"] == "city_default"]
    require(len(selected) == 1, "CameraParamLod city_default not unique")
    city = selected[0]
    dist, fov, dxf = (float(city[k]) for k in ("dist", "fov", "dxf"))
    require((dist, fov, dxf) == (20, 7.5, 150) and dist * fov == dxf, "city_default config changed")
    ages = numeric_rows(resolve(SOURCES["age_table"]))
    age1 = [r for r in ages if r["age"] == "1"]
    require(len(age1) == 1 and age1[0]["townLevel"] == "1" and int(age1[0]["size"]) == 41 and
            max(int(r["size"]) for r in ages) == 57, "City age size source changed")
    native = json.loads((resolve(SOURCES["native3"])).read_text(encoding="utf-8-sig"))
    require(native["qualification"] == "transform_fixture_only" and native["lod_qualified"] is False and
            native["init_id"] == 5 and native["type_id"] == 1 and native["level"] == 1 and
            (native["grid_x"], native["grid_y"]) == (-5, 5), "Hall Native3 identity/qualification changed")
    nodes = [n for n in native["nodes"] if n["phase"] == "runtime_replay_hall" and
             n["component_type"] == "SpriteRenderer" and n["source_file_id"] == "212751466141171401"]
    require(len(nodes) == 1 and nodes[0]["path"] == "build_1_101_1/sprite[0]" and
            nodes[0]["source_guid"] == "c825e8897df8e424e817e4ed05eef53e", "Hall Native3 renderer changed")
    world_src = (resolve(SOURCES["native3"])).parent / nodes[0]["matrix_path"]
    require(world_src.stat().st_size == 64 and sha(world_src) == nodes[0]["matrix_sha256"], "Hall matrix changed")
    geometry = json.loads((resolve(SOURCES["geometry"])).read_text(encoding="utf-8-sig"))
    row = geometry["rows"][27]
    require(row["index"] == 27 and row["source_key"] == "initial:5" and
            row["positions"]["count"] == 240 and row["positions"]["stride_bytes"] == 12,
            "Hall geometry row27 changed")
    positions_src = resolve(row["positions"]["path"])
    require(sha(positions_src) == row["positions"]["sha256"] and positions_src.stat().st_size == 2880,
            "Hall geometry positions changed")
    hall = (resolve(SOURCES["hall"])).read_text(encoding="utf-8-sig")
    require("&212751466141171401" in hall and
            "m_Sprite: {fileID: 21300000, guid: 57ec7ddf0b743a140b2707fe14b536e2, type: 3}" in hall,
            "Hall renderer or sprite identity changed")
    state = {"schema": "rok.hall_camera_lod_source_input.v1", "scene_camera_local_position": scene_position,
             "scene_camera_rotation_xyzw": quat, "scene_serialized_fov": scene_fov,
             "near": near, "far": far, "camera_target_xyz": [0, 0, 0], "output_width": 1440, "output_height": 810,
             "city_default_dist": dist, "city_default_fov": fov, "city_default_dxf": dxf,
             "scene_lod_thresholds": scene_thresholds, "container_lod_thresholds": container_thresholds,
             "curve_keys": keys, "castle_max_scale": max_scale, "age1_size": 41, "max_age_size": 57,
             "hall_world_matrix_path": "ProbeInputs/hall_runtime_world_column.f32le",
             "positions_path": "ProbeInputs/hall_row27_positions.f32le",
             "sample_vertex_indices": [0, 60, 120, 180, 239],
             "qualification": "camera_transform_fixture_only", "source_behaviors_executed": False}
    return state, {"world": world_src, "positions": positions_src}


def main() -> None:
    require(EXPORTER.is_file() and VERIFY.is_file(), "New exporter/verifier missing")
    for name, relative in SOURCES.items():
        require(sha(resolve(relative)) == PINNED[name], f"Pinned source drift: {relative}")
    state, binary_sources = source_state()
    PROBE.mkdir(parents=True, exist_ok=True)
    copied = []
    for group, count in (("ProjectSettings", 19), ("Packages", 2)):
        members = sorted(p for p in (SETTINGS / group).iterdir() if p.is_file())
        require(len(members) == count, f"Sealed Unity {group} file count changed")
        for source in members:
            dest = PROBE / group / source.name
            create(dest, source.read_bytes())
            copied.append({"source_path": rel(source), "probe_path": rel(dest).removeprefix(rel(PROBE) + "/"),
                           "sha256": sha(source)})
    source_copies = []
    for name, relative in SOURCES.items():
        source = resolve(relative)
        dest = PROBE / "ProbeInputs/SourceCopies" / name / source.name
        create(dest, source.read_bytes())
        source_copies.append({"name": name, "source_path": relative,
                              "probe_path": rel(dest).removeprefix(rel(PROBE) + "/"), "sha256": sha(source)})
    for name, source in binary_sources.items():
        dest = PROBE / state["hall_world_matrix_path" if name == "world" else "positions_path"]
        create(dest, source.read_bytes())
        source_copies.append({"name": name, "source_path": rel(source),
                              "probe_path": rel(dest).removeprefix(rel(PROBE) + "/"), "sha256": sha(source)})
    create(INPUT, (json.dumps(state, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    contract = {"schema": "rok.hall_camera_lod_contract.v2", "probe_root": PROBE.as_posix(),
                "repo_root": ROOT.as_posix(), "unity_version": "2019.4.28f1c1",
                "producer_path": rel(Path(__file__)), "producer_sha256": sha(Path(__file__)),
                "verifier_path": rel(VERIFY), "verifier_sha256": sha(VERIFY),
                "exporter_path": rel(EXPORTER), "exporter_sha256": sha(EXPORTER),
                "input_path": rel(INPUT).removeprefix(rel(PROBE) + "/"), "input_sha256": sha(INPUT),
                "sources": source_copies, "settings_files": copied,
                "matrix_contract": "Unity Matrix4x4[row,col] sixteen IEEE754 float32 little-endian, row-major 64B; worldToCamera and projection use Unity column-vector multiplication P*V*world; WorldToViewportPoint XY bottom-left normalized, Z forward-positive.",
                "qualifier": "camera_transform_fixture_only", "source_behaviors_executed": False,
                "limitations": ["No GameApp hotfix/account runtime", "No actual WorldCamera, LevelDetailCastle or GridCollideMgr MonoBehaviour execution", "No source material/alpha/LOD final visibility or main-view rendering"]}
    create(CONTRACT, (json.dumps(contract, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    (PROBE / "OutputRuns").mkdir(exist_ok=True)
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    require(not REPORT.exists(), f"Preparation report already exists: {REPORT}")
    report = {"schema": "rok.hall_camera_lod_prepare.v2", "prepared": True,
              "unity_started": False, "contract_path": rel(CONTRACT), "contract_sha256": sha(CONTRACT),
              "source_copies": len(source_copies), "settings_files": len(copied),
              "producer_sha256": sha(Path(__file__)), "exporter_sha256": sha(EXPORTER),
              "verifier_sha256": sha(VERIFY), "qualifier": contract["qualifier"]}
    with os.fdopen(os.open(REPORT, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as out:
        out.write((json.dumps(report, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
