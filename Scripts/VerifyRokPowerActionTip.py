"""Source verification for profile action-point HelpTip; runtime gates remain separate."""
import json
import re
from pathlib import Path
from rok_data_paths import resolve
from PIL import Image
from BuildRokUnityUiAnimationManifest import Builder
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text
from PrepareRokUiTextures import unity_sprite_rect

ROOT = Path(__file__).resolve().parents[1]
UI = 'ref/client-unity/Assets/BundleAssets/UI/'


def main():
    builder = Builder()
    mediator = builder.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/PlayerInfo/PlayerDataMediator.cs')
    assert 'view.m_btn_apHelp_GameButton.onClick.AddListener(ShowPowerAction)' in mediator
    show = mediator.split('private void ShowPowerAction()', 1)[1].split('private void UpdateTimeTick()', 1)[0]
    assert 'Instantiate("UI_Pop_PowerTextTip"' in show
    assert 'HelpTipData.Style.arrowUp' in show and 'm_pl_content.transform' in show
    for field in ['vitalityRecoveryTime', 'vitalityRecoveryMulti', 'lastActionForceTime', 'GetServerTime()']:
        assert field in mediator
    prefab = builder.read(UI + 'UIPrefabs/ui_prefabs_player/UIPlayerData/UI_Pop_PowerTextTip.prefab')
    docs = dict(re.findall(r'--- !u!\d+ &(-?\d+)\n(.*?)(?=\n--- !u!|\Z)', prefab, re.S))
    expected = {'lbl_des': (16, 38.050003), 'lbl_tip': (18, 108.9), 'lbl_des2': (16, 61.1)}
    geometry = {}
    for name, (pixels, height) in expected.items():
        obj = next(value for value in docs.values() if '\n  m_Name: ' + name + '\n' in value)
        components = [docs[key] for key in re.findall(r'component: {fileID: (-?\d+)}', obj)]
        rect = components[0]
        text = next(value for value in components if 'm_FontSize:' in value)
        width, actual_height = map(float, re.search(r'm_SizeDelta: {x: ([\d.]+), y: ([\d.]+)}', rect).groups())
        assert width == 380 and abs(height - actual_height) < .0001
        assert int(re.search(r'm_FontSize: (\d+)', text)[1]) == pixels
        assert 'm_Alignment: 4' in text and 'm_Color: {r: 0, g: 0, b: 0, a: 1}' in text
        geometry[name] = {'size': [width, actual_height], 'font_pixels': pixels, 'font_points_96dpi': pixels * .75}
    assert 'm_SizeDelta: {x: 380, y: 240}' in prefab
    help_tip = builder.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/HelpTips/HelpTipMediator.cs')
    assert 'new Vector2(40f,40f)' in help_tip
    assert 'm_helpTipData.childSize + Frame' in help_tip
    atlas_path = UI + 'UITextureStatic/ui_res_common/ui_common.png'
    atlas = builder.read(atlas_path + '.meta')
    assert tuple(unity_sprite_rect(atlas, 'pl_com_1002_1')) == (774, 276, 72, 72)
    image = Image.open(resolve(atlas_path)).convert('RGBA')
    center_alpha = image.getpixel((810, image.height - 1 - 312))[3]
    assert center_alpha == 204  # Preserve source ~80% alpha; do not force opaque.
    clip = builder.clip(UI + 'UIAnimation/shared_uianimation/UA_TextTipAnime_On.anim')
    assert len(clip['tracks']) == 1 and clip['duration_seconds'] == .25
    track = clip['tracks'][0]
    assert track['attribute'] == 'm_LocalScale' and track['path'] == ''
    for axis in ['x', 'y']:
        assert [key['value'][axis] for key in track['keys']] == [0, 1, 1]
    assert [key['value']['z'] for key in track['keys']] == [1, 1, 1]
    assert abs(track['keys'][1]['time'] - .11666667) < .000001
    for key in track['keys']:
        assert key['weighted_mode'] == 0
        assert all(key['in_slope'][axis] == key['out_slope'][axis] == 0 for axis in ['x', 'y', 'z'])
    cpp = (ROOT / 'Source/Rok/RokUnityModalWidget.cpp').read_text(encoding='utf-8')
    section = cpp.split('void URokUnityModalWidget::HandleProfileActionHelpClicked()', 1)[1].split('void URokUnityModalWidget::HandleProfileActionAddClicked()', 1)[0]
    language, _ = read_language_lookup()
    for key in [100707, 100709]:
        assert localized_text(language, key, 'cn') in section
    assert '<color=green>{2}</color>' in localized_text(language, 100708, 'cn')
    assert 'FontPoints(18)' in section and 'FontPoints(16)' in section
    assert 'SetRenderOpacity' not in section and 'SetBrushColor' not in section
    assert 'Timer' not in section and 'PlayerActionTipClickAway' in section
    output = ROOT / 'Saved/RokUnityMigration/power_action_tip_source_verify.json'
    output.write_text(json.dumps({'source_verified': True, 'geometry': geometry, 'wrapper_size': [420, 280],
        'source_background_center_alpha': center_alpha, 'sources': builder.sources, 'clip': clip,
        'unknown_fields': ['recovery interval', 'recovery multiplier', 'remaining recovery time'],
        'remaining_gates': ['Rok.Ui.PlayerProfile.ActionTip', 'real profile button runtime screenshot and text layout']}, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'source_verified': True, 'output': str(output)}))


if __name__ == '__main__':
    main()
