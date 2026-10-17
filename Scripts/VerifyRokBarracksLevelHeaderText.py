"""Verify native Title_Four header glyph meshes and stage the isolated UE source-text package."""
import argparse
import hashlib
import json
import math
import re
import shutil
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
TARGET = ROOT / 'Saved/RokUnityMigration/BarracksLevelHeaderText'
CATALOG = ROOT / 'Saved/RokUnityMigration/building_level_details.json'
EXPECTED_SOURCES = {
    'ref/client-unity/Assets/StreamingAssets/Config/Bin/Language.bin',
    'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Item_BuildingLevelData_Title_Four.prefab',
    'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateMediator.cs',
    'Saved/UnityBarracksLevelProbe/Artifacts/native_20261008_094049_295/render_evidence.json',
    'Saved/RokUnityMigration/building_level_details.json',
    'Scripts/PrepareRokBarracksLevelHeaderText.py',
    'Scripts/RenderRokBarracksLevelHeaderText.cs',
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def verify(directory):
    directory = Path(directory).resolve()
    assert directory.parent == (ROOT / 'Saved/UnityBarracksLevelHeaderTextProbe/Artifacts').resolve()
    inputs_file = directory / 'ReferenceInputs.json'
    mesh_file = directory / 'source_text_meshes.json'
    inputs = json.loads(inputs_file.read_text(encoding='utf-8'))
    native = json.loads(mesh_file.read_text(encoding='utf-8'))
    assert inputs['schema'] == 'rok.barracks.source_text.inputs.v1'
    assert native['schema'] == 'rok.barracks.source_text.v1'
    assert native['inputSha256'] == sha(inputs_file)
    assert native['unityVersion'] == '2019.4.28f1c1'
    assert native['prefab'] == inputs['prefab']
    assert inputs['renderer_sha256'] == inputs['copied_renderer_sha256']
    assert {s['path'] for s in inputs['sources']} == EXPECTED_SOURCES
    for source in inputs['sources']:
        assert sha(ROOT / source['path']) == source['sha256'], source['path']
    specs = {s['key']: s for s in inputs['sourceTexts']}
    assert set(specs) == {'header_0', 'header_1', 'header_2', 'header_3'}
    assert len(native['texts']) == 4 and {t['key'] for t in native['texts']} == set(specs)
    catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
    type9 = next(t for t in catalog['tables'] if t['type'] == 9)
    assert [specs[f'header_{i}']['text'] for i in range(4)] == type9['headers']
    assert type9['header_ids'] == [180310, 180329, 180352, 180314]
    native_level = json.loads((ROOT / 'Saved/UnityBarracksLevelProbe/Artifacts/native_20261008_094049_295/render_evidence.json').read_text(encoding='utf-8'))
    detail_nodes = native_level['detailWindowGeometry']
    summaries = []
    for text in native['texts']:
        spec = specs[text['key']]
        assert text['text'] == spec['text'] and text['sourceNode'] == spec['sourceNode']
        assert text['fontSize'] == spec['fontSize'] == 22
        assert text['style'] == spec['style'] == 0 and text['alignment'] == spec['alignment'] == 4
        assert 2 <= text['effectiveFontSize'] <= 24 and spec['bestFit']
        assert abs(text['width'] - spec['width']) < .01 and abs(text['height'] - spec['height']) < .01
        assert abs(text['width'] - 250) < .01 and abs(text['height'] - 50) < .01
        assert abs(text['x'] - spec['x']) < .05 and abs(text['y'] - spec['y']) < .05
        assert text['pixelsPerUnit'] == 1 and text['lineCount'] == 1
        name = text['sourceNode']
        observed = [r for r in detail_nodes if r['path'].endswith('/pl_tilte/UI_Item_BuildingLevelData_Title_Four(Clone)/' + name)]
        assert len(observed) == 1 and observed[0]['text'] == text['text']
        for k in ('x', 'y', 'width', 'height'):
            assert abs(text[k] - observed[0]['screenRect'][k]) < .05, (text['key'], k)
        count = len(text['xy'])
        assert count > 0 and count % 4 == 0
        assert len(text['uv']) == len(text['colors']) == count
        assert len(text['triangles']) == count // 4 * 6
        assert all(isinstance(i, int) and 0 <= i < count for i in text['triangles'])
        assert all(math.isfinite(v) and abs(v) < 10000 for point in text['xy'] for v in point.values())
        assert all(math.isfinite(v) and 0 <= v <= 1 for point in text['uv'] for v in point.values())
        assert {tuple(c[k] for k in ('r', 'g', 'b', 'a')) for c in text['colors']} == {(230, 217, 180, 255)}
        glyphs = {g['codepoint']: g for g in text['glyphs']}
        assert set(map(ord, re.sub('<.*?>', '', text['text']))) <= set(glyphs)
        for i in range(0, count, 4):
            quad = text['uv'][i:i + 4]
            assert any(all(any(abs(p['x'] - glyph[corner]['x']) < 1e-6 and
                               abs(p['y'] - glyph[corner]['y']) < 1e-6
                               for corner in ('uvBottomLeft', 'uvBottomRight', 'uvTopLeft', 'uvTopRight'))
                           for p in quad) for glyph in glyphs.values()), (text['key'], i)
        atlas = (directory / text['atlas']).resolve()
        assert atlas.parent == directory / 'SourceTextAtlases'
        assert sha(atlas) == text['atlasSha256']
        image = Image.open(atlas).convert('RGBA')
        assert max(image.size) <= 8192 and image.getchannel('A').getextrema()[1] > 0
        ys = [p['y'] for p in text['xy']]
        summaries.append({'key': text['key'], 'text': text['text'], 'effectiveFontSize': text['effectiveFontSize'],
                          'glyphBoundsY': [min(ys), max(ys)], 'rect': [text['x'], text['y'], text['width'], text['height']],
                          'atlasSha256': text['atlasSha256']})
    verified = {'schema': 'rok.barracks.level_header_text.verify.v1', 'passed': True,
                'native_directory': str(directory), 'input_sha256': native['inputSha256'],
                'mesh_sha256': sha(mesh_file), 'headers': summaries,
                'scope': 'Original Title_Four LanguageText per-glyph mesh/atlas; UE drawing and full GameApp/login remain separate.'}
    (directory / 'glyph_verify.json').write_text(json.dumps(verified, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return verified, inputs, native


def stage(directory, inputs, native, verified):
    directory = Path(directory).resolve()
    TARGET.mkdir(parents=True, exist_ok=True)
    atlases = {}
    staged = json.loads(json.dumps(native))
    title_source = (ROOT / 'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Item_BuildingLevelData_Title_Four.prefab').read_text(encoding='utf-8-sig')
    for item in staged['texts']:
        # OnPopulateMesh returns vertices around the source RectTransform pivot.
        # SRokBarracksSourceText paints XY in its local top-left pixel space.
        label_start = title_source.index('m_Name: ' + item['sourceNode'] + '\n')
        label_end = title_source.find('--- !u!1 ', label_start + 1)
        source_block = title_source[label_start:label_end if label_end != -1 else None]
        assert 'm_Pivot: {x: 0.5, y: 0.5}' in source_block, item['sourceNode']
        source_item = next(row for row in native['texts'] if row['key'] == item['key'])
        offset_x = item['width'] * .5
        offset_y = item['height'] * .5
        item['xy'] = [{'x': point['x'] + offset_x, 'y': point['y'] + offset_y}
                      for point in source_item['xy']]
        assert len(item['xy']) == len(source_item['xy'])
        for raw, converted in zip(source_item['xy'], item['xy']):
            assert abs(converted['x'] - offset_x - raw['x']) < 1e-6
            assert abs(converted['y'] - offset_y - raw['y']) < 1e-6
            assert 0 <= converted['x'] <= item['width'] and 0 <= converted['y'] <= item['height']
        old = item['atlas']
        if old not in atlases:
            image = Image.open(directory / old).convert('RGBA')
            white = Image.new('RGBA', image.size, (255, 255, 255, 255))
            white.putalpha(image.getchannel('A'))
            name = 'glyph_' + item['atlasSha256'] + '.png'
            white.save(TARGET / name)
            atlases[old] = {'path': name, 'sha256': sha(TARGET / name), 'source_sha256': item['atlasSha256']}
        item['atlas'] = atlases[old]['path']
        item['atlasSha256'] = atlases[old]['sha256']
    (TARGET / 'texts.json').write_text(json.dumps(staged, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    shutil.copyfile(directory / 'ReferenceInputs.json', TARGET / 'ReferenceInputs.json')
    shutil.copyfile(directory / 'source_text_meshes.json', TARGET / 'source_text_meshes.json')
    manifest = {'schema': 'rok.barracks.source_text.package.v1',
                'texts_sha256': sha(TARGET / 'texts.json'), 'catalog_sha256': sha(CATALOG),
                'input_sha256': sha(TARGET / 'ReferenceInputs.json'),
                'native_mesh_sha256': sha(TARGET / 'source_text_meshes.json'),
                'native_directory': str(directory), 'atlases': list(atlases.values()),
                'coordinate_transform_script': 'Scripts/VerifyRokBarracksLevelHeaderText.py',
                'coordinate_transform_script_sha256': sha(Path(__file__).resolve()),
                'coordinate_transform': {'scope': 'header_0..3 only',
                    'source': 'Original Title_Four label RectTransform pivot-local UGUI mesh; y inverted at native export',
                    'pivot': [0.5, 0.5], 'runtime': 'top-left local pixels for SRokBarracksSourceText',
                    'equation': 'runtime.x = native.xy.x + width*0.5; runtime.y = native.xy.y + height*0.5'}}
    (TARGET / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    packaged_verify = dict(verified, coordinate_transform_applied=True,
                           staged_texts_sha256=manifest['texts_sha256'],
                           coordinate_transform_script_sha256=manifest['coordinate_transform_script_sha256'])
    (TARGET / 'glyph_verify.json').write_text(json.dumps(packaged_verify, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    assert manifest['texts_sha256'] == sha(TARGET / 'texts.json')
    assert manifest['catalog_sha256'] == sha(CATALOG)
    assert manifest['coordinate_transform_script_sha256'] == sha(Path(__file__).resolve())
    assert staged['inputSha256'] == manifest['input_sha256']
    loaded = json.loads((TARGET / 'texts.json').read_text(encoding='utf-8'))
    for item in loaded['texts']:
        source_item = next(row for row in native['texts'] if row['key'] == item['key'])
        for converted, raw in zip(item['xy'], source_item['xy']):
            assert abs(converted['x'] - raw['x'] - item['width']*.5) < 1e-6
            assert abs(converted['y'] - raw['y'] - item['height']*.5) < 1e-6
        assert item['text'] == source_item['text'] and item['uv'] == source_item['uv']
        assert item['colors'] == source_item['colors'] and item['triangles'] == source_item['triangles']
    print(json.dumps({'passed': True, 'package': str(TARGET), 'headers': len(staged['texts']),
                      'atlases': len(atlases)}, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('directory')
    args = parser.parse_args()
    verified, inputs, native = verify(args.directory)
    stage(args.directory, inputs, native, verified)
