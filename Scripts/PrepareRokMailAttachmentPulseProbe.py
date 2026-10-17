"""Prepare a native original-clip transform probe; never launch an engine.

The source parent is transparent and its child is Spine. This probe intentionally
produces numeric native evidence, not invented substitute pictures of that child.
"""
import hashlib
import json
import shutil
import subprocess
from pathlib import Path
from VerifyRokLeaderboardOverviewLayout import documents, node, vector
from VerifyRokMailAttachmentPulse import CLIP, PREFAB, main as verify_source

ROOT = Path(__file__).resolve().parents[1]
TARGET = ROOT / 'Saved/UnityMailAttachmentPulseProbe'
UNITY = Path('C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    verify_source()
    assert TARGET.resolve().parent == (ROOT / 'Saved').resolve()
    def out(relative):
        p = (TARGET / relative).resolve()
        assert p.is_relative_to(TARGET.resolve())
        p.parent.mkdir(parents=True, exist_ok=True)
        return p
    copies = []
    def copy(source, target):
        p = out(target)
        shutil.copy2(source, p)
        assert sha(source) == sha(p)
        copies.append({'source': source.relative_to(ROOT).as_posix(), 'target': target, 'sha256': sha(source)})
    clip = ROOT / CLIP
    copy(clip, 'Assets/Source/UA_Item_HotTask_bag.anim')
    copy(Path(str(clip) + '.meta'), 'Assets/Source/UA_Item_HotTask_bag.anim.meta')
    renderer = ROOT / 'Scripts/RenderRokMailAttachmentPulseProbe.cs'
    copy(renderer, 'Assets/Editor/' + renderer.name)
    prefab = ROOT / PREFAB
    parts = node(documents(prefab.read_text(encoding='utf-8-sig')), 'img_bag')
    def xy(name):
        a = vector(parts[0], name)
        return {'x': a[0], 'y': a[1]}
    sources = [prefab, clip, Path(str(clip) + '.meta'), renderer, Path(__file__).resolve(),
        ROOT / 'ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs',
        ROOT / 'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_MailBtn_SubView.cs']
    manifest = {'scope': 'Source-derived isolated native Unity original legacy clip sampling on extracted transparent img_bag RectTransform. Not full MailMediator, live mailbox, or Spine child rendering.',
        'prefab': PREFAB, 'clip': 'Assets/Source/UA_Item_HotTask_bag.anim',
        'size': xy('m_SizeDelta'), 'pivot': xy('m_Pivot'), 'anchorMin': xy('m_AnchorMin'), 'anchorMax': xy('m_AnchorMax'),
        'position': xy('m_AnchoredPosition'), 'arabFlags': 2,
        'times': [0, .05, .1, .15, .2, .283333335, .36666667, .466666665, .56666666, .65833333, .75, 1.5, 1.999, 2, 2.1, 2.2],
        'byte_identical_copies': copies,
        'sources': [{'path': p.relative_to(ROOT).as_posix(), 'sha256': sha(p)} for p in sources],
        'remaining_gates': ['Unity original-clip sampling not yet executed', 'Compare native scale values to UE helper/runtime trace', 'Spine UE_TreasureBox_stay and authority enclosure state not migrated']}
    out('ReferenceInputs.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    out('Packages/manifest.json').write_text(json.dumps({'dependencies': {'com.unity.modules.ui': '1.0.0', 'com.unity.modules.animation': '1.0.0', 'com.unity.modules.jsonserialize': '1.0.0'}}), encoding='utf-8')
    out('ProjectSettings/ProjectVersion.txt').write_text('m_EditorVersion: 2019.4.28f1c1\n', encoding='utf-8')
    refs = list((UNITY / 'Managed/UnityEngine').glob('*.dll'))
    refs += list((UNITY / 'MonoBleedingEdge/lib/mono/4.7.1-api').glob('*.dll'))
    refs += list((UNITY / 'MonoBleedingEdge/lib/mono/4.7.1-api/Facades').glob('*.dll'))
    refs += [UNITY / 'Managed/UnityEditor.dll']
    args = ['/nologo', '/target:library', '/define:UNITY_EDITOR', '/out:' + str(out('Validation/ProbeSyntax.dll'))]
    args += ['/reference:' + str(p) for p in refs]
    args += [str(TARGET / 'Assets/Editor/RenderRokMailAttachmentPulseProbe.cs')]
    rsp = out('Validation/compiler.rsp'); rsp.write_text('\n'.join('"' + a + '"' for a in args), encoding='utf-8')
    result = subprocess.run([str(UNITY / 'Tools/Roslyn/csc.exe'), '@' + str(rsp)], capture_output=True)
    out('Validation/compiler.log').write_bytes(result.stdout + result.stderr)
    out('Validation/compiler_result.json').write_text(json.dumps({'exit_code': result.returncode, 'unity_started': False}), encoding='utf-8')
    if result.returncode:
        raise RuntimeError('Probe compile failed; see Validation/compiler.log')
    print(json.dumps({'prepared': True, 'unity_started': False, 'samples': len(manifest['times']), 'output': str(TARGET)}))


if __name__ == '__main__':
    main()
