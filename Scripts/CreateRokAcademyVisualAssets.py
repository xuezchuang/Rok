"""Create only Academy source sprite, Unity-imported GPU mask, and R/G material.

Run through the UE 5.8 editor on L_RokUnityGameAppCity with environment
ROK_ACADEMY_VISUAL_APPLY=1. No actor, map, source, or other product asset writes.
The material reproduces map_building_with_mask-5's R/G day formula; UE
translucent compositing and full-city visual parity remain separate gates.
"""

import hashlib
import json
import os
from pathlib import Path

import unreal


ROOT = Path("D:/ueDemo/Rok")
EXPECTED_WORLD = "L_RokUnityGameAppCity"
CATALOG = ROOT / "Saved/RokUnityMigration/AcademyVisualSource/catalog.json"
NATIVE_ENV = os.environ.get("ROK_ACADEMY_NATIVE_RUN", "")
NATIVE = Path(NATIVE_ENV).resolve() if NATIVE_ENV else None
DEST = "/Game/RokUnity/City/Academy/build_13_101_2"
SPRITE_NAME = "T_build_13_101_2_SpriteRaw"
MASK_NAME = "T_build_13_101_2_MaskUnityGpu256"
MATERIAL_NAME = "M_build_13_101_2_SourceRG"
REPORT = ROOT / "Saved/RokUnityMigration/AcademyVisualAssets/ue_asset_create.json"

CODE = """float4 col = MainSample * Renderer;
float red2 = MaskSample.r * 2.0;
float team1 = saturate(red2 - 1.0);
float3 gammaColor = col.rgb * ((1.0 - team1 - MaskSample.g)
    + 2.0 * Team1.rgb * team1 + 2.0 * Team2.rgb * MaskSample.g);
gammaColor *= Lighting.rgb;
float3 linearColor = lerp(gammaColor / 12.92,
    pow(max((gammaColor + 0.055) / 1.055, 0.0), 2.4),
    step(0.04045, gammaColor));
float opacity = col.a * saturate(red2) * Transparency;
return float4(linearColor, opacity);
"""


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def path_of(name):
    return DEST + "/" + name


def identity():
    project = Path(unreal.Paths.project_dir()).resolve()
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = subsystem.get_editor_world() if subsystem else None
    if project != ROOT.resolve() or world is None or world.get_name() != EXPECTED_WORLD:
        raise RuntimeError(f"Wrong Academy asset target: project={project}, world={world.get_name() if world else None}")
    return world


