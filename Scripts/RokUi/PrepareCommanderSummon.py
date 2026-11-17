"""Combine the native unowned and enabled-recruit presentation states."""
from copy import deepcopy
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
disabled = json.loads((root / 'commander_unowned.json').read_text(encoding='utf-8-sig'))
enabled = json.loads((root / 'commander_summon_enabled.json').read_text(encoding='utf-8-sig'))
names = {'CommanderSummonPanel', 'CommanderSculpture', 'CommanderSummonRequirement',
         'CommanderRecruitDisabledArt', 'CommanderSummonAction'}
disabled['nodes'] = [node for node in disabled['nodes'] if node['name'] in names]
assert {node['name'] for node in disabled['nodes']} == names
button = next(node for node in disabled['nodes'] if node['name'] == 'CommanderSummonAction')
enabled_button = next(node for node in enabled['nodes'] if node['name'] == 'CommanderSummonAction')
content = button['children'][0]
enabled_content = enabled_button['children'][0]
background = next(node for node in enabled_content['children'] if node['name'] == 'CommanderSummonActionBackground')
content['children'].insert(0, deepcopy(background))
(root / 'commander_summon_page.json').write_text(
    json.dumps(disabled, ensure_ascii=False, indent=1), encoding='utf-8')
print('commander_summon_page', len(disabled['nodes']))
