"""Source/hash and Gamma arithmetic gates; actual GPU/alpha intersection is separate."""
import hashlib,json
import numpy as np
from PIL import Image
from PrepareRokUnityGroveGammaProduct import ROOT,OUT,CODE
from rok_data_paths import resolve

def main():
    spec=json.loads((OUT/'product_input.json').read_text());native=json.loads((OUT/'native_render.json').read_text())
    for row in spec['input_files']+[spec['producer']]:assert hashlib.sha256((resolve(row['path'])).read_bytes()).hexdigest()==row['sha256']
    assert spec['shader_code']==CODE and spec['mode']=='unity_decoded_1024'
    source=np.asarray(Image.open(OUT/'native_imported_texture.png').convert('RGBA'),dtype=np.float64)/255
    rgb=source[:,:,:3];opaque=source[:,:,3]==1
    decode=lambda x:np.where(x<=.04045,x/12.92,((x+.055)/1.055)**2.4)
    encode=lambda x:np.where(x<=.0031308,x*12.92,1.055*x**(1/2.4)-.055)
    rows=[]
    for frame in native['frames']:
        light=np.asarray(frame['spriteColor']);expected=rgb*light;linear=decode(expected)
        error=np.abs(encode(linear)-expected)[opaque]
        wrong=np.abs(encode(decode(rgb)*light)-expected)[opaque]
        assert error.max()<1e-12 and wrong.max()>.01
        rows.append(dict(name=frame['name'],opaque_atlas_texels=int(opaque.sum()),correct_roundtrip_max_error=float(error.max()),old_order_max_error=float(wrong.max())))
    result=dict(passed=True,rows=rows,actual_ue_texture_verified=False,actual_old_alpha_intersection_verified=False,scope='All actual Unity decoded opaque atlas texels; no claim of on-screen visible coverage. UE internal pixel acceptance additionally requires actual old opacity255 and decoded source alpha255 intersection.')
    (OUT/'product_verify.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
