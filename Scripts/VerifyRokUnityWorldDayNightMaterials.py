"""Verify new resource night graphs against the independently tested village graph."""
import ast,hashlib,json
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
def shader(path):
    text=path.read_text(encoding='utf-8-sig')
    tree=ast.parse(text)
    return text,next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='DAY_SHADER_CODE' for t in n.targets))
def main():
    failures=[];records=[]
    _,reference=shader(ROOT/'Scripts/CreateRokUnityVillagePresentationMaterial.py')
    for kind,count in [('Food',9),('Wood',18),('Stone',18)]:
        path=ROOT/f'Scripts/CreateRokUnity{kind}DepositMaterial.py';text,code=shader(path)
        if code!=reference:failures.append(kind+' source Gamma/night formula differs')
        if 'transparency, night_light)' not in text or '"Transparency", "NightLight")' not in text or 'night_light = vector("NightLight", (0, 0, 0, 0))' not in text:failures.append(kind+' input/default missing')
        data=json.loads((ROOT/f'Saved/RokUnityMigration/unity_{kind.lower()}_deposit.json').read_text(encoding='utf-8'))
        if len(data['items'])!=count:failures.append(kind+' count')
        for item in data['items']:
            mask=item['sprites'][0]['mask'];p=ROOT/mask['path']
            if hashlib.sha256(p.read_bytes()).hexdigest()!=mask['sha256']:failures.append(kind+' mask hash')
            with Image.open(p) as image:
                if image.convert('RGB').getchannel('B').getextrema()[1]!=0:failures.append(kind+' nonzero mask blue')
        records.append({'kind':kind,'source_masks':count,'generator_sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
    report={'passed':not failures,'failures':failures,'records':records,'limits':['Run existing independent VillageDayNightMaterial verifier for source shader hashes/math.','UE material import, continuous runtime and Gamma edges remain separate gates.']}
    out=ROOT/'Saved/RokUnityMigration/WorldDayNightContinuous/material_verify.json';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report));return bool(failures)
if __name__=='__main__':raise SystemExit(main())
