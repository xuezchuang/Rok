"""Prepare two isolated Unity UV rasters from the accepted Hall Native3 world matrices."""
from __future__ import annotations

import hashlib
import json
import math
import os
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityHallWorldRasterProbeV1"
SEALED_SETTINGS = ROOT / "Saved/UnityCityGammaFamilyStateProbeV2"
GEOMETRY = ROOT / "Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json"
GEOMETRY_SHA = "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273"
NATIVE3 = ROOT / "Saved/UnityCityHallWorldProbeV1/OutputRuns/20261009_032009_843_25796"
NATIVE3_SHA = "01b924d764395708e20766834595fa7943f1ec61c70215b6058914c2cc80ab5e"
NATIVE3_VERIFY = ROOT / "Saved/RokUnityMigration/BatchReview20261009QuestChapterAndWorldHall/hall_native3_verified_v4.json"
SOURCE_SHADER = ROOT / "Saved/UnityCitySourceGeometryProbeV1/Assets/ProbeLocalUv.shader"
SHADER_SHA = "cd7dd4ebde11b9b62ee174a42b915de7881f536fc9a55dda e9ff053381bb4684".replace(" ", "")
SOURCE_PREFAB = ROOT / "ref/client-unity/Assets/BundleAssets/Scene/City_building/city_building_1/build_1_101_1.prefab"
SOURCE_PREFAB_SHA = "498032280f6bba9414bc61e7be9a3df61e7aa66358a663ba476d8032ac715c9a"
SOURCE_FAMILIES = ROOT / "Saved/UnityCityGammaRemainingFamiliesProbe/OutputRuns/20261008_230313_337_7496/native_families.json"
CONTRACT = PROBE / "hall_world_raster_contract_v4.json"
EXPORTER = PROBE / "Assets/Editor/RenderRokUnityCityHallWorldRasterNative.cs"
VERIFIER = ROOT / "Scripts/VerifyRokUnityCityHallWorldRasterProbe.py"
SHADER = PROBE / "Assets/ProbeLocalUv.shader"
REPORT = ROOT / "Saved/RokUnityMigration/BatchReview20261009QuestChapterAndWorldHall/hall_world_raster_native_implementation_revision4.json"
PHASES = ("prefab_default_hall", "runtime_replay_hall")


def need(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rel(path: Path) -> str:
    return path.resolve().relative_to(ROOT).as_posix()


def f32(value: float) -> float:
    need(math.isfinite(value), "Nonfinite matrix/fit value")
    return struct.unpack("<f", struct.pack("<f", value))[0]


def place(path: Path, content: bytes) -> None:
    need(path.resolve().is_relative_to(PROBE.resolve()), "Isolated output escapes project")
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        need(path.read_bytes() == content, f"Existing isolated input differs: {path}")
    else:
        with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644), "wb") as stream:
            stream.write(content)


def matrix(raw: bytes) -> list[list[float]]:
    need(len(raw) == 64, "World matrix must be 64B")
    vals = struct.unpack("<16f", raw)
    return [list(vals[4 * i:4 * i + 4]) for i in range(4)]


def pack_matrix(rows: list[list[float]]) -> bytes:
    return struct.pack("<16f", *(f32(value) for row in rows for value in row))


def mul(a: list[list[float]], b: list[list[float]]) -> list[list[float]]:
    return [[f32(sum(a[i][k] * b[k][j] for k in range(4))) for j in range(4)] for i in range(4)]


def transpose(a: list[list[float]]) -> list[list[float]]:
    return [[a[j][i] for j in range(4)] for i in range(4)]


def spec_copy(spec: dict, output: Path) -> dict:
    source = ROOT / spec["path"]
    need(sha(source) == spec["sha256"] and source.stat().st_size == spec["byte_count"] == spec["count"] * spec["stride_bytes"],
         f"Source typed binary differs: {source}")
    place(output, source.read_bytes())
    return {"source_path": rel(source), "probe_path": rel(output).removeprefix(rel(PROBE) + "/"),
            "sha256": spec["sha256"], "count": spec["count"],
            "stride_bytes": spec["stride_bytes"], "byte_count": spec["byte_count"]}


