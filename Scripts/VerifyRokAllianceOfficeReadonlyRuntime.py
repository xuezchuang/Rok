#!/usr/bin/env python3
"""Strict Alliance Office actual-Slate receipt verifier; self-test is synthetic only."""
from __future__ import annotations
import argparse, copy, hashlib, io, json, re
from pathlib import Path
from PIL import Image

SELF = Path(__file__).resolve()
ROOT = SELF.parents[3] if SELF.parents[1].name == "PendingAllianceOfficeRowLayoutPolishRuntimeAuditV4" else SELF.parents[1]
STAGE = ROOT / "Saved/PendingAllianceOfficeRowLayoutPolishRuntimeAuditV4"
SCHEMA = "rok.alliance_office_row_layout_polish.runtime_audit.v4"
SCOPE = "Actual HUD Alliance welcome to four-definition reference and measured row layout, close/reopen; anonymous config only"
PHASES = ("00_city_hud", "01_alliance_welcome", "02_office_reference", "03_office_2001",
          "04_office_3001", "05_office_4001", "06_welcome_return", "07_hud_return",
          "08_alliance_reopened", "09_office_reopened", "10_office_1001_after_reopen", "11_final_hud")
CLICKS = (("open_alliance_from_hud", "AllianceFeatureButton", 0, "/Script/Rok.RokUiAnimatedButton"),
          ("open_office_reference", "AllianceOfficeReferenceOpen", 633, "/Script/Rok.RokUnityIndexedButton"),
          ("select_office_2001", "AllianceOfficeReferenceHit_2001", 641, "/Script/Rok.RokUnityIndexedButton"),
          ("select_office_3001", "AllianceOfficeReferenceHit_3001", 642, "/Script/Rok.RokUnityIndexedButton"),
          ("select_office_4001", "AllianceOfficeReferenceHit_4001", 643, "/Script/Rok.RokUnityIndexedButton"),
          ("back_to_alliance_welcome", "AllianceOfficeReferenceBack", 634, "/Script/Rok.RokUnityIndexedButton"),
          ("close_alliance_to_hud", "FeatureFrameClose", 1, "/Script/Rok.RokUnityIndexedButton"),
          ("reopen_alliance_from_hud", "AllianceFeatureButton", 0, "/Script/Rok.RokUiAnimatedButton"),
          ("reopen_office_reference", "AllianceOfficeReferenceOpen", 633, "/Script/Rok.RokUnityIndexedButton"),
          ("select_office_1001_after_reopen", "AllianceOfficeReferenceHit_1001", 640, "/Script/Rok.RokUnityIndexedButton"),
          ("final_close_alliance_to_hud", "FeatureFrameClose", 1, "/Script/Rok.RokUnityIndexedButton"))
CAMERA = ("camera_x", "camera_y", "camera_z", "camera_pitch", "camera_yaw", "camera_roll",
          "camera_scale_x", "camera_scale_y", "camera_scale_z")
FALSE_CLAIMS = ("world_time_advanced_by_audit", "source_pixel_parity_claim", "unity_pixel_parity_accepted",
                "alliance_membership_accepted", "office_appointment_or_bonus_accepted", "account_alliance_state_accepted")
FILES = ("Source/Rok/RokAllianceOfficeReadonlyAuditSubsystem.h",
         "Source/Rok/RokAllianceOfficeReadonlyAuditSubsystem.cpp",
         "Scripts/VerifyRokAllianceOfficeReadonlyRuntime.py")
IDS = (1001, 2001, 3001, 4001)
INTRO = "仅展示配置定义，不代表当前任命、人数、权限或已生效加成；选择不会改变账号状态。"

def need(ok, message):
    if not ok: raise ValueError(message)
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def checked(relative):
    need(relative and not Path(relative).is_absolute() and ".." not in Path(relative).parts, "path escape")
    p=(ROOT/relative).resolve(strict=True)
    need(ROOT.resolve() in p.parents and p.is_file(), "input outside project")
    return p
