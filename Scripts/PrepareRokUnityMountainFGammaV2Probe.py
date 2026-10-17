"""Prepare a separate Unity 2019 Mountain F GPU decode probe; never launch Unity."""
import hashlib
import json
import shutil
import subprocess
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
OLD = ROOT / 'Saved/UnityMountainFGammaProbe'
TARGET = ROOT / 'Saved/UnityMountainFGammaV2Probe'
OLD_REPORT = ROOT / 'Saved/RokUnityMigration/MountainFGamma'
OUT = ROOT / 'Saved/RokUnityMigration/MountainFGammaV2'
UNITY_DATA = Path('C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data')
MIP_SHADER = '''Shader "Hidden/RokMountainFMipDecodeV2" {
Properties { _MainTex ("Original DXT1 texture", 2D) = "white" {} _Mip ("Source mip", Float) = 0 }
SubShader { Tags { "RenderType"="Opaque" } Cull Off ZWrite Off ZTest Always
Pass { CGPROGRAM
#pragma vertex vert
#pragma fragment frag
#pragma target 3.0
#include "UnityCG.cginc"
sampler2D _MainTex;
float _Mip;
struct appdata { float4 vertex : POSITION; float2 uv : TEXCOORD0; };
struct v2f { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
v2f vert(appdata v) { v2f o; o.pos = UnityObjectToClipPos(v.vertex); o.uv = v.uv; return o; }
fixed4 frag(v2f i) : SV_Target { return tex2Dlod(_MainTex, float4(i.uv, 0, _Mip)); }
ENDCG } }
Fallback Off }
'''


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    old_manifest = OLD_REPORT / 'manifest.json'
    source = json.loads(old_manifest.read_text(encoding='utf-8'))
    assert source['schema'] == 'rok.mountain_f_gamma.v1'
    for entry in source['source_files']:
        if entry.get('stub'):
            continue
        current = resolve(entry['path'])
        assert digest(current) == entry['sha256'], f'Changed source input: {current}'
    assert (OLD / 'Assets').exists() and (OLD / 'ProjectSettings').exists()
    OUT.mkdir(parents=True, exist_ok=True)
    shutil.copytree(OLD / 'Assets', TARGET / 'Assets', dirs_exist_ok=True)
    shutil.copytree(OLD / 'ProjectSettings', TARGET / 'ProjectSettings', dirs_exist_ok=True)
    shutil.copytree(OLD / 'Packages', TARGET / 'Packages', dirs_exist_ok=True)
    for entry in source['source_files']:
        if entry.get('stub'):
            continue
        relative = Path(entry['path']).relative_to('ref/client-unity') if entry['path'].startswith('ref/client-unity/') else None
        if relative is not None:
            copied = TARGET / relative
            assert copied.is_file() and digest(copied) == entry['sha256'], f'Isolated source differs: {copied}'
    old_editor = TARGET / 'Assets/Editor/RenderRokUnityMountainFGamma.cs'
    old_editor.unlink(missing_ok=True)
    old_editor.with_suffix('.cs.meta').unlink(missing_ok=True)
    new_editor = TARGET / 'Assets/Editor/RenderRokUnityMountainFGammaV2.cs'
    new_editor.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / 'Scripts/RenderRokUnityMountainFGammaV2.cs', new_editor)
    shader = TARGET / 'Assets/Editor/RokMountainFMipDecodeV2.shader'
    shader.write_text(MIP_SHADER, encoding='utf-8')

    refs = (list((UNITY_DATA / 'Managed/UnityEngine').glob('*.dll'))
            + list((UNITY_DATA / 'MonoBleedingEdge/lib/mono/4.7.1-api').glob('*.dll'))
            + list((UNITY_DATA / 'MonoBleedingEdge/lib/mono/4.7.1-api/Facades').glob('*.dll'))
            + [UNITY_DATA / 'Managed/UnityEditor.dll'])
    validation = TARGET / 'Validation'
    validation.mkdir(exist_ok=True)
    args = ['/nologo', '/target:library', '/define:UNITY_EDITOR',
            '/out:' + str(validation / 'ReferenceSyntax.dll')]
    args += ['/reference:' + str(path) for path in refs]
    args += [str(path) for path in (TARGET / 'Assets').rglob('*.cs')]
    response = validation / 'compiler.rsp'
    response.write_text('\n'.join('"' + arg + '"' for arg in args), encoding='utf-8')
    checked = subprocess.run([str(UNITY_DATA / 'Tools/Roslyn/csc.exe'), '@' + str(response)], capture_output=True)
    (validation / 'compiler.log').write_bytes(checked.stdout + checked.stderr)
    report = {
        'schema': 'rok.mountain_f_gamma_v2.prepare.v1',
        'source_manifest_sha256': digest(old_manifest),
        'source_files_match_old_manifest': True,
        'isolated_project': TARGET.as_posix(),
        'editor_script_sha256': digest(ROOT / 'Scripts/RenderRokUnityMountainFGammaV2.cs'),
        'probe_shader_sha256': digest(shader),
        'compiler_exit_code': checked.returncode,
        'unity_started': False,
        'product_modified': False,
        'scope': 'Existing isolated source snapshot plus V2 GPU readback exporter; old report and UE assets untouched.',
    }
    (OUT / 'prepare.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if checked.returncode:
        raise RuntimeError(f'Unity 2019 C# compile failed: {validation / "compiler.log"}')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
