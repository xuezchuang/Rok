#!/usr/bin/env python3
"""Root-safe wrapper for the frozen strict verifier and City Buff AuditV3."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import tempfile
from pathlib import Path

ORIGINAL_SHA256 = "817d6b44bf54743b9a58992c15d50a5b25e2ba77ee4b54a02ad6b9aaee6d017f"
ORIGINAL_REL = Path("Scripts/VerifyRokCityBuffReferenceRuntime.py")
ORIGINAL_STAGE_REL = Path("Saved/PendingCityBuffReferenceRuntimeAuditV1/VerifyRokCityBuffReferenceRuntime.py")
AUDIT3_REL = Path("Saved/PendingCityBuffReferenceRuntimeAuditV3")
AUDIT3_READY_SHA256 = "bc7cf0cf08990e0802276ae25d7a8bea9f3354600659642c35231de2e66f97eb"
AUDIT3_REPORT_SHA256 = "eb0c9b4e9ce20e51839eaf52669a3faa964348c2fb23f022175ed1ab44e90aba"
AUDIT3_FILES = {
    "RokCityBuffReferenceAuditSubsystem.cpp": ("a39bbba228799c78fc0f798c7ba79e14d4432f6fdd3bce3da36488734086d399", 32271),
    "RokCityBuffReferenceAuditSubsystem.cpp.patch": ("87d38ff471d56d3d5dcd918a67fdb5521e71ac5a823d5a2cef197e97d2fc4710", 1470),
    "RokCityBuffReferenceAuditSubsystem.h": ("8154bd83f2c02602fd8eb733cef34f36e577b37381784347d42c02f3c6b91b11", 2923),
    "RokUnityModalWidget.friend.patch": ("037aa96537a33345942be0cbadd1d33e0e1969c393995341ed4dcce77fde6643", 520),
    "RokUnityModalWidget.h.candidate": ("4c1059fefad4a983f1b666446c1a87e255aeaa37fec3a1c9e097df910ea6b7ef", 12538),
    "VerifyRokCityBuffReferenceRuntime.py": ("817d6b44bf54743b9a58992c15d50a5b25e2ba77ee4b54a02ad6b9aaee6d017f", 13550),
}
SUPPLEMENT_STAGE_REL = Path("Saved/PendingCityBuffReferenceVerifierSupplementV4")
RUNS_REL = Path("Saved/RokUnityMigration/CityBuffReferenceRuntimeAuditV3")

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def discover_root(script_path: Path) -> Path:
    matches = [p.resolve() for p in script_path.resolve().parents if (p / "Rok.uproject").is_file()]
    unique = list(dict.fromkeys(matches))
    if len(unique) != 1:
        raise ValueError(f"expected exactly one Rok.uproject ancestor, found {len(unique)}")
    return unique[0]

def verify_audit3(root: Path) -> Path:
    stage = root / AUDIT3_REL
    ready = stage / "ready.json"
    report = stage / "report.json"
    if not ready.is_file() or sha(ready) != AUDIT3_READY_SHA256:
        raise ValueError("AuditV3 ready pin mismatch")
    if not report.is_file() or sha(report) != AUDIT3_REPORT_SHA256:
        raise ValueError("AuditV3 report pin mismatch")
    for name, (expected_sha, expected_bytes) in AUDIT3_FILES.items():
        file = stage / name
        if not file.is_file() or file.stat().st_size != expected_bytes or sha(file) != expected_sha:
            raise ValueError(f"AuditV3 file pin mismatch: {name}")
    return stage

def verify_published_cpp(root: Path, stage: Path) -> None:
    published = root / "Source/Rok/RokCityBuffReferenceAuditSubsystem.cpp"
    candidate = stage / "RokCityBuffReferenceAuditSubsystem.cpp"
    if not published.is_file() or sha(published) != sha(candidate):
        raise ValueError("published AuditV3 CPP does not exactly match the pinned AuditV3 candidate")

def bind_and_run(script_path: Path, argv: list[str], expected_sha: str = ORIGINAL_SHA256) -> None:
    root = discover_root(script_path)
    stage = verify_audit3(root)
    live = root / ORIGINAL_REL
    frozen = root / ORIGINAL_STAGE_REL
    if not live.is_file() or not frozen.is_file():
        raise FileNotFoundError("frozen original verifier missing from live or V1 audit stage")
    live_sha, frozen_sha = sha(live), sha(frozen)
    if live_sha != expected_sha or frozen_sha != expected_sha or live_sha != frozen_sha:
        raise ValueError("frozen original verifier SHA mismatch")
    verify_published_cpp(root, stage)
    spec = importlib.util.spec_from_file_location("frozen_city_buff_runtime_verifier_v4", frozen)
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
    if discover_root(root / "Scripts" / "VerifyRokCityBuffReferenceRuntimeSupplementV4.py") != root:
        raise AssertionError("simulated Scripts-layout root mismatch")
    verify_audit3(root)
    if sha(root / ORIGINAL_REL) != ORIGINAL_SHA256 or sha(root / ORIGINAL_STAGE_REL) != ORIGINAL_SHA256:
        raise AssertionError("frozen original verifier SHA mismatch")
    try:
        bind_and_run(here, ["--preflight"], expected_sha="0" * 64)
    except ValueError as exc:
        if "frozen original verifier SHA mismatch" not in str(exc):
            raise
    else:
        raise AssertionError("wrong-original-SHA case was accepted")
    # This current checkout is not yet published as AuditV3. The exact published-vs-AuditV3 check
    # must reject it until the reviewed candidate is integrated.
    try:
        verify_published_cpp(root, root / AUDIT3_REL)
    except ValueError as exc:
        if "does not exactly match" not in str(exc):
            raise
    else:
        raise AssertionError("unpublished AuditV3 CPP unexpectedly matched live source")
    with tempfile.TemporaryDirectory(prefix="citybuff_root_v4_") as temp:
        base = Path(temp)
        fixture_stage = base / AUDIT3_REL
        fixture_source = base / "Source/Rok/RokCityBuffReferenceAuditSubsystem.cpp"
        fixture_source.parent.mkdir(parents=True)
        fixture_stage.mkdir(parents=True)
        candidate_bytes = (root / AUDIT3_REL / "RokCityBuffReferenceAuditSubsystem.cpp").read_bytes()
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
    print("PASS: Saved/Scripts root discovery, no-root, ambiguous-root, frozen SHA, AuditV3 pins and exact published-CPP gate")

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
