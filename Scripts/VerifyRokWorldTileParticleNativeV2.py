"""Strict offline validator for the isolated Unity 2019 world-tile particle V2 witness."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BATCH = ROOT / "Saved/RokUnityMigration/BatchReview20261009WorldSearchAndTileEffects/TileParticleSource"
PROBE = ROOT / "Saved/UnityWorldTileParticleProbeV2"
SOURCE = BATCH / "source_v1.json"
PREPARE = BATCH / "native_prepare_v2.json"
CONTRACT = PROBE / "probe_contract.json"
EXPORTER = ROOT / "Scripts/UnityRokWorldTileParticleOracleV2.cs"
PRODUCER = ROOT / "Scripts/PrepareRokWorldTileParticleProbeV2.py"
SOURCE_SHA = "3ac643b894e56d1a6b07935beb1ddee94967d453798bb8dcb23f63b0fb4f2161"
TIMES = (0.0, 0.5, 1.0, 2.5, 4.9, 5.1, 7.5)
NAMES = ("bowen", "bowen (1)")
SEED_WARNING = "setting the random seed while system is still playing"


def fail(message: str) -> None:
    raise ValueError(message)


def check(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pin(path: Path) -> dict:
    check(path.is_file(), f"missing pinned file: {path}")
    return {"path": path.relative_to(ROOT).as_posix(), "sha256": sha(path), "bytes": path.stat().st_size}


def close(a: float, b: float, tolerance: float = 1e-5) -> bool:
    return math.isfinite(float(a)) and math.isfinite(float(b)) and abs(float(a) - float(b)) <= tolerance


def vector_close(actual: dict, source: dict, tolerance: float = 1e-5) -> bool:
    return all(close(actual[k], source[k], tolerance) for k in ("x", "y", "z"))


def quat_close(actual: dict, source: dict, tolerance: float = 1e-5) -> bool:
    keys = ("x", "y", "z", "w")
    return all(close(actual[k], source[k], tolerance) for k in keys) or all(
        close(actual[k], -source[k], tolerance) for k in keys)


def qmul(a: dict, b: dict) -> dict:
    ax, ay, az, aw = (a[k] for k in ("x", "y", "z", "w"))
    bx, by, bz, bw = (b[k] for k in ("x", "y", "z", "w"))
    return {
        "x": aw * bx + ax * bw + ay * bz - az * by,
        "y": aw * by - ax * bz + ay * bw + az * bx,
        "z": aw * bz + ax * by - ay * bx + az * bw,
        "w": aw * bw - ax * bx - ay * by - az * bz,
    }


def qrotate(q: dict, v: dict) -> dict:
    qv = {"x": v["x"], "y": v["y"], "z": v["z"], "w": 0.0}
    qi = {"x": -q["x"], "y": -q["y"], "z": -q["z"], "w": q["w"]}
    result = qmul(qmul(q, qv), qi)
    return {k: result[k] for k in ("x", "y", "z")}


def validate_inputs() -> tuple[dict, dict, dict, list[dict]]:
    check(sha(SOURCE) == SOURCE_SHA, "source manifest SHA drift")
    source = json.loads(SOURCE.read_text(encoding="utf-8"))
    check(source["schema"] == "rok.world_tile_particle_source.v1", "source schema drift")
    check(len(source["systems"]) == 2 and all(
        len([key for key in row["particle_system"] if key.endswith("Module")]) == 22
        and "InitialModule" in row["particle_system"] for row in source["systems"]),
        "source module census: InitialModule plus 21 modules per system")
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    prepare = json.loads(PREPARE.read_text(encoding="utf-8"))
    check(contract["schema"] == "rok.world_tile_particle_probe_contract.v2", "contract schema")
    check(prepare["schema"] == "rok.world_tile_particle_native_prepare.v2"
          and prepare["status"] == "prepared_not_executed", "producer report status")
    check(contract["source_manifest_sha256"] == SOURCE_SHA, "contract source manifest SHA")
    check(contract["exporter_sha256"] == sha(EXPORTER), "contract exporter SHA")
    check(contract["prepare_sha256"] == sha(PRODUCER), "contract producer SHA")
    check(contract["verifier_sha256"] == sha(Path(__file__)), "contract verifier SHA")
    check(prepare["contract"]["sha256"] == sha(CONTRACT), "prepare report contract SHA")
    check(prepare["source_manifest"]["sha256"] == SOURCE_SHA, "prepare report source SHA")
    check(prepare["oracle_source"]["sha256"] == sha(EXPORTER)
          and prepare["producer_source"]["sha256"] == sha(PRODUCER)
          and prepare["verifier_source"]["sha256"] == sha(Path(__file__)), "prepare report script SHA")
    check(prepare["isolated_project"] == PROBE.relative_to(ROOT).as_posix()
          and prepare["native_witness_args"][5] == "UnityRokWorldTileParticleOracleV2.Run",
          "prepare report isolated identity/entry")
    records: list[dict] = []
    for kind, name in (("prefab", "Tile-JuDian-A2_lod2.prefab"),
                       ("material", "Ring_06_light.mat"), ("texture", "Ring_07.png")):
        row = source["source_files"][kind]
        path, meta = ROOT / row["path"], ROOT / row["meta_path"]
        copy, copy_meta = PROBE / "Assets/ProbeInputs" / name, PROBE / "Assets/ProbeInputs" / (name + ".meta")
        check(sha(path) == row["sha256"] == contract[kind + "_sha256"], f"{kind} source SHA")
        check(sha(meta) == row["meta_sha256"], f"{kind} source meta SHA")
        check(sha(copy) == sha(path) and sha(copy_meta) == sha(meta), f"{kind} isolated copy")
        check(contract[kind + "_guid"] == row["guid"], f"{kind} GUID")
        records.extend((pin(path), pin(meta), pin(copy), pin(copy_meta)))
    check(sha(PROBE / "Assets/Editor/UnityRokWorldTileParticleOracleV2.cs") == sha(EXPORTER),
          "isolated exporter copy differs")
    records.extend((pin(SOURCE), pin(CONTRACT), pin(PREPARE), pin(EXPORTER), pin(PRODUCER),
                    pin(Path(__file__)), pin(PROBE / "Assets/Editor/UnityRokWorldTileParticleOracleV2.cs")))
    return source, contract, prepare, records


def validate_run(run: dict, system_name: str, time: float) -> None:
    count = run["particle_count"]
    particles = run["particles"]
    check(type(count) is int and 0 <= count <= 1024, f"{system_name} count range at {time}")
    check(isinstance(particles, list) and len(particles) == count,
          f"{system_name} full particle payload count at {time}")
    check(run["simulation_space"] == "Local" and run["is_playing"] is False,
          f"{system_name} reset simulation state at {time}")
    for field in ("position_min", "position_max"):
        check(all(math.isfinite(float(run[field][axis])) for axis in ("x", "y", "z")),
              f"{system_name} finite bounds")
    for field in ("size_min", "size_max", "remaining_lifetime_min", "remaining_lifetime_max"):
        check(math.isfinite(float(run[field])), f"{system_name} finite {field}")
    check(run["size_min"] <= run["size_max"] and
          run["remaining_lifetime_min"] <= run["remaining_lifetime_max"], f"{system_name} range order")
    for particle in particles:
        for field in ("position", "velocity"):
            check(all(math.isfinite(float(particle[field][axis])) for axis in ("x", "y", "z")),
                  f"{system_name} finite particle {field}")
        for field in ("remaining_lifetime", "start_lifetime", "start_size", "current_size", "rotation"):
            check(math.isfinite(float(particle[field])), f"{system_name} finite particle {field}")
        check(0 <= particle["random_seed"] <= 0xFFFFFFFF and
              particle["start_lifetime"] > 0 and particle["current_size"] >= 0,
              f"{system_name} particle physical fields")
        check(set(particle["start_color"]) == {"r", "g", "b", "a"} and
              all(type(v) is int and 0 <= v <= 255 for v in particle["start_color"].values()),
              f"{system_name} particle start color bytes")


def validate_pair(repeats: list[dict], system_name: str, time: float) -> None:
    check(isinstance(repeats, list) and len(repeats) == 2,
          f"{system_name} two independent reset outputs")
    for run in repeats:
        validate_run(run, system_name, time)
    check(json.dumps(repeats[0], sort_keys=True, separators=(",", ":"), allow_nan=False) ==
          json.dumps(repeats[1], sort_keys=True, separators=(",", ":"), allow_nan=False),
          f"{system_name} two full particle runs differ at {time}")


def validate_seed_log(log: str) -> None:
    check(SEED_WARNING not in log.lower(), "Unity rejected randomSeed while playing")


def validate_system(system: dict, source_row: dict) -> dict:
    name = system["name"]
    check(name == source_row["name"], "system identity/order")
    expected_chain = source_row["ancestor_chain_child_to_root"]
    native_chain = system["ancestor_chain_child_to_root"]
    check(len(native_chain) == len(expected_chain), f"{name} hierarchy depth")
    for native, expected in zip(native_chain, expected_chain):
        check(native["name"].replace("(Clone)", "") == expected["name"], f"{name} hierarchy node")
        check(vector_close(native["local_position"], expected["local_position"], 2e-4)
              and vector_close(native["local_scale"], expected["local_scale"], 2e-4)
              and quat_close(native["local_rotation"], expected["local_rotation"], 2e-4),
              f"{name} source ancestor local TRS")
    child, parent = expected_chain[:2]
    rotated = qrotate(parent["local_rotation"], {
        axis: child["local_position"][axis] * parent["local_scale"][axis] for axis in ("x", "y", "z")})
    expected_world = {axis: parent["local_position"][axis] + rotated[axis] for axis in ("x", "y", "z")}
    check(vector_close(system["world_position"], expected_world, 3e-4), f"{name} composed world position")
    check(quat_close(system["world_rotation"], qmul(parent["local_rotation"], child["local_rotation"]), 3e-4),
          f"{name} composed world rotation")
    check(vector_close(system["world_lossy_scale"], child["local_scale"], 3e-4),
          f"{name} composed world scale")
    check(system["render_mode"] == "Mesh" and system["render_alignment"] == "Local"
          and system["mesh_name"] == "Quad"
          and system["mesh_guid"] == "0000000000000000e000000000000000"
          and system["mesh_local_file_id"] == source_row["renderer"]["m_Mesh"]["fileID"] == 10210
          and system["mesh_asset_path"] and system["mesh_vertex_count"] == 4
          and system["mesh_triangle_index_count"] == 6, f"{name} built-in mesh actual binding")
    check(close(system["duration"], 5.0) and system["loop"] is True
          and system["play_on_awake"] is True and system["prewarm"] is False
          and system["simulation_space"] == "Local" and system["scaling_mode"] == "Local"
          and system["shape_type"] == "SingleSidedEdge"
          and close(system["shape_radius"], 0.58) and close(system["shape_arc"], 153.4)
          and system["emission_rate_over_time"]["mode"] == "Constant"
          and close(system["emission_rate_over_time"]["constant_max"], 1.0),
          f"{name} native source particle configuration")
    check(system["material_asset_paths"] == ["Assets/ProbeInputs/Ring_06_light.mat"] * 2,
          f"{name} two source material bindings")
    check(len(system["samples"]) == len(TIMES), f"{name} times count")
    nonzero = 0
    summary = []
    for expected_time, sample in zip(TIMES, system["samples"]):
        check(close(sample["time_seconds"], expected_time, 1e-4), f"{name} sample time")
        repeats = sample["independent_resets"]
        validate_pair(repeats, name, expected_time)
        nonzero += int(repeats[0]["particle_count"] > 0)
        summary.append({"time": expected_time, "particle_count": repeats[0]["particle_count"]})
    check(nonzero >= 2, f"{name} no meaningful nonzero simulation samples")
    return {"name": name, "nonzero_times": nonzero, "samples": summary}


def validate_native(data: dict, log: str, source: dict, contract: dict, expected_contract_sha: str) -> list[dict]:
    check(data["schema"] == "rok.world_tile_particle_native_witness.v2"
          and data["unity_version"] == "2019.4.28f1c1", "native schema/Unity version")
    check(data["project_path"].replace("\\", "/") == str(PROBE).replace("\\", "/"),
          "native isolated project identity")
    check(data["contract_sha256"] == expected_contract_sha
          and data["source_manifest_sha256"] == SOURCE_SHA
          and data["exporter_sha256"] == contract["exporter_sha256"], "native input pins")
    for kind in ("prefab", "material", "texture"):
        check(data[kind + "_sha256"] == contract[kind + "_sha256"], f"native {kind} source hash")
    check(data["fixed_random_seed"] == 424242
          and "Stop(true, StopEmittingAndClear)" in data["reset_policy"]
          and "Simulate(time,true,true,true)" in data["reset_policy"], "native reset policy")
    material = data["material"]
    check(material["asset_path"] == "Assets/ProbeInputs/Ring_06_light.mat"
          and material["shader_name"] == "Mobile/Particles/Alpha Blended"
          and material["main_texture_asset_path"] == "Assets/ProbeInputs/Ring_07.png"
          and material["texture_width"] == material["texture_height"] == 128
          and material["render_queue"] == 3000 and material["render_type_tag"] == "Transparent"
          and material["has_main_texture"] is True and material["has_tint"] is False
          and material["has_inv_fade"] is False, "native source shader/texture actual binding")
    stored = next(v["_TintColor"] for v in source["material"]["m_SavedProperties"]["m_Colors"]
                  if "_TintColor" in v)
    check(stored == {"r": 0.5, "g": 0.5, "b": 0.5, "a": 0.5},
          "serialized TintColor source changed")
    validate_seed_log(log)
    check("Rok world tile particle native witness:" in log, "Unity witness completion marker absent")
    systems = data["systems"]
    check(isinstance(systems, list) and len(systems) == 2
          and [row["name"] for row in systems] == list(NAMES), "native system count/order")
    return [validate_system(native, source_row) for native, source_row in zip(systems, source["systems"])]


def negative_self_test() -> dict:
    import copy
    run = {"particle_count": 1, "particles": [{
        "position": {"x": 0., "y": 0., "z": 0.},
        "velocity": {"x": 0., "y": 0., "z": 0.},
        "remaining_lifetime": 1., "start_lifetime": 2., "start_size": 1.,
        "current_size": 1., "rotation": 0., "random_seed": 42,
        "start_color": {"r": 255, "g": 255, "b": 255, "a": 255}}],
        "position_min": {"x": 0., "y": 0., "z": 0.},
        "position_max": {"x": 0., "y": 0., "z": 0.},
        "size_min": 1., "size_max": 1.,
        "remaining_lifetime_min": 1., "remaining_lifetime_max": 1.,
        "simulation_space": "Local", "is_playing": False}
    validate_pair([copy.deepcopy(run), copy.deepcopy(run)], "synthetic", 2.5)
    rejected: list[str] = []

    def deny(name: str, candidate: dict) -> None:
        try:
            validate_run(candidate, "synthetic", 2.5)
        except ValueError:
            rejected.append(name)
            return
        fail("negative test accepted: " + name)

    bad = copy.deepcopy(run); bad["particle_count"] = 2; deny("particle_count_payload_mismatch", bad)
    bad = copy.deepcopy(run); bad["particles"][0]["position"]["x"] = float("nan")
    deny("nonfinite_particle_position", bad)
    bad = copy.deepcopy(run); bad["is_playing"] = True; deny("playing_after_reset", bad)
    bad = copy.deepcopy(run); bad["particles"][0]["start_lifetime"] = 0
    deny("invalid_particle_lifetime", bad)
    pair = [copy.deepcopy(run), copy.deepcopy(run)]
    pair[1]["particles"][0]["position"]["x"] = 0.25
    try:
        validate_pair(pair, "synthetic", 2.5)
    except ValueError:
        rejected.append("independent_repeat_particle_mismatch")
    else:
        fail("negative test accepted: independent_repeat_particle_mismatch")
    try:
        validate_seed_log("Warning: " + SEED_WARNING + " is not supported")
    except ValueError:
        rejected.append("seed_warning_in_unity_log")
    else:
        fail("negative test accepted: seed_warning_in_unity_log")
    with tempfile.TemporaryDirectory(prefix="rok_tile_native_v2_negative_") as folder:
        path = Path(folder) / "pinned.txt"
        path.write_bytes(b"original")
        expected = sha(path)
        path.write_bytes(b"tampered")
        check(sha(path) != expected, "tampered source pin went undetected")
        rejected.append("tampered_input_digest")
    return {"in_memory_positive": True, "negative_cases": rejected, "real_native_run": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--native", help="Immutable V2 OutputRuns/.../particle_native.json")
    mode.add_argument("--self-test", action="store_true")
    parser.add_argument("--log", help="Unity batch log for --native")
    args = parser.parse_args()
    if args.self_test:
        print(json.dumps(negative_self_test(), ensure_ascii=False))
        return
    check(args.log is not None, "--log required for native verification")
    native, log_path = Path(args.native).resolve(), Path(args.log).resolve()
    check(native.is_file() and native.parent.parent == PROBE / "OutputRuns"
          and native.name == "particle_native.json", "native file is not an isolated V2 run")
    check(log_path.is_file() and log_path.is_relative_to(ROOT / "Saved/Logs"), "Unity log path")
    source, contract, prepare, input_pins = validate_inputs()
    data = json.loads(native.read_text(encoding="utf-8"))
    log = log_path.read_text(encoding="utf-8-sig", errors="replace")
    systems = validate_native(data, log, source, contract, sha(CONTRACT))
    report = {
        "schema": "rok.world_tile_particle_native_verify.v2",
        "passed": True, "goal_complete": False, "full_migration_accepted": False,
        "source_only_native_particle_state": True, "ue_effect_accepted": False,
        "native": pin(native), "unity_log": pin(log_path),
        "input_pins": input_pins, "systems": systems,
        "source_module_census_per_system": {"InitialModule": 1, "other_modules": 21},
        "seed_warning_count": log.lower().count(SEED_WARNING),
        "limits": ["No GPU pixels, tile LOD activation, game-camera rendering or UE effect acceptance."],
    }
    target = BATCH / ("native_verify_v2_" + native.parent.name + ".json")
    payload = (json.dumps(report, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if target.exists():
        check(target.read_bytes() == payload, "refusing to overwrite different native verify report")
    else:
        with os.fdopen(os.open(target, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644), "wb") as stream:
            stream.write(payload)
    print(json.dumps({"passed": True, "report": target.relative_to(ROOT).as_posix(),
                      "report_sha256": sha(target), "systems": systems}, ensure_ascii=False))


if __name__ == "__main__":
    main()
