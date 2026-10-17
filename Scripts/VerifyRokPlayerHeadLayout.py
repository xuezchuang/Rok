"""Source-derived Chinese player head layout, independent of server ownership."""
import json
import re
from pathlib import Path
from BuildRokUnityUiAnimationManifest import Builder
from VerifyRokLeaderboardOverviewLayout import documents, node, vector
from PrepareRokUiTextures import unity_sprite_rect

ROOT = Path(__file__).resolve().parents[1]
UI = 'ref/client-unity/Assets/BundleAssets/UI/'


def main():
    builder = Builder()
    base = UI + 'UIPrefabs/ui_prefabs_player/UIPlayerHeadSet/'
    raw = builder.read(base + 'UI_Win_PlayerHeadPic.prefab')
    window = documents(raw)
    row = documents(builder.read(base + 'UI_LC_PlayerHeadPic.prefab'))
    item = documents(builder.read(base + 'UI_Item_PlayerHeadSelect.prefab'))
    assert vector(node(window, 'rect')[0], 'm_SizeDelta') == (1024, 624)
    assert vector(node(window, 'rect')[0], 'm_AnchoredPosition') == (0, -18)
    left, right = node(window, 'left'), node(window, 'pl_right')
    assert vector(left[0], 'm_AnchoredPosition') == (-145.6, 0)
    assert vector(right[0], 'm_AnchoredPosition') == (348.2, 0)
    assert all(any('_arab_Layout: 2' in c for c in n) for n in [left, right])
    arab = builder.read('ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs')
    assert 'isArabUI == Skyunion.LanguageUtils.IsArabic()' in arab and 'pos.x = -pos.x' in arab
    sv = node(window, 'sv_head')
    assert vector(sv[0], 'm_SizeDelta') == (643.6, 497.3)
    assert vector(sv[0], 'm_AnchoredPosition') == (3.2000122, -6.8)
    assert any('offset: 0' in c and 'spacing: 0' in c for c in sv)
    grid = next(c for c in node(row, 'UI_LC_PlayerHeadPic') if 'm_CellSize:' in c)
    assert vector(grid, 'm_CellSize') == (135, 135) and vector(grid, 'm_Spacing') == (10, 0)
    assert 'm_ConstraintCount: 4' in grid and 'm_StartCorner: 1' in grid
    assert any('_arab_Layout: 512' in c for c in node(row, 'UI_LC_PlayerHeadPic'))
    assert vector(node(item, 'btn_click')[0], 'm_SizeDelta') == (126, 126)
    assert vector(node(item, 'img_selet')[0], 'm_SizeDelta') == (135, 135)
    # This sprite is an opaque glow disk, rendered BEHIND the head to make a rim.
    # Verifying only its rect misses a real opaque-disk regression above the head.
    selection = node(item, 'img_selet')
    assert 'm_RootOrder: 0' in selection[0]
    assert any('m_Material: {fileID: 0}' in c and 'm_Type: 0' in c for c in selection)
    item_root = node(item, 'UI_Item_PlayerHeadSelect')[0]
    children = re.search(r'm_Children:\n(.*?)\n  m_Father:', item_root, re.S)[1]
    assert re.findall(r'fileID: (\d+)', children)[:2] == ['6304286507583712598', '4848898531829830215']
    item_raw = builder.read(base + 'UI_Item_PlayerHeadSelect.prefab')
    assert 'propertyPath: m_RootOrder\n      value: 1' in item_raw
    from PIL import Image
    atlas_path = ROOT / (UI + 'UITextureStatic/ui_res_setting/ui_setting.png')
    with Image.open(atlas_path) as atlas:
        assert atlas.mode == 'RGBA' and atlas.size == (1024, 1024)
        assert atlas.getpixel((472, 271)) == (255, 255, 163, 255)
    for name, pixels, alignment in [('lbl_name', 26, 4), ('lbl_desc', 18, 1), ('lbl_rank', 20, 4)]:
        text = next(c for c in node(window, name) if 'm_FontSize:' in c)
        assert f'm_FontSize: {pixels}' in text and f'm_Alignment: {alignment}' in text
    common = builder.read(UI + 'UITextureStatic/ui_res_common/ui_common.png.meta')
    setting = builder.read(UI + 'UITextureStatic/ui_res_setting/ui_setting.png.meta')
    page = builder.read(UI + 'UITextureStatic/ui_res_common/ui_page.png.meta')
    for meta, name, rect in [(common, 'pl_com_1001', (436, 165, 63, 55)),
                             (common, 'btn_com_1008_1', (205, 370, 145, 103)),
                             (common, 'btn_com_1008_2', (205, 266, 121, 102)),
                             (setting, 'pl_set_1002', (405, 686, 135, 135)),
                             (setting, 'pl_set_1003', (1, 41, 110, 42)),
                             (page, 'paging_601_1', (1, 1057, 60, 60)),
                             (page, 'paging_602', (1, 1119, 60, 60))]:
        assert tuple(unity_sprite_rect(meta, name)) == rect
    side = builder.read(UI + 'UIPrefabs/ui_prefabs_noaddressable/UI_Model_SideBtn.prefab')
    page_prefab = builder.read(UI + 'UIPrefabs/ui_prefabs_noaddressable/UI_Model_PageButton_Side.prefab')
    assert 'value: UI_Model_PageButton_1' in side and 'm_SizeDelta: {x: 145, y: 103}' in page_prefab
    # Nested source IDs are XOR-remapped by the prefab instance; both H/D IDs
    # map exactly to the window override targets, rather than guessing colors.
    assert 3961802389050468819 ^ 3961802388413814477 ^ 2752163931539029148 == 2752163932177171330
    assert 3961802389935590895 ^ 3961802388413814477 ^ 2752163931539029148 == 2752163933079087038
    mediator = builder.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/PlayerInfo/UI_Win_PlayerHeadPicMediator.cs')
    assert 'private int colNum = 4;' in mediator and 'Role_SetRoleHead.request' in mediator
    assert '.Play(' not in mediator
    list_view = builder.read('ref/client-unity/Assets/Scripts/Client/Compoment/UIComponent/ListView.cs')
    assert 'component.anchoredPosition = v;' in list_view and 'new Vector2(0.5f, 1)' in list_view
    report = {'source_verified': True, 'sources': builder.sources,
              'chinese_preview_center_x': 720 - 348.2, 'chinese_list_center_x': 720 + 145.6 + 3.2,
              'viewport': [557, 191.15, 623.6, 477.3], 'cell': [135, 135], 'columns': 4,
              'row_spacing': 0, 'source_child_animation': None,
              'remaining_gates': ['Rok.Ui.PlayerHead.SourceLayout', 'runtime screenshot and scrolling',
                                  '15 frame sprites missing; ownership and apply server unavailable']}
    output = ROOT / 'Saved/RokUnityMigration/player_head_layout_verify.json'
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'source_verified': True, 'output': str(output)}))


if __name__ == '__main__':
    main()
