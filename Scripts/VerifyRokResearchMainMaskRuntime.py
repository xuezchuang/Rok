"""Verify actual GameApp ResearchMain mask routes and rendered Type1 Close."""
import argparse
import hashlib
import json
import math
import re
import traceback
from pathlib import Path
from rok_data_paths import resolve

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / 'Saved/RokUnityMigration/ResearchMainMaskAudit'
NATIVE = ROOT / 'Saved/UnityResearchMainMaskProbe/Artifacts/native_20261008_151902_577/render_evidence.json'
VERIFIED = ROOT / 'Saved/RokUnityMigration/ResearchMainMaskNative/verified_capture.json'
NATIVE_SHA = '71503eb9a3e581907c6e802fd8552a07cf809646c75edc6bb1ba6ce40a330b01'
VERIFIED_SHA = '47a1c386d9f73860830b64a528051b8649da04432f3695cfb52e70ea8ada44da'
NAMES = ['01_main_open', '02_main_mask_close_early',
         '03_city_after_mask_close', '04_main_reopened',
         '05_main_mask_duplicate_mid', '06_duplicate_close_terminal',
         '07_open_preempted_close', '08_open_preempted_terminal',
         '09_detail_open', '10_detail_mask_only', '11_main_survived_detail',
         '12_x_force_closed', '13_x_force_interrupts_mask_close']
BUTTONS = ['ResearchMainBodyHitLeft', 'ResearchMainHeaderHitLeft',
           'FeatureScreenInputBlocker', 'FeatureScreenInputBlocker',
           'FeatureScreenInputBlocker', 'FeatureScreenInputBlocker',
           'ResearchNodeHit_102', 'ResearchDetailMaskHit', 'ResearchCloseButton',
           'FeatureScreenInputBlocker', 'ResearchCloseButton']

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def near(actual, expected, tolerance, label):
    assert math.isfinite(actual) and abs(actual-expected) <= tolerance, (
        label, actual, expected, tolerance)

def hermite(curve, elapsed):
    keys = curve['keyframes']
    if elapsed <= keys[0]['time']:
        return keys[0]['value']
    if elapsed >= keys[-1]['time']:
        return keys[-1]['value']
    left, right = next((a, b) for a, b in zip(keys, keys[1:])
                       if a['time'] <= elapsed <= b['time'])
    width = right['time'] - left['time']
    t = (elapsed-left['time'])/width
    return ((2*t**3-3*t**2+1)*left['value']
            +(t**3-2*t**2+t)*width*left['outSlope']
            +(-2*t**3+3*t**2)*right['value']
            +(t**3-t**2)*width*right['inSlope'])

def source_curves():
    path = ROOT / 'Saved/RokUnityMigration/ResearchAnimationNative/animation_data.json'
    data = json.loads(path.read_text(encoding='utf-8-sig'))
    assert data['schema'] == 'rok.research.animation_data.v2'
    clip = next(x for x in data['clips'] if x['phase'] == 'detail_close')
    assert clip['durationSeconds'] == .15
    curves = {x['propertyPath']: x for x in clip['curves']}
    assert set(curves) == {'m_LocalScale.x', 'm_LocalScale.y',
                           'm_LocalScale.z', 'm_Alpha'}
    for row in data['sourceFiles']:
        assert sha(resolve(row['path'])) == row['sha256'], row['path']
    ui = (ROOT / 'ref/client-unity/Assets/Scripts/Hotfix/MVC/UI.cs').read_text(encoding='utf-8-sig')
    manager = (ROOT / 'ref/client-unity/Assets/Skyunion/RunTime/UIManager/UIManager.cs').read_text(encoding='utf-8-sig')
    mediator = (ROOT / 'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchMainMediator.cs').read_text(encoding='utf-8-sig')
    assert re.search(r's_ResearchMain\s*=.*EnumMaskStatus\.kTouchClose', ui)
    assert re.search(r'kTouchClose.*CloseUI', manager, re.DOTALL)
    assert 'IsAniCloseing' in manager and 'CloseAnimatorUI' in manager
    assert re.search(r'CloseUI\s*\(\s*UI\.s_ResearchMain\s*,\s*true\s*,\s*true\s*\)', mediator)
    return curves

