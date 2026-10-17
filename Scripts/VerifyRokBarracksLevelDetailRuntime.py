"""Verify real Init6 barracks level-detail navigation and source-derived content.

The runtime harness broadcasts navigation delegates and captures real Slate frames.
This verifier does not claim a physical pointer hit test or an upgrade command.
"""
import argparse
import csv
import hashlib
import json
from collections import Counter
from pathlib import Path

import numpy as np
from PIL import Image

from BuildRokUnityEconomyCatalog import localized_text, read_language_lookup
from PrepareRokUiTextures import unity_sprite_rect
from rok_data_paths import DATA_ROOT, label, resolve

ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / 'Saved/RokUnityMigration/BarracksLevelDetailAudit'
GAP = ROOT / 'Saved/BarracksParentWindow/next_building_info_button_gap.json'
CATALOG = DATA_ROOT / 'building_level_details.json'
STEMS = [
    '01_selected_city', '02_barracks_upgrade', '03_level_detail_initial',
    '04_level_detail_last_rows', '05_back_to_upgrade',
    '06_level_detail_reopened_retained', '07_closed_city',
    '08_new_upgrade_window', '09_new_window_detail_reset', '10_final_city',
]
PAGES = ['closed', 'upgrade', 'detail', 'detail', 'upgrade', 'detail',
         'closed', 'upgrade', 'detail', 'closed']
ACTIONS = [
    'BuildingUpgradeButton', 'BarracksLevelInfoButton',
    'BarracksLevelBackButton', 'BarracksLevelInfoButton', 'Type2CloseButton',
    'BuildingUpgradeButton', 'BarracksLevelInfoButton', 'Type2CloseButton',
]
HEADER_IDS = [180310, 180329, 180352, 180314]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def rectangle(obj):
    return tuple(float(obj[key]) for key in ('x', 'y', 'width', 'height'))


def assert_rect(obj, expected, tolerance=.25):
    actual = rectangle(obj)
    assert all(abs(a-b) <= tolerance for a, b in zip(actual, expected)), \
        f'{actual} differs from {expected}'


def read_csv(name):
    path = ROOT / 'ref/config-tables/编译客户端配置/CSV' / name
    with path.open(encoding='utf-8-sig', newline='') as stream:
        return [row for row in csv.DictReader(stream) if row.get('level', '').isdigit()]


