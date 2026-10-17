"""Fail-closed verification for the independent ResearchMain mask-close capture."""
import argparse
import hashlib
import json
import math
import struct
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'Saved/UnityResearchMainMaskProbe'
PACKAGE = ROOT / 'Saved/RokUnityMigration/ResearchMainMaskNative'
PHASES = ('mask_close_normal', 'mask_close_interrupt_open')
TIMES = (0, .03, .075, .12, .15)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def near(actual, expected, tolerance, label):
    assert math.isfinite(actual) and abs(actual - expected) <= tolerance, (label, actual, expected, tolerance)


def hermite(keys, time):
    if time <= keys[0]['time']:
        return keys[0]['value']
    if time >= keys[-1]['time']:
        return keys[-1]['value']
    for a, b in zip(keys, keys[1:]):
        if a['time'] <= time <= b['time']:
            dt = b['time'] - a['time']
            u = (time - a['time']) / dt
            return ((2*u**3-3*u**2+1)*a['value'] + (u**3-2*u**2+u)*dt*a['outSlope'] +
                    (-2*u**3+3*u**2)*b['value'] + (u**3-u**2)*dt*b['inSlope'])
    raise AssertionError(time)


def curve(clip, property_name, time):
    binding = next(c for c in clip['curves'] if c['propertyPath'] == property_name)
    assert binding['relativeClipPath'] == '' and binding['interpolation'] == 'cubicHermiteUnweighted'
    assert all(k['weightedMode'] == 0 and k['inSlope'] == 0 and k['outSlope'] == 0
               for k in binding['keyframes'])
    return hermite(binding['keyframes'], time)


def png_dimensions(path):
    b = path.read_bytes()[:24]
    assert b[:8] == b'\x89PNG\r\n\x1a\n', path
    return struct.unpack('>II', b[16:24])


