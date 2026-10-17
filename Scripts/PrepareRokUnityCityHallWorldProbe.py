"""Prepare an isolated, transform-only Unity Hall GameApp fixture; never run Unity."""
from __future__ import annotations

import csv
import hashlib
import json
import os
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REF = ROOT / "ref/client-unity/Assets"
PROBE = ROOT / "Saved/UnityCityHallWorldProbeV1"
SEALED = ROOT / "Saved/UnityCityGammaFamilyStateProbeV2"
SCENE = REF / "BundleAssets/Scenes/GameApp.unity"
CONTAINER = REF / "BundleAssets/Scene/City/city_function/CityBuildingContainer.prefab"
HALL = REF / "BundleAssets/Scene/City_building/city_building_1/build_1_101_1.prefab"
EXPORTER = PROBE / "Assets/Editor/RenderRokUnityCityHallWorldNative.cs"
CONTRACT = PROBE / "hall_world_contract_v4.json"
REPORT = ROOT / "Saved/RokUnityMigration/BatchReview20261009QuestChapterAndWorldHall/implementation_revision4.json"
EXPECTED = {
    "scene": "34c0185bef408f1b0563be0185a35de6f72d99f4d5988dc8d6904e70ccd513c7",
    "container": "fc608b1af95af05e8eac25a9b2a35bb4a09a35ea1bdd40b9a80ffe69e14c39fa",
    "hall": "498032280f6bba9414bc61e7be9a3df61e7aa66358a663ba476d8032ac715c9a",
    "init": "a42f339e386789ccd8907b98876f9296994c2c85ceb94f724babf33045979e62",
    "type": "92a5072e165b4780893ed85bca12b2b1c7ee71287c2ff222c2101c5274d10896",
    "model": "4a50eba91ef9bafe3da680a5cfe4a429edc7e956927e08f549b302cc75ef4880",
    "mediator": "31fb4027dc1ae56ea356dc8bf4c331af36ac17802c11fbf2064cb05b6f941253",
    "proxy": "f29a339374d212d3985bb4692dd6920b70d542d70791d3c4dd9b893abfe4cdbf",
    "city_obj_data": "dd7fada809b364a15350270240170043b6c086dd96eb5378a75b4397826fd0b9",
}
CSVROOT = ROOT / "ref/config-tables/编译客户端配置/CSV"
CSV_PATHS = {"init": CSVROOT / "InitBuilding.csv", "type": CSVROOT / "BuildingTypeConfig.csv",
             "model": CSVROOT / "BuildingModelData.csv"}
CODE_PATHS = {"mediator": REF / "Scripts/Hotfix/MVC/GlobalMediator/CityGlobalMediator.cs",
              "proxy": REF / "Scripts/Hotfix/MVC/Proxy/CityBuildingProxy.cs",
              "city_obj_data": REF / "Scripts/Hotfix/Common/Utils/CityObjData.cs"}
GUID = re.compile(r"guid: ([0-9a-f]{32})")


