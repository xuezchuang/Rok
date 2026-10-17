"""Extract only the source-configured Scout action icon from the original atlas."""
import csv
import hashlib
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'Saved/RokUiProcessed/rok_building_menu_scout.png'
REPORT = ROOT / 'Saved/RokUnityMigration/scout_menu_texture.json'
ATLAS = ROOT / 'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/ui_hud.png'
MENU = ROOT / 'ref/config-tables/编译客户端配置/CSV/BuildingMenuData.csv'


def extract():
    menu = next(row for row in list(csv.DictReader(MENU.read_text(encoding='utf-8-sig').splitlines()))[1:] if row['ID'] == '137')
    assert menu['func'] == 'openUI' and menu['param'] == '1022'
    assert menu['btnIcon'] == 'ui_hud[btn_hud_1041]'
    lines = ATLAS.with_suffix('.tpsheet').read_text(encoding='utf-8-sig').splitlines()
    x, y, width, height = map(int, next(line for line in lines if line.startswith('btn_hud_1041;')).split(';')[1:5])
    assert [x, y, width, height] == [91, 525, 88, 88]
    atlas = Image.open(ATLAS).convert('RGBA')
    # TexturePacker source coordinates are bottom-left; PIL is top-left.
    return atlas.crop((x, atlas.height - y - height, x + width, atlas.height - y)), {
        'building_type': 28, 'menu_id': 137, 'open_ui': 1022, 'source_icon': menu['btnIcon'],
        'source': ATLAS.relative_to(ROOT).as_posix(), 'source_rect': [x, y, width, height],
        'ue_target': '/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_scout',
        'output': OUTPUT.relative_to(ROOT).as_posix(),
        'source_hashes': {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in [ATLAS, ATLAS.with_suffix('.tpsheet'), ATLAS.with_suffix('.png.meta'), MENU]},
        'visual_verified': False}


if __name__ == '__main__':
    image, report = extract()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True); image.save(OUTPUT)
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'output': str(OUTPUT), 'size': image.size, 'source_icon': report['source_icon']}))
