"""Merge offline chat states into one editable page with distinct selection visuals."""
from copy import deepcopy
from pathlib import Path
import json

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'

def load(name):
    return json.loads((root / (name + '.json')).read_text(encoding='utf-8-sig'))

def walk(nodes):
    for node in nodes:
        yield node
        yield from walk(node.get('children', []))

def lookup(spec, name):
    return next(node for node in walk(spec['nodes']) if node['name'] == name)

def merge(target, extra):
    # Merge children by stable name while retaining the primary spec's design properties.
    by_name = {node['name']: node for node in target}
    for node in extra:
        prior = by_name.get(node['name'])
        if prior is None:
            prior = deepcopy(node)
            target.append(prior)
            by_name[node['name']] = prior
        else:
            merge(prior.setdefault('children', []), node.get('children', []))

names = ['chat_empty', 'chat_fixture_world', 'chat_fixture_alliance', 'chat_fixture_private', 'chat_fixture_emoji']
specs = {name: load(name) for name in names}
page = deepcopy(specs['chat_empty'])
for name in names[1:]:
    merge(page['nodes'], specs[name]['nodes'])

# Preserve both normal and selected appearance as authored widget properties.
selected_specs = ['chat_fixture_world', 'chat_fixture_alliance', 'chat_fixture_private']
for i in range(3):
    row = lookup(page, f'ChatChannelRow_{i}')
    normal_spec = specs[selected_specs[(i + 1) % 3]]
    selected_spec = specs[selected_specs[i]]
    for prefix in ('ChatChannelShade', 'ChatChannelLabel'):
        original_name = f'{prefix}_{i}'
        normal = deepcopy(lookup(normal_spec, original_name))
        selected = deepcopy(lookup(selected_spec, original_name))
        selected['name'] = f'{prefix}Selected_{i}'
        idx = next(j for j, n in enumerate(row['children']) if n['name'] == original_name)
        row['children'][idx] = normal
        row['children'].insert(idx + 1, selected)

# Channel art comes from two Unity atlas rectangles; keep each as an editable image.
canvas = lookup(page, 'ChatContentCanvas')
alliance_icon = deepcopy(lookup(specs['chat_fixture_alliance'], 'ChatConversationChannelIcon'))
alliance_icon['name'] = 'ChatConversationAllianceIcon'
pos = next(i for i, n in enumerate(canvas['children']) if n['name'] == 'ChatConversationChannelIcon')
canvas['children'].insert(pos + 1, alliance_icon)
page['label'] = 'Chat'
(root / 'chat_page.json').write_text(json.dumps(page, ensure_ascii=False, indent=1), encoding='utf-8')
print('chat page nodes:', sum(1 for _ in walk(page['nodes'])))
