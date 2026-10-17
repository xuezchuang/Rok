"""Read-only source extraction of upgrade-window BuildShow glow. No engine launch."""
import hashlib,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CLIENT=ROOT/'ref/client-unity'
def build():
    paths={
        'window':'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Win_BuildingUpdate.prefab',
        'effect':'Assets/BundleAssets/UI/UIEffect/UI_Effect_BuildShow.prefab',
        'material':'Assets/BundleAssets/Effect/Materials/shared_effectmaterials/0UIEffect.mat',
        'shader':'Assets/BundleAssets/Map/Shader/UI-Add.shader',
        'controller':'Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UI_10007.controller',
        'animation':'Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UI_10007.anim',
        'texture':'Assets/BundleAssets/Effect/UIsprite/shared_uisprite/star003.png',
        'rotate':'Assets/Scripts/Client/Compoment/UIComponent/DeemoRotate.cs'}
    files={k:CLIENT/v for k,v in paths.items()}
    effect=files['effect'].read_text(encoding='utf-8-sig')
    docs={m[2]:m[3] for m in re.finditer(r'^--- !u!(\d+) &(-?\d+)\s*\n(.*?)(?=^--- !u!|\Z)',effect,re.M|re.S)}
    layers=[]
    def vec(text,key):return [float(x.split(':')[1]) for x in re.search(r'\b'+key+r': \{([^}]+)',text)[1].split(',')]
    for key,go in docs.items():
        name=re.search(r'^  m_Name: (img_star\S+)',go,re.M)
        if not name:continue
        components=[docs[x] for x in re.findall(r'component: \{fileID: (-?\d+)',go)]
        rect=next(x for x in components if x.startswith('RectTransform:'))
        image=next(x for x in components if 'm_Sprite:' in x);rotate=next(x for x in components if 'rotateSpeed:' in x)
        layers.append({'name':name[1],'sibling':int(re.search('m_RootOrder: (\d+)',rect)[1]),'size':vec(rect,'m_SizeDelta'),'scale':vec(rect,'m_LocalScale'),'rotation_quaternion':vec(rect,'m_LocalRotation'),'euler_hint':vec(rect,'m_LocalEulerAnglesHint'),'anchored_position':vec(rect,'m_AnchoredPosition'),'pivot':vec(rect,'m_Pivot'),'serialized_color':vec(image,'m_Color'),'rotate_speed':vec(rotate,'rotateSpeed'),'is_local':int(re.search('isLocal: (\d+)',rotate)[1])})
    anim=files['animation'].read_text(encoding='utf-8-sig');segment=anim.split('  m_FloatCurves:')[1].split('  m_PPtrCurves:')[0];curves=[]
    for m in re.finditer(r'  - curve:\n(.*?)(?=  - curve:|\Z)',segment,re.S):
        s=m[1];keys=[]
        for k in re.finditer(r'        time: ([^\n]+)\n        value: ([^\n]+)\n        inSlope: ([^\n]+)\n        outSlope: ([^\n]+)',s):keys.append(dict(zip(['time','value','inSlope','outSlope'],map(float,k.groups()))))
        curves.append({'path':re.search('    path: (.+)',s)[1],'attribute':re.search('    attribute: (.+)',s)[1],'keys':keys})
    assert len(layers)==2 and len(curves)==2
    shader=files['shader'].read_text(encoding='utf-8-sig');assert 'Blend SrcAlpha One' in shader
    window=files['window'].read_text(encoding='utf-8-sig')
    instance=next(x for x in window.split('PrefabInstance:') if 'm_SourcePrefab: {fileID: 100100000, guid: 8a5ca138f6cd6f44596340e6d9368f08' in x)
    overrides=[{'property':a,'value':b} for a,b in re.findall(r'propertyPath: ([^\n]+)\n      value: ([^\n]*)',instance)]
    sources=list(files.values())+[Path(str(files['texture'])+'.meta'),ROOT/'Source/Rok/RokUnityModalWidget.cpp',Path(__file__).resolve()]
    result={'schema':'rok.building.preview_glow.source.v1','source_window_name':'UI_Win_BuildingUpdate (no UI_Win_BuildUpLevel file in current source)',
        'layers':sorted(layers,key=lambda x:x['sibling']),'curves':curves,'duration':2,'loop':True,'sample_rate':60,
        'source_instance':{'parent':'Rect/pl_content/Left','sibling_order':1,'relation':'Sibling before pl_buildImg; not a child destroyed by Loadleft','overrides':overrides},
        'shader':{'path':paths['shader'],'blend':'SrcAlpha One','formula':'C=(tex2D(MainTex,UV*ST.xy+ST.zw)+TextureSampleAdd)*vertexColor*materialColor; optional clip multiplies alpha; output.rgb = destination.rgb + C.rgb*C.a','material_color':[1,1,1,1],'note':'Saved material Src/Dst blend values do not override hard-coded shader Blend. Serialized image alpha is superseded by Animator curves during play.'},
        'rotation':'DeemoRotate.Update: transform.Rotate(rotateSpeed * Time.deltaTime, Space.Self). Layer2 has Y180 mirror, so do not copy z-speed as same screen rotation direction.',
        'ue_current':{'StarA':{'size':[430,430],'position':[213,260],'angle':12,'tint':[1,.78,.25,.55]},'StarB':{'size':[360,360],'position':[248,295],'angle':-18,'tint':[1,.68,.16,.32]},'material':'ordinary Slate image brush; not explicit source additive material','animation':'No source alpha loop or DeemoRotate for these images in BuildBuildingUpgrade'},
        'static_differences':['Source sizes425 and550 vs UE430 and360','Source RGB white vs UE extra gold tint','Source two-second alpha curves .15/.06/.15 and .1/.2/.1 vs UE fixed .55/.32','Source local rotations+4/+3 deg/s with second mirrored Y180 vs UE fixed12/-18','Source hardcoded additive SrcAlpha One vs current ordinary alpha UI brush'],
        'native_status':'Not sampled with real tick. Existing BuildingSourcePreview native disabled all Animators and ran editor updates; its static glow background is not proof of source animation or default-time alpha.',
        'next_scope':['Dedicated glow widget/material and source timeline, leaving building preview image/data untouched','New isolated native PlayMode probe with original effect animator and original DeemoRotate active; record actual alpha/quaternion/mesh at0,.5,1,1.5,2+ plus background/individual layers','Verify Gamma additive composite separately from UE linear blending; compare actual event/window clock rather than invent source phase','Retain source sibling order, ancestor layout and clipping; shared opening-window animation remains separate'],
        'sources':[{'path':p.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sources]}
    out=ROOT/'Saved/RokUnityMigration/BuildingPreviewGlowSource';out.mkdir(parents=True,exist_ok=True);(out/'source_glow.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');return result
if __name__=='__main__':print(json.dumps(build(),ensure_ascii=True,indent=2))
