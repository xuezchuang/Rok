"""Source layout checks; does not substitute for real UMG screenshots."""
import json
from pathlib import Path
from BuildRokTrainParameterLayout import main as build
from PrepareRokUiTextures import unity_sprite_rect

ROOT = Path(__file__).resolve().parents[1]
UI = 'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/'

def main():
    build()
    source = json.loads((ROOT / 'Saved/RokUnityMigration/train_parameter_layout.json').read_text(encoding='utf-8'))
    for name, position, size in [
        ('pl_right', [639.85, 98.4], [624.1, 659]),
        ('plArmyList', [656.9, 90.27], [590, 190.03]),
        ('lbl_desc_name', [712.36, 286.47], [528.35, 30]),
        ('lbl_desc_content', [668.395, 328.6], [567.01, 60.9]),
        ('pl_res_cost', [656.9, 398.4], [590, 160]),
        ('btn_detail', [581, 124.9], [60, 60]),
    ]:
        assert all(abs(a-b) < .001 for a,b in zip(source[name]['position'], position)), name
        assert all(abs(a-b) < .001 for a,b in zip(source[name]['size'], size)), name
    window = (ROOT / source['source']).read_text(encoding='utf-8')
    assert 'm_FontSize: 26' in window and 'm_FontSize: 18' in window
    quantity = (ROOT / (UI + 'ui_prefabs_noaddressable/UI_Model_TrainResCost.prefab')).read_text(encoding='utf-8')
    for token in ['m_SizeDelta: {x: 136.53, y: 40}', 'm_AnchoredPosition: {x: -232.8, y: 29.2}',
                  'm_AnchoredPosition: {x: -27.6, y: 26.56}', '_arab_Layout: 34', 'm_SizeDelta: {x: 22, y: 41}']:
        assert token in quantity, token
    for non_source in ['btn_minus', 'btn_plus', 'btn_max']:
        assert non_source not in quantity
    head = (ROOT / (UI + 'ui_prefabs_noaddressable/UI_Model_ArmyTrainHead.prefab')).read_text(encoding='utf-8')
    assert 'm_SizeDelta: {x: 105.4, y: 105.1}' in head and 'm_SizeDelta: {x: 126, y: 126}' in head
    atlas_root = ROOT / 'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic'
    common = (atlas_root / 'ui_res_common/ui_common.png.meta').read_text(encoding='utf-8')
    troops = (atlas_root / 'ui_res_troops/ui_troops.png.meta').read_text(encoding='utf-8')
    for name, rect in [('pl_com_1001', (436, 165, 63, 55)), ('pl_com_1008', (1003, 932, 400, 42)),
                       ('btn_com_1007', (782, 237, 37, 37)), ('btn_com_max3', (1490, 617, 274, 84))]:
        assert tuple(unity_sprite_rect(common, name)) == rect, name
    for name, rect in [('pl_tro_1005', (1, 1392, 447, 655)), ('pl_tro_1001', (129, 742, 126, 126))]:
        assert tuple(unity_sprite_rect(troops, name)) == rect, name
    mediator = (ROOT / 'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/ArmyTrain/TrainArmyMediator.cs').read_text(encoding='utf-8')
    assert 'view.m_pl_train_Animator.Play("Show")' in mediator
    show_detail = mediator.split('void ShowAttrDetail(', 1)[1].split('private ', 1)[0]
    for token in ['m_detailObj.gameObject.SetActive(true)', 'm_pl_right_ArabLayoutCompment.gameObject.SetActive(false)', 'm_pl_detailUp.gameObject.SetActive(false)']:
        assert token in show_detail, token
    assert 'm_pl_left' not in show_detail

    assert 'm_headViewList.Add(view.m_UI_Model_ArmyTrainHead_1)' in mediator
    cpp = (ROOT / 'Source/Rok/RokUnityFeatureScreenWidget.cpp').read_text(encoding='utf-8')
    scope = cpp.split('void URokUnityFeatureScreenWidget::ApplyTrainParameterLayout', 1)[1].split('void URokUnityFeatureScreenWidget::BuildTrainArmyDetail', 1)[0]
    for token in ['TrainArmyAnimatedChildren.Add(Right)', 'TrainArmyMinus', 'TrainArmyPlus', 'TrainArmyMax',
                  'Detail.Name', 'Detail.Description', 'TrainArmyQuantityInput', 'TrainResourceCostRoot']:
        assert token in scope, token
    assert all(token not in scope for token in ['SubmitCommand', 'SendSproto', 'TrySpend', 'AddTroop'])
    shared = cpp.split('void URokUnityFeatureScreenWidget::BuildTrainArmySharedDisplay', 1)[1].split('void URokUnityFeatureScreenWidget::ApplyTrainParameterLayout', 1)[0]
    for token in ['TrainParameterLeftArt', 'TrainParameterLeftBacking', 'TrainParameterStaticPreview', '模型加载失败', 'TrainArmyAnimatedModel', 'TrainArmyDetailPortrait', 'TrainArmySelectedIcon', 'FVector2D(270, 250)', 'FVector2D(280)']:
        assert token in shared, token
    build_scope = cpp.split('void URokUnityFeatureScreenWidget::BuildTrainArmy()', 1)[1].split('void URokUnityFeatureScreenWidget::BuildTrainArmyQuantityControls', 1)[0]
    assert build_scope.index('BuildTrainArmySharedDisplay(') < build_scope.index('BuildTrainArmyDetail(') < build_scope.index('TrainFirstChild')
    assert 'bTrainArmyDetailVisible ?' not in build_scope
    assert 'TrainArmyContentShade' not in build_scope
    output = ROOT / 'Saved/RokUnityMigration/train_parameter_layout_verify.json'
    output.write_text(json.dumps({'source_verified': True, 'source_layout': source,
        'remaining': ['real runtime screenshots', 'Spine presentation remains static source head preview',
                      'training queue, instant completion price and owned roster unavailable'],
        'runtime_verified': False, 'visual_verified': False}, ensure_ascii=False, indent=2), encoding='utf-8')
    print('PASS: Chinese training parameter rectangles, source head ordering, quantity geometry and animation ownership')

if __name__ == '__main__':
    main()