def source_contract():
    if NATIVE is None or NATIVE.parent != (ROOT / "Saved/UnityAcademyVisualProbe/OutputRuns").resolve():
        raise RuntimeError("Set ROK_ACADEMY_NATIVE_RUN to the exact verified Academy OutputRuns child")
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    if catalog["schema"] != "rok.academy_visual_source.v1" or catalog["model_id"] != "build_13_101_2":
        raise RuntimeError("Wrong Academy source catalog")
    for relative, expected in catalog["sources"].items():
        if sha(ROOT / relative) != expected:
            raise RuntimeError(f"Academy source hash changed: {relative}")
    formula = catalog["material"]["shader_formula"]
    if (formula["shader_name"] != "custom/map/map_building_with_mask-5" or
            formula["blend"] != "Blend SrcAlpha OneMinusSrcAlpha, SrcAlpha OneMinusSrcAlpha" or
            formula["cull"] != "Cull Off" or formula["zwrite"] != "ZWrite Off" or
            "mask.g" not in formula["rgb"] or "mask.r" not in formula["mask_1"] or
            "mask.b" in formula["rgb"] or "mask.a" in formula["rgb"]):
        raise RuntimeError("Unexpected original Academy R/G shader chain")
    main = next(n for n in catalog["prefab"]["nodes"]
                if n["file_id"] == catalog["prefab"]["main_sprite_game_object_file_id"])
    root = next(n for n in catalog["prefab"]["nodes"]
                if n["file_id"] == catalog["prefab"]["root_game_object_file_id"])
    if (not root["active"] or not main["active"] or not main["renderer"]["enabled"] or
            main["transform"]["parent_transform_file_id"] != root["transform"]["file_id"] or
            main["renderer"]["sprite_guid"] != catalog["sprite"]["guid"] or
            main["mask_sprite"]["mask_sprite_guid"] != catalog["mask"]["guid"]):
        raise RuntimeError("Academy prefab rendering identity changed")
    if (catalog["sprite"]["size"] != [512, 426] or catalog["mask"]["size"] != [512, 426] or
            catalog["sprite"]["importer"]["pixels_to_units"] != 100 or
            catalog["mask"]["importer"]["pixels_to_units"] != 440):
        raise RuntimeError("Academy source texture geometry changed")
    native_verify = json.loads((NATIVE / "native_verify.json").read_text(encoding="utf-8"))
    native_render = json.loads((NATIVE / "native_render.json").read_text(encoding="utf-8"))
    if (native_verify["schema"] != "rok.academy_visual_native_verify.v2" or not native_verify["passed"] or
            Path(native_verify["run_directory"]).resolve() != NATIVE or
            native_verify["catalog_sha256"] != sha(CATALOG) or
            native_verify["native_report_sha256"] != sha(NATIVE / "native_render.json") or
            native_render["schema"] != "rok.academy_visual_native_render.v2" or
            native_render["colorSpace"] != "Gamma" or native_render["unityVersion"] != "2019.4.28f1c1"):
        raise RuntimeError("Academy source native render has not passed verification")
    exported_mask = NATIVE / native_render["maskExport"]["png"]
    if (native_render["maskExport"]["png"] != "academy_mask_imported_gpu_rgba.png" or
            native_verify["gpu_decoded_mask_export_sha256"] != sha(exported_mask) or
            [native_render["maskTextureWidth"], native_render["maskTextureHeight"]] != [256, 213] or
            native_verify["native_imported_mask_size"] != [256, 213]):
        raise RuntimeError("Verified Unity GPU-decoded 256x213 Academy mask missing")
    geometry = native_render["geometry"]
    if (geometry["rect"]["width"] != 512 or geometry["rect"]["height"] != 426 or
            abs(geometry["pixelsPerUnit"] - 100) > 1e-6 or
            len(geometry["vertices"]) < 4 or
            native_render["maskBinding"] != "original Client.MaskSprite.Start MaterialPropertyBlock _Mask = mask_sprite.texture"):
        raise RuntimeError("Academy native sprite geometry/mask proof changed")
    return catalog, native_verify, native_render, root, main


def texture_flags():
    return {
        "srgb": False,  # Source Unity Gamma-space tex2D arithmetic must see encoded bytes.
        "compression_settings": unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "filter": unreal.TextureFilter.TF_BILINEAR,
        "address_x": unreal.TextureAddress.TA_CLAMP,
        "address_y": unreal.TextureAddress.TA_CLAMP,
        "never_stream": True,
    }


def metadata(asset, key, value):
    unreal.EditorAssetLibrary.set_metadata_tag(asset, key, str(value))


def expected_meta(catalog, source_key, native_render):
    importer = catalog[source_key]["importer"]
    result = {
        "RokAcademySourceCatalogSha256": sha(CATALOG),
        "RokAcademySourcePngSha256": catalog[source_key]["sha256"],
        "RokAcademySourceMetaSha256": catalog[source_key]["meta_sha256"],
        "RokAcademySourceRole": source_key,
        "RokAcademyUnityPpu": importer["pixels_to_units"],
        "RokAcademyUnityPivot": json.dumps(importer["pivot"], separators=(",", ":")),
        "RokAcademyUnitySpriteMeshType": importer["sprite_mesh_type"],
        "RokAcademyAcceptance": "native-source-probed;ue-runtime-render-pending",
    }
    if source_key == "mask":
        result["RokAcademyUnityGpuMaskExportSha256"] = sha(NATIVE / native_render["maskExport"]["png"])
        result["RokAcademyUnityGpuMaskExportMethod"] = native_render["maskExport"]["method"]
        result["RokAcademyUnityGpuMaskSize"] = "256x213"
    return result


