"""Prepare continuous native timing evidence; never launch an engine."""
import hashlib,json,re,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
TARGET=ROOT/'Saved/UnityWorldDayNightProbe'
def main():
    assets=TARGET/'Assets'; assets.mkdir(parents=True,exist_ok=True)
    records=[]
    def copy(src,dst):
        dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
        records.append({'source':str(src.relative_to(ROOT)),'destination':str(dst.relative_to(ROOT)),'sha256':hashlib.sha256(src.read_bytes()).hexdigest()})
    for src in (ROOT/'Saved/UnityVillageDayNightProbe/Assets/Source').iterdir():
        if src.is_file():copy(src,assets/'Source'/src.name)
    client=ROOT/'ref/client-unity/Assets'
    paths=['Scripts/Client/LightingManager/LightManager.cs','Scripts/Client/LightingManager/Lightning.cs','Scripts/Client/LightingManager/NightingObject.cs','Scripts/Client/LightingManager/NightingMask.cs','Scripts/Client/Compoment/MaskSprite.cs','Skyunion/RunTime/Utils/Core/MonoSingleton.cs','Skyunion/RunTime/Utils/Timer.cs']
    for path in paths:
        src=client/path;copy(src,assets/'Source'/src.name)
        meta=Path(str(src)+'.meta')
        if meta.exists():copy(meta,assets/'Source'/meta.name)
    src=client/'Scripts/Hotfix/MVC/GlobalMediator/GameEventGlobalMediator.cs'
    text=src.read_text(encoding='gb18030')
    def method(name):
        start=text.index('        public '+name);brace=text.index('{',start);depth=1;end=brace+1
        while depth:
            if text[end]=='{':depth+=1
            if text[end]=='}':depth-=1
            end+=1
        return text[start:end]
    # Exact source method bodies, isolated from unrelated mediator dependencies.
    bodies='\n'.join(method(n) for n in ['bool IsDay()','void DayNightNtf()','void DayNightInit()','void CalcDayNight(int forceIndex = -1)'])
    fields=text[text.index('        private int[] m_lighttingTime'):text.index('        private int m_time')]
    adapter='using System; using UnityEngine; using Skyunion; using Client;\npublic class ProbeSourceDayNight {\n'+fields+'\nint m_loopTime; int[] m_lightStepAccumTime=new int[8]; bool m_isInit,m_isForceDayNight; Timer m_dayNightTimer;\n'+bodies+'\n}\n'
    (assets/'ProbeSourceDayNight.cs').write_text(adapter,encoding='utf-8')
    records.append({'source':str(src.relative_to(ROOT)),'sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'extracted_methods':['IsDay','DayNightNtf','DayNightInit','CalcDayNight']})
    copy(ROOT/'Scripts/ProbeRokUnityWorldDayNight.cs',assets/'ProbeRokUnityWorldDayNight.cs')
    for rel in ['Packages/manifest.json','ProjectSettings/ProjectVersion.txt']:
        copy(ROOT/'Saved/UnityVillageDayNightProbe'/rel,TARGET/rel)
    package_path=TARGET/'Packages/manifest.json'
    packages=json.loads(package_path.read_text(encoding='utf-8'))
    source_packages=client.parent/'Packages/manifest.json'
    source_dependencies=json.loads(source_packages.read_text(encoding='utf-8'))['dependencies']
    # Preserve source built-in modules as a group: prefab BoxCollider, source
    # CanvasGroup, sprite rendering and any serialized engine components must
    # not be silently stripped by a reduced probe package manifest.
    packages['dependencies'].update({k:v for k,v in source_dependencies.items() if k.startswith('com.unity.modules.')})
    records.append({'source':str(source_packages.relative_to(ROOT)),'sha256':hashlib.sha256(source_packages.read_bytes()).hexdigest(),'copied_builtin_packages':sorted(k for k in source_dependencies if k.startswith('com.unity.modules.'))})
    package_path.write_text(json.dumps(packages,indent=2)+'\n',encoding='utf-8')
    out=ROOT/'Saved/RokUnityMigration/WorldDayNightContinuous';out.mkdir(parents=True,exist_ok=True)
    (out/'prepare.json').write_text(json.dumps({'target':str(TARGET),'copies':records,'asset_adapter':'Controlled callback completion; NOT Addressables latency evidence. Original MaskSprite callback body executes unchanged.','source_code':'Original Nighting/LightManager/Timer and exact extracted GEM methods; local seconds stub only.'},indent=2)+'\n',encoding='utf-8')
    print(TARGET)
if __name__=='__main__':main()
