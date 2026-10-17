"""Independent static and native-result gates for the isolated Hall GameApp camera fixture."""
from __future__ import annotations

import argparse
import copy
import csv
import hashlib
import json
import math
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityHallCameraLodProbeV1"
CONTRACT = PROBE / "hall_camera_lod_contract_v2.json"
EXPECTED_SOURCES = {
    "scene": ("ref/client-unity/Assets/BundleAssets/Scenes/GameApp.unity", "34c0185bef408f1b0563be0185a35de6f72d99f4d5988dc8d6904e70ccd513c7"),
    "container": ("ref/client-unity/Assets/BundleAssets/Scene/City/city_function/CityBuildingContainer.prefab", "fc608b1af95af05e8eac25a9b2a35bb4a09a35ea1bdd40b9a80ffe69e14c39fa"),
    "hall": ("ref/client-unity/Assets/BundleAssets/Scene/City_building/city_building_1/build_1_101_1.prefab", "498032280f6bba9414bc61e7be9a3df61e7aa66358a663ba476d8032ac715c9a"),
    "camera_table": ("ref/config-tables/编译客户端配置/CSV/CameraParamLod.csv", "6c7163d7e6ca333a244a59ecf5a2ac37e6ad469320cc9cdac5a75bb47f6cf02c"),
    "age_table": ("ref/config-tables/编译客户端配置/CSV/CityAgeSize.csv", "36dfe1f46a4c1f3b005202c81692f75a90774125c401915ba1031f1d94232aff"),
    "native3": ("Saved/UnityCityHallWorldProbeV1/OutputRuns/20261009_032009_843_25796/hall_world_native.json", "01b924d764395708e20766834595fa7943f1ec61c70215b6058914c2cc80ab5e"),
    "native3_contract": ("Saved/UnityCityHallWorldProbeV1/hall_world_contract_v4.json", "60bfd58cc2c4897a49cb4440474825c641cc30685e40dc1fa6e404063fd56dce"),
    "geometry": ("Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json", "c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273"),
}
MATRIX_NAMES = ("camera_local_to_world", "camera_world_to_camera", "camera_projection",
                "camera_gpu_projection_rt", "camera_gpu_projection_screen", "hall_native3_world_source")
SAMPLE_INDICES = (0, 60, 120, 180, 239)


def need(ok: bool, reason: str) -> None:
    if not ok:
        raise AssertionError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def eq(a: float, b: float, tolerance: float, what: str) -> None:
    need(math.isfinite(a) and math.isfinite(b) and abs(a - b) <= tolerance,
         f"{what}: got {a}, expected {b}, tol {tolerance}")


def path_in(base: Path, relative: str) -> Path:
    need(isinstance(relative, str) and relative and not Path(relative).is_absolute() and
         ".." not in Path(relative).parts and "\\" not in relative, f"Unsafe probe path: {relative}")
    path = (base / relative).resolve()
    need(path.is_relative_to(base.resolve()), f"Probe path escaped: {relative}")
    return path


