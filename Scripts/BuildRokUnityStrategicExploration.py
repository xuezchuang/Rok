"""Source contract for UI_Item_LodMenu.OnCaveAndVillage (not player progress)."""
from pathlib import Path
import csv
import hashlib
import json
import re
import sys
from PIL import Image
from rok_data_paths import DATA_ROOT, label

root = Path(__file__).resolve().parents[1]
owner = root / 'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_LodMenu_SubView.cs'
rs = root / 'ref/client-unity/Assets/Scripts/Hotfix/MVC/RS.cs'
table = root / 'ref/config-tables/编译客户端配置/CSV/MapFixPoint.csv'
atlas = root / 'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/map_icon.png.meta'
owner_text = owner.read_text(encoding='utf-8-sig')
assert 'Mathf.CeilToInt(id / 64f)' in owner_text
assert '(1L << (id % 64))' in owner_text
assert 'if(type<70000)' in owner_text
assert 'lod.IsExplore = true' in owner_text
rs_text = rs.read_text(encoding='utf-8-sig', errors='replace')
atlas_text = atlas.read_text(encoding='utf-8-sig')

def evidence(path):
    return {'path': label(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

visuals = {}
runtime_visuals = {}
atlas_image = Image.open(atlas.with_suffix('')).convert('RGBA')
crop_dir = DATA_ROOT / 'StrategicExplorationIcons'
if '--verify' not in sys.argv:
    crop_dir.mkdir(parents=True, exist_ok=True)
for kind in ('cave', 'village'):
    names = re.search(rf'{kind}_lod3_icon = new\[\] \{{ "map_icon\[([^]]+)\]", "map_icon\[([^]]+)\]"', rs_text)
    assert names, kind
    sprites = []
    for name in names.groups():
        match = re.search(rf'      name: {name}\n      rect:\n        serializedVersion: 2\n        x: (\d+)\n        y: (\d+)\n        width: (\d+)\n        height: (\d+)', atlas_text)
        assert match, name
        sprites.append({'name': name, 'rect_bottom_left': list(map(int, match.groups()))})
    prefab = root / f'ref/client-unity/Assets/BundleAssets/Scene/Map_building/map_building_lod/{kind}_lod3.prefab'
    curve = prefab.read_text().split('  m_scale_explore_curve:', 1)[1].split('  m_PreInfinity:', 1)[0]
    keys = [list(map(float, row)) for row in re.findall(r'time: (\S+)\s+value: (\S+)\s+inSlope: (\S+)\s+outSlope: (\S+)', curve)]
    assert keys and all(a[0] < b[0] for a,b in zip(keys, keys[1:]))
    visuals[kind] = {'prefab': evidence(prefab), 'sprites_unopened_opened': sprites, 'explore_curve': keys}
    for state, sprite in enumerate(sprites):
        x,y,w,h = sprite['rect_bottom_left']
        crop = atlas_image.crop((x, atlas_image.height-y-h, x+w, atlas_image.height-y))
        path = crop_dir / (sprite['name'] + '.png')
        if '--verify' in sys.argv:
            actual = Image.open(path).convert('RGBA')
            assert actual.size == crop.size and actual.tobytes() == crop.tobytes(), 'Source RGBA crop mismatch'
        else:
            crop.save(path)
        runtime_visuals[str((1 if kind == 'cave' else 3) + state)] = {
            'texture': evidence(path), 'sprite': sprite, 'explore_curve': keys,
            'pixels_per_unit': float(re.search(r'spritePixelsToUnits: ([\d.]+)', atlas_text)[1])}

with table.open(encoding='utf-8-sig', newline='') as stream:
    rows = list(csv.DictReader(stream))[1:]
points = []
cells = [[] for _ in range(576)]
for raw in rows:
    row = {k: int(v) for k,v in raw.items()}
    if row['group'] != 1:
        continue
    ident, x, y = row['ID'], row['posX'], row['posY']
    assert ident > 0 and 0 <= x < 7200 and 0 <= y < 7200
    cell = x // 300 + y // 300 * 24
    cells[cell].append(ident)
    points.append({'id': ident, 'type': row['type'], 'kind': 'cave' if row['type'] < 70000 else 'village',
        'unity_position': [x,y], 'progress_word': (ident+63)//64, 'progress_bit': ident%64})
assert points and len({p['id'] for p in points}) == len(points)
for cy in range(24):
    for cx in range(24):
        indexed = {ident for y in range(max(0,cy-1),min(24,cy+2)) for x in range(max(0,cx-1),min(24,cx+2)) for ident in cells[x+y*24]}
        expected = {p['id'] for p in points if abs(p['unity_position'][0]//300-cx)<=1 and abs(p['unity_position'][1]//300-cy)<=1}
        assert indexed == expected
result = {'schema': 'rok.unity.strategic_exploration.v1', 'sources': [evidence(p) for p in (owner,rs,table,atlas)],
    'visuals': visuals, 'runtime_visuals': runtime_visuals, 'points': points, 'cells': cells, 'cell_size': 300, 'cells_per_axis': 24,
    'progress_source': 'CurrentRoleInfo.villageCaves[index].rule', 'missing_word_opened': False,
    'status': 'runtime_presentation_local_missing_progress_defaults_unopened'}
output = DATA_ROOT / 'unity_strategic_exploration.json'
if '--verify' in sys.argv:
    assert json.loads(output.read_text(encoding='utf-8')) == result, 'Stale source export'
else:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(f'PASS: {len(points)} source exploration points, 4 runtime sprites, 576 neighborhood checks; no player progress fabricated')
