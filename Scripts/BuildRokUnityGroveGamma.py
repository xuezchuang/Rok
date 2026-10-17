"""Independent source lighting/texture contract; never updates center-scale evidence."""
import hashlib,json,re
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/RokUnityMigration/GroveGamma'
PREFAB='Grove_co_tree_01_adorning'

def build():
    source=ROOT/'ref/client-unity'
    mediator=source/'Assets/Scripts/Hotfix/MVC/GlobalMediator/GameEventGlobalMediator.cs'
    text=mediator.read_text(encoding='gb18030')
    section=text.split('private Color[][] lightSetting',1)[1].split('private float[] m_lightSettingValue',1)
    colors=[[float(v.strip().rstrip('f')) for v in m.split(',')] for m in re.findall(r'new Color\(([^)]+)\)',section[0])]
    intensities=[float(v.strip().rstrip('f')) for v in re.search(r'new\[\] \{([^}]+)',section[1])[1].split(',')]
    assert len(colors)==8 and len(intensities)==4
    samples=[]
    for i,name in ((1,'day'),(2,'dusk'),(3,'night')):
        ambient,directional=colors[i*2:i*2+2]
        samples.append(dict(name=name,preset=i,ambient=ambient,directional=directional,intensity=intensities[i],
            spriteColor=[a+b*intensities[i]*.75 for a,b in zip(ambient,directional)]))
    center=ROOT/'Saved/RokUnityMigration/GroveCenterScale/manifest.json'
    previous=json.loads(center.read_text())
    texture=source/'Assets/BundleAssets/land/Texture/map_tree_atlas.png'
    files=[mediator,texture,Path(str(texture)+'.meta'),source/'ProjectSettings/ProjectSettings.asset',
        source/f'Assets/BundleAssets/land/Tile/{PREFAB}.prefab',source/f'Assets/BundleAssets/land/Mesh/AutoMake/{PREFAB}.asset',
        source/'Assets/BundleAssets/land/Material/map_tree_nomask_1.mat',source/'Assets/BundleAssets/land/Shaders/customlandmaptree-16-nomask.shader',
        source/'Assets/Scripts/Client/Compoment/Map/LightManager.cs']
    # Resolve source LightManager without changing any reference file.
    if not files[-1].exists():
        matches=list((source/'Assets/Scripts').rglob('LightManager.cs'));assert len(matches)==1;files[-1]=matches[0]
    alpha=Image.open(texture).convert('RGBA').getchannel('A');hist=alpha.histogram()
    output=dict(schema='rok.grove_gamma.v1',prefab=PREFAB,dxf=1509,tree_scale=previous['bake_scale'],curve=previous['curve'],samples=samples,
        source_files=[dict(path=p.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in files],
        center_manifest_sha256=hashlib.sha256(center.read_bytes()).hexdigest(),
        source_png=dict(size=list(alpha.size),alpha_zero=hist[0],alpha_opaque=hist[255],alpha_partial=sum(hist[1:255])),
        contract=dict(lighting='Source stable presets; ambient + directional * intensity * .75, no saturation',
            fragment='Gamma tex2D(MainTex).rgb * SpriteColor.rgb; sampled alpha unchanged',
            vertex='Original source shader and original mesh at fixed source dxf1509; TreeRot remains original0',
            scope='Isolated fixed-camera tree asset color reference, not full GameView; transparent alpha blending and edges remain independent acceptance gates',
            native_texture='Record actual texture width/height/format/mips plus importer defaults/platform settings; source PNG histogram is not imported GPU alpha'))
    OUT.mkdir(parents=True,exist_ok=True);(OUT/'manifest.json').write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps(dict(samples=samples,source_png=output['source_png'])))
if __name__=='__main__':build()
