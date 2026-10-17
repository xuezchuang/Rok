"""Actual Grove Gamma evidence: current renderer metadata and conservative opaque PNG pixels.
Use bundled Python (NumPy/Pillow): this script never launches either engine.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from rok_data_paths import resolve
import struct
import numpy as np
from PIL import Image, ImageFilter
from VerifyRokGroveCenterScaleRuntime import source_data, triangle, finite, MESH, MATERIAL

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'Saved/RokUnityMigration/GroveGamma'
CENTER=ROOT/'Saved/RokUnityMigration/GroveCenterScale'
AUDITS=ROOT/'Saved/RokUnityMigration/GroveGammaAudit'
RAW='/Game/RokUnityWorldMapPrototype/Textures/T_GroveGamma_UnityDecoded1024.T_GroveGamma_UnityDecoded1024'
OPACITY='/Game/RokUnityWorldMapPrototype/Textures/T_WorldSource_map_tree_atlas_7e11c882.T_WorldSource_map_tree_atlas_7e11c882'
MIN_PIXELS=32

def load(path): return json.loads(path.read_text(encoding='utf-8-sig'))
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def image(path): return np.asarray(Image.open(path).convert('RGBA'))
def erode(mask,radius=1): return np.asarray(Image.fromarray(np.uint8(mask)*255).filter(ImageFilter.MinFilter(2*radius+1)))==255

def capture_order(capture, previous=None):
    request,processed=capture['request'],capture['processed']
    for state in (request,processed):
        assert type(state['frame']) is int and state['frame']>=0
        assert type(state['elapsed']) in (float,int) and np.isfinite(state['elapsed']) and state['elapsed']>=0
    # FViewport::Draw -> ProcessScreenShots saves the PNG, then broadcasts the
    # processed delegate synchronously before LaunchEngineLoop increments GFrameCounter.
    assert processed['frame']>=request['frame'] and processed['elapsed']>=request['elapsed'], 'Screenshot completion precedes its request'
    if previous is not None:
        assert request['frame']>=previous['processed']['frame']+3, 'Source phase did not settle through three actual frames'
        assert request['elapsed']-previous['processed']['elapsed']>=.5-1e-6, 'Source phase did not settle through half a second of actual ticks'

def sources():
    center,_,positions,uv,colors,indices=source_data()
    source,native=load(SOURCE/'manifest.json'),load(SOURCE/'native_render.json')
    assert source['schema']=='rok.grove_gamma.v1' and source['center_manifest_sha256']==sha(CENTER/'manifest.json')
    assert source['prefab']=='Grove_co_tree_01_adorning' and source['dxf']==native['dxf']==1509
    assert native['unityVersion']=='2019.4.28f1c1' and native['colorSpace']=='Gamma' and native['shader']=='custom/landmap/tree-16-nomask'
    assert native['treeRot']==0 and abs(source['tree_scale']-native['treeScale'])<2e-6 and abs(native['treeScale']-center['bake_scale'])<2e-6
    assert len(source['source_files'])==9
    for item in source['source_files']:
        path=(resolve(item['path'])).resolve();assert path.is_relative_to((ROOT/'ref').resolve()) and sha(path)==item['sha256']
    texture=native['texture'];assert texture['format']=='DXT5' and [texture['width'],texture['height'],texture['mips']]==[1024,1024,1]
    decoded=image(SOURCE/'native_imported_texture.png');assert decoded.shape==(1024,1024,4)
    assert int((decoded[:,:,3]==0).sum())==texture['gpuAlphaZero'] and int((decoded[:,:,3]==255).sum())==texture['gpuAlphaOpaque']
    assert int(((decoded[:,:,3]>0)&(decoded[:,:,3]<255)).sum())==texture['gpuAlphaPartial']
    assert len(source['samples'])==len(native['frames'])==3
    native_images=[]
    for name,a,b in zip(['day','dusk','night'],source['samples'],native['frames']):
        assert a['name']==b['name']==name and np.max(np.abs(np.array(a['spriteColor'])-b['spriteColor']))<1e-6
        path=Path(b['path']).resolve();assert path==(SOURCE/f'native_{name}.png').resolve()
        data=image(path);assert data.shape==(720,1280,4);native_images.append(data[:,:,:3]/255.)
    # Original source prefab root and renderer transforms are identity; source shader TreeRot=0.
    unity_positions=positions[:,[0,2,1]].astype(np.float64)/100
    cam=np.array([native['cameraPosition'][k] for k in 'xyz']);angle=np.deg2rad(native['cameraEuler']['x'])
    assert abs(native['cameraEuler']['y'])<1e-6 and abs(native['cameraEuler']['z'])<1e-6
    right=np.array([1,0,0]);up=np.array([0,np.cos(angle),np.sin(angle)]);forward=np.array([0,-np.sin(angle),np.cos(angle)])
    delta=unity_positions-cam;depth=delta@forward;assert (depth>0).all()
    focal=720/2/np.tan(np.deg2rad(native['fov'])/2)
    projected=np.column_stack([640+(delta@right)/depth*focal,360-(delta@up)/depth*focal,depth,uv])
    return source,native,center,positions,uv,colors,indices,decoded,native_images,projected

def decode_platform(row,directory):
    result=[]
    for index,mip in enumerate(row['platform_mips']):
        path=Path(mip['file']).resolve();assert path.parent==directory and mip['level']==index
        data=path.read_bytes();assert len(data)==mip['bytes'] and sha(path)==mip['sha256']
        w,h=mip['width'],mip['height'];assert type(w) is int and type(h) is int and w>0 and h>0
        fmt=row['pixel_format'];raw=np.frombuffer(data,dtype=np.uint8)
        if fmt in ('B8G8R8A8','R8G8B8A8'):
            assert len(raw)==w*h*4
            rgba=raw.reshape(h,w,4)
            if fmt=='B8G8R8A8':rgba=rgba[:,:,[2,1,0,3]]
        else:
            assert fmt in ('DXT1','DXT3','DXT5')
            stride=8 if fmt=='DXT1' else 16;bw,bh=(w+3)//4,(h+3)//4
            assert len(raw)==bw*bh*stride
            blocks=raw.reshape(-1,stride);n=len(blocks)
            if fmt=='DXT5':
                a=blocks[:,0].astype(np.int32);b=blocks[:,1].astype(np.int32);table=np.zeros((n,8),dtype=np.float64);table[:,0]=a;table[:,1]=b
                for j in range(2,8):table[:,j]=np.where(a>b,((8-j)*a+(j-1)*b)/7,((6-j)*a+(j-1)*b)/5 if j<6 else (0 if j==6 else 255))
                bits=sum(blocks[:,2+j].astype(np.uint64)<<(8*j) for j in range(6))
                alpha=np.column_stack([table[np.arange(n),((bits>>(3*j))&7).astype(int)] for j in range(16)])
            elif fmt=='DXT3':
                alpha=np.column_stack([((blocks[:,j//2]>>(4*(j%2)))&15)*17 for j in range(16)])
            else:
                c0=blocks[:,0].astype(np.uint16)+(blocks[:,1].astype(np.uint16)<<8);c1=blocks[:,2].astype(np.uint16)+(blocks[:,3].astype(np.uint16)<<8)
                bits=sum(blocks[:,4+j].astype(np.uint32)<<(8*j) for j in range(4))
                alpha=np.column_stack([np.where((c0<=c1)&(((bits>>(2*j))&3)==3),0,255) for j in range(16)])
            aimg=alpha.reshape(bh,bw,4,4).transpose(0,2,1,3).reshape(bh*4,bw*4)[:h,:w]
            rgba=np.zeros((h,w,4),dtype=np.float64);rgba[:,:,3]=aimg
        result.append(rgba)
    assert len(result)==row['mips'] and list(result[0].shape[:2])==[row['height'],row['width']]
    return result

def footprint(texture,uv,address=(1,1)):
    """Four bilinear texels, no claim these CPU footprints are GPU pixel readback."""
    h,w=texture.shape[:2];x=uv[...,0]*w-.5;y=(1-uv[...,1])*h-.5
    x0=np.floor(x).astype(int);y0=np.floor(y).astype(int);fx=x-x0;fy=y-y0
    def wrap(v,n,mode):
        if mode==1:return np.clip(v,0,n-1)
        assert mode==0, 'Mirror/border sampler needs an explicit implementation'
        return v%n
    texels=[texture[wrap(y0+dy,h,address[1]),wrap(x0+dx,w,address[0])] for dx,dy in [(0,0),(1,0),(0,1),(1,1)]]
    weights=[(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy]
    sampled=sum(t*wgt[...,None] for t,wgt in zip(texels,weights))
    return sampled,np.min([t[...,3] for t in texels],axis=0),np.max([t[...,3] for t in texels],axis=0)

def raster(projected,triangles,size,decoded,opacity=None,opacity_info=None):
    w,h=size;opaque=np.zeros((h,w),bool);partial=np.zeros((h,w),bool);zbuffer=np.full((h,w),np.inf);uvbuffer=np.zeros((h,w,2))
    # Include both windings conservatively: an overlapping partial-alpha source triangle poisons a candidate even if it might be culled.
    for triangle_indices in triangles:
        p=projected[np.asarray(triangle_indices,dtype=int)];xy=p[:,:2];depth=p[:,2];uv=p[:,3:5]
        lo=np.maximum(np.floor(xy.min(0)).astype(int),[0,0]);hi=np.minimum(np.ceil(xy.max(0)).astype(int),[w-1,h-1])
        if (lo>hi).any():continue
        x,y=np.meshgrid(np.arange(lo[0],hi[0]+1)+.5,np.arange(lo[1],hi[1]+1)+.5)
        den=(xy[1,1]-xy[2,1])*(xy[0,0]-xy[2,0])+(xy[2,0]-xy[1,0])*(xy[0,1]-xy[2,1])
        if abs(den)<1e-8:continue
        def bary(px,py):
            a=((xy[1,1]-xy[2,1])*(px-xy[2,0])+(xy[2,0]-xy[1,0])*(py-xy[2,1]))/den
            b=((xy[2,1]-xy[0,1])*(px-xy[2,0])+(xy[0,0]-xy[2,0])*(py-xy[2,1]))/den
            return np.stack([a,b,1-a-b],axis=-1)
        bs=bary(x,y);inside=(bs>=-1e-7).all(axis=-1)
        if not inside.any():continue
        def uv_at(bs):
            weights=bs/depth;return (weights@uv)/weights.sum(axis=-1)[...,None]
        uvs=uv_at(bs);z=1/(bs/depth).sum(axis=-1)
        _,low,high=footprint(decoded,uvs);is_opaque=low==255;has_alpha=high>0
        if opacity is not None:
            top=opacity[0];texsize=np.array([top.shape[1],top.shape[0]])
            dx=(uv_at(bary(x+1,y))-uvs)*texsize;dy=(uv_at(bary(x,y+1))-uvs)*texsize
            rho=np.maximum(np.linalg.norm(dx,axis=-1),np.linalg.norm(dy,axis=-1));lod=np.log2(np.maximum(rho,1))
            first=max(0,len(opacity)-opacity_info['resident_mips']);base=np.clip(np.floor(lod).astype(int),first,len(opacity)-1)
            # Reject uncertainty in the footprint and its adjacent mips, including possible trilinear/anisotropic selection.
            low_old=np.full(x.shape,255.);high_old=np.zeros(x.shape)
            for offset in (-1,0,1):
                levels=np.clip(base+offset,first,len(opacity)-1)
                for level in np.unique(levels):
                    _,a,b=footprint(opacity[level],uvs,(opacity_info['address_x'],opacity_info['address_y']))
                    use=levels==level;low_old[use]=np.minimum(low_old[use],a[use]);high_old[use]=np.maximum(high_old[use],b[use])
            is_opaque&=low_old==255;has_alpha|=high_old>0
        ys,xs=slice(lo[1],hi[1]+1),slice(lo[0],hi[0]+1)
        partial[ys,xs]|=inside&has_alpha&~is_opaque
        opaque[ys,xs]|=inside&is_opaque
        nearer=inside&is_opaque&(z<zbuffer[ys,xs]);zbuffer[ys,xs][nearer]=z[nearer];uvbuffer[ys,xs][nearer]=uvs[nearer]
    return erode(opaque&~partial),uvbuffer

def compare(label,images,colors,mask,uvs,decoded):
    day=images[0];ratios=np.array(colors)/np.array(colors[0])
    # Candidate selection does not inspect later frame agreement, so a bad phase cannot remove itself from the statistics.
    mask=mask&(day.min(2)>.04)&(day.max(2)<.93)&((day*ratios.max(axis=0)).max(2)<.97)
    count=int(mask.sum());assert count>=MIN_PIXELS,(label,'insufficient conservative opaque pixels',count)
    rows=[]
    texture_rgb=footprint(decoded,uvs)[0][:,:,:3]/255.
    for index in (1,2):
        error=np.abs(images[index]-day*ratios[index])[mask]
        ratio=np.median(images[index][mask]/day[mask],axis=0)
        mean=error.mean(0);p95=np.percentile(error,95,axis=0)
        assert mean.max()<=3/255 and p95.max()<=6/255 and np.max(abs(ratio-ratios[index]))<=.045,(label,index,count,mean,p95,ratio,ratios[index])
        rows.append({'sample':index,'pixels':count,'expected_gamma_ratio':ratios[index].tolist(),'measured_ratio_median':ratio.tolist(),'mean_rgb_error':mean.tolist(),'p95_rgb_error':p95.tolist()})
    absolute=[]
    for index in range(3):
        expected=np.clip(texture_rgb*np.array(colors[index]),0,1);error=np.abs(images[index]-expected)[mask]
        absolute.append({'sample':index,'mean_rgb_error':error.mean(0).tolist(),'p95_rgb_error':np.percentile(error,95,axis=0).tolist()})
    # UV-pixel correspondences are checked numerically too; these tolerances include AA/filter rounding but reject a different atlas region.
    assert max(max(r['mean_rgb_error']) for r in absolute)<=.025 and max(max(r['p95_rgb_error']) for r in absolute)<=.065,(label,'absolute decoded texture mismatch',absolute)
    return {'pixels':count,'ratios':rows,'absolute_decoded_texture':absolute,'mask_bounds':[int(v) for v in [np.where(mask)[1].min(),np.where(mask)[0].min(),np.where(mask)[1].max(),np.where(mask)[0].max()]]}

def verify(directory=None):
    cpp=(ROOT/'Source/Rok/RokGroveGammaAuditSubsystem.cpp').read_text(encoding='utf-8')
    for token in ['IsGroveGammaTargetWorld(Cast<UWorld>(Outer))','bOwnsTargetWorld = IsGroveGammaTargetWorld(GetWorld())','bOwnsTargetWorld && !bFinished','GetUsedTextures(Used)','LockReadOnly()','GetViewportSize','projected_actual_vertices','RestoreParameters()','RestoreVisibility()','Raw->CompressionSettings!=TC_VectorDisplacementmap','RequestExitWithStatus(false, Failures.IsEmpty() ? 0 : 1)']:
        assert token in cpp,token
    for token in ['SetActorScale3D(', 'SetActorTransform(', 'SetActorLocation(', 'SetScalarParameterValue(', 'TestingAdvance', 'SetTimeDilation(', 'FlushRenderingCommands(']:
        assert token not in cpp,token
    source,native,center,positions,uv,colors,indices,decoded,native_images,native_projected=sources()
    native_mask,native_uv=raster(native_projected,indices,(1280,720),decoded)
    native_comparison=compare('native',native_images,[r['spriteColor'] for r in native['frames']],native_mask,native_uv,decoded)
    if directory is None:
        print(json.dumps({'source_and_native_opaque_passed':True,'native':native_comparison}));return
    directory=directory.resolve();assert directory.is_relative_to(AUDITS.resolve());report=load(directory/'manifest.json')
    assert report['passed'] and report['complete'] and not report['failures'] and report['owns_target_world'] and report['audit_version']==1
    assert Path(report['project']).resolve()==ROOT and report['world']=='/Game/Maps/L_RokUnityWorldMapPrototype'
    assert report['gamma_source_manifest_sha256']==sha(SOURCE/'manifest.json') and report['gamma_native_manifest_sha256']==sha(SOURCE/'native_render.json')
    assert report['source_manifest_sha256']==sha(CENTER/'manifest.json') and report['native_manifest_sha256']==sha(CENTER/'native_render.json')
    assert report['native_decoded_texture_sha256']==sha(SOURCE/'native_imported_texture.png')
    assert report['native_image_sha256']==[sha(Path(frame['path'])) for frame in native['frames']]
    assert report['visibility_restored'] and report['parameters_restored'] and report['capture_includes_slate_ui']
    assert len(report['parameter_restore_records'])==11 and all(r['restored'] and r['original_color']==r['restored_color'] and abs(r['original_tree_scale']-r['restored_tree_scale'])<2e-6 for r in report['parameter_restore_records'])
    buffers=report['actual_render_buffers'];assert buffers['mesh']==MESH and buffers['buffer_source_match_complete'] and buffers['full_precision_uvs'] and not buffers['nanite_enabled'] and buffers['render_lod_count']==1 and buffers['allow_cpu_access'] and buffers['uv_channels']==3
    rows=np.asarray(buffers['render_vertices_source_mapping']);mapping=rows[:,0].astype(int)
    assert set(mapping)==set(range(377)) and np.array_equal(mapping,rows[:,0]) and np.max(abs(rows[:,1:4]-positions[mapping]))<.005
    assert np.array_equal(rows[:,4:6].astype(np.float32),uv[mapping]) and np.array_equal(rows[:,6:10].astype(np.float32),colors[mapping])
    assert Counter(triangle(t) for t in buffers['render_triangles_source_mapping'])==Counter(triangle(t[[0,2,1]]) for t in indices)
    textures=report['actual_textures'];raw=textures['raw_rgb'];opacity_info=textures['preserved_opacity']
    assert raw['texture']==RAW and opacity_info['texture']==OPACITY and not raw['srgb'] and [raw['width'],raw['height'],raw['mips'],raw['resident_mips']]==[1024,1024,1,1]
    assert raw['compression_settings']==5 and raw['filter']==1 and raw['address_x']==raw['address_y']==1
    raw_pixels=decode_platform(raw,directory);assert np.array_equal(raw_pixels[0],decoded),'Actual raw platform RGBA differs from decoded source GPU texture'
    opacity=decode_platform(opacity_info,directory);assert 1<=opacity_info['resident_mips']<=len(opacity)
    expected={}
    for entry in center['instances']:
        a=entry['source_instance'];q=np.deg2rad(a['ue_yaw'])/2
        expected[(f'UnityTile_{entry["tile"][0]}_{entry["tile"][1]}',f'UnityObjectIndex_{a["template_index"]}')]=[a['ue_relative_to_focus'][k] for k in 'xyz']+[a['ue_actor_scale_for_centimeter_asset'][k] for k in 'xyz']+[0,0,np.sin(q),np.cos(q)]
    captures=report['captures'];assert len(captures)==3
    actual_images=[];artifacts=[];baseline={};projected=None;actual_indices=None
    for i,capture in enumerate(captures):
        path=Path(capture['file']).resolve();assert path.parent==directory and path.name==f'{i:02d}_{source["samples"][i]["name"]}.png'
        data=path.read_bytes();assert len(data)==capture['file_bytes'] and sha(path)==capture['sha256'] and struct.unpack('>II',data[16:24])==(1440,810)
        actual_images.append(image(path)[:,:,:3]/255.);artifacts.append({'file':path.name,'sha256':sha(path)})
        for phase in ['request','processed']:
            state=capture[phase];assert state['sample_index']==i and state['sample_name']==path.stem and state['dxf']==1509 and abs(state['native_scale']-source['tree_scale'])<2e-6
            assert state['actual_viewport_size']==[1440,810] and state['actual_texture_flags_unchanged'] and len(state['instances'])==11
            assert 'RokWorldMapCameraPawn' in state['view_target']
            keys=set()
            for actor in state['instances']:
                key=(actor['tile_tag'],actor['index_tag']);assert key in expected and key not in keys;keys.add(key)
                t=np.array(actor['location']+actor['scale']+actor['rotation_quaternion']);e=np.array(expected[key])
                assert np.max(abs(t[:3]-e[:3]))<.01 and np.max(abs(t[3:]-e[3:]))<1e-5 and actor['source_transform_unchanged']
                assert actor['material_parent']==MATERIAL and actor['source_tree_scale']==float(np.float32(center['bake_scale'])) and abs(actor['source_tree_scale']-source['tree_scale'])<2e-6
                assert np.max(abs(np.array(actor['actual_sprite_color'])-np.array(source['samples'][i]['spriteColor']+[1])))<2e-6
                if key in baseline:assert np.max(abs(t-baseline[key]))<1e-5
                else:baseline[key]=t
                if not report['isolated_existing_asset'] or actor['component']==state['focus_component']:assert actor['visible_parent_chain']
            p=np.array(state['projected_actual_vertices']);t=np.array(state['actual_triangle_indices'])
            assert p.shape==(len(mapping),8) and np.isfinite(p).all() and (p[:,2]>0).all() and t.shape==(309,3) and np.array_equal(t,t.astype(int)) and t.min()>=0 and t.max()<len(mapping)
            assert Counter(triangle(mapping[x.astype(int)]) for x in t)==Counter(triangle(x) for x in buffers['render_triangles_source_mapping'])
            assert np.array_equal(p[:,3:5].astype(np.float32),rows[:,4:6].astype(np.float32))
            focus=next(a for a in state['instances'] if a['component']==state['focus_component'])
            local=rows[:,1:4]*np.array(focus['scale']);q=focus['rotation_quaternion'];yaw=2*np.arctan2(q[2],q[3])
            rotation=np.array([[np.cos(yaw),-np.sin(yaw),0],[np.sin(yaw),np.cos(yaw),0],[0,0,1]])
            assert np.max(abs(p[:,5:8]-(local@rotation.T+np.array(focus['location']))))<.0001

            if projected is not None:assert np.max(abs(p-projected))<1e-4 and np.array_equal(t,actual_indices)
            projected=p;actual_indices=t.astype(int)
        capture_order(capture,captures[i-1] if i else None)
    mask,uvs=raster(projected[:,:5],actual_indices,(1440,810),decoded,opacity,opacity_info)
    # Exclude the known HUD margins without selecting by measured color agreement.
    mask[:110]=False;mask[670:]=False;mask[:,:300]=False;mask[:,1150:]=False
    comparison=compare('ue',actual_images,[r['spriteColor'] for r in source['samples']],mask,uvs,decoded)
    result={'passed':True,'manifest_sha256':sha(directory/'manifest.json'),'source_manifest_sha256':sha(SOURCE/'manifest.json'),'native_manifest_sha256':sha(SOURCE/'native_render.json'),
            'native_opaque':native_comparison,'ue_opaque':comparison,'artifacts':artifacts,'native_artifacts':[{'file':frame['path'],'sha256':sha(Path(frame['path']))} for frame in native['frames']],'raw_platform_matches_native_decoded_rgba':True,
            'actual_opacity_platform_mips_checked':len(opacity),'gpu_pixel_readback':False,'full_transparency_parity':False,'full_shader_parity':False,
            'screenshot_ordering':{'request_processed_monotonic':True,'same_frame_processed_callback_allowed':True,'minimum_between_phase_frames':3,'minimum_between_phase_seconds':.5,
                                   'engine_source':'UnrealClient.cpp:1872 ProcessScreenShots; GameViewportClient.cpp:2556 saves then :2583 broadcasts processed; LaunchEngineLoop.cpp:6131 increments frame counter'},
            'scope':'Three actual renderer PNG states, Gamma-before-linear RGB ratios and decoded atlas correspondence only on conservative source/actual-old-opacity fully opaque footprints. Candidates use perspective-correct actual LOD projection; any overlapping potentially partial-alpha triangle is rejected, then screen eroded. Partial alpha includes baked shadows; blend order, edges, all GPU filtering details, WPO output and unobstructed normal-world equivalence remain separate.'}
    (directory/'runtime_verify.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path,nargs='?');parser.add_argument('--source-only',action='store_true');args=parser.parse_args()
    if not args.source_only:assert args.directory is not None
    verify(None if args.source_only else args.directory)
