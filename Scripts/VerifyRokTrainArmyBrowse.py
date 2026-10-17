"""Source contracts for read-only tier browsing and the visual regression fix."""
import csv
import json
from pathlib import Path
from PIL import Image
from BuildRokUnityUiAnimationManifest import Builder

ROOT = Path(__file__).resolve().parents[1]


def main():
    builder = Builder()
    mediator = builder.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/ArmyTrain/TrainArmyMediator.cs')
    click = mediator.split('private void ClickHead(int index)', 1)[1].split('//获取训练容量', 1)[0]
    assert 'm_selectIndex = index;' in click and 'm_currTrainCount = m_trainCapacity;' in click
    assert click.index('m_selectIndex = index;') < click.index('if (m_selectIndex > m_unlockMaxIndex)')
    assert 'view.m_UI_Model_TrainResCost.gameObject.SetActive(false)' in click
    assert 'view.m_UI_btn_train.gameObject.SetActive(false)' in click
    assert 'view.m_UI_btn_complete.gameObject.SetActive(false)' in click
    assert 'SendSproto' not in click
    ue = builder.read('Source/Rok/RokUnityFeatureScreenWidget.cpp')
    route = ue.split('if (ActionId >= 1900 && ActionId < 1905)', 1)[1].split('if (ActionId >= 1920', 1)[0]
    assert 'SelectedTrainTier = RequestedTier;' in route
    assert 'SelectedTrainCount = FMath::Max(1, TrainArmyBaseCapacity);' in route
    assert 'Troop->StudyId' not in route
    assert all(token not in route for token in ('SendSproto', 'SubmitCommand', 'TrySpend', 'AddTroop', 'RemoveTroop'))
    training = ue.split('if (ActionId == 1940)', 1)[1].split('if (ActionId >= 2000', 1)[0]
    assert 'if (Troop->StudyId != 0)' in training and 'TrainingBoundary' in training
    assert all(token not in training for token in ('SendSproto', 'SubmitCommand', 'TrySpend', 'AddTroop', 'RemoveTroop'))
    assert 'Child->SetVisibility(ESlateVisibility::Collapsed)' in ue
    assert 'TrainArmyStudyStatus' in ue and '科技待同步' in ue
    source = ROOT / 'ref/config-tables/编译客户端配置/CSV/Arms.csv'
    rows = list(csv.DictReader(source.read_text(encoding='utf-8-sig').splitlines()))[1:]
    standard = [r for r in rows if int(r['subType']) == 1 and int(r['armsType']) in (1, 2, 3, 4)]
    assert len(standard) == 20
    for kind in range(1, 5):
        assert sorted(int(r['armsLv']) for r in standard if int(r['armsType']) == kind) == [1, 2, 3, 4, 5]
    prefab = builder.read('ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_troops/UIArmyTrain/UI_Item_TrainSoldierDetail.prefab')
    description = prefab.split('m_GameObject: {fileID: 1815539305356269515}', 2)[-1].split('--- !u!1 ', 1)[0]
    assert 'm_FontSize: 20' in description and 'm_VerticalOverflow: 0' in description
    assert 'm_SizeDelta: {x: 510.8, y: 74.1}' in prefab
    tip = builder.read('ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Pop_TextTip.prefab')
    assert 'm_FontSize: 18' in tip and 'm_Material: {fileID: 0}' in tip
    tip_logic = builder.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/HelpTips/HelpTipMediator.cs')
    assert 'Frame = new Vector2(40f,40f)' in tip_logic
    assert 'view.m_lbl_text_LanguageText.rectTransform.sizeDelta + Frame' in tip_logic
    assert 'offset += view.m_img_arrowSideButtom_PolygonImage.rectTransform.sizeDelta.y' in tip_logic
    atlas = ROOT / 'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png'
    image = Image.open(atlas).convert('RGBA')
    alpha = image.getpixel((774 + 36, image.height - 276 - 72 + 36))[3]
    assert alpha == 204, 'Keep actual source tooltip transparency'
    assert '18.0f * 72.0f / 96.0f' in ue and 'Description->SetClipping(EWidgetClipping::ClipToBounds)' in ue
    report = {'source_verified': True, 'standard_tiers': 20, 'sources': builder.sources,
              'authority': 'Selection is presentation-only; study ownership unknown, training controls hidden, no request or inventory mutation',
              'visual_fix': {'description_pixels': 20, 'description_points': 15, 'description_rect': [510.8, 74.1],
                             'tip_pixels': 18, 'tip_points': 13.5, 'tip_text_width': 500, 'tip_background_width': 540,
                             'source_center_alpha': alpha, 'default_text_shadow_removed': True},
              'runtime_entry': 'city InitId6 -> BuildingFeatureButton -> TrainArmyTier_0..4 -> TrainArmyDetailButton -> TrainArmyDetailBackButton',
              'remaining_gates': ['Rok.Ui.TrainArmy.TierBrowse and regression tests', 'Real tier runtime audit', 'Re-capture and inspect TrainDetail visual overlap/readability']}
    output = ROOT / 'Saved/RokUnityMigration/train_army_browse_source_verify.json'
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'source_verified': True, 'standard_tiers': 20, 'source_tip_alpha': alpha}))


if __name__ == '__main__':
    main()
