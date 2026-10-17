"""Verify GameApp HUD research reference navigation against Study and Unity UI evidence.

This reference path has no Academy actor or synchronized account research state.
Virtual Slate routes are bounded app events, not hardware mouse evidence.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / 'Saved/RokUnityMigration/ResearchAudit'
CATALOG = ROOT / 'Saved/RokUnityMigration/research_catalog.json'
ATLAS = ROOT / 'Saved/RokUnityMigration/ResearchNative/atlas_manifest.json'
FIXTURE = ROOT / 'Saved/RokUnityMigration/ResearchNative/research_fixture.json'
NATIVE_DIR = ROOT / 'Saved/UnityResearchNativeProbe/Artifacts/native_20261008_124117_758'
NATIVE = NATIVE_DIR / 'render_evidence.json'
NATIVE_VERIFY = ROOT / 'Saved/RokUnityMigration/ResearchNative/verified_capture.json'
NATIVE_SHA256 = '1b3cb8f8b07c921ddc12e3c949ba23c011928253a549f05ef3a4934d43e959fb'
POWER_NATIVE_DIR = ROOT / 'Saved/UnityResearchNativeProbe/Artifacts/native_20261008_131955_853'
POWER_NATIVE = POWER_NATIVE_DIR / 'render_evidence.json'
POWER_NATIVE_SHA256 = 'f05fac57d082384898e52d02fae759e63c05f24beea51122f22e7effec8deb7d'
POWER_NATIVE_VERIFY = ROOT / 'Saved/RokUnityMigration/ResearchNative/verified_power_capture.json'
POWER_FIXTURE = ROOT / 'Saved/RokUnityMigration/ResearchNative/research_power_fixture.json'
GRAY_SOURCE = ROOT / 'Saved/RokUnityMigration/ResearchNative/gray_material_source.json'
GRAY_UE = ROOT / 'Saved/RokUnityMigration/ResearchNative/gray_material_ue.json'
GRAY_UASSET = ROOT / 'Content/RokPrototype/UI/Research/M_RokResearchGrayUI.uasset'
COMMON_SHEET = ROOT / 'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.tpsheet'
COMMON_PNG = ROOT / 'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png'
COMMON_UASSET = ROOT / 'Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.uasset'
COMMON_ASSET_PATH = '/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common'
STEMS = [
    '01_economy_start', '02_economy_detail_level1',
    '03_economy_detail_level2', '04_local_research_boundary',
    '05_economy_return', '06_economy_end',
    '07_economy_end_detail_128', '08_power_levels_128',
    '09_economy_end_after_power_back', '10_economy_end_return',
    '11_military_start', '12_military_detail_level1',
    '13_military_detail_level2', '14_military_return',
    '15_military_end', '16_economy_retained_end',
    '17_city_after_research', '18_reopened_economy_start',
    '19_mail_feature_switch', '20_final_city',
]
PAGES = [
    'tree', 'detail', 'detail', 'detail', 'tree', 'tree', 'detail', 'power',
    'detail', 'tree', 'tree', 'detail', 'detail', 'tree', 'tree', 'tree',
    'closed', 'tree', 'mail', 'closed',
]
STEPS = list(range(1, 40, 2))
TYPES = [1] * 10 + [2] * 5 + [1, None, 1, None, None]
STUDIES = [0, 102, 102, 102, 0, 0, 128, 128, 128, 0,
           0, 202, 202, 0, 0, 0, None, 0, None, None]
LEVELS = [1, 1, 2, 2, 1, 1, 1, 1, 1, 1,
          1, 1, 2, 1, 1, 1, None, 1, None, None]
EXPECTED_ACTIONS = [
    ('ResearchNodeHit_102', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('ResearchDetailLevelNext', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('ResearchDetailBoundaryButton', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('ResearchDetailBack', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('ResearchEconomyScroll', 'UScrollBox.ScrollToEnd'),
    ('ResearchNodeHit_128', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('ResearchDetailMore', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('ResearchPowerBack', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('ResearchDetailBack', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('ResearchMilitaryTab', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('ResearchNodeHit_202', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('ResearchDetailLevelNext', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('ResearchDetailBack', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('ResearchMilitaryScroll', 'UScrollBox.ScrollToEnd'),
    ('ResearchEconomyTab', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('ResearchCloseButton', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('MailFeatureHitButton', 'UButton.OnClicked.Broadcast'),
    ('FeatureFrameClose', 'UButton.OnClicked.Broadcast'),
]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def rect(value):
    return tuple(float(value[key]) for key in ('x', 'y', 'width', 'height'))


def assert_rect(actual, expected, tolerance=2):
    a = rect(actual)
    b = rect(expected) if isinstance(expected, dict) else tuple(expected)
    assert all(abs(x - y) <= tolerance for x, y in zip(a, b)), (a, b)


def overlaps(first, second):
    ax, ay, aw, ah = rect(first)
    bx, by, bw, bh = rect(second)
    return aw > 0 and ah > 0 and bw > 0 and bh > 0 and \
        ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah


def visible(widget):
    return widget['attached'] and widget['visible'] and 'pixel_rect' in widget


def assert_common_brush(image, sprite_name):
    sheet = COMMON_SHEET.read_text(encoding='utf-8-sig')
    assert ':size=2048x1024' in sheet
    matches = [line.split(';') for line in sheet.splitlines()
               if line.startswith(sprite_name + ';')]
    assert len(matches) == 1
    _, x, y, width, height, *_ = matches[0]
    x, y, width, height = map(int, (x, y, width, height))
    assert Image.open(COMMON_PNG).size == (2048, 1024)
    assert image['texture'].startswith(COMMON_ASSET_PATH)
    uv = image['uv']
    actual = (uv['min_x'], uv['min_y'], uv['max_x'], uv['max_y'])
    expected = (x / 2048, (1024 - y - height) / 1024,
                (x + width) / 2048, (1024 - y) / 1024)
    assert all(abs(a - b) <= 1e-5 for a, b in zip(actual, expected)), (
        sprite_name, actual, expected)


def catalog_source():
    catalog = json.loads(CATALOG.read_text(encoding='utf-8-sig'))
    assert catalog['schema'] == 'rok.research_catalog.v1'
    assert catalog['summary'] == {
        'rows': 410, 'study_types': 71,
        'type_rows': {'1': 193, '2': 217},
        'level_one_nodes': {'1': 28, '2': 43},
        'civilization_overrides': 8,
    }
    assert len(catalog['rows']) == 410 and len(catalog['civilization_overrides']) == 8
    for relative, digest in catalog['sources'].items():
        path = (ROOT / relative).resolve()
        assert path.is_relative_to(ROOT.resolve()) and sha(path) == digest, relative
    nodes = {row['study_type']: row for row in catalog['rows'] if row['level'] == 1}
    assert len(nodes) == 71
    assert sum(row['type'] == 1 for row in nodes.values()) == 28
    assert sum(row['type'] == 2 for row in nodes.values()) == 43
    by_level = {(row['study_type'], row['level']): row for row in catalog['rows']}
    assert len(by_level) == 410
    for study, row in nodes.items():
        assert {(study, level) for level in range(1, row['max_level'] + 1)} <= by_level.keys()
        for pre in row['preconditions']:
            if pre['study_type']:
                assert (pre['study_type'], pre['level']) in by_level
    return catalog, nodes, by_level


def assert_node_record(observed, source, atlas_by_reference):
    assert (observed['id'], observed['type'], observed['study_type'],
            observed['level'], observed['max_level'], observed['campus_level'],
            observed['columns'], observed['location'], observed['name'],
            observed['description'], observed['function_text'], observed['icon_source']) == (
                source['id'], source['type'], source['study_type'], 1,
                source['max_level'], source['campus_level'], source['columns'],
                source['location'], source['name'], source['description'],
                source['function_text'], source['icon']['source'])
    assert observed['has_location'] and observed['preconditions'] == source['preconditions']
    assert observed['source_row'] == source['source_row']
    assert rect(observed['icon_rect']) == tuple(source['icon']['rect'])
    assert (observed['icon_atlas_size']['width'], observed['icon_atlas_size']['height']) == \
           tuple(source['icon']['atlas_size'])
    assert (observed['food'], observed['wood'], observed['stone'], observed['gold'],
            observed['seconds'], observed['power']) == (
                source['base_cost']['food'], source['base_cost']['wood'],
                source['base_cost']['stone'], source['base_cost']['gold'],
                source['base_seconds'], source['power'])
    assert observed['root']['attached'] and observed['icon']['attached']
    assert observed['name_widget']['text'] == source['name']
    level_text = observed['level_widget']['text']
    assert level_text == f'--/{source["max_level"]}', (source['study_type'], level_text)
    icon = atlas_by_reference[source['icon']['source']]
    # Locked nodes use a dynamic gray material whose SourceAtlas parameter is
    # the imported atlas; the Slate brush resource is the material instance.
    source_atlas = observed['icon']['texture']
    if any(pre['study_type'] > 0 for pre in source['preconditions']):
        assert source_atlas and not source_atlas.startswith(icon['ueAssetPath']), (
            source['study_type'], source_atlas)
    else:
        assert source_atlas.startswith(icon['ueAssetPath']), (
            source['study_type'], source_atlas)
    uv = observed['icon']['uv']
    for actual, expected in zip((uv['min_x'], uv['min_y'], uv['max_x'], uv['max_y']),
                                (icon['slateUV'][key] for key in ('u0', 'v0', 'u1', 'v1'))):
        assert abs(actual - expected) <= 1e-5


def changed_pixels(first, second, area, minimum=100):
    x, y, width, height = (round(value) for value in rect(area))
    x0, y0 = max(x, 0), max(y, 0)
    x1, y1 = min(x + width, first.shape[1]), min(y + height, first.shape[0])
    assert x1 > x0 and y1 > y0
    changed = int((np.max(np.abs(first[y0:y1, x0:x1] - second[y0:y1, x0:x1]), axis=2) >= 8).sum())
    assert changed >= minimum, (changed, minimum, area)
    return changed


def native_source():
    check = json.loads(NATIVE_VERIFY.read_text(encoding='utf-8-sig'))
    native = json.loads(NATIVE.read_text(encoding='utf-8-sig'))
    assert check['schema'] == 'rok.research.native_verified.v1'
    assert check['evidenceSha256'] == sha(NATIVE) == NATIVE_SHA256
    assert check['atlasManifestSha256'] == sha(ATLAS)
    assert check['fixtureSha256'] == native['fixtureSha256'] == sha(FIXTURE)
    assert native['inputSha256'] == sha(NATIVE_DIR / 'ReferenceInputs.json')
    assert native['complete'] and native['failure'] == ''
    assert native['syntheticUnresearched'] and native['noAccountOrNetworkAction']
    assert native['width'] == 1440 and native['height'] == 810
    samples = {sample['key']: sample for sample in native['samples']}
    assert list(samples) == ['economy_start', 'economy_end', 'military_start',
                             'military_end', 'detail_locked_102']
    for sample in samples.values():
        png = NATIVE_DIR / sample['png']
        assert png.is_file() and sha(png) == sample['sha256']
        expected_count = 28 if sample['category'] == 1 else 43
        assert len(sample['nodes']) == sample['totalNodeCount'] == expected_count
        assert sample['contentWidth'] > sample['viewportWidth'] > 1000
    return samples


def power_native_source():
    check = json.loads(POWER_NATIVE_VERIFY.read_text(encoding='utf-8-sig'))
    native = json.loads(POWER_NATIVE.read_text(encoding='utf-8-sig'))
    assert check['schema'] == 'rok.research.native_verified.v1'
    assert check['evidenceSha256'] == sha(POWER_NATIVE) == POWER_NATIVE_SHA256
    assert check['fixtureSha256'] == native['fixtureSha256'] == sha(POWER_FIXTURE)
    assert check['inputsSha256'] == native['inputSha256']
    assert native['inputSha256'] == sha(POWER_NATIVE_DIR / 'ReferenceInputs.json')
    assert check['atlasManifestSha256'] == sha(ATLAS)
    assert native['complete'] and native['failure'] == ''
    assert native['syntheticUnresearched'] and native['noAccountOrNetworkAction']
    assert (native['width'], native['height']) == (1440, 810)
    samples = {sample['key']: sample for sample in native['samples']}
    assert list(samples) == ['economy_start', 'economy_end', 'military_start',
                             'military_end', 'detail_locked_102',
                             'detail_locked_128', 'detail_power_128',
                             'detail_back_128']
    for sample in samples.values():
        png = POWER_NATIVE_DIR / sample['png']
        assert png.is_file() and sha(png) == sample['sha256']
    assert len(samples['detail_power_128']['powerRows']) == 10
    return samples


def reviewed_boundary_source():
    path = ROOT / 'Source/Rok/RokResearchScreenWidget.cpp'
    body = path.read_text(encoding='utf-8-sig')
    start = body.index('if (ActionId == BoundaryAction)')
    end = body.index('\n    }', start)
    branch = body[start:end]
    assert 'LastBoundaryActionId = ActionId;' in branch
    assert 'DetailCosts->SetText(' in branch
    assert '研究队列、资源与账号等级未同步' in branch
    assert all(token not in branch for token in ('StudyTechnology(', 'Request(', 'Grant', 'SendNotification'))
    return sha(path)


def gray_material_source():
    source = json.loads(GRAY_SOURCE.read_text(encoding='utf-8-sig'))
    ue = json.loads(GRAY_UE.read_text(encoding='utf-8-sig'))
    assert source['schema'] == 'rok.research.gray_material_source.v1'
    assert ue['schema'] == 'rok.research.gray_material_ue.v1'
    assert source['target'] == ue['asset'] == '/Game/RokPrototype/UI/Research/M_RokResearchGrayUI'
    assert ue['sourceProofSha256'] == sha(GRAY_SOURCE)
    assert ue['hlslSha256'] == source['hlslSha256']
    assert ue['uassetSha256'] == sha(GRAY_UASSET)
    assert source['unityColorSpace'] == 'Gamma'
    assert not ue['visualVerified']  # A runtime screenshot gate is still required.
    baseline_inputs = json.loads((NATIVE_DIR / 'ReferenceInputs.json').read_text(encoding='utf-8-sig'))
    power_inputs = json.loads((POWER_NATIVE_DIR / 'ReferenceInputs.json').read_text(encoding='utf-8-sig'))
    for item in source['sources']:
        path = (ROOT / item['path']).resolve()
        assert path.is_relative_to(ROOT.resolve())
        if item['path'] == 'Scripts/RenderRokResearchNative.cs':
            # The five-frame gray proof pins the earlier probe version.
            # The eight-frame supplement pins the later script and its live bytes.
            assert any(entry['path'] == item['path'] and
                       entry['sha256'] == item['sha256']
                       for entry in baseline_inputs['sources'])
            assert any(entry['path'] == item['path'] and
                       entry['sha256'] == sha(path)
                       for entry in power_inputs['sources'])
        else:
            assert sha(path) == item['sha256'], item['path']
    return {'source_sha256': sha(GRAY_SOURCE), 'ue_manifest_sha256': sha(GRAY_UE),
            'uasset_sha256': sha(GRAY_UASSET)}


def native_tree_geometry(state, sample):
    assert_rect(state['root']['pixel_rect'], sample['mainWindow']['screenRect'], 1)
    assert_rect(state['economy_tab']['pixel_rect'], sample['economyTab']['screenRect'], 1)
    assert_rect(state['military_tab']['pixel_rect'], sample['militaryTab']['screenRect'], 1)
    scroll = state['economy_scroll'] if sample['category'] == 1 else state['military_scroll']
    source_viewport = sample['economyViewport'] if sample['category'] == 1 \
        else sample['militaryViewport']
    assert_rect(scroll['pixel_rect'], source_viewport['screenRect'], 1)
    observed = {node['study_type']: node for node in state['nodes'] if node['type'] == sample['category']}
    source = {node['studyType']: node for node in sample['nodes']}
    assert observed.keys() == source.keys()
    native_column_x = {}
    for node in sample['nodes']:
        column = node['column']
        native_column_x[column] = min(native_column_x.get(column, float('inf')),
                                      node['root']['screenRect']['x'])
    assert set(native_column_x) == set(range(1, 16))
    columns = {(column['type'], column['column']): column['widget']
               for column in state['columns']}
    assert len(columns) == 30
    first_x = native_column_x[1]
    for column_index in range(1, 16):
        widget = columns[sample['category'], column_index]
        assert widget['attached'] and 'slot_rect' in widget
        slot = widget['slot_rect']
        assert abs(slot['x'] - (native_column_x[column_index] - first_x)) <= .75, (
            sample['key'], column_index, slot['x'], native_column_x[column_index] - first_x)
        assert abs(slot['y'] - columns[sample['category'], 1]['slot_rect']['y']) <= .1
    # Unreal does not arrange every off-screen child in a horizontal ScrollBox.
    # Its CachedGeometry can be zero or retain an earlier visible position.
    # Compare every node's authored local slot to the native source row and
    # compare rendered pixel geometry only where the native node intersects
    # the actual masked viewport.
    anchor = sample['nodes'][0]
    anchor_slot = observed[anchor['studyType']]['root']['slot_rect']
    source_y_to_slot = anchor['root']['screenRect']['y'] - anchor_slot['y']
    visible_nodes = 0
    for study, node in observed.items():
        native = source[study]
        assert node['id'] == native['studyId']
        assert (node['columns'], node['location'], node['max_level']) == (
            native['column'], native['location'], native['maxLevel'])
        assert node['icon_source'] == native['iconReference']
        slot = node['root']['slot_rect']
        expected_local_x = anchor_slot['x'] + native['root']['screenRect']['x'] - \
            native_column_x[native['column']]
        assert_rect(slot, (expected_local_x,
                           native['root']['screenRect']['y'] - source_y_to_slot,
                           native['root']['screenRect']['width'], native['root']['screenRect']['height']), .5)
        if not overlaps(native['root']['screenRect'], source_viewport['screenRect']):
            continue
        visible_nodes += 1
        for widget, native_key in (('root', 'root'), ('hit', 'button'), ('icon', 'icon')):
            assert_rect(node[widget]['pixel_rect'], native[native_key]['screenRect'], 2)
    assert visible_nodes == sample['visibleNodeCount'], (visible_nodes, sample['visibleNodeCount'])
    lines = {line['source_study_type']: line for line in state['lines']}
    for study, native in source.items():
        active = native['line']['activeInHierarchy'] or native['line2']['activeInHierarchy']
        assert (study in lines) == active, study
        if not active:
            continue
        line = lines[study]
        for sprite_field, widget_field, native_field in (
                ('primary_sprite', 'primary_widget', 'line'),
                ('secondary_sprite', 'secondary_widget', 'line2')):
            source_line = native[native_field]
            expected = (f'ui_research[{source_line["spriteName"]}]'
                        if source_line['activeInHierarchy'] else '')
            assert line[sprite_field] == expected, (study, sprite_field, line[sprite_field], expected)
            if expected and overlaps(source_line['screenRect'], source_viewport['screenRect']):
                assert line[widget_field]['attached']
                assert_rect(line[widget_field]['pixel_rect'], source_line['screenRect'], 2)
    if sample['scrollX'] == 0:
        assert abs(state['economy_scroll_offset'] if sample['category'] == 1
                   else state['military_scroll_offset']) <= 1
    else:
        offset = state['economy_scroll_offset'] if sample['category'] == 1 \
            else state['military_scroll_offset']
        assert abs(offset + sample['scrollX']) <= 2


def assert_power_page(state, native, by_level):
    detail_geometry = native['detailGeometry']
    def source(suffix):
        matches = [item for item in detail_geometry if item['path'].endswith(suffix)]
        assert len(matches) == 1, (suffix, len(matches))
        return matches[0]

    assert visible(state['power_scroll']) and visible(state['power_back'])
    assert not visible(state['detail_more'])
    assert abs(state['power_scroll_offset']) <= 1
    assert abs(state['power_scroll_end']) <= 1
    assert_rect(state['power_scroll']['pixel_rect'],
                source('/pl_power/sv_power')['screenRect'], 2)
    assert_rect(state['power_back']['pixel_rect'],
                source('/title/btn_back')['screenRect'], 2)
    assert_rect(state['power_header_background']['pixel_rect'],
                source('/pl_power/bg')['screenRect'], 2)
    assert_common_brush(state['power_header_background'], 'pl_com_1012_2')
    assert_common_brush(state['power_back_icon'], 'btn_com_1000')
    header_fields = (
        ('power_level_header', 'power_level_header_region', '/pl_power/level'),
        ('power_effect_header', 'power_effect_header_region', '/pl_power/lbl_subDes'),
        ('power_value_header', 'power_value_header_region', '/pl_power/power'),
    )
    for text_key, region_key, path in header_fields:
        source_header = source(path)
        text, region = state[text_key], state[region_key]
        assert text['text'] == source_header['text']
        assert abs(text['font_points'] - source_header['fontSizeUsed'] * .75) <= .1
        assert_rect(region['pixel_rect'], source_header['screenRect'], 2)
        expected_color = tuple(round(source_header['color'][key] * 255)
                               for key in ('r', 'g', 'b', 'a'))
        actual_color = tuple(text['display_srgb'][key] for key in ('r', 'g', 'b', 'a'))
        assert actual_color == expected_color, (path, actual_color, expected_color)

    config = by_level[128, 1]
    assert config['max_level'] == 10
    assert state['power_effect_header']['text'] == config['buff_description']
    rows, source_rows = state['power_rows'], native['powerRows']
    assert len(rows) == len(source_rows) == 10
    for level, (row, source_row) in enumerate(zip(rows, source_rows), 1):
        config = by_level[128, level]
        assert row['level'] == source_row['level'] == level
        assert source_row['studyType'] == 128
        assert source_row['power'] == config['power']
        assert source_row['desData'] == config['description_data']
        assert visible(row['extent']) and visible(row['root'])
        assert_rect(row['root']['pixel_rect'], source_row['root']['screenRect'], 2)
        for text_key, region_key, native_key, expected_text in (
                ('level_text', 'level_region', 'levelLabel', str(level)),
                ('effect_text', 'effect_region', 'effectLabel',
                 config['description_data'] or config['function_text']),
                ('power_text', 'power_region', 'powerLabel', f'+{config["power"]:,}')):
            text, source_text = row[text_key], source_row[native_key]
            assert text['text'] == source_text['text'] == expected_text
            assert abs(text['font_points'] - source_text['fontSizeUsed'] * .75) <= .1
            assert_rect(row[region_key]['pixel_rect'], source_text['screenRect'], 2)
            color = tuple(text['display_srgb'][key] for key in ('r', 'g', 'b', 'a'))
            assert color == (255, 255, 255, 255)
        line = row['line']
        assert_rect(line['pixel_rect'], source_row['line']['screenRect'], 2)
        assert_common_brush(line, 'pl_com_1005')


def verify(run):
    run = run.resolve()
    assert run.parent == RUNS.resolve()
    manifest_path = run / 'manifest.json'
    data = json.loads(manifest_path.read_text(encoding='utf-8-sig'))
    assert data['schema'] == 'rok.research.runtime.v1'
    assert data['complete'] and data['passed'] and data['failures'] == []
    assert data['audit_flag'] == '-RokResearchAudit'
    assert Path(data['project']).resolve() == ROOT.resolve()
    assert data['world'] == '/Game/Maps/L_RokUnityGameAppCity'
    assert data['entry_kind'].startswith('HUD command_line_reference research_reference')
    assert data['native_baseline_evidence'] == NATIVE.relative_to(ROOT).as_posix()
    assert data['native_power_supplement_evidence'] == POWER_NATIVE.relative_to(ROOT).as_posix()
    assert data['native_power_fixture'] == POWER_FIXTURE.relative_to(ROOT).as_posix()
    assert data['native_power_verified'] == POWER_NATIVE_VERIFY.relative_to(ROOT).as_posix()
    assert not data['real_academy_entry_verified']
    assert not data['account_research_state_synced'] and not data['research_queue_synced']
    assert data['capture_includes_slate_ui'] and data['expected_capture_count'] == len(STEMS)
    catalog, source_nodes, by_level = catalog_source()
    assert data['catalog_sha256'] == sha(CATALOG)
    atlas = json.loads(ATLAS.read_text(encoding='utf-8-sig'))
    assert atlas['schema'] == 'rok.research.atlas.v1'
    atlas_by_reference = {entry['reference']: entry for entry in atlas['sprites']}
    assert len(atlas_by_reference) == len(atlas['sprites']) >= 100
    fixture = json.loads(FIXTURE.read_text(encoding='utf-8-sig'))
    assert fixture['schema'] == 'rok.research.native_fixture.v1'
    assert fixture['state']['synthetic'] and not fixture['state']['accountBacked']
    assert fixture['nodeCount'] == 71 and fixture['studyRowCount'] == 410
    native_samples = native_source()
    power_native_samples = power_native_source()
    gray_evidence = gray_material_source()
    capture_verifier_sha256 = None
    for relative, info in data['input_hashes'].items():
        path = (ROOT / relative).resolve()
        if relative == 'Scripts/VerifyRokResearchRuntime.py':
            # This script is an offline verifier, not a runtime render input.
            # Preserve the audit-captured identity and report this revision.
            assert path.is_relative_to(ROOT.resolve()) and path.is_file()
            assert info['bytes'] > 0 and len(info['sha256']) == 64
            capture_verifier_sha256 = info['sha256']
            continue
        assert path.is_relative_to(ROOT.resolve()) and path.stat().st_size == info['bytes']
        assert sha(path) == info['sha256'], relative
    assert capture_verifier_sha256 is not None
    assert len(data['input_hashes']) >= 16
    baseline = data['baseline']
    assert baseline['academy_actors'] == 0 and baseline['source_building_snapshots'] >= 8
    assert len(baseline['profile_sha256']) == len(baseline['resources_sha256']) == 64
    assert len(baseline['building_state_sha256']) == baseline['source_building_snapshots']
    assert all(len(row['sha256']) == 64 for row in baseline['building_state_sha256'])
    assert all(len(value) == 64 for value in baseline['troop_state_sha256'])
    assert [(row['button'], row['method']) for row in data['actions']] == EXPECTED_ACTIONS
    virtual = [row for row in data['actions'] if row['method'].startswith('FSlateApplication.')]
    assert len(virtual) == 14
    for row in virtual:
        assert row['slate_hit_path_valid'] and row['target_in_hit_path']
        assert row['target_hovered_after_move'] and row['down_handled'] and row['up_handled']
        assert len(row['slate_widget_path_types']) >= 3
    assert data['actions'][-2]['scope'].startswith('Programmatic existing HUD')
    boundary = [row for row in data['actions'] if row['button'] == 'ResearchDetailBoundaryButton']
    assert len(boundary) == 1 and boundary[0]['boundary_local_only_source_reviewed']
    assert data['boundary_reviewed_ui_sha256'] == reviewed_boundary_source()
    assert len(data['captures']) == len(STEMS)
    images, artifacts = [], []
    for index, capture in enumerate(data['captures']):
        file = Path(capture['file']).resolve()
        assert file.parent == run and file.stem == STEMS[index]
        assert file.stat().st_size == capture['file_bytes'] and sha(file) == capture['png_sha256']
        image = np.asarray(Image.open(file).convert('RGB'), dtype=np.int16)
        assert image.shape == (810, 1440, 3)
        images.append(image)
        artifacts.append({'name': file.name, 'sha256': capture['png_sha256']})
        for stage in ('request', 'processed'):
            state = capture[stage]
            assert state['step'] == STEPS[index] and state['page'] == PAGES[index]
            assert state['state_unchanged']
            assert_rect(state['viewport_pixels'], (0, 0, 1440, 810), .1)
            assert state['research_attached'] == (PAGES[index] in ('tree', 'detail', 'power'))
            if PAGES[index] not in ('tree', 'detail', 'power'):
                continue
            assert state['reference_preview'] and not state['account_research_synced']
            assert not state['research_queue_synced'] and state['load_error'] == ''
            assert (state['selected_type'], state['selected_study_type'],
                    state['viewed_config_level']) == (TYPES[index], STUDIES[index], LEVELS[index])
            assert state['last_boundary_action_id'] == (30 if 3 <= index <= 15 else -1)
            assert state['power_page_open'] == (PAGES[index] == 'power')
            assert visible(state['root']) and len(state['nodes']) == 71
            observed = {row['study_type']: row for row in state['nodes']}
            assert observed.keys() == source_nodes.keys()
            for study, row in observed.items():
                assert_node_record(row, source_nodes[study], atlas_by_reference)
                assert row['root']['visible'] == (row['type'] == TYPES[index])
            assert visible(state['detail_root']) == (PAGES[index] in ('detail', 'power'))
            assert visible(state['detail_update_root']) == (PAGES[index] == 'detail')
            assert visible(state['power_root']) == (PAGES[index] == 'power')
            assert visible(state['economy_scroll']) == (TYPES[index] == 1)
            assert visible(state['military_scroll']) == (TYPES[index] == 2)
            sample_key = ('economy_end' if index in (5, 6, 7, 8, 9, 15) else
                          'military_end' if index == 14 else
                          'economy_start' if TYPES[index] == 1 else 'military_start')
            native_tree_geometry(state, native_samples[sample_key])
            if PAGES[index] in ('detail', 'power'):
                source = by_level[STUDIES[index], LEVELS[index]]
                detail_native = (power_native_samples[
                    'detail_locked_128' if index == 6 else
                    'detail_power_128' if index == 7 else
                    'detail_back_128'] if index in (6, 7, 8) else
                    native_samples['detail_locked_102'])
                assert_rect(state['detail_root']['pixel_rect'],
                            detail_native['detailWindow']['screenRect'], 2)
                assert state['detail_title']['text'] == source['name']
                assert f'配置等级 {source["level"]}/{source["max_level"]}' in state['detail_level']['text']
                assert state['detail_level']['text'].startswith('账号等级 —')
                assert state['detail_description']['text'] == source['description']
                if index == 3:
                    assert '研究队列、资源与账号等级未同步' in state['detail_costs']['text']
                elif STUDIES[index] != 102:
                    assert '来源配置需求' in state['detail_costs']['text']
                elif index < 3:
                    assert '来源配置需求' in state['detail_costs']['text']
                values = [source['base_cost'][key] for key in ('food', 'wood', 'stone', 'gold')]
                values += [source['base_seconds'], source['campus_level']]
                assert len(state['detail_cost_values']) == 6
                assert all(str(value) in widget['text'] for value, widget in
                           zip(values, state['detail_cost_values']))
                assert state['detail_progress']['text'] == f'--/{source["max_level"]}'
                previous = by_level.get((source['study_type'], source['level'] - 1))
                expected_power_delta = source['power'] - (previous['power'] if previous else 0)
                assert str(expected_power_delta) in state['detail_power_delta']['text']
            if PAGES[index] == 'power':
                assert STUDIES[index] == 128 and source['max_level'] == 10
                assert_power_page(state, power_native_samples['detail_power_128'],
                                  by_level)
            if index in (5, 15):
                assert state['economy_scroll_offset'] > 100
                assert abs(state['economy_scroll_offset'] - state['economy_scroll_end']) <= 1
            if index == 14:
                assert state['military_scroll_offset'] > 100
                assert abs(state['military_scroll_offset'] - state['military_scroll_end']) <= 1
            if index in (0, 17):
                assert abs(state['economy_scroll_offset']) <= 1
                assert abs(state['military_scroll_offset']) <= 1
    visual = {
        'economy_node_detail': changed_pixels(images[0], images[1],
            data['captures'][1]['processed']['root']['pixel_rect']),
        'detail_level_change': changed_pixels(images[1], images[2],
            data['captures'][2]['processed']['detail_root']['pixel_rect'], 20),
        'local_boundary_feedback': changed_pixels(images[2], images[3],
            data['captures'][3]['processed']['detail_root']['pixel_rect'], 20),
        'economy_horizontal_end': changed_pixels(images[4], images[5],
            data['captures'][5]['processed']['economy_scroll']['pixel_rect']),
        'economy_end_detail': changed_pixels(images[5], images[6],
            data['captures'][6]['processed']['detail_root']['pixel_rect']),
        'all_levels_open': changed_pixels(images[6], images[7],
            data['captures'][7]['processed']['power_scroll']['pixel_rect']),
        'all_levels_back': changed_pixels(images[7], images[8],
            data['captures'][8]['processed']['detail_root']['pixel_rect']),
        'military_tab': changed_pixels(images[9], images[10],
            data['captures'][10]['processed']['military_scroll']['pixel_rect']),
        'military_horizontal_end': changed_pixels(images[13], images[14],
            data['captures'][14]['processed']['military_scroll']['pixel_rect']),
    }
    result = {
        'schema': 'rok.research.runtime_verify.v1', 'passed': True,
        'manifest_sha256': sha(manifest_path), 'catalog_sha256': sha(CATALOG),
        'atlas_sha256': sha(ATLAS), 'fixture_sha256': sha(FIXTURE),
        'native_evidence_sha256': sha(NATIVE),
        'native_verified_sha256': sha(NATIVE_VERIFY),
        'power_native_evidence_sha256': sha(POWER_NATIVE),
        'power_native_verified_sha256': sha(POWER_NATIVE_VERIFY),
        'gray_material_source': gray_evidence,
        'common_sprite_source_sha256': sha(COMMON_SHEET),
        'common_atlas_uasset_sha256': sha(COMMON_UASSET),
        'verifier_sha256_at_capture': capture_verifier_sha256,
        'verifier_sha256_at_validation': sha(Path(__file__)),
        'power_page_scope': 'Isolated Unity source-prefab 128 power-page PNG and 10 source rows verified; no account research state',
        'artifacts': artifacts, 'visual_changed_pixels': visual,
        'scope': data['scope'],
    }
    output = run / 'runtime_verify.json'
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('run', type=Path)
    verify(parser.parse_args().run)
