"""Source-only Scout Camp extraction. No queue or fog state is synthesized."""
import csv
import hashlib
import json
import re
from pathlib import Path
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text
from BuildRokUnityUiAnimationManifest import Builder, unity_blocks, field, referenced_id
from rok_data_paths import DATA_ROOT, label, resolve

ROOT = Path(__file__).resolve().parents[1]
OUT = DATA_ROOT / 'scout_catalog.json'
UI = 'ref/client-unity/Assets/BundleAssets/UI/'
CSV = 'ref/config-tables/编译客户端配置/CSV/'
MEDIATOR = 'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIScout/UI_Win_ScoutMediator.cs'
PROXY = 'ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/ScoutProxy.cs'
PREFAB = UI + 'UIPrefabs/ui_prefabs_troops/UIScout/'


def build(write=True):
    b = Builder()
    def table(name):
        return list(csv.DictReader(b.read(CSV + name + '.csv').splitlines()))[1:]
    levels = [{key: int(value) for key, value in row.items()} for row in table('BuildingScoutcamp')]
    building = next(row for row in table('BuildingTypeConfig') if row['type'] == '28')
    route = next(row for row in table('BuildingMenuData') if row['ID'] == '137')
    b.read(MEDIATOR); b.read(PROXY)
    b.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_ScoutDesc_SubView.cs')
    b.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_Scout_SubView.cs')
    b.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/CMD/EnumConstant.cs')
    b.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/UI.cs')
    b.read('ref/client-unity/Assets/Skyunion/RunTime/UIManager/UIManager.cs')
    layouts = {}
    arabic_flags = {}
    text_styles = {}
    for relative in [PREFAB + name + '.prefab' for name in ['UI_Win_Scout', 'UI_Item_Scout', 'UI_Item_ScoutDesc']] + [UI + 'UIPrefabs/ui_prefabs_common/UICommon/UI_Model_Window_Type1.prefab']:
        blocks = unity_blocks(b.read(relative))
        names = {entry['file_id']: field(entry['text'], 'm_Name') for entry in blocks if entry['class_id'] == 1}
        arabic_flags[relative] = {(names.get(referenced_id(entry['text'], 'm_GameObject')) or entry['file_id']): int(field(entry['text'], '_arab_Layout'))
                                 for entry in blocks if field(entry['text'], '_arab_Layout') is not None}
        layouts[relative] = [{
            'name': names.get(referenced_id(entry['text'], 'm_GameObject'), ''),
            'anchor_min': field(entry['text'], 'm_AnchorMin'), 'anchor_max': field(entry['text'], 'm_AnchorMax'),
            'position': field(entry['text'], 'm_AnchoredPosition'), 'size': field(entry['text'], 'm_SizeDelta'),
            'pivot': field(entry['text'], 'm_Pivot'), 'scale': field(entry['text'], 'm_LocalScale')}
            for entry in blocks if entry['class_id'] == 224 and field(entry['text'], 'm_AnchoredPosition')]
        styles = {}
        for entry in blocks:
            name = names.get(referenced_id(entry['text'], 'm_GameObject'), '')
            if field(entry['text'], 'm_FontSize') is not None:
                styles.setdefault(name, {}).update({key: field(entry['text'], key) for key in
                    ['m_FontSize', 'm_FontStyle', 'm_Color', 'm_Alignment', 'm_LineSpacing',
                     'm_RichText', 'm_HorizontalOverflow', 'm_VerticalOverflow']})
            if field(entry['text'], 'm_EffectDistance') is not None:
                styles.setdefault(name, {})['shadow'] = {key: field(entry['text'], key) for key in
                    ['m_EffectColor', 'm_EffectDistance', 'm_UseGraphicAlpha']}
        text_styles[relative] = styles
    b.index_motion_sources()
    animations = []
    for family in ['Level', 'Content']:
        prefix = UI + 'UIAnimation/shared_uianimation/UA_BuildingDesc_' + family
        b.guid(prefix + '.controller')
        b.read(prefix + '.controller')
        animations.append(b.clip(prefix + 'Show.anim'))
    for name in ['UA_WinAnime_T1_On', 'UA_WinAnime_T1_Off']:
        animations.append(b.clip(UI + 'UIAnimation/shared_uianimation/' + name + '.anim'))
    b.guid(UI + 'UIAnimation/shared_uianimation/UA_WinAnime_T1.controller')
    b.read(UI + 'UIAnimation/shared_uianimation/UA_WinAnime_T1.controller')
    language, _ = read_language_lookup()
    language_path = 'ref/config-tables/s_Language.xlsm'
    b.sources[language_path] = {'path': language_path, 'sha256': hashlib.sha256(resolve(language_path).read_bytes()).hexdigest()}
    b.read('ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs')
    b.read('ref/client-unity/Assets/Scripts/Client/Compoment/UIExtend/LanguageText.cs')
    text = {str(key): localized_text(language, key, 'cn') for key in [180021, 181002, 181144, 181145, 181147, 181151, 181156, 181157, 181158, 181164]}
    sprite_specs = {'back': ('ui_common', '-6581278422971796230'), 'close': ('ui_common', '-3607114351329744118'),
                    'info': ('ui_common', '-4553837397768982576'), 'summary': ('ui_common', '-8854419702911518851'),
                    'line': ('ui_common', '7590114395838110580'),
                    'body': ('ui_frame', '-4541723960758869015'), 'header': ('ui_frame', '-7211365897215382766')}
    sprites = {}
    for role, (atlas, identity) in sprite_specs.items():
        sheets = list(resolve(UI).rglob(atlas + '.tpsheet'))
        assert len(sheets) == 1, (atlas, sheets)
        sheet = sheets[0]
        meta = b.read(label(sheet.with_suffix('.png.meta')))
        ids = dict(re.findall(r'213: (-?\d+)\n\s+second: (\S+)', meta))
        name = ids[identity]
        lines = b.read(label(sheet)).splitlines()
        rect = list(map(int, next(line for line in lines if line.startswith(name + ';')).split(';')[1:5]))
        size = list(map(int, next(line for line in lines if line.startswith(':size='))[6:].split('x')))
        target = '/Game/RokPrototype/UI/' + ('MainHudAtlases/rok_hud_atlas_common' if atlas == 'ui_common' else 'ModalAtlases/rok_modal_atlas_frame')
        sprites[role] = {'name': name, 'file_id': identity, 'source': label(sheet.with_suffix('.png')), 'rect': rect, 'size': size, 'target': target}
        image_path = sprites[role]['source']
        b.sources[image_path] = {'path': image_path, 'sha256': hashlib.sha256(resolve(image_path).read_bytes()).hexdigest()}
    result = {'building_type': 28, 'menu': route, 'building': building, 'levels': levels, 'text': text,
              'layouts': layouts, 'arabic_flags': arabic_flags, 'text_styles': text_styles,
              'sprites': sprites, 'animations': animations, 'sources': b.sources,
              'queue_state_available': False, 'fog_state_available': False,
              'authority': {'queue_count': 'ScoutProxy.GetScoutNum = mlistSouts.Count from Role_ScoutsInfo.scoutsQueue',
                            'config_capacity': 'BuildingScoutcamp.scoutNumber is capacity, not live queue',
                            'dispatch': 'ScoutProxy.SearPos sends Map_Scouts.request; not wired in this browse slice'},
              'ue_target': 'Source/Rok/RokScoutScreenWidget.cpp', 'visual_verified': False}
    if write:
        OUT.parent.mkdir(parents=True, exist_ok=True)
        OUT.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps({'levels': len(levels), 'sprites': len(sprites), 'output': str(OUT)}, ensure_ascii=False))
    return result


if __name__ == '__main__': build()
