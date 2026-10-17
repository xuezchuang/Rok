"""Prepare an isolated Unity 2019 live-particle pixel probe. Never launches Unity."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "Saved/UnityWorldTileParticleProbeV2"
PROJECT = ROOT / "Saved/UnityWorldTileParticleVisualProbeV1"
SOURCE = PROJECT / "Source/UnityRokWorldTileParticleVisualProbeV1.cs"
NATIVE = BASE / "OutputRuns/20261009_065833_466_26596/particle_native.json"
BASE_CONTRACT = BASE / "probe_contract.json"
CONTRACT = PROJECT / "probe_contract.json"
BATCH = ROOT / "Saved/RokUnityMigration/BatchReview20261009WorldSearchAndTileEffects/NextBatch/TileParticleVisualSource"
REPORT = BATCH / "visual_probe_prepare_v1.json"
NATIVE_SHA = "ce7d850f6dbb696a0d34e96df2d7f7ac0658eceadacd694d579a5f1ad3b6c75c"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def exact(path: Path, payload: bytes) -> None:
    if not path.resolve().is_relative_to(PROJECT.resolve()):
        raise ValueError(f"output escaped isolated project: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if path.read_bytes() != payload:
            raise ValueError(f"existing probe file differs, never overwrite: {path}")
        return
    with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as stream:
        stream.write(payload)


def main() -> None:
    if sha(NATIVE) != NATIVE_SHA:
        raise ValueError("sealed native V2 particle witness changed")
    native = json.loads(NATIVE.read_text(encoding="utf-8"))
    source = json.loads(BASE_CONTRACT.read_text(encoding="utf-8"))
    if native["schema"] != "rok.world_tile_particle_native_witness.v2" or len(native["systems"]) != 2:
        raise ValueError("native V2 source identity changed")
    if source["schema"] != "rok.world_tile_particle_probe_contract.v2":
        raise ValueError("base contract identity changed")
    if not SOURCE.is_file():
        raise ValueError("new probe exporter source missing")
    for folder in ("ProjectSettings", "Packages"):
        for source_file in sorted((BASE / folder).rglob("*")):
            if source_file.is_file():
                exact(PROJECT / folder / source_file.relative_to(BASE / folder), source_file.read_bytes())
    settings = (PROJECT / "ProjectSettings/ProjectSettings.asset").read_text(encoding="utf-8")
    if "m_ActiveColorSpace: 0" not in settings:
        raise ValueError("isolated project does not use source Gamma color space")
    for name, key in (("Tile-JuDian-A2_lod2.prefab", "prefab"),
                      ("Ring_06_light.mat", "material"), ("Ring_07.png", "texture")):
        asset = BASE / "Assets/ProbeInputs" / name
        if sha(asset) != source[key + "_sha256"]:
            raise ValueError(f"base source asset changed: {key}")
        exact(PROJECT / "Assets/ProbeInputs" / name, asset.read_bytes())
        exact(PROJECT / "Assets/ProbeInputs" / (name + ".meta"),
              (BASE / "Assets/ProbeInputs" / (name + ".meta")).read_bytes())
    exporter = PROJECT / "Assets/Editor/UnityRokWorldTileParticleVisualProbeV1.cs"
    exact(exporter, SOURCE.read_bytes())
    exact(exporter.with_name(exporter.name + ".meta"),
          (BASE / "Assets/Editor/UnityRokWorldTileParticleOracleV2.cs.meta").read_bytes())
    verifier = ROOT / "Scripts/VerifyRokUnityWorldTileParticleVisualProbeV1.py"
    contract = {
        "schema": "rok.world_tile_particle_visual_contract.v1",
        "unity_version_target": "2019.4.28f1c1",
        "source_native_sha256": NATIVE_SHA,
        "base_contract_sha256": sha(BASE_CONTRACT),
        "prefab_sha256": source["prefab_sha256"],
        "material_sha256": source["material_sha256"],
        "texture_sha256": source["texture_sha256"],
        "exporter_sha256": sha(SOURCE),
        "prepare_sha256": sha(Path(__file__)),
        "verifier_sha256": sha(verifier),
        "times_seconds": [2.5, 4.9, 5.1, 7.5],
        "independent_resets_per_time": 2,
        "fixed_random_seed": 424242,
        "camera": {"kind": "top_down_orthographic_diagnostic", "size": [1024, 1024],
                   "orthographic_size": 8, "clear_rgba": [0, 0, 0, 0]},
    }
    exact(CONTRACT, (json.dumps(contract, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    report = {
        "schema": "rok.world_tile_particle_visual_prepare.v1",
        "status": "prepared_not_rendered",
        "project": PROJECT.relative_to(ROOT).as_posix(),
        "source_native": {"path": NATIVE.relative_to(ROOT).as_posix(), "sha256": NATIVE_SHA},
        "exporter": {"path": SOURCE.relative_to(ROOT).as_posix(), "sha256": sha(SOURCE)},
        "producer": {"path": Path(__file__).relative_to(ROOT).as_posix(), "sha256": sha(Path(__file__))},
        "verifier": {"path": verifier.relative_to(ROOT).as_posix(), "sha256": sha(verifier)},
        "contract": {"path": CONTRACT.relative_to(ROOT).as_posix(), "sha256": sha(CONTRACT)},
        "run_argv": ["-batchmode", "-quit", "-projectPath", str(PROJECT), "-executeMethod",
                     "UnityRokWorldTileParticleVisualProbeV1.Run", "-logFile",
                     str(PROJECT / "OutputRuns/unity_console.log")],
        "scope": "Actual Unity ParticleSystem mesh-quad GPU rendering and ReadPixels, not a baked image; isolated diagnostic camera, not GameApp world camera or LOD.",
        "requires_root_unity_execution": True,
    }
    BATCH.mkdir(parents=True, exist_ok=True)
    payload = (json.dumps(report, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    if REPORT.exists() and REPORT.read_bytes() != payload:
        raise ValueError("previous prepare report differs; preserve and issue a revision")
    if not REPORT.exists():
        with os.fdopen(os.open(REPORT, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as stream:
            stream.write(payload)
    print(json.dumps({"prepared": True, "project": str(PROJECT),
                      "contract_sha256": sha(CONTRACT), "report_sha256": sha(REPORT)}))


if __name__ == "__main__":
    main()
