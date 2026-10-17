"""Read source behavior/layout contracts, without claiming runtime acceptance."""
import json
from pathlib import Path
from BuildRokUnityUiAnimationManifest import Builder

ROOT = Path(__file__).resolve().parents[1]


def main():
    builder = Builder()
    common_path = 'ref/client-unity/Assets/Scripts/Hotfix/Common/Utils/CommonTools.cs'
    # This legacy file has non-UTF8 comments; executable tokens are ASCII.
    common = (ROOT / common_path).read_bytes().decode('latin1')
    logic = common.split('public class InputSliderControl', 1)[1].split('public class ResCostModel', 1)[0]
    for token in ['int.Parse(val)', 'num = MinVal;', 'num = MaxVal;', '(int)(val * MaxVal)',
                  '(float)num / MaxVal', 'm_isChangeInput = false;', 'm_isChangeSilder = false;',
                  'Time.realtimeSinceStartup - m_lastPlaySoundTime > 0.02f', 'RS.SoundUiCommonSlider']:
        assert token in logic, token
    mediator = builder.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/ArmyTrain/TrainArmyMediator.cs')
    assert 'm_UI_Model_TrainResCost.m_ipt_count_GameInput.textComponent.gameObject.SetActive(false)' in mediator
    prefab = builder.read('ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_noaddressable/UI_Model_TrainResCost.prefab')
    assert 'm_Direction: 1' in prefab and '_arab_Layout: 34' in prefab
    assert 'm_SizeDelta: {x: 460, y: 19}' in prefab
    arab = builder.read('ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs')
    assert 'isArabUI == Skyunion.LanguageUtils.IsArabic()' in arab
    assert 'compment.direction = Slider.Direction.LeftToRight;' in arab
    cpp = (ROOT / 'Source/Rok/RokUnityFeatureScreenWidget.cpp').read_text(encoding='utf-8')
    update = cpp.split('void URokUnityFeatureScreenWidget::UpdateTrainArmyQuantity(int32 Count, float SliderPosition)', 1)[1].split('void URokUnityFeatureScreenWidget::BuildTrainArmyDetail', 1)[0]
    assert 'TGuardValue<bool> Guard(bTrainArmyQuantitySync, true)' in update
    assert 'RebuildModeContent' not in update and 'bTrainArmyQuantityEditable' in update
    assert all(t not in update for t in ['SubmitCommand', 'SendSproto', 'TrySpend', 'AddTroop'])
    assert (ROOT / 'ref/client-unity/Assets/BundleAssets/Souds/Sound_Ui_CommonScroll.mp3').is_file()
    output = ROOT / 'Saved/RokUnityMigration/train_army_quantity_source_verify.json'
    output.write_text(json.dumps({'source_verified': True, 'sources': builder.sources, 'legacy_controller': common_path,
        'chinese_slider_direction': 'RightToLeft serialized + Awake mirror = LeftToRight',
        'authority': 'Local requested count only; unknown research hides controls; no added submit path',
        'remaining_gates': ['ImportRokTrainArmyQuantitySound.py', 'Rok.Ui.TrainArmy.QuantityInput', 'Real input/drag/focus and screenshot acceptance']}, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'source_verified': True, 'output': str(output)}))


if __name__ == '__main__':
    main()
