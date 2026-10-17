"""Create/strictly verify three isolated city Gamma diagnostic assets in UE5.8.

Default: read-only preflight. Set ROK_CITY_GAMMA_DIAGNOSTIC_APPLY=1 to create
missing assets. Existing targets are verified and never replaced. The caller
must already have /Game/Maps/L_RokUnityGameAppCity loaded in the right project.
No actor, map, old asset, post-process volume, or global blendable is changed.
"""

import hashlib
import json
import os
import re
import struct
from datetime import datetime, timezone
from pathlib import Path

import unreal

import CreateRokCityGammaMaterials as gamma


ROOT = gamma.ROOT
WORLD = gamma.WORLD
APPLY_ENV = "ROK_CITY_GAMMA_DIAGNOSTIC_APPLY"
REPAIR_ENV = "ROK_CITY_GAMMA_REPAIR_UNSEALED_OUTPUT"
SOURCE = ROOT / "Saved/UnityCityGammaCompositeProbe/OutputRuns/20261008_171930_737_10560/pattern_source.png"
SOURCE_SHA = "c98ccb2de10f61fac604813eb09107a35351bc98d3cc5d13a8c62f6b5d066705"
DEST = "/Game/RokDiagnostics/CityGammaComposite"
TEXTURE = DEST + "/T_CityGammaPatternSource"
PATTERN = DEST + "/M_CityGammaAuditPattern"
OUTPUT = DEST + "/M_CityGammaExactOutput"
REPORT_DIR = ROOT / "Saved/RokUnityMigration/CityGammaDiagnosticAssets"
BAD_OUTPUT_ASSET_SHA = "8f5ab99fe98b999ae51d7df78e0a00118f37fbd500766f5fd0f648ecc0fb964a"
BAD_OUTPUT_CODE_SHA = "db7337c8e5a23be103633b2d3b8596983173840e8ef5dd4c1ec5902f22e7d223"
BAD_OUTPUT_CODE = """float3 linear = max(SceneRGB / View.PreExposure, 0.0);
return lerp(linear * 12.92,
    1.055 * pow(linear, 1.0 / 2.4) - 0.055,
    step(0.0031308, linear));
"""
BAD_RUN_LOG = ROOT / "Saved/Logs/CityGammaAssets1_20261009.log"
DIAGNOSTIC_CREATE_REPORT = REPORT_DIR / "20261008_173704_596317_46652_create.json"
VARIANT_CREATE_REPORT = ROOT / "Saved/RokUnityMigration/CityGammaMaterials/20261008_173700_759485_46652_create.json"
PATTERN_CODE = """float3 gammaColor = max(PatternGamma.rgb, 0.0);
return lerp(gammaColor / 12.92,
    pow(max((gammaColor + 0.055) / 1.055, 0.0), 2.4),
    step(0.04045, gammaColor));
"""
OUTPUT_CODE = """float3 LinearColor = max(SceneRGB.rgb / View.PreExposure, 0.0);
return lerp(LinearColor * 12.92,
    1.055 * pow(LinearColor, 1.0 / 2.4) - 0.055,
    step(0.0031308, LinearColor));
"""


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def enum_value(cls, *names):
    enum = getattr(unreal, cls, None)
    if enum is None:
        raise RuntimeError(f"UE5.8 enum {cls} unavailable")
    for name in names:
        value = getattr(enum, name, None)
        if value is not None:
            return value
    raise RuntimeError(f"UE5.8 {cls} has none of the source-named values {names}")


def replacing_tonemapper():
    return enum_value("BlendableLocation", "BL_REPLACING_TONEMAPPER", "BL_ReplacingTonemapper")


def postprocess_input0():
    return enum_value("SceneTextureId", "PPI_POST_PROCESS_INPUT0", "PPI_PostProcessInput0")


def disk_path(asset_path):
    return ROOT / "Content" / (asset_path.removeprefix("/Game/") + ".uasset")


def texture_flags():
    return {"srgb": False,
            "compression_settings": unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP,
            "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            "filter": unreal.TextureFilter.TF_NEAREST,
            "address_x": unreal.TextureAddress.TA_CLAMP,
            "address_y": unreal.TextureAddress.TA_CLAMP,
            "never_stream": True}


