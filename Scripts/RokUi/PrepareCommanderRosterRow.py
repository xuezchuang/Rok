"""Extract editable roster row and head slots from the legacy commander tree."""
from copy import deepcopy
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
data = json.loads((root / 'commander_skill_0.json').read_text(encoding='utf-8-sig'))


def walk(nodes):
    for node in nodes:
        yield node
        yield from walk(node.get('children', []))


by_name = {node['name']: node for node in walk(data['nodes'])}
row = by_name['CommanderRosterRow_0']
heads = deepcopy(row['children'])
assert [node['name'] for node in heads] == ['CommanderHead_0', 'CommanderHead_1']
for index, node in enumerate(heads):
    node['name'] = f'CommanderRosterHeadSlot_{index}'
    node['children'] = []
    node['props'] = {'Visibility': 'SelfHitTestInvisible'}
size = deepcopy(by_name['CommanderRosterRowSize_0'])
size['name'] = 'CommanderRosterDesignedSize'
size['children'] = []
size['props']['Visibility'] = 'Collapsed'
size['slot'] = {'class': '/Script/UMG.CanvasPanelSlot', 'props': {}}
out = {'version': data['version'], 'label': 'Commander roster row',
       'root': 'ItemRoot', 'nodes': heads + [size], 'notes': []}
(root / 'commander_roster_row_page.json').write_text(
    json.dumps(out, ensure_ascii=False, indent=1), encoding='utf-8')
print('commander_roster_row_page', len(out['nodes']))
