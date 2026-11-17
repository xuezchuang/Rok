"""Extract the account-provided commander progress and talent state labels."""
from pathlib import Path
import json

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
data = json.loads((root / 'commander_talent_editor.json').read_text(encoding='utf-8-sig'))
names = {'CommanderServerProgress', 'CommanderServerTalentState'}
data['nodes'] = [node for node in data['nodes'] if node['name'] in names]
assert {node['name'] for node in data['nodes']} == names
(root / 'commander_server_state_page.json').write_text(
    json.dumps(data, ensure_ascii=False, indent=1), encoding='utf-8')
print('commander_server_state_page', len(data['nodes']))
