"""Strict two-process, real-HUD/Slate acceptance for source GeneralSetting audio."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import struct
import tempfile
import wave
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "rok.audio_settings.runtime_audit.v1"
PROJECT = "D:/ueDemo/Rok"
WORLD = "/Game/Maps/L_RokUnityGameAppCity"
UI_H = "Source/Rok/RokUnityFeatureScreenWidget.h"
UI_CPP = "Source/Rok/RokUnityFeatureScreenWidget.cpp"
UI_REPORT = "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/general_settings_audio_ui_implementation_v1.json"
ROUTER_REPORT = "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/audio_router_implementation_revision2.json"
NATIVE = "Saved/UnityCityAudioProbeV1/OutputRuns/20261009_053128_273_40300/native_city_audio.json"
NATIVE_REPORT = "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/city_audio_probe_native_verify_20261009_053128_273_40300.json"
IMPORT_PREPARE = "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/city_bgm_import_prepare_revision2.json"
IMPORT_JOURNAL = "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/city_bgm_import_20261009T054000384701Z_42692.json"
IMPORT_ACCEPTANCE = "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/root_city_bgm_asset_import_acceptance_v1.json"
APPROVED_PINS = {
    UI_H: "0cfc67fc51e0ed174554d7872705a87c7689fd8ede3f3bd0f570a26bd1975d01",
    UI_CPP: "18a9076ad3bbf397e3a1fd9d8dcc980f033558097fc89dd4ccdecc4c5352c151",
    UI_REPORT: "cbca49ae8826253e22da1d28f6ac6663a88584c0f4c3f86e1fe05119d39356c0",
    NATIVE: "b0a30182ca378bd1257c6aab0beea0b7be376457d14ea86f2e6ca449d2d0d088",
    NATIVE_REPORT: "f7f7c938b68554f90b343d348f0e6f894cecd37d883a6ffa39eb5d8ee4b74499",
    "Source/Rok/RokAudioRouterSubsystem.h": "3bb32367215ef4991ea6728e30313632ad146391251e2d8b065c28af61ee81d8",
    "Source/Rok/RokAudioRouterSubsystem.cpp": "0292ea975a38bce110cea4b57b28476ee4b670a7f3d4263a75132356cb770816",
    ROUTER_REPORT: "a9799b08e253e62d001f86bb99d36f4ba2a9c0f6a85e698d4d2b165b523d867a",
    IMPORT_PREPARE: "1a4fbb91fdd159aeb409fcf7e95955badbc3d68ec00d1c349e3b829e7652e0e7",
    IMPORT_JOURNAL: "f7e217a3ad06dadb480c3d328aa03fccbfac48f635aa7a506baaefb9a294cc0b",
    IMPORT_ACCEPTANCE: "9173ed15ae5e51129152c6ce349fbf0eff66079d26a0a63040e7a8b1645f2df4",
    "Source/Rok/RokPlayerController.cpp": "f5c297bfb9b972db07e7725a9663a42e2c35f928625167e033e1bad97c6c5ef6",
    "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/audio_route_source_contract_v1.json": "6b5fa6b2986e4c32e49927b2f1ad908c17289414d82134313ff9b14c2be39831",
    "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/batch_start.json": "c3ae398661006504b22499a5b3a91f4d08620d9952b9517c3bb37e57f47c14bd",
    "Saved/RokUnityMigration/CityAudio/catalog.json": "53a61954423b3d383b3ac68a34ea0be913022e4cfd854c5033baf57ffa6c1d9d",
}
PRIMARY_PHASES = [
    "00_hud", "01_profile", "02_settings_hub", "03_general_top",
    "04_sfx_zero", "05_sfx_fraction", "06_sfx_full", "07_sfx_final",
    "08_audio_scrolled", "09_music_zero", "10_music_zero_next_track",
    "11_music_fraction", "12_music_full", "13_music_final",
    "14_closed", "15_settings_hub_reopened", "16_general_reopened",
]
RESUME_PHASES = [
    "00_resume_hud", "01_resume_general_top", "02_resume_audio_scrolled", "03_resume_closed",
]
PRIMARY_ACTIONS = [
    ("button", "PlayerProfileHitButton"),
    ("button", "ProfileSettingsButton"),
    ("button", "SettingsHubGeneralButton"),
    ("slider", "GeneralSettingsSfxSlider"),
    ("slider", "GeneralSettingsSfxSlider"),
    ("slider", "GeneralSettingsSfxSlider"),
    ("slider", "GeneralSettingsSfxSlider"),
    ("wheel", "GeneralSettingsScroll"),
    ("slider", "GeneralSettingsMusicSlider"),
    ("slider", "GeneralSettingsMusicSlider"),
    ("slider", "GeneralSettingsMusicSlider"),
    ("slider", "GeneralSettingsMusicSlider"),
    ("button", "FeatureFrameClose"),
    ("button", "PlayerProfileHitButton"),
    ("button", "ProfileSettingsButton"),
    ("button", "SettingsHubGeneralButton"),
]
RESUME_ACTIONS = [
    ("button", "PlayerProfileHitButton"),
    ("button", "ProfileSettingsButton"),
    ("button", "SettingsHubGeneralButton"),
    ("wheel", "GeneralSettingsScroll"),
    ("button", "FeatureFrameClose"),
]


class Rejection(AssertionError):
    pass


def need(condition: bool, message: str) -> None:
    if not condition:
        raise Rejection(message)


def near(actual: float, expected: float, tolerance: float = 0.03) -> bool:
    return math.isfinite(float(actual)) and abs(float(actual) - expected) <= tolerance


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def point(value: dict) -> tuple[float, float]:
    return float(value["x"]), float(value["y"])


def rect(widget: dict) -> tuple[float, float, float, float]:
    ax, ay = point(widget["absolute_top_left"])
    bx, by = point(widget["absolute_bottom_right"])
    return min(ax, bx), min(ay, by), max(ax, bx), max(ay, by)


def enclosed(outer: tuple[float, float, float, float], inner: tuple[float, float, float, float], tolerance=1.0) -> bool:
    return (inner[0] >= outer[0] - tolerance and inner[1] >= outer[1] - tolerance
            and inner[2] <= outer[2] + tolerance and inner[3] <= outer[3] + tolerance)


def intersects(outer: tuple[float, float, float, float], inner: tuple[float, float, float, float]) -> bool:
    return outer[0] < inner[2] and outer[2] > inner[0] and outer[1] < inner[3] and outer[3] > inner[1]


def slot(widget: dict, x: float, y: float, width: float, height: float, name: str) -> None:
    need(widget["exists"], f"{name} absent")
    px, py = point(widget["slot_position"])
    sx, sy = point(widget["slot_size"])
    need(near(px, x, .08) and near(py, y, .08)
         and near(sx, width, .08) and near(sy, height, .08), f"{name} source slot differs")


def check_png(capture: dict, directory: Path) -> None:
    path = Path(capture["png"])
    need(path.resolve().parent == directory.resolve(), "PNG escaped audit run directory")
    payload = path.read_bytes()
    need(len(payload) > 4096 and payload[:8] == bytes.fromhex("89504e470d0a1a0a"),
         "PNG missing/truncated")
    need(payload[12:16] == b"IHDR" and struct.unpack(">II", payload[16:24]) == (1440, 810),
         "PNG is not real 1440x810 frame")
    need(len(payload) == capture["png_bytes"] and sha(path) == capture["png_sha256"],
         "PNG byte/hash mismatch")


def check_inputs(manifest: dict) -> None:
    pins = manifest["inputs_sha256"]
    required = set(APPROVED_PINS) | {
        "Source/Rok/RokAudioSettingsAuditSubsystem.h",
        "Source/Rok/RokAudioSettingsAuditSubsystem.cpp",
        "Scripts/VerifyRokAudioSettingsRuntime.py",
        "Source/Rok/RokAudioRouterSubsystem.h",
        "Source/Rok/RokAudioRouterSubsystem.cpp",
        "Source/Rok/RokPlayerController.cpp",
        "Source/Rok/RokClientSettingsSubsystem.h",
        "Source/Rok/RokClientSettingsSubsystem.cpp",
        "Source/Rok/RokMainHudWidget.cpp",
        "Source/Rok/RokWorldMapHudWidget.cpp",
        "Content/RokPrototype/UI/Audio/CityBgm/Bgm_PeaceDay01.uasset",
        "Content/RokPrototype/UI/Audio/CityBgm/Bgm_PeaceDay02.uasset",
        "Content/RokPrototype/UI/Audio/CityBgm/Bgm_PeaceNight01.uasset",
        "Content/RokPrototype/UI/Audio/CityBgm/Bgm_PeaceNight02.uasset",
        IMPORT_PREPARE, IMPORT_JOURNAL, IMPORT_ACCEPTANCE,
        ROUTER_REPORT,
        "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/audio_route_source_contract_v1.json",
        "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/batch_start.json",
    }
    need(set(pins) == required, "source input pin set differs")
    for relative, expected in pins.items():
        path = resolve(relative)
        need(path.is_file() and sha(path) == expected, f"source input hash drift: {relative}")
        if relative in APPROVED_PINS:
            need(expected == APPROVED_PINS[relative], f"approved source pin differs: {relative}")


def check_imported_pcm(manifest: dict, directory: Path) -> None:
    native = json.loads(resolve(NATIVE).read_text(encoding="utf-8"))
    report = json.loads(resolve(NATIVE_REPORT).read_text(encoding="utf-8"))
    prepare = json.loads(resolve(IMPORT_PREPARE).read_text(encoding="utf-8"))
    journal = json.loads(resolve(IMPORT_JOURNAL).read_text(encoding="utf-8"))
    acceptance = json.loads(resolve(IMPORT_ACCEPTANCE).read_text(encoding="utf-8"))
    need(native["schema"] == "rok.unity_city_audio_probe.native.v2"
         and report["status"] == "passed"
         and report["native_manifest_sha256"] == sha(resolve(NATIVE))
         and report["contract_sha256"] == native["contract_sha256"],
         "Native1 Unity PCM source chain invalid")
    need(prepare["schema"] == "rok.city_bgm_import.prepare.v2"
         and prepare["native_inputs"]["manifest_sha256"] == sha(resolve(NATIVE))
         and prepare["native_inputs"]["strict_verify_sha256"] == sha(resolve(NATIVE_REPORT))
         and journal["schema"] == "rok.city_bgm_import.v1"
         and journal["status"] == "imported_and_readback_passed"
         and journal["input_pins"]["native_manifest_sha256"] == sha(resolve(NATIVE))
         and journal["input_pins"]["strict_verify_sha256"] == sha(resolve(NATIVE_REPORT))
         and acceptance["schema"] == "rok.city_bgm.root_asset_import_acceptance.v1"
         and acceptance["passed"]
         and acceptance["import_manifest"]["sha256"] == sha(resolve(IMPORT_JOURNAL))
         and acceptance["asset_verification"]["status"] == "passed"
         and sha(ROOT / "Saved/UnityCityAudioProbeV1/probe_contract.json") == native["contract_sha256"]
         and sha(ROOT / "Saved/UnityCityAudioProbeV1/Assets/Editor/ExportRokUnityCityAudioProbe.cs") == native["exporter_sha256"],
         "Native1 contract/exporter/import/strict report chain differs")
    source = {item["name"]: item for item in native["clips"]}
    reviewed = {item["name"]: item for item in report["tracks"]}
    imported_journal = {item["name"]: item for item in journal["input_pins"]["clips"]}
    accepted_assets = {item["name"]: item for item in acceptance["asset_verification"]["assets"]}
    expected_names = {"Bgm_PeaceDay01", "Bgm_PeaceDay02", "Bgm_PeaceNight01", "Bgm_PeaceNight02"}
    imported = {item["name"]: item for item in manifest["imported_pcm"]}
    need(set(source) == set(reviewed) == set(imported) == set(imported_journal)
         == set(accepted_assets) == expected_names,
         "four source/imported BGM identities differ")
    for name in sorted(expected_names):
        clip, review, ue = source[name], reviewed[name], imported[name]
        imported_source = imported_journal[name]
        accepted = accepted_assets[name]
        package_path = f"Content/RokPrototype/UI/Audio/CityBgm/{name}.uasset"
        package_records = accepted["package_files"]
        need(len(package_records) == 1 and package_records[0]["path"] == package_path,
             f"accepted package path/count differs: {name}")
        package = resolve(package_path)
        need(manifest["inputs_sha256"][package_path] == package_records[0]["sha256"]
             and package.is_file() and sha(package) == package_records[0]["sha256"]
             and package.stat().st_size == package_records[0]["bytes"]
             and accepted["object_path"] == f"/Game/RokPrototype/UI/Audio/CityBgm/{name}.{name}"
             and accepted["properties"]["sound_asset_compression_type"] == "<SoundAssetCompressionType.PCM: 2>"
             and accepted["properties"]["virtualization_mode"] == "<VirtualizationMode.PLAY_WHEN_SILENT: 1>"
             and accepted["properties"]["volume"] == 1.0
             and accepted["properties"]["pitch"] == 1.0
             and accepted["properties"]["looping"] is False,
             f"current BGM package differs from pinned imported package: {name}")
        need(sha(resolve(clip["source_path"])) == clip["source_sha256"]
             and sha(resolve(clip["meta_path"])) == clip["meta_sha256"],
             f"Unity source MP3/meta drift: {name}")
        need(clip["wav_sha256"] == review["wav_sha256"]
             and clip["pcm_f32le_sha256"] == review["pcm_f32le_sha256"]
             and clip["frequency"] == review["frequency"]
             and clip["channels"] == review["channels"]
             and clip["sample_frames"] == review["sample_frames"],
             f"Native1 strict track review differs: {name}")
        need(imported_source["wav_sha256"] == clip["wav_sha256"]
             and imported_source["sample_rate"] == clip["frequency"]
             and imported_source["channels"] == clip["channels"],
             f"actual UE import journal source identity differs: {name}")
        wav_path = resolve(NATIVE).parent / clip["wav_path"]
        need(sha(wav_path) == clip["wav_sha256"], f"Native1 WAV hash differs: {name}")
        with wave.open(str(wav_path), "rb") as wav:
            need(wav.getcomptype() == "NONE" and wav.getsampwidth() == 2
                 and wav.getframerate() == clip["frequency"]
                 and wav.getnchannels() == clip["channels"]
                 and wav.getnframes() == clip["sample_frames"],
                 f"Native1 WAV is not pinned PCM16: {name}")
            expected_pcm = wav.readframes(wav.getnframes())
        need(hashlib.sha256(expected_pcm).hexdigest() == clip["wav_pcm16_sha256"],
             f"Native1 WAV PCM payload hash differs: {name}")
        imported_path = Path(ue["pcm_path"])
        need(imported_path.resolve().parent == directory.resolve()
             and imported_path.name == f"imported_{name}.pcm16le"
             and imported_path.is_file(), f"UE imported PCM evidence path differs: {name}")
        imported_bytes = imported_path.read_bytes()
        need(ue["ue_object_path"] == f"/Game/RokPrototype/UI/Audio/CityBgm/{name}.{name}"
             and ue["compression_enum"] == "PCM"
             and ue["sample_rate"] == clip["frequency"]
             and ue["channels"] == clip["channels"]
             and ue["sample_frames"] == clip["sample_frames"]
             and ue["pcm_bytes"] == len(expected_pcm) == len(imported_bytes)
             and ue["pcm_sha256"] == sha(imported_path)
             and imported_bytes == expected_pcm,
             f"UE GetImportedSoundWaveData PCM16 differs byte-for-byte from Native1: {name}")


def check_business(state: dict, baseline: dict) -> None:
    business = state["business"]
    need(business == baseline, "account/task/wallet/queues/heroes/troops changed")
    need(business["profile_kind"] == "local_preview" and business["role_id"] == ""
         and business["civilization_id"] == 0, "account profile is not local preview")
    need(business["http_dispatch_attempts"] == 0 and business["http_suppressed_requests"] == 0,
         "HTTP dispatched or suppressed")


def check_route(state: dict, *, require_playing: bool = False) -> None:
    route = state["route"]
    need(route["city_bgm_event_id"] == 1008, "source night BGM event changed")
    need("Bgm_PeaceNight" in route["bgm_sound_path"], "BGM sound object is not night source")
    need(near(route["music_setting"], state["settings_music"], .006)
         and near(route["sfx_setting"], state["settings_sfx"], .006),
         "router channels did not track settings")
    need(near(route["bgm_component_volume"], state["settings_music"], .006),
         "real BGM component gain differs")
    if require_playing:
        need(route["bgm_component_playing"], "BGM component is not playing")
    else:
        need(route["bgm_component_playing"] or route["bgm_pending_next_delay"] > 0,
             "BGM neither playing nor waiting for source interval")


def check_general(state: dict, *, scrolled: bool, sfx: float, music: float) -> None:
    need(state["screen_open"] and not state["modal_open"], "general screen route absent")
    need(near(state["settings_sfx"], sfx, .006)
         and near(state["settings_music"], music, .006), "selected audio settings differ")
    if not (near(sfx, 1.0, .006) and near(music, 1.0, .006)):
        need(state["settings_disk_values_found"]
             and near(state["disk_sfx"], sfx, .006)
             and near(state["disk_music"], music, .006),
             "fresh disk audio preferences differ")
    widgets = state["widgets"]
    scroll = widgets["GeneralSettingsScroll"]
    audio = widgets["GeneralSettingsAudioItem"]
    slot(scroll, 220.6, 124.85, 1004.4, 630.15, "source scroll viewport")
    slot(audio, 0, 367, 1000, 367, "source audio item")
    scroll_rect = rect(scroll)
    need(near(scroll_rect[2] - scroll_rect[0], 1004.4, 1.2)
         and near(scroll_rect[3] - scroll_rect[1], 630.15, 1.2),
         "scroll rendered geometry differs")
    sfx_widget = widgets["GeneralSettingsSfxSlider"]
    music_widget = widgets["GeneralSettingsMusicSlider"]
    slot(sfx_widget, 90.895, 98.635, 822.55, 22.01, "SFX source slider")
    slot(music_widget, 90.895, 252.995, 822.55, 22.01, "Music source slider")
    for name, widget, value in (("Sfx", sfx_widget, sfx), ("Music", music_widget, music)):
        need(widget["enabled"] and near(widget["min"], 0) and near(widget["max"], 1)
             and near(widget["step"], .001, .0001) and not widget["mouse_uses_step"],
             f"{name} is not enabled continuous 0..1 slider")
        need(near(widget["value"], value, .006) and near(widget["render_scale"]["x"], -1, .001),
             f"{name} value or RightToLeft direction differs")
        actual_value = float(widget["value"])
        need(widgets[f"GeneralSettings{name}Title"]["text"] == ("音效音量" if name == "Sfx" else "音乐音量"),
             f"{name} source title differs")
        need(widgets[f"GeneralSettings{name}Description"]["text"] ==
             ("调整游戏内音效的音量大小" if name == "Sfx" else "调整游戏内背景音乐的音量大小"),
             f"{name} source description differs")
        slot(widgets[f"GeneralSettings{name}Frame"], 75, 88.64 if name == "Sfx" else 243,
             850, 42, f"{name} source frame")
        slot(widgets[f"GeneralSettings{name}Background"], 89,
             98.64 if name == "Sfx" else 253, 822, 22, f"{name} source background")
        fill = widgets[f"GeneralSettings{name}Fill"]
        slot(fill, 92.895 + 818.55 * (1 - actual_value),
             100.14 if name == "Sfx" else 254.5, 818.55 * actual_value, 19,
             f"{name} RightToLeft fill")
        need(fill["visible"] == (actual_value > .001), f"{name} zero-fill visibility differs")
        need(widgets[f"GeneralSettings{name}IconLeft"]["exists"]
             and widgets[f"GeneralSettings{name}IconRight"]["exists"],
             f"{name} source endpoint art absent")
    need(rect(sfx_widget)[1] < rect(music_widget)[1], "SFX must be above Music")
    need(enclosed(scroll_rect, rect(sfx_widget), 1.2), "SFX slider clipped unexpectedly")
    if scrolled:
        need(state["scroll_offset"] > 20 and enclosed(scroll_rect, rect(music_widget), 1.2),
             "Music slider not revealed by real scroll")
    else:
        need(abs(state["scroll_offset"]) < 1.2
             and intersects(scroll_rect, rect(music_widget))
             and not enclosed(scroll_rect, rect(music_widget), .5),
             "Music initial source viewport clipping differs")
    check_route(state)


def check_slider_action(action: dict) -> None:
    need(action["own_game_window"] and action["foreground"], "input was not on foreground own window")
    need(action["slider_hit_path"] and action["down_handled"] and action["up_handled"]
         and any("SSlider" in item for item in action["hit_path_types"]),
         "slider was not driven by real SSlider input")
    need(action["feedback_sequence_after"] == action["feedback_sequence_before"] + 1
         and action["feedback_component_created"]
         and "Sound_Ui_CommonScroll" in action["feedback_sound_path"],
         "source 4005 feedback was not created once")
    need(near(action["feedback_pre_change_volume"], action["sfx_before"], .006)
         and near(action["feedback_component_volume"], action["sfx_before"], .006),
         "feedback creation gain was not old SFX value")
    need(near(action["value_after"], action["target_value"], .006),
         "Slate slider did not reach target")


def check_actions(actions: list[dict], expected: list[tuple[str, str]]) -> None:
    need([(a["kind"], a["selector"]) for a in actions] == expected,
         "real HUD/Slate action sequence differs")
    slider_actions = []
    for action in actions:
        need(action["own_game_window"] and action["foreground"], "input was not on foreground own window")
        if action["kind"] == "button":
            need(action["down_handled"] and action["up_handled"]
                 and action["target_in_hit_path"] and "SButton" in action["terminal_type"]
                 and action["primary_before"] == action["primary_after"],
                 "button was not handled real SButton click")
        elif action["kind"] == "wheel":
            need(action["scroll_hit_path"] and action["handled"]
                 and any("SScrollBox" in item for item in action["hit_path_types"]),
                 "wheel did not hit real SScrollBox")
        else:
            check_slider_action(action)
            slider_actions.append(action)
    if slider_actions:
        need([round(a["target_value"], 2) for a in slider_actions] ==
             [0, .37, 1, .37, 0, .37, 1, .37], "0/.37/1/final sequence differs")
        for group in (slider_actions[:4], slider_actions[4:]):
            xs = [item["absolute_point"]["x"] for item in group[:3]]
            need(xs[0] > xs[1] > xs[2], "source RightToLeft input positions reversed")


def check_stage(path: Path, stage: str) -> dict:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    phases = PRIMARY_PHASES if stage == "primary" else RESUME_PHASES
    actions = PRIMARY_ACTIONS if stage == "primary" else RESUME_ACTIONS
    need(manifest["schema"] == SCHEMA and manifest["stage"] == stage,
         "audio run schema/stage differs")
    need(manifest["complete"] and manifest["passed"] and not manifest["failures"],
         "audio runtime did not complete cleanly")
    need(Path(manifest["project"]).as_posix().rstrip("/").lower() == PROJECT.lower()
         and manifest["world"] == WORLD, "wrong project/map")
    need(manifest["cross_process_resume_required_for_final_acceptance"],
         "cross-process persistence gate disabled")
    need(manifest["scalability_sha256_initial"] == manifest["scalability_sha256_final"],
         "Scalability settings changed")
    need([item["phase"] for item in manifest["captures"]] == phases, "capture phase list differs")
    check_actions(manifest["actions"], actions)
    check_inputs(manifest)
    check_imported_pcm(manifest, path.parent)
    baseline = manifest["captures"][0]["processed_state"]["business"]
    first_primary = stage == "primary"
    for capture in manifest["captures"]:
        check_png(capture, path.parent)
        need(capture["requested_frame"] <= capture["processed_frame"], "screenshot processed before request")
        for key in ("requested_state", "processed_state", "next_frame_state"):
            state = capture[key]
            check_business(state, baseline)
            need(state["primary_press_count"] == manifest["captures"][0]["processed_state"]["primary_press_count"],
                 "world primary press count changed")
            need(state["graphics_tier"] == manifest["captures"][0]["processed_state"]["graphics_tier"]
                 and state["frame_rate_option"] == manifest["captures"][0]["processed_state"]["frame_rate_option"],
                 "graphics/frame settings changed")
            check_route(state)
        need(capture["next_frame_state"]["frame"] > capture["processed_frame"],
             "screenshot lacks natural next-frame capture")
    states = {item["phase"]: item["processed_state"] for item in manifest["captures"]}
    if first_primary:
        expected = {
            "03_general_top": (False, None, None),
            "04_sfx_zero": (False, 0, None),
            "05_sfx_fraction": (False, .37, None),
            "06_sfx_full": (False, 1, None),
            "07_sfx_final": (False, .37, None),
            "08_audio_scrolled": (True, .37, None),
            "09_music_zero": (True, .37, 0),
            "10_music_zero_next_track": (True, .37, 0),
            "11_music_fraction": (True, .37, .37),
            "12_music_full": (True, .37, 1),
            "13_music_final": (True, .37, .37),
            "16_general_reopened": (False, .37, .37),
        }
        initial_sfx = states["03_general_top"]["settings_sfx"]
        initial_music = states["03_general_top"]["settings_music"]
        for phase, (scrolled, sfx, music) in expected.items():
            check_general(states[phase], scrolled=scrolled,
                          sfx=initial_sfx if sfx is None else sfx,
                          music=initial_music if music is None else music)
        after = states["10_music_zero_next_track"]["route"]
        need(manifest["zero_start_finished_count"] >= 0
             and after["bgm_naturally_finished_count"] > manifest["zero_start_finished_count"]
             and after["bgm_sound_path"] != manifest["zero_start_track_path"]
             and after["bgm_track_started_world_seconds"] > manifest["zero_start_track_started_world_seconds"]
             and after["bgm_track_elapsed_seconds"] > .3
             and near(after["bgm_component_volume"], 0, .006)
             and after["bgm_component_playing"],
             "silent night BGM did not naturally advance to another real component track")
    else:
        check_general(states["01_resume_general_top"], scrolled=False, sfx=.37, music=.37)
        check_general(states["02_resume_audio_scrolled"], scrolled=True, sfx=.37, music=.37)
    final = manifest["final_state"]
    need(near(final["settings_sfx"], .37, .006)
         and near(final["settings_music"], .37, .006)
         and near(final["disk_sfx"], .37, .006)
         and near(final["disk_music"], .37, .006)
         and final["settings_disk_values_found"], "fresh persisted disk values differ")
    return manifest


def check_pair(primary: dict, resume: dict) -> None:
    need(primary["pid"] != resume["pid"], "resume was not a fresh process")
    need(primary["inputs_sha256"] == resume["inputs_sha256"], "source inputs changed between processes")
    old = primary["captures"][-1]["processed_state"]
    new = resume["captures"][0]["processed_state"]
    need(near(old["settings_sfx"], new["settings_sfx"], .006)
         and near(old["settings_music"], new["settings_music"], .006)
         and near(new["route"]["bgm_component_volume"], .37, .006),
         "restart did not hydrate persisted settings into real BGM route")


def self_test() -> None:
    actual_pngs = sorted((ROOT / "Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial"
                          / "AudioSettingsAudit").glob("*_primary/00_hud.png"))
    need(bool(actual_pngs), "self-test requires a genuine audio primary HUD PNG")
    actual_png = actual_pngs[0]
    with tempfile.TemporaryDirectory() as scratch:
        scratch_dir = Path(scratch)
        good = scratch_dir / "actual_hud.png"
        good.write_bytes(actual_png.read_bytes())
        capture = {"png": str(good), "png_bytes": good.stat().st_size,
                   "png_sha256": sha(good)}
        check_png(capture, scratch_dir)
        bad = scratch_dir / "bad_signature.png"
        corrupted = bytearray(good.read_bytes())
        corrupted[0] ^= 1
        bad.write_bytes(corrupted)
        bad_capture = {"png": str(bad), "png_bytes": bad.stat().st_size,
                       "png_sha256": sha(bad)}
        try:
            check_png(bad_capture, scratch_dir)
        except Rejection as exc:
            need("PNG missing/truncated" in str(exc), "bad signature rejected for wrong reason")
        else:
            raise Rejection("negative PNG signature mutation accepted")
    a = {"absolute_top_left": {"x": 100, "y": 10},
         "absolute_bottom_right": {"x": 0, "y": 30}}
    need(rect(a) == (0, 10, 100, 30), "mirrored slider rectangle normalization failed")
    need(not enclosed((0, 0, 100, 100), (0, 95, 100, 110)), "clip negative failed")
    need(intersects((0, 0, 100, 100), (0, 95, 100, 110)), "partial clip control failed")
    action = {"kind": "slider", "selector": "GeneralSettingsSfxSlider",
              "own_game_window": True, "foreground": True, "slider_hit_path": True,
              "down_handled": True, "up_handled": True, "hit_path_types": ["SSlider"],
              "feedback_sequence_before": 1, "feedback_sequence_after": 2,
              "feedback_component_created": True,
              "feedback_sound_path": "/Game/Sound_Ui_CommonScroll",
              "feedback_pre_change_volume": .37, "feedback_component_volume": .37,
              "sfx_before": .37, "value_after": .5, "target_value": .5,
              "absolute_point": {"x": 500}}
    check_slider_action(action)
    for field, bad in (("foreground", False), ("slider_hit_path", False),
                       ("feedback_sequence_after", 1), ("feedback_pre_change_volume", 1.0),
                       ("value_after", .9)):
        altered = copy.deepcopy(action)
        altered[field] = bad
        try:
            check_slider_action(altered)
        except Rejection:
            pass
        else:
            raise Rejection(f"negative action mutation accepted: {field}")
    print("PASS audio verifier self-test: real PNG/signature negative, mirrored geometry, clip, five action negatives")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("primary", nargs="?", type=Path)
    parser.add_argument("resume", nargs="?", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    need(args.primary is not None and args.resume is not None,
         "two manifest paths required: primary and fresh-process resume")
    primary = check_stage(args.primary, "primary")
    resume = check_stage(args.resume, "resume")
    check_pair(primary, resume)
    print("PASS audio settings runtime: 17+4 PNGs, real Slate 0/.37/1, silent next BGM track, restart persistence")


if __name__ == "__main__":
    main()
