import hashlib
import json
import math
import time
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
MIGRATION_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
WORLD_MANIFEST_PATH = MIGRATION_DIR / "unity_world_map.json"
ASSET_MANIFEST_PATH = MIGRATION_DIR / "unity_world_map_assets.json"
GENERATION_PATH = MIGRATION_DIR / "unity_world_map_ue_generation.json"
UE_VERIFY_PATH = MIGRATION_DIR / "unity_world_map_ue_verify.json"
TARGET_LEVEL = "/Game/Maps/L_RokUnityWorldMapPrototype"
TARGET_LEVEL_FILE = (
    PROJECT_ROOT
    / "Content"
    / "Maps"
    / "L_RokUnityWorldMapPrototype.umap"
)
CAPTURE_REPORT_PATH = MIGRATION_DIR / "unity_world_map_visual_capture.json"
TACTICAL_OUTPUT_NAME = "RokWorldMapPrototype_map_tactical.png"
OVERVIEW_OUTPUT_NAME = "RokWorldMapPrototype_3x3_overview.png"
CAPTURE_WIDTH = 1600
CAPTURE_HEIGHT = 1000
CAPTURE_ASPECT = CAPTURE_WIDTH / CAPTURE_HEIGHT


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path, expected_schema):
    if not path.exists():
        raise RuntimeError(f"Missing generated evidence: {path}")
    payload = json.loads(path.read_text(encoding="utf-8"))
    if payload.get("schema") != expected_schema:
        raise RuntimeError(
            f"Unexpected schema in {path.name}: {payload.get('schema')}"
        )
    return payload


def normalize(vector):
    length = math.sqrt(
        float(vector.x) * float(vector.x)
        + float(vector.y) * float(vector.y)
        + float(vector.z) * float(vector.z)
    )
    if length <= 0.0001:
        raise RuntimeError("Cannot normalize a zero camera direction")
    return unreal.Vector(
        float(vector.x) / length,
        float(vector.y) / length,
        float(vector.z) / length,
    )


def unity_direction_to_ue(direction):
    return normalize(
        unreal.Vector(
            -float(direction["x"]),
            float(direction["z"]),
            float(direction["y"]),
        )
    )


def look_rotation(forward):
    try:
        return unreal.MathLibrary.make_rot_from_x(forward)
    except Exception:
        return unreal.KismetMathLibrary.make_rot_from_x(forward)


