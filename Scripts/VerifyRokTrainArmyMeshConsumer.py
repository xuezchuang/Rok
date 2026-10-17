"""Source/consumer contract checks only; UE runtime and visual acceptance remain separate."""
import hashlib,json,math,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    base=ROOT/'Saved/RokUnityMigration/TrainArmy'
    report=json.loads((base/'export_report.json').read_text(encoding='utf-8'))
    catalog=json.loads((ROOT/'Saved/RokUnityMigration/unity_gameplay_catalog.json').read_text(encoding='utf-8-sig'))
    rows={r['id']:r for r in catalog['troops']}
    verified=[]
    for entry in report['results']:
        if entry.get('status')!='exported':continue
        data=(ROOT/entry['output']).read_bytes()
        assert hashlib.sha256(data).hexdigest()==entry['sha256']
        mesh=json.loads(data)
        assert rows[mesh['arms_id']]['show_asset']==mesh['arms_show']==entry['arms_show']
        assert mesh['schema']=='rok.train_army.spine_mesh.v1' and mesh['source_scale']==.01
        assert mesh['pma']['exported_rgba']=='straight attachment*slot*skeleton tint; consumer must apply source PMA once'
        v=i=0
        for frame in mesh['frames']:
            for draw in frame['draws']:
                assert draw['blend']=='Normal'
                assert all(math.isfinite(x) and abs(x)<=10000 for x in draw['xy'])
                assert all(math.isfinite(x) and 0<=x<=1 for x in draw['uv'])
                assert len(draw['xy'])<=65534 and len(draw['xy'])==len(draw['uv'])
                v+=len(draw['xy'])//2;i+=len(draw['triangles'])
        assert v<=2000000 and i<=6000000
        verified.append(mesh['arms_id'])
    assert len(verified)==29
    canvas=(ROOT/'ref/client-unity/Assets/Skyunion/RunTime/UIManager/Resources/UIRoot.prefab').read_text(encoding='utf-8-sig')
    assert 'm_ReferencePixelsPerUnit: 100' in canvas
    project=(ROOT/'ref/client-unity/ProjectSettings/ProjectSettings.asset').read_text(encoding='utf-8-sig')
    assert 'm_ActiveColorSpace: 0' in project
    shader=(ROOT/'ref/client-unity/Assets/Third/Spine/spine-unity/Modules/SkeletonGraphic/Shaders/Spine-SkeletonGraphic.shader').read_text(encoding='utf-8-sig')
    assert 'Blend One OneMinusSrcAlpha' in shader
    cpp=(ROOT/'Source/Rok/RokTrainArmyMeshWidget.cpp').read_text(encoding='utf-8-sig')
    for token in ['Texture->SRGB=false','ESlateDrawEffect::PreMultipliedAlpha|ESlateDrawEffect::NoGamma','PremultipliedTint','show_asset','Source missing or SHA256 changed']:
        assert token in cpp,token
    assert 'RokCommanderCamera' not in cpp
    native=json.loads((base/'native_render.json').read_text()) if (base/'native_render.json').exists() else json.loads((base/'NativeReference/native_render.json').read_text())
    max_xy=max_uv=0
    for c in native['captures']:
        mesh=json.loads((base/f"arms_{c['armsId']}_idle_mesh.json").read_text(encoding='utf-8'))
        frame=mesh['frames'][round(c['time']/mesh['duration']*len(mesh['frames']))%len(mesh['frames'])]
        xy=[p for d in frame['draws'] for p in zip(d['xy'][::2],d['xy'][1::2])]
        uv=[p for d in frame['draws'] for p in zip(d['uv'][::2],d['uv'][1::2])]
        assert len(xy)==len(c['vertices']) and len(uv)==len(c['uv'])
        for p,n in zip(xy,c['vertices']):max_xy=max(max_xy,abs(p[0]*100-n['x']),abs(p[1]*100-n['y']))
        for p,n in zip(uv,c['uv']):max_uv=max(max_uv,abs(p[0]-n['x']),abs(1-p[1]-n['y']))
    assert max_xy<.001 and max_uv<.000001
    out={'source_verified':True,'arms_ids':verified,'native_xy_error':max_xy,'native_uv_error':max_uv,'runtime_verified':False,'visual_verified':False}
    (base/'consumer_verify.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
    print(json.dumps(out))
if __name__=='__main__':main()
