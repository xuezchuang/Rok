"""Strict source/native data gate before creating UE full-precision UV assets."""
import hashlib,json
import numpy as np
from PIL import Image
from BuildRokUnityGroveCenterScale import ROOT,OUT
from rok_data_paths import resolve


def main():
    d=json.loads((OUT/'manifest.json').read_text(encoding='utf-8'))
    for record in d['source_files']:
        assert hashlib.sha256((resolve(record['path'])).read_bytes()).hexdigest()==record['sha256'],record['path']
    p=np.array(d['positions'],dtype=np.float32);c=np.array(d['source_color_rgba'],dtype=np.float32);center=c[:,:3]*np.float32(180)
    # UV1=rg and UV2=ba store all four original channels in float32.
    uv1=c[:,:2].copy();uv2=c[:,2:].copy();roundtrip=np.column_stack((uv1,uv2));error=float(np.abs(roundtrip-c).max())
    assert error==0 and len(p)==377 and len(np.unique(center,axis=0))==16 and len(d['instances'])==11
    assert max(s['baked_reconstruction_max_unity_error'] for s in d['samples'])<1e-6
    assert d['samples'][0]['old_actor_scale_max_unity_error']>2
    native=OUT/'native_render.json';native_checks=None
    if native.exists():
        n=json.loads(native.read_text(encoding='utf-8'))
        def values(items,keys):return np.array([[v[k] for k in keys] for v in items],dtype=np.float32)
        assert n['shader']=='custom/landmap/tree-16-nomask' and n['colorSpace']=='Gamma'
        pe=float(np.abs(values(n['positions'],'xyz')-p).max());ce=float(np.abs(values(n['colors'],'rgba')-c).max());ue=float(np.abs(values(n['uv0'],'xy')-d['uv0']).max())
        assert max(pe,ce,ue)<1e-6
        assert np.array_equal(np.array(n['triangles']).reshape(-1,3),d['triangles'])
        assert len(n['frames'])==len(d['samples'])==9
        errors=[];images=[]
        for expected,actual in zip(d['samples'],n['frames']):
            assert expected['name']==actual['name'] and expected['dxf']==actual['dxf']
            assert abs(expected['scale']-actual['scale'])<2e-6
            expected_positions=center+(p-center)*np.float32(actual['scale'])
            e=float(np.abs(values(actual['evaluatedLocalPositions'],'xyz')-expected_positions).max());assert e<2e-6
            errors.append(e);images.append(np.asarray(Image.open(actual['path']).convert('RGB')))
        assert np.array_equal(images[0],images[-1]),'Source return to480 differs'
        changed=int(np.any(images[0]!=images[2],axis=2).sum());assert changed>1000,'Source GPU images did not change with scale'
        native_checks={'position_max_error':pe,'color_max_error':ce,'uv0_max_error':ue,'native_cpu_formula_max_error':max(errors),'curve_samples':9,'gpu_changed_pixels_480_vs1509':changed,'gpu_return480_identical':True,'scope':'Native Mesh/AnimationCurve reads and original shader screenshots; native evaluatedLocalPositions remain CPU formula, not GPU vertex readback.'}
    report={'source_passed':True,'float32_uv_transport_rgba_max_error':error,'half_uv_center_error_unity':d['transport']['half_uv_center_max_unity_error'],'native_checked':native.exists(),'native':native_checks,'ue_asset_created':False,'open_gates':['Actual UE imported UV1/UV2 precision and original UV0 correspondence','11-instance map binding/tag-only runtime scale path','Real source-comparison UE zoom sequence and constant per-tree centers']}
    (OUT/'verify.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
