"""Verify the opt-in GameApp research animation audit against Unity clips.

The source probe is an isolated reference preview, not an account-backed
Academy. Runtime verification is intentionally fail-closed until both the
native capture and the UE audit manifest are present.
"""
import argparse
import hashlib
import json
import math
import traceback
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / 'Saved/RokUnityMigration/ResearchAnimationNative/animation_data.json'
RUNS = ROOT / 'Saved/RokUnityMigration/ResearchAnimationAudit'
BASELINE_NATIVE = ROOT / 'Saved/UnityResearchAnimationProbe/Artifacts/native_20261008_135653_326/render_evidence.json'
BASELINE_VERIFIED = ROOT / 'Saved/RokUnityMigration/ResearchAnimationNative/verified_animation_capture.json'
BASELINE_NATIVE_SHA256 = '5607042daade23b1adda6c4962f65a0d7b9009dd7eab7de1e32dae67f98c1e48'
BASELINE_VERIFIED_SHA256 = '20b29b9447a09f70ace1737343c7f20ed22f0f653909f2b84eac0c67c212e5e9'
NATIVE = ROOT / 'Saved/UnityResearchAnimationProbe/Artifacts/native_20261008_141523_430/render_evidence.json'
VERIFIED = ROOT / 'Saved/RokUnityMigration/ResearchAnimationNative/verified_animation_close_capture.json'
NATIVE_SHA256 = '373d40bbc9f229a0c34512fa1a0671ea76bf5a403ab08d49a1b3dcca12871de0'
VERIFIED_SHA256 = 'ecb5b5b19968a9fe64dd557141c6fcf76bed4897fb5af8c5ee178bdc6df07a40'
REVIEWED_UI_SHA256 = 'fe2e13b0c6524fde86f4432c68fdbc01de6d81d2eec3c0fc47389dc93a889e49'
REVIEWED_FEATURE_SHA256 = '2af290091b67d2fa614056caee9c7e18405953f8cee88936dee3a162790b3701'
CAPTURE_VERIFIER_IDENTITIES = {
    'd3be9ec0dc39bfbc44782e47df9e3e3c592d3a2718a2e3ddc26968792bdac7c3': 30048,
    '85c6d911f825a077fb5665f8398bded9b0dfb417e9d48034720aa89832d26c6c': 34289,
}
VERIFIER_PATH = 'Scripts/VerifyRokResearchAnimationRuntime.py'
NAMES = [
    '01_open102_early', '02_close102_early', '03_close102_callback',
    '04_open102_mid', '05_close102_duplicate_mid',
    '06_close102_duplicate_callback', '07_open102_late',
    '08_close128_during_open', '09_close128_callback',
    '10_power_overlap_early', '11_content_early',
    '12_power_mid', '13_content_mid',
    '14_power_late', '15_content_late',
    '16_content_after_power_interrupt', '17_close_with_child_show',
    '18_close_from_power_callback', '19_main_force_closed',
    '20_reference_reopened_reset',
]
ROOT_OPEN = {0, 3, 6, 9}
POWER_SHOW = {9, 11, 13}
CONTENT_SHOW = {10, 12, 14, 15, 16}
CLOSE = {1, 4, 7, 16}
CLOSED_DETAIL = {2, 5, 8, 17}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def hermite(curve, elapsed):
    keys = curve['keyframes']
    assert len(keys) >= 2 and all(a['time'] < b['time'] for a, b in zip(keys, keys[1:]))
    assert curve['interpolation'] == 'cubicHermiteUnweighted'
    if elapsed <= keys[0]['time']:
        return keys[0]['value']
    if elapsed >= keys[-1]['time']:
        return keys[-1]['value']
    left, right = next((a, b) for a, b in zip(keys, keys[1:])
                       if a['time'] <= elapsed <= b['time'])
    width = right['time'] - left['time']
    t = (elapsed - left['time']) / width
    return ((2 * t**3 - 3 * t**2 + 1) * left['value']
            + (t**3 - 2 * t**2 + t) * width * left['outSlope']
            + (-2 * t**3 + 3 * t**2) * right['value']
            + (t**3 - t**2) * width * right['inSlope'])


