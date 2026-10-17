"""Strict offline check of one isolated Unity 2019 construction-effect export."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
from pathlib import Path
from rok_data_paths import resolve


ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "Saved/RokUnityMigration/BatchReview20261009Construction/construction_effect_source_inventory.json"
EXPECTED_INVENTORY = "01dd55e415bfc8f9284642f22e73f47e111a1b34ccaf126788e7357189844ffe"
PREFABS = {"Build_shelf_5", "build_3003", "build_3004"}
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def need(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            h.update(chunk)
    return h.hexdigest()


def inside(parent: Path, child: Path) -> bool:
    return child == parent or parent in child.parents


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", required=True, type=Path)
    parser.add_argument("--log", required=True, type=Path)
    args = parser.parse_args()
    run = args.run.resolve()
    probe = run.parent.parent
    need(run.is_dir() and run.parent.name == "OutputRuns" and
         probe.parent == (ROOT / "Saved").resolve() and
         re.fullmatch(r"UnityCityConstructionEffectsProbeV[2-9][0-9]*", probe.name),
         "Run is outside a revisioned isolated probe")
    log = args.log.resolve()
    need(log.is_file() and log.parent == (ROOT / "Saved/Logs").resolve(),
         "Unity log missing or outside Saved/Logs")
    log_text = log.read_text(encoding="utf-8-sig", errors="replace")
    need("Setting the random seed while system is still playing" not in log_text and
         "ROK_CONSTRUCTION_EFFECT_NATIVE_EXPORTED" in log_text and
         str(run).replace("\\", "/") in log_text.replace("\\", "/") and
         " error CS" not in log_text and "Unhandled Exception" not in log_text,
         "Unity log is unrelated to this run or contains seed, compiler or unhandled runtime errors")
    need(digest(INVENTORY) == EXPECTED_INVENTORY, "Sealed source inventory changed")
    contract_path = probe / "source_contract.json"
    need(contract_path.is_file(), "Probe source contract missing")
    contract = json.loads(contract_path.read_text(encoding="utf-8-sig"))
    need(contract["schema"] == "rok.construction_effect_native_probe_contract.v1", "Contract schema differs")
    need(contract["unity_version"] == "2019.4.28f1c1" and
         contract["source_color_space"] == "Gamma" and
         contract["source_inventory_sha256"] == EXPECTED_INVENTORY and
         Path(contract["probe_root"]).resolve() == probe and
         Path(contract["repo_root"]).resolve() == ROOT.resolve(), "Contract identity differs")
    need(contract["exporter_sha256"] ==
         digest(probe / "Assets/Editor/ExportRokUnityConstructionEffectsNative.cs"),
         "Exporter code is not pinned to this run's copied source")
    settings = ROOT / "ref/client-unity/ProjectSettings/ProjectSettings.asset"
    need(digest(settings) == contract["source_project_settings_sha256"] and
         "  m_ActiveColorSpace: 0" in settings.read_text(encoding="utf-8-sig"),
         "Source Gamma project setting changed")
    source_files = contract["source_files"]
    need(len(source_files) == 54 and sum(bool(item["copied"]) for item in source_files) == 46,
         "Source-copy inventory differs")
    omitted_scripts = contract["omitted_script_guid_refs"]
    need(len(omitted_scripts) == 4 and all(".cs" in entry for entry in omitted_scripts),
         "Referenced missing-script GUID inventory differs")
    need(contract["allowed_builtin_guid_refs"] ==
         ["0000000000000000f000000000000000"],
         "Unity built-in GUID allowlist differs")
    for item in source_files:
        rel = item["source_path"]
        need(rel.startswith("ref/client-unity/Assets/") and ".." not in rel,
             f"Unsafe source path: {rel}")
        source = resolve(rel)
        need(source.is_file() and digest(source) == item["source_sha256"] and
             digest(Path(str(source) + ".meta")) == item["meta_sha256"],
             f"Source asset/meta changed: {rel}")
        if item["copied"]:
            probe_rel = item["probe_path"]
            copied = (probe / probe_rel).resolve()
            need(probe_rel.startswith("Assets/Source/") and inside(probe, copied) and
                 copied.is_file() and digest(copied) == item["source_sha256"] and
                 digest(Path(str(copied) + ".meta")) == item["meta_sha256"],
                 f"Copied asset/meta changed: {probe_rel}")
        else:
            need(rel.endswith(".cs") and item["probe_path"] == "",
                 f"Non-script dependency omitted: {rel}")
    report_path = run / "native_effect_inventory.json"
    need(report_path.is_file() and not (run / "failure.txt").exists(),
         "Native export failed or is incomplete")
    report = json.loads(report_path.read_text(encoding="utf-8-sig"))
    need(report["schema"] == "rok.construction_effect_native_inventory.v1" and
         report["passed"] is True and
         report["game_app_loaded"] is False and
         report["server_events_replayed"] is False and
         report["sample_is_authored_curve"] is False and
         report["unity_version"] == "2019.4.28f1c1" and
         report["color_space"] == "Gamma" and
         Path(report["project_path"]).resolve() == probe and
         Path(report["run_directory"]).resolve() == run and
         report["source_inventory_sha256"] == EXPECTED_INVENTORY and
         report["source_contract_sha256"] == digest(contract_path) and
         report["exporter_sha256"] == contract["exporter_sha256"],
         "Native export identity or scope differs")
    need(report["allowed_builtin_guid_refs"] == contract["allowed_builtin_guid_refs"],
         "Native built-in shader GUID scope differs")
    need(report.get("failures") in (None, []), "Native export reported failures")
    need({p["name"] for p in report["prefabs"]} == PREFABS and
         len(report["prefabs"]) == 3, "Missing source prefabs")
    need({c["name"] for c in report["animation_clips"]} >=
         {"build_scaffold_1", "build_scaffold_5", "build_scaffold_6", "build_scaffold_10"},
         "Source scaffold clips missing")
    need(report["materials"] and all(
        m["shader_source_available"] == bool((m["shader"] or {}).get("source_path")) and
        (m["shader_runtime_resolved"] or
         (m["authored_shader_guid"] in contract["allowed_builtin_guid_refs"] and
          not m["shader_source_available"]))
        for m in report["materials"]),
        "Material shader resolution/source availability incomplete or non-builtin missing")
    for clip in report["animation_clips"]:
        need(clip["length"] >= 0 and clip["frame_rate"] > 0 and
             isinstance(clip["curves"], list) and isinstance(clip["events"], list),
             f"Clip fields incomplete: {clip['name']}")
        for curve in clip["curves"]:
            times = [k["time"] for k in curve["keys"]]
            need(times == sorted(times) and all(math.isfinite(t) for t in times),
                 f"Clip key order invalid: {clip['name']}")
    png_count = 0
    for prefab in report["prefabs"]:
        name = prefab["name"]
        need(prefab["asset"]["source_path"].endswith("/" + name + ".prefab") and
             prefab["nodes"] and prefab["source_derived_horizon"] > 0 and
             prefab["observed_step"] > 0 and len(prefab["samples"]) > 1 and
             prefab["lifecycle_limit"], f"Incomplete prefab export: {name}")
        need(all("active_self" in n and "active_in_hierarchy" in n and
                 "components" in n and "local_to_world" in n for n in prefab["nodes"]),
             f"Hierarchy/state incomplete: {name}")
        if name == "Build_shelf_5":
            need(prefab["animations"], "Scaffold Animation components missing")
        else:
            need(prefab["particle_systems"], f"Particle systems missing: {name}")
        for particle in prefab["particle_systems"]:
            need(particle["serialized_unity_json"] and
                 particle["derived_one_cycle_end"] >= 0 and
                 particle["start_delay_max"] >= 0 and
                 particle["start_lifetime_max"] >= 0,
                 f"Authored particle fields missing: {name}")
        samples = prefab["samples"]
        times = [s["time"] for s in samples]
        need(abs(times[0]) <= 1e-5 and times == sorted(times) and
             abs(times[-1] - prefab["source_derived_horizon"]) <= 1e-3 and
             all(math.isfinite(t) for t in times), f"Source-derived sample axis differs: {name}")
        need(any(s["diagnostic_png"] and
                 any(p["count"] > 0 for p in s["particles"]) for s in samples),
             f"No diagnostic captured while native particles were live: {name}")
        for sample in samples:
            need(len(sample["particles"]) == len(prefab["particle_systems"]),
                 f"Particle sample count differs: {name}")
            for particle in sample["particles"]:
                need(particle["count"] >= 0 and
                     len(particle["particles"]) == particle["count"] and
                     particle["stopped_before_seed"] is True and
                     particle["seed_verified_before_simulate"] is True and
                     isinstance(particle["simulation_seed"], int),
                     f"Particle observation incomplete: {name}")
            png = sample["diagnostic_png"]
            if png:
                path = (run / png).resolve()
                need(path.parent == run and path.is_file() and
                     path.read_bytes()[:8] == PNG_SIGNATURE and
                     digest(path) == sample["diagnostic_png_sha256"],
                     f"Diagnostic PNG invalid: {png}")
                png_count += 1
    need(png_count >= 6, "Source-derived diagnostic frames missing")
    need(report["diagnostic_camera"] and report["runtime_lifecycle_limit"] and
         isinstance(report["unresolved_references"], list),
         "Isolation limits or unresolved references omitted")
    need(all(any(entry in unresolved for unresolved in report["unresolved_references"])
             for entry in omitted_scripts),
         "Missing-script references were not disclosed in native output")
    print(f"PASS: Unity 2019 construction effect native export, 3 prefabs, "
          f"{len(report['animation_clips'])} clips, {png_count} diagnostic PNGs, run={run}")


if __name__ == "__main__":
    main()
