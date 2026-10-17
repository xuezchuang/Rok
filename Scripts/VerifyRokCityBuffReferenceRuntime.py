#!/usr/bin/env python3
"""Strict real HUD/Slate City Buff receipts; --preflight checks staged inputs only."""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import re
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
STAGE = ROOT / "Saved/PendingCityBuffReferenceRuntimeAuditV1"
RUNS = ROOT / "Saved/RokUnityMigration/CityBuffReferenceRuntimeAudit"
SCHEMA = "rok.city_buff_reference.runtime_audit.v1"
AUDIT_FILES = {
    "Source/Rok/RokCityBuffReferenceAuditSubsystem.h": "RokCityBuffReferenceAuditSubsystem.h",
    "Source/Rok/RokCityBuffReferenceAuditSubsystem.cpp": "RokCityBuffReferenceAuditSubsystem.cpp",
    "Source/Rok/RokUnityModalWidget.h": "RokUnityModalWidget.h.candidate",
    "Scripts/VerifyRokCityBuffReferenceRuntime.py": "VerifyRokCityBuffReferenceRuntime.py",
}
PRODUCT = {
    "Source/Rok/RokUnityModalWidget.cpp": ("Saved/PendingCityBuffConfigReferenceV2/RokUnityModalWidget.cpp.candidate", "9dc34a7279c15f7cf153c8f063cb06d565be9f9792e50def35bac2195241f05c"),
    "Source/Rok/RokUnityModalWidget.h": ("Saved/PendingCityBuffReferenceRuntimeAuditV1/RokUnityModalWidget.h.candidate", "4c1059fefad4a983f1b666446c1a87e255aeaa37fec3a1c9e097df910ea6b7ef"),
    "Saved/RokUnityMigration/city_buff_reference.json": ("Saved/PendingCityBuffConfigReferenceV2/city_buff_reference.json", "850733b77e4dfaf330177c2ba035ce538f489d13f28997b27f94d0a66703b1f0"),
}
PINNED = {
    "Source/Rok/RokMainHudWidget.cpp": "6cb5e93c80e60fc2a6663d584e6d531fadd0da2f9a4c7583b2477557640da0f1",
    "Source/Rok/RokPlayerController.cpp": "f5c297bfb9b972db07e7725a9663a42e2c35f928625167e033e1bad97c6c5ef6",
    "Saved/RokUnityMigration/ConfigTables/Language.json": "e228eca8deb7c8daa4a2d910f736c57567db11322d558544309bfbaca412277d",
    "Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.uasset": "eedd8ff20861fd25ab543eaaa833a7177b9990829c09e573d9532d1a08717d3e",
    "ref/config-tables/编译客户端配置/CSV/CityBuff.csv": "53432006ce01a2dc55ca05098f6a02fc9fdda5417ea5a50d5f5d307803009e6a",
    "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_mainmenu.tpsheet": "04f4c19bff18cfb23414b51b94255ae78e01a3871e26550d7ea3cc4e3a2b13bd",
    "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UICityManager/UI_Pop_BuffListMediator.cs": "9ce4ad4aee502f59dc8b8ab2ae2e1db736bf141ed5269f6ac07781c1f3a713bb",
}
ATLAS = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"
RECTS = ((38, 142, 35, 35), (114, 476, 35, 35))
TITLES = ("食物采集速度+10%", "食物产量+25%")
FIELDS = ("配置时长 21600 秒（非剩余时间）", "配置时长 28800 秒（非剩余时间）")

def need(ok, message):
    if not ok:
        raise ValueError(message)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def checked(relative):
    p = Path(relative)
    need(not p.is_absolute() and ".." not in p.parts, "input path escape")
    result = (ROOT / p).resolve(strict=True)
    need(ROOT.resolve() in result.parents and result.is_file(), "input outside repo")
    return result

