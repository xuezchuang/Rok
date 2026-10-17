#!/usr/bin/env python3
"""Root-safe V5 output and layout-settlement gate around the frozen strict verifier."""
from __future__ import annotations
import argparse
import copy
import hashlib
import importlib.util
import json
import math
import tempfile
from pathlib import Path

ORIGINAL_SHA256 = "817d6b44bf54743b9a58992c15d50a5b25e2ba77ee4b54a02ad6b9aaee6d017f"
ORIGINAL_REL = Path("Scripts/VerifyRokCityBuffReferenceRuntime.py")
ORIGINAL_STAGE_REL = Path("Saved/PendingCityBuffReferenceRuntimeAuditV1/VerifyRokCityBuffReferenceRuntime.py")
AUDIT5_REL = Path("Saved/PendingCityBuffReferenceRuntimeAuditV5")
AUDIT5_READY_SHA256 = "689ff6a657b7818615c93240ab523722d4be35cb943b9f556a992517b3496549"
AUDIT5_REPORT_SHA256 = "bef2861801103e977aacafc5aab95c8a41dcde027cda9ecb6ebf496cf70ce179"
AUDIT5_FILES = {
    "RokCityBuffReferenceAuditSubsystem.cpp": ("25715969b0b3607eba623edb9e81572de29583a6b2f87baa09ca8670fa10b972", 34251),
    "RokCityBuffReferenceAuditSubsystem.cpp.patch": ("bc8349c51f798a1c88275b9bb1220facfa9948e23580cf8292cfb935d1309872", 540),
    "RokCityBuffReferenceAuditSubsystem.h": ("8154bd83f2c02602fd8eb733cef34f36e577b37381784347d42c02f3c6b91b11", 2923),
    "RokUnityModalWidget.h.candidate": ("4c1059fefad4a983f1b666446c1a87e255aeaa37fec3a1c9e097df910ea6b7ef", 12538),
    "VerifyRokCityBuffReferenceRuntime.py": (ORIGINAL_SHA256, 13550),
}
SUPPLEMENT_STAGE_REL = Path("Saved/PendingCityBuffReferenceVerifierSupplementV7")
RUNS_REL = Path("Saved/RokUnityMigration/CityBuffReferenceRuntimeAuditV5")

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def discover_root(script_path: Path) -> Path:
    matches = [p.resolve() for p in script_path.resolve().parents if (p / "Rok.uproject").is_file()]
    unique = list(dict.fromkeys(matches))
    if len(unique) != 1:
        raise ValueError(f"expected exactly one Rok.uproject ancestor, found {len(unique)}")
    return unique[0]

def verify_audit5(root: Path) -> Path:
    stage = root / AUDIT5_REL
    ready = stage / "ready.json"
    report = stage / "report.json"
    if not ready.is_file() or sha(ready) != AUDIT5_READY_SHA256:
        raise ValueError("AuditV5 ready pin mismatch")
    if not report.is_file() or sha(report) != AUDIT5_REPORT_SHA256:
        raise ValueError("AuditV5 report pin mismatch")
    for name, (expected_sha, expected_bytes) in AUDIT5_FILES.items():
        file = stage / name
        if not file.is_file() or file.stat().st_size != expected_bytes or sha(file) != expected_sha:
            raise ValueError(f"AuditV5 file pin mismatch: {name}")
    return stage

def verify_published_cpp(root: Path, stage: Path) -> None:
    published = root / "Source/Rok/RokCityBuffReferenceAuditSubsystem.cpp"
    candidate = stage / "RokCityBuffReferenceAuditSubsystem.cpp"
    if not published.is_file() or sha(published) != sha(candidate):
        raise ValueError("published AuditV5 CPP does not exactly match the pinned AuditV5 candidate")