def source_values() -> dict:
    for path, expected in EXPECTED_SOURCES.values():
        need(sha(ROOT / path) == expected, f"Pinned source SHA drift: {path}")
    scene = (ROOT / EXPECTED_SOURCES["scene"][0]).read_text(encoding="utf-8-sig")
    camera = re.search(r"(?m)^--- !u!20 &105374294\s*\n(.*?)(?=^--- !u!|\Z)", scene, re.S | re.M)
    transform = re.search(r"(?m)^--- !u!4 &105374290\s*\n(.*?)(?=^--- !u!|\Z)", scene, re.S | re.M)
    parent = re.search(r"(?m)^--- !u!4 &425647252\s*\n(.*?)(?=^--- !u!|\Z)", scene, re.S | re.M)
    need(camera is not None and transform is not None and parent is not None, "GameApp source camera YAML absent")
    def v4(text: str, name: str) -> list[float]:
        found = re.search(rf"(?m)^  {name}: \{{x: ([^,]+), y: ([^,]+), z: ([^,]+), w: ([^}}]+)\}}", text)
        need(found is not None, f"Source quaternion absent {name}")
        return [float(x) for x in found.groups()]
    q = v4(transform.group(1), "m_LocalRotation")
    need(v4(parent.group(1), "m_LocalRotation") == [0, 0, 0, 1] and
         "m_LocalPosition: {x: 0, y: 0, z: 0}" in parent.group(1), "Source camera parent changed")
    need(q == [.38268343, 0.0, 0.0, .92387956] and
         "m_LocalPosition: {x: 3600, y: 12.870208, z: 3600}" in transform.group(1),
         "Source Camera transform changed")
    for label, expected in (("field of view", "30"), ("near clip plane", "2"), ("far clip plane", "3350")):
        need(f"{label}: {expected}" in camera.group(1), f"Source Camera {label} changed")
    def rows(name: str) -> list[dict]:
        with (ROOT / EXPECTED_SOURCES[name][0]).open("r", encoding="utf-8-sig", newline="") as stream:
            return list(csv.DictReader(stream))[1:]
    row = [r for r in rows("camera_table") if r["ID"] == "city_default"]
    need(len(row) == 1, "city_default not unique")
    dxf = float(row[0]["dxf"])
    dist = float(row[0]["dist"])
    fov = float(row[0]["fov"])
    need((dist, fov, dxf) == (20, 7.5, 150) and dist * fov == dxf, "Source city_default differs")
    ages = rows("age_table")
    need(len([r for r in ages if r["age"] == "1" and r["size"] == "41" and r["townLevel"] == "1"]) == 1 and
         max(int(r["size"]) for r in ages) == 57, "Source age size changed")
    native3 = json.loads((ROOT / EXPECTED_SOURCES["native3"][0]).read_text(encoding="utf-8-sig"))
    need(native3["qualification"] == "transform_fixture_only" and not native3["lod_qualified"],
         "Native3 is not the expected transform-only fixture")
    matches = [n for n in native3["nodes"] if n["phase"] == "runtime_replay_hall" and
               n["component_type"] == "SpriteRenderer" and n["source_file_id"] == "212751466141171401"]
    need(len(matches) == 1 and matches[0]["source_guid"] == "c825e8897df8e424e817e4ed05eef53e",
         "Native3 Hall renderer identity differs")
    world = (ROOT / EXPECTED_SOURCES["native3"][0]).parent / matches[0]["matrix_path"]
    need(sha(world) == matches[0]["matrix_sha256"] and world.stat().st_size == 64, "Native3 matrix changed")
    geometry = json.loads((ROOT / EXPECTED_SOURCES["geometry"][0]).read_text(encoding="utf-8-sig"))
    row27 = geometry["rows"][27]
    need(row27["index"] == 27 and row27["source_key"] == "initial:5" and
         row27["positions"]["count"] == 240 and row27["positions"]["stride_bytes"] == 12,
         "Source geometry row27 differs")
    positions = ROOT / row27["positions"]["path"]
    need(sha(positions) == row27["positions"]["sha256"] and positions.stat().st_size == 2880,
         "Hall positions buffer differs")
    container = (ROOT / EXPECTED_SOURCES["container"][0]).read_text(encoding="utf-8-sig")
    need("guid: 7c3728ec13555fb4d8d560e7ec13d2a7" in container and
         "  m_max_scale: 7.8" in container and
         re.search(r"time: 1\s+value: 1", container) and re.search(r"time: 250\s+value: 1", container),
         "Source Hall curve identity changed")
    hall = (ROOT / EXPECTED_SOURCES["hall"][0]).read_text(encoding="utf-8-sig")
    need("&212751466141171401" in hall and
         "m_Sprite: {fileID: 21300000, guid: 57ec7ddf0b743a140b2707fe14b536e2, type: 3}" in hall,
         "Source Hall prefab identity changed")
    return {"q": q, "dist": dist, "fov": fov, "dxf": dxf, "world": world, "positions": positions}


def validate_input(state: dict, source: dict) -> None:
    need(state["schema"] == "rok.hall_camera_lod_source_input.v1" and
         state["qualification"] == "camera_transform_fixture_only" and
         state["source_behaviors_executed"] is False, "Fixture qualifier changed")
    need(state["scene_camera_rotation_xyzw"] == source["q"] and
         state["scene_camera_local_position"] == [3600, 12.870208, 3600] and
         state["camera_target_xyz"] == [0, 0, 0], "Source/target camera transform changed")
    need((state["city_default_dist"], state["city_default_fov"], state["city_default_dxf"]) ==
         (source["dist"], source["fov"], source["dxf"]) and
         (state["near"], state["far"], state["scene_serialized_fov"]) == (2, 3350, 30),
         "Source camera parameters changed")
    need((state["output_width"], state["output_height"]) == (1440, 810) and
         state["scene_lod_thresholds"] == [1500, 3000, 8000, 20000, 42500, 999999] and
         state["container_lod_thresholds"] == [1500, 3000, 8000, 20000, 42500],
         "Viewport or LOD thresholds changed")
    keys = state["curve_keys"]
    need([(k["time"], k["value"]) for k in keys] ==
         [(1, 1), (250, 1), (1500, 8), (2250, 8), (2999.999, 8), (3000, 0)] and
         keys[0]["out_slope"] == "0" and keys[1]["in_slope"] in ("-0", "0") and
         state["castle_max_scale"] == 7.8 and state["age1_size"] == 41 and state["max_age_size"] == 57,
         "Source curve/age input changed")
    need(state["sample_vertex_indices"] == list(SAMPLE_INDICES) and
         state["hall_world_matrix_path"] == "ProbeInputs/hall_runtime_world_column.f32le" and
         state["positions_path"] == "ProbeInputs/hall_row27_positions.f32le", "Source projection samples changed")


