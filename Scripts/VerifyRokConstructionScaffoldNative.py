"""Strict offline verifier for the isolated Build_shelf_5 Unity native capture."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
from pathlib import Path

from PIL import Image, ImageChops


ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityConstructionScaffoldProbe"
INVENTORY = ROOT / "Saved/RokUnityMigration/BatchReview20261009Construction/construction_effect_source_inventory.json"
EXPECTED_INVENTORY = "01dd55e415bfc8f9284642f22e73f47e111a1b34ccaf126788e7357189844ffe"
CAMERA_SHA = "716f0a86f71e3d587e5d78eb75666268d544ed23be5862821be0ca84b6bae1bb"
PHASES = [
    "00_start", "01_clip_mid", "02_one_second", "03_clip_end",
    "04_before_two_second_timer", "05_two_second_timer_before",
    "06_two_second_timer_after",
]
CLIPS = {"build_scaffold_1", "build_scaffold_5", "build_scaffold_6", "build_scaffold_10"}
MASK_SCRIPT_GUID = "f43e688d317b80c4d9560e9cb03707e9"


def need(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def sha(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            h.update(chunk)
    return h.hexdigest()


def source_mask_pairings(contract: dict) -> set[tuple[str, str, str, int, str, int]]:
    sources = contract["source_files"]
    guid_path = {}
    for row in sources:
        meta = ROOT / (row["source_path"] + ".meta")
        match = re.search(r"(?m)^guid: ([0-9a-f]{32})", meta.read_text(encoding="utf-8-sig"))
        need(match is not None, f"GUID missing: {meta}")
        guid_path[match.group(1)] = row["source_path"]
    rows = set()
    for number in (1, 5, 6, 10):
        path = ROOT / (
            "ref/client-unity/Assets/BundleAssets/Scene/City_building/city_building_1/"
            f"build_scaffold_{number}.prefab"
        )
        documents = re.split(r"(?m)(?=^--- !u!\d+ &\d+)", path.read_text(encoding="utf-8-sig"))
        names: dict[str, str] = {}
        sprites: dict[str, tuple[str, int]] = {}
        masks: dict[str, tuple[str, int]] = {}
        for document in documents:
            game_object = re.match(r"--- !u!1 &(\d+)", document)
            sprite_renderer = re.match(r"--- !u!212 &\d+", document)
            mono = re.match(r"--- !u!114 &\d+", document)
            if game_object:
                name = re.search(r"(?m)^  m_Name: (.+)$", document)
                if name:
                    names[game_object.group(1)] = name.group(1).strip()
            elif sprite_renderer:
                owner = re.search(r"m_GameObject: \{fileID: (\d+)", document)
                sprite = re.search(r"m_Sprite: \{fileID: (\d+), guid: ([0-9a-f]{32})", document)
                if owner and sprite:
                    sprites[owner.group(1)] = (sprite.group(2), int(sprite.group(1)))
            elif mono and f"guid: {MASK_SCRIPT_GUID}" in document:
                owner = re.search(r"m_GameObject: \{fileID: (\d+)", document)
                mask = re.search(r"m_mask_sprite: \{fileID: (\d+), guid: ([0-9a-f]{32})", document)
                if owner and mask:
                    masks[owner.group(1)] = (mask.group(2), int(mask.group(1)))
        need(len(sprites) == len(masks) == 2, f"Scaffold source pair count differs: {path}")
        for owner, (sprite_guid, sprite_id) in sprites.items():
            need(owner in masks and names[owner] in ("sprite1", "sprite2"),
                 f"Scaffold source owner pairing differs: {path}")
            mask_guid, mask_id = masks[owner]
            need(guid_path[sprite_guid].endswith(".png") and
                 guid_path[mask_guid].endswith("_mask.png"),
                 f"Scaffold source sprite/mask GUID failed to resolve: {path}")
            rows.add((f"build_scaffold_{number}", names[owner], sprite_guid,
                      sprite_id, mask_guid, mask_id))
    need(len(rows) == 8, "Source scaffold mapping is not eight unique layers")
    return rows


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", required=True, type=Path)
    parser.add_argument("--log", required=True, type=Path)
    args = parser.parse_args()
    run = args.run.resolve()
    log = args.log.resolve()
    need(run.is_dir() and run.parent == (PROBE / "OutputRuns").resolve(),
         "Wrong isolated scaffold run")
    need(log.is_file() and log.parent == (ROOT / "Saved/Logs").resolve(),
         "Unity log missing or outside Saved/Logs")
    log_text = log.read_text(encoding="utf-8-sig", errors="replace").replace("\\", "/")
    need("ROK_CONSTRUCTION_SCAFFOLD_NATIVE_RENDERED" in log_text and
         str(run).replace("\\", "/") in log_text and
         "Setting the random seed while system is still playing" not in log_text and
         " error CS" not in log_text and "Unhandled Exception" not in log_text,
         "Unity log is unrelated to run or contains seed/compiler/runtime errors")
    need(sha(INVENTORY) == EXPECTED_INVENTORY, "Sealed construction source inventory changed")
    contract_path = PROBE / "source_contract.json"
    contract = json.loads(contract_path.read_text(encoding="utf-8-sig"))
    need(contract["schema"] == "rok.construction_scaffold_native_contract.v1" and
         contract["source_inventory_sha256"] == EXPECTED_INVENTORY and
         contract["unity_version"] == "2019.4.28f1c1" and
         contract["source_color_space"] == "Gamma" and
         Path(contract["repo_root"]).resolve() == ROOT.resolve() and
         Path(contract["probe_root"]).resolve() == PROBE.resolve() and
         contract["exporter_sha256"] ==
         sha(PROBE / "Assets/Editor/RenderRokConstructionScaffoldNative.cs"),
         "Scaffold contract/project/renderer identity differs")
    need(contract["camera_evidence_sha256"] == CAMERA_SHA and
         sha(ROOT / contract["camera_evidence_path"]) == CAMERA_SHA and
         contract["camera"]["source_row"] == "city_default" and
         contract["camera"]["vertical_fov"] == 7.5 and
         contract["camera"]["width"] == 1280 and
         contract["camera"]["height"] == 720,
         "Source city_default camera identity differs")
    need(contract["allowed_builtin_guid_refs"] ==
         ["0000000000000000f000000000000000"] and
         len(contract["omitted_script_guid_refs"]) == 4 and
         len(contract["source_files"]) == 54 and
         sum(bool(row["copied"]) for row in contract["source_files"]) == 46,
         "Source dependency closure/allowlist differs")
    for row in contract["source_files"]:
        original = ROOT / row["source_path"]
        need(original.is_file() and sha(original) == row["source_sha256"] and
             sha(Path(str(original) + ".meta")) == row["meta_sha256"],
             f"Original source/meta changed: {original}")
        if row["copied"]:
            copy = (PROBE / row["probe_path"]).resolve()
            need(row["probe_path"].startswith("Assets/Source/") and
                 PROBE.resolve() in copy.parents and
                 sha(copy) == row["source_sha256"] and
                 sha(Path(str(copy) + ".meta")) == row["meta_sha256"],
                 f"Isolated copy/meta differs: {copy}")
    source_pairings = source_mask_pairings(contract)
    contracted_pairings = {
        (row["scaffold"], row["sprite_node"], row["sprite_guid"],
         row["sprite_file_id"], row["mask_guid"], row["mask_file_id"])
        for row in contract["mask_bindings"]
    }
    need(source_pairings == contracted_pairings, "Eight mask mappings differ from source prefab YAML")
    for row in contract["mask_bindings"]:
        need(sha(PROBE / row["sprite_path"]) == row["sprite_sha256"] and
             sha(PROBE / row["mask_path"]) == row["mask_sha256"] and
             sha(ROOT / row["source_prefab_path"]) == row["source_prefab_sha256"],
             "Source mask mapping file hash differs")
    report_path = run / "native_scaffold_render.json"
    need(report_path.is_file() and not (run / "failure.txt").exists(),
         "Scaffold native capture failed")
    report = json.loads(report_path.read_text(encoding="utf-8-sig"))
    need(report["schema"] == "rok.construction_scaffold_native_render.v1" and
         report["passed"] is True and report["game_app_loaded"] is False and
         report["server_event_replayed"] is False and
         report["actual_mask_script_loaded"] is False and
         report["unity_version"] == "2019.4.28f1c1" and
         report["color_space"] == "Gamma" and
         Path(report["project_path"]).resolve() == PROBE.resolve() and
         Path(report["run_directory"]).resolve() == run and
         report["source_inventory_sha256"] == EXPECTED_INVENTORY and
         report["source_contract_sha256"] == sha(contract_path) and
         report["renderer_sha256"] == contract["exporter_sha256"] and
         report["camera_evidence_sha256"] == CAMERA_SHA,
         "Scaffold native identity or isolation limit differs")
    need(len(report["source_nodes"]) == 16 and len(report["layers"]) == 8 and
         len(report["source_animation_components"]) == len(report["clips"]) == 4 and
         {clip["name"] for clip in report["clips"]} == CLIPS and
         sum(len(clip["curves"]) for clip in report["clips"]) == 15,
         "Scaffold source hierarchy/clip shape differs")
    need(all(not anim["source_enabled"] and anim["source_play_automatically"]
             for anim in report["source_animation_components"]),
         "Authored Animation component state differs")
    for clip in report["clips"]:
        need(clip["legacy"] and clip["frame_rate"] == 60 and
             1.8 <= clip["length"] < 2 and
             all(curve["type"] == "UnityEngine.Transform" and
                 len(curve["keys"]) == 4 and
                 all(math.isfinite(k["time"]) and math.isfinite(k["value"])
                     for k in curve["keys"]) for curve in clip["curves"]),
             f"Source clip curve/keys incomplete: {clip['name']}")
    layer_pairings = set()
    for layer in report["layers"]:
        parts = layer["node"].split("/")
        scaffold = re.sub(r"\[\d+\]$", "", parts[-2])
        sprite_name = re.sub(r"\[\d+\]$", "", parts[-1])
        layer_pairings.add((scaffold, sprite_name, layer["sprite"]["guid"],
                            layer["sprite"]["file_id"], layer["mask_sprite"]["guid"],
                            layer["mask_sprite"]["file_id"]))
        need(layer["mask_property_block_verified"] is True and
             layer["shader_name"] == "custom/map/map_building_with_mask-5" and
             layer["mask_texture_sha256"] == layer["mask_sprite"]["sha256"] and
             layer["material_serialized_unity_json"] and
             layer["vertices"] and layer["uv"] and layer["triangles"],
             f"Per-sprite masked source geometry incomplete: {layer['node']}")
    need(layer_pairings == source_pairings, "Runtime sprite/mask GUID pairs differ from source")
    need(report["camera"] == contract["camera"] and
         abs(report["applied_city_transparency"] - 1.0) < 1e-5 and
         report["lifecycle_boundary"] and report["particle_limit"],
         "Camera/Shader global/lifecycle scope incomplete")
    background_path = run / report["background_control_png"]
    need(background_path.is_file() and sha(background_path) == report["background_control_sha256"],
         "Background control PNG missing or changed")
    background = Image.open(background_path).convert("RGB")
    need(background.size == (1280, 720), "Background control extent differs")
    frames = report["frames"]
    need([frame["phase"] for frame in frames] == PHASES and len(frames) == 7,
         "Scaffold lifecycle phases differ")
    times = [frame["elapsed_seconds"] for frame in frames]
    need(times == sorted(times) and abs(times[0]) < 1e-5 and
         abs(times[-2] - 2.0) < 1e-5 and abs(times[-1] - 2.0) < 1e-5,
         "Source 2-second timer sample times differ")
    need(all(frame["build_3003_active"] and all(frame["animation_enabled"])
             for frame in frames[:-1]) and
         frames[-1]["build_3003_active"] is False and
         not any(frames[-1]["animation_enabled"]) and
         frames[-1]["observation_kind"] ==
         "source_mediator_2s_callback_equivalent_not_GameApp_execution",
         "Mediator-derived 2-second transition differs")
    nonempty = []
    for frame in frames:
        image_path = run / frame["png"]
        need(image_path.is_file() and image_path.parent == run and
             sha(image_path) == frame["png_sha256"],
             f"Frame hash missing or changed: {frame['phase']}")
        image = Image.open(image_path).convert("RGB")
        need(image.size == background.size, f"Frame extent differs: {frame['phase']}")
        diff = ImageChops.difference(image, background)
        bbox = diff.getbbox()
        count = sum(1 for p in diff.getdata() if p != (0, 0, 0))
        need(count == frame["non_background_pixels"],
             f"Reported non-background pixel count differs: {frame['phase']}")
        if bbox:
            x0, y0, x1, y1 = bbox
            need([frame["bbox_x0"], frame["bbox_y0"], frame["bbox_x1"], frame["bbox_y1"]] ==
                 [x0, image.height - y1, x1, image.height - y0],
                 f"Reported source-screen bbox differs: {frame['phase']}")
            nonempty.append(frame["phase"])
        else:
            need(count == 0, f"Empty bbox with changed pixels: {frame['phase']}")
        need(len(frame["layers"]) == 8 and
             all(layer["mask_property_block_verified"] for layer in frame["layers"]) and
             all(any(layer["node"] == source_layer["node"] and
                     layer["mask_texture_guid"] == source_layer["mask_sprite"]["guid"] and
                     layer["mask_texture_sha256"] == source_layer["mask_sprite"]["sha256"]
                     for source_layer in report["layers"])
                 for layer in frame["layers"]) and
             all(len(layer["world_vertices"]) == len(layer["screen_vertices"])
                 for layer in frame["layers"]),
             f"Per-frame source renderer geometry missing: {frame['phase']}")
    need(len(nonempty) >= 3 and "01_clip_mid" in nonempty and
         "02_one_second" in nonempty and "06_two_second_timer_after" in nonempty,
         "Scaffold mask/animation images are not visibly nonempty")
    print(f"PASS: masked Unity scaffold, {len(frames)} frames, "
          f"{len(nonempty)} visible against background, 8 source mask bindings, run={run}")


if __name__ == "__main__":
    main()