def main() -> None:
    need(sha(GEOMETRY) == GEOMETRY_SHA and sha(NATIVE3 / "hall_world_native.json") == NATIVE3_SHA,
         "Sealed geometry or Hall Native3 source changed")
    need(sha(SOURCE_SHADER) == SHADER_SHA and sha(SOURCE_PREFAB) == SOURCE_PREFAB_SHA,
         "Original shader/Hall prefab changed")
    need(EXPORTER.is_file() and VERIFIER.is_file(), "Isolated C# exporter or verifier absent")
    geometry = json.loads(GEOMETRY.read_text(encoding="utf-8"))
    row = geometry["rows"][27]
    source_family = json.loads(SOURCE_FAMILIES.read_text(encoding="utf-8-sig"))["rows"][0]
    need(row["index"] == 27 and row["source_key"] == source_family["source_key"] == "initial:5" and
         row["source_json_pointer"] == "/rows/0" and
         source_family["renderer_file_id_decimal"] == "212751466141171401" and
         source_family["prefab_guid"] == "c825e8897df8e424e817e4ed05eef53e" and
         "m_Sprite: {fileID: 21300000, guid: 57ec7ddf0b743a140b2707fe14b536e2, type: 3}" in SOURCE_PREFAB.read_text(encoding="utf-8"),
         "Row27 does not identify the exact Hall renderer and Sprite")
    uv = next((item["binary"] for item in row["uv_channels"] if item["channel"] == 0), None)
    need(uv is not None and uv["components"] == ["x", "y", "z", "w"] and
         row["positions"]["components"] == ["x", "y", "z"] and row["indices"]["submeshes"][0]["source_topology"] == "Triangles" and
         row["positions"]["count"] == uv["count"] == 240 and row["indices"]["binary"]["count"] == 714,
         "Hall full UV/geometry source shape differs")
    native = json.loads((NATIVE3 / "hall_world_native.json").read_text(encoding="utf-8-sig"))
    need(native["schema"] == "rok.unity_city_hall_world_native.v3" and len(native["nodes"]) == 38 and
         native["qualification"] == "transform_fixture_only" and not native["lod_qualified"],
         "Hall Native3 qualification differs")
    verify = json.loads(NATIVE3_VERIFY.read_text(encoding="utf-8-sig"))
    need(verify["passed"] is True and verify["native"]["native_sha256"] == NATIVE3_SHA,
         "Hall Native3 strict source verification absent")
    settings = []
    for folder, count in (("ProjectSettings", 19), ("Packages", 2)):
        files = sorted(item for item in (SEALED_SETTINGS / folder).iterdir() if item.is_file())
        need(len(files) == count, f"Sealed Unity {folder} differs")
        for source in files:
            dest = PROBE / folder / source.name
            place(dest, source.read_bytes())
            settings.append({"source_path": rel(source), "probe_path": rel(dest), "sha256": sha(source)})
    place(SHADER, SOURCE_SHADER.read_bytes())
    geom = {name: spec_copy(spec, PROBE / f"ProbeInputs/Geometry/{name}.bin") for name, spec in
            (("positions", row["positions"]), ("uv0", uv), ("indices", row["indices"]["binary"]))}
    positions = list(struct.iter_unpack("<3f", (ROOT / row["positions"]["path"]).read_bytes()))
    indices = list(struct.iter_unpack("<I", (ROOT / row["indices"]["binary"]["path"]).read_bytes()))
    need(len(positions) == 240 and all(i[0] < 240 for i in indices), "Hall source indices out of range")
    world_specs = []
    all_world_vertices = []
    for index, phase in enumerate(PHASES):
        matches = [node for node in native["nodes"] if node["phase"] == phase and
                   node["component_type"] == "SpriteRenderer" and node["path"] == "build_1_101_1/sprite[0]" and
                   node["source_guid"] == "c825e8897df8e424e817e4ed05eef53e" and
                   node["source_file_id"] == "212751466141171401"]
        need(len(matches) == 1, f"Exact Native3 Hall renderer missing: {phase}")
        source_matrix = NATIVE3 / matches[0]["matrix_path"]
        need(source_matrix.stat().st_size == 64 and sha(source_matrix) == matches[0]["matrix_sha256"],
             "Native3 world matrix source differs")
        dest = PROBE / f"ProbeInputs/Matrices/{phase}_world_column.f32le"
        place(dest, source_matrix.read_bytes())
        w = matrix(source_matrix.read_bytes())
        transformed = [tuple(f32(sum(w[axis][j] * vertex[j] for j in range(3)) + w[axis][3])
                             for axis in range(3)) for vertex in positions]
        all_world_vertices.extend(transformed)
        world_specs.append({"index": index, "phase": phase, "source_key": "initial:5",
                            "renderer_file_id": "212751466141171401", "renderer_prefab_guid": "c825e8897df8e424e817e4ed05eef53e",
                            "world_matrix": {"source_path": rel(source_matrix),
                                             "probe_path": rel(dest).removeprefix(rel(PROBE) + "/"),
                                             "sha256": sha(source_matrix), "count": 16,
                                             "stride_bytes": 4, "byte_count": 64},
                            "world_vertex_ranges": [[min(v[axis] for v in transformed), max(v[axis] for v in transformed)]
                                                    for axis in range(3)]})
    ranges = [[min(vertex[axis] for vertex in all_world_vertices), max(vertex[axis] for vertex in all_world_vertices)]
              for axis in range(3)]
    pairs = [(a, b) for a in range(3) for b in range(a + 1, 3)]
    a, b = max(pairs, key=lambda pair: ((ranges[pair[0]][1] - ranges[pair[0]][0]) *
                                        (ranges[pair[1]][1] - ranges[pair[1]][0]), -pair[0], -pair[1]))
    need((a, b) == (0, 2) and all(ranges[axis][1] - ranges[axis][0] > 1e-5 for axis in (a, b)),
         "World union projected plane changed or degenerate")
    centers = [f32((ranges[axis][0] + ranges[axis][1]) * .5) for axis in (a, b)]
    scale = f32(1.8 / max(ranges[a][1] - ranges[a][0], ranges[b][1] - ranges[b][0]))
    column = [[0.0] * 4 for _ in range(4)]
    column[0][a], column[1][b] = scale, scale
    column[0][3], column[1][3] = f32(-centers[0] * scale), f32(-centers[1] * scale)
    column[2][3], column[3][3] = .5, 1.0
    fit_path = PROBE / "ProbeInputs/Matrices/shared_world_to_clip_column.f32le"
    place(fit_path, pack_matrix(column))
    for case in world_specs:
        world = matrix((ROOT / case["world_matrix"]["source_path"]).read_bytes())
        final = transpose(mul(column, world))
        target = PROBE / f"ProbeInputs/Matrices/{case['phase']}_source_to_clip_row.f32le"
        place(target, pack_matrix(final))
        relative = rel(target).removeprefix(rel(PROBE) + "/")
        case["source_to_clip_row_major"] = {"path": relative, "probe_path": relative,
                                            "sha256": sha(target), "count": 16,
                                            "stride_bytes": 4, "byte_count": 64}
    contract = {"schema": "rok.unity_city_hall_world_raster_contract.v4",
                "repo_root": ROOT.as_posix(), "probe_root": PROBE.as_posix(), "unity_version": "2019.4.28f1c1",
                "producer_sha256": sha(Path(__file__)), "exporter_sha256": sha(EXPORTER),
                "verifier_sha256": sha(VERIFIER),
                "shader_source_path": rel(SOURCE_SHADER), "shader_sha256": SHADER_SHA,
                "settings_files": settings,
                "hall_native3": {"native_path": rel(NATIVE3 / "hall_world_native.json"), "sha256": NATIVE3_SHA,
                                 "verified_path": rel(NATIVE3_VERIFY), "verified_sha256": sha(NATIVE3_VERIFY),
                                 "renderer_file_id": "212751466141171401", "prefab_guid": "c825e8897df8e424e817e4ed05eef53e",
                                 "sprite_guid": "57ec7ddf0b743a140b2707fe14b536e2",
                                 "source_prefab_path": rel(SOURCE_PREFAB), "source_prefab_sha256": SOURCE_PREFAB_SHA,
                                 "source_families_path": rel(SOURCE_FAMILIES), "source_families_sha256": sha(SOURCE_FAMILIES)},
                "geometry": {"manifest_path": rel(GEOMETRY), "sha256": GEOMETRY_SHA,
                             "row_index": 27, "source_key": "initial:5", "source_row_sha256": row["source_row_sha256"],
                             **geom},
                "shared_fit": {"qualification": "diagnostic_union_world_AABB_not_GameApp_camera",
                               "axes": [a, b], "world_ranges": ranges, "centers": centers, "uniform_scale": scale,
                               "world_to_clip_column": {"path": rel(fit_path).removeprefix(rel(PROBE) + "/"),
                                                        "probe_path": rel(fit_path).removeprefix(rel(PROBE) + "/"),
                                                        "sha256": sha(fit_path), "count": 16,
                                                        "stride_bytes": 4, "byte_count": 64}},
                "matrix_convention": "Native3 W is Unity column-vector world=f32 W*local, row-major 64B. Shared C is column-vector clip=C*world. Shader and UE row-vector final=(C*W)^T stored row-major f32LE 64B; fixed union fit preserves relative translation.",
                "target": {"width": 128, "height": 128, "format": "ARGB32", "read_write": "Linear",
                           "anti_aliasing": 1, "mips": False, "filter": "Point", "clear_rgba": [.07, .15, .71, 1.0],
                           "shader": "mul(input.vertex,_ProbeLocalToClip); float4(UV.x,UV.y,.25,1); CullOff/ZTestAlways/ZWriteOff/BlendOff",
                           "readback": "ReadPixels RGBA32 bottom-left; one row reversal to canonical top-left raw; PNG decoded must equal raw"},
                "cases": world_specs,
                "scope": "private source geometry x accepted Hall Native3 world-matrix UV diagnostic; not GameApp camera, material/alpha, LOD, main view or full migration"}
    place(CONTRACT, (json.dumps(contract, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    (PROBE / "OutputRuns").mkdir(parents=True, exist_ok=True)
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    need(not REPORT.exists(), "Preparation report exists")
    report = {"schema": "rok.hall_world_raster_native_implementation.revision4", "prepared": True,
              "unity_started": False, "contract_path": rel(CONTRACT), "contract_sha256": sha(CONTRACT),
              "producer_sha256": sha(Path(__file__)), "exporter_sha256": sha(EXPORTER),
              "verifier_sha256": sha(VERIFIER),
              "shader_sha256": SHADER_SHA, "case_count": 2, "axes": [a, b],
              "shared_fit_sha256": sha(fit_path), "native3_sha256": NATIVE3_SHA,
              "scope": contract["scope"]}
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
