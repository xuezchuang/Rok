"""Verify native six-text glyph provenance, topology and source rich colors."""
from pathlib import Path
import argparse, hashlib, json, math, re
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
def verify(directory):
    directory=Path(directory).resolve();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    inputs=json.loads((directory/'ReferenceInputs.json').read_text(encoding='utf-8'))
    report=json.loads((directory/'source_text_meshes.json').read_text(encoding='utf-8'))
    assert inputs['schema']=='rok.barracks.source_text.inputs.v1' and report['schema']=='rok.barracks.source_text.v1'
    assert report['inputSha256']==sha(directory/'ReferenceInputs.json')
    for source in inputs['sources']: assert sha(ROOT/source['path'])==source['sha256'],source['path']
    assert inputs['renderer_sha256']==inputs['copied_renderer_sha256']
    specs={s['key']:s for s in inputs['sourceTexts']}
    assert set(specs)=={'level_before','level_after','label_0','label_1','value_0','value_1'}
    assert len(report['texts'])==6 and {t['key'] for t in report['texts']}==set(specs)
    colors={}
    for t in report['texts']:
        s=specs[t['key']]
        assert t['text']==s['text'] and t['fontSize']==s['fontSize'] and t['style']==s['style'] and t['alignment']==s['alignment']
        assert abs(t['width']-s['width'])<.01 and abs(t['height']-s['height'])<.01 and t['pixelsPerUnit']==1 and t['lineCount']==1
        count=len(t['xy']); assert count>0 and count%4==0 and len(t['uv'])==len(t['colors'])==count and len(t['triangles'])==count//4*6
        assert all(isinstance(i,int) and 0<=i<count for i in t['triangles'])
        assert all(math.isfinite(v) and abs(v)<10000 for p in t['xy'] for v in p.values())
        assert all(math.isfinite(v) and 0<=v<=1 for p in t['uv'] for v in p.values())
        assert all(all(isinstance(c[k],int) and 0<=c[k]<=255 for k in ['r','g','b','a']) for c in t['colors'])
        colors[t['key']]=sorted({tuple(c[k] for k in ['r','g','b','a']) for c in t['colors']})
        glyphs={g['codepoint']:g for g in t['glyphs']};plain=re.sub('<.*?>','',t['text'])
        assert set(map(ord,plain))-set(glyphs)==set()
        for i in range(0,count,4):
            uv=t['uv'][i:i+4]
            assert any(all(any(abs(p['x']-g[k]['x'])<1e-6 and abs(p['y']-g[k]['y'])<1e-6 for k in ['uvBottomLeft','uvBottomRight','uvTopLeft','uvTopRight']) for p in uv) for g in glyphs.values()),(t['key'],i)
        atlas=(directory/t['atlas']).resolve(); assert atlas.parent==directory/'SourceTextAtlases' and sha(atlas)==t['atlasSha256']
        image=Image.open(atlas).convert('RGBA');assert max(image.size)<=8192 and image.getchannel('A').getextrema()[1]>0
    for key,cs in colors.items():
        expected={(255,255,255,255)} if key.startswith('label') else {(226,209,155,255)}
        if key.startswith('value'):expected.add((0,128,0,255))
        assert set(cs)==expected,(key,cs)
    result={'passed':True,'text_count':6,'atlas_count':len({t['atlas'] for t in report['texts']}),'input_sha256':report['inputSha256'],'colors':colors,'scope':'Native per-glyph provenance and source colors; actual UE painting and visual comparison remain open.'}
    (directory/'glyph_verify.json').write_text(json.dumps(result,indent=2),encoding='utf-8');return result
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('directory');print(json.dumps(verify(p.parse_args().directory),indent=2))
