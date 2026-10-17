"""Verify prepared and, after root's Unity run, native sampler observations."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCitySamplerProbeV1"
CONTRACT = PROBE / "sampler_contract.json"
EXPORTER = PROBE / "Assets/Editor/ExportRokUnityCitySamplerProbe.cs"
PRODUCER = ROOT / "Scripts/PrepareRokUnityCitySamplerProbe.py"
INVENTORY = ROOT / "Saved/RokUnityMigration/BatchReview20261009NightTimeAndExpedition/all32_source_rhi_admission_inventory.json"
GEOMETRY = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json"
UNITY_DATA = Path("C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data")


def need(condition: bool, reason: str) -> None:
    if not condition:
        raise AssertionError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def repo_path(value: str) -> Path:
    resolved = (ROOT / value).resolve()
    need(resolved.is_relative_to(ROOT), f"Source path escapes repository: {value}")
    return resolved


def safe_output(path: Path, content: bytes) -> None:
    need(path.resolve().is_relative_to(PROBE.resolve()), "Verification output escapes isolated project")
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        need(path.read_bytes() == content, "Existing verification output differs")
    else:
        with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as stream:
            stream.write(content)


def json_bytes(value: dict) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def settings_digest(records: list[dict]) -> str:
    return hashlib.sha256(json.dumps(records, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def source_values(meta: str) -> dict:
    result = {}
    for key, output in (("filterMode", "serialized_filter_mode"), ("wrapU", "serialized_wrap_u"),
                        ("wrapV", "serialized_wrap_v")):
        match = re.search(rf"(?m)^\s+{key}:\s*(-?\d+)\s*$", meta)
        need(match is not None, f"Source meta lacks {key}")
        result[output] = int(match.group(1))
    return result


def prepared() -> dict:
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    need(contract["schema"] == "rok.unity_city_sampler_contract.v1"
         and contract["repo_root"] == ROOT.as_posix()
         and contract["probe_root"] == PROBE.as_posix()
         and contract["unity_version"] == "2019.4.28f1c1", "Prepared project identity differs")
    need(contract["producer_sha256"] == sha(PRODUCER)
         and contract["exporter_sha256"] == sha(EXPORTER), "Prepared producer/exporter changed")
    need(contract["inventory_sha256"] == sha(INVENTORY)
         and contract["geometry_manifest_sha256"] == sha(GEOMETRY), "Frozen source inputs changed")
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    source_chains = {item["source_guid"]: item for item in inventory["texture_chains"]
                     if "source_sampler_meta" in item}
    need(len(source_chains) == len(contract["source_files"]) == 22,
         "22 masked source chains required")
    seen = set()
    defaults = 0
    for item in contract["source_files"]:
        guid = item["source_guid"]
        need(guid in source_chains and guid not in seen, "Texture GUID absent or duplicated")
        seen.add(guid)
        chain = source_chains[guid]
        src = repo_path(item["source_path"])
        meta = repo_path(item["meta_path"])
        copied = PROBE / item["probe_path"]
        need(item["source_path"] == chain["source_original"]["path"]
             and item["source_sha256"] == chain["source_original"]["sha256"] == sha(src)
             and item["meta_path"] == chain["source_sampler_meta"]["path"]
             and item["meta_sha256"] == chain["source_sampler_meta"]["sha256"] == sha(meta)
             and copied.resolve().is_relative_to((PROBE / "Assets/Source").resolve())
             and sha(copied) == sha(src) and sha(Path(str(copied) + ".meta")) == sha(meta),
             f"Source/copied texture byte identity differs: {guid}")
        expected = source_values(meta.read_text(encoding="utf-8-sig"))
        need(all(item[key] == value for key, value in expected.items()),
             f"Source serialized sampler differs: {guid}")
        defaults += expected["serialized_filter_mode"] == -1
    need(defaults == 12, "Twelve source default-filter metas required")
    for label, records, digest, count in (("ProjectSettings", contract["settings_files"],
                                          contract["settings_digest_sha256"], 19),
                                         ("Packages", contract["packages_files"],
                                          contract["packages_digest_sha256"], 2)):
        need(len(records) == count and settings_digest(records) == digest,
             f"Sealed {label} digest/count differs")
        for record in records:
            source = repo_path(record["source_path"])
            destination = repo_path(record["probe_path"])
            need(destination.resolve().is_relative_to((PROBE / label).resolve())
                 and sha(source) == sha(destination) == record["sha256"],
                 f"Sealed {label} file differs: {record['source_path']}")
    return contract


def compile_exporter() -> dict:
    compiler = UNITY_DATA / "Tools/Roslyn/csc.exe"
    need(compiler.is_file(), "Unity 2019 Roslyn compiler absent")
    refs = list((UNITY_DATA / "Managed/UnityEngine").glob("*.dll"))
    refs += list((UNITY_DATA / "MonoBleedingEdge/lib/mono/4.7.1-api").glob("*.dll"))
    refs += list((UNITY_DATA / "MonoBleedingEdge/lib/mono/4.7.1-api/Facades").glob("*.dll"))
    refs.append(UNITY_DATA / "Managed/UnityEditor.dll")
    need(all(ref.is_file() for ref in refs), "Unity 2019 static compiler reference absent")
    output = PROBE / "Validation/ProbeSyntax.dll"
    output.parent.mkdir(parents=True, exist_ok=True)
    args = ["/nologo", "/target:library", "/deterministic+", "/define:UNITY_EDITOR", "/out:" + str(output)]
    args += ["/reference:" + str(ref) for ref in refs]
    args.append(str(EXPORTER))
    response = PROBE / "Validation/compiler.rsp"
    response.write_text("\n".join('"' + arg + '"' for arg in args), encoding="utf-8")
    process = subprocess.run([str(compiler), "@" + str(response)], capture_output=True, check=False)
    log = process.stdout + process.stderr
    (PROBE / "Validation/compiler.log").write_bytes(log)
    need(process.returncode == 0, "Unity 2019 exporter static compilation failed; see Validation/compiler.log")
    return {"compiler": str(compiler), "exit_code": process.returncode,
            "static_compiler_output_sha256": sha(output), "unity_started": False}


def verify_run(run: Path, contract: dict) -> dict:
    run = run.resolve()
    need(run.parent == (PROBE / "OutputRuns").resolve() and run.is_dir(), "Run outside isolated project")
    need(sha(run / "sampler_contract.json") == sha(CONTRACT)
         and sha(run / "ExportRokUnityCitySamplerProbe.cs") == sha(EXPORTER),
         "Run input copies differ from frozen preparation")
    native = json.loads((run / "native_sampler.json").read_text(encoding="utf-8-sig"))
    need(native["schema"] == "rok.unity_city_sampler_native.v1"
         and native["complete"] and not native["game_app_loaded"]
         and native["contract_sha256"] == sha(CONTRACT)
         and native["exporter_sha256"] == sha(EXPORTER)
         and native["unity_version"] == "2019.4.28f1c1"
         and native["run_directory"] == run.as_posix(), "Native run identity/completion differs")
    for key in ("active_build_target", "active_build_target_group", "application_platform",
                "graphics_device", "player_color_space", "quality_active_color_space",
                "quality_level_name", "render_pipeline_asset"):
        need(native[key], f"Native platform/default context missing: {key}")
    original = {item["source_guid"]: item for item in contract["source_files"]}
    observed = native["textures"]
    need(len(observed) == 22 and {item["source_guid"] for item in observed} == set(original),
         "Native 22 texture observations incomplete")
    unknown_results = []
    for item in observed:
        spec = original[item["source_guid"]]
        for key in ("source_path", "source_sha256", "meta_sha256", "probe_path",
                    "serialized_filter_mode", "serialized_wrap_u", "serialized_wrap_v"):
            need(item[key] == spec[key], f"Native source identity differs: {key}/{item['source_guid']}")
        need(item["imported_width"] > 0 and item["imported_height"] > 0
             and item["imported_mipmap_count"] > 0
             and item["importer_filter_mode_int"] in (-1, 0, 1, 2)
             and item["imported_texture_filter_mode_int"] in (0, 1, 2)
             and item["importer_wrap_u_int"] >= 0 and item["importer_wrap_v_int"] >= 0
             and item["imported_texture_wrap_u_int"] >= 0 and item["imported_texture_wrap_v_int"] >= 0,
             f"Native effective sampler/dimension absent: {item['source_guid']}")
        for field in ("default_platform", "active_platform"):
            need(isinstance(item[field]["overridden"], bool) and item[field]["max_texture_size"] > 0,
                 f"Native platform settings incomplete: {item['source_guid']}/{field}")
        if spec["serialized_filter_mode"] == -1:
            unknown_results.append({"source_guid": item["source_guid"],
                                    "importer_filter": item["importer_filter_mode"],
                                    "imported_texture_filter": item["imported_texture_filter_mode"],
                                    "active_build_target": native["active_build_target"]})
    need(len(unknown_results) == 12, "Twelve source default-filter results required")
    result = {"schema": "rok.unity_city_sampler_native_verify.v1", "passed": True,
              "qualification": "isolated Unity 2019 editor imported Texture2D filter only; GameApp active draw/sampler still unobserved",
              "native_sha256": sha(run / "native_sampler.json"), "contract_sha256": sha(CONTRACT),
              "exporter_sha256": sha(EXPORTER), "verifier_sha256": sha(Path(__file__)),
              "textures_checked": 22, "source_default_filters_observed": unknown_results,
              "unity_version": native["unity_version"], "active_build_target": native["active_build_target"],
              "active_build_target_group": native["active_build_target_group"]}
    safe_output(run / "verified_sampler.json", json_bytes(result))
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prepared", action="store_true")
    parser.add_argument("--run", type=Path)
    args = parser.parse_args()
    need(args.prepared != bool(args.run), "Specify exactly one of --prepared or --run")
    contract = prepared()
    if args.prepared:
        compile_result = compile_exporter()
        result = {"schema": "rok.unity_city_sampler_prepare_verify.v1", "passed": True,
                  "contract_sha256": sha(CONTRACT), "producer_sha256": sha(PRODUCER),
                  "exporter_sha256": sha(EXPORTER), "verifier_sha256": sha(Path(__file__)),
                  "source_textures_checked": 22, "default_filter_metas": 12,
                  "settings_files_checked": len(contract["settings_files"]),
                  "packages_files_checked": len(contract["packages_files"]), **compile_result}
        safe_output(PROBE / "prepared_verify.json", json_bytes(result))
    else:
        result = verify_run(args.run, contract)
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
