"""Pin Unity GPU-decoded atlas as a distinct, reproducible RGB-only product input."""
import hashlib,json
from pathlib import Path
from rok_data_paths import resolve
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/RokUnityMigration/GroveGamma'
MATERIAL='/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_GroveCenterScale'
RAW='/Game/RokUnityWorldMapPrototype/Textures/T_GroveGamma_UnityDecoded1024'
CODE='''float3 g = RawRGB * Lighting;
return float3(g.r <= 0.04045 ? g.r / 12.92 : pow((g.r + 0.055) / 1.055, 2.4),
              g.g <= 0.04045 ? g.g / 12.92 : pow((g.g + 0.055) / 1.055, 2.4),
              g.b <= 0.04045 ? g.b / 12.92 : pow((g.b + 0.055) / 1.055, 2.4));'''

def main():
    native=json.loads((OUT/'native_render.json').read_text());source=json.loads((OUT/'manifest.json').read_text())
    for row in source['source_files']:
        assert hashlib.sha256((resolve(row['path'])).read_bytes()).hexdigest()==row['sha256']
    texture=native['texture']
    assert native['colorSpace']=='Gamma' and texture['format']=='DXT5' and texture['graphicsFormat']=='RGBA_DXT5_UNorm'
    image=Image.open(OUT/'native_imported_texture.png');assert image.mode=='RGBA' and image.size==(1024,1024)
    assert texture['width']==1024 and texture['height']==1024 and texture['mips']==1
    hist=image.getchannel('A').histogram()
    assert [hist[0],hist[255],sum(hist[1:255])]==[texture['gpuAlphaZero'],texture['gpuAlphaOpaque'],texture['gpuAlphaPartial']]
    paths=['manifest.json','native_render.json','native_imported_texture.png']
    result=dict(schema='rok.grove_gamma.product_input.v1',mode='unity_decoded_1024',material=MATERIAL,raw_texture=RAW,
        input_files=[dict(path=(OUT/p).relative_to(ROOT).as_posix(),sha256=hashlib.sha256((OUT/p).read_bytes()).hexdigest()) for p in paths],
        producer=dict(path='Scripts/RenderRokUnityGroveGamma.cs',sha256=hashlib.sha256((ROOT/'Scripts/RenderRokUnityGroveGamma.cs').read_bytes()).hexdigest()),
        native_texture=texture,shader_code=CODE,
        raw_asset_settings=dict(srgb=False,compression='TC_VECTOR_DISPLACEMENTMAP (uncompressed RGBA8)',mips=1,filter='bilinear',address='clamp'),
        provenance='Original Unity atlas imported with original meta to1024 DXT5, GPU Graphics.Blit to matching1024 RGBA8 Linear RenderTexture, ReadPixels to PNG. This is a texture decode readback, not a scene screenshot or resampled camera image.',
        accepted_scope='Raw RGB sample times UnitySpriteColor in Gamma, then exact sRGB decode for UE emissive. Existing source-center WPO and old opacity texture/input remain untouched.',
        alpha_contract='New raw alpha is intentionally unused. Existing previously accepted opacity texture/sample remains, so this slice does not silently change compression-dependent edge coverage.',
        repeatability='Run only as post-step after CreateRokUnityGroveCenterScale. Re-running center generator resets its material RGB path; re-apply this script explicitly.',
        open_gates=['Actual UE raw texture format/bytes and three-state rendered internal colors','Unity Gamma versus UE linear alpha compositing','Original alpha texture versus1024 DXT5 alpha discrepancy remains deliberately unchanged'])
    (OUT/'product_input.json').write_text(json.dumps(result,indent=2)+'\n');print('ROK_GROVE_GAMMA_PRODUCT_INPUT_READY')
if __name__=='__main__':main()
