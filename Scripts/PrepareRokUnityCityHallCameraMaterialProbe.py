"""Prepare an isolated Hall source-camera/source-material Unity2019 fixture; never launch Unity."""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityHallCameraMaterialProbeV1"
ASSET_SOURCE = ROOT / "ref/client-unity/Assets"
SEED = ROOT / "Saved/UnityCityHallWorldProbeV1/Assets/Source"
SETTINGS = ROOT / "Saved/UnityCityHallCameraLodProbeV1"
NATIVE3 = ROOT / "Saved/UnityCityHallWorldProbeV1/OutputRuns/20261009_032009_843_25796/hall_world_native.json"
NATIVE_CAMERA = ROOT / "Saved/UnityCityHallCameraLodProbeV1/OutputRuns/20261009_045949_616_43000/native_camera_lod.json"
GEOMETRY = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json"
EXPORTER = PROBE / "Assets/Editor/RenderRokUnityCityHallCameraMaterialNative.cs"
SHADER = PROBE / "Assets/ProbeHallTimeWitness.shader"
CONTRACT = PROBE / "hall_camera_material_contract.json"
SOURCE_INPUT = PROBE / "ProbeInputs/source_input.json"
REPORT = ROOT / "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/hall_material_native_prepare.json"
VERIFY = ROOT / "Scripts/VerifyRokUnityCityHallCameraMaterialProbe.py"
UNITY_DATA = Path("C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data")

PINNED = {
    NATIVE3.relative_to(ROOT).as_posix(): "01b924d764395708e20766834595fa7943f1ec61c70215b6058914c2cc80ab5e",
    GEOMETRY.relative_to(ROOT).as_posix(): "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273",
    "ref/client-unity/Assets/BundleAssets/Scene/City_building/city_building_1/build_1_101_1.prefab": "498032280f6bba9414bc61e7be9a3df61e7aa66358a663ba476d8032ac715c9a",
    "ref/client-unity/Assets/BundleAssets/Scene/City/city_function/city_building_night.mat": "71a4b98a801bb1d1f82422e4a21f173a02f60646d127485aeae95881dd044614",
    "ref/client-unity/Assets/BundleAssets/Scene/City/city_function/map_building.mat": "8738d9a68d7e385281d21b2dbb25ab340f6fcee089213d52308453b69fc7b47f",
    "ref/client-unity/Assets/BundleAssets/Map/Shader/cm_city_mask_night.shader": "292ec5dc10f4aa3fca7ecfc2bf5790bbd6910051ee126cffd16fa96ff0f98065",
    "ref/client-unity/Assets/BundleAssets/Map/Building/map_building_with_mask-5.shader": "d26a3a08eaccb769f0ea086316b36c80176095651059c7b881e3b5400d8acbfa",
    "ref/client-unity/Assets/Scripts/Client/Compoment/MaskSprite.cs": "c69770babf57e714a573bb402c30fe8b1b8ff98a3b77abb3efcf46f32d717609",
    "ref/client-unity/Assets/Scripts/Client/LightingManager/NightingMask.cs": "cc6578617d1561916efa08e92aade94414276d3c07aba262d342ad63980abfee",
    "ref/client-unity/Assets/Scripts/Client/LightingManager/LightManager.cs": "36a4a37fb8f603dad319cfcb464028b93d0d213bb12adda0527ef5a783364620",
    "ref/client-unity/Assets/Scripts/Hotfix/MVC/CMD/SwitchMainCityCmd.cs": "b6155b7c40b3280974f8796b0b6234bfa1d293fdddadf3f10ddc36e807a0dddf",
}


def need(ok: bool, reason: str) -> None:
    if not ok:
        raise AssertionError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rel(path: Path) -> str:
    return path.resolve().relative_to(ROOT).as_posix()