def expected_metadata(role):
    common = {"RokCityGammaDiagnosticScope": "isolated-scene-capture-only;not-global;not-gameplay-accepted",
              "RokCityGammaDiagnosticSourcePngSHA256": SOURCE_SHA,
              "RokCityGammaDiagnosticOldAssetCheckpointSHA256": gamma.CHECKPOINT_SHA,
              "RokCityGammaDiagnosticNativePath": str(SOURCE.relative_to(ROOT)).replace("\\", "/"),
              "RokCityGammaDiagnosticRole": role}
    if role == "pattern_material":
        common.update({"RokCityGammaDiagnosticHlslSHA256": hashlib.sha256(PATTERN_CODE.encode()).hexdigest(),
                       "RokCityGammaDiagnosticTransfer": "raw-Gamma-pattern-to-linear-Emissive"})
    if role == "exact_output_material":
        common.update({"RokCityGammaDiagnosticHlslSHA256": hashlib.sha256(OUTPUT_CODE.encode()).hexdigest(),
                       "RokCityGammaDiagnosticTransfer": "PostProcessInput0-divide-View.PreExposure-then-sRGB-OETF",
                       "RokCityGammaDiagnosticBlendable": "explicit-scene-capture-only;BL_ReplacingTonemapper",
                       "RokCityGammaDiagnosticRequiredRT": "force_linear_gamma=1;no-second-sRGB-encode"})
    return common


def write_metadata(asset, role):
    for key, value in expected_metadata(role).items():
        unreal.EditorAssetLibrary.set_metadata_tag(asset, key, str(value))


def check_metadata(asset, role):
    for key, value in expected_metadata(role).items():
        if str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key)) != str(value):
            raise RuntimeError(f"{asset.get_path_name()} metadata differs: {key}")


def preflight():
    gamma.preflight()  # Project/world + old 25-asset checkpoint and nine exact SourceRG inputs.
    pattern_bytes = SOURCE.read_bytes()
    if sha(SOURCE) != SOURCE_SHA or pattern_bytes[:8] != b"\x89PNG\r\n\x1a\n" or \
            struct.unpack(">II", pattern_bytes[16:24]) != (1280, 720):
        raise RuntimeError("Pinned native 1280x720 pattern_source.png changed")
    replacing_tonemapper()
    postprocess_input0()
    if "View.PreExposure" not in OUTPUT_CODE or "SceneRGB.rgb / View.PreExposure" not in OUTPUT_CODE or \
            "step(0.0031308, LinearColor)" not in OUTPUT_CODE or \
            "step(0.04045, gammaColor)" not in PATTERN_CODE:
        raise RuntimeError("Exact diagnostic transfer code changed")
    return {"pattern_source_sha256": SOURCE_SHA, "old_checkpoint_sha256": gamma.CHECKPOINT_SHA}


def verify_texture(asset):
    if not isinstance(asset, unreal.Texture2D):
        raise RuntimeError("CityGamma diagnostic pattern is not Texture2D")
    expected_size = [1280, 720]
    before_size = [int(asset.blueprint_get_size_x()), int(asset.blueprint_get_size_y())]
    after_size = before_size
    if before_size != expected_size:
        # UE5.8 can expose a 32x32 placeholder while the loaded texture's
        # PlatformData is compiling. Finish that work in the verified world.
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = subsystem.get_editor_world() if subsystem else None
        if world is None or world.get_name() != WORLD:
            raise RuntimeError(f"CityGamma diagnostic texture cannot finish compilation outside {WORLD}; "
                               f"before={before_size} expected={expected_size}")
        unreal.SystemLibrary.execute_console_command(world, "Editor.AsyncTextureCompilationFinishAll")
        after_size = [int(asset.blueprint_get_size_x()), int(asset.blueprint_get_size_y())]
        unreal.log(f"ROK_CITY_GAMMA_DIAGNOSTIC_TEXTURE_SIZE before={before_size} after={after_size} "
                   f"expected={expected_size}")
    if after_size != expected_size:
        raise RuntimeError(f"CityGamma diagnostic pattern dimensions differ: "
                           f"before={before_size} after={after_size} expected={expected_size}")
    for key, value in texture_flags().items():
        if asset.get_editor_property(key) != value:
            raise RuntimeError(f"CityGamma diagnostic pattern flag differs: {key}")
    check_metadata(asset, "raw_pattern_texture")
    return {"asset": TEXTURE, "size": [1280, 720], "source_png_sha256": SOURCE_SHA}


