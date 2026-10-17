"""Strict real-pixel Unity Gamma vs UE GameApp compositor fixture verification.

This is an isolated diagnostic fixture, not acceptance of actual city placement.
The native PNGs, UE RHI BGRA8 bytes, and UE PNGs are compared directly. No CPU
composition or analytic substitute image is used for the visual gates.
"""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
RUN_ROOT = ROOT / "Saved/RokUnityMigration/CityGammaCompositeAudit"
NATIVE_ROOT = ROOT / "Saved/UnityCityGammaCompositeProbe/OutputRuns/20261008_171930_737_10560"
NATIVE_REPORT = NATIVE_ROOT / "native_render.json"
NATIVE_VERIFY = NATIVE_ROOT / "native_verify.json"
CONTRACT = ROOT / "Saved/UnityCityGammaCompositeProbe/probe_contract.json"
PATTERN = NATIVE_ROOT / "pattern_source.png"
ACADEMY_ASSET = ROOT / "Saved/RokUnityMigration/AcademyPresentationMesh/ue_mesh_create.json"
RESOURCE_ASSET = ROOT / "Saved/RokUnityMigration/CityResourcePresentationAssets/ue_assets_verify.json"
SIZE = (1280, 720)
NATIVE_PINNED_SHA = {
    "native_render": "e948d7706b8d6ea2166146247993bbacb6c4cafcc673dce8c1c0a03f9d19294c",
    "native_contract": "b8ca67cc92f6499debc4ea6649c4b2444ca09da603c524eaf9649be89fe74fa8",
    "native_verify": "2a23340b402ec83637d82d2f86c4331dfb6026ffe4a7d68a7c0c355b1be36411",
    "pattern_source": "c98ccb2de10f61fac604813eb09107a35351bc98d3cc5d13a8c62f6b5d066705",
}
MODELS = {
    "academy": ("build_13_101_2", "/Game/RokUnity/City/Academy/build_13_101_2"),
    "farm": ("build_2_101_1", "/Game/RokUnity/City/ResourceBuildings/build_2_101_1"),
    "wood": ("build_3_101_1", "/Game/RokUnity/City/ResourceBuildings/build_3_101_1"),
}
EXACT_OUTPUT_BASE = ("/Game/RokDiagnostics/CityGammaComposite/"
                     "M_CityGammaExactOutput.M_CityGammaExactOutput")
