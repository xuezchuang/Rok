"""Fail-closed verification of real city/world HUD coordinate-search input."""

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

ROOT = Path(__file__).resolve().parents[1]
PROJECT = "D:/ueDemo/Rok"
CITY = "/Game/Maps/L_RokUnityGameAppCity"
WORLD = "/Game/Maps/L_RokUnityWorldMapPrototype"
SCHEMA = "rok.world_position_search.runtime_audit.v1"
BATCH = "Saved/RokUnityMigration/BatchReview20261009WorldSearchAndTileEffects"
UI_H = "Source/Rok/RokUnityFeatureScreenWidget.h"
UI_CPP = "Source/Rok/RokUnityFeatureScreenWidget.cpp"
UI_REPORT = f"{BATCH}/NextBatch/WorldSearchSource/implementation_revision2.json"
SOURCE_MANIFEST = f"{BATCH}/WorldSearchSource/manifest.json"
SOURCE_CONTRACT = f"{BATCH}/WorldSearchSource/new coordinate_behavior_contract_v1.json"
APPROVED = {
    UI_H: "0cfc67fc51e0ed174554d7872705a87c7689fd8ede3f3bd0f570a26bd1975d01",
    UI_CPP: "8e36689c2897d1396f49d562055ab303752f7c53553bb204c088ee873f2e44fe",
    UI_REPORT: "32a547c86f7820428679f202a53f0f694fa011e028d21ef9a11d4e3545a9b70b",
    SOURCE_MANIFEST: "1d75c0cebc8e037b7c8e3e08c4d4c9416daa3d2bc9fcdcbfe5e7f050a6119b9a",
    SOURCE_CONTRACT: "c737b25ad688e9a16e1bf5913d4e8461480d686c4f9d20120c0415fea011ed6c",
    "Saved/RokUnityMigration/unity_world_map_streaming_data.json":
        "72dd593850c582b4358a1f86f347391ea135f6f3edc25a50da7222daac2c3ba2",
    "Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.uasset":
        "eedd8ff20861fd25ab543eaaa833a7177b9990829c09e573d9532d1a08717d3e",
}
REQUIRED = [
    "Source/Rok/RokWorldPositionSearchAuditSubsystem.h",
    "Source/Rok/RokWorldPositionSearchAuditSubsystem.cpp",
    "Scripts/VerifyRokWorldPositionSearchRuntime.py",
    UI_H, UI_CPP, "Source/Rok/RokMainHudWidget.cpp",
    "Source/Rok/RokWorldMapHudWidget.cpp", "Source/Rok/RokStrategyCameraPawn.cpp",
    "Source/Rok/RokWorldMapCameraPawn.cpp", "Source/Rok/RokWorldMapTypes.h",
    "Source/Rok/RokWorldMapTypes.cpp", "Source/Rok/RokWorldMapConfigActor.h",
    "Source/Rok/RokWorldMapConfigActor.cpp", "Source/Rok/RokWorldMapStreamingActor.h",
    "Source/Rok/RokWorldMapStreamingActor.cpp", "Source/Rok/RokWorldMapRuntimeDataAsset.h",
    "Source/Rok/RokWorldMapRuntimeDataAsset.cpp", "Source/Rok/RokServerSubsystem.h",
    "Source/Rok/RokServerSubsystem.cpp", "Source/Rok/RokUnityRuntimeStateSubsystem.h",
    "Source/Rok/RokUnityRuntimeStateSubsystem.cpp", UI_REPORT, SOURCE_MANIFEST,
    SOURCE_CONTRACT, "Saved/RokUnityMigration/unity_world_map_streaming_data.json",
    "Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.uasset",
]
PHASES = [
    "00_city_hud", "01_city_search_open", "02_city_x_empty_pre_go",
    "03_city_empty_tip", "04_city_valid_inputs", "05_world_after_city_go",
    "06_world_search_open", "07_world_y_empty_pre_go", "08_world_empty_tip",
    "09_world_malformed_alpha", "10_world_malformed_decimal", "11_world_overflow",
    "12_world_bounds_0_1200", "13_world_bound_focus_0_1200",
    "14_world_bounds_1200_0", "15_world_bound_focus_1200_0", "16_closed",
]
ACTIONS = [
    ("button", "PositionSearchButton", None),
    ("edit", "WorldPositionXInput", ""),
    ("button", "WorldPositionGo", None),
    ("edit", "WorldPositionXInput", "100"),
    ("edit", "WorldPositionYInput", "50"),
    ("button", "WorldPositionGo", None),
    ("button", "WorldMapCoordinateSearchButton", None),
    ("edit", "WorldPositionYInput", ""),
    ("button", "WorldPositionGo", None),
    ("edit", "WorldPositionYInput", "50"),
    ("edit", "WorldPositionXInput", "12abc"),
    ("button", "WorldPositionGo", None),
    ("edit", "WorldPositionXInput", "12.5"),
    ("button", "WorldPositionGo", None),
    ("edit", "WorldPositionXInput", "2147483648"),
    ("button", "WorldPositionGo", None),
    ("edit", "WorldPositionXInput", "0"),
    ("edit", "WorldPositionYInput", "1200"),
    ("button", "WorldPositionGo", None),
    ("button", "WorldMapCoordinateSearchButton", None),
    ("edit", "WorldPositionXInput", "1200"),
    ("edit", "WorldPositionYInput", "0"),
    ("button", "WorldPositionGo", None),
    ("button", "WorldMapCoordinateSearchButton", None),
    ("button", "FeatureFrameClose", None),
]
GO_INPUTS = {
    2: ("", "157"), 5: ("100", "50"), 8: ("100", ""),
    11: ("12abc", "50"), 13: ("12.5", "50"),
    15: ("2147483648", "50"), 18: ("0", "1200"),
    22: ("1200", "0"),
}
EXPECTED_TEXT = {
    1: ("1146", "157"), 2: ("", "157"), 3: ("", "157"),
    4: ("100", "50"), 6: ("100", "50"), 7: ("100", ""),
    8: ("100", ""), 9: ("12abc", "50"), 10: ("12.5", "50"),
    11: ("2147483648", "50"), 12: ("0", "1200"),
    14: ("1200", "0"),
}
OPEN_PHASES = set(EXPECTED_TEXT)
PRECEDING_ACTION_COUNTS = [0, 1, 2, 3, 5, 6, 7, 8, 9, 12, 14, 16, 18, 19, 22, 23, 25]
TIP_PHASES = {3: "未输入坐标", 8: "未输入坐标",
              9: "请输入有效坐标。", 10: "请输入有效坐标。",
              11: "请输入有效坐标。"}


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
    return near(obj["x"], expected[0], tolerance) and near(obj["y"], expected[1], tolerance)


