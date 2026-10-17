"""Prepare only: isolated native SkeletonGraphic comparison; never launch Unity."""
import hashlib,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
TARGET=ROOT/"Saved/UnityTrainSpineProbe"
def main():
    assets=TARGET/"Assets"; assets.mkdir(parents=True,exist_ok=True)
    copied=[]
    def copy(src,dst):
        dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
        copied.append({"source":src.relative_to(ROOT).as_posix(),"copy":dst.relative_to(ROOT).as_posix(),"sha256":hashlib.sha256(src.read_bytes()).hexdigest()})
    runtime=ROOT/"ref/client-unity/Assets/Third/Spine"
    for src in runtime.rglob("*"):
        if src.is_file():copy(src,assets/"Spine"/src.relative_to(runtime))
    for model in ("Arms1_1_SD","Arms1_2_SD","ArmsSpecial3_3_SD"):
        srcdir=ROOT/"ref/client-unity/Assets/BundleAssets/Troop/spine_troop"/model/"Materials"
        for src in srcdir.iterdir():
            if src.is_file():copy(src,assets/"Models"/model/src.name)
    for relative in ("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_troops/UIArmyTrain/UI_Win_TrainArmy.prefab","ref/client-unity/Assets/BundleAssets/Troop/shared_spriteunit/SkeletonGraphicDefault_1.mat"):
        src=ROOT/relative
        copy(src,assets/"Source"/src.name);copy(Path(str(src)+".meta"),assets/"Source"/(src.name+".meta"))
    copy(ROOT/"Scripts/RokTrainSpineExport/RenderNative.cs",assets/"RenderNative.cs")
    (TARGET/"Packages").mkdir(exist_ok=True)
    modules=["ui","uielements","animation","physics","physics2d","particlesystem","imageconversion","jsonserialize","imgui"]
    (TARGET/"Packages/manifest.json").write_text(json.dumps({"dependencies":{"com.unity.ugui":"1.0.0",**{"com.unity.modules."+m:"1.0.0" for m in modules}}}),encoding="utf-8")
    (TARGET/"ProjectSettings").mkdir(exist_ok=True)
    copy(ROOT/"Saved/UnityBuiltinShaderProbe/ProjectSettings/ProjectVersion.txt",TARGET/"ProjectSettings/ProjectVersion.txt")
    out=ROOT/"Saved/RokUnityMigration/TrainArmy/native_prepare.json"
    out.write_text(json.dumps({"prepared":True,"launched":False,"project":str(TARGET),"source_copies":copied,"scope":"Standalone source pl_spine at exact local transform inside source-sized pl_left; diagnostic center placement, not full training screen."},indent=2),encoding="utf-8")
    print("Prepared isolated Train Spine source project; Unity not launched")
if __name__=="__main__":main()
