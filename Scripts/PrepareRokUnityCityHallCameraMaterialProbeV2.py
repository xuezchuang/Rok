"""Prepare a separate Unity2019 Hall material compression probe; never start Unity."""
from __future__ import annotations

import hashlib
import json
import os
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OLD = ROOT / "Saved/UnityCityHallCameraMaterialProbeV1"
NEW = ROOT / "Saved/UnityCityHallCameraMaterialProbeV2"
OLD_RUN = OLD / "OutputRuns/20261009_052943_467_46956"
OLD_NATIVE = OLD_RUN / "hall_camera_material_native.json"
OLD_NATIVE_SHA = "3e2d70c5a9c2a845f5e34b8e9e80fe95d2218142b9697a9d63236beb4d6ef451"
OLD_CONTRACT = OLD / "hall_camera_material_contract.json"
OLD_CONTRACT_SHA = "4bac306793ef6a94cab283ab169a3d2cd912f27649f3d093df0efb6ab260df2f"
EXPORTER = NEW / "Assets/Editor/RenderRokUnityCityHallCameraMaterialNativeV2.cs"
VERIFY = ROOT / "Scripts/VerifyRokUnityCityHallCameraMaterialProbeV2.py"
CONTRACT = NEW / "hall_camera_material_contract_v2.json"
REPORT = ROOT / "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/hall_camera_material_v2_native_prepare.json"
TEXTURE_DIR = "Assets/Source/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1"
MUTABLE_META = {f"{TEXTURE_DIR}/build_1_101_1{x}.png.meta" for x in ("", "_mask")}
PHASES = ("day_black", "day_white", "night_black", "night_white")


