#!/usr/bin/env python3
"""Strict ActivityCalendar runtime receipt verifier. --self-test uses synthetic rejections only."""
from __future__ import annotations
import argparse
import copy
import hashlib
import io
import json
import re
from functools import lru_cache
from pathlib import Path
from rok_data_paths import resolve
from PIL import Image

ROOT = Path(__file__).resolve().parents[1] if Path(__file__).resolve().parent.name == "Scripts" else Path(__file__).resolve().parents[2]
STAGE = ROOT / "Saved/PendingActivityCalendarReadonlyRuntimeAuditV8"
SCHEMA = "rok.activity_calendar_readonly.runtime_audit.v2"
SCOPE = "Actual HUD Activity Action 100, 89 source rows, first/second/last selection and real Slate scroll; static reference only"
PHASES = ("00_city_hud", "01_activity_landing", "02_action100_reset", "03_calendar_second",
          "04_calendar_last_row", "05_calendar_last_selected", "06_hud_return")
CLICKS = (("open_activity_from_hud", "ActivityButton", 0, "/Script/Rok.RokUiAnimatedButton"),
          ("select_calendar_action100", "ActivityMenuHit_0", 100, "/Script/Rok.RokUnityIndexedButton"),
          ("select_calendar_second", None, 16001, "/Script/Rok.RokUnityIndexedButton"),
          ("select_calendar_last", None, 16088, "/Script/Rok.RokUnityIndexedButton"),
          ("close_activity_to_hud", "ActivityClose", 1, "/Script/Rok.RokUnityIndexedButton"))
CAMERA = ("camera_x", "camera_y", "camera_z", "camera_pitch", "camera_yaw", "camera_roll",
          "camera_scale_x", "camera_scale_y", "camera_scale_z")
FALSE_CLAIMS = ("world_time_advanced_by_audit", "source_pixel_parity_claim", "unity_pixel_parity_accepted",
                "full_calendar_migration_accepted", "account_activity_state_accepted", "reward_or_claim_state_accepted")
AUDIT_FILES = ("Source/Rok/RokActivityCalendarReadonlyAuditSubsystem.h",
               "Source/Rok/RokActivityCalendarReadonlyAuditSubsystem.cpp",
               "Scripts/VerifyRokActivityCalendarReadonlyRuntime.py")

def need(ok, message):
    if not ok: raise ValueError(message)

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def checked(relative):
    need(relative and not Path(relative).is_absolute() and ".." not in Path(relative).parts, "path escape")
    path = resolve(relative).resolve(strict=True)
    need(ROOT.resolve() in path.parents and path.is_file(), "input outside project")
    return path

def expected_inputs():
    report = json.loads((STAGE / "release_report.json").read_text(encoding="utf-8"))
    need(report["schema"] == "rok.activity_calendar_readonly.audit_stage.v8"
         and set(report["files"]) == set(AUDIT_FILES), "stage schema/files")
    cpp = (STAGE / "RokActivityCalendarReadonlyAuditSubsystem.cpp").read_text(encoding="utf-8")
    pins = dict(re.findall(r'\{TEXT\("([^\"]+)"\), TEXT\("([0-9a-f]{64})"\)\}', cpp))
    need(len(pins) >= 36 and pins == report["source_and_product_inputs_sha256"], "source/product pin set")
    need("if (!bSettled)" in cpp and "if (Child->IsVisible() &&" not in cpp
         and "ProcessMouseWheelOrGestureEvent" in cpp and "RequestScreenshot" in cpp
         and "ActivityCalendarReferenceHit_%d" in cpp, "native input/settled/capture gate")
    return {**pins, **{p: report["files"][p]["sha256"] for p in AUDIT_FILES}}

def source_rows():
    cal = json.loads(checked("Saved/RokUnityMigration/ConfigTables/ActivityCalendar.json").read_text(encoding="utf-8"))["rows"]
    lang = json.loads(checked("Saved/RokUnityMigration/ConfigTables/Language.json").read_text(encoding="utf-8"))["rows"]
    cn = {int(r["id"]): r["cn"] for r in lang if str(r["id"]).isdigit()}
    out = []
    for r in cal[:-1]:
        title = cn.get(int(r["l_nameID"]), "")
        if not title or "\ufffd" in title: continue
        sketch = cn.get(int(r["l_sketchID"]), "")
        if "\ufffd" in sketch: sketch = ""
        out.append({"id": int(r["ID"]), "title": title, "sketch": sketch, "icon": r["icon"]})
    need(len(cal) == 123 and len(cn) == 8067 and len(out) == 89
         and len({r["id"] for r in out}) == 89
         and sum(bool(r["sketch"]) for r in out) == 64
         and sum(bool(r["icon"]) for r in out) == 88, "source row/count projection")
    return out

