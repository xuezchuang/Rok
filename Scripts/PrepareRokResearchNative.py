"""Prepare a source-backed, isolated Unity 2019 research UI presentation probe.

This never launches Unity and never touches the read-only reference checkout.
"""
import csv
import hashlib
import json
import re
import shutil
import subprocess
from collections import Counter, defaultdict
from pathlib import Path

from PrepareRokBarracksLevelNative import parse_languages

ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / 'ref/client-unity'
TARGET = ROOT / 'Saved/UnityResearchNativeProbe'
PACKAGE = ROOT / 'Saved/RokUnityMigration/ResearchNative'
RESEARCH = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIResearch/'
ATLAS = 'Assets/BundleAssets/UI/UITextureStatic/ui_res_research/'
ROOT_PREFAB = 'Assets/Skyunion/RunTime/UIManager/Resources/UIRoot.prefab'
PREFABS = [RESEARCH + name + '.prefab' for name in
           ('UI_Win_ResearchMain', 'UI_Win_ResearchUpate', 'UI_ResearchItem',
            'UI_ResearchSubItem', 'UI_ResearchPreItem', 'UI_ResearchPowerItem')]
ATTR_ITEM = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_AttrItem.prefab'
GRAY_MATERIAL = 'Assets/BundleAssets/Default/UI_GrayWithMask.mat'
ATLASES = [ATLAS + name + '.png' for name in
           ('ui_research', 'research_icon1', 'research_icon2', 'img_research_bg')]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def main():
    assert TARGET.resolve().parent == (ROOT / 'Saved').resolve()
    assert PACKAGE.resolve().parent == (ROOT / 'Saved/RokUnityMigration').resolve()
    sources, copies, adapters, missing = [], [], [], []

    def output(relative):
        path = (TARGET / relative).resolve()
        assert path.is_relative_to(TARGET.resolve())
        path.parent.mkdir(parents=True, exist_ok=True)
        return path

    def record(path):
        row = {'path': path.relative_to(ROOT).as_posix(), 'sha256': sha(path)}
        if row not in sources:
            sources.append(row)

    def copy(path, relative):
        target = output(relative)
        shutil.copyfile(path, target)
        assert sha(path) == sha(target)
        record(path)
        copies.append({'source': path.relative_to(ROOT).as_posix(), 'target': relative, 'sha256': sha(path)})

    study_path = ROOT / 'ref/config-tables/编译客户端配置/CSV/Study.csv'
    data_path = ROOT / 'ref/config-tables/编译客户端配置/CSV/StudyData.csv'
    language_path = CLIENT / 'Assets/StreamingAssets/Config/Bin/Language.bin'
    with study_path.open(encoding='utf-8-sig', newline='') as stream:
        studies = [row for row in csv.DictReader(stream) if row['ID'].isdigit()]
    with data_path.open(encoding='utf-8-sig', newline='') as stream:
        overrides = [row for row in csv.DictReader(stream) if row['ID'].isdigit()]
    language = {int(row[0]): row[1] for row in parse_languages(language_path)}
    assert len(studies) == 410
    first = [row for row in studies if row['studyLv'] == '1']
    assert Counter(row['type'] for row in first) == {'1': 28, '2': 43}
    max_levels = defaultdict(int)
    for row in studies:
        max_levels[int(row['studyType'])] = max(max_levels[int(row['studyType'])], int(row['studyLv']))
    assert {max(int(row['columns']) for row in first if row['type'] == type_id) for type_id in ('1', '2')} == {15}
    # Country is account state, so no StudyData civilization override is applied.
    # Still preserve the table and report all eight alternative icon/name chains.
    fields = ('ID', 'l_nameID', 'l_des', 'type', 'studyType', 'studyLv', 'campusLv',
              'preconditionStudy1', 'preconditionLv1', 'preconditionStudy2', 'preconditionLv2',
              'preconditionStudy3', 'preconditionLv3', 'preconditionStudy4', 'preconditionLv4',
              'needFood', 'needWood', 'needStone', 'needGold', 'l_buffDesID', 'desData',
              'costTime', 'power', 'icon', 'columns', 'location')
    nodes = []
    for row in first:
        node = {key: (row[key] if key in ('desData', 'icon') else int(row[key])) for key in fields}
        node['name'] = language[node['l_nameID']]
        node['description'] = language[node['l_des']]
        node['maxLevel'] = max_levels[node['studyType']]
        node['currentLevel'] = 0
        node['source'] = study_path.relative_to(ROOT).as_posix()
        nodes.append(node)
    detail = next(node for node in nodes if node['studyType'] == 102)
    assert detail['preconditionStudy1'] == 101 and detail['preconditionLv1'] == 1
    assert detail['currentLevel'] == 0
    detail_levels = [row for row in studies if row['studyType'] == str(detail['studyType'])]
    assert len(detail_levels) == detail['maxLevel'] == 5
    assert [int(row['studyLv']) for row in detail_levels] == list(range(1, 6))
    assert all(row['desData'] for row in detail_levels)
    more_detail = next(node for node in nodes if node['studyType'] == 128)
    more_levels = [row for row in studies if row['studyType'] == '128']
    assert more_detail['type'] == 1 and more_detail['columns'] == 15
    assert len(more_levels) == more_detail['maxLevel'] == 10
    assert [int(row['studyLv']) for row in more_levels] == list(range(1, 11))
    assert all(row['desData'] for row in more_levels)
    fixture = {
        'schema': 'rok.research.native_fixture.v1',
        'scope': 'Original prefabs + source Study/Language values in isolated all-unresearched deterministic presentation. No GameApp/login, technology queue, civilization or Academy/account state.',
        'resolution': {'width': 1440, 'height': 810},
        'state': {'name': 'all_unresearched', 'synthetic': True, 'accountBacked': False,
                  'academyLevel': None, 'civilizationId': None, 'researchQueue': None},
        'nodeCount': len(nodes), 'studyRowCount': len(studies),
        'columns': {'economy': 15, 'military': 15},
        'itemPositions': {'economy': [0, 210, 105, 0, -105, -210, 160, 0, 0, 0, 0, 0],
                          'military': [0, 228, 158, 150, 76, 0, -76, -150, -158, -228, 0, 0, 0, 0, 0, 0]},
        'nodes': nodes,
        'detailStudyType': detail['studyType'],
        'detailLevels': [{key: (row[key] if key in ('desData', 'icon') else int(row[key] or '0')) for key in fields}
                         for row in detail_levels],
        'moreDetailStudyType': more_detail['studyType'],
        'moreLevels': [{key: (row[key] if key in ('desData', 'icon') else int(row[key] or '0')) for key in fields}
                       for row in more_levels],
        'detailLockedLanguageId': 402019,
        'studyDataAlternatives': overrides,
        'language': [{'id': i, 'text': language[i]} for i in sorted({402017, 402018, 402019, 180306, 300005, 300016, 300017, 300102, 300103, 300104} |
                    {n['l_nameID'] for n in nodes} | {n['l_des'] for n in nodes}) if i in language],
        'sourceFiles': {p.relative_to(ROOT).as_posix(): sha(p) for p in
                        (study_path, data_path, language_path,
                         CLIENT / 'Assets/Scripts/Hotfix/MVC/Proxy/ResearchProxy.cs',
                         CLIENT / 'Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchMainMediator.cs',
                         CLIENT / 'Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchUpateMediator.cs',
                         CLIENT / 'Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchPowerItemView.cs',
                         CLIENT / 'Assets/Scripts/Client/Compoment/UIComponent/ListView.cs',
                         CLIENT / 'Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_ResearchSubItem_SubView.cs',
                         CLIENT / 'Assets/Scripts/Hotfix/MVC/RS.cs')},
    }
    write_json(PACKAGE / 'research_power_fixture.json', fixture)
    record(PACKAGE / 'research_power_fixture.json')
    copy(PACKAGE / 'research_power_fixture.json', 'ResearchPowerFixture.json')

    roots = [CLIENT / 'Assets/BundleAssets/UI', CLIENT / 'Assets/Scripts/Client/Compoment',
             CLIENT / 'Assets/Skyunion/RunTime/UIManager', CLIENT / 'Assets/Scripts/Hotfix']
    meta_names = subprocess.check_output(['rg', '--files', '--hidden', '--no-ignore', '-g', '*.meta',
                                          *map(str, roots)], text=True, encoding='utf-8').splitlines()
    index = {}
    for name in meta_names:
        meta = Path(name)
        if not meta.exists():
            continue
        match = re.search(r'^guid: (\w+)', meta.read_text(encoding='utf-8-sig', errors='replace')[:180], re.M)
        if match:
            index[match[1]] = Path(str(meta)[:-5])
    queue = [CLIENT / path for path in [ROOT_PREFAB, *PREFABS, ATTR_ITEM, GRAY_MATERIAL, *ATLASES]]
    seen, scripts, yaml = set(), {}, ''
    while queue:
        path = queue.pop()
        if path in seen:
            continue
        seen.add(path)
        if not path.exists():
            missing.append({'source': str(path), 'reason': 'meta exists but extracted asset missing'})
            continue
        if path.suffix == '.cs':
            scripts[path.stem] = path
            record(path)
            continue
        relative = path.relative_to(CLIENT).as_posix()
        copy(path, relative)
        copy(Path(str(path) + '.meta'), relative + '.meta')
        if path.suffix in {'.prefab', '.mat', '.anim', '.controller', '.asset', '.shader'}:
            body = path.read_text(encoding='utf-8-sig', errors='replace')
            yaml += body
            for guid in set(re.findall(r'guid: (\w+)', body)):
                if guid in index:
                    queue.append(index[guid])
                elif not guid.startswith('000000'):
                    missing.append({'guid': guid, 'referencedBy': relative})

    stubs = {
        'LanguageText': 'namespace GameFramework { public static class ReferenceMarker {} } namespace UnityEngine.UI { public class LanguageText : Text { [UnityEngine.SerializeField] private int m_languageId = -1; public int SourceLanguageId { get { return m_languageId; } } public TextGenerator CaptureGenerator() { var g = cachedTextGenerator; g.Populate(text, GetGenerationSettings(rectTransform.rect.size)); return g; } } }',
        'ArabLayoutCompment': 'public class ArabLayoutCompment : UnityEngine.MonoBehaviour { [UnityEngine.SerializeField] private int _arab_Layout; public int SourceFlags { get { return _arab_Layout; } } }',
        'GameButton': 'public class GameButton : UnityEngine.UI.Button {}',
        'GameSlider': 'public class GameSlider : UnityEngine.UI.Slider {}',
        'GameToggle': 'public class GameToggle : UnityEngine.UI.Toggle {}',
        'BtnAnimation': 'public class BtnAnimation : UnityEngine.MonoBehaviour {}',
        'ListView': 'public class ListView : UnityEngine.MonoBehaviour { public int layoutType; }',
        'ViewBinder': 'public class ViewBinder : UnityEngine.MonoBehaviour {}',
        'UIZoomCurve': 'namespace Client { public class UIZoomCurve : UnityEngine.MonoBehaviour {} }',
        'GrayChildrens': 'public class GrayChildrens : UnityEngine.MonoBehaviour {}',
    }
    for name, path in scripts.items():
        relative = 'Assets/ReferenceComponents/' + name + '.cs'
        copy(Path(str(path) + '.meta'), relative + '.meta')
        if name == 'PolygonImage':
            copy(path, relative)
        else:
            assert name in stubs, ('Unreviewed source component', name)
            target = output(relative)
            target.write_text(stubs[name] + '\n', encoding='utf-8')
            adapters.append({'class': name, 'target': relative, 'sha256': sha(target),
                             'source': path.relative_to(ROOT).as_posix(),
                             'scope': 'Inert isolated presentation adapter retaining source serialized GUID.'})
    # Original PolygonImage uses the project Arabic helper.
    arabic = CLIENT / 'Assets/Scripts/Client/Compoment/UIExtend/ArabicSupport.cs'
    copy(arabic, 'Assets/ReferenceComponents/ArabicSupport.cs')
    copy(Path(str(arabic) + '.meta'), 'Assets/ReferenceComponents/ArabicSupport.cs.meta')
    renderer = ROOT / 'Scripts/RenderRokResearchNative.cs'
    copy(renderer, 'Assets/Editor/' + renderer.name)
    for path in (Path(__file__).resolve(), study_path, data_path, language_path,
                 CLIENT / 'Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchPowerItemView.cs',
                 CLIENT / 'Assets/Scripts/Client/Compoment/UIComponent/ListView.cs'):
        record(path)
    ids = {int(s) for s in re.findall(r'm_languageId: (-?\d+)', yaml) if int(s) > 0}
    ids |= {int(s) for s in re.findall(r'propertyPath: m_languageId\s+value: (-?\d+)', yaml) if int(s) > 0}
    ids |= {402017, 402018, 402019, 180306}
    inputs = {'schema': 'rok.research.native_inputs.v1', 'probeVersion': 2,
              'scope': fixture['scope'], 'width': 1440, 'height': 810,
              'uiRootPrefab': ROOT_PREFAB, 'mainPrefab': PREFABS[0], 'detailPrefab': PREFABS[1],
              'itemPrefab': PREFABS[2], 'subItemPrefab': PREFABS[3], 'powerItemPrefab': PREFABS[5],
              'detailAttrItemPrefab': ATTR_ITEM,
              'atlasPaths': ATLASES, 'grayMaterialPath': GRAY_MATERIAL,
              'fixtureSha256': sha(PACKAGE / 'research_power_fixture.json'),
              'fixture': fixture,
              'prefabTexts': [{'id': i, 'text': language[i]} for i in sorted(ids) if i in language],
              'sources': sources, 'byteIdenticalCopies': copies, 'adapters': adapters,
              'missingExtractedGuids': sorted(missing, key=lambda x: str(x)),
              'limitations': ['No hotfix mediator or GameApp/login/account state',
                              'No country-specific StudyData override without server role',
                              'Research mediator hotfix is not invoked; node/line/gray state is source-rule emulation in isolated probe']}
    write_json(output('ReferenceInputs.json'), inputs)
    write_json(output('Packages/manifest.json'), {'dependencies': {key: '1.0.0' for key in
               ('com.unity.ugui', 'com.unity.modules.ui', 'com.unity.modules.imageconversion',
                'com.unity.modules.jsonserialize', 'com.unity.modules.imgui', 'com.unity.modules.animation')}})
    output('ProjectSettings/ProjectVersion.txt').write_text('m_EditorVersion: 2019.4.28f1c1\n', encoding='utf-8')
    unity = Path('C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data')
    ugui = ROOT / 'Saved/UnityBarracksLevelProbe/Library/ScriptAssemblies/UnityEngine.UI.dll'
    assert ugui.exists()
    refs = list((unity / 'Managed/UnityEngine').glob('*.dll'))
    refs += list((unity / 'MonoBleedingEdge/lib/mono/4.7.1-api').glob('*.dll'))
    refs += list((unity / 'MonoBleedingEdge/lib/mono/4.7.1-api/Facades').glob('*.dll'))
    refs += [unity / 'Managed/UnityEditor.dll', ugui]
    args = ['/nologo', '/target:library', '/define:UNITY_EDITOR',
            '/out:' + str(output('Validation/ProbeSyntax.dll'))]
    args += ['/reference:' + str(p) for p in refs]
    args += [str(p) for p in (TARGET / 'Assets/ReferenceComponents').glob('*.cs')]
    args += [str(TARGET / 'Assets/Editor/RenderRokResearchNative.cs')]
    response = output('Validation/compiler.rsp')
    response.write_text('\n'.join('"' + arg + '"' for arg in args), encoding='utf-8')
    result = subprocess.run([str(unity / 'Tools/Roslyn/csc.exe'), '@' + str(response)], capture_output=True)
    output('Validation/compiler.log').write_bytes(result.stdout + result.stderr)
    write_json(output('Validation/compiler_result.json'), {'exitCode': result.returncode, 'unityStarted': False})
    if result.returncode:
        raise RuntimeError('Research native probe syntax failed; see Validation/compiler.log')
    print(json.dumps({'prepared': True, 'unityStarted': False, 'nodeCount': len(nodes),
                      'copies': len(copies), 'adapters': len(adapters), 'missingGuids': len(missing)}))


if __name__ == '__main__':
    main()
