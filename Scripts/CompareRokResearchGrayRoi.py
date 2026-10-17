"""Compare fixed native/UE locked Research icon rectangles after real RHI capture.

The ROI is the intersection of both reported icon rects, only when both icon
rects are fully inside their reported scroll viewports. All source icon sprite
rects are opaque, so no inferred pixel mask or background removal is used.
"""
import hashlib
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image
from rok_data_paths import DATA_ROOT, label

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / 'Saved/UnityResearchNativeProbe/Artifacts/native_20261008_131955_853'
UE = ROOT / 'Saved/RokUnityMigration/ResearchAudit/20261008_132329_43244'
ATLAS = DATA_ROOT / 'ResearchNative/atlas_manifest.json'
OUT = ROOT / 'Saved/RokUnityMigration/ResearchNative/gray_roi_comparison.json'
PAIRS = [('economy_start', '01_economy_start.png', 0),
         ('economy_end', '06_economy_end.png', 5),
         ('military_start', '11_military_start.png', 10),
         ('military_end', '15_military_end.png', 14)]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inside(rect, viewport):
    return (rect['x'] >= viewport['x'] - .1 and rect['y'] >= viewport['y'] - .1 and
            rect['x'] + rect['width'] <= viewport['x'] + viewport['width'] + .1 and
            rect['y'] + rect['height'] <= viewport['y'] + viewport['height'] + .1)


def bounds(rect):
    return (rect['x'], rect['y'], rect['x'] + rect['width'], rect['y'] + rect['height'])


def sample_rect(image, rect, count=88):
    # Evaluate both screenshot rectangles at identical source-normalized UVs.
    # This removes only their recorded geometry delta, never image-fits pixels.
    xs = rect['x'] + (np.arange(count, dtype=np.float32) + .5) * rect['width'] / count - .5
    ys = rect['y'] + (np.arange(count, dtype=np.float32) + .5) * rect['height'] / count - .5
    x = np.broadcast_to(xs[None, :], (count, count))
    y = np.broadcast_to(ys[:, None], (count, count))
    x0 = np.floor(x).astype(np.int32)
    y0 = np.floor(y).astype(np.int32)
    x1, y1 = x0+1, y0+1
    assert x0.min() >= 0 and y0.min() >= 0 and x1.max() < image.shape[1] and y1.max() < image.shape[0]
    wx, wy = (x-x0)[..., None], (y-y0)[..., None]
    return ((1-wy)*((1-wx)*image[y0,x0]+wx*image[y0,x1])+
            wy*((1-wx)*image[y1,x0]+wx*image[y1,x1]))


