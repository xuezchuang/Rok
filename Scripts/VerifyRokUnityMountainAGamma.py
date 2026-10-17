"""Source/math checks and native same-geometry color comparison; no engine launch."""
import ast
import hashlib
import json
from pathlib import Path
from rok_data_paths import resolve
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'Saved/RokUnityMigration/MountainAGamma'


def main():
    manifest = json.loads((OUT/'manifest.json').read_text(encoding='utf-8'))
    failures = []
    for record in manifest['source_files'] + [manifest['lighting_manifest']]:
        path = resolve(record['path'])
        if hashlib.sha256(path.read_bytes()).hexdigest() != record['sha256']:
            failures.append('Source changed: '+str(path))
    for name in ('CreateRokUnityMountainAGammaMaterial.py', 'PrepareRokUnityMountainAGammaProbe.py', 'VerifyRokUnityMountainAGamma.py', 'AuditRokUnityMountainAGamma.py', 'CreateRokUnityWorldMapPrototype.py'):
        ast.parse((ROOT/'Scripts'/name).read_text(encoding='utf-8-sig'))
    shader = (ROOT/'ref/client-unity/Assets/BundleAssets/land/Shaders/z_cm_slg_mountain.shader').read_text()
    assert 'color.rgb *= _SpriteColor;' in shader
    assert 'm_ActiveColorSpace: 0' in (ROOT/'ref/client-unity/ProjectSettings/ProjectSettings.asset').read_text()
    code = (ROOT/'Scripts/CreateRokUnityMountainAGammaMaterial.py').read_text()
    assert 'RawRGB * Lighting' in code and 'srgb=False' in code and 'SAMPLERTYPE_LINEAR_COLOR' in code
    assert 'g.r <= 0.04045' in code and 'pow((g.r + 0.055) / 1.055, 2.4)' in code
    def decode(x):
        return np.where(x<=.04045, x/12.92, ((x+.055)/1.055)**2.4)
    red = manifest['samples'][2]['spriteColor'][0]
    counterexample = {'raw': .5, 'night_r': red, 'old_linear': float(decode(.5)*red), 'source_equivalent_linear': float(decode(.5*red))}
    assert abs(counterexample['old_linear']-counterexample['source_equivalent_linear'])>.04
    native = OUT/'native_render.json'
    comparisons = []
    native_passed = False
    if native.exists():
        data=json.loads(native.read_text(encoding='utf-8'))
        assert data['colorSpace']=='Gamma' and data['shader']=='custom/landmap/z_cm_slg_mountain'
        assert data['vertices']>0 and data['triangles']>0 and data['mipmapCount']>0
        assert len(data['frames'])==3
        images=[]
        for expected, frame in zip(manifest['samples'],data['frames']):
            assert expected['name']==frame['name']
            assert np.max(np.abs(np.array(expected['spriteColor'])-frame['spriteColor']))<1e-6
            images.append(np.asarray(Image.open(frame['path']).convert('RGB'),dtype=np.float64)/255)
        day=images[0]
        # Exclude unchanged background, saturation, dark pixels and silhouette
        # antialiasing. Native body is opaque, so interior color ratios directly
        # test source multiplication under the exact same camera/mip sample.
        mask=(np.max(np.abs(day-images[-1]),axis=2)>.12)&(day.max(axis=2)<.95)&(day.min(axis=2)>.08)
        interior=mask.copy()
        for dy,dx in ((-1,0),(1,0),(0,-1),(0,1)):
            interior &= np.roll(mask,(dy,dx),(0,1))
        assert int(interior.sum())>1000, 'Insufficient actual opaque mountain pixels'
        for i in (1,2):
            ratio=np.array(data['frames'][i]['spriteColor'])/data['frames'][0]['spriteColor']
            error=np.abs(day*ratio-images[i])[interior]
            maximum=float(error.max())
            comparisons.append({'sample':data['frames'][i]['name'],'interior_pixels':int(interior.sum()),'max_rgb_error':maximum,'mean_rgb_error':float(error.mean()),'tolerance':3/255})
            if maximum>3/255:
                failures.append('Native color ratio mismatch: '+data['frames'][i]['name'])
        native_passed=not failures
    report={'source_and_math_passed':not failures,'native_compared':native.exists(),'native_passed':native_passed,'counterexample':counterexample,'native_comparisons':comparisons,'failures':failures,
            'open_gates':['UE asset upgrade and actual world mountain screenshots','Native versus UE geometry/framing and interior color comparison','Cross-engine mip filtering/compression and display transfer','Continuous day/night regression; no network clock claim']}
    (OUT/'verify.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))
    if failures:
        raise SystemExit(1)


if __name__=='__main__':
    main()