def normalized(path: object) -> str:
    return str(path).replace("\\", "/").rstrip("/")


def check_png(capture: dict, run_dir: Path) -> None:
    path = Path(capture["png"])
    need(path.resolve().parent == run_dir.resolve(), "PNG outside unique run directory")
    payload = path.read_bytes()
    need(len(payload) > 4096 and payload[:8] == bytes.fromhex("89504e470d0a1a0a"),
         "PNG signature/size invalid")
    need(len(payload) == capture["png_bytes"] and sha(path) == capture["png_sha256"],
         "PNG bytes/hash differ")
    pos, image, idat, ended = 8, None, bytearray(), False
    while pos + 12 <= len(payload):
        length = struct.unpack_from(">I", payload, pos)[0]
        need(length <= len(payload) - pos - 12, "PNG chunk truncated")
        kind = payload[pos + 4:pos + 8]
        content = payload[pos + 8:pos + 8 + length]
        crc = struct.unpack_from(">I", payload, pos + 8 + length)[0]
        need(zlib.crc32(kind + content) & 0xffffffff == crc, "PNG CRC invalid")
        pos += length + 12
        if kind == b"IHDR":
            need(image is None and len(content) == 13, "PNG IHDR invalid")
            image = struct.unpack(">IIBBBBB", content)
        elif kind == b"IDAT":
            idat.extend(content)
        elif kind == b"IEND":
            ended = True
            break
    need(image is not None and ended and pos == len(payload), "PNG structure incomplete")
    width, height, depth, color, compression, filtering, interlace = image
    need((width, height) == (1440, 810) and depth == 8 and color in (2, 6)
         and (compression, filtering, interlace) == (0, 0, 0), "PNG real 1440x810 format invalid")
    channels = 3 if color == 2 else 4
    raw = zlib.decompress(bytes(idat))
    row = 1 + width * channels
    need(len(raw) == height * row and all(raw[y * row] <= 4 for y in range(height)),
         "PNG pixel scanlines invalid")


