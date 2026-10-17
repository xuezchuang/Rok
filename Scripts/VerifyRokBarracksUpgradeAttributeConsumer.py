"""Read-only source/data/consumer contract check; UE and visual gates are separate."""
import hashlib
import json
from pathlib import Path
from rok_data_paths import DATA_ROOT, resolve

ROOT = Path(__file__).resolve().parents[1]
data = json.loads((DATA_ROOT / 'barracks_upgrade_attribute.json').read_text(encoding='utf-8'))
assert data['schema'] == 'rok.barracks.upgrade_attribute.v1'
assert (data['building_type'], data['current_level'], data['next_level']) == (9, 1, 2)
assert [(r['label_id'], r['label'], r['current'], r['delta'], r['separator']) for r in data['rows']] == [
    (180329, '步兵单位训练容量', 20, 30, True), (180314, '战力', 5, 5, False)]
assert [r['value'] for r in data['rows']] == ['20 + <color=green>30</color>', '5 + <color=green>5</color>']
assert data['row_height'] == 40 and data['attribute_height'] == 110
assert data['omitted_attack']['current'] == data['omitted_attack']['next'] == 0
assert len(data['sources']) == len({r['path'] for r in data['sources']}) == 11
for row in data['sources']:
    assert hashlib.sha256(resolve(row['path']).read_bytes()).hexdigest() == row['sha256'], row['path']
cpp = (ROOT / 'Source/Rok/RokBarracksUpgradeAttributes.cpp').read_text(encoding='utf-8')
modal = (ROOT / 'Source/Rok/RokUnityModalWidget.cpp').read_text(encoding='utf-8')
assert all(row['path'] in cpp for row in data['sources'])
for token in ['Rows->Num()!=2', 'Seen.Contains(Path)', 'Sources->Num()!=Required.Num()', 'RokTrainArmyMesh::Sha256(Bytes)!=Hash',
              'GlyphData->Load(LoadError)', 'Values.Add(Value)', 'Label->Configure(GlyphData,20,false,3', 'Value->Configure(GlyphData,20,false,5', '22.5+40*I', 'FIntRect(436,165,499,220)',
              'FIntRect(1,611,571,613)', 'if(I==1)Line->SetVisibility(ESlateVisibility::Collapsed)']:
    assert token in cpp, token
assert 'URokBarracksUpgradeAttributes::Supports(Preview.CurrentState.UnityBuildingTypeId, Preview.CurrentState.Level, Preview.TargetLevel)' in modal
assert '"等级{0}"' in modal
assert 'TEXT("UpgradeCurrentLevelRegion")' in modal and 'TEXT("UpgradeNextLevelRegion")' in modal
assert 'Arrow->SetRenderScale(FVector2D(-1, 1))' in modal
assert 'FVector2D(779.3, 193.82)' in modal
assert 'GetUpgradeSubsystem' not in cpp and 'StartUpgrade' not in cpp
print('PASS: bounded barracks 1->2, exact 2 source rows / 11 hashes / green-only increment / 40px row / 110px container. UE/native visual comparison remains separate.')