def point(v): return float(v["x"]),float(v["y"])
def shown(w): return w["exists"] is True and w["visible"] is True and w["enabled"] is True
def native_run(manifest,pid,require_file=True):
    need(type(pid) is int and pid>0 and manifest.is_absolute() and manifest.name=="manifest.json",
         "native manifest identity")
    native_root=(STAGE/"RuntimeAudit").absolute()
    run=manifest.parent
    match=re.fullmatch(r"([1-9][0-9]{14,18})_([1-9][0-9]*)",run.name)
    need(run.parent==native_root and match is not None and int(match.group(2))==pid
         and 630822816000000000<=int(match.group(1))<662380416000000000,
         "native output root/ticks/PID")
    if require_file:
        need(native_root.is_dir() and run.is_dir() and manifest.is_file()
             and not native_root.is_symlink() and not run.is_symlink() and not manifest.is_symlink()
             and run.parent.resolve(strict=True)==native_root.resolve(strict=True)
             and manifest.resolve(strict=True).parent==run.resolve(strict=True),
             "native manifest actual directory/symlink")
    return run
def within(outer,inner,tol=1.0):
    a,b=point(outer["absolute_top_left"]),point(outer["absolute_bottom_right"])
    c,d=point(inner["absolute_top_left"]),point(inner["absolute_bottom_right"])
    return a[0]<b[0] and a[1]<b[1] and c[0]<d[0] and c[1]<d[1] and a[0]-tol<=c[0] and a[1]-tol<=c[1] and d[0]<=b[0]+tol and d[1]<=b[1]+tol

def check_layout(observed,back):
    need(len(observed)==4 and "canvas_slot" in back,"layout rows/back slot")
    back_top=point(back["absolute_top_left"])[1]
    previous_bottom=float("-inf")
    for row in observed:
        bg,hit,bonus=(row[k] for k in ("Row","Hit","Bonus"))
        for w in (bg,hit,bonus):
            slot=w["canvas_slot"]
            need(all(v>0 for v in point(slot["size"])) and all(v>0 for v in point(w["local_size"])),"positive row/slot geometry")
        bgslot,hitslot,bonusslot=(w["canvas_slot"] for w in (bg,hit,bonus))
        need(all(abs(a-b)<=.25 for a,b in zip(point(bgslot["position"]),point(hitslot["position"])))
             and all(abs(a-b)<=.25 for a,b in zip(point(bgslot["size"]),point(hitslot["size"])))
             and within(bg,hit) and within(hit,bg) and within(bg,bonus) and within(hit,bonus),"row/hit/bonus actual bounds")
        bp,bs=point(bgslot["position"]),point(bgslot["size"])
        tp,ts=point(bonusslot["position"]),point(bonusslot["size"])
        need(bp[0]<=tp[0] and bp[1]<=tp[1] and tp[0]+ts[0]<=bp[0]+bs[0]+.25
             and tp[1]+ts[1]<=bp[1]+bs[1]+.25,"bonus canvas slot contained")
        desired=point(bonus["desired_size"])
        need(desired[0]>0 and desired[1]>0 and desired[0]<=ts[0]+1 and desired[1]<=ts[1]+1,
             "wrapped bonus desired size contained")
        top,bottom=point(bg["absolute_top_left"])[1],point(bg["absolute_bottom_right"])[1]
        need(top>=previous_bottom+2 and bottom+8<=back_top,"rows disjoint and return button clear")
        previous_bottom=bottom

def expected_inputs():
    report=json.loads((STAGE/"release_report.json").read_text(encoding="utf-8"))
    need(report["schema"] == "rok.alliance_office_row_layout_polish.audit_stage.v4" and set(report["files"])==set(FILES),"stage schema/files")
    cpp=(STAGE/FILES[1]).read_text(encoding="utf-8")
    pins=dict(re.findall(r'\{TEXT\("([^\"]+)"\), TEXT\("([0-9a-f]{64})"\)\}',cpp))
    need(len(pins)==53 and pins==report["source_and_product_inputs_sha256"],"source/product pins")
    product_cpp=checked("Saved/PendingAllianceOfficeRowLayoutPolishV3/Source/Rok/RokUnityFeatureScreenWidget.cpp").read_text(encoding="utf-8-sig")
    open_alliance=product_cpp.split("void URokUnityFeatureScreenWidget::OpenAlliance(",1)[1].split("\n}",1)[0]
    need("SelectedAllianceOfficeIndex = 0;" in open_alliance,"reopen flow must reset office selection to index zero")
    need("if (!bSettled)" in cpp and "if (Child->IsVisible() &&" not in cpp
         and "ProcessMouseButtonDownEvent" in cpp and "RequestScreenshot" in cpp
         and "AllianceOfficeReferenceHit_1001" in cpp and "AllianceOfficeReferenceBack" in cpp
         and "OfficeReferenceLayoutValid(Screen.Get(), OfficeIds)" in cpp,"native gate")
    return {**pins,**{p:report["files"][p]["sha256"] for p in FILES}}

