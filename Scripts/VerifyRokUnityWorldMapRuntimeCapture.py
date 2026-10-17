import hashlib
import json
import re
from pathlib import Path

from PIL import Image, ImageStat


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
LOG_PATH = (
    PROJECT_ROOT
    / "Saved"
    / "Logs"
    / "Runtime_RokWorldMapPhase1_Delayed.log"
)
MODULES_PATH = PROJECT_ROOT / "Binaries" / "Win64" / "UnrealEditor.modules"
AUTOMATION_PATH = (
    PROJECT_ROOT
    / "Saved"
    / "Automation"
    / "WorldMapPhase1_20260726_Final"
    / "index.json"
)
VISUAL_CAPTURE_PATH = (
    MIGRATION_DIR / "unity_world_map_visual_capture.json"
)
OUTPUT_PATH = MIGRATION_DIR / "unity_world_map_runtime_capture_verify.json"
TARGET_LEVEL = "/Game/Maps/L_RokUnityWorldMapPrototype"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_record(path):
    return {
        "path": str(path.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def verify_runtime_capture():
    failures = []
    required_paths = (
        LOG_PATH,
        MODULES_PATH,
        AUTOMATION_PATH,
        MIGRATION_DIR / "unity_world_map.json",
        MIGRATION_DIR / "unity_world_map_assets.json",
        MIGRATION_DIR / "unity_world_map_ue_generation.json",
        MIGRATION_DIR / "unity_world_map_ue_verify.json",
        VISUAL_CAPTURE_PATH,
    )
    for path in required_paths:
        if not path.exists():
            failures.append(f"Missing required runtime evidence: {path}")
    if failures:
        raise RuntimeError("\n".join(failures))

    log_text = LOG_PATH.read_text(encoding="utf-8", errors="replace")
    modules = json.loads(MODULES_PATH.read_text(encoding="utf-8"))
    asset_manifest = json.loads(
        (
            MIGRATION_DIR / "unity_world_map_assets.json"
        ).read_text(encoding="utf-8")
    )
    ue_verify = json.loads(
        (
            MIGRATION_DIR / "unity_world_map_ue_verify.json"
        ).read_text(encoding="utf-8")
    )
    expected_module = modules.get("Modules", {}).get("Rok", "")
    screenshot_matches = re.findall(
        r'Tracing Screenshot "([^"]+)" taken with size: (\d+) x (\d+)',
        log_text,
    )
    if not screenshot_matches:
        failures.append("Runtime log has no completed screenshot record")
        screenshot_name = ""
        logged_width = 0
        logged_height = 0
    else:
        screenshot_name, width_text, height_text = screenshot_matches[-1]
        logged_width = int(width_text)
        logged_height = int(height_text)

    screenshot_path = (
        PROJECT_ROOT
        / "Saved"
        / "Screenshots"
        / "WindowsEditor"
        / f"{screenshot_name}.png"
    )
    if not screenshot_path.exists():
        failures.append(
            f"Runtime screenshot referenced by log is missing: {screenshot_path}"
        )

    required_log_markers = (
        f"{TARGET_LEVEL} -game",
        "Game class is 'RokWorldMapGameMode'",
        f"Load map complete {TARGET_LEVEL}",
        "RokWorldMapAutomationCaptureDelaySeconds=",
        "RokWorldMapAutomationSelectCityDelaySeconds=",
        "Rok world-map collision policy applied:",
        "Rok world-map tree LOD initialized:",
        "Rok world-map unattended city selection scheduled after",
        "Rok world-map automation city selection: projected=true "
        "selected=true",
        "Rok world-map unattended screenshot scheduled after",
        "Rok world-map delayed runtime screenshot requested.",
    )
    for marker in required_log_markers:
        if marker not in log_text:
            failures.append(f"Runtime log is missing marker: {marker}")
    if expected_module and expected_module not in log_text:
        failures.append(
            f"Runtime did not load current editor module: {expected_module}"
        )
    collision_matches = re.findall(
        r"Rok world-map collision policy applied: "
        r"nonselectable_components=(\d+) selectable_components=(\d+)\.",
        log_text,
    )
    if not collision_matches:
        failures.append(
            "Runtime log has no completed world-map collision policy record"
        )
        collision_counts = {
            "nonselectable_components": 0,
            "selectable_components": 0,
        }
    else:
        nonselectable_text, selectable_text = collision_matches[-1]
        collision_counts = {
            "nonselectable_components": int(nonselectable_text),
            "selectable_components": int(selectable_text),
        }
        expected_nonselectable = (
            int(ue_verify.get("ground_actor_count", 0))
            + int(ue_verify.get("source_object_actor_count", 0))
        )
        expected_selectable = int(
            ue_verify.get("selectable_collision_component_count", 0)
        )
        if collision_counts["nonselectable_components"] != (
            expected_nonselectable
        ):
            failures.append(
                "Runtime collision policy did not normalize every terrain "
                "and source-object component"
            )
        if collision_counts["selectable_components"] != expected_selectable:
            failures.append(
                "Runtime collision policy did not configure every selectable "
                "world-map component"
            )
    tree_lod_matches = re.findall(
        r"Rok world-map tree LOD initialized: "
        r"components=(\d+) dxf=([0-9.]+) source_scale=([0-9.]+)\.",
        log_text,
    )
    if not tree_lod_matches:
        failures.append("Runtime log has no tree LOD initialization record")
        tree_lod_record = {}
    else:
        tree_count_text, tree_dxf_text, tree_scale_text = (
            tree_lod_matches[-1]
        )
        tree_lod_record = {
            "components": int(tree_count_text),
            "dxf": float(tree_dxf_text),
            "source_scale": float(tree_scale_text),
        }
        if tree_lod_record["components"] != int(
            ue_verify.get("tree_source_actor_count", 0)
        ):
            failures.append(
                "Runtime tree LOD policy did not bind every source tree actor"
            )
        expected_tree_dxf = float(
            asset_manifest.get("tree_scale_bake_dxf", 0.0)
        )
        expected_tree_scale = float(
            asset_manifest.get("tree_scale_bake_value", 0.0)
        )
        if abs(tree_lod_record["dxf"] - expected_tree_dxf) > 0.001:
            failures.append(
                "Runtime tree LOD initialized at the wrong dxf"
            )
        if (
            abs(tree_lod_record["source_scale"] - expected_tree_scale)
            > 0.000001
        ):
            failures.append(
                "Runtime tree LOD scale differs from the source curve"
            )
    selection_matches = re.findall(
        r"Rok world-map automation city selection: "
        r"projected=(true|false) selected=(true|false) "
        r"actor=([^ ]+) screen=\(([-0-9.]+),([-0-9.]+)\)\.",
        log_text,
    )
    if not selection_matches:
        failures.append(
            "Runtime log has no trace-based city selection record"
        )
        selection_record = {}
    else:
        (
            projected_text,
            selected_text,
            selected_actor,
            screen_x_text,
            screen_y_text,
        ) = selection_matches[-1]
        selection_record = {
            "projected": projected_text == "true",
            "selected": selected_text == "true",
            "actor": selected_actor,
            "screen": [float(screen_x_text), float(screen_y_text)],
        }
        if (
            not selection_record["projected"]
            or not selection_record["selected"]
            or selection_record["actor"]
            != "RokWorldPlayerCity_1146_157"
        ):
            failures.append(
                "Runtime trace did not select the authoritative player city"
            )

    image_record = {}
    if screenshot_path.exists():
        with Image.open(screenshot_path) as image:
            rgb = image.convert("RGB")
            width, height = rgb.size
            mean_rgb = [float(value) for value in ImageStat.Stat(rgb).mean]
            extrema = [[int(low), int(high)] for low, high in rgb.getextrema()]
            histogram = rgb.convert("L").histogram()
            pixel_count = width * height
            non_dark_fraction = (
                sum(histogram[12:]) / pixel_count if pixel_count else 0.0
            )
        if (width, height) != (logged_width, logged_height):
            failures.append(
                "Runtime screenshot dimensions do not match the log: "
                f"image={width}x{height} log={logged_width}x{logged_height}"
            )
        if min(mean_rgb) <= 20.0:
            failures.append(
                f"Runtime screenshot mean RGB is unexpectedly dark: {mean_rgb}"
            )
        if non_dark_fraction <= 0.8:
            failures.append(
                "Runtime screenshot is mostly dark: "
                f"non_dark_fraction={non_dark_fraction:.6f}"
            )
        if min(high - low for low, high in extrema) <= 80:
            failures.append(
                f"Runtime screenshot lacks expected color range: {extrema}"
            )
        image_record = {
            **source_record(screenshot_path),
            "width": width,
            "height": height,
            "mean_rgb": mean_rgb,
            "rgb_extrema": extrema,
            "non_dark_fraction": non_dark_fraction,
        }

    automation = json.loads(AUTOMATION_PATH.read_text(encoding="utf-8-sig"))
    if (
        automation.get("succeeded") != 7
        or automation.get("failed") != 0
        or automation.get("notRun") != 0
    ):
        failures.append(
            "Final Rok.WorldMap automation report is not 7/7 successful"
        )

    visual_capture = json.loads(
        VISUAL_CAPTURE_PATH.read_text(encoding="utf-8")
    )
    capture_bindings = {
        "source_world_manifest": (
            MIGRATION_DIR / "unity_world_map.json"
        ),
        "source_asset_manifest": (
            MIGRATION_DIR / "unity_world_map_assets.json"
        ),
        "source_ue_generation": (
            MIGRATION_DIR / "unity_world_map_ue_generation.json"
        ),
        "source_ue_verification": (
            MIGRATION_DIR / "unity_world_map_ue_verify.json"
        ),
        "target_level_package": (
            PROJECT_ROOT
            / "Content"
            / "Maps"
            / "L_RokUnityWorldMapPrototype.umap"
        ),
    }
    for role, path in capture_bindings.items():
        if visual_capture.get(role, {}).get("sha256") != sha256(path):
            failures.append(
                f"Visual capture is stale for {role}"
            )
    visual_images = {}
    for role in ("map_tactical", "prototype_3x3_overview"):
        record = visual_capture.get(role, {})
        image_path = PROJECT_ROOT / record.get("path", "")
        if not image_path.exists():
            failures.append(
                f"Visual capture image is missing for {role}: {image_path}"
            )
            continue
        if sha256(image_path) != record.get("sha256"):
            failures.append(
                f"Visual capture image hash changed for {role}"
            )
        with Image.open(image_path) as image:
            rgb = image.convert("RGB")
            width, height = rgb.size
            mean_rgb = [float(value) for value in ImageStat.Stat(rgb).mean]
            extrema = [
                [int(low), int(high)]
                for low, high in rgb.getextrema()
            ]
            histogram = rgb.convert("L").histogram()
            pixel_count = width * height
            non_dark_fraction = (
                sum(histogram[12:]) / pixel_count
                if pixel_count
                else 0.0
            )
        if (width, height) != (
            record.get("width"),
            record.get("height"),
        ):
            failures.append(
                f"Visual capture dimensions changed for {role}"
            )
        if min(mean_rgb) <= 15.0 or non_dark_fraction <= 0.4:
            failures.append(
                f"Visual capture is unexpectedly dark for {role}"
            )
        if min(high - low for low, high in extrema) <= 60:
            failures.append(
                f"Visual capture lacks color range for {role}"
            )
        visual_images[role] = {
            **source_record(image_path),
            "width": width,
            "height": height,
            "mean_rgb": mean_rgb,
            "rgb_extrema": extrema,
            "non_dark_fraction": non_dark_fraction,
        }

    payload = {
        "schema": "rok.unity_world_map_runtime_capture_verify.v1",
        "target_level": TARGET_LEVEL,
        "acceptance_scope": "local_visual_runtime_only",
        "failure_count": len(failures),
        "failures": failures,
        "runtime_log": source_record(LOG_PATH),
        "runtime_module": expected_module,
        "runtime_collision_policy_counts": collision_counts,
        "runtime_tree_lod": tree_lod_record,
        "runtime_city_selection": selection_record,
        "runtime_screenshot": image_record,
        "source_world_manifest": source_record(
            MIGRATION_DIR / "unity_world_map.json"
        ),
        "source_asset_manifest": source_record(
            MIGRATION_DIR / "unity_world_map_assets.json"
        ),
        "source_ue_generation": source_record(
            MIGRATION_DIR / "unity_world_map_ue_generation.json"
        ),
        "source_ue_verification": source_record(
            MIGRATION_DIR / "unity_world_map_ue_verify.json"
        ),
        "source_automation_report": source_record(AUTOMATION_PATH),
        "source_visual_capture": source_record(VISUAL_CAPTURE_PATH),
        "visual_capture_images": visual_images,
        "external_backend_502_observed": "HTTP 502" in log_text,
        "external_backend_note": (
            "Backend connectivity is outside the phase-one local world-map "
            "runtime acceptance scope."
        ),
    }
    OUTPUT_PATH.write_text(
        json.dumps(payload, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    if failures:
        raise RuntimeError("\n".join(failures))
    print(
        "Rok Unity world-map runtime capture verification passed: "
        f"image={logged_width}x{logged_height} "
        f"module={expected_module} automation=7/7"
    )


if __name__ == "__main__":
    verify_runtime_capture()
