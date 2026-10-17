"""Validate independent prepared source and optional native results; no engine."""
import hashlib,json
from pathlib import Path
from PIL import Image
from BuildRokUnityGroveGamma import ROOT,OUT

def main():
    data=json.loads((OUT/'manifest.json').read_text());prep=json.loads((OUT/'prepare.json').read_text())
    assert Path(prep['project']).resolve()==(ROOT/'Saved/UnityGroveGammaProbe').resolve()
    assert data['dxf']==1509 and [s['name'] for s in data['samples']]==['day','dusk','night']
    for row in data['source_files']+prep['copies']:
        assert hashlib.sha256((ROOT/row.get('path',row.get('source'))).read_bytes()).hexdigest()==row['sha256']
    assert hashlib.sha256((ROOT/'Saved/RokUnityMigration/GroveCenterScale/manifest.json').read_bytes()).hexdigest()==data['center_manifest_sha256']
    for s in data['samples']:
        assert len(s['spriteColor'])==3
        assert all(abs(s['spriteColor'][i]-(s['ambient'][i]+s['directional'][i]*s['intensity']*.75))<1e-12 for i in range(3))
    result=dict(preparation_passed=True,native_checked=False,product_changed=False,alpha_edge_equivalence=False)
    path=OUT/'native_render.json'
    if path.exists():
        native=json.loads(path.read_text());texture=native['texture']
        assert native['unityVersion']=='2019.4.28f1c1' and native['colorSpace']=='Gamma'
        assert native['vertices']==377 and native['triangles']==309 and native['treeRot']==0
        assert native['dxf']==1509 and abs(native['treeScale']-data['tree_scale'])<2e-6
        assert len(native['frames'])==3
        for sample,frame in zip(data['samples'],native['frames']):
            assert sample['name']==frame['name'] and all(abs(a-b)<2e-6 for a,b in zip(sample['spriteColor'],frame['spriteColor']))
            assert Image.open(frame['path']).size==(1280,720)
        assert texture['gpuAlphaZero']+texture['gpuAlphaOpaque']+texture['gpuAlphaPartial']==texture['width']*texture['height']
        assert Image.open(OUT/'native_imported_texture.png').size==(texture['width'],texture['height'])
        result.update(native_checked=True,texture=texture)
    result['scope']='Source hashes/lighting/native metadata gates only. Actual three-image visual and opaque-interior numerical comparison remain required; alpha edges and UE product are not accepted.'
    (OUT/'verify.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
