"""Read-only UE asset audit for Farm/Sawmill source presentation assets.

The UE asset files and current map are never saved or modified. The mesh OBJ
export under Saved is evidence only; it proves the nonzero Unity source faces
survive with their position/UV, allowing only exact-zero source faces to drop.
"""

import json

import unreal

import CreateRokCityResourcePresentationAssets as assets


REPORT = assets.ROOT / "Saved/RokUnityMigration/CityResourcePresentationAssets/ue_assets_verify.json"


def verify():
    catalog, models, plan = assets.preflight()
    if any(row["create_roles"] for row in plan):
        raise RuntimeError("Resource presentation assets are missing; runtime visual acceptance pending")
    rows = []
    for model, (record, manifest, manifest_path, obj_path) in models.items():
        paths = manifest["ue_assets"]
        sprite = unreal.load_asset(paths["sprite"])
        mask = unreal.load_asset(paths["mask"])
        material = unreal.load_asset(paths["material"])
        mesh = unreal.load_asset(paths["mesh"])
        assets.verify_texture(sprite, model, record, "sprite")
        assets.verify_texture(mask, model, record, "mask")
        graph = assets.verify_material(material, model, catalog, record, sprite, mask)
        topology = assets.verify_mesh(mesh, model, manifest, obj_path, material, export_faces=True)
        rows.append({"model": model, "assets": paths,
                     "source_mesh_manifest": str(manifest_path),
                     "source_mesh_manifest_sha256": assets.sha(manifest_path),
                     "source_obj_sha256": assets.sha(obj_path),
                     "material_graph": graph, "imported_mesh": topology,
                     "source_zero_area_triangle_indices": manifest["source_zero_area_triangle_indices"],
                     "source_nonzero_faces_proved": topology["source_nonzero_faces_proved"]})
    report = {"schema": "rok.city_resource_presentation_assets.ue_verify.v1",
              "passed": True, "project": str(assets.ROOT), "world": assets.WORLD,
              "native_run": str(assets.native_run()),
              "source_catalog_sha256": assets.sha(assets.source.CATALOG),
              "native_verified_capture_sha256": assets.sha(assets.source.SUMMARY),
              "models": rows, "map_saved": False, "asset_write": False,
              "runtime_visual_acceptance": "pending actual UE capture compared with verified Unity source"}
    report_path = assets.next_evidence_path(REPORT)
    report["report_path"] = str(report_path)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    with report_path.open("x", encoding="utf-8") as output:
        output.write(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    unreal.log("ROK_CITY_RESOURCE_PRESENTATION_ASSETS_VERIFIED " + str(report_path) + " runtime visual pending")
    return report


if __name__ == "__main__":
    verify()
