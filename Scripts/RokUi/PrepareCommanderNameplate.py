"""Extract commander title, name and three talent buttons from the live legacy view."""
from pathlib import Path
import json

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
data = json.loads((root / 'commander_skill_0.json').read_text(encoding='utf-8-sig'))
names = {'CommanderAppellation', 'CommanderName'}
names.update(f'CommanderRoleBg_{index}' for index in range(3))
names.update(f'CommanderTalentLabel_{index}' for index in range(3))
data['nodes'] = [node for node in data['nodes'] if node['name'] in names]
assert {node['name'] for node in data['nodes']} == names
(root / 'commander_nameplate_page.json').write_text(
    json.dumps(data, ensure_ascii=False, indent=1), encoding='utf-8')
print('commander_nameplate_page', len(data['nodes']))
