"""Check the Academy visual extraction against unmodified Unity source bytes."""
import csv
import hashlib
import json

from PIL import Image

from BuildRokAcademyVisualSource import (ROOT, OUT, PREFAB, SPRITE, MASK, MATERIAL, SHADER,
                                         MASK_SCRIPT, MODEL_CSV, MODEL_CATALOG, build)

REPORT = OUT.with_name("verify.json")


def verify():
    manifest = json.loads(OUT.read_text(encoding="utf-8"))
    assert manifest == build(write=False), "Academy visual source extraction is stale"
    assert manifest["schema"] == "rok.academy_visual_source.v1" and manifest["model_id"] == "build_13_101_2"
    assert manifest["qualification"].startswith("Unity source extraction only")
    for name, expected_hash in manifest["sources"].items():
        path = ROOT / name
        assert path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == expected_hash
    assert len(manifest["sources"]) == 14
    prefab = PREFAB.read_text(encoding="utf-8-sig")
    root, sprite, floor, board = (next(n for n in manifest["prefab"]["nodes"] if n["name"] == name)
                                  for name in ("build_13_101_2", "sprite", "FloorTile_delete", "UpgradeBoard"))
    assert len(manifest["prefab"]["nodes"]) == 12
    assert root["active"] and sprite["active"] and not floor["active"] and not board["active"]
    assert root["transform"]["parent_transform_file_id"] == 0
    assert sprite["transform"]["parent_transform_file_id"] == root["transform"]["file_id"]
    for node in (root, sprite, floor, board):
        assert f"m_Name: {node['name']}" in prefab
        assert node["layer"] == 0 and node["tag"] == "Untagged"
    assert sprite["renderer"]["sprite_guid"] == manifest["sprite"]["guid"]
    assert sprite["renderer"]["material_guid"] == manifest["material"]["guid"]
    assert sprite["mask_sprite"]["mask_sprite_guid"] == manifest["mask"]["guid"]
    assert sprite["mask_sprite"]["script_guid"] == manifest["mask_binding"]["script_guid"]
    assert all(sprite["renderer"][key] == 0 for key in ("sorting_layer_id", "sorting_layer", "sorting_order"))
    assert not sprite["renderer"]["flip_x"] and not sprite["renderer"]["flip_y"]
    assert board["renderer"]["sprite_guid"] != sprite["renderer"]["sprite_guid"]
    assert floor["renderer"]["sprite_file_id"] == 0
    for path, key in ((SPRITE, "sprite"), (MASK, "mask")):
        with Image.open(path) as png:
            rgba = png.convert("RGBA")
            record = manifest[key]
            assert list(png.size) == record["size"] == [512, 426] and png.mode == record["mode"] == "RGBA"
            for channel, band in zip("rgba", rgba.split()):
                statistics = record["channels"][channel]
                assert statistics == {"min": band.getextrema()[0], "max": band.getextrema()[1],
                                      "nonzero_pixels": sum(band.histogram()[1:])}
        meta = path.with_name(path.name + ".meta").read_text(encoding="utf-8-sig")
        assert f"guid: {record['guid']}" in meta
        assert f"spritePixelsToUnits: {record['importer']['pixels_to_units']}" in meta
    assert manifest["sprite"]["importer"]["pixels_to_units"] == 100
    assert manifest["mask"]["importer"]["pixels_to_units"] == 440
    assert manifest["sprite"]["importer"]["pivot"] == manifest["mask"]["importer"]["pivot"] == [0.5, 0.5]
    assert manifest["mask"]["channels"]["g"]["nonzero_pixels"] == 0
    assert manifest["mask"]["channels"]["r"]["nonzero_pixels"] > 0
    assert manifest["mask"]["channels"]["b"]["nonzero_pixels"] > 0
    material = MATERIAL.read_text(encoding="utf-8-sig")
    shader = SHADER.read_text(encoding="utf-8-sig")
    script = MASK_SCRIPT.read_text(encoding="latin1")
    assert f"guid: {manifest['material']['shader_guid']}" in material
    assert 'propertyBlock.SetTexture("_Mask", m_mask_sprite.texture)' in script
    assert "m_sprite_renderer.SetPropertyBlock(propertyBlock)" in script
    assert "fixed4 mask = tex2D(_Mask, IN.texcoord);" in shader
    assert "mask_1 = (mask.r * 2.0)" in shader
    assert "tmpvar_5 = clamp((mask_1 - 1.0), 0.0, 1.0)" in shader
    for key in ("rgb", "alpha", "sprite_color_multiply"):
        assert manifest["material"]["shader_formula"][key] in shader
    assert manifest["material"]["shader_formula"]["queue"] == "Transparent-5"
    assert manifest["material"]["city_transparency"] == 1.0
    assert manifest["material"]["color_1"] == {"r": 0.0, "g": 0.254902, "b": 1.0, "a": 1.0}
    assert manifest["material"]["color_2"] == {"r": 0.772549, "g": 0.3254902, "b": 0.2392157, "a": 1.0}
    with MODEL_CSV.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))[1:]
    expected = sorted((int(r["ID"]), int(r["type"]), int(r["civType"]), int(r["age"]))
                      for r in rows if r["modelId"] == "build_13_101_2")
    actual = sorted((r["id"], r["type"], r["civilization"], r["age"]) for r in manifest["matching_model_keys"])
    assert actual == expected and len(actual) == 7
    source_index = json.loads(MODEL_CATALOG.read_text(encoding="utf-8"))
    assert actual == sorted((r["id"], r["type"], r["civilization"], r["age"])
                            for r in source_index["models"] if r["model_id"] == "build_13_101_2")
    report = {"schema": "rok.academy_visual_source.verify.v1", "status": "passed",
              "catalog_sha256": hashlib.sha256(OUT.read_bytes()).hexdigest(),
              "source_count": len(manifest["sources"]), "prefab_nodes_checked": len(manifest["prefab"]["nodes"]),
              "matching_model_keys_checked": len(actual), "png_size": [512, 426],
              "mask_r_nonzero_pixels": manifest["mask"]["channels"]["r"]["nonzero_pixels"],
              "mask_g_nonzero_pixels": manifest["mask"]["channels"]["g"]["nonzero_pixels"],
              "qualification": "extract-only; no UE visual/runtime acceptance"}
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"verified Academy source: {len(actual)} model keys, 12 prefab nodes, 512x426 sprite/mask")
    return report


if __name__ == "__main__":
    verify()
