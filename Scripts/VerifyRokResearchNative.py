"""Strict source/hash/geometry verification for the isolated research UI native capture."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from rok_data_paths import DATA_ROOT, resolve

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'Saved/UnityResearchNativeProbe'
PACKAGE = ROOT / 'Saved/RokUnityMigration/ResearchNative'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rect(node, visible=False):
    assert node and node['path']
    r = node['screenRect']
    assert all(math.isfinite(r[k]) for k in ('x', 'y', 'width', 'height')), node['path']
    assert r['width'] >= 0 and r['height'] >= 0
    if visible:
        assert node['activeInHierarchy'] and r['width'] > 0 and r['height'] > 0, node['path']
    assert len(node['screenCorners']) == 4
    return r


def close(a, b, tol=.55):
    assert abs(a-b) <= tol, (a, b)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('artifact', nargs='?', type=Path)
    parser.add_argument('--prepared-only', action='store_true')
    args = parser.parse_args()
    inputs = json.loads((PROJECT / 'ReferenceInputs.json').read_text(encoding='utf-8'))
    fixture = json.loads((PACKAGE / 'research_power_fixture.json').read_text(encoding='utf-8'))
    atlases = json.loads((DATA_ROOT / 'ResearchNative/atlas_manifest.json').read_text(encoding='utf-8'))
    assert inputs['schema'] == 'rok.research.native_inputs.v1'
    assert inputs['probeVersion'] == 2
    assert fixture['schema'] == 'rok.research.native_fixture.v1'
    assert inputs['fixtureSha256'] == sha(PACKAGE / 'research_power_fixture.json')
    assert inputs['fixture'] == fixture
    assert fixture['studyRowCount'] == 410 and fixture['nodeCount'] == len(fixture['nodes']) == 71
    assert fixture['state']['synthetic'] and not fixture['state']['accountBacked']
    assert fixture['state']['academyLevel'] is None and fixture['state']['civilizationId'] is None
    assert sum(n['type'] == 1 for n in fixture['nodes']) == 28
    assert sum(n['type'] == 2 for n in fixture['nodes']) == 43
    assert fixture['columns'] == {'economy': 15, 'military': 15}
    assert fixture['detailStudyType'] == 102 and fixture['detailLockedLanguageId'] == 402019
    assert fixture['moreDetailStudyType'] == 128 and len(fixture['moreLevels']) == 10
    assert [row['studyLv'] for row in fixture['moreLevels']] == list(range(1, 11))
    assert all(row['studyType'] == 128 and row['desData'] for row in fixture['moreLevels'])
    assert inputs['powerItemPrefab'].endswith('/UI_ResearchPowerItem.prefab')
    assert len(fixture['studyDataAlternatives']) == 8
    assert atlases['schema'] == 'rok.research.atlas.v1' and len(atlases['atlases']) == 4
    assert len(atlases['sprites']) == 122
    sprite_by_reference = {s['reference']: s for s in atlases['sprites']}
    for node in fixture['nodes']:
        assert node['icon'] in sprite_by_reference, node['icon']
        assert node['currentLevel'] == 0 and node['maxLevel'] >= 1
    for relative, expected in atlases['sources'].items():
        assert sha(resolve(relative)) == expected, relative
    for atlas in atlases['atlases']:
        assert sha(resolve(atlas['source'])) == sha(resolve(atlas['output'])) == atlas['sha256']
    menu = atlases['menuIcon']
    assert menu['sourceReference'] == 'ui_hud[btn_hud_1018]'
    assert menu['ueAssetPath'] == '/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_research'
    assert sha(resolve(menu['output'])) == menu['sha256']
    for src in inputs['sources']:
        assert sha(resolve(src['path'])) == src['sha256'], src['path']
    for row in inputs['byteIdenticalCopies']:
        assert sha(resolve(row['source'])) == sha(PROJECT / row['target']) == row['sha256'], row['target']
    for row in inputs['adapters']:
        assert sha(PROJECT / row['target']) == row['sha256'], row['target']
    assert json.loads((PROJECT / 'Validation/compiler_result.json').read_text(encoding='utf-8'))['exitCode'] == 0
    if args.prepared_only:
        print(json.dumps({'preparedVerified': True, 'nativeCapture': False, 'nodes': 71,
                          'sourceFiles': len(inputs['sources']), 'copiedAssets': len(inputs['byteIdenticalCopies']),
                          'atlases': 4, 'slices': 122}))
        return
    artifacts = sorted((PROJECT / 'Artifacts').glob('native_*'))
    artifact = args.artifact or (artifacts[-1] if artifacts else None)
    assert artifact and (artifact / 'render_evidence.json').exists(), 'Native Unity capture missing'
    evidence = json.loads((artifact / 'render_evidence.json').read_text(encoding='utf-8'))
    assert evidence['schema'] == 'rok.research.native.v2' and evidence['complete'], evidence.get('failure')
    assert not evidence['failure']
    assert evidence['unityVersion'] == '2019.4.28f1c1' and evidence['playMode']
    assert evidence['width'] == evidence['cameraPixelWidth'] == 1440
    assert evidence['height'] == evidence['cameraPixelHeight'] == 810
    assert evidence['inputSha256'] == sha(artifact / 'ReferenceInputs.json') == sha(PROJECT / 'ReferenceInputs.json')
    assert evidence['fixtureSha256'] == sha(PACKAGE / 'research_power_fixture.json')
    for key in ('originalUiRoot', 'originalMainPrefab', 'originalItemPrefab', 'originalSubItemPrefab',
                'originalDetailPrefab', 'originalPowerItemPrefab', 'syntheticUnresearched', 'noAccountOrNetworkAction'):
        assert evidence[key], key
    assert [s['key'] for s in evidence['samples']] == ['economy_start', 'economy_end',
        'military_start', 'military_end', 'detail_locked_102', 'detail_locked_128',
        'detail_power_128', 'detail_back_128']
    out = []
    for sample in evidence['samples']:
        assert sha(artifact / sample['png']) == sample['sha256']
        assert sample['canvasScaleFactor'] > 0
        assert sample['category'] in (1, 2)
        assert sample['totalNodeCount'] == len(sample['nodes']) == (28 if sample['category'] == 1 else 43)
        assert sample['viewportWidth'] > 0 and sample['contentWidth'] > sample['viewportWidth']
        close(sample['contentWidth'], 15 * 420.3, 2)
        if sample['key'].endswith('_end') or sample['key'] in ('detail_locked_128', 'detail_power_128', 'detail_back_128'):
            close(sample['scrollX'], sample['viewportWidth'] - sample['contentWidth'], 2)
        else:
            close(sample['scrollX'], 0, 2)
        assert sample['visibleNodeCount'] > 0
        for key in ('uiRoot', 'mainWindow', 'windowLayer', 'sidePanel', 'economyTab', 'militaryTab',
                    'economyScroll', 'economyViewport', 'economyContent', 'militaryScroll',
                    'militaryViewport', 'militaryContent'):
            rect(sample[key])
        fixture_nodes = {n['studyType']: n for n in fixture['nodes'] if n['type'] == sample['category']}
        for node in sample['nodes']:
            config = fixture_nodes[node['studyType']]
            assert node['studyId'] == config['ID']
            assert (node['column'], node['location'], node['currentLevel'], node['maxLevel']) == (
                config['columns'], config['location'], 0, config['maxLevel'])
            assert node['iconReference'] == config['icon']
            assert node['name']['text'] == config['name']
            assert node['level']['text'] == f"0/{config['maxLevel']}"
            assert node['icon']['spriteName'] == sprite_by_reference[config['icon']]['spriteName']
            assert node['button']['spriteName'] == ('img_research_btnBgLight' if node['hasReachedPrerequisites'] else 'img_research_btnBgDark')
            assert node['sourceGrayShaderEmulated'] is True
            if not node['hasReachedPrerequisites']:
                assert 'UI_GrayWithMask' in node['icon']['materialName'], node['icon']['materialName']
            for part in ('root', 'button', 'icon', 'iconBackground', 'name', 'level', 'progress', 'line', 'line2'):
                rect(node[part])
        if sample['key'] in ('detail_locked_102', 'detail_locked_128', 'detail_power_128', 'detail_back_128'):
            rect(sample['detailWindow'], visible=True)
            def detail_at(suffix):
                result = [n for n in sample['detailGeometry'] if n['path'].endswith(suffix)]
                assert len(result) == 1, (suffix, len(result))
                return result[0]
            study_type = 102 if sample['key'] == 'detail_locked_102' else 128
            assert detail_at('/rect/img_update/pl_right/lbl_unlock')['text'] == next(row['text'] for row in fixture['language'] if row['id'] == 402019)
            assert detail_at('/rect/img_update/pl_left/pb_bar/lbl_barText')['text'] == '0/' + str(fixture_nodes[study_type]['maxLevel'])
            assert not detail_at('/rect/img_update/pl_right/pl_condition')['activeInHierarchy']
            assert detail_at('/rect/img_update/pl_left/img_icon')['spriteName'] == sprite_by_reference[fixture_nodes[study_type]['icon']]['spriteName']
            assert detail_at('/UI_Model_Window_Type1/title/lbl_title')['text'] == fixture_nodes[study_type]['name']
            assert detail_at('/UI_Model_AttrItem0/layout/lbl_crrVaule')['text'].startswith('0.0%+')
            assert detail_at('/UI_Model_AttrItem1/layout/lbl_crrVaule')['text'].startswith('0+')
            assert sample['detailPowerActive'] == (sample['key'] == 'detail_power_128')
            assert sample['detailUpdateActive'] != sample['detailPowerActive']
            assert sample['detailBackActive'] == (sample['key'] == 'detail_power_128')
            if sample['key'] == 'detail_power_128':
                assert not detail_at('/rect/img_update')['activeInHierarchy']
                rect(detail_at('/rect/pl_power'), visible=True)
                rect(detail_at('/rect/pl_power/sv_power'), visible=True)
                rect(detail_at('/rect/pl_power/sv_power/v_list_view'), visible=True)
                assert detail_at('/rect/pl_power/lbl_subDes')['text'] == next(
                    row['text'] for row in fixture['language'] if row['id'] == fixture['moreLevels'][0]['l_buffDesID'])
                assert len(sample['powerRows']) == 10
                for index, row in enumerate(sample['powerRows']):
                    source = fixture['moreLevels'][index]
                    assert (row['studyType'], row['level'], row['power'], row['desData']) == (
                        128, index+1, source['power'], source['desData'])
                    assert row['levelLabel']['text'] == str(index+1)
                    assert row['effectLabel']['text'] == source['desData']
                    assert row['powerLabel']['text'] == f"+{source['power']:,}"
                    for part in ('root', 'levelLabel', 'effectLabel', 'powerLabel', 'line'):
                        rect(row[part], visible=True)
                    close(row['root']['screenRect']['height'], 50, .6)
                    if index:
                        close(row['root']['screenRect']['y']-sample['powerRows'][index-1]['root']['screenRect']['y'], 50, .6)
                close(detail_at('/rect/pl_power/sv_power/v_list_view/c_list_view')['anchoredPosition']['y'], 0, .01)
                assert sample['detailAnimatorState'] == 'Show' and sample['detailAnimatorNormalizedTime'] >= .99
            elif sample['key'] == 'detail_back_128':
                assert sample['detailAnimatorState'] == 'Show' and sample['detailAnimatorNormalizedTime'] >= .99
                assert len(sample['powerRows']) == 10 and all(not row['root']['activeInHierarchy'] for row in sample['powerRows'])
            else:
                assert not sample['powerRows'] and detail_at('/rect/img_update/pl_right/lbl_unlock')['activeInHierarchy']
        else:
            assert not sample['detailWindow']['path'] and not sample['detailGeometry']
        out.append({'key': sample['key'], 'png': str(artifact / sample['png']),
                    'viewport': sample['economyViewport']['screenRect'] if sample['category'] == 1 else sample['militaryViewport']['screenRect'],
                    'scrollX': sample['scrollX'], 'visibleNodes': sample['visibleNodeCount']})
    verified = {'schema': 'rok.research.native_verified.v1', 'artifact': str(artifact),
                'evidenceSha256': sha(artifact / 'render_evidence.json'),
                'inputsSha256': sha(artifact / 'ReferenceInputs.json'), 'atlasManifestSha256': sha(DATA_ROOT / 'ResearchNative/atlas_manifest.json'),
                'fixtureSha256': sha(PACKAGE / 'research_power_fixture.json'), 'samples': out,
                'qualification': 'Original prefabs/atlases with source-configured deterministic unresearched state; not account data or full hotfix execution.'}
    target = PACKAGE / 'verified_power_capture.json'
    target.write_text(json.dumps(verified, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'verified': True, 'artifact': str(artifact), 'evidenceSha256': verified['evidenceSha256'],
                      'samples': [row['key'] for row in out]}))


if __name__ == '__main__':
    main()
