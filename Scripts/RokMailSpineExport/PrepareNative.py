"""Prepare isolated original SkeletonGraphic; never launch an engine."""
import hashlib,json,shutil
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from rok_data_paths import resolve
ROOT=Path(__file__).resolve().parents[2]
TARGET=ROOT/'Saved/UnityMailSpineProbe'
def main():
    assets=TARGET/'Assets';assets.mkdir(parents=True,exist_ok=True);copied=[]
    def copy(src,dst):
        dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
        copied.append({'source':src.relative_to(ROOT).as_posix(),'copy':dst.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(src.read_bytes()).hexdigest()})
    for source,dest in [('ref/client-unity/Assets/Third/Spine','Spine'),('ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UE_TreasureBox','TreasureBox')]:
        directory=resolve(source)
        for src in directory.rglob('*'):
            if src.is_file():copy(src,assets/dest/src.relative_to(directory))
    for relative in ['ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_mail/UIMail/UI_Item_MailBtn.prefab','ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_AnimationBox.prefab']:
        src=resolve(relative);copy(src,assets/'Source'/src.name);copy(Path(str(src)+'.meta'),assets/'Source'/(src.name+'.meta'))
    copy(ROOT/'Scripts/RokMailSpineExport/RenderNative.cs',assets/'Editor/RenderNative.cs')
    (TARGET/'Packages').mkdir(exist_ok=True)
    modules=['ui','uielements','animation','physics','physics2d','particlesystem','imageconversion','jsonserialize','imgui']
    (TARGET/'Packages/manifest.json').write_text(json.dumps({'dependencies':{'com.unity.ugui':'1.0.0',**{'com.unity.modules.'+m:'1.0.0' for m in modules}}}),encoding='utf-8')
    (TARGET/'ProjectSettings').mkdir(exist_ok=True)
    (TARGET/'ProjectSettings/ProjectVersion.txt').write_text('m_EditorVersion: 2019.4.28f1c1\n',encoding='utf-8')
    output=ROOT/'Saved/RokUnityMigration/MailSpine/native_prepare.json'
    output.write_text(json.dumps({'prepared':True,'launched':False,'project':str(TARGET),'source_copies':copied,'scope':'Original nested mail SkeletonGraphic plus original PMA material, extracted parent Rect; isolated tenfold camera magnification, not full MailMediator.'},indent=2),encoding='utf-8')
    print(output)
if __name__=='__main__':main()