def check_inputs(inputs: dict) -> None:
    need(set(inputs) == set(REQUIRED), "source input set changed")
    for item in REQUIRED:
        path = ROOT / item
        need(path.is_file() and inputs[item] == sha(path), f"source pin differs: {item}")
        if item in APPROVED:
            need(inputs[item] == APPROVED[item], f"approved UI source changed: {item}")
    cpp = (ROOT / REQUIRED[1]).read_text(encoding="utf-8")
    forbidden = ["->FocusDisplayCoordinate(", "->RequestReturnToWorldMapAtDisplayCoordinate(",
                 "->SetViewCenterUnity(", "->SetText(", "->SetActorLocation(",
                 "->SetActorRotation(", "->HandleAction(", "->FlushTesting("]
    need(not any(token in cpp for token in forbidden), "audit directly invokes product setter/action")


def check_edit_deletion(action: dict, index: int) -> None:
    before = action.get("text_before")
    erased = action.get("text_after_erase")
    handled = action.get("backspace_handled")
    noop = action.get("backspace_empty_noop")
    need(isinstance(before, str) and erased == "", f"edit {index} did not clear text before typing")
    need(handled is True or (before == "" and handled is False and noop is True),
         f"edit {index} Backspace did not delete nonempty text")
    need(noop is (before == "" and handled is False),
         f"edit {index} empty Backspace no-op flag is forged")


def check_actions(actions: list, pid: int) -> None:
    need(len(actions) == len(ACTIONS), "action count differs")
    previous = -1
    for i, (actual, expected) in enumerate(zip(actions, ACTIONS)):
        kind, selector, value = expected
        need((actual.get("kind"), actual.get("selector")) == (kind, selector),
             f"action {i} identity/order differs")
        need(actual.get("frame", -1) >= previous, f"action {i} frame reversed")
        previous = actual["frame"]
        need(actual.get("own_game_window") is True and actual.get("foreground") is True
             and actual.get("target_in_hit_path") is True, f"action {i} wrong window/hit path")
        types = actual.get("hit_path_types", [])
        need(isinstance(types, list) and types, f"action {i} no Slate path")
        cx, cy = xy(actual["absolute_center"])
        need(0 <= cx <= 1440 and 0 <= cy <= 810, f"action {i} outside game client")
        if kind == "button":
            need(actual.get("method") == "Slate mouse down/up"
                 and actual.get("down_handled") is True and actual.get("up_handled") is True
                 and "SButton" in actual.get("terminal_type", ""),
                 f"action {i} lacked genuine button down/up")
            if i in GO_INPUTS:
                x, y = GO_INPUTS[i]
                need((actual.get("x_before"), actual.get("y_before")) == (x, y),
                     f"Go action {i} did not see expected raw text before focus loss")
                if i in (2, 8, 11, 13, 15):
                    need((actual.get("x_after_dispatch"), actual.get("y_after_dispatch"))
                         == (x, y), f"invalid Go action {i} changed raw text on commit")
        else:
            need(actual.get("requested_text") == value and actual.get("text_after") == value,
                 f"action {i} typed value differs")
            need("EditableText" in actual.get("keyboard_focused_type", "")
                 and any("SEditableText" in typ for typ in types),
                 f"action {i} lacked genuine edit focus/path")
            for field in ("focus_down_handled", "focus_up_handled", "select_all_handled",
                          "all_characters_handled"):
                need(actual.get(field) is True, f"action {i} {field} false")
            check_edit_deletion(actual, i)
    need(pid > 0, "process identity invalid")


def check_projection(proj: dict, world: bool) -> None:
    need(proj["valid"] is True and near(proj["display_scale"], 6, .0001),
         "world projection invalid or scale not six")
    need(same_xy(proj["unity_min"], (0, 0), .001)
         and same_xy(proj["unity_max"], (7200, 7200), .001)
         and same_xy(proj["display_min"], (0, 0), .001)
         and same_xy(proj["display_max"], (1200, 1200), .001),
         "world projection bounds differ")
    if world:
        need(proj["source"] == "world_config_actor" and proj["config_actor_count"] == 1
             and proj["config_actor_path"].startswith("/Game/Maps/"),
             "world map projection not sourced from unique config actor")
    else:
        need(proj["source"] == "local_projection_fallback"
             and proj["config_actor_count"] == 0, "city unexpectedly uses map actor")