@lru_cache(maxsize=1)
def mapped_icon_ids():
    product = checked("Saved/PendingActivityCalendarReadonlyCatalogV6/Source/Rok/RokUnityFeatureScreenWidget.cpp").read_text(encoding="utf-8")
    block = product.split("static constexpr FSourceSprite Sprites[] = {",1)[1].split("};",1)[0]
    ids = [int(x) for x in re.findall(r"\{(\d+),\d+,\d+,\d+,\d+\}",block)]
    need(len(ids) == len(set(ids)) == 32, "32 source atlas sprite definitions")
    return set(ids)

def uses_atlas(icon, mapped):
    match = re.fullmatch(r"activity_icon\[activity_icon_(\d+)\]",icon)
    return bool(match and int(match.group(1)) in mapped)

def point(v): return float(v["x"]), float(v["y"])
def shown(w): return w["exists"] is True and w["visible"] is True and w["enabled"] is True
def within(row, scroll):
    a,b = point(row["absolute_top_left"]), point(row["absolute_bottom_right"])
    c,d = point(scroll["absolute_top_left"]), point(scroll["absolute_bottom_right"])
    return c[0]-.5 <= a[0] < b[0] <= d[0]+.5 and c[1]-.5 <= a[1] < b[1] <= d[1]+.5

def check_claim(d, rows):
    need(d["schema"] == SCHEMA and d["complete"] is True and d["passed"] is True
         and d["failures"] == [] and d["attempts"] == [] and d["scope"] == SCOPE
         and d["baseline_phase"] == PHASES[0] and d["completion_events"] == 0
         and d["calendar_count"] == 89 and d["first_calendar_id"] == rows[0]["id"]
         and d["last_calendar_id"] == rows[-1]["id"], "run/scope/source IDs")
    need(all(d[name] is False for name in FALSE_CLAIMS), "overclaim")
    need(d["engine_major"] == 5 and d["engine_minor"] == 8
         and Path(d["project"]).resolve() == ROOT.resolve()
         and Path(d["engine_dir"]).resolve() == Path("D:/ue/UE_5.8/Engine").resolve()
         and d["world"] == "/Game/Maps/L_RokUnityGameAppCity"
         and isinstance(d["pid"], int) and d["pid"] > 0, "UE/map/PID identity")

def check_business(b):
    need("UnityInitId_5" in b["hall_tags"] and "UnityBuildingType_1" in b["hall_tags"]
         and b["hall_actor_path"] in b["city_actor_paths"], "Hall identity")
    need(b["active_upgrades"] == b["pending_completed_upgrades"] == b["active_construction"] == 0
         and b["construction_ticket_1_exists"] is False and b["completed_upgrade_events"] == 0,
         "construction not idle")
    need(b["profile_kind"] == "local_preview" and b["profile_has_role_id"] is False
         and b["profile_role_id"] == "" and b["profile_has_civilization"] is False
         and b["profile_civilization_id"] == 0 and b["server_local_only"] is True
         and b["http_dispatch_attempts"] == b["http_suppressed_requests"] == 0, "account/HTTP boundary")
    need(len(b["starter_heroes"]) == 5 and len(b["starter_troops"]) == 4
         and len(b["city_actor_paths"]) == len(set(b["city_actor_paths"])), "bootstrap snapshot")
    for name in ("wallet_food","wallet_wood","wallet_stone","wallet_gold","owned_queues",
                 "rate_food","rate_wood","rate_stone","rate_gold","task_id","task_current",
                 "task_target","task_chain","trainable_troop_ids","profile_local_hero_score",
                 "profile_local_troop_power","profile_local_preview_score"):
        need(name in b, "business field absent: " + name)

