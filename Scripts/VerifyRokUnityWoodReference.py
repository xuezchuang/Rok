"""Verify provenance and native render outputs; UE visual comparison stays explicit."""
import hashlib
import json
from pathlib import Path
from PIL import Image
from BuildRokUnityWoodDeposit import ROOT, OUT, source


def main():
    directory = ROOT / "Saved/RokUnityMigration/WoodReference"
    report = json.loads((directory/"native_render.json").read_text(encoding="utf-8"))
    data = json.loads(OUT.read_text(encoding="utf-8"))
    native = json.loads(OUT.with_name("unity_wood_deposit_native_probe.json").read_text(encoding="utf-8"))
    assert report["unityVersion"] == "2019.4.28f1c1"
    assert report["colorSpace"] == "Gamma" and report["graphicsDevice"] != "Null Device"
    assert len(report["frames"]) == 3
    assets = [data["source"]] + [s[k] for item in data["items"] for s in item["sprites"] for k in ("texture", "mask", "material")]
    provenance = []
    for asset in assets:
        path = ROOT/asset["path"]
        copied = ROOT/"Saved/UnityWoodDepositProbe/Assets/Source"/path.name
        assert hashlib.sha256(copied.read_bytes()).hexdigest() == asset["sha256"]
        assert Path(str(path)+".meta").read_bytes() == Path(str(copied)+".meta").read_bytes()
        provenance += [source(path), source(Path(str(path)+".meta"))]
    frames = []
    for frame in report["frames"]:
        expected = next(s for s in native["runs"][0]["samples"] if abs(s["dxf"]-frame["dxf"]) < .001)
        assert set(frame["active"]) == set(expected["active"])
        assert abs(frame["distance"]*frame["verticalFov"]-frame["dxf"]) < .001
        path = Path(frame["path"])
        with Image.open(path) as image:
            assert image.size == (1280, 720)
            colors = image.convert("RGB").getcolors(1280*720)
            assert colors and len(colors) > 1000, "Reference rendered blank or failed shader"
        frames.append({**source(path), "dxf":frame["dxf"], "active":frame["active"], "distinct_colors":len(colors)})
    provenance.extend(source(ROOT/path) for path in (
        "Scripts/RenderRokUnityWoodDeposit.cs",
        "ref/client-unity/Assets/Scripts/Client/Compoment/WorldCamera.cs",
        "ref/client-unity/Assets/Scripts/Client/Compoment/MaskSprite.cs",
        "ref/client-unity/Assets/Scripts/Client/LightingManager/LightManager.cs",
        "ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/GameEventGlobalMediator.cs",
        "ref/config-tables/编译客户端配置/CSV/CameraParamLod.csv",
        "ref/client-unity/Assets/BundleAssets/Map/Building/map_building_with_mask-5.shader",
    ))
    result = {"passed":True, "frames":frames, "provenance":provenance,
              "scope":report["scope"], "ue_visual_comparison":"Separate visual gate; this verifier does not certify UE orientation, blending or terrain composition."}
    (directory/"native_render_verify.json").write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("Native wood reference verified: 3 graphics renders, original asset/meta copies and camera/active states match.")


if __name__ == "__main__":
    main()
