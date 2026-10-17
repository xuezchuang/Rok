"""Prepare six semantic UGUI glyph exports; never runs Unity or edits old probes."""
import hashlib, json, shutil, subprocess
from pathlib import Path
from PrepareRokHospitalExplanationNativeReference import parse_languages
ROOT = Path(__file__).resolve().parents[1]
TARGET = ROOT / 'Saved/UnityBarracksSourceTextProbe'
def main():
    sha=lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    binary=ROOT/'ref/client-unity/Assets/StreamingAssets/Config/Bin/Language.bin'
    language={int(r[0]):r[1] for r in parse_languages(binary)}
    native=ROOT/'Saved/UnityBarracksUpgradeAttributeNativeReferenceProbe/Artifacts/native_20260912_130037_811/render_evidence.json'
    assert sha(native)=='481af78aefa943372f46129077ed161f966c1341e377995b66cac807ce2883c6'
    evidence=json.loads(native.read_text(encoding='utf-8'))
    specs=[]
    def add(key, text, item, rich=False):
        rect=item['rect']; color=item['color']
        specs.append(dict(key=key,text=text,fontSize=item['fontSize'],style=item.get('fontStyle',0),alignment=item['alignment'],
          width=rect['width'],height=rect['height'],x=rect['x'],y=rect['y'],rich=rich,color=color,
          horizontalOverflow=0,verticalOverflow=0,lineSpacing=1,bestFit=False))
    add('level_before',language[180306].format(1),evidence['levelBefore'],True)
    add('level_after','2',evidence['levelAfter'],True)
    for i,row in enumerate(evidence['rows']):
        add('label_'+str(i),language[[180329,180314][i]],row['label'])
        add('value_'+str(i),language[180356].format([20,5][i],[30,5][i]),row['value'],True)
    renderer=ROOT/'Scripts/RenderRokBarracksSourceText.cs'
    sources=[binary,native,Path(__file__).resolve(),renderer,
      ROOT/'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Item_Buildinglevelup.prefab',
      ROOT/'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_AttrItem.prefab']
    for folder in ['Assets/Editor','Packages','ProjectSettings','Validation']: (TARGET/folder).mkdir(parents=True,exist_ok=True)
    copied=TARGET/'Assets/Editor/RenderRokBarracksSourceText.cs';shutil.copyfile(renderer,copied)
    inputs=dict(schema='rok.barracks.source_text.inputs.v1',sourceTexts=specs,
        sources=[dict(path=p.relative_to(ROOT).as_posix(),sha256=sha(p)) for p in sources],
        renderer_sha256=sha(renderer),copied_renderer_sha256=sha(copied),
        scope='Six original semantic Text objects only; original built-in Arial, source regular/bold, rich colors and actual source rectangles. No full UI raster replacement.')
    (TARGET/'ReferenceInputs.json').write_text(json.dumps(inputs,ensure_ascii=False,indent=2),encoding='utf-8')
    (TARGET/'Packages/manifest.json').write_text(json.dumps({'dependencies':{k:'1.0.0' for k in ['com.unity.ugui','com.unity.modules.ui','com.unity.modules.imageconversion','com.unity.modules.jsonserialize','com.unity.modules.imgui']}}),encoding='utf-8')
    (TARGET/'ProjectSettings/ProjectVersion.txt').write_text('m_EditorVersion: 2019.4.28f1c1\n',encoding='utf-8')
    unity=Path('C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data')
    refs=list((unity/'Managed/UnityEngine').glob('*.dll'))+list((unity/'MonoBleedingEdge/lib/mono/4.7.1-api').glob('*.dll'))+list((unity/'MonoBleedingEdge/lib/mono/4.7.1-api/Facades').glob('*.dll'))
    refs += [unity/'Managed/UnityEditor.dll',ROOT/'Saved/UnityBarracksUpgradeAttributeNativeReferenceProbe/Library/ScriptAssemblies/UnityEngine.UI.dll']
    args=['/nologo','/target:library','/define:UNITY_EDITOR','/out:'+str(TARGET/'Validation/ReferenceSyntax.dll')]+['/reference:'+str(p) for p in refs]+[str(copied)]
    rsp=TARGET/'Validation/compiler.rsp';rsp.write_text('\n'.join('"'+a+'"' for a in args),encoding='utf-8')
    run=subprocess.run([str(unity/'Tools/Roslyn/csc.exe'),'@'+str(rsp)],capture_output=True)
    (TARGET/'Validation/compiler.log').write_bytes(run.stdout+run.stderr)
    assert run.returncode==0,'See isolated Validation/compiler.log'
    print('Prepared 6 source Text specs; compiler-only PASS; Unity not started:',TARGET)
if __name__=='__main__':main()