def verify_texture(texture, catalog, role, native_render):
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Academy {role} is not Texture2D")
    for key, value in texture_flags().items():
        if texture.get_editor_property(key) != value:
            raise RuntimeError(f"Academy {role} texture flag differs: {key}")
    expected_size = catalog[role]["size"] if role == "sprite" else [256, 213]
    if [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())] != expected_size:
        raise RuntimeError(f"Academy {role} texture dimensions differ")
    for key, value in expected_meta(catalog, role, native_render).items():
        if str(unreal.EditorAssetLibrary.get_metadata_tag(texture, key)) != str(value):
            raise RuntimeError(f"Academy {role} source metadata differs: {key}")


def import_texture(catalog, role, name, native_render):
    existing = unreal.load_asset(path_of(name))
    if existing is not None:
        verify_texture(existing, catalog, role, native_render)
        return existing, False
    source = (ROOT / catalog[role]["path"] if role == "sprite" else
              NATIVE / native_render["maskExport"]["png"])
    task = unreal.AssetImportTask()
    for key, value in {
        "filename": str(source), "destination_path": DEST, "destination_name": name,
        "factory": unreal.TextureFactory(), "automated": True,
        "replace_existing": False, "save": False,
    }.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(path_of(name))
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Academy {role} source PNG import failed")
    for key, value in texture_flags().items():
        texture.set_editor_property(key, value)
    for key, value in expected_meta(catalog, role, native_render).items():
        metadata(texture, key, value)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError(f"Academy {role} texture save failed")
    verify_texture(texture, catalog, role, native_render)
    return texture, True


def expected_material_meta(catalog, native_verify):
    return {
        "RokAcademySourceCatalogSha256": sha(CATALOG),
        "RokAcademyShaderSha256": catalog["sources"][catalog["material"]["shader_path"]],
        "RokAcademyMaterialSha256": catalog["sources"][catalog["material"]["path"]],
        "RokAcademyNativeVerifySha256": sha(NATIVE / "native_verify.json"),
        "RokAcademyUnityGpuMaskExportSha256": sha(NATIVE / "academy_mask_imported_gpu_rgba.png"),
        "RokAcademyHlslSha256": hashlib.sha256(CODE.encode("utf-8")).hexdigest(),
        "RokAcademyMaskBinding": "MaskSprite.MPB:_Mask=mask.texture;same-main-uv",
        "RokAcademySourceTransparencyBinding": "shader-global-not-declared-material-property",
        "RokAcademyGammaTransfer": "source-Gamma-arithmetic-then-sRGB-decode-to-UE-linear",
        "RokAcademyColor1": json.dumps(catalog["material"]["color_1"], sort_keys=True),
        "RokAcademyColor2": json.dumps(catalog["material"]["color_2"], sort_keys=True),
        "RokAcademyAcceptance": "native-source-probed;ue-runtime-render-pending",
    }


