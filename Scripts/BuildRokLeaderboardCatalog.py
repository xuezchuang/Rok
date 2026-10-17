"""Extract the nine base-ranking bindings used by UI_Win_RankingMediator."""
import csv
import json
import re
from pathlib import Path
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text
from PrepareRokUiTextures import unity_sprite_rect
from rok_data_paths import DATA_ROOT

ROOT = Path(__file__).resolve().parents[1]


def build():
    source = ROOT / 'ref/config-tables/编译客户端配置/CSV/Leaderboard.csv'
    rows = list(csv.DictReader(source.read_text(encoding='utf-8-sig').splitlines()))[1:]
    base = sorted((row for row in rows if row['list'] == '2'), key=lambda row: int(row['ID']))
    language, _ = read_language_lookup()
    meta = ROOT / 'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_player/ui_player.png.meta'
    atlas = meta.read_text(encoding='utf-8-sig')
    result = []
    for row in base[:9]:  # BindEntryBtns creates exactly nine source slots.
        icon = row['icon']
        rect = []
        if icon:
            match = re.fullmatch(r'ui_player\[([^]]+)\]', icon)
            assert match, icon
            rect = list(unity_sprite_rect(atlas, match[1]))
        assert not row['flag'], 'This slice has no nonempty base-ranking flag binding'
        columns = [localized_text(language, int(key), 'cn') for key in row['typeNameID'].split('|') if key]
        assert len(columns) in (0, 2)
        result.append({'id': int(row['ID']), 'list': 2, 'target': int(row['target']), 'type': int(row['type']),
                       'show_limit': int(row['showLimit']), 'name': localized_text(language, int(row['nameID']), 'cn') if row['nameID'] else '',
                       'columns': columns, 'icon': icon, 'rect': rect})
    assert len(result) == 9
    output = DATA_ROOT / 'leaderboard_catalog.json'
    output.write_text(json.dumps({'source': str(source.relative_to(ROOT)), 'entry_prefab': 'UI_Item_GuildRankingBtn',
        'server_gate': 'Rank_ShowRankFirst.response; UE is explicitly a configuration preview', 'entries': result}, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'entries': len(result), 'output': str(output)}))


if __name__ == '__main__':
    build()