def verify_settlement_manifest(data: dict, route: str) -> None:
    def frame_number(value) -> bool:
        return (type(value) in (int, float) and math.isfinite(value)
                and float(value).is_integer() and value >= 0)
    labels = (["outside_close"] if route == "production" else
              ["more_to_city_manager", "close_city_manager_to_hud"])
    phases = (["00_buff_popup", "01_hud_return"] if route == "production" else
              ["00_buff_popup", "01_city_manager", "02_hud_return"])
    actions, captures = data["actions"], data["captures"]
    if len(actions) != len(labels) or [c["phase"] for c in captures] != phases:
        raise ValueError("V7 route action/capture shape")
    baseline = data["baseline_business"]
    last_frame = -1
    for index, (action, label) in enumerate(zip(actions, labels)):
        before, immediate, settled = action["before"], action["immediate_after"], action["after"]
        immediate_frame, after_frame = action["immediate_frame"], action["after_frame"]
        capture = captures[index + 1]
        if (action["label"] != label or action["after_layout_settled"] is not True
                or not frame_number(immediate_frame) or not frame_number(after_frame)
                or not frame_number(capture["requested_frame"])
                or immediate_frame <= last_frame or after_frame <= immediate_frame
                or capture["requested_frame"] < after_frame
                or immediate["frame"] != immediate_frame or settled["frame"] != after_frame
                or capture["requested_state"]["frame"] != capture["requested_frame"]
                or before["business"] != baseline or immediate["business"] != baseline
                or settled["business"] != baseline
                or immediate["business_boundary"] is not True
                or settled["business_boundary"] is not True):
            raise ValueError(f"V7 native settled action receipt: {route}/{label}")
        last_frame = after_frame

def verify_settlement_outputs(root: Path) -> None:
    runs = root / RUNS_REL
    for route in ("production", "fixture"):
        manifest = runs / route / "manifest.json"
        if not manifest.is_file():
            raise FileNotFoundError(f"V7 native manifest missing: {manifest}")
        verify_settlement_manifest(json.loads(manifest.read_text(encoding="utf-8")), route)

def bind_and_run(script_path: Path, argv: list[str], expected_sha: str = ORIGINAL_SHA256) -> None:
    root = discover_root(script_path)
    stage = verify_audit5(root)
    live = root / ORIGINAL_REL
    frozen = root / ORIGINAL_STAGE_REL
    if not live.is_file() or not frozen.is_file():
        raise FileNotFoundError("frozen original verifier missing from live or V1 audit stage")
    live_sha, frozen_sha = sha(live), sha(frozen)
    if live_sha != expected_sha or frozen_sha != expected_sha or live_sha != frozen_sha:
        raise ValueError("frozen original verifier SHA mismatch")
    verify_published_cpp(root, stage)
    if "--preflight" not in argv:
        verify_settlement_outputs(root)
    spec = importlib.util.spec_from_file_location("frozen_city_buff_runtime_verifier_v5", frozen)
    if spec is None or spec.loader is None:
        raise ImportError("cannot load frozen verifier")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.ROOT = root
    module.STAGE = stage
    module.RUNS = root / RUNS_REL
    old_argv = __import__("sys").argv
    try:
        __import__("sys").argv = [str(frozen), *argv]
        module.main()
    finally:
        __import__("sys").argv = old_argv

