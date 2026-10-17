"""Prepare an isolated Unity 2019 ResearchMain mask-close sampler; never launch Unity."""
import hashlib
import json
import shutil
import subprocess
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'Saved/UnityResearchAnimationProbe'
TARGET = ROOT / 'Saved/UnityResearchMainMaskProbe'
ANIMATION = ROOT / 'Saved/RokUnityMigration/ResearchAnimationNative/animation_data.json'
RENDERER = ROOT / 'Scripts/RenderRokResearchMainMaskNative.cs'
SOURCES = (
    'ref/client-unity/Assets/Skyunion/RunTime/UIManager/Resources/UIRoot.prefab',
    'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIResearch/UI_Win_ResearchMain.prefab',
    'ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1.controller',
    'ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1_On.anim',
    'ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1_Off.anim',
    'ref/client-unity/Assets/Scripts/Hotfix/MVC/UI.cs',
    'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchMainMediator.cs',
    'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchMainView.cs',
    'ref/client-unity/Assets/Skyunion/RunTime/UIManager/UIManager.cs',
    'ref/client-unity/Assets/Skyunion/RunTime/UIManager/ViewBinder.cs',
    'ref/client-unity/Assets/Skyunion/RunTime/UIManager/GameView.cs',
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def main():
    assert TARGET.resolve().parent == (ROOT / 'Saved').resolve()
    assert BASE.is_dir() and ANIMATION.is_file() and RENDERER.is_file()
    base_inputs = BASE / 'ReferenceInputs.json'
    assert json.loads(base_inputs.read_text(encoding='utf-8'))['schema'] == 'rok.research.animation_native_inputs.v1'
    animation = json.loads(ANIMATION.read_text(encoding='utf-8'))
    assert animation['schema'] == 'rok.research.animation_data.v2'
    open_clip, close_clip = animation['clips'][0], animation['clips'][3]
    assert (open_clip['stateName'], open_clip['durationSeconds']) == ('Open', .25)
    assert (close_clip['stateName'], close_clip['durationSeconds']) == ('Close', .15)
    assert close_clip['events'] == [{'time': .15, 'functionName': 'CloseAnimatorCallback'}]
    copied = 0
    for src in (BASE / 'Assets').rglob('*'):
        if not src.is_file() or 'Editor' in src.relative_to(BASE / 'Assets').parts or src.suffix == '.unity':
            continue
        dst = TARGET / src.relative_to(BASE)
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
        assert sha(src) == sha(dst)
        copied += 1
    for src, name in ((BASE / 'SourceResearchInputs.json', 'SourceResearchInputs.json'),
                      (ANIMATION, 'AnimationData.json')):
        shutil.copyfile(src, TARGET / name)
        assert sha(src) == sha(TARGET / name)
    editor = TARGET / 'Assets/Editor/RenderRokResearchMainMaskNative.cs'
    editor.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(RENDERER, editor)
    assert sha(editor) == sha(RENDERER)
    adapter = TARGET / 'Assets/ReferenceComponents/ViewBinder.cs'
    assert adapter.is_file()
    manifest = {
        'schema': 'rok.research_main_mask.inputs.v1',
        'scope': 'Original UIRoot and ResearchMain prefab/Animator in an isolated Unity 2019 scene; no GameApp, account, queue or network.',
        'width': 1440, 'height': 810,
        'uiRootPrefab': 'Assets/Skyunion/RunTime/UIManager/Resources/UIRoot.prefab',
        'mainPrefab': 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIResearch/UI_Win_ResearchMain.prefab',
        'sourceResearchInputsSha256': sha(TARGET / 'SourceResearchInputs.json'),
        'animationDataSha256': sha(ANIMATION),
        'rendererSha256': sha(RENDERER),
        'prepareSha256': sha(Path(__file__)),
        'viewBinderAdapterSha256': sha(adapter),
        'copiedAssetFileCount': copied,
        'sourceFiles': [{'path': p, 'sha256': sha(resolve(p))} for p in SOURCES],
        'maskRule': {
            'sourceMethod': 'UIManager.AddMask', 'status': 'kTouchClose',
            'sourceUi': 'UI.s_ResearchMain', 'layer': 'WindowLayer',
            'color': {'r': 0, 'g': 0, 'b': 0, 'a': .8},
            'click': 'Button.onClick -> CloseUI(uiInfo) default non-force',
            'recreatedFromSourceRule': True, 'actualUiManagerInvoked': False,
        },
        'forceCloseBoundary': {
            'sourceMethod': 'ResearchMainMediator.CloseRes',
            'call': 'CloseUI(UI.s_ResearchMain, true, true)',
            'forceRemovesWithoutAnimatorClose': True,
            'simulatedInAnimationSamples': False,
        },
        'limitations': [
            'The full UIManager and hotfix mediator are not invoked. A source-rule mask and Button are recreated under the original UIRoot WindowLayer.',
            'Mask Button pointer-click invokes the isolated CloseAnimatorUI-equivalent guard and original Animator.Play(Close,-1,0).',
            'The original Close AnimationEvent reaches a source-method-compatible ViewBinder adapter; manager removal is simulated only after capture.',
        ],
    }
    write(TARGET / 'ReferenceInputs.json', manifest)
    (TARGET / 'Packages').mkdir(parents=True, exist_ok=True)
    shutil.copyfile(BASE / 'Packages/manifest.json', TARGET / 'Packages/manifest.json')
    (TARGET / 'ProjectSettings').mkdir(parents=True, exist_ok=True)
    for name in ('ProjectVersion.txt', 'ProjectSettings.asset', 'QualitySettings.asset',
                 'GraphicsSettings.asset', 'TagManager.asset'):
        src = BASE / 'ProjectSettings' / name
        if src.exists():
            shutil.copyfile(src, TARGET / 'ProjectSettings' / name)
    unity = Path('C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data')
    ugui = ROOT / 'Saved/UnityBarracksLevelProbe/Library/ScriptAssemblies/UnityEngine.UI.dll'
    assert unity.is_dir() and ugui.is_file()
    refs = list((unity / 'Managed/UnityEngine').glob('*.dll'))
    refs += list((unity / 'MonoBleedingEdge/lib/mono/4.7.1-api').glob('*.dll'))
    refs += list((unity / 'MonoBleedingEdge/lib/mono/4.7.1-api/Facades').glob('*.dll'))
    refs += [unity / 'Managed/UnityEditor.dll', ugui]
    validation = TARGET / 'Validation'
    validation.mkdir(parents=True, exist_ok=True)
    args = ['/nologo', '/target:library', '/define:UNITY_EDITOR',
            '/out:' + str(validation / 'ProbeSyntax.dll')]
    args += ['/reference:' + str(p) for p in refs]
    args += [str(p) for p in (TARGET / 'Assets/ReferenceComponents').glob('*.cs')]
    args += [str(editor)]
    response = validation / 'compiler.rsp'
    response.write_text('\n'.join('"' + arg + '"' for arg in args), encoding='utf-8')
    result = subprocess.run([str(unity / 'Tools/Roslyn/csc.exe'), '@' + str(response)], capture_output=True)
    (validation / 'compiler.log').write_bytes(result.stdout + result.stderr)
    write(validation / 'compiler_result.json', {'exitCode': result.returncode, 'unityStarted': False})
    if result.returncode:
        raise RuntimeError('ResearchMain mask probe syntax failed; see Validation/compiler.log')
    print(json.dumps({'prepared': True, 'unityStarted': False, 'project': str(TARGET),
                      'inputSha256': sha(TARGET / 'ReferenceInputs.json'),
                      'copiedSourceAssetCount': copied}, ensure_ascii=False))


if __name__ == '__main__':
    main()
