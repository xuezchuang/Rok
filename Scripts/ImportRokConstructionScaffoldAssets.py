"""Fail-closed UE5.8 import of prepared Build_shelf_5 V1 source inputs.

Default run is read-only. Set ROK_SCAFFOLD_ASSETS_APPLY=1 in the exact Rok
L_RokUnityGameAppCity editor to create 16 raw textures and 8 tight StaticMeshes.
No material, actor, map, construction ticket or existing asset is modified.
"""
from __future__ import annotations

import collections
import hashlib
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import PrepareRokConstructionScaffoldAssets as source
import VerifyRokConstructionScaffoldAssets as verify

WORLD = "L_RokUnityGameAppCity"
APPLY_ENV = "ROK_SCAFFOLD_ASSETS_APPLY"
RESUME_ENV = "ROK_SCAFFOLD_ASSETS_RESUME_PROOF"
REPORT = source.OUT / "ue_import_report.json"


def asset_file(asset_path):
    assert asset_path.startswith(source.PACKAGE + "/")
    return source.ROOT / "Content" / (asset_path.removeprefix("/Game/") + ".uasset")


def identity(unreal):
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world() if editor else None
    actual = world.get_name() if world else None
    print("Scaffold import identity:", project, actual)
    if project != source.ROOT.resolve() or actual != WORLD:
        raise RuntimeError("Wrong scaffold import project/world")
    return world


def metadata(unreal, asset, expected, apply=False):
    for key,value in expected.items():
        if apply:
            unreal.EditorAssetLibrary.set_metadata_tag(asset,key,str(value))
        elif str(unreal.EditorAssetLibrary.get_metadata_tag(asset,key)) != str(value):
            raise RuntimeError(f"Scaffold asset metadata differs: {asset.get_path_name()} {key}")


def texture_settings(unreal):
    # Raw RGBA channels remain unmodified GPU inputs for the future source
    # Gamma/mask shader. These flags do not claim Unity compositing parity.
    return {"srgb":False,
            "compression_settings":unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP,
            "mip_gen_settings":unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            "lod_group":unreal.TextureGroup.TEXTUREGROUP_WORLD,
            "filter":unreal.TextureFilter.TF_BILINEAR,
            "address_x":unreal.TextureAddress.TA_CLAMP,
            "address_y":unreal.TextureAddress.TA_CLAMP,
            "never_stream":True}


def texture_meta(row, role, manifest_sha):
    item=row["sources"][role]
    return {"RokScaffoldV1PreparedSha256":manifest_sha,
            "RokScaffoldV1Node":row["node"],"RokScaffoldV1Role":role,
            "RokScaffoldV1Guid":item["guid"],"RokScaffoldV1FileID":item["file_id"],
            "RokScaffoldV1SourceSha256":item["sha256"],
            "RokScaffoldV1SourceMetaSha256":item["meta_sha256"],
            "RokScaffoldV1RawOnly":"true"}


def finish_texture_compilation(unreal, world):
    # UE5.8 can report a temporary 32x32 PlatformData placeholder immediately
    # after import. A targeted identity-world finish is verification, not a save.
    unreal.SystemLibrary.execute_console_command(world,"Editor.AsyncTextureCompilationFinishAll")


def verify_texture(unreal, world, asset, row, role, manifest_sha, apply=False):
    if not isinstance(asset, unreal.Texture2D):
        raise RuntimeError(f"Scaffold {row['name']}/{role} is not Texture2D")
    for key,value in texture_settings(unreal).items():
        if apply:
            asset.set_editor_property(key,value)
        if asset.get_editor_property(key) != value:
            raise RuntimeError(f"Scaffold texture flag differs: {row['name']}/{role}/{key}")
    item=row["sources"][role]
    dimensions=[int(asset.blueprint_get_size_x()),int(asset.blueprint_get_size_y())]
    if dimensions != item["size"]:
        finish_texture_compilation(unreal,world)
        dimensions=[int(asset.blueprint_get_size_x()),int(asset.blueprint_get_size_y())]
    if dimensions != item["size"]:
        raise RuntimeError(f"Scaffold texture dimension mismatch: {row['name']}/{role} actual={dimensions} expected={item['size']}")
    metadata(unreal,asset,texture_meta(row,role,manifest_sha),apply)
    return {"size":dimensions,"raw_flags_verified":True}