def source_rows():
    raw=json.loads(checked("Saved/RokUnityMigration/ConfigTables/AllianceOfficially.json").read_text(encoding="utf-8"))["rows"]
    language=json.loads(checked("Saved/RokUnityMigration/ConfigTables/Language.json").read_text(encoding="utf-8"))["rows"]
    cn={int(x["id"]):x["cn"] for x in language}
    need(len(raw)==4 and len(cn)==8067,"source table counts")
    out=[]
    for x in raw:
        rid=int(x["ID"]);need(rid in IDS,"office ID")
        locale=tuple(int(x[k]) for k in ("l_officiallyID","l_desID","l_addDesID"))
        name,desc,bonus=(cn[k] for k in locale)
        bonus=bonus.replace("<color=green>","").replace("</color>","")
        for i,value in enumerate(x["addDesData"].split("|")):
            need(value.isdigit() and "{"+str(i)+"}" in bonus,"bonus source values")
            bonus=bonus.replace("{"+str(i)+"}",value)
        need(all(s and "\ufffd" not in s for s in (name,desc,bonus)) and "{" not in bonus and "}" not in bonus,"unresolved locale")
        icon=f"ui_alliance[icon_ali_achi{rid//1000}]"
        need(x["icon"]==icon,"atlas icon source")
        out.append(dict(id=rid,name_locale_id=locale[0],description_locale_id=locale[1],bonus_locale_id=locale[2],
                        name=name,description=desc,bonus=bonus,icon=icon,sprite=f"icon_ali_achi{rid//1000}"))
    out.sort(key=lambda r:r["id"])
    need(tuple(r["id"] for r in out)==IDS,"source IDs/order")
    return out

def check_claim(d):
    need(d["schema"]==SCHEMA and d["complete"] is True and d["passed"] is True
         and d["failures"]==[] and d["attempts"]==[] and d["scope"]==SCOPE
         and d["baseline_phase"]==PHASES[0] and d["completion_events"]==0 and d["office_count"]==4,"claim/scope")
    need(all(d[k] is False for k in FALSE_CLAIMS),"unsupported acceptance claim")
    need(d["engine_major"]==5 and d["engine_minor"]==8 and Path(d["project"]).resolve()==ROOT.resolve()
         and Path(d["engine_dir"]).resolve()==Path("D:/ue/UE_5.8/Engine").resolve()
         and d["world"]=="/Game/Maps/L_RokUnityGameAppCity" and isinstance(d["pid"],int) and d["pid"]>0,"UE/map/PID")

def check_business(b):
    need("UnityInitId_5" in b["hall_tags"] and "UnityBuildingType_1" in b["hall_tags"]
         and b["hall_actor_path"] in b["city_actor_paths"],"Hall identity")
    need(b["active_upgrades"]==b["pending_completed_upgrades"]==b["active_construction"]==0
         and b["construction_ticket_1_exists"] is False and b["completed_upgrade_events"]==0,"construction idle")
    need(b["profile_kind"]=="local_preview" and b["profile_has_role_id"] is False and b["profile_role_id"]==""
         and b["profile_has_civilization"] is False and b["profile_civilization_id"]==0
         and b["server_local_only"] is True and b["http_dispatch_attempts"]==b["http_suppressed_requests"]==0,"account/HTTP")
    need(len(b["starter_heroes"])==5 and len(b["starter_troops"])==4
         and len(b["city_actor_paths"])==len(set(b["city_actor_paths"])),"bootstrap snapshot")
    for k in ("wallet_food","wallet_wood","wallet_stone","wallet_gold","owned_queues","rate_food","rate_wood",
              "rate_stone","rate_gold","task_id","task_current","task_target","task_chain","trainable_troop_ids",
              "profile_local_hero_score","profile_local_troop_power","profile_local_preview_score"):
        need(k in b,"business field missing: "+k)

