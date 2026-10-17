"""Extract local FAQ text and Chinese source rectangles; never launch an engine."""
import csv
import hashlib
import json
import re
import struct
from pathlib import Path
from BuildRokTrainParameterLayout import documents, vector
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text
from PIL import Image
from rok_data_paths import DATA_ROOT, label

ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / 'ref/client-unity/Assets/BundleAssets/UI'
PREFABS = UI / 'UIPrefabs/ui_prefabs_player/UIHelper'

def build():
    language, _ = read_language_lookup()
    sources = []
    def read(path):
        sources.append({'path': label(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        return path.read_text(encoding='utf-8-sig')
    rows = list(csv.DictReader(read(ROOT/'ref/config-tables/编译客户端配置/CSV/GameSprite.csv').splitlines()))[1:]
    binary_path=ROOT/'ref/client-unity/Assets/StreamingAssets/Config/Bin/GameSprite.bin'
    binary=binary_path.read_bytes();offset=0
    def take(fmt):
        nonlocal offset
        value=struct.unpack_from('<'+fmt,binary,offset)[0];offset+=struct.calcsize('<'+fmt);return value
    strings=[]
    for _ in range(take('i')):
        length=take('H');strings.append(binary[offset:offset+length].decode('utf-8'));offset+=length
    count,columns=take('i'),take('B');assert columns==4
    decoded=[[strings[take('i')] for _ in range(columns)] for _ in range(count)]
    assert offset==len(binary) and decoded==[[r[k] for k in ['ID','title','subTitle','text']] for r in rows]
    sources.append({'path':label(binary_path),'sha256':hashlib.sha256(binary).hexdigest()})
    entries = [{'id': int(r['ID']), 'group_id': int(r['title']), 'question_id': int(r['subTitle']), 'answer_id': int(r['text']),
                'group': localized_text(language,int(r['title']),'cn'), 'question': localized_text(language,int(r['subTitle']),'cn'),
                'answer': localized_text(language,int(r['text']),'cn')} for r in rows]
    # DataService.TableBase.QueryRecords preserves records[]/binary row order.
    # Mediator adds a parent only at the FIRST encounter of title, then appends every
    # child to the current flat tree; late repeated titles do not create new parents.
    seen=set();current=None
    for entry in entries:
        if entry['group_id'] not in seen:seen.add(entry['group_id']);current=entry['group_id']
        entry['display_group_id']=current
    for relative in ['Assets/Skyunion/RunTime/DataService/DataService.cs','Assets/Skyunion/RunTime/DataService/DataServiceBinary.cs','Assets/Scripts/Hotfix/MVC/View_Mediator/Helper/UI_Win_HelperMediator.cs']:
        read(ROOT/'ref/client-unity'/relative)
    assert len(entries)==70 and len({e['id'] for e in entries})==70
    assert len({e['group_id'] for e in entries})==7
    assert all(e[k] and '\ufffd' not in e[k] for e in entries for k in ('group','question','answer'))
    assert not any('<' in e['answer'] for e in entries), 'Rich text requires a dedicated renderer'
    atlases = {}
    for name,folder in [('common','ui_res_common'),('frame','ui_res_common'),('setting','ui_res_setting'),('mail','ui_res_mail')]:
        path=UI/f'UITextureStatic/{folder}/ui_{name}.png.meta'; text=read(path)
        guid=re.search(r'^guid: (\w+)',text,re.M)[1]; size=Image.open(Path(str(path)[:-5])).size
        sprites={}
        for block in re.split(r'\n    - serializedVersion:',text)[1:]:
            match=re.search(r'\n      internalID: (-?\d+)',block)
            if not match: continue
            rect=re.search(r'\n      rect:\n        serializedVersion: \d+\n        x: (\d+)\n        y: (\d+)\n        width: (\d+)\n        height: (\d+)',block)
            if not rect: continue
            sprites[match[1]]={'atlas':name,'sprite':re.search(r'\n      name: (.*)',block)[1],'uv':list(map(int,rect.groups())), 'atlas_size':list(size)}
        atlases[guid]=sprites
    def extract(path,root_size):
        docs=documents(read(path)); rects={k:v for k,v in docs.items() if v.startswith('RectTransform:') and 'm_GameObject:' in v}
        cache={}; result={}
        def resolve(key):
            if key in cache:return cache[key]
            r=rects[key];go=docs[re.search(r'm_GameObject: \{fileID: (-?\d+)',r)[1]]
            name=re.search(r'\n  m_Name: (.*)',go)[1];parent=re.search(r'm_Father: \{fileID: (-?\d+)',r)[1]
            comps=[docs[i] for i in re.findall(r'component: \{fileID: (-?\d+)',go)]
            flags=[int(m[1]) for c in comps if (m:=re.search(r'_arab_Layout: (\d+)',c))]
            f=0
            for flag in flags:
                if not(flag&1):f|=flag
            low,high,pivot,pos,delta=[vector(r,k) for k in ['m_AnchorMin','m_AnchorMax','m_Pivot','m_AnchoredPosition','m_SizeDelta']]
            if parent=='0': origin=[0.,0.];size=list(root_size);full=''
            else:
                po,ps,pp=resolve(parent)
                if f&2:low[0],high[0],pivot[0],pos[0]=1-high[0],1-low[0],1-pivot[0],-pos[0]
                size=[ps[i]*(high[i]-low[i])+delta[i] for i in range(2)]
                origin=[po[i]+ps[i]*(low[i]+(high[i]-low[i])*pivot[i])+pos[i]-pivot[i]*size[i] for i in range(2)]
                full=(pp+'/' if pp else '')+name
                if full in result: full+='__'+key
            node={'rect':[origin[0],root_size[1]-origin[1]-size[1],*size], 'source_rect':key,'active':'m_IsActive: 1' in go,'flip_x':bool(f&16), 'flags':f}
            scale=re.search(r'm_LocalScale: \{x: ([^,]+)',r)
            if scale and float(scale[1])<0:node['flip_x']=not node['flip_x']
            node['flip_y']=float(re.search(r'm_LocalScale: \{x: [^,]+, y: ([^,]+)',r)[1])<0
            rotation=float(re.search(r'm_LocalEulerAnglesHint: \{x: [^,]+, y: [^,]+, z: ([^}]+)',r)[1])
            assert rotation in (0,180,-180), rotation
            if rotation:node['flip_x']=not node['flip_x'];node['flip_y']=not node['flip_y']
            if node['flip_x']:node['rect'][0]+=(2*pivot[0]-1)*size[0]
            if node['flip_y']:node['rect'][1]-=(2*pivot[1]-1)*size[1]
            for c in comps:
                if 'm_FontData:' in c:
                    a=int(re.search(r'm_Alignment: (\d+)',c)[1]);node['alignment']=(a//3)*3+(2-a%3 if f&8 else a%3)
                    node['font_px']=int(re.search(r'm_FontSize: (\d+)',c)[1]);node['bold']='m_FontStyle: 1' in c
                    for field, source in [('best_fit','m_BestFit'),('min_size','m_MinSize'),('max_size','m_MaxSize'),('horizontal_overflow','m_HorizontalOverflow'),('vertical_overflow','m_VerticalOverflow')]:
                        node[field]=int(re.search(source+r': (\d+)',c)[1])
                    node['line_spacing']=float(re.search(r'm_LineSpacing: ([^\r\n]+)',c)[1])
                    lid=int(re.search(r'm_languageId: (-?\d+)',c)[1]);node['text_id']=lid;node['text']=localized_text(language,lid,'cn') if lid>0 else ''
                sprite=re.search(r'm_Sprite: \{fileID: (-?\d+), guid: (\w+)',c)
                if sprite and sprite[2] in atlases:
                    node.update(atlases[sprite[2]][sprite[1]])
                if 'm_Color:' in c and ('m_FontData:' in c or 'm_Sprite:' in c):
                    color=re.search(r'm_Color: \{r: ([^,]+), g: ([^,]+), b: ([^,]+), a: ([^}]+)',c)
                    node['rgba']=list(map(float,color.groups()))
            result[full]=node;cache[key]=(origin,size,full);return cache[key]
        for key in rects:resolve(key)
        return result
    layouts={
        'window':extract(PREFABS/'UI_Win_Helper.prefab',[1164,787]),
        'frame':extract(UI/'UIPrefabs/ui_prefabs_common/UICommon/UI_Model_Window_Type3.prefab',[1148,754]),
        'group':extract(PREFABS/'UI_Item_Helper.prefab',[1074,43]),
        'question':extract(PREFABS/'UI_Item_HelperItem.prefab',[1040,54])}
    layouts['frame']['title/lbl_title']['text']=localized_text(language,300082,'cn')
    layouts['frame']['title/lbl_title']['text_id']=300082
    result={'schema':'rok.help.v1','entries':entries,'layouts':layouts,'sources':sources,'group_order':list(dict.fromkeys(e['group_id'] for e in entries)),
            'boundary':'Local FAQ only; customer-service SDK unavailable. T1 lifecycle belongs to hosting Feature screen.'}
    out=DATA_ROOT / 'help_catalog.json';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'entries':len(entries),'groups':len(result['group_order']),'output':str(out)}))
    return result

if __name__=='__main__':build()
