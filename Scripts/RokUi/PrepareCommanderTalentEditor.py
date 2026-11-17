"""Extract the offline server-fixture talent editor and its dynamic node row."""
from copy import deepcopy
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
data = json.loads((root / 'commander_talent_editor.json').read_text(encoding='utf-8-sig'))
names = {
    'CommanderTalentEditor', 'CommanderTalentEditorTitle', 'CommanderTalentEditorBack',
    'CommanderTalentPage_1', 'CommanderTalentPage_2', 'CommanderTalentPage_3',
    'CommanderTalentPoints', 'CommanderTalentNodeScroll', 'CommanderTalentNameInput',
    'CommanderTalentRename', 'CommanderTalentSwitch', 'CommanderTalentReset',
}
data['nodes'] = [node for node in data['nodes'] if node['name'] in names]
assert {node['name'] for node in data['nodes']} == names
scroll = next(node for node in data['nodes'] if node['name'] == 'CommanderTalentNodeScroll')
row = deepcopy(scroll['children'][0]['children'][0])
assert row['name'] == 'CommanderTalentNode_1'
row_nodes = row['children']
assert {node['name'] for node in row_nodes} == {'CommanderTalentNode_1Label', 'CommanderTalentNode_1Learn'}
scroll['children'] = []
waiting = deepcopy(next(node for node in data['nodes'] if node['name'] == 'CommanderTalentPoints'))
waiting['name'] = 'CommanderTalentEditorWaiting'
waiting['slot']['props']['LayoutData'] = waiting['slot']['props']['LayoutData'].replace(
    'Top=209.000000', 'Top=179.000000').replace('Bottom=30.000000', 'Bottom=32.000000')
data['nodes'].append(waiting)
(root / 'commander_talent_editor_page.json').write_text(
    json.dumps(data, ensure_ascii=False, indent=1), encoding='utf-8')
row_spec = {'version': data['version'], 'label': 'Commander talent node',
            'root': 'ItemRoot', 'nodes': row_nodes, 'notes': []}
(root / 'commander_talent_node_page.json').write_text(
    json.dumps(row_spec, ensure_ascii=False, indent=1), encoding='utf-8')
confirm = json.loads((root / 'commander_talent_confirm.json').read_text(encoding='utf-8-sig'))
confirm_names = {'CommanderConfirmShade', 'CommanderConfirmCost', 'CommanderConfirmAccept', 'CommanderConfirmCancel'}
confirm['nodes'] = [node for node in confirm['nodes'] if node['name'] in confirm_names]
assert {node['name'] for node in confirm['nodes']} == confirm_names
(root / 'commander_confirm_page.json').write_text(
    json.dumps(confirm, ensure_ascii=False, indent=1), encoding='utf-8')
print('commander_talent_editor_page', len(data['nodes']), 'node', len(row_nodes), 'confirm', len(confirm['nodes']))
