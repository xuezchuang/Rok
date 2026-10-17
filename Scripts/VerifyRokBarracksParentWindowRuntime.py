"""Strict real-route Type2 parent geometry audit for Init6 barracks; not full-window parity."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / 'Saved/RokUnityMigration/BarracksParentWindowAudit'
NATIVE_V2 = ROOT / 'Saved/UnityBarracksParentWindowProbe/Artifacts/native_20260925_111125_541'
NATIVE_V4 = ROOT / 'Saved/UnityBarracksParentWindowV4Probe/Artifacts/native_20260925_115024_446'
NATIVE_V5 = ROOT / 'Saved/UnityBarracksParentWindowV5Probe/Artifacts/native_20260925_120725_828'
ATLAS_TEXTURE = '/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common'
NAMES = ['Type2BodyLeft', 'Type2BodyRight', 'Type2HeaderLeft',
         'Type2HeaderRight', 'Type2Title', 'Type2CloseButton']
SOURCE_SUFFIX = ['/win/frame1', '/win/frame2', '/win/frame3',
                 '/win/frame4', '/title/lbl_title']
DEFAULT_RECTS = [(177, 139.2, 543, 660.8), (720, 139.2, 543, 660.8),
                 (160, 108, 560, 60), (720, 108, 560, 60),
                 (307.65, 115, 824.7, 40), (1191.5, 99.5, 69, 69)]
Z_ORDER = [0, 0, 2, 2, 3, 4]
CAPTURE_STEMS = ['01_selected_city', '02_barracks_upgrade', '03_closed',
                 '04_barracks_reopened', '05_barracks_info_default', '06_final_city']
PAGE_KINDS = ['closed', 'barracks_upgrade', 'closed',
              'barracks_upgrade', 'barracks_info', 'closed']
ACTIONS = ['BuildingUpgradeButton', 'Type2CloseButton',
           'BuildingUpgradeButton', 'Type2CloseButton',
           'BuildingInfoButton', 'Type2CloseButton']


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def rect_tuple(value):
    return tuple(float(value[key]) for key in ('x', 'y', 'width', 'height'))


def assert_rect(actual, expected, tolerance=.2):
    assert all(abs(a-b) <= tolerance for a, b in zip(rect_tuple(actual), expected)), \
        f'rect {rect_tuple(actual)} differs from {expected}'


def source_rectangles():
    v2_path = NATIVE_V2 / 'render_evidence.json'
    v4_path = NATIVE_V4 / 'render_evidence.json'
    v2 = json.loads(v2_path.read_text(encoding='utf-8-sig'))
    v4 = json.loads(v4_path.read_text(encoding='utf-8-sig'))
    check_v2 = json.loads((NATIVE_V2 / 'verify.json').read_text(encoding='utf-8-sig'))
    check_v4 = json.loads((NATIVE_V4 / 'verify_v4.json').read_text(encoding='utf-8-sig'))
    assert check_v2['passed'] and check_v2['evidence_sha256'] == sha(v2_path)
    assert check_v4['passed'] and check_v4['evidence_sha256'] == sha(v4_path)
    assert v2['complete'] and v2['projectionInitialized'] and v2['playMode']
    assert v4['complete'] and v4['projectionInitialized'] and v4['playMode']
    assert v2['samples'][-1]['rootScale'] == 1 and v2['samples'][-1]['stateName'] == 'Open'
    assert v4['terminalClose']['stateName'] == 'Open'
    assert v4['terminalClose']['close']['path'].endswith('/UI_Model_Window_Type1/title/btn_close')
    assert v4['terminalClose']['close']['buttonEnabled'] and v4['terminalClose']['close']['interactable']
    assert v4['terminalClose']['close']['graphicEnabled'] and v4['terminalClose']['close']['raycastTarget']
    nodes = v2['samples'][-1]['nodes']
    expected = []
    for suffix in SOURCE_SUFFIX:
        matches = [node for node in nodes if node['path'].endswith(suffix)]
        assert len(matches) == 1 and matches[0]['active'] and matches[0]['visible']
        expected.append(rect_tuple(matches[0]['screenRect']))
    expected.append(rect_tuple(v4['terminalClose']['close']['screenRect']))
    assert v2['fixtureTitleApplied'] == '将步兵营地升至等级2'
    return expected, {'native_v2_sha256': sha(v2_path), 'native_v4_sha256': sha(v4_path),
                      'native_v2_terminal_png_sha256': v2['samples'][-1]['sha256'],
                      'native_v4_close_path': v4['terminalClose']['close']['path']}


def source_background():
    evidence = NATIVE_V5 / 'render_evidence.json'
    native = json.loads(evidence.read_text(encoding='utf-8-sig'))
    verified = json.loads((NATIVE_V5 / 'verify_v5.json').read_text(encoding='utf-8-sig'))
    assert verified['passed'] and verified['evidence_sha256'] == sha(evidence)
    assert native['complete'] and native['projectionInitialized'] and native['playMode']
    assert native['terminalWindowRectTransformCount'] == 129
    assert native['terminalWindowGraphicCount'] == 91
    nodes = [item for item in native['terminalWindowGeometry']
             if item['path'].endswith('/Rect/pl_content/Left/bg')]
    assert len(nodes) == 1
    node = nodes[0]
    assert node['activeInHierarchy'] and node['hierarchyGraphicVisible']
    assert node['graphicType'] == 'UnityEngine.UI.PolygonImage'
    assert node['spriteName'] == 'pl_com_1008'
    assert node['spriteAssetPath'] == 'Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png'
    assert node['localScale']['x'] == node['localScale']['y'] == 1
    return rect_tuple(node['screenRect']), {
        'native_v5_sha256': sha(evidence), 'native_v5_bg_path': node['path'],
        'native_v5_bg_sprite': node['spriteName'],
        'native_v5_bg_texture_sha256': sha(ROOT / 'ref/client-unity' / node['spriteAssetPath']),
        'native_v5_bg_sprite_meta_sha256': sha(ROOT / 'ref/client-unity' / (node['spriteAssetPath'] + '.meta'))}


def verify(run):
    run = run.resolve()
    assert run.parent == RUNS.resolve(), 'Expected a generated BarracksParentWindowAudit run directory'
    manifest = run / 'manifest.json'
    data = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert data['schema'] in ('rok.barracks.parent_window.runtime.v1',
                              'rok.barracks.parent_window.runtime.v2')
    background_contract = data['schema'].endswith('.v2')
    assert data['complete'] and data['passed'] and not data['failures']
    assert data['expected_capture_count'] == 6 and data['capture_includes_slate_ui'] and data['owns_target_world']
    assert Path(data['project']).resolve() == ROOT.resolve()
    assert data['world'] == '/Game/Maps/L_RokUnityGameAppCity'
    assert [item['button'] for item in data['actions']] == ACTIONS
    source, source_info = source_rectangles()
    native_bg, native_bg_info = source_background() if background_contract else (None, {})
    assert len(data['captures']) == 6
    images = []
    artifacts = []
    for index, capture in enumerate(data['captures']):
        file = Path(capture['file']).resolve()
        assert file.parent == run and file.stem == CAPTURE_STEMS[index]
        assert file.stat().st_size == capture['file_bytes'] and sha(file) == capture['png_sha256']
        image = Image.open(file).convert('RGB')
        assert image.size == (1440, 810)
        images.append(np.asarray(image, dtype=np.int16))
        expected = source if PAGE_KINDS[index] == 'barracks_upgrade' else DEFAULT_RECTS
        for stage in ('request', 'processed'):
            state = capture[stage]
            assert state['page_kind'] == PAGE_KINDS[index] and state['state_unchanged']
            assert_rect(state['viewport_pixels'], (0, 0, 1440, 810))
            parents = state['parent_widgets']
            if PAGE_KINDS[index] == 'closed':
                assert not parents
                assert 'preview_background' not in state
                continue
            assert set(parents) == set(NAMES)
            for i, name in enumerate(NAMES):
                widget = parents[name]
                assert_rect(widget['slot_rect'], expected[i], .05)
                assert_rect(widget['pixel_rect'], expected[i])
                assert widget['visible'] and widget['z_order'] == Z_ORDER[i]
                assert widget['mirror_x'] == (i in (1, 3))
            if PAGE_KINDS[index] == 'barracks_upgrade':
                assert parents['Type2Title']['text']
                if background_contract:
                    bg = state['preview_background']
                    assert_rect(bg['slot_rect'], native_bg, .05)
                    assert_rect(bg['pixel_rect'], native_bg)
                    assert bg['visible'] and not bg['mirror_x'] and bg['z_order'] == 0
                    assert bg['atlas_texture'] == ATLAS_TEXTURE
                    assert bg['native_sprite'] == 'pl_com_1008' and bg['nine_slices_source_verified']
                    assert bg['pixel_rect']['y'] + bg['pixel_rect']['height'] < source[0][1] + source[0][3]
            else:
                assert 'preview_background' not in state
        assert capture['processed']['frame'] >= capture['request']['frame']
        artifacts.append({'name': file.name, 'sha256': capture['png_sha256'],
                          'page_kind': PAGE_KINDS[index]})
    # Fixed source-frame strips prove the real Slate screenshots visibly include
    # the parent frame. They are chosen from native V2 geometry, not UE deltas.
    closed = images[0]
    strips = [(195, 130, 245, 700), (1195, 130, 1245, 700), (180, 65, 1260, 115)]
    visible_pixels = []
    for index in (1, 3):
        counts = []
        for x0, y0, x1, y1 in strips:
            change = np.max(np.abs(images[index][y0:y1, x0:x1] - closed[y0:y1, x0:x1]), axis=2) >= 8
            count = int(change.sum())
            assert count > 200, f'Actual parent strip absent in {CAPTURE_STEMS[index]}: {count}'
            counts.append(count)
        visible_pixels.append({'capture': CAPTURE_STEMS[index], 'changed_pixels_in_fixed_source_strips': counts})
    # Reopen must restore identical parent geometry; the unrelated Type2 info
    # page remains on its pre-existing default layout.
    for name in NAMES:
        assert_rect(data['captures'][1]['processed']['parent_widgets'][name]['pixel_rect'],
                    rect_tuple(data['captures'][3]['processed']['parent_widgets'][name]['pixel_rect']))
    result = {'passed': True, 'manifest_sha256': sha(manifest), **source_info, **native_bg_info,
              'preview_background_contract': 'native_v5_verified' if background_contract else 'legacy_parent_only',
              'artifacts': artifacts, 'visible_parent_strips': visible_pixels,
              'scope': 'Real Init6/type9 navigation and Slate screenshot geometry against native V2 frame/V4 close; same-actor BuildingInfo default Type2 regression. No physical pointer hit-test, upgrade command, or full-window pixel parity.'}
    out = run / 'runtime_verify.json'
    out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(out)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('run', type=Path)
    verify(parser.parse_args().run)
