"""Prepare an isolated, source-derived native FAQ reference; never launch Unity."""
import hashlib
import json
import re
import shutil
import struct
import subprocess
from pathlib import Path
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text, LANGUAGE_WORKBOOK

ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / 'ref/client-unity'
TARGET = ROOT / 'Saved/UnityHelpNativeReferenceProbe'
LANG = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_player/UIHelper/'


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
    assert cols == 7
    rows = [[strings[read('i')] for _ in range(cols)] for _ in range(count)]
    assert pos == len(data)
    return [r for r in rows if r[4] == '1']


def main():
    assert TARGET.resolve().parent == (ROOT / 'Saved').resolve()
    def out(relative):
        path = (TARGET / relative).resolve()
        assert path.is_relative_to(TARGET.resolve())
        path.parent.mkdir(parents=True, exist_ok=True)
        return path
    sources, copies, adapters = [], [], []
    def record(p):
        row = {'path': p.relative_to(ROOT).as_posix(), 'sha256': digest(p)}
        if row not in sources:
            sources.append(row)
    def copy(p, relative):
        target = out(relative)
        shutil.copy2(p, target)
        assert digest(p) == digest(target)
        record(p)
        copies.append({'source': p.relative_to(ROOT).as_posix(), 'target': relative, 'sha256': digest(p)})

    # Only the relevant UI folders; preserve nested prefab/animation/atlas GUIDs.
    index = {}
    roots = [CLIENT / 'Assets/BundleAssets/UI', CLIENT / 'Assets/Scripts/Client/Compoment', CLIENT / 'Assets/Skyunion/RunTime/UIManager']
    files = subprocess.check_output(['rg', '--files', '--hidden', '--no-ignore', '-g', '*.meta', *map(str, roots)], text=True).splitlines()
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
    queue = [CLIENT / (LANG + 'UI_Win_Helper.prefab'), CLIENT / (LANG + 'UI_Item_Helper.prefab'), CLIENT / (LANG + 'UI_Item_HelperItem.prefab')]
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
    expected = {'LanguageText', 'ArabLayoutCompment', 'PolygonImage', 'GameToggle', 'GameButton', 'BtnAnimation', 'ListView', 'ViewBinder'}
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
    assert all((r['flags'] & ~539) == 0 for r in arab_inventory)
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
        'BtnAnimation': 'public class BtnAnimation : UnityEngine.MonoBehaviour {}',
        'ListView': 'public class ListView : UnityEngine.MonoBehaviour {}',
        'ViewBinder': 'public class ViewBinder : UnityEngine.MonoBehaviour {}',
    }
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
    from BuildRokHelpCatalog import build
    catalog = build()
    language, _ = read_language_lookup()
    ids = sorted(set(int(n) for n in re.findall(r'm_languageId: (-?\d+)', all_text) if int(n) > 0)
                 | set(map(int, re.findall(r'propertyPath: m_languageId\s+value: (\d+)', all_text))))
    entries = [{'id': i, 'text': localized_text(language, i, 'cn')} for i in ids]
    assert all(e['text'] for e in entries)
    arabic = CLIENT / 'Assets/Scripts/Client/Compoment/UIExtend/ArabicSupport.cs'
    copy(arabic, 'Assets/ReferenceComponents/ArabicSupport.cs')
    copy(Path(str(arabic) + '.meta'), 'Assets/ReferenceComponents/ArabicSupport.cs.meta')
    for p in [LANGUAGE_WORKBOOK, Path(__file__).resolve(), CLIENT / 'Assets/Scripts/Hotfix/MVC/View_Mediator/Helper/UI_Win_HelperMediator.cs']:
        record(p)
    renderer = ROOT / 'Scripts/RenderRokHelpNativeReference.cs'
    copy(renderer, 'Assets/Editor/' + renderer.name)
    source_texts = []
    def text_spec(key, section, path, text, auto_height=False):
        node = catalog['layouts'][section][path]
        if not text:
            raise ValueError('Empty export text ' + key)
        source_texts.append({'key': key, 'text': text, 'width': node['rect'][2], 'height': node['rect'][3],
                             'fontSize': node['font_px'], 'style': 1 if node['bold'] else 0,
                             'alignment': node['alignment'], 'autoHeight': auto_height, 'bestFit': bool(node['best_fit']),
                             'minSize': node['min_size'], 'maxSize': node['max_size'], 'horizontalOverflow': node['horizontal_overflow'],
                             'verticalOverflow': node['vertical_overflow'], 'lineSpacing': node['line_spacing']})
    for row in catalog['entries']:
        text_spec('list:' + str(row['id']), 'question', 'btn_btn/lbl_title', row['question'])
        text_spec('question:' + str(row['id']), 'window', 'pl_detial/lbl_question', row['question'])
        text_spec('answer:' + str(row['id']), 'window', 'pl_detial/sv_list/v/c/lbl_detial', row['answer'], True)
    for group in catalog['group_order']:
        row = next(e for e in catalog['entries'] if e['group_id'] == group)
        text_spec('group:' + str(group), 'group', 'btn_btn/lbl_title', row['group'])
    for section in ['window', 'frame']:
        for path, node in catalog['layouts'][section].items():
            if 'font_px' in node and not path.startswith('pl_detial/'):
                text_spec(section + ':' + path, section, path, node['text'])
    assert len(source_texts) == 222 and len({s['key'] for s in source_texts}) == 222
    manifest = {'scope': 'Isolated native Chinese FAQ prefab/layout/text reference; explicit local tree presentation equivalent, not full mediator/service.',
                'prefab': LANG + 'UI_Win_Helper.prefab', 'rowPrefab': LANG + 'UI_Item_Helper.prefab', 'questionPrefab': LANG + 'UI_Item_HelperItem.prefab',
                'width': 1440, 'height': 810, 'texts': entries, 'questions': catalog['entries'], 'sourceTexts': source_texts,
                'sources': sources, 'byte_identical_copies': copies, 'adapters': adapters,
                'nativeUguiGuids': native, 'sourceArabFlagInventory': arab_inventory,
                'remaining_gates': ['Unity import/render not executed by prepare', 'Chinese only; no SDK or account state']}
    out('ReferenceInputs.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    out('Packages/manifest.json').write_text(json.dumps({'dependencies': {k: '1.0.0' for k in ['com.unity.ugui', 'com.unity.modules.ui', 'com.unity.modules.imageconversion', 'com.unity.modules.jsonserialize', 'com.unity.modules.imgui', 'com.unity.modules.animation']}}), encoding='utf-8')
    out('ProjectSettings/ProjectVersion.txt').write_text('m_EditorVersion: 2019.4.28f1c1\n', encoding='utf-8')
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
    args += [str(TARGET / 'Assets/Editor/RenderRokHelpNativeReference.cs')]
    response = out('Validation/compiler.rsp')
    response.write_text('\n'.join('"' + arg + '"' for arg in args[1:]), encoding='utf-8')
    checked = subprocess.run([args[0], '@' + str(response)], capture_output=True)
    out('Validation/compiler.log').write_bytes(checked.stdout + checked.stderr)
    out('Validation/compiler_result.json').write_text(json.dumps({'exit_code': checked.returncode,
        'unity_started': False, 'compiler': str(unity / 'Tools/Roslyn/csc.exe'),
        'scope': 'All generated adapters, original ArabicSupport/PolygonImage and renderer compiled against Unity2019 assemblies; no engine launch.'}), encoding='utf-8')
    if checked.returncode:
        raise RuntimeError('Reference C# compilation failed; see Validation/compiler.log')
    print(json.dumps({'prepared': True, 'unity_started': False, 'copies': len(copies), 'adapters': len(adapters), 'questions': len(catalog['entries'])}))


if __name__ == '__main__':
    main()
