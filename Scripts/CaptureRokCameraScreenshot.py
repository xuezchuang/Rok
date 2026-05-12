import os
import sys
import time

import unreal

PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokPrototypeScene as proto


TARGET_LEVEL = os.environ.get("ROK_CAPTURE_LEVEL", "/Game/Maps/L_RokCityCluster501Age5")
OUTPUT_PATH = os.environ.get("ROK_CAPTURE_OUTPUT", os.path.join(unreal.Paths.project_saved_dir(), "RokCameraCapture.png"))
WIDTH = int(os.environ.get("ROK_CAPTURE_WIDTH", "1600"))
HEIGHT = int(os.environ.get("ROK_CAPTURE_HEIGHT", "950"))
CAMERA_LABEL = os.environ.get("ROK_CAPTURE_CAMERA_LABEL", "Camera_RokRuntimeCity")
SKIP_FINISH_LOADING = os.environ.get("ROK_CAPTURE_SKIP_FINISH_LOADING", "1") == "1"
CAPTURE_METHOD = os.environ.get("ROK_CAPTURE_METHOD", "scene_capture")
QUIT_EDITOR_ON_DONE = os.environ.get("ROK_CAPTURE_QUIT_EDITOR", "0") == "1"
USE_SPRITE_PROXY_PLANES = os.environ.get("ROK_CAPTURE_USE_PROXY_PLANES", "1") == "1"


def actor_label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def find_camera_actor():
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Missing capture map: {0}".format(TARGET_LEVEL))

    unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    if not SKIP_FINISH_LOADING:
        unreal.AutomationLibrary.finish_loading_before_screenshot()

    camera = None
    candidates = []
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor_label(actor) == CAMERA_LABEL:
            camera_component = actor.get_component_by_class(unreal.CameraComponent)
            if camera_component:
                candidates.append(actor)
    if candidates:
        camera = max(candidates, key=lambda actor: actor.get_actor_location().z)
    if not camera:
        raise RuntimeError("Missing camera actor: {0}".format(CAMERA_LABEL))
    location = camera.get_actor_location()
    rotation = camera.get_actor_rotation()
    camera_component = camera.get_component_by_class(unreal.CameraComponent)
    ortho_width = camera_component.get_editor_property("ortho_width") if camera_component else "n/a"
    unreal.log(
        "Rok camera capture using {0}: class={1}, location=({2:.2f},{3:.2f},{4:.2f}), rotation=({5:.2f},{6:.2f},{7:.2f}), ortho_width={8}".format(
            actor_label(camera),
            camera.get_class().get_name(),
            location.x,
            location.y,
            location.z,
            rotation.roll,
            rotation.pitch,
            rotation.yaw,
            ortho_width,
        )
    )
    return camera


def ensure_output_dir():
    output_dir = os.path.dirname(OUTPUT_PATH)
    if output_dir and not os.path.isdir(output_dir):
        os.makedirs(output_dir)
    return output_dir


def copy_camera_projection(source_camera, capture_component):
    camera_component = source_camera.get_component_by_class(unreal.CameraComponent)
    if not camera_component:
        raise RuntimeError("{0} is missing CameraComponent".format(CAMERA_LABEL))

    projection_mode = camera_component.get_editor_property("projection_mode")
    capture_component.set_editor_property("projection_type", projection_mode)
    if projection_mode == unreal.CameraProjectionMode.ORTHOGRAPHIC:
        capture_component.set_editor_property("ortho_width", camera_component.get_editor_property("ortho_width"))
    else:
        capture_component.set_editor_property("fov_angle", camera_component.get_editor_property("field_of_view"))
    try:
        capture_component.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    except Exception:
        pass
    for prop, value in [
        ("capture_every_frame", False),
        ("capture_on_movement", False),
        ("always_persist_rendering_state", True),
    ]:
        try:
            capture_component.set_editor_property(prop, value)
        except Exception:
            pass


def create_render_target(world):
    rendering_library = getattr(unreal, "RenderingLibrary", None) or getattr(unreal, "KismetRenderingLibrary", None)
    if not rendering_library:
        raise RuntimeError("RenderingLibrary is unavailable in this UE Python environment")
    try:
        return rendering_library.create_render_target2d(
            world,
            WIDTH,
            HEIGHT,
            unreal.TextureRenderTargetFormat.RTF_RGBA8,
            unreal.LinearColor(0.0, 0.0, 0.0, 1.0),
            False,
        )
    except TypeError:
        return rendering_library.create_render_target2d(world, WIDTH, HEIGHT)


def material_billboard_element(component):
    try:
        elements = component.get_editor_property("elements")
    except Exception:
        return None
    if not elements:
        return None
    return elements[0]


def element_property(element, name, default=None):
    try:
        return element.get_editor_property(name)
    except Exception:
        return default


def camera_facing_plane_rotation(camera, location):
    to_camera = camera.get_actor_location() - location
    up_vector = unreal.MathLibrary.get_up_vector(camera.get_actor_rotation())
    try:
        return unreal.MathLibrary.make_rot_from_zy(to_camera, up_vector)
    except Exception:
        return unreal.MathLibrary.find_look_at_rotation(location, camera.get_actor_location())


