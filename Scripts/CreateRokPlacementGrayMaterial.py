r"""Prepare source proof, then create one dedicated Placement UI gray material.

Offline: C:\Python312\python.exe Scripts\CreateRokPlacementGrayMaterial.py --prepare-only
UE 5.8 Editor Python, exact Rok/GameApp world only: ... --create
This does not update the map or any existing asset. Runtime RHI/pixel acceptance is separate.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import uuid
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Saved/RokUnityMigration/BatchReview20261009ConstructionVisual"
SOURCE_REPORT = OUT / "placement_gray_material_source_v3.json"
CREATE_REPORT_ROOT = OUT / "PlacementGrayMaterial"
SHADER = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UIShader/ImageGrayShader.shader"
SHADER_META = Path(str(SHADER) + ".meta")
UNITY_MATERIAL = ROOT / "ref/client-unity/Assets/BundleAssets/Default/UI_GrayWithMask.mat"
UNITY_MATERIAL_META = Path(str(UNITY_MATERIAL) + ".meta")
UNITY_PROJECT_SETTINGS = ROOT / "ref/client-unity/ProjectSettings/ProjectSettings.asset"
ATLAS_MANIFEST = ROOT / "Saved/RokUnityMigration/PlacementUiAtlas/20261008_221417_249217_45548/atlas_manifest.json"
ATLAS_IMPORT = ROOT / "Saved/RokUnityMigration/PlacementUiAtlas/20261008_221417_249217_45548/ue_import_report.json"
ATLAS_ASSET = ROOT / "Content/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud.uasset"
SOURCE_SHADER_SHA256 = "3af4b57bb8519c4dadc5f79a34aa2106e0c6f548686b001a016b5e39f608b273"
ATLAS_IMPORT_SHA256 = "450b07d353293173cb021b9ae491525cf9f20b6968fe22c0fc88efa2cc19ad66"
ATLAS = "/Game/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud"
PACKAGE = "/Game/RokPrototype/UI/PlacementAtlases"
NAME = "M_RokPlacementGrayUI"
TARGET = f"{PACKAGE}/{NAME}"
UASSET = ROOT / "Content/RokPrototype/UI/PlacementAtlases/M_RokPlacementGrayUI.uasset"

# Unity 2019 Gamma ImageGrayShader: tex2D(_MainTex) * IN.color, then the source
# weights (.299,.587,.184). SourceTint is a *Gamma numeric* UI vertex tint; no
# UE VertexColor node is read here. Slate applies the UImage/parent color after
# material evaluation, so the owner must keep UImage color white and apply only
# any intended parent fade there. The texture sample RGB is sRGB-decoded by UE.
GRAY_HLSL = """float3 sampleLinear = max(SampleRGBA.rgb, 0.0);
float3 sampleGamma = lerp(12.92 * sampleLinear,
    1.055 * pow(sampleLinear, 1.0 / 2.4) - 0.055,
    step(0.0031308, sampleLinear));
float3 tintedGamma = sampleGamma * SourceTint.rgb;
float grayGamma = saturate(dot(tintedGamma, float3(0.299, 0.587, 0.184)));
float grayLinear = grayGamma <= 0.04045 ? grayGamma / 12.92 :
    pow((grayGamma + 0.055) / 1.055, 2.4);