def source_inputs(preflight):
    cpp = (STAGE / "RokCityBuffReferenceAuditSubsystem.cpp").read_text(encoding="utf-8")
    pins = dict(re.findall(r'\{TEXT\("([^\"]+)"\), TEXT\("([0-9a-f]{64})"\)\}', cpp))
    expected = {path: h for path, (_, h) in PRODUCT.items()} | PINNED
    need(pins == expected, "audit input pins differ from reviewed product/source set")
    for path, (staged, h) in PRODUCT.items():
        need(sha(checked(staged)) == h, f"staged product drift: {path}")
        if not preflight:
            need(sha(checked(path)) == h, f"published product drift: {path}")
    for path, h in PINNED.items():
        need(sha(checked(path)) == h, f"source pin drift: {path}")
    for published, staged in AUDIT_FILES.items():
        need((STAGE / staged).is_file(), f"missing staged audit: {staged}")
        if not preflight:
            need(sha(checked(published)) == sha(STAGE / staged), f"published audit drift: {published}")
    need("ProcessMouseButtonDownEvent" in cpp and "LocateWindowUnderMouse" in cpp
         and "RequestScreenshot" in cpp and "\\x89PNG\\r\\n\\x1a\\n" in cpp
         and "GetClientRect" in cpp and "GetForegroundWindow" in cpp
         and "GetCityPlacementPreviewSnapshot" in cpp and "GetHttpDispatchAttemptCount" in cpp
         and "Click(Cast<UButton>" in cpp and "ModalDismissMask" in cpp
         and "BuffListMoreButton" in cpp and "FeatureFrameClose" in cpp,
         "native UI/input/capture/boundary mechanism absent")
    header = (STAGE / "RokUnityModalWidget.h.candidate").read_text(encoding="utf-8")
    need("friend class URokCityBuffReferenceAuditSubsystem;" in header, "modal friend missing")
    return expected

def point(v):
    return float(v["x"]), float(v["y"])

def widget(state, name):
    return state["widgets"][name]

def shown(w):
    a, b = point(w["top_left"]), point(w["bottom_right"])
    return w["exists"] is True and w["visible"] is True and 0 <= a[0] < b[0] <= 1441 and 0 <= a[1] < b[1] <= 811

def source_uv(rect):
    x, y, w, h = rect
    return ((x / 1024, (1024 - y - h) / 1024), ((x + w) / 1024, (1024 - y) / 1024))

def close_point(a, b):
    return all(abs(x - y) <= 0.0002 for x, y in zip(a, b))

def check_open(state, reference):
    need(state["authority"] == ("local_fixture" if reference else "server")
         and state["reference"] is reference and state["modal_open"] is True
         and state["modal_reference_flag"] is reference
         and state["city_manager_open"] is False and state["business_boundary"] is True,
         "open authority/modal state")
    need(shown(widget(state, "BuffListPopupTitle")) and shown(widget(state, "BuffListMoreButton"))
         and shown(widget(state, "ModalDismissMask")), "open popup controls")
    if not reference:
        need(shown(widget(state, "BuffListServiceBoundaryTitle"))
             and widget(state, "BuffListServiceBoundaryTitle")["text"] == "城市增益状态尚未接入"
             and all(widget(state, f"BuffListDefinitionRow_{i}")["exists"] is False for i in (0, 1))
             and widget(state, "BuffListReferencePreviewNotice")["exists"] is False,
             "production must show no active/reference rows")
        return
    need(shown(widget(state, "BuffListReferencePreviewNotice"))
         and widget(state, "BuffListReferencePreviewNotice")["text"] == "城市增益静态配置预览 · 非实时/未激活"
         and widget(state, "BuffListServiceBoundaryTitle")["exists"] is False,
         "persistent static fixture authority")
    for i in (0, 1):
        title, field, icon = (widget(state, f"BuffListDefinitionTitle_{i}"),
                              widget(state, f"BuffListDefinitionFields_{i}"),
                              widget(state, f"BuffListDefinitionIcon_{i}"))
        need(shown(title) and title["text"] == TITLES[i]
             and shown(field) and field["text"] == FIELDS[i]
             and shown(icon) and icon["texture"] == ATLAS,
             f"fixture row {i} title/field/icon")
        uv0, uv1 = source_uv(RECTS[i])
        need(close_point(point(icon["uv_min"]), uv0) and close_point(point(icon["uv_max"]), uv1),
             f"fixture icon {i} exact source rect")

def check_manager(state):
    need(state["reference"] is True and state["authority"] == "local_fixture"
         and state["modal_open"] is False and state["city_manager_open"] is True
         and state["business_boundary"] is True
         and shown(widget(state, "CityManagerListScroll"))
         and shown(widget(state, "FeatureFrameClose")), "More did not reach City Manager")

def check_return(state):
    need(state["modal_open"] is False and state["city_manager_open"] is False
         and state["business_boundary"] is True, "return to HUD state")

def check_png(capture, expected_name):
    p = Path(capture["png"])
    need(p.is_absolute() and p.name == expected_name + ".png"
         and p.resolve().is_relative_to(RUNS.resolve()), "screenshot path")
    raw = p.read_bytes()
    need(raw[:8] == b"\x89PNG\r\n\x1a\n" and len(raw) > 50000
         and sha(p) == capture["png_sha256"] and len(raw) == capture["png_bytes"],
         "canonical PNG signature/hash/size")
    with Image.open(io.BytesIO(raw)) as im:
        need(im.format == "PNG" and im.size == (1440, 810), "PNG decode/extent")
        rgb = im.convert("RGB")
        sample = list(rgb.resize((180, 101)).getdata())
        middle = list(rgb.crop((350, 120, 1090, 690)).resize((92, 71)).getdata())
    need(sum(max(px) > 20 for px in sample) / len(sample) > .24
         and len(set(sample)) > 70 and len(set(middle)) > 35,
         "black or immature screenshot")

