"""Prepare a source-pinned, isolated Unity 2019 world-tile particle witness; never launch Unity."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
BATCH = ROOT / "Saved/RokUnityMigration/BatchReview20261009WorldSearchAndTileEffects/TileParticleSource"
SOURCE_MANIFEST = BATCH / "source_v1.json"
PROBE = ROOT / "Saved/UnityWorldTileParticleProbeV1"
BASE = ROOT / "Saved/UnityCityHallCameraLodProbeV1"
ORACLE = ROOT / "Scripts/UnityRokWorldTileParticleOracle.cs"
CONTRACT = PROBE / "probe_contract.json"
REPORT = BATCH / "native_prepare_v1.json"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def create_exact(path: Path, data: bytes) -> None:
    if not path.resolve().is_relative_to(PROBE.resolve()):
        raise ValueError(f"isolated-project output escaped: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if path.read_bytes() != data:
            raise ValueError(f"existing probe input differs: {path}")
        return
    with os.fdopen(os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644), "wb") as stream:
        stream.write(data)


def main() -> None:
    manifest = json.loads(SOURCE_MANIFEST.read_text(encoding="utf-8"))
    if manifest["schema"] != "rok.world_tile_particle_source.v1":
        raise ValueError("source oracle missing or changed")
    if not BASE.is_dir():
        raise ValueError("existing Unity 2019 isolated project template missing")
    for source in sorted((BASE / "ProjectSettings").rglob("*")):
        if source.is_file():
            create_exact(PROBE / "ProjectSettings" / source.relative_to(BASE / "ProjectSettings"), source.read_bytes())
    for source in sorted((BASE / "Packages").rglob("*")):
        if source.is_file():
            data = source.read_bytes()
            if source.name in ("manifest.json", "packages-lock.json"):
                package = json.loads(data)
                if source.name == "manifest.json":
                    package["dependencies"]["com.unity.modules.particlesystem"] = "1.0.0"
                else:
                    package["dependencies"]["com.unity.modules.particlesystem"] = {
                        "version": "1.0.0", "depth": 0, "source": "builtin", "dependencies": {}}
                data = (json.dumps(package, ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode("utf-8")
            create_exact(PROBE / "Packages" / source.relative_to(BASE / "Packages"), data)
    pinned = manifest["source_files"]
    mapping = {"prefab": "Tile-JuDian-A2_lod2.prefab", "material": "Ring_06_light.mat",
               "texture": "Ring_07.png"}
    for kind, dest in mapping.items():
        record = pinned[kind]
        source = resolve(record["path"])
        meta = resolve(record["meta_path"])
        if digest(source) != record["sha256"] or digest(meta) != record["meta_sha256"]:
            raise ValueError(f"source input drift: {kind}")
        create_exact(PROBE / "Assets/ProbeInputs" / dest, source.read_bytes())
        create_exact(PROBE / "Assets/ProbeInputs" / (dest + ".meta"), meta.read_bytes())
    create_exact(PROBE / "Assets/Editor/UnityRokWorldTileParticleOracle.cs", ORACLE.read_bytes())
    contract = {
        "schema": "rok.world_tile_particle_probe_contract.v1",
        "unity_version_target": "2019.4.28f1c1",
        "source_manifest_sha256": digest(SOURCE_MANIFEST),
        "prefab_sha256": pinned["prefab"]["sha256"],
        "material_sha256": pinned["material"]["sha256"],
        "texture_sha256": pinned["texture"]["sha256"],
        "prefab_guid": pinned["prefab"]["guid"],
        "material_guid": pinned["material"]["guid"],
        "texture_guid": pinned["texture"]["guid"],
    }
    create_exact(CONTRACT, (json.dumps(contract, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    report = {
        "schema": "rok.world_tile_particle_native_prepare.v1",
        "status": "prepared_not_executed",
        "isolated_project": PROBE.relative_to(ROOT).as_posix(),
        "unity_version_target": "2019.4.28f1c1",
        "source_manifest": {"path": SOURCE_MANIFEST.relative_to(ROOT).as_posix(),
                            "sha256": digest(SOURCE_MANIFEST)},
        "oracle_source": {"path": ORACLE.relative_to(ROOT).as_posix(), "sha256": digest(ORACLE)},
        "contract": {"path": CONTRACT.relative_to(ROOT).as_posix(), "sha256": digest(CONTRACT)},
        "project_template": BASE.relative_to(ROOT).as_posix(),
        "native_witness_args": ["-batchmode", "-quit", "-projectPath", str(PROBE),
                                "-executeMethod", "UnityRokWorldTileParticleOracle.Run",
                                "-logFile", str(PROBE / "OutputRuns/unity_console.log")],
        "required_environment": "Unity 2019.4.28f1c1 desktop Editor; launch only by root in a later step",
        "witness_scope": "Resolve imported shader/texture/mesh and sample both source particle systems with fixed random seed; no map-level LOD or GPU-pixel acceptance.",
        "not_run": True,
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    payload = (json.dumps(report, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if REPORT.exists() and REPORT.read_bytes() != payload:
        raise ValueError("existing prepare report differs; preserve it and write a revision")
    if not REPORT.exists():
        REPORT.write_bytes(payload)
    print(json.dumps({"project": str(PROBE), "contract_sha256": digest(CONTRACT),
                      "report_sha256": digest(REPORT), "native_witness_executed": False}, ensure_ascii=False))


if __name__ == "__main__":
    main()
