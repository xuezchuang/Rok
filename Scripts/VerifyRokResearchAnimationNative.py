"""Strict source/hash/Animator-state check for the independent Research animation capture."""
import argparse
import hashlib
import json
import math
import struct
from collections import Counter
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'Saved/UnityResearchAnimationProbe'
PACKAGE = ROOT / 'Saved/RokUnityMigration/ResearchAnimationNative'
PHASES = ('detail_open', 'power_show_128', 'detail_back_128',
          'open_then_more_overlap_128', 'power_interrupted_to_back_128',
          'detail_close_normal', 'open_then_close_128', 'power_then_close_128',
          'close_then_more_128', 'close_then_back_128')
GRID = (0, .05, .1, .15, .2, .25, .3)
CLOSE_GRID = (0, .05, .1, .15, .2)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def data(path):
    return json.loads(path.read_text(encoding='utf-8'))


def close(actual, expected, tolerance, where):
    assert math.isfinite(actual) and abs(actual - expected) <= tolerance, (where, actual, expected, tolerance)


def predicted(animation, phase, time, field):
    clip = next(x for x in animation['clips'] if x['phase'] == phase)
    sample = min(clip['predictedSamples'], key=lambda x: abs(x['elapsedSeconds'] - time))
    close(sample['elapsedSeconds'], time, 1e-6, 'source sample time')
    return sample['values'][field]


def png_size(path):
    b = path.read_bytes()[:24]
    assert b[:8] == b'\x89PNG\r\n\x1a\n'
    return struct.unpack('>II', b[16:24])


