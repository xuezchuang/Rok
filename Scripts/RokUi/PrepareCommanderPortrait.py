"""Extract the dynamic model slot and its real input target from the commander view."""
from copy import deepcopy
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
data = json.loads((root / 'commander_skill_0.json').read_text(encoding='utf-8-sig'))
portrait = deepcopy(next(node for node in data['nodes'] if node['name'] == 'CommanderAnimatedPortrait'))
portrait['name'] = 'CommanderPortraitSlot'
portrait['class'] = '/Script/UMG.CanvasPanel'
portrait['props'] = {'Visibility': 'SelfHitTestInvisible'}
portrait['children'] = []
action = deepcopy(next(node for node in data['nodes'] if node['name'] == 'CommanderPortraitAction'))
data['nodes'] = [portrait, action]
(root / 'commander_portrait_page.json').write_text(
    json.dumps(data, ensure_ascii=False, indent=1), encoding='utf-8')
print('commander_portrait_page', len(data['nodes']))
