#!/usr/bin/env python3
"""Strict manifest-first review of the opt-in real-Slate Army Statistics run."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import struct
from pathlib import Path
from rok_data_paths import resolve

SCRIPT = Path(__file__).resolve()
ROOT = SCRIPT.parents[1] if SCRIPT.parent.name == "Scripts" else SCRIPT.parents[2]
TARGET_CPP_SHA = "f2f006b9a6b5c889827a2d2249a375cd0debf724b749b4af9ba9b4d4697b345f"
PHASES = [
    "00_city_hud", "01_profile", "02_local_summary", "03_inside_unknown",
    "04_outside_unknown", "05_summary_return", "06_closed_hud",
]
ACTIONS = [
    "open_profile_from_hud", "open_army_statistics_from_profile",
    "inside_city_unknown", "outside_city_unknown", "return_local_summary",
    "close_army_statistics",
]


def need(ok: bool, why: str) -> None:
    if not ok:
        raise ValueError(why)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def under_root(path: Path) -> Path:
    resolved = path.resolve(strict=True)
    need(resolved == ROOT or ROOT in resolved.parents, f"path outside project: {path}")
    return resolved


def png_size(data: bytes) -> tuple[int, int]:
    need(data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR", "not a PNG")
    width, height = struct.unpack(">II", data[16:24])
    return width, height


def widget(state: dict, name: str) -> dict:
    result = state["widgets"].get(name)
    need(isinstance(result, dict), f"widget state missing: {name}")
    return result


def present(state: dict, name: str) -> bool:
    return widget(state, name)["exists"] is True and widget(state, name)["visible"] is True


def absent(state: dict, name: str) -> bool:
    return widget(state, name)["exists"] is False


def troop_number(text: str, prefix: str) -> int:
    match = re.fullmatch(re.escape(prefix) + r"\s+([0-9,]+)", text)
    need(match is not None, f"unexpected troop field: {text}")
    return int(match.group(1).replace(",", ""))


def check_state(state: dict, phase: str, baseline: dict) -> None:
    need(state["business"] == baseline, f"business mutated at {phase}")
    need(baseline["profile_kind"] == "local_preview" and baseline["role_id"] == ""
         and baseline["civilization_id"] == 0, "account context not anonymous local preview")
    need(baseline["http_dispatch_attempts"] == 0 and baseline["http_suppressed_requests"] == 0,
         "HTTP boundary changed")
    stacks = baseline["starter_troops"]
    need(len(stacks) == 4 and sum(x["count"] for x in stacks) == 200
         and sum(x["power"] for x in stacks) == 200, "bootstrap source is not 4/200/200")
    need(state["primary_press_count"] >= 0, "primary press counter missing")
    if phase in ("00_city_hud", "06_closed_hud"):
        need(not state["screen_open"] and not state["modal_open"], "HUD stack did not return")
        need(present(state, "RokMainHud") and present(state, "PlayerProfileHitButton"),
             "HUD stack or profile entry hidden")
        return
    if phase == "01_profile":
        need(not state["screen_open"] and state["modal_open"] and present(state, "ProfileArmyButton"),
             "profile modal/army entry absent")
        return
    need(state["screen_open"] and not state["modal_open"], f"Army feature stack wrong: {phase}")
    mode = widget(state, "FeatureModeCanvas")
    need(mode["exists"] and mode["visible"] and mode["visibility"] == "SelfHitTestInvisible"
         and abs(mode["opacity"] - 1) <= .01
         and abs(mode["local_size"]["x"] - 1440) <= .1
         and abs(mode["local_size"]["y"] - 810) <= .1, "design canvas not fully open")
    labels = ["本地汇总", "城内", "城外"]
    selected = 0 if phase in ("02_local_summary", "05_summary_return") else (1 if phase == "03_inside_unknown" else 2)
    for i, label in enumerate(labels):
        name = f"ArmyStatisticsPage_{i}"
        need(present(state, name) and widget(state, name)["enabled"], f"tab {i} unavailable")
        tab_label = widget(state, name + "Label")
        need(tab_label.get("text") == label, f"tab {i} label wrong")
        color = tab_label.get("text_color")
        expected = (1, 1, 1, 1) if i == selected else (.72, .72, .70, 1)
        need(isinstance(color, dict) and all(abs(color[key] - value) < .01
             for key, value in zip(("r", "g", "b", "a"), expected)), f"tab {i} selected color wrong")
    source = widget(state, "ArmyStatisticsDataSource")
    need(source["visible"], "source boundary text hidden")
    if phase in ("02_local_summary", "05_summary_return"):
        need("非账号部队状态" in source["text"], "local aggregate boundary absent")
        count = widget(state, "ArmyStatisticsTotalCount")
        power = widget(state, "ArmyStatisticsTotalPower")
        need(count["visible"] and power["visible"]
             and count["text"] == "本地预览兵力：200" and power["text"] == "本地预览战力：200",
             "local totals not exact")
        need(present(state, "ArmyStatisticsTroopList") and absent(state, "ArmyStatisticsInsideEmpty")
             and absent(state, "ArmyStatisticsOutsideEmpty"), "aggregate structure wrong")
        for i, stack in enumerate(stacks):
            need(present(state, f"ArmyStatisticsRow_{i}"), f"source troop row {i} absent")
            name = widget(state, f"ArmyStatisticsRowName_{i}")["text"]
            need(f"配置 {stack['id']}" in name and f"T{stack['level']}" in name,
                 f"source troop identity {i} wrong")
            need(troop_number(widget(state, f"ArmyStatisticsRowCount_{i}")["text"], "数量") == stack["count"],
                 f"source troop quantity {i} wrong")
            need(troop_number(widget(state, f"ArmyStatisticsRowPower_{i}")["text"], "战力") == stack["power"],
                 f"source troop power {i} wrong")
        return
    inside = phase == "03_inside_unknown"
    need(phase in ("03_inside_unknown", "04_outside_unknown"), "unexpected phase")
    wanted = "ArmyStatisticsInsideEmpty" if inside else "ArmyStatisticsOutsideEmpty"
    other = "ArmyStatisticsOutsideEmpty" if inside else "ArmyStatisticsInsideEmpty"
    need(present(state, wanted) and absent(state, other), "location empty state wrong")
    need("位置数据尚未接入" in widget(state, wanted)["text"]
         and "位置归属数据未接入" in source["text"], "unknown deployment not explained")
    need(absent(state, "ArmyStatisticsTotalCount") and absent(state, "ArmyStatisticsTotalPower")
         and absent(state, "ArmyStatisticsTroopList") and absent(state, "ArmyStatisticsRow_0"),
         "category silently reused global totals or troop rows")




def _point(value: dict) -> tuple[float, float]:
    return float(value["x"]), float(value["y"])

def _whole_number(value: object) -> bool:
    return not isinstance(value, bool) and isinstance(value, (int, float)) and float(value).is_integer()

def _corners(geometry: dict, allow_zero: bool = False) -> list[tuple[float, float]]:
    points = [_point(p) for p in geometry.get("absolute_corners", [])]
    need(len(points) == 4, "geometry must have four absolute corners")
    size = geometry.get("local_size", {})
    valid = (float(size.get("x", -1)) >= 0 and float(size.get("y", -1)) >= 0) if allow_zero else (float(size.get("x", 0)) > 0 and float(size.get("y", 0)) > 0)
    need(valid, "geometry local size invalid")
    return points

def _inside(point: tuple[float, float], polygon: list[tuple[float, float]], tol: float = 0.25) -> bool:
    signs = []
    for a, b in zip(polygon, polygon[1:] + polygon[:1]):
        cross = (b[0]-a[0])*(point[1]-a[1]) - (b[1]-a[1])*(point[0]-a[0])
        signs.append(cross)
    return all(x >= -tol for x in signs) or all(x <= tol for x in signs)

def _geometry_close(a: dict, b: dict, tol: float = 0.25) -> bool:
    aa, bb = _corners(a), _corners(b)
    sa, sb = _point(a["local_size"]), _point(b["local_size"])
    return all(abs(x-y) <= tol for p,q in zip(aa,bb) for x,y in zip(p,q)) and all(abs(x-y) <= tol for x,y in zip(sa,sb))

def verify_hit_proof(action: dict) -> None:
    nodes = action.get("hit_path_nodes")
    need(isinstance(nodes, list) and nodes, "missing detailed Slate hit path")
    ti, bi = action.get("target_path_index"), action.get("owned_button_path_index")
    need(_whole_number(ti) and 0 <= int(ti) < len(nodes), "target Slate identity not in hit path")
    ti = int(ti)
    need(nodes[ti].get("identity") == action.get("target_slate_identity"), "target identity/path node mismatch")
    need(action.get("target_in_hit_path") is True and action.get("own_game_window") is True
         and action.get("game_window_foreground") is True and action.get("target_hovered") is True,
         "target/window/foreground/hover gate failed")
    need(_whole_number(bi) and int(bi) == ti and 0 <= int(bi) < len(nodes), "owned button must be exact target UButton node")
    bi = int(bi)
    need(nodes[bi].get("identity") == action.get("owned_button_slate_identity"), "button identity/path node mismatch")
    need("SButton" in nodes[bi].get("type", ""), "target Slate node is not SButton")
    need(action.get("owned_button_in_hit_path") is True and action.get("umg_path_present") is True
         and any("SObjectWidget" in n.get("type", "") for n in nodes), "UMG target hit path proof missing")
    target_type = action.get("target_widget_type")
    need(action.get("target_widget_is_u_button") is True
         and action.get("target_widget_class_path") in {"/Script/UMG.Button", "/Script/Rok.RokUiAnimatedButton",
                                                     "/Script/Rok.RokUnityIndexedButton"},
         "target widget class is not source-pinned UButton/URokUiAnimatedButton")
    need(action.get("target_widget_identity") == action.get("owned_button_widget_identity")
         and action.get("target_widget_name") == action.get("owned_button_widget_name")
         and action.get("target_widget_type") == action.get("owned_button_widget_type"), "owned target is not same UButton")
    chain = action.get("owned_button_parent_chain")
    need(isinstance(chain, list) and len(chain) == 1 and chain[0].get("is_target") is True
         and chain[0].get("widget_identity") == action.get("target_widget_identity")
         and chain[0].get("widget_name") == action.get("target_widget_name")
         and chain[0].get("widget_type") == action.get("target_widget_type"), "UWidget parent chain does not terminate at target")
    for index, node in enumerate(nodes):
        need(node.get("index") == index and node.get("identity") and node.get("type"), "Slate path node identity/order invalid")
        _corners(node.get("geometry", {}))
    path_geometry = nodes[ti].get("geometry", {})
    target_geometry = action.get("target_geometry", {})
    need(_geometry_close(path_geometry, target_geometry), "target path geometry differs from click target geometry")
    polygon = _corners(path_geometry)
    center = _point(action.get("absolute_center", {}))
    derived_center = (sum(x for x,_ in polygon)/4, sum(y for _,y in polygon)/4)
    need(abs(center[0]-derived_center[0]) <= 0.25 and abs(center[1]-derived_center[1]) <= 0.25,
         "absolute click center does not match target geometry")
    need(_inside(center, polygon), "click center outside target geometry")
    button_geometry = _corners(nodes[bi].get("geometry", {}))
    need(_inside(center, button_geometry), "click center outside owned SButton geometry")
    stability = action.get("two_frame_stability", {})
    pf, cf = stability.get("previous_frame"), stability.get("current_frame")
    need(_whole_number(pf) and _whole_number(cf) and float(cf)-float(pf) >= 1, "target geometry stability frames not distinct")
    previous, current = stability.get("previous_geometry", {}), stability.get("current_geometry", {})
    need(_geometry_close(previous, current) and _geometry_close(current, target_geometry), "two-frame target geometry drift")
    need(_corners(previous) and _corners(current), "two-frame geometry evidence missing")

def verify_preflight_attempt(attempt: dict) -> None:
    need(attempt.get("phase") == "preflight_failure" and attempt.get("failure_reason"), "failed attempt missing reason")
    need(_whole_number(attempt.get("frame")) and _whole_number(attempt.get("step")), "failed attempt missing frame/step")
    if attempt.get("target_exists"):
        need(attempt.get("target_widget_identity") and attempt.get("target_widget_name"), "failed target attempt missing identity")
        geometry = attempt.get("target_geometry")
        if geometry:
            _corners(geometry, allow_zero=True)


def verify(run: Path) -> dict:
    run = under_root(run)
    manifest_path = under_root(run / "manifest.json")
    m = json.loads(manifest_path.read_text(encoding="utf-8"))
    need(m["schema"] == "rok.army_statistics.runtime_audit.v1" and m["complete"] is True
         and m["passed"] is True and m["failures"] == [], "runtime itself did not pass")
    need(m.get("attempts") == [], "successful run contains failed preflight attempts")
    need(Path(m["project"]).resolve() == ROOT and m["world"] == "/Game/Maps/L_RokUnityGameAppCity",
         "wrong project/world")
    need(m["world_time_advanced_by_audit"] is False and m["pid"] > 0, "invalid runtime provenance")
    need(run.name.endswith("_" + str(m["pid"])), "run directory does not bind PID")
    inputs = m["inputs_sha256"]
    need(inputs["Source/Rok/RokUnityFeatureScreenWidget.cpp"] == TARGET_CPP_SHA,
         "staged UI V3 target not integrated")
    for required_button_source in ("Source/Rok/RokUiAnimatedButton.h", "Source/Rok/RokUiAnimatedButton.cpp"):
        need(required_button_source in inputs, f"missing source-pinned UButton class input: {required_button_source}")
    for rel, expected in inputs.items():
        need(re.fullmatch(r"[0-9a-f]{64}", expected) is not None, f"bad hash: {rel}")
        need(sha(under_root(resolve(rel))) == expected, f"input drift: {rel}")
    actions, captures = m["actions"], m["captures"]
    need(len(actions) == 6 and len(captures) == 7, "capture/action cardinality wrong")
    need([a["label"] for a in actions] == ACTIONS and [c["phase"] for c in captures] == PHASES,
         "real click or phase order changed")
    baseline = captures[0]["requested_state"]["business"]
    primary = captures[0]["requested_state"]["primary_press_count"]
    camera = captures[0]["requested_state"]["camera_location"]
    camera_z = captures[0]["requested_state"]["camera_z"]
    camera_rotation = captures[0]["requested_state"]["camera_rotation_yaw_pitch"]
    last_frame = -1
    for i, action in enumerate(actions):
        need(action["method"] == "Slate ProcessMouseMoveEvent+DownEvent+UpEvent"
             and action["own_game_window"] and action["game_window_foreground"]
             and action["target_hovered"] and action["down_handled"] and action["up_handled"],
             f"Slate action {i} not proven")
        verify_hit_proof(action)
        need(action["primary_before"] == primary and action["primary_after"] == primary,
             f"action {i} reached world press")
        need(captures[i]["processed_frame"] < action["frame"] < captures[i+1]["requested_frame"],
             f"action {i} not bracketed by natural captures")
    for capture in captures:
        phase = capture["phase"]
        path = under_root(Path(capture["png"]))
        need(path.parent == run and path.name == phase + ".png", "PNG path escaped run")
        data = path.read_bytes()
        need(len(data) == capture["png_bytes"] and hashlib.sha256(data).hexdigest() == capture["png_sha256"],
             "PNG hash/bytes mismatch")
        need(png_size(data) == (1440, 810), "PNG viewport not 1440x810")
        need(last_frame < capture["requested_frame"] <= capture["processed_frame"] < capture["next_frame_state"]["frame"],
             "screenshot frame order wrong")
        last_frame = capture["next_frame_state"]["frame"]
        for sample in ("requested_state", "processed_state", "next_frame_state"):
            state = capture[sample]
            need(state["primary_press_count"] == primary and state["camera_location"] == camera
                 and state["camera_z"] == camera_z and state["camera_rotation_yaw_pitch"] == camera_rotation,
                 "world press or city camera changed")
            check_state(state, phase, baseline)
    need(m["final_state"]["business"] == baseline and m["final_state"]["primary_press_count"] == primary,
         "final business state changed")
    return {"passed": True, "captures": len(captures), "actions": len(actions),
            "source_ui_sha256": TARGET_CPP_SHA, "run": str(run)}


def self_test() -> None:
    baseline = {"profile_kind": "local_preview", "role_id": "", "civilization_id": 0,
                "http_dispatch_attempts": 0, "http_suppressed_requests": 0,
                "starter_troops": [{"id": n, "level": 1, "count": 50, "power": 50} for n in range(4)]}
    state = {"business": baseline, "primary_press_count": 0, "screen_open": True, "modal_open": False,
             "widgets": {}}
    def w(exists=True, visible=True, text=""):
        return {"exists": exists, "visible": visible, "text": text, "enabled": True,
                "visibility": "SelfHitTestInvisible", "opacity": 1,
                "text_color": {"r": 1, "g": 1, "b": 1, "a": 1},
                "local_size": {"x": 1440, "y": 810}}
    state["widgets"] = {"FeatureModeCanvas": w(), "ArmyStatisticsDataSource": w(text="非账号部队状态"),
                        "ArmyStatisticsTotalCount": w(text="本地预览兵力：200"),
                        "ArmyStatisticsTotalPower": w(text="本地预览战力：200"),
                        "ArmyStatisticsTroopList": w(), "ArmyStatisticsInsideEmpty": w(False, False),
                        "ArmyStatisticsOutsideEmpty": w(False, False)}
    for i, label in enumerate(("本地汇总", "城内", "城外")):
        state["widgets"][f"ArmyStatisticsPage_{i}"] = w()
        state["widgets"][f"ArmyStatisticsPage_{i}Label"] = w(text=label)
        if i:
            state["widgets"][f"ArmyStatisticsPage_{i}Label"]["text_color"] = {
                "r": .72, "g": .72, "b": .70, "a": 1}
    for i in range(4):
        state["widgets"][f"ArmyStatisticsRow_{i}"] = w()
        state["widgets"][f"ArmyStatisticsRowName_{i}"] = w(text=f"步兵 T1 · 配置 {i}")
        state["widgets"][f"ArmyStatisticsRowCount_{i}"] = w(text="数量 50")
        state["widgets"][f"ArmyStatisticsRowPower_{i}"] = w(text="战力 50")
    check_state(state, "02_local_summary", baseline)
    def geom(left=10, top=20, right=40, bottom=50):
        return {"local_size": {"x": right-left, "y": bottom-top},
                "absolute_corners": [{"x":left,"y":top},{"x":right,"y":top},
                                     {"x":right,"y":bottom},{"x":left,"y":bottom}]}
    def hit(own=True, foreground=True):
        g=geom()
        nodes=[{"index":0,"type":"SWindow","identity":"w","geometry":geom(0,0,1440,810)},
               {"index":1,"type":"SObjectWidget","identity":"umg","geometry":geom(0,0,1440,810)},
               {"index":2,"type":"SButton","identity":"target-slate","geometry":g}]
        return {"hit_path_nodes":nodes,"target_path_index":2,"target_slate_identity":"target-slate",
                "target_in_hit_path":True,"owned_button_path_index":2,"owned_button_slate_identity":"target-slate",
                "owned_button_in_hit_path":True,"umg_path_present":True,"target_widget_type":"RokUiAnimatedButton","target_widget_class_path":"/Script/Rok.RokUiAnimatedButton",
                "target_widget_is_u_button":True,"target_widget_name":"ArmyButton","target_widget_identity":"u-target",
                "owned_button_widget_type":"RokUiAnimatedButton","owned_button_widget_name":"ArmyButton",
                "owned_button_widget_identity":"u-target","owned_button_parent_chain":[{"widget_name":"ArmyButton",
                "widget_type":"RokUiAnimatedButton","widget_identity":"u-target","is_target":True}],
                "target_geometry":g,"absolute_center":{"x":25,"y":35},
                "two_frame_stability":{"previous_frame":100,"current_frame":101,"previous_geometry":g,"current_geometry":g},
                "own_game_window":own,"game_window_foreground":foreground,"target_hovered":True}
    verify_hit_proof(hit())
    indexed = hit()
    indexed["target_widget_type"] = "RokUnityIndexedButton"
    indexed["target_widget_class_path"] = "/Script/Rok.RokUnityIndexedButton"
    indexed["owned_button_widget_type"] = "RokUnityIndexedButton"
    indexed["owned_button_parent_chain"][0]["widget_type"] = "RokUnityIndexedButton"
    verify_hit_proof(indexed)  # source-pinned subclass declared in FeatureScreenWidget.h
    negatives=[]
    x=hit();x["target_path_index"]=-1;negatives.append((x,"missing target path"))
    x=hit();x["target_slate_identity"]="other";negatives.append((x,"wrong target Slate identity"))
    x=hit();x["owned_button_path_index"]=1;x["owned_button_slate_identity"]="umg";negatives.append((x,"ancestor/unrelated button"))
    x=hit();x["target_widget_identity"]="other";negatives.append((x,"false UWidget ownership chain"))
    x=hit();x["target_widget_type"]="ForeignIndexedButton";x["target_widget_class_path"]="/Script/Rok.ForeignIndexedButton";x["owned_button_widget_type"]="ForeignIndexedButton";x["owned_button_parent_chain"][0]["widget_type"]="ForeignIndexedButton";negatives.append((x,"foreign UButton subclass"))
    x=hit(own=False);negatives.append((x,"foreign window"))
    x=hit(foreground=False);negatives.append((x,"not foreground"))
    x=hit();x["hit_path_nodes"][2]["type"]="STextBlock";negatives.append((x,"non-SButton target"))
    x=hit();x["hit_path_nodes"][1]["geometry"]=geom(0,0,0,0);negatives.append((x,"missing path-node geometry"))
    x=hit();x["two_frame_stability"]["current_frame"]=100;negatives.append((x,"same stability frame"))
    x=hit();x["two_frame_stability"]["current_frame"]=101.5;negatives.append((x,"fractional stability frame"))
    x=hit();x["two_frame_stability"]["current_geometry"]=geom(11,20,41,50);negatives.append((x,"stability geometry drift"))
    x=hit();x["absolute_center"]={"x":99,"y":99};negatives.append((x,"center outside target"))
    x=hit();x["owned_button_parent_chain"][0]["is_target"]=False;negatives.append((x,"chain endpoint not target"))
    for bad,label in negatives:
        try:verify_hit_proof(bad)
        except (ValueError,KeyError):continue
        raise AssertionError(f"V3 hit-path negative passed: {label}")
    valid_attempt={"phase":"preflight_failure","failure_reason":"target widget missing","frame":9,"step":4,"target_exists":False}
    verify_preflight_attempt(valid_attempt)
    for bad in ({**valid_attempt,"failure_reason":""},{**valid_attempt,"frame":None},
                {**valid_attempt,"target_exists":True,"target_widget_name":"A","target_widget_identity":""}):
        try:verify_preflight_attempt(bad)
        except (ValueError,KeyError):continue
        raise AssertionError("preflight failure evidence negative passed")
    mutations = [
        ("business", "http_dispatch_attempts", 1),
        ("widgets", "ArmyStatisticsTotalCount", w(text="本地预览兵力：999")),
        ("widgets", "ArmyStatisticsRowCount_0", w(text="数量 99")),
        ("widgets", "ArmyStatisticsPage_0Label", w(text="总兵力")),
    ]
    for path, key, value in mutations:
        bad = copy.deepcopy(state)
        bad[path][key] = value
        try:
            check_state(bad, "02_local_summary", baseline)
        except ValueError:
            continue
        raise AssertionError(f"negative mutation passed: {path}.{key}")
    for phase, empty_name, other_name, prefix, selected in (
        ("03_inside_unknown", "ArmyStatisticsInsideEmpty", "ArmyStatisticsOutsideEmpty", "城内", 1),
        ("04_outside_unknown", "ArmyStatisticsOutsideEmpty", "ArmyStatisticsInsideEmpty", "城外", 2),
    ):
        local = copy.deepcopy(state)
        local["widgets"]["ArmyStatisticsDataSource"]["text"] = "位置归属数据未接入；不按配置兵种推断城内或城外。"
        local["widgets"]["ArmyStatisticsTotalCount"] = w(False, False)
        local["widgets"]["ArmyStatisticsTotalPower"] = w(False, False)
        local["widgets"]["ArmyStatisticsTroopList"] = w(False, False)
        local["widgets"][empty_name] = w(text=prefix + "兵力位置数据尚未接入。")
        local["widgets"][other_name] = w(False, False)
        local["widgets"]["ArmyStatisticsRow_0"] = w(False, False)
        local["widgets"]["ArmyStatisticsPage_0Label"]["text_color"] = {
            "r": .72, "g": .72, "b": .70, "a": 1}
        local["widgets"][f"ArmyStatisticsPage_{selected}Label"]["text_color"] = {
            "r": 1, "g": 1, "b": 1, "a": 1}
        check_state(local, phase, baseline)
        bad = copy.deepcopy(local)
        bad["widgets"]["ArmyStatisticsTotalCount"] = w(text="本地预览兵力：200")
        try:
            check_state(bad, phase, baseline)
        except ValueError:
            pass
        else:
            raise AssertionError(f"category-total negative mutation passed: {phase}")
    print("SELFTEST PASS: Army pages, exact UButton parent chain, two-frame/four-corner geometry, preflight-attempt shape, and 14 hit-path negatives")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
    else:
        need(args.run is not None, "--run required")
        print(json.dumps(verify(args.run), ensure_ascii=False, indent=2))