def check_settled(p):
    need(p["ready"] is True and p["structure_valid"] is True and p["direct_children"]>5
         and p["unsettled_children"]==p["unsettled_not_visible_children"]==0
         and 0<=p["not_visible_children"]<=p["direct_children"]
         and p["remaining_texture_compilations"]==0 and p["texture_count"]==4
         and p["texture_rhi_ready"] is True and p["portrait_gate_applicable"] is False
         and p["portrait_present"] is False,"child animation/atlas RHI")

def check_state(s,phase,business,primary,selection,camera,pid,rows):
    need(s["window_foreground"] is True and s["window_owner_pid"]==s["foreground_owner_pid"]==pid
         and s["client_width"]==1440 and s["client_height"]==810
         and s["own_window_identity"]==s["foreground_window_identity"]
         and re.fullmatch(r"0x[0-9A-Fa-f]+",s["own_window_identity"]),"foreground/PID/extent")
    need(s["business"]==business and s["primary_press_count"]==primary and s["selected_actor"]==selection
         and s["modal_open"] is False and all(abs(s[k]-v)<=.001 for k,v in zip(CAMERA,camera)),"business/HTTP/camera")
    w=s["widgets"]
    if phase in (PHASES[0],PHASES[7],PHASES[11]):
        need(s["screen_open"] is False and shown(w["AllianceFeatureButton"]),"HUD phase");return
    mode=w["FeatureModeCanvas"]
    need(s["screen_open"] is True and shown(w["FeatureFrameClose"]) and shown(w["FeatureFrameTitle"])
         and mode["visible"] is True
         and abs(mode["opacity"]-1)<=.01 and all(abs(v-1)<=.01 for v in point(mode["scale"])),"Alliance frame")
    check_settled(s["alliance_ui_settled"])
    need(s["active_mode"]==6 and s["alliance_preview_state"]==s["selected_alliance_page"]==0
         and s["pending_action_id"]==-1,"no joined/fixture action")
    if phase in (PHASES[1],PHASES[6],PHASES[8]):
        need(s["office_reference_open"] is False and w["FeatureFrameTitle"]["text"]=="联盟"
             and shown(w["AllianceOfficeReferenceOpen"])
             and w["AllianceOfficeReferenceBack"]["exists"] is False,"welcome entry");return
    need(s["office_reference_open"] is True and w["FeatureFrameTitle"]["text"]=="官职配置参考"
         and shown(w["AllianceOfficeReferenceBack"])
         and w["AllianceOfficeReferenceIntro"]["text"]==INTRO
         and w["AllianceOfficeReferenceUnavailable"]["exists"] is False and s["office_count"]==4,"reference scope")
    obs=s["office_rows"];need(len(obs)==4,"four office rows")
    for source,actual in zip(rows,obs):
        need(actual["id"]==source["id"] and actual["name_locale_id"]==source["name_locale_id"]
             and actual["description_locale_id"]==source["description_locale_id"]
             and actual["bonus_locale_id"]==source["bonus_locale_id"]
             and actual["source_name"]==source["name"] and actual["source_description"]==source["description"]
             and actual["source_bonus"]==source["bonus"] and actual["source_icon"]==source["icon"]
             and actual["icon_sprite"]==source["sprite"],"source config/locale")
        need(all(shown(actual[k]) for k in ("Row","Name","Description","Bonus","Icon","Hit"))
             and actual["Name"]["text"]==f"#{source['id']} · {source['name']}"
             and actual["Description"]["text"]==source["description"]
             and actual["Bonus"]["text"]=="配置加成说明："+source["bonus"],"rendered row/icon")
    check_layout(obs,w["AllianceOfficeReferenceBack"])
    expected={PHASES[2]:0,PHASES[3]:1,PHASES[4]:2,PHASES[5]:3,PHASES[9]:0,PHASES[10]:0}[phase]
    need(s["selected_office_index"]==expected,"selected definition")