def material_common(asset, domain):
    if not isinstance(asset, unreal.Material):
        raise RuntimeError(f"CityGamma diagnostic material missing: {asset}")
    expected = {"material_domain": domain, "shading_model": unreal.MaterialShadingModel.MSM_UNLIT,
                "blend_mode": unreal.BlendMode.BLEND_OPAQUE, "two_sided": True}
    for key, value in expected.items():
        if asset.get_editor_property(key) != value:
            raise RuntimeError(f"{asset.get_path_name()} material property differs: {key}")


def verify_pattern(asset, texture):
    material_common(asset, unreal.MaterialDomain.MD_SURFACE)
    edit = unreal.MaterialEditingLibrary
    expressions = list(edit.get_material_expressions(asset))
    if len(expressions) != 2:
        raise RuntimeError("Pattern material must contain exactly TextureSampleParameter2D + Custom")
    samples = [node for node in expressions if isinstance(node, unreal.MaterialExpressionTextureSampleParameter2D)]
    customs = [node for node in expressions if isinstance(node, unreal.MaterialExpressionCustom)]
    if len(samples) != 1 or len(customs) != 1:
        raise RuntimeError("Pattern material node types differ")
    sample, custom = samples[0], customs[0]
    if str(sample.get_editor_property("parameter_name")) != "SourcePattern" or \
            sample.get_editor_property("texture") != texture or \
            sample.get_editor_property("sampler_type") != unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR or \
            custom.get_editor_property("code") != PATTERN_CODE or \
            custom.get_editor_property("output_type") != unreal.CustomMaterialOutputType.CMOT_FLOAT3 or \
            [str(item.get_editor_property("input_name")) for item in custom.get_editor_property("inputs")] != ["PatternGamma"] or \
            list(edit.get_inputs_for_material_expression(asset, custom)) != [sample] or \
            edit.get_material_property_input_node(asset, unreal.MaterialProperty.MP_EMISSIVE_COLOR) != custom:
        raise RuntimeError("Pattern material texture/EOTF/Emissive graph differs")
    check_metadata(asset, "pattern_material")
    return {"asset": PATTERN, "expression_count": 2,
            "hlsl_sha256": hashlib.sha256(PATTERN_CODE.encode()).hexdigest()}


def verify_output(asset):
    material_common(asset, unreal.MaterialDomain.MD_POST_PROCESS)
    if asset.get_editor_property("blendable_location") != replacing_tonemapper():
        raise RuntimeError("ExactOutput is not BL_ReplacingTonemapper")
    edit = unreal.MaterialEditingLibrary
    expressions = list(edit.get_material_expressions(asset))
    if len(expressions) != 2:
        raise RuntimeError("ExactOutput must contain exactly SceneTexture + Custom")
    scenes = [node for node in expressions if isinstance(node, unreal.MaterialExpressionSceneTexture)]
    customs = [node for node in expressions if isinstance(node, unreal.MaterialExpressionCustom)]
    if len(scenes) != 1 or len(customs) != 1:
        raise RuntimeError("ExactOutput node types differ")
    scene, custom = scenes[0], customs[0]
    if scene.get_editor_property("scene_texture_id") != postprocess_input0() or \
            custom.get_editor_property("code") != OUTPUT_CODE or \
            custom.get_editor_property("output_type") != unreal.CustomMaterialOutputType.CMOT_FLOAT3 or \
            [str(item.get_editor_property("input_name")) for item in custom.get_editor_property("inputs")] != ["SceneRGB"] or \
            list(edit.get_inputs_for_material_expression(asset, custom)) != [scene] or \
            edit.get_material_property_input_node(asset, unreal.MaterialProperty.MP_EMISSIVE_COLOR) != custom:
        raise RuntimeError("ExactOutput PostProcessInput0/OETF/Emissive graph differs")
    check_metadata(asset, "exact_output_material")
    return {"asset": OUTPUT, "expression_count": 2,
            "blendable_location": "BL_ReplacingTonemapper", "scene_texture": "PostProcessInput0",
            "hlsl_sha256": hashlib.sha256(OUTPUT_CODE.encode()).hexdigest(),
            "required_rt_force_linear_gamma": 1.0}