def check_action(action, label, target, outside):
    need(action["label"] == label and action["target"] == target
         and action["method"] == "ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent"
         and action["exact_target"] is True and action["owned_foreground"] is True
         and action["down_handled"] is True and action["up_handled"] is True
         and action["outside_popup"] is outside
         and "SButton" in action["slate_path_types"]
         and "SObjectWidget" in action["slate_path_types"], "real Slate action")
    p, a, b = point(action["absolute_point"]), point(action["absolute_top_left"]), point(action["absolute_bottom_right"])
    need(0 <= a[0] <= p[0] <= b[0] <= 1441 and 0 <= a[1] <= p[1] <= b[1] <= 811,
         "action absolute geometry")

def check_receipt(route, expected_inputs):
    d = json.loads((RUNS / route / "manifest.json").read_text(encoding="utf-8"))
    reference = route == "fixture"
    phases = ["00_buff_popup", "01_city_manager", "02_hud_return"] if reference else ["00_buff_popup", "01_hud_return"]
    need(d["schema"] == SCHEMA and d["complete"] is True and d["passed"] is True
         and d["failures"] == [] and d["route"] == ("buff_reference" if reference else "buff")
         and d["authority"] == ("local_fixture" if reference else "server")
         and d["world"] == "/Game/Maps/L_RokUnityGameAppCity"
         and d["engine_major"] == 5 and d["engine_minor"] == 8
         and Path(d["project"]).resolve() == ROOT.resolve()
         and d["world_time_advanced_by_audit"] is False
         and d["active_buff_state_accepted"] is False
         and d["source_pixel_parity_claim"] is False,
         "receipt identity or claims")
    need(d["inputs_sha256"] == expected_inputs, "runtime source/product pin mismatch")
    need(Path(d["engine_dir"]).resolve() == Path("D:/ue/UE_5.8/Engine").resolve(), "engine directory")
    need(len(d["captures"]) == len(phases) and len(d["actions"]) == (2 if reference else 1)
         and [c["phase"] for c in d["captures"]] == phases,
         "phase/action order")
    baseline = d["baseline_business"]
    need(baseline["server_local_only"] is True and baseline["http_dispatch_attempts"] == 0
         and baseline["http_suppressed_requests"] == 0 and baseline["profile_has_role_id"] is False,
         "local-only baseline")
    for capture, phase in zip(d["captures"], phases):
        check_png(capture, phase)
        need(capture["requested_state"]["business"] == baseline
             and capture["processed_state"]["business"] == baseline
             and capture["next_frame_state"]["business"] == baseline
             and capture["requested_state"]["business_boundary"] is True
             and capture["processed_state"]["business_boundary"] is True
             and capture["next_frame_state"]["business_boundary"] is True,
             "business/HTTP/camera boundary across screenshot")
    check_open(d["captures"][0]["processed_state"], reference)
    if reference:
        check_action(d["actions"][0], "more_to_city_manager", "BuffListMoreButton", False)
        check_action(d["actions"][1], "close_city_manager_to_hud", "FeatureFrameClose", False)
        check_open(d["actions"][0]["before"], True)
        check_manager(d["actions"][0]["after"])
        check_manager(d["actions"][1]["before"])
        check_return(d["actions"][1]["after"])
        check_manager(d["captures"][1]["processed_state"])
    else:
        check_action(d["actions"][0], "outside_close", "ModalDismissMask", True)
        check_open(d["actions"][0]["before"], False)
        check_return(d["actions"][0]["after"])
    need(all(a["before"]["business"] == baseline and a["after"]["business"] == baseline
             and a["before"]["business_boundary"] is True and a["after"]["business_boundary"] is True
             for a in d["actions"]), "business/camera/HTTP boundary across input")
    check_return(d["captures"][-1]["processed_state"])
    need(d["final_state"]["business"] == baseline and d["final_state"]["business_boundary"] is True,
         "final business/camera/HTTP boundary")
    return d["pid"]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preflight", action="store_true")
    args = parser.parse_args()
    inputs = source_inputs(args.preflight)
    if args.preflight:
        print("PASS: staged audit/product source pins, native input/capture and Modal friend candidate")
        return
    a, b = check_receipt("production", inputs), check_receipt("fixture", inputs)
    need(a != b, "production and fixture must be separate native runs")
    print("PASS: production and fixture real HUD/Slate receipts, source rows/icons, canonical nonblack PNG, boundary")

if __name__ == "__main__":
    main()
