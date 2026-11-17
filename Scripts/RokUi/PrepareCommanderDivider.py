"""Extract the unowned-commander separator from the real server fixture."""
from copy import deepcopy
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
data = json.loads((root / 'commander_talent_editor.json').read_text(encoding='utf-8-sig'))


def walk(nodes):
    for node in nodes:
        yield node
        yield from walk(node.get('children', []))


divider = deepcopy(next(node for node in walk(data['nodes']) if node['name'] == 'CommanderUnrecruitedDivider'))
divider['slot'] = {'class': '/Script/UMG.CanvasPanelSlot', 'props': {
    'LayoutData': '(Offsets=(Right=304.000000,Bottom=38.000000),Anchors=(Minimum=(X=0.000000,Y=0.000000),Maximum=(X=0.000000,Y=0.000000)),Alignment=(X=0.000000,Y=0.000000))'}}
out = {'version': data['version'], 'label': 'Commander roster divider',
       'root': 'ItemRoot', 'nodes': [divider], 'notes': []}
(root / 'commander_divider_page.json').write_text(
    json.dumps(out, ensure_ascii=False, indent=1), encoding='utf-8')
print('commander_divider_page', len(out['nodes']))
