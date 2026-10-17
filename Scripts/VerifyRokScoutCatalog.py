"""Verify Scout browse source/config identity; deliberately not runtime acceptance."""
import json
from pathlib import Path
from rok_data_paths import resolve
from BuildRokScoutCatalog import build, OUT, ROOT, PREFAB, UI, MEDIATOR, PROXY
from BuildRokUnityUiAnimationManifest import unity_blocks, field, referenced_id


def verify():
    generated = json.loads(OUT.read_text(encoding='utf-8'))
    expected = build(write=False)
    assert generated == expected, 'Scout manifest is stale; regenerate from source'
    assert generated['building_type'] == 28
    assert generated['building']['menuButtons'].split('|') == ['101', '102', '103', '137']
    assert generated['menu']['func'] == 'openUI' and generated['menu']['param'] == '1022'
    assert (generated['menu']['showLevelMin'], generated['menu']['showLevelMax']) == ('1', '25')
    levels = generated['levels']
    assert [row['level'] for row in levels] == list(range(1, 26))
    assert [row['scoutNumber'] for row in levels] == [1] * 4 + [2] * 6 + [3] * 15
    assert all(row['ID'] == row['level'] and row['scoutSpeedMulti'] == row['level'] * 50 for row in levels)
    mediator = (resolve(MEDIATOR)).read_text(encoding='utf-8-sig')
    proxy = (resolve(PROXY)).read_text(encoding='utf-8-sig')
    assert 'int nNum = m_scoutProxy.GetScoutNum();' in mediator
    assert 'return mlistSouts.Count;' in proxy and 'Role_ScoutsInfo.request' in proxy and 'scoutsInfo.scoutsQueue.Values' in proxy
    assert 'Map_Scouts.request req' in proxy
    assert 'view.m_UI_Model_Window_Type1.AddBackEvent(ShowList);' in mediator
    assert 'view.m_UI_Model_Window_Type1.setCloseHandle(onClose);' in mediator
    assert 'view.m_lbl_Desc_Animator.Play("Show");' in mediator and 'view.m_sv_list_view_Animator.Play("Show");' in mediator
    for index, start_x in [(0, 50), (1, -50)]:
        clip = generated['animations'][index]
        assert clip['duration_seconds'] == 0.25 and not clip['loop']
        assert len(clip['tracks']) == 2
        for track in clip['tracks']:
            assert track['path'] == ''
            assert [key['time'] for key in track['keys']] == [0, 0.25]
            assert [key['value'] for key in track['keys']] == ([start_x, 0] if track['attribute'] == 'm_AnchoredPosition.x' else [0, 1])
            assert all(key['in_slope'] == key['out_slope'] == key['weighted_mode'] == 0 for key in track['keys'])
        family = 'Level' if index == 0 else 'Content'
        path = UI + 'UIAnimation/shared_uianimation/UA_BuildingDesc_' + family + '.controller'
        controller = (resolve(path)).read_text(encoding='utf-8-sig')
        assert clip['source_guid'] in controller.split('m_Name: Show', 1)[1].split('--- !u!', 1)[0]
        assert 'm_Name: Empty' in controller
    prefab = (ROOT / (PREFAB + 'UI_Win_Scout.prefab')).read_text(encoding='utf-8-sig')
    assert 'guid: 50dabf849dcde3344bbe81589976fb47' in prefab
    assert 'guid: 74c158eabc81d4b469554d8e8482308f' in prefab
    assert 'guid: 79ffb0d19ef4cd34193888a1a1bb63e4' in prefab
    flags = generated['arabic_flags'][PREFAB + 'UI_Item_ScoutDesc.prefab']
    assert flags['btn_info'] == 2 and flags['lbl_test'] == 10 and flags['lbl_progress'] == 0
    assert not generated['arabic_flags'][PREFAB + 'UI_Win_Scout.prefab']
    arabic_source = (ROOT / 'ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs').read_text(encoding='utf-8-sig')
    assert 'isArabUI == Skyunion.LanguageUtils.IsArabic()' in arabic_source
    assert 'anchorMin.x = 1 - gui.anchorMax.x;' in arabic_source and 'pos.x = -pos.x;' in arabic_source
    source_sizes = {row['name']: row['size'] for row in generated['layouts'][PREFAB + 'UI_Win_Scout.prefab']}
    assert source_sizes['UI_Win_Scout'] == {'x': 1440, 'y': 810}
    assert source_sizes['lbl_Desc'] == {'x': 928, 'y': 520}
    assert source_sizes['sv_list_view'] == {'x': 1016, 'y': 613}
    desc_style = generated['text_styles'][PREFAB + 'UI_Win_Scout.prefab']['lbl_Desc']
    assert {key: desc_style[key] for key in ['m_FontSize', 'm_FontStyle', 'm_Alignment', 'm_LineSpacing',
            'm_RichText', 'm_HorizontalOverflow', 'm_VerticalOverflow']} == {
        'm_FontSize': 20, 'm_FontStyle': 1, 'm_Alignment': 2, 'm_LineSpacing': 1.3,
        'm_RichText': 1, 'm_HorizontalOverflow': 0, 'm_VerticalOverflow': 0}
    blocks = unity_blocks(prefab)
    desc_id = next(entry['file_id'] for entry in blocks if entry['class_id'] == 1 and field(entry['text'], 'm_Name') == 'lbl_Desc')
    desc_components = [entry for entry in blocks if referenced_id(entry['text'], 'm_GameObject') == desc_id]
    assert all(field(entry['text'], 'm_Content') is None for entry in desc_components), 'Source description must not acquire a ScrollRect'
    title_shadow = generated['text_styles'][UI + 'UIPrefabs/ui_prefabs_common/UICommon/UI_Model_Window_Type1.prefab']['lbl_title']['shadow']
    assert title_shadow == {'m_EffectColor': {'r': 0.11372549, 'g': 0.08627451, 'b': 0.08627451, 'a': 1},
                            'm_EffectDistance': {'x': 0, 'y': -2}, 'm_UseGraphicAlpha': 1}
    assert len(generated['text']['181002'].split('\n\n')) == 4
    assert '\\n' not in generated['text']['181002'] and '<' not in generated['text']['181002']
    assert len(generated['text']['181002']) > 200 and all(generated['text'][key] for key in generated['text'])
    assert generated['sprites']['back']['rect'] == [703, 282, 69, 69]
    assert generated['sprites']['info']['rect'] == [782, 237, 37, 37]
    assert generated['queue_state_available'] is False and generated['fog_state_available'] is False
    report = {'source_verified': True, 'levels': 25, 'capacity_unlock_levels': [1, 5, 11],
              'route': {'building_type': 28, 'menu_id': 137, 'open_ui': 1022},
              'chinese_layout': {'summary_info_side': 'left; flags 2', 'summary_text_alignment': 'left; flags 10',
                                 'main_description_alignment': 'right; no ArabLayout component',
                                 'main_description_container': 'fixed 928x520, horizontal wrap and vertical truncate; no ScrollRect',
                                 'title_shadow_unity_offset': [0, -2], 'title_shadow_ue_offset': [0, 2]},
              'remaining_gates': ['UE5.8 compile and Rok.Ui.Scout automation', 'Real runtime buttons and Unity visual comparison',
                                  'Role_ScoutsInfo queue, fog progress, map travel, dispatch and server responses'],
              'visual_verified': False, 'sources': generated['sources']}
    output = ROOT / 'Saved/RokUnityMigration/scout_catalog_verify.json'
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'source_verified': True, 'levels': 25, 'output': str(output)}))


if __name__ == '__main__': verify()