def verify_all():
    preflight()
    texture = unreal.load_asset(TEXTURE)
    pattern = unreal.load_asset(PATTERN)
    output = unreal.load_asset(OUTPUT)
    rows = {"texture": verify_texture(texture), "pattern_material": verify_pattern(pattern, texture),
            "exact_output_material": verify_output(output)}
    for row in rows.values():
        row["asset_sha256"] = sha(disk_path(row["asset"]))
    return rows


def create_texture():
    existing = unreal.load_asset(TEXTURE)
    if existing is not None:
        return verify_texture(existing), False
    if unreal.EditorAssetLibrary.does_asset_exist(TEXTURE) or disk_path(TEXTURE).exists():
        raise RuntimeError("Existing diagnostic pattern cannot be loaded; refusing overwrite")
    task = unreal.AssetImportTask()
    for key, value in {"filename": str(SOURCE), "destination_path": DEST,
                       "destination_name": "T_CityGammaPatternSource", "factory": unreal.TextureFactory(),
                       "automated": True, "replace_existing": False, "save": False}.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(TEXTURE)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Raw diagnostic PNG import failed")
    for key, value in texture_flags().items():
        texture.set_editor_property(key, value)
    write_metadata(texture, "raw_pattern_texture")
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError("Raw diagnostic texture save failed")
    return verify_texture(texture), True


def custom_expression(material, code, input_name):
    custom = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionCustom)
    custom.set_editor_property("code", code)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    source_input = unreal.CustomInput()
    source_input.set_editor_property("input_name", input_name)
    custom.set_editor_property("inputs", [source_input])
    return custom


def create_material(path, role, texture=None):
    existing = unreal.load_asset(path)
    if existing is not None:
        return (verify_pattern(existing, texture) if role == "pattern_material" else verify_output(existing)), False
    if unreal.EditorAssetLibrary.does_asset_exist(path) or disk_path(path).exists():
        raise RuntimeError(f"Existing diagnostic material cannot be loaded: {path}")
    edit = unreal.MaterialEditingLibrary
    name = path.rsplit("/", 1)[1]
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(asset, unreal.Material):
        raise RuntimeError(f"Diagnostic material creation failed: {path}")
    asset.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE
                              if role == "pattern_material" else unreal.MaterialDomain.MD_POST_PROCESS)
    asset.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    asset.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    asset.set_editor_property("two_sided", True)
    if role == "pattern_material":
        sample = edit.create_material_expression(asset, unreal.MaterialExpressionTextureSampleParameter2D)
        sample.set_editor_property("parameter_name", "SourcePattern")
        sample.set_editor_property("texture", texture)
        sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        custom = custom_expression(asset, PATTERN_CODE, "PatternGamma")
        upstream, output = sample, "RGBA"
    else:
        asset.set_editor_property("blendable_location", replacing_tonemapper())
        scene = edit.create_material_expression(asset, unreal.MaterialExpressionSceneTexture)
        scene.set_editor_property("scene_texture_id", postprocess_input0())
        custom = custom_expression(asset, OUTPUT_CODE, "SceneRGB")
        upstream, output = scene, "Color"
    if not edit.connect_material_expressions(upstream, output, custom,
                                             "PatternGamma" if role == "pattern_material" else "SceneRGB") or \
            not edit.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError(f"Diagnostic material graph connection failed: {path}")
    write_metadata(asset, role)
    finish_material_compilation(asset, "before save")
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"Diagnostic material save failed: {path}")
    wait_saved_material_compilation(asset)
    return (verify_pattern(asset, texture) if role == "pattern_material" else verify_output(asset)), True


