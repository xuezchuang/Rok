"""Verify completed native source snapshots; must run after Unity reports completion."""
import json,hashlib
from pathlib import Path
from PIL import Image
from BuildRokUnityVillageDayNight import ROOT,OUT

def main():
    source=json.loads(OUT.read_text(encoding='utf-8'))
    prepared=json.loads(OUT.with_name('unity_village_day_night_prepare.json').read_text(encoding='utf-8'))
    for record in prepared['source_copies']:
        assert hashlib.sha256((ROOT/record['copy']).read_bytes()).hexdigest()==record['sha256']
        assert hashlib.sha256((ROOT/record['source']).read_bytes()).hexdigest()==record['sha256']
    directory=ROOT/'Saved/RokUnityMigration/VillageDayNightReference'
    native=json.loads((directory/'native_render.json').read_text(encoding='utf-8'))
    assert native['unityVersion']=='2019.4.28f1c1' and native['colorSpace']=='Gamma' and native['graphicsDevice']!='Null Device'
    expected={s['name']:s for s in source['samples']}
    assert len(native['frames'])==24 and {f['sample'] for f in native['frames']}==set(expected)
    source_ids={m['transform_id'] for m in source['masks']};frames=[]
    for frame in native['frames']:
        sample=expected[frame['sample']]
        assert abs(frame['elapsed']-sample['elapsed'])<1e-6 and abs(frame['fixedDelay']-sample['fixed_delay'])<1e-6
        assert frame['resolvedNight']==sample['resolved_night'] and frame['nightMaterials']==(13 if sample['resolved_night'] else 0)
        assert len(frame['active'])==13 and set(frame['active'])==source_ids and frame['dxf']==480
        error=max(abs(a-b) for a,b in zip(frame['spriteColor'],sample['sprite_color']))
        assert error<2e-6
        path=Path(frame['path'])
        if not path.is_absolute(): path=ROOT/path
        with Image.open(path) as image:
            assert image.size==(1280,720)
            colors=image.convert('RGB').getcolors(1280*720); assert colors and len(colors)>1000
        frames.append({'sample':frame['sample'],'max_color_error':error,'night_materials':frame['nightMaterials'],'image_sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
    result={'passed':True,'frames':frames,'scope':native['scope'],'remaining':'UE actual material/render comparison; source clock/coroutine behavior and full map night rendering remain outside this snapshot verification.'}
    (directory/'native_verify.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('PASS: 24 native source snapshots, 13 exact source IDs, materials/light parameters and original copied assets')
if __name__=='__main__':main()
