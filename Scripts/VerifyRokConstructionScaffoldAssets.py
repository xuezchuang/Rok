"""Read-only source, prepared-input and saved UE asset evidence verification."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import BuildRokConstructionScaffoldAnimationData as animation
import PrepareRokConstructionScaffoldAssets as source

MANIFEST = source.OUT / "prepared_manifest.json"
ANIMATION = source.OUT / "animation_data.json"
IMPORT = source.OUT / "ue_import_report.json"
RESUME_FAILURE = (source.ROOT / "Saved/RokUnityMigration/BatchReview20261009ScaffoldAndFamilyState"
                  / "scaffold_root_import_20261008_234037_377056.json")
RESUME_FAILURE_SHA = "cbab362964603d74c68317f0a9be2ba27cf05d4fd9c9212758d92f508edb6f16"
PRIOR_CHECKPOINT = (source.ROOT / "Saved/RokUnityMigration"
                    / "parallel_migration_batch_20261008_233452_912766_construction_visual.json")
PRIOR_CHECKPOINT_SHA = "bee2b7e4b5b0ce6b439a4a992232b33c2d039495b0460fe0c9ee2b67a7068e5d"


def verify_prepared():
    native, contract = source.identity()
    manifest = source.read_json(MANIFEST)
    clips = source.read_json(ANIMATION)
    if (manifest["schema"] != "rok.construction_scaffold.assets_prepared.v1"
            or manifest["native_sha256"] != source.NATIVE_SHA
            or manifest["contract_sha256"] != source.CONTRACT_SHA
            or manifest["plan_sha256"] != source.sha(source.PLAN)
            or manifest["ue_root"] != source.PACKAGE
            or len(manifest["layers"]) != 8
            or len({row["name"] for row in manifest["layers"]}) != 8):
        raise RuntimeError("Prepared scaffold manifest identity/count differs")
    native_by_node = {x["node"]:x for x in native["layers"]}
    for row in manifest["layers"]:
        layer = native_by_node.pop(row["node"])
        name = Path(layer["sprite"]["source_path"]).stem
        geom = {k:layer[k] for k in ("sprite_rect","texture_rect","pivot",
                                      "pixels_per_unit","vertices","uv","triangles")}
        if (row["name"] != name or row["geometry"] != geom or
                row["geometry_sha256"] != source.canonical_sha(geom) or
                row["vertex_count"] != len(geom["vertices"]) or
                row["triangle_count"]*3 != len(geom["triangles"])):
            raise RuntimeError(f"Source scaffold geometry changed: {name}")
        obj = source.OUT / row["obj_file"]
        if (source.sha(obj) != row["obj_sha256"] or
                obj.read_text(encoding="utf-8") != source.obj_text(layer,f"SM_{name}_SourceSpriteMesh")):
            raise RuntimeError(f"Source scaffold OBJ topology/UV changed: {name}")
        for role,item in (("sprite",layer["sprite"]),("mask",layer["mask_sprite"])):
            origin,meta = source.checked_source(item,21300000)
            entry = row["sources"][role]
            copy = source.OUT / entry["prepared_file"]
            if (entry["guid"] != item["guid"] or entry["file_id"] != 21300000 or
                    entry["sha256"] != source.sha(origin) or
                    entry["meta_sha256"] != source.sha(meta) or
                    not copy.is_file() or source.sha(copy) != entry["sha256"] or
                    entry["ue_asset"] != f"{source.PACKAGE}/T_{name}_{role.capitalize()}Raw"):
                raise RuntimeError(f"Scaffold raw source/prepared copy differs: {name}/{role}")
        if row["ue_mesh_asset"] != f"{source.PACKAGE}/SM_{name}_SourceSpriteMesh":
            raise RuntimeError(f"Scaffold UE mesh target differs: {name}")
    if native_by_node:
        raise RuntimeError("Prepared scaffold manifest omitted original nodes")
    if (clips["schema"] != "rok.construction_scaffold.animation_data.v1" or
            clips["native_sha256"] != source.NATIVE_SHA or
            clips["contract_sha256"] != source.CONTRACT_SHA or
            (clips["clip_count"],clips["binding_count"],clips["key_count"]) != (4,15,60)):
        raise RuntimeError("Scaffold animation data identity/count differs")
    native_clips = {c["name"]:c for c in native["clips"]}
    for clip in clips["clips"]:
        original = native_clips.pop(clip["name"])
        path,meta=source.checked_source(original["asset"],7400000)
        if (clip["source_sha256"] != source.sha(path) or
                clip["source_meta_sha256"] != source.sha(meta) or
                clip["source_guid"] != original["asset"]["guid"] or
                clip["source_file_id"] != 7400000):
            raise RuntimeError(f"Scaffold source AnimationClip identity differs: {clip['name']}")
        for key in ("asset","name","wrap_mode","legacy","loop_time","length","frame_rate"):
            if clip[key] != original[key]:
                raise RuntimeError(f"Scaffold source AnimationClip {key} differs")
        by_property = {c["property"]:c for c in original["curves"]}
        if len(clip["curves"]) != len(by_property):
            raise RuntimeError("Scaffold curve count differs")
        for curve in clip["curves"]:
            src = by_property.pop(curve["property"])
            if any(curve[k] != src[k] for k in ("path","type","property","pre_wrap","post_wrap","keys")):
                raise RuntimeError("Scaffold exact authored curve differs")
            expect=[{"time":(a["time"]+b["time"])/2,
                     "value":animation.evaluate(src["keys"],(a["time"]+b["time"])/2)}
                    for a,b in zip(src["keys"],src["keys"][1:])]
            if curve["interpolation"] != "unweighted_cubic_Hermite" or curve["midpoint_samples"] != expect:
                raise RuntimeError("Scaffold Hermite sample differs")
        if by_property:
            raise RuntimeError("Scaffold animation binding omitted")
    if native_clips:
        raise RuntimeError("Scaffold source clip omitted")
    return manifest,clips


def verify_resume_proof(manifest, proof_path, check_prior=True):
    """Authorize only the two exact Texture2D files from the failed first import."""
    path=Path(proof_path).resolve()
    if path != RESUME_FAILURE.resolve() or source.sha(path) != RESUME_FAILURE_SHA:
        raise RuntimeError("Scaffold resume proof path/hash is not the sealed first failure")
    proof=source.read_json(path)
    first=manifest["layers"][0]
    expected={first["sources"][role]["ue_asset"]:role for role in ("sprite","mask")}
    expected_files={str((source.ROOT/"Content"/(asset.removeprefix("/Game/")+".uasset")).resolve()):asset
                    for asset in expected}
    observed=proof.get("observed_assets")
    if (proof.get("schema") != "rok.scaffold.root_import.v1" or proof.get("passed") is not False
            or proof.get("project") != str(source.ROOT.resolve())
            or proof.get("world") != "L_RokUnityGameAppCity"
            or proof.get("script_sha256") != "e41c90669f517d73a32081dffe0fda281360b61a9cb5b0cbbc07386f73f18030"
            or proof.get("guarded_assets_and_map") != 59
            or proof.get("changed_prior") != []
            or "StaticMeshEditorSubsystem unavailable" not in proof.get("error", "")
            or not isinstance(observed,list) or len(observed) != 2
            or {x.get("path") for x in observed} != set(expected_files)):
        raise RuntimeError("Scaffold resume proof does not describe the sealed two-texture failure")
    by_asset={expected_files[x["path"]]:x["sha256"] for x in observed}
    for asset,sha in by_asset.items():
        disk=source.ROOT/"Content"/(asset.removeprefix("/Game/")+".uasset")
        if not disk.is_file() or source.sha(disk) != sha:
            raise RuntimeError(f"Scaffold resumed texture changed since failed import: {asset}")
    if check_prior:
        if source.sha(PRIOR_CHECKPOINT) != PRIOR_CHECKPOINT_SHA:
            raise RuntimeError("Sealed pre-import asset checkpoint changed")
        prior=source.read_json(PRIOR_CHECKPOINT)
        snapshots=prior["asset_snapshot"]
        if len(snapshots) != 58:
            raise RuntimeError("Sealed prior asset count differs")
        for entry in snapshots:
            file=Path(entry["path"])
            if not file.is_file() or source.sha(file) != entry["sha256"]:
                raise RuntimeError(f"Previously accepted UE asset changed: {file}")
        map_file=source.ROOT/"Content/Maps/L_RokUnityGameAppCity.umap"
        if source.sha(map_file) != proof["map_sha256"]:
            raise RuntimeError("City map changed since failed scaffold import")
    return {"path":str(path.relative_to(source.ROOT)),"sha256":RESUME_FAILURE_SHA,
            "reused_assets":by_asset}


def verify_import(manifest):
    report=source.read_json(IMPORT)
    if (report["schema"] != "rok.construction_scaffold.ue_import.v1" or
            not report["passed"] or report["project"] != str(source.ROOT.resolve()) or
            report["world"] != "L_RokUnityGameAppCity" or
            report["prepared_sha256"] != source.sha(MANIFEST) or
            len(report["assets"]) != 24):
        raise RuntimeError("Scaffold UE import report identity/count differs")
    resume=report.get("resume")
    if resume:
        proved=verify_resume_proof(manifest,source.ROOT/resume["path"])
        if resume != proved:
            raise RuntimeError("Scaffold UE import resume metadata differs from sealed failure")
        reused={x["asset"] for x in report["assets"] if x.get("reused_after_failure") is True}
        if reused != set(proved["reused_assets"]):
            raise RuntimeError("Scaffold UE import reused asset set differs")
    expected={r["ue_mesh_asset"] for r in manifest["layers"]}
    expected.update(x["ue_asset"] for r in manifest["layers"] for x in r["sources"].values())
    for item in report["assets"]:
        asset=item["asset"]
        if asset not in expected:
            raise RuntimeError(f"Unexpected scaffold imported asset: {asset}")
        expected.remove(asset)
        path=source.ROOT/"Content"/(asset.removeprefix("/Game/")+".uasset")
        if not path.is_file() or source.sha(path)!=item["uasset_sha256"]:
            raise RuntimeError(f"Scaffold UE asset missing/changed: {asset}")
        if resume and asset in resume["reused_assets"] and item["uasset_sha256"] != resume["reused_assets"][asset]:
            raise RuntimeError(f"Scaffold resumed texture was rewritten: {asset}")
        if item["kind"] == "mesh" and (not item["face_topology_proved"] or
                                         item["uv_channels"] != 1):
            raise RuntimeError("Scaffold imported mesh topology/UV was not proved")
    if expected or report["map_saved"] or report["material_created"]:
        raise RuntimeError("Scaffold import omissions or scope violation")
    return report


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--require-import",action="store_true")
    args=parser.parse_args()
    manifest,clips=verify_prepared()
    imported=verify_import(manifest) if args.require_import else None
    print(json.dumps({"passed":True,"prepared_sha256":source.sha(MANIFEST),
                      "animation_sha256":source.sha(ANIMATION),"layers":8,"clips":4,
                      "ue_import_verified":bool(imported)}))


if __name__=="__main__":
    main()
