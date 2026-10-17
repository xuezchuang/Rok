"""Create three isolated city materials for the registered-RT Gamma compositor.

Run in UE 5.8 with L_RokUnityGameAppCity open. The default is read-only
preflight. Set ROK_CITY_GAMMA_MATERIALS_APPLY=1 for new assets only. This script
never changes the source materials, textures, actors, map, or old reports.
"""

import hashlib
import json
import os
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path("D:/ueDemo/Rok")
WORLD = "L_RokUnityGameAppCity"
APPLY_ENV = "ROK_CITY_GAMMA_MATERIALS_APPLY"
CHECKPOINT = ROOT / "Saved/RokUnityMigration/parallel_migration_batch_20261008_170200.json"
CHECKPOINT_SHA = "88da0fb9e71304919fbb775f6c06149682a24a0667bc956a207cbd06ce93b2d6"
ACADEMY_VERIFY = ROOT / "Saved/RokUnityMigration/AcademyVisualAssets/ue_asset_verify.json"
RESOURCE_VERIFY = ROOT / "Saved/RokUnityMigration/CityResourcePresentationAssets/ue_assets_verify.json"
ACADEMY_CATALOG = ROOT / "Saved/RokUnityMigration/AcademyVisualSource/catalog.json"
RESOURCE_CATALOG = ROOT / "Saved/RokUnityMigration/CityResourceBuildingVisualSources/catalog.json"
RESOURCE_NATIVE_VERIFIED = ROOT / "Saved/RokUnityMigration/CityResourceVisualNative/verified_capture.json"
REPORT_DIR = ROOT / "Saved/RokUnityMigration/CityGammaMaterials"
DEST = "/Game/RokUnity/City/GammaComposite"
MODELS = ("build_13_101_2", "build_2_101_1", "build_3_101_1")
ORIGINAL_CODE = """float4 col = MainSample * Renderer;
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
DECODE_BLOCK = """float3 linearColor = lerp(gammaColor / 12.92,
    pow(max((gammaColor + 0.055) / 1.055, 0.0), 2.4),
    step(0.04045, gammaColor));
