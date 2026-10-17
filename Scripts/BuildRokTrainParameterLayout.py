"""Extract the training parameter page from native Unity RectTransforms (Chinese Awake layout)."""
import json
import re
from pathlib import Path
from rok_data_paths import resolve as data_resolve

ROOT = Path(__file__).resolve().parents[1]
UI = 'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/'

def documents(text):
    return dict(re.findall(r'--- !u!\d+ &(-?\d+)(?: stripped)?\n(.*?)(?=\n--- !u!|\Z)', text, re.S))

def vector(text, key):
    return [float(v) for v in re.search(key + r': \{x: ([\d.eE+-]+), y: ([\d.eE+-]+)\}', text).groups()]

def main():
    path = UI + 'ui_prefabs_troops/UIArmyTrain/UI_Win_TrainArmy.prefab'
    docs = documents(data_resolve(path).read_text(encoding='utf-8-sig'))
    objects = {key: value for key, value in docs.items() if value.startswith('GameObject:')}
    rects = {key: value for key, value in docs.items() if value.startswith('RectTransform:') and 'm_GameObject:' in value}
    by_name = {}
    for key, rect in rects.items():
        game = re.search(r'm_GameObject: \{fileID: (-?\d+)\}', rect).group(1)
        name = re.search(r'\n  m_Name: (.*)', objects[game]).group(1)
        by_name[name] = key

    cache = {}
    def resolve(key):
        if key in cache:
            return cache[key]
        rect = rects[key]
        parent = re.search(r'm_Father: \{fileID: (-?\d+)\}', rect).group(1)
        if parent == '0':
            cache[key] = ([0., 0.], [1440., 810.])
            return cache[key]
        origin, parent_size = resolve(parent)
        low, high = vector(rect, 'm_AnchorMin'), vector(rect, 'm_AnchorMax')
        pivot, position = vector(rect, 'm_Pivot'), vector(rect, 'm_AnchoredPosition')
        delta = vector(rect, 'm_SizeDelta')
        game = re.search(r'm_GameObject: \{fileID: (-?\d+)\}', rect).group(1)
        components = re.findall(r'component: \{fileID: (-?\d+)\}', objects[game])
        flags = [int(match.group(1)) for component in components
                 if (match := re.search(r'_arab_Layout: (\d+)', docs[component]))]
        if any((flag & 2) and not (flag & 1) for flag in flags):
            low[0], high[0] = 1 - high[0], 1 - low[0]
            pivot[0], position[0] = 1 - pivot[0], -position[0]
        size = [parent_size[i] * (high[i] - low[i]) + delta[i] for i in range(2)]
        bottom = [origin[i] + parent_size[i] * (low[i] + (high[i] - low[i]) * pivot[i])
                  + position[i] - pivot[i] * size[i] for i in range(2)]
        cache[key] = (bottom, size)
        return cache[key]

    names = ['imgContentBg', 'pl_left', 'pl_right', 'pl_train', 'plArmyList', 'pl_desc',
             'lbl_desc_name', 'lbl_desc_content', 'img_army_type_icon', 'pl_res_cost',
             'imgCostBg', 'imgTestbg', 'imgBg', 'bg', 'pl_detailUp', 'btn_detail']
    result = {}
    for name in names:
        origin, size = resolve(by_name[name])
        result[name] = {'position': [origin[0], 810 - origin[1] - size[1]], 'size': size,
                        'unity_rect_id': by_name[name]}
    grid = docs['6103206474764137101']
    assert 'm_StartCorner: 0' in grid and vector(grid, 'm_CellSize') == [118, 178]
    head_instances = []
    for block in docs.values():
        if not block.startswith('PrefabInstance:'):
            continue
        match = re.search(r'propertyPath: m_Name\n      value: UI_Model_ArmyTrainHead_(\d)\n', block)
        if match:
            order = int(re.search(r'propertyPath: m_RootOrder\n      value: (\d+)', block).group(1))
            head_instances.append({'tier': int(match.group(1)), 'sibling': order})
    assert sorted(head_instances, key=lambda row: row['tier']) == [{'tier': i, 'sibling': 5-i} for i in range(1, 6)]
    result['head_grid'] = {'cell': [118, 178], 'chinese_corner': 1, 'instances': head_instances,
                           'visible_tier_order': [1, 2, 3, 4, 5]}
    result['source'] = path
    result['authority_boundary'] = 'No synchronized roster, bonus, training queue or research is inferred from layout.'
    result['visual_verified'] = False
    output = ROOT / 'Saved/RokUnityMigration/train_parameter_layout.json'
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'output': str(output), 'right': result['pl_right'], 'heads': result['head_grid']}))

if __name__ == '__main__':
    main()