def check_settled(p):
    need(p["ready"] is True and p["structure_valid"] is True and p["direct_children"] > 5
         and p["unsettled_children"] == p["unsettled_not_visible_children"] == 0
         and 0 <= p["not_visible_children"] <= p["direct_children"]
         and p["remaining_texture_compilations"] == 0 and p["texture_count"] == 2
         and p["texture_rhi_ready"] is True and p["portrait_gate_applicable"] is False
         and p["portrait_present"] is False, "animated child/texture/RHI unsettled")

def check_state(s, phase, business, primary, selection, camera, pid, rows):
    need(s["window_foreground"] is True and s["window_owner_pid"] == s["foreground_owner_pid"] == pid
         and s["client_width"] == 1440 and s["client_height"] == 810
         and s["own_window_identity"] == s["foreground_window_identity"]
         and re.fullmatch(r"0x[0-9A-Fa-f]+", s["own_window_identity"]), "foreground/PID/extent")
    need(s["business"] == business and s["primary_press_count"] == primary
         and s["selected_actor"] == selection and s["modal_open"] is False
         and all(abs(s[k]-v) <= .001 for k,v in zip(CAMERA,camera)), "business/HTTP/camera/world input")
    w = s["widgets"]
    if phase in (PHASES[0],PHASES[-1]):
        need(s["screen_open"] is False and shown(w["ActivityButton"]), "HUD phase")
        return
    m = w["FeatureModeCanvas"]
    need(s["screen_open"] is True and shown(w["ActivityMenuHit_0"])
         and shown(w["ActivityClose"]) and m["visible"] is True
         and abs(m["opacity"]-1) <= .01 and all(abs(x-1) <= .01 for x in point(m["scale"])), "Activity frame")
    check_settled(s["activity_ui_settled"])
    if phase == PHASES[1]:
        need(s["selected_activity_index"] == 1 and s["selected_calendar_id"] == 0
             and s["pending_action_id"] == -1 and s["calendar_count"] == 89,
             "source Activity landing selection")
        need(all(w[name]["exists"] is False for name in
                 ("ActivityCalendarReferenceScroll", "ActivityCalendarReferenceScope",
                  "ActivityCalendarSelectedTitle", "ActivityCalendarSelectedSketch",
                  "ActivityCalendarSelectedIcon", "ActivityCalendarSelectedNeutralIcon"))
             and s["scroll_offset"] == s["scroll_end"] == -1
             and "all_rows" not in s and "sample_rows" not in s,
             "calendar prematurely available on Activity landing")
        return
    need(s["selected_activity_index"] == 0 and s["pending_action_id"] == -1
         and s["calendar_count"] == 89 and shown(w["ActivityCalendarReferenceScroll"])
         and w["ActivityCalendarReferenceScope"]["text"] == "活动配置资料预览 · 活动时间待服务器发布"
         and s["scroll_end"] > 9000, "source calendar panel")
    selected = 1 if phase in (PHASES[3],PHASES[4]) else 88 if phase == PHASES[5] else 0
    need(s["selected_calendar_id"] == rows[selected]["id"]
         and w["ActivityCalendarSelectedTitle"]["text"] == rows[selected]["title"], "selected source title")
    sketch = w["ActivityCalendarSelectedSketch"]
    need(sketch["exists"] is False if not rows[selected]["sketch"] else
         sketch["exists"] is True and sketch["text"] == rows[selected]["sketch"], "selected source sketch")
    need(w["ActivityCalendarSelectedIcon"]["exists"] != w["ActivityCalendarSelectedNeutralIcon"]["exists"],
         "selected icon/fallback")
    samples = s["sample_rows"]
    need(set(samples) == {str(rows[i]["id"]) for i in (0,1,88)}, "row sample IDs")
    for i in (0,1,88):
        x = samples[str(rows[i]["id"])]
        need(x["Row"]["exists"] is True and x["Title"]["text"] == rows[i]["title"]
             and x["Hit"]["exists"] is True
             and x["Icon"]["exists"] != x["NeutralIcon"]["exists"], "source row title/icon/hit")
    mapped = mapped_icon_ids()
    all_rows = s["all_rows"]
    need(len(all_rows) == 89 and s["sketch_source_count"] == 64
         and s["atlas_icon_row_count"] == sum(uses_atlas(r["icon"],mapped) for r in rows),
         "89 titles/64 optional sketches/atlas row count")
    for source, observed in zip(rows,all_rows):
        expected_icon = uses_atlas(source["icon"],mapped)
        need(observed["id"] == source["id"] and observed["source_title"] == observed["rendered_title"] == source["title"]
             and observed["source_sketch"] == source["sketch"]
             and observed["atlas_icon"] is expected_icon and observed["neutral_icon"] is not expected_icon,
             "source-derived title/sketch/icon mismatch")
    last = phase in (PHASES[4],PHASES[5])
    visible = 88 if last else 0
    need(within(samples[str(rows[visible]["id"])]["Row"], w["ActivityCalendarReferenceScroll"]), "row offscreen")
    need(abs(s["scroll_offset"]-s["scroll_end"]) <= 1 if last else s["scroll_offset"] <= 1,
         "scroll extent")

