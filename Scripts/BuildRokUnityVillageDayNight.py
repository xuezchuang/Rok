"""Extract explicit village day/night presentation snapshots from source."""
import json,re,hashlib
from pathlib import Path
from PIL import Image
from rok_data_paths import DATA_ROOT, label, resolve
ROOT=Path(__file__).resolve().parents[1]
OUT=DATA_ROOT / 'unity_village_day_night.json'
def source(path):
    path=Path(path);return {'path':label(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
def rgb(body,key):
    m=re.search(r'- '+re.escape(key)+r': \{r: ([^,]+), g: ([^,]+), b: ([^,]+), a: ([^}]+)\}',body)
    assert m,key
    return [float(v) for v in m.groups()]
def lighting(a,b,t):
    t=min(max(t,0),1)
    ambient=[x+(y-x)*t for x,y in zip(a['ambient'],b['ambient'])]
    direction=[x+(y-x)*t for x,y in zip(a['directional'],b['directional'])]
    intensity=a['intensity']+(b['intensity']-a['intensity'])*t
    return [x+y*intensity*.75 for x,y in zip(ambient,direction)]
def build():
    mediator=ROOT/'ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/GameEventGlobalMediator.cs'
    # Source mediator is legacy Chinese encoding; only numeric declarations are parsed.
    body=mediator.read_text(encoding='gb18030')
    durations=[int(v.strip()) for v in re.search(r'm_lighttingTime = new\[\] \{([^}]+)',body)[1].split(',')]
    section=body.split('private Color[][] lightSetting',1)[1].split('private float[] m_lightSettingValue',1)
    colors=[[float(v.strip().rstrip('f')) for v in m.split(',')] for m in re.findall(r'new Color\(([^)]+)\)',section[0])]
    intensities=[float(v.strip().rstrip('f')) for v in re.search(r'new\[\] \{([^}]+)',section[1])[1].split(',')]
    assert len(colors)==8 and len(intensities)==4 and durations==[57,3,1677,3,57,3,597,3]
    presets=[{'name':name,'ambient':colors[i*2],'directional':colors[i*2+1],'intensity':intensities[i]} for i,name in enumerate(('dawn','day','dusk','night'))]
    materials={}
    for name,asset in [('day','map_building'),('night','city_building_night')]:
        path=ROOT/f'ref/client-unity/Assets/BundleAssets/Scene/City/city_function/{asset}.mat'
        text=path.read_text(encoding='utf-8-sig')
        materials[name]={'source':source(path),'color1':rgb(text,'_Color_1'),'color2':rgb(text,'_Color_2'),'color3':rgb(text,'_Color_3'),'render_queue_override':int(re.search(r'm_CustomRenderQueue: (-?\d+)',text)[1])}
    village_path=DATA_ROOT / 'unity_village_presentation.json'
    village=json.loads(village_path.read_text(encoding='utf-8')); masks=[]
    for item in village['items']:
        mask=resolve(item['sprites'][0]['mask']['path'])
        with Image.open(mask) as image: maximum=image.convert('RGBA').getchannel('B').getextrema()[1]
        assert maximum==0
        masks.append({'transform_id':item['source_transform_id'],'mask':source(mask),'blue_max':maximum})
    scenarios=[{'name':'night_on_phase5_initial','from_preset':2,'to_preset':3,'duration':3,'initial_night':False,'requested_night':True,'condition':'CalcDayNight initial oldIndex=-1 at phase5 sets night while dusk-to-night interpolation starts.'},
               {'name':'night_off_phase0_boundary','from_preset':0,'to_preset':0,'duration':3,'initial_night':True,'requested_night':False,'condition':'Continuous phase7 has completed at dawn; phase0 requests day material while existing dawn lighting is held.'}]
    samples=[]
    for scenario in scenarios:
        for delay in (0,.5,1):
            for elapsed in (0,.5,1,3):
                samples.append({'name':scenario['name']+'_delay'+str(delay)+'_t'+str(elapsed),'scenario':scenario['name'],'from_preset':scenario['from_preset'],'to_preset':scenario['to_preset'],'duration':3,'elapsed':elapsed,'fixed_delay':delay,'resolved_night':scenario['requested_night'] if elapsed>=delay else scenario['initial_night'],'sprite_color':lighting(presets[scenario['from_preset']],presets[scenario['to_preset']],elapsed/3)})
    records=[source(mediator)]
    for relative in ('Client/LightingManager/LightManager.cs','Client/LightingManager/NightingObject.cs','Client/LightingManager/NightingMask.cs','Client/Compoment/MaskSprite.cs'):
        records.append(source(ROOT/'ref/client-unity/Assets/Scripts'/relative))
    for relative in ('Map/Building/map_building_with_mask-5.shader','Map/Shader/cm_city_mask_night.shader'):
        records.append(source(ROOT/'ref/client-unity/Assets/BundleAssets'/relative))
    result={'schema':'rok.village.day_night.v1','source_village':source(village_path),'target_ids':[639,599],'presets':presets,'phase_durations':durations,'loop_seconds':sum(durations),'materials':materials,'masks':masks,'sources':records,'scenarios':scenarios,'samples':samples,
            'contract':{'snapshot_only':True,'phase_input':'Explicit local presentation parameters, no server clock or authority state synthesized.','lighting':'Clamp elapsed/duration then Lerp ambient/directional/intensity separately; SpriteColor=ambient+directional*intensity*0.75, no RGB saturation.','material_delay':'Fixed diagnostic delay; resolved state is a post-deadline snapshot, elapsed>=delay. This does not reproduce coroutine frame scheduling, random distribution, interruption or pooling.','caller':'Normal phase5 retains previous day material until phase6. Initial oldIndex=-1 also requests material on odd phases. Night request is independent from light interpolation.','mask':'All 13 source masks have B=0: no emissive windows may be fabricated.','night_shader':'Gamma RGB team formula times SpriteColor, then +Color3.rgb*saturate(abs(sin(shaderSeconds*.5))+.5)*mask.b. Opacity=Main.a*Renderer.a*saturate(2*mask.r)*CityTransparency. Color3.a is not used.'}}
    OUT.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(f'Extracted {len(samples)} explicit snapshots: {OUT}')
if __name__=='__main__':build()