def finish_material_compilation(asset, stage):
    edit = unreal.MaterialEditingLibrary
    initial_errors = [str(value) for value in edit.recompile_material(asset)]
    # UE5.8 GetStatistics calls FMaterialResource::FinishCompilation() internally.
    # RecompileMaterial alone can return before shader worker errors arrive.
    edit.get_statistics(asset)
    final_errors = [str(value) for value in edit.recompile_material(asset)]
    edit.get_statistics(asset)
    settled_errors = [str(value) for value in edit.recompile_material(asset)]
    edit.get_statistics(asset)
    errors = initial_errors + final_errors + settled_errors
    if errors:
        raise RuntimeError(f"Diagnostic material compilation failed {asset.get_path_name()} {stage}: {errors}")
    return {"initial_errors": initial_errors, "final_errors": final_errors,
            "settled_errors": settled_errors, "wait": "MaterialEditingLibrary.GetStatistics/FinishCompilation"}


def wait_saved_material_compilation(asset):
    # GetStatistics waits for material shader jobs without PostEditChange or
    # MarkPackageDirty; RecompileMaterial after SaveLoadedAsset dirties the package.
    unreal.MaterialEditingLibrary.get_statistics(asset)
    return {"wait": "MaterialEditingLibrary.GetStatistics/FinishCompilation",
            "recompile_after_save": False}


def pinned_unsealed_assets():
    if hashlib.sha256(BAD_OUTPUT_CODE.encode()).hexdigest() != BAD_OUTPUT_CODE_SHA:
        raise RuntimeError("Bad first-run HLSL pin changed")
    checkpoint = json.loads(gamma.CHECKPOINT.read_text(encoding="utf-8-sig"))
    old_paths = {str(Path(row["path"]).resolve()).lower() for row in checkpoint["asset_snapshot"]}
    if str(disk_path(OUTPUT).resolve()).lower() in old_paths:
        raise RuntimeError("ExactOutput is present in the sealed old-asset checkpoint")
    diagnostic = json.loads(DIAGNOSTIC_CREATE_REPORT.read_text(encoding="utf-8-sig"))
    variants = json.loads(VARIANT_CREATE_REPORT.read_text(encoding="utf-8-sig"))
    if diagnostic.get("schema") != "rok.city_gamma_diagnostic_assets.create.v1" or \
            not diagnostic.get("passed") or \
            diagnostic["assets"]["exact_output_material"]["asset_sha256"] != BAD_OUTPUT_ASSET_SHA or \
            diagnostic["assets"]["exact_output_material"]["hlsl_sha256"] != BAD_OUTPUT_CODE_SHA or \
            variants.get("schema") != "rok.city_gamma_materials.create.v1" or not variants.get("passed"):
        raise RuntimeError("First-run unsealed asset provenance does not match")
    protected = {}
    for role in ("texture", "pattern_material"):
        row = diagnostic["assets"][role]
        protected[row["asset"]] = row["asset_sha256"]
    for row in variants["models"]:
        protected[row["asset"]] = row["asset_sha256"]
    if len(protected) != 5:
        raise RuntimeError("Expected exactly five unrelated new assets")
    for path, expected in protected.items():
        if sha(disk_path(path)) != expected:
            raise RuntimeError(f"Protected new asset changed: {path}")
    log = BAD_RUN_LOG.read_text(encoding="utf-8-sig", errors="replace")
    if "Failed to compile Material /Game/RokDiagnostics/CityGammaComposite/M_CityGammaExactOutput" not in log or \
            "syntax error: unexpected token 'linear'" not in log:
        raise RuntimeError("Pinned first-run shader failure log missing")
    return protected


def active_shader_log():
    command_line = str(unreal.SystemLibrary.get_command_line())
    match = re.search(r'(?:^|\s)-abslog=(?:"([^"]+)"|(\S+))', command_line, re.IGNORECASE)
    if match is None:
        raise RuntimeError("Guarded repair requires a dedicated -abslog= path for shader error inspection")
    path = Path(match.group(1) or match.group(2)).resolve()
    if not path.is_relative_to((ROOT / "Saved/Logs").resolve()) or path == BAD_RUN_LOG.resolve():
        raise RuntimeError("Repair -abslog must be a new path under this project's Saved/Logs")
    return path