def check_click(a,spec,pid,business,primary):
    label,target,action,cls=spec
    need(a["label"]==label and a["target"]==target and a["action_id"]==action
         and a["target_widget_class"]==cls
         and a["method"]=="ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent","click identity")
    need(all(a[k] is True for k in ("target_exists","exact_button_in_path","umg_path_present","target_hovered",
                                    "own_game_window","foreground","down_handled","up_handled"))
         and a["failure"]=="" and a["stable_frames"]>=2,"real Slate click")
    i,types,nodes=a["exact_button_path_index"],a["hit_path_types"],a["hit_path_nodes"]
    need(0<=i<len(types)==len(nodes) and "SButton" in types[i] and any("SObjectWidget" in t for t in types)
         and nodes[i]["slate_identity"]==a["target_slate_identity"]
         and len({n["slate_identity"] for n in nodes})==len(nodes),"exact Slate hit path")
    need(a["own_window_pid"]==pid and a["own_window_identity"]==a["foreground_window_identity"]
         and a["business_before"]==a["business_after"]==business
         and a["primary_before"]==a["primary_after"]==primary,"click business/PID")
    center,left,right,size=map(point,(a["absolute_center"],a["absolute_top_left"],a["absolute_bottom_right"],a["local_size"]))
    need(size[0]>0 and size[1]>0 and left[0]<center[0]<right[0] and left[1]<center[1]<right[1],"click geometry")

def check_png_raw(raw,digest,count):
    need(len(raw)>=8 and raw[:8]==b"\x89PNG\r\n\x1a\n", "PNG canonical signature")
    need(len(raw)==count and hashlib.sha256(raw).hexdigest()==digest,"PNG hash/bytes")
    with Image.open(io.BytesIO(raw)) as im:
        need(im.format=="PNG" and im.size==(1440,810),"original PNG dimensions");im.verify()
    with Image.open(io.BytesIO(raw)) as im:
        im.load();need(im.convert("RGB").getcolors(maxcolors=256) is None,"blank/placeholder PNG")
def check_png(path,digest,count):
    check_png_raw(path.read_bytes(),digest,count)

def validate(d,manifest_path):
    rows=source_rows();check_claim(d)
    run=native_run(manifest_path,d["pid"])
    inputs=expected_inputs();need(d["inputs_sha256"]==inputs,"runtime input SHA set")
    for path,digest in inputs.items():need(sha(checked(path))==digest,"input drift: "+path)
    h=checked("Source/Rok/RokUnityFeatureScreenWidget.h").read_text(encoding="utf-8-sig")
    need(h.count("friend class URokAllianceOfficeReadonlyAuditSubsystem;")==2
         and all("friend class "+x+";" in h for x in (
             "URokCommanderSkillReadonlyDetailAuditSubsystem","URokDailyTaskReadonlyCatalogAuditSubsystem",
             "URokVipDailyPointHelpTipAuditSubsystem","URokActivityCalendarReadonlyAuditSubsystem",
             "URokAllianceTechnologyReadonlyAuditSubsystem","URokTaskCompletedRowAnimationAuditSubsystem")),"friend preservation")
    business=d["baseline_business"];check_business(business)
    caps,acts=d["captures"],d["actions"]
    need(len(caps)==12 and tuple(c["phase"] for c in caps)==PHASES
         and len(acts)==11 and tuple(a["label"] for a in acts)==tuple(x[0] for x in CLICKS),"12 PNG/11 click order")
    first=caps[0]["requested_state"]
    primary,selection=first["primary_press_count"],first["selected_actor"]
    camera=tuple(first[k] for k in CAMERA);previous=-1
    for i,cap in enumerate(caps):
        frames=(cap["requested_frame"],cap["processed_frame"],cap["next_frame"])
        need(previous<frames[0]<=frames[1]<frames[2],"capture frame ordering");previous=frames[2]
        path=Path(cap["png"])
        need(path.is_absolute() and path==(run/f"{PHASES[i]}.png") and path.is_file()
             and not path.is_symlink() and path.resolve(strict=True).parent==run.resolve(strict=True),"PNG origin")
        need(cap["png_width"]==1440 and cap["png_height"]==810,"PNG stated dimensions")
        check_png(path,cap["png_sha256"],cap["png_bytes"])
        for key,frame in zip(("requested_state","processed_state","next_frame_state"),frames):
            need(cap[key]["frame"]==frame,"sample frame")
            check_state(cap[key],PHASES[i],business,primary,selection,camera,d["pid"],rows)
    need(all(caps[i]["png_sha256"]!=caps[i+1]["png_sha256"] for i in range(11)),"distinct UI PNG")
    for i,(action,spec) in enumerate(zip(acts,CLICKS)):
        need(caps[i]["next_frame"]<action["frame"]<caps[i+1]["requested_frame"],"click time")
        check_click(action,spec,d["pid"],business,primary)
    check_state(d["final_state"],PHASES[-1],business,primary,selection,camera,d["pid"],rows)
    need(d["final_state"]["frame"]>=caps[-1]["next_frame"],"final time")

