"""Extract the whole PlayerHead table. Missing artwork stays explicitly missing."""
import csv
import hashlib
import json
import re
from pathlib import Path
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'Saved/RokUnityMigration/player_head_catalog.json'


def build():
    source = ROOT / 'ref/config-tables/编译客户端配置/CSV/PlayerHead.csv'
    language, _ = read_language_lookup()
    rows = list(csv.DictReader(source.read_text(encoding='utf-8-sig').splitlines()))[1:]
    art = ROOT / 'ref/client-unity/Assets/BundleAssets/UI'
    entries, atlases = [], {}
    for row in rows:
        atlas, sprite = re.fullmatch(r'(.+)\[(.+)\]', row['icon']).groups()
        if atlas not in atlases:
            matches = list(art.rglob(atlas + '.tpsheet'))
            if len(matches) > 1:
                raise ValueError('Ambiguous atlas: ' + atlas)
            info = {'sprites': {}, 'size': [0, 0], 'source': '', 'target': ''}
            if matches:
                sheet = matches[0]
                texture = sheet.with_suffix('.png')
                if texture.exists():
                    lines = sheet.read_text(encoding='utf-8-sig').splitlines()
                    info['size'] = list(map(int, next(line[6:] for line in lines if line.startswith(':size=')).split('x')))
                    info['source'] = str(texture.relative_to(ROOT)).replace('\\', '/')
                    info['target'] = ('/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_player_head' if atlas == 'player_head'
                                      else '/Game/RokPrototype/UI/PlayerHeadAtlases/' + atlas)
                    for line in lines:
                        if line and not line.startswith(('#', ':')):
                            parts = line.split(';')
                            info['sprites'][parts[0]] = list(map(int, parts[1:5]))
            atlases[atlas] = info
        info = atlases[atlas]
        rect = info['sprites'].get(sprite, [0, 0, 0, 0])
        entries.append({'id': int(row['ID']), 'group': int(row['group']), 'get': int(row['get']),
                        'name': localized_text(language, int(row['l_nameID'] or 0), 'cn'),
                        'description': localized_text(language, int(row['l_decID'] or 0), 'cn'),
                        'tag': localized_text(language, int(row['l_tagID'] or 0), 'cn'),
                        'icon': row['icon'], 'atlas': atlas, 'sprite': sprite,
                        'texture': info['target'], 'rect': rect, 'size': info['size'],
                        'art_available': rect[2] > 0 and rect[3] > 0})
    result = {'source': str(source.relative_to(ROOT)), 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'entries': entries, 'atlases': atlases,
              'missing_art_ids': [row['id'] for row in entries if not row['art_available']],
              'server_ownership_available': False, 'visual_verified': False}
    assert len({row['id'] for row in entries}) == len(entries)
    assert sum(row['group'] == 1 for row in entries) == 12
    assert sum(row['group'] == 2 for row in entries) == 16
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'heads': 12, 'frames': 16, 'missing_art_ids': result['missing_art_ids'],
                      'output': str(OUTPUT)}, ensure_ascii=False))
    return result


if __name__ == '__main__':
    build()
