"""Prepare original prefab/shader/FBX in an isolated Unity project; never launch."""
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT/'ref/client-unity'
TARGET = ROOT/'Saved/UnityMountainFGammaProbe'
OUT = ROOT/'Saved/RokUnityMigration/MountainFGamma'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    index = {}
    for meta in (SOURCE/'Assets').rglob('*.meta'):
        match = re.search(r'^guid: ([0-9a-f]{32})', meta.read_text(encoding='utf-8-sig', errors='replace'), re.M)
        if match:
            index[match[1]] = Path(str(meta)[:-5])
    copied = []
    ignored_material_slots = []
    def copy(src, dst):
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        copied.append({'path': src.relative_to(ROOT).as_posix(), 'sha256': hashlib.sha256(src.read_bytes()).hexdigest()})
    todo = [SOURCE/'Assets/BundleAssets/land/Tile/AutoMake/Mountain/build_mountain_F.prefab']
    seen = set()
    while todo:
        src = todo.pop()
        if src in seen:
            continue
        seen.add(src)
        dst = TARGET/src.relative_to(SOURCE)
        if src.suffix == '.cs':
            if src.name != 'ObjectPoolItem.cs':
                raise RuntimeError(f'Unreviewed native script dependency: {src}')
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_text('namespace Skyunion { public class ObjectPoolItem : UnityEngine.MonoBehaviour { public int m_max_spawn_number_high=3,m_max_spawn_number_mid=2,m_max_spawn_number_low=1; } }\n', encoding='utf-8')
            copied.append({'path': src.relative_to(ROOT).as_posix(), 'sha256': hashlib.sha256(src.read_bytes()).hexdigest(), 'stub': 'Pool ownership only; no instantiation service in isolated opaque body render.'})
        else:
            copy(src, dst)
        copy(Path(str(src)+'.meta'), Path(str(dst)+'.meta'))
        if src.suffix.lower() in ('.prefab', '.mat', '.shader'):
            content = src.read_text(encoding='utf-8-sig')
            if src.suffix.lower() == '.mat':
                # Both original mountain shaders declare/sample only _MainTex.
                # Legacy Standard shader slots remain serialized but are unused.
                shader_guid = re.search(r'm_Shader:.*guid: ([0-9a-f]{32})', content)[1]
                shader_text = index[shader_guid].read_text(encoding='utf-8-sig')
                samplers = re.findall(r'sampler2D\s+(\w+)\s*;', shader_text)
                if samplers != ['_MainTex']:
                    raise RuntimeError(f'Unreviewed mountain shader samplers: {samplers}')
                main_guid = re.search(r'- _MainTex:\s*m_Texture:.*guid: ([0-9a-f]{32})', content)[1]
                relevant = [shader_guid, main_guid]
                ignored_material_slots.append({'material': src.relative_to(ROOT).as_posix(), 'unused_guids': sorted(set(re.findall(r'guid: ([0-9a-f]{32})', content))-set(relevant)), 'reason': 'Source shader only samples _MainTex; material bytes are unchanged.'})
            else:
                relevant = re.findall(r'guid: ([0-9a-f]{32})', content)
            for guid in relevant:
                if guid.startswith('0000000000000000'):
                    continue
                if guid not in index:
                    raise RuntimeError(f'Unresolved source GUID {guid}')
                todo.append(index[guid])
    copy(ROOT/'Scripts/RenderRokUnityMountainFGamma.cs', TARGET/'Assets/Editor/RenderRokUnityMountainFGamma.cs')
    for relative in ('ProjectSettings/ProjectSettings.asset', 'ProjectSettings/ProjectVersion.txt'):
        copy(SOURCE/relative, TARGET/relative)
    packages = json.loads((SOURCE/'Packages/manifest.json').read_text(encoding='utf-8-sig'))
    (TARGET/'Packages').mkdir(exist_ok=True)
    (TARGET/'Packages/manifest.json').write_text(json.dumps({'dependencies': {k:v for k,v in packages['dependencies'].items() if k.startswith('com.unity.modules.')}}, indent=2), encoding='utf-8')
    from BuildRokUnityMountainFGamma import build, DECODE_SHADER
    source_data=build()
    shader_path=TARGET/'Assets/RokMountainFMipDecode.shader'
    shader_path.write_text(DECODE_SHADER,encoding='utf-8')
    report=dict(source_data,source_files=copied,unused_material_slots=ignored_material_slots,isolated_project=str(TARGET),
        diagnostic_mip_shader={'path':shader_path.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(shader_path.read_bytes()).hexdigest()},
        scope='Original F prefab/body FBX/material/meta; F_edge excluded as current UE. Pool fields-only stub, original texture GPU mip decode via explicit tex2Dlod diagnostic shader; no full-world equivalence.')
    (OUT/'manifest.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    unity=Path('C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data')
    refs=list((unity/'Managed/UnityEngine').glob('*.dll'))+list((unity/'MonoBleedingEdge/lib/mono/4.7.1-api').glob('*.dll'))+list((unity/'MonoBleedingEdge/lib/mono/4.7.1-api/Facades').glob('*.dll'))+[unity/'Managed/UnityEditor.dll']
    validation=TARGET/'Validation';validation.mkdir(exist_ok=True)
    args=['/nologo','/target:library','/define:UNITY_EDITOR','/out:'+str(validation/'ReferenceSyntax.dll')]+['/reference:'+str(p) for p in refs]+[str(p) for p in (TARGET/'Assets').rglob('*.cs')]
    response=validation/'compiler.rsp';response.write_text('\n'.join('"'+arg+'"' for arg in args),encoding='utf-8')
    checked=subprocess.run([str(unity/'Tools/Roslyn/csc.exe'),'@'+str(response)],capture_output=True)
    (validation/'compiler.log').write_bytes(checked.stdout+checked.stderr)
    (OUT/'compiler_result.json').write_text(json.dumps(dict(exit_code=checked.returncode,unity_started=False,scope='Generated F native renderer and fields-only pool stub compiled against Unity2019 assemblies; shaders/native rendering not executed.'),indent=2))
    if checked.returncode:raise RuntimeError('F native C# syntax/API compile failed: '+str(validation/'compiler.log'))
    print(f'PREPARED {TARGET}; {len(seen)} source dependencies; no engine launched')


if __name__ == '__main__':
    main()
