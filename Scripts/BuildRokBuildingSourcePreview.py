"""Extract one explicitly selected scene-presentation model; never infer player country."""
import csv,hashlib,json,re
from pathlib import Path
from PIL import Image
from rok_data_paths import DATA_ROOT, label, resolve
ROOT=Path(__file__).resolve().parents[1]
CLIENT=ROOT/'ref/client-unity'
MODEL='build_9_101_1'
PREFAB='Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuilding/UI_'+MODEL+'.prefab'
TEXTURE='Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/'+MODEL+'.png'
WINDOW='Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Win_BuildingUpdate.prefab'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def build(native=None):
    prefab=CLIENT/PREFAB;texture=CLIENT/TEXTURE;meta=Path(str(texture)+'.meta')
    text=prefab.read_text(encoding='utf-8-sig');mt=meta.read_text(encoding='utf-8-sig')
    guid=re.search(r'^guid: (\w+)',mt,re.M)[1]
    assert guid=='4bb871ef2938a6d48a9b46af5cd8f287' and 'guid: '+guid in text
    assert 'm_Type: 0' in text and 'm_Material: {fileID: 0}' in text and 'm_UseSpriteMesh: 0' in text
    assert not re.search(r'^--- !u!(95|198) ',text,re.M)
    blocks=re.split(r'^--- !u!\d+ &',text,flags=re.M)
    image_rect=next(b for b in blocks if 'RectTransform:' in b and 'm_SizeDelta: {x: 512, y: 490}' in b)
    def vec(key):
        v=re.search(key+r': \{([^}]+)\}',image_rect)[1]
        return [float(x.split(':')[1]) for x in v.split(',')]
    image=Image.open(texture).convert('RGBA');hist=image.getchannel('A').histogram()
    initial=DATA_ROOT/'unity_initial_city_state.json'
    record=next(x for x in json.loads(initial.read_text(encoding='utf-8-sig'))['buildings'] if x['init_id']==6)
    assert record['type_id']==9 and record['visual']['model_id']==MODEL
    table=next((ROOT/'ref/config-tables').glob('*/CSV/BuildingModelData.csv'))
    with table.open(encoding='utf-8-sig',newline='') as f:
        mappings=[r for r in csv.DictReader(f) if r['type']=='9' and r['age']=='1']
    sources=[prefab,Path(str(prefab)+'.meta'),texture,meta,CLIENT/WINDOW,table,initial,
        CLIENT/'Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateMediator.cs',
        CLIENT/'Assets/Scripts/Hotfix/MVC/Proxy/CityBuildingProxy.cs',CLIENT/'Assets/Scripts/Hotfix/MVC/Proxy/PlayerProxy.cs',
        CLIENT/'Assets/Scripts/Client/Compoment/UIExtend/PolygonImage.cs',
        CLIENT/'Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs',Path(__file__).resolve()]
    data={'schema':'rok.building.source_preview.v1','model_id':MODEL,'ui_prefab':PREFAB,'window_prefab':WINDOW,
        'parent_path':'Rect/pl_content/Left/pl_buildImg','sprite':{'path':label(texture),'guid':guid,'sha256':sha(texture),'size':list(image.size),'alpha_extrema':list(image.getchannel('A').getextrema()),'transparent_pixels':hist[0],
            'import':{k:int(re.search(r'\b'+k+r': (-?\d+)',mt)[1]) for k in ['sRGBTexture','enableMipMap','alphaIsTransparency','wrapU','wrapV','spriteMode','spriteMeshType']}},
        'image':{'name':'img','size':vec('m_SizeDelta'),'scale':vec('m_LocalScale')[:2],'source_scale3':vec('m_LocalScale'),'pivot':vec('m_Pivot'),'anchored_position':vec('m_AnchoredPosition'),'anchor_min':vec('m_AnchorMin'),'anchor_max':vec('m_AnchorMax'),'type':0,'use_sprite_mesh':False,'tint':[1,1,1,1],'material':'default UI','animated':False},
        'binding':{'kind':'explicit_existing_scene_model_presentation','init_id':6,'type_id':9,'source_model_id':record['visual']['model_id'],'sets_profile_civilization':False},
        'source_selection_rule':'Unity GetImgIdByType concatenates building type + logged-in RoleInfo.country + current townhall era, not target building level. Local profile country remains unknown.',
        'age1_mappings':mappings,'missing_reference_prefabs':['UI_build_9_301_1','UI_build_9_401_1'],
        'sources':[{'path':label(p),'sha256':sha(p)} for p in sources]}
    data['layout']={'pending':True}
    if native:
        evidence_path=Path(native).resolve();assert evidence_path.is_relative_to((ROOT/'Saved/UnityBuildingSourcePreviewProbe/Artifacts').resolve())
        evidence=json.loads(evidence_path.read_text(encoding='utf-8-sig'));inputs_path=evidence_path.parent/'ReferenceInputs.json';inputs=json.loads(inputs_path.read_text(encoding='utf-8-sig'))
        assert evidence['nativeRendered'] and not evidence.get('failure') and evidence['modelId']==MODEL
        assert evidence['inputSha256']==sha(inputs_path)
        verified=json.loads((evidence_path.parent/'verify.json').read_text(encoding='utf-8'));assert verified['passed'] and verified['evidence_sha256']==sha(evidence_path)
        for source in inputs['sources']:assert sha(resolve(source['path']))==source['sha256']
        assert evidence['spriteGuid']==guid and len(evidence['vertices'])==4 and len(evidence['triangles'])==6
        points=evidence['screenVertices'];left=min(v['x'] for v in points);top=min(v['y'] for v in points);width=max(v['x'] for v in points)-left;height=max(v['y'] for v in points)-top
        assert width>0 and height>0
        data['layout']={'pending':False,'position':[left,top],'size':[width,height],'image_position':[0,0],'image_size':[width,height]}
        data['native_evidence']={'path':label(evidence_path),'sha256':sha(evidence_path),'input_sha256':evidence['inputSha256'],'ancestors':evidence['ancestors'],'quad':points,'uv':evidence['uv']}
    target=DATA_ROOT/'BuildingSourcePreview';target.mkdir(parents=True,exist_ok=True)
    (target/'source_preview.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8');return data
if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('--native');args=parser.parse_args()
    print(json.dumps(build(args.native),ensure_ascii=True,indent=2))
