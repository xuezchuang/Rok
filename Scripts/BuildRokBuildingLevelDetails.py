"""Building level detail tables, following the Unity BuildingDesc/BuildingUpdate mediators."""
import csv
import json
from pathlib import Path
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text

ROOT = Path(__file__).resolve().parents[1]
CSV = ROOT / 'ref/config-tables/编译客户端配置/CSV'


def read(name):
    return list(csv.DictReader((CSV / name).read_text(encoding='utf-8-sig').splitlines()))[1:]


def build():
    language, _ = read_language_lookup()
    produce = read('BuildingResourcesProduce.csv')
    power = {int(row['ID']): row for row in read('BuildingLevelData.csv')}
    types = {int(row['type']): row for row in read('BuildingTypeConfig.csv')}
    tables = []
    for kind in range(2, 6):
        ids = [180310, 180316 + (kind - 2) * 2, 180317 + (kind - 2) * 2, 180314]
        rows = []
        for source in sorted((r for r in produce if int(r['type']) == kind), key=lambda r: int(r['level'])):
            level = int(source['level'])
            definition = power[kind * 100 + level]
            assert int(definition['type']) == kind and int(definition['level']) == level
            rows.append({'level': level, 'produce_speed': int(source['produceSpeed']),
                         'gather_max': int(source['gatherMax']), 'power': int(definition['power'])})
        assert [row['level'] for row in rows] == list(range(1, 26))
        tables.append({'type': kind, 'name': localized_text(language, int(types[kind]['l_nameId']), 'cn'), 'description': localized_text(language, int(types[kind]['l_descId']), 'cn'), 'headers': [localized_text(language, key, 'cn') for key in ids], 'rows': rows})
    barracks = read('BuildingBarracks.csv')
    attack_template = localized_text(language, 180357, 'cn')
    assert attack_template == '{0}%'
    barracks_rows = []
    for source in sorted(barracks, key=lambda r: int(r['level'])):
        level = int(source['level'])
        definition = power[900 + level]
        assert int(source['ID']) == level
        assert int(definition['type']) == 9 and int(definition['level']) == level
        attack = int(source['infantryAttackMulti'])
        attack_text = attack_template.format(f'{attack / 10:g}') if attack > 0 else ''
        barracks_rows.append({'level': level, 'infantry_train_number': int(source['infantryTrainNumber']),
                              'infantry_attack_multi': attack, 'infantry_attack_text': attack_text,
                              'power': int(definition['power'])})
    assert [row['level'] for row in barracks_rows] == list(range(1, 26))
    assert [row['infantry_train_number'] for row in barracks_rows[:2]] == [20, 50]
    tables.append({'type': 9, 'name': localized_text(language, int(types[9]['l_nameId']), 'cn'),
                   'description': localized_text(language, int(types[9]['l_descId']), 'cn'),
                   'header_ids': [180310, 180329, 180352, 180314],
                   'source_table': 'ref/config-tables/编译客户端配置/CSV/BuildingBarracks.csv',
                   'power_table': 'ref/config-tables/编译客户端配置/CSV/BuildingLevelData.csv',
                   'headers': [localized_text(language, key, 'cn') for key in (180310, 180329, 180352, 180314)],
                   'rows': barracks_rows})
    assert all(t['name'] and t['description'] and all(t['headers']) for t in tables)
    result = {'source': 'BuildingDescMediator.ResBuildLevelInfo/GetPower; UI_Win_BuildingUpdateMediator.ReadBuildLevelInfo/GetPower', 'power_semantics': 'row.power; not cumulative',
              'barracks_attack_semantics': 'zero blank; positive raw permille / 1000 * 100 formatted with language 180357',
              'tables': tables, 'runtime_verified': False, 'visual_verified': False}
    path = ROOT / 'Saved/RokUnityMigration/building_level_details.json'
    path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'tables': 5, 'rows': 125, 'output': str(path)}))


if __name__ == '__main__':
    build()
