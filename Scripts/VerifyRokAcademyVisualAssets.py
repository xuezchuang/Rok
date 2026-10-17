"""Read-only UE 5.8 verification of dedicated Academy source visual assets.

This does not accept scene placement, full city runtime presentation, or visual
pixel parity. It writes only a Saved verification JSON; no asset/map saves.
"""

import json

import unreal

import CreateRokAcademyVisualAssets as academy


def verify():
    world = academy.identity()
    catalog, native_verify, native_render, root, main = academy.source_contract()
    sprite = unreal.load_asset(academy.path_of(academy.SPRITE_NAME))
    mask = unreal.load_asset(academy.path_of(academy.MASK_NAME))
    material = unreal.load_asset(academy.path_of(academy.MATERIAL_NAME))
    academy.verify_texture(sprite, catalog, "sprite", native_render)
    academy.verify_texture(mask, catalog, "mask", native_render)
    graph = academy.verify_material(material, catalog, native_verify, sprite, mask)
    result = {
        "schema": "rok.academy_visual_assets.ue_verify.v1",
        "passed": True,
        "project": str(academy.ROOT), "world": world.get_name(),
        "source_catalog_sha256": academy.sha(academy.CATALOG),
        "native_render_sha256": academy.sha(academy.NATIVE / "native_render.json"),
        "assets": {"sprite": academy.path_of(academy.SPRITE_NAME),
                   "mask": academy.path_of(academy.MASK_NAME),
                   "material": academy.path_of(academy.MATERIAL_NAME)},
        "material_graph": graph,
        "sprite_rect": native_render["geometry"]["rect"],
        "sprite_pivot_pixels": native_render["geometry"]["pivot"],
        "sprite_pixels_per_unit": native_render["geometry"]["pixelsPerUnit"],
        "source_root_transform": root["transform"],
        "source_sprite_transform": main["transform"],
        "original_mask_source_png_size": catalog["mask"]["size"],
        "actual_unity_imported_mask_size": [native_render["maskTextureWidth"], native_render["maskTextureHeight"]],
        "unity_gpu_export_mask_sha256": native_verify["gpu_decoded_mask_export_sha256"],
        "unity_export_control_delta": native_verify["export_control_delta"],
        "unity_native_source_probe_passed": True,
        "ue_runtime_render_accepted": False,
        "scope": "Dedicated Academy source textures/material and source geometry evidence; no map or runtime visual acceptance.",
    }
    path = academy.REPORT.with_name("ue_asset_verify.json")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log(f"ROK_ACADEMY_VISUAL_ASSETS_VERIFY_PASS {path}; runtime visual acceptance pending")


if __name__ == "__main__":
    verify()