def source():
    gap = json.loads(GAP.read_text(encoding='utf-8-sig'))
    source_prefixes = (
        'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateView.cs',
        'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateMediator.cs',
        'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Win_BuildingUpdate.prefab',
        'ref/config-tables/编译客户端配置/CSV/BuildingBarracks.csv',
        'ref/config-tables/编译客户端配置/CSV/BuildingLevelData.csv',
    )
    for relative in source_prefixes:
        path = resolve(relative)
        assert path.stat().st_size == gap['input_hashes'][relative]['bytes']
        assert sha(path) == gap['input_hashes'][relative]['sha256'], relative
    native_v5 = ROOT / 'Saved/UnityBarracksParentWindowV5Probe/Artifacts/native_20260925_120725_828'
    native_path = native_v5 / 'render_evidence.json'
    assert sha(native_path) == gap['input_hashes'][label(native_path)]['sha256']
    native = json.loads(native_path.read_text(encoding='utf-8-sig'))
    check = json.loads((native_v5 / 'verify_v5.json').read_text(encoding='utf-8-sig'))
    assert check['passed'] and check['evidence_sha256'] == sha(native_path)
    assert native['complete'] and native['playMode'] and native['projectionInitialized']
    button = gap['native_v5']['button_nodes']['btn_BuildLevelInfo']
    glyph = gap['native_v5']['button_nodes']['img_polygonImage']
    assert button['active'] and button['interactable'] and button['raycastTarget']
    assert glyph['spriteName'] == 'btn_com_1007' and not glyph['raycastTarget']
    assert_rect(button['rect'], (211, 227, 60, 60))
    assert_rect(glyph['rect'], (222, 238, 38, 38))
    assert gap['unity_behavior']['barracks_rows'].startswith('ReadBuildLevelInfo Barracks case')

    native_detail_run = ROOT / 'Saved/UnityBarracksLevelProbe/Artifacts/native_20261008_094049_295'
    native_detail_path = native_detail_run / 'render_evidence.json'
    native_detail = json.loads(native_detail_path.read_text(encoding='utf-8-sig'))
    native_detail_check = json.loads((native_detail_run / 'verify_level_native.json').read_text(encoding='utf-8-sig'))
    assert native_detail_check['schema'] == 'rok.barracks_level_native_verify.v1'
    assert native_detail_check['passed'] and not native_detail_check['failures']
    assert native_detail_check['evidence_sha256'] == sha(native_detail_path)
    assert native_detail['complete'] and native_detail['playMode'] and native_detail['projectionInitialized']
    assert native_detail['levelRowCount'] == 25 and native_detail['currentLevel'] == 1
    assert [sample['phase'] for sample in native_detail['samples']] == ['parent'] * 4 + ['detail'] * 3 + ['back'] * 2
    for sample in native_detail['samples']:
        path = Path(sample['png'])
        if not path.is_absolute():
            path = native_detail_run / path
        assert path.resolve().parent == native_detail_run.resolve()
        assert sha(path) == sample['sha256']
    geometry = native_detail['detailWindowGeometry']
    def unique(suffix):
        matches = [node for node in geometry if node['path'].endswith(suffix)]
        assert len(matches) == 1, suffix
        return matches[0]['screenRect']
    source_scroll = unique('/Rect/pl_sv_level/sv_levelData')
    source_back = unique('/UI_Model_Window_Type1/title/btn_back')
    back_nodes = [node for node in geometry if node['path'].endswith('/UI_Model_Window_Type1/title/btn_back')]
    assert len(back_nodes) == 1
    back_node = back_nodes[0]
    assert back_node['spriteName'] == 'btn_com_1000'
    assert back_node['spriteAssetPath'] == 'Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png'
    atlas_meta_path = ROOT / 'ref/client-unity' / (back_node['spriteAssetPath'] + '.meta')
    back_sprite_rect = unity_sprite_rect(atlas_meta_path.read_text(encoding='utf-8-sig'), 'btn_com_1000')
    assert tuple(back_sprite_rect) == (703, 282, 69, 69)
    assert_rect(source_scroll, (208, 182.85, 1024, 546.8))
    assert_rect(source_back, (179.5, 54.5, 69, 69))
    source_headers = [unique('/Rect/pl_sv_level/pl_tilte/UI_Item_BuildingLevelData_Title_Four(Clone)/lbl_name' + str(i))
                      for i in range(1, 5)]
    for i in range(1, 5):
        suffix = '/Rect/pl_sv_level/pl_tilte/UI_Item_BuildingLevelData_Title_Four(Clone)/lbl_name' + str(i)
        header_nodes = [node for node in geometry if node['path'].endswith(suffix)]
        assert len(header_nodes) == 1
        assert header_nodes[0]['pivot'] == {'x': .5, 'y': .5}
    for header, x in zip(source_headers, (195, 465, 725, 995)):
        assert_rect(header, (x, 132.85, 250, 50))
    source_rows = sorted((node for node in geometry if node['path'].endswith('/UI_Item_BuildingLevelData_Info_Four(Clone)')),
                         key=lambda node: node['siblingIndex'])
    assert [node['siblingIndex'] for node in source_rows] == list(range(25))
    assert_rect(source_rows[0]['screenRect'], (208, 182.85, 1024, 50))
    assert_rect(source_rows[-1]['screenRect'], (208, 1382.85, 1024, 50))
    selected = [node for node in geometry
                if node['path'].endswith('/Rect/pl_sv_level/sv_levelData/v/c/UI_Item_BuildingLevelData_Info_Four(Clone)/img_select')
                and node.get('activeSelf')]
    assert len(selected) == 1
    source_highlight = selected[0]['screenRect']
    assert_rect(source_highlight, (238, 181.35, 964, 53))

    catalog = json.loads(CATALOG.read_text(encoding='utf-8-sig'))
    matches = [table for table in catalog['tables'] if table['type'] == 9]
    assert len(matches) == 1
    barracks = matches[0]
    assert barracks['header_ids'] == HEADER_IDS
    assert barracks['source_table'] == 'ref/config-tables/编译客户端配置/CSV/BuildingBarracks.csv'
    language, _ = read_language_lookup()
    headers = [localized_text(language, key, 'cn') for key in HEADER_IDS]
    assert barracks['headers'] == headers and all(headers)
    template = localized_text(language, 180357, 'cn')
    assert template == '{0}%'
    rows = {int(row['level']): row for row in read_csv('BuildingBarracks.csv')}
    powers = {int(row['ID']): int(row['power']) for row in read_csv('BuildingLevelData.csv')}
    assert sorted(rows) == list(range(1, 26))
    assert [row['level'] for row in barracks['rows']] == list(range(1, 26))
    expected = []
    for level in range(1, 26):
        row = rows[level]
        raw = int(row['infantryAttackMulti'])
        attack = template.format(f'{raw / 10:g}') if raw else ''
        power = powers[900 + level]
        manifest_row = barracks['rows'][level - 1]
        assert manifest_row == {
            'level': level,
            'infantry_train_number': int(row['infantryTrainNumber']),
            'infantry_attack_multi': raw,
            'infantry_attack_text': attack,
            'power': power,
        }
        expected.append([str(level), str(int(row['infantryTrainNumber'])), attack, str(power)])
    header_base = DATA_ROOT / 'BarracksLevelHeaderText'
    package = json.loads((header_base / 'manifest.json').read_text(encoding='utf-8-sig'))
    header_texts = json.loads((header_base / 'texts.json').read_text(encoding='utf-8-sig'))
    header_inputs = json.loads((header_base / 'ReferenceInputs.json').read_text(encoding='utf-8-sig'))
    native_mesh = json.loads((header_base / 'source_text_meshes.json').read_text(encoding='utf-8-sig'))
    glyph_verify = json.loads((ROOT / 'Saved/RokUnityMigration/BarracksLevelHeaderText/glyph_verify.json').read_text(encoding='utf-8-sig'))
    assert package['schema'] == 'rok.barracks.source_text.package.v1'
    assert package['coordinate_transform']['scope'] == 'header_0..3 only'
    assert package['coordinate_transform']['pivot'] == [.5, .5]
    assert package['coordinate_transform']['runtime'] == 'top-left local pixels for SRokBarracksSourceText'
    assert package['coordinate_transform']['equation'] == \
        'runtime.x = native.xy.x + width*0.5; runtime.y = native.xy.y + height*0.5'
    assert header_texts['schema'] == native_mesh['schema'] == 'rok.barracks.source_text.v1'
    assert header_inputs['schema'] == 'rok.barracks.source_text.inputs.v1'
    assert glyph_verify['schema'] == 'rok.barracks.level_header_text.verify.v1' and glyph_verify['passed']
    assert package['texts_sha256'] == sha(header_base / 'texts.json')
    assert package['input_sha256'] == header_texts['inputSha256'] == sha(header_base / 'ReferenceInputs.json')
    assert package['native_mesh_sha256'] == glyph_verify['mesh_sha256'] == sha(header_base / 'source_text_meshes.json')
    assert package['catalog_sha256'] == sha(CATALOG)
    expected_paths = {
        'ref/client-unity/Assets/StreamingAssets/Config/Bin/Language.bin',
        'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Item_BuildingLevelData_Title_Four.prefab',
        'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateMediator.cs',
        'Saved/UnityBarracksLevelProbe/Artifacts/native_20261008_094049_295/render_evidence.json',
        'Saved/RokUnityMigration/building_level_details.json',
        'Scripts/PrepareRokBarracksLevelHeaderText.py',
        'Scripts/RenderRokBarracksLevelHeaderText.cs',
    }
    assert {entry['path'] for entry in header_inputs['sources']} == expected_paths
    assert all(sha(resolve(entry['path'])) == entry['sha256'] for entry in header_inputs['sources'])
    assert len(package['atlases']) == 1
    atlas = package['atlases'][0]
    assert atlas['path'].startswith('glyph_') and atlas['path'].endswith('.png')
    assert sha(header_base / atlas['path']) == atlas['sha256']
    assert len(header_texts['texts']) == len(native_mesh['texts']) == len(glyph_verify['headers']) == 4
    for index, (text, mesh, verified) in enumerate(zip(header_texts['texts'], native_mesh['texts'], glyph_verify['headers'])):
        key = f'header_{index}'
        assert text['key'] == mesh['key'] == verified['key'] == key
        assert text['text'] == mesh['text'] == verified['text'] == headers[index]
        assert (text['fontSize'], text['effectiveFontSize'], text['style'], text['alignment']) == (22, 24, 0, 4)
        assert (text['width'], text['height'], text['lineCount']) == (250, 50, 1)
        assert text['atlas'] == atlas['path'] and text['atlasSha256'] == atlas['sha256']
        assert all(color == {'r': 230, 'g': 217, 'b': 180, 'a': 255} for color in text['colors'])
        assert len(text['xy']) == len(text['uv']) == len(text['colors']) == 4 * len(text['glyphs'])
        assert len(text['triangles']) == 6 * len(text['glyphs'])
        assert_rect({'x': text['x'], 'y': text['y'], 'width': text['width'], 'height': text['height']},
                    rectangle(source_headers[index]))
        assert_rect({'x': verified['rect'][0], 'y': verified['rect'][1],
                     'width': verified['rect'][2], 'height': verified['rect'][3]}, rectangle(source_headers[index]))
        assert all(text[k] == mesh[k] for k in ('sourceNode', 'width', 'height', 'preferredWidth',
                  'preferredHeight', 'pixelsPerUnit', 'x', 'y', 'fontSize', 'effectiveFontSize',
                  'style', 'alignment', 'lineCount', 'uv', 'colors', 'triangles', 'glyphs'))
        assert len(text['xy']) == len(mesh['xy'])
        for packed, raw in zip(text['xy'], mesh['xy']):
            assert abs(packed['x'] - (raw['x'] + text['width'] / 2)) <= .01
            # The native exporter already inverted Unity's Y into Slate-down Y.
            assert abs(packed['y'] - (raw['y'] + text['height'] / 2)) <= .01
    native_header_preview = Path(package['native_directory']) / 'headers_preview.png'
    assert native_header_preview.resolve().is_file()
    return headers, expected, {'gap_sha256': sha(GAP), 'native_v5_sha256': sha(native_path),
                               'native_detail_sha256': sha(native_detail_path),
                               'catalog_sha256': sha(CATALOG),
                               'native_back_atlas_meta_sha256': sha(atlas_meta_path),
                               'header_package_sha256': sha(header_base / 'manifest.json'),
                               'header_native_mesh_sha256': sha(header_base / 'source_text_meshes.json'),
                               'header_glyph_atlas_sha256': atlas['sha256'],
                               'header_native_preview_sha256': sha(native_header_preview)}, {
                                   'scroll': rectangle(source_scroll),
                                   'back': rectangle(source_back),
                                   'headers': [rectangle(header) for header in source_headers],
                                   'highlight': rectangle(source_highlight),
                                   'back_sprite_rect': tuple(back_sprite_rect),
                               }


