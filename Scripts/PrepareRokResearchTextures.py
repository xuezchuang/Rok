"""Export source research atlases, TexturePacker slice evidence and Academy menu icon."""
import csv
import hashlib
import json
import re
import shutil
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic'
OUT = ROOT / 'Saved/RokUiProcessed/ResearchAtlases'
REPORT = ROOT / 'Saved/RokUnityMigration/ResearchNative/atlas_manifest.json'
RESEARCH = SOURCE / 'ui_res_research'
HUD = SOURCE / 'ui_res_map/ui_hud.png'
MENU = ROOT / 'ref/config-tables/编译客户端配置/CSV/BuildingMenuData.csv'
TABLE = ROOT / 'ref/config-tables/编译客户端配置/CSV/BuildingTypeConfig.csv'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sheet(path):
    result = {}
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        match = re.fullmatch(r'([^;]+);\s*(\d+);\s*(\d+);\s*(\d+);\s*(\d+);.*', line)
        if match:
            name, x, y, w, h = match.groups()
            if name in result:
                raise ValueError('Duplicate source sprite ' + name)
            result[name] = dict(x=int(x), y=int(y), width=int(w), height=int(h))
    return result


def main():
    assert OUT.resolve().is_relative_to((ROOT / 'Saved').resolve())
    assert REPORT.resolve().is_relative_to((ROOT / 'Saved').resolve())
    OUT.mkdir(parents=True, exist_ok=True)
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    atlases, sprites, sources = [], [], {}
    for name in ('ui_research', 'research_icon1', 'research_icon2', 'img_research_bg'):
        image = RESEARCH / (name + '.png')
        meta = RESEARCH / (name + '.png.meta')
        assert image.exists() and meta.exists()
        ue_path = '/Game/RokPrototype/UI/ResearchAtlases/rok_research_' + name
        dest = OUT / ('rok_research_' + name + '.png')
        shutil.copyfile(image, dest)
        assert sha(dest) == sha(image)
        with Image.open(image) as im:
            size = {'width': im.width, 'height': im.height}
        atlases.append({'atlasName': name, 'source': image.relative_to(ROOT).as_posix(),
                        'sourceMeta': meta.relative_to(ROOT).as_posix(),
                        'atlasSize': size, 'output': dest.relative_to(ROOT).as_posix(),
                        'ueAssetPath': ue_path, 'sha256': sha(image)})
        for path in (image, meta):
            sources[path.relative_to(ROOT).as_posix()] = sha(path)
        tpsheet = RESEARCH / (name + '.tpsheet')
        if tpsheet.exists():
            sources[tpsheet.relative_to(ROOT).as_posix()] = sha(tpsheet)
            rects = sheet(tpsheet)
        else:
            rects = {name: dict(x=0, y=0, width=size['width'], height=size['height'])}
        for sprite, rect in rects.items():
            assert rect['x'] >= 0 and rect['y'] >= 0
            assert rect['x'] + rect['width'] <= size['width']
            assert rect['y'] + rect['height'] <= size['height']
            sprites.append({'reference': f'{name}[{sprite}]', 'atlasName': name,
                            'spriteName': sprite, 'rect': rect, 'atlasSize': size,
                            'ueAssetPath': ue_path,
                            'slateUV': {'u0': rect['x']/size['width'],
                                        'v0': 1-(rect['y']+rect['height'])/size['height'],
                                        'u1': (rect['x']+rect['width'])/size['width'],
                                        'v1': 1-rect['y']/size['height']}})
    with MENU.open(encoding='utf-8-sig', newline='') as stream:
        menu = next(row for row in csv.DictReader(stream) if row['ID'] == '110')
    with TABLE.open(encoding='utf-8-sig', newline='') as stream:
        building = next(row for row in csv.DictReader(stream) if row['type'] == '13')
    assert menu['func'] == 'study' and menu['btnIcon'] == 'ui_hud[btn_hud_1018]'
    assert '110' in building['menuButtons'].split('|')
    assert (menu['showLevelMin'], menu['showLevelMax']) == ('1', '25')
    rect = sheet(HUD.with_suffix('.tpsheet'))['btn_hud_1018']
    menu_output = ROOT / 'Saved/RokUiProcessed/rok_building_menu_research.png'
    with Image.open(HUD) as im:
        rgba = im.convert('RGBA')
        x, y, w, h = (rect[key] for key in ('x', 'y', 'width', 'height'))
        crop = rgba.crop((x, rgba.height-y-h, x+w, rgba.height-y))
        crop.save(menu_output)
    for path in (HUD, HUD.with_suffix('.tpsheet'), HUD.with_suffix('.png.meta'), MENU, TABLE):
        sources[path.relative_to(ROOT).as_posix()] = sha(path)
    report = {'schema': 'rok.research.atlas.v1',
              'coordinateSystem': 'rect uses Unity/TexturePacker bottom-left pixels; slateUV uses top-left normalized coordinates',
              'atlases': atlases, 'sprites': sprites,
              'menuIcon': {'buildingType': 13, 'menuId': 110, 'sourceReference': menu['btnIcon'],
                           'sourceAtlas': HUD.relative_to(ROOT).as_posix(), 'rect': rect,
                           'output': menu_output.relative_to(ROOT).as_posix(),
                           'ueAssetPath': '/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_research',
                           'sha256': sha(menu_output)},
              'sources': sources}
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'schema': report['schema'], 'atlases': len(atlases), 'sprites': len(sprites),
                      'menuIcon': report['menuIcon']['ueAssetPath'], 'manifest': str(REPORT)}))


if __name__ == '__main__':
    main()