def component_set_visible(component, visible):
    try:
        component.set_visibility(visible)
    except Exception:
        pass


def spawn_sprite_proxy_planes(camera):
    if not USE_SPRITE_PROXY_PLANES:
        return [], []

    plane_mesh = proto.load_primitive("plane")
    spawned = []
    hidden_components = []
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_class().get_name() != "RokCitySpriteActor":
            continue
        billboard = actor.get_component_by_class(unreal.MaterialBillboardComponent)
        if not billboard:
            continue
        element = material_billboard_element(billboard)
        if not element:
            continue
        material = element_property(element, "material")
        width = float(element_property(element, "base_size_x", 100.0) or 100.0)
        height = float(element_property(element, "base_size_y", 100.0) or 100.0)
        location = actor.get_actor_location()
        proxy = unreal.EditorLevelLibrary.spawn_actor_from_object(
            plane_mesh,
            location,
            camera_facing_plane_rotation(camera, location),
        )
        proxy.set_actor_label("RokTempCaptureProxy_{0}".format(actor_label(actor)))
        proxy.set_actor_scale3d(unreal.Vector(width / 100.0, height / 100.0, 1.0))
        proxy.set_actor_hidden_in_game(False)
        mesh_component = proxy.get_component_by_class(unreal.StaticMeshComponent)
        if mesh_component:
            mesh_component.set_material(0, material)
            mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            try:
                mesh_component.set_editor_property(
                    "translucency_sort_priority",
                    billboard.get_editor_property("translucency_sort_priority"),
                )
            except Exception:
                pass
        component_set_visible(billboard, False)
        hidden_components.append(billboard)
        spawned.append(proxy)
    unreal.log("Rok camera capture proxy planes spawned: {0}".format(len(spawned)))
    return spawned, hidden_components


def cleanup_capture_proxies(proxy_actors, hidden_components):
    for component in hidden_components:
        component_set_visible(component, True)
    for actor in proxy_actors:
        try:
            unreal.EditorLevelLibrary.destroy_actor(actor)
        except Exception:
            pass


def export_render_target(world, render_target):
    output_dir = ensure_output_dir()
    output_name = os.path.basename(OUTPUT_PATH)
    rendering_library = getattr(unreal, "RenderingLibrary", None) or getattr(unreal, "KismetRenderingLibrary", None)
    rendering_library.export_render_target(world, render_target, output_dir, output_name)
    if not os.path.exists(OUTPUT_PATH):
        raise RuntimeError("RenderTarget export did not create output file: {0}".format(OUTPUT_PATH))


def capture_with_scene_capture(camera):
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        raise RuntimeError("Editor world is unavailable")

    render_target = create_render_target(world)
    proxy_actors, hidden_components = spawn_sprite_proxy_planes(camera)
    scene_capture = None
    try:
        scene_capture = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SceneCapture2D,
            camera.get_actor_location(),
            camera.get_actor_rotation(),
        )
        scene_capture.set_actor_label("RokTempSceneCaptureForScreenshot")
        capture_component = scene_capture.get_component_by_class(unreal.SceneCaptureComponent2D)
        if not capture_component:
            raise RuntimeError("SceneCapture2D is missing SceneCaptureComponent2D")
        copy_camera_projection(camera, capture_component)
        capture_component.set_editor_property("texture_target", render_target)
        capture_component.capture_scene()
        time.sleep(0.5)
        export_render_target(world, render_target)
    finally:
        if scene_capture:
            unreal.EditorLevelLibrary.destroy_actor(scene_capture)
        cleanup_capture_proxies(proxy_actors, hidden_components)
    unreal.log("Rok camera scene capture exported: {0}".format(OUTPUT_PATH))


def capture_with_highres_screenshot(camera):
    ensure_output_dir()
    task = unreal.AutomationLibrary.take_high_res_screenshot(
        WIDTH,
        HEIGHT,
        OUTPUT_PATH,
        camera=camera,
        mask_enabled=False,
        capture_hdr=False,
        delay=0.2,
    )
    for _ in range(120):
        try:
            if task.is_task_done():
                break
        except Exception:
            pass
        time.sleep(0.1)

    unreal.log("Rok camera screenshot requested: {0}".format(OUTPUT_PATH))


def prepare_editor_viewport(camera):
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(
        camera.get_actor_location(),
        camera.get_actor_rotation(),
    )
    unreal.log("Rok editor viewport prepared from camera: {0}".format(CAMERA_LABEL))


def capture_camera_screenshot():
    camera = find_camera_actor()
    if CAPTURE_METHOD == "editor_viewport":
        prepare_editor_viewport(camera)
    elif CAPTURE_METHOD == "highres":
        capture_with_highres_screenshot(camera)
    else:
        capture_with_scene_capture(camera)


if __name__ == "__main__":
    try:
        capture_camera_screenshot()
    except Exception as exc:
        unreal.log_error(str(exc))
        sys.exit(1)
    finally:
        if QUIT_EDITOR_ON_DONE:
            try:
                unreal.SystemLibrary.quit_editor()
            except Exception:
                pass
