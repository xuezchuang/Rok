"""Extract source Research detail animation curves for an isolated Unity probe.

The reference checkout is read only. This producer does not start an engine.
"""
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / 'ref/client-unity'
OUT = ROOT / 'Saved/RokUnityMigration/ResearchAnimationNative/animation_data.json'
ANIM = 'Assets/BundleAssets/UI/UIAnimation/shared_uianimation/'
PREFAB = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIResearch/UI_Win_ResearchUpate.prefab'
MEDIATOR = 'Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchUpateMediator.cs'
UTIL = 'Assets/Scripts/Client/Utils/ClientUtils.cs'
MANAGER = 'Assets/Skyunion/RunTime/UIManager/UIManager.cs'
BINDER = 'Assets/Skyunion/RunTime/UIManager/ViewBinder.cs'
UI_REGISTRY = 'Assets/Scripts/Hotfix/MVC/UI.cs'
MAIN_MEDIATOR = 'Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchMainMediator.cs'
CLIPS = [
    ('detail_open', 'UA_WinAnime_T1', 'UA_WinAnime_T1_On', '.', 'Open'),
    ('power_show_128', 'UA_BuildingDesc_Level', 'UA_BuildingDesc_LevelShow', 'rect/pl_power', 'Show'),
    ('detail_back_128', 'UA_BuildingDesc_Content', 'UA_BuildingDesc_ContentShow', 'rect/img_update', 'Show'),
    ('detail_close', 'UA_WinAnime_T1', 'UA_WinAnime_T1_Off', '.', 'Close'),
]
NUM = r'[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source(path):
    p = CLIENT / path
    assert p.is_file(), p
    return {'path': p.relative_to(ROOT).as_posix(), 'sha256': sha(p)}


def scalar_keys(block):
    keys = []
    for match in re.finditer(r'(?m)^\s+- serializedVersion: 3\s*\n(?P<fields>(?:\s{8,}[^\n]+\n)+)', block):
        fields = match.group('fields')
        def field(name):
            m = re.search(r'(?m)^\s+' + name + r':\s*(' + NUM + r')\s*$', fields)
            assert m, (name, fields)
            return float(m.group(1))
        keys.append({'time': field('time'), 'value': field('value'),
                     'inSlope': field('inSlope'), 'outSlope': field('outSlope'),
                     'tangentMode': int(field('tangentMode')),
                     'weightedMode': int(field('weightedMode')),
                     'inWeight': field('inWeight'), 'outWeight': field('outWeight')})
    return keys


def extract_scalar_curves(text):
    # Unity's m_EditorCurves stores scalar bindings even when the runtime clip
    # stores the same XY values in one m_ScaleCurves vector curve.
    section = text.split('  m_EditorCurves:', 1)[1].split('  m_EulerEditorCurves:', 1)[0]
    result = []
    for part in section.split('  - curve:\n')[1:]:
        m = re.search(r'(?m)^    attribute: (.+)\s*$', part)
        if not m:
            continue
        attr = m.group(1).strip()
        path_match = re.search(r'(?m)^    path:[ \t]*([^\r\n]*)$', part)
        class_match = re.search(r'(?m)^    classID:\s*(\d+)', part)
        assert path_match and class_match
        keys = scalar_keys(part.split('    attribute:', 1)[0])
        assert len(keys) >= 2, (attr, keys)
        assert all(k['weightedMode'] == 0 and k['inSlope'] == 0 and k['outSlope'] == 0 for k in keys)
        result.append({'propertyPath': attr, 'relativeClipPath': path_match.group(1).strip(),
                       'component': {'4': 'Transform', '224': 'RectTransform', '225': 'CanvasGroup'}[class_match.group(1)],
                       'coordinateSpace': 'localScale' if attr.startswith('m_LocalScale') else
                       ('anchoredPosition' if attr.startswith('m_AnchoredPosition') else 'unitlessAlpha'),
                       'interpolation': 'cubicHermiteUnweighted', 'keyframes': keys})
    return result


def evaluate(keys, time):
    if time <= keys[0]['time']:
        return keys[0]['value']
    if time >= keys[-1]['time']:
        return keys[-1]['value']
    for left, right in zip(keys, keys[1:]):
        if left['time'] <= time <= right['time']:
            dt = right['time'] - left['time']
            u = (time - left['time']) / dt
            return ((2*u**3-3*u**2+1)*left['value'] + (u**3-2*u**2+u)*dt*left['outSlope'] +
                    (-2*u**3+3*u**2)*right['value'] + (u**3-u**2)*dt*right['inSlope'])
    raise AssertionError(time)


