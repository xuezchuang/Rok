"""Verify source data and animation contracts; no visual acceptance claim."""
import json
import re
from BuildRokTrainArmyDetail import ROOT, OUTPUT, build, PREFAB
from BuildRokUnityUiAnimationManifest import Builder


def main():
    saved = json.loads(OUTPUT.read_text(encoding='utf-8'))
    assert saved == build(write=False), 'Generated catalog differs from source'
    entries = saved['entries']
    assert len(entries) == 54 and len({row['id'] for row in entries}) == 54
    for row in entries:
        assert len(row['attributes']) == 7 and len(row['skills']) <= 3
        assert [a['unity_y'] for a in row['attributes']] == [119, 76, 33, -10, -53, -96, -139]
        assert [a['modifier_unknown'] for a in row['attributes']] == [False, True, True, True, False, False, False]
        for sprite in row['attributes'] + row['skills']:
            x, y, w, h = sprite['rect']
            assert x >= 0 and y >= 0 and w > 0 and h > 0
            assert x + w <= sprite['size'][0] and y + h <= sprite['size'][1]
    builder = Builder()
    mediator = builder.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/ArmyTrain/TrainArmyMediator.cs')
    assert 'm_btn_detail_GameButton.onClick.AddListener(OnDetail)' in mediator
    assert 'AddBackEvent(OnDetailReturn)' in mediator
    assert 'm_detailObj.GetComponent<Animator>().Play("Show")' in mediator
    assert 'view.m_pl_train_Animator.Play("Show")' in mediator
    for i in range(7):
        assert f'm_UI_Model_TrainSoldierAttr_{i + 1}.SetTipId({1000 + i})' in mediator
    assert 'SetOffset(30).SetWidth(500)' in mediator and 'SetOffset(23).SetWidth(500)' in mediator
    clips = []
    for name, start in [('Data', 40), ('Train', -40)]:
        prefix = 'ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UA_TrainArmy_' + name
        clip = builder.clip(prefix + 'Show.anim')
        controller = builder.read(prefix + '.controller')
        assert clip['source_guid'] in controller.split('m_Name: Show', 1)[1].split('--- !u!', 1)[0]
        assert clip['duration_seconds'] == 0.25 and len(clip['tracks']) == 2
        for track in clip['tracks']:
            assert track['path'] == ''
            assert [k['time'] for k in track['keys']] == [0, 0.25]
            assert [k['value'] for k in track['keys']] == ([start, 0] if track['attribute'] == 'm_AnchoredPosition.x' else [0, 1])
            assert all(k['in_slope'] == k['out_slope'] == k['weighted_mode'] == 0 for k in track['keys'])
        clips.append(clip)
    arab = builder.read('ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs')
    assert 'if (isArabUI == Skyunion.LanguageUtils.IsArabic())' in arab
    assert 'pivot.x = 1 - pivot.x' in arab and 'pos.x = -pos.x' in arab
    attr = builder.read('ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_noaddressable/UI_Model_TrainSoldierAttr.prefab')
    assert attr.count('_arab_Layout: 10') == 2
    assert 'm_AnchoredPosition: {x: 324.04, y: 0}' in attr
    assert 'm_AnchoredPosition: {x: -323.33, y: 0}' in attr
    detail = builder.read(PREFAB.relative_to(ROOT).as_posix())
    assert 'm_SizeDelta: {x: 624, y: 642}' in detail
    assert 'm_StartCorner: 1' in detail and '_arab_Layout: 512' in detail
    assert 'm_CellSize: {x: 100, y: 100}' in detail and 'm_Spacing: {x: 40, y: 0}' in detail
    # The actual hierarchy matters independently of the Grid start corner.
    children = ['9158245338975005400', '6731985674433741608', '4020775400646493511']
    assert '  - {fileID: ' + children[0] + '}\n  - {fileID: ' + children[1] + '}\n  - {fileID: ' + children[2] + '}' in detail
    for child, expected in zip(children, (3, 2, 1)):
        transform = detail.split('--- !u!224 &' + child + ' stripped', 1)[1].split('--- !u!', 1)[0]
        instance = re.search(r'm_PrefabInstance: \{fileID: (\d+)', transform)[1]
        block = detail.split('--- !u!1001 &' + instance + '\n', 1)[1].split('--- !u!', 1)[0]
        assert f'value: UI_Model_TrainSoldierSkill_{expected}' in block
    rs = ROOT / 'ref/client-unity/Assets/Scripts/Hotfix/MVC/RS.cs'
    assert b'"ui_troops[icon_tro_1001_1]"' in rs.read_bytes()
    sheet = builder.read('ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_troops/ui_troops.tpsheet')
    for record in ('icon_tro_1001_1;450;1906;38;38;', 'icon_tro_1001_3;87;413;38;38;',
                   'icon_tro_1001_2;44;369;38;38;', 'icon_tro_1001_4;1;92;38;38;'):
        assert record in sheet
    common = builder.read('ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.tpsheet')
    assert 'icon_com_monsterframe;1;151;134;134;' in common
    report = {'source_verified': True, 'rows': len(entries), 'sources': builder.sources, 'clips': clips,
              'chinese_layout': 'Awake mirrors PosX/TextAlignX; attribute label left right-aligned, value right left-aligned; Grid StartCorner 1->0 but actual sibling order Skill_3,2,1',
              'runtime_entry': 'BuildingFeatureButton -> OpenTrainArmy -> TrainArmyDetailButton -> TrainArmyDetailBackButton',
              'player_modifiers_available': False, 'visual_verified': False,
              'remaining_gates': ['UE build and Rok.Ui.Animation.TrainArmy.DetailNavigation', 'ImportRokTrainArmyDetail.py in verified city editor', 'Real city interaction screenshots and source visual comparison']}
    output = OUTPUT.with_name('train_army_detail_source_verify.json')
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'source_verified': True, 'rows': len(entries), 'clips': len(clips)}))


if __name__ == '__main__':
    main()
