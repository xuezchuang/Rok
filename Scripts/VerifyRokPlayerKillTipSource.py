"""Extract/verify the tooltip source contract; does not claim runtime acceptance."""
import json
import re
from pathlib import Path
from PrepareRokUiTextures import unity_sprite_rect, unity_sprite_vector

root = Path(__file__).resolve().parents[1]
ui = root / 'ref/client-unity/Assets/BundleAssets/UI'
prefab = ui / 'UIPrefabs/ui_prefabs_player/UIPlayerData/UI_KillTip.prefab'
text = prefab.read_text(encoding='utf-8-sig')
assert 'm_SizeDelta: {x: 332, y: 163.37}' in text
assert 'm_CellSize: {x: 144, y: 24}' in text
assert 'm_Spacing: {x: 0, y: 28}' in text
assert 'm_StartCorner: 1' in text
assert '_arab_Layout: 512' in text
arab = (root / 'ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs').read_text(encoding='utf-8-sig')
assert 'if (isArabUI == Skyunion.LanguageUtils.IsArabic())' in arab
assert 'm_AnchoredPosition: {x: 1, y: -116.6}' in text
assert 'm_SizeDelta: {x: 288, y: 153.29999}' in text
clip = (ui / 'UIAnimation/shared_uianimation/UA_TextTipAnime_On.anim').read_text()
scale = clip.split('  m_ScaleCurves:', 1)[1].split('  m_FloatCurves:', 1)[0]
keys = re.findall(r'time: ([\d.]+)\n\s+value: \{x: ([\d.]+), y: ([\d.]+), z: ([\d.]+)\}', scale)
assert keys == [('0', '0', '0', '1'), ('0.11666667', '1', '1', '1'), ('0.25', '1', '1', '1')]
assert scale.count('inSlope: {x: 0, y: 0, z: 0}') == 3
assert scale.count('outSlope: {x: 0, y: 0, z: 0}') == 3
assert '  m_FloatCurves: []' in clip
meta = (ui / 'UITextureStatic/ui_res_player/ui_player.png.meta').read_text()
sprites = {}
expected = [(1, 11, 23, 24), (1, 37, 24, 24), (461, 999, 24, 24), (53, 115, 24, 24), (1, 63, 24, 24)]
for index in range(5):
    name = 'icon_player_' + str(index + 1)
    sprites[name] = unity_sprite_rect(meta, name)
    assert sprites[name] == expected[index]
common = (ui / 'UITextureStatic/ui_res_common/ui_common.png.meta').read_text()
assert unity_sprite_rect(common, 'pl_com_1002_1') == (774, 276, 72, 72)
assert unity_sprite_vector(common, 'pl_com_1002_1', 'border', ('x', 'y', 'z', 'w')) == [21, 24, 21, 23]
assert unity_sprite_rect(common, 'pl_com_1002_3') == (120, 137, 15, 12)
mediator = (root / 'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/PlayerInfo/PlayerDataMediator.cs').read_text(encoding='utf-8-sig')
assert 'onClick.AddListener(ShowKill)' in mediator
assert 'Instantiate("UI_KillTip"' in mediator and 'SetStyle(HelpTipData.Style.arrowUp).Show()' in mediator
report = {'source': str(prefab.relative_to(root)), 'child_size': [332, 163.37], 'frame_padding_total': [40, 40],
          'grid': {'cell': [144, 24], 'spacing': [0, 28], 'saved_start_corner': 'upper_right', 'chinese_runtime_start_corner': 'upper_left'},
          'entry_scale_keys': keys, 'sprites': sprites, 'failures': [], 'statistics': 'server_unknown',
          'runtime_verified': False, 'visual_verified': False}
output = root / 'Saved/RokUnityMigration/player_kill_tip_source.json'
output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False))