def configure_capture_component(component, render_target, horizontal_fov):
    component.set_editor_property("texture_target", render_target)
    component.set_editor_property("fov_angle", float(horizontal_fov))
    component.set_editor_property(
        "capture_source",
        unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,
    )
    for property_name, value in (
        ("capture_every_frame", False),
        ("capture_on_movement", False),
        ("always_persist_rendering_state", True),
    ):
        try:
            component.set_editor_property(property_name, value)
        except Exception:
            pass

    post_process = component.get_editor_property("post_process_settings")
    for property_name, value in (
        ("override_auto_exposure_method", True),
        ("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
        ("override_auto_exposure_bias", True),
        ("auto_exposure_bias", 0.0),
        ("override_auto_exposure_apply_physical_camera_exposure", True),
        ("auto_exposure_apply_physical_camera_exposure", False),
        ("override_bloom_intensity", True),
        ("bloom_intensity", 0.0),
        ("override_vignette_intensity", True),
        ("vignette_intensity", 0.0),
        ("override_motion_blur_amount", True),
        ("motion_blur_amount", 0.0),
        ("override_tone_curve_amount", True),
        ("tone_curve_amount", 0.0),
        ("override_expand_gamut", True),
        ("expand_gamut", 0.0),
        ("override_blue_correction", True),
        ("blue_correction", 0.0),
    ):
        try:
            post_process.set_editor_property(property_name, value)
        except Exception:
            pass
    component.set_editor_property("post_process_settings", post_process)
    component.set_editor_property("post_process_blend_weight", 1.0)


def create_render_target():
    render_target = unreal.TextureRenderTarget2D()
    render_target.set_editor_property("size_x", CAPTURE_WIDTH)
    render_target.set_editor_property("size_y", CAPTURE_HEIGHT)
    render_target.set_editor_property(
        "render_target_format",
        unreal.TextureRenderTargetFormat.RTF_RGBA8,
    )
    render_target.set_editor_property(
        "clear_color",
        unreal.LinearColor(0.0, 0.0, 0.0, 1.0),
    )
    try:
        render_target.update_resource_immediate(True)
    except Exception:
        pass
    return render_target


def capture(world, output_name, target, forward, distance, horizontal_fov):
    camera_location = target - forward * float(distance)
    camera_rotation = look_rotation(forward)
    capture_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SceneCapture2D,
        camera_location,
        camera_rotation,
    )
    capture_actor.set_actor_label(f"RokTempWorldMapCapture_{output_name}")
    # Editor-mode spawning can use the level-editor click location even when
    # a transform was supplied. Re-apply and verify the source camera
    # transform so a successful PNG cannot come from the wrong viewpoint.
    capture_actor.set_actor_location(
        camera_location,
        sweep=False,
        teleport=True,
    )
    capture_actor.set_actor_rotation(
        camera_rotation,
        teleport_physics=True,
    )
    actual_location = capture_actor.get_actor_location()
    actual_rotation = capture_actor.get_actor_rotation()
    actual_forward = normalize(
        unreal.MathLibrary.get_forward_vector(actual_rotation)
    )
    location_error = math.sqrt(
        (float(actual_location.x) - float(camera_location.x)) ** 2
        + (float(actual_location.y) - float(camera_location.y)) ** 2
        + (float(actual_location.z) - float(camera_location.z)) ** 2
    )
    forward_dot = (
        float(actual_forward.x) * float(forward.x)
        + float(actual_forward.y) * float(forward.y)
        + float(actual_forward.z) * float(forward.z)
    )
    if location_error > 1.0 or forward_dot < 0.999:
        unreal.EditorLevelLibrary.destroy_actor(capture_actor)
        raise RuntimeError(
            "SceneCapture2D did not accept the requested source camera "
            f"transform: location_error={location_error:.3f}, "
            f"forward_dot={forward_dot:.6f}"
        )
    unreal.log(
        "Rok world-map capture camera verified: "
        f"location=({actual_location.x:.3f},"
        f"{actual_location.y:.3f},{actual_location.z:.3f}) "
        f"forward_dot={forward_dot:.6f}"
    )
    component = capture_actor.get_component_by_class(
        unreal.SceneCaptureComponent2D
    )
    if not component:
        unreal.EditorLevelLibrary.destroy_actor(capture_actor)
        raise RuntimeError("SceneCapture2D has no SceneCaptureComponent2D")

    render_target = create_render_target()
    output_path = MIGRATION_DIR / output_name
    output_suffix = output_path.suffix or ".png"
    pending_name = (
        f"{output_path.stem}.pending{output_suffix}"
    )
    pending_path = MIGRATION_DIR / pending_name
    if pending_path.exists():
        pending_path.unlink()
    try:
        configure_capture_component(
            component,
            render_target,
            horizontal_fov,
        )
        # The first off-screen view establishes texture-streaming requests for
        # this otherwise non-ticking capture. Export only after a second render
        # so a fresh process cannot record black fallback textures.
        component.capture_scene()
        time.sleep(1.5)
        unreal.AutomationLibrary.finish_loading_before_screenshot()
        component.capture_scene()
        time.sleep(0.75)
        export_result = unreal.RenderingLibrary.export_render_target(
            world,
            render_target,
            str(MIGRATION_DIR),
            pending_name,
        )
        if export_result is False:
            unreal.log_warning(
                "RenderingLibrary returned false after exporting "
                f"{pending_path}; validating the file directly."
            )
        time.sleep(0.2)
    finally:
        unreal.EditorLevelLibrary.destroy_actor(capture_actor)

    if not pending_path.exists() or pending_path.stat().st_size <= 0:
        raise RuntimeError(
            f"Fresh capture output was not created: {pending_path}"
        )
    pending_path.replace(output_path)
    return {
        "path": str(output_path.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "sha256": sha256(output_path),
        "width": CAPTURE_WIDTH,
        "height": CAPTURE_HEIGHT,
        "target_ue": {
            "x": float(target.x),
            "y": float(target.y),
            "z": float(target.z),
        },
        "camera_location_ue": {
            "x": float(actual_location.x),
            "y": float(actual_location.y),
            "z": float(actual_location.z),
        },
        "camera_forward_ue": {
            "x": float(forward.x),
            "y": float(forward.y),
            "z": float(forward.z),
        },
        "distance_ue": float(distance),
        "horizontal_fov_degrees": float(horizontal_fov),
    }


def capture_world_map():
    world_manifest = load_json(
        WORLD_MANIFEST_PATH,
        "rok.unity_world_map.v1",
    )
    asset_manifest = load_json(
        ASSET_MANIFEST_PATH,
        "rok.unity_world_map_assets.v1",
    )
    generation = load_json(
        GENERATION_PATH,
        "rok.unity_world_map_ue_generation.v1",
    )
    ue_verify = load_json(
        UE_VERIFY_PATH,
        "rok.unity_world_map_ue_verify.v1",
    )
    if ue_verify.get("failure_count") != 0:
        raise RuntimeError(
            "UE map verification must pass before visual capture"
        )
    expected_bindings = {
        "source_world_manifest": WORLD_MANIFEST_PATH,
        "source_asset_manifest": ASSET_MANIFEST_PATH,
        "source_ue_generation": GENERATION_PATH,
        "target_level_package": TARGET_LEVEL_FILE,
    }
    for role, path in expected_bindings.items():
        record = ue_verify.get(role, {})
        if record.get("sha256") != sha256(path):
            raise RuntimeError(
                f"UE verification is stale for {role}"
            )
    if generation.get("source_world_manifest", {}).get(
        "sha256"
    ) != sha256(WORLD_MANIFEST_PATH):
        raise RuntimeError("UE generation uses a stale world manifest")
    if generation.get("source_asset_manifest", {}).get(
        "sha256"
    ) != sha256(ASSET_MANIFEST_PATH):
        raise RuntimeError("UE generation uses a stale asset manifest")
    if generation.get("target_level_package", {}).get(
        "sha256"
    ) != sha256(TARGET_LEVEL_FILE):
        raise RuntimeError("UE generation does not bind the current map")
    if asset_manifest.get("source_world_manifest", {}).get(
        "sha256"
    ) != sha256(WORLD_MANIFEST_PATH):
        raise RuntimeError("Asset manifest is stale for the world manifest")
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError(f"Missing world-map prototype: {TARGET_LEVEL}")

    unreal.EditorLoadingAndSavingUtils.load_map(TARGET_LEVEL)
    unreal.AutomationLibrary.finish_loading_before_screenshot()
    world = unreal.EditorLevelLibrary.get_editor_world()
    current_package = (
        str(world.get_outermost().get_name())
        if world
        else "<no editor world>"
    )
    if current_package != TARGET_LEVEL:
        raise RuntimeError(
            f"Capture loaded {current_package}, expected {TARGET_LEVEL}"
        )

    camera = world_manifest["map_tactical_camera"]
    scale = float(
        world_manifest["coordinate_contract"][
            "ue_units_per_unity_unit"
        ]
    )
    tactical_forward = unity_direction_to_ue(camera["unity_forward"])
    tactical_capture = capture(
        world,
        TACTICAL_OUTPUT_NAME,
        unreal.Vector(0.0, 0.0, 0.0),
        tactical_forward,
        float(camera["distance_unity_units"]) * scale,
        float(camera["horizontal_fov_degrees_at_16_10"]),
    )

    tile_bounds = world_manifest["prototype_window"]["tile_bounds"]
    tile_size = float(
        world_manifest["coordinate_contract"]["tile_size_unity_units"]
    )
    focus = world_manifest["prototype_window"]["focus"][
        "unity_coordinate"
    ]
    unity_center_x = (
        (float(tile_bounds["min_x"]) + float(tile_bounds["max_x"]) + 1.0)
        * tile_size
        * 0.5
    )
    unity_center_y = (
        (float(tile_bounds["min_y"]) + float(tile_bounds["max_y"]) + 1.0)
        * tile_size
        * 0.5
    )
    overview_target = unreal.Vector(
        -(unity_center_x - float(focus["x"])) * scale,
        (unity_center_y - float(focus["y"])) * scale,
        0.0,
    )
    overview_vertical_fov = 30.0
    overview_horizontal_fov = 2.0 * math.degrees(
        math.atan(
            math.tan(math.radians(overview_vertical_fov) * 0.5)
            * CAPTURE_ASPECT
        )
    )
    overview_capture = capture(
        world,
        OVERVIEW_OUTPUT_NAME,
        overview_target,
        unreal.Vector(0.0, 0.0, -1.0),
        540000.0,
        overview_horizontal_fov,
    )

    report = {
        "schema": "rok.unity_world_map_visual_capture.v1",
        "target_level": TARGET_LEVEL,
        "source_world_manifest": {
            "path": str(
                WORLD_MANIFEST_PATH.relative_to(PROJECT_ROOT)
            ).replace("\\", "/"),
            "sha256": sha256(WORLD_MANIFEST_PATH),
        },
        "source_ue_verification": {
            "path": str(
                UE_VERIFY_PATH.relative_to(PROJECT_ROOT)
            ).replace("\\", "/"),
            "sha256": sha256(UE_VERIFY_PATH),
        },
        "source_asset_manifest": {
            "path": str(
                ASSET_MANIFEST_PATH.relative_to(PROJECT_ROOT)
            ).replace("\\", "/"),
            "sha256": sha256(ASSET_MANIFEST_PATH),
        },
        "source_ue_generation": {
            "path": str(
                GENERATION_PATH.relative_to(PROJECT_ROOT)
            ).replace("\\", "/"),
            "sha256": sha256(GENERATION_PATH),
        },
        "target_level_package": {
            "path": str(
                TARGET_LEVEL_FILE.relative_to(PROJECT_ROOT)
            ).replace("\\", "/"),
            "sha256": sha256(TARGET_LEVEL_FILE),
        },
        "map_tactical": tactical_capture,
        "prototype_3x3_overview": overview_capture,
    }
    CAPTURE_REPORT_PATH.write_text(
        json.dumps(report, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    unreal.log(
        "Rok Unity world-map captures exported: "
        f"{TACTICAL_OUTPUT_NAME}, {OVERVIEW_OUTPUT_NAME}"
    )


if __name__ == "__main__":
    capture_world_map()
