"""Prepare isolated native meshes for the four original barracks level headers; never launch Unity."""
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

from PrepareRokHospitalExplanationNativeReference import parse_languages

ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / 'ref/client-unity'
TARGET = ROOT / 'Saved/UnityBarracksLevelHeaderTextProbe'
LEVEL_EVIDENCE = ROOT / 'Saved/UnityBarracksLevelProbe/Artifacts/native_20261008_094049_295/render_evidence.json'
CATALOG = ROOT / 'Saved/RokUnityMigration/building_level_details.json'
PREFAB_REL = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Item_BuildingLevelData_Title_Four.prefab'
PREFAB = CLIENT / PREFAB_REL
LANGUAGE = CLIENT / 'Assets/StreamingAssets/Config/Bin/Language.bin'
MEDIATOR = CLIENT / 'Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateMediator.cs'
IDS = [180310, 180329, 180352, 180314]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    assert TARGET.resolve().parent == (ROOT / 'Saved').resolve()
    native = json.loads(LEVEL_EVIDENCE.read_text(encoding='utf-8'))
    assert native['complete'] and native['levelRowCount'] == 25
    rows = native['detailWindowGeometry']
    catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
    type9 = next(t for t in catalog['tables'] if t['type'] == 9)
    assert type9['header_ids'] == IDS
    language = {int(row[0]): row[1] for row in parse_languages(LANGUAGE)}
    assert type9['headers'] == [language[i] for i in IDS]
    source = PREFAB.read_text(encoding='utf-8-sig')
    specs = []
    for index, key in enumerate(IDS):
        name = f'lbl_name{index + 1}'
        source_start = source.index('m_Name: ' + name + '\n')
        source_end = source.find('--- !u!1 ', source_start + 1)
        block = source[source_start:source_end if source_end != -1 else None]
        required = ('m_Font: {fileID: 10102,', 'm_FontSize: 22', 'm_FontStyle: 0',
                    'm_BestFit: 1', 'm_MinSize: 2', 'm_MaxSize: 24', 'm_Alignment: 4',
                    'm_HorizontalOverflow: 0', 'm_VerticalOverflow: 0',
                    'm_Color: {r: 0.9019608, g: 0.8509804, b: 0.7058824, a: 1}')
        assert all(item in block for item in required), (name, 'source typography drift')
        found = [r for r in rows if r['path'].endswith('/pl_tilte/UI_Item_BuildingLevelData_Title_Four(Clone)/' + name)]
        assert len(found) == 1
        row = found[0]
        assert row['text'] == language[key] and row['activeInHierarchy']
        rect = row['screenRect']
        assert abs(rect['width'] - 250) < .01 and abs(rect['height'] - 50) < .01
        color = row['graphicColor']
        assert all(abs(color[k] - value) < .00001 for k, value in
                   {'r': .9019608, 'g': .8509804, 'b': .7058824, 'a': 1}.items())
        specs.append(dict(key=f'header_{index}', text=language[key], fontSize=22,
                          style=0, alignment=4, width=rect['width'], height=rect['height'],
                          x=rect['x'], y=rect['y'], rich=True, color=color,
                          horizontalOverflow=0, verticalOverflow=0, lineSpacing=1,
                          bestFit=True, bestFitMinSize=2, bestFitMaxSize=24,
                          languageId=key, sourceNode=name))
    renderer = ROOT / 'Scripts/RenderRokBarracksLevelHeaderText.cs'
    sources = [LANGUAGE, PREFAB, MEDIATOR, LEVEL_EVIDENCE, CATALOG,
               Path(__file__).resolve(), renderer]
    for folder in ('Assets/Editor', 'Packages', 'ProjectSettings', 'Validation'):
        (TARGET / folder).mkdir(parents=True, exist_ok=True)
    # The earlier isolated level probe already contains byte-identical transitive
    # source prefab/atlas dependencies and the inert script adapters needed to import.
    previous = ROOT / 'Saved/UnityBarracksLevelProbe/Assets'
    for folder in ('BundleAssets', 'ReferenceComponents', 'Skyunion'):
        shutil.copytree(previous / folder, TARGET / 'Assets' / folder, dirs_exist_ok=True)
    assert sha(TARGET / PREFAB_REL) == sha(PREFAB)
    copied = TARGET / 'Assets/Editor/RenderRokBarracksLevelHeaderText.cs'
    shutil.copyfile(renderer, copied)
    inputs = dict(schema='rok.barracks.source_text.inputs.v1', sourceTexts=specs,
                  prefab=PREFAB_REL, nativeLevelEvidence=LEVEL_EVIDENCE.relative_to(ROOT).as_posix(),
                  catalog=CATALOG.relative_to(ROOT).as_posix(),
                  sourceRootRect={'x': 208.00003051757812, 'y': 132.8499755859375,
                                  'width': 1024.0, 'height': 50.00006103515625},
                  sources=[dict(path=p.relative_to(ROOT).as_posix(), sha256=sha(p)) for p in sources],
                  renderer_sha256=sha(renderer), copied_renderer_sha256=sha(copied),
                  scope='Four original Title_Four LanguageText header meshes and GPU font atlases, 22px Normal MiddleCenter BestFit on source 250x50 cells; no whole-text raster or GameApp/login.')
    (TARGET / 'ReferenceInputs.json').write_text(json.dumps(inputs, ensure_ascii=False, indent=2), encoding='utf-8')
    (TARGET / 'Packages/manifest.json').write_text(json.dumps({'dependencies': {k: '1.0.0' for k in
        ('com.unity.ugui', 'com.unity.modules.ui', 'com.unity.modules.imageconversion',
         'com.unity.modules.jsonserialize', 'com.unity.modules.imgui')}}, indent=2), encoding='utf-8')
    (TARGET / 'ProjectSettings/ProjectVersion.txt').write_text('m_EditorVersion: 2019.4.28f1c1\n', encoding='utf-8')
    unity = Path('C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data')
    refs = list((unity / 'Managed/UnityEngine').glob('*.dll'))
    refs += list((unity / 'MonoBleedingEdge/lib/mono/4.7.1-api').glob('*.dll'))
    refs += list((unity / 'MonoBleedingEdge/lib/mono/4.7.1-api/Facades').glob('*.dll'))
    refs += [unity / 'Managed/UnityEditor.dll', ROOT / 'Saved/UnityBarracksLevelProbe/Library/ScriptAssemblies/UnityEngine.UI.dll']
    args = ['/nologo', '/target:library', '/define:UNITY_EDITOR',
            '/out:' + str(TARGET / 'Validation/ReferenceSyntax.dll')]
    args += ['/reference:' + str(p) for p in refs]
    args += [str(p) for p in (TARGET / 'Assets/ReferenceComponents').glob('*.cs')]
    args += [str(copied)]
    rsp = TARGET / 'Validation/compiler.rsp'
    rsp.write_text('\n'.join('"' + arg + '"' for arg in args), encoding='utf-8')
    checked = subprocess.run([str(unity / 'Tools/Roslyn/csc.exe'), '@' + str(rsp)], capture_output=True)
    (TARGET / 'Validation/compiler.log').write_bytes(checked.stdout + checked.stderr)
    (TARGET / 'Validation/compiler_result.json').write_text(json.dumps({
        'exit_code': checked.returncode, 'unity_started': False, 'source_text_count': 4}), encoding='utf-8')
    assert checked.returncode == 0, 'See isolated Validation/compiler.log'
    print(json.dumps({'prepared': True, 'unity_started': False, 'headers': 4, 'project': str(TARGET)}))


if __name__ == '__main__':
    main()