def create_material(catalog, native_verify, sprite, mask):
    existing = unreal.load_asset(path_of(MATERIAL_NAME))
    if existing is not None:
        verify_material(existing, catalog, native_verify, sprite, mask)
        return existing, False
    edit = unreal.MaterialEditingLibrary
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MATERIAL_NAME, DEST, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(material, unreal.Material):
        raise RuntimeError("Academy material creation failed")
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)

    def node(kind):
        return edit.create_material_expression(material, kind)

    def connect(source, output, target, input_name):
        if not edit.connect_material_expressions(source, output, target, input_name):
            raise RuntimeError(f"Academy material connection failed: {output} -> {input_name}")

    samples = {}
    for parameter, texture in (("Sprite", sprite), ("Mask", mask)):
        sample = node(unreal.MaterialExpressionTextureSampleParameter2D)
        sample.set_editor_property("parameter_name", parameter)
        sample.set_editor_property("texture", texture)
        sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        samples[parameter] = sample

    def color(name, values):
        value = node(unreal.MaterialExpressionVectorParameter)
        value.set_editor_property("parameter_name", name)
        value.set_editor_property("default_value", unreal.LinearColor(*values))
        return value

    color1 = catalog["material"]["color_1"]
    color2 = catalog["material"]["color_2"]
    main = next(n for n in catalog["prefab"]["nodes"]
                if n["file_id"] == catalog["prefab"]["main_sprite_game_object_file_id"])
    renderer = main["renderer"]["color"]
    lighting = [1.07203974, 1.07203974, 1.07203974, 1.0]
    params = {
        "Color1": color("Color1", [color1[k] for k in "rgba"]),
        "Color2": color("Color2", [color2[k] for k in "rgba"]),
        "RendererColor": color("RendererColor", [renderer[k] for k in "rgba"]),
        "SpriteColor": color("SpriteColor", lighting),
    }
    transparency = node(unreal.MaterialExpressionScalarParameter)
    transparency.set_editor_property("parameter_name", "CityTransparency")
    # Unity's shader reads an explicit global, despite the .mat's serialized
    # field. The dedicated UE scalar is a controllable translation of that
    # render-time global, not a claim that Material.GetFloat returned 1.
    transparency.set_editor_property("default_value", catalog["material"]["city_transparency"])
    shader = node(unreal.MaterialExpressionCustom)
    shader.set_editor_property("description", "Unity Academy source R/G mask Gamma arithmetic then exact sRGB decode")
    shader.set_editor_property("code", CODE)
    shader.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    names = ("MainSample", "MaskSample", "Team1", "Team2", "Renderer", "Lighting", "Transparency")
    inputs = []
    for name in names:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        inputs.append(entry)
    shader.set_editor_property("inputs", inputs)
    bindings = (
        (samples["Sprite"], "RGBA", "MainSample"), (samples["Mask"], "RGBA", "MaskSample"),
        (params["Color1"], "RGBA", "Team1"), (params["Color2"], "RGBA", "Team2"),
        (params["RendererColor"], "RGBA", "Renderer"), (params["SpriteColor"], "RGBA", "Lighting"),
        (transparency, "", "Transparency"),
    )
    for source, output, name in bindings:
        connect(source, output, shader, name)
    rgb = node(unreal.MaterialExpressionComponentMask)
    alpha = node(unreal.MaterialExpressionComponentMask)
    for channel in "rgb":
        rgb.set_editor_property(channel, True)
        alpha.set_editor_property(channel, False)
    rgb.set_editor_property("a", False)
    alpha.set_editor_property("a", True)
    connect(shader, "", rgb, "")
    connect(shader, "", alpha, "")
    if not edit.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR) or \
       not edit.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError("Academy material output binding failed")
    for key, value in expected_material_meta(catalog, native_verify).items():
        metadata(material, key, value)
    compile_errors = [str(item) for item in edit.recompile_material(material)]
    if compile_errors:
        raise RuntimeError("Academy material compile failed: " + "; ".join(compile_errors))
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Academy material save failed")
    verify_material(material, catalog, native_verify, sprite, mask)
    return material, True


