"""Strict verifier for the original UIRoot, PlayMode Animator, and parent-window screen geometry."""
import hashlib
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ARTIFACTS = ROOT / 'Saved/UnityBarracksParentWindowProbe/Artifacts'
REQUIRED = {'UIRoot', 'Container', 'WindowLayer', 'UI_Win_BuildingUpdate',
            'UI_Model_Window_Type1', 'win', 'frame1', 'frame2', 'frame3', 'frame4',
            'title', 'lbl_title', 'pl_upgrade', 'UI_Item_BuildingUpgrade',
            'img_resBg', 'pl_btns'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(path, output=None):
    path = Path(path).resolve()
    assert path.name == 'render_evidence.json' and path.parent.parent == ARTIFACTS.resolve()
    evidence = json.loads(path.read_text(encoding='utf-8-sig'))
    inputs_path = path.parent / 'ReferenceInputs.json'
    inputs = json.loads(inputs_path.read_text(encoding='utf-8-sig'))
    failures = []

    def check(ok, reason):
        if not ok:
            failures.append(reason)

    check(evidence.get('complete') and not evidence.get('failure'), 'native run incomplete')
    check(inputs.get('probeVersion') == 2, 'old diagnostic producer is not accepted as v2')
    check(evidence.get('playMode') and evidence.get('nativeAnimatorEnabled'), 'original Animator did not run in PlayMode')
    check(evidence.get('projectionInitialized'), 'camera projection not initialized before screen geometry')
    check(evidence.get('unityVersion') == '2019.4.28f1c1', 'wrong Unity version')
    check(evidence.get('inputSha256') == digest(inputs_path), 'input hash mismatch')
    check(evidence.get('fixture') == 'source_prefab_presentation_only', 'fixture scope changed')
    check(evidence.get('nativeCameraClearFlags') == 'Depth', 'original UIRoot camera clear flags changed')
    check(evidence.get('clearPolicy') == 'GL.Clear(true,true,Color.clear)', 'isolated target clear policy missing')
    check(inputs.get('renderPolicy', {}).get('clearTargetBeforeEachCameraRender') == evidence.get('clearPolicy'),
          'render policy differs from v2 input')
    setup_path = path.parent / 'setup_evidence.json'
    check(setup_path.is_file(), 'Chinese presentation setup evidence missing')
    if setup_path.is_file():
        setup = json.loads(setup_path.read_text(encoding='utf-8-sig'))
        check(evidence.get('setupSha256') == digest(setup_path), 'setup evidence hash mismatch')
        check(evidence.get('layoutFlagsApplied') == setup.get('layoutFlagsApplied'), 'runtime layout trace differs from setup')
        check(evidence.get('languageBindings') == setup.get('languageBindings'), 'runtime language trace differs from setup')
        check(evidence.get('fixtureTitleApplied') == setup.get('fixtureTitleApplied') == inputs.get('fixtureTitle'),
              'source-shaped Chinese barracks title fixture mismatch')
    flags = evidence.get('layoutFlagsApplied') or []
    check(len(flags) > 0, 'source ArabLayout controls not processed')
    check(len({row.get('path') for row in flags}) == len(flags), 'ArabLayout flags applied more than once')
    for row in flags:
        mask = row.get('flags', -1)
        check(mask >= 0 and mask & ~635 == 0, 'unsupported ArabLayout source flag')
        check(row.get('arabicOnly') == bool(mask & 1) and row.get('applied') == (mask & 1 == 0),
              'Chinese ArabLayout semantics incorrect')
    text_by_id = {row['id']: row['text'] for row in inputs.get('texts') or []}
    bindings = evidence.get('languageBindings') or []
    check(len(bindings) > 0, 'no source LanguageText bound')
    check(any(any('\u4e00' <= c <= '\u9fff' for c in row.get('text', '')) for row in bindings),
          'Chinese source text absent from bound controls')
    for row in bindings:
        check(row.get('text') == text_by_id.get(row.get('id')) and bool(row.get('path')),
              'bound LanguageText differs from source Language.bin')
    for source in inputs['sources']:
        check(digest(ROOT / source['path']) == source['sha256'], 'source changed: ' + source['path'])
    samples = evidence.get('samples') or []
    check(len(samples) == 4, 'four actual Animator samples required')
    check(evidence.get('clearCount', 0) >= len(samples), 'capture target was not cleared before every sample')
    labels = ['first_rendered_frame', 'at_or_after_7f', 'at_or_after_0_25s', 'at_or_after_0_5s']
    thresholds = inputs['sampleThresholdsSeconds']
    for i, sample in enumerate(samples):
        check(sample.get('label') == labels[i], f'sample {i} label')
        elapsed = sample.get('elapsedGameTime', -1)
        check(elapsed >= thresholds[i] - 0.001, f'sample {i} before game-time threshold')
        check(sample.get('elapsedRealtime', -1) >= 0 and sample.get('gameDeltaTime', -1) >= 0,
              f'sample {i} missing actual timing')
        if i:
            check(elapsed > samples[i - 1].get('elapsedGameTime', 0), f'sample {i} game time not increasing')
            check(sample.get('unityFrame', 0) > samples[i - 1].get('unityFrame', -1), f'sample {i} frame not increasing')
        check(sample.get('stateName') == 'Open', f'sample {i} did not evaluate original Open state')
        check(sample.get('animatorStateLength', 0) > 0, f'sample {i} missing source controller state length')
        if i == 1:
            check(sample.get('unityFrame', -1) >= 7 and sample.get('animatorNormalizedTime', -1) >= 7 / 15 - 0.01,
                  '7f sample before actual Animator phase')
        if i >= 2:
            check(sample.get('animatorNormalizedTime', -1) >= 0.99, f'sample {i} before native Open terminal phase')
        filename = sample.get('png', '')
        image = path.parent / filename
        check(image.parent == path.parent and image.is_file(), f'sample {i} PNG missing')
        if image.is_file():
            check(digest(image) == sample.get('sha256'), f'sample {i} PNG hash')
            from PIL import Image
            from PIL import ImageStat
            frame = Image.open(image).convert('RGBA')
            check(frame.size == (1440, 810), f'sample {i} PNG dimensions')
            check(max(ImageStat.Stat(frame).stddev[:3]) > 3, f'sample {i} image appears blank')
        nodes = sample.get('nodes') or []
        names = {node['path'].split('/')[-1] for node in nodes}
        check(REQUIRED <= names, f'sample {i} source controls/ancestors missing: {sorted(REQUIRED - names)}')
        for node in nodes:
            quad = node.get('screenQuad') or []
            rect = node.get('screenRect') or {}
            check(len(quad) == 4, f'sample {i} {node.get("path")} screen quad')
            if len(quad) == 4:
                bounds = [min(p['x'] for p in quad), min(p['y'] for p in quad),
                          max(p['x'] for p in quad) - min(p['x'] for p in quad),
                          max(p['y'] for p in quad) - min(p['y'] for p in quad)]
                check(all(math.isfinite(v) for v in bounds), f'sample {i} nonfinite geometry')
                check(all(abs(rect.get(key, float('inf')) - value) < 0.01 for key, value in zip(('x', 'y', 'width', 'height'), bounds)),
                      f'sample {i} screen rect not derived from quad')
    if len(samples) == 4:
        final = samples[-1]
        check(abs(final.get('rootScale', -1) - 1) < 0.02, 'Open terminal scale must return to one')
        check(final.get('elapsedGameTime', 0) >= 0.5, 'terminal sample earlier than source animation end')
        nodes = final.get('nodes') or []
        def one(name):
            found = [n for n in nodes if n['path'].split('/')[-1] == name]
            check(len(found) == 1, f'expected one {name}, found {len(found)}')
            return found[0] if len(found) == 1 else {}
        layer = one('WindowLayer')
        check(layer.get('anchorMin') == {'x': 0, 'y': 0} and layer.get('anchorMax') == {'x': 1, 'y': 1},
              'source WindowLayer must full-stretch')
        check(layer.get('sizeDelta') == {'x': 0, 'y': 0}, 'source WindowLayer offsets changed')
        nested = one('UI_Model_Window_Type1').get('screenRect') or {}
        expected = inputs['referenceGeometry']['nested_type2']
        check(all(abs(nested.get(key, float('inf')) - value) <= 2 for key, value in zip(('x', 'y', 'width', 'height'), expected)),
              'nested Type1-named/Type2-GUID source window does not end at centered 1120x700')
        check(one('lbl_title').get('text') == inputs.get('fixtureTitle'), 'final visible window title is not Chinese source fixture')
        for control in ('win', 'frame1', 'frame2', 'frame3', 'frame4', 'title', 'lbl_title', 'pl_upgrade', 'img_resBg', 'pl_btns'):
            matches = [n for n in nodes if n['path'].split('/')[-1] == control and n.get('visible')]
            check(bool(matches), f'final source {control} not visible')
    result = {'passed': not failures, 'failures': failures, 'evidence_sha256': digest(path),
              'scope': 'Native parent hierarchy and original Animator geometry only; presentation fixture, no authoritative gameplay or UE parity.'}
    destination = Path(output) if output else path.parent / 'verify.json'
    destination.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence')
    parser.add_argument('--output')
    args = parser.parse_args()
    result = verify(args.evidence, args.output)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    sys.exit(0 if result['passed'] else 1)