def check_click(a, spec, pid, business, primary):
    label,target,action,cls = spec
    need(a["label"] == label and (target is None or a["target"] == target)
         and a["action_id"] == action and a["target_widget_class"] == cls
         and a["method"] == "ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent", "click identity")
    need(all(a[k] is True for k in ("target_exists","exact_button_in_path","umg_path_present",
                                    "target_hovered","own_game_window","foreground","down_handled","up_handled"))
         and a["failure"] == "" and a["stable_frames"] >= 2, "real Slate click")
    i,types,nodes = a["exact_button_path_index"],a["hit_path_types"],a["hit_path_nodes"]
    need(0 <= i < len(types) == len(nodes) and "SButton" in types[i]
         and any("SObjectWidget" in t for t in types)
         and nodes[i]["slate_identity"] == a["target_slate_identity"]
         and len({n["slate_identity"] for n in nodes}) == len(nodes), "exact Slate hit path")
    need(a["own_window_pid"] == pid and a["own_window_identity"] == a["foreground_window_identity"]
         and a["business_before"] == a["business_after"] == business
         and a["primary_before"] == a["primary_after"] == primary, "click business/PID")
    center,left,right,size = map(point,(a["absolute_center"],a["absolute_top_left"],
                                       a["absolute_bottom_right"],a["local_size"]))
    need(size[0] > 0 and size[1] > 0 and left[0] < center[0] < right[0]
         and left[1] < center[1] < right[1], "click geometry")

def check_wheel(a,pid,business,primary):
    need(a["label"] == "scroll_calendar_to_last_row" and a["target"] == "ActivityCalendarReferenceScroll"
         and a["target_widget_class"] == "/Script/UMG.ScrollBox" and a["action_id"] == 0
         and a["method"] == "ProcessMouseWheelOrGestureEvent" and a["wheel_delta"] == -2048
         and a["exact_scroll_in_path"] is True and a["own_game_window"] is True
         and a["foreground"] is True and a["wheel_handled"] is True and a["own_window_pid"] == pid
         and a["failure"] == "" and a["hit_path_types"], "real Slate wheel")
    need(a["business_before"] == a["business_after"] == business
         and a["primary_before"] == a["primary_after"] == primary
         and a["scroll_before"] >= 0 and a["scroll_after"] >= a["scroll_before"], "wheel boundary")

def check_png(path,digest,count):
    raw = path.read_bytes()
    need(len(raw) == count and sha(path) == digest, "original PNG hash/bytes")
    check_png_signature(raw)
    with Image.open(io.BytesIO(raw)) as im:
        need(im.format == "PNG" and im.size == (1440,810), "original PNG dimensions")
        im.verify()
    with Image.open(io.BytesIO(raw)) as im:
        im.load()
        need(im.convert("RGB").getcolors(maxcolors=256) is None, "blank/placeholder PNG")

def check_png_signature(raw):
    need(raw[:8] == b"\x89PNG\r\n\x1a\n", "canonical PNG eight-byte signature")

def check_action_order(acts, scroll_events):
    need(6 <= len(acts) <= 101 and scroll_events == len(acts)-5
         and 1 <= scroll_events <= 96
         and tuple(a["label"] for a in acts) == tuple(x[0] for x in CLICKS[:3])
         + ("scroll_calendar_to_last_row",)*scroll_events + tuple(x[0] for x in CLICKS[3:]),
         "five real clicks and wheel order including Action 100")

