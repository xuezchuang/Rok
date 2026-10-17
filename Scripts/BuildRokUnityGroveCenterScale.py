"""Extract only Grove_co_tree_01_adorning raw mesh/float COLOR centers."""
import hashlib
import json
import math
import re
import struct
from pathlib import Path
from rok_data_paths import resolve
import numpy as np
import PrepareRokUnityWorldMapAssets as source

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/RokUnityMigration/GroveCenterScale'
PREFAB='Grove_co_tree_01_adorning'


def main():
    mesh=ROOT/f'ref/client-unity/Assets/BundleAssets/land/Mesh/AutoMake/{PREFAB}.asset'
    text=mesh.read_text(encoding='utf-8')
    parsed=source.parse_unity_mesh_asset(mesh,False)
    count=len(parsed['positions'])
    channels=[tuple(map(int,m)) for m in re.findall(r'- stream:\s*(\d+)\s*\n\s*offset:\s*(\d+)\s*\n\s*format:\s*(\d+)\s*\n\s*dimension:\s*(\d+)',text)]
    assert len(channels)>=5 and channels[3][0]==0 and channels[3][2:]==(0,4)
    raw=bytes.fromhex(re.search(r'^\s*_typelessdata:\s*([0-9a-fA-F]+)\s*$',text,re.M)[1])
    assert len(raw)%count==0
    stride=len(raw)//count
    colors=np.array([struct.unpack_from('<4f',raw,i*stride+channels[3][1]) for i in range(count)],dtype=np.float32)
    # Reverse the established OBJ transport only, returning original Unity xyz.
    positions=np.array([[x/100,z/100,-y/100] for x,y,z in parsed['positions']],dtype=np.float32)
    centers=colors[:,:3]*np.float32(180)
    unique=np.unique(centers,axis=0)
    assert count==377 and len(unique)==16 and len(parsed['triangles'])==309
    triangles=[[a,c,b] for _,a,b,c in parsed['triangles']]
    assert all(np.array_equal(centers[a],centers[b]) and np.array_equal(centers[a],centers[c]) for a,b,c in triangles), 'Triangle crosses source COLOR centers'
    curvepath=resolve(source.TREE_SCALE_CURVE_SOURCE)
    keys=source.parse_unity_curve(curvepath,'m_tree_scale_curve')
    curve_text=curvepath.read_text(encoding='utf-8').split('  m_tree_scale_curve:',1)[1].split('  m_unit_scale_curve:',1)[0]
    weights=re.findall(r'inWeight:\s*(\S+)\s*\n\s*outWeight:\s*(\S+)',curve_text)
    assert len(weights)==len(keys)
    for key,(iw,ow) in zip(keys,weights):
        key['in_weight']=float(iw);key['out_weight']=float(ow)
    def evaluate(dxf):
        # This source's final infinite-tangent interval is a constant step.
        # Preserve original tangents/weights below for native AnimationCurve check.
        if keys[-2]['dxf']<=dxf<=keys[-1]['dxf']:
            assert keys[-2]['scale']==keys[-1]['scale'] and math.isinf(keys[-2]['out_slope'])
            return keys[-2]['scale']
        return source.evaluate_unweighted_unity_curve(keys,dxf,'tree')
    samples=[]
    bake=np.float32(source.evaluate_unweighted_unity_curve(keys,1509,'tree'))
    baked=centers+(positions-centers)*bake
    for index,dxf in enumerate((480,900,1509,3000,4200,7000,4200,1509,480)):
        scale=np.float32(evaluate(dxf))
        expected=centers+(positions-centers)*scale
        reconstructed=centers+(baked-centers)*(scale/bake)
        old=baked*(scale/bake)
        samples.append({'name':f'{index:02d}_dxf{dxf}','dxf':dxf,'scale':float(scale),'baked_reconstruction_max_unity_error':float(np.abs(expected-reconstructed).max()),'old_actor_scale_max_unity_error':float(np.abs(old-expected).max())})
    half=colors.astype(np.float16).astype(np.float32)[:,:3]*np.float32(180)
    files=[mesh,Path(str(mesh)+'.meta'),ROOT/f'ref/client-unity/Assets/BundleAssets/land/Tile/{PREFAB}.prefab',ROOT/'ref/client-unity/Assets/BundleAssets/land/Shaders/customlandmaptree-16-nomask.shader',curvepath,ROOT/'ref/client-unity/Assets/Scripts/Client/LodManager/LevelDetailScalerManager.cs']
    world=json.loads((ROOT/'Saved/RokUnityMigration/unity_world_map.json').read_text(encoding='utf-8'))
    instances=[{'tile':[t['x'],t['y']],'source_instance':i} for t in world['prototype_window']['tiles'] for i in t['instances'] if i['prefab_id']==PREFAB]
    assert len(instances)==11
    native_keys=[dict(k,in_slope=str(k['in_slope']),out_slope=str(k['out_slope'])) for k in keys]
    report={'schema':'rok.grove_center_scale.v1','prefab':PREFAB,'source_files':[{'path':p.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in files],
        'positions':positions.tolist(),'uv0':parsed['uvs'],'source_color_rgba':colors.tolist(),'centers_unity':centers.tolist(),'triangles':triangles,'unique_centers_unity':unique.tolist(),'curve':native_keys,'samples':samples,'bake_scale':float(bake),'instances':instances,
        'transport':{'uv0':'original source UV0 retained; engine basis/import conversion must be separately verified','uv1':'source COLOR.rg float32','uv2':'source COLOR.ba float32','source_color':'RGBA float32, preserved losslessly in extra UVs; never quantize to vertex FColor','full_precision_uvs_required':True,'float32_center_roundtrip_max_unity_error':0.0,'half_uv_center_max_unity_error':float(np.abs(half-centers).max())},
        'scope':'One existing prefab/11 placement instances. Per-tree centers stay fixed; only source local vertices change with scale. No changes to source placement, material color/daynight, other vegetation or authoritative state.'}
    OUT.mkdir(parents=True,exist_ok=True);(OUT/'manifest.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'vertices':count,'centers':len(unique),'triangles':len(triangles),'instances':len(instances),'transport':report['transport'],'samples':samples},indent=2))


if __name__=='__main__':main()
