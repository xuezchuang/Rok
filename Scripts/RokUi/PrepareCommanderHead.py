"""Build a reusable commander head spec from actual legacy roster states."""
from copy import deepcopy
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
source = json.loads((root / 'commander_skill_0.json').read_text(encoding='utf-8-sig'))


def walk(nodes):
    for node in nodes:
        yield node
        yield from walk(node.get('children', []))


by_name = {node['name']: node for node in walk(source['nodes'])}
head = by_name['CommanderHead_0']
nodes = deepcopy(head['children'])
dark = deepcopy(by_name['CommanderHeadDark_5'])
dark['name'] = 'CommanderHeadDark_0'
nodes.append(dark)
missing = deepcopy(by_name['CommanderHeadStateShade_0'])
missing['name'] = 'CommanderHeadMissing_0'
missing['props']['BrushColor'] = '(R=0.070000,G=0.070000,B=0.075000,A=0.960000)'
missing['slot'] = deepcopy(by_name['CommanderHeadPortrait_0']['slot'])
nodes.append(missing)
names = {node['name'] for node in nodes}
assert names == {
    'CommanderHeadFrame_0', 'CommanderHeadPortrait_0', 'CommanderHeadSelection_0',
    'CommanderHeadStateShade_0', 'CommanderHeadState_0', 'CommanderHeadAction_0',
    'CommanderHeadDark_0', 'CommanderHeadMissing_0',
}
out = {'version': source['version'], 'label': 'Commander head item',
       'root': 'ItemRoot', 'nodes': nodes, 'notes': []}
(root / 'commander_head_page.json').write_text(
    json.dumps(out, ensure_ascii=False, indent=1), encoding='utf-8')
print('commander_head_page', len(nodes))
