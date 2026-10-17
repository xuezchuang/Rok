#!/usr/bin/env python3
"""Root-safe wrapper for the frozen strict City Buff runtime verifier."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
import os
import tempfile
from pathlib import Path

ORIGINAL_SHA256 = "817d6b44bf54743b9a58992c15d50a5b25e2ba77ee4b54a02ad6b9aaee6d017f"
ORIGINAL_REL = Path("Scripts/VerifyRokCityBuffReferenceRuntime.py")
ORIGINAL_STAGE_REL = Path("Saved/PendingCityBuffReferenceRuntimeAuditV1/VerifyRokCityBuffReferenceRuntime.py")
STAGE_REL = Path("Saved/PendingCityBuffReferenceVerifierSupplementV2")
STAGE_NAME = "PendingCityBuffReferenceRuntimeAuditV1"
RUNS_REL = Path("Saved/RokUnityMigration/CityBuffReferenceRuntimeAudit")

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def discover_root(script_path: Path) -> Path:
    matches = [p.resolve() for p in script_path.resolve().parents if (p / "Rok.uproject").is_file()]
    unique = list(dict.fromkeys(matches))
    if len(unique) != 1:
        raise ValueError(f"expected exactly one Rok.uproject ancestor, found {len(unique)}")
    return unique[0]

def bind_and_run(script_path: Path, argv: list[str], expected_sha: str = ORIGINAL_SHA256) -> None:
    root = discover_root(script_path)
    live = root / ORIGINAL_REL
    staged = root / ORIGINAL_STAGE_REL
    if not live.is_file() or not staged.is_file():
        raise FileNotFoundError("frozen original verifier missing from live or V1 audit stage")
    live_sha, staged_sha = sha(live), sha(staged)
    if live_sha != expected_sha or staged_sha != expected_sha or live_sha != staged_sha:
        raise ValueError("frozen original verifier SHA mismatch")
    spec = importlib.util.spec_from_file_location("frozen_city_buff_runtime_verifier", staged)
    if spec is None or spec.loader is None:
        raise ImportError("cannot load frozen verifier")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.ROOT = root
    module.STAGE = root / "Saved" / STAGE_NAME
    module.RUNS = root / RUNS_REL
    old_argv = __import__("sys").argv
    try:
        __import__("sys").argv = [str(staged), *argv]
        module.main()
    finally:
        __import__("sys").argv = old_argv

def self_test() -> None:
    here = Path(__file__).resolve()
    root = discover_root(here)
    if root != discover_root(root / "Scripts" / "VerifyRokCityBuffReferenceRuntimeSupplementV2.py"):
        raise AssertionError("simulated Scripts-layout root mismatch")
    if sha(root / ORIGINAL_REL) != ORIGINAL_SHA256 or sha(root / ORIGINAL_STAGE_REL) != ORIGINAL_SHA256:
        raise AssertionError("current frozen original SHA mismatch")
    with tempfile.TemporaryDirectory(prefix="citybuff_root_v2_") as temp:
        base = Path(temp)
        lone = base / "single" / "Saved" / "Stage" / "verifier.py"
        lone.parent.mkdir(parents=True)
        (base / "single" / "Rok.uproject").write_text("{}", encoding="utf-8")
        if discover_root(lone) != (base / "single").resolve():
            raise AssertionError("simulated Saved-layout root mismatch")
        try:
            discover_root(base / "noroot" / "x.py")
        except ValueError:
            pass
        else:
            raise AssertionError("no-root case was accepted")
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
            bind_and_run(here, ["--preflight"], expected_sha="0" * 64)
        except ValueError as exc:
            if "SHA mismatch" not in str(exc):
                raise
        else:
            raise AssertionError("wrong-original-SHA case was accepted")
    print("PASS: current Saved layout, simulated Scripts layout, no-root, ambiguous-root, wrong-original-SHA")

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