def prepared() -> tuple[dict, dict, dict]:
    need(CONTRACT.is_file(), "Prepared contract missing")
    c = json.loads(CONTRACT.read_text(encoding="utf-8-sig"))
    need(c["schema"] == "rok.hall_camera_lod_contract.v2" and Path(c["probe_root"]).resolve() == PROBE.resolve() and
         Path(c["repo_root"]).resolve() == ROOT.resolve() and c["unity_version"] == "2019.4.28f1c1" and
         c["qualifier"] == "camera_transform_fixture_only" and c["source_behaviors_executed"] is False,
         "Prepared contract identity/qualification differs")
    for path_key, sha_key in (("producer_path", "producer_sha256"), ("verifier_path", "verifier_sha256"),
                              ("exporter_path", "exporter_sha256")):
        path = ROOT / c[path_key]
        need(path.is_file() and sha(path) == c[sha_key], f"Prepared script SHA mismatch: {path_key}")
    source = source_values()
    need(len(c["sources"]) == 10 and {s["name"] for s in c["sources"]} == set(EXPECTED_SOURCES) | {"world", "positions"},
         "Contract source identity set differs")
    for spec in c["sources"]:
        original = ROOT / spec["source_path"]
        copied = path_in(PROBE, spec["probe_path"])
        need(original.is_file() and copied.is_file() and sha(original) == sha(copied) == spec["sha256"],
             f"Source copy differs: {spec['name']}")
        if spec["name"] in EXPECTED_SOURCES:
            need((spec["source_path"], spec["sha256"]) == EXPECTED_SOURCES[spec["name"]],
                 f"Pinned source changed: {spec['name']}")
        else:
            need(original.resolve() == source[spec["name"]].resolve(),
                 f"World/geometry binary source changed: {spec['name']}")
    need(len(c["settings_files"]) == 21, "Unity Settings/Packages count differs")
    for spec in c["settings_files"]:
        original = ROOT / spec["source_path"]
        copied = path_in(PROBE, spec["probe_path"])
        need(original.is_file() and copied.is_file() and sha(original) == sha(copied) == spec["sha256"],
             f"Unity Settings/Packages changed: {spec['source_path']}")
    input_path = path_in(PROBE, c["input_path"])
    need(c["input_path"] == "ProbeInputs/source_camera_lod_input_v1.json" and
         sha(input_path) == c["input_sha256"], "Typed input path/SHA changed")
    state = json.loads(input_path.read_text(encoding="utf-8-sig"))
    validate_input(state, source)
    return c, state, source


def matrix(path: Path) -> list[list[float]]:
    need(path.stat().st_size == 64, f"Matrix is not 64B: {path}")
    values = struct.unpack("<16f", path.read_bytes())
    need(all(math.isfinite(v) for v in values), f"Nonfinite matrix: {path}")
    return [list(values[i * 4:(i + 1) * 4]) for i in range(4)]


def mulv(m: list[list[float]], v: tuple[float, float, float, float]) -> tuple[float, float, float, float]:
    return tuple(sum(m[row][col] * v[col] for col in range(4)) for row in range(4))


def quat_forward(q: list[float]) -> tuple[float, float, float]:
    x, y, z, w = q
    norm = math.sqrt(x*x + y*y + z*z + w*w)
    x, y, z, w = (v / norm for v in (x, y, z, w))
    return (2 * (x*z + w*y), 2 * (y*z - w*x), 1 - 2 * (x*x + y*y))