def need(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_once(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if path.read_bytes() != data and path in (CONTRACT, REPORT) and not (NEW / "OutputRuns").exists():
            path.write_bytes(data)  # Pre-run producer correction; no native evidence exists.
        else:
            need(path.read_bytes() == data, f"Prepared file drift: {path}")
    else:
        with os.fdopen(os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644), "wb") as stream:
            stream.write(data)


def main() -> None:
    need(sha(OLD_NATIVE) == OLD_NATIVE_SHA and sha(OLD_CONTRACT) == OLD_CONTRACT_SHA,
         "Sealed V1 oracle or contract changed")
    need(not (NEW / "OutputRuns").exists(), "V2 project already executed; no preparation overwrite")
    need(EXPORTER.is_file() and VERIFY.is_file(), "V2 exporter or verifier absent")
    old = json.loads(OLD_CONTRACT.read_text(encoding="utf-8"))
    native = json.loads(OLD_NATIVE.read_text(encoding="utf-8"))
    need(native["complete"] and [c["phase"] for c in native["cases"]] == list(PHASES),
         "V1 four-phase oracle differs")
    for folder in ("Assets", "Packages", "ProjectSettings"):
        old_folder, new_folder = OLD / folder, NEW / folder
        if not new_folder.exists():
            shutil.copytree(old_folder, new_folder, ignore=(shutil.ignore_patterns("RenderRokUnityCityHallCameraMaterialNative.cs")
                                                       if folder == "Assets" else None))
        for file in old_folder.rglob("*"):
            if not file.is_file() or file.name == "RenderRokUnityCityHallCameraMaterialNative.cs":
                continue
            dest = new_folder / file.relative_to(old_folder)
            need(dest.is_file() and sha(dest) == sha(file), f"V2 source/settings copy differs: {dest}")
    need(not (NEW / "Assets/Editor/RenderRokUnityCityHallCameraMaterialNative.cs").exists(),
         "Copied V1 exporter must not compile in V2")
    # Typed inputs are generated from their sealed sources, not copied as part of the Unity project.
    camera_path = ROOT / "Saved/UnityCityHallCameraLodProbeV1/OutputRuns/20261009_045949_616_43000/native_camera_lod.json"
    native3_path = ROOT / "Saved/UnityCityHallWorldProbeV1/OutputRuns/20261009_032009_843_25796/hall_world_native.json"
    geometry_path = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json"
    camera = json.loads(camera_path.read_text(encoding="utf-8"))
    native3 = json.loads(native3_path.read_text(encoding="utf-8"))
    hall_node = next(n for n in native3["nodes"] if n["phase"] == "runtime_replay_hall" and
                     n["component_type"] == "SpriteRenderer" and n["source_file_id"] == "212751466141171401")
    row = json.loads(geometry_path.read_text(encoding="utf-8"))["rows"][27]
    source_map = {
        "native_camera_lod.json": camera_path,
        "hall_native3_world.f32le": native3_path.parent / hall_node["matrix_path"],
        "hall_positions.bin": ROOT / row["positions"]["path"],
        "hall_uv0.bin": ROOT / row["uv_channels"][0]["binary"]["path"],
        "hall_indices.bin": ROOT / row["indices"]["binary"]["path"],
    }
    for key in ("camera_world_to_camera", "camera_projection", "camera_gpu_projection_rt"):
        record = next(m for m in camera["matrices"] if m["name"] == key)
        source_map[key + ".f32le"] = camera_path.parent / record["path"]
    source_map["source_input.json"] = OLD / "ProbeInputs/source_input.json"
    for name, origin in source_map.items():
        need(origin.is_file(), f"Sealed typed input source absent: {origin}")
        write_once(NEW / "ProbeInputs" / name, origin.read_bytes())
    contract = dict(old)
    contract.update(schema="rok.hall_camera_material_contract.v2", probe_root=NEW.as_posix(),
                    exporter_sha256=sha(EXPORTER), producer_sha256=sha(Path(__file__)),
                    verifier_sha256=sha(VERIFY), v1_native_manifest_path=OLD_NATIVE.as_posix(),
                    v1_native_manifest_sha256=OLD_NATIVE_SHA,
                    v1_case_raw=[{"path": c["phase"], "sha256": c["raw_top_left_rgba8"]["sha256"]}
                                 for c in native["cases"]],
                    scope="V1 byte-identical four-phase Hall material fixture plus measured renderer matrix and "
                          "isolated readable DXT5/DXT1 imported blocks; no GameApp/main-view acceptance")
    pins = [dict(p) for p in old["input_files"]]
    for pin in pins:
        if pin["path"] == "Assets/Editor/RenderRokUnityCityHallCameraMaterialNative.cs":
            pin.update(path="Assets/Editor/RenderRokUnityCityHallCameraMaterialNativeV2.cs", sha256=sha(EXPORTER))
    need(len(pins) == len({p["path"] for p in pins}) and len(MUTABLE_META.intersection(p["path"] for p in pins)) == 2,
         "V1 source pin set or two texture metas absent")
    for pin in pins:
        need(sha(NEW / pin["path"]) == pin["sha256"], f"V2 copied input differs: {pin['path']}")
    contract["input_files"] = sorted(pins, key=lambda p: p["path"])
    write_once(CONTRACT, (json.dumps(contract, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    report = {"schema": "rok.hall_camera_material_v2_prepare.v1", "ready": True,
              "unity_started": False, "probe_root": NEW.relative_to(ROOT).as_posix(),
              "contract": CONTRACT.relative_to(ROOT).as_posix(), "contract_sha256": sha(CONTRACT),
              "exporter_sha256": sha(EXPORTER), "producer_sha256": sha(Path(__file__)),
              "verifier_sha256": sha(VERIFY), "v1_native_sha256": OLD_NATIVE_SHA,
              "original_texture_meta_pins": {p["path"]: p["sha256"] for p in pins if p["path"] in MUTABLE_META},
              "source_copy_count": len(pins), "project_folders": ["Assets", "Packages", "ProjectSettings"],
              "generated_typed_input_files": {name: sha(origin) for name, origin in source_map.items()},
              "expected_source_formats": {"main": "DXT5", "mask": "DXT1"},
              "qualifier": "source_camera_material_fixture_only"}
    write_once(REPORT, (json.dumps(report, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    print(json.dumps(report, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
