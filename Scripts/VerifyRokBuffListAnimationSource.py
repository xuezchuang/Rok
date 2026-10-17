"""Verify the Unity source contract used by BuffList's child-only Pop playback.

This checks source identity/curves, not UE runtime or visual acceptance.
Run Rok.Ui.Animation.BuffList.ChildPopLifecycle separately in UE 5.8.
"""
import json
from pathlib import Path
import re


def verify(root):
    ui = root / 'ref/client-unity/Assets/BundleAssets/UI'
    prefab_path = ui / 'UIPrefabs/ui_prefabs_build/UICityManager/UI_Pop_BuffList.prefab'
    animation_dir = ui / 'UIAnimation/shared_uianimation'
    prefab = prefab_path.read_text(encoding='utf-8-sig')
    blocks = dict(re.findall(r'--- !u!\d+ &(\d+)\n(.*?)(?=\n--- !u!|\Z)', prefab, re.S))
    failures = []

    def require(condition, message):
        if not condition:
            failures.append(message)

    position_ids = [key for key, body in blocks.items() if '  m_Name: pl_pos\n' in body]
    require(len(position_ids) == 1, 'Exactly one pl_pos GameObject is required')
    controller = animation_dir / 'UA_PopAnime.controller'
    guid = re.search(r'^guid: (\w+)', Path(str(controller) + '.meta').read_text(), re.M).group(1)
    if position_ids:
        animators = [body for body in blocks.values() if body.startswith('Animator:')
                     and 'm_GameObject: {fileID: ' + position_ids[0] + '}' in body]
        require(len(animators) == 1 and 'guid: ' + guid in animators[0], 'pl_pos must own UA_PopAnime')
    require(sum(body.startswith('Animator:') for body in blocks.values()) == 1,
            'Additional Animator changes the root-only close contract')
    manager_path = root / 'ref/client-unity/Assets/Skyunion/RunTime/UIManager/UIManager.cs'
    manager = manager_path.read_text(encoding='utf-8-sig')
    close = manager.split('public void CloseAnimatorUI(', 1)[1].split('public void CloseLayerUI(', 1)[0]
    require('uiInfo.uiObj.GetComponent<Animator>()' in close
            and re.search(r'if \(animator == null\)\s*\{\s*OnCloseUI\(', close),
            'UIManager must immediately close a screen without a root Animator')
    controller_text = controller.read_text()
    samples = {}
    for suffix, expected in [('On', [(0, 0), (0.11666667, 1.1), (0.25, 1)]),
                             ('Off', [(0, 1), (0.25, 0)])]:
        path = animation_dir / ('UA_PopAnime_' + suffix + '.anim')
        clip = path.read_text()
        clip_guid = re.search(r'^guid: (\w+)', Path(str(path) + '.meta').read_text(), re.M).group(1)
        require('guid: ' + clip_guid in controller_text, suffix + ' clip missing from controller')
        scale = clip.split('  m_ScaleCurves:\n', 1)[1].split('  m_FloatCurves:', 1)[0]
        keys = re.findall(r'time: ([\d.]+)\n\s+value: \{x: ([\d.]+), y: ([\d.]+), z: ([\d.]+)\}', scale)
        actual = [(float(t), float(x)) for t, x, y, z in keys]
        require(actual == expected, suffix + ' scale keys changed')
        require(all(x == y and float(z) == 1 for t, x, y, z in keys), suffix + ' XY/Z semantics changed')
        require(scale.count('inSlope: {x: 0, y: 0, z: 0}') == len(expected)
                and scale.count('outSlope: {x: 0, y: 0, z: 0}') == len(expected), suffix + ' Hermite tangents changed')
        require('  m_FloatCurves: []' in clip, suffix + ' unexpectedly animates opacity')
        require('    path: \n' in scale, suffix + ' must animate the Animator object')
        samples[suffix] = actual
    background = next(body for body in blocks.values() if body.startswith('RectTransform:')
                      and 'm_GameObject: {fileID: 779267890223285826}' in body)
    require('m_AnchoredPosition: {x: 3.3, y: 0}' in background
            and 'm_SizeDelta: {x: 450.3, y: 339.3}' in background
            and 'm_Pivot: {x: 0.9, y: 1}' in background, 'Source relative scale origin changed')
    return {'source_prefab': str(prefab_path.relative_to(root)), 'scale_keys': samples,
            'dismissal': 'immediate: root Animator absent; Off clip not triggered',
            'failures': failures, 'runtime_verified': False, 'visual_verified': False}


if __name__ == '__main__':
    result = verify(Path(__file__).resolve().parents[1])
    print(json.dumps(result, ensure_ascii=False, indent=2))
    raise SystemExit(bool(result['failures']))
