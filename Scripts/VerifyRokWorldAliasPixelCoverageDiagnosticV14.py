"""Strict V14 ground-sort tree pixel proof, delegating V13's full native gates.

V13 is immutable. Only its run/schema, derived data identity and new Content
package are adapted; A1/A2/B/A3 PNG, fence, projection, alias and HTTP checks
remain the exact reviewed V13 functions.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path("D:/ueDemo/Rok")
HERE = ROOT / "Saved/PendingWorldAliasPixelCoverageDiagnosticV6"
READY = HERE / "ready_for_root_review.json"
OLD_STAGE = ROOT / "Saved/PendingWorldAliasPixelCoverageDiagnosticV4"
OLD_READY = OLD_STAGE / "ready_for_root_review.json"
OLD_READY_SHA = "14515da3e036ab311250c0b14c67d2e68bf5e20fc399ce0208ba66d725947da9"
OLD_VERIFIER = OLD_STAGE / "VerifyRokWorldAliasPixelCoverageDiagnosticV13.py"
OLD_VERIFIER_SHA = "1b5ff9b8588732b175966989aa6f91bbd5bd8691a466c90dec68dfc54e9c07e9"
OLD_REVIEW = ROOT / "Saved/PendingWorldAliasPixelCoverageDiagnosticIndependentReviewV4/review.json"
OLD_REVIEW_SHA = "3555f01ee5866d351a1817562158ae6645ee6f9304dd91bdbfaeaf6c80288507"
BLOCKED_V5_READY = ROOT / "Saved/PendingWorldAliasPixelCoverageDiagnosticV5/ready_for_root_review.json"
BLOCKED_V5_READY_SHA = "95f7ef85d751ef9cf8996ccdf3a604ea09abb3dfac3623fd7b9ccae71748efc2"
BLOCKED_V5_REVIEW = ROOT / "Saved/PendingWorldAliasPixelCoverageDiagnosticIndependentReviewV5/review.json"
BLOCKED_V5_REVIEW_SHA = "74140882bc5125c559139c7ec03647b3bba809ef604c024f4fba8e20edc2f8eb"
OLD_DATA = ROOT / "Saved/RokUnityMigration/unity_world_map_streaming_data.json"
OLD_DATA_SHA = "72dd593850c582b4358a1f86f347391ea135f6f3edc25a50da7222daac2c3ba2"
NEW_DATA = HERE / "unity_world_map_streaming_data_priority_v5.json"
NEW_DATA_SHA = "4650cc3ea5209f11a5220d04d409fb0a9dabffeff6f96d370fb102df51a4c473"
NEW_PAYLOAD = "31862a9e4847eb2856fd1c1801ecc6dfd5b6b77673cb2cc6315fb6c85d2ea1ad"
OLD_PAYLOAD = "09298c671a05b3c220e24b25c7c7362c4d7f0cff1218a29d7c305e7471f86b6f"
NEW_WORLD = "/Game/Maps/L_RokWorldStreamingDiagnosticGroundSortV2"
NEW_MAP = ROOT / "Content/Maps/L_RokWorldStreamingDiagnosticGroundSortV2.umap"
NEW_ASSET_OBJECT = ("/Game/RokDiagnostics/WorldStreaming/V2/"
                    "DA_RokWorldStreamGroundSortLod01V2.DA_RokWorldStreamGroundSortLod01V2")
NEW_ASSET = ROOT / ("Content/RokDiagnostics/WorldStreaming/V2/"
                    "DA_RokWorldStreamGroundSortLod01V2.uasset")
CREATE_SCHEMA = "rok.world_ground_sort_diagnostic_editor_create.v5"
RUN_FOLDER = "WorldAliasPixelCoverageDiagnosticRunsV14"
SCHEMA = "rok.world_alias_pixel_coverage_diagnostic_runtime.v14"


def require(value: bool, reason: str) -> None:
    if not value:
        raise AssertionError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pinned_json(path: Path, digest: str) -> dict:
    require(path.is_file() and not path.is_symlink() and sha(path) == digest,
            "frozen JSON pin drift: " + str(path))
    result = json.loads(path.read_text(encoding="utf-8"))
    require(type(result) is dict, "frozen JSON is not an object")
    return result


def load_v13():
    require(sha(OLD_VERIFIER) == OLD_VERIFIER_SHA,
            "V13 verifier changed")
    spec = importlib.util.spec_from_file_location("frozen_product4_v13", OLD_VERIFIER)
    require(spec is not None and spec.loader is not None, "cannot load V13")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def verify_stage(prior, *, runtime: bool) -> dict:
    previous = pinned_json(OLD_READY, OLD_READY_SHA)
    review = pinned_json(OLD_REVIEW, OLD_REVIEW_SHA)
    blocked = pinned_json(BLOCKED_V5_READY, BLOCKED_V5_READY_SHA)
    blocked_review = pinned_json(BLOCKED_V5_REVIEW, BLOCKED_V5_REVIEW_SHA)
    require(blocked["new_payload_sha256"] == NEW_PAYLOAD
            and blocked_review["reviewed_ready_sha256"] == BLOCKED_V5_READY_SHA
            and blocked_review["blocking_static_findings"],
            "blocked Product5 history or review changed")
    require(previous["native_coverage_schema"] ==
            "rok.world_alias_pixel_coverage_diagnostic_runtime.v13"
            and review["review_status"] == "STATIC_PASS_RUNTIME_PENDING"
            and review["reviewed_ready_sha256"] == OLD_READY_SHA
            and review["blocking_static_findings"] == [],
            "prior Product4/review identity changed")
    for row in previous["stage_files"] + previous["read_only_source_pins"]:
        path = ROOT / row["path"]
        if runtime and row["path"] == "Source/Rok/RokWorldMapStreamingActor.cpp":
            continue
        require(path.is_file() and path.stat().st_size == row["bytes"]
                and sha(path) == row["sha256"], "prior Product4 pin drift: " + row["path"])
    require(sha(OLD_DATA) == OLD_DATA_SHA and sha(NEW_DATA) == NEW_DATA_SHA,
            "old or derived data bytes drift")
    ready = json.loads(READY.read_text(encoding="utf-8"))
    require(ready["schema"] == "rok.world_alias_pixel_coverage_diagnostic_stage.v14"
            and ready["status"] == "READY_FOR_INDEPENDENT_STATIC_REVIEW_ONLY"
            and ready["revision_of_product4_ready_sha256"] == OLD_READY_SHA
            and ready["product4_review_sha256"] == OLD_REVIEW_SHA
            and ready["revises_blocked_product5_ready_sha256"] == BLOCKED_V5_READY_SHA
            and ready["product5_blocking_review_sha256"] == BLOCKED_V5_REVIEW_SHA
            and ready["native_schema"] == SCHEMA
            and ready["coverage_run_root"].endswith(RUN_FOLDER)
            and ready["coverage_world"] == NEW_WORLD
            and ready["coverage_asset"] == NEW_ASSET_OBJECT
            and ready["new_payload_sha256"] == NEW_PAYLOAD
            and ready["unflagged_world"] == prior.base.WORLD
            and ready["unflagged_payload_sha256"] == OLD_PAYLOAD
            and ready["unflagged_five_phase_contract"] == [848, 1688, 1688, 0, 1688]
            and ready["coverage_rgb_delta_threshold"] == 16
            and ready["coverage_min_connected_pixels"] == 64
            and ready["noise_max_rgb"] == 2
            and ready["alias_count"] == 24
            and ready["lod01_dependencies"] == 243
            and ready["lod2_missing_dependencies"] == 249
            and ready["changed_ground_prefabs"] == 20
            and ready["snow_transition_mirrored_parts_per_lod"] == [69, 69, 69]
            and ready["ue_build_or_runtime_passed"] is False
            and ready["formal_map_lod_accepted"] is False
            and ready["full_migration_accepted"] is False,
            "Product6 stage scope or numeric gates changed")
    staged_names = {Path(row["path"]).name for row in ready["stage_files"]}
    require(len(ready["stage_files"]) == len(staged_names) == 19
            and {"CompileRokUnityWorldMapStreamingData.py",
                 "RokWorldInstancedMaterialDiagnosticGameMode.h",
                 "RokWorldInstancedMaterialDiagnosticGameMode.cpp",
                 "RokWorldMapStreamingActor.cpp",
                 "RokWorldMapRuntimeDataAsset.cpp",
                 "RokWorldStreamingSourceMirrorTests.cpp",
                 "CreateRokWorldGroundSortDiagnosticV5.py",
                 "VerifyRokWorldAliasPixelCoverageDiagnosticV14.py",
                 "unity_world_map_streaming_data_priority_v5.json",
                 "unity_world_map_streaming_data_priority_v5_compile.json",
                 "derived_payload_comparison.json", "minimal_source_diff.patch",
                 "root_acceptance_recipe.json", "implementation_report.md"} <= staged_names,
            "required Product6 stage files missing or duplicated")
    source_paths = {row["path"] for row in ready["read_only_source_pins"]}
    require(len(source_paths) == len(ready["read_only_source_pins"]) == 16
            and {"Source/Rok/RokWorldMapCameraPawn.h",
                 "Source/Rok/RokWorldMapCameraPawn.cpp",
                 "Source/Rok/RokCityViewTypes.cpp",
                 "Source/Rok/RokWorldMapStreamingActor.h",
                 "Source/Rok/RokWorldMapStreamingActor.cpp",
                 "ref/config-tables/编译客户端配置/CSV/CameraParamLod.csv",
                 "Saved/RokUnityMigration/ConfigTables/CameraParamLod.json"} <= source_paths,
            "source camera/streaming capture pin set changed")
    require({"Source/Rok/RokWorldMapRuntimeDataAsset.cpp",
                 "Source/Rok/Tests/RokWorldStreamingSourceMirrorTests.cpp"} <= source_paths,
            "runtime data validation or native test source pins missing")
    require(len(ready["generation_source_pins"]) == 8
            and len({row["path"] for row in ready["generation_source_pins"]}) == 8,
            "generation input source set changed")
    for row in (ready["stage_files"] + ready["read_only_source_pins"]
                + ready["generation_source_pins"]):
        path = ROOT / row["path"]
        if not runtime and row["path"] in {
                "Source/Rok/RokWorldMapStreamingActor.cpp",
                "Source/Rok/RokWorldMapRuntimeDataAsset.cpp",
                "Source/Rok/Tests/RokWorldStreamingSourceMirrorTests.cpp"}:
            path = HERE / Path(row["path"]).name
        require(path.is_file() and path.stat().st_size == row["bytes"]
                and sha(path) == row["sha256"], "Product6 byte pin drift: " + row["path"])
    if not runtime:
        row = ready["pre_integration_compiler_pin"]
        path = ROOT / row["path"]
        require(path.is_file() and path.stat().st_size == row["bytes"]
                and sha(path) == row["sha256"], "preintegration compiler drift")
        row = ready["pre_integration_streaming_cpp_pin"]
        path = ROOT / row["path"]
        require(path.is_file() and path.stat().st_size == row["bytes"]
                and sha(path) == row["sha256"], "preintegration streaming actor drift")
        for field in ("pre_integration_runtime_data_cpp_pin",
                      "pre_integration_source_mirror_test_cpp_pin"):
            row = ready[field]
            path = ROOT / row["path"]
            require(path.is_file() and path.stat().st_size == row["bytes"]
                    and sha(path) == row["sha256"], field + " drift")
    require(ready["derived_payload_sha256"] == NEW_DATA_SHA
            and ready["old_v13_failed_manifest_sha256"] ==
                "50349fef2643b4c0ad2c2b498b3d2f3be09775ed52c95b5566f40d5cb488c4cd",
            "derived payload or historical failure pin drift")
    failed = ROOT / ready["old_v13_failed_manifest_path"]
    require(sha(failed) == ready["old_v13_failed_manifest_sha256"],
            "historical zero-pixel native manifest changed")
    if runtime:
        pairs = (("RokWorldInstancedMaterialDiagnosticGameMode.h",
                  "Source/Rok/RokWorldInstancedMaterialDiagnosticGameMode.h"),
                 ("RokWorldInstancedMaterialDiagnosticGameMode.cpp",
                  "Source/Rok/RokWorldInstancedMaterialDiagnosticGameMode.cpp"),
                 ("CompileRokUnityWorldMapStreamingData.py",
                  "Scripts/CompileRokUnityWorldMapStreamingData.py"),
                 ("RokWorldMapStreamingActor.cpp",
                  "Source/Rok/RokWorldMapStreamingActor.cpp"),
                 ("RokWorldMapRuntimeDataAsset.cpp",
                  "Source/Rok/RokWorldMapRuntimeDataAsset.cpp"),
                 ("RokWorldStreamingSourceMirrorTests.cpp",
                  "Source/Rok/Tests/RokWorldStreamingSourceMirrorTests.cpp"),
                 ("CreateRokWorldGroundSortDiagnosticV5.py",
                  "Scripts/CreateRokWorldGroundSortDiagnosticV5.py"),
                 ("VerifyRokWorldAliasPixelCoverageDiagnosticV14.py",
                  "Scripts/VerifyRokWorldAliasPixelCoverageDiagnosticV14.py"))
        for staged, live in pairs:
            require(sha(HERE / staged) == sha(ROOT / live),
                    "live implementation differs from staged Product6: " + live)
    return ready


def verify_derived_data() -> dict:
    old = json.loads(OLD_DATA.read_text(encoding="utf-8"))
    new = json.loads(NEW_DATA.read_text(encoding="utf-8"))
    require(old["CanonicalPayloadSha256"] == OLD_PAYLOAD
            and new["CanonicalPayloadSha256"] == NEW_PAYLOAD,
            "old/new canonical hashes differ")
    replacement = copy.deepcopy(old)
    replacement["CanonicalPayloadSha256"] = NEW_PAYLOAD
    old_by_recipe = {row["RecipeId"]: row for row in replacement["Recipes"]}
    new_by_recipe = {row["RecipeId"]: row for row in new["Recipes"]}
    require(len(old_by_recipe) == len(new_by_recipe) == 396
            and set(old_by_recipe) == set(new_by_recipe),
            "recipe identity/count drift")
    changes = []
    for recipe_id, old_recipe in old_by_recipe.items():
        new_recipe = new_by_recipe[recipe_id]
        require(len(old_recipe["Parts"]) == len(new_recipe["Parts"]),
                "part count drift")
        for index, (old_part, new_part) in enumerate(zip(
                old_recipe["Parts"], new_recipe["Parts"])):
            if old_part == new_part:
                continue
            require(old_recipe["Lod"] in (0, 1)
                    and old_part["TranslucencySortPriority"] == 0
                    and new_part["TranslucencySortPriority"] == -5
                    and "M_WorldGround_" in old_part["Materials"][0],
                    "non-ground, wrong-LOD or nonpriority data drift")
            old_part["TranslucencySortPriority"] = -5
            require(old_part == new_part, "ground part changed beyond priority")
            changes.append((recipe_id, index))
    require(len(changes) == 1188 and len({x[0] for x in changes}) == 132
            and replacement == new,
            "derived payload changed beyond 1188 ground priorities")
    comparison = json.loads((HERE / "derived_payload_comparison.json")
                            .read_text(encoding="utf-8"))
    require(comparison["only_ground_priority_changed"] is True
            and comparison["changed_part_count"] == 1188
            and comparison["changed_recipe_count"] == 132
            and {(x["recipe_id"], x["part_index"]) for x in comparison["changes"]}
                == set(changes), "source-trace comparison drift")
    return {"old_payload_sha256": OLD_PAYLOAD, "new_payload_sha256": NEW_PAYLOAD,
            "changed_ground_parts": 1188, "changed_recipes": 132}


def verify_creation_receipt(path: Path) -> dict:
    receipt = json.loads(path.read_text(encoding="utf-8"))
    require(receipt["schema"] == CREATE_SCHEMA
            and receipt["status"] == "created_pending_runtime_acceptance"
            and receipt["runtime_data_asset"] == NEW_ASSET_OBJECT.split(".", 1)[0]
            and receipt["diagnostic_map"] == NEW_WORLD
            and receipt["source_json_sha256"] == NEW_DATA_SHA
            and receipt["source_map_sha256_before"] ==
                "151a43e787a7b08bf54522fdc8940c41f7e7b6f7614b932c5acb2c43fcd93b79"
            and receipt["source_asset_sha256_before"] ==
                "eb8181300f29c4253bbb9d7917f3c8ce5017da829aa0c640e10f455c8e9792d4"
            and receipt["old_content_bytes_unchanged"] is True
            and receipt["added_content_paths"] == [
                "Maps/L_RokWorldStreamingDiagnosticGroundSortV2.umap",
                "RokDiagnostics/WorldStreaming/V2/DA_RokWorldStreamGroundSortLod01V2.uasset"]
            and receipt["lod01_dependency_count"] == 243
            and receipt["lod2_missing_dependency_count"] == 249
            and receipt["static_visual_ready"] is False
            and receipt["runtime_ready"] is False,
            "new Content creation receipt did not prove exactly two new assets")
    require(NEW_ASSET.is_file() and NEW_MAP.is_file()
            and sha(NEW_ASSET) == receipt["data_asset_uasset_sha256"]
            and sha(NEW_MAP) == receipt["diagnostic_map_umap_sha256"],
            "new Content changed after creation receipt")
    return {"creation_receipt_sha256": sha(path),
            "new_asset_sha256": sha(NEW_ASSET), "new_map_sha256": sha(NEW_MAP)}


def verify_ground_sort_samples(report: dict, ground_sources: set[str]) -> dict:
    totals = []
    for index, sample in enumerate(report["samples"]):
        ground_count = tree_count = 0
        for item in sample["actual_hism_components"]:
            aliases = item["material_aliases"]
            sources = {alias["source_object"] for alias in aliases}
            if sources & ground_sources:
                require(len(aliases) == 1 and sources <= ground_sources
                        and item["mesh"].endswith("SM_RokWorld_Plane60.SM_RokWorld_Plane60")
                        and item["translucency_sort_priority"] == -5
                        and item["material_diagnostic_0"]["blend_mode"] == 2,
                        f"phase {index}: source ground did not reach translucent HISM priority -5")
                ground_count += 1
            if item["name"] == "WorldMapChunk_16_2_Batch_193_Gen_0":
                require(item["translucency_sort_priority"] == 0
                        and item["material_diagnostic_0"]["blend_mode"] == 2,
                        f"phase {index}: target tree sort/material drift")
                tree_count += 1
        require(ground_count >= 4 and tree_count == 1,
                f"phase {index}: expected ground/tree HISM evidence missing")
        totals.append(ground_count)
    return {"ground_hism_counts": totals, "target_tree_priority": 0,
            "ground_priority": -5}


def configure_coverage(prior, new_data: dict) -> None:
    base = prior.base
    base.PINS = {NEW_DATA if path == base.SOURCE else path: digest
                 for path, digest in base.PINS.items()}
    base.PINS[NEW_DATA] = NEW_DATA_SHA
    base.SOURCE = NEW_DATA
    base.PAYLOAD_SHA = NEW_PAYLOAD
    base.WORLD = NEW_WORLD
    base.ASSET_PATH = NEW_ASSET_OBJECT
    prior.HERE = HERE
    prior.READY = READY
    prior.RUN_FOLDER = RUN_FOLDER
    prior.SCHEMA = SCHEMA
    prior.PIN_FIELDS["candidate_verifier_sha256"] = (
        "VerifyRokWorldAliasPixelCoverageDiagnosticV14.py")
    require(new_data["CanonicalPayloadSha256"] == NEW_PAYLOAD,
            "coverage source payload not derived")


def negative_cases(prior, ground_sources: set[str]) -> list[str]:
    rejected = []
    exemplar = next(iter(ground_sources))
    ground = {"name": "Ground", "mesh":
              "/Game/RokUnityWorldMapPrototype/Meshes/SM_RokWorld_Plane60.SM_RokWorld_Plane60",
              "material_aliases": [{"source_object": exemplar}],
              "translucency_sort_priority": -5,
              "material_diagnostic_0": {"blend_mode": 2}}
    tree = {"name": prior.TARGET_COMPONENT, "mesh": prior.TARGET_MESH,
            "material_aliases": [{"source_object": prior.TARGET_SOURCE}],
            "translucency_sort_priority": 0,
            "material_diagnostic_0": {"blend_mode": 2}}
    valid_items = [copy.deepcopy(ground) for _ in range(4)] + [tree]
    require(verify_ground_sort_samples({"samples": [
        {"actual_hism_components": valid_items}] * 4}, ground_sources)
        ["ground_hism_counts"] == [4] * 4,
        "valid ground/tree priority fixture did not pass")
    for label, priority, blend, source in (
            ("ground_kept_zero", 0, 2, exemplar),
            ("ground_wrong_minus_four", -4, 2, exemplar),
            ("ground_opaque", -5, 0, exemplar),
            ("target_tree_negative", -5, 2, prior.TARGET_SOURCE)):
        items = copy.deepcopy(valid_items)
        if label == "target_tree_negative":
            items[-1]["translucency_sort_priority"] = -5
        else:
            items[0]["material_aliases"][0]["source_object"] = source
            items[0]["translucency_sort_priority"] = priority
            items[0]["material_diagnostic_0"]["blend_mode"] = blend
        try:
            verify_ground_sort_samples({"samples": [
                {"actual_hism_components": items}] * 4}, ground_sources)
        except AssertionError:
            rejected.append(label)
        else:
            raise AssertionError("ground-sort negative accepted: " + label)
    return rejected


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-runtime", type=Path)
    parser.add_argument("--runtime", type=Path)
    parser.add_argument("--creation-receipt", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    require(args.runtime is None or
            (args.baseline_runtime is not None and args.creation_receipt is not None),
            "V14 runtime needs fresh V10 baseline and new Content creation receipt")
    prior = load_v13()
    ready = verify_stage(prior, runtime=args.runtime is not None)
    result = {"schema": "rok.world_alias_pixel_coverage_offline_verify.v14",
              "stage_sha256": sha(READY), "data": verify_derived_data()}
    prior.verify_camera_source_chain()
    alias, by_source = prior.base.validate_alias_evidence(
        prior.base.ALIAS_PATH, prior.base.ACTUAL_ALIAS_SHA)
    require(len(by_source) == 24 and by_source[prior.TARGET_SOURCE]["target_object"]
            == prior.TARGET_ALIAS, "24-alias chain drift")
    old_source = json.loads(OLD_DATA.read_text(encoding="utf-8"))
    prior.base.verify_source()
    prior.verify_source_target(old_source)
    if args.baseline_runtime:
        prior.base.verify_runtime(
            json.loads(args.baseline_runtime.read_text(encoding="utf-8")),
            old_source, args.baseline_runtime.resolve(), alias, by_source,
            prior.base.ACTUAL_ALIAS_SHA)
        result["unflagged_five_phase_manifest_sha256"] = sha(args.baseline_runtime)
    new_source = json.loads(NEW_DATA.read_text(encoding="utf-8"))
    configure_coverage(prior, new_source)
    prior.base.verify_source()
    result["source_target"] = prior.verify_source_target(new_source)
    assets = json.loads((ROOT / "Saved/RokUnityMigration/unity_world_map_assets.json")
                        .read_text(encoding="utf-8"))
    ground_sources = {("/Game/RokUnityWorldMapPrototype/Materials/M_WorldGround_"
                       + item["prefab_id"] + ".M_WorldGround_" + item["prefab_id"])
                      for item in assets["baked_ground_materials"]}
    if args.runtime:
        result["creation"] = verify_creation_receipt(args.creation_receipt)
        report = json.loads(args.runtime.read_text(encoding="utf-8"))
        result["coverage"] = prior.verify_runtime(args.runtime.resolve(), new_source,
                                                  alias, by_source, ready)
        result["ground_sort"] = verify_ground_sort_samples(report, ground_sources)
    if args.self_test:
        result["v13_negative_cases_rejected"] = prior.negative_cases()
        result["ground_sort_negative_cases_rejected"] = negative_cases(prior, ground_sources)
        old_failure = ROOT / ready["old_v13_failed_manifest_path"]
        try:
            prior.verify_runtime(old_failure, new_source, alias, by_source, ready)
        except AssertionError:
            result["old_failed_v13_manifest_rejected"] = True
        else:
            raise AssertionError("historical failed V13 manifest accepted under V14")
    result["passed"] = True
    print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