def reject(fn,label):
    try:fn()
    except (ValueError,KeyError,TypeError,OSError,SyntaxError):return
    raise RuntimeError("negative accepted: "+label)

def self_test():
    native=(STAGE/"RuntimeAudit"/"639271800000000000_1234"/"manifest.json").absolute()
    need(native_run(native,1234,False)==native.parent,"native path positive fixture")
    origin_cases=0
    for label,bad,pid in (
        ("external manifest",ROOT/"Saved/OtherRuntime/639271800000000000_1234/manifest.json",1234),
        ("wrong PID",native,5678),
        ("wrong ticks",STAGE/"RuntimeAudit/999_1234/manifest.json",1234),
        ("zero ticks",STAGE/"RuntimeAudit/000_1234/manifest.json",1234),
        ("nested output",STAGE/"RuntimeAudit/extra/639271800000000000_1234/manifest.json",1234),
        ("wrong basename",STAGE/"RuntimeAudit/639271800000000000_1234/old.json",1234)):
        reject(lambda bad=bad,pid=pid:native_run(bad.absolute(),pid,False),label);origin_cases+=1
    rows=source_rows()
    d={"schema":SCHEMA,"complete":True,"passed":True,"failures":[],"attempts":[],"scope":SCOPE,
       "baseline_phase":PHASES[0],"completion_events":0,"office_count":4,"engine_major":5,"engine_minor":8,
       "project":str(ROOT),"engine_dir":"D:/ue/UE_5.8/Engine","world":"/Game/Maps/L_RokUnityGameAppCity",
       "pid":1234,**{k:False for k in FALSE_CLAIMS}}
    p={"ready":True,"structure_valid":True,"direct_children":12,"unsettled_children":0,
       "not_visible_children":3,"unsettled_not_visible_children":0,"remaining_texture_compilations":0,
       "texture_count":4,"texture_rhi_ready":True,"portrait_gate_applicable":False,"portrait_present":False}
    check_claim(d);check_settled(p);cases=origin_cases
    for k,bad in (("complete",False),("passed",False),("failures",["x"]),("attempts",[{}]),
                  ("office_count",3),("engine_minor",7),("pid",0),("completion_events",1),
                  ("world","/Game/Other"),("alliance_membership_accepted",True),
                  ("office_appointment_or_bonus_accepted",True),("account_alliance_state_accepted",True)):
        x=copy.deepcopy(d);x[k]=bad;reject(lambda x=x:check_claim(x),"claim "+k);cases+=1
    for k,bad in (("ready",False),("structure_valid",False),("direct_children",0),
                  ("unsettled_children",1),("unsettled_not_visible_children",1),
                  ("remaining_texture_compilations",1),("texture_count",3),("texture_rhi_ready",False),
                  ("portrait_gate_applicable",True),("portrait_present",True)):
        x=copy.deepcopy(p);x[k]=bad;reject(lambda x=x:check_settled(x),"settled "+k);cases+=1
    for bad in ("","../escape","/absolute/escape"):
        reject(lambda bad=bad:checked(bad),"path "+bad);cases+=1
    click={"label":CLICKS[1][0],"target":CLICKS[1][1],"action_id":633,"target_widget_class":CLICKS[1][3],
           "method":"ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent",
           **{k:True for k in ("target_exists","exact_button_in_path","umg_path_present","target_hovered",
                                  "own_game_window","foreground","down_handled","up_handled")},
           "failure":"","stable_frames":2,"exact_button_path_index":1,"hit_path_types":["SObjectWidget","SButton"],
           "hit_path_nodes":[{"slate_identity":"0x1"},{"slate_identity":"0x2"}],"target_slate_identity":"0x2",
           "own_window_pid":1234,"own_window_identity":"0x10","foreground_window_identity":"0x10",
           "business_before":{"http_dispatch_attempts":0},"business_after":{"http_dispatch_attempts":0},
           "primary_before":3,"primary_after":3,"absolute_center":{"x":150,"y":150},
           "absolute_top_left":{"x":100,"y":100},"absolute_bottom_right":{"x":200,"y":200},"local_size":{"x":100,"y":100}}
    check_click(click,CLICKS[1],1234,click["business_before"],3)
    for k,bad in (("action_id",634),("target","AllianceWelcomeJoin"),("exact_button_in_path",False),
                  ("down_handled",False),("target_slate_identity","0x3"),("foreground",False),
                  ("own_window_pid",987),("primary_after",4),("business_after",{"http_dispatch_attempts":1})):
        x=copy.deepcopy(click);x[k]=bad
        reject(lambda x=x:check_click(x,CLICKS[1],1234,click["business_before"],3),"click "+k);cases+=1
    def xy(x,y): return {"x":x,"y":y}
    def widget(text="",exists=True,x=0,y=0,width=1,height=1,desired=None):
        return {"exists":exists,"visible":exists,"enabled":exists,"text":text,"opacity":1,
                "scale":xy(1,1),"local_size":xy(width,height),"desired_size":xy(*(desired or (width,height))),
                "absolute_top_left":xy(x,y),"absolute_bottom_right":xy(x+width,y+height),
                "canvas_slot":{"position":xy(x,y),"size":xy(width,height)}}
    observed=[]
    for index,row in enumerate(rows):
        y=235+112*index
        observed.append({"id":row["id"],"name_locale_id":row["name_locale_id"],
                         "description_locale_id":row["description_locale_id"],"bonus_locale_id":row["bonus_locale_id"],
                         "source_name":row["name"],"source_description":row["description"],
                         "source_bonus":row["bonus"],"source_icon":row["icon"],"icon_sprite":row["sprite"],
                         "Row":widget(x=198,y=y,width=1040,height=108),
                         "Name":widget(f"#{row['id']} · {row['name']}"),
                         "Description":widget(row["description"]),
                         "Bonus":widget("配置加成说明："+row["bonus"],x=302,y=y+66,width=870,height=42,desired=(800,32)),
                         "Icon":widget(),"Hit":widget(x=198,y=y,width=1040,height=108)})
    state={"window_foreground":True,"window_owner_pid":1234,"foreground_owner_pid":1234,
           "client_width":1440,"client_height":810,"own_window_identity":"0x10","foreground_window_identity":"0x10",
           "business":{"http_dispatch_attempts":0},"primary_press_count":3,"selected_actor":"",
           "modal_open":False,"screen_open":True,"alliance_ui_settled":p,
           "active_mode":6,"alliance_preview_state":0,"selected_alliance_page":0,"pending_action_id":-1,
           "office_reference_open":True,"office_count":4,"office_rows":observed,"selected_office_index":0,
           "widgets":{"FeatureModeCanvas":widget(),"FeatureFrameClose":widget(),
                      "FeatureFrameTitle":widget("官职配置参考"),
                      "AllianceOfficeReferenceBack":widget(x=208,y=735,width=138,height=54),"AllianceOfficeReferenceIntro":widget(INTRO),
                      "AllianceOfficeReferenceUnavailable":widget(exists=False)}}
    for k in CAMERA:state[k]=0
    check_state(state,PHASES[2],state["business"],3,"",(0,)*len(CAMERA),1234,rows)
    reopened=copy.deepcopy(state);reopened["selected_office_index"]=0
    check_state(reopened,PHASES[9],state["business"],3,"",(0,)*len(CAMERA),1234,rows)
    wrong_reopen=copy.deepcopy(reopened);wrong_reopen["selected_office_index"]=3
    reject(lambda:check_state(wrong_reopen,PHASES[9],state["business"],3,"",(0,)*len(CAMERA),1234,rows),"reopen reset must reject stale index 3");cases+=1
    mutants=(("selected_office_index",1),("active_mode",5),("alliance_preview_state",2),("office_reference_open",False),
             ("window_foreground",False),("foreground_owner_pid",99),("client_width",1280),
             ("primary_press_count",4),("camera_x",1),("business",{"http_dispatch_attempts":1}))
    for k,bad in mutants:
        x=copy.deepcopy(state);x[k]=bad
        reject(lambda x=x:check_state(x,PHASES[2],state["business"],3,"",(0,)*len(CAMERA),1234,rows),"state "+k);cases+=1
    for group,key,bad in (("widgets","AllianceOfficeReferenceIntro",widget("false membership")),
                          ("office_rows",0,{**observed[0],"source_name":"forged"}),
                          ("office_rows",0,{**observed[0],"Icon":widget(exists=False)}),
                          ("office_rows",0,{**observed[0],"Bonus":widget("claimable")})):
        x=copy.deepcopy(state);x[group][key]=bad
        reject(lambda x=x:check_state(x,PHASES[2],state["business"],3,"",(0,)*len(CAMERA),1234,rows),"source/widget "+str(key));cases+=1
    for label,mutate in (
        ("bonus slot exceeds row",lambda x:x["office_rows"][0]["Bonus"]["canvas_slot"]["size"].update(y=48)),
        ("bonus desired height overflows",lambda x:x["office_rows"][1]["Bonus"]["desired_size"].update(y=47)),
        ("bonus desired width overflows",lambda x:x["office_rows"][1]["Bonus"]["desired_size"].update(x=900)),
        ("bonus actual overflows",lambda x:x["office_rows"][0]["Bonus"]["absolute_bottom_right"].update(y=350)),
        ("hit slot shrunk",lambda x:x["office_rows"][1]["Hit"]["canvas_slot"]["size"].update(y=90)),
        ("hit actual shrunk",lambda x:x["office_rows"][1]["Hit"]["absolute_bottom_right"].update(y=415)),
        ("row overlaps next",lambda x:x["office_rows"][1]["Row"]["absolute_top_left"].update(y=330)),
        ("return button overlap",lambda x:x["widgets"]["AllianceOfficeReferenceBack"]["absolute_top_left"].update(y=670)),
        ("row slot zero",lambda x:x["office_rows"][3]["Row"]["canvas_slot"]["size"].update(y=0))):
        x=copy.deepcopy(state);mutate(x)
        reject(lambda x=x:check_state(x,PHASES[2],state["business"],3,"",(0,)*len(CAMERA),1234,rows),label);cases+=1
    im=Image.new("RGB",(1440,810),(0,0,0))
    for i in range(257):im.putpixel((i,0),(i%256,i//256,0))
    stream=io.BytesIO();im.save(stream,format="PNG");png=stream.getvalue()
    digest=hashlib.sha256(png).hexdigest()
    check_png_raw(png,digest,len(png))
    reject(lambda:check_png_raw(png,"0"*64,len(png)),"PNG hash");cases+=1
    bad_cr=b"\x89PNG\r\n\x1a\r"+png[8:];reject(lambda:check_png_raw(bad_cr,hashlib.sha256(bad_cr).hexdigest(),len(bad_cr)),"bad CR PNG signature");cases+=1
    reject(lambda:check_png_raw(png,digest,len(png)+1),"PNG size");cases+=1
    truncated=png[:-20]
    reject(lambda:check_png_raw(truncated,hashlib.sha256(truncated).hexdigest(),len(truncated)),"PNG truncation");cases+=1
    blank=io.BytesIO();Image.new("RGB",(1440,810),(255,255,255)).save(blank,format="PNG")
    raw=blank.getvalue();reject(lambda:check_png_raw(raw,hashlib.sha256(raw).hexdigest(),len(raw)),"placeholder PNG");cases+=1
    wrong=io.BytesIO();Image.new("RGB",(1280,720),(1,2,3)).save(wrong,format="PNG")
    raw=wrong.getvalue();reject(lambda:check_png_raw(raw,hashlib.sha256(raw).hexdigest(),len(raw)),"PNG dimensions");cases+=1
    expected_inputs()
    return cases

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--self-test",action="store_true")
    parser.add_argument("--manifest",type=Path);args=parser.parse_args()
    need(args.self_test != bool(args.manifest),"choose one mode")
    if args.self_test:
        print(json.dumps({"synthetic_negative_cases":self_test(),"actual_runtime_verified":False}));return
    p=args.manifest.absolute();need(p.is_file(),"manifest missing");validate(json.loads(p.read_text(encoding="utf-8")),p)
    print(json.dumps({"actual_runtime_verified":True,"png_count":12,"office_rows":4}))
if __name__=="__main__":main()