def visible_table_changed(first, second, area, threshold=500):
    x, y, width, height = (int(v) for v in rectangle(area))
    x0, y0 = max(0, x), max(0, y)
    x1, y1 = min(1440, x + width), min(810, y + height)
    assert x1 > x0 and y1 > y0
    delta = np.max(np.abs(second[y0:y1, x0:x1] - first[y0:y1, x0:x1]), axis=2)
    count = int((delta >= 8).sum())
    assert count > threshold, f'Expected visible Slate change in {area}: {count} pixels'
    return count


def compare_header_glyphs(ue_images, native_rects, enforce=True):
    native_path = ROOT / 'Saved/UnityBarracksLevelHeaderTextProbe/Artifacts/native_20261008_100928_276/headers_preview.png'
    native = np.asarray(Image.open(native_path).convert('RGB'), dtype=np.int16)
    assert native.shape == (810, 1440, 3)
    def gold_mask(pixels):
        return (pixels[:, :, 0] > 140) & (pixels[:, :, 0] - pixels[:, :, 1] > 7) \
            & (pixels[:, :, 1] - pixels[:, :, 2] > 15)
    expected_core_rgb = np.array([230, 217, 180], dtype=np.int16)
    captures = {}
    for capture_name, ue_image in ue_images.items():
        assert ue_image.shape == native.shape
        metrics = []
        for column, rect in enumerate(native_rects['headers']):
            x, y, width, height = rect
            x0, y0 = round(x), round(y)
            x1, y1 = x0 + round(width), y0 + round(height)
            source = native[y0:y1, x0:x1]
            rendered = ue_image[y0:y1, x0:x1]
            source_mask = gold_mask(source)
            rendered_mask = gold_mask(rendered)
            source_pixels = int(source_mask.sum())
            rendered_pixels = int(rendered_mask.sum())
            intersection = int((source_mask & rendered_mask).sum())
            union = int((source_mask | rendered_mask).sum())
            assert source_pixels > 100 and rendered_pixels > 100 and union > 0
            iou = intersection / union
            source_core = source_mask & np.all(source == expected_core_rgb, axis=2)
            core_count = int(source_core.sum())
            assert core_count >= 20, f'Header {column} native opaque core too small: {core_count}'
            rendered_core = rendered[source_core]
            core_delta = np.abs(rendered_core - expected_core_rgb)
            max_channel_delta = int(core_delta.max())
            mismatch_count = int(np.any(core_delta > 1, axis=1).sum())
            mode_rgb, mode_count = Counter(map(tuple, rendered_core.tolist())).most_common(1)[0]
            edge_overlap = source_mask & rendered_mask & ~source_core
            edge_delta = np.abs(rendered[edge_overlap] - source[edge_overlap])
            metric = {
                'column': column,
                'source_pixels': source_pixels,
                'ue_pixels': rendered_pixels,
                'intersection_pixels': intersection,
                'mask_iou': round(iou, 4),
                'source_opaque_core_rgb': expected_core_rgb.tolist(),
                'source_opaque_core_pixels': core_count,
                'ue_rgb_mode_at_source_core': list(mode_rgb),
                'ue_rgb_mode_count': mode_count,
                'opaque_core_max_channel_delta': max_channel_delta,
                'opaque_core_pixels_outside_rgb_1': mismatch_count,
                'edge_overlap_pixels': int(edge_overlap.sum()),
                'edge_rgb_mean_abs_delta': round(float(edge_delta.mean()), 3) if edge_delta.size else None,
                'edge_rgb_p95_abs_delta': round(float(np.percentile(edge_delta, 95)), 3) if edge_delta.size else None,
            }
            metrics.append(metric)
            if enforce:
                assert iou > .5, f'{capture_name} header {column} glyph overlap too low: {iou:.3f}'
                assert mismatch_count == 0, (f'{capture_name} header {column} opaque core RGB differs from Unity '
                                             f'by more than 1: {mismatch_count}/{core_count} pixels, '
                                             f'UE mode {mode_rgb}, max channel delta {max_channel_delta}')
        captures[capture_name] = metrics
    return captures


