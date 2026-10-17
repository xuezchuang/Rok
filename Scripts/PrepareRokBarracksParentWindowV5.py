"""Prepare independent V5 all-window geometry inventory; never launch Unity."""
import hashlib
import json
import re
import shutil
import struct
import subprocess
import csv
from pathlib import Path
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text, LANGUAGE_WORKBOOK

ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / 'ref/client-unity'
TARGET = ROOT / 'Saved/UnityBarracksParentWindowV5Probe'
LANG = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/'
ITEM = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_AttrItem.prefab'


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
    roots = [CLIENT / 'Assets/BundleAssets', CLIENT / 'Assets/Scripts/Client/Compoment', CLIENT / 'Assets/Skyunion/RunTime/UIManager', CLIENT / 'Assets/Skyunion/RunTime/InputManager']
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
    queue = [CLIENT / (LANG + 'UI_Win_BuildingUpdate.prefab'), CLIENT / ITEM, CLIENT / 'Assets/Skyunion/RunTime/UIManager/Resources/UIRoot.prefab']
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
    expected = {'LanguageText', 'ArabLayoutCompment', 'PolygonImage', 'GameToggle', 'GameButton', 'BtnAnimation', 'ListView', 'ViewBinder', 'GameSlider', 'UIParticleOrder', 'DeemoRotate', 'UIZoomCurve', 'InputHandler', 'Empty4Raycast'}
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
        'UIZoomCurve': 'namespace Client { public class UIZoomCurve : UnityEngine.MonoBehaviour {} }',
        'InputHandler': 'namespace Skyunion { public class InputHandler : UnityEngine.MonoBehaviour {} }',
        'Empty4Raycast': 'namespace UnityEngine.UI { public class Empty4Raycast : MaskableGraphic { protected override void OnPopulateMesh(VertexHelper vh) { vh.Clear(); } } }',
    }
    if 'UIParticleOrder' in script_paths:
        codes['UIParticleOrder'] = script_paths['UIParticleOrder'].read_text(encoding='utf-8-sig').replace('using Skyunion;', '')
    for name, p in script_paths.items():
        relative = 'Assets/ReferenceComponents/' + name + '.cs'
        copy(Path(str(p) + '.meta'), relative + '.meta')
        if name in {'PolygonImage', 'DeemoRotate'}:
            copy(p, relative)
        else:
            target = out(relative)
            target.write_text(codes[name] + '\n', encoding='utf-8')
            adapters.append({'class': name, 'target': relative, 'sha256': digest(target), 'source': p.relative_to(ROOT).as_posix(),
                             'scope': 'Native visual base or inert service adapter; explicit source-driven renderer supplies presentation only.'})
    arabic = CLIENT / 'Assets/Scripts/Client/Compoment/UIExtend/ArabicSupport.cs'
    copy(arabic, 'Assets/ReferenceComponents/ArabicSupport.cs')
    copy(Path(str(arabic) + '.meta'), 'Assets/ReferenceComponents/ArabicSupport.cs.meta')
    binary = CLIENT / 'Assets/StreamingAssets/Config/Bin/Language.bin'
    language = {int(row[0]): row[1] for row in parse_languages(binary)}
    ids = set(int(n) for n in re.findall(r'm_languageId: (-?\d+)', all_text) if int(n) > 0)
    ids |= set(map(int, re.findall(r'propertyPath: m_languageId\s+value: (\d+)', all_text)))
    missing = sorted(ids - language.keys())
    if missing:
        raise RuntimeError('Missing source Chinese Language.bin IDs: ' + str(missing))
    texts = [{'id': i, 'text': language[i]} for i in sorted(ids)]
    record(binary)
    table = ROOT / 'ref/config-tables/编译客户端配置/CSV/BuildingTypeConfig.csv'
    with table.open(encoding='utf-8-sig', newline='') as stream:
        reader = csv.DictReader(stream)
        next(reader)  # schema types row
        barracks = next(row for row in reader if row['type'] == '9')
    fixture_title = language[180519].format(language[int(barracks['l_nameId'])], 2)
    record(table)
    record(CLIENT / 'Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateMediator.cs')
    review = ROOT / 'Saved/RokUnityMigration/BarracksParentWindowSource/source_review.json'
    record(review)
    for source in json.loads(review.read_text(encoding='utf-8'))['sources']:
        record(ROOT / source['path'])
    record(Path(__file__).resolve())
    renderer = ROOT / 'Scripts/RenderRokBarracksParentWindowV5.cs'
    copy(renderer, 'Assets/Editor/' + renderer.name)
    manifest = {
        'scope': 'Original UIRoot/WindowLayer and UI_Win_BuildingUpdate native Animator PlayMode presentation fixture; no login or authoritative game state',
        'prefab': LANG + 'UI_Win_BuildingUpdate.prefab',
        'uiRootPrefab': 'Assets/Skyunion/RunTime/UIManager/Resources/UIRoot.prefab',
        'width': 1440, 'height': 810, 'probeVersion': 5,
        'sampleThresholdsSeconds': [0, 7.0/60.0, 0.25, 0.5],
        'texts': texts,
        'fixtureTitle': fixture_title,
        'renderPolicy': {'nativeCameraClearFlags': 'Depth', 'clearTargetBeforeEachCameraRender': 'GL.Clear(true,true,Color.clear)',
                         'colorSpace': 'Gamma', 'language': 'Chinese', 'arabLayoutAppliedOnce': True},
        'fixture': {'name': 'source_prefab_presentation_only', 'resourceBalances': 'unknown', 'queueState': 'unknown', 'buildingLevel': 'unknown'},
        'runtime_adapter': {'UIRoot_localScale': 'Serialized UIRoot root scale is zero. V5 preserves it; V3 measured Canvas normalization without a forced assignment.',
                            'component_scope': 'UIZoomCurve and InputHandler are inert isolated adapters; no login, input routing or game services execute.'},
        'closeCaptureContract': {'state': 'Original UA_WinAnime_T1 Open terminal in native PlayMode',
                                 'controls': ['btn_close', 'title', 'UI_Model_Window_Type1'],
                                 'nestedSourcePrefab': 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_Window_Type2.prefab',
                                 'predictionForIndependentCheck': [1189.5, 40.0, 69.0, 69.0],
                                 'method': 'Read actual RectTransform world corners, camera screen corners, Button/Graphic properties; no coordinate overrides or click.'},
        'geometryInventoryContract': {'scope': 'All RectTransform descendants including inactive objects under source UI_Win_BuildingUpdate at natural Open terminal',
                                      'fields': ['hierarchy path', 'sibling index', 'local and lossy scale', 'world/screen corners', 'screen rect', 'active state', 'Graphic type/sprite/material/shader/enabled/raycast/cull'],
                                      'intent': 'Locate actual ancestry and visual coverage for left background/frame/content and build-image controls without assigning UE expected coordinates.'},
        'sources': sources, 'byte_identical_copies': copies, 'adapters': adapters,
        'sourceArabFlagInventory': arab_inventory,
        'referenceGeometry': json.loads(review.read_text(encoding='utf-8'))['geometry_inference'],
        'remainingGates': ['Unity PlayMode native render', 'strict verifier', 'UE runtime integration']
    }
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
    args += [str(TARGET / 'Assets/Editor/RenderRokBarracksParentWindowV5.cs')]
    response = out('Validation/compiler.rsp')
    response.write_text('\n'.join('"' + arg + '"' for arg in args[1:]), encoding='utf-8')
    checked = subprocess.run([args[0], '@' + str(response)], capture_output=True)
    out('Validation/compiler.log').write_bytes(checked.stdout + checked.stderr)
    out('Validation/compiler_result.json').write_text(json.dumps({'exit_code': checked.returncode,
        'unity_started': False, 'compiler': str(unity / 'Tools/Roslyn/csc.exe'),
        'scope': 'All generated adapters and original parent-window renderer compiled against Unity2019 assemblies; no engine launch.'}), encoding='utf-8')
    if checked.returncode:
        raise RuntimeError('Reference C# compilation failed; see Validation/compiler.log')
    print(json.dumps({'prepared':True,'unity_started':False,'copies':len(copies),'adapters':len(adapters)}))

if __name__ == '__main__':
    main()
