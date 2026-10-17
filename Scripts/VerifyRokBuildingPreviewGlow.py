"""Validate real PlayMode glow alpha/rotation and same-frame additive layer captures."""
import argparse,hashlib,json,math,sys
from pathlib import Path
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def verify(path):
    p=Path(path).resolve();assert p.is_relative_to((ROOT/'Saved/UnityBuildingPreviewGlowProbe/Artifacts').resolve())
    e=json.loads(p.read_text(encoding='utf-8-sig'));ip=p.parent/'ReferenceInputs.json';inputs=json.loads(ip.read_text(encoding='utf-8-sig'));fail=[]
    def check(ok,why):
        if not ok:fail.append(why)
    check(e.get('nativeRendered') and e.get('playMode') and e.get('visibilityRestored') and not e.get('failure'),'native PlayMode/restore')
    check(e.get('inputSha256')==sha(ip),'input hash')
    for s in inputs['sources']:check(sha(ROOT/s['path'])==s['sha256'],'source hash '+s['path'])
    trace=e.get('trace',[]);check(len(trace)>6,'natural per-frame trace');max_alpha=0;max_rotation=0;screen_directions=[]
    def alpha(t,vals):
        t=(t%1)*2;idx=min(1,int(t));u=t-idx;return vals[idx]+(vals[idx+1]-vals[idx])*(3*u*u-2*u*u*u)
    def quat(q):return np.array([q['x'],q['y'],q['z'],q['w']],float)
    def multiply(a,b):return np.r_[a[3]*b[:3]+b[3]*a[:3]+np.cross(a[:3],b[:3]),a[3]*b[3]-np.dot(a[:3],b[:3])]
    for n,s in enumerate(trace):
        check(len(s['layers'])==2,'two actual layers')
        for j,l in enumerate(s['layers']):
            check(l['shader']=='UI/Add','actual source additive shader')
            check(all(abs(l['tint'][k]-1)<.0001 for k in ['r','g','b']),'source white RGB tint')
            check(len(l['vertices'])==4 and len(l['triangles'])==6,'actual native layer quad')
            expected=alpha(s['animatorNormalizedTime'],[.15,.06,.15] if j==0 else [.1,.2,.1]);max_alpha=max(max_alpha,abs(l['tint']['a']-expected))
            if n:
                prev=trace[n-1];check(s['frame']>prev['frame'] and s['time']>=prev['time'],'trace order')
                a=quat(prev['layers'][j]['localRotation']);a[:3]*=-1;b=quat(l['localRotation']);delta=multiply(a,b);angle=math.degrees(2*math.atan2(delta[2],delta[3]));angle=(angle+180)%360-180
                max_rotation=max(max_rotation,abs(angle-(4 if j==0 else 3)*s['deltaTime']))
    check(max_alpha<.002,'source Animator Hermite alpha mismatch')
    check(max_rotation<.02,'source DeemoRotate actual local quaternion step mismatch')
    if trace:
        check(trace[-1]['animatorNormalizedTime']-trace[0]['animatorNormalizedTime']>=1,'complete natural 2 second cycle')
        check(len({int((s['animatorNormalizedTime']%1)*4) for s in trace})==4,'all four alpha loop phase quarters observed')
        for j in range(2):
            angles=[]
            for s in [trace[0],trace[-1]]:
                a,b=s['layers'][j]['screenVertices'][:2];angles.append(math.atan2(b['y']-a['y'],b['x']-a['x']))
            screen_directions.append((angles[1]-angles[0]+math.pi)%(2*math.pi)-math.pi)
        check(screen_directions[0]*screen_directions[1]<0,'Y180 mirror produces opposite projected rotation directions')
    caps=e.get('captures',[]);check(len(caps)==24,'six times four same-frame captures');metrics=[]
    for index in range(6):
        group={c['name']:c for c in caps if c['sample']==index};check(set(group)=={'none','both','layer0','layer1'},'complete layer group')
        if len(group)!=4:continue
        check(len({c['state']['frame'] for c in group.values()})==1 and len({c['state']['time'] for c in group.values()})==1,'same-frame layer contribution')
        pixels={}
        for name,c in group.items():
            q=p.parent/c['png'];assert q.resolve().parent==p.parent;check(sha(q)==c['sha256'],'capture hash');im=Image.open(q).convert('RGB');check(im.size==(1440,810),'capture dimensions');pixels[name]=np.array(im,dtype=float)
        a=pixels['layer0']-pixels['none'];b=pixels['layer1']-pixels['none'];both=pixels['both']-pixels['none']
        changed0=int(np.any(abs(a)>2,axis=2).sum());changed1=int(np.any(abs(b)>2,axis=2).sum());check(changed0>100 and changed1>100,'actual individual layer pixels')
        # Quantization and saturation are measured, not a fabricated pixel-identical threshold.
        unclipped=np.all(pixels['both']<254,axis=2);error=abs(both-a-b);metrics.append({'sample':index,'layer0_pixels':changed0,'layer1_pixels':changed1,'unclipped_additive_mae':float(error[unclipped].mean())})
    result={'passed':not fail,'failures':fail,'evidence_sha256':sha(p),'max_alpha_error':max_alpha,'max_local_rotation_error_degrees':max_rotation,'projected_rotation_delta_radians':screen_directions,'pixel_metrics':metrics,'scope':'Source native actual PlayMode only; additive pixel errors descriptive, UE parity and shared window lifecycle remain separate gates.'}
    (p.parent/'verify.json').write_text(json.dumps(result,indent=2),encoding='utf-8');return result
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('evidence');a=parser.parse_args();r=verify(a.evidence);print(json.dumps(r,indent=2));sys.exit(0 if r['passed'] else 1)