def validate(d,run):
    rows = source_rows(); check_claim(d,rows)
    inputs = expected_inputs()
    need(d["inputs_sha256"] == inputs, "runtime input SHA set")
    for path,digest in inputs.items(): need(sha(checked(path)) == digest, "input bytes drift: " + path)
    h = checked("Source/Rok/RokUnityFeatureScreenWidget.h").read_text(encoding="utf-8-sig")
    need(h.count("friend class URokActivityCalendarReadonlyAuditSubsystem;") == 2
         and "friend class URokVipDailyPointHelpTipAuditSubsystem;" in h, "friend header")
    business = d["baseline_business"]; check_business(business)
    caps,acts = d["captures"],d["actions"]
    need(len(caps) == 7 and tuple(c["phase"] for c in caps) == PHASES,
         "seven ordered source phase PNG")
    check_action_order(acts,d["scroll_events"])
    first = caps[0]["requested_state"]
    primary,selection = first["primary_press_count"],first["selected_actor"]
    camera = tuple(first[k] for k in CAMERA)
    previous = -1
    for i,cap in enumerate(caps):
        frames = (cap["requested_frame"],cap["processed_frame"],cap["next_frame"])
        need(previous < frames[0] <= frames[1] < frames[2], "capture frame ordering")
        previous = frames[2]
        path = Path(cap["png"])
        need(path.is_absolute() and path.resolve(strict=True) == (run/f"{PHASES[i]}.png").resolve(strict=True)
             and path.parent.resolve() == run.resolve(), "PNG origin")
        need(cap["png_width"] == 1440 and cap["png_height"] == 810, "PNG stated dimensions")
        check_png(path,cap["png_sha256"],cap["png_bytes"])
        for key,frame in zip(("requested_state","processed_state","next_frame_state"),frames):
            need(cap[key]["frame"] == frame, "sample frame")
            check_state(cap[key],PHASES[i],business,primary,selection,camera,d["pid"],rows)
    need(all(caps[i]["png_sha256"] != caps[i+1]["png_sha256"] for i in (0,2,3,4)), "distinct UI PNG")
    for ai,lo,hi in ((0,0,1),(1,1,2),(2,2,3),(-2,4,5),(-1,5,6)):
        need(caps[lo]["next_frame"] < acts[ai]["frame"] < caps[hi]["requested_frame"], "click time")
    for a in acts[3:-2]:
        need(caps[3]["next_frame"] < a["frame"] < caps[4]["requested_frame"], "wheel time")
        check_wheel(a,d["pid"],business,primary)
    for a,spec in zip((acts[0],acts[1],acts[2],acts[-2],acts[-1]),CLICKS):
        check_click(a,spec,d["pid"],business,primary)
    need(acts[2]["target"] == f"ActivityCalendarReferenceHit_{rows[1]['id']}"
         and acts[-2]["target"] == f"ActivityCalendarReferenceHit_{rows[-1]['id']}", "selected real source IDs")
    check_state(d["final_state"],PHASES[-1],business,primary,selection,camera,d["pid"],rows)
    need(d["final_state"]["frame"] >= caps[-1]["next_frame"], "final state timing")

