"""Validate source identity, geometry and complete train export coverage."""
import csv, hashlib, json, math
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Saved/RokUnityMigration/TrainArmy"
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    report=json.loads((OUT/"export_report.json").read_text(encoding="utf-8"))
    rows={int(r["ID"]):r for r in csv.DictReader((ROOT/"ref/config-tables/编译客户端配置/CSV/Arms.csv").open(encoding="utf-8-sig")) if r["ID"].isdigit()}
    assert len(report["results"]) == len(rows) == 54
    assert len({r["arms_id"] for r in report["results"]}) == 54
    checked=[]
    for result in report["results"]:
        row=rows[result["arms_id"]]
        if not row["armsShow"]:
            assert result["status"] == "unsupported_empty_armsShow"
            assert row["armsType"] == "7"
            continue
        assert result["status"] == "exported"
        path=ROOT/result["output"]
        assert sha(path) == result["sha256"]
        clip=json.loads(path.read_text(encoding="utf-8"))
        assert clip["schema"] == "rok.train_army.spine_mesh.v1"
        assert clip["arms_id"] == result["arms_id"] and clip["arms_show"] == row["armsShow"]
        assert clip["animation"] == "idle" and clip["loop"]
        assert clip["frame_count"] == len(clip["frames"]) == 200 and clip["unique_poses"] == 200
        assert clip["duration"] > 0 and clip["slot_blends"] == ["Normal"]
        assert set(clip["attachment_types"]) <= {"MeshAttachment","RegionAttachment"}
        for src in clip["sources"]: assert sha(ROOT/src["path"]) == src["sha256"]
        poses=set()
        for index,frame in enumerate(clip["frames"]):
            assert abs(frame["time"] - index*clip["duration"]/clip["frame_count"]) < .000001
            assert frame["draws"]
            pose=[]
            for draw in frame["draws"]:
                assert draw["blend"] == "Normal" and (ROOT/draw["texture"]).is_file()
                assert len(draw["xy"]) == len(draw["uv"]) and len(draw["xy"]) % 2 == 0
                assert draw["triangles"] and len(draw["triangles"]) % 3 == 0
                assert all(0 <= i < len(draw["xy"])/2 for i in draw["triangles"])
                assert all(math.isfinite(v) for key in ("xy","uv","rgba") for v in draw[key])
                pose.extend(draw["xy"])
            poses.add(tuple(pose))
        assert len(poses) == 200
        checked.append({"arms_id":clip["arms_id"],"duration":clip["duration"],"frames":len(clip["frames"])})
    assert len(checked)==29 and report["unsupported"]==25 and report["failures"]==0
    result={"passed":True,"source_models":29,"explicitly_unsupported":25,"checked":checked,"remaining":"UE PMA composition, source Canvas projection, runtime switching and native visual comparison"}
    (OUT/"export_verify.json").write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print("PASS: 29 animated models, 5800 frames, source hashes and 54 Arms rows")
if __name__ == "__main__": main()