def main():
    native = json.loads((NATIVE / 'render_evidence.json').read_text(encoding='utf-8'))
    ue = json.loads((UE / 'manifest.json').read_text(encoding='utf-8'))
    atlas = json.loads(ATLAS.read_text(encoding='utf-8'))
    sprites = {row['reference']: row for row in atlas['sprites']}
    sheets = {row['atlasName']: row for row in atlas['atlases']}
    assert native['schema'] == 'rok.research.native.v2' and native['complete']
    assert ue['capture_includes_slate_ui'] and len(ue['captures']) >= 15
    pairs = []
    for key, file_name, ue_index in PAIRS:
        sample = next(row for row in native['samples'] if row['key'] == key)
        capture = ue['captures'][ue_index]
        assert Path(capture['file']).name == file_name
        assert sha(UE / file_name) == capture['png_sha256']
        native_image = np.asarray(Image.open(NATIVE / sample['png']).convert('RGB'), dtype=np.float32)
        ue_image = np.asarray(Image.open(UE / file_name).convert('RGB'), dtype=np.float32)
        assert native_image.shape == ue_image.shape == (810, 1440, 3)
        native_view = sample['economyViewport' if key.startswith('economy') else 'militaryViewport']['screenRect']
        ue_request = capture['request']
        ue_view = ue_request['economy_scroll' if key.startswith('economy') else 'military_scroll']['pixel_rect']
        ue_nodes = {row['study_type']: row for row in ue_request['nodes']}
        rows = []
        for node in sample['nodes']:
            if node['hasReachedPrerequisites']:
                continue
            counterpart = ue_nodes[node['studyType']]
            nr = node['icon']['screenRect']
            ur = counterpart['icon']['pixel_rect']
            if not counterpart['icon']['visible'] or not inside(nr, native_view) or not inside(ur, ue_view):
                continue
            if not ('UI_GrayWithMask' in node['icon']['materialName'] and
                    'MID_M_RokResearchGrayUI' in counterpart['icon']['texture']):
                raise AssertionError(('Missing actual gray material', key, node['studyType']))
            reference = node['iconReference']
            sprite = sprites[reference]
            assert node['icon']['spriteName'] == sprite['spriteName']
            assert counterpart['icon_sprite'] == sprite['spriteName']
            uv = counterpart['icon']['uv']
            source_uv = sprite['slateUV']
            for field, actual in [('u0', uv['min_x']), ('v0', uv['min_y']),
                                  ('u1', uv['max_x']), ('v1', uv['max_y'])]:
                assert abs(actual - source_uv[field]) <= 1e-6, (key, node['studyType'], field)
            image = Image.open(ROOT / sheets[sprite['atlasName']]['source']).convert('RGBA')
            sr = sprite['rect']
            alpha = np.asarray(image.crop((sr['x'], image.height - sr['y'] - sr['height'],
                                           sr['x'] + sr['width'], image.height - sr['y'])).getchannel('A'))
            assert np.all(alpha == 255), (key, node['studyType'], 'Source icon is not fully opaque')
            nbox, ubox = bounds(nr), bounds(ur)
            # The UE end-scroll offset can differ by about one screen pixel.
            # Retain that offset in the raw comparison and report it explicitly.
            geometry_delta = [float(b-a) for a,b in zip(nbox,ubox)]
            assert max(abs(x) for x in geometry_delta) < 2, (key, node['studyType'], nbox, ubox)
            left = math.ceil(max(nbox[0], ubox[0]))
            top = math.ceil(max(nbox[1], ubox[1]))
            right = math.floor(min(nbox[2], ubox[2]))
            bottom = math.floor(min(nbox[3], ubox[3]))
            assert right-left >= 86 and bottom-top >= 86
            n = native_image[top:bottom, left:right]
            u = ue_image[top:bottom, left:right]
            delta = np.abs(n-u)
            aligned_n = sample_rect(native_image, nr)
            aligned_u = sample_rect(ue_image, ur)
            aligned_delta = np.abs(aligned_n-aligned_u)
            spread_n = n.max(axis=2)-n.min(axis=2)
            spread_u = u.max(axis=2)-u.min(axis=2)
            luma_n = n.mean(axis=2).ravel()
            luma_u = u.mean(axis=2).ravel()
            correlation = float(np.corrcoef(luma_n,luma_u)[0,1]) if luma_n.std() and luma_u.std() else None
            rows.append({'studyType': node['studyType'], 'sourceSprite': reference,
                         'atlas': sprite['atlasName'], 'sourceUv': source_uv,
                         'nativeRect': nr, 'ueRect': ur, 'roi': {'x': left, 'y': top,
                             'width': right-left, 'height': bottom-top, 'pixels': (right-left)*(bottom-top)},
                         'ueMinusNativeBoundsPx': geometry_delta,
                         'meanAbsRgb8': float(delta.mean()), 'medianAbsRgb8': float(np.median(delta)),
                         'p95AbsRgb8': float(np.percentile(delta,95)), 'maxAbsRgb8': float(delta.max()),
                         'geometryAlignedMeanAbsRgb8': float(aligned_delta.mean()),
                         'geometryAlignedP95AbsRgb8': float(np.percentile(aligned_delta,95)),
                         'geometryAlignedLumaCorrelation': float(np.corrcoef(
                             aligned_n.mean(axis=2).ravel(),aligned_u.mean(axis=2).ravel())[0,1]),
                         'meanSignedRgb8': [float(x) for x in (u-n).mean(axis=(0,1))],
                         'nativeMeanRgb8': [float(x) for x in n.mean(axis=(0,1))],
                         'ueMeanRgb8': [float(x) for x in u.mean(axis=(0,1))],
                         'nativeMeanChannelSpread8': float(spread_n.mean()),
                         'ueMeanChannelSpread8': float(spread_u.mean()),
                         'lumaCorrelation': correlation})
        assert rows, key
        pairs.append({'nativeKey': key, 'ueCapture': file_name, 'nativePngSha256': sha(NATIVE/sample['png']),
                      'uePngSha256': sha(UE/file_name), 'iconCount': len(rows),
                      'meanPerIconAbsRgb8': float(np.mean([r['meanAbsRgb8'] for r in rows])),
                      'meanPerIconGeometryAlignedAbsRgb8': float(np.mean([r['geometryAlignedMeanAbsRgb8'] for r in rows])),
                      'meanLumaCorrelation': float(np.mean([r['lumaCorrelation'] for r in rows if r['lumaCorrelation'] is not None])),
                      'icons': rows})
    report = {'schema': 'rok.research.gray_roi_comparison.v1',
              'scope': 'Locked icons with actual native UI_GrayWithMask and UE MID_M_RokResearchGrayUI; exact icon rect intersection, fully inside both source scroll viewports. No text, inferred masks, or selected best pixels.',
              'acceptance': 'Raw observation only. No unapproved absolute pixel-error threshold or full-screen visual acceptance.',
              'sources': {label(p): sha(p) for p in
                          (NATIVE/'render_evidence.json', UE/'manifest.json', ATLAS,
                           ROOT/'Content/RokPrototype/UI/Research/M_RokResearchGrayUI.uasset')},
              'pairs': pairs,
              'producer': {'path': label(Path(__file__)), 'sha256': sha(Path(__file__))}}
    OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'report': str(OUT), 'sha256': sha(OUT),
                      'pairs': [{'key': p['nativeKey'], 'icons': p['iconCount'],
                                 'meanAbsRgb8': p['meanPerIconAbsRgb8'],
                                 'geometryAlignedMeanAbsRgb8': p['meanPerIconGeometryAlignedAbsRgb8'],
                                 'meanLumaCorrelation': p['meanLumaCorrelation']} for p in pairs]}))


if __name__ == '__main__':
    main()
