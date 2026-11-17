"""Extract the commander back button and bottom power display from the live shell."""
from copy import deepcopy
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
data = json.loads((root / 'commander_chrome_shell.json').read_text(encoding='utf-8-sig'))


def walk(nodes):
    for node in nodes:
        yield node
        yield from walk(node.get('children', []))


chrome = next(node for node in walk(data['nodes']) if node['name'] == 'CommanderChrome')
nodes = deepcopy(chrome['children'])
assert {node['name'] for node in nodes} == {'CommanderBack', 'CommanderPowerFooter'}
spec = {'version': data['version'], 'label': 'Commander chrome', 'root': 'ItemRoot',
        'nodes': nodes, 'notes': []}
(root / 'commander_chrome_page.json').write_text(
    json.dumps(spec, ensure_ascii=False, indent=1), encoding='utf-8')
print('commander_chrome_page', len(nodes))