def import_texture(unreal, world, row, role, manifest_sha):
    item=row["sources"][role]
    path=item["ue_asset"]
    task=unreal.AssetImportTask()
    for key,value in {"filename":str(source.OUT/item["prepared_file"]),
                      "destination_path":path.rsplit("/",1)[0],
                      "destination_name":path.rsplit("/",1)[1],
                      "automated":True,"replace_existing":False,"save":False}.items():
        task.set_editor_property(key,value)
    task.set_editor_property("factory",unreal.TextureFactory())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if getattr(task,"errors",None):
        raise RuntimeError(f"Scaffold TextureFactory errors: {task.errors}")
    asset=unreal.load_asset(path)
    evidence=verify_texture(unreal,world,asset,row,role,manifest_sha,apply=True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset) or not asset_file(path).is_file():
        raise RuntimeError(f"Scaffold texture save failed: {path}")
    return asset,evidence


def source_face_signatures(path, exported=False):
    vertices=[];uvs=[];faces=[]
    for line in path.read_text(encoding="utf-8").splitlines():
        p=line.split()
        if not p:continue
        if p[0]=="v":vertices.append(tuple(map(float,p[1:4])))
        if p[0]=="vt":uvs.append(tuple(map(float,p[1:3])))
        if p[0]=="f":
            if len(p)!=4:raise RuntimeError("Scaffold OBJ has a nontriangle face")
            corners=[]
            for token in p[1:]:
                parts=token.split("/")
                if len(parts)<2 or not parts[1]:raise RuntimeError("Scaffold OBJ face lacks UV")
                x,y,z=vertices[int(parts[0])-1]
                if exported:
                    # Source OBJ (x,0,z) becomes UE/exported OBJ (x,z,0).
                    pos=(x,y,z)
                else:pos=(x,z,y)
                uv=uvs[int(parts[1])-1]
                corners.append(tuple(round(v,4) for v in pos)+tuple(round(v,6) for v in uv))
            faces.append(tuple(sorted(corners)))
    return vertices,uvs,collections.Counter(faces),faces


def mesh_meta(row,manifest_sha):
    return {"RokScaffoldV1PreparedSha256":manifest_sha,
            "RokScaffoldV1Node":row["node"],
            "RokScaffoldV1GeometrySha256":row["geometry_sha256"],
            "RokScaffoldV1ObjSha256":row["obj_sha256"],
            "RokScaffoldV1Pivot":json.dumps(row["geometry"]["pivot"],sort_keys=True,separators=(",",":")),
            "RokScaffoldV1SourceTriangleCount":row["triangle_count"],
            "RokScaffoldV1RawOnly":"true"}


def static_mesh_editor(unreal):
    editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if not editor:raise RuntimeError("UE5.8 StaticMeshEditorSubsystem unavailable")
    return editor


def verify_mesh(unreal, asset, row, manifest_sha, exported_obj):
    if not isinstance(asset,unreal.StaticMesh):
        raise RuntimeError(f"Scaffold {row['name']} mesh is not StaticMesh")
    editor=static_mesh_editor(unreal)
    settings=editor.get_lod_build_settings(asset,0)
    if (not settings.get_editor_property("use_full_precision_u_vs") or
            settings.get_editor_property("remove_degenerates")):
        raise RuntimeError(f"Scaffold mesh build settings differ: {row['name']}")
    actual_count=int(asset.get_num_triangles(0))
    nonzero=row["triangle_count"]-len(row["zero_area_triangles"])
    if not nonzero<=actual_count<=row["triangle_count"] or int(asset.get_num_tex_coords(0))!=1:
        raise RuntimeError(f"Scaffold mesh triangle/UV count differs: {row['name']}")
    vertices=row["geometry"]["vertices"]
    expected_lo=[min(-v["x"]*100 for v in vertices),0,min(v["y"]*100 for v in vertices)]
    expected_hi=[max(-v["x"]*100 for v in vertices),0,max(v["y"]*100 for v in vertices)]
    bounds=asset.get_bounds()
    lo=[bounds.origin.x-bounds.box_extent.x,bounds.origin.y-bounds.box_extent.y,bounds.origin.z-bounds.box_extent.z]
    hi=[bounds.origin.x+bounds.box_extent.x,bounds.origin.y+bounds.box_extent.y,bounds.origin.z+bounds.box_extent.z]
    if any(abs(a-b)>0.05 for a,b in zip(lo+hi,expected_lo+expected_hi)):
        raise RuntimeError(f"Scaffold mesh pivot/bounds differ: {row['name']} {lo} {hi}")
    metadata(unreal,asset,mesh_meta(row,manifest_sha))
    _,_,expected,source_faces=source_face_signatures(source.OUT/row["obj_file"])
    _,_,actual,_=source_face_signatures(exported_obj,exported=True)
    missing=expected-actual;extra=actual-expected
    zero_allowed=collections.Counter(source_faces[i] for i in row["zero_area_triangles"])
    if extra or missing-zero_allowed or sum(missing.values())!=row["triangle_count"]-actual_count:
        raise RuntimeError(f"Scaffold mesh imported face/UV topology differs: {row['name']} missing={sum(missing.values())} extra={sum(extra.values())}")
    return {"face_topology_proved":True,"source_triangles":row["triangle_count"],
            "imported_triangles":actual_count,"source_zero_area_omitted":sum(missing.values()),
            "uv_channels":1,"bounds_min_cm":lo,"bounds_max_cm":hi,
            "export_obj_sha256":source.sha(exported_obj)}