def source_contract():
    data = json.loads(CONTRACT.read_text(encoding='utf-8-sig'))
    assert data['schema'] == 'rok.research.animation_data.v2'
    clips = {clip['phase']: clip for clip in data['clips']}
    assert set(clips) == {'detail_open', 'power_show_128', 'detail_back_128', 'detail_close'}
    for item in data['sourceFiles']:
        path = (ROOT / item['path']).resolve()
        assert path.is_relative_to(ROOT.resolve()) and sha(path) == item['sha256'], item['path']
    expected_properties = {
        'detail_open': {'m_LocalScale.x', 'm_LocalScale.y'},
        'power_show_128': {'m_Alpha', 'm_AnchoredPosition.x'},
        'detail_back_128': {'m_Alpha', 'm_AnchoredPosition.x'},
        'detail_close': {'m_LocalScale.x', 'm_LocalScale.y', 'm_LocalScale.z', 'm_Alpha'},
    }
    for phase, clip in clips.items():
        assert clip['durationSeconds'] == (.15 if phase == 'detail_close' else .25)
        assert not clip['loop']
        curves = {curve['propertyPath']: curve for curve in clip['curves']}
        assert set(curves) == expected_properties[phase]
        assert clip['sampleTimesSeconds'][0] == 0
        assert clip['sampleTimesSeconds'][-1] >= (.2 if phase == 'detail_close' else .3)
        for sample in clip['predictedSamples']:
            elapsed = sample['elapsedSeconds']
            for property_path, curve in curves.items():
                assert abs(hermite(curve, elapsed) - sample['values'][property_path]) <= 1e-7
    assert 'default Empty' in data['sourceActions']['detail_open']
    assert 'PlayUIAnimation' in data['sourceActions']['power_show_128']
    assert 'PlayUIAnimation' in data['sourceActions']['detail_back_128']
    assert 'IsAniCloseing' in data['sourceActions']['detail_close']
    assert 'CloseAnimatorCallback' in data['sourceActions']['detail_close_event']
    assert 'true,true' in data['sourceActions']['main_close']
    return data, clips


def close(actual, expected, tolerance, where):
    assert math.isfinite(actual) and abs(actual - expected) <= tolerance, (
        where, actual, expected, tolerance)


def rect_close(actual, expected, tolerance, where):
    for key in ('x', 'y', 'width', 'height'):
        close(actual[key], expected[key], tolerance, (where, key))