def verify(run):
    run = run.resolve()
    assert run.parent == RUNS.resolve(), 'Expected generated BarracksLevelDetailAudit run directory'
    manifest = run / 'manifest.json'
    data = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert data['schema'] == 'rok.barracks.level_detail.runtime.v3'
    assert data['header_render_pipeline'] == ('Four source-mesh headers use the standard Slate vertex-color path '
                                              'with source Color32 E6D9B4; no extra NoGamma conversion. Runtime PNG '
                                              'opaque cores must match Unity source RGB per channel within 1.')
    assert data['complete'] and data['passed'] and not data['failures']
    assert data['expected_capture_count'] == len(STEMS)
    assert data['capture_includes_slate_ui'] and data['owns_target_world']
    assert Path(data['project']).resolve() == ROOT.resolve()
    assert data['world'] == '/Game/Maps/L_RokUnityGameAppCity'
    assert data['audit_flag'] == '-RokBarracksLevelDetailAudit'
    assert [item['button'] for item in data['actions']] == ACTIONS
    assert all(item['method'] == 'UButton.OnClicked.Broadcast' for item in data['actions'])
    for relative, info in data['input_hashes'].items():
        path = resolve(relative).resolve()
        assert path.is_relative_to(ROOT.resolve()) and path.is_file()
        assert path.stat().st_size == info['bytes'] and sha(path) == info['sha256'], relative
    assert len(data['input_hashes']) >= 12
    for name in ('manifest.json', 'texts.json', 'ReferenceInputs.json', 'source_text_meshes.json'):
        assert 'Saved/RokUnityMigration/BarracksLevelHeaderText/' + name in data['input_hashes']
    assert any(path.startswith('Saved/RokUnityMigration/BarracksLevelHeaderText/glyph_')
               and path.endswith('.png') for path in data['input_hashes'])

    headers, rows, sources, native_rects = source()
    assert len(data['captures']) == len(STEMS)
    images = []
    artifacts = []
    for index, capture in enumerate(data['captures']):
        file = Path(capture['file']).resolve()
        assert file.parent == run and file.stem == STEMS[index]
        assert file.stat().st_size == capture['file_bytes'] and sha(file) == capture['png_sha256']
        image = Image.open(file).convert('RGB')
        assert image.size == (1440, 810)
        images.append(np.asarray(image, dtype=np.int16))
        artifacts.append({'name': file.name, 'sha256': capture['png_sha256'], 'page': PAGES[index]})
        for stage in ('request', 'processed'):
            state = capture[stage]
            assert state['page'] == PAGES[index] and state['state_unchanged']
            assert_rect(state['viewport_pixels'], (0, 0, 1440, 810))
            if PAGES[index] == 'closed':
                assert 'detail_root' not in state and 'rows' not in state
                continue
            detail = state['detail_root']
            info = state['info_button']
            glyph = state['info_glyph']
            back = state['back_button']
            back_glyph = state['back_glyph']
            assert detail['attached'] and info['attached'] and glyph['attached']
            assert back['attached'] and back_glyph['attached']
            assert_rect(info['slot_rect'], (211, 227, 60, 60), .05)
            if PAGES[index] == 'upgrade':
                assert not state['detail_open'] and not detail['visible']
                assert info['visible'] and not back['visible']
                assert_rect(info['pixel_rect'], (211, 227, 60, 60))
                assert glyph['visible']
                assert_rect(glyph['pixel_rect'], (222, 238, 38, 38))
                assert 'rows' not in state
                continue
            assert state['detail_open'] and detail['visible']
            assert back['visible'] and not info['visible']
            assert state['selected_level'] == 1 and state['page_time'] >= .24
            assert_rect(back['pixel_rect'], native_rects['back'])
            assert back_glyph['visible']
            assert_rect(back_glyph['pixel_rect'], native_rects['back'])
            assert back_glyph['atlas_texture'] == '/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common'
            sx, sy, sw, sh = native_rects['back_sprite_rect']
            expected_uv = (sx / 2048, (1024 - sy - sh) / 1024,
                           (sx + sw) / 2048, (1024 - sy) / 1024)
            uv = back_glyph['uv']
            observed_uv = tuple(uv[key] for key in ('min_x', 'min_y', 'max_x', 'max_y'))
            assert all(abs(a - b) <= 1e-5 for a, b in zip(observed_uv, expected_uv))
            assert state['headers'] and [h['text'] for h in state['headers']] == headers
            for col, (header, source_rect) in enumerate(zip(state['headers'], native_rects['headers'])):
                assert header['source_mesh']
                assert header['source_key'] == f'header_{col}'
                assert header['paint_count'] > 0 and header['paint_key'] == f'header_{col}'
                assert header['source_font_px'] == 22
                assert header['source_effective_font_px'] == 24
                assert header['source_style'] == 0 and header['source_alignment'] == 4
                assert header['source_color_srgb'] == {'r': 230, 'g': 217, 'b': 180, 'a': 255}
                assert header['source_region']['attached']
                assert_rect(header['source_region']['slot_rect'], source_rect, .05)
                assert_rect(header['source_region']['pixel_rect'], source_rect)
            assert [r['level'] for r in state['rows']] == list(range(1, 26))
            for observed, expected in zip(state['rows'], rows):
                cells = observed['cells']
                assert len(cells) == 4
                for column in (0, 1, 3):
                    assert int(cells[column].replace(',', '').replace('，', '').replace(' ', '')) == int(expected[column])
                assert cells[2] == expected[2]
            assert state['scroll']['attached'] and state['scroll']['visible']
            assert_rect(state['scroll']['slot_rect'], native_rects['scroll'], .05)
            assert_rect(state['scroll']['pixel_rect'], native_rects['scroll'])
            assert state['current_row']['attached']
            assert state['first_row_text']['font_points'] == 16.5
            assert state['first_row_text']['display_srgb'] == {'r': 255, 'g': 255, 'b': 255, 'a': 255}
            if index in (2, 8):
                assert abs(state['scroll_offset']) <= 1
                assert abs(state['diagnostic_scroll_offset']) <= 1
                assert_rect(state['current_row']['pixel_rect'], native_rects['highlight'])
            elif index in (3, 5):
                assert state['scroll_offset'] > 100
                assert state['diagnostic_scroll_offset'] > 100
                area = state['scroll']['pixel_rect']
                last = state['last_row']['pixel_rect']
                ax, ay, aw, ah = rectangle(area)
                _, ly, _, lh = rectangle(last)
                assert ly < ay + ah and ly + lh > ay, 'Level25 did not enter visible scroll region'

    for stage in ('request', 'processed'):
        scrolled = data['captures'][3][stage]
        retained = data['captures'][5][stage]
        assert abs(scrolled['scroll_offset'] - retained['scroll_offset']) <= 1
        assert abs(scrolled['diagnostic_scroll_offset'] - retained['diagnostic_scroll_offset']) <= 1

    table = data['captures'][2]['processed']['scroll']['pixel_rect']
    detail_vs_upgrade = visible_table_changed(images[1], images[2], table)
    scroll_delta = visible_table_changed(images[2], images[3], table)
    reset_delta = visible_table_changed(images[5], images[8], table)
    header_glyphs = compare_header_glyphs({STEMS[index]: images[index] for index in (2, 3, 5, 8)}, native_rects)
    result = {
        'passed': True,
        'manifest_sha256': sha(manifest),
        **sources,
        'artifacts': artifacts,
        'visible_table_changed_pixels': {
            'entry': detail_vs_upgrade, 'scroll': scroll_delta, 'new_window_reset': reset_delta,
        },
        'native_header_glyph_pixel_and_color': header_glyphs,
        'scope': 'Real Init6/type9 city HUD upgrade and all-level detail navigation. Same-window back/reopen retains ListView scroll; whole-window reopen resets it. Actual Slate captures and unchanged economy/profile/buildings/queues. Delegates broadcast directly; physical pointer hit testing and upgrade commands not exercised.',
    }
    out = run / 'runtime_verify.json'
    out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(out)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('run', type=Path)
    verify(parser.parse_args().run)
