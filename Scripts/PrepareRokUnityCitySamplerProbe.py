"""Prepare an isolated Unity 2019 importer observation for 22 masked city textures."""

from __future__ import annotations

import hashlib
import json
import os
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCitySamplerProbeV1"
SOURCE_PROBE = ROOT / "Saved/UnityCityGammaFamilyStateProbeV2"
INVENTORY = ROOT / "Saved/RokUnityMigration/BatchReview20261009NightTimeAndExpedition/all32_source_rhi_admission_inventory.json"
INVENTORY_SHA = "fcbf6c325b8235df1a6852356fe5dc52a2c522963492da46fab8dd05c54dc543"
GEOMETRY = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json"
GEOMETRY_SHA = "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273"
EXPORTER = PROBE / "Assets/Editor/ExportRokUnityCitySamplerProbe.cs"
CONTRACT = PROBE / "sampler_contract.json"


def need(condition: bool, reason: str) -> None:
    if not condition:
        raise AssertionError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative(path: Path) -> str:
    resolved = path.resolve()
    need(resolved.is_relative_to(ROOT), f"Path escapes repository: {path}")
    return resolved.relative_to(ROOT).as_posix()


def place_bytes(path: Path, content: bytes) -> None:
    need(path.resolve().is_relative_to(PROBE.resolve()), f"Probe output escapes project: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        need(path.read_bytes() == content, f"Existing isolated probe input differs: {path}")
    else:
        with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as stream:
            stream.write(content)


def serialized(meta: str, name: str) -> int:
    match = re.search(rf"(?m)^\s+{re.escape(name)}:\s*(-?\d+)\s*$", meta)
    need(match is not None, f"Source meta lacks {name}")
    return int(match.group(1))


def source_textures(inventory: dict) -> list[dict]:
    result = []
    guids = set()
    for chain in inventory["texture_chains"]:
        if "source_sampler_meta" not in chain:
            continue
        original = chain["source_original"]
        meta = chain["source_sampler_meta"]
        src = ROOT / original["path"]
        src_meta = Path(str(src) + ".meta")
        need(relative(src_meta) == meta["path"] and src.suffix.lower() == ".png",
             f"Unexpected masked texture pair: {original['path']}")
        need(sha(src) == original["sha256"] and sha(src_meta) == meta["sha256"],
             f"Frozen source bytes differ: {original['path']}")
        meta_text = src_meta.read_text(encoding="utf-8-sig")
        guid = re.search(r"(?m)^guid:\s*([0-9a-f]{32})\s*$", meta_text)
        need(guid is not None and guid.group(1) == chain["source_guid"]
             and chain["source_guid"] not in guids, f"Source GUID differs/duplicates: {original['path']}")
        guids.add(chain["source_guid"])
        values = {"filter_mode": serialized(meta_text, "filterMode"),
                  "wrap_u": serialized(meta_text, "wrapU"),
                  "wrap_v": serialized(meta_text, "wrapV")}
        need(values == {"filter_mode": meta["serialized_filter_mode"],
                        "wrap_u": meta["serialized_wrap_u"], "wrap_v": meta["serialized_wrap_v"]},
             f"Source sampler serialization differs: {original['path']}")
        probe_path = "Assets/Source/" + original["path"].removeprefix("ref/client-unity/Assets/")
        need(probe_path != "Assets/Source/" + original["path"], "Source path not under client Unity Assets")
        place_bytes(PROBE / probe_path, src.read_bytes())
        place_bytes(PROBE / (probe_path + ".meta"), src_meta.read_bytes())
        result.append({"source_path": original["path"], "source_sha256": original["sha256"],
                       "meta_path": meta["path"], "meta_sha256": meta["sha256"],
                       "source_guid": chain["source_guid"], "probe_path": probe_path,
                       "serialized_filter_mode": values["filter_mode"],
                       "serialized_wrap_u": values["wrap_u"], "serialized_wrap_v": values["wrap_v"]})
    need(len(result) == len(guids) == 22
         and sum(x["serialized_filter_mode"] == -1 for x in result) == 12,
         "Expected 22 masked textures, including 12 default-filter metas")
    return result


def copy_project_inputs(folder: str) -> tuple[list[dict], str]:
    source = SOURCE_PROBE / folder
    files = sorted(path for path in source.iterdir() if path.is_file())
    need((len(files) == 19 if folder == "ProjectSettings" else len(files) == 2),
         f"Sealed Unity 2019 {folder} file count differs")
    records = []
    for src in files:
        destination = PROBE / folder / src.name
        place_bytes(destination, src.read_bytes())
        records.append({"source_path": relative(src), "probe_path": relative(destination), "sha256": sha(src)})
    digest = hashlib.sha256(json.dumps(records, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return records, digest


def main() -> None:
    need(sha(INVENTORY) == INVENTORY_SHA and sha(GEOMETRY) == GEOMETRY_SHA,
         "Pinned 32-row source inventory/geometry changed")
    need(EXPORTER.is_file(), "Isolated Unity exporter source absent")
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    need(inventory["counts"]["masked_unique_texture_chains"] == 22,
         "Frozen masked source count differs")
    settings, settings_digest = copy_project_inputs("ProjectSettings")
    packages, packages_digest = copy_project_inputs("Packages")
    need((PROBE / "ProjectSettings/ProjectVersion.txt").read_text(encoding="utf-8")
         .splitlines()[0] == "m_EditorVersion: 2019.4.28f1c1", "Sealed Unity editor version differs")
    textures = source_textures(inventory)
    contract = {
        "schema": "rok.unity_city_sampler_contract.v1",
        "qualification": "22 masked source PNG/meta imports in isolated Unity2019 editor; GameApp effective sampler remains separate",
        "repo_root": ROOT.as_posix(), "probe_root": PROBE.as_posix(),
        "unity_version": "2019.4.28f1c1", "inventory_sha256": INVENTORY_SHA,
        "geometry_manifest_sha256": GEOMETRY_SHA,
        "producer_sha256": sha(Path(__file__)), "exporter_sha256": sha(EXPORTER),
        "settings_source": relative(SOURCE_PROBE), "settings_files": settings,
        "packages_files": packages, "settings_digest_sha256": settings_digest,
        "packages_digest_sha256": packages_digest, "source_files": textures,
    }
    encoded = (json.dumps(contract, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    place_bytes(CONTRACT, encoded)
    (PROBE / "OutputRuns").mkdir(parents=True, exist_ok=True)
    print(json.dumps({"contract": relative(CONTRACT), "sha256": sha(CONTRACT),
                      "source_textures": len(textures), "default_filter_unknown": 12,
                      "project_settings": len(settings), "packages": len(packages)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