return float4(grayLinear, grayLinear, grayLinear,
    saturate(SampleRGBA.a * SourceTint.a));"""


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_proof() -> dict:
    shader = SHADER.read_text(encoding="utf-8-sig")
    material = UNITY_MATERIAL.read_text(encoding="utf-8-sig")
    shader_meta = SHADER_META.read_text(encoding="utf-8-sig")
    assert sha(SHADER) == SOURCE_SHADER_SHA256, "Unity gray shader source changed"
    guid_match = re.search(r"^guid: ([0-9a-f]{32})$", shader_meta, re.M)
    assert guid_match, "Unity shader GUID missing"
    guid = guid_match.group(1)
    assert f"m_Shader: {{fileID: 4800000, guid: {guid}, type: 3}}" in material
    for exact in (
        'Shader "UI/Default Gray with Mask"',
        "OUT.color = IN.color * _Color;",
        "half4 color = tex2D(_MainTex, IN.texcoord) * IN.color;",
        "float c = 0.299*color.r + 0.587*color.g + 0.184*color.b;",
        "color.r = color.g = color.b = c;",
        "color.a *= UnityGet2DClipping(IN.worldPosition.xy, _ClipRect);",
    ):
        assert exact in shader, f"Unity gray shader expression changed: {exact}"
    assert "_UseUIAlphaClip: 0" in material
    assert "_Color: {r: 1, g: 1, b: 1, a: 1}" in material
    assert re.search(r"^\s*m_ActiveColorSpace: 0\s*$",
                     UNITY_PROJECT_SETTINGS.read_text(encoding="utf-8-sig"), re.M), \
        "Unity source project is not Gamma color space"
    atlas_source = json.loads(ATLAS_MANIFEST.read_text(encoding="utf-8"))
    atlas_import = json.loads(ATLAS_IMPORT.read_text(encoding="utf-8"))
    assert atlas_source["schema"] == "rok.placement_ui_atlas_source.v1"
    assert atlas_import["schema"] == "rok.placement_ui_atlas_import.v1"
    assert atlas_source["ue_target"] == atlas_import["asset"] == ATLAS
    assert atlas_import["passed"] is True and atlas_import["manifest_sha256"] == sha(ATLAS_MANIFEST)
    assert atlas_import["asset_uasset_sha256"] == sha(ATLAS_ASSET) == ATLAS_IMPORT_SHA256
    assert atlas_import["texture_settings"]["srgb"] == "True"
    assert atlas_import["texture_settings"]["never_stream"] == "True"
    sources = [SHADER, SHADER_META, UNITY_MATERIAL, UNITY_MATERIAL_META, UNITY_PROJECT_SETTINGS,
               ATLAS_MANIFEST, ATLAS_IMPORT, ATLAS_ASSET]
    script = Path(__file__).resolve()
    return {
        "schema": "rok.placement.gray_material_source.v3",
        "scope": "One new Placement-only UI material; source/formula graph proof, not UE RHI or pixel acceptance",
        "unity_version": "2019.4.28f1c1",
        "unity_color_space": "Gamma",
        "unity_shader": "UI/Default Gray with Mask",
        "unity_shader_guid": guid,
        "unity_material": "UI_GrayWithMask",
        "source_formula": "sampleGamma=tex2D(MainTex,UV)*GammaUIVertexTint*MaterialColor; grayGamma=saturate(.299R+.587G+.184B); alpha=sampleAlpha*vertexAlpha*materialAlpha*UnityRectMask",
        "ue_formula": "sampleLinear=SourceAtlas(sRGB, Slate UVRegion); sampleGamma=sRGBEncode(sampleLinear); grayGamma=saturate(dot(sampleGamma*SourceTint.rgb,[.299,.587,.184])); UI FinalColor=sRGBDecode(grayGamma); Opacity=sampleAlpha*SourceTint.a; Slate/UMG clipping and parent fade external",
        "tint_contract": "VectorParameter SourceTint output RGBA index 5 contains original Unity Gamma RGBA numeric values; UImage ColorAndOpacity white; parent fade may multiply once in Slate; material never reads VertexColor",
        "alpha_contract": "Straight alpha, no premultiply; source alpha clip disabled, clipping delegated to Slate/UMG",
        "uv_contract": "Slate Brush UVRegion / TextureCoordinate0; no MID UV transform",
        "hlsl": GRAY_HLSL,
        "hlsl_sha256": hashlib.sha256(GRAY_HLSL.encode("utf-8")).hexdigest(),
        "target": TARGET,
        "source_atlas": ATLAS,
        "parameters": {"SourceAtlas": ATLAS, "SourceTint": [1.0, 1.0, 1.0, 1.0]},
        "source_files": [{"path": p.relative_to(ROOT).as_posix(), "sha256": sha(p)} for p in sources],
        "producer": {"path": script.relative_to(ROOT).as_posix(), "sha256": sha(script)},
    }


def prepare() -> None:
    proof = source_proof()
    encoded = json.dumps(proof, ensure_ascii=False, indent=2) + "\n"
    OUT.mkdir(parents=True, exist_ok=True)
    if SOURCE_REPORT.exists():
        assert SOURCE_REPORT.read_text(encoding="utf-8") == encoded, "Existing proof differs; refuse overwrite"
    else:
        SOURCE_REPORT.write_text(encoded, encoding="utf-8", newline="\n")
    print(json.dumps({"prepared": True, "sourceProof": str(SOURCE_REPORT),
                      "sourceProofSha256": sha(SOURCE_REPORT), "target": TARGET,
                      "hlslSha256": proof["hlsl_sha256"]}))


def checked_rgba_binding(value: object, label: str) -> dict:
    """UE bool+one-out wraps as str/None; accept the older (True, str) form too."""
    if isinstance(value, str):
        name, raw = value, value
    elif (type(value) is tuple and len(value) == 2 and value[0] is True
          and isinstance(value[1], str)):
        name, raw = value[1], [value[0], value[1]]
    else:
        raise RuntimeError(f"{label} binding returned unsupported {type(value).__name__}: {value!r}")
    if name != "RGBA":
        raise RuntimeError(f"{label} connected {name!r}, expected RGBA output index 5")
    return {"python_type": type(value).__name__, "raw": raw, "name": name, "index": 5}


def create() -> None:
    import unreal

    project = Path(unreal.Paths.project_dir()).resolve()
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_name = world.get_name() if world else ""
    if project != ROOT.resolve() or world_name != "L_RokUnityGameAppCity":
        raise RuntimeError(f"Wrong UE target project/world: {project} / {world_name}")
    if not SOURCE_REPORT.is_file() or json.loads(SOURCE_REPORT.read_text(encoding="utf-8")) != source_proof():
        raise RuntimeError("Prepared source proof is absent or differs from current source/script/atlas")
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET) or UASSET.exists():
        raise RuntimeError("Dedicated Placement material already exists; refuse overwrite")
    atlas = unreal.load_asset(ATLAS)
    if (not isinstance(atlas, unreal.Texture2D) or not atlas.get_editor_property("srgb")
            or sha(ATLAS_ASSET) != ATLAS_IMPORT_SHA256):
        raise RuntimeError("SourceAtlas asset or sRGB/source hash differs")

    # All source and target identity checks precede the only asset mutation.
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, PACKAGE, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(material, unreal.Material) or material.get_path_name() != TARGET + "." + NAME:
        raise RuntimeError("Placement gray material creation failed")
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    edit = unreal.MaterialEditingLibrary

    def node(kind):
        return edit.create_material_expression(material, kind)

    def connect(src, pin, dest, input_name):
        if not edit.connect_material_expressions(src, pin, dest, input_name):
            raise RuntimeError(f"Placement gray graph connection failed: {input_name}")

    sample = node(unreal.MaterialExpressionTextureSampleParameter2D)
    sample.set_editor_property("parameter_name", "SourceAtlas")
    sample.set_editor_property("texture", atlas)
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    tint = node(unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property("parameter_name", "SourceTint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    shader = node(unreal.MaterialExpressionCustom)
    shader.set_editor_property("description", "Unity2019GammaImageGrayPlacementNoVertexColorV1")
    shader.set_editor_property("code", GRAY_HLSL)
    shader.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for input_name in ("SampleRGBA", "SourceTint"):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", input_name)
        inputs.append(entry)
    shader.set_editor_property("inputs", inputs)
    connect(sample, "RGBA", shader, "SampleRGBA")
    # UE 5.8 VectorParameter output 0 is RGB. The alpha-bearing RGBA output
    # is named RGBA at index 5; the Custom HLSL uses SourceTint.a.
    connect(tint, "RGBA", shader, "SourceTint")
    rgb = node(unreal.MaterialExpressionComponentMask)
    alpha = node(unreal.MaterialExpressionComponentMask)
    for channel in ("r", "g", "b"):
        rgb.set_editor_property(channel, True)
        alpha.set_editor_property(channel, False)
    rgb.set_editor_property("a", False)
    alpha.set_editor_property("a", True)
    connect(shader, "", rgb, "")
    connect(shader, "", alpha, "")
    if not edit.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Placement gray Emissive/FinalColor bind failed")
    if not edit.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError("Placement gray Opacity bind failed")
    ui = node(unreal.MaterialExpressionSubstrateUI)
    connect(rgb, "", ui, "Color")
    connect(alpha, "", ui, "Opacity")
    if not edit.connect_material_property(ui, "", unreal.MaterialProperty.MP_FRONT_MATERIAL):
        raise RuntimeError("Placement gray Substrate UI bind failed")
    unreal.EditorAssetLibrary.set_metadata_tag(material, "RokPlacementGraySourceSha256", sha(SOURCE_REPORT))
    unreal.EditorAssetLibrary.set_metadata_tag(material, "RokPlacementGrayHlslSha256", source_proof()["hlsl_sha256"])
    # Check the actual graph before the first disk save. UE5.8's Python plugin
    # PackReturnValues returns a bare FString for bool+one-out on success and
    # None on false; also accept a legacy (True, FString) wrapper.
    expressions = edit.get_material_expressions(material)
    customs = [expr for expr in expressions if isinstance(expr, unreal.MaterialExpressionCustom)]
    vertex_colors = [expr for expr in expressions if isinstance(expr, unreal.MaterialExpressionVertexColor)]
    tint_outputs = edit.get_material_expression_output_names(tint)
    sample_outputs = edit.get_material_expression_output_names(sample)
    tint_binding = edit.get_input_node_output_name_for_material_expression(shader, tint)
    sample_binding = edit.get_input_node_output_name_for_material_expression(shader, sample)
    verified_tint = checked_rgba_binding(tint_binding, "SourceTint")
    verified_sample = checked_rgba_binding(sample_binding, "SourceAtlas")
    if (len(expressions) != 6 or len(customs) != 1 or vertex_colors
            or len(tint_outputs) <= 5 or tint_outputs[5] != "RGBA"
            or len(sample_outputs) <= 5 or sample_outputs[5] != "RGBA"
            or hashlib.sha256(customs[0].get_editor_property("code").encode("utf-8")).hexdigest()
            != source_proof()["hlsl_sha256"]
            or edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_FRONT_MATERIAL) != ui
            or edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY) != alpha):
        raise RuntimeError("Placement gray graph differs before save")
    graph = []
    for expr in expressions:
        row = {"type": expr.get_class().get_name()}
        if isinstance(expr, unreal.MaterialExpressionTextureSampleParameter2D):
            row["parameter"] = str(expr.get_editor_property("parameter_name"))
            row["texture"] = expr.get_editor_property("texture").get_path_name()
        elif isinstance(expr, unreal.MaterialExpressionVectorParameter):
            row["parameter"] = str(expr.get_editor_property("parameter_name"))
            row["default"] = [1.0, 1.0, 1.0, 1.0]
        elif isinstance(expr, unreal.MaterialExpressionCustom):
            row["codeSha256"] = hashlib.sha256(expr.get_editor_property("code").encode("utf-8")).hexdigest()
            row["inputs"] = [str(x.get_editor_property("input_name")) for x in expr.get_editor_property("inputs")]
        graph.append(row)
    edit.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Placement gray material save failed")
    if not UASSET.is_file():
        raise RuntimeError("Placement gray material UAsset missing after save")
    report = {
        "schema": "rok.placement.gray_material_create.v1",
        "project": str(project), "world": world_name,
        "asset": TARGET, "uasset_sha256": sha(UASSET),
        "source_proof": SOURCE_REPORT.relative_to(ROOT).as_posix(),
        "source_proof_sha256": sha(SOURCE_REPORT),
        "script_sha256": sha(Path(__file__)),
        "source_shader_sha256": sha(SHADER),
        "atlas_uasset_sha256": sha(ATLAS_ASSET),
        "hlsl_sha256": source_proof()["hlsl_sha256"],
        "default_atlas": ATLAS,
        "source_tint_default_gamma_rgba": [1.0, 1.0, 1.0, 1.0],
        "graph": graph,
        "connections": [
            "SourceAtlas.RGBA(output_index=5) -> Custom.SampleRGBA",
            "SourceTint.RGBA(output_index=5) -> Custom.SourceTint",
            "Custom.RGB -> SubstrateUI.Color / Emissive",
            "Custom.A -> SubstrateUI.Opacity / Material.Opacity",
            "SubstrateUI -> Material.FrontMaterial",
        ],
        "verified_source_atlas_output": verified_sample,
        "verified_source_tint_output": verified_tint,
        "reads_vertex_color": False,
        "visual_verified": False,
        "rhi_verified": False,
    }
    run_dir = CREATE_REPORT_ROOT / (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
                                    + f"_{os.getpid()}_{uuid.uuid4().hex[:8]}")
    run_dir.mkdir(parents=True, exist_ok=False)
    (run_dir / "create.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                                          encoding="utf-8", newline="\n")
    print(json.dumps({"created": True, "asset": TARGET, "report": str(run_dir / "create.json"),
                      "uassetSha256": report["uasset_sha256"]}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--prepare-only", action="store_true")
    mode.add_argument("--create", action="store_true")
    args = parser.parse_args()
    if args.prepare_only:
        prepare()
    else:
        create()