def verify_run(run: Path, c: dict, state: dict, source: dict) -> dict:
    need(run.resolve().parent == (PROBE / "OutputRuns").resolve() and
         re.fullmatch(r"\d{8}_\d{6}_\d{3}_\d+", run.name) is not None,
         "Run outside unique isolated OutputRuns")
    manifest_path = run / "native_camera_lod.json"
    need(manifest_path.is_file() and not (run / "failure.txt").exists(), "Native run incomplete/failed")
    native = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    need(native["schema"] == "rok.unity_city_hall_camera_lod_native.v1" and native["complete"] is True and
         native["contract_sha256"] == sha(CONTRACT) and native["input_sha256"] == c["input_sha256"] and
         native["exporter_sha256"] == c["exporter_sha256"], "Native code/input/hash provenance differs")
    for name, expected in (("hall_camera_lod_contract_v2.json", sha(CONTRACT)),
                           ("source_camera_lod_input_v1.json", c["input_sha256"]),
                           ("RenderRokUnityCityHallCameraLodNative.cs", c["exporter_sha256"])):
        need(sha(run / name) == expected, f"Native frozen input snapshot differs: {name}")
    need(native["graphics_device"] == "Direct3D11" and native["color_space"] == "Gamma" and
         native["qualifier"] == "camera_transform_fixture_only" and native["source_behaviors_executed"] is False and
         native["lod_final_visibility_qualified"] is False and native["source_material_alpha_qualified"] is False and
         native["main_view_qualified"] is False and native["image_count"] == 0,
         "Native graphics or qualification claims differ")
    need((native["width"], native["height"]) == (1440, 810), "Native 1440x810 viewport differs")
    need(native["target_xyz"] == [0, 0, 0], "Native fixture city-origin target differs")
    source_scene_position = [3600, 12.870208, 3600]
    scene_position = native["scene_serialized_camera_local_position"]
    need(len(scene_position) == 3 and all(struct.pack("<f", float(actual)) == struct.pack("<f", float(expected))
                                          for actual, expected in zip(scene_position, source_scene_position)),
         "Native serialized GameApp scene position differs at IEEE754 float32 bits")
    q = source["q"]
    forward = quat_forward(q)
    expected_pos = tuple(-20 * value for value in forward)
    for index, (actual, expected) in enumerate(zip(native["camera_forward_xyz"], forward)):
        eq(actual, expected, 2e-6, f"Camera forward[{index}]")
    for index, (actual, expected) in enumerate(zip(native["camera_position_xyz"], expected_pos)):
        eq(actual, expected, 2e-4, f"Camera position[{index}]")
    for index, (actual, expected) in enumerate(zip(native["camera_rotation_xyzw"], q)):
        eq(actual, expected, 2e-6, f"Camera rotation[{index}]")
    for key, expected in (("field_of_view", 7.5), ("aspect", 1440 / 810),
                          ("near_clip", 2), ("far_clip", 3350),
                          ("source_dxf", 150), ("measured_dxf", 150),
                          ("curve_scale", 1), ("source_max_scale", 7.8),
                          ("age_clamped_max_scale", 7.8 * 41 / 57),
                          ("effective_grid_item_scale", 1)):
        eq(native[key], expected, 2e-5, key)
    need(native["camera_lod_level"] == native["castle_lod_level"] == 0,
         "Source LOD selection at dxf150 differs")
    for actual, expected in zip(native["pixel_rect_xywh"], (0, 0, 1440, 810)):
        eq(actual, expected, 1e-5, "Camera pixelRect")
    records = native["matrices"]
    need(len(records) == 6 and [r["name"] for r in records] == list(MATRIX_NAMES),
         "Native camera matrix set/order differs")
    matrices = {}
    for spec in records:
        path = path_in(run, spec["path"])
        need(spec["path"] == f"matrices/{spec['name']}.f32le" and spec["byte_count"] == 64 and
             sha(path) == spec["sha256"], f"Native matrix SHA/path differs: {spec['name']}")
        matrices[spec["name"]] = matrix(path)
    need((run / records[-1]["path"]).read_bytes() == source["world"].read_bytes(),
         "Native3 Hall world matrix was modified")
    view = matrices["camera_world_to_camera"]
    projection = matrices["camera_projection"]
    # Unity Camera view uses OpenGL-style -Z forward. Recompose rows from source quaternion, not native view.
    x, y, z, w = (v / math.sqrt(sum(t*t for t in q)) for v in q)
    right = (1 - 2*(y*y + z*z), 2*(x*y + w*z), 2*(x*z - w*y))
    up = (2*(x*y - w*z), 1 - 2*(x*x + z*z), 2*(y*z + w*x))
    axes = (right, up, tuple(-v for v in forward))
    for row, axis in enumerate(axes):
        for col, expected in enumerate(axis): eq(view[row][col], expected, 3e-5, f"View rotation[{row},{col}]")
        eq(view[row][3], -sum(axis[i]*expected_pos[i] for i in range(3)), 3e-4,
           f"View translation[{row}]")
    yscale = 1 / math.tan(math.radians(7.5) / 2)
    xscale = yscale / (1440 / 810)
    expected_projection = ((0, 0, xscale), (1, 1, yscale),
                           (2, 2, -(3350 + 2) / (3350 - 2)),
                           (2, 3, -2*3350*2 / (3350 - 2)), (3, 2, -1))
    for row, col, expected in expected_projection:
        eq(projection[row][col], expected, 2e-4, f"Projection[{row},{col}]")
    need(len(native["projected_hall_vertices"]) == len(SAMPLE_INDICES), "Hall projection sample count differs")
    original = source["positions"].read_bytes()
    world = matrices["hall_native3_world_source"]
    for sample, index in zip(native["projected_hall_vertices"], SAMPLE_INDICES):
        need(sample["vertex_index"] == index, "Hall projection sample index differs")
        local = struct.unpack_from("<3f", original, index * 12)
        world4 = mulv(world, (*local, 1.0))
        camera4 = mulv(view, world4)
        clip = mulv(projection, camera4)
        need(abs(clip[3]) > 1e-8, "Hall projected W degenerate")
        expected_viewport = ((clip[0] / clip[3] + 1) * .5,
                             (clip[1] / clip[3] + 1) * .5, -camera4[2])
        for label, actuals, expecteds, tolerance in (("local", sample["local_xyz"], local, 1e-6),
                                                     ("world", sample["world_xyz"], world4[:3], 1e-4),
                                                     ("viewport", sample["viewport_xyz"], expected_viewport, 3e-4)):
            for coord, (actual, expected) in enumerate(zip(actuals, expecteds)):
                eq(actual, expected, tolerance, f"Hall sample {index} {label}[{coord}]")
    # The GPU projection depends on D3D11 RT convention. It is captured as exact bytes, not substituted for P.
    need(matrices["camera_gpu_projection_rt"] != matrices["camera_gpu_projection_screen"],
         "D3D11 render-target and screen GPU projection variants unexpectedly identical")
    return {"schema": "rok.hall_camera_lod_native_verify.v2", "passed": True,
            "native_path": str(manifest_path.relative_to(ROOT)).replace("\\", "/"),
            "native_sha256": sha(manifest_path), "contract_sha256": sha(CONTRACT),
            "source_pins_checked": len(EXPECTED_SOURCES), "source_copies_checked": 10,
            "matrix_count": len(records), "projected_hall_points": len(SAMPLE_INDICES),
            "qualification": native["qualifier"], "source_behaviors_executed": False,
            "main_view_accepted": False}


