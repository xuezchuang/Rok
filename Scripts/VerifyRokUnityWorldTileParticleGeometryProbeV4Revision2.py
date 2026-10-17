"""Strict Unity 2019 source geometry, particle reset, and live GPU checks."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "Saved/UnityWorldTileParticleProbeV2"
PROJECT = ROOT / "Saved/UnityWorldTileParticleGeometryProbeV4"
NATIVE = BASE / "OutputRuns/20261009_065833_466_26596/particle_native.json"
VISUAL = ROOT / "Saved/UnityWorldTileParticleVisualProbeV3/OutputRuns/20261009_072207_835_41412/visual_native.json"
CONTRACT = PROJECT / "probe_contract.json"
SOURCE = PROJECT / "Source/UnityRokWorldTileParticleGeometryProbeV4.cs"
REPORT = ROOT / "Saved/RokUnityMigration/BatchReview20261009WorldSearchAndTileEffects/TileParticleGeometrySource/geometry_probe_prepare_v4.json"
ORIGINAL_VERIFIER = ROOT / "Scripts/VerifyRokUnityWorldTileParticleGeometryProbeV4.py"
ORIGINAL_VERIFIER_SHA = "e2f685f8a91a95bfcc7adf3d2fb88b991afc5f97b622b9faf7b6846d324f779e"
NATIVE_SHA = "ce7d850f6dbb696a0d34e96df2d7f7ac0658eceadacd694d579a5f1ad3b6c75c"
VISUAL_SHA = "f8a48d3bb4e5c30f01dd1d8d595bbb05b935252d1e68050a93e2e988add21a6d"
BUILTIN = Path("C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data/Resources/unity_builtin_extra")
BUILTIN_SHA = "c73e9529e350fcc391f46a5cc323293a07a4b5ed71bb69b25d888fa794a99547"
TIMES = (2.5, 4.9, 5.1, 7.5)
NAMES = ("bowen", "bowen (1)")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path: Path) -> dict:
    require(path.is_file(), f"missing file: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def near(a: float, b: float, tol: float = 2e-4) -> bool:
    return math.isfinite(float(a)) and math.isfinite(float(b)) and abs(a - b) <= tol


def vector_equal(a: dict, b: dict, tol: float = 2e-4) -> bool:
    return all(near(a[k], b[k], tol) for k in ("x", "y", "z"))


def quaternion_equal(a: dict, b: dict, tol: float = 2e-4) -> bool:
    keys = ("x", "y", "z", "w")
    return (all(near(a[k], b[k], tol) for k in keys)
            or all(near(a[k], -b[k], tol) for k in keys))


def matrix_values(record: dict) -> list[float]:
    values = record["row_major"]
    require(len(values) == 16 and all(math.isfinite(float(value)) for value in values),
            "finite 4x4 row-major Unity matrix")
    return values


def transform_point(matrix: list[float], point: dict) -> dict:
    result = [sum(matrix[row * 4 + column] * value for column, value in
                  enumerate((point["x"], point["y"], point["z"], 1.0)))
              for row in range(3)]
    return dict(zip(("x", "y", "z"), result))


def matrix_inverse_check(local_to_world: dict, world_to_local: dict) -> None:
    left, right = matrix_values(local_to_world), matrix_values(world_to_local)
    for row in range(4):
        for column in range(4):
            value = sum(left[row * 4 + k] * right[k * 4 + column] for k in range(4))
            require(near(value, float(row == column), 1e-3),
                    "source localToWorld/worldToLocal inverse pair")


def geometry_check(system: dict, source: dict) -> dict:
    # Unity 2019 JsonUtility serializes a null nested [Serializable] MatrixState
    # as {"row_major": []}. The path and simulation enum remain source truth.
    empty_custom_matrix = system["custom_local_to_world"] in (None, {"row_major": []})
    require(system["simulation_space"] == source["simulation_space"] == "Local"
            and system["scaling_mode"] == source["scaling_mode"]
            and system["custom_simulation_space"] == ""
            and empty_custom_matrix,
            "source ParticleSystem simulation/scaling/custom-space semantics")
    require(system["renderer_render_mode"] == source["render_mode"] == "Mesh"
            and system["renderer_alignment"] == source["render_alignment"] == "Local",
            "actual source ParticleSystemRenderer mesh/alignment semantics")
    for key in ("local_position", "local_scale"):
        require(vector_equal(system[key], source[key]), f"source system {key}")
    require(quaternion_equal(system["local_rotation"], source["local_rotation"]),
            "source system local rotation")
    for key in ("renderer_pivot", "renderer_flip"):
        require(all(math.isfinite(float(system[key][axis])) for axis in ("x", "y", "z")),
                f"finite renderer {key}")
    require(math.isfinite(float(system["renderer_normal_direction"]))
            and math.isfinite(float(system["renderer_sorting_fudge"])),
            "finite source renderer orientation/sort parameters")
    bounds = system["renderer_world_bounds"]
    require(all(math.isfinite(float(bounds[part][axis])) for part in ("m_Center", "m_Extent")
                for axis in ("x", "y", "z")), "finite renderer world bounds")
    matrix_inverse_check(system["local_to_world"], system["world_to_local"])
    transform = matrix_values(system["local_to_world"])
    require(vector_equal(transform_point(transform, {"x": 0, "y": 0, "z": 0}),
                         system["world_position"], 1e-3),
            "source matrix origin equals world position")
    quad = system["source_quad"]
    require((quad["name"], quad["asset_path"], quad["guid"], quad["file_id"])
            == ("Quad", "Library/unity default resources",
                "0000000000000000e000000000000000", 10210),
            "actual Unity built-in source Quad identity")
    require(quad["vertex_count"] == source["mesh_vertex_count"] == 4
            and quad["sub_mesh_count"] == 1
            and len(quad["vertices"]) == len(quad["normals"]) == len(quad["uv0"]) == 4
            and len(quad["triangle_indices"]) == source["mesh_triangle_index_count"] == 6,
            "source built-in Quad topology")
    require(all(vector_equal(quad["bounds"][part], source["mesh_bounds"][part])
                for part in ("m_Center", "m_Extent")),
            "source built-in Quad bounds")
    require(set(quad["triangle_indices"]) == {0, 1, 2, 3}
            and all(type(index) is int and 0 <= index < 4 for index in quad["triangle_indices"]),
            "source Quad triangle indices")
    require(len({tuple(vertex[axis] for axis in ("x", "y", "z"))
                 for vertex in quad["vertices"]}) == 4,
            "source Quad distinct vertices")
    require(all(all(math.isfinite(float(row[axis])) for axis in ("x", "y", "z"))
                for row in quad["vertices"] + quad["normals"]),
            "source Quad finite vertex/normal data")
    require(all(near(sum(normal[axis] ** 2 for axis in ("x", "y", "z")), 1.0, 2e-3)
                for normal in quad["normals"]), "source Quad unit normals")
    require(all(all(math.isfinite(float(uv[axis])) and -1e-4 <= uv[axis] <= 1.0001
                    for axis in ("x", "y")) for uv in quad["uv0"]),
            "source Quad finite normalized UV0")
    for begin in (0, 3):
        a, b, c = [quad["vertices"][index] for index in quad["triangle_indices"][begin:begin + 3]]
        u = [b[axis] - a[axis] for axis in ("x", "y", "z")]
        v = [c[axis] - a[axis] for axis in ("x", "y", "z")]
        cross = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                 u[0] * v[1] - u[1] * v[0])
        require(sum(value * value for value in cross) > 1e-8,
                "source Quad nondegenerate triangle")
    return quad


def check_prepared() -> tuple[dict, dict]:
    require(sha(NATIVE) == NATIVE_SHA, "sealed V2 native particle witness SHA drift")
    require(sha(VISUAL) == VISUAL_SHA and load(VISUAL)["complete"] is True,
            "sealed V3 live GPU witness SHA/completion drift")
    require(sha(BUILTIN) == BUILTIN_SHA, "Unity 2019 binary built-in shader archive drift")
    native = load(NATIVE)
    require(native["schema"] == "rok.world_tile_particle_native_witness.v2"
            and [row["name"] for row in native["systems"]] == list(NAMES),
            "sealed native witness identity")
    contract = load(CONTRACT)
    report = load(REPORT)
    require(contract["schema"] == "rok.world_tile_particle_geometry_contract.v4"
            and report["schema"] == "rok.world_tile_particle_geometry_prepare.v4"
            and report["status"] == "prepared_not_rendered", "prepare schema/status")
    require(contract["source_native_sha256"] == NATIVE_SHA
            and report["source_native"]["sha256"] == NATIVE_SHA,
            "source witness pin")
    require(contract["source_gpu_v3_sha256"] == VISUAL_SHA
            and report["source_gpu_v3"]["sha256"] == VISUAL_SHA,
            "prior live GPU witness pin")
    require(contract["unity_builtin_extra_sha256"] == BUILTIN_SHA
            and report["builtin_shader_source"]["packaged_archive_sha256"] == BUILTIN_SHA
            and report["builtin_shader_source"]["status"] == "not_found_locally"
            and report["builtin_shader_source"]["blend_formula"] is None
            and report["builtin_shader_source"]["alpha_formula"] is None,
            "packaged archive is not verified ShaderLab blend/alpha source")
    require(contract["exporter_sha256"] == sha(SOURCE)
            == sha(PROJECT / "Assets/Editor/UnityRokWorldTileParticleGeometryProbeV4.cs")
            == report["exporter"]["sha256"], "exporter pin")
    require(contract["prepare_sha256"] == sha(ROOT / "Scripts/PrepareRokUnityWorldTileParticleGeometryProbeV4.py")
            == report["producer"]["sha256"], "prepare script pin")
    require(sha(ORIGINAL_VERIFIER) == ORIGINAL_VERIFIER_SHA
            and contract["verifier_sha256"] == ORIGINAL_VERIFIER_SHA
            == report["verifier"]["sha256"], "verifier script pin")
    require(report["contract"]["sha256"] == sha(CONTRACT), "contract pin")
    require(contract["times_seconds"] == list(TIMES)
            and contract["fixed_random_seed"] == 424242
            and contract["independent_resets_per_time"] == 2,
            "time/reset/seed contract")
    require(contract["source_particle_system_names_sorted"] ==
            sorted(("bowen", "bowen (1)", "shuihua", "xia-shui", "shuahua2", "shitou (1)"))
            and contract["selected_particle_system_names"] == list(NAMES)
            and contract["render_modes_per_reset"] ==
            ["bowen_only", "bowen_1_only", "combined"]
            and "GetCurrentColor" in contract["particle_current_color"]
            and "Particle.GetCurrentSize3D(system)" in contract["geometry_fields"]
            and "builtin Quad vertices/normals/uv0/triangles/bounds" in contract["geometry_fields"],
            "full six-component and selected two-component source contract")
    require("m_ActiveColorSpace: 0" in (PROJECT / "ProjectSettings/ProjectSettings.asset").read_text(),
            "source Gamma ProjectSettings")
    for kind, name in (("prefab", "Tile-JuDian-A2_lod2.prefab"),
                       ("material", "Ring_06_light.mat"), ("texture", "Ring_07.png")):
        actual = PROJECT / "Assets/ProbeInputs" / name
        source = BASE / "Assets/ProbeInputs" / name
        require(sha(actual) == sha(source) == contract[kind + "_sha256"],
                f"{kind} source/copy pin")
        require(sha(actual.with_name(name + ".meta")) == sha(source.with_name(name + ".meta")),
                f"{kind} meta pin")
    return contract, native


def pixel_check(frame: dict, run_dir: Path, width: int, height: int) -> bytes:
    png_name, raw_name = frame["png"], frame["raw_bgra"]
    require(Path(png_name).name == png_name and Path(raw_name).name == raw_name,
            "frame image path must be a local filename")
    png, raw = run_dir / png_name, run_dir / raw_name
    require(png.is_file() and raw.is_file(), "missing live-render frame files")
    require(sha(png) == frame["png_sha256"] and sha(raw) == frame["raw_bgra_sha256"],
            "recorded frame file SHA differs")
    raw_bytes = raw.read_bytes()
    require(len(raw_bytes) == width * height * 4, "BGRA dimensions")
    with Image.open(png) as image:
        require(image.format == "PNG" and image.mode == "RGBA" and image.size == (width, height),
                "PNG RGBA format and dimensions")
        rgba = image.tobytes()
    expected_bgra = bytearray(len(rgba))
    alpha = rgb = 0
    for at in range(0, len(rgba), 4):
        r, g, b, a = rgba[at:at + 4]
        expected_bgra[at:at + 4] = bytes((b, g, r, a))
        alpha += a > 0
        rgb += r > 0 or g > 0 or b > 0
    require(raw_bytes == expected_bgra, "raw BGRA and decoded PNG disagree")
    require(alpha == frame["alpha_nonzero_pixels"] and rgb == frame["rgb_nonzero_pixels"]
            and alpha > 0 and rgb > 0, "non-clear GPU image/pixel census")
    return raw_bytes


def contribution_check(combined: bytes, isolated_raws: list[bytes]) -> None:
    require(len(isolated_raws) == 2 and isolated_raws[0] != isolated_raws[1]
            and all(combined != single for single in isolated_raws),
            "composite must contain distinct pixels contributed by both source systems")


def particle_check(actual: dict, native: dict, time: float) -> None:
    require(actual["particle_count"] == native["particle_count"] > 0,
            f"source particle count at {time}")
    require(len(actual["particles"]) == actual["particle_count"], "particle array length")
    for record, source in zip(actual["particles"], native["particles"]):
        for name in ("position", "velocity"):
            require(vector_equal(record[name], source[name]), f"particle {name} at {time}")
        for name in ("remaining_lifetime", "start_lifetime", "current_size", "rotation"):
            require(near(record[name], source[name]), f"particle {name} at {time}")
        require(record["random_seed"] == source["random_seed"]
                and record["start_color"] == source["start_color"],
                f"particle seed/color at {time}")
        age = 1.0 - record["remaining_lifetime"] / record["start_lifetime"]
        require(0.0 <= age <= 1.0 and near(record["normalized_age"], age, 1e-4),
                f"particle normalized age at {time}")
        current = record["current_color"]
        require(all(type(current[key]) is int and 0 <= current[key] <= 255
                    for key in ("r", "g", "b", "a")), "current particle Color32 channels")
        require(all(abs(current[channel] - record["start_color"][channel]) <= 1
                    for channel in ("r", "g", "b")),
                "source constant-white RGB gradient preserves particle start RGB")
        alpha_keys = native["source_alpha_keys"]
        if age <= alpha_keys[1]["time"]:
            alpha = alpha_keys[1]["alpha"] * age / alpha_keys[1]["time"]
        else:
            alpha = alpha_keys[1]["alpha"] * (1.0 - age) / (1.0 - alpha_keys[1]["time"])
        expected_alpha = record["start_color"]["a"] * alpha
        require(abs(current["a"] - expected_alpha) <= 2.0,
                f"current particle alpha follows 0->0.447->0 at normalized age {age}")
        for key in ("rotation3d", "axis_of_rotation", "current_size3d", "world_position"):
            require(all(math.isfinite(float(record[key][axis])) for axis in ("x", "y", "z")),
                    f"finite source particle {key} at {time}")
        require(all(record["current_size3d"][axis] > 0 for axis in ("x", "y", "z")),
                f"positive current source 3D size at {time}")
        require(vector_equal(record["world_position"],
                             transform_point(matrix_values(actual["local_to_world"]),
                                             record["position"]), 1e-3),
                f"per-particle local-to-world mapping at {time}")


def gradient_check(actual: dict, native: dict) -> None:
    require(actual["enabled"] is True and actual["mode"] == "Gradient"
            and native["mode"] == "Gradient", "source ColorOverLifetime enabled/mode")
    require(len(actual["color_keys"]) == len(native["color_keys"]) == 2
            and len(actual["alpha_keys"]) == len(native["alpha_keys"]) == 3,
            "source color/alpha key counts")
    for key, source in zip(actual["color_keys"], native["color_keys"]):
        require(near(key["time"], source["time"])
                and all(near(key["color"][channel], source["color"][channel])
                        and near(key["color"][channel], 1.0)
                        for channel in ("r", "g", "b", "a")),
                "constant-white source RGB gradient keys")
    for key, source in zip(actual["alpha_keys"], native["alpha_keys"]):
        require(near(key["time"], source["time"])
                and near(key["alpha"], source["alpha"]),
                "source alpha gradient keys")
    require(near(actual["alpha_keys"][0]["alpha"], 0.0)
            and near(actual["alpha_keys"][1]["alpha"], 0.4470588)
            and near(actual["alpha_keys"][2]["alpha"], 0.0),
            "source alpha 0->0.447->0")


def check_run(run_dir: Path, contract: dict, native: dict) -> dict:
    require(run_dir.resolve().is_relative_to((PROJECT / "OutputRuns").resolve()),
            "run directory escaped isolated project")
    evidence = load(run_dir / "geometry_native.json")
    camera = evidence["camera"]
    require(evidence["schema"] == "rok.world_tile_particle_geometry.v4"
            and evidence["complete"] is True
            and evidence["contract_sha256"] == sha(CONTRACT)
            and evidence["exporter_sha256"] == sha(SOURCE)
            and evidence["source_native_sha256"] == NATIVE_SHA,
            "native visual report identity/completion")
    require(evidence["unity_version"] == "2019.4.28f1c1"
            and Path(evidence["project_path"]).resolve() == PROJECT.resolve(),
            "Unity/project identity")
    require(evidence["prefab_sha256"] == contract["prefab_sha256"]
            and evidence["material_sha256"] == contract["material_sha256"]
            and evidence["texture_sha256"] == contract["texture_sha256"],
            "source asset file pins")
    require(evidence["source_material_shader"] == "Mobile/Particles/Alpha Blended"
            and evidence["source_texture_path"] == "Assets/ProbeInputs/Ring_07.png"
            and (evidence["source_texture_width"], evidence["source_texture_height"]) == (128, 128)
            and evidence["material_has_tint_property"] is False
            and evidence["material_has_inv_fade_property"] is False,
            "actual source shader/property/texture binding")
    require(evidence["discovered_particle_system_count"] == 6
            and evidence["discovered_particle_system_names"] ==
            contract["source_particle_system_names_sorted"]
            and evidence["selected_particle_system_names"] == list(NAMES)
            and evidence["selected_hierarchy_paths"] ==
            [system["hierarchy_path"] for system in native["systems"]],
            "actual Unity prefab full six-component topology and exact NativeV2 selection")
    require(evidence["fixed_random_seed"] == 424242
            and "Stop(true,StopEmittingAndClear)" in evidence["reset_policy"],
            "seed/reset method")
    require(camera["purpose"] == "isolated top-down orthographic diagnostic of the two particle systems only"
            and camera["width"] == camera["height"] == 1024
            and near(camera["orthographic_size"], 8.0)
            and near(camera["near_clip"], 0.1) and near(camera["far_clip"], 100.0)
            and type(camera["render_texture_srgb"]) is bool
            and camera["project_color_space"] == camera["active_color_space"] == "Gamma"
            and camera["render_texture_format"] == "ARGB32"
            and camera["capture_texture_format"] == "RGBA32"
            and camera["raw_pixel_format"] == "BGRA8"
            and camera["raw_row_order"] == "top-to-bottom",
            "diagnostic camera/color/raw contract")
    require(all(near(camera["clear_color"][key], 0.0) for key in ("r", "g", "b", "a")),
            "transparent render target clear")
    expected_camera = {
        "x": (native["systems"][0]["world_position"]["x"] + native["systems"][1]["world_position"]["x"]) / 2,
        "y": (native["systems"][0]["world_position"]["y"] + native["systems"][1]["world_position"]["y"]) / 2 + 20,
        "z": (native["systems"][0]["world_position"]["z"] + native["systems"][1]["world_position"]["z"]) / 2,
    }
    require(vector_equal(camera["position"], expected_camera)
            and quaternion_equal(camera["rotation"],
                                 {"x": 0.70710678, "y": 0.0, "z": 0.0, "w": 0.70710678}),
            "source-centered top-down diagnostic camera pose")
    samples = evidence["samples"]
    require(len(samples) == len(TIMES), "sample count")
    checked_files: set[str] = set()
    source_quad_reference: dict | None = None
    for sample, time in zip(samples, TIMES):
        require(near(sample["time_seconds"], time, 1e-4), "fixed nonzero sample time")
        repeats = sample["independent_resets"]
        require(len(repeats) == 2, "two independent source resets")
        raws = []
        for index, frame in enumerate(repeats):
            require(frame["reset_index"] == index and frame["fixed_seed"] == 424242,
                    "independent reset index/seed")
            require(len(frame["systems"]) == 2
                    and [system["name"] for system in frame["systems"]] == list(NAMES),
                    "two exact source particle systems")
            for system, source in zip(frame["systems"], native["systems"]):
                require(system["hierarchy_path"] == source["hierarchy_path"]
                        and system["mesh_guid"] == "0000000000000000e000000000000000"
                        and system["mesh_file_id"] == 10210
                        and system["shader_name"] == "Mobile/Particles/Alpha Blended"
                        and system["material_paths"] == ["Assets/ProbeInputs/Ring_06_light.mat"] * 2,
                        "source hierarchy/quad/material binding")
                require(vector_equal(system["world_position"], source["world_position"])
                        and vector_equal(system["world_lossy_scale"], source["world_lossy_scale"])
                        and quaternion_equal(system["world_rotation"], source["world_rotation"]),
                        "source world TRS")
                quad = geometry_check(system, source)
                if source_quad_reference is None:
                    source_quad_reference = quad
                else:
                    require(quad == source_quad_reference,
                            "built-in source Quad topology changed by system/time/reset")
                source_sample = next((row for row in source["samples"]
                                      if near(row["time_seconds"], time, 1e-4)), None)
                require(source_sample is not None, "native V2 time missing")
                gradient_check(system["color_over_lifetime"], source["color_over_lifetime"])
                reference = dict(source_sample["independent_resets"][0])
                reference["source_alpha_keys"] = source["color_over_lifetime"]["alpha_keys"]
                particle_check(system, reference, time)
            for name in (frame["png"], frame["raw_bgra"]):
                require(name not in checked_files, "frame filename reused")
                checked_files.add(name)
            combined = pixel_check(frame, run_dir, 1024, 1024)
            isolated = frame["isolated_renderings"]
            require(len(isolated) == 2
                    and [entry["system_name"] for entry in isolated] == list(NAMES),
                    "both source systems have explicit isolated GPU renders")
            isolated_raws = []
            for entry in isolated:
                evidence = entry["pixels"]
                for name in (evidence["png"], evidence["raw_bgra"]):
                    require(name not in checked_files, "isolated frame filename reused")
                    checked_files.add(name)
                isolated_raws.append(pixel_check(evidence, run_dir, 1024, 1024))
            contribution_check(combined, isolated_raws)
            raws.append((combined, isolated_raws))
        require(repeats[0]["png_sha256"] == repeats[1]["png_sha256"]
                and repeats[0]["raw_bgra_sha256"] == repeats[1]["raw_bgra_sha256"]
                and raws[0] == raws[1]
                and all(first["pixels"][key] == second["pixels"][key]
                        for first, second in zip(repeats[0]["isolated_renderings"],
                                                 repeats[1]["isolated_renderings"])
                        for key in ("png_sha256", "raw_bgra_sha256")),
                "independent reset GPU frames differ")
        require(json.dumps(repeats[0]["systems"], sort_keys=True) ==
                json.dumps(repeats[1]["systems"], sort_keys=True),
                "independent reset particle data differ")
    require(len(checked_files) == 48, "four times x two resets x three renders x two files")
    return {"schema": "rok.world_tile_particle_geometry_verify.v4_revision2", "passed": True,
            "run": run_dir.relative_to(ROOT).as_posix(),
            "revised_verifier_sha256": sha(Path(__file__)),
            "original_verifier_sha256": ORIGINAL_VERIFIER_SHA,
            "evidence_sha256": sha(run_dir / "geometry_native.json"),
            "image_file_count": len(checked_files),
            "nonzero_times": list(TIMES), "two_reset_frames_equal": True,
            "pixel_contract": "each isolated system and composite PNG RGBA equals raw BGRA; each has positive pixels; combined differs from each source system",
            "geometry_contract": "source Quad topology, Particle 3D pose/axis/size, mesh renderer mode/alignment and inverse world matrices verified"}


def self_test() -> None:
    with tempfile.TemporaryDirectory() as folder:
        directory = Path(folder)
        image = Image.new("RGBA", (2, 2), (0, 0, 0, 0))
        image.putpixel((0, 0), (10, 20, 30, 40))
        path, raw = directory / "good.png", directory / "good.bgra"
        image.save(path)
        raw.write_bytes(bytes((30, 20, 10, 40)) + bytes(12))
        frame = {"png": "good.png", "raw_bgra": "good.bgra", "png_sha256": sha(path),
                 "raw_bgra_sha256": sha(raw), "alpha_nonzero_pixels": 1,
                 "rgb_nonzero_pixels": 1}
        pixel_check(frame, directory, 2, 2)
        negative = 0
        for mutate in ("sha", "raw", "clear", "path"):
            bad = dict(frame)
            if mutate == "sha": bad["png_sha256"] = "0" * 64
            if mutate == "raw": raw.write_bytes(bytes(16))
            if mutate == "clear": bad["alpha_nonzero_pixels"] = 0
            if mutate == "path": bad["png"] = "../good.png"
            try:
                pixel_check(bad, directory, 2, 2)
            except ValueError:
                negative += 1
            else:
                raise AssertionError(f"negative case accepted: {mutate}")
            raw.write_bytes(bytes((30, 20, 10, 40)) + bytes(12))
        require(negative == 4, "negative-case coverage")
        source_a, source_b = bytes((1, 0)), bytes((0, 1))
        contribution_check(bytes((1, 1)), [source_a, source_b])
        for combined, sources in ((source_a, [source_a, source_b]),
                                  (bytes((1, 1)), [source_a, source_a]),
                                  (bytes((1, 1)), [source_a])):
            try:
                contribution_check(combined, sources)
            except ValueError:
                negative += 1
            else:
                raise AssertionError("missing/identical isolated contribution accepted")
        identity = [float(row == column) for row in range(4) for column in range(4)]
        shifted = identity.copy()
        shifted[3] = 5.0
        inverse = identity.copy()
        inverse[3] = -5.0
        matrix_inverse_check({"row_major": shifted}, {"row_major": inverse})
        invalid = inverse.copy()
        invalid[3] = -4.0
        try:
            matrix_inverse_check({"row_major": shifted}, {"row_major": invalid})
        except ValueError:
            negative += 1
        else:
            raise AssertionError("non-inverse source matrices accepted")
        v = lambda x, y, z: {"x": x, "y": y, "z": z}
        q = {"x": 0, "y": 0, "z": 0, "w": 1}
        quad = {"name": "Quad", "asset_path": "Library/unity default resources",
                "guid": "0000000000000000e000000000000000", "file_id": 10210,
                "vertex_count": 4, "sub_mesh_count": 1,
                "vertices": [v(-.5, -.5, 0), v(.5, -.5, 0),
                             v(.5, .5, 0), v(-.5, .5, 0)],
                "normals": [v(0, 0, -1)] * 4,
                "uv0": [{"x": 0, "y": 0}, {"x": 1, "y": 0},
                        {"x": 1, "y": 1}, {"x": 0, "y": 1}],
                "triangle_indices": [0, 2, 1, 0, 3, 2],
                "bounds": {"m_Center": v(0, 0, 0), "m_Extent": v(.5, .5, 0)}}
        source = {"simulation_space": "Local", "scaling_mode": "Local",
                  "render_mode": "Mesh", "render_alignment": "Local",
                  "local_position": v(0, 0, 0), "local_scale": v(1, 1, 1),
                  "local_rotation": q, "mesh_vertex_count": 4,
                  "mesh_triangle_index_count": 6, "mesh_bounds": quad["bounds"]}
        system = {"simulation_space": "Local", "scaling_mode": "Local",
                  "custom_simulation_space": "", "custom_local_to_world": {"row_major": []},
                  "renderer_render_mode": "Mesh", "renderer_alignment": "Local",
                  "renderer_pivot": v(0, 0, 0), "renderer_flip": v(0, 0, 0),
                  "renderer_normal_direction": 1.0, "renderer_sorting_fudge": 0.0,
                  "renderer_world_bounds": quad["bounds"],
                  "local_position": v(0, 0, 0), "local_scale": v(1, 1, 1),
                  "local_rotation": q, "world_position": v(0, 0, 0),
                  "local_to_world": {"row_major": identity},
                  "world_to_local": {"row_major": identity},
                  "source_quad": quad}
        geometry_check(system, source)
        for mutate in ("quad_vertex", "triangle", "renderer_mode", "world_matrix",
                       "custom_matrix", "custom_mode"):
            bad = copy.deepcopy(system)
            if mutate == "quad_vertex":
                bad["source_quad"]["vertices"][1] = bad["source_quad"]["vertices"][0]
            if mutate == "triangle":
                bad["source_quad"]["triangle_indices"][0] = 9
            if mutate == "renderer_mode":
                bad["renderer_render_mode"] = "Billboard"
            if mutate == "world_matrix":
                bad["local_to_world"]["row_major"][3] = 1.0
            if mutate == "custom_matrix":
                bad["custom_local_to_world"]["row_major"] = identity
            if mutate == "custom_mode":
                bad["simulation_space"] = "Custom"
            try:
                geometry_check(bad, source)
            except ValueError:
                negative += 1
            else:
                raise AssertionError(f"invalid source geometry accepted: {mutate}")
    print(json.dumps({"self_test_passed": True, "negative_cases_rejected": negative}))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--run", type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
    if args.prepare_only or args.run:
        contract, native = check_prepared()
        if args.run:
            result = check_run(args.run, contract, native)
            print(json.dumps(result, indent=2))
        else:
            print(json.dumps({"prepare_verified": True, "contract_sha256": sha(CONTRACT)}))
    if not args.self_test and not args.prepare_only and not args.run:
        parser.error("pass --prepare-only, --self-test, or --run PATH")


if __name__ == "__main__":
    main()
