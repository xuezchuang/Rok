"""Prepare commander account-waiting and missing-preview states."""
from copy import deepcopy
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
data = json.loads((root / 'commander_server_waiting.json').read_text(encoding='utf-8-sig'))
names = {'CommanderWaitingBackground', 'CommanderServerWaiting'}
data['nodes'] = [node for node in data['nodes'] if node['name'] in names]
assert {node['name'] for node in data['nodes']} == names
missing = deepcopy(next(node for node in data['nodes'] if node['name'] == 'CommanderServerWaiting'))
missing['name'] = 'CommanderPreviewUnavailable'
missing['props']['Font'] = missing['props']['Font'].replace('Size=26.000000', 'Size=18.000000').replace(
    'TypefaceFontName="Regular"', 'TypefaceFontName="Bold"')
missing['slot']['props']['LayoutData'] = '(Offsets=(Left=430.000000,Top=385.000000,Right=420.000000,Bottom=36.000000),Anchors=(Minimum=(X=0.000000,Y=0.000000),Maximum=(X=0.000000,Y=0.000000)),Alignment=(X=0.000000,Y=0.000000))'
missing['slot']['props']['ZOrder'] = '2'
data['nodes'].append(missing)
(root / 'commander_status_page.json').write_text(
    json.dumps(data, ensure_ascii=False, indent=1), encoding='utf-8')
print('commander_status_page', len(data['nodes']))
