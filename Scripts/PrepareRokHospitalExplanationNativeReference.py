"""Prepare an isolated, source-derived native hospital explanation reference; never launch Unity."""
import hashlib
import json
import re
import shutil
import struct
import subprocess
from pathlib import Path
from rok_data_paths import resolve
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text, LANGUAGE_WORKBOOK

ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / 'ref/client-unity'
TARGET = ROOT / 'Saved/UnityHospitalExplanationNativeReferenceProbe'
LANG = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIHospital/'


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def parse_languages(path):
    data = path.read_bytes()
    pos = 0
    def read(fmt):
        nonlocal pos
        result = struct.unpack_from('<' + fmt, data, pos)[0]
        pos += struct.calcsize('<' + fmt)
        return result
    strings = []
    for _ in range(read('i')):
        length = read('H')
        strings.append(data[pos:pos + length].decode('utf-8'))
        pos += length
    count, cols = read('i'), read('B')
    assert cols == 6
    rows = [[strings[read('i')] for _ in range(cols)] for _ in range(count)]
    assert pos == len(data)
    return rows


def main():
    assert TARGET.resolve().parent == (ROOT / 'Saved').resolve()
    def out(relative):
        path = (TARGET / relative).resolve()
        assert path.is_relative_to(TARGET.resolve())
        path.parent.mkdir(parents=True, exist_ok=True)
        return path
    previous_inputs = TARGET / 'ReferenceInputs.json'
    if previous_inputs.exists():
        snapshot = out('Validation/previous_inputs_' + digest(previous_inputs) + '.json')
        if not snapshot.exists():
            snapshot.write_bytes(previous_inputs.read_bytes())
    sources, copies, adapters = [], [], []
    def record(p):
        row = {'path': p.relative_to(ROOT).as_posix(), 'sha256': digest(p)}
        if row not in sources:
            sources.append(row)
    def copy(p, relative):
        target = out(relative)
        # Existing Hidden Windows files cannot be opened with wb. Never change source attributes.
        if target.exists():
            import stat
            target.chmod(target.stat().st_mode | stat.S_IWRITE)
            with p.open('rb') as src, target.open('r+b') as dst:
                shutil.copyfileobj(src, dst); dst.truncate()
        else:
            shutil.copyfile(p, target)
        assert digest(p) == digest(target)
        record(p)
        copies.append({'source': p.relative_to(ROOT).as_posix(), 'target': relative, 'sha256': digest(p)})

    # Only the relevant UI folders; preserve nested prefab/animation/atlas GUIDs.
    index = {}
    roots = [CLIENT / 'Assets/BundleAssets', CLIENT / 'Assets/Scripts/Client/Compoment', CLIENT / 'Assets/Skyunion/RunTime/UIManager']
    files = subprocess.check_output(['rg', '--files', '--hidden', '--no-ignore', '-g', '*.meta', *map(str, roots)], text=True, encoding='utf-8').splitlines()
    for name in files:
        meta = Path(name)
        match = re.search(r'^guid: (\w+)', meta.read_text(encoding='utf-8-sig', errors='replace')[:150], re.M)
        if match:
            index[match[1]] = Path(str(meta)[:-5])
    native = {'8a8695521f0d02e499659fee002a26c2': 'GridLayoutGroup', '31a19414c41e5ae4aae2af33fee712f6': 'Mask',
              '1aa08ab6e0800fa44ae55d278d1423e3': 'ScrollRect', 'cfabb0440166ab443bba8876756fdfa9': 'Shadow', '3245ec927659c4140ac4f8d17403cc18': 'ContentSizeFitter', '2fafe2cfe61f6974895a912c3755e8f1': 'ToggleGroup'}
    for meta in (CLIENT/'Library/PackageCache/com.unity.ugui@1.0.0/Runtime').rglob('*.cs.meta'):
        guid=re.search(r'^guid: (\w+)',meta.read_text(encoding='utf-8-sig'),re.M)
        if guid:native[guid[1]]=meta.name[:-8]
    queue = [CLIENT / (LANG + 'UI_Win_Hospital.prefab')]
    seen, script_paths, all_text = set(), {}, ''
    while queue:
        p = queue.pop()
        if p in seen:
            continue
        seen.add(p)
        if p.suffix == '.cs':
            script_paths[p.stem] = p
            record(p)
            continue
        relative = p.relative_to(CLIENT).as_posix()
        copy(p, relative)
        copy(Path(str(p) + '.meta'), relative + '.meta')
        if p.suffix in {'.prefab', '.controller', '.anim', '.mat'}:
            text = p.read_text(encoding='utf-8-sig')
            all_text += text
            for guid in set(re.findall(r'guid: (\w+)', text)):
                if guid in index:
                    queue.append(index[guid])
                elif guid not in native and not guid.startswith('000000'):
                    raise RuntimeError('Unresolved source dependency ' + guid)
    expected = {'LanguageText', 'ArabLayoutCompment', 'PolygonImage', 'GameToggle', 'GameButton', 'BtnAnimation', 'ListView', 'ViewBinder', 'GameSlider', 'UIParticleOrder'}
    assert set(script_paths) <= expected, set(script_paths)
    arab_inventory = []
    for p in sorted(seen):
        if p.suffix != '.prefab':
            continue
        text = p.read_text(encoding='utf-8-sig')
        for match in re.finditer(r'_arab_Layout: (\d+)|propertyPath: _arab_Layout\s+value: (\d+)', text):
            arab_inventory.append({'prefab': p.relative_to(CLIENT).as_posix(),
                'line': text[:match.start()].count('\n') + 1,
                'flags': int(match[1] or match[2]), 'nested_override': match[2] is not None})
    assert all((r['flags'] & ~635) == 0 for r in arab_inventory)
    codes = {
        'LanguageText': '''using UnityEngine;
namespace GameFramework { public static class ReferenceMarker {} }
namespace UnityEngine.UI
{
    public class LanguageText : Text
    {
        [SerializeField] private int m_languageId = -1;
        public int SourceLanguageId { get { return m_languageId; } }
        public override string text
        {
            get
            {
                return ArabicSupport.ArabicFixer.IsRtl(base.text)
                    ? ArabicSupport.ArabicFixer.FixRtl(ArabicSupport.ArabicFixer.FixWithoutRtl(base.text, false, false, false), false, false, false)
                    : base.text;
            }
            set { base.text = value; }
        }
    }
}
''',
        'ArabLayoutCompment': 'using UnityEngine; public class ArabLayoutCompment : MonoBehaviour { [SerializeField] private int _arab_Layout; public int SourceFlags {get{return _arab_Layout;}} }',
        'GameToggle': 'public class GameToggle : UnityEngine.UI.Toggle {}',
        'GameButton': 'public class GameButton : UnityEngine.UI.Button {}',
        'GameSlider': 'public class GameSlider : UnityEngine.UI.Slider {}',
        'BtnAnimation': 'public class BtnAnimation : UnityEngine.MonoBehaviour {}',
        'ListView': 'public class ListView : UnityEngine.MonoBehaviour {}',
        'ViewBinder': 'public class ViewBinder : UnityEngine.MonoBehaviour {}',
    }
    if 'UIParticleOrder' in script_paths:
        codes['UIParticleOrder'] = script_paths['UIParticleOrder'].read_text(encoding='utf-8-sig').replace('using Skyunion;', '')
    for name, p in script_paths.items():
        relative = 'Assets/ReferenceComponents/' + name + '.cs'
        copy(Path(str(p) + '.meta'), relative + '.meta')
        if name == 'PolygonImage':
            copy(p, relative)
        else:
            target = out(relative)
            target.write_text(codes[name] + '\n', encoding='utf-8')
            adapters.append({'class': name, 'target': relative, 'sha256': digest(target), 'source': p.relative_to(ROOT).as_posix(),
                             'scope': 'Native visual base or inert service adapter; explicit source-driven renderer supplies presentation only.'})
    binary = CLIENT / 'Assets/StreamingAssets/Config/Bin/Language.bin'
    language = {int(row[0]): row[1] for row in parse_languages(binary)}
    ids = set(int(n) for n in re.findall(r'm_languageId: (-?\d+)', all_text) if int(n) > 0)
    ids |= set(map(int, re.findall(r'propertyPath: m_languageId\s+value: (\d+)', all_text)))
    ids.add(181001)
    entries = [{'id': i, 'text': language[i]} for i in sorted(ids)]
    explanation = language[181001]
    assert explanation.count('<color=yellow>') == 13 and '\n7.' in explanation
    arabic = CLIENT / 'Assets/Scripts/Client/Compoment/UIExtend/ArabicSupport.cs'
    copy(arabic, 'Assets/ReferenceComponents/ArabicSupport.cs')
    copy(Path(str(arabic) + '.meta'), 'Assets/ReferenceComponents/ArabicSupport.cs.meta')
    mediator = CLIENT / 'Assets/Scripts/Hotfix/MVC/View_Mediator/Hospital/HospitalMediator.cs'
    for source in [binary, mediator, CLIENT / 'Assets/Scripts/Client/Utils/LanguageUtils.cs', Path(__file__).resolve()]: record(source)
    renderer = ROOT / 'Scripts/RenderRokHospitalExplanationNativeReference.cs'
    copy(renderer, 'Assets/Editor/' + renderer.name)
    manifest = {'scope': 'Original hospital prefab rendered by native UGUI; explicit presentation bindings reproduce OnExplain/OnBack. No full mediator, treatment, owned wounded units, server state or physical pointer claim.',
        'prefab': LANG + 'UI_Win_Hospital.prefab', 'width': 1440, 'height': 810, 'texts': entries,
        'sources': sources, 'byte_identical_copies': copies, 'adapters': adapters, 'sourceArabFlagInventory': arab_inventory,
        'contract': {'language_id': 181001, 'yellow_spans': 13, 'height': 'preferredHeight - anchoredPosition.y', 'same_instance_reopen': 'preserves scroll', 'destroy_and_reinstantiate': 'prefab initial scroll; new first-explanation measurement'},
        'remaining_gates': ['Unity import/render not executed by prepare', 'No full service lifecycle; reopen explicitly creates a fresh original prefab']}
    out('ReferenceInputs.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    out('Packages/manifest.json').write_text(json.dumps({'dependencies': {k:'1.0.0' for k in ['com.unity.ugui','com.unity.modules.ui','com.unity.modules.imageconversion','com.unity.modules.jsonserialize','com.unity.modules.imgui','com.unity.modules.animation','com.unity.modules.particlesystem']}}),encoding='utf-8')
    out('ProjectSettings/ProjectVersion.txt').write_text('m_EditorVersion: 2019.4.28f1c1\n',encoding='utf-8')
    # Compiler-only check; never starts the Unity editor. The original UGUI assembly
    # becomes available after package import (or the already prepared Graphics probe).
    unity = Path('C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data')
    ugui = TARGET / 'Library/ScriptAssemblies/UnityEngine.UI.dll'
    if not ugui.exists():
        ugui = ROOT / 'Saved/UnityGraphicsNativeReferenceProbe/Library/ScriptAssemblies/UnityEngine.UI.dll'
    if not ugui.exists():
        raise RuntimeError('Syntax check requires the previously imported native UGUI assembly')
    refs = list((unity / 'Managed/UnityEngine').glob('*.dll'))
    refs += list((unity / 'MonoBleedingEdge/lib/mono/4.7.1-api').glob('*.dll'))
    refs += list((unity / 'MonoBleedingEdge/lib/mono/4.7.1-api/Facades').glob('*.dll'))
    refs += [unity / 'Managed/UnityEditor.dll', ugui]
    args = [str(unity / 'Tools/Roslyn/csc.exe'), '/nologo', '/target:library', '/define:UNITY_EDITOR',
            '/out:' + str(out('Validation/ReferenceSyntax.dll'))]
    args += ['/reference:' + str(p) for p in refs]
    args += [str(p) for p in (TARGET / 'Assets/ReferenceComponents').glob('*.cs')]
    args += [str(TARGET / 'Assets/Editor/RenderRokHospitalExplanationNativeReference.cs')]
    response = out('Validation/compiler.rsp')
    response.write_text('\n'.join('"' + arg + '"' for arg in args[1:]), encoding='utf-8')
    checked = subprocess.run([args[0], '@' + str(response)], capture_output=True)
    out('Validation/compiler.log').write_bytes(checked.stdout + checked.stderr)
    out('Validation/compiler_result.json').write_text(json.dumps({'exit_code': checked.returncode,
        'unity_started': False, 'compiler': str(unity / 'Tools/Roslyn/csc.exe'),
        'scope': 'All generated adapters, original ArabicSupport/PolygonImage and renderer compiled against Unity2019 assemblies; no engine launch.'}), encoding='utf-8')
    if checked.returncode:
        raise RuntimeError('Reference C# compilation failed; see Validation/compiler.log')
    print(json.dumps({'prepared':True,'unity_started':False,'copies':len(copies),'adapters':len(adapters)}))

def verify_native(evidence_path):
    """Verify an already-rendered isolated result; never launch an engine."""
    from PIL import Image
    import numpy as np
    evidence_path = Path(evidence_path).resolve()
    assert evidence_path.is_relative_to((TARGET / 'Artifacts').resolve())
    report = json.loads(evidence_path.read_text(encoding='utf-8-sig'))
    inputs_path = evidence_path.parent / 'ReferenceInputs.json'
    inputs = json.loads(inputs_path.read_text(encoding='utf-8-sig'))
    assert report['nativeRendered'] and not report.get('failure') and not report['fullMediator'] and not report['sourcePrefabModified']
    assert report['unityVersion'] == '2019.4.28f1c1' and report['inputSha256'] == digest(inputs_path)
    for source in inputs['sources']:
        assert digest(resolve(source['path'])) == source['sha256'], source['path']
    expected_text = next(t['text'] for t in inputs['texts'] if t['id'] == 181001)
    assert expected_text.count('<color=yellow>') == 13
    frames = report['frames']
    assert len(frames) == 5
    viewport_images, pixel_evidence = [], []
    for frame in frames:
        image = Path(frame['png']).resolve()
        assert image.parent == evidence_path.parent and digest(image) == frame['sha256']
        with Image.open(image) as im:
            assert im.size == (1440, 810) and im.convert('RGBA').getchannel('A').getbbox() is not None
            viewport = next(n['rect'] for n in frame['nodes'] if n['path'] == 'Rect/Right/pl_explain/sv_explain_view/v_explain_view')
            x, y, w, h = (viewport[k] for k in ('x', 'y', 'width', 'height'))
            box = (round(x), round(y), round(x + w), round(y + h))
            assert 0 <= box[0] < box[2] <= im.width and 0 <= box[1] < box[3] <= im.height
            pixels = np.asarray(im.convert('RGBA').crop(box)).astype(np.int16)
            viewport_images.append(pixels)
            opaque = pixels[:, :, 3] >= 128
            yellow = opaque & (pixels[:, :, 0] >= 160) & (pixels[:, :, 1] >= 160) & (pixels[:, :, 2] <= 100) & (abs(pixels[:, :, 0] - pixels[:, :, 1]) <= 30)
            body = opaque & (pixels[:, :, :3].min(axis=2) >= 160) & (pixels[:, :, :3].max(axis=2) - pixels[:, :, :3].min(axis=2) <= 40)
            # Presence only, not a Unity/UE pixel-fidelity score: frame alpha cannot substitute for visible text.
            if frame['explanationActive']:
                assert yellow.any() and body.any(), 'Missing actual yellow/body text pixels in explanation viewport'
            pixel_evidence.append({'name': frame['name'], 'viewport_box': box, 'yellow_pixels': int(yellow.sum()), 'body_pixels': int(body.sum())})
        assert frame['text'] == expected_text and frame['richText'] and frame['fontSize'] == 20
        assert abs(frame['lineSpacing'] - 1.25) < 1e-6 and frame['alignment'] == 0
        assert frame['lineCount'] > 1 and frame['vertices'] and frame['glyphs']
        glyphs = {g['code']: g['found'] for g in frame['glyphs']}
        plain = re.sub(r'<[^>]+>', '', expected_text)
        assert all(glyphs.get(ord(c), False) for c in plain if not c.isspace()), 'Missing source character glyph'
        assert abs(frame['contentHeight'] - frame['preferredHeight'] + frame['labelPosition']['y']) < .05
    assert frames[0]['contentHeight'] > frames[0]['viewportSize']['y']
    assert abs(frames[1]['normalizedScroll']) < .001
    assert frames[1]['windowInstance'] == frames[3]['windowInstance'] and frames[1]['labelInstance'] == frames[3]['labelInstance']
    assert abs(frames[1]['contentPosition']['y'] - frames[3]['contentPosition']['y']) < .05
    assert frames[4]['windowInstance'] != frames[0]['windowInstance'] and abs(frames[4]['normalizedScroll'] - frames[0]['normalizedScroll']) < .001
    assert viewport_images[0].shape == viewport_images[1].shape == viewport_images[3].shape
    assert not np.array_equal(viewport_images[0], viewport_images[1]), 'Top/bottom screenshots did not change'
    assert np.array_equal(viewport_images[1], viewport_images[3]), 'Returned explanation viewport pixels differ from preserved bottom'
    result = {'passed': True, 'evidence_sha256': digest(evidence_path), 'frames': 5,
              'pixel_evidence': pixel_evidence,
              'scope': 'Original prefab, native text/layout/ScrollRect snapshots. Viewport text presence and same-instance preservation, not Unity/UE pixel parity. Explicit presentation bindings and fresh instantiation, not full mediator/pool/service lifecycle.'}
    (evidence_path.parent / 'verify.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--verify', type=Path)
    args = parser.parse_args()
    if args.verify:
        verify_native(args.verify)
    else:
        main()