def native_source(curves):
    assert sha(NATIVE) == NATIVE_SHA and sha(VERIFIED) == VERIFIED_SHA
    native = json.loads(NATIVE.read_text(encoding='utf-8-sig'))
    verified = json.loads(VERIFIED.read_text(encoding='utf-8-sig'))
    assert native['schema'] == 'rok.research_main_mask.native.v1'
    assert native['complete'] and not native['failure']
    assert native['width'] == 1440 and native['height'] == 810
    assert native['originalUiRoot'] and native['originalMainPrefab']
    assert native['originalAnimatorController'] and native['recreatedSourceMask']
    assert not native['actualUiManagerInvoked'] and native['managerRemovalSimulated']
    assert native['forceCloseOnlySourceContract'] and native['noAccountOrNetworkAction']
    assert verified['schema'] == 'rok.research_main_mask.native_verified.v1'
    assert verified['complete'] and verified['renderEvidenceSha256'] == NATIVE_SHA
    assert verified['sampleCount'] == 11
    assert verified['phaseCounts'] == {'mask_close_normal': 5,
                                        'mask_close_interrupt_open': 5,
                                        'main_removed_after_callback': 1}
    samples = native['samples']
    assert len(samples) == 11
    source_images = []
    for group, phase in enumerate(('mask_close_normal', 'mask_close_interrupt_open')):
        for index, expected_time in enumerate((0,.03,.075,.12,.15)):
            row = samples[group*5+index]
            assert row['phase'] == phase
            near(row['elapsedSeconds'], expected_time, 1e-5, ('native time',phase,index))
            assert row['mainExists'] and row['maskExists'] and row['maskBelowMain']
            assert row['maskHasButton'] and row['maskNullSprite'] and row['maskRaycastTarget']
            assert row['sourceButtonPointerClick'] and not row['actualUiManagerInvoked']
            assert row['closeStartCount'] == 1
            assert row['mask']['activeInHierarchy'] and row['mainWindow']['activeInHierarchy']
            mask_rect = row['mask']['screenRect']
            for key, expected in {'x':0,'y':0,'width':1440,'height':810}.items():
                near(mask_rect[key], expected, .1, ('native mask rect',key))
            color = row['maskImageColor']
            assert color['r'] == color['g'] == color['b'] == 0
            near(color['a'],.8,1e-5,'native mask alpha')
            for axis in ('x','y'):
                near(row['mainWindow']['localScale'][axis],
                     hermite(curves[f'm_LocalScale.{axis}'],expected_time),.015,
                     ('native scale',phase,index,axis))
            near(row['mainWindow']['canvasGroupAlpha'],
                 hermite(curves['m_Alpha'],expected_time),.015,
                 ('native opacity',phase,index))
            if index == 0:
                assert row['maskPointerClicks'] == 1
                assert row['duplicateCloseIgnoredCount'] == 0
                if group == 1:
                    assert row['outgoingOpenScale'] > 1.05
                else:
                    near(row['outgoingOpenScale'],1,.001,'native normal open scale')
            if index in (1,2,3):
                assert row['maskPointerClicks'] == 2
                assert row['duplicateCloseIgnoredCount'] == 1
            if index < 4:
                assert row['isAniClosing'] and row['closeAnimatorEventCount'] == 0
                assert row['managerCallbackCount'] == 0
            else:
                assert row['closeAnimatorEventCount'] == row['managerCallbackCount'] == 1
                assert row['managerRemovalRequested']
            png = NATIVE.parent / row['png']
            assert png.is_file() and sha(png) == row['sha256']
            assert verified['pngSha256'][row['key']] == row['sha256']
            source_images.append(image(png))
    removed = samples[10]
    assert removed['phase'] == 'main_removed_after_callback'
    assert not removed['mainExists'] and not removed['maskExists']
    assert removed['simulatedRemoval'] and not removed['actualUiManagerInvoked']
    assert removed['closeAnimatorEventCount'] == removed['managerCallbackCount'] == 1
    removed_png = NATIVE.parent / removed['png']
    assert sha(removed_png) == removed['sha256']
    assert verified['pngSha256'][removed['key']] == removed['sha256']
    assert diff(source_images[0],source_images[1]) > 1000
    assert diff(source_images[3],image(removed_png)) > 1000
    return native