def check_streaming_dependency(dependency: dict, world: bool) -> None:
    actors = dependency["actor_paths"]
    assets = dependency["loaded_runtime_data_asset_paths"]
    need(dependency["actor_count"] == len(actors)
         and len(actors) <= 1 and len(assets) <= len(actors),
         "streaming actor/data asset identity ambiguous")
    need(all(isinstance(x, str) and x.startswith("/Game/") for x in actors)
         and all(isinstance(x, str) and x.startswith("/Game/") for x in assets),
         "streaming dependency is not a loaded project object")
    if not world:
        need(not actors and not assets, "city unexpectedly loaded streaming actor")


def check_camera(state: dict, phase: int, first_city: dict) -> None:
    if phase <= 4:
        need(state["city_primary_press_count"] == first_city["city_primary_press_count"],
             "city PrimaryPress changed")
        for field in ("city_camera_location", "city_camera_rotation"):
            for axis in ("x", "y", "z"):
                need(near(state[field][axis], first_city[field][axis], .1),
                     f"city camera {field}.{axis} changed")
        need(near(state["city_camera_dxf"], first_city["city_camera_dxf"], .1),
             "city camera zoom changed")
        need("StrategyCameraPawn" in state["pawn_class"], "city pawn identity differs")
    else:
        need("WorldMapCameraPawn" in state["pawn_class"], "world pawn identity differs")
        display = (100, 50) if phase <= 12 else ((0, 1200) if phase <= 14 else (1200, 0))
        unity = tuple(x * 6 for x in display)
        need(same_xy(state["view_center_display"], display, .1)
             and same_xy(state["view_center_unity"], unity, .6),
             f"camera display/Unity sixfold center differs in phase {phase}")


def check_widget(widget: dict, name: str, visible: bool) -> None:
    need(widget["exists"] is True, f"{name} missing")
    if visible:
        need(widget["visible"] is True and widget["enabled"] is True,
             f"{name} not interactive")
        ax, ay = xy(widget["absolute_top_left"])
        bx, by = xy(widget["absolute_bottom_right"])
        need(0 <= ax < bx <= 1441 and 0 <= ay < by <= 811,
             f"{name} geometry outside client")


def check_raw_text(widgets: dict, phase: int) -> None:
    if phase in EXPECTED_TEXT:
        actual = (widgets["WorldPositionXInput"]["text"],
                  widgets["WorldPositionYInput"]["text"])
        need(actual == EXPECTED_TEXT[phase], f"state {phase} input raw text differs")


def check_identity_business(state: dict, phase: int, pid: int, game: dict,
                            city_business: dict, world_business: dict) -> None:
    world = phase >= 5
    need(normalized(state["project"]).lower() == PROJECT.lower() and state["pid"] == pid,
         f"state {phase} project/process identity differs")
    need(state["world"] == (WORLD if world else CITY), f"state {phase} map identity differs")
    need(state["game_business"] == game and game["profile_kind"] == "local_preview"
         and game["role_id"] == "" and game["civilization_id"] == 0
         and game["profile_has_role"] is False
         and game["profile_has_civilization"] is False
         and game["profile_civilization_name"] == ""
         and game["http_dispatch_attempts"] == 0
         and game["http_suppressed_requests"] == 0,
         f"state {phase} changed account/business/HTTP")
    need(state["world_business"] == (world_business if world else city_business),
         f"state {phase} changed wallet/queues")
    need(state["world_business"]["available"] is True,
         f"state {phase} lacks business subsystem")