def self_test() -> None:
    here = Path(__file__).resolve()
    root = discover_root(here)
    if discover_root(root / "Scripts" / "VerifyRokCityBuffReferenceRuntimeSupplementV7.py") != root:
        raise AssertionError("simulated Scripts-layout root mismatch")
    verify_audit5(root)
    if sha(root / ORIGINAL_REL) != ORIGINAL_SHA256 or sha(root / ORIGINAL_STAGE_REL) != ORIGINAL_SHA256:
        raise AssertionError("frozen original verifier SHA mismatch")
    try:
        bind_and_run(here, ["--preflight"], expected_sha="0" * 64)
    except ValueError as exc:
        if "frozen original verifier SHA mismatch" not in str(exc):
            raise
    else:
        raise AssertionError("wrong-original-SHA case was accepted")
    # This current checkout is not yet published as AuditV5. The exact published-vs-AuditV5 check
    # must reject it until the reviewed candidate is integrated.
    try:
        verify_published_cpp(root, root / AUDIT5_REL)
    except ValueError as exc:
        if "does not exactly match" not in str(exc):
            raise
    else:
        raise AssertionError("unpublished AuditV5 CPP unexpectedly matched live source")
    with tempfile.TemporaryDirectory(prefix="citybuff_root_v5_") as temp:
        base = Path(temp)
        fixture_stage = base / AUDIT5_REL
        fixture_source = base / "Source/Rok/RokCityBuffReferenceAuditSubsystem.cpp"
        fixture_source.parent.mkdir(parents=True)
        fixture_stage.mkdir(parents=True)
        candidate_bytes = (root / AUDIT5_REL / "RokCityBuffReferenceAuditSubsystem.cpp").read_bytes()
        fixture_source.write_bytes(candidate_bytes)
        (fixture_stage / "RokCityBuffReferenceAuditSubsystem.cpp").write_bytes(candidate_bytes)
        verify_published_cpp(base, fixture_stage)
        lone = base / "single" / "Saved" / "Stage" / "verifier.py"
        lone.parent.mkdir(parents=True)
        (base / "single" / "Rok.uproject").write_text("{}", encoding="utf-8")
        if discover_root(lone) != (base / "single").resolve():
            raise AssertionError("simulated Saved-layout root mismatch")
        nested = base / "single" / "nested"
        nested.mkdir()
        (nested / "Rok.uproject").write_text("{}", encoding="utf-8")
        try:
            discover_root(nested / "Saved" / "Stage" / "verifier.py")
        except ValueError:
            pass
        else:
            raise AssertionError("ambiguous-root case was accepted")
        try:
            discover_root(base / "noroot" / "x.py")
        except ValueError:
            pass
        else:
            raise AssertionError("no-root case was accepted")
    baseline = {"server_local_only": True, "http_dispatch_attempts": 0, "http_suppressed_requests": 0}
    def state(frame: int) -> dict:
        return {"frame": frame, "business": copy.deepcopy(baseline), "business_boundary": True}
    positive = {
        "baseline_business": baseline,
        "actions": [{"label": "outside_close", "before": state(10),
                     "immediate_after": state(10), "immediate_frame": 10,
                     "after": state(12), "after_frame": 12,
                     "after_layout_settled": True}],
        "captures": [{"phase": "00_buff_popup"},
                     {"phase": "01_hud_return", "requested_frame": 12,
                      "requested_state": state(12)}],
    }
    verify_settlement_manifest(positive, "production")
    for label, mutate in (
        ("unsettled", lambda d: d["actions"][0].__setitem__("after_layout_settled", False)),
        ("same_frame", lambda d: d["actions"][0].__setitem__("after_frame", 10)),
        ("fabricated_after_frame", lambda d: d["actions"][0]["after"].__setitem__("frame", 11)),
        ("fabricated_immediate_frame", lambda d: d["actions"][0]["immediate_after"].__setitem__("frame", 11)),
        ("early_capture", lambda d: d["captures"][1].__setitem__("requested_frame", 11)),
        ("changed_business", lambda d: d["actions"][0]["after"]["business"].__setitem__("http_dispatch_attempts", 1)),
        ("wrong_route", lambda d: d["actions"][0].__setitem__("label", "more_to_city_manager")),
    ):
        bad = copy.deepcopy(positive)
        mutate(bad)
        try:
            verify_settlement_manifest(bad, "production")
        except ValueError:
            pass
        else:
            raise AssertionError(f"V7 negative accepted: {label}")
    print("PASS: V5 pins/root/published-CPP gates; V7 settled-frame receipt and seven negative cases")

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    args, rest = parser.parse_known_args()
    if args.self_test:
        self_test()
        if not rest:
            return
    bind_and_run(Path(__file__), rest)

if __name__ == "__main__":
    main()
