"""Source-derived, read-only training details; no simulated player modifiers."""
import csv
import hashlib
import json
import re
from pathlib import Path
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text, LANGUAGE_WORKBOOK

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'Saved/RokUnityMigration/train_army_detail.json'
UI = ROOT / 'ref/client-unity/Assets/BundleAssets/UI'
PREFAB = UI / 'UIPrefabs/ui_prefabs_troops/UIArmyTrain/UI_Item_TrainSoldierDetail.prefab'


def build(write=True):
    sources = [PREFAB, LANGUAGE_WORKBOOK]
    def table(name):
        path = ROOT / 'ref/config-tables/编译客户端配置/CSV' / (name + '.csv')
        sources.append(path)
        return {int(r['ID']): r for r in list(csv.DictReader(path.read_text(encoding='utf-8-sig').splitlines()))[1:]}
    arms, skills, tips, ages = [table(n) for n in ('Arms', 'ArmsSkill', 'HelpTips', 'CityAgeSize')]
    language, _ = read_language_lookup()
    def loc(value):
        return localized_text(language, int(value or 0), 'cn')
    # TextBlock does not interpret Unity's rich-text tags.
    def plain(value):
        return re.sub(r'</?(?:color|size|b|i)(?:=[^>]*)?>', '', value).replace('\\n', '\n')
    labels = {}
    troop_meta = UI / 'UITextureStatic/ui_res_troops/ui_troops.png.meta'
    sources.append(troop_meta)
    sprite_names = dict(re.findall(r'213: (-?\d+)\s+second: (\S+)', troop_meta.read_text(encoding='utf-8-sig')))
    for block in PREFAB.read_text(encoding='utf-8-sig').split('--- !u!1001'):
        name = re.search(r'value: UI_Model_TrainSoldierAttr_(\d+)', block)
        if name:
            label = re.search(r'propertyPath: m_languageId\s+value: (\d+)', block)
            position = re.search(r'target: \{fileID: 3816625114770455691[^}]+}\s+propertyPath: m_AnchoredPosition.y\s+value: ([\d.-]+)', block)
            sprite = re.search(r'propertyPath: m_Sprite\s+value:\s+objectReference: \{fileID: (-?\d+)', block)
            sprite_name = sprite_names[sprite[1] if sprite else '-2338034985553448765']
            labels[int(name[1])] = (loc(label[1]), float(position[1]), 'ui_troops[' + sprite_name + ']')
    assert len(labels) == 7
    atlases = {}
    def icon(value):
        atlas, sprite = re.fullmatch(r'(.+)\[(.+)\]', value).groups()
        if atlas not in atlases:
            matches = list(UI.rglob(atlas + '.tpsheet'))
            assert len(matches) == 1, value
            sheet = matches[0]
            texture = sheet.with_suffix('.png')
            assert texture.is_file()
            sources.extend([sheet, texture])
            lines = sheet.read_text(encoding='utf-8-sig').splitlines()
            size = list(map(int, next(l[6:] for l in lines if l.startswith(':size=')).split('x')))
            sprites = {p[0]: list(map(int, p[1:5])) for l in lines if l and not l.startswith(('#', ':')) for p in [l.split(';')]}
            atlases[atlas] = {'source': texture.relative_to(ROOT).as_posix(), 'target': '/Game/RokPrototype/UI/TrainArmyDetail/' + atlas, 'size': size, 'sprites': sprites}
        info = atlases[atlas]
        return {'texture': info['target'], 'size': info['size'], 'rect': info['sprites'][sprite], 'source_icon': value}
    entries = []
    for identifier, row in arms.items():
        values = [loc(ages[int(row['age'])]['l_nameId'])] + [row[k] for k in ('attack', 'defense', 'hpMax', 'speed', 'capacity', 'militaryCapability')]
        attrs = [{'label': labels[i + 1][0], 'value': values[i], 'unity_y': labels[i + 1][1], 'tip_id': 1000 + i,
                  'tip': plain(loc(tips[1000 + i]['l_data1'])), 'modifier_unknown': 1 <= i <= 3,
                  **icon(labels[i + 1][2])} for i in range(7)]
        detail_skills = []
        for skill_id in filter(None, row['armsSkill'].split('|')):
            skill = skills[int(skill_id)]
            tip = tips[int(skill['tipsID'])]
            text = loc(tip['l_typeID']).replace('{0}', loc(tip['l_data1'])).replace('{1}', loc(tip['l_data2']))
            detail_skills.append({'id': int(skill_id), 'name': plain(loc(skill['l_skillID'])), 'tip': plain(text), **icon(skill['skillIcon'])})
        assert len(detail_skills) <= 3
        entries.append({'id': identifier, 'army_type': int(row['armsType']), 'tier': int(row['armsLv']),
                        'name': plain(loc(row['l_armsID'])), 'description': plain(loc(row['l_desID'])),
                        'attributes': attrs, 'skills': detail_skills})
    result = {'entries': entries, 'atlases': atlases,
              'sources': {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
              'player_modifiers_available': False, 'visual_verified': False}
    if write:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps({'entries': len(entries), 'atlases': list(atlases), 'output': str(OUTPUT)}, ensure_ascii=False))
    return result


if __name__ == '__main__':
    build()
