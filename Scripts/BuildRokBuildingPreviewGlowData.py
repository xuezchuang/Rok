"""Build immutable-native-backed sampling data without changing any prior producer/input."""
import hashlib,json,math
from pathlib import Path
import numpy as np
from rok_data_paths import DATA_ROOT, label
ROOT=Path(__file__).resolve().parents[1]
NATIVE=ROOT/'Saved/UnityBuildingPreviewGlowProbe/Artifacts/native_20260912_134632_019/render_evidence.json'
SOURCE=ROOT/'Saved/RokUnityMigration/BuildingPreviewGlowSource/source_glow.json'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rotate(q,v):
    q=np.array(q,float);v=np.array(v,float);return v+2*np.cross(q[:3],np.cross(q[:3],v)+q[3]*v)
def build():
    e=json.loads(NATIVE.read_text());source=json.loads(SOURCE.read_text());verified=json.loads((NATIVE.parent/'verify.json').read_text());assert verified['passed'] and verified['evidence_sha256']==sha(NATIVE)
    layers=[];traces=[]
    for j,s in enumerate(source['layers']):
        actual=e['trace'][1]['layers'][j];q=[actual['localRotation'][k] for k in ['x','y','z','w']];scale=s['scale'];centers=[]
        for v,p in zip(actual['vertices'],actual['screenVertices']):
            transformed=rotate(q,[v[k]*scale[a] for a,k in enumerate(['x','y','z'])]);centers.append([p['x']-transformed[0],p['y']+transformed[1]])
        center=np.mean(centers,axis=0);assert np.max(np.abs(np.array(centers)-center))<.001
        curve=next(c for c in source['curves'] if c['path']==s['name'])
        layers.append({'name':s['name'],'center':center.tolist(),'size':s['size'],'scale':scale,'initial_rotation':s['rotation_quaternion'],'degrees_per_second':s['rotate_speed'][2],'alpha_keys':curve['keys'],'vertices':[[v[k] for k in ['x','y','z']] for v in actual['vertices']],'uv':[[v['x'],v['y']] for v in actual['uv']],'triangles':actual['triangles']})
    for n,state in enumerate(e['trace']):
        traces.append({'time':state['elapsed'],'projection_valid':n>0,'alpha':[l['tint']['a'] for l in state['layers']],'rotation':[[l['localRotation'][k] for k in ['x','y','z','w']] for l in state['layers']],'screen_quad':[[[p['x'],p['y']] for p in l['screenVertices']] for l in state['layers']]})
    files=[SOURCE,NATIVE,NATIVE.parent/'ReferenceInputs.json',NATIVE.parent/'verify.json',Path(__file__).resolve()]
    data={'schema':'rok.building.preview_glow.data.v1','duration':2,'layers':layers,'native_samples':traces,'projection_caveat':'Native first Read preceded first Camera.Render: first absolute screen quad invalid. Geometry uses stabilized trace[1:]; first alpha/local quaternion remain valid. No claim of first-frame absolute projection parity.','sources':[{'path':label(p),'sha256':sha(p)} for p in files]}
    out=DATA_ROOT/'BuildingPreviewGlow';out.mkdir(parents=True,exist_ok=True);(out/'glow_data.json').write_text(json.dumps(data,indent=2),encoding='utf-8');print(json.dumps({'layers':layers,'samples':len(traces)}));return data
if __name__=='__main__':build()