def verify(artifact):
    artifact = artifact.resolve()
    assert artifact.is_dir() and artifact.parent == (PROJECT / 'Artifacts').resolve(), artifact
    report_path = artifact / 'render_evidence.json'
    report = read(report_path)
    inputs = read(artifact / 'ReferenceInputs.json')
    animation = read(artifact / 'AnimationData.json')
    source = read(artifact / 'SourceResearchInputs.json')
    assert inputs['schema'] == 'rok.research_main_mask.inputs.v1'
    assert report['schema'] == 'rok.research_main_mask.native.v1'
    assert report['complete'] and not report.get('failure')
    assert report['playMode'] and report['originalUiRoot'] and report['originalMainPrefab']
    assert report['originalAnimatorController'] and report['recreatedSourceMask']
    assert report['sourceCloseEventMethodAdapted'] and report['managerRemovalSimulated']
    assert report['forceCloseOnlySourceContract'] and report['noAccountOrNetworkAction']
    assert not report['actualUiManagerInvoked']
    assert report['unityVersion'] == '2019.4.28f1c1'
    assert (report['width'], report['height'], report['cameraPixelWidth'], report['cameraPixelHeight']) == (1440, 810, 1440, 810)
    assert report['inputSha256'] == sha(artifact / 'ReferenceInputs.json') == sha(PROJECT / 'ReferenceInputs.json')
    for key, filename, current in (
        ('sourceResearchInputsSha256', 'SourceResearchInputs.json', PROJECT / 'SourceResearchInputs.json'),
        ('animationDataSha256', 'AnimationData.json', ROOT / 'Saved/RokUnityMigration/ResearchAnimationNative/animation_data.json'),
    ):
        assert inputs[key] == report[key] == sha(artifact / filename) == sha(current), key
    assert inputs['rendererSha256'] == report['rendererSha256'] == sha(ROOT / 'Scripts/RenderRokResearchMainMaskNative.cs')
    assert inputs['prepareSha256'] == sha(ROOT / 'Scripts/PrepareRokResearchMainMaskNative.py')
    assert inputs['viewBinderAdapterSha256'] == report['viewBinderAdapterSha256'] == sha(PROJECT / 'Assets/ReferenceComponents/ViewBinder.cs')
    assert inputs['uiRootPrefab'] == source['uiRootPrefab'] and inputs['mainPrefab'] == source['mainPrefab']
    for item in inputs['sourceFiles']:
        assert sha(ROOT / item['path']) == item['sha256'], item['path']
    rule = inputs['maskRule']
    assert (rule['sourceMethod'], rule['status'], rule['sourceUi'], rule['layer']) == (
        'UIManager.AddMask', 'kTouchClose', 'UI.s_ResearchMain', 'WindowLayer')
    assert rule['color'] == {'r':0, 'g':0, 'b':0, 'a':.8}
    assert rule['recreatedFromSourceRule'] and not rule['actualUiManagerInvoked']
    assert inputs['forceCloseBoundary']['call'] == 'CloseUI(UI.s_ResearchMain, true, true)'
    assert inputs['forceCloseBoundary']['forceRemovesWithoutAnimatorClose']
    assert not inputs['forceCloseBoundary']['simulatedInAnimationSamples']
    ui_source = (ROOT / 'ref/client-unity/Assets/Scripts/Hotfix/MVC/UI.cs').read_text(encoding='utf-8')
    manager_source = (ROOT / 'ref/client-unity/Assets/Skyunion/RunTime/UIManager/UIManager.cs').read_text(encoding='utf-8')
    mediator_source = (ROOT / 'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchMainMediator.cs').read_text(encoding='utf-8')
    assert 's_ResearchMain' in ui_source and 'EnumMaskStatus.kTouchClose' in ui_source
    assert 'maskImg.color = new Color(0, 0, 0, 0.8f)' in manager_source
    assert 'CloseUI(uiInfo)' in manager_source
    assert 'CloseUI(UI.s_ResearchMain,true,true)' in mediator_source
    assert animation['schema'] == 'rok.research.animation_data.v2'
    on, off = animation['clips'][0], animation['clips'][3]
    assert (on['stateName'], on['durationSeconds'], off['stateName'], off['durationSeconds']) == ('Open', .25, 'Close', .15)
    assert off['events'] == [{'time': .15, 'functionName': 'CloseAnimatorCallback'}]
    for clip in (on, off):
        assert all(c['relativeClipPath'] == '' and c['interpolation'] == 'cubicHermiteUnweighted'
                   for c in clip['curves'])
        assert all(k['weightedMode'] == 0 and k['inSlope'] == 0 and k['outSlope'] == 0
                   for c in clip['curves'] for k in c['keyframes'])
    samples = report['samples']
    assert len(samples) == 11 and Counter(s['phase'] for s in samples) == {
        PHASES[0]: 5, PHASES[1]: 5, 'main_removed_after_callback': 1}
    assert len({s['key'] for s in samples}) == len(samples)
    assert [s['phase'] for s in samples] == [p for p in PHASES for _ in TIMES] + ['main_removed_after_callback']
    for phase in PHASES:
        rows = [s for s in samples if s['phase'] == phase]
        for s, expected_time in zip(rows, TIMES):
            time = s['elapsedSeconds']
            near(time, expected_time, 1e-6, (phase, 'time'))
            assert s['key'] == phase + '_t' + str(round(expected_time*10000)).zfill(4)
            assert s['animatorState'] == 'Close' and s['mainExists'] and s['maskExists']
            assert not s['simulatedRemoval'] and not s['actualUiManagerInvoked']
            assert s['sourceButtonPointerClick'] and s['maskHasButton'] and s['maskNullSprite']
            assert s['maskBelowMain'] and s['maskRaycastTarget']
            for channel, value in (('r',0),('g',0),('b',0),('a',.8)):
                near(s['maskImageColor'][channel], value, 1e-5, (phase,time,'mask color',channel))
            assert s['closeStartCount'] == 1
            assert s['maskPointerClicks'] == (2 if 0 < expected_time < .15 else 1)
            assert s['duplicateCloseIgnoredCount'] == s['maskPointerClicks'] - 1
            assert s['closeAnimatorEventCount'] == s['managerCallbackCount']
            if time < .15:
                assert s['isAniClosing'] and not s['managerRemovalRequested']
                assert s['closeAnimatorEventCount'] == 0
            else:
                assert not s['isAniClosing'] and s['managerRemovalRequested']
                assert s['closeAnimatorEventCount'] == 1
            for axis in ('x','y','z'):
                near(s['mainWindow']['localScale'][axis], curve(off,'m_LocalScale.'+axis,time), .003,
                     (phase,time,'main scale',axis))
            near(s['mainWindow']['canvasGroupAlpha'], curve(off,'m_Alpha',time), .005,
                 (phase,time,'main alpha'))
            near(s['normalizedTime'], time/.15, .02, (phase,time,'normalizedTime'))
            expected_open = .05 if phase == 'mask_close_interrupt_open' else .25
            near(s['outgoingOpenScale'], curve(on,'m_LocalScale.x',expected_open), .003,
                 (phase,time,'outgoing Open scale'))
            mask_rect = s['mask']['screenRect']
            for key, value in (('x',0),('y',0),('width',1440),('height',810)):
                near(mask_rect[key], value, .75, (phase,time,'mask geometry',key))
            assert s['mask']['activeInHierarchy'] and s['mainWindow']['activeInHierarchy']
            for key in ('mainWindow','mask'):
                rect = s[key]['screenRect']
                assert all(math.isfinite(rect[v]) and rect[v] >= 0 if v in ('width','height') else math.isfinite(rect[v])
                           for v in ('x','y','width','height'))
    removed = samples[-1]
    assert removed['key'] == removed['phase'] == 'main_removed_after_callback'
    assert not removed['mainExists'] and not removed['maskExists'] and removed['simulatedRemoval']
    assert removed['managerRemovalRequested'] and removed['closeAnimatorEventCount'] == removed['managerCallbackCount'] == 1
    for s in samples:
        assert s['png'] == s['key'] + '.png'
        image = artifact / s['png']
        assert image.is_file() and sha(image) == s['sha256']
        assert png_dimensions(image) == (1440,810)
    verified = {'schema':'rok.research_main_mask.native_verified.v1', 'complete':True,
                'artifact':artifact.relative_to(ROOT).as_posix(),
                'renderEvidenceSha256':sha(report_path), 'inputSha256':sha(artifact/'ReferenceInputs.json'),
                'sampleCount':len(samples), 'phaseCounts':dict(Counter(s['phase'] for s in samples)),
                'pngSha256':{s['key']:s['sha256'] for s in samples},
                'actualUiManagerInvoked':False, 'managerRemovalSimulated':True}
    path = PACKAGE / 'verified_capture.json'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(verified,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'verified':True,'artifact':str(artifact),'evidenceSha256':verified['renderEvidenceSha256'],
                      'verifiedPath':str(path),'sampleCount':len(samples)},ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('artifact',type=Path)
    verify(parser.parse_args().artifact)
