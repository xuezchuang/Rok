"""Verify source table identity and executable BuildingDesc animation paths."""
import json
import re
import csv
from pathlib import Path
from BuildRokBuildingLevelDetails import read
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text
from BuildRokUnityUiAnimationManifest import Builder
from PrepareRokUiTextures import unity_sprite_rect

ROOT = Path(__file__).resolve().parents[1]


def main():
    catalog = json.loads((ROOT / 'Saved/RokUnityMigration/building_level_details.json').read_text(encoding='utf-8'))
    powers = {int(row['ID']): int(row['power']) for row in read('BuildingLevelData.csv')}
    produces = {(int(row['type']), int(row['level'])): row for row in read('BuildingResourcesProduce.csv')}
    assert [t['type'] for t in catalog['tables']] == [2, 3, 4, 5, 9]
    for table in catalog['tables'][:4]:
        assert len(table['headers']) == 4 and table['name'] and table['description']
        assert [r['level'] for r in table['rows']] == list(range(1, 26))
        for row in table['rows']:
            source = produces[table['type'], row['level']]
            assert row['power'] == powers[table['type'] * 100 + row['level']]
            assert row['produce_speed'] == int(source['produceSpeed'])
            assert row['gather_max'] == int(source['gatherMax'])
    barracks = catalog['tables'][4]
    assert barracks['header_ids'] == [180310, 180329, 180352, 180314]
    assert barracks['source_table'] == 'ref/config-tables/编译客户端配置/CSV/BuildingBarracks.csv'
    assert barracks['power_table'] == 'ref/config-tables/编译客户端配置/CSV/BuildingLevelData.csv'
    language, _ = read_language_lookup()
    types = {int(row['type']): row for row in read('BuildingTypeConfig.csv')}
    assert barracks['name'] == localized_text(language, int(types[9]['l_nameId']), 'cn')
    assert barracks['description'] == localized_text(language, int(types[9]['l_descId']), 'cn')
    assert barracks['headers'] == [localized_text(language, key, 'cn') for key in (180310, 180329, 180352, 180314)]
    assert len(barracks['headers']) == 4 and all(barracks['headers'])
    # Parse the source CSV afresh instead of reusing the catalog builder's barracks rows.
    source_path = ROOT / 'ref/config-tables/编译客户端配置/CSV/BuildingBarracks.csv'
    with source_path.open(encoding='utf-8-sig', newline='') as source_file:
        source_rows = {int(row['level']): row for row in csv.DictReader(source_file) if row['level'].isdigit()}
    assert sorted(source_rows) == list(range(1, 26))
    assert [row['level'] for row in barracks['rows']] == list(range(1, 26))
    template = localized_text(language, 180357, 'cn')
    assert template == '{0}%'
    for row in barracks['rows']:
        source = source_rows[row['level']]
        level = row['level']
        assert int(source['ID']) == level
        assert row['infantry_train_number'] == int(source['infantryTrainNumber'])
        raw_attack = int(source['infantryAttackMulti'])
        assert row['infantry_attack_multi'] == raw_attack
        expected_attack = template.format(f'{raw_attack / 10:g}') if raw_attack > 0 else ''
        assert row['infantry_attack_text'] == expected_attack
        # GetPower indexes the current type/level row; no sum of preceding levels.
        assert row['power'] == powers[900 + level]
    assert [(row['infantry_train_number'], row['infantry_attack_multi'], row['infantry_attack_text'], row['power'])
            for row in (barracks['rows'][0], barracks['rows'][1], barracks['rows'][24])] == [
                (20, 0, '', 5), (50, 0, '', 10), (2000, 20, '2%', powers[925])]
    builder = Builder()
    mediator = builder.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/BuildingDescMediator.cs')
    assert 'view.m_pl_content_Animator.Play("Show")' in mediator
    assert 'view.m_pl_sv_level_Animator.Play("Show")' in mediator
    assert 'return define.power;' in mediator
    assert 'SetActive((index + 1) == m_buildingLevel)' in mediator
    update = builder.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateMediator.cs')
    barracks_case = update.split('case (int)EnumCityBuildingType.Barracks:', 2)[-1].split('case (int)EnumCityBuildingType.Stable:', 1)[0]
    assert all(f'm_titleList.Add(LanguageUtils.getText({key}))' in barracks_case for key in (180310, 180329, 180352, 180314))
    assert 'QueryRecords<BuildingBarracksDefine>()' in barracks_case
    assert 'if (list[i].infantryAttackMulti > 0)' in barracks_case
    assert 'LanguageUtils.getTextFormat(180357, (float)list[i].infantryAttackMulti / 1000 * 100)' in barracks_case
    assert 'm_contentList[index].Add("")' in barracks_case
    assert 'GetPower((int)m_buildingInfo.type, list[i].level)' in barracks_case
    assert 'return define.power;' in update
    show = mediator.split('private void ShowLevelInfo(bool isFirst)', 1)[1].split('private string OnGetItemPrefabName', 1)[0]
    assert show.index('if (m_isCreateLevelItem)') < show.index('FillContent(')
    assert 'RefreshAndGotoIndex' not in show
    ui = 'ref/client-unity/Assets/BundleAssets/UI/'
    for name in ['UI_Win_BuildingDesc', 'UI_Item_BuildingLevelData_Title_Four', 'UI_Item_BuildingLevelData_Info_Four']:
        builder.read(ui + 'UIPrefabs/ui_prefabs_build/UIBuildingDesc/' + name + '.prefab')
    prefab = builder.read(ui + 'UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Win_BuildingDesc.prefab')
    documents = dict(re.findall(r'--- !u!\d+ &(-?\d+)\n(.*?)(?=\n--- !u!|\Z)', prefab, re.S))

    def node(name):
        obj = next(value for value in documents.values() if '\n  m_Name: ' + name + '\n' in value)
        components = [documents[key] for key in re.findall(r'component: {fileID: (-?\d+)}', obj)]
        return components[0], components

    def vector(rect, field):
        return tuple(float(value) for value in re.search(field + r': {x: ([\d.-]+), y: ([\d.-]+)}', rect).groups())

    left, left_components = node('Left')
    info, info_components = node('btn_BuildLevelInfo')
    assert any(re.search(r'_arab_Layout: 2(?:\n|$)', text) for text in left_components)
    assert any(re.search(r'_arab_Layout: 2(?:\n|$)', text) for text in info_components)
    assert vector(left, 'm_AnchorMin') == (1, .5)
    assert vector(info, 'm_Pivot') == (0, .5)
    assert vector(info, 'm_SizeDelta') == (60, 60)
    # Both flag-2 nodes mirror X/anchor/pivot on Chinese Awake. Parent content center Y is 450 + 27.
    info_x = 208 - vector(left, 'm_AnchoredPosition')[0] - vector(info, 'm_AnchoredPosition')[0] - 60
    info_y = 450 + 27 - vector(info, 'm_AnchoredPosition')[1] - 30
    assert abs(info_x - 222.05) < .001 and info_y == 272
    target = (ROOT / 'Source/Rok/RokUnityModalWidget.cpp').read_text(encoding='utf-8')
    assert 'FVector2D(222.05f, 272), FVector2D(60)' in target
    assert 'InfoStyle.SetNormalPadding(FMargin(11))' in target
    atlas = builder.read(ui + 'UITextureStatic/ui_res_common/ui_common.png.meta')
    sprites = {'btn_com_1007': (782, 237, 37, 37), 'btn_com_1000': (703, 282, 69, 69),
               'pl_com_1012_2': (1003, 976, 964, 47), 'pl_com_1012_1': (1, 885, 964, 53),
               'pl_com_1005': (1, 611, 570, 2)}
    for name, rect in sprites.items():
        assert tuple(unity_sprite_rect(atlas, name)) == rect
    clips = []
    for name, start in [('Level', 50), ('Content', -50)]:
        prefix = 'ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_BuildingDesc_' + name
        clip = builder.clip(prefix + 'Show.anim')
        state = builder.read(prefix + '.controller').split('m_Name: Show', 1)[1].split('--- !u!', 1)[0]
        assert clip['source_guid'] in state and clip['duration_seconds'] == .25
        assert len(clip['tracks']) == 2
        for track in clip['tracks']:
            assert track['path'] == ''
            assert [k['time'] for k in track['keys']] == [0, .25]
            assert [k['value'] for k in track['keys']] == ([start, 0] if track['attribute'] == 'm_AnchoredPosition.x' else [0, 1])
            assert all(k['in_slope'] == k['out_slope'] == k['weighted_mode'] == 0 for k in track['keys'])
        clips.append(clip)
    output = ROOT / 'Saved/RokUnityMigration/building_level_details_verify.json'
    output.write_text(json.dumps({'source_verified': True, 'rows': 125, 'barracks_rows': 25, 'barracks_headers': [180310, 180329, 180352, 180314], 'clips': clips, 'sources': builder.sources, 'sprites': sprites,
        'source_route': 'GlobalCityMenuHudMediator -> BuildingDesc with buildingIndex; no type-only route',
        'reference_preview': 'opt-in only, unknown current level',
        'remaining_gates': ['UE build and Rok.Ui.Building.LevelDetails', 'type9 real route runtime and visual comparison', 'real resource actor entry absent in initial city']}, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'source_verified': True, 'rows': 125, 'output': str(output)}))


if __name__ == '__main__':
    main()
