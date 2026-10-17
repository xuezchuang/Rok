"""Import only the verified Academy source SpriteMesh into a dedicated UE asset.

Default entry is a genuinely read-only preflight. Root invokes apply through
UE 5.8 with ROK_ACADEMY_MESH_APPLY=1 after checking the project/map identity.
This module never edits the three existing Academy texture/material assets or
any map/actor. The source-probe prefab pose is intentionally not applied.
"""

import hashlib
import json
import os
from pathlib import Path

import unreal

import CreateRokAcademyVisualAssets as academy
import PrepareRokAcademySourceMesh as source_mesh


ROOT = Path("D:/ueDemo/Rok")
DEST = academy.DEST
MESH_NAME = source_mesh.MESH_NAME
ASSET = DEST + "/" + MESH_NAME
REPORT = ROOT / "Saved/RokUnityMigration/AcademyPresentationMesh/ue_mesh_create.json"
PRECISION_REPAIR_REPORT = ROOT / "Saved/RokUnityMigration/AcademyPresentationMesh/ue_mesh_precision_repair.json"
PRECISION_REPAIR_DIAGNOSTIC = ROOT / "Saved/RokUnityMigration/AcademyPresentationMesh/ue_mesh_precision_repair_diagnostic.json"
PRECISION_REPAIR_ENV = "ROK_ACADEMY_MESH_PRECISION_REPAIR"
META = {
    "RokAcademySourceGeometrySHA256": "source_geometry_sha256",
    "RokAcademyObjSHA256": "obj_sha256",
    "RokAcademyNativeRenderSHA256": "native_render_sha256",
    "RokAcademySourceCatalogSHA256": "source_catalog_sha256",
    "RokAcademySpriteSourceSHA256": "source_sprite_png_sha256",
    "RokAcademyUvMapping": "uv_mapping",
    "RokAcademyFaceOrder": "face_order",
    "RokAcademyRuntimePose": "runtime_pose_status",
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def package_file(asset_path):
    if not asset_path.startswith("/Game/"):
        raise RuntimeError("Academy asset package is outside /Game")
    path = ROOT / "Content" / (asset_path.removeprefix("/Game/") + ".uasset")
    if not path.is_file():
        raise RuntimeError(f"Academy asset package missing: {path}")
    return path


def static_mesh_editor():
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem is None:
        raise RuntimeError("UE5.8 StaticMeshEditorSubsystem unavailable; cannot read or repair UV build settings")
    return subsystem


def lod0_build_settings(mesh):
    settings = static_mesh_editor().get_lod_build_settings(mesh, 0)
    bools = ("use_mikk_t_space", "recompute_normals", "recompute_tangents",
             "compute_weighted_normals", "remove_degenerates", "build_reversed_index_buffer",
             "use_high_precision_tangent_basis", "use_full_precision_u_vs",
             "use_backwards_compatible_f16_trunc_u_vs", "generate_lightmap_u_vs",
             "generate_distance_field_as_if_two_sided", "support_face_remap")
    integers = ("min_lightmap_resolution", "src_lightmap_index", "dst_lightmap_index",
                "max_lumen_mesh_cards")
    values = {}
    for key in bools + integers:
        try:
            value = settings.get_editor_property(key)
        except Exception:
            continue  # UE Python spelling varies; the complete struct repr follows.
        values[key] = bool(value) if key in bools else int(value)
    for key in ("use_full_precision_u_vs", "remove_degenerates", "generate_lightmap_u_vs"):
        if key not in values:
            raise RuntimeError(f"Academy LOD0 required build setting unavailable: {key}")
    values["all_build_settings_repr"] = str(settings)
    return settings, values


def prepared_contract(run):
    run = run.resolve()
    catalog, verified, rendered = source_mesh.load_source(run)
    output = source_mesh.DEST_ROOT / run.name
    manifest_path = output / "source_mesh_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    obj_path = output / manifest["obj_file"]
    geometry = rendered["geometry"]
    expected_obj, expected_vertices, expected_faces = source_mesh.expected_obj(run, geometry)
    bounds_min = [min(vertex[axis] for vertex in expected_vertices) for axis in range(3)]
    bounds_max = [max(vertex[axis] for vertex in expected_vertices) for axis in range(3)]
    child_position = geometry["childLocalPosition"]
    child_rotation = geometry["childLocalRotation"]
    child_scale = geometry["childLocalScale"]
    expected_relative = {
        "translation_cm": [-child_position["x"] * 500.0,
                           child_position["z"] * 500.0,
                           child_position["y"] * 500.0],
        "rotation_quaternion_xyzw": [-child_rotation["x"], child_rotation["z"],
                                     child_rotation["y"], child_rotation["w"]],
        "uniform_scale": 5.0 * child_scale["x"],
        "scope": "Source prefab child-local transform relative to its root; root/world city pose supplied separately",
    }
    if (manifest["schema"] != "rok.academy_source_sprite_mesh.v1" or
            Path(manifest["source_run"]).resolve() != run or
            manifest["obj_file"] != MESH_NAME + ".obj" or
            obj_path.parent.resolve() != output.resolve() or
            obj_path.read_bytes() != expected_obj.encode("utf-8") or
            manifest["obj_sha256"] != sha(obj_path) or
            manifest["source_catalog_sha256"] != sha(source_mesh.CATALOG_PATH) or
            manifest["native_render_sha256"] != sha(run / "native_render.json") or
            manifest["native_verify_sha256"] != sha(run / "native_verify.json") or
            manifest["source_geometry_sha256"] != source_mesh.canonical_sha(
                {key: rendered["geometry"][key]
                 for key in ("vertices", "uvs", "triangles", "rect", "pivot", "pixelsPerUnit")}) or
            manifest["source_sprite_png_sha256"] != catalog["sprite"]["sha256"] or
            manifest["ue_local_bounds_min_cm"] != bounds_min or
            manifest["ue_local_bounds_max_cm"] != bounds_max or
            manifest["project_ue_units_per_unity_unit"] != 500.0 or
            manifest["obj_ue_units_per_unity_unit"] != 100.0 or
            manifest["component_relative_transform_example"] != expected_relative or
            manifest["mesh_asset"] != ASSET or
            manifest["material_asset"] != academy.path_of(academy.MATERIAL_NAME) or
            manifest["vertex_count"] != 49 or manifest["uv_count"] != 49 or
            manifest["triangle_count"] != 47 or len(expected_vertices) != 49 or
            len(expected_faces) != 47):
        raise RuntimeError("Prepared Academy source SpriteMesh OBJ/manifest differs from verified native geometry")
    return manifest, manifest_path, obj_path, rendered


def preflight():
    world = academy.identity()
    catalog, native_verify, native_render, _, _ = academy.source_contract()
    run = academy.NATIVE
    manifest, manifest_path, obj_path, rendered = prepared_contract(run)
    sprite = unreal.load_asset(academy.path_of(academy.SPRITE_NAME))
    mask = unreal.load_asset(academy.path_of(academy.MASK_NAME))
    material = unreal.load_asset(academy.path_of(academy.MATERIAL_NAME))
    # Texture2D::GetSizeX/Y uses a placeholder while PlatformData builds in
    # the full Editor. UE5.8's public console command synchronously finishes
    # texture compilation; the sealed verifier below still checks exact sizes.
    if not isinstance(sprite, unreal.Texture2D) or not isinstance(mask, unreal.Texture2D):
        raise RuntimeError("Academy source sprite/mask asset type differs")
    before = [(int(t.blueprint_get_size_x()), int(t.blueprint_get_size_y()))
              for t in (sprite, mask)]
    if before != [(512, 426), (256, 213)]:
        unreal.SystemLibrary.execute_console_command(world, "Editor.AsyncTextureCompilationFinishAll")
        after = [(int(t.blueprint_get_size_x()), int(t.blueprint_get_size_y()))
                 for t in (sprite, mask)]
        unreal.log(f"ROK_ACADEMY_TEXTURE_COMPILE_SIZE before={before} after={after}")
    academy.verify_texture(sprite, catalog, "sprite", native_render)
    academy.verify_texture(mask, catalog, "mask", native_render)
    academy.verify_material(material, catalog, native_verify, sprite, mask)
    return world, manifest, manifest_path, obj_path, material


def verify_mesh(mesh, manifest, require_metadata=True):
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Academy source SpriteMesh asset is not a StaticMesh")
    actual = {
        "vertex_count_lod0": int(mesh.get_num_vertices(0)),
        "triangle_count_lod0": int(mesh.get_num_triangles(0)),
        "uv_channel_count_lod0": int(mesh.get_num_tex_coords(0)),
    }
    if (actual["vertex_count_lod0"] != manifest["vertex_count"] or
            actual["triangle_count_lod0"] != manifest["triangle_count"] or
            actual["uv_channel_count_lod0"] != 1):
        raise RuntimeError(f"Academy imported SpriteMesh topology differs: {actual}")
    bounds = mesh.get_bounds()
    lo = [bounds.origin.x - bounds.box_extent.x,
          bounds.origin.y - bounds.box_extent.y,
          bounds.origin.z - bounds.box_extent.z]
    hi = [bounds.origin.x + bounds.box_extent.x,
          bounds.origin.y + bounds.box_extent.y,
          bounds.origin.z + bounds.box_extent.z]
    if any(abs(float(value) - expected) > 0.05
           for measured, pinned in ((lo, manifest["ue_local_bounds_min_cm"]),
                                    (hi, manifest["ue_local_bounds_max_cm"]))
           for value, expected in zip(measured, pinned)):
        raise RuntimeError(f"Academy imported SpriteMesh bounds differ: min={lo}, max={hi}")
    if require_metadata:
        for tag, key in META.items():
            if str(unreal.EditorAssetLibrary.get_metadata_tag(mesh, tag)) != str(manifest[key]):
                raise RuntimeError(f"Academy source SpriteMesh metadata differs: {tag}")
    actual["bounds_min_cm"] = lo
    actual["bounds_max_cm"] = hi
    return actual


def apply():
    world, manifest, manifest_path, obj_path, material = preflight()
    if os.environ.get("ROK_ACADEMY_MESH_APPLY") != "1":
        raise RuntimeError("Academy source mesh is preflight-only unless ROK_ACADEMY_MESH_APPLY=1")
    mesh = unreal.load_asset(ASSET)
    created = False
    if mesh is None:
        # No replace/reimport path: an existing asset is verified, never overwritten.
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(obj_path))
        task.set_editor_property("destination_path", DEST)
        task.set_editor_property("destination_name", MESH_NAME)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("replace_existing_settings", False)
        task.set_editor_property("save", False)
        options = unreal.FbxImportUI()
        options.set_editor_property("import_mesh", True)
        options.set_editor_property("import_as_skeletal", False)
        options.set_editor_property("import_materials", False)
        options.set_editor_property("import_textures", False)
        options.set_editor_property("automated_import_should_detect_type", False)
        static_data = options.get_editor_property("static_mesh_import_data")
        static_data.set_editor_property("combine_meshes", True)
        static_data.set_editor_property("generate_lightmap_u_vs", False)
        static_data.set_editor_property("auto_generate_collision", False)
        static_data.set_editor_property("build_nanite", False)
        task.set_editor_property("factory", unreal.FbxFactory())
        task.set_editor_property("options", options)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        mesh = unreal.load_asset(ASSET)
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError("Academy source SpriteMesh import did not produce the dedicated asset")
        verify_mesh(mesh, manifest, require_metadata=False)
        settings, _ = lod0_build_settings(mesh)
        settings.set_editor_property("use_full_precision_u_vs", True)
        static_mesh_editor().set_lod_build_settings(mesh, 0, settings)
        if not lod0_build_settings(mesh)[1]["use_full_precision_u_vs"]:
            raise RuntimeError("Academy source SpriteMesh full-precision UV build setting was not applied")
        for tag, key in META.items():
            unreal.EditorAssetLibrary.set_metadata_tag(mesh, tag, str(manifest[key]))
        if not unreal.EditorAssetLibrary.save_loaded_asset(mesh):
            raise RuntimeError("Academy source SpriteMesh save failed")
        created = True
    topology = verify_mesh(mesh, manifest)
    report = {
        "schema": "rok.academy_source_sprite_mesh.ue_import.v1",
        "project": str(ROOT), "world": world.get_name(),
        "mesh_asset": ASSET, "material_asset": material.get_path_name(),
        "source_mesh_manifest": str(manifest_path),
        "source_mesh_manifest_sha256": sha(manifest_path),
        "source_obj_sha256": sha(obj_path),
        "native_source_run": manifest["source_run"],
        "created": created, "topology": topology,
        "pose": "local SpriteMesh only; probe prefab transform and runtime city pose not baked",
        "visual_acceptance": "pending actual UE source camera render comparison",
        "map_saved": False, "existing_academy_assets_modified": False,
    }
    report_path = (ROOT / "Saved/RokUnityMigration/AcademyPresentationMesh/ue_mesh_create_retry.json"
                   if REPORT.is_file() else REPORT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return report


def repair_precision():
    # A separate opt-in path for the already-created source mesh. The original
    # create report and the three sealed Academy material/texture assets remain
    # untouched; no OBJ reimport or material edit occurs here.
    world, manifest, manifest_path, obj_path, material = preflight()
    if os.environ.get(PRECISION_REPAIR_ENV) != "1":
        raise RuntimeError(f"Academy UV repair requires {PRECISION_REPAIR_ENV}=1")
    mesh = unreal.load_asset(ASSET)
    topology_before = verify_mesh(mesh, manifest)
    mesh_file = package_file(ASSET)
    sealed_paths = tuple(package_file(academy.path_of(name)) for name in
                         (academy.SPRITE_NAME, academy.MASK_NAME, academy.MATERIAL_NAME))
    sealed_before = {str(path): sha(path) for path in sealed_paths}
    manifest_sha_before = sha(manifest_path)
    create_report_sha_before = sha(REPORT) if REPORT.is_file() else None
    mesh_sha_before = sha(mesh_file)
    settings, before = lod0_build_settings(mesh)
    if PRECISION_REPAIR_REPORT.is_file():
        existing = json.loads(PRECISION_REPAIR_REPORT.read_text(encoding="utf-8"))
        recorded_settings = existing.get("lod0_build_settings_after", {})
        if (existing.get("schema") != "rok.academy_source_sprite_mesh.precision_repair.v1" or
                existing.get("project") != str(ROOT) or existing.get("world") != world.get_name() or
                existing.get("asset") != ASSET or
                existing.get("package_sha256_after") != mesh_sha_before or
                existing.get("source_manifest_sha256") != manifest_sha_before or
                existing.get("source_obj_sha256") != sha(obj_path) or
                existing.get("original_create_report_sha256") != create_report_sha_before or
                existing.get("sealed_texture_material_packages_sha256_unchanged") != sealed_before or
                existing.get("topology_after") != topology_before or
                not before["use_full_precision_u_vs"] or
                any(recorded_settings.get(key) != value for key, value in before.items()
                    if key != "all_build_settings_repr")):
            raise RuntimeError("Existing Academy UV repair report differs from current mesh/source/sealed assets")
        return existing
    if not before["use_full_precision_u_vs"]:
        settings.set_editor_property("use_full_precision_u_vs", True)
        static_mesh_editor().set_lod_build_settings(mesh, 0, settings)
        _, after_rebuild = lod0_build_settings(mesh)
        changed = {key: {"before": value, "after": after_rebuild.get(key)}
                   for key, value in before.items()
                   if key != "all_build_settings_repr" and after_rebuild.get(key) != value}
        if (not after_rebuild["use_full_precision_u_vs"] or
                set(changed) != {"use_full_precision_u_vs"}):
            diagnostic = {
                "schema": "rok.academy_source_sprite_mesh.precision_repair_diagnostic.v1",
                "stage": "after-LOD0-PostEditChange-before-save",
                "project": str(ROOT), "world": world.get_name(), "asset": ASSET,
                "package_sha256_before": mesh_sha_before, "package_sha256_at_rejection": sha(mesh_file),
                "lod0_build_settings_before": before,
                "lod0_build_settings_after_rebuild": after_rebuild,
                "changed_fields": changed,
                "source_manifest_sha256": manifest_sha_before,
                "original_create_report_sha256": create_report_sha_before,
                "sealed_texture_material_packages_sha256_before": sealed_before,
                "topology_before": topology_before,
                "asset_saved": False, "resource_assets_created": False,
            }
            diagnostic_path = PRECISION_REPAIR_DIAGNOSTIC
            suffix = 2
            while diagnostic_path.exists():
                diagnostic_path = PRECISION_REPAIR_DIAGNOSTIC.with_name(
                    f"ue_mesh_precision_repair_diagnostic{suffix}.json")
                suffix += 1
            diagnostic_path.parent.mkdir(parents=True, exist_ok=True)
            diagnostic_path.write_text(
                json.dumps(diagnostic, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            raise RuntimeError("Academy full-precision UV rebuild changed another pinned build flag")
        verify_mesh(mesh, manifest)
        if not unreal.EditorAssetLibrary.save_loaded_asset(mesh):
            raise RuntimeError("Academy full-precision UV mesh save failed")
    topology_after = verify_mesh(mesh, manifest)
    _, after = lod0_build_settings(mesh)
    if not after["use_full_precision_u_vs"]:
        raise RuntimeError("Academy source mesh UV precision is still half")
    if any(sha(path) != expected for path, expected in sealed_before.items()):
        raise RuntimeError("A sealed Academy sprite/mask/material package changed during mesh precision repair")
    if sha(manifest_path) != manifest_sha_before or \
       (sha(REPORT) if REPORT.is_file() else None) != create_report_sha_before:
        raise RuntimeError("Academy source mesh manifest or original create report changed")
    report = {
        "schema": "rok.academy_source_sprite_mesh.precision_repair.v1",
        "project": str(ROOT), "world": world.get_name(), "asset": ASSET,
        "package_file": str(mesh_file), "package_sha256_before": mesh_sha_before,
        "package_sha256_after": sha(mesh_file),
        "lod0_build_settings_before": before, "lod0_build_settings_after": after,
        "source_manifest": str(manifest_path), "source_manifest_sha256": manifest_sha_before,
        "source_obj_sha256": sha(obj_path), "original_create_report_sha256": create_report_sha_before,
        "sealed_texture_material_packages_sha256_unchanged": sealed_before,
        "topology_before": topology_before, "topology_after": topology_after,
        "reimported": False, "map_saved": False,
        "runtime_visual_acceptance": "pending new RHI capture after full-precision UV rebuild",
    }
    PRECISION_REPAIR_REPORT.parent.mkdir(parents=True, exist_ok=True)
    with PRECISION_REPAIR_REPORT.open("x", encoding="utf-8") as output:
        output.write(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    return report


if __name__ == "__main__":
    preflight()
    print("ACADEMY_SOURCE_MESH_READONLY_PREFLIGHT_PASS")
