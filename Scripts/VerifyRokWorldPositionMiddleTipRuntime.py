"""Strict verifier for the opt-in real-Slate MiddleTip timeline."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import struct
import tempfile
import zlib
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path("D:/ueDemo/Rok")
PROJECT = "D:/ueDemo/Rok"
CITY = "/Game/Maps/L_RokUnityGameAppCity"
WORLD = "/Game/Maps/L_RokUnityWorldMapPrototype"
SCHEMA = "rok.world_position_middle_tip.runtime_audit.v4"
UI_H = "Source/Rok/RokUnityFeatureScreenWidget.h"
UI_CPP = "Source/Rok/RokUnityFeatureScreenWidget.cpp"
COMMON_ASSET = "Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.uasset"
MAIN_ASSET = "Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.uasset"
COMMON_OBJECT = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"
APPROVED = {
    UI_H: "93a1a7c2d9097982510372f6550065c528671b89fe32da3421036ba70c2bf7d1",
    UI_CPP: "c85d0159031c839256c7045a896f077e9c65618994a89632d1f7c26defeca4e6",
    COMMON_ASSET: "a3f6245b2c64f552e7b8a253e98f637d6ca0292a4aab87e5fd9183f03c76309f",
    MAIN_ASSET: "eedd8ff20861fd25ab543eaaa833a7177b9990829c09e573d9532d1a08717d3e",
    "Saved/RokUnityMigration/BatchReview20261009MiddleTipAndTileParticleGeometry/root_middle_tip_integration_v1.json":
        "3d2eb7cd1aa2dc16aa5a6ab8754b7226523e6182abf52eec02c76ce3390f63db",
    "Saved/RokUnityMigration/BatchReview20261009MiddleTipAndTileParticleGeometry/MiddleTipSourceReferenceV1/source_hierarchy_finding_v1.json":
        "b567652dd9394867ee0e209fd8b30c4d5b68bef4f821a0546d7a5d397a0cd7d3",
    "ref/client-unity/Assets/Scripts/Hotfix/Common/Utils/Tip.cs":
        "cc11cf269d5a0edae7bc050218315164fa5582e28d7ccaaa2e94501220a31938",
    "ref/client-unity/Assets/Scripts/Client/Compoment/UIComponent/AutoPlayAndDestroyTip.cs":
        "3e484caf64e17b3f896d53fdcf3e2d6da141ed48e97ffa1030a89f0d0ab4c8a2",
    "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Common_MidTips.prefab":
        "463874b7a16b4b139ec650fea180f49cfb86b546a4028252661cdc74f91e07be",
    "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UA_Common_TipInMid_On.anim":
        "f765a15bb47173462fe5f1cc1f88aa476345fe780978b95ab38ccf77797f5b58",
    "ref/client-unity/Assets/StreamingAssets/Config/Bin/Config.bin":
        "4dd2f3769da115cbb80ce4db876cd1d083345aaee194ebad878d7d700c74c4f8",
}
REQUIRED = [
    "Source/Rok/RokWorldPositionMiddleTipAuditSubsystem.h",
    "Source/Rok/RokWorldPositionMiddleTipAuditSubsystem.cpp",
    "Scripts/VerifyRokWorldPositionMiddleTipRuntime.py", UI_H, UI_CPP,
    "Source/Rok/RokMainHudWidget.cpp", "Source/Rok/RokWorldMapHudWidget.cpp",
    "Source/Rok/RokStrategyCameraPawn.cpp", "Source/Rok/RokWorldMapCameraPawn.cpp",
    "Source/Rok/RokWorldMapTypes.h", "Source/Rok/RokWorldMapTypes.cpp",
    "Source/Rok/RokWorldMapConfigActor.h", "Source/Rok/RokWorldMapConfigActor.cpp",
    "Source/Rok/RokWorldMapStreamingActor.h", "Source/Rok/RokWorldMapStreamingActor.cpp",
    "Source/Rok/RokWorldMapRuntimeDataAsset.h", "Source/Rok/RokWorldMapRuntimeDataAsset.cpp",
    "Source/Rok/RokServerSubsystem.h", "Source/Rok/RokServerSubsystem.cpp",
    "Source/Rok/RokUnityRuntimeStateSubsystem.h", "Source/Rok/RokUnityRuntimeStateSubsystem.cpp",
    "ref/client-unity/Assets/Scripts/Hotfix/Common/Utils/Tip.cs",
    "ref/client-unity/Assets/Scripts/Client/Compoment/UIComponent/AutoPlayAndDestroyTip.cs",
    "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Common_MidTips.prefab",
    "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UA_Common_TipInMid_On.anim",
    "ref/client-unity/Assets/StreamingAssets/Config/Bin/Config.bin",
    "Saved/RokUnityMigration/BatchReview20261009MiddleTipAndTileParticleGeometry/root_middle_tip_integration_v1.json",
    "Saved/RokUnityMigration/BatchReview20261009MiddleTipAndTileParticleGeometry/MiddleTipSourceReferenceV1/source_hierarchy_finding_v1.json",
    COMMON_ASSET, MAIN_ASSET,
]
PHASES = [
    "00_city_hud", "01_city_search_open", "02_city_x_empty_pre_go",
    "03_tip_first_frame", "04_tip_0133", "05_tip_0167", "06_tip_0500",
    "07_tip_2250", "08_tip_3100_collapsed", "09_city_valid_inputs",
    "10_world_after_valid_go",
]
ACTIONS = [
    ("button", "PositionSearchButton", None),
    ("edit", "WorldPositionXInput", ""),
    ("button", "WorldPositionGo", None),
    ("edit", "WorldPositionXInput", "100"),
    ("edit", "WorldPositionYInput", "50"),
    ("button", "WorldPositionGo", None),
]
PRECEDING_ACTIONS = [0, 1, 2, 3, 3, 3, 3, 3, 3, 5, 6]
TIMES = {3: (0, .10), 4: (.13333334, .09), 5: (.16666667, .10),
         6: (.5, .12), 7: (2.25, .15), 8: (3.1, .2)}


class Rejection(AssertionError):
    pass


def need(condition: bool, reason: str) -> None:
    if not condition:
        raise Rejection(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def near(a: object, b: float, tolerance: float = .1) -> bool:
    try:
        value = float(a)
        return math.isfinite(value) and abs(value - b) <= tolerance
    except (TypeError, ValueError):
        return False


def xy(obj: dict) -> tuple[float, float]:
    return float(obj["x"]), float(obj["y"])


def same_xy(obj: dict, expected: tuple[float, float], tolerance: float = .1) -> bool:
    return all(near(a, b, tolerance) for a, b in zip(xy(obj), expected))


def normalized(path: object) -> str:
    return str(path).replace("\\", "/").rstrip("/")


def decoded_png_bgra(width: int, height: int, channels: int, raw: bytes) -> bytes:
    """Decode real PNG scanlines, then reconstruct captured FColor BGRA bytes."""
    row_bytes = width * channels
    previous = bytearray(row_bytes)
    output = bytearray(width * height * 4)
    dst = 0
    for y in range(height):
        start = y * (row_bytes + 1)
        mode = raw[start]
        scanline = raw[start + 1:start + 1 + row_bytes]
        line = bytearray(row_bytes)
        for x, value in enumerate(scanline):
            a = line[x - channels] if x >= channels else 0
            b = previous[x]
            c = previous[x - channels] if x >= channels else 0
            if mode == 0:
                predictor = 0
            elif mode == 1:
                predictor = a
            elif mode == 2:
                predictor = b
            elif mode == 3:
                predictor = (a + b) // 2
            elif mode == 4:
                p = a + b - c
                distances = (abs(p-a), abs(p-b), abs(p-c))
                predictor = (a,b,c)[distances.index(min(distances))]
            else:
                raise Rejection("PNG scanline filter differs")
            line[x] = (value + predictor) & 255
        for x in range(width):
            offset = x * channels
            output[dst:dst+4] = bytes((line[offset+2], line[offset+1],
                                       line[offset], line[offset+3] if channels == 4 else 255))
            dst += 4
        previous = line
    return bytes(output)


def check_capture_provenance(capture: dict, last_capture_monotonic: float,
                             previous_write_monotonic: float) -> float:
    need(capture["capture_delegate"] == "UGameViewportClient::OnScreenshotCaptured"
         and capture["capture_pixel_format"] == "FColor_BGRA8_top_left_opaque"
         and (capture["capture_width"], capture["capture_height"],
              capture["capture_pixel_count"]) == (1440, 810, 1440 * 810)
         and capture["requested_frame"] <= capture["capture_frame"] <= capture["processed_frame"]
         and float(capture["capture_monotonic_seconds"]) > 0
         and capture["png_encoder"] == "FImageUtils::PNGCompressImageArray_BGRA8"
         and capture["png_write_phase"] == "Finish_after_all_11_paint_captures",
         "real Slate paint/deferred PNG provenance differs")
    write_time = float(capture["png_write_monotonic_seconds"])
    need(write_time >= last_capture_monotonic and write_time >= previous_write_monotonic,
         "PNG encoded before all original paint captures or write order reversed")
    return write_time


def check_png(capture: dict, run_dir: Path) -> None:
    path = Path(capture["png"])
    need(path.resolve().parent == run_dir.resolve(), "PNG outside unique run directory")
    payload = path.read_bytes()
    need(len(payload) > 4096 and payload[:8] == bytes.fromhex("89504e470d0a1a0a"),
         "PNG signature/size invalid")
    need(len(payload) == capture["png_bytes"] and sha(path) == capture["png_sha256"],
         "PNG byte count/hash differ")
    pos, image, data, ended = 8, None, bytearray(), False
    while pos + 12 <= len(payload):
        length = struct.unpack_from(">I", payload, pos)[0]
        need(length <= len(payload) - pos - 12, "PNG truncated chunk")
        kind = payload[pos + 4:pos + 8]
        body = payload[pos + 8:pos + 8 + length]
        crc = struct.unpack_from(">I", payload, pos + 8 + length)[0]
        need(zlib.crc32(kind + body) & 0xffffffff == crc, "PNG CRC differs")
        pos += length + 12
        if kind == b"IHDR":
            need(image is None and len(body) == 13, "PNG IHDR invalid")
            image = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            data.extend(body)
        elif kind == b"IEND":
            ended = True
            break
    need(image is not None and ended and pos == len(payload), "PNG incomplete")
    width, height, depth, color, compression, filtering, interlace = image
    need((width, height, depth) == (1440, 810, 8) and color in (2, 6)
         and (compression, filtering, interlace) == (0, 0, 0), "PNG dimensions/format invalid")
    channels = 3 if color == 2 else 4
    raw = zlib.decompress(bytes(data))
    stride = 1 + width * channels
    need(len(raw) == height * stride and all(raw[y * stride] <= 4 for y in range(height)),
         "PNG scanlines invalid")
    pixels = decoded_png_bgra(width, height, channels, raw)
    need(hashlib.sha256(pixels).hexdigest() == capture["raw_bgra_sha256"],
         "PNG decoded pixels differ from captured FColor BGRA bytes")
    need(all(pixels[index] == 255 for index in range(3, len(pixels), 4)),
         "PNG alpha differs from opaque source capture")


def check_inputs(inputs: dict) -> None:
    need(set(inputs) == set(REQUIRED), "source input set differs")
    for item in REQUIRED:
        path = resolve(item)
        need(path.is_file() and inputs[item] == sha(path), f"source pin differs: {item}")
        if item in APPROVED:
            need(inputs[item] == APPROVED[item], f"approved product source differs: {item}")
    audit = (resolve(REQUIRED[1])).read_text(encoding="utf-8-sig")
    forbidden = ["->FocusDisplayCoordinate(", "->RequestReturnToWorldMapAtDisplayCoordinate(",
                 "->SetViewCenterUnity(", "->SetText(", "->SetActorLocation(",
                 "->SetActorRotation(", "->HandleAction(", "->FlushTesting("]
    need(not any(token in audit for token in forbidden), "audit directly calls product setter/action")


def check_actions(actions: list, viewport_origin: tuple[float, float],
                  viewport_size: tuple[float, float]) -> None:
    need(len(actions) == len(ACTIONS), "action count differs")
    previous = -1
    for i, (action, (kind, selector, requested)) in enumerate(zip(actions, ACTIONS)):
        need((action.get("kind"), action.get("selector")) == (kind, selector),
             f"action {i} selector/order differs")
        need(action.get("frame", -1) >= previous, f"action {i} frame reversed")
        previous = action["frame"]
        need(action.get("own_game_window") is True and action.get("foreground") is True
             and action.get("target_in_hit_path") is True, f"action {i} lost own Slate hitpath")
        need(isinstance(action.get("hit_path_types"), list) and action["hit_path_types"],
             f"action {i} lacks Slate path")
        x, y = xy(action["absolute_center"])
        ox, oy = viewport_origin
        width, height = viewport_size
        need(0 <= x-ox <= width and 0 <= y-oy <= height,
             f"action {i} outside measured viewport client rect")
        if kind == "button":
            need(action.get("method") == "Slate mouse down/up"
                 and action.get("down_handled") is True and action.get("up_handled") is True
                 and "SButton" in action.get("terminal_type", ""),
                 f"action {i} did not dispatch real SButton press")
            if i == 2:
                need((action.get("x_before"), action.get("y_before")) == ("", "157")
                     and (action.get("x_after_dispatch"), action.get("y_after_dispatch"))
                     == ("", "157"), "empty Go changed raw coordinates")
            if i == 5:
                need((action.get("x_before"), action.get("y_before")) == ("100", "50"),
                     "valid Go did not see 100,50")
        else:
            need(action.get("requested_text") == requested
                 and action.get("text_after") == requested, f"action {i} typed wrong text")
            need("EditableText" in action.get("keyboard_focused_type", "")
                 and any("SEditableText" in t for t in action["hit_path_types"]),
                 f"action {i} lacked Slate editable focus")
            need(all(action.get(key) is True for key in
                     ("focus_down_handled", "focus_up_handled", "select_all_handled",
                      "all_characters_handled")), f"action {i} did not handle keyboard input")
            before, after = action.get("text_before"), action.get("text_after_erase")
            handled, noop = action.get("backspace_handled"), action.get("backspace_empty_noop")
            need(isinstance(before, str) and after == ""
                 and (handled is True or (before == "" and handled is False and noop is True))
                 and noop is (before == "" and handled is False),
                 f"action {i} did not genuinely clear input")


def curve(t: float, t0: float, v0: float, s0: float,
          t1: float, v1: float, s1: float) -> float:
    u = max(0.0, min(1.0, (t - t0) / (t1 - t0)))
    d = t1 - t0
    return ((2*u**3 - 3*u*u + 1)*v0 + (u**3 - 2*u*u + u)*s0*d
            + (-2*u**3 + 3*u*u)*v1 + (u**3-u*u)*s1*d)


def expected_transform(t: float) -> tuple[float, float, float]:
    scale = curve(t, 0, 0, 0, .13333334, 1, 0) if t < .13333334 else 1
    alpha = (curve(t, 0, .5, 0, .16666667, 1, 0) if t < .16666667
             else 1 if t <= 2.25 else curve(t, 2.25, 1, 0, 3, 0, 0))
    y = (curve(t, 0, -20, 0, 1.5, 5, 10) if t < 1.5
         else curve(t, 1.5, 5, 10, 2, 10, 0) if t < 2 else 10)
    return scale, alpha, -y


def check_tip(state: dict, phase: int, trigger_frame: int) -> None:
    tip = state["middle_tip"]
    root, bg, host, text = (tip["root"], tip["background"],
                            tip["text_host"], tip["text"])
    need(all(w["exists"] is True for w in (root, bg, text)), "MiddleTip widget missing")
    # UE5.8 ESlateVisibility: Visible=0, Collapsed=1, Hidden=2, HitTestInvisible=3.
    if phase < 3:
        need(root["visible"] is False, "MiddleTip visible before empty Go")
        return
    need(tip["trigger_frame"] == trigger_frame and state["frame"] > trigger_frame,
         "MiddleTip trigger/frame identity differs")
    need(tip["source_curve_clock"] == "WorldPositionMiddleTipElapsedSeconds_NativeTick"
         and math.isfinite(tip["seconds_since_trigger"])
         and math.isfinite(tip["monotonic_seconds_since_trigger"])
         and tip["seconds_since_trigger"] >= 0
         and tip["monotonic_seconds_since_trigger"] >= 0,
         "MiddleTip audit/monotonic clock absent")
    age = float(tip["ui_animation_elapsed_seconds"])
    if phase >= 8:
        need(root["visible"] is False and root["visibility_enum"] == 1
             and near(root["render_opacity"], 0, .001)
             and near(age, -1, .001),
             "MiddleTip did not naturally collapse at 3 seconds")
        return
    need(math.isfinite(age) and 0 <= age < 3,
         "MiddleTip widget NativeTick source clock absent")
    need(root["visible"] is True and root["visibility_enum"] == 3
         and bg["visible"] is True and text["visible"] is True
         and text["text"] == "未输入坐标", "MiddleTip content/visibility differs")
    need(near(text["font_size"], 16.5, .001)
         and same_xy(text["shadow_offset"], (1, 1), .01),
         "MiddleTip font/shadow source differs")
    need(all(near(text["text_color"][axis], 1, .001) for axis in ("r", "g", "b", "a"))
         and all(near(text["shadow_color"][axis], 0, .001) for axis in ("r", "g", "b"))
         and near(text["shadow_color"]["a"], .5, .002),
         "MiddleTip white text/black half-alpha shadow differs")
    need(bg["brush_resource"] == COMMON_OBJECT and bg["rhi_ready"] is True
         and same_xy(bg["texture_size"], (2048, 1024), .1)
         and same_xy(bg["rhi_extent"], (2048, 1024), .1),
         "MiddleTip did not use ready source atlas")
    need(same_xy(bg["uv_min"], (1/2048, 340/1024), .00001)
         and same_xy(bg["uv_max"], (602/2048, 409/1024), .00001),
         "MiddleTip source UV rect differs")
    need(near(bg["tint_alpha"], .60784316, .002), "MiddleTip image tint alpha differs")
    need(same_xy(root["local_size"], (640, 69), 1.1), "MiddleTip 640x69 root differs")
    need(tip["animated_group_widget_name"] == "WorldPositionMiddleTipRoot"
         and tip["same_animated_group"] is True
         and bg["parent_name"] == "WorldPositionMiddleTipRoot"
         and host["exists"] is True
         and host["parent_name"] == "WorldPositionMiddleTipRoot"
         and text["parent_name"] == "WorldPositionMiddleTipTextHost",
         "MiddleTip image/text do not share the animated root")
    root_slot = root["canvas_slot"]
    need(same_xy(root["render_pivot"], (.5, .5), .001)
         and same_xy(root_slot["anchors_min"], (.5, .5), .001)
         and same_xy(root_slot["anchors_max"], (.5, .5), .001)
         and same_xy(root_slot["alignment"], (.5, .5), .001)
         and same_xy(root_slot["position"], (0, 0), .01)
         and same_xy(root_slot["size"], (640, 69), .01),
         "MiddleTip animated root intrinsic layout differs")
    need(same_xy(bg["render_scale"], (1, 1), .001)
         and same_xy(text["render_scale"], (1, 1), .001)
         and same_xy(bg["render_translation"], (0, 0), .001)
         and same_xy(text["render_translation"], (0, 0), .001)
         and near(bg["render_opacity"], 1, .001)
         and near(text["render_opacity"], 1, .001),
         "MiddleTip child owns an animation instead of following the group")
    need(same_xy(host["render_scale"], (1, 1), .001)
         and same_xy(host["render_translation"], (0, 0), .001)
         and near(host["render_opacity"], 1, .001)
         and same_xy(host["local_size"], (640, 60), 1.1)
         and same_xy(host["canvas_slot"]["position"], (0, 4.5), .01)
         and same_xy(host["canvas_slot"]["size"], (640, 60), .01)
         and near(host["border_brush_color"]["a"], 0, .001),
         "MiddleTip centered transparent text host differs")
    need(same_xy(bg["canvas_slot"]["position"], ((640-bg["canvas_slot"]["size"]["x"])/2, 0), .01)
         and near(bg["canvas_slot"]["size"]["y"], 69, .01),
         "MiddleTip background intrinsic layout differs")
    bx, by = xy(bg["local_size"])
    need(50 <= bx <= 601 and near(by, 69, 1), "MiddleTip preferred+50 source background size differs")
    preferred = xy(text["desired_size"])[0]
    source_width = max(50, min(640, (preferred if preferred > 0 else 5*22) + 50))
    need(near(bx, source_width, 1.1), "MiddleTip background is not preferred text width+50")
    scale, alpha, translation = expected_transform(age)
    need(same_xy(root["render_scale"], (scale, scale), .14)
         and near(root["render_opacity"], alpha, .14)
         and near(root["render_translation"]["x"], 0, .01)
         and near(root["render_translation"]["y"], translation, 2.5),
         "MiddleTip animated group alpha/scale/Y differs from Show clip")
    # Only the processed snapshot is coupled to the screenshot's paint. Requested
    # and next-frame snapshots may carry the previous Slate geometry cache.
    if state["geometry_sample_stage"] == "screenshot_processed_after_paint":
        root_center = tuple((root["absolute_top_left"][a] + root["absolute_bottom_right"][a])/2
                            for a in ("x", "y"))
        ox, oy = xy(state["viewport_desktop_origin"])
        vw, vh = xy(state["viewport_local_size"])
        need(abs((root_center[0] - ox) - vw/2) <= 3
             and abs((root_center[1] - oy) - (vh/2 + root["render_translation"]["y"])) <= 3,
             "MiddleTip painted group center differs from measured viewport and own Y")
        if scale > .25:
            for child in (bg, host, text):
                child_center = tuple((child["absolute_top_left"][a] + child["absolute_bottom_right"][a])/2
                                     for a in ("x", "y"))
                need(all(abs(a-b) <= 3 for a, b in zip(root_center, child_center)),
                     "MiddleTip image/text did not follow common painted group geometry")


def check_state(state: dict, phase: int, baseline: dict, trigger_frame: int) -> None:
    world = phase == 10
    need(normalized(state["project"]).lower() == PROJECT.lower()
         and state["world"] == (WORLD if world else CITY)
         and state["pid"] == baseline["pid"], "project/map/PID changed")
    need(state["viewport_geometry_valid"] is True
         and same_xy(state["viewport_local_size"], (1440, 810), .5)
         and same_xy(state["viewport_desktop_origin"],
                     xy(baseline["viewport_desktop_origin"]), 1.0),
         "measured viewport/client desktop origin or size drifted")
    need(state["geometry_sample_stage"] in
         ("request_before_paint", "screenshot_processed_after_paint",
          "next_frame_before_paint", "final_state"),
         "geometry sampling stage absent")
    prepared = state["prepared_middle_atlas"]
    need(prepared["object_path"] == COMMON_OBJECT and prepared["rhi_ready"] is True
         and same_xy(prepared["rhi_extent"], (2048, 1024), .1),
         "MiddleTip source atlas was not RHI-ready before first HUD click")
    game = state["game_business"]
    need(game == baseline["game_business"] and game["profile_kind"] == "local_preview"
         and game["profile_has_role"] is False and game["profile_has_civilization"] is False
         and game["role_id"] == "" and game["http_dispatch_attempts"] == 0
         and game["http_suppressed_requests"] == 0, "account/business/HTTP changed")
    if world:
        need(state["world_hud_root_count"] == 1 and state["city_hud_root_count"] == 0
             and state["tactical_hud_count"] == 1 and state["tactical_owned_by_world_hud"] is True
             and state["screen_open"] is False, "world HUD ownership/travel differs")
        need(same_xy(state["view_center_display"], (100, 50), .1)
             and same_xy(state["view_center_unity"], (600, 300), .6)
             and state["world_coordinate_atlas"]["rhi_ready"] is True,
             "world camera/coordinate atlas differs")
        need(state["world_hud_coordinate"]["text"] == "#1   X:100 Y:50",
             "world HUD coordinate text is stale")
        projection = state["projection"]
        need(projection["source"] == "world_config_actor"
             and projection["config_actor_count"] == 1
             and near(projection["display_scale"], 6, .0001),
             "world projection source/config differs")
        city_wallet = baseline["world_business"]
        need(state["world_business"]["available"] is True
             and all(state["world_business"][key] == city_wallet[key] for key in
                     ("wallet_food", "wallet_wood", "wallet_stone", "wallet_gold",
                      "active_upgrade_count", "active_construction_count")),
             "world travel changed wallet/queues")
    else:
        need(state["city_hud_root_count"] == 1 and state["world_hud_root_count"] == 0
             and state["tactical_hud_count"] == 0 and state["unowned_main_hud_count"] == 0,
             "city HUD ownership differs")
        need(state["world_business"] == baseline["world_business"]
             and state["city_primary_press_count"] == baseline["city_primary_press_count"],
             "city wallet/queue/primary press changed")
        for field in ("city_camera_location", "city_camera_rotation"):
            need(all(near(state[field][axis], baseline[field][axis], .1)
                     for axis in ("x", "y", "z")), "city camera moved during empty Go")
        need(near(state["city_camera_dxf"], baseline["city_camera_dxf"], .1),
             "city zoom changed during empty Go")
        if phase:
            widgets = state["widgets"]
            need(state["screen_open"] is True and state["search_controls_visible"] is True,
                 "search window closed during MiddleTip")
            expected = ("1146", "157") if phase == 1 else (("100", "50") if phase == 9 else ("", "157"))
            need((widgets["WorldPositionXInput"]["text"],
                  widgets["WorldPositionYInput"]["text"]) == expected,
                 "coordinate input text changed")
            check_tip(state, phase, trigger_frame)


def verify(manifest_path: Path) -> dict:
    manifest_path = manifest_path.resolve()
    run_dir = manifest_path.parent
    doc = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    need(doc["schema"] == SCHEMA and doc["complete"] is True and doc["passed"] is True
         and doc["failures"] == [], "audit incomplete/failed")
    need(normalized(doc["project"]).lower() == PROJECT.lower() and doc["pid"] > 0,
         "manifest project/PID differs")
    need(run_dir.name.endswith(f"_{doc['pid']}"), "unique run/PID directory differs")
    check_inputs(doc["inputs_sha256"])
    actions, captures = doc["actions"], doc["captures"]
    need(len(captures) == len(PHASES), "capture count differs")
    need(doc["screenshot_capture_delegate"] == "UGameViewportClient::OnScreenshotCaptured"
         and doc["screenshot_delegate_cvar"] == 1
         and doc["png_deferred_until"] == "Finish_after_all_11_paint_captures"
         and doc["deferred_png_complete"] is True
         and doc["cached_raw_frames"] == len(PHASES),
         "capture delegate/deferred pixel provenance differs")
    last_capture_monotonic = float(captures[-1]["capture_monotonic_seconds"])
    baseline = captures[0]["processed_state"]
    check_actions(actions, xy(baseline["viewport_desktop_origin"]),
                  xy(baseline["viewport_local_size"]))
    trigger = int(doc["tip_trigger_frame"])
    need(trigger == actions[2]["frame"] and doc["tip_trigger_monotonic_seconds"] > 0
         and doc["tip_trigger_audit_elapsed_seconds"] > 0, "actual tip trigger frame/time absent")
    world_business = captures[10]["processed_state"]["world_business"]
    previous_frame = -1
    previous_write_monotonic = last_capture_monotonic
    for index, (cap, name) in enumerate(zip(captures, PHASES)):
        need(cap["phase"] == name, f"capture {index} phase/order differs")
        need(cap["requested_frame"] > previous_frame
             and cap["requested_frame"] <= cap["processed_frame"]
             and cap["processed_frame"] < cap["next_frame_state"]["frame"],
             f"capture {index} requested/processed/next frame order differs")
        previous_frame = cap["next_frame_state"]["frame"]
        previous_write_monotonic = check_capture_provenance(
            cap, last_capture_monotonic, previous_write_monotonic)
        check_png(cap, run_dir)
        for field in ("requested_state", "processed_state", "next_frame_state"):
            expected_stage = {"requested_state": "request_before_paint",
                              "processed_state": "screenshot_processed_after_paint",
                              "next_frame_state": "next_frame_before_paint"}[field]
            need(cap[field]["geometry_sample_stage"] == expected_stage,
                 f"capture {index} geometry stage differs: {field}")
            check_state(cap[field], index, baseline, trigger)
            if index == 10:
                need(cap[field]["world_business"] == world_business,
                     "world wallet/queues changed across capture frames")
        if index in TIMES:
            target, tolerance = TIMES[index]
            clock = (cap["requested_monotonic_seconds_since_tip"] if index == 8
                     else cap["requested_ui_animation_elapsed_seconds"])
            need(near(clock, target, tolerance),
                 f"MiddleTip {name} natural source-clock capture time differs")
            need(near(cap["requested_state"]["middle_tip"]["seconds_since_trigger"],
                      cap["requested_seconds_since_tip"], .025),
                 f"MiddleTip {name} audit-clock timestamp/state mismatch")
            need(near(cap["requested_state"]["middle_tip"][
                          "monotonic_seconds_since_trigger"],
                      cap["requested_monotonic_seconds_since_tip"], .025),
                 f"MiddleTip {name} monotonic timestamp/state mismatch")
            if index < 8:
                need(near(cap["requested_state"]["middle_tip"][
                              "ui_animation_elapsed_seconds"],
                          cap["requested_ui_animation_elapsed_seconds"], .001),
                     f"MiddleTip {name} widget-clock timestamp/state mismatch")
        need(cap["requested_state"]["frame"] >= actions[PRECEDING_ACTIONS[index]-1]["frame"]
             if PRECEDING_ACTIONS[index] else True, f"capture {index} preceded required action")
    check_state(doc["final_state"], 10, baseline, trigger)
    need(doc["final_state"]["world_business"] == world_business,
         "world wallet/queues changed after capture")
    need(doc["final_state"]["screen_open"] is False, "final search still open")
    return {"schema": "rok.world_position_middle_tip.runtime_verify.v4",
            "manifest_verified": True, "passed": True, "captures": len(captures),
            "actions": len(actions), "png": len(captures), "pid": doc["pid"],
            "manifest": str(manifest_path), "manifest_sha256": sha(manifest_path),
            "verifier_sha256": sha(Path(__file__))}


def self_test() -> dict:
    need(near(.133, .13333334, .01), "time comparison positive failed")
    need(not near(.5, .13333334, .01), "time comparison negative failed")
    need(expected_transform(0) == (0, .5, 20), "first curve sample differs")
    need(near(expected_transform(.13333334)[0], 1, .0001), "scale endpoint differs")
    need(near(expected_transform(.16666667)[1], 1, .0001), "alpha rise endpoint differs")
    need(near(expected_transform(2.25)[1], 1, .0001), "alpha hold endpoint differs")
    need(near(expected_transform(3)[1], 0, .0001), "alpha fade endpoint differs")
    rejected = 0
    def reject(call) -> None:
        nonlocal rejected
        try:
            call()
        except Rejection:
            rejected += 1
        else:
            raise AssertionError("negative mutation unexpectedly passed")
    reject(lambda: check_actions([{"kind": "edit", "selector": "WorldPositionXInput"}]
                                 * len(ACTIONS), (180, 140), (1440, 810)))
    reject(lambda: check_actions([], (180, 140), (1440, 810)))
    with tempfile.TemporaryDirectory() as temp:
        directory = Path(temp)
        def chunk(kind: bytes, body: bytes) -> bytes:
            return struct.pack(">I", len(body)) + kind + body + struct.pack(
                ">I", zlib.crc32(kind + body) & 0xffffffff)
        raw = b"\x00" * (810 * (1 + 1440 * 3))
        payload = (bytes.fromhex("89504e470d0a1a0a")
                   + chunk(b"IHDR", struct.pack(">IIBBBBB", 1440, 810, 8, 2, 0, 0, 0))
                   + chunk(b"IDAT", zlib.compress(raw, 1)) + chunk(b"IEND", b""))
        path = directory / "test.png"
        path.write_bytes(payload)
        capture = {"png": str(path), "png_bytes": len(payload),
                   "png_sha256": hashlib.sha256(payload).hexdigest(),
                   "raw_bgra_sha256": hashlib.sha256(bytes((0,0,0,255)) * (1440*810)).hexdigest()}
        check_png(capture, directory)
        reject(lambda: check_png({**capture, "png_sha256": "0"*64}, directory))
        reject(lambda: check_png({**capture, "raw_bgra_sha256": "0"*64}, directory))
        sample = {"capture_delegate": "UGameViewportClient::OnScreenshotCaptured",
                  "capture_pixel_format": "FColor_BGRA8_top_left_opaque",
                  "capture_width": 1440, "capture_height": 810,
                  "capture_pixel_count": 1440*810,
                  "requested_frame": 100, "capture_frame": 101,
                  "processed_frame": 101, "capture_monotonic_seconds": 10.1,
                  "png_encoder": "FImageUtils::PNGCompressImageArray_BGRA8",
                  "png_write_phase": "Finish_after_all_11_paint_captures",
                  "png_write_monotonic_seconds": 20.0}
        need(check_capture_provenance(sample, 19.0, 19.0) == 20.0,
             "capture provenance positive failed")
        for field, value in (("capture_delegate", "FScreenshotRequest::OnScreenshotCaptured"),
                             ("capture_width", 32), ("capture_pixel_count", 0),
                             ("capture_frame", 99), ("capture_monotonic_seconds", 0),
                             ("png_write_monotonic_seconds", 18.0)):
            reject(lambda field=field, value=value: check_capture_provenance(
                {**sample, field: value}, 19.0, 19.0))
        path.write_bytes(b"not PNG!" + payload[8:])
        capture["png_sha256"] = sha(path)
        reject(lambda: check_png(capture, directory))
        corrupt = bytearray(payload)
        corrupt[29] ^= 1
        path.write_bytes(corrupt)
        capture["png_sha256"] = sha(path)
        reject(lambda: check_png(capture, directory))
    age = .5
    scale, alpha, translation = expected_transform(age)
    root = {"exists": True, "visible": True, "visibility_enum": 3,
            "render_opacity": alpha, "local_size": {"x": 640, "y": 69},
            "absolute_top_left": {"x": 580, "y": 510.5 + translation},
            "absolute_bottom_right": {"x": 1220, "y": 579.5 + translation},
            "render_scale": {"x": scale, "y": scale},
            "render_translation": {"x": 0, "y": translation},
            "render_pivot": {"x": .5, "y": .5},
            "canvas_slot": {"anchors_min": {"x": .5, "y": .5},
                            "anchors_max": {"x": .5, "y": .5},
                            "alignment": {"x": .5, "y": .5},
                            "position": {"x": 0, "y": 0},
                            "size": {"x": 640, "y": 69}}}
    bg = {"exists": True, "visible": True, "brush_resource": COMMON_OBJECT,
          "rhi_ready": True, "texture_size": {"x": 2048, "y": 1024},
          "rhi_extent": {"x": 2048, "y": 1024},
          "uv_min": {"x": 1/2048, "y": 340/1024},
          "uv_max": {"x": 602/2048, "y": 409/1024},
          "tint_alpha": .60784316, "local_size": {"x": 160, "y": 69},
          "absolute_top_left": {"x": 820, "y": 510.5 + translation},
          "absolute_bottom_right": {"x": 980, "y": 579.5 + translation},
          "render_scale": {"x": 1, "y": 1},
          "render_translation": {"x": 0, "y": 0},
          "render_opacity": 1, "parent_name": "WorldPositionMiddleTipRoot",
          "canvas_slot": {"position": {"x": 240, "y": 0},
                          "size": {"x": 160, "y": 69}}}
    host = {"exists": True, "visible": True,
            "parent_name": "WorldPositionMiddleTipRoot",
            "local_size": {"x": 640, "y": 60},
            "absolute_top_left": {"x": 580, "y": 515 + translation},
            "absolute_bottom_right": {"x": 1220, "y": 575 + translation},
            "render_scale": {"x": 1, "y": 1},
            "render_translation": {"x": 0, "y": 0},
            "render_opacity": 1,
            "border_brush_color": {"r": 0, "g": 0, "b": 0, "a": 0},
            "canvas_slot": {"position": {"x": 0, "y": 4.5},
                            "size": {"x": 640, "y": 60}}}
    text = {"exists": True, "visible": True, "text": "未输入坐标",
            "font_size": 16.5, "shadow_offset": {"x": 1, "y": 1},
            "text_color": dict.fromkeys("rgba", 1),
            "shadow_color": {"r": 0, "g": 0, "b": 0, "a": .5},
            "desired_size": {"x": 110, "y": 30},
            "absolute_top_left": {"x": 845, "y": 532.5 + translation},
            "absolute_bottom_right": {"x": 955, "y": 557.5 + translation},
            "render_scale": {"x": 1, "y": 1},
            "render_translation": {"x": 0, "y": 0},
            "render_opacity": 1, "parent_name": "WorldPositionMiddleTipTextHost"}
    state = {"middle_tip": {"root": root, "background": bg,
                            "text_host": host, "text": text,
                            "animated_group_widget_name": "WorldPositionMiddleTipRoot",
                            "same_animated_group": True,
                            "trigger_frame": 100, "seconds_since_trigger": age,
                            "monotonic_seconds_since_trigger": age,
                            "ui_animation_elapsed_seconds": age,
                            "source_curve_clock": "WorldPositionMiddleTipElapsedSeconds_NativeTick"},
             "frame": 101, "geometry_sample_stage": "screenshot_processed_after_paint",
             "viewport_desktop_origin": {"x": 180, "y": 140},
             "viewport_local_size": {"x": 1440, "y": 810}}
    check_tip(state, 6, 100)
    for target, field, value in (("root", "visibility_enum", 0),
                                 ("root", "render_opacity", .1),
                                 ("root", "render_scale", {"x": 0, "y": 0}),
                                 ("root", "render_translation", {"x": 0, "y": 0}),
                                 ("background", "parent_name", "WrongRoot"),
                                 ("background", "render_scale", {"x": .7, "y": .7}),
                                 ("text", "parent_name", "WrongRoot"),
                                 ("text", "render_translation", {"x": 0, "y": 8}),
                                 ("background", "brush_resource", "/Engine/BasicShapes"),
                                 ("background", "rhi_ready", False),
                                 ("background", "uv_min", {"x": .1, "y": .1}),
                                 ("text", "text", "wrong")):
        broken = copy.deepcopy(state)
        broken["middle_tip"][target][field] = value
        reject(lambda broken=broken: check_tip(broken, 6, 100))
    for field, value in (("ui_animation_elapsed_seconds", .1),
                         ("source_curve_clock", "audit_tick"),
                         ("monotonic_seconds_since_trigger", -1)):
        broken = copy.deepcopy(state)
        broken["middle_tip"][field] = value
        reject(lambda broken=broken: check_tip(broken, 6, 100))
    broken = copy.deepcopy(state)
    broken["viewport_desktop_origin"] = {"x": 0, "y": 0}
    reject(lambda: check_tip(broken, 6, 100))
    broken = copy.deepcopy(state)
    broken["middle_tip"]["text_host"]["parent_name"] = "WrongRoot"
    reject(lambda: check_tip(broken, 6, 100))
    return {"passed": True, "positive_cases": 8, "negative_cases": rejected}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    need(args.manifest is not None or args.self_test, "provide manifest or --self-test")
    try:
        result = verify(args.manifest) if args.manifest else {"manifest_verified": False}
        if args.self_test:
            result["self_test"] = self_test()
        result["mode"] = "manifest_verify" if args.manifest else "self_test_only"
        result["passed"] = True
    except (Rejection, KeyError, ValueError, OSError, zlib.error) as exc:
        result = {"passed": False, "manifest_verified": False, "error": str(exc)}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        with args.report.open("x", encoding="utf-8") as stream:
            json.dump(result, stream, ensure_ascii=False, indent=2)
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
