"""Prepare or verify a strictly isolated Unity 2019 masked-city sprite probe.

Preparation copies only source assets and their .meta files. Verification treats
native output as data and never interprets it as GameApp draw-order evidence.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import struct
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityGammaMaskedSpritesProbeV2"
CLOSURE = ROOT / "Saved/RokUnityMigration/CityGammaSourceClosure/20261008T221227Z_39568_2aac39ec/catalog.json"
CLOSURE_SHA256 = "6349487fdd83e0bf3a0f93aaab4b41643026dec5d253185a9cd36a7fd9d0d49c"
EXPORTER = ROOT / "Scripts/ExportRokUnityCityGammaMaskedSpritesNative.cs"
RUNNER = ROOT / "Scripts/RunRokUnityCityGammaMaskedSpritesNative.ps1"
SCHEMA = "rok.city_gamma_masked_sprites_probe_contract.v1"
NATIVE_SCHEMA = "rok.city_gamma_masked_sprites_native.v1"
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
EXPECTED_CATEGORIES = {"initial_source_building": 23, "city_gate_flag": 2, "dynamic_local_new": 2}
SOURCE_PREFIX = "ref/client-unity/Assets/"


def need(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path: Path) -> dict:
    need(path.is_file(), f"missing: {path}")
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    need(isinstance(value, dict), f"invalid JSON root: {path}")
    return value


def normalized_source(value: str) -> str:
    p = value.replace("\\", "/")
    if p.startswith("Assets/"):
        p = "ref/client-unity/" + p
    need(p.startswith(SOURCE_PREFIX) and ".." not in Path(p).parts, f"unsafe source path: {value}")
    return p


def selected_rows() -> list[dict]:
    need(sha(CLOSURE) == CLOSURE_SHA256, "sealed 32-row closure changed")
    catalog = read(CLOSURE)
    need(catalog["schema"] == "rok.city_gamma.source_closure_catalog.v1" and
         catalog["ready_for_raster"] is False and len(catalog["rows"]) == 32,
         "closure identity/readiness differs")
    rows = [row for row in catalog["rows"] if row["source_semantics"].get("shader") ==
            "custom/map/map_building_with_mask-5" or row["source_semantics"].get("shader_name") ==
            "custom/map/map_building_with_mask-5"]
    need(len(rows) == 27 and Counter(row["category"] for row in rows) == EXPECTED_CATEGORIES,
         "the 27-row shader-family allowlist differs")
    need(len({row["source_key"] for row in rows}) == 27, "duplicate source row")
    return rows


def source_files(rows: list[dict]) -> list[dict]:
    expected: dict[str, str] = {}
    for row in rows:
        roles = {item["role"]: item for item in row["source_files"]}
        need(all(role in roles for role in ("prefab", "raw_sprite", "raw_mask", "material", "shader")),
             f"incomplete row source files: {row['source_key']}")
        for role in ("prefab", "raw_sprite", "raw_mask", "material", "shader"):
            item = roles[role]
            rel = normalized_source(item["source_path"])
            source = ROOT / rel
            need(item["exists"] and source.is_file() and sha(source) == item["sha256"],
                 f"closure source changed: {rel}")
            if rel in expected:
                need(expected[rel] == item["sha256"], f"source alias hash conflict: {rel}")
            expected[rel] = item["sha256"]
    files = []
    for relative, digest in sorted(expected.items()):
        source = ROOT / relative
        meta = Path(str(source) + ".meta")
        need(meta.is_file(), f"Unity metadata missing: {relative}")
        meta_text = meta.read_text(encoding="utf-8-sig")
        guid = re.search(r"(?m)^guid:\s*([0-9a-fA-F]{32})\s*$", meta_text)
        need(guid is not None, f"Unity GUID missing: {relative}")
        probe_rel = "Assets/Source/" + relative[len(SOURCE_PREFIX):]
        files.append({"source_path": relative, "source_sha256": digest,
                      "meta_sha256": sha(meta), "source_guid": guid.group(1).lower(),
                      "probe_path": probe_rel})
    need(len({f["source_guid"] for f in files}) == len(files), "copied source GUID collision")
    return files


def row_contract(rows: list[dict], files: list[dict]) -> list[dict]:
    lookup = {f["source_path"]: f for f in files}
    result = []
    for row in rows:
        roles = {item["role"]: item for item in row["source_files"]}
        paths = {role: lookup[normalized_source(roles[role]["source_path"])]["probe_path"]
                 for role in ("prefab", "raw_sprite", "raw_mask", "material", "shader")}
        guids = {role: lookup[normalized_source(roles[role]["source_path"])]["source_guid"]
                 for role in paths}
        result.append({"source_key": row["source_key"], "category": row["category"],
                       "model_id": row["model_id"], "source_queue": row["source_queue"],
                       "expected_source_vertices": row["source_geometry"]["vertices"],
                       "expected_source_triangles": row["source_geometry"]["triangles"],
                       "source_sorting_layer": row["source_sorting_layer"],
                       "source_sorting_order": row["source_sorting_order"],
                       "renderer_hint": row["source_renderer_path"],
                       "prefab": paths["prefab"], "sprite": paths["raw_sprite"],
                       "mask": paths["raw_mask"], "material": paths["material"],
                       "shader": paths["shader"], "guids": guids,
                       "known_file_ids": {key: str(value) for key, value in
                                          row["unity_file_ids_exact"].items()},
                       "binding_limit": "Prefab source renderer selected by sprite.texture + material + leaf name; raw mask path comes from closure and prefab/default PropertyBlock may not bind it."})
    return result


def prepare() -> None:
    rows = selected_rows()
    files = source_files(rows)
    need(EXPORTER.is_file() and RUNNER.is_file(), "exporter/runner missing")
    settings = ROOT / "ref/client-unity/ProjectSettings/ProjectSettings.asset"
    need(settings.is_file() and re.search(r"(?m)^  m_ActiveColorSpace: 0$",
                                          settings.read_text(encoding="utf-8-sig")),
         "source Unity project is not Gamma")
    contract = {"schema": SCHEMA, "repo_root": ROOT.as_posix(), "probe_root": PROBE.as_posix(),
                "unity_version": "2019.4.28f1c1", "source_color_space": "Gamma",
                "closure_sha256": CLOSURE_SHA256, "exporter_sha256": sha(EXPORTER),
                "runner_sha256": sha(RUNNER), "source_settings_sha256": sha(settings),
                "source_files": files, "rows": row_contract(rows, files),
                "scope": "Isolated prefab/default source assets only. No GameApp runtime PropertyBlock, equal-queue draw order, or visual acceptance."}
    manifest_path = PROBE / "source_contract.json"
    has_native_runs = (PROBE / "OutputRuns").exists() and any((PROBE / "OutputRuns").iterdir())
    if manifest_path.exists():
        if read(manifest_path) != contract:
            need(not has_native_runs,
                 "existing isolated contract differs after native capture; preserve pinned probe")
            manifest_path.write_text(json.dumps(contract, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    else:
        manifest_path.parent.mkdir(parents=True, exist_ok=True)
        manifest_path.write_text(json.dumps(contract, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for item in files:
        source = ROOT / item["source_path"]
        target = PROBE / item["probe_path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        for current, copy, digest in ((source, target, item["source_sha256"]),
                                      (Path(str(source) + ".meta"), Path(str(target) + ".meta"),
                                       item["meta_sha256"])):
            if copy.exists():
                need(sha(copy) == digest, f"isolated copy differs: {copy}")
            else:
                shutil.copyfile(current, copy)
                need(sha(copy) == digest, f"isolated copy hash differs: {copy}")
    editor = PROBE / "Assets/Editor/ExportRokUnityCityGammaMaskedSpritesNative.cs"
    editor.parent.mkdir(parents=True, exist_ok=True)
    if editor.exists():
        if sha(editor) != contract["exporter_sha256"]:
            need(not has_native_runs, "isolated exporter differs after native capture")
            shutil.copyfile(EXPORTER, editor)
    else:
        shutil.copyfile(EXPORTER, editor)
    packages = PROBE / "Packages/manifest.json"
    version = PROBE / "ProjectSettings/ProjectVersion.txt"
    for path, text in ((packages, '{"dependencies":{"com.unity.modules.imageconversion":"1.0.0","com.unity.modules.jsonserialize":"1.0.0"}}\n'),
                       (version, "m_EditorVersion: 2019.4.28f1c1\n")):
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.exists():
            need(path.read_text(encoding="utf-8-sig").startswith(text.strip()), f"isolated project config differs: {path}")
        else:
            path.write_text(text, encoding="utf-8")
    print(f"PREPARED {PROBE}: {len(rows)} allowlisted rows, {len(files)} copied source assets + metadata; Unity not started")


def verify(run: Path) -> None:
    run = run.resolve()
    need(run.is_dir() and run.parent == (PROBE / "OutputRuns").resolve(),
         "run is outside the isolated probe")
    rows = selected_rows()
    contract_path = PROBE / "source_contract.json"
    contract = read(contract_path)
    need(contract["schema"] == SCHEMA and contract["closure_sha256"] == CLOSURE_SHA256 and
         contract["unity_version"] == "2019.4.28f1c1" and contract["source_color_space"] == "Gamma" and
         Path(contract["repo_root"]).resolve() == ROOT.resolve() and
         Path(contract["probe_root"]).resolve() == PROBE.resolve() and
         contract["exporter_sha256"] == sha(EXPORTER) ==
         sha(PROBE / "Assets/Editor/ExportRokUnityCityGammaMaskedSpritesNative.cs") and
         contract["runner_sha256"] == sha(RUNNER), "probe/tool identity differs")
    need(len(contract["rows"]) == 27 and {r["source_key"] for r in contract["rows"]} ==
         {r["source_key"] for r in rows}, "probe allowlist differs")
    for item in contract["source_files"]:
        source, copy = ROOT / item["source_path"], PROBE / item["probe_path"]
        need(sha(source) == sha(copy) == item["source_sha256"] and
             sha(Path(str(source) + ".meta")) == sha(Path(str(copy) + ".meta")) == item["meta_sha256"],
             f"source/copy hash differs: {item['source_path']}")
    report = read(run / "native_masked_sprites.json")
    need(not (run / "failure.txt").exists() and report["schema"] == NATIVE_SCHEMA and
         report["complete"] is True and report["unity_version"] == "2019.4.28f1c1" and
         report["color_space"] == "Gamma" and report["game_app_loaded"] is False and
         report["runtime_property_blocks_observed"] is False and
         report["actual_equal_queue_order_observed"] is False and
         Path(report["run_directory"]).resolve() == run and
         report["source_contract_sha256"] == sha(contract_path) and
         report["exporter_sha256"] == sha(EXPORTER), "native identity/scope differs")
    exports = report["rows"]
    need(len(exports) == 27 and {row["source_key"] for row in exports} ==
         {row["source_key"] for row in contract["rows"]}, "native row identity differs")
    by_key = {row["source_key"]: row for row in contract["rows"]}
    for row in exports:
        source = by_key[row["source_key"]]
        need(row["category"] == source["category"] and row["model_id"] == source["model_id"] and
             row["prefab_path"] == source["prefab"] and row["renderer_sprite_path"] == source["sprite"] and
             row["mask_input_path"] == source["mask"] and row["material_path"] == source["material"] and
             row["shader_path"] == source["shader"] and row["shader_name"] ==
             "custom/map/map_building_with_mask-5" and row["prefab_default_only"] is True and
             row["source_renderer_hint"] == source["renderer_hint"] and
             row["renderer_path"].split("/")[-1] == source["renderer_hint"].split("/")[-1],
             f"native source binding differs: {row['source_key']}")
        need(all(row[role + "_guid"] == source["guids"][source_role] for role, source_role in
                 (("prefab", "prefab"), ("sprite", "raw_sprite"), ("mask", "raw_mask"),
                  ("material", "material"), ("shader", "shader"))),
             f"native AssetDatabase GUID differs: {row['source_key']}")
        need(row["source_queue"] == 2995 and row["material_render_queue"] == 2995 and
             row["sorting_layer_id"] == 0 and row["sorting_order"] == 0,
             f"native queue/sorting differs: {row['source_key']}")
        need(len(row["sprite_vertices"]) == source["expected_source_vertices"] and
             len(row["sprite_triangles"]) // 3 == source["expected_source_triangles"] and
             len(row["sprite_vertices"]) == len(row["sprite_uv"]) and
             len(row["sprite_triangles"]) >= 3 and len(row["sprite_triangles"]) % 3 == 0 and
             max(row["sprite_triangles"]) < len(row["sprite_vertices"]),
             f"native sprite topology invalid: {row['source_key']}")
        for kind in ("sprite", "mask"):
            name = row[kind + "_gpu_png"]
            path = (run / name).resolve()
            need(path.parent == run and path.is_file() and path.read_bytes()[:8] == PNG_MAGIC and
                 sha(path) == row[kind + "_gpu_png_sha256"],
                 f"native GPU texture PNG invalid: {row['source_key']}:{kind}")
            png = path.read_bytes()
            need(png[12:16] == b"IHDR" and
                 struct.unpack_from(">II", png, 16) ==
                 (row[kind + "_texture_width"], row[kind + "_texture_height"]),
                 f"native GPU PNG dimensions differ: {row['source_key']}:{kind}")
        need(row["renderer_file_id_decimal"] and re.fullmatch(r"[0-9]+", row["renderer_file_id_decimal"]),
             f"renderer fileId missing/lossy: {row['source_key']}")
        if source["category"] == "dynamic_local_new" and source["known_file_ids"].get("sprite_renderer_file_id"):
            need(row["renderer_file_id_decimal"] == source["known_file_ids"]["sprite_renderer_file_id"],
                 f"prefab renderer fileId differs: {row['source_key']}")
        need(row["mask_binding_scope"] != "runtime_verified" and row["property_block_scope"] ==
             "prefab_default_only", "runtime property block falsely claimed")
    print(f"PASS: 27 isolated prefab/default native rows and GPU texture PNG pairs; run={run}")


def main() -> None:
    parser = argparse.ArgumentParser()
    choice = parser.add_mutually_exclusive_group(required=True)
    choice.add_argument("--prepare", action="store_true")
    choice.add_argument("--run", type=Path)
    args = parser.parse_args()
    if args.prepare:
        prepare()
    else:
        verify(args.run)


if __name__ == "__main__":
    main()
