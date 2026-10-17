"""Validate base ranking source IDs, sprite bindings and two-stage close behavior."""
import csv
import json
from pathlib import Path
from PIL import Image
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text
from PrepareRokUiTextures import unity_sprite_rect
from rok_data_paths import DATA_ROOT, resolve

ROOT = Path(__file__).resolve().parents[1]


def main():
    data = json.loads((DATA_ROOT / 'leaderboard_catalog.json').read_text(encoding='utf-8'))['entries']
    rows = list(csv.DictReader((ROOT / 'ref/config-tables/编译客户端配置/CSV/Leaderboard.csv').read_text(encoding='utf-8-sig').splitlines()))[1:]
    expected = sorted((row for row in rows if row['list'] == '2'), key=lambda row: int(row['ID']))[:9]
    language, _ = read_language_lookup()
    atlas_path = ROOT / 'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_player/ui_player.png'
    assert Image.open(atlas_path).size == (512, 1024)
    atlas = Path(str(atlas_path) + '.meta').read_text(encoding='utf-8-sig')
    assert len(data) == len(expected) == 9
    for actual, source in zip(data, expected):
        assert actual['id'] == int(source['ID']) and actual['list'] == 2
        assert actual['type'] == int(source['type']) and actual['target'] == int(source['target'])
        assert actual['show_limit'] == int(source['showLimit'])
        assert actual['name'] == (localized_text(language, int(source['nameID']), 'cn') if source['nameID'] else '')
        assert actual['icon'] == source['icon']
        assert actual['columns'] == [localized_text(language, int(key), 'cn') for key in source['typeNameID'].split('|') if key]
        if actual['icon']:
            name = actual['icon'].split('[', 1)[1][:-1]
            assert actual['rect'] == list(unity_sprite_rect(atlas, name))
        else:
            assert actual['rect'] == []
    assert data[-1]['id'] == 401 and not data[-1]['name'] and not data[-1]['icon']
    source_path = 'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Ranking/UI_Win_RankingMediator.cs'
    mediator = resolve(source_path).read_text(encoding='utf-8-sig')
    bind = mediator.split('private void BindEntryBtns()', 1)[1].split('private void DataProcess()', 1)[0]
    assert bind.count('m_entryBtns.Add(') == 9
    assert 'm_baseLeaderboardDefines.Sort((a, b) => { return a.ID - b.ID; });' in mediator
    gate = mediator.split('void QueryShowRankFirstRes(', 1)[1].split('private void BindEntryBtns()', 1)[0]
    assert 'view.m_pl_Total_GridLayoutGroup.gameObject.SetActive(true)' in gate
    close = mediator.split('private void onClose()', 1)[1]
    assert 'if (m_isOpenDetail && view.data == null)' in close
    assert close.index('m_isOpenDetail = false') < close.index('CoreUtils.uiManager.CloseUI(UI.s_ranking)')
    cpp = (ROOT / 'Source/Rok/RokUnityModalWidget.cpp').read_text(encoding='utf-8')
    close_target = cpp.split('void URokUnityModalWidget::RequestCloseModal()', 1)[1].split('TSharedRef<SWidget>', 1)[0]
    assert close_target.index('SelectedRankingCategory = INDEX_NONE') < close_target.index('WindowAnimation.Close()')
    assert '排行榜配置预览' in cpp and 'RankPlayerVictories' not in cpp and 'RankEventPoints' not in cpp
    output = ROOT / 'Saved/RokUnityMigration/leaderboard_catalog_verify.json'
    output.write_text(json.dumps({'source_verified': True, 'source_mediator': source_path,
        'ids': [row['id'] for row in data], 'icon_count': sum(bool(row['icon']) for row in data),
        'server_gate': 'Rank_ShowRankFirst.response unavailable; explicit config preview only',
        'remaining_gates': ['Rok.Ui.PlayerProfile.LeaderboardCatalog', 'profile route screenshots and two-stage close']}, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'source_verified': True, 'entries': len(data), 'output': str(output)}))


if __name__ == '__main__':
    main()
