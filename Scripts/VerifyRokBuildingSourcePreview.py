"""Verify explicit model native UI preview and its baseline pixel contribution."""
import argparse,hashlib,json,sys
from pathlib import Path
from rok_data_paths import resolve
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def verify(path,output=None):
    p=Path(path).resolve();assert p.is_relative_to((ROOT/'Saved/UnityBuildingSourcePreviewProbe/Artifacts').resolve())
    e=json.loads(p.read_text(encoding='utf-8-sig'));ip=p.parent/'ReferenceInputs.json';i=json.loads(ip.read_text(encoding='utf-8-sig'));fail=[]
    def check(ok,why):
        if not ok:fail.append(why)
    check(e.get('nativeRendered') and not e.get('failure'),'native execution')
    check(e.get('modelId')=='build_9_101_1' and e.get('inputSha256')==sha(ip),'model/input identity')
    for row in i['sources']:check(sha(resolve(row['path']))==row['sha256'],'source changed '+row['path'])
    check(e.get('spriteGuid')==i['contract']['sprite']['guid'],'actual sprite GUID')
    check(e.get('shaderName')=='UI/Default','actual source default UI shader')
    check(len(e.get('vertices',[]))==4 and len(e.get('triangles',[]))==6 and len(e.get('uv',[]))==4,'actual native simple quad')
    points=e.get('screenVertices',[]);check(len(points)==4,'actual projected quad')
    r=e.get('imageRect',{});counts={}
    if len(points)==4:
        bounds=[min(v['x'] for v in points),min(v['y'] for v in points),max(v['x'] for v in points)-min(v['x'] for v in points),max(v['y'] for v in points)-min(v['y'] for v in points)]
        check(bounds[2]>0 and bounds[3]>0,'positive actual quad area')
        # The actual quad may apply source transparent padding; it must fit the image rect.
        check(bounds[0]>=r['x']-1 and bounds[1]>=r['y']-1 and bounds[0]+bounds[2]<=r['x']+r['width']+1 and bounds[1]+bounds[3]<=r['y']+r['height']+1,'native quad inside source image rect')
    nodes=e.get('ancestors',[]);check(any(n['name']=='pl_buildImg' for n in nodes),'source container ancestry')
    check(bool(nodes) and all(abs(nodes[0]['localScale'][k]-.7)<.0001 for k in ['x','y']),'original image scale .7')
    check(all(abs(n['alpha']-1)<.001 for n in nodes),'source visible ancestors')
    frames=e.get('frames',[]);check(len(frames)==2,'baseline and preview captures');images=[]
    for idx,f in enumerate(frames):
        q=p.parent/f['png'];assert q.resolve().parent==p.parent
        check(sha(q)==f['sha256'],'capture hash');im=Image.open(q).convert('RGBA');check(im.size==(1440,810),'capture size');images.append(np.asarray(im).astype(int));check(f['buildingVisible']==(idx==1),'baseline visibility')
    if len(images)==2 and r:
        diff=np.max(abs(images[1]-images[0]),axis=2);changed=diff>2
        x0=max(0,int(np.floor(r['x']))-2);y0=max(0,int(np.floor(r['y']))-2);x1=min(1440,int(np.ceil(r['x']+r['width']))+2);y1=min(810,int(np.ceil(r['y']+r['height']))+2)
        roi=changed[y0:y1,x0:x1];outside=changed.copy();outside[y0:y1,x0:x1]=False
        counts={'changed_inside_image':int(roi.sum()),'changed_outside_image':int(outside.sum()),'image_roi':[x0,y0,x1,y1]}
        check(roi.sum()>100,'source building makes actual visible pixel contribution')
        check(outside.sum()==0,'changes outside building source rect')
    result={'passed':not fail,'failures':fail,'pixel_comparison':counts,'evidence_sha256':sha(p),'scope':'Explicit existing scene-model UI preview; no player civilization/server authority or full window/UE parity acceptance.'}
    destination=Path(output) if output else p.parent/'verify.json';destination.write_text(json.dumps(result,indent=2),encoding='utf-8');return result
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('evidence');parser.add_argument('--output');a=parser.parse_args();r=verify(a.evidence,a.output);print(json.dumps(r,indent=2));sys.exit(0 if r['passed'] else 1)
