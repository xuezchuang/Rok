"""Validate source-backed snapshots; native output remains a separate gate."""
import json,hashlib,math
from PIL import Image
from BuildRokUnityVillageDayNight import ROOT,OUT,lighting

def main():
    data=json.loads(OUT.read_text(encoding='utf-8'))
    assert data['schema']=='rok.village.day_night.v1' and data['contract']['snapshot_only']
    assert len(data['presets'])==4 and len(data['samples'])==24 and data['loop_seconds']==2400
    assert data['phase_durations']==[57,3,1677,3,57,3,597,3]
    records=data['sources']+[data['source_village']]+[m['source'] for m in data['materials'].values()]+[m['mask'] for m in data['masks']]
    for record in records: assert hashlib.sha256((ROOT/record['path']).read_bytes()).hexdigest()==record['sha256']
    assert len(data['masks'])==13
    for record in data['masks']:
        with Image.open(ROOT/record['mask']['path']) as image: assert image.convert('RGBA').getchannel('B').getextrema()[1]==0
        assert record['blue_max']==0
    assert data['materials']['night']['color1']==[1,1,1,1] and data['materials']['night']['color2']==[1,1,1,1]
    assert data['materials']['night']['render_queue_override']==2996
    night=lighting(data['presets'][3],data['presets'][3],1)
    assert max(abs(a-b) for a,b in zip(night,[.3101465,.75661655,1.434704425]))<1e-8
    scenarios={s['name']:s for s in data['scenarios']}
    cases=set()
    for sample in data['samples']:
        scenario=scenarios[sample['scenario']]
        expected=scenario['requested_night'] if sample['elapsed']>=sample['fixed_delay'] else scenario['initial_night']
        assert sample['resolved_night']==expected
        expected_color=lighting(data['presets'][sample['from_preset']],data['presets'][sample['to_preset']],sample['elapsed']/sample['duration'])
        assert max(abs(a-b) for a,b in zip(expected_color,sample['sprite_color']))<1e-10
        cases.add((sample['scenario'],sample['fixed_delay'],sample['elapsed']))
    assert len(cases)==24
    # Midpoint with changing intensity rejects an incorrect Lerp of final SpriteColor.
    p0,p1=data['presets'][:2];middle=lighting(p0,p1,.5)
    wrong=[(a+b)*.5 for a,b in zip(lighting(p0,p0,0),lighting(p1,p1,1))]
    assert max(abs(a-b) for a,b in zip(middle,wrong))>.001
    result={'passed':True,'snapshots':24,'mask_blue_zero_items':13,'stable_night_sprite_color':night,'independent_nonlinear_midpoint':middle,'source_only':True,'native_gate':'Not run by this verifier','limits':data['contract']}
    OUT.with_name('unity_village_day_night_verify.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('PASS: 24 snapshots, original hashes, 13 zero-blue masks and independent lighting boundary checks')
if __name__=='__main__':main()
