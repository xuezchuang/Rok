"""Prepare isolated Gold GridCollide native probe; never touch the live Unity project."""
import json
import shutil
from pathlib import Path
from rok_data_paths import resolve
from BuildRokWorldStructurePresentation import ROOT, OUT


def main():
    target = ROOT / "Saved/UnityGoldDepositProbe"
    source_dir = target / "Assets/Source"
    source_dir.mkdir(parents=True, exist_ok=True)
    data = json.loads(OUT.read_text(encoding="utf-8"))
    model = data["models"]["build_Rssgold_Big"]
    paths = {model["source"]["path"]}
    paths.update(i[k]["path"] for i in model["items"] for k in ("texture", "mask", "material"))
    checkpoint = data["models"]["build_Checkpoint_Big"]
    paths.add(checkpoint["source"]["path"])
    paths.update(i[k]["path"] for i in checkpoint["items"] for k in ("texture", "mask", "material") if i[k])
    paths.update(i["source"]["path"] for i in checkpoint["unsupported"] if i.get("source"))
    for model_id in ("build_BarbarianCitadel_Big", "build_HolyLand1_Big", "build_HolyLand2_Big"):
        structure = data["models"][model_id]
        paths.add(structure["source"]["path"])
        paths.update(i[k]["path"] for i in structure["items"] for k in ("texture", "mask", "material") if i[k])
    paths.update("ref/client-unity/Assets/Scripts/Client/"+p for p in (
        "TileCollideManager/GridCollideMgr.cs", "TileCollideManager/GridCollideItem.cs", "Utils/RotateSquar.cs",
        "LodManager/LevelDetailBase.cs", "LodManager/LevelDetailCastle.cs", "LodManager/LevelDetailDeposit.cs"))
    paths.add("ref/client-unity/Assets/BundleAssets/Map/Building/map_building_with_mask-5.shader")
    for relative in paths:
        path = resolve(relative)
        shutil.copy2(path, source_dir / path.name)
        shutil.copy2(str(path)+".meta", str(source_dir/path.name)+".meta")
    shutil.copy2(ROOT/"Scripts/ProbeRokUnityGoldDeposit.cs", target/"Assets/ProbeRokUnityGoldDeposit.cs")
    shutil.copy2(ROOT/"Scripts/ProbeRokUnityCheckpointMaterial.cs", target/"Assets/ProbeRokUnityCheckpointMaterial.cs")
    shutil.copy2(ROOT/"Scripts/ProbeRokUnityStructureActivity.cs", target/"Assets/ProbeRokUnityStructureActivity.cs")
    shutil.copy2(ROOT/"Scripts/RenderRokUnityGoldDeposit.cs", target/"Assets/RenderRokUnityGoldDeposit.cs")
    (target/"Assets/ProbeDependencies.cs").write_text('''// Isolated dependencies only; source GridCollide and LOD scripts are unchanged.
public static class Common { public static float Distance=480, Previous=480; public static float GetLodDistance(){return Distance;} }
public static class ClientUtils { public static ProbeLodManager lodManager=new ProbeLodManager(); }
public class ProbeLodManager { public float GetLodDistance(){return Common.Distance;} public float GetPreviousLodDistance(){return Common.Previous;} public void AddHandler(System.Action action){} public void RemoveHandler(System.Action action){} }
namespace Skyunion { public static class CoreUtils { public static object assetService=new object(); } }
''', encoding="utf-8")
    (target/"Packages").mkdir(exist_ok=True)
    (target/"Packages/manifest.json").write_text(json.dumps({"dependencies": {"com.unity.modules.jsonserialize":"1.0.0", "com.unity.modules.imgui":"1.0.0", "com.unity.modules.imageconversion":"1.0.0"}}), encoding="utf-8")
    (target/"ProjectSettings").mkdir(exist_ok=True)
    shutil.copy2(ROOT/"Saved/UnityBuiltinShaderProbe/ProjectSettings/ProjectVersion.txt", target/"ProjectSettings/ProjectVersion.txt")
    print(target)


if __name__ == "__main__":
    main()