def repair_unsealed_output():
    protected = pinned_unsealed_assets()
    target = disk_path(OUTPUT)
    if sha(target) != BAD_OUTPUT_ASSET_SHA:
        raise RuntimeError("ExactOutput is not the one known broken unsealed asset")
    asset = unreal.load_asset(OUTPUT)
    if not isinstance(asset, unreal.Material):
        raise RuntimeError("ExactOutput cannot be loaded for guarded repair")
    material_common(asset, unreal.MaterialDomain.MD_POST_PROCESS)
    edit = unreal.MaterialEditingLibrary
    customs = [node for node in edit.get_material_expressions(asset)
               if isinstance(node, unreal.MaterialExpressionCustom)]
    if len(customs) != 1 or customs[0].get_editor_property("code") != BAD_OUTPUT_CODE or \
            hashlib.sha256(customs[0].get_editor_property("code").encode()).hexdigest() != BAD_OUTPUT_CODE_SHA:
        raise RuntimeError("ExactOutput Custom HLSL is not the pinned failed first-run code")
    # The original graph and metadata must match the first run apart from its HLSL hash.
    expressions = list(edit.get_material_expressions(asset))
    scenes = [node for node in expressions if isinstance(node, unreal.MaterialExpressionSceneTexture)]
    if len(expressions) != 2 or len(scenes) != 1 or \
            scenes[0].get_editor_property("scene_texture_id") != postprocess_input0() or \
            customs[0].get_editor_property("output_type") != unreal.CustomMaterialOutputType.CMOT_FLOAT3 or \
            [str(item.get_editor_property("input_name")) for item in customs[0].get_editor_property("inputs")] != ["SceneRGB"] or \
            list(edit.get_inputs_for_material_expression(asset, customs[0])) != scenes or \
            edit.get_material_property_input_node(asset, unreal.MaterialProperty.MP_EMISSIVE_COLOR) != customs[0]:
        raise RuntimeError("ExactOutput first-run graph differs")
    if asset.get_editor_property("blendable_location") != replacing_tonemapper() or \
            str(unreal.EditorAssetLibrary.get_metadata_tag(asset, "RokCityGammaDiagnosticHlslSHA256")) != BAD_OUTPUT_CODE_SHA:
        raise RuntimeError("ExactOutput original material or metadata differs")
    for key, value in expected_metadata("exact_output_material").items():
        if key != "RokCityGammaDiagnosticHlslSHA256" and \
                str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key)) != str(value):
            raise RuntimeError(f"ExactOutput first-run metadata differs: {key}")
    runtime_log = active_shader_log()
    if runtime_log.exists():
        runtime_log_start = runtime_log.stat().st_size
    else:
        raise RuntimeError("Repair -abslog file has not been opened by this Editor process")
    backup_dir = REPORT_DIR / "Backups"
    backup_dir.mkdir(parents=True, exist_ok=True)
    backup = backup_dir / (datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S_%f") +
                           f"_{os.getpid()}_M_CityGammaExactOutput_bad.uasset")
    original_bytes = target.read_bytes()
    with backup.open("xb") as stream:
        stream.write(original_bytes)
    if sha(backup) != BAD_OUTPUT_ASSET_SHA:
        raise RuntimeError("ExactOutput backup hash mismatch; refusing repair")
    customs[0].set_editor_property("code", OUTPUT_CODE)
    write_metadata(asset, "exact_output_material")
    before_save_compile = finish_material_compilation(asset, "repair before save")
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError("Repaired ExactOutput save failed")
    after_save_compile = wait_saved_material_compilation(asset)
    new_log = runtime_log.read_bytes()[runtime_log_start:].decode("utf-8", errors="replace")
    if re.search(r"Failed to compile Material .*M_CityGammaExactOutput|"
                 r"M_CityGammaExactOutput[^\r\n]*error:|"
                 r"/Engine/Generated/Material\.ush[^\r\n]*error:", new_log, re.IGNORECASE):
        raise RuntimeError(f"Repaired ExactOutput shader failed in current Editor log: {runtime_log}")
    verify_output(asset)
    for path, expected in protected.items():
        if sha(disk_path(path)) != expected:
            raise RuntimeError(f"Repair changed unrelated new asset: {path}")
    after_sha = sha(target)
    if after_sha == BAD_OUTPUT_ASSET_SHA:
        raise RuntimeError("ExactOutput disk bytes did not change after repair")
    path = report({"schema": "rok.city_gamma_diagnostic_assets.repair.v1", "passed": True,
                   "project": str(ROOT), "world": WORLD, "asset": OUTPUT,
                   "reason": "First unsealed ExactOutput Custom HLSL used reserved word linear",
                   "failed_run_log": str(BAD_RUN_LOG), "failed_run_log_sha256": sha(BAD_RUN_LOG),
                   "repair_run_log": str(runtime_log),
                   "bad_hlsl_sha256": BAD_OUTPUT_CODE_SHA,
                   "corrected_hlsl_sha256": hashlib.sha256(OUTPUT_CODE.encode()).hexdigest(),
                   "backup": str(backup), "backup_sha256": sha(backup),
                   "before_asset_sha256": BAD_OUTPUT_ASSET_SHA, "after_asset_sha256": after_sha,
                   "protected_new_asset_sha256": protected,
                   "before_save_compile": before_save_compile,
                   "after_save_compile": after_save_compile,
                   "scope": "One guarded repair of an unsealed diagnostic output material; runtime visual acceptance pending."},
                  "repair")
    unreal.log(f"ROK_CITY_GAMMA_DIAGNOSTIC_REPAIRED {path}")