def main():
    clips = []
    provenance = [source(p) for p in (PREFAB, MEDIATOR, UTIL, MANAGER, BINDER, UI_REGISTRY, MAIN_MEDIATOR)]
    for phase, controller, clip, target, state in CLIPS:
        anim_path = ANIM + clip + '.anim'
        controller_path = ANIM + controller + '.controller'
        text = (CLIENT / anim_path).read_text(encoding='utf-8-sig')
        duration = float(re.search(r'm_StopTime:\s*(' + NUM + r')', text).group(1))
        loop = int(re.search(r'm_LoopTime:\s*(\d+)', text).group(1))
        sample_rate = float(re.search(r'm_SampleRate:\s*(' + NUM + r')', text).group(1))
        curves = extract_scalar_curves(text)
        assert duration == (.15 if phase == 'detail_close' else .25) and loop == 0 and sample_rate == 60
        expected = {'detail_open': {'m_LocalScale.x', 'm_LocalScale.y'},
                    'power_show_128': {'m_AnchoredPosition.x', 'm_Alpha'},
                    'detail_back_128': {'m_AnchoredPosition.x', 'm_Alpha'},
                    'detail_close': {'m_LocalScale.x', 'm_LocalScale.y', 'm_LocalScale.z', 'm_Alpha'}}[phase]
        assert {c['propertyPath'] for c in curves} == expected, (phase, curves)
        assert all(c['relativeClipPath'] == '' for c in curves)
        if phase == 'detail_open':
            assert [k['time'] for k in curves[0]['keyframes']] == [0, .11666667, .25]
        if phase == 'detail_close':
            assert duration == .15
        times = [0, .05, .1, .15, .2] if phase == 'detail_close' else [0, .05, .1, .15, .2, .25, .3]
        if phase == 'detail_open':
            times.insert(3, .11666667)
        events = [{'time':float(t), 'functionName':fn} for t,fn in
                  re.findall(r'(?m)^  - time: (' + NUM + r')\s*\n    functionName: ([A-Za-z_]+)',
                             text.split('  m_Events:',1)[1] if '  m_Events:' in text else '')]
        if phase == 'detail_close':
            assert events == [{'time':.15,'functionName':'CloseAnimatorCallback'}], events
        else:
            assert not events, (phase,events)
        clips.append({'phase': phase, 'controller': controller_path, 'clip': anim_path,
                      'animatorTargetPath': target, 'stateName': state, 'durationSeconds': duration,
                      'sampleRate': sample_rate, 'loop': False, 'wrapMode': 'clamp', 'events':events,
                      'curves': curves, 'sampleTimesSeconds': times,
                      'predictedSamples': [{'elapsedSeconds': t,
                                            'values': {c['propertyPath']: evaluate(c['keyframes'], t) for c in curves}}
                                           for t in times]})
        for p in (controller_path,anim_path):
            row=source(p)
            if row not in provenance:
                provenance.append(row)
    mediator = (CLIENT / MEDIATOR).read_text(encoding='utf-8-sig', errors='replace')
    util = (CLIENT / UTIL).read_text(encoding='utf-8-sig', errors='replace')
    assert 'PlayUIAnimation(view.m_pl_power_Animator,"Show")' in mediator.replace(' ', '')
    assert 'PlayUIAnimation(view.m_img_update_Animator,"Show")' in mediator.replace(' ', '')
    assert 'animator.Play(strName)' in util
    manager=(CLIENT/MANAGER).read_text(encoding='utf-8-sig',errors='replace')
    binder=(CLIENT/BINDER).read_text(encoding='utf-8-sig',errors='replace')
    main=(CLIENT/MAIN_MEDIATOR).read_text(encoding='utf-8-sig',errors='replace')
    assert 'CoreUtils.uiManager.CloseUI(UI.s_ResearchUpdate)' in mediator
    assert 'animator.Play("Close", -1, 0)' in manager and 'uiInfo.gameView.IsAniCloseing = true' in manager
    assert 'onCloseAnimatorCallback()' in binder and 'IsAniCloseing = false' in binder
    assert 'CloseUI(UI.s_ResearchMain,true,true)' in main.replace(' ','')
    assert len({p['path'] for p in provenance}) == len(provenance)
    report = {'schema': 'rok.research.animation_data.v2',
              'scope': 'Source clip/controller extraction and explicit isolated all-unresearched StudyType128 presentation; no GameApp/login or real account progression.',
              'prefab': PREFAB, 'sourceFiles': provenance,
              'sourceActions': {
                  'detail_open': 'Root UI_Win_ResearchUpate Animator default Open; img_update child default Empty, no child Show on initial entry.',
                  'power_show_128': 'onMore disables img_update, enables pl_power, calls ClientUtils.PlayUIAnimation(pl_power,"Show"), enables title back.',
                  'detail_back_128': 'OnBack enables img_update, disables pl_power, calls ClientUtils.PlayUIAnimation(img_update,"Show"), disables title back.',
                  'interrupt': 'The outgoing subtree is disabled immediately; the incoming Show restarts via Animator.Play without a reverse clip or CrossFade.',
                  'detail_close': 'ResearchUpateMediator.CloseRes calls CloseUI(s_ResearchUpdate) with default IsForceClose=false. UIManager checks IsAniCloseing, installs onCloseAnimatorCallback=>OnCloseUI, plays root Close at normalized time 0 and speed 1. Root Close preempts root Open while active child Show can continue independently until removal.',
                  'detail_close_event': 'UA_WinAnime_T1_Off emits CloseAnimatorCallback at 0.15s. ViewBinder invokes manager callback, then clears IsAniCloseing. ResearchUpdate PopWin OnCloseUI calls RemoveViewFromLayer and GameObject.Destroy(uiObj).',
                  'main_close': 'ResearchMainMediator.CloseRes invokes CloseUI(s_ResearchMain,true,true), bypassing its Animator. This separate main close is not a detail Close clip.'},
              'clips': clips,
              'notes': ['Source .anim keys and controller topology are authoritative; isolated Animator samples will validate actual evaluated values.',
                        'All clip binding paths are empty relative to the animator target; XY localScale and anchoredPosition are Unity local transform coordinates.']}
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'path': str(OUT), 'sha256': sha(OUT), 'clipCount': len(clips),
                      'bindingCount': sum(len(c['curves']) for c in clips)}, ensure_ascii=False))


if __name__ == '__main__':
    main()