def get_native(clips):
    assert sha(BASELINE_NATIVE) == BASELINE_NATIVE_SHA256
    assert sha(BASELINE_VERIFIED) == BASELINE_VERIFIED_SHA256
    assert NATIVE.is_file() and VERIFIED.is_file()
    assert sha(NATIVE) == NATIVE_SHA256
    assert sha(VERIFIED) == VERIFIED_SHA256
    evidence = json.loads(NATIVE.read_text(encoding='utf-8'))
    verified = json.loads(VERIFIED.read_text(encoding='utf-8'))
    assert evidence['schema'] == 'rok.research.animation_native.v2'
    assert evidence['complete'] and not evidence.get('failure')
    assert evidence['playMode'] and evidence['syntheticFixture']
    assert evidence['noAccountOrNetworkAction']
    assert all(evidence[key] for key in ('originalUiRoot', 'originalMainPrefab',
                                         'originalDetailPrefab', 'originalAnimatorControllers'))
    assert evidence['width'] == evidence['cameraPixelWidth'] == 1440
    assert evidence['height'] == evidence['cameraPixelHeight'] == 810
    assert verified['schema'] == 'rok.research.animation_native_close_verified.v1'
    assert verified['complete'] and verified['renderEvidenceSha256'] == NATIVE_SHA256
    assert verified['sampleCount'] == len(evidence['samples']) == 62
    assert (ROOT / verified['artifact'] / 'render_evidence.json').resolve() == NATIVE.resolve()
    assert evidence['animationDataSha256'] == sha(CONTRACT)
    phases = [
        'detail_open', 'power_show_128', 'detail_back_128',
        'open_then_more_overlap_128', 'power_interrupted_to_back_128',
        'detail_close_normal', 'open_then_close_128', 'power_then_close_128',
        'close_then_more_128', 'close_then_back_128']
    assert [x['phase'] for x in evidence['samples']] == [
        phase for phase, count in zip(phases, (8, 7, 7, 7, 7, 5, 5, 5, 5, 5))
        for _ in range(count)] + ['detail_removed_after_callback']
    assert verified['phaseCounts'] == {
        **dict(zip(phases, (8, 7, 7, 7, 7, 5, 5, 5, 5, 5))),
        'detail_removed_after_callback': 1}
    for sample in evidence['samples']:
        image = NATIVE.parent / sample['png']
        assert image.is_file() and sha(image) == sample['sha256']
        assert verified['pngSha256'][sample['key']] == sample['sha256']
        with Image.open(image) as picture:
            assert picture.size == (1440, 810)
        if sample['phase'] == 'detail_removed_after_callback':
            assert not sample['detailExists'] and sample['simulatedRemoval']
            assert sample['closeAnimatorEventCount'] == sample['managerCallbackCount'] == 1
            continue
        assert sample['originalRootAnimator']
        assert sample['originalUpdateAnimator']
        assert sample['originalPowerAnimator']
        assert sample['rootState'] == ('Close' if sample['phase'] in phases[5:] else 'Open')
        assert sample['detailWindow']['activeSelf']
        assert sample['backButton']['activeSelf'] == sample['backActive']
        if sample['phase'] in ('power_show_128', 'open_then_more_overlap_128'):
            assert sample['backActive'] and sample['powerActive'] and not sample['updateActive']
        if sample['phase'] in ('detail_open', 'detail_back_128',
                               'power_interrupted_to_back_128',
                               'detail_close_normal', 'open_then_close_128'):
            assert not sample['backActive'] and not sample['powerActive']
            assert sample['updateActive']
        phase = sample['phase']
        t = sample['elapsedSeconds']
        if phase in phases[5:]:
            for axis in ('x', 'y'):
                close_curve = next(c for c in clips['detail_close']['curves']
                                   if c['propertyPath'] == 'm_LocalScale.' + axis)
                close(sample['detailWindow']['localScale'][axis],
                      hermite(close_curve, t), .003, (sample['key'], 'native close scale', axis))
            alpha_curve = next(c for c in clips['detail_close']['curves']
                               if c['propertyPath'] == 'm_Alpha')
            close(sample['detailWindow']['canvasGroupAlpha'],
                  hermite(alpha_curve, t), .005, (sample['key'], 'native close alpha'))
            assert sample['closeAnimatorEventCount'] == sample['managerCallbackCount']
            if t < .15 - 1e-5:
                assert sample['isAniClosing'] and not sample['managerDestroyRequested']
            if t >= .2 - 1e-5:
                assert sample['managerDestroyRequested'] and not sample['isAniClosing']
            if phase == 'detail_close_normal' and .05 - 1e-5 <= t < .15 - 1e-5:
                assert sample['duplicateCloseIgnored']
        if phase == 'detail_open':
            root_time = t
        elif phase == 'open_then_more_overlap_128':
            root_time = min(.3, .05 + t)
        elif phase in phases[5:]:
            root_time = None
        else:
            root_time = .25
        if root_time is not None:
            for axis in ('x', 'y'):
                root_curve = next(c for c in clips['detail_open']['curves']
                                  if c['propertyPath'] == 'm_LocalScale.' + axis)
                close(sample['detailWindow']['localScale'][axis],
                      hermite(root_curve, root_time), .003,
                      (sample['key'], 'native root', axis))
        if phase in ('power_show_128', 'open_then_more_overlap_128'):
            for field, measured, tolerance in (
                ('m_AnchoredPosition.x', sample['powerRoot']['anchoredPosition']['x'], .12),
                ('m_Alpha', sample['powerRoot']['canvasGroupAlpha'], .005)):
                curve = next(c for c in clips['power_show_128']['curves']
                             if c['propertyPath'] == field)
                close(measured, hermite(curve, t), tolerance, (sample['key'], field))
        if phase in ('detail_back_128', 'power_interrupted_to_back_128'):
            for field, measured, tolerance in (
                ('m_AnchoredPosition.x', sample['updateRoot']['anchoredPosition']['x'], .12),
                ('m_Alpha', sample['updateRoot']['canvasGroupAlpha'], .005)):
                curve = next(c for c in clips['detail_back_128']['curves']
                             if c['propertyPath'] == field)
                close(measured, hermite(curve, t), tolerance, (sample['key'], field))
        if phase == 'power_interrupted_to_back_128':
            curve_x = next(c for c in clips['power_show_128']['curves']
                           if c['propertyPath'] == 'm_AnchoredPosition.x')
            curve_a = next(c for c in clips['power_show_128']['curves']
                           if c['propertyPath'] == 'm_Alpha')
            close(sample['interruptedOutgoingPowerX'], hermite(curve_x, .1), .12,
                  (sample['key'], 'interrupted power x'))
            close(sample['interruptedOutgoingPowerAlpha'], hermite(curve_a, .1), .005,
                  (sample['key'], 'interrupted power alpha'))
    return evidence


