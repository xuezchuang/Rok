"""Prepare source proof or create the one dedicated Research gray UI material.

Run ``python Scripts/CreateRokResearchGrayMaterial.py --prepare-only`` first.
The normal mode is for UE 5.8 Python in the exact Rok city-map editor only.
"""
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Saved/RokUnityMigration/ResearchNative'
SOURCE_REPORT = OUT / 'gray_material_source.json'
UE_REPORT = OUT / 'gray_material_ue.json'
SHADER = ROOT / 'ref/client-unity/Assets/BundleAssets/UI/UIShader/ImageGrayShader.shader'
SHADER_META = Path(str(SHADER) + '.meta')
MATERIAL = ROOT / 'ref/client-unity/Assets/BundleAssets/Default/UI_GrayWithMask.mat'
MATERIAL_META = Path(str(MATERIAL) + '.meta')
RENDERER = ROOT / 'Scripts/RenderRokResearchNative.cs'
NATIVE = ROOT / 'Saved/UnityResearchNativeProbe/Artifacts/native_20261008_124117_758/render_evidence.json'
ATLAS_MANIFEST = OUT / 'atlas_manifest.json'
IMPORT_REPORT = OUT / 'ue_import.json'
DESTINATION = '/Game/RokPrototype/UI/Research'
NAME = 'M_RokResearchGrayUI'
TARGET = DESTINATION + '/' + NAME
DEFAULT_ATLAS = '/Game/RokPrototype/UI/ResearchAtlases/rok_research_research_icon1'
ASSET_FILE = ROOT / 'Content/RokPrototype/UI/Research/M_RokResearchGrayUI.uasset'
BEFORE_REPAIR = OUT / 'BeforeGrayRepair'
OLD_ASSET_SHA256 = 'ff02403cf7cf1375cd08260e0a758c12cea79bf42bae499950e299d3b4cb127b'
OLD_SOURCE_SHA256 = '0fdddb1bec41dd83f1bdaf704bca44108ac82cd835579e5abe8e583b89ba65b1'
OLD_HLSL_SHA256 = 'e1d0d2f77b7219a680b613ccb4c471405839a7453018275b8adad0d83a4f2458'

