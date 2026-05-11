import os
import sys
import time

import unreal


TARGET_LEVEL = os.environ.get("ROK_CAPTURE_LEVEL", "/Game/Maps/L_RokCityCluster501Age5")
OUTPUT_PATH = os.environ.get("ROK_CAPTURE_OUTPUT", os.path.join(unreal.Paths.project_saved_dir(), "RokCameraCapture.png"))
WIDTH = int(os.environ.get("ROK_CAPTURE_WIDTH", "1600"))
HEIGHT = int(os.environ.get("ROK_CAPTURE_HEIGHT", "950"))
CAMERA_LABEL = os.environ.get("ROK_CAPTURE_CAMERA_LABEL", "Camera_RokRuntimeCity")
SKIP_FINISH_LOADING = os.environ.get("ROK_CAPTURE_SKIP_FINISH_LOADING", "1") == "1"


def actor_label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def capture_camera_screenshot():
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Missing capture map: {0}".format(TARGET_LEVEL))

    unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    if not SKIP_FINISH_LOADING:
        unreal.AutomationLibrary.finish_loading_before_screenshot()

    camera = None
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor_label(actor) == CAMERA_LABEL:
            camera = actor
            break
    if not camera:
        raise RuntimeError("Missing camera actor: {0}".format(CAMERA_LABEL))

    output_dir = os.path.dirname(OUTPUT_PATH)
    if output_dir and not os.path.isdir(output_dir):
        os.makedirs(output_dir)

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


if __name__ == "__main__":
    try:
        capture_camera_screenshot()
    except Exception as exc:
        unreal.log_error(str(exc))
        sys.exit(1)
