"""Source-only extraction of HospitalMediator.OnExplain's Chinese local page."""
import json,re,hashlib,struct
from pathlib import Path
from BuildRokTrainParameterLayout import documents,vector
from BuildRokUnityEconomyCatalog import read_language_lookup,localized_text,LANGUAGE_WORKBOOK
from rok_data_paths import DATA_ROOT, label
ROOT=Path(__file__).resolve().parents[1]
PREFAB=ROOT/'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIHospital/UI_Win_Hospital.prefab'
MEDIATOR=ROOT/'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Hospital/HospitalMediator.cs'
BINARY=ROOT/'ref/client-unity/Assets/StreamingAssets/Config/Bin/Language.bin'
ARABIC=ROOT/'ref/client-unity/Assets/Scripts/Client/Compoment/UIExtend/ArabicSupport.cs'
ARAB_LAYOUT=ROOT/'ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs'
LANGUAGE_UTILS=ROOT/'ref/client-unity/Assets/Scripts/Client/Utils/LanguageUtils.cs'
REQUIRED_SOURCES=[PREFAB,MEDIATOR,LANGUAGE_WORKBOOK,BINARY,ARABIC,ARAB_LAYOUT,LANGUAGE_UTILS]
def runtime_text():
    data=BINARY.read_bytes();pos=0
    def take(fmt):
        nonlocal pos
        result=struct.unpack_from('<'+fmt,data,pos)[0];pos+=struct.calcsize('<'+fmt);return result
    strings=[]
    for _ in range(take('i')):
        size=take('H');strings.append(data[pos:pos+size].decode('utf-8'));pos+=size
    count,cols=take('i'),take('B');assert cols==6
    values=[]
    for _ in range(count):
        row=[strings[take('i')] for _ in range(cols)]
        if row[0]=='181001':values.append(row[1])
    assert pos==len(data) and len(values)==1
    return values[0]
def build():
    docs=documents(PREFAB.read_text(encoding='utf-8-sig'));rects={k:v for k,v in docs.items() if v.startswith('RectTransform:') and 'm_GameObject:' in v};cache={};nodes={}
    def resolve(key):
        if key in cache:return cache[key]
        r=rects[key];go=docs[re.search(r'm_GameObject: \{fileID: (-?\d+)',r)[1]];name=re.search(r'\n  m_Name: (.*)',go)[1];parent=re.search(r'm_Father: \{fileID: (-?\d+)',r)[1]
        comps=[docs[x] for x in re.findall(r'component: \{fileID: (-?\d+)',go)];flags=0
        for c in comps:
            f=re.search(r'_arab_Layout: (\d+)',c)
            if f and not int(f[1])&1:flags|=int(f[1])
        low,high,pivot,pos,delta=[vector(r,k) for k in ['m_AnchorMin','m_AnchorMax','m_Pivot','m_AnchoredPosition','m_SizeDelta']]
        if parent=='0':origin=[0.,0.];size=delta;path=''
        else:
            po,ps,pp=resolve(parent)
            if flags&2:low[0],high[0],pivot[0],pos[0]=1-high[0],1-low[0],1-pivot[0],-pos[0]
            size=[ps[i]*(high[i]-low[i])+delta[i] for i in range(2)];origin=[po[i]+ps[i]*(low[i]+(high[i]-low[i])*pivot[i])+pos[i]-pivot[i]*size[i] for i in range(2)];path=(pp+'/' if pp else '')+name
        nodes[path]={'origin':origin,'size':size,'flags':flags,'anchored_position':pos}
        for c in comps:
            if 'm_FontData:' in c:
                a=int(re.search(r'm_Alignment: (\d+)',c)[1]);nodes[path].update(font_px=int(re.search(r'm_FontSize: (\d+)',c)[1]),alignment=a//3*3+(2-a%3 if flags&8 else a%3),line_spacing=float(re.search(r'm_LineSpacing: ([^\r\n]+)',c)[1]))
        cache[key]=(origin,size,path);return cache[key]
    for k in rects:resolve(k)
    root=nodes['']['size'];offset=[(1440-root[0])/2,(810-root[1])/2]
    for n in nodes.values():n['rect']=[offset[0]+n['origin'][0],offset[1]+root[1]-n['origin'][1]-n['size'][1],*n['size']]
    language,_=read_language_lookup();workbook_text=localized_text(language,181001,'cn');text=runtime_text();yellow=re.findall(r'<color=yellow>(.*?)</color>',text)
    assert len(re.findall(r'(?m)^[1-7]\.',text))==7 and len(yellow)==13
    med=MEDIATOR.read_text(encoding='utf-8-sig');assert 'preferredHeight- view.m_lbl_explain' in med and 'm_isShowedExplain' in med
    selected={k:v for k,v in nodes.items() if k.startswith('Rect/Right/pl_explain') or k in ['Rect','Rect/Right','Rect/Left','Rect/Left/pl_count']}
    out={'schema':'rok.hospital.explanation.v1','language_id':181001,'source_text':text,'rich_text':text.replace('<color=yellow>','<yellow>').replace('</color>','</>'),'yellow_segments':yellow,'text_source_comparison':{'workbook_matches_runtime_bin':workbook_text==text,'authoritative':'Language.bin','workbook_text':workbook_text if workbook_text!=text else None},'nodes':selected,'sources':[{'path':label(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in REQUIRED_SOURCES],'lifecycle':'One existing page; first open sets text/content height, back only toggles visibility, same-instance scroll preserved; new window resets.'}
    target=DATA_ROOT / 'hospital_explanation.json';target.write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8');print(target);print('181001 workbook/runtime identical:',workbook_text==text);return out
if __name__=='__main__':build()


