"""Actual barracks preview artifact verifier; never launches an engine."""
import argparse,hashlib,json,math
from pathlib import Path
import numpy as np
from PIL import Image
from rok_data_paths import DATA_ROOT, resolve
ROOT=Path(__file__).resolve().parents[1]
PARENT_SOURCE=ROOT/'Saved/UnityBarracksParentWindowProbe/Artifacts/native_20260925_111125_541/render_evidence.json'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def source_type9_parent_rects():
    evidence=json.loads(PARENT_SOURCE.read_text(encoding='utf-8-sig'))
    assert evidence['complete'] and not evidence['failure'] and evidence['nativeAnimatorEnabled']
    nodes={node['path'].split('/')[-1]:node for node in evidence['samples'][-1]['nodes']}
    def rect(name,expected):
        item=nodes[name]['screenRect'];actual=(item['x'],item['y'],item['x']+item['width'],item['y']+item['height'])
        assert all(abs(a-b)<.03 for a,b in zip(actual,expected)),name
        return actual
    return rect('frame1',(194.,94.2,737.,755.)),rect('frame3',(160.,63.,720.,123.))
def verify(directory):
    directory=directory.resolve();assert directory.is_relative_to((ROOT/'Saved/RokUnityMigration/BarracksUpgradeAttributeAudit').resolve())
    path=directory/'manifest.json';m=json.loads(path.read_text(encoding='utf-8-sig'))
    layout_contract=m.get('parent_layout_contract','legacy_type2_v1_unversioned')
    assert layout_contract in ('legacy_type2_v1_unversioned','type9_parent_v2')
    body_rect,header_rect=((177.,139.2,720.,800.),(160.,108.,720.,168.)) if layout_contract=='legacy_type2_v1_unversioned' else source_type9_parent_rects()
    assert m['passed'] and m['complete'] and not m['failures'] and m['capture_includes_slate_ui'] and m['owns_target_world']
    assert Path(m['project']).resolve()==ROOT.resolve() and m['world']=='/Game/Maps/L_RokUnityGameAppCity'
    sourcepath=DATA_ROOT / 'barracks_upgrade_attribute.json';source=json.loads(sourcepath.read_text(encoding='utf-8-sig'))
    assert source['building_type']==9 and source['current_level']==1 and source['next_level']==2
    assert [(r['current'],r['delta']) for r in source['rows']]==[(20,30),(5,5)]
    assert source['row_height']==40 and source['attribute_height']==110
    assert source['omitted_attack']['current']==source['omitted_attack']['next']==0
    for s in source['sources']:
        f=resolve(s['path']).resolve();assert f.is_relative_to(ROOT.resolve()) and sha(f)==s['sha256']
    native_path=ROOT/'Saved/UnityBarracksUpgradeAttributeNativeReferenceProbe/Artifacts/native_20260912_130037_811/render_evidence.json'
    native=json.loads(native_path.read_text(encoding='utf-8-sig'));assert native['nativeRendered'] and not native['failure'] and native['levelArrowMirrored']
    assert native['levelBefore']['text']=='等级1' and native['levelAfter']['text']=='2'
    glyphpath=DATA_ROOT / 'BarracksSourceText/texts.json'
    glyphs={g['key']:g for g in json.loads(glyphpath.read_text(encoding='utf-8'))['texts']}
    assert len(glyphs)==6
    for key in ['level_before','level_after']:
        g=glyphs[key];assert g['fontSize']==24 and g['style']==1 and g['alignment']==3
        assert all(c=={'r':226,'g':209,'b':155,'a':255} for c in g['colors'])
    for g in glyphs.values():assert sha(glyphpath.parent/g['atlas'])==g['atlasSha256']
    captures=m['captures'];assert len(captures)==m['expected_capture_count']==7
    names=['01_selected_city','02_upgrade_attributes','06_preview_hidden','07_preview_restored','03_city_returned','04_reopened_attributes','05_city_final'];artifacts=[]
    for index,c in enumerate(captures):
        f=Path(c['file']).resolve();assert f.parent==directory and f.stem==names[index] and f.stat().st_size==c['file_bytes'] and sha(f)==c['png_sha256']
        image=Image.open(f).convert('RGB');assert image.size==(1440,810)
        pixels=np.asarray(image).astype(int);counts=[];level_counts=[]
        for phase in ['request','processed']:
            state=c[phase];opened=index in [1,2,3,5]
            assert state['open']==opened and state['attributes_attached']==opened and state['state_unchanged']
            assert state['preview_attached']==opened and state['preview_hidden']==(index==2)
            if opened:
                if layout_contract=='type9_parent_v2':
                    for key,expected in (('body_left_pixel_rect',body_rect),('header_left_pixel_rect',header_rect)):
                        actual=state[key]
                        assert all(abs(actual[field]-value)<1 for field,value in zip(('x','y','right','bottom'),expected)),(index,phase,key)
                        assert abs(actual['local_width']-(expected[2]-expected[0]))<1
                        assert abs(actual['local_height']-(expected[3]-expected[1]))<1
                if m.get('glow_diagnostic_contract')==1:
                    assert state['glow_diagnostic_fixed_time'] and state['glow_time']==0 and state['glow_painted_layers']==2
                preview=state['preview'];assert preview['model']=='build_9_101_1' and preview['visible']==(index!=2)
                assert preview['texture']=='/Game/RokPrototype/Textures/RuntimeCity/UnityGameApp_build_9_101_1.UnityGameApp_build_9_101_1'
                assert (preview['width'],preview['height'])==(512,490) and preview['srgb'] and preview['mips']==1 and preview['render_resource']
                assert len(state['glyphs'])==6
                assert [g['source_key'] for g in state['glyphs']]==['level_before','level_after','label_0','value_0','label_1','value_1']
                for glyph in state['glyphs']:
                    assert glyph['paint_count']>0 and glyph['source_key']==glyph['paint_key'] and glyph['text']==glyphs[glyph['source_key']]['text']
                assert state['level_before']=='等级1' and state['level_after']=='2' and state['level_arrow_mirrored']
                assert abs(state['container_height']-110)<.1 and len(state['rows'])==2
                assert abs(state['rows'][1]['local_y']-state['rows'][0]['local_y']-40)<.1
                for row,expect in zip(state['rows'],source['rows']):
                    assert row['label']==expect['label'] and row['value']==expect['value']
                    assert row['separator']==expect['separator']
        assert c['processed']['frame']>=c['request']['frame']
        if index in [1,2,3,5]:
            for row in c['processed']['rows']:
                x=max(0,math.floor(row['pixel_x']));y=max(0,math.floor(row['pixel_y']));right=min(1440,math.ceil(row['pixel_right']));bottom=min(810,math.ceil(row['pixel_bottom']))
                assert right>x and bottom>y
                crop=pixels[y:bottom,x:right];green=(crop[:,:,1]>80)&(crop[:,:,1]>crop[:,:,0]+30)&(crop[:,:,1]>crop[:,:,2]+30);beige=(crop[:,:,0]>140)&(crop[:,:,1]>120)&(crop[:,:,2]>65)&(crop[:,:,0]>=crop[:,:,1])&(crop[:,:,1]>crop[:,:,2])
                white=crop.min(axis=2)>180
                assert green.sum()>3 and beige.sum()>10 and white.sum()>10,'Missing actual green delta/beige value/white label pixels'
                counts.append({'crop':[x,y,right,bottom],'green':int(green.sum()),'source_beige':int(beige.sum()),'white_label':int(white.sum())})
            for region,reference in zip(c['processed']['level_pixel_regions'],[native['levelBefore'],native['levelAfter']]):
                x=max(0,math.floor(region['x'])-2);y=max(0,math.floor(region['y']));right=min(1440,math.ceil(region['right'])+2)
                bottom=min(810,math.ceil(region['bottom']))
                assert bottom>y and right>x,'Invalid complete level region'
                crop=pixels[y:bottom,x:right];mask=(crop[:,:,0]>140)&(crop[:,:,1]>120)&(crop[:,:,2]>65)&(crop[:,:,0]>crop[:,:,1]+5)&(crop[:,:,1]>crop[:,:,2]+15)
                ys,xs=np.nonzero(mask);assert len(ys)>20,'Level glyph missing above attribute background'
                bbox=[int(xs.min()+x),int(ys.min()+y),int(xs.max()+x+1),int(ys.max()+y+1)]
                height=bbox[3]-bbox[1];native_glyph=reference['generatedGlyphRect'];native_rect=reference['rect']
                center=(bbox[1]+bbox[3])/2-region['y'];expected_center=native_glyph['y']+native_glyph['height']/2-native_rect['y']
                assert height>=native_glyph['height']*.55 and abs(center-expected_center)<4,'Level glyph clipped or vertically misplaced versus native center'
                assert bbox[3]<c['processed']['attribute_pixel_top']-1,'Level glyph overlaps attribute background'
                level_counts.append({'glyph_bbox':bbox,'pixels':int(len(ys)),'center_offset':center,'native_center_offset':expected_center})
        artifacts.append({'name':f.name,'sha256':sha(f),'attribute_pixels':counts,'level_pixels':level_counts})
    assert [a['button'] for a in m['actions']]==['BuildingUpgradeButton','Type2CloseButton','BuildingUpgradeButton','Type2CloseButton']
    for a,b in zip(captures[1]['processed']['rows'],captures[5]['processed']['rows']):
        assert a['label']==b['label'] and a['value']==b['value'] and a['separator']==b['separator']
        for key in ['local_y','pixel_x','pixel_y','pixel_right','pixel_bottom']:assert abs(a[key]-b[key])<.1
    # Only the building Image was Hidden. Compare the stable source parent panel,
    # not the independently animated city/HUD outside the modal.
    hidden=np.asarray(Image.open(captures[2]['file']).convert('RGB')).astype(int)
    restored=np.asarray(Image.open(captures[3]['file']).convert('RGB')).astype(int)
    first=np.asarray(Image.open(captures[1]['file']).convert('RGB')).astype(int)
    rect=captures[3]['processed']['preview'];x,y=math.floor(rect['x']),math.floor(rect['y']);r,b=math.ceil(rect['right']),math.ceil(rect['bottom'])
    changed=np.max(abs(restored-hidden),axis=2)>2
    panel=np.zeros(changed.shape,dtype=bool);panel[212:712,178:678]=True
    # pl_buildImg is a transparent container, not an opaque background. The
    # underlying Type2BodyLeft follows the explicit audited layout; its source left edge has
    # fractional alpha. Exclude exactly nonopaque bilinear footprints, derived
    # from the actual nine-slice sprite rather than from observed image deltas.
    framepath=ROOT/'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_frame.png'
    alpha=np.asarray(Image.open(framepath).convert('RGBA'))[:,:,3]
    def mapped(v,target,source,first,last):
        return np.where(v<first,v,np.where(v>target-last,source-(target-v),first+(v-first)*(source-first-last)/(target-first-last)))
    yy,xx=np.indices(changed.shape)
    bx,by,br,bb=body_rect
    u=1+mapped(xx+.5-bx,br-bx,524,493,15)-.5
    v=(2048-822-659)+mapped(yy+.5-by,bb-by,659,278,291)-.5
    ix=np.clip(np.floor(u).astype(int),0,alpha.shape[1]-2);iy=np.clip(np.floor(v).astype(int),0,alpha.shape[0]-2)
    opaque=(alpha[iy,ix]==255)&(alpha[iy,ix+1]==255)&(alpha[iy+1,ix]==255)&(alpha[iy+1,ix+1]==255)
    excluded_transparent=int((panel&~opaque&changed).sum());panel&=opaque
    roi=np.zeros(changed.shape,dtype=bool);roi[y:b,x:r]=True
    assert int((changed&roi).sum())>10000,'Building preview did not contribute substantial actual pixels'
    assert not (changed&panel&~roi).any(),'Hidden diagnostic changed unrelated source parent content'
    assert not (np.max(abs(restored-first),axis=2)[panel]>2).any(),'Restoration did not reproduce original preview'
    preview_pixels={'changed_inside':int((changed&roi).sum()),'changed_outside_in_parent':int((changed&panel&~roi).sum()),'roi':[x,y,r,b],'excluded_source_transparent_pixels':excluded_transparent,'frame_source_sha256':sha(framepath)}
    result={'passed':True,'parent_layout_contract':layout_contract,'body_left_pixel_rect':body_rect,'parent_source_sha256':sha(PARENT_SOURCE) if layout_contract=='type9_parent_v2' else None,'preview_pixel_difference':preview_pixels,'manifest_sha256':sha(path),'source_sha256':sha(sourcepath),'native_level_evidence_sha256':sha(native_path),'artifacts':artifacts,'scope':'Source two-row text/layout, actual attribute pixels and read-only real-delegate lifecycle; no upgrade command, physical hit-test or full-window equivalence.'}
    (directory/'runtime_verify.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(directory/'runtime_verify.json')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('directory',type=Path);verify(p.parse_args().directory)
