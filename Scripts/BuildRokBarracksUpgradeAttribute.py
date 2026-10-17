"""Extract only the source barracks level 1 -> 2 upgrade attribute presentation."""
import csv, hashlib, json
from pathlib import Path
from PrepareRokHospitalExplanationNativeReference import parse_languages
ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / 'ref/client-unity'
MEDIATOR = CLIENT / 'Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateMediator.cs'
WINDOW = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Win_BuildingUpdate.prefab'
ITEM = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_AttrItem.prefab'
def build():
    tables = ROOT / 'ref/config-tables/编译客户端配置/CSV'
    def rows(name):
        with (tables / (name + '.csv')).open(encoding='utf-8-sig', newline='') as f:
            return {int(r['ID']): r for r in csv.DictReader(f) if r['ID'].isdigit()}
    barracks, levels = rows('BuildingBarracks'), rows('BuildingLevelData')
    binary = CLIENT / 'Assets/StreamingAssets/Config/Bin/Language.bin'
    language = {int(r[0]): r[1] for r in parse_languages(binary)}
    a, b = barracks[1], barracks[2]
    assert [int(a['infantryTrainNumber']), int(b['infantryTrainNumber'])] == [20, 50]
    assert int(a['infantryAttackMulti']) == int(b['infantryAttackMulti']) == 0
    assert [int(levels[i]['power']) for i in (901, 902)] == [5, 10]
    result = []
    for key, label in [('infantryTrainNumber', 180329), ('infantryAttackMulti', 180352)]:
        current, nxt = int(a[key]), int(b[key])
        if current != nxt:
            assert key == 'infantryTrainNumber'  # This bounded slice has no percentage row.
            result.append(dict(label_id=label, label=language[label], current=current, delta=nxt-current))
    result.append(dict(label_id=180314, label=language[180314], current=5, delta=5))
    for i, row in enumerate(result):
        row.update(value=language[180356].format(row['current'], row['delta']), separator=i < len(result)-1)
    atlas = CLIENT/'Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png'
    arab = CLIENT/'Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs'
    level_prefab=CLIENT/'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Item_Buildinglevelup.prefab'
    sources = [level_prefab, atlas, Path(str(atlas)+'.meta'), arab, MEDIATOR, CLIENT/WINDOW, CLIENT/ITEM, binary, tables/'BuildingBarracks.csv', tables/'BuildingLevelData.csv', Path(__file__).resolve()]
    manifest = dict(schema='rok.barracks.upgrade_attribute.v1', building_type=9, current_level=1, next_level=2,
        sprite_metadata={'attribute_background': {'name':'pl_com_1001','rect':[436,165,63,55],'border_lbrt':[16,13,15,13]}, 'separator': {'name':'pl_com_1005','rect':[1,611,570,2],'image_type':0,'target_size':[536,3]}},
        level_presentation={'before':language[180306].format(1),'after':'2','before_language_id':180306,'prefab':level_prefab.relative_to(ROOT).as_posix()},
        rows=result, row_height=40, attribute_height=110, height_rule='source item rect height * row count + 30',
        omitted_attack=dict(current=0, next=0, reason='source only adds changed fields'),
        scope='Presentation-only source 1->2; no building preview, ownership, command, cost or queue mutation.',
        sources=[dict(path=p.relative_to(ROOT).as_posix(), sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sources])
    target=ROOT/'Saved/RokUnityMigration/barracks_upgrade_attribute.json'
    target.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    return manifest
if __name__ == '__main__':
    print(json.dumps(build(), ensure_ascii=True, indent=2))
