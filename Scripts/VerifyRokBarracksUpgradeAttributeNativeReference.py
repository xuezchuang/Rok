"""Verify actual native attribute region, not the surrounding upgrade window."""
import hashlib,json,sys
from pathlib import Path
from rok_data_paths import resolve
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
TARGET=ROOT/'Saved/UnityBarracksUpgradeAttributeNativeReferenceProbe/Artifacts'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def verify(path, output=None):
    path=Path(path).resolve();assert path.is_relative_to(TARGET.resolve())
    evidence=json.loads(path.read_text(encoding='utf-8-sig'));inputs_path=path.parent/'ReferenceInputs.json'
    inputs=json.loads(inputs_path.read_text(encoding='utf-8-sig'));fail=[]
    def check(ok,reason):
        if not ok:fail.append(reason)
    check(evidence.get('nativeRendered') and not evidence.get('failure'),'native execution failed')
    check(evidence.get('inputSha256')==sha(inputs_path),'input hash mismatch')
    for src in inputs['sources']:check(sha(resolve(src['path']))==src['sha256'],'source changed: '+src['path'])
    check(evidence.get('unityVersion')=='2019.4.28f1c1','Unity version')
    check(abs(evidence.get('attributeHeight',-1)-110)<.05 and abs(evidence.get('rowHeight',-1)-40)<.05,'source row/attribute heights')
    rows=evidence.get('rows',[]);check(len(rows)==2,'must have two source rows')
    if 'levelBefore' in inputs:
        for field in ['levelBefore','levelAfter']:
            t=evidence.get(field,{})
            check(t.get('text')==inputs[field],field+' actual source text')
            check(t.get('fontSize')==24 and t.get('allGlyphsFound'),field+' source font/glyphs')
            r=t.get('rect',{})
            check(r.get('width',0)>0 and r.get('height',0)>0,field+' fitted actual rect')
            g=t.get('generatedGlyphRect',{})
            check(g.get('width',0)>0 and g.get('height',0)>0,field+' generated glyph bounds')
            check(t.get('generatedVertexCount',0)==len(t.get('generatedLocalVertices',[])) and t.get('generatedVertexCount',0)>=4 and t.get('nondegenerateQuadCount',0)>0,field+' actual complete glyph quads')
            check(t.get('fontStyle')==1,field+' source bold')
        check(evidence.get('beforeHorizontalFit')==2 and evidence.get('afterHorizontalFit')==2,'native preferred-size fit mode')
        arrow=evidence.get('levelArrowRect',{})
        check(arrow.get('width',0)>0 and arrow.get('height',0)>0,'native arrow rect')
        corners=evidence.get('levelArrowScreenCorners',[])
        check(len(corners)==4 and len(evidence.get('levelArrowWorldCorners',[]))==4,'actual arrow corners')
        if len(corners)==4:
            bounds=[min(v['x'] for v in corners),min(v['y'] for v in corners),max(v['x'] for v in corners)-min(v['x'] for v in corners),max(v['y'] for v in corners)-min(v['y'] for v in corners)]
            check(all(abs(arrow[k]-v)<.001 for k,v in zip(['x','y','width','height'],bounds)),'arrow AABB from actual corners')
            check(evidence.get('levelArrowMirrored')==(evidence['levelArrowDirectedRect']['width']<0),'arrow mirror preserved')
    image_path=path.parent/evidence.get('png','missing.png');assert image_path.resolve().parent==path.parent
    check(image_path.is_file(),'actual PNG missing');counts=[]
    if image_path.is_file():
        check(sha(image_path)==evidence.get('sha256'),'PNG hash mismatch')
        image=Image.open(image_path).convert('RGBA');check(image.size==(1440,810),'PNG size')
        pixels=np.asarray(image)
        for i,(row,expected) in enumerate(zip(rows,inputs['rows'])):
            check(row['label']['text']==expected['label'] and row['value']['text']==expected['value'],f'row {i} source text')
            check(row['separator']==(i==0),f'row {i} separator')
            for name in ['label','value']:
                t=row[name];check(t['fontSize']==20 and t['allGlyphsFound'] and t['lineCount']>=1,f'row {i} {name} font/glyph')
            check(row['value']['richText'],f'row {i} richtext disabled')
            r=row['rect'];x0=max(0,int(r['x']));y0=max(0,int(r['y']));x1=min(1440,int(r['x']+r['width']));y1=min(810,int(r['y']+r['height']))
            crop=pixels[y0:y1,x0:x1].astype(int)
            check(crop.size>0,f'row {i} visible rect')
            if crop.size:
                green=(crop[:,:,1]>80)&(crop[:,:,1]>crop[:,:,0]+30)&(crop[:,:,1]>crop[:,:,2]+30)&(crop[:,:,3]>128)
                white=(crop[:,:,:3].min(axis=2)>180)&(crop[:,:,3]>128)
                counts.append(dict(row=i,green=int(green.sum()),white=int(white.sum())))
                check(green.sum()>3 and white.sum()>10,f'row {i} missing actual green increment/white label pixels')
    result=dict(passed=not fail,failures=fail,evidence_sha256=sha(path),pixel_counts=counts,
        scope='Native original attribute region and pixel presence only; no UE parity, building preview, business/server or full-page acceptance.')
    destination=Path(output) if output is not None else path.parent/'verify.json'
    destination.write_text(json.dumps(result,indent=2),encoding='utf-8');return result
if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence')
    parser.add_argument('--output',help='Write a separate verification result without replacing historical verify.json')
    args=parser.parse_args()
    result=verify(args.evidence,args.output)
    print(json.dumps(result,indent=2))
    sys.exit(0 if result['passed'] else 1)
