"""Extract the star, level and experience display from the commander screen."""
from copy import deepcopy
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
data = json.loads((root / 'commander_skill_0.json').read_text(encoding='utf-8-sig'))
names = {'CommanderLevel', 'CommanderExpTrack', 'CommanderExp',
         'CommanderSkillTitle', 'CommanderSkillShade'}
names.update(f'CommanderStar_{index}' for index in range(6))
data['nodes'] = [node for node in data['nodes'] if node['name'] in names]
assert {node['name'] for node in data['nodes']} == names
track = next(node for node in data['nodes'] if node['name'] == 'CommanderExpTrack')
fill = deepcopy(track)
fill['name'] = 'CommanderExpFill'
fill['props']['BrushColor'] = '(R=0.130000,G=0.430000,B=0.140000,A=0.850000)'
fill['slot']['props']['ZOrder'] = '6'
data['nodes'].append(fill)
(root / 'commander_progress_page.json').write_text(
    json.dumps(data, ensure_ascii=False, indent=1), encoding='utf-8')
print('commander_progress_page', len(data['nodes']))