"""
GAMMA_CODE = ORIGINAL_CODE.replace(DECODE_BLOCK, "").replace(
    "return float4(linearColor, opacity);", "return float4(gammaColor, opacity);")
ORIGINAL_CODE_SHA = "f097a9a354c815ff0a4ef1f530533117c40c2c66a22f33702273aa1d125d425c"
CUSTOM_INPUTS = ("MainSample", "MaskSample", "Team1", "Team2", "Renderer", "Lighting", "Transparency")


def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha(path):
    return sha_bytes(Path(path).read_bytes())


def source_path(asset_path):
    return ROOT / "Content" / (asset_path.removeprefix("/Game/") + ".uasset")


def target_path(model):
    return f"{DEST}/{model}/M_{model}_SourceGammaBeforeDOF"


def identity():
    project = Path(unreal.Paths.project_dir()).resolve()
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = subsystem.get_editor_world() if subsystem else None
    if project != ROOT.resolve() or not world or world.get_name() != WORLD:
        raise RuntimeError(f"Wrong project/world: {project} / {world.get_name() if world else None}")
    return world


def before_dof():
    enum = getattr(unreal, "MaterialTranslucencyPass", None)
    if enum is None:
        raise RuntimeError("UE5.8 MaterialTranslucencyPass reflection unavailable")
    for name in ("MTP_BEFORE_DOF", "MTP_BeforeDOF"):
        value = getattr(enum, name, None)
        if value is not None:
            return value
    raise RuntimeError("UE5.8 MTP_BeforeDOF enum unavailable")


def graph(asset, code, source_textures=None, source_parameters=None, expected_pass=None):
    if not isinstance(asset, unreal.Material):
        raise RuntimeError(f"Not a Material: {asset}")
    for key, value in (("material_domain", unreal.MaterialDomain.MD_SURFACE),
                       ("shading_model", unreal.MaterialShadingModel.MSM_UNLIT),
                       ("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT),
                       ("two_sided", True)):
        if asset.get_editor_property(key) != value:
            raise RuntimeError(f"{asset.get_path_name()} property differs: {key}")
    if expected_pass is not None and asset.get_editor_property("translucency_pass") != expected_pass:
        raise RuntimeError(f"{asset.get_path_name()} is not MTP_BeforeDOF")
    edit = unreal.MaterialEditingLibrary
    expressions = list(edit.get_material_expressions(asset))
    if len(expressions) != 10:
        raise RuntimeError(f"{asset.get_path_name()} must have exactly 10 nodes")

    def nodes(kind, count):
        found = [node for node in expressions if isinstance(node, kind)]
        if len(found) != count:
            raise RuntimeError(f"{asset.get_path_name()} {kind.__name__} count differs")
        return found

    samples = {str(node.get_editor_property("parameter_name")): node
               for node in nodes(unreal.MaterialExpressionTextureSampleParameter2D, 2)}
    vectors = {str(node.get_editor_property("parameter_name")): node
               for node in nodes(unreal.MaterialExpressionVectorParameter, 4)}
    scalar = nodes(unreal.MaterialExpressionScalarParameter, 1)[0]
    custom = nodes(unreal.MaterialExpressionCustom, 1)[0]
    masks = nodes(unreal.MaterialExpressionComponentMask, 2)
    if set(samples) != {"Sprite", "Mask"} or set(vectors) != {
        "Color1", "Color2", "RendererColor", "SpriteColor"
    } or str(scalar.get_editor_property("parameter_name")) != "CityTransparency":
        raise RuntimeError(f"{asset.get_path_name()} parameter names differ")
    textures = {role: samples[role].get_editor_property("texture") for role in ("Sprite", "Mask")}
    for role, texture in textures.items():
        if not isinstance(texture, unreal.Texture2D) or samples[role].get_editor_property(
                "sampler_type") != unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR:
            raise RuntimeError(f"{asset.get_path_name()} {role} sample differs")
        for key, value in (("srgb", False),
                           ("compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP),
                           ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
                           ("filter", unreal.TextureFilter.TF_BILINEAR),
                           ("address_x", unreal.TextureAddress.TA_CLAMP),
                           ("address_y", unreal.TextureAddress.TA_CLAMP), ("never_stream", True)):
            if texture.get_editor_property(key) != value:
                raise RuntimeError(f"{texture.get_path_name()} texture flag differs: {key}")
        if source_textures and texture != source_textures[role]:
            raise RuntimeError(f"{asset.get_path_name()} {role} source texture changed")
    parameters = {name: vectors[name].get_editor_property("default_value")
                  for name in vectors}
    parameters["CityTransparency"] = scalar.get_editor_property("default_value")
    if source_parameters:
        for name in vectors:
            if any(abs(getattr(parameters[name], channel) - getattr(source_parameters[name], channel)) > 1e-6
                   for channel in "rgba"):
                raise RuntimeError(f"{asset.get_path_name()} {name} value changed")
        if abs(parameters["CityTransparency"] - source_parameters["CityTransparency"]) > 1e-6:
            raise RuntimeError(f"{asset.get_path_name()} CityTransparency changed")
    input_names = tuple(str(item.get_editor_property("input_name"))
                        for item in custom.get_editor_property("inputs"))
    upstream = [samples["Sprite"], samples["Mask"], vectors["Color1"], vectors["Color2"],
                vectors["RendererColor"], vectors["SpriteColor"], scalar]
    if custom.get_editor_property("code") != code or custom.get_editor_property(
            "output_type") != unreal.CustomMaterialOutputType.CMOT_FLOAT4 or input_names != CUSTOM_INPUTS or \
            list(edit.get_inputs_for_material_expression(asset, custom)) != upstream:
        raise RuntimeError(f"{asset.get_path_name()} Custom code/input topology differs")
    emissive = edit.get_material_property_input_node(asset, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = edit.get_material_property_input_node(asset, unreal.MaterialProperty.MP_OPACITY)
    if emissive not in masks or opacity not in masks or emissive == opacity or \
            any(not bool(emissive.get_editor_property(key)) for key in "rgb") or \
            bool(emissive.get_editor_property("a")) or \
            any(bool(opacity.get_editor_property(key)) for key in "rgb") or \
            not bool(opacity.get_editor_property("a")) or \
            any(list(edit.get_inputs_for_material_expression(asset, item)) != [custom]
                for item in (emissive, opacity)):
        raise RuntimeError(f"{asset.get_path_name()} output topology differs")
    return {"textures": textures, "parameters": parameters, "expression_count": len(expressions)}


def metadata(model, source_material, source_sha):
    return {
        "RokCityGammaCompositeModel": model,
        "RokCityGammaCompositeScope": "registered-RT-gamma-compositor-only;normal-view-unbound;not-visual-accepted",
        "RokCityGammaCompositeSourceMaterial": source_material,
        "RokCityGammaCompositeSourceSHA256": source_sha,
        "RokCityGammaCompositeCheckpointSHA256": CHECKPOINT_SHA,
        "RokCityGammaCompositeOldHlslSHA256": ORIGINAL_CODE_SHA,
        "RokCityGammaCompositeNewHlslSHA256": sha_bytes(GAMMA_CODE.encode("utf-8")),
        "RokCityGammaCompositePass": "MTP_BeforeDOF",
        "RokCityGammaCompositeColor": "Unity-Gamma-before-UE-linear-decode",
        "RokCityGammaCompositeAlpha": "col.a*saturate(mask.r*2)*CityTransparency",
        "RokCityGammaCompositeNoSceneColor": "true",
    }


def check_metadata(asset, expected):
    for key, value in expected.items():
        if str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key)) != str(value):
            raise RuntimeError(f"{asset.get_path_name()} metadata differs: {key}")


def preflight():
    identity()
    before_dof()
    if sha(CHECKPOINT) != CHECKPOINT_SHA or sha_bytes(ORIGINAL_CODE.encode("utf-8")) != ORIGINAL_CODE_SHA:
        raise RuntimeError("Pinned old-asset checkpoint or SourceRG HLSL changed")
    checkpoint = json.loads(CHECKPOINT.read_text(encoding="utf-8-sig"))
    if checkpoint.get("schema") != "rok.parallel-migration-checkpoint.v7" or len(checkpoint["asset_snapshot"]) != 25:
        raise RuntimeError("Old-asset checkpoint structure changed")
    asset_hashes = {str(Path(row["path"]).resolve()): row["sha256"]
                    for row in checkpoint["asset_snapshot"]}
    academy_report = json.loads(ACADEMY_VERIFY.read_text(encoding="utf-8-sig"))
    resource_report = json.loads(RESOURCE_VERIFY.read_text(encoding="utf-8-sig"))
    if academy_report.get("schema") != "rok.academy_visual_assets.ue_verify.v1" or \
            not academy_report.get("passed") or \
            resource_report.get("schema") != "rok.city_resource_presentation_assets.ue_verify.v1" or \
            not resource_report.get("passed") or \
            {row["model"] for row in resource_report["models"]} != set(MODELS[1:]):
        raise RuntimeError("Source visual/native/catalog verification reports are not accepted")
    if sha(ACADEMY_CATALOG) != academy_report["source_catalog_sha256"] or \
            sha(RESOURCE_CATALOG) != resource_report["source_catalog_sha256"] or \
            sha(RESOURCE_NATIVE_VERIFIED) != resource_report["native_verified_capture_sha256"]:
        raise RuntimeError("Source catalog or verified native capture changed")
    academy_runs = list((ROOT / "Saved/UnityAcademyVisualProbe/OutputRuns").glob("*/native_render.json"))
    matched_academy_runs = [path for path in academy_runs
                            if sha(path) == academy_report["native_render_sha256"]]
    if len(matched_academy_runs) != 1:
        raise RuntimeError("Exact Academy native render report is missing or ambiguous")
    academy_native = matched_academy_runs[0]
    academy_native_verify = academy_native.with_name("native_verify.json")
    academy_create = json.loads((ROOT / "Saved/RokUnityMigration/AcademyVisualAssets/ue_asset_create.json").read_text(
        encoding="utf-8-sig"))
    if sha(academy_native_verify) != academy_create["native_verify_sha256"] or \
            not json.loads(academy_native_verify.read_text(encoding="utf-8-sig"))["passed"]:
        raise RuntimeError("Academy native source verification changed")
    records = {MODELS[0]: academy_report["assets"]}
    records.update({row["model"]: row["assets"] for row in resource_report["models"]})
    sources = {}
    for model in MODELS:
        paths = records[model]
        for role in ("material", "sprite", "mask"):
            disk = source_path(paths[role]).resolve()
            if str(disk) not in asset_hashes or sha(disk) != asset_hashes[str(disk)]:
                raise RuntimeError(f"Pinned source {model}/{role} bytes changed")
        material = unreal.load_asset(paths["material"])
        if material is None:
            raise RuntimeError(f"Old SourceRG material missing: {model}")
        topology = graph(material, ORIGINAL_CODE)
        for role in ("Sprite", "Mask"):
            if topology["textures"][role].get_path_name().split(".", 1)[0] != paths[role.lower()]:
                raise RuntimeError(f"Old SourceRG {model}/{role} texture path changed")
        sources[model] = {"paths": paths, "source_sha256": asset_hashes[str(source_path(paths["material"]).resolve())],
                          "graph": topology}
    return sources


def verify_target(model, asset, record):
    graph(asset, GAMMA_CODE, record["graph"]["textures"], record["graph"]["parameters"], before_dof())
    check_metadata(asset, metadata(model, record["paths"]["material"], record["source_sha256"]))
    return {"asset": target_path(model), "source_material": record["paths"]["material"],
            "source_material_sha256": record["source_sha256"],
            "hlsl_sha256": sha_bytes(GAMMA_CODE.encode("utf-8")),
            "expression_count": 10, "translucency_pass": "MTP_BeforeDOF"}


def create_target(model, record):
    path = target_path(model)
    existing = unreal.load_asset(path)
    if existing is not None:
        return verify_target(model, existing, record), False
    if unreal.EditorAssetLibrary.does_asset_exist(path) or source_path(path).exists():
        raise RuntimeError(f"Existing Gamma target cannot be loaded; will not overwrite: {path}")
    parent, name = path.rsplit("/", 1)
    unreal.EditorAssetLibrary.make_directory(parent)
    edit = unreal.MaterialEditingLibrary
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, parent, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(asset, unreal.Material):
        raise RuntimeError(f"Could not create Gamma material: {path}")
    for key, value in (("material_domain", unreal.MaterialDomain.MD_SURFACE),
                       ("shading_model", unreal.MaterialShadingModel.MSM_UNLIT),
                       ("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT),
                       ("two_sided", True), ("translucency_pass", before_dof())):
        asset.set_editor_property(key, value)

    def node(cls):
        return edit.create_material_expression(asset, cls)

    def connect(source, output, target, input_name):
        if not edit.connect_material_expressions(source, output, target, input_name):
            raise RuntimeError(f"{path} graph connection failed: {input_name}")

    old = record["graph"]
    samples = {}
    for role in ("Sprite", "Mask"):
        sample = node(unreal.MaterialExpressionTextureSampleParameter2D)
        sample.set_editor_property("parameter_name", role)
        sample.set_editor_property("texture", old["textures"][role])
        sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        samples[role] = sample
    vectors = {}
    for name in ("Color1", "Color2", "RendererColor", "SpriteColor"):
        vector = node(unreal.MaterialExpressionVectorParameter)
        vector.set_editor_property("parameter_name", name)
        vector.set_editor_property("default_value", old["parameters"][name])
        vectors[name] = vector
    scalar = node(unreal.MaterialExpressionScalarParameter)
    scalar.set_editor_property("parameter_name", "CityTransparency")
    scalar.set_editor_property("default_value", old["parameters"]["CityTransparency"])
    custom = node(unreal.MaterialExpressionCustom)
    custom.set_editor_property("code", GAMMA_CODE)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property("description", "Source Gamma color for registered RT; alpha unchanged")
    inputs = []
    for name in CUSTOM_INPUTS:
        item = unreal.CustomInput()
        item.set_editor_property("input_name", name)
        inputs.append(item)
    custom.set_editor_property("inputs", inputs)
    for upstream, output, name in ((samples["Sprite"], "RGBA", "MainSample"),
                                   (samples["Mask"], "RGBA", "MaskSample"),
                                   (vectors["Color1"], "RGBA", "Team1"),
                                   (vectors["Color2"], "RGBA", "Team2"),
                                   (vectors["RendererColor"], "RGBA", "Renderer"),
                                   (vectors["SpriteColor"], "RGBA", "Lighting"),
                                   (scalar, "", "Transparency")):
        connect(upstream, output, custom, name)
    rgb = node(unreal.MaterialExpressionComponentMask)
    alpha = node(unreal.MaterialExpressionComponentMask)
    for channel in "rgb":
        rgb.set_editor_property(channel, True)
        alpha.set_editor_property(channel, False)
    rgb.set_editor_property("a", False)
    alpha.set_editor_property("a", True)
    connect(custom, "", rgb, "")
    connect(custom, "", alpha, "")
    if not edit.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR) or \
            not edit.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError(f"{path} output connection failed")
    for key, value in metadata(model, record["paths"]["material"], record["source_sha256"]).items():
        unreal.EditorAssetLibrary.set_metadata_tag(asset, key, str(value))
    errors = [str(error) for error in edit.recompile_material(asset)]
    if errors:
        raise RuntimeError(f"{path} compile failed: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"{path} save failed")
    return verify_target(model, asset, record), True


def report(data, kind):
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    name = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S_%f") + f"_{os.getpid()}_{kind}.json"
    path = REPORT_DIR / name
    with path.open("x", encoding="utf-8") as stream:
        json.dump(data, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    return path


def main():
    sources = preflight()
    if os.environ.get(APPLY_ENV) != "1":
        unreal.log("ROK_CITY_GAMMA_MATERIALS_PREFLIGHT_PASS; no asset, map, or report write")
        return
    # All source/editor/graph checks precede the first target creation.
    before_dof()
    for model in MODELS:
        existing = unreal.load_asset(target_path(model))
        if existing is not None:
            verify_target(model, existing, sources[model])
        elif unreal.EditorAssetLibrary.does_asset_exist(target_path(model)) or \
                source_path(target_path(model)).exists():
            raise RuntimeError(f"Existing Gamma target cannot be loaded: {model}")
    rows = []
    for model in MODELS:
        row, created = create_target(model, sources[model])
        row["created"] = created
        row["asset_sha256"] = sha(source_path(row["asset"]))
        rows.append(row)
    # A new report path is mandatory; older evidence is never replaced.
    path = report({"schema": "rok.city_gamma_materials.create.v1", "passed": True,
                   "project": str(ROOT), "world": WORLD, "checkpoint_sha256": CHECKPOINT_SHA,
                   "scope": "Three new registered-RT Gamma materials only; ordinary view unbound; no map/actor/source asset write or visual acceptance claim.",
                   "models": rows}, "create")
    unreal.log(f"ROK_CITY_GAMMA_MATERIALS_CREATED {path}")


if __name__ == "__main__":
    main()
