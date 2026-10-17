"""Prepare isolated village day/night source render. Does not launch Unity."""
import json,shutil,hashlib
from pathlib import Path
from BuildRokUnityVillageDayNight import ROOT,OUT

def main():
    target=ROOT/'Saved/UnityVillageDayNightProbe'; assets=target/'Assets'; assets.mkdir(parents=True,exist_ok=True)
    copied=[]
    def copy(src,dst):
        dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
        copied.append({'source':src.relative_to(ROOT).as_posix(),'copy':dst.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(src.read_bytes()).hexdigest()})
    original=ROOT/'Saved/UnityVillagePresentationProbe/Assets/Source'
    for src in original.iterdir():
        if src.is_file():copy(src,assets/'Source'/src.name)
    data=json.loads(OUT.read_text(encoding='utf-8'))
    for record in [m['source'] for m in data['materials'].values()]+[r for r in data['sources'] if r['path'].endswith('.shader')]:
        src=ROOT/record['path'];copy(src,assets/'Source'/src.name);copy(Path(str(src)+'.meta'),assets/'Source'/(src.name+'.meta'))
    for src in [ROOT/'Scripts/ProbeRokUnityVillagePresentation.cs',ROOT/'Saved/UnityVillagePresentationProbe/Assets/ProbeDependencies.cs',ROOT/'Scripts/RenderRokUnityVillageDayNight.cs']:
        copy(src,assets/src.name)
    for relative in ('Packages/manifest.json','ProjectSettings/ProjectVersion.txt'):
        copy(ROOT/'Saved/UnityVillagePresentationProbe'/relative,target/relative)
    report={'prepared':True,'launched':False,'target':str(target),'source_copies':copied,'scope':'Original source prefab/assets/shaders. Explicit snapshot driver applies documented material state and light parameters; not source async service/coroutine or server clock emulation.'}
    OUT.with_name('unity_village_day_night_prepare.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(target)
if __name__=='__main__':main()
