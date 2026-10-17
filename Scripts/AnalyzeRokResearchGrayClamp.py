"""Read-only source/atlas audit of the Research gray material's pre-blend clamp.

Writes only a supplemental report; the saved UI material and its historical
source/UE reports remain immutable pending a real-RHI visual comparison.
"""
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / 'Saved/RokUnityMigration/ResearchNative'
SHADER = ROOT / 'ref/client-unity/Assets/BundleAssets/UI/UIShader/ImageGrayShader.shader'
ATLAS = PACKAGE / 'atlas_manifest.json'
FIXTURE = PACKAGE / 'research_fixture.json'
SOURCE = PACKAGE / 'gray_material_source.json'
UE = PACKAGE / 'gray_material_ue.json'
ASSET = ROOT / 'Content/RokPrototype/UI/Research/M_RokResearchGrayUI.uasset'
OUT = PACKAGE / 'gray_material_clamp_supplement.json'
KNOWN_ASSET_SHA = 'fdd69eac3a6457b137433ec07b8bb4c6b101e286c20c070fdb1bf8e2ff21f693'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    shader = SHADER.read_text(encoding='utf-8-sig')
    assert 'float c = 0.299*color.r + 0.587*color.g + 0.184*color.b;' in shader
    assert 'color.r = color.g = color.b = c;' in shader
    assert 'return color;' in shader and 'Blend SrcAlpha OneMinusSrcAlpha' in shader
    assert 'color.a *= UnityGet2DClipping(IN.worldPosition.xy, _ClipRect);' in shader
    source = json.loads(SOURCE.read_text(encoding='utf-8'))
    created = json.loads(UE.read_text(encoding='utf-8'))
    fixture = json.loads(FIXTURE.read_text(encoding='utf-8'))
    atlas = json.loads(ATLAS.read_text(encoding='utf-8'))
    assert 'grayGamma=clamp(' in source['sourceFormula']
    assert 'saturate(dot(' in source['hlsl']
    assert created['asset'] == '/Game/RokPrototype/UI/Research/M_RokResearchGrayUI'
    assert created['sourceProofSha256'] == sha(SOURCE)
    assert created['uassetSha256'] == sha(ASSET) == KNOWN_ASSET_SHA
    assert fixture['schema'] == 'rok.research.native_fixture.v1' and fixture['nodeCount'] == 71
    sprites = {row['reference']: row for row in atlas['sprites']}
    sheets = {row['atlasName']: row for row in atlas['atlases']}
    locked = sorted({node['icon'] for node in fixture['nodes'] if any(
        node['preconditionStudy' + str(i)] for i in range(1, 5))})
    images = {name: np.asarray(Image.open(ROOT / sheet['source']).convert('RGBA'), dtype=np.float32) / 255
              for name, sheet in sheets.items()}
    total = {'spritePixels': 0, 'fragmentGrayOverOneAtTexelCenter': 0,
             'overOneAndOpaque': 0, 'overOneAndPartiallyTransparent': 0,
             'overOneAndFullyTransparent': 0}
    maximum = 0.0
    for reference in locked:
        row = sprites[reference]
        r = row['rect']
        image = images[row['atlasName']]
        height = image.shape[0]
        pixels = image[height - r['y'] - r['height']:height - r['y'], r['x']:r['x'] + r['width']]
        assert pixels.shape[:2] == (r['height'], r['width'])
        gray = pixels[:, :, 0] * .299 + pixels[:, :, 1] * .587 + pixels[:, :, 2] * .184
        alpha = pixels[:, :, 3]
        high = gray > 1.0
        total['spritePixels'] += gray.size
        total['fragmentGrayOverOneAtTexelCenter'] += int(np.count_nonzero(high))
        total['overOneAndOpaque'] += int(np.count_nonzero(high & (alpha == 1)))
        total['overOneAndPartiallyTransparent'] += int(np.count_nonzero(high & (alpha > 0) & (alpha < 1)))
        total['overOneAndFullyTransparent'] += int(np.count_nonzero(high & (alpha == 0)))
        maximum = max(maximum, float(gray.max()))
    assert total['fragmentGrayOverOneAtTexelCenter'] == (
        total['overOneAndOpaque'] + total['overOneAndPartiallyTransparent'] +
        total['overOneAndFullyTransparent'])
    report = {
        'schema': 'rok.research.gray_clamp_supplement.v1',
        'corrects': 'gray_material_source.json sourceFormula pre-blend clamp wording only; existing HLSL and uasset are unchanged.',
        'sourceFragmentFormula': 'half4 color=tex2D(MainTex,UV)*vertexColor*materialColor; float c=.299*color.r+.587*color.g+.184*color.b; color.rgb=c; color.a*=UnityRectClip; return color; no explicit clamp before blend.',
        'sourceBlend': 'Blend SrcAlpha OneMinusSrcAlpha; target-format clamp, if any, occurs after blending and is not generally equivalent to a fragment-stage saturate.',
        'savedUeDifference': 'Current UE Custom HLSL saturates grayGamma before Slate alpha blending. For interpolated texels with gray>1 and 0<alpha<1, the result could differ; do not claim exact pixel equivalence without real-RHI ROI comparison.',
        'texelCenterMethod': '69 unique locked icon sprite rects, sampled from source RGBA PNGs in Unity bottom-left sprite-rect coordinates; source vertex/material color is white in the native locked-node fixture.',
        'uniqueLockedIconSprites': len(locked), 'counts': total, 'maxUnclampedGray': maximum,
        'interpretation': 'Every observed over-one source texel center is fully opaque; pre-clamp is harmless for those centers after straight-alpha blend to an SDR UNORM target. Bilinear edge samples and final Slate output remain a runtime visual gate.',
        'sources': {path.relative_to(ROOT).as_posix(): sha(path) for path in
                    (SHADER, ATLAS, FIXTURE, SOURCE, UE, ASSET)},
        'producer': {'path': Path(__file__).relative_to(ROOT).as_posix(), 'sha256': sha(Path(__file__))},
    }
    OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'report': str(OUT), 'sha256': sha(OUT), 'counts': total,
                      'maxUnclampedGray': maximum}, ensure_ascii=False))


if __name__ == '__main__':
    main()