def check_state(state, curves, kind):
    assert state['state_unchanged']
    assert state['viewport_pixels'] == {'x': 0, 'y': 0, 'width': 1440, 'height': 810}
    mode = state['feature_mode_canvas']
    if kind == 'closed':
        assert not state['screen_open'] and state['mask_phase'] == 'Closed'
        assert not state['mask_closing']
        return
    assert state['screen_open'] and state['research_mode']
    assert state['reference_preview'] and not state['account_research_synced']
    assert not state['research_queue_synced'] and state['selected_type'] == 1
    assert state['feature_input_blocker']['visible']
    for key in ('feature_input_blocker', 'feature_dimmer'):
        rect = state[key]['pixel_rect']
        for field, expected in {'x':0,'y':0,'width':1440,'height':810}.items():
            near(rect[field],expected,2,('full-screen mask geometry',key,field))
    for key in ('main_body_hit_left', 'main_body_hit_right',
                'main_header_hit_left', 'main_header_hit_right'):
        assert state[key]['attached'], key
    color = state['feature_dimmer_color']
    assert color['r'] == color['g'] == color['b'] == 0
    near(color['a'], .8, .001, 'dimmer alpha')
    if kind == 'closing':
        assert state['mask_closing'] and state['mask_phase'] == 'Closing'
        elapsed = state['mask_close_elapsed_seconds']
        assert 0 <= elapsed < .15
        assert mode['visible'] and state['feature_dimmer']['visible']
        for axis in ('x', 'y'):
            near(mode[f'render_scale_{axis}'],
                 hermite(curves[f'm_LocalScale.{axis}'], elapsed), .015, ('scale', axis))
        near(mode['render_opacity'], hermite(curves['m_Alpha'], elapsed), .015, 'opacity')
    else:
        assert not state['mask_closing'] and state['mask_phase'] == 'Open'
        assert mode['visible']

def image(path):
    result = np.asarray(Image.open(path).convert('RGB'), dtype=np.int16)
    assert result.shape[:2] == (810, 1440), (path, result.shape)
    return result

def diff(a, b):
    return int(np.count_nonzero(np.max(np.abs(a-b), axis=2) >= 24))

