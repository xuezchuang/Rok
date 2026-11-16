"""Bake the four original Unity Captain backgrounds and locked icon art into packaged RokData.
Run from Windows with Unity 2019.4.28f1c1 and Pillow. Pass --motion to bake all four animated scenes.
Only the scratch project under Saved is opened.
"""
from pathlib import Path
import re
import shutil
import subprocess
import sys
from PIL import Image
root = Path(__file__).resolve().parents[1]
src = root / 'ref/client-unity'
work = root / 'Saved/RokUnityMigration/Commander/SceneBake/UnitySceneProbe'
scenes = 'Assets/BundleAssets/Scene/City/city_heroshow'
motion = sys.argv[1:] == ['--motion']
if sys.argv[1:] and not motion:
    raise SystemExit('Usage: BuildRokCommanderScenes.py [--motion]')
scene_files = [f'{scenes}/HeroShow_{i:02}.prefab' for i in range(1, 5)]
uv_animation = 'Assets/BundleAssets/Effect/Scripts/UVAnimation.cs'
index = {}
for path in (src/'Assets').rglob('*.meta'):
    try: data = path.open('rb').read(180)
    except OSError: continue
    match = re.search(rb'^guid: ([a-f0-9]{32})',data,re.M)
    if match: index[match[1]] = path.relative_to(src).as_posix()[:-5]
pending = scene_files + [uv_animation]
pending += [p.relative_to(src).as_posix() for p in (src/'Assets/Third/Spine').rglob('*.cs') if '/Editor/' not in p.as_posix()]
seen = set()
while pending:
    name = pending.pop()
    if name in seen: continue
    seen.add(name)
    p = src/name
    if not p.is_file() or (p.suffix == '.cs' and name != uv_animation
                           and not name.startswith('Assets/Third/Spine/')):
        continue
    out = work/name
    out.parent.mkdir(parents=True, exist_ok=True)
    for f in (p,Path(str(p)+'.meta')):
        if f.is_file():
            shutil.copyfile(f, out.parent/f.name)
    sheet = p.with_suffix('.tpsheet')
    if p.suffix.lower() == '.png' and sheet.is_file():
        shutil.copyfile(sheet, out.parent/sheet.name)
    if p.suffix in ('.prefab','.mat','.asset','.controller','.anim','.renderTexture','.shader','.cginc','.fbx'):
        data=p.read_bytes()
        pending += [index[g] for g in re.findall(rb'guid: ([a-f0-9]{32})',data) if g in index]
        if p.suffix in ('.shader','.cginc'):
            pending += [str((p.parent/str(s,'utf-8')).resolve().relative_to(src.resolve())).replace('\\','/') for s in re.findall(rb'#include "([^"]+)"',data) if (p.parent/str(s,'utf-8')).is_file()]
for name in ['ProjectVersion.txt','TagManager.asset']:
    p=work/'ProjectSettings'/name
    p.parent.mkdir(exist_ok=True)
    shutil.copyfile(src/'ProjectSettings'/name,p)
p=work/'Packages/manifest.json'
p.parent.mkdir(exist_ok=True)
p.write_text('{"dependencies":{"com.unity.ugui":"1.0.0","com.unity.modules.animation":"1.0.0","com.unity.modules.particlesystem":"1.0.0","com.unity.modules.imgui":"1.0.0","com.unity.modules.imageconversion":"1.0.0","com.unity.modules.physics":"1.0.0","com.unity.modules.physics2d":"1.0.0"}}',encoding='utf-8')

editor = work / 'Assets/Editor'
editor.mkdir(parents=True, exist_ok=True)
shutil.copyfile(Path(__file__).with_name('RenderRokCommanderScenes.cs'), editor / 'RokCommanderSceneBake.cs')
unity = Path(r'C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Unity.exe')
method = 'StartMotion' if motion else 'Run'
unity_args = [str(unity), '-batchmode', '-projectPath', str(work),
              '-executeMethod', f'RokCommanderSceneBake.{method}',
              '-logFile', str(work.parent/('UnityMotion.log' if motion else 'Unity.log'))]
if not motion:
    unity_args.append('-quit')
subprocess.run(unity_args, check=True, timeout=420 if motion else 180, creationflags=subprocess.CREATE_NO_WINDOW)
if motion:
    output = root / 'Content/Movies/Commander'
    output.mkdir(parents=True, exist_ok=True)
    for file in scene_files:
        name = Path(file).stem + '.mp4'
        clip = work.parent/'UnityMotion'/name
        if clip.stat().st_size == 0:
            raise RuntimeError(f'Unity produced an empty clip: {clip}')
        shutil.copyfile(clip, output/name)
    raise SystemExit(0)

output = root / 'Content/RokData/Commander'
(output/'Scenes').mkdir(parents=True, exist_ok=True)
for file in scene_files:
    name = Path(file).stem + '.png'
    shutil.copyfile(work.parent/'UnityBackgrounds'/name, output/'Scenes'/name)

def gray(source, target):
    # UISprites/DefaultGray, with alpha carried through from the source sprite.
    source = source.convert('RGBA')
    luminance = source.convert('RGB').convert('L', (.299, .518, .184, 0))
    Image.merge('RGBA', (luminance, luminance, luminance, source.getchannel('A'))).save(target)


source = src/'Assets/BundleAssets/UI/UITextureStatic'
(output/'Textures').mkdir(exist_ok=True)
for name in ('hero_skill', 'hero_skill2'):
    gray(Image.open(source/f'ui_res_hero/{name}.png'), output/f'Textures/{name}_gray.png')
