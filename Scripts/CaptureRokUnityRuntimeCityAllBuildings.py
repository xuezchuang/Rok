import math
import os

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
TARGET_LEVEL = os.environ.get("ROK_UNITY_RUNTIME_CITY_ALL_BUILDINGS_LEVEL", "/Game/Maps/L_RokUnityRuntimeCityAllBuildings")
OUTPUT_DIR = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration")
OUTPUT_NAME = os.environ.get("ROK_UNITY_RUNTIME_CITY_ALL_BUILDINGS_CAPTURE_NAME", "runtime_city_all_buildings_capture.png")
UE_UNITS_PER_UNITY_UNIT = 500.0
CAMERA_ASPECT = 16.0 / 10.0
CAMERA_VERTICAL_FOV = 7.5
CAMERA_FORWARD_UNITY = (0.0, -0.7, 0.7)


def normalize_vector(vector):
    length = math.sqrt(float(vector.x) * float(vector.x) + float(vector.y) * float(vector.y) + float(vector.z) * float(vector.z))
    if length < 0.0001:
        return unreal.Vector(0.0, 0.0, 0.0)
    return unreal.Vector(float(vector.x) / length, float(vector.y) / length, float(vector.z) / length)


def camera_forward_ue():
    x, y, z = CAMERA_FORWARD_UNITY
    return normalize_vector(unreal.Vector(x, z, y))


def unity_planar_to_ue(x, z, y=0.0):
    return unreal.Vector(-x * UE_UNITS_PER_UNITY_UNIT, z * UE_UNITS_PER_UNITY_UNIT, y * UE_UNITS_PER_UNITY_UNIT)


def camera_location(view_center, view_distance):
    return unity_planar_to_ue(view_center[0], view_center[1], 0.0) - camera_forward_ue() * (view_distance * UE_UNITS_PER_UNITY_UNIT)


def vertical_to_horizontal_fov(vertical_fov, aspect_ratio):
    return 2.0 * math.degrees(math.atan(math.tan(math.radians(vertical_fov) * 0.5) * aspect_ratio))


def look_rotation_from_forward(forward):
    try:
        return unreal.MathLibrary.make_rot_from_x(forward)
    except Exception:
        return unreal.KismetMathLibrary.make_rot_from_x(forward)


def capture_scene():
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Missing map: {0}".format(TARGET_LEVEL))
    unreal.EditorLoadingAndSavingUtils.load_map(TARGET_LEVEL)
    world = unreal.EditorLevelLibrary.get_editor_world()

    render_target = unreal.TextureRenderTarget2D()
    render_target.set_editor_property("size_x", 1600)
    render_target.set_editor_property("size_y", 1000)
    try:
        render_target.set_editor_property("render_target_format", unreal.TextureRenderTargetFormat.RTF_RGBA8)
    except Exception:
        pass
    render_target.set_editor_property("clear_color", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
    try:
        render_target.update_resource_immediate(True)
    except Exception:
        pass

    forward = camera_forward_ue()
    capture = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SceneCapture2D,
        camera_location((0.0, 0.0), 20.0),
        look_rotation_from_forward(forward),
    )
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    component.set_editor_property("texture_target", render_target)
    component.set_editor_property("fov_angle", vertical_to_horizontal_fov(CAMERA_VERTICAL_FOV, CAMERA_ASPECT))
    component.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    try:
        pp = component.get_editor_property("post_process_settings")
        for prop, value in [
            ("override_auto_exposure_method", True),
            ("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
            ("override_auto_exposure_bias", True),
            ("auto_exposure_bias", 0.0),
            ("override_auto_exposure_apply_physical_camera_exposure", True),
            ("auto_exposure_apply_physical_camera_exposure", False),
            ("override_tone_curve_amount", True),
            ("tone_curve_amount", 0.0),
            ("override_blue_correction", True),
            ("blue_correction", 0.0),
            ("override_expand_gamut", True),
            ("expand_gamut", 0.0),
            ("override_bloom_intensity", True),
            ("bloom_intensity", 0.0),
            ("override_vignette_intensity", True),
            ("vignette_intensity", 0.0),
            ("override_motion_blur_amount", True),
            ("motion_blur_amount", 0.0),
        ]:
            try:
                pp.set_editor_property(prop, value)
            except Exception:
                pass
        component.set_editor_property("post_process_settings", pp)
        component.set_editor_property("post_process_blend_weight", 1.0)
    except Exception as error:
        unreal.log_warning("Skipping runtime city capture post-process setup: {0}".format(error))
    try:
        component.set_editor_property("always_persist_rendering_state", True)
    except Exception:
        pass
    component.capture_scene()
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    if not unreal.RenderingLibrary.export_render_target(world, render_target, OUTPUT_DIR, OUTPUT_NAME):
        raise RuntimeError("Failed to export render target")
    unreal.EditorLevelLibrary.destroy_actor(capture)
    unreal.log("Captured runtime city all-buildings to {0}".format(os.path.join(OUTPUT_DIR, OUTPUT_NAME)))


if __name__ == "__main__":
    capture_scene()