def verify(artifact):
    artifact = artifact.resolve()
    assert artifact.is_dir() and artifact.parent == (PROJECT / 'Artifacts').resolve(), artifact
    report_path = artifact / 'render_evidence.json'
    report = data(report_path)
    inputs = data(artifact / 'ReferenceInputs.json')
    source = data(artifact / 'SourceResearchInputs.json')
    animation = data(artifact / 'AnimationData.json')
    assert inputs['schema'] == 'rok.research.animation_native_inputs.v1'
    assert animation['schema'] == 'rok.research.animation_data.v2'
    assert report['schema'] == 'rok.research.animation_native.v2' and report['complete'] and not report.get('failure')
    assert report['playMode'] and report['syntheticFixture'] and report['noAccountOrNetworkAction']
    assert report['sourceCloseEventMethodAdapted'] and not report['actualUiManagerInvoked']
    assert all(report[name] for name in ('originalUiRoot', 'originalMainPrefab', 'originalDetailPrefab', 'originalAnimatorControllers'))
    assert report['unityVersion'] == '2019.4.28f1c1'
    assert (report['width'], report['height'], report['cameraPixelWidth'], report['cameraPixelHeight']) == (1440, 810, 1440, 810)
    assert report['inputSha256'] == sha(artifact / 'ReferenceInputs.json') == sha(PROJECT / 'ReferenceInputs.json')
    for key, name, current in (
        ('sourceResearchInputsSha256', 'SourceResearchInputs.json', PROJECT / 'SourceResearchInputs.json'),
        ('sourceFixtureSha256', 'ResearchPowerFixture.json', PROJECT / 'ResearchPowerFixture.json'),
        ('animationDataSha256', 'AnimationData.json', PACKAGE / 'animation_data.json')):
        assert report[key] == inputs[key] == sha(artifact / name) == sha(current), key
    assert inputs['rendererSha256'] == report['rendererSha256'] == sha(ROOT / 'Scripts/RenderRokResearchAnimationNative.cs')
    assert inputs['prepareSha256'] == sha(ROOT / 'Scripts/PrepareRokResearchAnimationNative.py')
    assert inputs['instrumentedViewBinderSha256'] == report['instrumentedViewBinderSha256'] == sha(PROJECT / 'Assets/ReferenceComponents/ViewBinder.cs')
    assert source['fixture']['moreDetailStudyType'] == 128
    assert source['fixture']['state']['synthetic'] and not source['fixture']['state']['accountBacked']
    assert len(source['fixture']['moreLevels']) == 10
    for row in animation['sourceFiles']:
        assert sha(resolve(row['path'])) == row['sha256'], row['path']
    assert [x['phase'] for x in animation['clips']] == [*PHASES[:3], 'detail_close']
    assert all(x['durationSeconds'] == (.15 if x['phase']=='detail_close' else .25) and not x['loop'] for x in animation['clips'])
    assert animation['clips'][3]['events'] == [{'time':.15,'functionName':'CloseAnimatorCallback'}]
    assert all(not x['events'] for x in animation['clips'][:3])
    for clip in animation['clips']:
        assert all(binding['relativeClipPath'] == '' and binding['interpolation'] == 'cubicHermiteUnweighted' for binding in clip['curves'])
        assert all(k['weightedMode'] == 0 and k['inSlope'] == 0 and k['outSlope'] == 0
                   for b in clip['curves'] for k in b['keyframes'])
    samples = report['samples']
    assert len(samples) == 62, len(samples)
    counts = Counter(s['phase'] for s in samples)
    assert counts == {'detail_open': 8, **{phase: 7 for phase in PHASES[1:5]},
                      **{phase: 5 for phase in PHASES[5:]}, 'detail_removed_after_callback':1}
    assert len({s['key'] for s in samples}) == len(samples)
    assert [s['phase'] for s in samples] == [phase for phase in PHASES for _ in range(counts[phase])] + ['detail_removed_after_callback']
    for phase in PHASES:
        times = [s['elapsedSeconds'] for s in samples if s['phase'] == phase]
        expected = animation['clips'][0]['sampleTimesSeconds'] if phase == 'detail_open' else (CLOSE_GRID if phase in PHASES[5:] else GRID)
        assert len(times) == len(expected)
        for i, (a, b) in enumerate(zip(times, expected)):
            close(a, b, 1e-6, (phase, i, 'elapsedSeconds'))
    for s in samples:
        phase, t = s['phase'], s['elapsedSeconds']
        if phase == 'detail_removed_after_callback':
            assert s['key'] == phase
        else:
            assert s['key'].startswith(phase + '_t')
        assert s['png'] == s['key'] + '.png'
        image = artifact / s['png']
        assert image.is_file() and sha(image) == s['sha256']
        assert png_size(image) == (1440, 810)
        assert not s['actualUiManagerInvoked']
        if phase == 'detail_removed_after_callback':
            assert not s['detailExists'] and s['simulatedRemoval'] and s['managerDestroyRequested']
            assert s['closeAnimatorEventCount'] == s['managerCallbackCount'] == 1
            # Unity JsonUtility serializes null serializable nested classes as
            # default-valued objects; absence is signalled by detailExists.
            for key in ('detailWindow','updateRoot','powerRoot'):
                node=s.get(key)
                assert node is None or (node['path']=='' and not node['activeSelf'] and
                                        node['screenRect']['width']==0 and node['screenRect']['height']==0)
            continue
        assert s['detailExists'] and not s['simulatedRemoval']
        assert s['originalRootAnimator'] and s['originalUpdateAnimator'] and s['originalPowerAnimator']
        assert s['rootState'] == ('Close' if phase in PHASES[5:] else 'Open')
        if phase == 'detail_open':
            assert s['updateActive'] and not s['powerActive'] and not s['backActive']
            close(s['rootElapsedSeconds'], t, 1e-6, 'root clock')
            for axis in ('x', 'y'):
                close(s['detailWindow']['localScale'][axis], predicted(animation, 'detail_open', t, 'm_LocalScale.' + axis), .003, (phase,t,axis))
            assert s['powerState'] == 'inactive_or_other'
        elif phase == 'power_show_128':
            assert not s['updateActive'] and s['powerActive'] and s['backActive']
            assert s['powerState'] == 'Show'
            close(s['powerElapsedSeconds'], t, 1e-6, 'power clock')
            close(s['powerRoot']['anchoredPosition']['x'], predicted(animation, phase, t, 'm_AnchoredPosition.x'), .12, (phase,t,'power x'))
            close(s['powerRoot']['canvasGroupAlpha'], predicted(animation, phase, t, 'm_Alpha'), .005, (phase,t,'power alpha'))
        elif phase == 'detail_back_128':
            assert s['updateActive'] and not s['powerActive'] and not s['backActive']
            assert s['updateState'] == 'Show'
            close(s['updateElapsedSeconds'], t, 1e-6, 'update clock')
            close(s['updateRoot']['anchoredPosition']['x'], predicted(animation, phase, t, 'm_AnchoredPosition.x'), .12, (phase,t,'update x'))
            close(s['updateRoot']['canvasGroupAlpha'], predicted(animation, phase, t, 'm_Alpha'), .005, (phase,t,'update alpha'))
        elif phase == 'open_then_more_overlap_128':
            assert not s['updateActive'] and s['powerActive'] and s['backActive'] and s['powerState'] == 'Show'
            rt = min(.3, .05+t)
            close(s['rootElapsedSeconds'], rt, 1e-6, 'overlap root clock')
            close(s['powerElapsedSeconds'], t, 1e-6, 'overlap power clock')
            for axis in ('x','y'):
                # The source data's evaluator also clamps at the clip terminal.
                clip = animation['clips'][0]
                keys = next(b['keyframes'] for b in clip['curves'] if b['propertyPath'] == 'm_LocalScale.' + axis)
                root_expected = hermite(keys, rt)
                close(s['detailWindow']['localScale'][axis], root_expected, .003, (phase,t,'root scale'))
            close(s['powerRoot']['anchoredPosition']['x'], predicted(animation,'power_show_128',t,'m_AnchoredPosition.x'), .12, (phase,t,'power x'))
            close(s['powerRoot']['canvasGroupAlpha'], predicted(animation,'power_show_128',t,'m_Alpha'), .005, (phase,t,'power alpha'))
        elif phase == 'power_interrupted_to_back_128':
            assert s['updateActive'] and not s['powerActive'] and not s['backActive'] and s['updateState'] == 'Show'
            close(s['interruptedOutgoingPowerX'], predicted(animation,'power_show_128',.1,'m_AnchoredPosition.x'), .12, (phase,t,'interrupted x'))
            close(s['interruptedOutgoingPowerAlpha'], predicted(animation,'power_show_128',.1,'m_Alpha'), .005, (phase,t,'interrupted alpha'))
            close(s['updateRoot']['anchoredPosition']['x'], predicted(animation,'detail_back_128',t,'m_AnchoredPosition.x'), .12, (phase,t,'update x'))
            close(s['updateRoot']['canvasGroupAlpha'], predicted(animation,'detail_back_128',t,'m_Alpha'), .005, (phase,t,'update alpha'))
        else:
            assert phase in PHASES[5:]
            for axis in ('x','y','z'):
                close(s['detailWindow']['localScale'][axis], predicted(animation,'detail_close',t,'m_LocalScale.'+axis), .003, (phase,t,'close scale '+axis))
            close(s['detailWindow']['canvasGroupAlpha'], predicted(animation,'detail_close',t,'m_Alpha'), .005, (phase,t,'close alpha'))
            assert s['closeAnimatorEventCount'] == s['managerCallbackCount']
            if t < .15-1e-5:
                assert s['closeAnimatorEventCount'] == 0 and s['isAniClosing'] and not s['managerDestroyRequested']
            if t >= .2-1e-5:
                assert s['closeAnimatorEventCount'] == 1 and s['managerDestroyRequested'] and not s['isAniClosing']
            if phase in ('detail_close_normal','open_then_close_128'):
                assert s['updateActive'] and not s['powerActive'] and not s['backActive']
            if phase == 'detail_close_normal' and .05-1e-5 <= t < .15-1e-5:
                assert s['duplicateCloseIgnored']
            if phase == 'open_then_close_128':
                close(s['interruptedOutgoingRootScale'], predicted(animation,'detail_open',.05,'m_LocalScale.x'), .003, (phase,t,'preempted Open scale'))
            if phase == 'power_then_close_128':
                assert not s['updateActive'] and s['powerActive'] and s['backActive']
                close(s['interruptedOutgoingPowerX'], predicted(animation,'power_show_128',.1,'m_AnchoredPosition.x'), .12, (phase,t,'preempted power x'))
                close(s['interruptedOutgoingPowerAlpha'], predicted(animation,'power_show_128',.1,'m_Alpha'), .005, (phase,t,'preempted power alpha'))
                power_keys = next(c['keyframes'] for c in animation['clips'][1]['curves'] if c['propertyPath']=='m_AnchoredPosition.x')
                close(s['powerRoot']['anchoredPosition']['x'],hermite(power_keys,.1+t),.12,(phase,t,'concurrent power x'))
            if phase == 'close_then_more_128':
                assert s['powerActive'] == (t >= .05-1e-5) and s['backActive'] == (t >= .05-1e-5)
                assert s['updateActive'] == (t < .05-1e-5)
                if t >= .05-1e-5:
                    close(s['powerElapsedSeconds'],t-.05,1e-5,(phase,t,'power clock'))
                    power_keys = next(c['keyframes'] for c in animation['clips'][1]['curves'] if c['propertyPath']=='m_AnchoredPosition.x')
                    close(s['powerRoot']['anchoredPosition']['x'],hermite(power_keys,t-.05),.12,(phase,t,'concurrent power x'))
            if phase == 'close_then_back_128':
                assert s['updateActive'] == (t >= .05-1e-5)
                assert s['powerActive'] == (t < .05-1e-5) and s['backActive'] == (t < .05-1e-5)
                if t >= .05-1e-5:
                    close(s['updateElapsedSeconds'],t-.05,1e-5,(phase,t,'update clock'))
                    update_keys = next(c['keyframes'] for c in animation['clips'][2]['curves'] if c['propertyPath']=='m_AnchoredPosition.x')
                    close(s['updateRoot']['anchoredPosition']['x'],hermite(update_keys,t-.05),.12,(phase,t,'concurrent update x'))
        assert s['updateRoot']['activeSelf'] == s['updateActive']
        assert s['powerRoot']['activeSelf'] == s['powerActive']
        assert s['backButton']['activeSelf'] == s['backActive']
        for key in ('detailWindow','updateRoot','powerRoot','title','backButton','powerViewport'):
            rect = s[key]['screenRect']
            assert all(math.isfinite(rect[n]) for n in ('x','y','width','height')), (s['key'],key)
            assert rect['width'] > 0 and rect['height'] > 0, (s['key'],key)
    verified = {'schema':'rok.research.animation_native_close_verified.v1', 'complete':True,
                'artifact':artifact.relative_to(ROOT).as_posix(), 'renderEvidenceSha256':sha(report_path),
                'inputSha256':sha(artifact/'ReferenceInputs.json'), 'animationDataSha256':sha(artifact/'AnimationData.json'),
                'sourceFixtureSha256':sha(artifact/'ResearchPowerFixture.json'), 'sampleCount':len(samples),
                'phaseCounts':dict(counts), 'pngSha256':{s['key']:s['sha256'] for s in samples}}
    path = PACKAGE / 'verified_animation_close_capture.json'
    path.write_text(json.dumps(verified,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'verified':True,'artifact':str(artifact),'evidenceSha256':verified['renderEvidenceSha256'],
                      'verifiedPath':str(path),'sampleCount':len(samples)},ensure_ascii=False))


def hermite(keys, t):
    if t <= keys[0]['time']:
        return keys[0]['value']
    if t >= keys[-1]['time']:
        return keys[-1]['value']
    for a,b in zip(keys,keys[1:]):
        if a['time'] <= t <= b['time']:
            dt=b['time']-a['time']; u=(t-a['time'])/dt
            return ((2*u**3-3*u**2+1)*a['value']+(u**3-2*u**2+u)*dt*a['outSlope']+
                    (-2*u**3+3*u**2)*b['value']+(u**3-u**2)*dt*b['inSlope'])
    raise AssertionError(t)


if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('artifact',type=Path)
    verify(parser.parse_args().artifact)