def check_hud_identity(state: dict, world: bool) -> None:
    if world:
        need(state["world_hud_visible"] is True and state["city_hud_visible"] is False
             and state["city_hud_root_count"] == 0
             and state["world_hud_root_count"] == 1
             and state["tactical_hud_count"] == 1
             and state["unowned_main_hud_count"] == 0
             and state["city_hud_root_path"] == ""
             and state["world_hud_root_path"].startswith("/Game/")
             and "WorldMapTacticalHud" in state["tactical_hud_path"]
             and state["tactical_hud_in_viewport"] is False
             and state["tactical_owned_by_world_hud"] is True,
             "world HUD/root/tactical ownership differs")
    else:
        need(state["city_hud_visible"] is True
             and state["world_hud_visible"] is False
             and state["city_hud_root_count"] == 1
             and state["world_hud_root_count"] == 0
             and state["tactical_hud_count"] == 0
             and state["unowned_main_hud_count"] == 0
             and state["city_hud_root_path"].startswith("/Game/")
             and state["world_hud_root_path"] == ""
             and state["tactical_hud_path"] == "",
             "city top-level HUD identity differs")


def check_world_hud_ready(state: dict, phase: int) -> None:
    if phase < 5:
        return
    center = (100, 50) if phase <= 12 else ((0, 1200) if phase <= 14 else (1200, 0))
    label = state["world_hud_coordinate"]
    check_widget(label, "WorldMapCoordinateText", True)
    need(label["text"] == f"#1   X:{center[0]} Y:{center[1]}",
         f"phase {phase} world HUD coordinate lags camera")
    atlas = state["world_coordinate_atlas"]
    need(atlas["object_path"] == "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"
         and same_xy(atlas["source_size"], (1024, 1024), .01)
         and atlas["rhi_ready"] is True
         and same_xy(atlas["rhi_extent"], (1024, 1024), .01),
         "world coordinate atlas is not fully resident at source extent")
    need(state["world_ready_since_seconds"] >= 0
         and state["audit_elapsed_seconds"] - state["world_ready_since_seconds"] >= 1.95,
         "world HUD captured before natural stable resource interval")


def check_state(state: dict, phase: int, pid: int, first_city: dict,
                game: dict, city_business: dict, world_business: dict) -> None:
    world = phase >= 5
    check_identity_business(state, phase, pid, game, city_business, world_business)
    check_hud_identity(state, world)
    check_world_hud_ready(state, phase)
    need(state["screen_open"] is (phase in OPEN_PHASES)
         and state["search_controls_visible"] is (phase in OPEN_PHASES),
         f"state {phase} modal/open identity differs")
    check_projection(state["projection"], world)
    check_streaming_dependency(state["streaming_dependency"], world)
    check_camera(state, phase, first_city)
    widgets = state["widgets"]
    if phase in OPEN_PHASES:
        for name in ("WorldPositionXInput", "WorldPositionYInput", "WorldPositionGo",
                     "WorldPositionKingdomValue", "FeatureBoundaryFeedback", "FeatureFrameClose"):
            check_widget(widgets[name], name,
                         name != "FeatureBoundaryFeedback" or phase in TIP_PHASES)
    if phase in EXPECTED_TEXT:
        check_raw_text(widgets, phase)
        need(widgets["WorldPositionKingdomValue"]["visible"] is True,
             f"state {phase} kingdom label absent")
    if phase in TIP_PHASES:
        need(widgets["FeatureBoundaryFeedback"]["visible"] is True
             and widgets["FeatureBoundaryFeedback"]["text"] == TIP_PHASES[phase],
             f"state {phase} source tip/local invalid feedback differs")


def check_capture_timeline(captures: list, actions: list) -> None:
    need(len(captures) == len(PHASES)
         and [item["phase"] for item in captures] == PHASES,
         "capture count/order differs")
    last_frame = -1
    for i, capture in enumerate(captures):
        frames = [capture["requested_frame"], capture["processed_frame"],
                  capture["next_frame_state"]["frame"]]
        need(last_frame <= frames[0] <= frames[1] < frames[2],
             f"capture {i} not requested/processed/next natural frames")
        last_frame = frames[2]
        count = PRECEDING_ACTION_COUNTS[i]
        if count:
            need(actions[count - 1]["frame"] <= frames[0],
                 f"capture {i} precedes its real action")
        if count < len(actions):
            need(actions[count]["frame"] >= frames[2],
                 f"next input replayed before capture {i} completed")


