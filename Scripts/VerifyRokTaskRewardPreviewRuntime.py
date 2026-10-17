"""Verify real TaskInfo reward widget samples in a TaskDetailProjectionAudit run."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE_VERIFIER = ROOT / "Scripts/VerifyRokTaskDetailProjectionRuntime.py"
EXPECTED = [
    {"currency_id": 100, "quantity": 2000, "name": "食物", "count": "2,000", "rect": [865, 349, 80, 80]},
    {"currency_id": 101, "quantity": 2000, "name": "木材", "count": "2,000", "rect": [1085, 489, 80, 80]},
    {"currency_id": 104, "quantity": 50, "name": "钻石", "count": "50", "rect": [783, 435, 80, 80]},
]
ATLAS = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"
EXPECTED_PHASES = [
    "01_initial_hall1_task_1of2",
    "02_upgrade_pending_task_1of2",
    "03_natural_completion_task_2of2",
    "04_reopened_task_2of2",
]


def need(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_catalog(path: pathlib.Path) -> dict:
    catalog = json.loads(path.read_text(encoding="utf-8-sig"))
    need(catalog["schema"] == "rok.task_reward_preview.v1", "Reward catalog schema")
    need(catalog["scope"] == "TaskMain 200900 read-only source reward preview; no account inventory or claim state",
         "Reward catalog scope")
    need(catalog["task_id"] == 200900 and catalog["reward_group_id"] == 200900, "Task/reward group identity")
    need(catalog["atlas_asset"] == ATLAS, "Source icon atlas")
    entries = catalog["entries"]
    need(len(entries) == 3, "Exactly three source reward entries")
    for entry, expected, icon_index in zip(entries, EXPECTED, (1, 2, 5), strict=True):
        need(entry["kind"] == "currency" and entry["currency_id"] == expected["currency_id"], "Currency identity/order")
        need(entry["quantity"] == expected["quantity"] and entry["display_name_zh_cn"] == expected["name"],
             "Reward amount/localized label")
        need(entry["icon_key"] == f"ui_common[img_com_cur{icon_index}]", "Source atlas icon key")
        need(entry["icon_rect"] == expected["rect"] and entry["icon_atlas_size"] == [2048, 1024],
             "Source icon rectangle/atlas extent")
    for source in catalog["sources"]:
        source_path = ROOT / source["path"]
        need(source_path.is_file() and sha(source_path) == source["sha256"],
             f"Catalog source changed: {source['path']}")
    return catalog


def uv_for_rect(rect: list[int]) -> tuple[list[float], list[float]]:
    x, y, width, height = rect
    return ([x / 2048.0, 1.0 - (y + height) / 1024.0],
            [(x + width) / 2048.0, 1.0 - y / 1024.0])


def check_widget_state(state: dict, phase: str, sample: str) -> None:
    need(state["task_screen_open"] is True, f"{phase}/{sample}: TaskInfo must be open")
    need(state["task_reward_group_placeholder_exists"] is False, f"{phase}/{sample}: obsolete reward placeholder exists")
    rows = state["task_reward_preview_rows"]
    need(len(rows) == len(EXPECTED), f"{phase}/{sample}: real reward widget row count")
    for index, (row, expected) in enumerate(zip(rows, EXPECTED, strict=True)):
        base = f"TaskRewardPreview_0_{index}"
        need(row["selector"] == base and row["exists"] is True and row["visible"] is True,
             f"{phase}/{sample}/{base}: container selector/visibility")
        need(row["currency_id"] == expected["currency_id"] and row["quantity"] == expected["quantity"]
             and row["display_name"] == expected["name"], f"{phase}/{sample}/{base}: source row mapping")
        need(row["icon_exists"] is True and row["icon_visible"] is True and row["icon_resource_path"] == ATLAS,
             f"{phase}/{sample}/{base}: icon widget/atlas")
        uv_min, uv_max = uv_for_rect(expected["rect"])
        need(len(row["icon_uv_min"]) == 2 and len(row["icon_uv_max"]) == 2,
             f"{phase}/{sample}/{base}: brush UV region missing")
        need(all(abs(actual - wanted) < 1e-5 for actual, wanted in zip(row["icon_uv_min"], uv_min, strict=True))
             and all(abs(actual - wanted) < 1e-5 for actual, wanted in zip(row["icon_uv_max"], uv_max, strict=True)),
             f"{phase}/{sample}/{base}: brush UV does not match source rectangle")
        need(row["count_exists"] is True and row["count_visible"] is True
             and row["count_text"] == expected["count"], f"{phase}/{sample}/{base}: visible quantity text")
        tooltip = row["tooltip"]
        need(expected["name"] in tooltip and expected["count"] in tooltip
             and "服务器状态待同步" in tooltip, f"{phase}/{sample}/{base}: tooltip state/boundary")
    need(state["bootstrap_task_id"] == 200900 and state["bootstrap_task_type"] == 8
         and state["bootstrap_task_param1"] == 1 and state["bootstrap_task_param2"] == 2
         and state["bootstrap_task_reward_group"] == 200900, f"{phase}/{sample}: source task identity changed")
    need(state["bootstrap_task_current"] == 1 and state["bootstrap_task_target"] == 2
         and state["bootstrap_task_complete"] is False, f"{phase}/{sample}: account task state changed")
    need(state["task_action_label"] == "前往" and state["task_action_enabled"] is True,
         f"{phase}/{sample}: reward preview made task claimable")
    need(state["http_dispatch_attempts"] == 0 and state["suppressed_requests"] == 0,
         f"{phase}/{sample}: local-only network boundary")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("run", type=pathlib.Path, help="Unique TaskDetailProjectionAudit run directory")
    args = parser.parse_args()
    run = args.run.resolve()
    need(run.is_dir() and run.parent.name == "TaskDetailProjectionAudit", "Unexpected run directory")
    manifest_path = run / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    need(manifest["schema"] == "rok.task_detail_projection.runtime_audit.v1", "Task audit schema")
    need(manifest["complete"] is True and manifest["passed"] is True and not manifest["failures"],
         "Extended task audit did not pass")
    need("TaskMain 200900 source-derived currency display only" in manifest["reward_preview_scope"],
         "Reward scope statement missing")
    need(manifest["world_time_advanced_by_audit"] is False, "Audit advanced the world clock")
    for relative, expected_hash in manifest["inputs_sha256"].items():
        path = ROOT / relative
        need(path.is_file() and sha(path) == expected_hash, f"Runtime input hash mismatch: {relative}")

    catalog_path = ROOT / "Saved/RokUnityMigration/task_reward_preview.json"
    catalog = check_catalog(catalog_path)
    need(manifest["inputs_sha256"].get("Saved/RokUnityMigration/task_reward_preview.json") == sha(catalog_path),
         "Runtime did not pin the actual reward catalog")
    need(manifest["inputs_sha256"].get("Scripts/BuildRokUnityTaskRewardPreview.py") == sha(ROOT / "Scripts/BuildRokUnityTaskRewardPreview.py"),
         "Runtime did not pin the catalog builder")
    need(manifest["inputs_sha256"].get("Scripts/VerifyRokTaskRewardPreviewRuntime.py") == sha(pathlib.Path(__file__)),
         "Runtime did not pin this verifier")
    catalog_verifier = ROOT / "Scripts/VerifyRokUnityTaskRewardPreview.py"
    need(catalog_verifier.is_file()
         and manifest["inputs_sha256"].get("Scripts/VerifyRokUnityTaskRewardPreview.py") == sha(catalog_verifier),
         "Runtime did not pin the catalog verifier")

    captures = manifest["captures"]
    need([capture["phase"] for capture in captures] == EXPECTED_PHASES, "Task audit capture order")
    sampled_states = 0
    for capture in captures:
        for sample in ("requested_state", "processed_state", "next_frame_state"):
            need(sample in capture, f"{capture['phase']}: missing {sample}")
            check_widget_state(capture[sample], capture["phase"], sample)
            sampled_states += 1

    base_result = run / "verified_task_detail_projection.json"
    subprocess.run([sys.executable, str(BASE_VERIFIER), str(run)], check=True, cwd=ROOT,
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    base = json.loads(base_result.read_text(encoding="utf-8-sig"))
    manifest_hash = sha(manifest_path)
    need(base["passed"] is True and base["manifest_sha256"] == manifest_hash,
         "Base TaskDetailProjection verifier did not pass this exact manifest")

    result = {
        "schema": "rok.task_reward_preview.runtime_verify.v1",
        "passed": True,
        "manifest_sha256": manifest_hash,
        "base_task_detail_verifier": "verified_task_detail_projection.json",
        "base_task_detail_verifier_manifest_sha256": base["manifest_sha256"],
        "reward_catalog_sha256": sha(catalog_path),
        "task_id": catalog["task_id"],
        "reward_group_id": catalog["reward_group_id"],
        "entries": [{"currency_id": row["currency_id"], "quantity": row["quantity"],
                     "display_name_zh_cn": row["display_name_zh_cn"], "icon_rect": row["icon_rect"]}
                    for row in catalog["entries"]],
        "widget_samples_checked": sampled_states,
        "tooltip_evidence": "UWidget::GetToolTipText state samples only; this verifier does not claim the tooltip was shown in screenshots",
        "scope": "Read-only reward preview. Task bootstrap/progress/action, wallet, inventory, account and server state remain unchanged.",
    }
    out = run / "verified_task_reward_preview_runtime.json"
    out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
