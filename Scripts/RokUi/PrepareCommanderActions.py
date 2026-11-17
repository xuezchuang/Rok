"""Extract commander biography and owned-hero action presentation."""
from pathlib import Path
import json

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
data = json.loads((root / 'commander_skill_0.json').read_text(encoding='utf-8-sig'))
names = {'CommanderBiographyAction', 'CommanderSelectedSkillSummary', 'CommanderSkillUpgrade', 'CommanderTalent'}
data['nodes'] = [node for node in data['nodes'] if node['name'] in names]
assert {node['name'] for node in data['nodes']} == names
(root / 'commander_actions_page.json').write_text(
    json.dumps(data, ensure_ascii=False, indent=1), encoding='utf-8')
print('commander_actions_page', len(data['nodes']))
