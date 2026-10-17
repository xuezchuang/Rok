"""Verify source-derived cost presentation; runtime/visual acceptance stays separate."""
import json
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
SOURCES = []
def read(path, encoding='utf-8'):
    SOURCES.append(path)
    return resolve(path).read_text(encoding=encoding)

def main():
    common = read('ref/client-unity/Assets/Scripts/Hotfix/Common/Utils/CommonTools.cs', 'latin1').split('public class ResCostModel', 1)[1]
    for token in ['ResCostData[data.Key] > 0', 'data.Value.gameObject.SetActive(false)',
                  'ClientUtils.CurrencyFormat(ResCostData[data.Key])', 'm_playerProxy.GetResNumByType']:
        assert token in common, token
    mediator = read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/ArmyTrain/TrainArmyMediator.cs')
    for token in ['needFood * trainNum', 'needWood * trainNum', 'needStone * trainNum', 'needGlod * trainNum']:
        assert token in mediator, token
    util = read('ref/client-unity/Assets/Scripts/Client/Utils/ClientUtils.cs')
    assert 'Math.Floor((tmpNum - (int)tmpNum) * 10)' in util
    prefab_root = 'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_noaddressable/'
    grid = read(prefab_root + 'UI_Model_ResCost.prefab')
    assert 'm_StartCorner: 1' in grid and '_arab_Layout: 512' in grid
    assert 'm_ChildAlignment: 4' in grid
    train = read(prefab_root + 'UI_Model_TrainResCost.prefab')
    assert 'propertyPath: m_CellSize.x\n      value: 137' in train
    cell = read(prefab_root + 'UI_Model_ArmyTrainRes.prefab')
    for token in ['m_SizeDelta: {x: 42, y: 42}', 'm_AnchoredPosition: {x: 43.36, y: 0}', 'm_FontSize: 20']:
        assert token in cell, token
    arab = read('ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs')
    assert 'isArabUI == Skyunion.LanguageUtils.IsArabic()' in arab
    assert 'GridLayoutGroup.Corner.UpperLeft' in arab
    atlas = read('ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png.meta')
    for name, x, y in [('img_com_cur1', 865, 349), ('img_com_cur2', 1085, 489), ('img_com_cur3', 947, 431), ('img_com_cur4', 865, 431)]:
        block = atlas.split('name: ' + name + '\n', 1)[1].split('alignment:', 1)[0]
        assert f'x: {x}' in block and f'y: {y}' in block and 'width: 80' in block
    cpp = (ROOT / 'Source/Rok/RokUnityFeatureScreenWidget.cpp').read_text(encoding='utf-8')
    scope = cpp.split('void URokUnityFeatureScreenWidget::BuildTrainResourceCost()', 1)[1].split('void URokUnityFeatureScreenWidget::BuildTrainArmyDetail', 1)[0]
    assert 'RokTrainResourceCost::Format' in scope
    assert '加成、库存待同步' in scope
    assert all(token not in scope for token in ['GetResources(', 'TrySpend', 'SubmitCommand', 'SendSproto', 'AddTroop'])
    output = ROOT / 'Saved/RokUnityMigration/train_resource_cost_source_verify.json'
    output.write_text(json.dumps({'source_verified': True, 'sources': SOURCES,
        'authority': 'Config costs only. Stock sufficiency colors, bonuses and AddRes purchase are unimplemented.',
        'layout': '137x40 centered source cells; reused existing UE train panel cost area, full panel placement remains separate.',
        'runtime_verified': False, 'visual_verified': False}, ensure_ascii=False, indent=2), encoding='utf-8')
    print('PASS: training resource cost source contracts, 4 atlas identities, Chinese grid and no authority mutation')

if __name__ == '__main__':
    main()