EXACT_OUTPUT_LOCATION = "BL_ReplacingTonemapper"
# Fixed before the first UE composite run. The Academy single-frame 12/24
# values are reused for DIRECT native translucent pixels, not called a rerun
# of the old CPU-analytic source-model check.
BUDGET = {
    "background_max_rgb8": 2,
    "background_min_pixels": 600000,
    "background_exclusion_dilate_px": 2,
    "coverage_postblend_alpha_threshold_byte": 1,
    "core_erode_px": 1,
    "core_mae_rgb8": 8.0,
    "core_p95_rgb8": 16.0,
    "bbox_max_edge_px": 2,
    "mask_iou_min": .98,
    "academy_single_translucent_mae_rgb8": 12.0,
    "academy_single_translucent_p95_rgb8": 24.0,
    "order_native_changed_overlap_pixels_min": 100,
    "order_ue_changed_overlap_pixels_min": 100,
    "order_changed_pixel_max_channel_threshold_rgb8": 2,
    "order_valid_native_delta_channel_min_rgb8": 4,
    "order_sign_agreement_min": .95,
    "order_delta_difference_mae_rgb8": 8.0,
    "order_delta_difference_p95_rgb8": 16.0,
    "local_vertex_cm": .05,
    "uv_absolute": 1e-5,
    "projection_median_px": 1.0,
    "projection_max_px": 2.0,
}
STAT_FIELDS = (
    "before_passes", "after_passes", "valid_pre_exposure_passes",
    "invalid_pre_exposure_passes", "eligible_views_without_before",
    "unpaired_before_passes", "unpaired_after_passes", "target_conflicts",
    "rejected_textures", "pending_gpu_validations", "registered_targets",
)
NO_ERROR_STATS = (
    "invalid_pre_exposure_passes", "eligible_views_without_before",
    "unpaired_before_passes", "unpaired_after_passes", "target_conflicts", "rejected_textures",
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def require(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)


def close(actual: float, expected: float, tolerance: float = 1e-5) -> bool:
    return abs(float(actual) - float(expected)) <= tolerance


def png_rgb(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        image.load()
        require(image.size == SIZE and image.mode in ("RGB", "RGBA"),
                f"Wrong PNG dimensions/color type: {path}: {image.size}/{image.mode}")
        return np.asarray(image.convert("RGB"), dtype=np.uint8).copy()


def png_alpha(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        image.load()
        require(image.size == SIZE and image.mode == "RGBA", f"Coverage RGBA missing: {path}")
        return np.asarray(image, dtype=np.uint8)[:, :, 3].copy()


def erode(mask: np.ndarray, times: int = 1) -> np.ndarray:
    result = mask.astype(bool)
    for _ in range(times):
        padded = np.pad(result, 1, constant_values=False)
        result = np.logical_and.reduce([padded[dy:dy + result.shape[0], dx:dx + result.shape[1]]
                                        for dy in range(3) for dx in range(3)])
    return result


def dilate(mask: np.ndarray, times: int = 1) -> np.ndarray:
    result = mask.astype(bool)
    for _ in range(times):
        padded = np.pad(result, 1, constant_values=False)
        result = np.logical_or.reduce([padded[dy:dy + result.shape[0], dx:dx + result.shape[1]]
                                       for dy in range(3) for dx in range(3)])
    return result


def bbox(mask: np.ndarray) -> list[int]:
    yy, xx = np.nonzero(mask)
    require(len(xx) > 100, "Foreground silhouette has fewer than 100 pixels")
    return [int(xx.min()), int(yy.min()), int(xx.max()), int(yy.max())]


def stats_values(values: np.ndarray) -> dict:
    require(values.size >= 100, "Fewer than 100 RGB samples")
    absolute = np.abs(values.astype(np.float64))
    return {"rgb_samples": int(absolute.size), "mae": float(absolute.mean()),
            "p95": float(np.percentile(absolute, 95)), "max": float(absolute.max())}


def color_stats(native: np.ndarray, ue: np.ndarray, mask: np.ndarray) -> dict:
    require(int(mask.sum()) >= 100, "Fewer than 100 selected pixels")
    result = stats_values(native.astype(np.int16)[mask] - ue.astype(np.int16)[mask])
    result["pixels"] = int(mask.sum())
    return result


def bounded_path(raw: str, directory: Path, expected_name: str) -> Path:
    result = Path(raw).resolve()
    require(result.parent == directory.resolve() and result.name == expected_name and result.is_file(),
            f"Output path not exact run child: {raw}")
    return result


def asset_file(object_path: str) -> Path:
    require(object_path.startswith("/Game/") and ".." not in Path(object_path).parts,
            f"Unsafe asset path: {object_path}")
    package = object_path.split(".", 1)[0]
    result = ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")
    require(result.is_file(), f"Asset file missing: {result}")
    return result


def source_inputs(manifest: dict, native: dict, contract: dict, native_verify: dict) -> None:
    require(manifest["native_render_sha256"] == sha(NATIVE_REPORT) == NATIVE_PINNED_SHA["native_render"] and
            manifest["native_contract_sha256"] == sha(CONTRACT) == NATIVE_PINNED_SHA["native_contract"] and
            manifest["native_verify_sha256"] == sha(NATIVE_VERIFY) == NATIVE_PINNED_SHA["native_verify"] and
            manifest["pattern_source_sha256"] == sha(PATTERN) == NATIVE_PINNED_SHA["pattern_source"],
            "Pinned Unity native/contract/verify/pattern hash chain changed")
    require(native["schema"] == "rok.city_gamma_composite_native_render.v1" and
            contract["schema"] == "rok.city_gamma_composite_native_contract.v1" and
            native_verify["schema"] == "rok.city_gamma_composite_native_verify.v1" and
            native_verify["passed"] and
            native_verify["native_render_sha256"] == manifest["native_render_sha256"] and
            native_verify["contract_sha256"] == manifest["native_contract_sha256"] and
            native_verify["pattern_source_png_sha256"] == manifest["pattern_source_sha256"],
            "Unity native report is not the independently verified 8-frame chain")
    require(len(native["frames"]) == len(contract["fixtures"]) == len(manifest["frames"]) == 8 and
            [f["name"] for f in native["frames"]] == [f["name"] for f in contract["fixtures"]],
            "Native fixture list/order differs from source contract")
    for frame in native["frames"]:
        path = bounded_path(str(NATIVE_ROOT / frame["png"]), NATIVE_ROOT, frame["name"] + ".png")
        require(sha(path) == native_verify["frame_png_sha256"][frame["name"]],
                "Unity native frame changed: " + frame["name"])
    for relative, item in contract["source_files"].items():
        require(relative.startswith("ref/") and ".." not in Path(relative).parts,
                "Source chain contains path outside ref")
        copy = ROOT / "Saved/UnityCityGammaCompositeProbe" / item["probe_asset"]
        require(sha(ROOT / relative) == item["sha256"] == sha(copy) and
                sha(Path(str(ROOT / relative) + ".meta")) == item["meta_sha256"] ==
                sha(Path(str(copy) + ".meta")), "Unity source/import metadata changed: " + relative)
    catalogs = {"academy": ROOT / "Saved/RokUnityMigration/AcademyVisualSource/catalog.json",
                "resources": ROOT / "Saved/RokUnityMigration/CityResourceBuildingVisualSources/catalog.json"}
    require(all(sha(path) == contract["source_catalogs"][label] for label, path in catalogs.items()),
            "Unity source catalog SHA changed")
    require(native_verify["renderer_sha256"] == contract["renderer_script_sha256"] ==
            sha(ROOT / "Scripts/RenderRokCityGammaCompositeNative.cs"),
            "Native renderer SHA changed")
    expected_inputs = {
        "Source/Rok/RokCityGammaCompositeAuditSubsystem.h",
        "Source/Rok/RokCityGammaCompositeAuditSubsystem.cpp",
        "Source/RokRendering/Public/RokCityGammaCompositor.h",
        "Source/RokRendering/Private/RokCityGammaCompositor.cpp",
        "Shaders/Private/RokCityGammaTransfer.usf",
    }
    require(set(manifest["input_sha256"]) == expected_inputs, "UE capture source-file list changed")
    for relative, expected in manifest["input_sha256"].items():
        require(expected == sha(ROOT / relative), "UE capture source changed: " + relative)


def canonical_position(position: list[float], pivot: dict) -> tuple[float, float, float]:
    """Same pixel-lattice float32 roundtrip bound as sealed resource OBJ proof."""
    x, y, z = map(float, position)
    lattice_x = float(pivot["x"]) - round(float(pivot["x"]) - x)
    lattice_y = round(y + float(pivot["y"])) - float(pivot["y"])

    def bound(value: float) -> float:
        def ulp32(number: float) -> float:
            return 2.0 ** (math.floor(math.log2(max(abs(number), 1e-30))) - 23)
        return max(1e-6, 100.0 * ulp32(value / 100.0) + ulp32(value))

    require(abs(x - lattice_x) <= bound(x) and abs(y - lattice_y) <= bound(y) and
            abs(z) <= bound(z), "UE local vertex outside approved source pixel lattice")
    return lattice_x, lattice_y, 0.0


def geometry_faces(label: str, recorded: dict, source: dict, approved: dict) -> dict:
    geometry = source["geometry"]
    native_vertices, native_uvs, native_indices = (
        geometry["vertices"], geometry["uvs"], geometry["triangles"])
    ue_vertices = recorded["vertices"]
    ue_triangles = recorded["index_buffer_triangles"]
    require(len(native_vertices) == len(native_uvs) and len(native_indices) % 3 == 0 and
            len(ue_vertices) == recorded["vertex_count"] and
            len(ue_triangles) == recorded["triangle_count"] and
            all(len(triangle) == 3 for triangle in ue_triangles),
            label + " index/vertex buffer incomplete")
    require(recorded["vertex_count"] <= len(native_vertices) and
            recorded["triangle_count"] <= len(native_indices) // 3,
            label + " imported source mesh count exceeds Unity source")
    pivot = geometry["pivot"]
    source_corners = []
    for vertex, uv in zip(native_vertices, native_uvs):
        position = canonical_position([-float(vertex["x"]) * 100.0,
                                       float(vertex["y"]) * 100.0, 0.0], pivot)
        source_corners.append(tuple(round(value, 5) for value in position) +
                              (round(float(uv["x"]), 6), round(float(uv["y"]), 6)))
    source_faces = [tuple(sorted(source_corners[native_indices[i + j]] for j in range(3)))
                    for i in range(0, len(native_indices), 3)]
    source_by_position = collections.defaultdict(list)
    for index, corner in enumerate(source_corners):
        source_by_position[corner[:3]].append(index)
    allowed_zero = set(approved["source_zero_area_triangle_indices"])
    require(all(0 <= index < len(source_faces) for index in allowed_zero), label + " approved zero-area list invalid")
    # The sealed import proof allows only these exact source-face signatures
    # to be absent. No small-area cutoff is introduced here.
    expected_all = collections.Counter(source_faces)
    allowed_missing = collections.Counter(source_faces[index] for index in allowed_zero)
    require(approved["source_nonzero_faces_proved"] == len(source_faces) - len(allowed_zero),
            label + " approved nonzero-face proof differs")
    native_screen = np.asarray([[v["x"], SIZE[1] - v["y"]] for v in geometry["screenVertices"]], dtype=float)
    ue_screen = np.asarray([v["screen_xy_top_left_depth_cm"][:2] for v in ue_vertices], dtype=float)
    require(len(native_screen) == len(source_corners) and ue_screen.shape == (len(ue_vertices), 2) and
            np.isfinite(native_screen).all() and np.isfinite(ue_screen).all(),
            label + " source/UE projection evidence invalid")

    results = []
    for flip_v in (False, True):
        ue_corners = []
        for vertex in ue_vertices:
            # The prepared source OBJ was written on XZ (x,0,z). Unreal's
            # StaticMeshExporterOBJ writes the same mesh on XY (x,z,0), which
            # is the axis convention used by the sealed resource face proof.
            # RecordModel reads LOD.VertexPosition directly, before export:
            # map its XZ local axes to that proven exported-OBJ XY basis.
            local = vertex["local_cm"]
            require(len(local) == 3, label + " local vertex has wrong dimension")
            position = canonical_position([local[0], local[2], local[1]], pivot)
            uv = vertex["uv"]
            require(len(uv) == 2 and all(math.isfinite(float(value)) for value in uv),
                    label + " UE UV missing/nonfinite")
            v = 1.0 - float(uv[1]) if flip_v else float(uv[1])
            position_key = tuple(round(value, 5) for value in position)
            measured_key = position_key + (round(float(uv[0]), 6), round(v, 6))
            if label == "academy":
                # The sealed Academy budget checks absolute UV <=1e-5. One
                # coordinate falls on opposite sides of round6 (.3568075),
                # despite <1e-7 actual source/LOD error. Snap only to a
                # unique source UV at the exact pixel-lattice position.
                possible = []
                for source_index in source_by_position[position_key]:
                    original_uv = native_uvs[source_index]
                    error = max(abs(float(uv[0]) - float(original_uv["x"])),
                                abs(v - float(original_uv["y"])))
                    if error <= BUDGET["uv_absolute"]:
                        possible.append(source_corners[source_index])
                unique = set(possible)
                if len(unique) == 1:
                    measured_key = next(iter(unique))
            ue_corners.append(measured_key)
        require(all(0 <= index < len(ue_vertices) for face in ue_triangles for index in face),
                label + " UE triangle has invalid vertex index")
        observed = collections.Counter(tuple(sorted(ue_corners[index] for index in face))
                                       for face in ue_triangles)
        unexpected = observed - expected_all
        missing = expected_all - observed
        # Every nonzero source triangle must survive. UE may preserve a subset
        # of exact zero-area triangles; unknown or added faces are forbidden.
        face_match = (not unexpected and not (missing - allowed_missing) and
                      sum(missing.values()) == len(source_faces) - len(ue_triangles))
        results.append((flip_v, face_match, ue_corners, observed, missing, unexpected))
    matches = [entry for entry in results if entry[1]]
    require(len(matches) == 1, label + " source nonzero triangle/UV multiset differs or UV orientation ambiguous")
    flip_v, _, ue_corners, observed, missing, unexpected = matches[0]
    native_index_by_corner = collections.defaultdict(list)
    for index, corner in enumerate(source_corners):
        native_index_by_corner[corner].append(index)
    projection_errors = []
    uv_errors = []
    local_errors = []
    for index, corner in enumerate(ue_corners):
        candidates = native_index_by_corner.get(corner, [])
        require(candidates, label + " UE vertex is not a source position/UV")
        projection_errors.append(min(float(np.linalg.norm(ue_screen[index] - native_screen[j]))
                                     for j in candidates))
        raw_local = ue_vertices[index]["local_cm"]
        actual_local = np.asarray([raw_local[0], raw_local[2], raw_local[1]], dtype=float)
        local_errors.append(float(np.max(np.abs(actual_local - np.asarray(corner[:3])))))
        measured_uv = np.asarray(ue_vertices[index]["uv"], dtype=float)
        if flip_v:
            measured_uv[1] = 1.0 - measured_uv[1]
        uv_errors.append(min(float(np.max(np.abs(measured_uv - np.asarray([
            native_uvs[j]["x"], native_uvs[j]["y"]], dtype=float)))) for j in candidates))
    projection = {"median_px": float(np.median(projection_errors)),
                  "max_px": float(max(projection_errors))}
    require(projection["median_px"] <= BUDGET["projection_median_px"] and
            projection["max_px"] <= BUDGET["projection_max_px"],
            label + " source/UE projected tight mesh exceeds locked 1/2px budget")
    require(max(uv_errors) <= BUDGET["uv_absolute"] and
            max(local_errors) <= BUDGET["local_vertex_cm"],
            label + " source/UE local position or UV exceeds locked Academy precision budget")
    return {"native_vertices": len(native_vertices), "ue_vertices": len(ue_vertices),
            "native_triangles": len(source_faces), "ue_triangles": len(ue_triangles),
            "source_nonzero_faces_proved": approved["source_nonzero_faces_proved"],
            "exact_zero_area_source_faces_omitted": sum(missing.values()),
            "uv_orientation": "ue_v_flipped" if flip_v else "ue_v_direct",
            "ue_local_axis_mapping_to_sealed_obj": "LOD (x,y,z) -> OBJ-exported (x,z,y); source OBJ is (x,0,z)",
            "projection": projection, "local_vertex_max_cm": float(max(local_errors)),
            "uv_max_absolute": float(max(uv_errors)),
            "face_signature": ("unordered corners: source pixel-lattice local position round5 + "
                               + ("unique original source UV within locked Academy 1e-5 absolute budget"
                                  if label == "academy" else "source/UE UV round6 sealed resource precision")
                               + "; exact face multiset")}


def verify_models(manifest: dict, native: dict) -> dict:
    resource_approved = read(RESOURCE_ASSET)
    require(resource_approved["passed"] and
            resource_approved["schema"] == "rok.city_resource_presentation_assets.ue_verify.v1",
            "Previously sealed resource asset proof missing")
    approved_models = {row["model"]: row for row in resource_approved["models"]}
    academy_approved = read(ACADEMY_ASSET)
    require(academy_approved["schema"] == "rok.academy_source_sprite_mesh.ue_import.v1" and
            academy_approved["topology"]["vertex_count_lod0"] == 49 and
            academy_approved["topology"]["triangle_count_lod0"] == 47 and
            sha(Path(academy_approved["source_mesh_manifest"])) ==
            academy_approved["source_mesh_manifest_sha256"],
            "Previously sealed Academy mesh source proof missing")
    require(len(manifest["models"]) == len(native["models"]) == 3,
            "Three full native/UE source models required")
    summary = {}
    for row, source in zip(manifest["models"], native["models"]):
        label = source["label"]
        require(label in MODELS and row["label"] == label and row["source_model"] == MODELS[label][0],
                "Source/UE model order or identity changed")
        model, directory = MODELS[label]
        expected_mesh = f"{directory}/SM_{model}_SourceSpriteMesh.SM_{model}_SourceSpriteMesh"
        expected_material = (f"/Game/RokUnity/City/GammaComposite/{model}/"
                             f"M_{model}_SourceGammaBeforeDOF.M_{model}_SourceGammaBeforeDOF")
        require(row["mesh_asset"] == expected_mesh and row["material_asset"] == expected_material,
                label + " mesh/source Gamma material path differs")
        assets = manifest["asset_sha256"]
        require(assets[label + "_mesh"] == row["mesh_uasset_sha256"] == sha(asset_file(expected_mesh)) and
                assets[label + "_material"] == row["material_uasset_sha256"] == sha(asset_file(expected_material)),
                label + " captured mesh/material asset bytes changed")
        if label == "academy":
            source_geometry = source["geometry"]
            approved = {"source_zero_area_triangle_indices": [],
                        "source_nonzero_faces_proved": len(source_geometry["triangles"]) // 3}
            require(row["vertex_count"] == 49 and row["triangle_count"] == 47 and
                    academy_approved["mesh_asset"] == expected_mesh.split(".")[0],
                    "Academy tight mesh source count changed")
        else:
            pinned = approved_models[model]
            mesh_approval = pinned["imported_mesh"]
            require(row["vertex_count"] == mesh_approval["vertex_count_lod0"] and
                    row["triangle_count"] == mesh_approval["triangle_count_lod0"] and
                    sha(Path(pinned["source_mesh_manifest"])) == pinned["source_mesh_manifest_sha256"] and
                    mesh_approval["source_nonzero_faces_proved"] == pinned["source_nonzero_faces_proved"],
                    label + " imported tight mesh differs from sealed nonzero-face proof")
            approved = pinned
        summary[label] = geometry_faces(label, row, source, approved)
    for key, object_path in {
        "flat_background": "/Game/RokDiagnostics/Academy/M_AcademyAuditBackground",
        "pattern_background": "/Game/RokDiagnostics/CityGammaComposite/M_CityGammaAuditPattern",
        "pattern_texture": "/Game/RokDiagnostics/CityGammaComposite/T_CityGammaPatternSource",
        "exact_output": "/Game/RokDiagnostics/CityGammaComposite/M_CityGammaExactOutput",
    }.items():
        require(manifest["asset_sha256"][key] == sha(asset_file(object_path)),
                "GPU background/output asset changed: " + key)
    return summary


def verify_stats(manifest: dict) -> dict:
    def integral(value: object) -> bool:
        return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value) and value == int(value)

    initial = manifest["compositor_initial"]
    before_reset = manifest["compositor_before_reset"]
    after_reset = manifest["compositor_after_reset"]
    require(all(integral(initial[k]) for k in STAT_FIELDS) and
            initial["registered_targets"] == 1 and initial["pending_gpu_validations"] == 0,
            "Compositor did not start as one idle dedicated target")
    previous = initial
    frame_rows = []
    for frame in manifest["frames"]:
        before, after = frame["compositor_before"], frame["compositor_after"]
        require(all(integral(before[k]) and integral(after[k]) for k in STAT_FIELDS) and
                before == previous and before["registered_targets"] == 1 and
                before["pending_gpu_validations"] == 0 and after["registered_targets"] == 1 and
                after["pending_gpu_validations"] == 0,
                "Compositor frame counter discontinuity/pending GPU validation")
        deltas = {key: after[key] - before[key] for key in STAT_FIELDS}
        require(deltas["before_passes"] == deltas["after_passes"] == 1 and
                deltas["valid_pre_exposure_passes"] == 2 and
                all(deltas[key] == 0 for key in NO_ERROR_STATS),
                "Compositor frame lacks exactly paired Before/After and two valid exposures")
        frame_rows.append({"name": frame["name"], "deltas": deltas})
        previous = after
    require(previous == before_reset and before_reset["registered_targets"] == 1 and
            before_reset["pending_gpu_validations"] == 0 and
            before_reset["before_passes"] - initial["before_passes"] == 8 and
            before_reset["after_passes"] - initial["after_passes"] == 8 and
            before_reset["valid_pre_exposure_passes"] - initial["valid_pre_exposure_passes"] == 16 and
            all(before_reset[key] - initial[key] == 0 for key in NO_ERROR_STATS),
            "Global eight-frame compositor pass counts changed")
    require(after_reset["registered_targets"] == 0 and after_reset["pending_gpu_validations"] == 0 and
            all(after_reset[key] == before_reset[key] for key in STAT_FIELDS
                if key not in ("registered_targets", "pending_gpu_validations")),
            "Compositor did not restore to zero registered targets")
    return {"initial": initial, "frames": frame_rows, "before_reset": before_reset,
            "after_reset": after_reset}


def verify(manifest_path: Path) -> dict:
    manifest_path = manifest_path.resolve()
    run = manifest_path.parent
    require(manifest_path.name == "manifest.json" and run.parent == RUN_ROOT.resolve() and
            run.is_dir(), "--run must be a manifest.json in a direct CityGammaCompositeAudit child")
    manifest, native, native_verify, contract = (read(manifest_path), read(NATIVE_REPORT),
                                                 read(NATIVE_VERIFY), read(CONTRACT))
    require(manifest["schema"] == "rok.city_gamma_composite_runtime.v1" and
            manifest["complete"] and manifest["passed"] and not manifest["failures"],
            "UE eight-frame GPU audit failed or incomplete")
    require(Path(manifest["project"]).resolve() == ROOT and
            manifest["map"] == "/Game/Maps/L_RokUnityGameAppCity" and
            manifest["temporary_state_restored"] and not manifest["map_saved"] and
            not manifest["construction_ledger_touched"] and
            manifest["economy_before"] == manifest["economy_after"] != "" and
            manifest["actor_count_before"] == manifest["actor_count_after_restore"] and
            manifest["temporary_actor_count_before"] == manifest["temporary_actor_count_after_restore"] == 0,
            "Project/map/temporary actors/economy restoration differs")
    require("isolated" in manifest["scope"].lower() and
            "not actual city placement" in manifest["scope"].lower(),
            "Diagnostic scope incorrectly claims actual city acceptance")
    source_inputs(manifest, native, contract, native_verify)
    geometry = verify_models(manifest, native)
    compositor = verify_stats(manifest)
    coverage = {}
    for model in native["models"]:
        label = model["label"]
        path = NATIVE_ROOT / model["coveragePng"]
        require(sha(path) == native_verify["models"][label]["coverage_png_sha256"],
                label + " native postblend coverage PNG changed")
        coverage[label] = png_alpha(path)
    native_images, ue_images = {}, {}
    visual = {}
    background_bindings = {}
    exact_output_binding = None
    output_binding_rows = []
    for index, (source, fixture, frame) in enumerate(zip(native["frames"], contract["fixtures"], manifest["frames"])):
        name = source["name"]
        require(name == fixture["name"] == frame["name"] and
                frame["source_png"] == source["png"] and
                frame["background_kind"] == source["background"] and
                frame["extent"] == list(SIZE) and
                frame["capture_api"] == "SceneCapture FinalColorLDR -> RHI ReadPixels BGRA8 -> PNG" and
                frame["show_only_component_count"] == 4 and
                frame["post_processing"] and frame["post_process_material"] and
                frame["tonemapper_show_flag"] and frame["render_target_force_linear_gamma"] and
                not frame["render_target_srgb"] and close(frame["render_target_display_gamma"], 2.2, .001) and
                close(frame["camera_vertical_fov_degrees"], 7.5, 1e-4),
                name + " GPU capture/source fixture state changed")
        forward = np.asarray(frame["camera_forward_ue"], dtype=float)
        camera_position = np.asarray(frame["camera_position_cm"], dtype=float)
        background_location = np.asarray(frame["background_plane_world_location_cm"], dtype=float)
        horizontal_fov = math.degrees(2 * math.atan(math.tan(math.radians(7.5) / 2) * SIZE[0] / SIZE[1]))
        require(np.allclose(forward, np.asarray([0, .7, -.7]) / math.sqrt(.98), atol=1e-4) and
                np.allclose(camera_position, -forward * 10000.0, atol=.05) and
                np.allclose(background_location, forward * 2000.0, atol=.05) and
                close(frame["camera_horizontal_fov_degrees"], horizontal_fov, .001) and
                all(math.isfinite(float(v)) for v in frame["background_plane_world_scale"]),
                name + " source-camera/background-plane pose differs")
        require(frame["background_material"] and frame["exact_output_material"],
                name + " actual GPU background or exact output material missing")
        require(frame["exact_output_material"] != EXACT_OUTPUT_BASE,
                name + " expected a bound MID instance, not the base asset")
        require(frame.get("capture_always_persist_rendering_state") is True and
                frame.get("capture_view_state_present") is True and
                frame.get("capture_every_frame") is False and
                frame.get("exact_output_base_material") == EXACT_OUTPUT_BASE and
                frame.get("exact_output_bound_blendable_material") == frame["exact_output_material"] and
                frame.get("exact_output_blendable_location") == EXACT_OUTPUT_LOCATION and
                isinstance(frame.get("exact_output_blendable_count"), (int, float)) and
                not isinstance(frame["exact_output_blendable_count"], bool) and
                frame["exact_output_blendable_count"] == 1 and
                isinstance(frame.get("exact_output_blendable_weight"), (int, float)) and
                not isinstance(frame["exact_output_blendable_weight"], bool) and
                close(frame["exact_output_blendable_weight"], 1.0, 1e-6) and
                isinstance(frame.get("capture_post_process_blend_weight"), (int, float)) and
                not isinstance(frame["capture_post_process_blend_weight"], bool) and
                close(frame["capture_post_process_blend_weight"], 1.0, 1e-6),
                name + " persistent SceneCapture view or actual ReplacingTonemapper MID binding missing")
        if exact_output_binding is None:
            exact_output_binding = frame["exact_output_material"]
        require(frame["exact_output_material"] == exact_output_binding,
                name + " exact Gamma output material changed during capture")
        output_binding_rows.append({"name": name,
                                    "persist_rendering_state": frame["capture_always_persist_rendering_state"],
                                    "view_state_present": frame["capture_view_state_present"],
                                    "manual_capture": not frame["capture_every_frame"],
                                    "bound_mid": frame["exact_output_bound_blendable_material"],
                                    "base_material": frame["exact_output_base_material"],
                                    "blendable_location": frame["exact_output_blendable_location"],
                                    "blendable_count": frame["exact_output_blendable_count"],
                                    "blendable_weight": frame["exact_output_blendable_weight"],
                                    "component_blend_weight": frame["capture_post_process_blend_weight"]})
        existing_binding = background_bindings.setdefault(source["background"], frame["background_material"])
        require(existing_binding == frame["background_material"],
                name + " background material changed within same kind")
        source_color = [source["spriteColor"][key] for key in "rgba"]
        require(np.allclose(frame["source_sprite_color_gamma"], source_color, atol=1e-6),
                name + " source sprite lighting changed")
        require(len(frame["layers"]) == len(source["layers"]) == len(fixture["layers"]),
                name + " visible layer count changed")
        for actual, expected in zip(frame["layers"], source["layers"]):
            label = expected["label"]
            require(actual["label"] == label and actual["source_order"] == actual["sort_priority"] == expected["order"] and
                    actual["registered_visible"] and
                    actual["source_material_variant_sha256"] == manifest["asset_sha256"][label + "_material"] and
                    actual["source_material_variant"] == manifest["models"][[m["label"] for m in manifest["models"]].index(label)]["material_asset"],
                    name + " actual source layer order/binding changed")
        ue_path = bounded_path(frame["png"], run, f"{index:02d}_{name}.png")
        raw_path = bounded_path(frame["raw_bgra8"], run, f"{index:02d}_{name}.bgra8")
        require(sha(ue_path) == frame["png_sha256"] and sha(raw_path) == frame["raw_sha256"],
                name + " UE PNG/raw SHA differs from manifest")
        native_images[name] = png_rgb(NATIVE_ROOT / source["png"])
        ue = png_rgb(ue_path)
        raw = np.frombuffer(raw_path.read_bytes(), dtype=np.uint8)
        require(raw.size == SIZE[0] * SIZE[1] * 4,
                name + " raw RHI BGRA8 size differs")
        with Image.open(ue_path) as encoded:
            encoded.load()
            require(encoded.mode == "RGBA", name + " UE PNG did not retain BGRA8 alpha channel")
            encoded_rgba = np.asarray(encoded, dtype=np.uint8)
        require(np.array_equal(raw.reshape(SIZE[1], SIZE[0], 4)[..., [2, 1, 0, 3]], encoded_rgba),
                name + " PNG RGBA differs from same-frame raw RHI BGRA8")
        ue_images[name] = ue

    require(background_bindings["flat"] != background_bindings["pattern"],
            "Flat and pattern GPU background used one material instance")

    native_pattern, ue_pattern = native_images["pattern_only"], ue_images["pattern_only"]
    pattern_stats = color_stats(native_pattern, ue_pattern, np.ones(native_pattern.shape[:2], dtype=bool))
    require(pattern_stats["max"] <= BUDGET["background_max_rgb8"],
            f"Pattern-only opaque GPU background differs >2 RGB8: {pattern_stats}")
    visual["pattern_only"] = {"pure_background": pattern_stats,
                              "source_png_sha256": native_verify["frame_png_sha256"]["pattern_only"]}
    for source in native["frames"]:
        name = source["name"]
        if name == "pattern_only":
            continue
        native_pixels, ue_pixels = native_images[name], ue_images[name]
        labels = [item["label"] for item in source["layers"]]
        require(len(labels) == len(set(labels)) and labels, name + " empty/repeated source model")
        native_base = (native_pattern if source["background"] == "pattern" else
                       np.broadcast_to(native_pixels[0, 0], native_pixels.shape))
        ue_base = (ue_pattern if source["background"] == "pattern" else
                   np.broadcast_to(ue_pixels[0, 0], ue_pixels.shape))
        support = np.logical_or.reduce([coverage[label] > BUDGET["coverage_postblend_alpha_threshold_byte"]
                                        for label in labels])
        raw_support = np.logical_or.reduce([coverage[label] > 0 for label in labels])
        native_delta = np.max(np.abs(native_pixels.astype(np.int16) - native_base.astype(np.int16)), axis=2)
        ue_delta = np.max(np.abs(ue_pixels.astype(np.int16) - ue_base.astype(np.int16)), axis=2)
        # Select pure background from native evidence alone. UE-only artifacts
        # remain selected and must fail the max-2 RGB budget.
        pure_background = ~(dilate(raw_support | (native_delta > 2),
                                  BUDGET["background_exclusion_dilate_px"]))
        require(int(pure_background.sum()) >= BUDGET["background_min_pixels"],
                name + " pure background area was exhausted")
        background_stat = color_stats(native_pixels, ue_pixels, pure_background)
        require(background_stat["max"] <= BUDGET["background_max_rgb8"],
                name + f" source/UE pure background exceeds RGB8 max2: {background_stat}")
        native_mask, ue_mask = native_delta > 4, ue_delta > 4
        source_bbox, ue_bbox = bbox(native_mask), bbox(ue_mask)
        edge = max(abs(a - b) for a, b in zip(source_bbox, ue_bbox))
        union = int(np.count_nonzero(native_mask | ue_mask))
        iou = float(np.count_nonzero(native_mask & ue_mask) / union)
        require(edge <= BUDGET["bbox_max_edge_px"] and iou >= BUDGET["mask_iou_min"],
                f"{name} visible nontransparent mask differs: edge={edge}, IoU={iou:.6f}")
        core = erode(support, BUDGET["core_erode_px"])
        core_stat = color_stats(native_pixels, ue_pixels, core)
        require(core_stat["mae"] <= BUDGET["core_mae_rgb8"] and
                core_stat["p95"] <= BUDGET["core_p95_rgb8"],
                name + f" real-native core RGB exceeds fixed MAE8/P9516: {core_stat}")
        # Boundary is reported separately and cannot be hidden in the core.
        boundary = dilate(raw_support, 1) & ~core
        boundary_stat = color_stats(native_pixels, ue_pixels, boundary)
        row = {"source_png_sha256": native_verify["frame_png_sha256"][name],
               "ue_png_sha256": manifest["frames"][[f["name"] for f in manifest["frames"]].index(name)]["png_sha256"],
               "background": background_stat, "native_bbox_xyxy": source_bbox,
               "ue_bbox_xyxy": ue_bbox, "bbox_max_edge_px": edge, "mask_iou": iou,
               "native_visible_pixels": int(native_mask.sum()), "ue_visible_pixels": int(ue_mask.sum()),
               "source_coverage_union_pixels": int(support.sum()),
               "eroded_core": core_stat, "masked_boundary_diagnostic_only": boundary_stat}
        if name in ("academy_neutral", "academy_day"):
            alpha = coverage["academy"].astype(np.float64) / 255.0
            # Transparent-black RT stores a^2; sqrt yields a quantized support
            # estimate only. Source and UE frame colors remain direct GPU pixels.
            estimate = np.sqrt(alpha)
            translucent = core & (estimate >= .05) & (estimate <= .95)
            semi = color_stats(native_pixels, ue_pixels, translucent)
            require(semi["mae"] <= BUDGET["academy_single_translucent_mae_rgb8"] and
                    semi["p95"] <= BUDGET["academy_single_translucent_p95_rgb8"],
                    name + f" real-native translucent samples exceed retained 12/24 numerical budget: {semi}")
            row["academy_real_native_translucent"] = semi
            row["academy_alpha_note"] = "sqrt(quantized transparent-black postblend alpha) selects samples only; not source opacity measurement or old CPU analytic gate"
        visual[name] = row

    overlap = (coverage["academy"] > BUDGET["coverage_postblend_alpha_threshold_byte"]) & (
        coverage["farm"] > BUDGET["coverage_postblend_alpha_threshold_byte"])
    require(int(overlap.sum()) >= 1000, "Pinned Academy/Farm overlap support absent")
    order = {}
    for background in ("flat", "pattern"):
        a_name = "af_academy_front_" + background
        f_name = "af_farm_front_" + background
        native_delta = native_images[a_name].astype(np.int16) - native_images[f_name].astype(np.int16)
        ue_delta = ue_images[a_name].astype(np.int16) - ue_images[f_name].astype(np.int16)
        native_changed = np.max(np.abs(native_delta), axis=2) > BUDGET["order_changed_pixel_max_channel_threshold_rgb8"]
        ue_changed = np.max(np.abs(ue_delta), axis=2) > BUDGET["order_changed_pixel_max_channel_threshold_rgb8"]
        native_count, ue_count = int((native_changed & overlap).sum()), int((ue_changed & overlap).sum())
        require(native_count >= BUDGET["order_native_changed_overlap_pixels_min"] and
                ue_count >= BUDGET["order_ue_changed_overlap_pixels_min"],
                background + " source/UE sorting did not affect >=100 overlapping pixels")
        valid = overlap[:, :, None] & (
            np.abs(native_delta) >= BUDGET["order_valid_native_delta_channel_min_rgb8"])
        require(int(valid.sum()) >= 100, background + " no valid native ordering-delta channels")
        sign = float(np.mean(np.sign(native_delta[valid]) == np.sign(ue_delta[valid])))
        difference = stats_values(native_delta[valid] - ue_delta[valid])
        require(sign >= BUDGET["order_sign_agreement_min"] and
                difference["mae"] <= BUDGET["order_delta_difference_mae_rgb8"] and
                difference["p95"] <= BUDGET["order_delta_difference_p95_rgb8"],
                background + f" UE ordering delta direction/magnitude differs from native: sign={sign}, diff={difference}")
        order[background] = {"overlap_pixels": int(overlap.sum()),
                             "native_changed_overlap_pixels": native_count,
                             "ue_changed_overlap_pixels": ue_count,
                             "valid_native_delta_channels": int(valid.sum()),
                             "delta_sign_agreement": sign, "delta_difference_rgb8": difference}
    return {"schema": "rok.city_gamma_composite_runtime_verify.v1", "passed": True,
            "qualification": "Isolated artificial Unity Gamma ordered-overlap diagnostic only; not actual GameApp city placement, constructed buildings, or product visual acceptance.",
            "manifest_sha256": sha(manifest_path), "manifest": str(manifest_path),
            "verifier_sha256": sha(Path(__file__)),
            "predeclared_budget": BUDGET, "source_hashes": NATIVE_PINNED_SHA,
            "geometry": geometry, "compositor": compositor,
            "capture_output_binding": output_binding_rows,
            "frames": visual, "order_delta": order}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", required=True, type=Path,
                        help="Exact Saved/RokUnityMigration/CityGammaCompositeAudit/<UTC_PID>/manifest.json")
    args = parser.parse_args()
    run_path = args.run.resolve()
    if run_path.name != "manifest.json" or run_path.parent.parent != RUN_ROOT.resolve() or not run_path.is_file():
        parser.error("--run must point to an existing direct audit-run manifest.json")
    try:
        result = verify(run_path)
    except Exception as exc:
        result = {"schema": "rok.city_gamma_composite_runtime_verify.v1", "passed": False,
                  "manifest": str(run_path), "manifest_sha256": sha(run_path),
                  "verifier_sha256": sha(Path(__file__)),
                  "predeclared_budget": BUDGET, "error": f"{type(exc).__name__}: {exc}"}
    target = run_path.parent / "runtime_verify.json"
    with target.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(("CITY_GAMMA_COMPOSITE_RUNTIME_PASS" if result["passed"] else
           "CITY_GAMMA_COMPOSITE_RUNTIME_FAIL") + " " + str(target))
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
