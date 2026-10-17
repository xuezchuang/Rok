"""Read-only pulse runtime review against its immutable native mail geometry.

This script never rewrites historical pulse reports or native source evidence.
"""
import argparse
import hashlib
import json
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / "Saved/RokUnityMigration/MailAttachmentPulseAudit"
# Each approved native probe is bound to its source SHA, not the advancing
# MailRewardNative/verified_capture.json latest pointer. Keep historical rows.
APPROVED = {
    "mail_native_final_v1": (
        "Saved/UnityMailRewardNativeProbe/Artifacts/native_20261008_110445_250/render_evidence.json",
        "d60469e93e02c056a5e702a6886bbb0825f7c2da9240449909c30bef3a8697e8",
    ),
    "mail_native_final_v2": (
        "Saved/UnityMailRewardNativeProbe/Artifacts/native_20261008_114400_572/render_evidence.json",
        "34918ef7746a74dbfd08bb4512222d653b10af7dcc4b3e9ec9edd096973f4e8a",
    ),
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def close(value, expected, tolerance=1.0):
    return abs(value - expected) <= tolerance


def latest_manifest():
    candidates = sorted(RUNS.glob("*/manifest.json"), key=lambda path: path.stat().st_mtime, reverse=True)
    for path in candidates:
        try:
            if json.loads(path.read_text(encoding="utf-8")).get("geometry_contract") in APPROVED:
                return path
        except (ValueError, OSError):
            continue
    raise AssertionError("No supported pulse runtime geometry contract; run the engine audit first")


def rect(source):
    return tuple(float(source[key]) for key in ("x", "y", "width", "height"))


def verify(path):
    runtime = json.loads(path.read_text(encoding="utf-8"))
    contract = runtime["geometry_contract"]
    assert contract in APPROVED, "unsupported runtime geometry contract"
    source, approved_sha = APPROVED[contract]
    assert runtime["geometry_source"] == source + "#samples[0].leftMailRowGeometry", "runtime source identity"
    recorded_sha = runtime.get("geometry_source_sha256")
    assert (recorded_sha == approved_sha if contract == "mail_native_final_v2"
            else recorded_sha in (None, approved_sha)), "runtime approved source SHA"
    native_path = resolve(source)
    assert sha(native_path) == approved_sha, "native evidence changed after strict review"
    native = json.loads(native_path.read_text(encoding="utf-8"))
    assert native["complete"] and native["failure"] in (None, ""), "native probe incomplete"
    sample = next(s for s in native["samples"] if s["fixtureKey"] == "reference_single")
    assert sample["windowAnimatorState"] == "Open", "native window not in open state"
    row_node = next(n for n in sample["leftMailRowGeometry"] if n["path"].endswith("/UI_Item_MailBtn"))
    bag_node = next(n for n in sample["leftMailRowGeometry"] if n["path"].endswith("/UI_Item_MailBtn/img_bag"))
    row = rect(row_node["screenRect"])
    bag = rect(bag_node["screenRect"])
    native_scale = float(bag_node["localScale"]["x"])
    assert close(bag[2] / native_scale, 28, .01) and close(bag[3] / native_scale, 25, .01), "native dynamic bag baseline"
    assert bag_node["pivot"] == {"x": 1.0, "y": 1.0}, "native bag pivot"
    assert runtime["passed"] and runtime["complete"] and not runtime["failures"], "pulse runtime failed"
    assert runtime["spine_loops"] >= 1 and runtime["observed_peaks"] >= 3, "source loop/peak gate"
    for key in ("selection_same_instance_continued", "tab_paused", "tab_same_instance_resumed", "close_reopen_fresh_zero"):
        assert runtime[key], key
    captures = runtime["captures"]
    assert len(captures) == 8, "eight processed screenshots required"
    assert [capture["step"] for capture in captures] == [1, 3, 5, 7, 9, 11, 13, 15], "screenshot lifecycle steps"
    for capture in captures:
        image = Path(capture["file"])
        assert image.is_file() and image.read_bytes()[:8] == bytes((137, 80, 78, 71, 13, 10, 26, 10)), image
        if capture["step"] not in (3, 5, 9, 15):
            continue
        assert capture["source_row_geometry_pass"] and capture["source_bag_geometry_pass"] and capture["source_viewport_geometry_pass"]
        assert capture["processed_mesh_present"] and capture["processed_paint_count"] > 0, "actual mesh paint"
        viewport = rect(capture["viewport_cached_rect"])
        assert close(viewport[2], native["width"]) and close(viewport[3], native["height"]), "viewport size"
        actual_row = rect(capture["row_cached_rect"])
        assert all(close(a, b) for a, b in zip(actual_row, row)), "row cached rect vs native"
        pulse = rect(capture["pulse_cached_rect"])
        scale = capture["pulse_scale"]
        assert 1.0 <= scale <= 1.15001, "animation scale range"
        assert close(pulse[0] + pulse[2], bag[0] + bag[2]), "source bag right anchor"
        assert close(pulse[1], bag[1]), "source bag top anchor"
        assert close(pulse[2], 28 * scale) and close(pulse[3], 25 * scale), "source baseline and actual animation scale"
    print(f"PASS pulse runtime: {path} / 8 screenshots / final native row and phase-adjusted bag geometry / lifecycle")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, help="new pulse audit manifest; defaults to latest v2 contract")
    args = parser.parse_args()
    verify(args.manifest or latest_manifest())