def verify_material(material, catalog, native_verify, sprite, mask):
    if not isinstance(material, unreal.Material):
        raise RuntimeError("Academy source material is not a Material")
    for key, value in (("material_domain", unreal.MaterialDomain.MD_SURFACE),
                       ("shading_model", unreal.MaterialShadingModel.MSM_UNLIT),
                       ("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT), ("two_sided", True)):
        if material.get_editor_property(key) != value:
            raise RuntimeError(f"Academy material property differs: {key}")
    for key, value in expected_material_meta(catalog, native_verify).items():
        if str(unreal.EditorAssetLibrary.get_metadata_tag(material, key)) != str(value):
            raise RuntimeError(f"Academy material source metadata differs: {key}")
    edit = unreal.MaterialEditingLibrary
    expressions = list(edit.get_material_expressions(material))
    if len(expressions) != 10:
        raise RuntimeError(f"Academy material expression count differs: {len(expressions)}")
    def by_type(cls, count):
        found = [e for e in expressions if isinstance(e, cls)]
        if len(found) != count:
            raise RuntimeError(f"Academy material {cls.__name__} count differs: {len(found)}")
        return found
    samples = by_type(unreal.MaterialExpressionTextureSampleParameter2D, 2)
    vectors = by_type(unreal.MaterialExpressionVectorParameter, 4)
    scalars = by_type(unreal.MaterialExpressionScalarParameter, 1)
    customs = by_type(unreal.MaterialExpressionCustom, 1)
    masks = by_type(unreal.MaterialExpressionComponentMask, 2)
    sample_by_name = {str(s.get_editor_property("parameter_name")): s for s in samples}
    if set(sample_by_name) != {"Sprite", "Mask"}:
        raise RuntimeError("Academy material texture parameter identities differ")
    for name, target in (("Sprite", sprite), ("Mask", mask)):
        sample = sample_by_name[name]
        if sample.get_editor_property("texture") != target or \
           sample.get_editor_property("sampler_type") != unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR:
            raise RuntimeError(f"Academy {name} sample differs")
    values = {str(v.get_editor_property("parameter_name")): v.get_editor_property("default_value") for v in vectors}
    main = next(n for n in catalog["prefab"]["nodes"]
                if n["file_id"] == catalog["prefab"]["main_sprite_game_object_file_id"])
    expected = {
        "Color1": catalog["material"]["color_1"], "Color2": catalog["material"]["color_2"],
        "RendererColor": main["renderer"]["color"],
        "SpriteColor": dict(zip("rgba", [1.07203974, 1.07203974, 1.07203974, 1.0])),
    }
    if set(values) != set(expected):
        raise RuntimeError("Academy color parameter names differ")
    for name, source in expected.items():
        actual = values[name]
        if any(abs(getattr(actual, key) - source[key]) > 1e-5 for key in "rgba"):
            raise RuntimeError(f"Academy color parameter differs: {name}")
    if str(scalars[0].get_editor_property("parameter_name")) != "CityTransparency" or \
       abs(scalars[0].get_editor_property("default_value") - catalog["material"]["city_transparency"]) > 1e-5:
        raise RuntimeError("Academy transparency parameter differs")
    custom_inputs = list(customs[0].get_editor_property("inputs"))
    input_names = [str(i.get_editor_property("input_name")) for i in custom_inputs]
    if customs[0].get_editor_property("code") != CODE or \
       customs[0].get_editor_property("output_type") != unreal.CustomMaterialOutputType.CMOT_FLOAT4 or \
       input_names != ["MainSample", "MaskSample", "Team1", "Team2", "Renderer", "Lighting", "Transparency"]:
        raise RuntimeError("Academy R/G shader code/input contract differs")
    bound_nodes = list(edit.get_inputs_for_material_expression(material, customs[0]))
    vector_nodes = {str(v.get_editor_property("parameter_name")): v for v in vectors}
    if len(bound_nodes) != len(custom_inputs) or bound_nodes != [sample_by_name["Sprite"], sample_by_name["Mask"],
                       vector_nodes["Color1"], vector_nodes["Color2"],
                       vector_nodes["RendererColor"], vector_nodes["SpriteColor"], scalars[0]]:
        raise RuntimeError("Academy source mask/RGB material inputs disconnected or reordered")
    if sum(bool(m.get_editor_property("a")) for m in masks) != 1 or \
       sum(bool(m.get_editor_property("r")) and bool(m.get_editor_property("g")) and bool(m.get_editor_property("b")) for m in masks) != 1:
        raise RuntimeError("Academy RGB/alpha material masks differ")
    emissive = edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY)
    if emissive not in masks or opacity not in masks or emissive == opacity or \
       not all(bool(emissive.get_editor_property(k)) for k in "rgb") or \
       bool(emissive.get_editor_property("a")) or \
       any(bool(opacity.get_editor_property(k)) for k in "rgb") or \
       not bool(opacity.get_editor_property("a")):
        raise RuntimeError("Academy material output topology differs")
    for mask_node in (emissive, opacity):
        if list(edit.get_inputs_for_material_expression(material, mask_node)) != [customs[0]]:
            raise RuntimeError("Academy material output mask is not fed by source R/G shader")
    return {"expression_count": len(expressions), "parameters": sorted(values),
            "textures": {name: sample_by_name[name].get_editor_property("texture").get_path_name()
                         for name in ("Sprite", "Mask")},
            "hlsl_sha256": hashlib.sha256(CODE.encode("utf-8")).hexdigest()}


def apply():
    world = identity()
    catalog, native_verify, native_render, root, main = source_contract()
    # All identity/source/native checks are complete before the first asset write.
    if os.environ.get("ROK_ACADEMY_VISUAL_APPLY") != "1":
        raise RuntimeError("Academy assets are preflight-only unless ROK_ACADEMY_VISUAL_APPLY=1")
    unreal.EditorAssetLibrary.make_directory(DEST)
    sprite, sprite_created = import_texture(catalog, "sprite", SPRITE_NAME, native_render)
    mask, mask_created = import_texture(catalog, "mask", MASK_NAME, native_render)
    material, material_created = create_material(catalog, native_verify, sprite, mask)
    topology = verify_material(material, catalog, native_verify, sprite, mask)
    report = {
        "schema": "rok.academy_visual_assets.ue_create.v1", "project": str(ROOT),
        "world": world.get_name(), "scope": "Three dedicated Academy assets only; no actors, map save, old assets, or source edits.",
        "asset_directory": DEST, "sprite_texture": path_of(SPRITE_NAME),
        "mask_texture": path_of(MASK_NAME), "material": path_of(MATERIAL_NAME),
        "created": {"sprite": sprite_created, "mask": mask_created, "material": material_created},
        "source_catalog_sha256": sha(CATALOG), "native_verify_sha256": sha(NATIVE / "native_verify.json"),
        "native_render_sha256": sha(NATIVE / "native_render.json"),
        "sprite_source_sha256": catalog["sprite"]["sha256"], "mask_source_sha256": catalog["mask"]["sha256"],
        "mask_original_source_png_size": catalog["mask"]["size"],
        "mask_unity_imported_gpu_export_size": [native_render["maskTextureWidth"], native_render["maskTextureHeight"]],
        "mask_unity_gpu_export_sha256": sha(NATIVE / native_render["maskExport"]["png"]),
        "mask_export_control_delta": native_verify["export_control_delta"],
        "source_importer": {"sprite": catalog["sprite"]["importer"], "mask": catalog["mask"]["importer"]},
        "source_transform": {"root": root["transform"], "sprite_child": main["transform"]},
        "native_geometry": native_render["geometry"], "native_camera": native_render["camera"],
        "material_graph": topology, "texture_flags": {k: str(v) for k, v in texture_flags().items()},
        "source_transparency_binding": "Unity shader global; native render-time observed value, not Material.GetFloat",
        "unity_native_source_probe_passed": True, "ue_runtime_render_accepted": False,
        "placement_note": "Use source rect/PPU/pivot and root x child pose. UE plane UV-Y must match Unity SpriteRenderer; mask PPU is not geometry."
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log(f"ROK_ACADEMY_VISUAL_ASSETS_READY {REPORT}; runtime visual acceptance pending")


def main():
    if os.environ.get("ROK_ACADEMY_VISUAL_APPLY") == "1":
        apply()
    else:
        identity()
        source_contract()
        unreal.log("ROK_ACADEMY_VISUAL_ASSETS_READONLY_PREFLIGHT_PASS; no asset/map write")


if __name__ == "__main__":
    main()
