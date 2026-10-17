"""Build a read-only, fail-closed source catalog for the GameApp Gamma city layer.

This is an input inventory, never a renderer registration or an accepted draw packet.
Run with UE 5.8's bundled Python; ``--verify RUN_DIR`` rechecks a generated run.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
import uuid
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from rok_data_paths import resolve
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SAVED = ROOT / "Saved" / "RokUnityMigration"
OUTPUT_ROOT = SAVED / "CityGammaSourceClosure"
BASELINE = SAVED / "CityGammaLayerCensus" / "20261008_215152_26496_01" / "manifest.json"
POST = SAVED / "CityGammaLayerCensus" / "20261008_215218_26496_02" / "manifest.json"
CROSSWALK = SAVED / "BatchReview20261009Construction" / "gamma_source_layer_crosswalk.json"
SCENE = SAVED / "unity_runtime_city_all_buildings_scene_manifest.json"
BUILDINGS = SAVED / "unity_runtime_city_buildings_obj_manifest.json"
SUPPORT = SAVED / "unity_runtime_city_support_layers.json"
FLAGS = SAVED / "unity_runtime_city_gate_flags.json"
GROUND = SAVED / "unity_runtime_ground_obj_compensated_manifest.json"
GROUND_ALPHA = SAVED / "unity_runtime_ground_vertex_alpha.json"
RESOURCE_SOURCES = SAVED / "CityResourceBuildingVisualSources" / "catalog.json"
RESOURCE_NATIVE = SAVED / "CityResourceVisualNative" / "verified_capture.json"
RESOURCE_ASSETS = SAVED / "CityResourcePresentationAssets"
RASTER_HEADER = ROOT / "Source" / "RokRendering" / "Public" / "RokCityGammaRaster.h"
RASTER_CPP = ROOT / "Source" / "RokRendering" / "Private" / "RokCityGammaRaster.cpp"
SCHEMA = "rok.city_gamma.source_closure_catalog.v1"
EXPECTED_CATEGORIES = {"source_ground": 2, "city_support_layer": 2,
                       "city_gate_flag": 2, "initial_source_building": 24}
EXPECTED_QUEUES = {2991: 1, 2992: 2, 2993: 1, 2995: 27, 2996: 1}
DYNAMIC_MODELS = {"build_2_101_1": (1, 2, 133, 163, (512, 512), (256, 256)),
                  "build_3_101_1": (2, 3, 43, 41, (512, 459), (256, 230))}


class SourceError(RuntimeError):
    pass


def require(test: bool, message: str) -> None:
    if not test:
        raise SourceError(message)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path: Path) -> dict[str, Any]:
    require(path.is_file(), f"missing input: {path}")
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    require(isinstance(value, dict), f"JSON root is not an object: {path}")
    return value


def save(path: Path, value: dict[str, Any]) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def tag(tags: list[str], prefix: str) -> str | None:
    matches = [item[len(prefix):] for item in tags if item.startswith(prefix)]
    require(len(matches) <= 1, f"ambiguous tag {prefix}: {tags}")
    return matches[0] if matches else None


def project_path(value: str | None) -> Path | None:
    if not value or value in {"builtin", "unknown"}:
        return None
    path = Path(value)
    if path.is_absolute():
        return path
    normalized = value.replace("\\", "/")
    if normalized.startswith("Assets/"):
        return ROOT / "ref" / "client-unity" / normalized
    return resolve(normalized)


def source_file(value: str | None, role: str, expected_sha: str | None = None) -> dict[str, Any]:
    path = project_path(value)
    result = {"role": role, "source_path": value, "resolved_path": str(path) if path else None,
              "exists": bool(path and path.is_file()), "sha256": None, "expected_sha256": expected_sha}
    if result["exists"]:
        result["sha256"] = sha256(path)
        if expected_sha:
            require(result["sha256"].lower() == expected_sha.lower(),
                    f"source hash differs for {role}: {path}")
    return result


def obj_counts(path: Path) -> tuple[int, int, int]:
    vertices = uvs = faces = 0
    with path.open("r", encoding="utf-8-sig", errors="replace") as stream:
        for line in stream:
            if line.startswith("v "):
                vertices += 1
            elif line.startswith("vt "):
                uvs += 1
            elif line.startswith("f "):
                points = line.split()[1:]
                require(len(points) >= 3, f"invalid OBJ face in {path}")
                faces += len(points) - 2
    return vertices, uvs, faces


def source_geometry(path_text: str | None, expected_sha: str | None,
                    expected_vertices: int | None, expected_triangles: int | None) -> dict[str, Any]:
    item = source_file(path_text, "source_obj", expected_sha)
    item["vertices"] = item["uvs"] = item["triangles"] = None
    if item["exists"]:
        v, uv, tri = obj_counts(Path(item["resolved_path"]))
        item.update(vertices=v, uvs=uv, triangles=tri)
        if expected_vertices is not None:
            require(v == expected_vertices, f"OBJ vertex count differs: {path_text}: {v} != {expected_vertices}")
        if expected_triangles is not None:
            require(tri == expected_triangles, f"OBJ triangle count differs: {path_text}: {tri} != {expected_triangles}")
    return item


def file_ids(record: dict[str, Any]) -> dict[str, int]:
    result = {key: value for key, value in record.items() if key.endswith("_file_id")}
    require(all(type(value) is int for value in result.values()), "Unity fileId was not parsed as an exact integer")
    return result


def ensure_census(before: dict[str, Any], after: dict[str, Any]) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    for label, data in (("baseline", before), ("post", after)):
        require(data.get("schema") == "rok.city_gamma_layer_census.v1", f"{label} census schema")
        require(data.get("map") == "/Game/Maps/L_RokUnityGameAppCity", f"{label} map")
        require(data.get("project", "").replace("\\", "/").casefold() == str(ROOT).replace("\\", "/").casefold() + "/",
                f"{label} project")
        require(data.get("current_promoted_source_actor_contract_matched") is True, f"{label} source actor contract")
        summary = data["summary"]
        require(summary["tagged_city_source_actors"] == 42 and summary["unique_initial_ids"] == 24 and
                summary["unique_interior_preview_ids"] == 0 and summary["invalid_or_overlapping_source_ids"] == 0,
                f"{label} source identity")
    require(before["input_sha256"]["Content/Maps/L_RokUnityGameAppCity.umap"] ==
            after["input_sha256"]["Content/Maps/L_RokUnityGameAppCity.umap"], "map input changed between censuses")
    old = {row["component"]: row for row in before["components"]}
    new = {row["component"]: row for row in after["components"]}
    require(len(old) == len(before["components"]) and len(new) == len(after["components"]), "duplicate component path")
    require(set(old) <= set(new), "preexisting component disappeared")
    added = [new[key] for key in sorted(set(new) - set(old))]
    require(len(added) == 4 and sum(row["effective_visible_attach_chain"] for row in added) == 2,
            "expected exactly two new source meshes and two hidden billboards")
    require(all(row["category"] == "local_new_construction" for row in added), "unexpected new component class")
    visible = [row for row in added if row["effective_visible_attach_chain"]]
    hidden = [row for row in added if not row["effective_visible_attach_chain"]]
    require(all(row["component_class"] == "/Script/Engine.StaticMeshComponent" for row in visible),
            "dynamic visible components are not static meshes")
    require(all(row["component_class"] == "/Script/Engine.MaterialBillboardComponent" and
                row["component"].endswith(".LegacyCitySpriteBillboard") for row in hidden),
            "legacy billboard may double-render")
    require({row["actor"] for row in visible} == {row["actor"] for row in hidden}, "billboard actor association")
    original_categories = set(EXPECTED_CATEGORIES)
    static_before = {row["component"]: row for row in before["components"] if
                     row["category"] in original_categories and row["effective_visible_attach_chain"]}
    static_after = {row["component"]: row for row in after["components"] if
                    row["category"] in original_categories and row["effective_visible_attach_chain"]}
    require(len(static_before) == len(static_after) == 30 and static_before.keys() == static_after.keys(),
            "the original 30 source components changed identity")
    require(Counter(row["category"] for row in static_after.values()) == EXPECTED_CATEGORIES,
            "source category counts differ")
    for key, row in static_after.items():
        prior = static_before[key]
        require(row["translucency_sort_priority"] == prior["translucency_sort_priority"] and
                row["mesh_asset"] == prior["mesh_asset"] and row["material_bindings"] == prior["material_bindings"] and
                row["effective_visible_attach_chain"], f"source identity/material/queue changed: {key}")
    return list(static_after.values()), visible


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify", type=Path, help="recheck a generated run without writing")
    parser.add_argument("--require-ready", action="store_true", help="also fail if raster input gaps remain")
    args = parser.parse_args()
    if args.verify:
        return verify(args.verify, args.require_ready)

    inputs = {"generator": Path(__file__), "baseline_census": BASELINE, "post_census": POST,
              "crosswalk": CROSSWALK, "scene": SCENE, "buildings": BUILDINGS, "support": SUPPORT,
              "flags": FLAGS, "ground": GROUND, "ground_alpha": GROUND_ALPHA,
              "resource_sources": RESOURCE_SOURCES, "resource_native": RESOURCE_NATIVE,
              "raster_header": RASTER_HEADER, "raster_cpp": RASTER_CPP}
    input_hashes = {key: sha256(path) for key, path in inputs.items()}
    before, after, crosswalk = load(BASELINE), load(POST), load(CROSSWALK)
    scene, buildings, support, flags = load(SCENE), load(BUILDINGS), load(SUPPORT), load(FLAGS)
    resource_catalog, resource_native = load(RESOURCE_SOURCES), load(RESOURCE_NATIVE)
    require(crosswalk["schema"] == "rok.city_gamma.source_layer_crosswalk.v1" and crosswalk["count"] == 30,
            "crosswalk schema or count")
    require(crosswalk["evidence_sha256"]["census"] == sha256(Path(crosswalk["baseline_census"])),
            "crosswalk source census hash differs")
    require(scene.get("target_level") == "/Game/Maps/L_RokUnityGameAppCity" and
            scene.get("buildings_spawned") == 24 and scene.get("extra_interior_buildings_spawned") == 0,
            "scene manifest does not describe promoted GameApp")
    require(resource_catalog.get("schema") == "rok.city_resource_building_visual_sources.v1" and
            resource_native.get("schema") == "rok.city_resource_visual_native_verify.v1" and
            resource_native.get("passed") is True and
            resource_native.get("source_catalog_sha256") == input_hashes["resource_sources"],
            "resource native source chain changed")
    raster_source = RASTER_CPP.read_text(encoding="utf-8-sig")
    require(all(name in raster_source for name in ("build_13_101_2", "build_2_101_1", "build_3_101_1", "PF_B8G8R8A8")),
            "raster allowlist or texture gate changed; review packet contract")
    old_static, new_dynamic = ensure_census(before, after)
    by_path = {row["component"]: row for row in old_static}
    crossed = {row["component_path"]: row for row in crosswalk["components"]}
    require(set(by_path) == set(crossed), "crosswalk does not identify the Run4 original 30")
    result_rows: list[dict[str, Any]] = []
    gaps: list[dict[str, str]] = []
    queue = Counter()
    assets = after["textures"]
    for path, observed in sorted(by_path.items()):
        entry = crossed[path]
        require(entry["category"] == observed["category"] and
                entry["source_queue"] == observed["translucency_sort_priority"] and
                entry["ue_visible_snapshot"]["mesh_asset"] == observed["mesh_asset"],
                f"source crosswalk identity differs: {path}")
        rows = static_source(entry, observed, scene, buildings, support, flags, after)
        result_rows.append(rows)
        queue[rows["source_queue"]] += 1
        gaps.extend({"source_key": rows["source_key"], "code": code} for code in rows["blocking_gaps"])
    models_seen: set[str] = set()
    for observed in sorted(new_dynamic, key=lambda row: row["component"]):
        row = dynamic_source(observed, after, resource_catalog, resource_native, assets)
        require(row["model_id"] not in models_seen, "duplicate dynamic model")
        models_seen.add(row["model_id"])
        result_rows.append(row)
        queue[row["source_queue"]] += 1
        gaps.extend({"source_key": row["source_key"], "code": code} for code in row["blocking_gaps"])
    require(models_seen == set(DYNAMIC_MODELS), "Farm/Wood dynamic mapping incomplete")
    require(dict(queue) == EXPECTED_QUEUES, f"source queue cardinality differs: {queue}")
    require(len(result_rows) == 32 and len({row["source_key"] for row in result_rows}) == 32,
            "closure row count or identity collision")
    # Shared blockers are intentionally not assigned an invented per-queue source order.
    gaps.extend([{"source_key": "entire_city_layer", "code": "native_equal_queue_order_and_depth_unverified"},
                 {"source_key": "entire_city_layer", "code": "atomic_main_view_suppression_and_restore_unimplemented"},
                 {"source_key": "entire_city_layer", "code": "normal_GameApp_gpu_pixel_comparison_missing"}])
    run = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + f"_{os.getpid()}_{uuid.uuid4().hex[:8]}"
    directory = OUTPUT_ROOT / run
    directory.mkdir(parents=True, exist_ok=False)
    catalog = {
        "schema": SCHEMA, "run": run,
        "status": "blocked_source_inputs_not_a_draw_packet",
        "ready_for_raster": False,
        "scope": "Promoted GameApp initial 30 source translucent components plus two formal local Farm/Wood; no UnityInitId invented and no rendering registration",
        "project": str(ROOT), "map": "/Game/Maps/L_RokUnityGameAppCity",
        "map_sha256": before["input_sha256"]["Content/Maps/L_RokUnityGameAppCity.umap"],
        "input_paths": {key: str(path.relative_to(ROOT)) for key, path in inputs.items()},
        "input_sha256": input_hashes,
        "baseline_component_set_sha256": before["component_set_sha256"],
        "post_component_set_sha256": after["component_set_sha256"],
        "counts": {"source_ground": 2, "city_support_layer": 2, "city_gate_flag": 2,
                   "initial_source_building": 24, "dynamic_local_new": 2, "visible_total": 32,
                   "hidden_legacy_billboards_excluded": 2},
        "queue_counts": {str(k): queue[k] for k in sorted(queue)},
        "rows": sorted(result_rows, key=lambda row: (row["source_queue"], row["source_key"])),
        "blocking_gaps": gaps,
        "packet_contract": {
            "api": "FRokCityGammaRasterInstance",
            "source": "Source/RokRendering/Public/RokCityGammaRaster.h",
            "identity": "ModelId + nonzero unique InstanceId; original 30 models currently rejected by the three-model allowlist",
            "geometry": "Vertices/UV/Indices must be a validated local-centimetre value snapshot; OBJ file presence alone does not satisfy approved geometry SHA1",
            "textures": "strong RHI SpriteTexture/MaskTexture references, current gate PF_B8G8R8A8, one mip/sample, SRGB=false; source-file hashes and asset identity verified on game thread",
            "sorting": "SortPriority from source queue, SourceOrder from verified native equal-queue order; no guessed SourceOrder values are exported",
            "transform": "LocalToWorld must be re-read for each live source component/LOD and each dynamic constructed actor; catalog observed transforms are evidence only",
            "other": "RendererTint/Team1/Team2/Lighting/Transparency require source shader and runtime environment proof per material family",
            "draw": "No packet emitted while any blocking gap remains; old 30 ordinary draws stay enabled"
        },
        "notes": ["Existing baked PNGs are derivatives and are never substituted for unapproved raw Gamma sprite/mask sources.",
                  "Original 30 source components and two local constructed actors have distinct identity namespaces.",
                  "This catalog does not infer UE GPU visibility from primitive attach-chain visibility."]
    }
    save(directory / "catalog.json", catalog)
    verification = {"schema": "rok.city_gamma.source_closure_verify.v1", "catalog_sha256": sha256(directory / "catalog.json"),
                    "catalog_structurally_valid": True, "ready_for_raster": False,
                    "row_count": 32, "blocking_gap_count": len(gaps),
                    "status": "catalog_generated_and_verified_but_raster_registration_blocked"}
    save(directory / "verify.json", verification)
    print(f"{directory}: 32 source-traced rows; {len(gaps)} blocking gaps; raster input disabled")
    return 2 if args.require_ready else 0


def static_source(entry: dict[str, Any], observed: dict[str, Any], scene: dict[str, Any],
                  buildings: dict[str, Any], support: dict[str, Any], flags: dict[str, Any],
                  after: dict[str, Any]) -> dict[str, Any]:
    category, key = entry["category"], entry["source_id"]
    if category == "initial_source_building":
        scene_record = next((x for x in scene["building_actors"] if x["init_id"] == key), None)
        record = next((x for x in buildings["records"] if x["init_id"] == key), None)
        require(scene_record and record and scene_record["model_id"] == record["model_id"], f"building source join {key}")
        require(tag(observed["tags"], "UnityInitId_") == str(key), f"building tag differs {key}")
        obj = source_geometry(record["obj_path"], None,
                              record["sprite_vertex_count"], record["sprite_triangle_count"])
        sources = [source_file(scene_record["prefab_path"], "prefab"),
                   source_file(scene_record["sprite_path"], "raw_sprite"),
                   source_file(scene_record["mask_path"], "raw_mask"),
                   source_file(scene_record["material_path"], "material"),
                   source_file(scene_record["shader_asset_path"], "shader"), obj]
        model = record["model_id"]
        gaps = ["raster_model_not_allowlisted", "original_UE_plane_not_approved_sprite_mesh",
                "raw_gamma_rhi_sprite_mask_not_imported_or_verified", "native_equal_queue_order_unverified",
                "source_renderer_file_id_untraced"]
        night = scene_record["shader_name"] == "custom/map/cm_city_mask_night"
        require(night or scene_record["shader_name"] == "custom/map/map_building_with_mask-5",
                f"unreviewed initial building shader: {key}")
        if night:
            gaps.append("night_shader_mask_b_light_color_unimplemented")
        source_semantics = {"shader": scene_record["shader_name"],
                            "mask": "mask.r alpha, mask.g recolor, and mask.b light color" if night else
                                    "mask.r alpha and mask.g recolor; source mask sampled before baking",
                            "baked_ue_texture_derivative": scene_record["baked_texture_path"],
                            "source_sprite_vertices": record["sprite_vertex_count"],
                            "source_sprite_triangles": record["sprite_triangle_count"]}
        local_transform = {"root_local_position": scene_record["root_local_position"],
                           "root_local_scale": scene_record["root_local_scale"],
                           "renderer_local_position": scene_record["renderer_local_position"],
                           "renderer_local_euler": scene_record["renderer_local_euler"],
                           "renderer_local_scale": scene_record["renderer_local_scale"]}
        source_key = f"initial:{key}"
        unity_file_ids = {}
    elif category == "source_ground":
        record = next((x for x in scene["ground_actors"] if x["unity_object"] == key), None)
        require(record and tag(observed["tags"], "UnityGroundObject_") == key, f"ground source join {key}")
        model = record["source_mesh_stem"]
        obj = source_geometry(record["obj_path"], record["source_obj_sha256"],
                              record["vertex_count"], record["triangle_count"])
        sources = [source_file(scene["source_files"]["CityWallPrefab"], "prefab"),
                   source_file(record["source_mesh"], "source_fbx", record["source_fbx_sha256"]),
                   source_file(record["unity_material_path"], "material"),
                   source_file(record["unity_shader_asset_path"], "shader"), obj]
        sources += [source_file(value, role) for role, value in sorted(record["unity_texture_properties"].items())]
        gaps = ["raster_model_not_allowlisted", "ground_multi_texture_shader_not_in_raster",
                "source_raw_gamma_rhi_textures_missing", "ground_submesh_slot_mapping_unverified",
                "source_renderer_file_id_untraced"]
        source_semantics = {"shader": record["unity_shader_name"],
                            "sampled_texture_properties": record["unity_texture_properties"],
                            "vertex_alpha": record["unity_vertex_alpha_evidence"].get("output_png_sha256"),
                            "baked_ue_texture_derivative": record["ue_texture_source"]}
        local_transform = {"source_geometry_world_baked": True, "ue_actor_location": record["ue_location"],
                           "ue_actor_rotation": record["ue_rotation"], "ue_actor_scale": record["ue_scale"]}
        source_key = f"ground:{key}"
        unity_file_ids = file_ids(record)
    elif category == "city_support_layer":
        record = next((x for x in support["records"] if x["layer_id"] == key), None)
        require(record and tag(observed["tags"], "UnityLayerId_") == key, f"support source join {key}")
        model = key
        obj = source_geometry(record["obj_project_path"], record["obj_sha256"],
                              record["vertex_count"], record["triangle_count"])
        sources = [source_file(record["source_prefab_unity_path"], "prefab", record["source_prefab_sha256"]),
                   source_file(record["material_unity_path"], "material", record["material_sha256"]),
                   source_file(record["shader_unity_path"], "shader",
                               record["shader_sha256"] if record["shader_sha256"] != "builtin" else None),
                   source_file(record["source_texture_unity_path"], "raw_texture", record["source_texture_sha256"]), obj]
        gaps = ["raster_model_not_allowlisted", "support_shader_family_not_in_raster",
                "source_raw_gamma_rhi_textures_missing"]
        source_semantics = {"shader": record["shader_unity_path"],
                            "alpha": record["source_alpha_policy"], "rgb": record["source_rgb_policy"],
                            "builtin_shader": record["shader_sha256"] == "builtin"}
        local_transform = {"local_position": record["local_position"],
                           "local_rotation_quaternion": record["local_rotation_quaternion"],
                           "local_scale": record["local_scale"], "runtime_world_matrix": record["runtime_world_matrix"]}
        source_key = f"support:{key}"
        unity_file_ids = file_ids(record)
    elif category == "city_gate_flag":
        record = next((x for x in flags["records"] if x["flag_id"] == key), None)
        require(record and tag(observed["tags"], "UnityFlagId_") == key, f"flag source join {key}")
        model = key
        obj = source_geometry(record["obj_project_path"], record["obj_sha256"],
                              record["vertex_count"], record["triangle_count"])
        sources = [source_file(record["source_prefab_unity_path"], "prefab", record["source_prefab_sha256"]),
                   source_file(record["sprite_unity_path"], "raw_sprite", record["sprite_sha256"]),
                   source_file(record["mask_unity_path"], "raw_mask", record["mask_sha256"]),
                   source_file(record["material_unity_path"], "material", record["material_sha256"]),
                   source_file(record["shader_unity_path"], "shader", record["shader_sha256"]), obj]
        gaps = ["raster_model_not_allowlisted", "runtime_flag_mask_and_buildGreen_shader_not_in_raster",
                "source_raw_gamma_rhi_sprite_mask_missing"]
        source_semantics = {"shader": record["shader_name"], "rgb": record["shader_rgb_formula"],
                            "alpha": record["shader_alpha_formula"],
                            "baked_ue_texture_derivative": record["baked_texture_project_path"],
                            "source_sprite_vertices": record["vertex_count"],
                            "source_sprite_triangles": record["triangle_count"]}
        local_transform = {"local_position": record["local_position"],
                           "local_rotation_quaternion": record["local_rotation_quaternion"],
                           "local_scale": record["local_scale"], "runtime_world_matrix": record["runtime_world_matrix"]}
        source_key = f"flag:{key}"
        unity_file_ids = file_ids(record)
        require(record["sprite_renderer_file_id"] > 2**53,
                f"flag renderer fileId no longer exercises exact 64-bit JSON: {key}")
    else:
        raise SourceError(f"unknown static category: {category}")
    require(entry["source_queue"] == observed["translucency_sort_priority"], f"source queue differs: {source_key}")
    if not obj["exists"]:
        gaps.append("source_obj_unavailable")
    for item in sources:
        if not item["exists"]:
            gaps.append(f"source_file_unavailable:{item['role']}")
    return {"source_key": source_key, "category": category, "model_id": model,
            "actor_path": observed["actor"], "component_path": observed["component"],
            "source_prefab": entry["source_prefab"], "source_renderer_path": entry["source_renderer_path"],
            "source_queue": entry["source_queue"], "source_order": None,
            "source_sorting_layer": entry["source_sorting_layer"],
            "source_sorting_order": entry["source_sorting_order"],
            "source_material_path": entry["source_material_path"],
            "source_semantics": source_semantics, "source_transform": local_transform,
            "unity_file_ids_exact": unity_file_ids,
            "source_files": sources, "source_geometry": obj,
            "observed_ue_mesh": observed["mesh_asset"],
            "observed_ue_materials": observed["material_bindings"],
            "observed_ue_world_transform_evidence_only": observed["world_transform"],
            "runtime_transform_policy": "read_live_component_each_snapshot_not_catalog_baseline",
            "blocking_gaps": sorted(set(gaps))}


def dynamic_source(observed: dict[str, Any], after: dict[str, Any],
                   source_catalog: dict[str, Any], source_native: dict[str, Any],
                   textures: dict[str, Any]) -> dict[str, Any]:
    model = tag(observed["tags"], "RokLocalSourceModel_")
    require(model in DYNAMIC_MODELS, f"unexpected dynamic model: {model}")
    expected_local_id, expected_type, expected_vertices, expected_triangles, sprite_size, mask_size = DYNAMIC_MODELS[model]
    require(tag(observed["tags"], "UnityBuildingType_") == str(expected_type) and
            tag(observed["tags"], "RokLocalBuildingId_") == str(expected_local_id) and
            "RokLocalConstructedBuilding" in observed["tags"] and
            tag(observed["tags"], "UnityInitId_") is None and
            observed["translucency_sort_priority"] == 2995,
            f"dynamic identity or queue differs: {model}")
    resource_run = Path(source_native["run_directory"]).name
    asset_manifest_path = RESOURCE_ASSETS / resource_run / model / "source_mesh_manifest.json"
    asset_manifest = load(asset_manifest_path)
    require(asset_manifest["schema"] == "rok.city_resource_source_sprite_mesh.v1" and
            asset_manifest["model"] == model and
            asset_manifest["source_catalog_sha256"] == sha256(RESOURCE_SOURCES) and
            asset_manifest["native_verified_capture_sha256"] == sha256(RESOURCE_NATIVE),
            f"dynamic source mesh chain differs: {model}")
    mesh = after["meshes"].get(observed["mesh_asset"])
    require(mesh and mesh["lod0_vertices"] == expected_vertices and
            mesh["lod0_triangles"] == expected_triangles and mesh["lod0_uv_channels"] == 1,
            f"dynamic UE mesh topology differs: {model}")
    obj = source_geometry(str(asset_manifest_path.parent / asset_manifest["obj_file"]),
                          asset_manifest["obj_sha256"], asset_manifest["vertex_count"],
                          asset_manifest["source_triangle_count"])
    catalog_sources = source_catalog["sources"]
    model_rows = [item for item in source_catalog["models"] if item["model_id"] == model]
    require(len(model_rows) == 1, f"dynamic prefab model ambiguity: {model}")
    prefab = model_rows[0]["prefab"]
    sprite_nodes = [node for node in prefab["nodes"] if node["name"] == "sprite" and node["renderer"]]
    root_nodes = [node for node in prefab["nodes"] if node["file_id"] == prefab["root_game_object_file_id"]]
    require(len(sprite_nodes) == len(root_nodes) == 1 and prefab["path"] in catalog_sources,
            f"dynamic prefab renderer binding ambiguous: {model}")
    sprite_node, root_node = sprite_nodes[0], root_nodes[0]
    renderer = sprite_node["renderer"]
    require(renderer["enabled"] and renderer["sorting_layer_id"] == 0 and
            renderer["sorting_order"] == 0 and sprite_node["active"],
            f"dynamic source renderer state differs: {model}")
    sprite_matches = [(path, digest) for path, digest in catalog_sources.items() if path.endswith(f"/{model}.png")]
    mask_matches = [(path, digest) for path, digest in catalog_sources.items() if path.endswith(f"/{model}_mask.png")]
    require(len(sprite_matches) == len(mask_matches) == 1, f"dynamic raw source ambiguity: {model}")
    material_path = source_catalog["material"]["path"]
    shader_path = source_catalog["material"]["shader_path"]
    require(material_path in catalog_sources and shader_path in catalog_sources,
            f"dynamic shader/material source unpinned: {model}")
    sources = [source_file(prefab["path"], "prefab", catalog_sources[prefab["path"]]),
               source_file(sprite_matches[0][0], "raw_sprite", sprite_matches[0][1]),
               source_file(mask_matches[0][0], "raw_mask", mask_matches[0][1]),
               source_file(material_path, "material", catalog_sources[material_path]),
               source_file(shader_path, "shader", catalog_sources[shader_path]), obj]
    material_bindings = observed["material_bindings"]
    require(len(material_bindings) == 1 and material_bindings[0]["slot"] == 0,
            f"dynamic material slot differs: {model}")
    material = after["materials"].get(material_bindings[0]["interface"])
    require(material and material["effective_blend_mode"] == 2 and
            material["base_translucency_pass"] == 1 and not material["base_disable_depth_test"],
            f"dynamic source material pass differs: {model}")
    roles = {}
    for role, suffix, size in (("sprite", "SpriteRaw", sprite_size), ("mask", "MaskUnityGpu256", mask_size)):
        matches = [textures[path] for path in material["used_texture_assets"] if path.endswith(f"_{suffix}.T_{model}_{suffix}")]
        require(len(matches) == 1, f"dynamic {role} binding ambiguity: {model}")
        tex = matches[0]
        require((tex["rhi_width"], tex["rhi_height"]) == size and tex["rhi_format"] == 2 and
                tex["rhi_mips"] == tex["rhi_samples"] == 1 and tex["rhi_srgb"] is False,
                f"dynamic {role} RHI descriptor differs: {model}")
        roles[role] = {"asset": tex["asset"], "asset_sha256": tex["asset_file_sha256"],
                       "size": size, "format_enum": 2, "srgb": False, "mips": 1, "samples": 1}
    gaps = ["native_equal_queue_order_unverified", "live_dynamic_transform_must_be_resampled"]
    require(all(item["exists"] for item in sources), f"dynamic raw source or OBJ unavailable: {model}")
    return {"source_key": f"local:{tag(observed['tags'], 'RokLocalBuildingId_')}:{model}",
            "category": "dynamic_local_new", "model_id": model,
            "actor_path": observed["actor"], "component_path": observed["component"],
            "source_prefab": prefab["path"], "source_renderer_path": f"{model}/sprite",
            "source_material_path": material_path,
            "source_sorting_layer": renderer["sorting_layer"],
            "source_sorting_order": renderer["sorting_order"],
            "source_queue": 2995, "source_order": None,
            "source_transform": {"root": root_node["transform"], "sprite": sprite_node["transform"]},
            "unity_file_ids_exact": {"root_game_object_file_id": root_node["file_id"],
                                     "root_transform_file_id": root_node["transform"]["file_id"],
                                     "sprite_game_object_file_id": sprite_node["file_id"],
                                     "sprite_transform_file_id": sprite_node["transform"]["file_id"],
                                     "sprite_renderer_file_id": renderer["file_id"],
                                     "mask_sprite_component_file_id": sprite_node["mask_sprite"]["file_id"]},
            "source_geometry": obj, "source_files": sources,
            "source_mesh_manifest": str(asset_manifest_path.relative_to(ROOT)),
            "source_mesh_manifest_sha256": sha256(asset_manifest_path),
            "source_sprite_vertices": asset_manifest["vertex_count"],
            "source_sprite_triangles_including_degenerate": asset_manifest["source_triangle_count"],
            "approved_ue_nonzero_triangles": expected_triangles,
            "observed_ue_geometry_sha256": {
                "position_f32_xyz": mesh["position_f32_xyz_sha256"],
                "uv_f32_per_vertex_channel": mesh["uv_f32_per_vertex_channel_sha256"],
                "index_u32": mesh["index_u32_sha256"]},
            "source_semantics": source_catalog["material"]["shader_formula"],
            "rhi_texture_descriptors": roles,
            "observed_ue_mesh": observed["mesh_asset"],
            "observed_ue_material": material_bindings[0]["interface"],
            "observed_ue_world_transform_evidence_only": observed["world_transform"],
            "runtime_transform_policy": "read_live_component_each_snapshot_at_current_camera_lod",
            "blocking_gaps": gaps}


def verify(directory: Path, require_ready: bool) -> int:
    directory = directory.resolve()
    require(directory.parent == OUTPUT_ROOT.resolve() and directory.is_dir(), "verify target is not one closure run")
    catalog, report = load(directory / "catalog.json"), load(directory / "verify.json")
    require(catalog.get("schema") == SCHEMA and report.get("schema") == "rok.city_gamma.source_closure_verify.v1",
            "closure schema differs")
    require(report["catalog_sha256"] == sha256(directory / "catalog.json"), "catalog changed after generation")
    for key, relative in catalog["input_paths"].items():
        require(sha256(resolve(relative)) == catalog["input_sha256"][key], f"input hash changed: {relative}")
    rows = catalog["rows"]
    require(len(rows) == 32 and len({row["source_key"] for row in rows}) == 32,
            "row identity or count differs")
    require(Counter(row["category"] for row in rows) == EXPECTED_CATEGORIES | {"dynamic_local_new": 2},
            "closure categories differ")
    require(Counter(row["source_queue"] for row in rows) == EXPECTED_QUEUES, "queue counts differ")
    require(not catalog["ready_for_raster"] and len(catalog["blocking_gaps"]) == report["blocking_gap_count"],
            "readiness gate was altered")
    source_records = {
        "city_support_layer": {row["layer_id"]: row for row in load(SUPPORT)["records"]},
        "city_gate_flag": {row["flag_id"]: row for row in load(FLAGS)["records"]},
    }
    dynamic_models = {row["model_id"]: row for row in load(RESOURCE_SOURCES)["models"]}
    for row in rows:
        if row["category"] == "dynamic_local_new":
            require(sha256(resolve(row["source_mesh_manifest"])) == row["source_mesh_manifest_sha256"],
                    f"source mesh manifest changed: {row['source_key']}")
            prefab = dynamic_models[row["model_id"]]["prefab"]
            root_node = next(node for node in prefab["nodes"] if node["file_id"] == prefab["root_game_object_file_id"])
            sprite_node = next(node for node in prefab["nodes"] if node["name"] == "sprite" and node["renderer"])
            expected_ids = {"root_game_object_file_id": root_node["file_id"],
                            "root_transform_file_id": root_node["transform"]["file_id"],
                            "sprite_game_object_file_id": sprite_node["file_id"],
                            "sprite_transform_file_id": sprite_node["transform"]["file_id"],
                            "sprite_renderer_file_id": sprite_node["renderer"]["file_id"],
                            "mask_sprite_component_file_id": sprite_node["mask_sprite"]["file_id"]}
            require(row["unity_file_ids_exact"] == expected_ids and
                    all(type(value) is int for value in expected_ids.values()),
                    f"dynamic prefab fileId differs: {row['source_key']}")
        if row["category"] in source_records:
            key = row["source_key"].split(":", 1)[1]
            require(row["unity_file_ids_exact"] == file_ids(source_records[row["category"]][key]),
                    f"Unity fileId differs: {row['source_key']}")
        elif row["category"] == "source_ground":
            require(row["unity_file_ids_exact"] == {}, "ground manifest unexpectedly gained fileIds")
        for item in row["source_files"]:
            path = Path(item["resolved_path"]) if item["resolved_path"] else None
            require(item["exists"] == bool(path and path.is_file()), f"source presence changed: {row['source_key']}")
            if item["exists"]:
                require(item["sha256"] == sha256(path), f"source hash changed: {path}")
    print(f"verified {directory}: 32 rows, {len(catalog['blocking_gaps'])} blockers, ready_for_raster=false")
    return 2 if require_ready else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, KeyError, ValueError, TypeError, SourceError) as exc:
        print(f"City Gamma source closure refused: {exc}", file=sys.stderr)
        sys.exit(1)
