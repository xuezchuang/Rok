"""Read-only evidence validation and descriptive pixel metrics, no invented golden threshold."""
from pathlib import Path
import hashlib
import json
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / "Saved/RokUnityMigration/PowerGain/ReplayVisual"


def main():
    report = json.loads((FOLDER / "native_replay_visual.json").read_text())
    failures = list(report["failures"])

    def check(condition, label):
        if not condition:
            failures.append(label)

    check(report["unityVersion"] == "2019.4.28f1c1", "source editor version")
    check(report["sourceRootDestroyed"], "last original AutoDestory root destroyed")
    check(len(report["captures"]) == 14, "all fourteen independent timing targets")
    check(len(report["frames"]) < 12000, "bounded actual tick log")
    for source in json.loads((FOLDER / "inputs.json").read_text())["inputs"]:
        check(hashlib.sha256(Path(source["path"]).read_bytes()).hexdigest() == source["sha256"], "source drift: " + source["path"])
    metrics = []
    for index, capture in enumerate(report["captures"]):
        row = {"target": capture["target"], "actual_elapsed": capture["state"]["elapsed"], "particle_count": capture["state"]["particleCount"], "images": []}
        check(len(capture["images"]) == 8, "eight layer/background images " + str(index))
        for colored in ("gray", "colored"):
            background = np.asarray(Image.open(FOLDER / f"{index:02}_{colored}_background.png").convert("RGB"), dtype=np.float32)
            for layer in ("mesh", "particles", "combined"):
                path = FOLDER / f"{index:02}_{colored}_{layer}.png"
                with Image.open(path) as image:
                    check(image.size == (640, 360), "native capture dimensions " + path.name)
                    pixels = np.asarray(image.convert("RGB"), dtype=np.float32)
                delta = pixels - background
                row["images"].append({"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                    "mean_delta": float(delta.mean()), "max_delta": float(delta.max()), "changed_pixels": int(np.any(delta != 0, axis=2).sum())})
        metrics.append(row)
        frames = [f for f in report["frames"] if f["run"] == index]
        events = [f for f in frames if f["operation"].startswith("after source replay")]
        check(len(events) == 2, "two actual replay operations in run " + str(index))
        if len(events) == 2:
            check(events[0]["rootLifetime"] == 2 and events[1]["rootLifetime"] == 3, "original absolute lifetime extension " + str(index))
        dead = next((f for f in frames if not f["rootAlive"]), None)
        check(dead is not None and dead["elapsed"] > 3, "strict source root destruction after deadline " + str(index))
    output = {"passed": not failures, "failures": failures, "captures": metrics,
              "scope": "Native real ticks and descriptive layer metrics only; no UE parity or subjective pixel-error threshold is asserted."}
    (FOLDER / "verify.json").write_text(json.dumps(output, indent=2), encoding="utf-8")
    print(json.dumps({"passed": output["passed"], "failures": failures, "captures": len(metrics)}))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