def report(data, kind):
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    filename = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S_%f") + f"_{os.getpid()}_{kind}.json"
    path = REPORT_DIR / filename
    with path.open("x", encoding="utf-8") as stream:
        json.dump(data, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    return path


def main():
    preflight()
    if os.environ.get(REPAIR_ENV) == "1":
        repair_unsealed_output()
        return
    if os.environ.get(APPLY_ENV) != "1":
        unreal.log("ROK_CITY_GAMMA_DIAGNOSTIC_PREFLIGHT_PASS; no asset/map/report write")
        return
    # Validate every existing target before any new asset is created.
    existing_texture = unreal.load_asset(TEXTURE)
    if existing_texture is not None:
        verify_texture(existing_texture)
    elif unreal.EditorAssetLibrary.does_asset_exist(TEXTURE) or disk_path(TEXTURE).exists():
        raise RuntimeError("Unreadable existing diagnostic texture")
    for path, role in ((PATTERN, "pattern_material"), (OUTPUT, "exact_output_material")):
        existing = unreal.load_asset(path)
        if existing is not None:
            if role == "pattern_material":
                if existing_texture is None:
                    raise RuntimeError("Existing pattern material has no verified diagnostic texture")
                verify_pattern(existing, existing_texture)
            else:
                verify_output(existing)
        elif unreal.EditorAssetLibrary.does_asset_exist(path) or disk_path(path).exists():
            raise RuntimeError(f"Unreadable existing diagnostic material: {path}")
    unreal.EditorAssetLibrary.make_directory(DEST)
    texture_row, texture_created = create_texture()
    texture = unreal.load_asset(TEXTURE)
    pattern_row, pattern_created = create_material(PATTERN, "pattern_material", texture)
    output_row, output_created = create_material(OUTPUT, "exact_output_material")
    rows = verify_all()
    rows["texture"]["created"] = texture_created
    rows["pattern_material"]["created"] = pattern_created
    rows["exact_output_material"]["created"] = output_created
    path = report({"schema": "rok.city_gamma_diagnostic_assets.create.v1", "passed": True,
                   "project": str(ROOT), "world": WORLD, "source_pattern_sha256": SOURCE_SHA,
                   "old_asset_checkpoint_sha256": gamma.CHECKPOINT_SHA,
                   "scope": "Three new isolated diagnostic assets; output material only for explicit SceneCapture blendable and force-linear RT. No global postprocess, actor/map/old asset write or runtime visual acceptance.",
                   "assets": rows}, "create")
    unreal.log(f"ROK_CITY_GAMMA_DIAGNOSTIC_CREATED {path}")


if __name__ == "__main__":
    main()