def image_at(capture):
    image = ROOT / capture['file']
    assert image.is_file() and sha(image) == capture['png_sha256']
    assert image.stat().st_size == capture['file_bytes']
    with Image.open(image) as picture:
        assert picture.size == (1440, 810), image
        return np.asarray(picture.convert('RGB'), dtype=np.int16)


def difference(images, a, b, roi, label, min_pixels=250):
    x = max(0, math.floor(roi['x']))
    y = max(0, math.floor(roi['y']))
    x1 = min(1440, math.ceil(roi['x'] + roi['width']))
    y1 = min(810, math.ceil(roi['y'] + roi['height']))
    assert x1 > x and y1 > y, label
    delta = np.max(np.abs(images[a][y:y1, x:x1] -
                             images[b][y:y1, x:x1]), axis=2)
    changed = int(np.count_nonzero(delta >= 16))
    assert changed >= min_pixels, (label, changed, min_pixels)
    return changed


def verify(run):
    run = run.resolve()
    assert run.is_dir() and run.parent == RUNS.resolve()
    report_file = run / 'runtime_verify.json'
    try:
        _, clips = source_contract()
        native = get_native(clips)
        manifest_file = run / 'manifest.json'
        manifest = json.loads(manifest_file.read_text(encoding='utf-8'))
        assert manifest['schema'] == 'rok.research_animation.runtime.v1'
        assert manifest['complete'] and manifest['passed'] and not manifest['failures']
        assert manifest['audit_flag'] == '-RokResearchAnimationAudit'
        assert Path(manifest['project']).resolve() == ROOT.resolve()
        assert manifest['world'] == '/Game/Maps/L_RokUnityGameAppCity'
        assert manifest['entry_kind'] == 'HUD command_line_reference research_reference'
        assert not manifest['real_academy_entry_verified']
        assert not manifest['account_research_state_synced']
        assert not manifest['research_queue_synced']
        assert manifest['expected_capture_count'] == len(NAMES)
        assert (ROOT / manifest['source_contract']).resolve() == CONTRACT.resolve()
        assert (ROOT / manifest['native_evidence']).resolve() == NATIVE.resolve()
        assert (ROOT / manifest['native_verified']).resolve() == VERIFIED.resolve()
        assert (ROOT / manifest['native_baseline_evidence']).resolve() == BASELINE_NATIVE.resolve()
        assert (ROOT / manifest['native_baseline_verified']).resolve() == BASELINE_VERIFIED.resolve()
        inputs = manifest['input_hashes']
        for relative, identity in inputs.items():
            path = (ROOT / relative).resolve()
            assert path.is_relative_to(ROOT.resolve()) and path.is_file(), relative
            if relative == VERIFIER_PATH:
                assert (CAPTURE_VERIFIER_IDENTITIES.get(identity['sha256'])
                        == identity['bytes']) or (
                        sha(path) == identity['sha256']
                        and path.stat().st_size == identity['bytes'])
            else:
                assert sha(path) == identity['sha256'] and path.stat().st_size == identity['bytes'], relative
        assert inputs[manifest['native_evidence']]['sha256'] == NATIVE_SHA256
        assert inputs[manifest['source_contract']]['sha256'] == sha(CONTRACT)
        assert inputs[manifest['native_verified']]['sha256'] == VERIFIED_SHA256
        assert inputs[manifest['native_baseline_evidence']]['sha256'] == BASELINE_NATIVE_SHA256
        assert inputs[manifest['native_baseline_verified']]['sha256'] == BASELINE_VERIFIED_SHA256
        assert inputs['Source/Rok/RokResearchScreenWidget.cpp']['sha256'] == REVIEWED_UI_SHA256
        assert inputs['Source/Rok/RokUnityFeatureScreenWidget.cpp']['sha256'] == REVIEWED_FEATURE_SHA256
        baseline = manifest['baseline']
        assert baseline['academy_actors'] == 0
        assert baseline['source_building_snapshots'] > 0
        assert baseline['building_state_sha256'] and baseline['troop_state_sha256']
        assert baseline['profile_sha256'] and baseline['resources_sha256']
        assert isinstance(baseline['local_inventory_fingerprint'], str)
        captures = manifest['captures']
        assert len(captures) == len(NAMES)
        assert [c['phase_name'] for c in captures] == NAMES
        assert len({c['png_sha256'] for c in captures}) == len(captures)
        images = [image_at(c) for c in captures]
        native_terminal = next(s for s in native['samples']
                               if s['phase'] == 'detail_open'
                               and abs(s['elapsedSeconds'] - .25) < 1e-5)
        native_power_terminal = next(s for s in native['samples']
                                     if s['phase'] == 'power_show_128'
                                     and abs(s['elapsedSeconds'] - .25) < 1e-5)
        open_curve = next(c for c in clips['detail_open']['curves']
                          if c['propertyPath'] == 'm_LocalScale.x')
        close_curves = {c['propertyPath']: c for c in clips['detail_close']['curves']}
        power_curves = {c['propertyPath']: c for c in clips['power_show_128']['curves']}
        content_curves = {c['propertyPath']: c for c in clips['detail_back_128']['curves']}
        for index, capture in enumerate(captures):
            request, processed = capture['request'], capture['processed']
            assert Path(capture['file']).resolve() == (run / (NAMES[index] + '.png'))
            assert request['step'] == processed['step']
            assert request['frame'] <= processed['frame']
            assert request['world_elapsed_seconds'] <= processed['world_elapsed_seconds']
            for state in (request, processed):
                assert state['state_unchanged']
                assert not state.get('account_research_synced')
                assert not state.get('research_queue_synced')
                assert state['viewport_pixels'] == {
                    'x': 0, 'y': 0, 'width': 1440, 'height': 810}
                if index == 18:
                    assert not state['screen_open'] and not state['research_mode']
                    continue
                assert state['screen_open'] and state['research_mode']
                assert state['reference_preview']
                assert state['selected_type'] == 1
                assert state['viewed_config_level'] == 1
                assert state['pending_action_id'] == (
                    22 if index == 16 and state is request else -1)
                expect_study = 0 if index in CLOSED_DETAIL or index == 19 else (
                    102 if index < 7 else 128)
                if state['detail_closed']:
                    expect_study = 0
                assert state['selected_study_type'] == expect_study
                assert state['feature_dimmer']['attached']
                for channel in ('r', 'g', 'b'):
                    close(state['feature_dimmer_color'][channel], 0, 1e-6,
                          (index, 'feature dimmer', channel))
                close(state['feature_dimmer_color']['a'], .8, 1e-6,
                      (index, 'feature dimmer alpha'))
                mask = state['detail_mask']
                shade = state['detail_mask_shade']
                hit = state['detail_mask_hit']
                assert mask['attached'] and shade['attached'] and hit['attached']
                assert mask['parent'] == state['detail_root']['parent'] == 'ResearchCanvas'
                assert shade['parent'] == hit['parent'] == 'ResearchDetailMask'
                for channel in ('r', 'g', 'b'):
                    close(state['detail_mask_shade_color'][channel], 0, 1e-6,
                          (index, 'detail shade', channel))
                close(state['detail_mask_shade_color']['a'], .8, 1e-6,
                      (index, 'detail shade alpha'))
                for panel in (mask, shade, hit):
                    rect_close(panel['slot_rect'], {'x': 0, 'y': 0,
                               'width': 1440, 'height': 810}, .1,
                               (index, 'source detail mask slot'))
                if state['detail_root']['visible']:
                    assert mask['visible'] and shade['visible'] and hit['visible']
                    assert not state['feature_dimmer']['visible']
                    for panel in (mask, shade, hit):
                        rect_close(panel['pixel_rect'], {'x': 0, 'y': 0,
                                   'width': 1440, 'height': 810}, 2,
                                   (index, 'source detail mask viewport'))
                else:
                    assert not mask['visible'] and not shade['visible'] and not hit['visible']
                    assert state['feature_dimmer']['visible']
                if state['root_open_active']:
                    elapsed = state['root_open_elapsed_seconds']
                    expected = hermite(open_curve, elapsed)
                    close(state['detail_root']['render_scale_x'], expected, .003,
                          (index, 'root scale x'))
                    close(state['detail_root']['render_scale_y'], expected, .003,
                          (index, 'root scale y'))
                    close(state['detail_root']['render_opacity'], 1, .005,
                          (index, 'open root opacity'))
                if state['detail_closing']:
                    assert state['phase'] == 'DetailClose'
                    elapsed = state['detail_close_elapsed_seconds']
                    close(state['detail_root']['render_scale_x'],
                          hermite(close_curves['m_LocalScale.x'], elapsed), .003,
                          (index, 'close scale x'))
                    close(state['detail_root']['render_scale_y'],
                          hermite(close_curves['m_LocalScale.y'], elapsed), .003,
                          (index, 'close scale y'))
                    close(state['detail_root']['render_opacity'],
                          hermite(close_curves['m_Alpha'], elapsed), .015,
                          (index, 'close opacity'))
                    assert not state['root_open_active']
                if state['detail_root']['visible']:
                    scale = state['detail_root']['render_scale_x']
                    source_root = native_terminal['detailWindow']['screenRect']
                    cx = source_root['x'] + source_root['width'] / 2
                    cy = source_root['y'] + source_root['height'] / 2
                    animated_rect = {
                        'x': cx - source_root['width'] * scale / 2,
                        'y': cy - source_root['height'] * scale / 2,
                        'width': source_root['width'] * scale,
                        'height': source_root['height'] * scale}
                    rect_close(state['detail_root']['pixel_rect'], animated_rect,
                               2, (index, 'source root render geometry'))
                if state['child_show_active'] and state['power_root']['visible']:
                    elapsed = state['child_show_elapsed_seconds']
                    close(state['power_root']['render_translation_x'],
                          hermite(power_curves['m_AnchoredPosition.x'], elapsed), .75,
                          (index, 'power x'))
                    close(state['power_root']['render_opacity'],
                          hermite(power_curves['m_Alpha'], elapsed), .015,
                          (index, 'power alpha'))
                    assert not state['detail_update_root']['visible']
                    assert state['power_back']['visible']
                    assert state['power_back']['parent'] == 'ResearchDetailRoot'
                    rect_close(state['power_back']['slot_rect'],
                               native_power_terminal['backButton']['screenRect'],
                               2, (index, 'source back slot'))
                if state['child_show_active'] and state['detail_update_root']['visible']:
                    elapsed = state['child_show_elapsed_seconds']
                    close(state['detail_update_root']['render_translation_x'],
                          hermite(content_curves['m_AnchoredPosition.x'], elapsed), .75,
                          (index, 'content x'))
                    close(state['detail_update_root']['render_opacity'],
                          hermite(content_curves['m_Alpha'], elapsed), .015,
                          (index, 'content alpha'))
                    assert not state['power_root']['visible']
                    assert not state['power_back']['visible']
                if state['detail_closed']:
                    assert state['phase'] == 'DetailClosed'
                    assert not state['detail_root']['visible']
                    assert not state['detail_closing']
                    assert not state['child_show_active']
                if index == 19:
                    assert state['phase'] == 'Idle'
                    assert not state['detail_root']['visible']
                    assert state['interrupt_count'] == 0
                    close(state['economy_scroll_offset'], 0, .01, 'reopen scroll')
            if index in ROOT_OPEN:
                assert request['root_open_active']
            if index in POWER_SHOW:
                assert request['phase'] == 'PowerShow'
                assert request['child_show_active']
                assert request['power_root']['visible']
            if index in CONTENT_SHOW - {16}:
                assert request['phase'] == 'ContentShow'
                assert request['child_show_active']
                assert request['detail_update_root']['visible']
            if index in POWER_SHOW | (CONTENT_SHOW - {16}):
                assert .025 <= request['child_show_elapsed_seconds'] < .25
            if index in CLOSE:
                assert request['detail_closing'] and request['phase'] == 'DetailClose'
                assert .025 <= request['detail_close_elapsed_seconds'] < .15
            if index in CLOSED_DETAIL:
                assert request['detail_closed']
            if index in (0, 3, 6):
                assert request['phase'] == 'DetailOpen'
            if index in (2, 5, 8, 17):
                assert request['phase'] == 'DetailClosed'
            if index == 19:
                assert request['phase'] == 'Idle'
        # The source and UE must show actual changing pixels in the animated
        # detail region, independent of self-reported clocks and transforms.
        roi = native_terminal['updateRoot']['screenRect']
        changed = {
            'detail_open': difference(images, 0, 6, native_terminal['detailWindow']['screenRect'],
                                      'detail open', 500),
            'detail_close': difference(images, 1, 2, native_terminal['detailWindow']['screenRect'],
                                       'detail close', 500),
            'open_preempted_by_close': difference(images, 7, 8,
                                                 native_terminal['detailWindow']['screenRect'],
                                                 'open then close', 500),
            'power_show': difference(images, 9, 13, roi, 'power show', 500),
            'content_show': difference(images, 10, 14, roi, 'content show', 500),
            'interrupted_back': difference(images, 14, 15, roi, 'interrupted back', 500),
            'power_close': difference(images, 16, 17, roi, 'power then close', 500),
            'main_force_close': difference(images, 17, 18,
                                           native_terminal['detailWindow']['screenRect'],
                                           'main force close', 500),
            'mask_dimming_military_tab': difference(images, 0, 2,
                                                  {'x': 1259, 'y': 304,
                                                   'width': 131, 'height': 95},
                                                  'detail mask dimming', 500),
        }
        actions = manifest['actions']
        assert len(actions) == 24
        assert [a['button'] for a in actions] == [
            'ResearchNodeHit_102', 'ResearchDetailMaskHit',
            'ResearchNodeHit_102', 'ResearchDetailBack', 'ResearchDetailBack',
            'ResearchNodeHit_102', 'ResearchDetailBack',
            'ResearchEconomyScroll', 'ResearchNodeHit_128', 'ResearchDetailBack',
            'ResearchNodeHit_128', 'ResearchDetailMore', 'ResearchPowerBack',
            'ResearchDetailMore', 'ResearchPowerBack',
            'ResearchDetailMore', 'ResearchPowerBack',
            'ResearchDetailMore', 'ResearchPowerBack',
            'ResearchDetailMore', 'ResearchDetailBack', 'ResearchPowerBack',
            'ResearchDetailMaskHit',
            'ResearchCloseButton']
        for action in actions:
            if action['button'] == 'ResearchEconomyScroll':
                assert action['method'] == 'UScrollBox.ScrollToEnd'
                continue
            assert action['method'] == 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'
            assert action['target_in_hit_path']
            assert action['target_hovered_after_move']
            assert action['down_handled'] and action['up_handled']
            assert action['slate_widget_path_types']
            assert action['before']['state_unchanged']
            if action['button'] == 'ResearchDetailMaskHit':
                assert action['masked_underlay'] == 'ResearchMilitaryTab'
                assert not action['underlay_in_hit_path']
                assert not action['underlay_hovered_after_move']
                assert action['before']['selected_type'] == 1
                assert action['before']['detail_mask_hit']['visible']
                assert not action['before']['feature_dimmer']['visible']
        assert actions[4]['before']['detail_closing']
        assert 0 < actions[4]['before']['detail_close_elapsed_seconds'] < .15
        assert captures[4]['request']['detail_close_elapsed_seconds'] > (
            actions[4]['before']['detail_close_elapsed_seconds']), 'duplicate Back restarted Close'
        assert actions[9]['before']['root_open_active'], 'Open was not active before Close'
        assert captures[7]['request']['interrupt_count'] > (
            actions[9]['before']['interrupt_count']), 'Close did not preempt Open'
        assert actions[11]['before']['root_open_active'], 'More did not begin during root Open'
        assert captures[9]['request']['root_open_active'], 'More must overlap root Open'
        assert captures[9]['request']['root_open_elapsed_seconds'] > (
            captures[9]['request']['child_show_elapsed_seconds'])
        assert actions[18]['before']['child_show_active']
        assert captures[15]['request']['interrupt_count'] == (
            actions[18]['before']['interrupt_count'] + 1)
        outgoing = captures[15]['request']['power_root']
        prior = actions[18]['before']['power_root']
        assert not outgoing['visible']
        assert 0 < outgoing['render_translation_x'] <= prior['render_translation_x'] < 50
        assert 0 < prior['render_opacity'] <= outgoing['render_opacity'] < 1
        close(outgoing['render_translation_x'] + 50 * outgoing['render_opacity'],
              50, .75, 'source interrupted Power retains last x/alpha')
        assert actions[20]['before']['child_show_active']
        assert actions[21]['before']['detail_closing']
        assert actions[21]['before']['child_show_active']
        assert actions[22]['before']['detail_closing']
        assert .055 <= actions[22]['before']['detail_close_elapsed_seconds'] < .14
        assert captures[16]['request']['detail_close_elapsed_seconds'] >= (
            actions[22]['before']['detail_close_elapsed_seconds'])
        assert captures[16]['processed']['detail_close_elapsed_seconds'] > (
            actions[22]['before']['detail_close_elapsed_seconds']), (
            'Masked second click reset Close before screenshot was processed',
            actions[22]['before']['detail_close_elapsed_seconds'],
            captures[16]['processed']['detail_close_elapsed_seconds'])
        assert 0 <= captures[17]['request']['world_elapsed_seconds'] - actions[22][
            'world_elapsed_seconds'] < .15, 'Masked second click restarted Close or leaked to tab'
        assert captures[16]['request']['detail_closing']
        assert captures[16]['request']['child_show_active']
        assert captures[16]['request']['detail_close_elapsed_seconds'] > (
            actions[21]['before']['detail_close_elapsed_seconds'])
        assert captures[16]['request']['child_show_elapsed_seconds'] > 0
        assert captures[17]['request']['detail_closed']
        assert captures[17]['request']['selected_study_type'] == 0
        assert 0 <= captures[18]['request']['world_elapsed_seconds'] - actions[-1][
            'world_elapsed_seconds'] < .15
        result = {
            'schema': 'rok.research_animation.runtime_verify.v1',
            'passed': True, 'captures': len(captures), 'actions': len(actions),
            'slate_clicks': 23, 'native_evidence_sha256': NATIVE_SHA256,
            'source_animation_sha256': sha(CONTRACT),
            'capture_verifier_sha256': inputs[VERIFIER_PATH]['sha256'],
            'current_verifier_sha256': sha(ROOT / VERIFIER_PATH),
            'pixel_changes_ge16': changed,
            'scope': 'GameApp HUD command-line reference, virtual Slate events; Academy/account/request unverified'}
    except Exception as exc:
        result = {'schema': 'rok.research_animation.runtime_verify.v1',
                  'passed': False, 'error': repr(exc),
                  'error_traceback': traceback.format_exc()}
    report_file.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n',
                           encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))
    if not result['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('run', type=Path)
    verify(parser.parse_args().run)
