"""Independent identity and missing-art checks against CSV and tpsheet source."""
import csv
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
data = json.loads((root / 'Saved/RokUnityMigration/player_head_catalog.json').read_text(encoding='utf-8'))
source = root / data['source']
rows = list(csv.DictReader(source.read_text(encoding='utf-8-sig').splitlines()))[1:]
assert data['source_sha256'] == hashlib.sha256(source.read_bytes()).hexdigest()
assert len(data['entries']) == len(rows) == 28
for source_row, row in zip(rows, data['entries']):
    assert row['id'] == int(source_row['ID'])
    assert row['group'] == int(source_row['group'])
    assert row['get'] == int(source_row['get'])
    assert row['icon'] == source_row['icon']
    if not source_row['l_nameID']:
        assert not row['name']
    atlas = data['atlases'][row['atlas']]
    if row['art_available']:
        sheet = (root / atlas['source']).with_suffix('.tpsheet')
        parts = next(line.split(';') for line in sheet.read_text().splitlines()
                     if line.startswith(row['sprite'] + ';'))
        assert row['rect'] == list(map(int, parts[1:5]))
        x, y, w, h = row['rect']
        assert w > 0 and h > 0 and x + w <= row['size'][0] and y + h <= row['size'][1]
    else:
        assert not row['texture'] and row['rect'] == [0, 0, 0, 0]
        assert row['id'] in data['missing_art_ids']
assert data['entries'][12]['id'] == 2000 and data['entries'][12]['art_available']
assert data['entries'][13]['id'] == 2001 and data['entries'][13]['get'] == 1
print(json.dumps({'failures': [], 'catalog_rows': len(rows), 'available_art_rows': sum(row['art_available'] for row in data['entries']),
                  'missing_art_ids': data['missing_art_ids'], 'visual_verified': False}))