def verify(run):
    manifest = json.loads((run/'manifest.json').read_text(encoding='utf-8-sig'))
    assert manifest['schema'] == 'rok.research_main_mask.runtime.v1'
    assert manifest['complete'] and manifest['passed'] and not manifest['failures']
    assert manifest['world'] == '/Game/Maps/L_RokUnityGameAppCity'
    assert Path(manifest['project']).resolve() == ROOT.resolve()
    assert manifest['audit_flag'] == '-RokResearchMainMaskAudit'
    assert manifest['entry_kind'] == 'HUD command_line_reference research_reference'
    assert manifest['real_academy_entry_verified'] is False
    assert manifest['account_research_state_synced'] is False
    assert manifest['research_queue_synced'] is False
    assert manifest['expected_capture_count'] == len(NAMES)
    for relative, record in manifest['input_hashes'].items():
        path = resolve(relative)
        assert path.is_file() and sha(path) == record['sha256'], relative
        assert path.stat().st_size == record['bytes']
    curves = source_curves()
    native = native_source(curves)
    assert manifest['main_native_evidence'] == NATIVE.relative_to(ROOT).as_posix()
    assert manifest['main_native_verified'] == VERIFIED.relative_to(ROOT).as_posix()
    assert manifest['input_hashes'][manifest['main_native_evidence']]['sha256'] == NATIVE_SHA
    assert manifest['input_hashes'][manifest['main_native_verified']]['sha256'] == VERIFIED_SHA
    baseline = manifest['baseline']
    assert baseline['academy_actors'] == 0 and baseline['source_building_snapshots'] > 0
    assert len(baseline['building_state_sha256']) == baseline['source_building_snapshots']
    captures = manifest['captures']
    assert len(captures) == len(NAMES)
    assert [x['phase_name'] for x in captures] == NAMES
    kinds = ['open', 'closing', 'closed', 'open', 'closing', 'closed',
             'closing', 'closed', 'open', 'open', 'open', 'closed', 'closed']
    images = []
    for index, (capture, kind) in enumerate(zip(captures, kinds)):
        path = Path(capture['file'])
        assert path.is_relative_to(run) and path.is_file() and path.name == NAMES[index]+'.png'
        assert path.stat().st_size == capture['file_bytes'] and sha(path) == capture['png_sha256']
        check_state(capture['request'], curves, kind)
        assert capture['processed']['state_unchanged']
        images.append(image(path))
    for index in (1, 4, 6):
        elapsed = captures[index]['request']['mask_close_elapsed_seconds']
        assert .02 <= elapsed <= (.14 if index == 4 else .085), (index, elapsed)
    assert captures[8]['request']['detail_root']['visible']
    assert captures[8]['request']['detail_mask']['visible']
    assert captures[9]['request']['detail_root']['visible']
    assert captures[9]['request']['detail_closing']
    assert not captures[9]['request']['mask_closing']
    assert not captures[10]['request']['detail_root']['visible']
    actions = manifest['actions']
    assert [x['button'] for x in actions] == BUTTONS
    for action in actions:
        assert action['method'] == 'FSlateApplication.ProcessMouseMoveEventAway+MoveEventTarget+DownEvent+UpEvent'
        assert 'move_away_handled' in action
        assert action['target_in_away_path'] is False
        assert math.hypot(action['absolute_x']-action['away_absolute_x'],
                          action['absolute_y']-action['away_absolute_y']) > 20
        assert action['target_in_hit_path'] and action['target_hovered_after_move']
        assert not action['underlay_in_hit_path'] and action['slate_widget_path_types']
        if action['button'] not in ('ResearchMainBodyHitLeft', 'ResearchMainHeaderHitLeft'):
            assert action['down_handled'] and action['up_handled']
    mask = [x for x in actions if x['button'] == 'FeatureScreenInputBlocker']
    assert len(mask) == 5
    for action in mask:
        assert action['after']['screen_open'] and action['after']['mask_closing']
    duplicate = mask[2]
    assert duplicate['before']['mask_closing']
    assert duplicate['after']['mask_close_elapsed_seconds']+.002 >= duplicate['before']['mask_close_elapsed_seconds']
    interrupted = mask[3]
    assert interrupted['before']['feature_mode_canvas']['render_scale_x'] > 1.05
    near(interrupted['after']['feature_mode_canvas']['render_scale_x'], 1, .005, 'open interruption scale')
    near(interrupted['after']['feature_mode_canvas']['render_opacity'], 1, .005, 'open interruption alpha')
    detail = next(x for x in actions if x['button'] == 'ResearchDetailMaskHit')
    assert detail['before']['detail_mask_hit']['visible']
    assert not detail['before']['mask_closing'] and not detail['after']['mask_closing']
    assert detail['before']['selected_type'] == detail['after']['selected_type'] == 1
    for action in (x for x in actions if x['button'] == 'ResearchCloseButton'):
        assert action['after']['screen_open'] and action['after']['pending_action_id'] == 1
    assert not captures[11]['request']['screen_open']
    assert not captures[12]['request']['screen_open']
    close_actions = [x for x in actions if x['button'] == 'ResearchCloseButton']
    assert captures[11]['request']['world_elapsed_seconds']-close_actions[0]['world_elapsed_seconds'] < .15
    assert captures[12]['request']['world_elapsed_seconds']-close_actions[1]['world_elapsed_seconds'] < .15
    changes = {'open_to_early': diff(images[0],images[1]),
               'early_to_closed': diff(images[1],images[2]),
               'detail_to_mask_close': diff(images[8],images[9]),
               'main_to_force_closed': diff(images[10],images[11])}
    assert all(x > 1000 for x in changes.values()), changes
    return {'schema': 'rok.research_main_mask.runtime_verify.v1', 'passed': True,
            'capture_count': len(captures), 'slate_action_count': len(actions),
            'visual_pixel_changes': changes, 'manifest_sha256': sha(run/'manifest.json'),
            'native_main_samples': len(native['samples']),
            'scope': 'Actual GameApp HUD/Slate and isolated Unity original Main prefab/Animator; Unity UIManager mask and removal are source-rule simulations'}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('run', nargs='?', type=Path)
    args = parser.parse_args()
    run = args.run or max((x for x in RUNS.iterdir() if (x/'manifest.json').exists()),
                          key=lambda x: x.stat().st_mtime)
    run = run.resolve()
    try:
        report = verify(run)
    except Exception as error:
        report = {'schema': 'rok.research_main_mask.runtime_verify.v1',
                  'passed': False, 'error': repr(error), 'traceback': traceback.format_exc()}
    (run/'runtime_verify.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False,indent=2))
    return 0 if report['passed'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