def put(path: Path, data: bytes) -> None:
    need(path.resolve().is_relative_to(PROBE.resolve()), "Writes must remain in new isolated probe")
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if path.read_bytes() != data:
            need(path in (CONTRACT, SOURCE_INPUT) and not (PROBE / "OutputRuns").exists(),
                 f"Prepared input drift: {path}")
            # The contract is revised only during the initial offline preparation,
            # before any Unity run exists. Source copies and typed bytes stay immutable.
            path.write_bytes(data)
    else:
        with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as output:
            output.write(data)


def object_guid(path: Path) -> str:
    import re
    meta = Path(str(path) + ".meta")
    found = re.search(r"(?m)^guid: ([0-9a-f]{32})$", meta.read_text(encoding="utf-8"))
    need(found is not None, f"GUID absent: {meta}")
    return found.group(1)


def main() -> None:
    need(EXPORTER.is_file() and SHADER.is_file() and VERIFY.is_file(), "New exporter/shader/verifier absent")
    for source, digest in PINNED.items():
        need(sha(resolve(source)) == digest, f"Pinned source changed: {source}")
    camera_sha = sha(NATIVE_CAMERA)
    camera_verified = NATIVE_CAMERA.parent / "verified_camera_lod_v2.json"
    need(json.loads(camera_verified.read_text(encoding="utf-8"))["passed"] is True, "NativeCamera2 not strictly verified")
    camera = json.loads(NATIVE_CAMERA.read_text(encoding="utf-8"))
    need(camera["complete"] and camera["qualifier"] == "camera_transform_fixture_only" and
         camera["source_behaviors_executed"] is False and camera["width"] == 1440 and camera["height"] == 810 and
         camera["field_of_view"] == 7.5 and camera["near_clip"] == 2 and camera["far_clip"] == 3350,
         "NativeCamera2 source-configured state differs")
    native3 = json.loads(NATIVE3.read_text(encoding="utf-8"))
    renderers = [n for n in native3["nodes"] if n["phase"] == "runtime_replay_hall" and
                 n["component_type"] == "SpriteRenderer" and n["source_file_id"] == "212751466141171401"]
    need(len(renderers) == 1 and renderers[0]["source_guid"] == "c825e8897df8e424e817e4ed05eef53e" and
         renderers[0]["matrix_sha256"] == "a53abcdc0613cc25b8b16761301e163ccc15e211b5ad889d753a83fb9d43571b",
         "Native3 Hall renderer/matrix identity differs")
    world_path = NATIVE3.parent / renderers[0]["matrix_path"]
    need(sha(world_path) == renderers[0]["matrix_sha256"] and world_path.stat().st_size == 64,
         "Native3 world matrix bytes differ")
    geometry = json.loads(GEOMETRY.read_text(encoding="utf-8"))["rows"][27]
    need(geometry["index"] == 27 and geometry["source_key"] == "initial:5" and
         geometry["positions"]["count"] == 240 and geometry["indices"]["binary"]["count"] == 714,
         "Hall source geometry row differs")

    pins: list[dict[str, str]] = []
    for source in sorted(SEED.rglob("*")):
        if not source.is_file():
            continue
        original = ASSET_SOURCE / source.relative_to(SEED)
        need(original.is_file(), f"Sealed dependency no longer exists in source: {original}")
        if sha(original) != sha(source):
            # Unity generated fresh GUIDs for directory .meta files in the older isolated project.
            # This project preserves the original ref directory GUIDs; asset file/meta bytes must match.
            need(source.suffix == ".meta" and b"folderAsset: yes" in source.read_bytes() and
                 b"folderAsset: yes" in original.read_bytes(),
                 f"Sealed prefab asset dependency/source mismatch: {original}")
        copied = PROBE / "Assets/Source" / source.relative_to(SEED)
        put(copied, original.read_bytes())
        pins.append({"path": rel(copied), "sha256": sha(copied), "source_path": rel(original),
                     "source_sha256": sha(original)})
    for item in [
        "BundleAssets/Scene/City/city_function/map_building.mat",
        "BundleAssets/Scene/City/city_function/map_building.mat.meta",
        "BundleAssets/Map/Building/map_building_with_mask-5.shader",
        "BundleAssets/Map/Building/map_building_with_mask-5.shader.meta",
    ]:
        original = ASSET_SOURCE / item
        copied = PROBE / "Assets/Source" / item
        put(copied, original.read_bytes())
        if not any(p["path"] == rel(copied) for p in pins):
            pins.append({"path": rel(copied), "sha256": sha(copied), "source_path": rel(original),
                         "source_sha256": sha(original)})
    for folder, count in (("ProjectSettings", 19), ("Packages", 2)):
        source_files = sorted(p for p in (SETTINGS / folder).iterdir() if p.is_file())
        need(len(source_files) == count, f"Sealed Unity {folder} count differs")
        for original in source_files:
            copied = PROBE / folder / original.name
            put(copied, original.read_bytes())
            pins.append({"path": rel(copied), "sha256": sha(copied), "source_path": rel(original),
                         "source_sha256": sha(original)})
    need((PROBE / "ProjectSettings/ProjectVersion.txt").read_text(encoding="utf-8").splitlines()[0] ==
         "m_EditorVersion: 2019.4.28f1c1", "Unity version changed")

    native_copy = PROBE / "ProbeInputs/native_camera_lod.json"
    world_copy = PROBE / "ProbeInputs/hall_native3_world.f32le"
    put(native_copy, NATIVE_CAMERA.read_bytes())
    put(world_copy, world_path.read_bytes())
    for source, copied in ((NATIVE_CAMERA, native_copy), (world_path, world_copy)):
        pins.append({"path": rel(copied), "sha256": sha(copied), "source_path": rel(source),
                     "source_sha256": sha(source)})
    camera_matrix_paths = {}
    for name in ("camera_world_to_camera", "camera_projection", "camera_gpu_projection_rt"):
        record = next((m for m in camera["matrices"] if m["name"] == name), None)
        need(record is not None and record["byte_count"] == 64, f"NativeCamera2 matrix record absent: {name}")
        original = NATIVE_CAMERA.parent / record["path"]
        need(original.stat().st_size == 64 and sha(original) == record["sha256"],
             f"NativeCamera2 matrix bytes differ: {name}")
        copied = PROBE / "ProbeInputs" / (name + ".f32le")
        put(copied, original.read_bytes())
        pins.append({"path": rel(copied), "sha256": sha(copied), "source_path": rel(original),
                     "source_sha256": sha(original)})
        camera_matrix_paths[name + "_path"] = "ProbeInputs/" + name + ".f32le"
    geometry_paths = {
        "positions": resolve(geometry["positions"]["path"]),
        "uv0": resolve(geometry["uv_channels"][0]["binary"]["path"]),
        "indices": resolve(geometry["indices"]["binary"]["path"]),
    }
    for name, original in geometry_paths.items():
        copied = PROBE / "ProbeInputs" / ("hall_" + name + ".bin")
        put(copied, original.read_bytes())
        pins.append({"path": rel(copied), "sha256": sha(copied), "source_path": rel(original),
                     "source_sha256": sha(original)})
    # Fixture globals are explicit controlled values. No account/day-cycle inference is made.
    source_input = {
        "schema": "rok.hall_camera_material_source_input.v1", "width": 1440, "height": 810,
        "camera_position_xyz": camera["camera_position_xyz"],
        "camera_rotation_xyzw": camera["camera_rotation_xyzw"],
        "fov": 7.5, "near_clip": 2.0, "far_clip": 3350.0,
        "sprite_color_rgba": [1.0, 1.0, 1.0, 1.0], "city_transparency": 1.0,
        "expected_shader_time_y": 0.0,
        "phase_order": ["day_black", "day_white", "night_black", "night_white"],
        "hall_world_matrix_path": "ProbeInputs/hall_native3_world.f32le",
        "native_camera_path": "ProbeInputs/native_camera_lod.json",
        "positions_path": "ProbeInputs/hall_positions.bin",
        "uv0_path": "ProbeInputs/hall_uv0.bin",
        "indices_path": "ProbeInputs/hall_indices.bin",
        **camera_matrix_paths,
        "qualification": "source_camera_material_fixture_only",
    }
    put(SOURCE_INPUT, (json.dumps(source_input, indent=2) + "\n").encode("utf-8"))
    pins.extend([{"path": rel(SOURCE_INPUT), "sha256": sha(SOURCE_INPUT)},
                 {"path": rel(SHADER), "sha256": sha(SHADER)},
                 {"path": rel(EXPORTER), "sha256": sha(EXPORTER)}])
    hall = ASSET_SOURCE / "BundleAssets/Scene/City_building/city_building_1/build_1_101_1.prefab"
    main = ASSET_SOURCE / "BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_1_101_1.png"
    mask = ASSET_SOURCE / "BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_1_101_1_mask.png"
    need(object_guid(hall) == "c825e8897df8e424e817e4ed05eef53e" and
         object_guid(main) == "57ec7ddf0b743a140b2707fe14b536e2" and
         object_guid(mask) == "c37165d75575008419379c96242599ba", "Source GUID identity differs")
    contract = {
        "schema": "rok.hall_camera_material_contract.v1", "probe_root": PROBE.as_posix(),
        "exporter_sha256": sha(EXPORTER), "time_shader_sha256": sha(SHADER),
        "producer_sha256": sha(Path(__file__)), "verifier_sha256": sha(VERIFY),
        "qualifier": "source_camera_material_fixture_only",
        "hall_prefab_asset": "Assets/Source/BundleAssets/Scene/City_building/city_building_1/build_1_101_1.prefab",
        "source_renderer_guid": "c825e8897df8e424e817e4ed05eef53e",
        "source_renderer_file_id": "212751466141171401",
        "sprite_guid": object_guid(main), "mask_guid": object_guid(mask),
        "day_material_asset": "Assets/Source/BundleAssets/Scene/City/city_function/map_building.mat",
        "night_material_asset": "Assets/Source/BundleAssets/Scene/City/city_function/city_building_night.mat",
        "source_input_path": "ProbeInputs/source_input.json", "source_input_sha256": sha(SOURCE_INPUT),
        "native_camera_source": {"path": rel(NATIVE_CAMERA), "sha256": camera_sha,
                                  "verified_path": rel(camera_verified), "verified_sha256": sha(camera_verified)},
        "native3_source": {"path": rel(NATIVE3), "sha256": sha(NATIVE3),
                           "world_matrix_path": rel(world_path), "world_matrix_sha256": sha(world_path)},
        "geometry_source": {"path": rel(GEOMETRY), "sha256": sha(GEOMETRY),
                            "source_row_sha256": geometry["source_row_sha256"]},
        "source_pins": PINNED, "input_files": sorted(pins, key=lambda value: value["path"]),
        "scope": "isolated source SpriteRenderer Hall day/night with Native3 transform and NativeCamera2 camera; explicit white/global transparency/time-zero GPU fixture; two opaque backgrounds; no GameApp scripts, account, LOD or mainview acceptance",
    }
    # The C# typed pin class intentionally ignores extra provenance fields.
    contract["input_files"] = [{"path": p["path"].removeprefix(rel(PROBE) + "/"), "sha256": p["sha256"]}
                               for p in contract["input_files"]]
    put(CONTRACT, (json.dumps(contract, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    report = {"schema": "rok.hall_camera_material_prepare.v1", "ready": True,
              "unity_not_started": True, "probe_root": rel(PROBE), "contract": rel(CONTRACT),
              "contract_sha256": sha(CONTRACT), "producer_sha256": sha(Path(__file__)),
              "verifier_sha256": sha(VERIFY), "exporter_sha256": sha(EXPORTER),
              "time_shader_sha256": sha(SHADER), "source_file_count": len(pins),
              "qualification": contract["qualifier"]}
    REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
