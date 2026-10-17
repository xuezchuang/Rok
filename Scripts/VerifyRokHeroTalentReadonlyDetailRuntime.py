#!/usr/bin/env python3
"""Strict verifier for one real local-bootstrap Hero talent Slate route.

Self-tests are synthetic rejection probes only, never runtime acceptance.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import io
import json
import re
from pathlib import Path
from rok_data_paths import resolve

from PIL import Image

SCRIPT = Path(__file__).resolve()
ROOT = SCRIPT.parents[1] if SCRIPT.parent.name == "Scripts" else SCRIPT.parents[2]
STAGE = ROOT / "Saved/PendingHeroTalentReadonlyDetailRuntimeAuditV6"
SCHEMA = "rok.hero_talent_readonly_detail.runtime_audit.v1"
SCOPE = "One actual local bootstrap commander: two catalog talent branch detail cards through real HUD and Slate; read-only preview only"
PHASES = ("00_city_hud", "01_commander_default_2007", "02_bootstrap_hero_selected",
          "03_branch_0_detail", "04_branch_1_detail", "05_detail_closed", "06_hud_return")
ACTIONS = ("open_commander_from_hud", "select_local_bootstrap_hero", "open_catalog_branch_0",
           "switch_to_catalog_branch_1", "close_talent_detail", "return_to_hud")
TARGETS = ("CommanderFeatureButton", "CommanderHeadAction_0", "CommanderTalentLabel_0",
           "CommanderTalentLabel_1", "CommanderTalentDetailClose", "CommanderBack")
CLASSES = ("/Script/Rok.RokUiAnimatedButton",) + ("/Script/Rok.RokUnityIndexedButton",) * 5
HERO_IDS = frozenset((1006, 1007, 1009, 1010, 1012))
AUDIT_FILES = ("Source/Rok/RokHeroTalentReadonlyDetailAuditSubsystem.h",
               "Source/Rok/RokHeroTalentReadonlyDetailAuditSubsystem.cpp",
               "Scripts/VerifyRokHeroTalentReadonlyDetailRuntime.py")
CAMERA = ("camera_x", "camera_y", "camera_z", "camera_pitch", "camera_yaw", "camera_roll",
          "camera_scale_x", "camera_scale_y", "camera_scale_z")
FALSE_CLAIMS = ("world_time_advanced_by_audit", "source_pixel_parity_claim", "unity_pixel_parity_accepted",
                "full_migration_accepted", "full_451_talent_tree_accepted", "other_bootstrap_heroes_all_branches_run")


def need(ok: bool, reason: str) -> None:
    if not ok:
        raise ValueError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def checked(root: Path, relative: str) -> Path:
    need(not Path(relative).is_absolute() and ".." not in Path(relative).parts, "input path escape")
    path = resolve(relative).resolve(strict=True)
    need(root.resolve() in path.parents, "input outside project")
    return path


def expected_inputs() -> dict[str, str]:
    report = json.loads((STAGE / "release_report.json").read_text(encoding="utf-8"))
    need(report["schema"] == "rok.hero_talent_readonly_detail.audit_stage.v6", "stage schema")
    need(set(report["files"]) == set(AUDIT_FILES), "audit stage file set")
    shared = checked(ROOT, "Saved/PendingCommanderUiSettledAuditV1/RokCommanderUiSettledProbe.h")
    need(sha(shared) == report["shared_settled_probe_sha256"], "shared settled probe bytes drift")
    cpp = checked(ROOT, "Source/Rok/RokHeroTalentReadonlyDetailAuditSubsystem.cpp").read_text(encoding="utf-8-sig")
    body = cpp.split("/** Read-only observation", 1)[1].split("constexpr TCHAR CityMap[]", 1)[0]
    need(hashlib.sha256(body.encode("utf-8")).hexdigest() == report["embedded_settled_probe_sha256"],
         "integrated V6 settled probe differs from reviewed implementation")
    need("if (Child->IsVisible() &&" not in body and "if (!bChildSettled)" in body,
         "hidden/uncached child can evade settled gate")
    return {**report["source_and_product_inputs_sha256"],
            **{p: report["files"][p]["sha256"] for p in AUDIT_FILES}}


def point(value: dict) -> tuple[float, float]:
    return float(value["x"]), float(value["y"])


def shown(widgets: dict, name: str) -> bool:
    v = widgets[name]
    return v["exists"] is True and v["visible"] is True and v["enabled"] is True


def hidden(widgets: dict, name: str) -> bool:
    v = widgets[name]
    return v["exists"] is False or v["visible"] is False


def check_settled(value: dict, require_mesh: bool) -> None:
    need(value["ready"] is True and value["structure_valid"] is True
         and value["roster_children"] > 0 and value["data_children"] > 0
         and value["unsettled_children"] == 0
         and 0 <= value["not_visible_children"] <= value["roster_children"] + value["data_children"]
         and value["unsettled_not_visible_children"] == 0
         and value["remaining_texture_compilations"] == 0,
         "Commander animated groups or texture compilation not settled")
    if require_mesh or value["portrait_present"]:
        need(value["portrait_present"] is True and value["portrait_attached"] is True
             and value["portrait_frames"] >= 2 and value["portrait_textures"] > 0
             and value["portrait_texture_property_valid"] is True
             and value["portrait_textures_rhi_ready"] is True
             and value["portrait_playing_standby"] is False
             and abs(value["portrait_camera_x"]) <= .01,
             "Commander portrait transition or RHI not settled")


def check_claim(d: dict) -> None:
    need(d["schema"] == SCHEMA and d["complete"] is True and d["passed"] is True
         and d["failures"] == [] and d["attempts"] == [], "run incomplete or failed")
    need(d["scope"] == SCOPE and d["baseline_phase"] == PHASES[0] and d["completion_events"] == 0,
         "scope/baseline/business completion mismatch")
    for name in FALSE_CLAIMS:
        need(d[name] is False, f"overclaim: {name}")
    need(d["engine_major"] == 5 and d["engine_minor"] == 8
         and Path(d["project"]).resolve() == ROOT.resolve()
         and Path(d["engine_dir"]).resolve() == Path("D:/ue/UE_5.8/Engine").resolve()
         and d["world"] == "/Game/Maps/L_RokUnityGameAppCity"
         and isinstance(d["pid"], int) and d["pid"] > 0, "project/engine/map/PID identity")


def check_business(b: dict) -> None:
    need("UnityInitId_5" in b["hall_tags"] and "UnityBuildingType_1" in b["hall_tags"]
         and b["hall_actor_path"] in b["city_actor_paths"], "Hall source actor identity")
    need(b["active_upgrades"] == b["pending_completed_upgrades"] == b["active_construction"] == 0
         and b["construction_ticket_1_exists"] is False and b["completed_upgrade_events"] == 0,
         "build/construction queue not idle")
    need(b["profile_kind"] == "local_preview" and b["profile_has_role_id"] is False
         and b["profile_role_id"] == "" and b["profile_has_civilization"] is False
         and b["profile_civilization_id"] == 0, "account boundary")
    need(b["server_local_only"] is True and b["http_dispatch_attempts"] == 0
         and b["http_suppressed_requests"] == 0, "HTTP boundary")
    need(len(b["starter_heroes"]) == 5 and {x["id"] for x in b["starter_heroes"]} == HERO_IDS
         and len(b["starter_troops"]) == 4, "bootstrap hero/troop snapshot")
    need(b["hall_actor_path"] in b["city_actor_paths"]
         and len(b["city_actor_paths"]) == len(set(b["city_actor_paths"])), "city actor identity")
    for name in ("wallet_food", "wallet_wood", "wallet_stone", "wallet_gold", "owned_queues",
                 "rate_food", "rate_wood", "rate_stone", "rate_gold", "task_id", "task_current",
                 "task_target", "task_chain", "trainable_troop_ids", "profile_local_hero_score",
                 "profile_local_troop_power", "profile_local_preview_score"):
        need(name in b, f"business field missing: {name}")
    need(all({"id", "level", "star", "max_star", "rare", "score", "skill_count", "talent_count",
              "icon", "model"} <= set(h) for h in b["starter_heroes"]), "hero progression snapshot incomplete")
    need(all({"id", "count", "level", "power", "train_seconds"} <= set(t)
             for t in b["starter_troops"]), "troop/training snapshot incomplete")


def source_talents(hero_id: int) -> tuple[tuple[int, int], tuple[tuple[str, str], tuple[str, str]]]:
    catalog = json.loads(checked(ROOT, "Saved/RokUnityMigration/unity_gameplay_catalog.json").read_text(encoding="utf-8"))
    hero = [x for x in catalog["heroes"] if x["id"] == hero_id]
    need(len(hero) == 1 and len(hero[0]["talent_ids"]) >= 2, "source hero/branches missing")
    ids = tuple(hero[0]["talent_ids"][:2])
    need(ids[0] != ids[1], "branches not distinct")
    texts = []
    for talent_id in ids:
        rows = [x for x in catalog["hero_talents"] if x["id"] == talent_id]
        need(len(rows) == 1, "source talent row missing/duplicate")
        row = rows[0]
        title, tips = row["display_name_zh_cn"] or row["display_name_en"], row["display_tips_zh_cn"] or row["display_tips_en"]
        need(bool(title) and bool(tips), "source title/DisplayTips missing")
        texts.append((title, tips))
    return ids, tuple(texts)


def check_state(s: dict, phase: str, baseline: dict, primary: int, selection: str, camera: tuple,
                hero_id: int, ids: tuple[int, int], texts: tuple) -> None:
    need(s["business"] == baseline and s["primary_press_count"] == primary
         and s["selected_actor"] == selection and s["modal_open"] is False, f"business/world input: {phase}")
    need(all(abs(s[k] - v) <= .001 for k, v in zip(CAMERA, camera)), f"camera moved: {phase}")
    w = s["widgets"]
    if phase in (PHASES[0], PHASES[6]):
        need(s["screen_open"] is False and shown(w, "CommanderFeatureButton"), "HUD not mounted")
        return
    mode = w["FeatureModeCanvas"]
    need(s["screen_open"] is True and shown(w, "CommanderBack") and mode["visible"] is True
         and abs(mode["opacity"] - 1) <= .01
         and all(abs(v - 1) <= .01 for v in point(mode["scale"])), "Commander not settled")
    check_settled(s["commander_ui_settled"], phase != PHASES[1])
    if phase == PHASES[1]:
        need(s["selected_hero_id"] == 2007 and s["selected_talent_id"] == -1
             and shown(w, "CommanderHeadAction_0"), "default Commander hero identity")
        return
    need(s["selected_hero_id"] == s["expected_bootstrap_hero_id"] == hero_id
         and (s["expected_talent_0_id"], s["expected_talent_1_id"]) == ids
         and shown(w, "CommanderTalentLabel_0") and shown(w, "CommanderTalentLabel_1"),
         "local bootstrap hero/branches absent")
    if phase in (PHASES[2], PHASES[5]):
        need(s["selected_talent_id"] == -1 and hidden(w, "CommanderTalentDetailFrame"),
             "closed detail remains visible/selected")
        return
    branch = 0 if phase == PHASES[3] else 1
    need(s["selected_talent_id"] == ids[branch]
         and all(shown(w, name) for name in ("CommanderTalentDetailFrame", "CommanderTalentDetailFrame_Center",
                                                "CommanderTalentDetailTitle", "CommanderTalentDetailTips",
                                                "CommanderTalentDetailClose")), "detail not mounted")
    need((w["CommanderTalentDetailTitle"]["text"], w["CommanderTalentDetailTips"]["text"]) == texts[branch],
         "localized title/DisplayTips differ from source")
    center = w["CommanderTalentDetailFrame_Center"]["local_size"]
    need(center["x"] > 0 and center["y"] > 0, "source nine-slice center absent")


def check_action(a: dict, index: int, pid: int, baseline: dict, primary: int) -> None:
    label = ACTIONS[index]
    need(a["label"] == label and a["target"] == TARGETS[index]
         and a["method"] == "ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent",
         f"action identity: {label}")
    need(all(a[k] is True for k in ("target_exists", "exact_button_in_path", "umg_path_present",
                                     "target_hovered", "own_game_window", "foreground", "down_handled", "up_handled"))
         and a["failure"] == "" and a["stable_frames"] >= 2, f"genuine Slate action proof: {label}")
    i, types, nodes = a["exact_button_path_index"], a["hit_path_types"], a["hit_path_nodes"]
    need(isinstance(i, int) and 0 <= i < len(types) and len(nodes) == len(types)
         and "SButton" in types[i] and any("SObjectWidget" in t for t in types)
         and nodes[i]["type"] == types[i] and nodes[i]["slate_identity"] == a["target_slate_identity"]
         and len({x["slate_identity"] for x in nodes}) == len(nodes), f"exact target not in hit path: {label}")
    need(a["target_widget_class"] == CLASSES[index]
         and re.fullmatch(r"0x[0-9A-Fa-f]+", a["target_widget_identity"])
         and a["own_window_identity"] == a["foreground_window_identity"]
         and re.fullmatch(r"0x[0-9A-Fa-f]+", a["own_window_identity"])
         and a["own_window_pid"] == pid, f"class/window/PID identity: {label}")
    need(a["primary_before"] == a["primary_after"] == primary
         and a["business_before"] == a["business_after"] == baseline, f"business/world mutation: {label}")
    # Indexed actions are deferred until the next widget tick; the last click's
    # synchronous stack still sees Commander open. Capture 06 proves it closed.
    need(a["screen_open_before"] is (index != 0) and a["screen_open_after"] is True
         and a["modal_open_before"] is False and a["modal_open_after"] is False,
         f"synchronous route state: {label}")
    center, left, right, size = map(point, (a["absolute_center"], a["absolute_top_left"],
                                           a["absolute_bottom_right"], a["local_size"]))
    need(size[0] > 0 and size[1] > 0 and left[0] < center[0] < right[0]
         and left[1] < center[1] < right[1], f"target geometry/center: {label}")


def check_png(path: Path, digest: str, count: int) -> None:
    raw = path.read_bytes()
    need(len(raw) == count and hashlib.sha256(raw).hexdigest() == digest, "original PNG hash/bytes")
    with Image.open(io.BytesIO(raw)) as im:
        need(im.format == "PNG" and im.size == (1440, 810), "original PNG format/extent")
        im.verify()
    with Image.open(io.BytesIO(raw)) as im:
        im.load()
        need(im.size == (1440, 810), "original PNG decode")


def validate(data: dict, run: Path) -> None:
    check_claim(data)
    expected = expected_inputs()
    need(data["inputs_sha256"] == expected, "runtime source/product/audit pin set")
    for name, digest in expected.items():
        need(sha(checked(ROOT, name)) == digest, f"input bytes drift: {name}")
    header = checked(ROOT, "Source/Rok/RokUnityFeatureScreenWidget.h").read_text(encoding="utf-8-sig")
    need(re.search(r"class\s+ROK_API\s+URokUnityIndexedButton\s*:\s*public\s+URokUiAnimatedButton\b", header),
         "indexed button inheritance drift")
    hero_id, baseline = data["selected_bootstrap_hero_id"], data["baseline_business"]
    check_business(baseline)
    need(hero_id in HERO_IDS and hero_id in {h["id"] for h in baseline["starter_heroes"]}, "not local bootstrap hero")
    ids, texts = source_talents(hero_id)
    need((data["talent_0_id"], data["talent_1_id"]) == ids, "branch ID/source mismatch")
    captures, actions = data["captures"], data["actions"]
    need(len(captures) == 7 and len(actions) == 6 and tuple(c["phase"] for c in captures) == PHASES
         and tuple(a["label"] for a in actions) == ACTIONS, "7 PNG/6 action sequence")
    first = captures[0]["requested_state"]
    primary, selection = first["primary_press_count"], first["selected_actor"]
    camera = tuple(first[k] for k in CAMERA)
    previous = -1
    for index, cap in enumerate(captures):
        phase = PHASES[index]
        frames = (cap["requested_frame"], cap["processed_frame"], cap["next_frame"])
        need(previous < frames[0] <= frames[1] < frames[2], f"screenshot frame ordering: {phase}")
        previous = frames[2]
        path = Path(cap["png"])
        need(path.is_absolute() and path.resolve(strict=True) == (run / f"{phase}.png").resolve(strict=True)
             and path.parent.resolve() == run.resolve(), f"screenshot path escape: {phase}")
        need(cap["png_width"] == 1440 and cap["png_height"] == 810, "PNG declared dimensions")
        check_png(path, cap["png_sha256"], cap["png_bytes"])
        for key, frame in zip(("requested_state", "processed_state", "next_frame_state"), frames):
            state = cap[key]
            need(state["frame"] == frame, f"state/frame mismatch: {phase}")
            check_state(state, phase, baseline, primary, selection, camera, hero_id, ids, texts)
    for index, action in enumerate(actions):
        need(captures[index]["next_frame"] < action["frame"] < captures[index + 1]["requested_frame"],
             f"click not between captures: {ACTIONS[index]}")
        check_action(action, index, data["pid"], baseline, primary)
    check_state(data["final_state"], PHASES[-1], baseline, primary, selection, camera, hero_id, ids, texts)
    need(data["final_state"]["frame"] >= captures[-1]["next_frame"], "final state before final screenshot")


def self_test() -> int:
    # Only the rejection gates are tested here; no synthetic runtime success is reported.
    claim = {"schema": SCHEMA, "complete": True, "passed": True, "failures": [], "attempts": [],
             "scope": SCOPE, "baseline_phase": PHASES[0], "completion_events": 0,
             "engine_major": 5, "engine_minor": 8, "project": str(ROOT), "engine_dir": "D:/ue/UE_5.8/Engine",
             "world": "/Game/Maps/L_RokUnityGameAppCity", "pid": 1234,
             **{name: False for name in FALSE_CLAIMS}}
    check_claim(claim)
    cases = 0
    for key, bad in (("passed", False), ("complete", False), ("attempts", [{}]),
                     ("scope", "full migration"), ("engine_minor", 7),
                     ("world", "/Game/Maps/Other"), ("completion_events", 1),
                     ("full_migration_accepted", True), ("unity_pixel_parity_accepted", True),
                     ("full_451_talent_tree_accepted", True), ("other_bootstrap_heroes_all_branches_run", True)):
        item = copy.deepcopy(claim); item[key] = bad
        try:
            check_claim(item)
        except (ValueError, KeyError, TypeError):
            cases += 1
        else:
            raise RuntimeError(f"negative claim accepted: {key}")
    business = {"http_dispatch_attempts": 0}
    action = {"label": ACTIONS[0], "target": TARGETS[0],
              "method": "ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent",
              **{key: True for key in ("target_exists", "exact_button_in_path", "umg_path_present",
                                       "target_hovered", "own_game_window", "foreground", "down_handled", "up_handled")},
              "failure": "", "stable_frames": 2, "exact_button_path_index": 2,
              "hit_path_types": ["SWindow", "SObjectWidget", "SButton"],
              "hit_path_nodes": [{"type": t, "slate_identity": hex(i + 1)}
                                 for i, t in enumerate(("SWindow", "SObjectWidget", "SButton"))],
              "target_slate_identity": "0x3", "target_widget_class": CLASSES[0],
              "target_widget_identity": "0x4", "own_window_identity": "0x5",
              "foreground_window_identity": "0x5", "own_window_pid": 1234,
              "primary_before": 0, "primary_after": 0, "business_before": business, "business_after": business,
              "screen_open_before": False, "screen_open_after": True,
              "modal_open_before": False, "modal_open_after": False,
              "absolute_center": {"x": 50, "y": 50}, "absolute_top_left": {"x": 0, "y": 0},
              "absolute_bottom_right": {"x": 100, "y": 100}, "local_size": {"x": 100, "y": 100}}
    check_action(action, 0, 1234, business, 0)
    for key, bad in (("target", "wrong"), ("target_widget_class", CLASSES[1]),
                     ("target_slate_identity", "0x8"), ("target_hovered", False),
                     ("own_game_window", False), ("foreground", False),
                     ("down_handled", False), ("up_handled", False),
                     ("stable_frames", 1), ("primary_after", 1),
                     ("business_after", {"http_dispatch_attempts": 1}),
                     ("screen_open_after", False)):
        item = copy.deepcopy(action); item[key] = bad
        try:
            check_action(item, 0, 1234, business, 0)
        except (ValueError, KeyError, TypeError):
            cases += 1
        else:
            raise RuntimeError(f"negative action accepted: {key}")
    ids, texts = source_talents(1006)
    widget = lambda: {"exists": True, "visible": True, "enabled": True}
    names = ("CommanderBack", "CommanderTalentLabel_0", "CommanderTalentLabel_1",
             "CommanderTalentDetailFrame", "CommanderTalentDetailFrame_Center",
             "CommanderTalentDetailTitle", "CommanderTalentDetailTips", "CommanderTalentDetailClose")
    widgets = {name: widget() for name in names}
    widgets["FeatureModeCanvas"] = {"visible": True, "opacity": 1,
                                    "scale": {"x": 1, "y": 1}}
    widgets["CommanderTalentDetailFrame_Center"]["local_size"] = {"x": 388, "y": 213}
    widgets["CommanderTalentDetailTitle"]["text"] = texts[0][0]
    widgets["CommanderTalentDetailTips"]["text"] = texts[0][1]
    state = {"business": business, "primary_press_count": 0, "selected_actor": "",
             "modal_open": False, "screen_open": True, "widgets": widgets,
             "selected_hero_id": 1006, "expected_bootstrap_hero_id": 1006,
             "expected_talent_0_id": ids[0], "expected_talent_1_id": ids[1],
             "selected_talent_id": ids[0], **{name: 0 for name in CAMERA},
             "commander_ui_settled": {
                 "ready": True, "structure_valid": True, "roster_children": 4,
                 "data_children": 8, "unsettled_children": 0,
                 "not_visible_children": 0, "unsettled_not_visible_children": 0,
                 "remaining_texture_compilations": 0, "portrait_present": True,
                 "portrait_attached": True, "portrait_frames": 12, "portrait_textures": 12,
                 "portrait_camera_x": 0, "portrait_texture_property_valid": True,
                 "portrait_textures_rhi_ready": True, "portrait_playing_standby": False}}
    check_state(state, PHASES[3], business, 0, "", (0,) * len(CAMERA), 1006, ids, texts)
    for path, bad in (("title", "wrong"), ("tips", "wrong"), ("center", 0),
                      ("selected_talent_id", ids[1]), ("selected_hero_id", 2007)):
        item = copy.deepcopy(state)
        if path == "title": item["widgets"]["CommanderTalentDetailTitle"]["text"] = bad
        elif path == "tips": item["widgets"]["CommanderTalentDetailTips"]["text"] = bad
        elif path == "center": item["widgets"]["CommanderTalentDetailFrame_Center"]["local_size"]["x"] = bad
        else: item[path] = bad
        try:
            check_state(item, PHASES[3], business, 0, "", (0,) * len(CAMERA), 1006, ids, texts)
        except (ValueError, KeyError, TypeError):
            cases += 1
        else:
            raise RuntimeError(f"negative detail state accepted: {path}")
    for key, bad in (("ready", False), ("structure_valid", False),
                     ("roster_children", 0), ("data_children", 0),
                     ("unsettled_children", 1), ("not_visible_children", -1),
                     ("unsettled_not_visible_children", 1),
                     ("remaining_texture_compilations", 1),
                     ("portrait_present", False), ("portrait_attached", False),
                     ("portrait_frames", 1), ("portrait_textures", 0),
                     ("portrait_camera_x", 1.0), ("portrait_texture_property_valid", False),
                     ("portrait_textures_rhi_ready", False), ("portrait_playing_standby", True)):
        item = copy.deepcopy(state)
        item["commander_ui_settled"][key] = bad
        try:
            check_state(item, PHASES[3], business, 0, "", (0,) * len(CAMERA), 1006, ids, texts)
        except (ValueError, KeyError, TypeError):
            cases += 1
        else:
            raise RuntimeError(f"negative settled state accepted: {key}")
    item = copy.deepcopy(state)
    item["commander_ui_settled"].update(not_visible_children=1, unsettled_children=1,
                                         unsettled_not_visible_children=1)
    try:
        check_state(item, PHASES[3], business, 0, "", (0,) * len(CAMERA), 1006, ids, texts)
    except (ValueError, KeyError, TypeError):
        cases += 1
    else:
        raise RuntimeError("uncached child with stale translation/opacity accepted")
    return cases


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path, help="absolute real Hero audit run directory")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    need(args.self_test or args.run is not None, "provide --self-test or --run")
    negatives = self_test() if args.self_test else 0
    if args.run is not None:
        need(args.run.is_absolute() and args.run.is_dir(), "run must be an absolute actual directory")
        run_root = ROOT / "Saved/PendingHeroTalentReadonlyDetailRuntimeAuditV6/RuntimeAudit"
        need(args.run.parent.resolve() == run_root.resolve()
             and re.fullmatch(r"[0-9]+_[0-9]+", args.run.name), "run outside fixed Hero audit root")
        validate(json.loads((args.run / "manifest.json").read_text(encoding="utf-8")), args.run)
    print(json.dumps({"schema": "rok.hero_talent_readonly_detail.runtime_verify.v1",
                      "passed": True, "actual_runtime_verified": args.run is not None,
                      "synthetic_negative_only": args.run is None,
                      "run": str(args.run) if args.run else None,
                      "negative_cases": negatives}, ensure_ascii=False))


if __name__ == "__main__":
    main()
