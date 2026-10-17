"""Verify original ranking card geometry and Chinese ArabLayout transformations."""
import json
import re
from pathlib import Path
from BuildRokUnityUiAnimationManifest import Builder
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text
from PrepareRokUiTextures import unity_sprite_rect, unity_sprite_vector

ROOT = Path(__file__).resolve().parents[1]
UI = 'ref/client-unity/Assets/BundleAssets/UI/'


def documents(text):
    return dict(re.findall(r'--- !u!\d+ &(-?\d+)\n(.*?)(?=\n--- !u!|\Z)', text, re.S))


def node(docs, name):
    obj = next(value for value in docs.values() if '\n  m_Name: ' + name + '\n' in value)
    return [docs[key] for key in re.findall(r'component: {fileID: (-?\d+)}', obj)]


def vector(text, field):
    return tuple(map(float, re.search(field + r': {x: ([\d.-]+), y: ([\d.-]+)}', text).groups()))


def main():
    builder = Builder()
    window = documents(builder.read(UI + 'UIPrefabs/ui_prefabs_player/UIRanking/UI_Win_Ranking.prefab'))
    source = documents(builder.read(UI + 'UIPrefabs/ui_prefabs_guild/UIGuildRanking/UI_Item_GuildRankingBtn.prefab'))
    group = node(window, 'pl_Total')
    grid = next(value for value in group if 'm_CellSize:' in value)
    assert vector(group[0], 'm_SizeDelta') == (1040, 664)
    assert vector(grid, 'm_CellSize') == (340, 144) and vector(grid, 'm_Spacing') == (10, 60)
    assert 'm_ChildAlignment: 4' in grid and 'm_StartCorner: 1' in grid and 'm_Top: 20' in grid
    assert any('_arab_Layout: 512' in value for value in group)
    first_y = 20 + (664 - 20 - (3 * 144 + 2 * 60)) / 2
    assert first_y == 66
    expected = {'lbl_title': ((144.4, 2.1), (194.7, 55.8), 20, 1, 5),
                'lbl_mid': ((144.8, 44.9), (174.71, 50), 18, 0, 5),
                'lbl_name': ((146, 95.065), (178.64, 41.07), 18, 0, 2)}
    for name, (position, size, pixels, bold, alignment) in expected.items():
        components = node(source, name)
        rect = components[0]
        assert vector(rect, 'm_SizeDelta') == size
        x, y = vector(rect, 'm_AnchoredPosition')
        actual = (170 - x - size[0] / 2, 72 - y - size[1] / 2)
        assert all(abs(a - b) < .001 for a, b in zip(actual, position))
        text = next(value for value in components if 'm_FontSize:' in value)
        assert f'm_FontSize: {pixels}' in text and f'm_FontStyle: {bold}' in text and f'm_Alignment: {alignment}' in text
        assert any('_arab_Layout: 10' in value for value in components)
        assert not any('m_EffectDistance:' in value for value in components)
    bg = node(source, 'bg')
    assert vector(bg[0], 'm_SizeDelta') == (253.83002, 145.6)
    assert any('_arab_Layout: 2' in value for value in bg)
    flag, icon = node(source, 'img_flag'), node(source, 'img_icon')
    assert vector(flag[0], 'm_AnchoredPosition') == (99.900024, -31)
    assert vector(icon[0], 'm_AnchoredPosition') == (0, 14)
    assert vector(icon[0], 'm_SizeDelta') == (148, 176)
    atlas = builder.read(UI + 'UITextureStatic/ui_res_common/ui_common.png.meta')
    assert tuple(unity_sprite_rect(atlas, 'pl_com_1015')) == (1952, 758, 40, 146)
    assert unity_sprite_vector(atlas, 'pl_com_1015', 'border', ('x', 'y', 'z', 'w')) == [17, 49, 15, 46]
    language, _ = read_language_lookup()
    assert localized_text(language, 300232, 'cn') == '--'
    mediator = builder.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Ranking/UI_Win_RankingMediator.cs')
    assert 'crossBar = LanguageUtils.getText(300232)' in mediator
    assert '.Play(' not in mediator  # No invented overview/detail animation.
    output = ROOT / 'Saved/RokUnityMigration/leaderboard_overview_layout_verify.json'
    output.write_text(json.dumps({'source_verified': True, 'sources': builder.sources, 'first_cell_y': first_y,
        'source_background': 'pl_com_1015', 'source_font_points': [15, 13.5, 13.5], 'unknown_marker': '--',
        'remaining_gates': ['Rok.Ui.PlayerProfile.LeaderboardOverviewLayout', 'runtime card layout screenshots', 'server leaderboard state unavailable']}, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'source_verified': True, 'output': str(output)}))


if __name__ == '__main__':
    main()
