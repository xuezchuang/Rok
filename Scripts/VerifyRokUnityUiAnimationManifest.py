"""Verify generated UI motion against current sources and independent key checks."""

import argparse
import json
import math
from pathlib import Path

from BuildRokUnityUiAnimationManifest import (
    COVERAGE_PATH, OUTPUT_DIR, OUTPUT_PATH, SourceError, build_manifest,
)


VERIFY_PATH = OUTPUT_DIR / "unity_ui_animation_manifest_verify.json"


def close(actual, expected):
    if isinstance(expected, dict):
        return isinstance(actual, dict) and all(close(actual.get(key), value) for key, value in expected.items())
    return isinstance(actual, (int, float)) and math.isclose(actual, expected, abs_tol=1e-6)


def verify(manifest, coverage_path=COVERAGE_PATH):
    failures = []
    try:
        rebuilt = build_manifest(coverage_path)
        if manifest != rebuilt:
            failures.append("Generated manifest differs from current sources; regenerate it")
    except (SourceError, OSError, UnicodeError, ValueError) as error:
        failures.append(str(error))
    clips = manifest.get("clips", {})

    def check_keys(name, path, attribute, expected):
        matches = [track for track in clips.get(name, {}).get("tracks", [])
                   if track.get("path") == path and track.get("attribute") == attribute]
        if len(matches) != 1:
            failures.append(f"{name}: expected one track {path}/{attribute}, found {len(matches)}")
            return
        keys = matches[0].get("keys", [])
        if len(keys) != len(expected) or any(not close(key.get("time"), time) or not close(key.get("value"), value)
                                           for key, (time, value) in zip(keys, expected)):
            failures.append(f"{name}: wrong source key values for {path}/{attribute}")

    check_keys("CommonBtnDownAni", "", "m_LocalScale", [(0, {"x": 1, "y": 1}), (1 / 12, {"x": .9, "y": .9})])
    check_keys("UA_InterfaceChat", "", "m_Color.a", [(0, 0), (.5, 1), (1, 0)])
    check_keys("UI_10010", "m_liupian001", "material._MainTex_ST.x",
               [(0, 1), (1 / 6, .26447147), (.85, -.09)])
    power_clip = clips.get("UI_10010", {})
    if power_clip.get("loop") or not close(power_clip.get("duration_seconds"), .85):
        failures.append("Power gain UV animation must be a non-looping 0.85 second clip")
    power_effect = manifest.get("source_chains", {}).get("power_gain_effect", {})
    if not close(power_effect.get("auto_destroy_seconds"), 1):
        failures.append("Power gain effect root must expire after one second")
    if power_effect.get("particle", {}).get("looping") != 0 or len(power_effect.get("materials", [])) != 2:
        failures.append("Power gain effect must retain its non-looping particle and both source materials")
    chat_flash = clips.get("UA_InterfaceChat", {})
    if not chat_flash.get("loop") or not close(chat_flash.get("duration_seconds"), 1):
        failures.append("HUD alliance unread flash must loop every one second")
    for track in chat_flash.get("tracks", []):
        for key in track.get("keys", []):
            if not close(key.get("in_slope"), 0) or not close(key.get("out_slope"), 0):
                failures.append("HUD alliance unread flash must preserve smooth zero tangents")
    check_keys("UA_ItemAnime_BCBShow", "", "m_LocalScale", [(0, {"x": 1, "y": 1}), (7 / 60, {"x": 1.2, "y": 1.2}), (1 / 3, {"x": 1, "y": 1})])
    check_keys("CommonBtnUpAni", "", "m_LocalScale", [(0, {"x": .9, "y": .9}), (1 / 12, {"x": 1.06, "y": 1.03}),
               (11 / 60, {"x": .97, "y": .95}), (17 / 60, {"x": 1, "y": 1})])
    check_keys("UA_PopAnime_On", "", "m_LocalScale", [(0, {"x": 0, "y": 0}), (7 / 60, {"x": 1.1, "y": 1.1}), (.25, {"x": 1, "y": 1})])
    check_keys("UA_WinAnime_T1_On", "", "m_LocalScale", [(0, {"x": 1.05, "y": 1.05}), (7 / 60, {"x": 1.1, "y": 1.1}), (.25, {"x": 1, "y": 1})])
    check_keys("UA_WinAnime_T1_Off", "", "m_Alpha", [(0, 1), (.15, 0)])
    check_keys("UA_WinAnime_IF_On", "", "m_Alpha", [(0, 0), (.25, 1)])
    check_keys("UA_CreateCharView_CiviChange", "sv_list_country", "m_AnchoredPosition.y", [(0, -200), (1 / 3, 1), (.5, 1)])
    check_keys("UA_CreateCharView_CiviChange", "sv_list_country", "m_Alpha", [(0, 0), (1 / 3, 1), (.5, 1)])
    check_keys("UA_BuildingCity_On", "", "m_AnchoredPosition.y", [(0, -100), (1 / 3, 0)])
    check_keys("UA_BuildingCity_On", "", "m_Alpha", [(0, 0), (1 / 3, 1)])
    check_keys("UA_BuildingCity_Off", "", "m_AnchoredPosition.y", [(0, 0), (1 / 3, -150)])
    check_keys("UA_BuildingCity_Off", "", "m_Alpha", [(0, 1), (1 / 3, 0)])
    for name, duration, hero_x, hero_alpha, flag_y, flag_alpha in (
        ("UA_CreateCharView_CharShow", .5, (-685, -485), (0, 1), (485, 415), (0, 1)),
        ("UA_CreateCharView_CharOut", 1 / 6, (-485, -685), (0, 0), (415, 485), (1, 0)),
    ):
        for path, attribute, values in (
            ("pl_horeSpine", "m_AnchoredPosition.x", hero_x),
            ("pl_horeSpine", "m_Alpha", hero_alpha),
            ("img_charCountryFlag", "m_AnchoredPosition.y", flag_y),
            ("img_charCountryFlag", "m_Alpha", flag_alpha),
        ):
            check_keys(name, path, attribute, [(0, values[0]), (duration, values[1])])
        if not close(clips.get(name, {}).get("duration_seconds"), duration) or clips.get(name, {}).get("loop"):
            failures.append(f"{name}: wrong civilization transition duration/loop")
    check_keys("UI_10011", "img_selectBox (1)", "m_LocalScale",
               [(0, {"x": 1.0038509, "y": 1.0038509}), (3.1333334, {"x": 1.2272912, "y": 1.2272912})])
    check_keys("UI_10011", "img_selectBox (1)", "m_Color.a", [(0, 0), (.4, 1), (3.1333334, 0)])
    check_keys("UI_10011", "img_selectBox (1)", "m_AnchoredPosition.y", [(0, .39077), (3.1333334, 23.187)])
    if not clips.get("UI_10011", {}).get("loop"):
        failures.append("Civilization highlight source loop was lost")
    pulse_alpha = next((track for track in clips.get("UI_10011", {}).get("tracks", [])
                        if track.get("attribute") == "m_Color.a"), {})
    pulse_keys = pulse_alpha.get("keys", [])
    if len(pulse_keys) != 3 or not close(pulse_keys[1].get("in_slope"), 2.5) or not close(pulse_keys[1].get("out_slope"), -.4632681):
        failures.append("Civilization highlight asymmetric alpha tangents were lost")
    check_keys("UA_IFSearchRes_On", "pl_btns", "m_AnchoredPosition.y", [(0, -130), (10 / 60, 50), (17 / 60, 15)])
    check_keys("UA_IFSearchRes_On", "pl_Pop", "m_LocalScale", [(0, {"x": 0, "y": 0}), (8 / 60, {"x": 0, "y": 0}), (.25, {"x": 1, "y": 1})])
    check_keys("UA_IFSearchRes_Off", "pl_btns", "m_AnchoredPosition.y", [(0, 15), (1 / 60, 15)])
    check_keys("UA_BuildingMenu_On", "pl_Title", "m_LocalScale", [(0, {"x": 0, "y": 0}),
               (7 / 60, {"x": 1.2, "y": 1.2}), (.25, {"x": 1, "y": 1})])
    check_keys("UA_BuildingMenu_On", "", "m_Alpha", [(0, 0), (7 / 60, 1), (.25, 1)])
    check_keys("UA_BuildingMenu_Off", "pl_Title", "m_LocalScale", [(0, {"x": 1, "y": 1}),
               (5 / 60, {"x": 1.2, "y": 1.2}), (.25, {"x": 0, "y": 0})])
    check_keys("UA_BuildingMenu_Off", "", "m_Alpha", [(0, 1), (5 / 60, 1), (.25, 0)])
    check_keys("UA_MainIF_HotTask_On", "img_bg", "m_SizeDelta.x", [(0, 230), (1 / 3, 166)])
    check_keys("UA_MainIF_HotTask_On", "img_bg", "m_SizeDelta.y", [(0, 322), (1 / 3, 43)])
    check_keys("UA_MainIF_HotTask_On", "img_bg/sv_list_tasks", "m_SizeDelta.y", [(0, 280), (1 / 3, 0)])
    check_keys("UA_MainIF_Rotation_ROn", "img_featuremask/pl_btns", "m_AnchoredPosition.x", [(0, 440.5), (1 / 30, 425.9), (.25, 0)])
    check_keys("UA_MainIF_Rotation_ROff", "img_featuremask/pl_btns", "m_AnchoredPosition.x", [(0, 0), (.25, 440.5)])
    check_keys("UA_Chat_On", "", "m_AnchoredPosition.x", [(0, -1000), (1 / 3, 0)])
    check_keys("UA_Chat_On", "", "m_Alpha", [(0, 0), (1 / 3, 1)])
    check_keys("UA_Chat_Off", "", "m_AnchoredPosition.x", [(0, 0), (1 / 3, -1000)])
    check_keys("UA_Chat_Off", "", "m_Alpha", [(0, 1), (1 / 3, 0)])
    check_keys("UA_TaskBox", "", "m_LocalScale", [(0, {"x": 1, "y": 1}), (1 / 6, {"x": 1.2, "y": 1.2}),
               (1 / 3, {"x": 1, "y": 1}), (.5, {"x": 1.2, "y": 1.2}), (2 / 3, {"x": 1, "y": 1}), (1.5, {"x": 1, "y": 1})])
    more = next((track for track in clips.get("UA_MainIF_Rotation_ROn", {}).get("tracks", [])
                 if track.get("path") == "img_featuremask/pl_btns" and track.get("attribute") == "m_AnchoredPosition.x"), {})
    keys = more.get("keys", [])
    if len(keys) < 2 or not close(keys[1].get("in_slope"), -233.6001) or not close(keys[1].get("out_slope"), -233.6001):
        failures.append("More-menu intermediate Hermite tangent was lost")
    for name in ("CommonBtnDownAni", "CommonBtnUpAni", "UA_PopAnime_On", "UA_WinAnime_T1_On", "UA_MainIF_HotTask_On",
                 "UA_Chat_On", "UA_Chat_Off", "UA_CreateCharView_CharShow", "UA_CreateCharView_CharOut"):
        for track in clips.get(name, {}).get("tracks", []):
            for key in track.get("keys", []):
                for slope_name in ("in_slope", "out_slope"):
                    slope = key.get(slope_name)
                    values = slope.values() if isinstance(slope, dict) else [slope]
                    if any(not close(value, 0) for value in values):
                        failures.append(f"{name}: expected zero source tangents")
    for name in ("UA_PopAnime_On", "UA_PopAnime_Off", "UA_WinAnime_T1_On"):
        if any(track.get("attribute") in {"m_Alpha", "m_Color.a"} for track in clips.get(name, {}).get("tracks", [])):
            failures.append(f"{name}: invented alpha track")
    for name, time in (("UA_PopAnime_Off_hasCallback", .25), ("UA_WinAnime_T1_Off", .15), ("UA_Chat_Off", 1 / 3)):
        if not any(event.get("function") == "CloseAnimatorCallback" and close(event.get("time"), time)
                   for event in clips.get(name, {}).get("events", [])):
            failures.append(f"{name}: missing close completion event")
    if not clips.get("UA_TaskBox", {}).get("loop"):
        failures.append("Task gift source loop was lost")
    for name in ("UA_Chat_On", "UA_Chat_Off"):
        clip = clips.get(name, {})
        if not close(clip.get("duration_seconds"), 1 / 3) or clip.get("loop"):
            failures.append(f"{name}: wrong root drawer duration/loop")
        if any(track.get("kind") == "scale" for track in clip.get("tracks", [])):
            failures.append(f"{name}: invented root scale animation")
    task_reward = manifest.get("source_chains", {}).get("task_reward", {})
    layout = task_reward.get("layout", {})
    for name, expected in {
        "row": {"size": {"x": 196, "y": 80}},
        "background": {"size": {"x": 196, "y": 80}, "top_left": {"x": 0, "y": 0}},
        "reward_icon": {"size": {"x": 30, "y": 30}, "top_left": {"x": 161.8, "y": 6.9}},
        "title": {"size": {"x": 135.23, "y": 30.71}, "top_left": {"x": 9.685, "y": 1.1}, "font_size": 20},
        "description": {"size": {"x": 176, "y": 44}, "top_left": {"x": 10, "y": 33}, "font_size": 18, "line_spacing": .8},
    }.items():
        if not close(layout.get(name), expected):
            failures.append(f"Task row {name}: wrong non-Arabic runtime layout")
    if not close(layout.get("list_spacing"), 4) or not close(layout.get("list_offset"), 0):
        failures.append("Task list must preserve source spacing 4 and offset 0")
    if layout.get("glow", {}).get("active_override") is not False:
        failures.append("Task UI_10035 glow is overridden inactive; do not invent default glow")
    expected_states = {"unfinished": {"reward_visible": False, "row_action": "GoScript"},
                       "finished": {"reward_visible": True, "row_action": "Task_TaskFinish"},
                       "chapter": {"reward_visible_when": "finished_count == task_count", "row_action": "open UI.s_Taskinfo"}}
    if task_reward.get("states") != expected_states:
        failures.append("Task reward visibility/action conditions differ from source")
    reward_sprite = layout.get("reward_icon", {}).get("sprite", {})
    if reward_sprite.get("file_id") != "-1554543322931129388" or not close(reward_sprite.get("rect_bottom_left"),
            {"x": 976, "y": 899, "width": 40, "height": 40}):
        failures.append("Task gift atlas identity/rectangle was lost")
    background_sprite = layout.get("background", {}).get("sprite", {})
    if background_sprite.get("file_id") != "-3733613599318417022" or not close(background_sprite.get("border_left_bottom_right_top"),
            {"x": 16, "y": 13, "z": 15, "w": 13}):
        failures.append("Task row atlas identity/border was lost")
    mappings = {mapping.get("id"): mapping for mapping in manifest.get("runtime_mappings", [])}
    build_city = mappings.get("build_city", {})
    if build_city.get("presentation_mode") != "none":
        failures.append("Build city child Animator must not become a root window animation")
    drawer_bindings = [binding for binding in build_city.get("controller_bindings", [])
                       if binding.get("mode") == "build_city_drawer"]
    if len(drawer_bindings) != 1 or drawer_bindings[0].get("object_path") != "UI_Win_BuildCity/rect" or drawer_bindings[0].get("is_root"):
        failures.append("Build city drawer source binding must remain on child rect")
    if clips.get("UA_BuildingCity_Off", {}).get("events"):
        failures.append("Build city Off has no source close completion event")
    if mappings.get("items", {}).get("presentation_mode") != "T1":
        failures.append("Items/Bag must use its actual T1 root controller")
    if mappings.get("city_hud", {}).get("presentation_mode") != "none":
        failures.append("HUD child animations must not become whole-screen popup animation")
    if mappings.get("chat", {}).get("presentation_mode") != "chat_drawer":
        failures.append("Chat must use its root drawer controller, not a generic window animation")
    if manifest.get("source_chains", {}).get("redpoint", {}).get("generic_loop_animation") is not False:
        failures.append("Generic redpoint breathing is not source-backed")
    return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=OUTPUT_PATH)
    parser.add_argument("--coverage", type=Path, default=COVERAGE_PATH)
    parser.add_argument("--output", type=Path, default=VERIFY_PATH)
    args = parser.parse_args()
    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8-sig"))
        failures = verify(manifest, args.coverage)
    except (OSError, UnicodeError, ValueError) as error:
        failures = [str(error)]
        manifest = {}
    result = {"schema": "rok.unity_ui_animation_manifest_verify.v1", "passed": not failures,
              "source_evidence_only": True, "runtime_visual_acceptance_claimed": False,
              "summary": manifest.get("summary", {}), "failure_count": len(failures), "failures": failures}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))
    raise SystemExit(0 if result["passed"] else 1)


if __name__ == "__main__":
    main()
