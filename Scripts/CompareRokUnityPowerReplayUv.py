"""Compare real Animator/renderer MPB UV values to the authored source curve.
No UE runtime, pixel-parity, or image-error threshold claim.
"""
import hashlib
import json
import math
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / "Saved/RokUnityMigration/PowerGain/ReplayVisual"


def main():
    native_path = FOLDER / "native_replay_visual.json"
    source_path = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UI_10010.anim"
    source = source_path.read_text(encoding="utf-8-sig")
    block = source.split("  m_FloatCurves:\n", 1)[1].split("    attribute: material._MainTex_ST.x", 1)[0]
    rows = re.findall(r"time: ([^\n]+)\n\s+value: ([^\n]+)\n\s+inSlope: ([^\n]+)\n\s+outSlope: ([^\n]+)", block)
    keys = [tuple(map(float, row)) for row in rows]
    if len(keys) != 3 or any(value != "0" for value in re.findall(r"weightedMode: (\d+)", block)):
        raise ValueError("Expected the three source unweighted Hermite keys")
    duration = float(re.search(r"m_StopTime: ([^\n]+)", source).group(1))

    def evaluate(time):
        if time <= keys[0][0]:
            return keys[0][1]
        if time >= keys[-1][0]:
            return keys[-1][1]
        a, b = next((a, b) for a, b in zip(keys, keys[1:]) if a[0] <= time <= b[0])
        length = b[0] - a[0]
        u = (time - a[0]) / length
        return ((2*u**3 - 3*u*u + 1)*a[1] + (u**3 - 2*u*u + u)*length*a[3]
                + (-2*u**3 + 3*u*u)*b[1] + (u**3 - u*u)*length*b[2])

    native = json.loads(native_path.read_text())
    failures = list(native["failures"])
    states = [row for row in native["frames"] if row["rootAlive"]]
    differences = []
    for row in states:
        expected = evaluate(row["animatorNormalized"] * duration)
        actual = row["uTiling"]
        differences.append(abs(expected - actual))
        block = row["propertyBlockMainTexST"]
        if not row["hasMaterialPropertyBlock"] or block["y"] != 1 or actual != block["x"]:
            failures.append("Missing effective renderer ST in run " + str(row["run"]))
    if not states or any(not math.isfinite(value) or value > 1e-5 for value in differences):
        failures.append("Native effective ST differs from source Hermite curve")
    distinct = len({round(row["uTiling"], 5) for row in states})
    if distinct < 3:
        failures.append("Native effective ST did not animate")
    captures = []
    for capture in native["captures"]:
        row = capture["state"]
        captures.append({"run": row["run"], "target": capture["target"], "actual_elapsed": row["elapsed"],
                         "root_alive": row["rootAlive"], "material_default_u": row["materialUTiling"],
                         "effective_mpb_u": row["uTiling"], "animator_normalized": row["animatorNormalized"],
                         "source_curve_u": evaluate(row["animatorNormalized"] * duration) if row["rootAlive"] else None})
    report = {"passed": not failures, "failures": failures, "live_state_count": len(states),
              "distinct_effective_u_values_5dp": distinct, "maximum_absolute_curve_error": max(differences, default=None),
              "source_clip_duration": duration, "source_keys": keys, "captures": captures,
              "native_sha256": hashlib.sha256(native_path.read_bytes()).hexdigest(),
              "source_clip_sha256": hashlib.sha256(source_path.read_bytes()).hexdigest(),
              "conclusion": "Native Animator drives Renderer MaterialPropertyBlock. The material asset's constant tiling=1 is not the effective renderer UV.",
              "open_gate": "New UE continuous-replay layer images at matching particle ages and effective mesh times remain unverified."}
    (FOLDER / "native_uv_comparison.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({key: report[key] for key in ("passed", "failures", "live_state_count", "distinct_effective_u_values_5dp", "maximum_absolute_curve_error")}))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