def self_test():
    check_png_signature(b"\x89PNG\r\n\x1a\n")
    try:check_png_signature(b"\x89PNG\r\n\x1a\r\n")
    except ValueError:signature_negative=1
    else:raise RuntimeError("old CR/LF PNG signature accepted")
    rows = source_rows()
    c = {"schema":SCHEMA,"complete":True,"passed":True,"failures":[],"attempts":[],"scope":SCOPE,
         "baseline_phase":PHASES[0],"completion_events":0,"calendar_count":89,
         "first_calendar_id":rows[0]["id"],"last_calendar_id":rows[-1]["id"],
         "engine_major":5,"engine_minor":8,"project":str(ROOT),"engine_dir":"D:/ue/UE_5.8/Engine",
         "world":"/Game/Maps/L_RokUnityGameAppCity","pid":1234,**{x:False for x in FALSE_CLAIMS}}
    p = {"ready":True,"structure_valid":True,"direct_children":12,"unsettled_children":0,
         "not_visible_children":3,"unsettled_not_visible_children":0,"remaining_texture_compilations":0,
         "texture_count":2,"texture_rhi_ready":True,"portrait_gate_applicable":False,"portrait_present":False}
    check_claim(c,rows); check_settled(p)
    cases = signature_negative
    fixture = json.loads((STAGE / "landing_phase_fixture.json").read_text(encoding="utf-8"))
    source_manifest = checked(fixture["source_manifest"])
    need(sha(source_manifest) == fixture["source_manifest_sha256"], "landing fixture source drift")
    landing = fixture["state"]
    need(landing == json.loads(source_manifest.read_text(encoding="utf-8"))["final_state"],
         "landing fixture differs from observed Runtime10 state")
    camera = tuple(landing[k] for k in CAMERA)
    check_state(landing,PHASES[1],landing["business"],landing["primary_press_count"],
                landing["selected_actor"],camera,landing["window_owner_pid"],rows)
    for key,bad in (("selected_activity_index",0),("selected_calendar_id",rows[0]["id"]),
                    ("pending_action_id",100),("scroll_offset",0)):
        x=copy.deepcopy(landing);x[key]=bad
        try:check_state(x,PHASES[1],landing["business"],landing["primary_press_count"],
                        landing["selected_actor"],camera,landing["window_owner_pid"],rows)
        except (ValueError,KeyError,TypeError):cases+=1
        else:raise RuntimeError("landing negative accepted: "+key)
    for name in ("ActivityCalendarReferenceScroll","ActivityCalendarReferenceScope",
                 "ActivityCalendarSelectedTitle","ActivityCalendarSelectedSketch",
                 "ActivityCalendarSelectedIcon","ActivityCalendarSelectedNeutralIcon"):
        x=copy.deepcopy(landing);x["widgets"][name]["exists"]=True
        try:check_state(x,PHASES[1],landing["business"],landing["primary_press_count"],
                        landing["selected_actor"],camera,landing["window_owner_pid"],rows)
        except (ValueError,KeyError,TypeError):cases+=1
        else:raise RuntimeError("premature calendar widget accepted: "+name)
    labels=[x[0] for x in CLICKS[:3]]+["scroll_calendar_to_last_row"]+[x[0] for x in CLICKS[3:]]
    check_action_order([{"label":x} for x in labels],1)
    for wrong in (labels[:1]+labels[2:], labels[:1]+["select_calendar_action101"]+labels[2:]):
        try:check_action_order([{"label":x} for x in wrong],1)
        except (ValueError,KeyError,TypeError):cases+=1
        else:raise RuntimeError("skipped or misrouted Action 100 accepted")
    for k,bad in (("complete",False),("passed",False),("failures",["x"]),("attempts",[{}]),
                  ("calendar_count",88),("first_calendar_id",0),("last_calendar_id",0),
                  ("engine_minor",7),("pid",0),("completion_events",1),("world","/Game/Other"),
                  ("account_activity_state_accepted",True),("reward_or_claim_state_accepted",True)):
        x=copy.deepcopy(c);x[k]=bad
        try:check_claim(x,rows)
        except (ValueError,KeyError,TypeError):cases+=1
        else:raise RuntimeError("claim negative accepted: "+k)
    for k,bad in (("ready",False),("structure_valid",False),("direct_children",0),
                  ("unsettled_children",1),("unsettled_not_visible_children",1),
                  ("remaining_texture_compilations",1),("texture_count",1),("texture_rhi_ready",False),
                  ("portrait_gate_applicable",True),("portrait_present",True)):
        x=copy.deepcopy(p);x[k]=bad
        try:check_settled(x)
        except (ValueError,KeyError,TypeError):cases+=1
        else:raise RuntimeError("settled negative accepted: "+k)
    for bad in ("","../escape","/absolute/escape"):
        try:checked(bad)
        except (ValueError,FileNotFoundError,IsADirectoryError):cases+=1
        else:raise RuntimeError("path negative accepted: "+bad)
    scroll={"absolute_top_left":{"x":100,"y":100},"absolute_bottom_right":{"x":1000,"y":700}}
    row={"absolute_top_left":{"x":110,"y":120},"absolute_bottom_right":{"x":900,"y":240}}
    need(within(row,scroll),"geometry fixture")
    for bad in (700,701,1000):
        x=copy.deepcopy(row);x["absolute_top_left"]["y"]=bad
        try:need(within(x,scroll),"offscreen row")
        except ValueError:cases+=1
        else:raise RuntimeError("geometry negative accepted")
    click={"label":"select_calendar_action100","target":"ActivityMenuHit_0","action_id":100,
           "target_widget_class":"/Script/Rok.RokUnityIndexedButton",
           "method":"ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent",
           **{k:True for k in ("target_exists","exact_button_in_path","umg_path_present",
                                  "target_hovered","own_game_window","foreground","down_handled","up_handled")},
           "failure":"","stable_frames":2,"exact_button_path_index":1,
           "hit_path_types":["SObjectWidget","SButton"],
           "hit_path_nodes":[{"slate_identity":"0x1"},{"slate_identity":"0x2"}],
           "target_slate_identity":"0x2","own_window_pid":1234,
           "own_window_identity":"0x10","foreground_window_identity":"0x10",
           "business_before":{"http_dispatch_attempts":0},"business_after":{"http_dispatch_attempts":0},
           "primary_before":3,"primary_after":3,
           "absolute_center":{"x":150,"y":150},"absolute_top_left":{"x":100,"y":100},
           "absolute_bottom_right":{"x":200,"y":200},"local_size":{"x":100,"y":100}}
    check_click(click,CLICKS[1],1234,click["business_before"],3)
    for k,bad in (("action_id",101),("target","ActivityMenuHit_1"),("exact_button_in_path",False),
                  ("down_handled",False),("target_slate_identity","0x3"),("foreground",False),
                  ("own_window_pid",987),("primary_after",4),("business_after",{"http_dispatch_attempts":1})):
        x=copy.deepcopy(click);x[k]=bad
        try:check_click(x,CLICKS[1],1234,click["business_before"],3)
        except (ValueError,KeyError,TypeError):cases+=1
        else:raise RuntimeError("click negative accepted: "+k)
    wheel={"label":"scroll_calendar_to_last_row","target":"ActivityCalendarReferenceScroll",
           "target_widget_class":"/Script/UMG.ScrollBox","action_id":0,
           "method":"ProcessMouseWheelOrGestureEvent","wheel_delta":-2048,
           "exact_scroll_in_path":True,"own_game_window":True,"foreground":True,
           "wheel_handled":True,"own_window_pid":1234,"failure":"","hit_path_types":["SScrollBox"],
           "business_before":{"http_dispatch_attempts":0},"business_after":{"http_dispatch_attempts":0},
           "primary_before":3,"primary_after":3,"scroll_before":0,"scroll_after":100}
    check_wheel(wheel,1234,wheel["business_before"],3)
    for k,bad in (("wheel_handled",False),("exact_scroll_in_path",False),("foreground",False),
                  ("scroll_after",-1),("business_after",{"http_dispatch_attempts":1}),
                  ("primary_after",4),("own_window_pid",1235)):
        x=copy.deepcopy(wheel);x[k]=bad
        try:check_wheel(x,1234,wheel["business_before"],3)
        except (ValueError,KeyError,TypeError):cases+=1
        else:raise RuntimeError("wheel negative accepted: "+k)
    mapped = mapped_icon_ids()
    need(len(mapped) == 32 and uses_atlas("activity_icon[activity_icon_1003]",mapped),"atlas mapping fixture")
    for icon in ("activity_icon[activity_icon_9999]","activity_icon_1003",""):
        try:need(uses_atlas(icon,mapped),"wrong icon accepted")
        except ValueError:cases+=1
        else:raise RuntimeError("icon negative accepted")
    expected_inputs()
    return cases

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--self-test",action="store_true")
    parser.add_argument("--manifest",type=Path);args=parser.parse_args()
    need(args.self_test != bool(args.manifest),"choose one mode")
    if args.self_test:
        print(json.dumps({"synthetic_negative_cases":self_test(),"actual_runtime_verified":False}));return
    manifest=args.manifest.resolve(strict=True)
    validate(json.loads(manifest.read_text(encoding="utf-8")),manifest.parent)
    print(json.dumps({"actual_runtime_verified":True,"png_count":7,"calendar_rows":89}))

if __name__=="__main__":main()
