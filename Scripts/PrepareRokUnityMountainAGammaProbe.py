"""Prepare original prefab/shader/FBX in an isolated Unity project; never launch."""
import hashlib
import json
import re
import shutil
from pathlib import Path
from rok_data_paths import DATA_ROOT, label

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT/'ref/client-unity'
TARGET = ROOT/'Saved/UnityMountainAGammaProbe'
OUT = ROOT/'Saved/RokUnityMigration/MountainAGamma'


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
        copied.append({'path': label(src), 'sha256': hashlib.sha256(src.read_bytes()).hexdigest()})
    todo = [SOURCE/'Assets/BundleAssets/land/Tile/AutoMake/Mountain/build_mountain_A.prefab']
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
            copied.append({'path': label(src), 'sha256': hashlib.sha256(src.read_bytes()).hexdigest(), 'stub': 'Pool ownership only; no instantiation service in isolated opaque body render.'})
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
                ignored_material_slots.append({'material': label(src), 'unused_guids': sorted(set(re.findall(r'guid: ([0-9a-f]{32})', content))-set(relevant)), 'reason': 'Source shader only samples _MainTex; material bytes are unchanged.'})
            else:
                relevant = re.findall(r'guid: ([0-9a-f]{32})', content)
            for guid in relevant:
                if guid.startswith('0000000000000000'):
                    continue
                if guid not in index:
                    raise RuntimeError(f'Unresolved source GUID {guid}')
                todo.append(index[guid])
    copy(ROOT/'Scripts/RenderRokUnityMountainAGamma.cs', TARGET/'Assets/Editor/RenderRokUnityMountainAGamma.cs')
    for relative in ('ProjectSettings/ProjectSettings.asset', 'ProjectSettings/ProjectVersion.txt'):
        copy(SOURCE/relative, TARGET/relative)
    packages = json.loads((SOURCE/'Packages/manifest.json').read_text(encoding='utf-8-sig'))
    (TARGET/'Packages').mkdir(exist_ok=True)
    (TARGET/'Packages/manifest.json').write_text(json.dumps({'dependencies': {k:v for k,v in packages['dependencies'].items() if k.startswith('com.unity.modules.')}}, indent=2), encoding='utf-8')
    lighting_path = DATA_ROOT / 'unity_village_day_night.json'
    lighting = json.loads(lighting_path.read_text(encoding='utf-8'))
    def rgb(a, b, t):
        x,y=lighting['presets'][a],lighting['presets'][b]
        intensity=x['intensity']+(y['intensity']-x['intensity'])*t
        return [x['ambient'][i]+(y['ambient'][i]-x['ambient'][i])*t + (x['directional'][i]+(y['directional'][i]-x['directional'][i])*t)*intensity*.75 for i in range(3)]
    samples = [{'name': n, 'spriteColor': rgb(a,b,t), 'fromPreset': a, 'toPreset': b, 'fraction': t}
               for n,a,b,t in [('stable_day',1,1,1), ('dusk_to_night_mid',2,3,.5), ('stable_night',3,3,1)]]
    report = {'schema': 'rok.mountain_a_gamma.v1', 'source_files': copied, 'unused_material_slots': ignored_material_slots,
              'lighting_manifest': {'path': label(lighting_path), 'sha256': hashlib.sha256(lighting_path.read_bytes()).hexdigest()},
              'samples': samples, 'isolated_project': str(TARGET),
              'scope': 'Original prefab body only; source edge disabled to match existing UE body-only route. Pool component stub; no full GameView, network time or LOD migration.',
              'sampling_gate': 'Original Unity texture/meta/mips/compression retained. UE raw texture importer settings recorded separately; cross-engine mip/compression identity remains a visual gate.'}
    (OUT/'manifest.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'PREPARED {TARGET}; {len(seen)} source dependencies; no engine launched')


if __name__ == '__main__':
    main()
