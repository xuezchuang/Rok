"""Independently audit Farm/Sawmill city-scene source chains and L1 rows."""
import csv
import hashlib
import json
import re
from pathlib import Path

from PIL import Image

from BuildRokCityResourceBuildingVisualSources import (
    ROOT, OUT, MODEL_CATALOG, MODEL_CSV, TYPE_CSV, LEVEL_CSV, PRODUCE_CSV,
    MATERIAL, SHADER, MASK_SCRIPT, build,
)


REPORT = OUT.with_name("verify.json")


def rows(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        data = list(csv.DictReader(stream))
    assert data and data[0][next(iter(data[0]))] == "int"
    return data[1:]


def verify():
    manifest = json.loads(OUT.read_text(encoding="utf-8"))
    assert manifest == build(write=False), "Resource building visual package is stale"
    assert manifest["schema"] == "rok.city_resource_building_visual_sources.v1"
    assert "local visual variant" in manifest["qualification"]
    assert len(manifest["models"]) == 2
    for relative, expected in manifest["sources"].items():
        path = ROOT / relative
        assert path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == expected
    catalog = json.loads(MODEL_CATALOG.read_text(encoding="utf-8"))
    source_models = rows(MODEL_CSV)
    types = {int(r["type"]): r for r in rows(TYPE_CSV)}
    levels = {(int(r["type"]), int(r["level"])): r for r in rows(LEVEL_CSV)}
    production = {(int(r["type"]), int(r["level"])): r for r in rows(PRODUCE_CSV)}
    material = MATERIAL.read_text(encoding="utf-8-sig")
    shader = SHADER.read_text(encoding="utf-8-sig")
    mask_script = MASK_SCRIPT.read_text(encoding="latin1")
    assert f"guid: {manifest['material']['shader_guid']}" in material
    assert "fixed4 mask = tex2D(_Mask, IN.texcoord);" in shader
    assert "mask_1 = (mask.r * 2.0)" in shader
    assert "tmpvar_5 = clamp((mask_1 - 1.0), 0.0, 1.0)" in shader
    assert 'propertyBlock.SetTexture("_Mask", m_mask_sprite.texture)' in mask_script
    assert "m_sprite_renderer.SetPropertyBlock(propertyBlock)" in mask_script
    assert manifest["material"]["shader_formula"]["queue"] == "Transparent-5"
    total_nodes = 0
    for model in manifest["models"]:
        key = model["key"]
        assert (key["type"], key["civilization"], key["age"]) in ((2, 101, 1), (3, 101, 1))
        exact = [row for row in catalog["models"] if
                 (row["type"], row["civilization"], row["age"]) ==
                 (key["type"], key["civilization"], key["age"])]
        assert len(exact) == 1 and exact[0]["id"] == model["model_data_id"]
        assert exact[0]["model_id"] == model["model_id"]
        assert model["prefab"]["path"] == exact[0]["scene_prefab"]
        assert "/Scene/City_building/" in model["prefab"]["path"]
        assert model["missing_sources"] == []
        raw = [r for r in source_models if (int(r["type"]), int(r["civType"]), int(r["age"])) ==
               (key["type"], key["civilization"], key["age"])]
        assert len(raw) == 1 and raw[0]["modelId"] == model["model_id"]
        expected_keys = sorted((int(r["ID"]), int(r["type"]), int(r["civType"]), int(r["age"]))
                               for r in source_models if r["modelId"] == model["model_id"])
        actual_keys = sorted((r["id"], r["type"], r["civilization"], r["age"])
                             for r in model["matching_model_keys"])
        assert actual_keys == expected_keys and len(actual_keys) == 2
        prefab = (ROOT / model["prefab"]["path"]).read_text(encoding="utf-8-sig")
        nodes = model["prefab"]["nodes"]
        total_nodes += len(nodes)
        assert len(nodes) == 12
        root = next(n for n in nodes if n["name"] == model["model_id"])
        main = next(n for n in nodes if n["name"] == "sprite")
        floor = next(n for n in nodes if n["name"] == "FloorTile_delete")
        board = next(n for n in nodes if n["name"] == "UpgradeBoard")
        assert root["active"] and main["active"] and not floor["active"] and not board["active"]
        assert main["transform"]["parent_transform_file_id"] == root["transform"]["file_id"]
        assert main["renderer"]["sprite_guid"] == model["sprite"]["guid"]
        assert main["renderer"]["material_guid"] == manifest["material"]["guid"]
        assert main["mask_sprite"]["mask_sprite_guid"] == model["mask"]["guid"]
        assert main["mask_sprite"]["script_guid"] == model["mask_binding"]["script_guid"]
        assert all(main["renderer"][name] == 0 for name in ("sorting_layer_id", "sorting_layer", "sorting_order"))
        assert not main["renderer"]["flip_x"] and not main["renderer"]["flip_y"]
        assert "m_Name: " + model["model_id"] in prefab
        assert floor["renderer"]["sprite_file_id"] == 0
        assert board["renderer"]["sprite_guid"] != main["renderer"]["sprite_guid"]
        for name in ("sprite", "mask"):
            record = model[name]
            path = ROOT / record["path"]
            with Image.open(path) as png:
                rgba = png.convert("RGBA")
                assert list(png.size) == record["size"]
                for channel, band in zip("rgba", rgba.split()):
                    assert record["channels"][channel] == {
                        "min": band.getextrema()[0], "max": band.getextrema()[1],
                        "nonzero_pixels": sum(band.histogram()[1:]),
                    }
            meta = Path(str(path) + ".meta").read_text(encoding="utf-8-sig")
            assert f"guid: {record['guid']}" in meta
            assert f"spritePixelsToUnits: {record['importer']['pixels_to_units']}" in meta
            assert "nPOTScale: " + str(record["platform"]["npot_scale"]) in meta
            for platform in record["platform"]["platform_settings"]:
                assert f"buildTarget: {platform['build_target']}" in meta
                assert f"maxTextureSize: {platform['max_texture_size']}" in meta
        assert model["sprite"]["size"] == model["mask"]["size"]
        assert model["sprite"]["importer"]["pixels_to_units"] == 100
        assert model["mask"]["importer"]["pixels_to_units"] == 440
        assert model["mask"]["channels"]["r"]["nonzero_pixels"] > 0
        t, level, produce = types[key["type"]], levels[(key["type"], 1)], production[(key["type"], 1)]
        assert model["footprint"] == {"width": int(t["width"]), "length": int(t["length"])}
        assert (int(level["width"]), int(level["length"])) == (5, 5)
        l1 = model["level_1"]
        assert l1["level_data_id"] == int(level["ID"])
        assert l1["cost"] == {name: int(level[name]) for name in ("food", "wood", "stone", "coin", "denar")}
        assert l1["duration_seconds"] == int(level["buildingTime"]) == 2
        assert l1["prerequisites"] == [{"type": int(level[f"reqType{i}"]), "level": int(level[f"reqLevel{i}"])}
                                       for i in range(1, 4)]
        assert l1["produce_data_id"] == int(produce["ID"])
        assert (l1["produce_per_hour"], l1["gather_min"], l1["gather_max"]) == (400, 66, 4000)
    assert next(m for m in manifest["models"] if m["key"]["type"] == 2)["level_1"]["cost"] == {
        "food": 0, "wood": 100, "stone": 0, "coin": 0, "denar": 0}
    assert next(m for m in manifest["models"] if m["key"]["type"] == 3)["level_1"]["prerequisites"][0] == {
        "type": 1, "level": 2}
    report = {"schema": "rok.city_resource_building_visual_sources.verify.v1", "status": "passed",
              "catalog_sha256": hashlib.sha256(OUT.read_bytes()).hexdigest(),
              "source_count": len(manifest["sources"]), "models_checked": 2,
              "prefab_nodes_checked": total_nodes, "matching_model_keys_checked": 4,
              "qualification": "extract-only; no UE asset or runtime visual acceptance"}
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"verified Farm/Sawmill source: 2 exact models, {total_nodes} prefab nodes")
    return report


if __name__ == "__main__":
    verify()
