"""Validate Unity-exported UI_10010 geometry before the UE runtime consumes it.

This validates the source export, not UE rendering or full migration acceptance.
"""
import hashlib
import json
import math
from pathlib import Path
from rok_data_paths import DATA_ROOT, resolve

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "Saved/RokUnityMigration/PowerGain"


def verify(data):
    failures = []

    def check(condition, message):
        if not condition:
            failures.append(message)

    check(data["unityVersion"] == "2019.4.28f1c1", "Wrong Unity version")
    check(data["runtimeAccepted"] is False, "Source export must not claim runtime acceptance")
    check(data["lifetime"] == 1, "Source AutoDestory lifetime changed")
    check(len(data["sources"]) == 8, "Incomplete source dependency chain")
    for source in data["sources"]:
        path = resolve(source["path"])
        for suffix, field in (("", "sha256"), (".meta", "metadataSha256")):
            actual = hashlib.sha256(Path(str(path) + suffix).read_bytes()).hexdigest()
            check(actual == source[field], f"Source changed: {source['path']}{suffix}")

    def draw_valid(draw, context):
        count = len(draw["positions"])
        check(count == len(draw["uv"]) == len(draw["colors"]), f"{context}: incomplete vertex streams")
        check(len(draw["indices"]) % 3 == 0, f"{context}: incomplete triangles")
        check(all(0 <= i < count for i in draw["indices"]), f"{context}: invalid index")
        check(all(math.isfinite(v) for key in ("positions", "uv", "colors")
                  for vertex in draw[key] for v in vertex.values()), f"{context}: nonfinite vertex")

    mesh = data["mesh"]
    draw_valid(mesh, "mesh")
    check(len(mesh["positions"]) == 6 and len(mesh["indices"]) == 12, "Authored mesh topology lost")
    check([round(c["a"]) for c in mesh["colors"]] == [1, 1, 1, 1, 0, 0], "Mesh trailing vertex fade lost")
    samples = data["samples"]
    check(len(samples) == 121 and data["sampleRate"] == 120, "Incomplete one-second source sampling")

    # Independent analytic oracle for the unweighted Hermite curve in UI_10010.anim.
    # Every Unity-evaluated sample must preserve both endpoint values and tangents.
    keys = [(0, 1, -8.567013), (0.16666667, 0.26447147, -0.6046871), (0.85, -0.09, 0)]

    def expected_tiling(time):
        if time >= keys[-1][0]:
            return keys[-1][1]
        a, b = (keys[0], keys[1]) if time < keys[1][0] else (keys[1], keys[2])
        duration = b[0] - a[0]
        t = (time - a[0]) / duration
        return ((2*t**3 - 3*t*t + 1)*a[1] + (t**3 - 2*t*t + t)*duration*a[2]
                + (-2*t**3 + 3*t*t)*b[1] + (t**3 - t*t)*duration*b[2])

    for index, sample in enumerate(samples):
        context = f"sample {index}"
        check(abs(sample["time"] - index / 120) < 1e-6, f"{context}: bad time")
        check(abs(sample["uTiling"] - expected_tiling(sample["time"])) < 1e-5, f"{context}: UV curve mismatch")
        check(sample["particleCount"] in (0, 1), f"{context}: unexpected particle burst count")
        draw_valid(sample["particle"], context)
        check(len(sample["particle"]["positions"]) == sample["particleCount"] * 4,
              f"{context}: source billboard missing")
    check(any(s["particleCount"] == 1 for s in samples), "Particle effect never emitted")
    check(data["roundTripMaxChannelError"] <= 2 and data["roundTripMeanChannelError"] <= 0.1,
          "Exported geometry fails source pixel comparison")
    for index in (0, 9, 20, 51, 102, 119):
        path = OUTPUT / f"unity_{index:03}.png"
        check(path.is_file() and path.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"), f"Missing reference frame {index}")
    return failures


if __name__ == "__main__":
    data = json.loads((DATA_ROOT / "PowerGain/source_geometry.json").read_text(encoding="utf-8"))
    failures = verify(data)
    report = {"scope": "Unity source export only; UE runtime and HUD visual acceptance remain open",
              "failures": failures, "samples": len(data["samples"]),
              "round_trip_max_channel_error": data["roundTripMaxChannelError"],
              "round_trip_mean_channel_error": data["roundTripMeanChannelError"]}
    (OUTPUT / "source_geometry_verify.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    raise SystemExit(bool(failures))
