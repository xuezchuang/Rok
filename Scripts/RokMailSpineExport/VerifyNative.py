"""Compare original SkeletonGraphic mesh to offline Spine export at exact samples."""
import hashlib,json,sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from rok_data_paths import DATA_ROOT
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Saved/RokUnityMigration/MailSpine'
def main():
    path=Path(sys.argv[1]) if len(sys.argv)>1 else sorted((OUT/'NativeReference').glob('*/native_mesh.json'))[-1]
    native=json.loads(path.read_text());clip=json.loads((DATA_ROOT/'MailSpine/gift5_stay_mesh.json').read_text())
    assert native['unityVersion']=='2019.4.28f1c1' and native['colorSpace']=='Gamma' and len(native['samples'])==8
    results=[]
    for sample in native['samples']:
        phase=sample['time']%clip['duration'];phase=0 if abs(phase-clip['duration'])<1e-6 else phase
        frame=min(clip['frames'],key=lambda f:abs(f['time']-phase));assert abs(frame['time']-phase)<1e-6
        xy=[];uv=[];tri=[];colors=[]
        for draw in frame['draws']:
            offset=len(xy);xy.extend(zip(draw['xy'][::2],draw['xy'][1::2]));uv.extend(zip(draw['uv'][::2],draw['uv'][1::2]));tri.extend(t+offset for t in draw['triangles'])
            r,g,b,a=draw['rgba'];colors.extend([(r*a,g*a,b*a,a)]*(len(draw['xy'])//2))
        assert sample['canvasPPU']==100 and len(xy)==len(sample['vertices']) and tri==sample['triangles']
        xe=max(abs(p[k]*100-v['xy'[k]]) for p,v in zip(xy,sample['vertices']) for k in range(2))
        ue=max(max(abs(p[0]-v['x']),abs(1-p[1]-v['y'])) for p,v in zip(uv,sample['uv']))
        ce=max(abs(c[k]*255-v['rgba'[k]]) for c,v in zip(colors,sample['colors']) for k in range(4))
        assert xe<.001 and ue<2e-6 and ce<=1,(xe,ue,ce)
        image=Path(sample['image']);assert image.read_bytes()[:8]==b'\x89PNG\r\n\x1a\n'
        results.append({'time':sample['time'],'vertices':len(xy),'xy_error_source_pixels':xe,'uv_error_unity_flip_v':ue,'pma_color_byte_error':ce,'image':str(image),'sha256':hashlib.sha256(image.read_bytes()).hexdigest()})
    result={'passed':True,'native_file':str(path),'native_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'samples':results,'scope':native['scope'],'remaining':'Actual UE Slate rendering and mail lifecycle; numerical source comparison does not prove raster match.'}
    (OUT/'native_verify.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
