"""Extract the readonly skill and talent detail pages from legacy captures."""
from pathlib import Path
from copy import deepcopy
import json

root = Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
for source, target, names in (
    ('commander_skill_0', 'commander_skill_page', ('CommanderSkillDetailFrame', 'CommanderSkillDetailTitle', 'CommanderSkillDetailScroll', 'CommanderSkillDetailClose')),
    ('commander_talent_0', 'commander_talent_page', ('CommanderTalentDetailFrame', 'CommanderTalentDetailTitle', 'CommanderTalentDetailTips', 'CommanderTalentDetailClose')),
    ('commander_biography', 'commander_biography_page', ('CommanderBiographyPanel', 'CommanderCapacity', 'CommanderDescription')),
    ('commander_sort', 'commander_sort_page', ('CommanderSortMenu', 'CommanderSortOption_0', 'CommanderSortOption_1', 'CommanderSortOption_2', 'CommanderSortOption_3')),
):
    data = json.loads((root / (source + '.json')).read_text(encoding='utf-8-sig'))
    data['nodes'] = [node for node in data['nodes'] if node['name'] in names]
    if source == 'commander_sort':
        def walk(nodes):
            for node in nodes:
                yield node
                yield from walk(node.get('children', []))
        by_name = {node['name']: node for node in walk(data['nodes'])}
        for index, style_name in ((0, 'CommanderSortSelectedStyle'), (1, 'CommanderSortNormalStyle')):
            style = deepcopy(by_name[f'CommanderSortOption_{index}Label'])
            style['name'] = style_name
            style['props']['Visibility'] = 'Collapsed'
            style['slot'] = deepcopy(by_name['CommanderSortMenu']['slot'])
            data['nodes'].append(style)
    found = {node['name'] for node in data['nodes'] if node['name'] in names}
    assert found == set(names), f'{source}: missing {set(names) - found}'
    (root / (target + '.json')).write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding='utf-8')
    print(target, len(data['nodes']))