def verify(manifest_path: Path) -> dict:
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    need(data.get("schema") == SCHEMA and data.get("complete") is True
         and data.get("passed") is True and data.get("failures") == [],
         "audit manifest incomplete/failed")
    need(normalized(data.get("project", "")).lower() == PROJECT.lower(),
         "audit project differs")
    pid = data["pid"]
    check_inputs(data["inputs_sha256"])
    actions, captures = data["actions"], data["captures"]
    check_actions(actions, pid)
    check_capture_timeline(captures, actions)
    need(data.get("input_attempts") is not None, "pre-dispatch diagnostic trace absent")
    first_city = captures[0]["next_frame_state"]
    game = first_city["game_business"]
    city_business = first_city["world_business"]
    world_business = captures[5]["next_frame_state"]["world_business"]
    for i, capture in enumerate(captures):
        frames = [capture["requested_frame"], capture["processed_frame"],
                  capture["next_frame_state"]["frame"]]
        check_png(capture, manifest_path.parent)
        for key in ("requested_state", "processed_state", "next_frame_state"):
            state = capture[key]
            need(state["frame"] == (frames[0] if key == "requested_state" else
                 (frames[1] if key == "processed_state" else frames[2])),
                 f"capture {i} {key} frame differs")
            check_state(state, i, pid, first_city, game, city_business, world_business)
    check_state(data["final_state"], 16, pid, first_city,
                game, city_business, world_business)
    need(data["final_state"]["frame"] >= captures[-1]["next_frame_state"]["frame"],
         "final state predates final capture")
    return {"passed": True, "captures": len(captures), "actions": len(actions),
            "city_to_world": True, "source_tip_100716": True,
            "projection_display_to_unity": "6x_once", "http_dispatch": 0}


def run_mode(manifest_path: Path | None, with_self_test: bool) -> dict:
    if manifest_path is not None:
        resolved = manifest_path.resolve()
        result = verify(resolved)  # A supplied manifest always runs the real gate first.
        result["mode"] = "manifest_with_self_test" if with_self_test else "manifest"
        result["manifest_verified"] = True
        result["manifest"] = str(resolved)
        if with_self_test:
            result["self_test"] = self_test()
        return result
    need(with_self_test, "manifest path required unless --self-test is set")
    return {"mode": "self_test_only", "manifest_verified": False, **self_test()}


