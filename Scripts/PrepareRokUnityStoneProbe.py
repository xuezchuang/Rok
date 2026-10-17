"""Prepare isolated native probe without changing live Unity sources or scenes."""
import json
import shutil
from pathlib import Path
from rok_data_paths import resolve
from BuildRokUnityStoneDeposit import ROOT, OUT


def main():
    target = ROOT / "Saved/UnityStoneDepositProbe"
    source_dir = target / "Assets/Source"
    source_dir.mkdir(parents=True, exist_ok=True)
    data = json.loads(OUT.read_text(encoding="utf-8"))
    paths = {data["source"]["path"]}
    paths.update(s[k]["path"] for i in data["items"] for s in i["sprites"] for k in ("texture", "mask", "material"))
    paths.update("ref/client-unity/Assets/Scripts/Client/"+p for p in (
        "TileCollideManager/GridCollideMgr.cs", "TileCollideManager/GridCollideItem.cs", "Utils/RotateSquar.cs",
        "LodManager/LevelDetailBase.cs", "LodManager/LevelDetailCastle.cs", "LodManager/LevelDetailDeposit.cs"))
    paths.add("ref/client-unity/Assets/BundleAssets/Map/Building/map_building_with_mask-5.shader")
    for relative in paths:
        path = resolve(relative)
        shutil.copy2(path, source_dir / path.name)
        shutil.copy2(str(path)+".meta", str(source_dir/path.name)+".meta")
    shutil.copy2(ROOT/"Scripts/ProbeRokUnityStoneDeposit.cs", target/"Assets/ProbeRokUnityStoneDeposit.cs")
    render_script = ROOT/"Scripts/RenderRokUnityStoneDeposit.cs"
    if render_script.exists():
        shutil.copy2(render_script, target/"Assets/RenderRokUnityStoneDeposit.cs")
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