def negative_tests(state: dict, source: dict) -> int:
    failures = 0
    for name, mutate in (
        ("source_fov", lambda v: v.__setitem__("city_default_fov", 8.0)),
        ("unknown_lod", lambda v: v.__setitem__("scene_lod_thresholds", [100])),
        ("false_component_claim", lambda v: v.__setitem__("source_behaviors_executed", True)),
        ("wrong_matrix_input", lambda v: v.__setitem__("hall_world_matrix_path", "ProbeInputs/other.f32le")),
        ("wrong_target", lambda v: v.__setitem__("camera_target_xyz", [1, 0, 0])),
    ):
        trial = copy.deepcopy(state)
        mutate(trial)
        try:
            validate_input(trial, source)
        except AssertionError:
            failures += 1
        else:
            raise AssertionError(f"Negative test was accepted: {name}")
    return failures


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prepared", action="store_true")
    parser.add_argument("--run", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    need(args.prepared or args.run is not None, "Use --prepared or --run")
    c, state, source = prepared()
    result = {"schema": "rok.hall_camera_lod_prepared_verify.v2", "passed": True,
              "contract_sha256": sha(CONTRACT), "source_pins_checked": len(EXPECTED_SOURCES),
              "source_copies_checked": len(c["sources"]), "settings_files_checked": len(c["settings_files"]),
              "qualification": "camera_transform_fixture_only"}
    if args.self_test:
        result["negative_tests_rejected"] = negative_tests(state, source)
    if args.run is not None:
        result = verify_run(args.run, c, state, source)
        if args.self_test:
            result["negative_tests_rejected"] = negative_tests(state, source)
        output = args.run / "verified_camera_lod_v2.json"
        need(not output.exists(), "Native verification report already exists")
        output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