def write_report_exclusive(path: Path, result: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8", newline="\n") as report:
        report.write(json.dumps(result, ensure_ascii=False, indent=2) + "\n")


def self_test() -> dict:
    # Negative tests exercise independent fail-closed gates, without fabricating a runtime pass.
    def rejects(fn, reason):
        try:
            fn()
        except (Rejection, KeyError, ValueError, zlib.error, OSError, IndexError):
            return
        raise AssertionError(f"negative case accepted: {reason}")

    def chunk(kind: bytes, content: bytes) -> bytes:
        return (struct.pack(">I", len(content)) + kind + content
                + struct.pack(">I", zlib.crc32(kind + content) & 0xffffffff))

    with tempfile.TemporaryDirectory() as directory:
        folder = Path(directory)
        image = (bytes.fromhex("89504e470d0a1a0a")
                 + chunk(b"IHDR", struct.pack(">IIBBBBB", 1440, 810, 8, 6, 0, 0, 0))
                 + chunk(b"IDAT", zlib.compress(bytes((1 + 1440 * 4) * 810)))
                 + chunk(b"IEND", b""))
        path = folder / "synthetic.png"
        path.write_bytes(image)
        record = {"png": str(path), "png_bytes": len(image),
                  "png_sha256": sha(path)}
        check_png(record, folder)
        path.write_bytes(b"BADPNG!!" + image[8:])
        record["png_sha256"] = sha(path)
        rejects(lambda: check_png(record, folder), "PNG signature")
        path.write_bytes(image)
        record["png_sha256"] = "0" * 64
        rejects(lambda: check_png(record, folder), "PNG hash")
        corrupt = bytearray(image)
        corrupt[29] ^= 1  # IHDR CRC remains the source value.
        path.write_bytes(corrupt)
        record["png_sha256"] = sha(path)
        rejects(lambda: check_png(record, folder), "PNG CRC")
        path.unlink()
        rejects(lambda: check_png(record, folder), "PNG missing")
        bad_manifest = folder / "bad_manifest.json"
        bad_manifest.write_text('{"schema":"not_the_runtime_audit"}', encoding="utf-8")
        rejects(lambda: run_mode(bad_manifest, True), "--self-test bypassed supplied manifest")
        report_path = folder / "report.json"
        write_report_exclusive(report_path, {"mode": "self_test_only"})
        rejects(lambda: write_report_exclusive(report_path, {"mode": "manifest"}),
                "--report overwrote prior result")
    rejects(lambda: check_actions([], 123), "missing actions")
    world_hud = {"world_hud_visible": True, "city_hud_visible": False,
                 "city_hud_root_count": 0, "world_hud_root_count": 1,
                 "tactical_hud_count": 1, "unowned_main_hud_count": 0,
                 "city_hud_root_path": "", "world_hud_root_path": "/Game/Maps/World.Hud",
                 "tactical_hud_path": "/Game/Maps/World.Hud.WorldMapTacticalHud",
                 "tactical_hud_in_viewport": False,
                 "tactical_owned_by_world_hud": True}
    check_hud_identity(world_hud, True)
    bad = copy.deepcopy(world_hud)
    bad["world_hud_visible"] = False
    rejects(lambda: check_hud_identity(bad, True), "world HUD absent")
    bad = copy.deepcopy(world_hud)
    bad["city_hud_root_count"] = 1
    rejects(lambda: check_hud_identity(bad, True), "double top-level HUD")
    bad = copy.deepcopy(world_hud)
    bad["tactical_owned_by_world_hud"] = False
    rejects(lambda: check_hud_identity(bad, True), "wrong tactical ownership")
    city_hud = {"world_hud_visible": False, "city_hud_visible": True,
                "city_hud_root_count": 1, "world_hud_root_count": 0,
                "tactical_hud_count": 0, "unowned_main_hud_count": 0,
                "city_hud_root_path": "/Game/Maps/City.Hud",
                "world_hud_root_path": "", "tactical_hud_path": ""}
    check_hud_identity(city_hud, False)
    bad = copy.deepcopy(city_hud)
    bad["city_hud_root_count"] = 0
    rejects(lambda: check_hud_identity(bad, False), "city HUD absent")
    world_ready = {"world_hud_coordinate": {"exists": True, "visible": True,
                   "enabled": True, "text": "#1   X:100 Y:50",
                   "absolute_top_left": {"x": 300, "y": 5},
                   "absolute_bottom_right": {"x": 500, "y": 40}},
                   "world_coordinate_atlas": {"object_path":
                   "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu",
                   "source_size": {"x": 1024, "y": 1024},
                   "rhi_extent": {"x": 1024, "y": 1024}, "rhi_ready": True},
                   "world_ready_since_seconds": 5.0, "audit_elapsed_seconds": 7.0}
    check_world_hud_ready(world_ready, 5)
    bad = copy.deepcopy(world_ready)
    bad["world_hud_coordinate"]["text"] = "#1   X:1146 Y:157"
    rejects(lambda: check_world_hud_ready(bad, 5), "stale HUD coordinate")
    bad = copy.deepcopy(world_ready)
    bad["world_coordinate_atlas"]["rhi_ready"] = False
    rejects(lambda: check_world_hud_ready(bad, 5), "atlas not RHI ready")
    bad = copy.deepcopy(world_ready)
    bad["audit_elapsed_seconds"] = 5.5
    rejects(lambda: check_world_hud_ready(bad, 5), "natural interval too short")
    check_edit_deletion({"text_before": "", "text_after_erase": "",
                         "backspace_handled": False, "backspace_empty_noop": True}, 0)
    check_edit_deletion({"text_before": "1146", "text_after_erase": "",
                         "backspace_handled": True, "backspace_empty_noop": False}, 0)
    rejects(lambda: check_edit_deletion({"text_before": "1146", "text_after_erase": "1146",
                                         "backspace_handled": True,
                                         "backspace_empty_noop": False}, 0),
            "text not actually erased")
    rejects(lambda: check_edit_deletion({"text_before": "1146", "text_after_erase": "",
                                         "backspace_handled": False,
                                         "backspace_empty_noop": False}, 0),
            "nonempty unhandled Backspace")
    rejects(lambda: check_edit_deletion({"text_before": "1146", "text_after_erase": "",
                                         "backspace_handled": False,
                                         "backspace_empty_noop": True}, 0),
            "forged empty no-op")
    expected_widgets = {"WorldPositionXInput": {"text": "12abc"},
                        "WorldPositionYInput": {"text": "50"}}
    check_raw_text(expected_widgets, 9)
    expected_widgets["WorldPositionXInput"]["text"] = "12"
    rejects(lambda: check_raw_text(expected_widgets, 9), "malformed raw text lost")
    sample_captures = [
        {"phase": phase, "requested_frame": i * 100 + 10,
         "processed_frame": i * 100 + 11,
         "next_frame_state": {"frame": i * 100 + 12}}
        for i, phase in enumerate(PHASES)
    ]
    sample_actions = [{"frame": next(i * 100 for i, count in
                        enumerate(PRECEDING_ACTION_COUNTS) if count > j)}
                      for j in range(len(ACTIONS))]
    check_capture_timeline(sample_captures, sample_actions)
    bad_captures = copy.deepcopy(sample_captures)
    bad_captures[8]["phase"] = PHASES[7]
    rejects(lambda: check_capture_timeline(bad_captures, sample_actions),
            "phase order")
    bad_captures = copy.deepcopy(sample_captures)
    bad_captures[8]["processed_frame"] = bad_captures[8]["requested_frame"] - 1
    rejects(lambda: check_capture_timeline(bad_captures, sample_actions),
            "processed frame before request")
    fake_proj = {"valid": True, "display_scale": 6, "unity_min": {"x": 0, "y": 0},
                 "unity_max": {"x": 7200, "y": 7200},
                 "display_min": {"x": 0, "y": 0},
                 "display_max": {"x": 1200, "y": 1200},
                 "source": "world_config_actor", "config_actor_count": 1,
                 "config_actor_path": "/Game/Maps/L_RokUnityWorldMapPrototype.Actor_1"}
    check_projection(fake_proj, True)
    bad = copy.deepcopy(fake_proj)
    bad["display_scale"] = 36
    rejects(lambda: check_projection(bad, True), "double six")
    bad = copy.deepcopy(fake_proj)
    bad["source"] = "local_projection_fallback"
    rejects(lambda: check_projection(bad, True), "projection identity")
    camera = {"pawn_class": "RokWorldMapCameraPawn", "view_center_display":
              {"x": 100, "y": 50}, "view_center_unity": {"x": 600, "y": 300}}
    check_camera(camera, 5, {})
    camera["view_center_unity"]["x"] = 3600
    rejects(lambda: check_camera(camera, 5, {}), "wrong camera/double six")
    edge_camera = {"pawn_class": "RokWorldMapCameraPawn", "view_center_display":
                   {"x": 0, "y": 1200}, "view_center_unity": {"x": 0, "y": 7200}}
    check_camera(edge_camera, 13, {})
    edge_camera["view_center_display"]["y"] = 1199
    rejects(lambda: check_camera(edge_camera, 13, {}), "edge center")
    clean_game = {"profile_kind": "local_preview", "role_id": "",
                  "civilization_id": 0, "profile_has_role": False,
                  "profile_has_civilization": False,
                  "profile_civilization_name": "", "http_dispatch_attempts": 0,
                  "http_suppressed_requests": 0}
    wallet = {"available": True, "wallet_food": 7, "active_upgrade_count": 0}
    fake_state = {"project": PROJECT, "pid": 123, "world": WORLD,
                  "game_business": clean_game, "world_business": wallet}
    check_identity_business(fake_state, 5, 123, clean_game, wallet, wallet)
    bad = copy.deepcopy(fake_state)
    bad["game_business"]["http_dispatch_attempts"] = 1
    rejects(lambda: check_identity_business(bad, 5, 123, clean_game, wallet, wallet),
            "nonzero HTTP")
    bad = copy.deepcopy(fake_state)
    bad["project"] = "D:/other/Rok"
    rejects(lambda: check_identity_business(bad, 5, 123, clean_game, wallet, wallet),
            "wrong project")
    return {"passed": True, "negative_cases": 26, "phases": len(PHASES),
            "actions": len(ACTIONS)}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path, nargs="?")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = run_mode(args.manifest, args.self_test)
    if args.report:
        write_report_exclusive(args.report, result)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
