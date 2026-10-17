"""Compare exported Spine frames against native SkeletonGraphic; no raster edits."""
import hashlib
import json
import math
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Saved/RokUnityMigration/TrainArmy"

def source(path):
    return {"path":path.relative_to(ROOT).as_posix(), "sha256":hashlib.sha256(path.read_bytes()).hexdigest()}

def sample(image, u, v):
    # PNG/PIL origin is top-left; texture bilinear coordinates address centers.
    x=u*image.width-.5; y=v*image.height-.5
    ix=math.floor(x); iy=math.floor(y); ax=x-ix; ay=y-iy
    result=[0.0]*4
    for dx,wx in ((0,1-ax),(1,ax)):
        for dy,wy in ((0,1-ay),(1,ay)):
            p=image.getpixel((min(max(ix+dx,0),image.width-1),min(max(iy+dy,0),image.height-1)))
            for k in range(4): result[k]+=p[k]/255*wx*wy
    return result

def main():
    native_path=OUT/"NativeReference/native_render.json"
    native=json.loads(native_path.read_text(encoding="utf-8"))
    assert native["unityVersion"] == "2019.4.28f1c1"
    assert native["colorSpace"] == "Gamma" and native["graphicsDevice"] != "Null Device"
    assert {(c["armsId"],c["time"]) for c in native["captures"]} == {(10101,0),(10101,.5),(10201,.5),(20131,.5)}
    assert len(native["captures"]) == 4
    textures={}; reports=[]; evidence=[source(native_path)]
    for capture in native["captures"]:
        path=OUT/f"arms_{capture['armsId']}_idle_mesh.json"
        clip=json.loads(path.read_text(encoding="utf-8")); evidence.append(source(path))
        assert clip["arms_show"] == capture["model"]
        assert capture["canvasPPU"] == 100
        frame=min(clip["frames"],key=lambda f:abs(f["time"]-capture["time"]))
        assert abs(frame["time"]-capture["time"]) < 1e-6
        positions=[];uv=[];triangles=[];colors=[];ranges=[]
        for draw in frame["draws"]:
            offset=len(positions)
            positions.extend(zip(draw["xy"][::2],draw["xy"][1::2]))
            uv.extend(zip(draw["uv"][::2],draw["uv"][1::2]))
            triangles.extend(i+offset for i in draw["triangles"])
            r,g,b,a=draw["rgba"]
            # Source SkeletonGraphic pmaVertexColors: vertex tint premultiplied once.
            colors.extend([(r*a,g*a,b*a,a)]*(len(draw["xy"])//2))
            ranges.append((draw,offset))
        assert len(positions)==len(capture["vertices"])==len(uv)==len(capture["uv"])==len(colors)==len(capture["colors"])
        assert triangles == capture["triangles"], "Native global triangle order differs"
        xy_error=max(abs(p[k]*100-v["xy"[k]]) for p,v in zip(positions,capture["vertices"]) for k in range(2))
        assert xy_error < .001, xy_error
        assert all(abs(v["z"])<1e-8 for v in capture["vertices"])
        uv_error=max(max(abs(p[0]-v["x"]),abs(1-p[1]-v["y"])) for p,v in zip(uv,capture["uv"]))
        uv_without_unity_flip_error=max(abs(p[1]-v["y"]) for p,v in zip(uv,capture["uv"]))
        assert uv_error < 2e-6
        assert uv_without_unity_flip_error > .1, "Sample must distinguish V directions"
        color_error=max(abs(p[k]*255-v["rgba"[k]]) for p,v in zip(colors,capture["colors"]) for k in range(4))
        assert color_error <= 1
        all_white=all(tuple(v[k] for k in "rgba")== (255,255,255,255) for v in capture["colors"])
        correct_errors=[]; extra_flip_errors=[]
        for draw,offset in ranges:
            texture_path=ROOT/draw["texture"]
            if texture_path not in textures:
                textures[texture_path]=Image.open(texture_path).convert("RGBA");evidence.append(source(texture_path))
            texture=textures[texture_path]
            for t in range(0,len(draw["triangles"]),3):
                indices=[offset+i for i in draw["triangles"][t:t+3]]
                eu=sum(uv[i][0] for i in indices)/3; ev=sum(uv[i][1] for i in indices)/3
                nu=sum(capture["uv"][i]["x"] for i in indices)/3; nv=sum(capture["uv"][i]["y"] for i in indices)/3
                exported=sample(texture,eu,ev)
                native_png=sample(texture,nu,1-nv)
                extra_flip=sample(texture,eu,1-ev)
                correct_errors.append(max(abs(a-b) for a,b in zip(exported,native_png)))
                extra_flip_errors.append(max(abs(a-b) for a,b in zip(extra_flip,native_png)))
        assert max(correct_errors)<.0001
        assert sum(extra_flip_errors)/len(extra_flip_errors)>.01
        image_path=Path(capture["path"])
        with Image.open(image_path) as image:
            assert image.size == (1440,810)
        evidence.append(source(image_path))
        reports.append({"arms_id":capture["armsId"],"time":capture["time"],"vertices":len(positions),"triangles":len(triangles)//3,
            "source_pixel_xy_max_error":xy_error,"uv_max_error_after_Unity_FlipV":uv_error,
            "uv_max_error_without_Unity_FlipV":uv_without_unity_flip_error,
            "pma_color_byte_max_error":color_error,"native_tints_all_white":all_white,
            "png_bilinear_max_error":max(correct_errors),"incorrect_extra_vflip_mean_error":sum(extra_flip_errors)/len(extra_flip_errors)})
    evidence.extend(source(ROOT/p) for p in (
        "ref/client-unity/Assets/Third/Spine/spine-unity/Asset Types/AtlasAsset.cs",
        "ref/client-unity/Assets/Third/Spine/spine-csharp/Atlas.cs",
        "ref/client-unity/Assets/Third/Spine/spine-unity/Modules/SkeletonGraphic/SkeletonGraphic.cs",
        "ref/client-unity/Assets/Skyunion/RunTime/UIManager/Resources/UIRoot.prefab"))
    result={"passed":True,"captures":reports,"sources":evidence,
        "uv_contract":"Export Atlas has PNG top-left V. Unity AtlasAsset.GetAtlas calls FlipV, hence native U=export U and native V=1-export V. Pass current exported UV directly to Slate; applying an additional V flip is wrong.",
        "pma_limit":"All four native poses have white opaque vertex tint; color match does not independently test fractional tint alpha. PMA-once is the source contract, not a closed runtime fractional-alpha test.",
        "scope":native["scope"],"remaining":"UE final PMA/texture sampling and full TrainArmy layout screenshot comparison remain separate gates."}
    (OUT/"native_verify.json").write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"passed":True,"captures":reports},indent=2))
if __name__ == "__main__": main()
