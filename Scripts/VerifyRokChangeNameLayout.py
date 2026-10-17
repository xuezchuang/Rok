"""Check the source input/description layout without mutating UE or server state."""
import json
from pathlib import Path
from BuildRokUnityUiAnimationManifest import Builder
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text
from VerifyRokLeaderboardOverviewLayout import documents, node, vector
from PrepareRokUiTextures import unity_sprite_rect, unity_sprite_vector

ROOT = Path(__file__).resolve().parents[1]
UI = 'ref/client-unity/Assets/BundleAssets/UI/'


def main():
    builder = Builder()
    raw = builder.read(UI + 'UIPrefabs/ui_prefabs_player/UIPlayerData/UI_Win_ChangeName.prefab')
    window = documents(raw)
    field = node(window, 'ipt_name')
    assert vector(field[0], 'm_AnchoredPosition') == (0, 108)
    assert vector(field[0], 'm_SizeDelta') == (782, 54)
    assert (720 - 782 / 2, 405 - 108 - 54 / 2) == (329, 270)
    for name in ['Text', 'Placeholder']:
        parts = node(window, name)
        assert vector(parts[0], 'm_SizeDelta') == (-40, 0)
        text = next(c for c in parts if 'm_FontSize:' in c)
        assert 'm_FontSize: 24' in text and 'm_FontStyle: 0' in text and 'm_Alignment: 5' in text
        assert any('_arab_Layout: 8' in c for c in parts)
    description = node(window, 'lbl_des')
    assert vector(description[0], 'm_AnchoredPosition') == (0, -18.700012)
    assert vector(description[0], 'm_SizeDelta') == (782, 103.600006)
    text = next(c for c in description if 'm_FontSize:' in c)
    assert 'm_FontSize: 20' in text and 'm_Alignment: 2' in text and 'm_VerticalOverflow: 0' in text
    assert any('_arab_Layout: 10' in c for c in description)
    assert not any('m_EffectDistance:' in c for c in description)
    atlas = builder.read(UI + 'UITextureStatic/ui_res_common/ui_common.png.meta')
    assert tuple(unity_sprite_rect(atlas, 'pl_com_1001')) == (436, 165, 63, 55)
    assert unity_sprite_vector(atlas, 'pl_com_1001', 'border', ('x', 'y', 'z', 'w')) == [16, 13, 15, 13]
    assert 'guid: 79ffb0d19ef4cd34193888a1a1bb63e4' in raw
    controller = builder.read(UI + 'UIAnimation/shared_uianimation/UA_WinAnime_T1.controller')
    assert 'guid: 9b669f7004bd4624d8f5f2e27a213619' in controller
    assert 'guid: 7e9d3031356ea6d48b8be9d6fc23c3e3' in controller
    builder.read(UI + 'UIAnimation/shared_uianimation/UA_WinAnime_T1_On.anim')
    builder.read(UI + 'UIAnimation/shared_uianimation/UA_WinAnime_T1_Off.anim')
    mediator = builder.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/PlayerInfo/ChangeNameMediator.cs')
    assert 'view.m_ipt_name_GameInput.onValueChanged.AddListener(OnTextChange)' in mediator
    assert 'm_bagProxy.GetItemNum(itemID) >0' in mediator and 'Role_ModifyName.request' in mediator
    assert '.Play(' not in mediator
    language, _ = read_language_lookup()
    assert localized_text(language, 100506, 'cn') == '点击此处输入城主昵称...'
    assert localized_text(language, 100513, 'cn').format(3, 15) == '您能够输入3至15个字符，可以由数字、字母和特殊符号组成'
    output = ROOT / 'Saved/RokUnityMigration/change_name_layout_verify.json'
    output.write_text(json.dumps({'source_verified': True, 'sources': builder.sources,
        'input': [329, 270, 782, 54], 'input_font_points': 18, 'input_horizontal_padding': 20,
        'description': [329, 371.9, 782, 103.6], 'description_font_points': 15,
        'remaining_gates': ['Rok.Ui.PlayerProfile.ChangeNameSourceLayout', 'runtime input and focus screenshots',
                            'authority inventory, cost selection and Role_ModifyName unavailable']}, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'source_verified': True, 'output': str(output)}))


if __name__ == '__main__':
    main()
