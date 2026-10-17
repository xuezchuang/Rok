"""Stage source glyph alpha masks with semantic text and Color32; no asset import."""
from pathlib import Path
import argparse, hashlib, json, shutil
from PIL import Image
from VerifyRokBarracksSourceText import verify
from rok_data_paths import DATA_ROOT
ROOT=Path(__file__).resolve().parents[1]
def build(directory):
    source=Path(directory).resolve();verify(source)
    target=DATA_ROOT/'BarracksSourceText';target.mkdir(parents=True,exist_ok=True)
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    data=json.loads((source/'source_text_meshes.json').read_text(encoding='utf-8'));atlases={}
    for item in data['texts']:
        old=item['atlas']
        if old not in atlases:
            image=Image.open(source/old).convert('RGBA');white=Image.new('RGBA',image.size,(255,255,255,255));white.putalpha(image.getchannel('A'))
            name='glyph_'+item['atlasSha256']+'.png';white.save(target/name);atlases[old]={'path':name,'sha256':sha(target/name),'source_sha256':item['atlasSha256']}
        item['atlas']=atlases[old]['path'];item['atlasSha256']=atlases[old]['sha256']
    (target/'texts.json').write_text(json.dumps(data,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
    for name in ['ReferenceInputs.json','source_text_meshes.json']:shutil.copyfile(source/name,target/name)
    manifest={'schema':'rok.barracks.source_text.package.v1','texts_sha256':sha(target/'texts.json'),'catalog_sha256':sha(DATA_ROOT / 'barracks_upgrade_attribute.json'),'input_sha256':sha(target/'ReferenceInputs.json'),'native_mesh_sha256':sha(target/'source_text_meshes.json'),'native_directory':str(source),'atlases':list(atlases.values())}
    (target/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8');print(target)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('directory');build(p.parse_args().directory)
