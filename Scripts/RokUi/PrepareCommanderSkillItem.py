"""Extract a reusable skill icon, level and action from the legacy commander view."""
from copy import deepcopy
import json
from pathlib import Path
from CutItemSpec import shift_slot

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
source = json.loads((root / 'commander_skill_0.json').read_text(encoding='utf-8-sig'))
by_name = {node['name']: node for node in source['nodes']}
names = ('CommanderSkillIcon_0', 'CommanderSkillIndex_0', 'CommanderSkillAction_0')
nodes = [deepcopy(by_name[name]) for name in names]
for node in nodes:
    shift_slot(node, (978.0, 468.0))
pending = deepcopy(by_name['CommanderSkillShade'])
pending['name'] = 'CommanderSkillPending_0'
pending['props']['BrushColor'] = '(R=0.100000,G=0.100000,B=0.105000,A=0.920000)'
pending['slot'] = deepcopy(by_name['CommanderSkillIcon_0']['slot'])
shift_slot(pending, (978.0, 468.0))
selected = deepcopy(pending)
selected['name'] = 'CommanderSkillPendingSelectedStyle_0'
selected['props']['BrushColor'] = '(R=0.460000,G=0.340000,B=0.100000,A=0.920000)'
selected['props']['Visibility'] = 'Collapsed'
nodes.extend((pending, selected))
out = {'version': source['version'], 'label': 'Commander skill item',
       'root': 'ItemRoot', 'nodes': nodes, 'notes': []}
(root / 'commander_skill_item_page.json').write_text(
    json.dumps(out, ensure_ascii=False, indent=1), encoding='utf-8')
print('commander_skill_item_page', len(nodes))
