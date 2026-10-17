"""Prepare a separate Unity 2019 Research animation sampling project; no engine launch."""
import hashlib
import json
import shutil
import subprocess
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
SOURCE_PROJECT = ROOT / 'Saved/UnityResearchNativeProbe'
TARGET = ROOT / 'Saved/UnityResearchAnimationProbe'
PACKAGE = ROOT / 'Saved/RokUnityMigration/ResearchAnimationNative'
PYTHON = Path(__file__).resolve()
RENDERER = ROOT / 'Scripts/RenderRokResearchAnimationNative.cs'
ANIMATION = PACKAGE / 'animation_data.json'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def main():
    assert TARGET.resolve().parent == (ROOT / 'Saved').resolve()
    assert SOURCE_PROJECT.is_dir() and ANIMATION.is_file() and RENDERER.is_file()
    old_inputs = SOURCE_PROJECT / 'ReferenceInputs.json'
    old_fixture = SOURCE_PROJECT / 'ResearchPowerFixture.json'
    previous = json.loads(old_inputs.read_text(encoding='utf-8'))
    assert previous['probeVersion'] == 2 and previous['fixture']['moreDetailStudyType'] == 128
    assert previous['fixtureSha256'] == sha(old_fixture)
    assert previous['fixture']['state']['synthetic'] and not previous['fixture']['state']['accountBacked']
    # Reuse only immutable copied source assets/adapters, never source project's
    # Editor script, scene, Library, artifacts or verification reports.
    for src in (SOURCE_PROJECT / 'Assets').rglob('*'):
        if not src.is_file() or 'Editor' in src.relative_to(SOURCE_PROJECT / 'Assets').parts or src.suffix == '.unity':
            continue
        rel = src.relative_to(SOURCE_PROJECT)
        dst = TARGET / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
        assert sha(src) == sha(dst)
    for row in previous['adapters']:
        assert sha(TARGET / row['target']) == row['sha256'], row['target']
    for src, name in ((old_inputs, 'SourceResearchInputs.json'),
                      (old_fixture, 'ResearchPowerFixture.json'),
                      (ANIMATION, 'AnimationData.json')):
        shutil.copyfile(src, TARGET / name)
        assert sha(src) == sha(TARGET / name)
    for row in previous['byteIdenticalCopies']:
        if row['target'] == 'Assets/Editor/RenderRokResearchNative.cs':
            continue  # The new project has its own independent Editor entry point.
        src = resolve(row['source'])
        dst = TARGET / row['target']
        assert sha(src) == row['sha256'] == sha(dst), row['source']
    # Keep the source ViewBinder GUID and its actual AnimationEvent method.
    # The full UIManager/GameApp is intentionally absent; this adapter exposes
    # callback ordering and a per-sample count for the isolated harness.
    binder_adapter = TARGET / 'Assets/ReferenceComponents/ViewBinder.cs'
    binder_adapter.write_text('''public class ViewBinder : UnityEngine.MonoBehaviour {
    public System.Action onCloseAnimatorCallback;
    public bool IsAniCloseing;
    public int CloseAnimatorEventCount;
    public void CloseAnimatorCallback() {
        CloseAnimatorEventCount++;
        if (onCloseAnimatorCallback != null) onCloseAnimatorCallback();
        IsAniCloseing = false;
    }
}
''', encoding='utf-8')
    renderer_copy = TARGET / 'Assets/Editor/RenderRokResearchAnimationNative.cs'
    renderer_copy.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(RENDERER, renderer_copy)
    assert sha(renderer_copy) == sha(RENDERER)
    manifest = {'schema': 'rok.research.animation_native_inputs.v1',
                'scope': 'Original prefabs/Animator clips, isolated synthetic all-unresearched StudyType128 display; no GameApp/login/network.',
                'width': 1440, 'height': 810, 'sourceResearchInputsSha256': sha(old_inputs),
                'sourceFixtureSha256': sha(old_fixture), 'animationDataSha256': sha(ANIMATION),
                'rendererSha256': sha(RENDERER), 'prepareSha256': sha(PYTHON),
                'instrumentedViewBinderSha256': sha(binder_adapter),
                'assetCopyCount': len(previous['byteIdenticalCopies']),
                'sourceProject': old_inputs.relative_to(ROOT).as_posix(),
                'animationData': ANIMATION.relative_to(ROOT).as_posix(),
                'sourceFiles': json.loads(ANIMATION.read_text(encoding='utf-8'))['sourceFiles'],
                'limitations': ['The hotfix mediator is not invoked; this isolated probe repeats its documented SetActive/Animator.Play sequence on original prefab Animator components.',
                                'A source-method-compatible ViewBinder adapter receives the original Close AnimationEvent; its callback marks manager removal, but the full UIManager stack is not running.',
                                'Account research levels and Academy state are unknown; StudyType128 display uses the prior explicit synthetic fixture.']}
    write(TARGET / 'ReferenceInputs.json', manifest)
    (TARGET / 'Packages').mkdir(parents=True, exist_ok=True)
    shutil.copyfile(SOURCE_PROJECT / 'Packages/manifest.json', TARGET / 'Packages/manifest.json')
    (TARGET / 'ProjectSettings').mkdir(parents=True, exist_ok=True)
    shutil.copyfile(SOURCE_PROJECT / 'ProjectSettings/ProjectVersion.txt', TARGET / 'ProjectSettings/ProjectVersion.txt')
    # Source project used Gamma; keep the same color space and camera settings.
    for name in ('ProjectSettings.asset', 'QualitySettings.asset', 'GraphicsSettings.asset', 'TagManager.asset'):
        src = SOURCE_PROJECT / 'ProjectSettings' / name
        if src.exists():
            shutil.copyfile(src, TARGET / 'ProjectSettings' / name)
    unity = Path('C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data')
    ugui = ROOT / 'Saved/UnityBarracksLevelProbe/Library/ScriptAssemblies/UnityEngine.UI.dll'
    assert unity.is_dir() and ugui.exists()
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
    args += [str(renderer_copy)]
    response = validation / 'compiler.rsp'
    response.write_text('\n'.join('"' + arg + '"' for arg in args), encoding='utf-8')
    result = subprocess.run([str(unity / 'Tools/Roslyn/csc.exe'), '@' + str(response)], capture_output=True)
    (validation / 'compiler.log').write_bytes(result.stdout + result.stderr)
    write(validation / 'compiler_result.json', {'exitCode': result.returncode, 'unityStarted': False})
    if result.returncode:
        raise RuntimeError('Animation probe syntax failed; see Validation/compiler.log')
    print(json.dumps({'prepared': True, 'unityStarted': False, 'project': str(TARGET),
                      'animationDataSha256': sha(ANIMATION), 'inputSha256': sha(TARGET / 'ReferenceInputs.json'),
                      'copiedSourceAssetCount': manifest['assetCopyCount']}))


if __name__ == '__main__':
    main()
