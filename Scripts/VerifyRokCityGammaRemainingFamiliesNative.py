"""Prepare and verify an isolated Unity 2019 input probe for five remaining city rows."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityGammaRemainingFamiliesProbe"
CLOSURE = ROOT / "Saved/RokUnityMigration/CityGammaSourceClosure/20261008T221227Z_39568_2aac39ec/catalog.json"
CLOSURE_SHA = "6349487fdd83e0bf3a0f93aaab4b41643026dec5d253185a9cd36a7fd9d0d49c"
EXPORTER = ROOT / "Scripts/ExportRokCityGammaRemainingFamiliesNative.cs"
RUNNER = ROOT / "Scripts/RunRokCityGammaRemainingFamiliesNative.ps1"
KEYS = ("initial:5", "ground:ground_1", "ground:ground_2", "support:boundary_support_road", "support:boundary_support_wall_shadow")
CONTRACT_SCHEMA = "rok.city_gamma_remaining_families_contract.v1"
NATIVE_SCHEMA = "rok.city_gamma_remaining_families_native.v1"
PREFIX = "ref/client-unity/Assets/"


def need(ok: bool, why: str) -> None:
    if not ok:
        raise ValueError(why)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path: Path) -> dict:
    need(path.is_file(), f"missing {path}")
    obj = json.loads(path.read_text(encoding="utf-8-sig"))
    need(isinstance(obj, dict), f"bad JSON root {path}")
    return obj


def source_rel(value: str) -> str:
    text = value.replace("\\", "/")
    at = text.find("Assets/")
    need(at >= 0 and text != "builtin", f"not a Unity asset: {value}")
    rel = "ref/client-unity/" + text[at:]
    need(rel.startswith(PREFIX) and ".." not in Path(rel).parts, f"unsafe source {value}")
    return rel


def rows() -> list[dict]:
    need(sha(CLOSURE) == CLOSURE_SHA, "sealed closure SHA differs")
    catalog = read(CLOSURE)
    need(catalog.get("schema") == "rok.city_gamma.source_closure_catalog.v1" and
         len(catalog.get("rows", [])) == 32, "closure schema/count differs")
    chosen = {r["source_key"]: r for r in catalog["rows"] if r["source_key"] in KEYS}
    need(set(chosen) == set(KEYS), "five-family row set differs")
    return [chosen[k] for k in KEYS]


def make_contract() -> dict:
    selected = rows()
    files: dict[str, dict] = {}
    rows_out = []
    for row in selected:
        role_map = {}
        for item in row["source_files"]:
            if item["role"] == "source_obj" or item["source_path"] == "builtin":
                continue
            rel = source_rel(item["source_path"])
            path = ROOT / rel
            need(item["exists"] and path.is_file() and sha(path) == item["sha256"], f"source drift: {rel}")
            meta = Path(str(path) + ".meta")
            need(meta.is_file(), f"missing source .meta: {rel}")
            match = re.search(r"(?m)^guid:\s*([0-9a-fA-F]{32})\s*$", meta.read_text(encoding="utf-8-sig"))
            need(match is not None, f"missing GUID: {rel}")
            record = {"source_path": rel, "source_sha256": item["sha256"],
                      "meta_sha256": sha(meta), "source_guid": match.group(1).lower(),
                      "probe_path": "Assets/Source/" + rel[len(PREFIX):]}
            if rel in files:
                need(files[rel] == record, f"source alias drift: {rel}")
            files[rel] = record
            role_map[item["role"]] = record["probe_path"]
        need("prefab" in role_map and "material" in role_map, f"row inputs incomplete: {row['source_key']}")
        rows_out.append({"source_key": row["source_key"], "category": row["category"],
                         "model_id": row["model_id"], "renderer_hint": row["source_renderer_path"],
                         "source_queue": row["source_queue"], "source_material": role_map["material"],
                         "source_prefab": role_map["prefab"], "roles": [
                             {"role": role, "probe_path": path} for role, path in sorted(role_map.items())],
                         "known_file_ids": [{"name": k, "decimal": str(v)} for k, v in
                                            sorted(row["unity_file_ids_exact"].items())],
                         "shader_kind": "builtin" if row["source_key"].endswith("wall_shadow") else "custom"})
    need(len({f["source_guid"] for f in files.values()}) == len(files), "source GUID collision")
    settings = ROOT / "ref/client-unity/ProjectSettings/ProjectSettings.asset"
    need(settings.is_file() and re.search(r"(?m)^  m_ActiveColorSpace: 0$", settings.read_text(encoding="utf-8-sig")),
         "source color space is not pinned Gamma")
    return {"schema": CONTRACT_SCHEMA, "repo_root": ROOT.as_posix(), "probe_root": PROBE.as_posix(),
            "unity_version": "2019.4.28f1c1", "source_color_space": "Gamma",
            "closure_sha256": CLOSURE_SHA, "exporter_sha256": sha(EXPORTER), "runner_sha256": sha(RUNNER),
            "source_settings_sha256": sha(settings), "source_files": [files[k] for k in sorted(files)],
            "rows": rows_out, "scope": "isolated imported prefab/material defaults only; no GameApp globals, PropertyBlocks, dynamic sorting, runtime visual acceptance"}


def prepare() -> None:
    contract = make_contract()
    path = PROBE / "source_contract.json"
    runs = PROBE / "OutputRuns"
    existing = runs.is_dir() and any(runs.iterdir())
    if path.exists():
        need(read(path) == contract or not existing, "probe contract changed after native run")
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or read(path) != contract:
        path.write_text(json.dumps(contract, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for item in contract["source_files"]:
        src = ROOT / item["source_path"]
        dst = PROBE / item["probe_path"]
        dst.parent.mkdir(parents=True, exist_ok=True)
        for a, b, digest in ((src, dst, item["source_sha256"]),
                             (Path(str(src) + ".meta"), Path(str(dst) + ".meta"), item["meta_sha256"])):
            if b.exists():
                need(sha(b) == digest, f"existing probe source differs: {b}")
            else:
                shutil.copyfile(a, b)
                need(sha(b) == digest, f"copy hash differs: {b}")
    editor = PROBE / "Assets/Editor/ExportRokCityGammaRemainingFamiliesNative.cs"
    editor.parent.mkdir(parents=True, exist_ok=True)
    if editor.exists():
        need(sha(editor) == contract["exporter_sha256"] or not existing, "probe exporter changed after native run")
    if not editor.exists() or sha(editor) != contract["exporter_sha256"]:
        shutil.copyfile(EXPORTER, editor)
    configs = ((PROBE / "Packages/manifest.json", '{"dependencies":{"com.unity.modules.imageconversion":"1.0.0","com.unity.modules.jsonserialize":"1.0.0","com.unity.modules.ui":"1.0.0"}}\n'),
               (PROBE / "ProjectSettings/ProjectVersion.txt", "m_EditorVersion: 2019.4.28f1c1\n"))
    for cfg, content in configs:
        cfg.parent.mkdir(parents=True, exist_ok=True)
        if cfg.exists():
            need(cfg.read_text(encoding="utf-8-sig") == content, f"project setting differs: {cfg}")
        else:
            cfg.write_text(content, encoding="utf-8")
    print(f"PREPARED {PROBE}: five rows, {len(contract['source_files'])} source assets and .meta; Unity not started")


def png_dimensions(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:24]
    need(header[:8] == b"\x89PNG\r\n\x1a\n" and header[12:16] == b"IHDR", f"bad PNG: {path}")
    return struct.unpack(">II", header[16:24])


def verify(run: Path) -> dict:
    run = run.resolve()
    need(run.is_dir() and run.parent == (PROBE / "OutputRuns").resolve(), "run outside isolated OutputRuns")
    contract_path = PROBE / "source_contract.json"
    contract = read(contract_path)
    need(contract == make_contract(), "contract/source/exporter/runner drift")
    report = read(run / "native_families.json")
    need(report.get("schema") == NATIVE_SCHEMA and report.get("complete") is True, "native export incomplete")
    need(report.get("source_contract_sha256") == sha(contract_path) and
         report.get("exporter_sha256") == contract["exporter_sha256"], "native contract/exporter mismatch")
    need(report.get("unity_version") == contract["unity_version"] and
         report.get("color_space") == "Gamma" and report.get("game_app_loaded") is False and
         report.get("scope") == "isolated_prefab_material_defaults_only", "native execution scope differs")
    out_rows = report.get("rows", [])
    need(len(out_rows) == 5 and [r.get("source_key") for r in out_rows] == list(KEYS), "native five-row order differs")
    inputs = {r["source_key"]: r for r in contract["rows"]}
    copied = {f["probe_path"]: f for f in contract["source_files"]}
    texture_files = 0
    for row in out_rows:
        inp = inputs[row["source_key"]]
        need(row.get("model_id") == inp["model_id"] and row.get("renderer_hint") == inp["renderer_hint"] and
             row.get("source_material") == inp["source_material"] and
             row.get("source_prefab") == inp["source_prefab"] and row.get("prefab_default_only") is True,
             f"row identity differs: {row['source_key']}")
        need(row.get("source_material_queue") == inp["source_queue"], f"queue differs: {row['source_key']}")
        need(row.get("prefab_guid") == copied[inp["source_prefab"]]["source_guid"] and
             row.get("source_material_guid") == copied[inp["source_material"]]["source_guid"],
             f"native prefab/material GUID differs: {row['source_key']}")
        roles = {item["role"]: item["probe_path"] for item in inp["roles"]}
        if inp["shader_kind"] == "custom":
            need(row.get("shader_source_scope") == "copied_custom_shader" and
                 row.get("shader_asset_path") == roles["shader"],
                 f"custom shader binding differs: {row['source_key']}")
        mesh = row.get("mesh") or {}
        need(len(mesh.get("vertices", [])) > 0 and len(mesh.get("uv_channels", [])) == 8 and
             len(mesh.get("submeshes", [])) > 0, f"mesh export incomplete: {row['source_key']}")
        need(all(len(s.get("indices", [])) > 0 and len(s["indices"]) % 3 == 0 for s in mesh["submeshes"]),
             f"mesh triangles incomplete: {row['source_key']}")
        if row["source_key"] != "initial:5":
            need(len(mesh.get("normals", [])) == len(mesh["vertices"]), f"mesh normals missing: {row['source_key']}")
        else:
            need(mesh.get("normals_scope") == "not_exposed_by_Unity2019_Sprite_vertices_api" and
                 len(mesh.get("normals", [])) == 0, "sprite normals falsely asserted")
        if "source_fbx" in roles:
            need(mesh.get("asset_path") == roles["source_fbx"], f"source FBX binding differs: {row['source_key']}")
        known = {f["name"]: f["decimal"] for f in inp["known_file_ids"]}
        if "mesh_renderer_file_id" in known:
            need(row.get("renderer_file_id_decimal") == known["mesh_renderer_file_id"] and
                 mesh.get("local_file_id_decimal") == known["mesh_file_id"],
                 f"pinned Unity file ID differs: {row['source_key']}")
        need(len(row.get("material_slots", [])) > 0 and len(row.get("declared_properties", [])) > 0,
             f"material slots/properties missing: {row['source_key']}")
        if row["source_key"] != "initial:5":
            need(row.get("source_material_bound_in_prefab") is True and
                 inp["source_material"] in [s.get("asset_path") for s in row["material_slots"]],
                 f"source material not bound to default prefab renderer: {row['source_key']}")
        else:
            need(row.get("source_material_binding_scope") ==
                 "material asset inspected separately from prefab renderer slots",
                 "night material/default prefab binding scope lost")
        need(len({p["name"] for p in row["declared_properties"]}) == len(row["declared_properties"]) and
             row.get("source_material_pass_count") == len(row.get("source_material_pass_names", [])),
             f"shader properties/pass enumeration incomplete: {row['source_key']}")
        sampler_names = {p["name"] for p in row["declared_properties"] if p["kind"] == "TexEnv"}
        reported_samplers = {t["role"].removeprefix("declared_sampler:") for t in row.get("textures", [])
                             if t["role"].startswith("declared_sampler:")}
        need(sampler_names == reported_samplers, f"declared sampler export incomplete: {row['source_key']}")
        need(row.get("active_blend_state") == "unknown_no_public_pass_state_api" and
             row.get("active_zwrite_state") == "unknown_no_public_pass_state_api", "active pass state falsely asserted")
        if inp["shader_kind"] == "builtin":
            need(row.get("shader_source_scope") == "builtin_unavailable" and
                 row.get("active_pass_selection") == "unknown_builtin_shader", "builtin shader overclaimed")
        for tex in row.get("textures", []):
            if not tex.get("gpu_png"):
                need(tex.get("binding_scope") == "unbound_declared_sampler" and
                     tex.get("default_texture_scope") in ("unknown_api_unavailable", "declared_shader_default_only"),
                     "unbound/default sampler falsely asserted")
                continue
            path = run / tex["gpu_png"]
            need(path.is_file() and path.name == tex["gpu_png"] and sha(path) == tex.get("gpu_png_sha256"),
                 f"GPU PNG hash differs: {path}")
            need(png_dimensions(path) == (tex["width"], tex["height"]) and tex["width"] > 0 and tex["height"] > 0,
                 f"GPU PNG size differs: {path}")
            texture_files += 1
        role_textures = {t["role"].removeprefix("source_role:") for t in row["textures"]
                         if t["role"].startswith("source_role:") and t.get("gpu_png")}
        needed_role_textures = {role for role in roles if role in
                                ("raw_sprite", "raw_mask", "raw_texture", "_Splat", "_Tex0", "_Tex1", "_Tex2", "_Tex3")}
        need(role_textures == needed_role_textures, f"source-role GPU dumps incomplete: {row['source_key']}")
    result = {"schema": "rok.city_gamma_remaining_families_native_verify.v1", "passed": True,
              "scope": "five isolated Unity 2019 Gamma prefab/material default input rows; not GameApp runtime acceptance",
              "run": str(run), "source_contract_sha256": sha(contract_path),
              "native_sha256": sha(run / "native_families.json"), "rows": list(KEYS), "gpu_texture_files_checked": texture_files,
              "unknowns_preserved": ["GameApp globals", "PropertyBlocks", "dynamic sorting", "active blend/ZWrite", "builtin active pass"]}
    destination = run / "native_verify.json"
    need(not destination.exists(), f"verification evidence exists: {destination}")
    destination.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"PASS five remaining families; {texture_files} GPU texture files: {run}")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--prepare", action="store_true")
    group.add_argument("--run", type=Path)
    args = parser.parse_args()
    if args.prepare:
        prepare()
    else:
        verify(args.run)