def need(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rel(path: Path) -> str:
    return path.resolve().relative_to(ROOT).as_posix()


def place(path: Path, data: bytes) -> None:
    need(path.resolve().is_relative_to(PROBE.resolve()), "Output escapes isolated project")
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        need(path.read_bytes() == data, f"Isolated input differs: {path}")
    else:
        with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as handle:
            handle.write(data)


def source_record(source: Path, copied: bool, reason: str = "") -> dict:
    record = {"source_path": rel(source), "sha256": sha(source), "copied": copied, "reason": reason}
    if copied:
        target = PROBE / "Assets/Source" / source.relative_to(REF)
        place(target, source.read_bytes())
        record["probe_asset_path"] = "Assets/Source/" + source.relative_to(REF).as_posix()
        meta = Path(str(source) + ".meta")
        need(meta.is_file(), f"Source meta missing: {source}")
        place(Path(str(target) + ".meta"), meta.read_bytes())
        record["meta_path"] = rel(meta)
        record["meta_sha256"] = sha(meta)
    return record


def parse_scene_root(text: str, goid: str, tid: str, name: str, parent: str) -> dict:
    go = re.search(rf"--- !u!1 &{goid}\nGameObject:\n(.*?)(?=--- !u!|\Z)", text, re.S)
    tr = re.search(rf"--- !u!4 &{tid}\nTransform:\n(.*?)(?=--- !u!|\Z)", text, re.S)
    need(go is not None and tr is not None, f"Scene root missing: {name}")
    need(f"m_Name: {name}" in go.group(1) and "m_IsActive: 1" in go.group(1), f"Scene root state differs: {name}")
    need(f"m_GameObject: {{fileID: {goid}}}" in tr.group(1) and f"m_Father: {{fileID: {parent}}}" in tr.group(1), f"Scene parent differs: {name}")
    for key, value in (("m_LocalPosition", "{x: 0, y: 0, z: 0}"),
                       ("m_LocalRotation", "{x: 0, y: 0, z: 0, w: 1}"),
                       ("m_LocalScale", "{x: 1, y: 1, z: 1}")):
        need(f"{key}: {value}" in tr.group(1), f"Scene source TRS differs: {name}/{key}")
    return {"name": name, "game_object_file_id": goid, "transform_file_id": tid,
            "parent_transform_file_id": parent, "active_self": True,
            "local_position": [0, 0, 0], "local_rotation_xyzw": [0, 0, 0, 1], "local_scale": [1, 1, 1]}


def main() -> None:
    for name, source in {"scene": SCENE, "container": CONTAINER, "hall": HALL, **CSV_PATHS, **CODE_PATHS}.items():
        need(sha(source) == EXPECTED[name], f"Pinned source changed: {name}")
    need(EXPORTER.is_file(), "Exporter C# absent")
    scene = SCENE.read_text(encoding="utf-8-sig")
    roots = [parse_scene_root(scene, "425647251", "425647252", "SceneObject", "0"),
             parse_scene_root(scene, "1384602275", "1384602276", "land_root", "425647252")]
    with CSV_PATHS["init"].open(encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.reader(handle))
    need(any(row[:5] == ["5", "1", "1", "-5", "5"] for row in rows), "InitBuilding Hall differs")
    need("m_LocalRotation: {x: 0, y: -0.38268343, z: 0, w: 0.92387956}" in HALL.read_text(encoding="utf-8"), "Hall prefab root default yaw differs")
    need("--- !u!212 &212751466141171401" in HALL.read_text(encoding="utf-8"), "Hall renderer source fileID absent")
    for folder, expected_count in (("ProjectSettings", 19), ("Packages", 2)):
        files = sorted(p for p in (SEALED / folder).iterdir() if p.is_file())
        need(len(files) == expected_count, f"Sealed {folder} count differs")
        for file in files:
            place(PROBE / folder / file.name, file.read_bytes())
    need((PROBE / "ProjectSettings/ProjectVersion.txt").read_text(encoding="utf-8").splitlines()[0] == "m_EditorVersion: 2019.4.28f1c1", "Unity version differs")
    guid_index = {}
    for meta in REF.rglob("*.meta"):
        match = GUID.search(meta.read_text(encoding="utf-8", errors="replace"))
        if match:
            guid_index[match.group(1)] = Path(str(meta)[:-5])
    copied = {}
    omitted = {}
    queue = [CONTAINER, HALL]
    while queue:
        source = queue.pop(0)
        if source in copied:
            continue
        need(source.is_file(), f"Required prefab/asset missing: {source}")
        copied[source] = source_record(source, True)
        if source.suffix.lower() not in (".prefab", ".mat", ".shader", ".asset"):
            continue
        for guid in sorted(set(GUID.findall(source.read_text(encoding="utf-8", errors="replace")))):
            dependency = guid_index.get(guid)
            if dependency is None:
                omitted[guid] = {"guid": guid, "reason": "unresolved_or_builtin_guid"}
            elif dependency.suffix.lower() in (".cs", ".dll", ".asmdef"):
                omitted[guid] = {"guid": guid, **source_record(dependency, False, "script_not_executed_transform_fixture_only")}
            elif dependency not in copied and dependency not in queue:
                queue.append(dependency)
    hall_guid = GUID.search(Path(str(HALL) + ".meta").read_text(encoding="utf-8")).group(1)
    container_guid = GUID.search(Path(str(CONTAINER) + ".meta").read_text(encoding="utf-8")).group(1)
    need(hall_guid == "c825e8897df8e424e817e4ed05eef53e" and
         container_guid == "db18574bd8f02dd49a79776c62b2d55b", "Pinned prefab GUID differs")
    contract = {
        "schema": "rok.unity_city_hall_world_probe_contract.v4", "unity_version": "2019.4.28f1c1",
        "repo_root": ROOT.as_posix(), "probe_root": PROBE.as_posix(),
        "producer_path": rel(Path(__file__)), "producer_sha256": sha(Path(__file__)),
        "exporter_path": rel(EXPORTER), "exporter_sha256": sha(EXPORTER),
        "source_paths_sha256": {rel(path): EXPECTED[name] for name, path in {"scene": SCENE, "container": CONTAINER, "hall": HALL, **CSV_PATHS, **CODE_PATHS}.items()},
        "scene_roots": roots, "source_files": [copied[key] for key in sorted(copied)],
        "omitted_dependencies": [omitted[key] for key in sorted(omitted)],
        "settings_files": [{"source_path": rel(file), "sha256": sha(file), "probe_path": rel(PROBE / folder / file.name)}
                           for folder in ("ProjectSettings", "Packages") for file in sorted((SEALED / folder).iterdir()) if file.is_file()],
        "hall": {"init_id": 5, "type_id": 1, "level": 1, "grid_x": -5, "grid_y": 5,
                 "footprint_width": 9, "footprint_length": 9, "visual_civilization": 101, "age": 1,
                 "city_position_x": 0, "city_position_y": 0,
                 "container_asset": copied[CONTAINER]["probe_asset_path"], "hall_asset": copied[HALL]["probe_asset_path"],
                 "expected_hall_guid": hall_guid, "expected_container_guid": container_guid,
                 "expected_renderer_file_id": "212751466141171401",
                 "container_local_euler_xyz": [0, -45, 0], "hall_local_position_xyz": [-0.1, 0.01, 0.9],
                 "hall_local_euler_xyz": [0, 0, 0]},
        "matrix_contract": "Unity localToWorld uses column-vector ParentWorld*TRS; matrix indexed [row,col] and flattened row-major, 16 IEEE754 float32 little-endian values per 64-byte file; no UE/world conversion",
        "dynamic_attach_contract": "Container source children retain their original order; one new Hall prefab child is appended at index sourceChildCount, then Hall is traversed exactly once in runtime_replay_hall phase",
        "prefab_asset_active_contract": "AssetDatabase persistent prefab asset retains serialized activeSelf but activeInHierarchy is false; runtime instances inherit scene parent activity",
        "rect_transform_contract": "Ground RectTransform under ordinary Hall Transform has anchorMin=anchorMax=(0.5,0.5); Unity imported localPosition.xy resolves from serialized anchoredPosition.xy=(-0.012,0), retaining localPosition.z=-0.002",
        "qualification": "transform_fixture_only", "lod_qualified": False,
        "limits": ["GameApp scene root TRS reconstructed from pinned YAML, entire GameApp not loaded",
                   "Source prefabs instantiated with omitted source MonoBehaviours; manager and WorldCamera not executed",
                   "No GameApp active LOD, final material layer, Unity account state, or UE main-view acceptance"]}
    place(CONTRACT, (json.dumps(contract, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    (PROBE / "OutputRuns").mkdir(parents=True, exist_ok=True)
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    report = {"schema": "rok.hall_world_native_prepare_revision4.v1", "prepared": True, "unity_started": False,
              "contract_path": rel(CONTRACT), "contract_sha256": sha(CONTRACT),
              "producer_sha256": sha(Path(__file__)), "exporter_sha256": sha(EXPORTER),
              "verifier_sha256": sha(ROOT / "Scripts/VerifyRokUnityCityHallWorldProbe.py"),
              "copied_asset_count": len(copied), "omitted_dependency_count": len(omitted),
              "qualification": contract["qualification"], "lod_qualified": False}
    need(not REPORT.exists(), "Preparation report already exists")
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
