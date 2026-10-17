"""Prepare only an isolated source grove render; root runs the engine."""
import hashlib,json,re,shutil
from pathlib import Path
from BuildRokUnityGroveCenterScale import ROOT,OUT,PREFAB


def main():
    source=ROOT/'ref/client-unity';target=ROOT/'Saved/UnityGroveCenterScaleProbe';index={};copies=[]
    for meta in (source/'Assets').rglob('*.meta'):
        m=re.search(r'^guid: ([0-9a-f]{32})',meta.read_text(encoding='utf-8-sig',errors='replace'),re.M)
        if m:index[m[1]]=Path(str(meta)[:-5])
    def copy(src,dst):
        dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst);copies.append({'source':src.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(src.read_bytes()).hexdigest()})
    todo=[source/f'Assets/BundleAssets/land/Tile/{PREFAB}.prefab'];seen=set()
    while todo:
        src=todo.pop()
        if src in seen:continue
        seen.add(src);dst=target/src.relative_to(source)
        if src.suffix=='.cs':
            assert src.name=='ObjectPoolItem.cs',f'Unreviewed behavior dependency {src}'
            dst.parent.mkdir(parents=True,exist_ok=True);dst.write_text('namespace Skyunion { public class ObjectPoolItem : UnityEngine.MonoBehaviour { public int m_max_spawn_number_high=3,m_max_spawn_number_mid=2,m_max_spawn_number_low=1; } }\n',encoding='utf-8')
        else:copy(src,dst)
        copy(Path(str(src)+'.meta'),Path(str(dst)+'.meta'))
        if src.suffix in ('.prefab','.mat'):
            text=src.read_text(encoding='utf-8-sig')
            guids=re.findall(r'guid: ([0-9a-f]{32})',text)
            if src.suffix=='.mat':guids=[re.search(r'm_Shader:.*guid: ([0-9a-f]{32})',text)[1],re.search(r'- _MainTex:\s*m_Texture:.*guid: ([0-9a-f]{32})',text)[1]]
            for guid in guids:
                if guid.startswith('0000000000000000'):continue
                assert guid in index,f'Unresolved GUID {guid}'
                todo.append(index[guid])
    copy(ROOT/'Scripts/RenderRokUnityGroveCenterScale.cs',target/'Assets/Editor/RenderRokUnityGroveCenterScale.cs')
    for p in ('ProjectSettings/ProjectSettings.asset','ProjectSettings/ProjectVersion.txt'):copy(source/p,target/p)
    packages=json.loads((source/'Packages/manifest.json').read_text(encoding='utf-8-sig'));(target/'Packages').mkdir(exist_ok=True)
    (target/'Packages/manifest.json').write_text(json.dumps({'dependencies':{k:v for k,v in packages['dependencies'].items() if k.startswith('com.unity.modules.')}},indent=2),encoding='utf-8')
    (OUT/'prepare.json').write_text(json.dumps({'project':str(target),'copies':copies,'stub':'ObjectPoolItem fields only; no pooling ownership behavior used. Original prefab/mesh/material/meta unchanged.','material_scope':'Source tree shader reads MainTex; unused serialized material slots excluded from dependency copying.','launched':False},indent=2)+'\n',encoding='utf-8')
    print(target)


if __name__=='__main__':main()