def export_mesh(unreal,asset,row):
    target=source.OUT/"UEFaceEvidence"/(row["name"]+"_lod0.obj")
    if target.exists():
        raise RuntimeError(f"Refuse to replace earlier scaffold mesh export: {target}")
    target.parent.mkdir(parents=True,exist_ok=True)
    task=unreal.AssetExportTask()
    for k,v in {"object":asset,"filename":str(target),"exporter":unreal.StaticMeshExporterOBJ(),
                "automated":True,"prompt":False,"replace_identical":False}.items():
        task.set_editor_property(k,v)
    if not unreal.Exporter.run_asset_export_task(task) or not target.is_file():
        raise RuntimeError(f"Scaffold mesh face export failed: {row['name']}")
    return target


def import_mesh(unreal,row,manifest_sha):
    path=row["ue_mesh_asset"]
    task=unreal.AssetImportTask()
    for key,value in {"filename":str(source.OUT/row["obj_file"]),
                      "destination_path":path.rsplit("/",1)[0],
                      "destination_name":path.rsplit("/",1)[1],
                      "automated":True,"replace_existing":False,"save":False}.items():
        task.set_editor_property(key,value)
    options=unreal.FbxImportUI()
    for key,value in (("import_mesh",True),("import_as_skeletal",False),
                      ("import_materials",False),("import_textures",False),
                      ("automated_import_should_detect_type",False)):
        options.set_editor_property(key,value)
    data=options.get_editor_property("static_mesh_import_data")
    for key,value in (("combine_meshes",True),("generate_lightmap_u_vs",False),
                      ("auto_generate_collision",False),("build_nanite",False),
                      ("remove_degenerates",False)):
        data.set_editor_property(key,value)
    task.set_editor_property("factory",unreal.FbxFactory())
    task.set_editor_property("options",options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset=unreal.load_asset(path)
    if not isinstance(asset,unreal.StaticMesh):
        raise RuntimeError(f"Scaffold FbxFactory OBJ import failed: {path}")
    editor=static_mesh_editor(unreal)
    build=editor.get_lod_build_settings(asset,0)
    build.set_editor_property("use_full_precision_u_vs",True)
    build.set_editor_property("remove_degenerates",False)
    editor.set_lod_build_settings(asset,0,build)
    metadata(unreal,asset,mesh_meta(row,manifest_sha),apply=True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset) or not asset_file(path).is_file():
        raise RuntimeError(f"Scaffold mesh save failed: {path}")
    return asset


def main():
    import unreal
    world=identity(unreal)
    manifest,animation=verify.verify_prepared()
    manifest_sha=source.sha(verify.MANIFEST)
    # Commandlet Python can import Texture2D but does not expose this editor
    # subsystem. Check it before creating even the first texture.
    static_mesh_editor(unreal)
    targets=[x["ue_asset"] for row in manifest["layers"] for x in row["sources"].values()]
    targets += [row["ue_mesh_asset"] for row in manifest["layers"]]
    existing=[path for path in targets if unreal.EditorAssetLibrary.does_asset_exist(path) or asset_file(path).exists()]
    resume_path=os.environ.get(RESUME_ENV)
    resume=None
    if existing:
        if len(existing)==24 and REPORT.exists() and not resume_path:
            report=verify.verify_import(manifest)
            for row in manifest["layers"]:
                for role,item in row["sources"].items():
                    verify_texture(unreal,world,unreal.load_asset(item["ue_asset"]),row,role,manifest_sha)
                mesh=unreal.load_asset(row["ue_mesh_asset"])
                proof=next(x for x in report["assets"] if x["asset"]==row["ue_mesh_asset"])
                obj=source.OUT/proof["export_obj"]
                verify_mesh(unreal,mesh,row,manifest_sha,obj)
            print(json.dumps({"existing_verified":True,"report":str(REPORT),"assets":24}))
            return
        if not resume_path or os.environ.get(APPLY_ENV)!="1" or REPORT.exists():
            raise RuntimeError(f"Partial/unreported scaffold assets exist; refuse overwrite: {existing}")
        resume=verify.verify_resume_proof(manifest,resume_path)
        if set(existing)!=set(resume["reused_assets"]):
            raise RuntimeError(f"Scaffold existing assets differ from sealed failure: {existing}")
        physical={str(x.resolve()) for x in (source.ROOT/"Content"/source.PACKAGE.removeprefix("/Game/")).rglob("*.uasset")}
        proven={str(asset_file(x).resolve()) for x in resume["reused_assets"]}
        if physical!=proven:
            raise RuntimeError(f"Scaffold namespace contains unproved assets: {physical-proven}")
        for role,item in manifest["layers"][0]["sources"].items():
            asset=unreal.load_asset(item["ue_asset"])
            if asset is None:
                raise RuntimeError(f"Sealed scaffold texture does not load: {item['ue_asset']}")
            verify_texture(unreal,world,asset,manifest["layers"][0],role,manifest_sha)
            if source.sha(asset_file(item["ue_asset"]))!=resume["reused_assets"][item["ue_asset"]]:
                raise RuntimeError(f"Read-only resume verification changed texture: {item['ue_asset']}")
    elif resume_path:
        raise RuntimeError("Scaffold resume proof specified but its two textures are absent")
    if os.environ.get(APPLY_ENV)!="1":
        print(json.dumps({"ready":True,"apply_required":APPLY_ENV,"project":str(source.ROOT),
                          "world":WORLD,"new_assets":targets,"prepared_sha256":manifest_sha}))
        return
    if REPORT.exists():
        raise RuntimeError("Scaffold import report exists but assets are absent; refuse ambiguity")
    unreal.EditorAssetLibrary.make_directory(source.PACKAGE)
    details=[]
    for row in manifest["layers"]:
        for role,item in row["sources"].items():
            reused=bool(resume and item["ue_asset"] in resume["reused_assets"])
            if reused:
                asset=unreal.load_asset(item["ue_asset"])
                proof=verify_texture(unreal,world,asset,row,role,manifest_sha)
            else:
                _,proof=import_texture(unreal,world,row,role,manifest_sha)
            details.append({"asset":item["ue_asset"],"kind":"texture","role":role,
                            "uasset_sha256":source.sha(asset_file(item["ue_asset"])),
                            **({"reused_after_failure":True} if reused else {}),**proof})
        mesh=import_mesh(unreal,row,manifest_sha)
        obj=export_mesh(unreal,mesh,row)
        proof=verify_mesh(unreal,mesh,row,manifest_sha,obj)
        details.append({"asset":row["ue_mesh_asset"],"kind":"mesh",
                        "uasset_sha256":source.sha(asset_file(row["ue_mesh_asset"])),
                        "export_obj":obj.relative_to(source.OUT).as_posix(),**proof})
    report={"schema":"rok.construction_scaffold.ue_import.v1","passed":True,
            "project":str(source.ROOT.resolve()),"world":WORLD,
            "prepared_sha256":manifest_sha,"animation_sha256":source.sha(verify.ANIMATION),
            "scope":"16 raw GPU textures and 8 tight source meshes only; no source-Gamma material",
            "assets":details,"map_saved":False,"material_created":False,
            "runtime_visual_acceptance":"pending"}
    if resume:
        report["resume"]=resume
    with REPORT.open("x",encoding="utf-8") as stream:
        json.dump(report,stream,ensure_ascii=False,indent=2);stream.write("\n")
    verify.verify_import(manifest)
    print(json.dumps({"imported":24,"report":str(REPORT),"sha256":source.sha(REPORT)}))


if __name__=="__main__":
    main()