# Unity 2019's Gamma project runs ImageGrayShader.tex2D() arithmetic in the
# encoded color domain. UE sRGB Texture2D sampling has already decoded RGB to
# linear. Restore the encoded sample before Unity's exact (including .184 B)
# weights, then return linear color to UE/Slate's SDR output transform.
# Source fixed4 output is UNORM/clamped; the weights sum to 1.07, so saturate.
GRAY_CODE = r'''float3 linearSample = max(SampleRGBA.rgb, 0.0);
float3 encodedSample = lerp(12.92 * linearSample,
    1.055 * pow(linearSample, 1.0 / 2.4) - 0.055,
    step(0.0031308, linearSample));
float3 unityGammaColor = encodedSample * VertexRGB;
float grayGamma = saturate(dot(unityGammaColor, float3(0.299, 0.587, 0.184)));
float grayLinear = grayGamma <= 0.04045 ? grayGamma / 12.92 :
    pow((grayGamma + 0.055) / 1.055, 2.4);
return float4(grayLinear, grayLinear, grayLinear,
    saturate(SampleRGBA.a * VertexAlpha));'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_proof():
    shader = SHADER.read_text(encoding='utf-8-sig')
    material = MATERIAL.read_text(encoding='utf-8-sig')
    shader_guid = re.search(r'^guid: (\w+)', SHADER_META.read_text(encoding='utf-8-sig'), re.M).group(1)
    assert re.search(r'm_Shader: \{fileID: 4800000, guid: ' + shader_guid + r', type: 3\}', material)
    assert 'Shader "UI/Default Gray with Mask"' in shader
    assert 'half4 color = tex2D(_MainTex, IN.texcoord) * IN.color;' in shader
    assert 'float c = 0.299*color.r + 0.587*color.g + 0.184*color.b;' in shader
    assert 'color.r = color.g = color.b = c;' in shader
    assert 'color.a *= UnityGet2DClipping(IN.worldPosition.xy, _ClipRect);' in shader
    assert 'OUT.color = IN.color * _Color;' in shader
    assert '_UseUIAlphaClip: 0' in material
    assert '_Color: {r: 1, g: 1, b: 1, a: 1}' in material
    assert 'QualitySettings.activeColorSpace != ColorSpace.Gamma' in RENDERER.read_text(encoding='utf-8')
    native = json.loads(NATIVE.read_text(encoding='utf-8'))
    assert native['schema'] == 'rok.research.native.v1' and native['complete'] and not native['failure']
    assert [s['key'] for s in native['samples']] == ['economy_start', 'economy_end',
                                                     'military_start', 'military_end', 'detail_locked_102']
    assert all(n['sourceGrayShaderEmulated'] for s in native['samples'] for n in s['nodes'])
    assert any('UI_GrayWithMask' in n['icon']['materialName'] for n in native['samples'][0]['nodes'])
    atlas = json.loads(ATLAS_MANIFEST.read_text(encoding='utf-8'))
    imported = json.loads(IMPORT_REPORT.read_text(encoding='utf-8'))
    assert atlas['schema'] == 'rok.research.atlas.v1' and imported['schema'] == 'rok.research.atlas_import.v1'
    assert len(atlas['atlases']) == 4 and len(imported['imported']) == 5
    assert imported['manifestSha256'] == sha(ATLAS_MANIFEST)
    sources = [SHADER, SHADER_META, MATERIAL, MATERIAL_META, RENDERER, NATIVE, ATLAS_MANIFEST, IMPORT_REPORT]
    return {
        'schema': 'rok.research.gray_material_source.v1',
        'qualification': 'Source-faithful gray arithmetic in isolated all-unresearched presentation; SDR pixel parity remains a UE runtime visual gate.',
        'unityVersion': '2019.4.28f1c1', 'unityColorSpace': 'Gamma',
        'unityShader': 'UI/Default Gray with Mask', 'unityShaderGuid': shader_guid,
        'unityMaterial': 'UI_GrayWithMask', 'unityMaterialColor': [1, 1, 1, 1],
        'unityAlphaClipEnabled': False,
        'sourceFormula': 'sampleGamma=tex2D(MainTex,UV)*VertexColor*MaterialColor; grayGamma=clamp(.299*R+.587*G+.184*B,0,1); alpha=sampleAlpha*vertexAlpha*materialAlpha*UnityRectMask',
        'ueFormula': 'sampleLinear=SourceAtlas(sRGB Texture2D,Slate UVRegion); sampleGamma=sRGBEncode(sampleLinear); grayGamma=clamp(dot(sampleGamma*VertexColor.rgb,[.299,.587,.184]),0,1); FinalColor=sRGBDecode(grayGamma.xxx); Opacity=sampleAlpha*VertexColor.a; clipping=UMG/Slate',
        'hlsl': GRAY_CODE, 'hlslSha256': hashlib.sha256(GRAY_CODE.encode('utf-8')).hexdigest(),
        'target': TARGET, 'defaultTexture': DEFAULT_ATLAS, 'textureParameter': 'SourceAtlas',
        'shaderDomain': 'UI', 'blendMode': 'Translucent', 'twoSided': True,
        'alphaHandling': 'Straight alpha, no premultiply and no material-local clipping; source _UseUIAlphaClip is off.',
        'uvHandling': 'Use Slate Brush UVRegion with TextureCoordinate0; no MID UV scale/offset parameter.',
        'sources': [{'path': p.relative_to(ROOT).as_posix(), 'sha256': sha(p)} for p in sources],
        'producer': {'path': Path(__file__).relative_to(ROOT).as_posix(), 'sha256': sha(Path(__file__))},
    }


def prepare():
    OUT.mkdir(parents=True, exist_ok=True)
    proof = source_proof()
    SOURCE_REPORT.write_text(json.dumps(proof, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'prepared': True, 'sourceProof': str(SOURCE_REPORT),
                      'sourceProofSha256': sha(SOURCE_REPORT), 'target': TARGET,
                      'hlslSha256': proof['hlslSha256']}))


def create_or_repair(repair):
    import unreal
    project = Path(unreal.Paths.project_dir()).resolve()
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_name = world.get_name() if world else ''
    print('Research gray material identity:', project, world_name)
    if project != ROOT.resolve() or world_name != 'L_RokUnityGameAppCity':
        raise RuntimeError('Wrong Research gray material project/world')
    expected = json.loads(SOURCE_REPORT.read_text(encoding='utf-8'))
    assert expected == source_proof(), 'Source gray formula/hash changed after preparation'
    exists = unreal.EditorAssetLibrary.does_asset_exist(TARGET)
    if repair:
        if not exists or not ASSET_FILE.exists() or sha(ASSET_FILE) != OLD_ASSET_SHA256:
            raise RuntimeError('Research gray repair requires the exact known unaccepted asset hash')
        if (sha(BEFORE_REPAIR / ASSET_FILE.name) != OLD_ASSET_SHA256 or
                sha(BEFORE_REPAIR / 'gray_material_source.json') != OLD_SOURCE_SHA256):
            raise RuntimeError('Research gray original backup/hash is missing or changed')
        old_report = json.loads((BEFORE_REPAIR / 'gray_material_ue.json').read_text(encoding='utf-8'))
        if (old_report.get('asset') != TARGET or
                old_report.get('uassetSha256') != OLD_ASSET_SHA256 or
                old_report.get('sourceProofSha256') != OLD_SOURCE_SHA256 or
                old_report.get('hlslSha256') != OLD_HLSL_SHA256):
            raise RuntimeError('Research gray previous graph evidence differs from known asset')
    elif exists or ASSET_FILE.exists():
        raise RuntimeError('Dedicated Research gray material already exists; refuse to overwrite')
    atlas = unreal.load_asset(DEFAULT_ATLAS)
    if not isinstance(atlas, unreal.Texture2D) or not atlas.get_editor_property('srgb'):
        raise RuntimeError('Imported default SourceAtlas missing or no longer sRGB')
    if repair:
        material = unreal.load_asset(TARGET)
    else:
        unreal.EditorAssetLibrary.make_directory(DESTINATION)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            NAME, DESTINATION, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(material, unreal.Material) or material.get_path_name() != TARGET + '.' + NAME:
        raise RuntimeError('Research gray material create/load failed')
    edit = unreal.MaterialEditingLibrary
    if repair:
        if (unreal.EditorAssetLibrary.get_metadata_tag(material, 'RokResearchGraySourceSha256') != OLD_SOURCE_SHA256 or
                unreal.EditorAssetLibrary.get_metadata_tag(material, 'RokResearchGrayHlslSha256') != OLD_HLSL_SHA256):
            raise RuntimeError('Research gray material metadata differs from archived graph')
        expressions = edit.get_material_expressions(material)
        customs = [x for x in expressions if isinstance(x, unreal.MaterialExpressionCustom)]
        if (len(expressions) != 6 or len(customs) != 1 or
                hashlib.sha256(customs[0].get_editor_property('code').encode('utf-8')).hexdigest() != OLD_HLSL_SHA256):
            raise RuntimeError('Research gray material graph differs from archived graph')
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('two_sided', True)
    edit.delete_all_material_expressions(material)

    def node(kind):
        return edit.create_material_expression(material, kind)

    def connect(src, pin, dest, input_name):
        if not edit.connect_material_expressions(src, pin, dest, input_name):
            raise RuntimeError('Research gray material graph connection failed: ' + input_name)

    sample = node(unreal.MaterialExpressionTextureSampleParameter2D)
    sample.set_editor_property('parameter_name', 'SourceAtlas')
    sample.set_editor_property('texture', atlas)
    sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    vertex = node(unreal.MaterialExpressionVertexColor)
    shader = node(unreal.MaterialExpressionCustom)
    shader.set_editor_property('description', 'Unity2019GammaImageGrayWithMaskV1')
    shader.set_editor_property('code', GRAY_CODE)
    shader.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for name in ('SampleRGBA', 'VertexRGB', 'VertexAlpha'):
        entry = unreal.CustomInput(); entry.set_editor_property('input_name', name); inputs.append(entry)
    shader.set_editor_property('inputs', inputs)
    connect(sample, 'RGBA', shader, 'SampleRGBA')
    # UE 5.8 VertexColor's default output masks alpha off. Its A component
    # must be connected separately; D3D compiled the old default as float3.
    connect(vertex, '', shader, 'VertexRGB')
    connect(vertex, 'A', shader, 'VertexAlpha')
    rgb = node(unreal.MaterialExpressionComponentMask)
    alpha = node(unreal.MaterialExpressionComponentMask)
    for name in ('r', 'g', 'b'):
        rgb.set_editor_property(name, True)
        alpha.set_editor_property(name, False)
    rgb.set_editor_property('a', False)
    alpha.set_editor_property('a', True)
    connect(shader, '', rgb, '')
    connect(shader, '', alpha, '')
    if not edit.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Research gray FinalColor/Emissive bind failed')
    if not edit.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError('Research gray Opacity bind failed')
    ui = node(unreal.MaterialExpressionSubstrateUI)
    connect(rgb, '', ui, 'Color')
    connect(alpha, '', ui, 'Opacity')
    if not edit.connect_material_property(ui, '', unreal.MaterialProperty.MP_FRONT_MATERIAL):
        raise RuntimeError('Research gray Substrate UI FrontMaterial bind failed')
    unreal.EditorAssetLibrary.set_metadata_tag(material, 'RokResearchGraySourceSha256', sha(SOURCE_REPORT))
    unreal.EditorAssetLibrary.set_metadata_tag(material, 'RokResearchGrayShaderSha256', sha(SHADER))
    unreal.EditorAssetLibrary.set_metadata_tag(material, 'RokResearchGrayHlslSha256', expected['hlslSha256'])
    edit.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('Research gray UI material save failed')
    if (material.get_editor_property('material_domain') != unreal.MaterialDomain.MD_UI or
            material.get_editor_property('blend_mode') != unreal.BlendMode.BLEND_TRANSLUCENT or
            material.get_editor_property('two_sided') is not True or
            sample.get_editor_property('sampler_type') != unreal.MaterialSamplerType.SAMPLERTYPE_COLOR or
            edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_FRONT_MATERIAL) != ui or
            edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY) != alpha):
        raise RuntimeError('Research gray UI material graph/properties changed after save')
    if not ASSET_FILE.exists():
        raise RuntimeError('Research gray UI material uasset missing after save')
    graph = [{'type': expr.get_class().get_name(),
              'parameter': str(expr.get_editor_property('parameter_name')) if isinstance(expr, unreal.MaterialExpressionTextureSampleParameter2D) else None,
              'codeSha256': hashlib.sha256(expr.get_editor_property('code').encode('utf-8')).hexdigest() if isinstance(expr, unreal.MaterialExpressionCustom) else None}
             for expr in edit.get_material_expressions(material)]
    proof = {'schema': 'rok.research.gray_material_ue.v1', 'project': str(project), 'world': world_name,
             'asset': TARGET, 'uassetSha256': sha(ASSET_FILE), 'sourceProofSha256': sha(SOURCE_REPORT),
             'scriptSha256': sha(Path(__file__)), 'hlslSha256': expected['hlslSha256'],
             'repairOfSha256': OLD_ASSET_SHA256 if repair else None,
             'backup': BEFORE_REPAIR.relative_to(ROOT).as_posix() if repair else None,
             'defaultAtlas': DEFAULT_ATLAS, 'textureParameter': 'SourceAtlas',
             'shaderDomain': str(material.get_editor_property('material_domain')),
             'blendMode': str(material.get_editor_property('blend_mode')),
             'twoSided': material.get_editor_property('two_sided'),
             'graph': graph, 'visualVerified': False}
    UE_REPORT.write_text(json.dumps(proof, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(proof, ensure_ascii=False))


if __name__ == '__main__':
    if '--prepare-only' in sys.argv:
        prepare()
    elif '--repair' in sys.argv:
        create_or_repair(True)
    elif '--create' in sys.argv:
        create_or_repair(False)
    else:
        raise RuntimeError('Specify --prepare-only, --create, or --repair explicitly')
