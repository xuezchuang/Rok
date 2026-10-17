"""Prepare an isolated Unity 2019 source BakeMesh probe. Never launches Unity."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "Saved/UnityWorldTileParticleProbeV2"
PROJECT = ROOT / "Saved/UnityWorldTileParticleBakeMeshProbeV5"
SOURCE = PROJECT / "Source/UnityRokWorldTileParticleBakeMeshProbeV5.cs"
NATIVE = BASE / "OutputRuns/20261009_065833_466_26596/particle_native.json"
VISUAL = ROOT / "Saved/UnityWorldTileParticleVisualProbeV3/OutputRuns/20261009_072207_835_41412/visual_native.json"
GEOMETRY = ROOT / "Saved/UnityWorldTileParticleGeometryProbeV4/OutputRuns/20261009_075655_706_35456/geometry_native.json"
BASE_CONTRACT = BASE / "probe_contract.json"
CONTRACT = PROJECT / "probe_contract.json"
BATCH = ROOT / "Saved/RokUnityMigration/BatchReview20261009MiddleTipAndTileParticleGeometry/TileBakedGeometrySource"
REPORT = BATCH / "bakemesh_probe_prepare_v5.json"
NATIVE_SHA = "ce7d850f6dbb696a0d34e96df2d7f7ac0658eceadacd694d579a5f1ad3b6c75c"
VISUAL_SHA = "f8a48d3bb4e5c30f01dd1d8d595bbb05b935252d1e68050a93e2e988add21a6d"
GEOMETRY_SHA = "11483cc64cc1564c1a22fca88f6da817d70b6bdfab259900f562c379d5eff1d4"
BAKEMESH_DOC = Path("C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data/Managed/UnityEngine/UnityEngine.ParticleSystemModule.xml")
BAKEMESH_DOC_SHA = "e11fcf456a59a5ca86d3f24c89f79c4348ebebeb1613c5f843d8a4dff160ba0f"
BUILTIN = Path("C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data/Resources/unity_builtin_extra")
BUILTIN_SHA = "c73e9529e350fcc391f46a5cc323293a07a4b5ed71bb69b25d888fa794a99547"


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
    if sha(VISUAL) != VISUAL_SHA or json.loads(VISUAL.read_text(encoding="utf-8"))["complete"] is not True:
        raise ValueError("sealed V3 live GPU witness changed or incomplete")
    if sha(GEOMETRY) != GEOMETRY_SHA or json.loads(GEOMETRY.read_text(encoding="utf-8"))["complete"] is not True:
        raise ValueError("sealed V4 runtime geometry witness changed or incomplete")
    if sha(BAKEMESH_DOC) != BAKEMESH_DOC_SHA or "BakeMesh(UnityEngine.Mesh,UnityEngine.Camera,System.Boolean)" not in BAKEMESH_DOC.read_text(encoding="utf-8"):
        raise ValueError("Unity 2019 BakeMesh API documentation changed")
    if sha(BUILTIN) != BUILTIN_SHA:
        raise ValueError("Unity 2019 packaged built-in shader archive changed")
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
    exporter = PROJECT / "Assets/Editor/UnityRokWorldTileParticleBakeMeshProbeV5.cs"
    exact(exporter, SOURCE.read_bytes())
    exact(exporter.with_name(exporter.name + ".meta"),
          (BASE / "Assets/Editor/UnityRokWorldTileParticleOracleV2.cs.meta").read_bytes())
    verifier = ROOT / "Scripts/VerifyRokUnityWorldTileParticleBakeMeshProbeV5.py"
    contract = {
        "schema": "rok.world_tile_particle_bakemesh_contract.v5",
        "unity_version_target": "2019.4.28f1c1",
        "source_native_sha256": NATIVE_SHA,
        "source_gpu_v3_sha256": VISUAL_SHA,
        "source_geometry_v4_sha256": GEOMETRY_SHA,
        "unity2019_bakemesh_api_doc_sha256": BAKEMESH_DOC_SHA,
        "bakemesh_api": "ParticleSystemRenderer.BakeMesh(Mesh,Camera,bool) false and true",
        "unity_builtin_extra_sha256": BUILTIN_SHA,
        "builtin_shader_source_status": "not_found_locally; packaged unity_builtin_extra is a binary asset archive, not ShaderLab source",
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
        "source_particle_system_names_sorted": ["bowen", "bowen (1)", "shitou (1)",
                                                "shuahua2", "shuihua", "xia-shui"],
        "selected_particle_system_names": ["bowen", "bowen (1)"],
        "render_modes_per_reset": ["bowen_only", "bowen_1_only", "combined"],
        "particle_current_color": "Particle.GetCurrentColor(system) plus normalized age and source ColorOverLifetime keys",
        "geometry_fields": ["Particle.rotation3D", "Particle.axisOfRotation", "Particle.GetCurrentSize3D(system)",
                            "Particle.position/worldPosition", "ParticleSystemRenderer.alignment/renderMode/pivot/flip",
                            "builtin Quad vertices/normals/uv0/triangles/bounds",
                            "system localToWorld/worldToLocal and simulation space"],
        "baked_geometry_fields": ["vertices", "normals", "uv0", "colors32", "triangles", "bounds"],
        "association_rule": "unordered quads by triangle connectivity, UV and per-quad Color32 multiset; never assume GetParticles array order",
        "camera": {"kind": "top_down_orthographic_diagnostic", "size": [1024, 1024],
                   "orthographic_size": 8, "clear_rgba": [0, 0, 0, 0]},
    }
    exact(CONTRACT, (json.dumps(contract, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    report = {
        "schema": "rok.world_tile_particle_bakemesh_prepare.v5",
        "status": "prepared_not_rendered",
        "project": PROJECT.relative_to(ROOT).as_posix(),
        "source_native": {"path": NATIVE.relative_to(ROOT).as_posix(), "sha256": NATIVE_SHA},
        "source_gpu_v3": {"path": VISUAL.relative_to(ROOT).as_posix(), "sha256": VISUAL_SHA},
        "source_geometry_v4": {"path": GEOMETRY.relative_to(ROOT).as_posix(), "sha256": GEOMETRY_SHA},
        "unity2019_bakemesh_api_doc": {"path": str(BAKEMESH_DOC), "sha256": BAKEMESH_DOC_SHA,
                                        "summary": "useTransform specifies whether to include Transform rotation and scale"},
        "builtin_shader_source": {"status": "not_found_locally", "shader_name": "Mobile/Particles/Alpha Blended",
                                  "searched": ["Unity 2019 Editor/Data *.shader and *.cginc",
                                               "repository ref/client-unity and ref/resources *.shader"],
                                  "packaged_archive": str(BUILTIN), "packaged_archive_sha256": BUILTIN_SHA,
                                  "blend_formula": None, "alpha_formula": None},
        "exporter": {"path": SOURCE.relative_to(ROOT).as_posix(), "sha256": sha(SOURCE)},
        "producer": {"path": Path(__file__).relative_to(ROOT).as_posix(), "sha256": sha(Path(__file__))},
        "verifier": {"path": verifier.relative_to(ROOT).as_posix(), "sha256": sha(verifier)},
        "contract": {"path": CONTRACT.relative_to(ROOT).as_posix(), "sha256": sha(CONTRACT)},
        "run_argv": ["-batchmode", "-quit", "-projectPath", str(PROJECT), "-executeMethod",
                     "UnityRokWorldTileParticleBakeMeshProbeV5.Run", "-logFile",
                     str(PROJECT / "OutputRuns/unity_console.log")],
        "scope": "Source prefab six-system identity and exact two active bowen selections. Two resets/four times retain isolated GPU captures and export actual ParticleSystemRenderer.BakeMesh false/true vertices/normals/UV/colors/triangles. Strict verifier checks mesh transform relation and unordered quad UV/Color32 association. ShaderLab blend/alpha source remains unavailable locally. Diagnostic only, not production replay.",
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
