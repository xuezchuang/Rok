"""Prepare 32 isolated Unity source-local geometry/UV diagnostic draws."""

from __future__ import annotations

import hashlib
import json
import math
import os
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCitySourceGeometryProbeV1"
SEALED_SETTINGS = ROOT / "Saved/UnityCityGammaFamilyStateProbeV2"
GEOMETRY = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json"
GEOMETRY_SHA = "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273"
SHADER = PROBE / "Assets/ProbeLocalUv.shader"
EXPORTER = PROBE / "Assets/Editor/ExportRokUnityCitySourceGeometryProbe.cs"
CONTRACT = PROBE / "geometry_probe_contract.json"


def need(condition: bool, reason: str) -> None:
    if not condition:
        raise AssertionError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative(path: Path) -> str:
    resolved = path.resolve()
    need(resolved.is_relative_to(ROOT), f"Path escapes repository: {path}")
    return resolved.relative_to(ROOT).as_posix()


def place(path: Path, content: bytes) -> None:
    need(path.resolve().is_relative_to(PROBE.resolve()), f"Isolated output escapes project: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        need(path.read_bytes() == content, f"Existing isolated input differs: {path}")
    else:
        with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as stream:
            stream.write(content)


def f32(value: float) -> float:
    need(math.isfinite(value), "Nonfinite local matrix component")
    return struct.unpack("<f", struct.pack("<f", value))[0]


def copy_sealed(folder: str, count: int) -> tuple[list[dict], str]:
    files = sorted(file for file in (SEALED_SETTINGS / folder).iterdir() if file.is_file())
    need(len(files) == count, f"Sealed Unity 2019 {folder} count differs")
    result = []
    for source in files:
        output = PROBE / folder / source.name
        place(output, source.read_bytes())
        result.append({"source_path": relative(source), "source_sha256": sha(source),
                       "probe_path": relative(output)})
    digest = hashlib.sha256(json.dumps(result, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return result, digest


def copy_binary(spec: dict) -> dict:
    source = ROOT / spec["path"]
    need(source.resolve().is_relative_to(GEOMETRY.parent.resolve()), "Geometry source outside sealed v1")
    need(sha(source) == spec["sha256"] and source.stat().st_size == spec["byte_count"]
         == spec["count"] * spec["stride_bytes"], f"Geometry binary differs: {source}")
    output = PROBE / "ProbeInputs/Geometry" / source.name
    place(output, source.read_bytes())
    return {"source_path": relative(source), "source_sha256": spec["sha256"],
            "probe_path": relative(output), "count": spec["count"],
            "stride_bytes": spec["stride_bytes"], "byte_count": spec["byte_count"]}


def case_matrix(positions: dict, contents: bytes) -> tuple[bytes, dict]:
    components = positions["components"]
    need(components in (["x", "y"], ["x", "y", "z"]), "Position source components differ")
    dims = len(components)
    need(len(contents) == positions["count"] * dims * 4, "Position source byte count differs")
    rows = [struct.unpack_from("<" + "f" * dims, contents, row * dims * 4)
            for row in range(positions["count"])]
    ranges = [(min(row[axis] for row in rows), max(row[axis] for row in rows)) for axis in range(dims)]
    need([list(pair) for pair in ranges] == positions["component_ranges"],
         "Binary source ranges do not match geometry manifest")
    axes = [axis for axis, (low, high) in enumerate(ranges) if high - low > 1e-5]
    need(len(axes) == 2 and all(high - low <= 1e-5 for axis, (low, high) in enumerate(ranges)
                                if axis not in axes), "Source geometry is not a 2-axis local plane")
    a, b = axes
    ca = f32((ranges[a][0] + ranges[a][1]) * 0.5)
    cb = f32((ranges[b][0] + ranges[b][1]) * 0.5)
    scale = f32(1.8 / max(ranges[a][1] - ranges[a][0], ranges[b][1] - ranges[b][0]))
    matrix = [[0.0] * 4 for _ in range(4)]
    matrix[a][0] = scale
    matrix[b][1] = scale
    matrix[3][0] = f32(-ca * scale)
    matrix[3][1] = f32(-cb * scale)
    matrix[3][2] = 0.5
    matrix[3][3] = 1.0
    values = [f32(matrix[row][col]) for row in range(4) for col in range(4)]
    raw = struct.pack("<16f", *values)
    return raw, {"axis_a": a, "axis_b": b, "axis_a_name": components[a],
                 "axis_b_name": components[b], "axis_a_center": ca, "axis_b_center": cb,
                 "uniform_scale": scale, "source_axis_ranges": [list(ranges[a]), list(ranges[b])],
                 "remaining_axes_max_span": max([ranges[i][1] - ranges[i][0]
                                                 for i in range(dims) if i not in axes] or [0.0]),
                 "matrix_values_row_major": values}


def main() -> None:
    need(sha(GEOMETRY) == GEOMETRY_SHA, "Sealed source geometry manifest changed")
    need(EXPORTER.is_file() and SHADER.is_file(), "Isolated C# exporter/shader absent")
    geometry = json.loads(GEOMETRY.read_text(encoding="utf-8"))
    need(geometry["counts"]["rows"] == 32 and len(geometry["rows"]) == 32,
         "Sealed 32 source rows differ")
    settings, settings_digest = copy_sealed("ProjectSettings", 19)
    packages, packages_digest = copy_sealed("Packages", 2)
    need((PROBE / "ProjectSettings/ProjectVersion.txt").read_text(encoding="utf-8").splitlines()[0]
         == "m_EditorVersion: 2019.4.28f1c1", "Sealed Unity version differs")
    copied: dict[str, dict] = {}
    cases = []
    matrix_files = []
    for index, row in enumerate(geometry["rows"]):
        need(row["index"] == index and len(row["indices"]["submeshes"]) == 1,
             f"Source geometry row/submesh differs: {index}")
        uv0 = next((item["binary"] for item in row["uv_channels"] if item["channel"] == 0), None)
        need(uv0 is not None, f"Source UV0 absent: {index}")
        color = row["colors"]["binary"]
        bindings = {"positions": row["positions"], "uv0": uv0,
                    "indices": row["indices"]["binary"]}
        if color is not None:
            bindings["colors"] = color
        item = {"index": index, "source_key": row["source_key"],
                "source_group": row["source_group"], "source_row_sha256": row["source_row_sha256"],
                "position_components": len(row["positions"]["components"]),
                "uv_components": len(uv0["components"]), "colors": None,
                "has_source_colors": color is not None}
        source_binding = {key: row[key] for key in ("flips", "root_trs", "renderer_local_trs",
                                                      "prefab_renderer_local_to_world", "source_queue",
                                                      "source_material", "source_shader",
                                                      "source_material_state", "texture_slots")}
        item["source_binding"] = source_binding
        item["source_binding_sha256"] = hashlib.sha256(json.dumps(
            source_binding, sort_keys=True, separators=(",", ":"), ensure_ascii=False
        ).encode("utf-8")).hexdigest()
        for name, spec in bindings.items():
            if spec["path"] not in copied:
                copied[spec["path"]] = copy_binary(spec)
            else:
                need(copied[spec["path"]]["source_sha256"] == spec["sha256"],
                     "Repeated geometry source path changed")
            item[name] = {"path": "ProbeInputs/Geometry/" + Path(spec["path"]).name,
                          "sha256": spec["sha256"], "count": spec["count"],
                          "stride_bytes": spec["stride_bytes"], "byte_count": spec["byte_count"]}
        raw, recipe = case_matrix(row["positions"], (ROOT / row["positions"]["path"]).read_bytes())
        matrix_path = PROBE / f"ProbeInputs/Matrices/row_{index:02d}_local_to_clip_f32le.bin"
        place(matrix_path, raw)
        matrix_files.append({"probe_path": relative(matrix_path), "sha256": sha(matrix_path),
                             "count": 16, "stride_bytes": 4, "byte_count": 64})
        item.update({key: recipe[key] for key in ("axis_a", "axis_b", "axis_a_name", "axis_b_name",
                                                     "axis_a_center", "axis_b_center", "uniform_scale")})
        item["matrix_recipe"] = recipe
        item["matrix"] = {"path": relative(matrix_path).removeprefix(relative(PROBE) + "/"),
                          "sha256": sha(matrix_path), "count": 16,
                          "stride_bytes": 4, "byte_count": 64}
        cases.append(item)
    need(len(copied) == 97 and len(matrix_files) == 32,
         "Exactly 97 source typed binaries and 32 matrices required")
    contract = {"schema": "rok.unity_city_source_geometry_probe_contract.v1",
                "repo_root": ROOT.as_posix(), "probe_root": PROBE.as_posix(),
                "unity_version": "2019.4.28f1c1", "geometry_manifest_sha256": GEOMETRY_SHA,
                "producer_sha256": sha(Path(__file__)), "exporter_sha256": sha(EXPORTER),
                "shader_sha256": sha(SHADER), "settings_source": relative(SEALED_SETTINGS),
                "settings_files": settings, "packages_files": packages,
                "settings_digest_sha256": settings_digest, "packages_digest_sha256": packages_digest,
                "binary_files": [copied[key] for key in sorted(copied)], "matrix_files": matrix_files,
                "matrix_layout": "16 IEEE754 float32LE values, row-major m00..m33; UE FMatrix44f.M[row][col] same bytes",
                "shader_matrix_expression": "mul(input.vertex, _ProbeLocalToClip); Unity SetMatrix[row,col] assigned from same 64B",
                "readback_row_contract": "Unity ReadPixels RGBA32 raw bottom-left first; one reversal writes canonical top-left raw; PNG decode must equal raw",
                "source_scope": "isolated source-local mesh/index/UV GPU diagnostic only; no source material/mips, GameApp world/LOD, main view or alpha proof",
                "target": {"width": 128, "height": 128, "format": "ARGB32",
                           "read_write": "Linear", "anti_aliasing": 1, "mips": False,
                           "clear_rgba": [0.07, 0.15, 0.71, 1.0],
                           "vertex_expression": "mul(input.vertex, _ProbeLocalToClip)",
                           "fragment_expression": "float4(UV.x,UV.y,0.25,1)",
                           "render_state": "CullOff/ZTestAlways/ZWriteOff/BlendOff"},
                "cases": cases}
    encoded = (json.dumps(contract, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    place(CONTRACT, encoded)
    (PROBE / "OutputRuns").mkdir(parents=True, exist_ok=True)
    print(json.dumps({"contract": relative(CONTRACT), "sha256": sha(CONTRACT),
                      "cases": 32, "source_binaries": len(copied),
                      "matrix_files": len(matrix_files), "unity_started": False}, ensure_ascii=False))


if __name__ == "__main__":
    main()
