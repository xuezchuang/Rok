"""Source identities/mip decode/native3 evidence; never launches engine."""
import hashlib,json
from pathlib import Path
from rok_data_paths import resolve
from PIL import Image
from BuildRokUnityMountainFGamma import ROOT,OUT

def main():
    data=json.loads((OUT/'manifest.json').read_text())
    assert data['schema']=='rok.mountain_f_gamma.v1' and len(data['instances'])==18
    for row in data['source_inputs']+data['source_files']+[data['diagnostic_mip_shader']]:assert hashlib.sha256((resolve(row['path'])).read_bytes()).hexdigest()==row['sha256']
    assert [x['name'] for x in data['samples']]==['stable_day','stable_dusk','stable_night']
    report=dict(prepared=True,native_checked=False,product_modified=False)
    path=OUT/'native_render.json'
    if path.exists():
        native=json.loads(path.read_text());assert native['unityVersion']=='2019.4.28f1c1' and native['colorSpace']=='Gamma'
        assert native['meshGuid']==data['body']['mesh_guid'] and native['meshLocalId']==data['body']['mesh_file_id'] and native['materialGuid']==data['body']['material_guid'] and native['subMeshes']==1
        assert 'build_mountain_F_edge' in native['excludedRenderers']
        assert len(native['sourcePositions'])==native['vertices']==len(native['sourceUV0']) and len(native['sourceTriangles'])==3*native['triangles']
        assert len(native['frames'])==3
        for s,f in zip(data['samples'],native['frames']):
            assert s['name']==f['name'] and len(f['spriteColor'])==3 and max(abs(a-b) for a,b in zip(s['spriteColor'],f['spriteColor']))<2e-6
            assert Image.open(f['path']).size==(1280,720)
        assert len(native['texture']['decodedMips'])==native['mipmapCount']
        for level,mip in enumerate(native['texture']['decodedMips']):
            image=Image.open(mip['path']);hist=image.convert('RGBA').getchannel('A').histogram()
            assert image.size==(max(1,native['textureWidth']>>level),max(1,native['textureHeight']>>level))
            assert sum(hist[1:255])==0, 'DXT1 decoder returned fractional alpha (unbound texture/invalid decode)'
            if level==0:
                extrema=image.convert('RGB').getextrema();assert max(b for a,b in extrema)-min(a for a,b in extrema)>=16, 'F mip0 is degenerate'
            assert mip['level']==level and [hist[0],hist[255],sum(hist[1:255])]==[mip['alphaZero'],mip['alphaOpaque'],mip['alphaPartial']]
        report.update(native_checked=True,texture_format=native['textureFormat'],texture_size=[native['textureWidth'],native['textureHeight']],mips=native['mipmapCount'])
    report['scope']='Native metadata/hash/artifact gates. Actual source images and mip/raster color comparisons remain required; Fedge/LOD and UE product not accepted.'
    (OUT/'verify.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':
    try:main()
    except Exception as error:
        (OUT/'verify.json').write_text(json.dumps(dict(prepared=True,native_checked=False,passed=False,failure=str(error),product_modified=False),indent=2)+'\n')
        raise
