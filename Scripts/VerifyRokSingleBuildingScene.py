import sys
import os

import unreal

PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = PROJECT_ROOT + "/Scripts"
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

TARGET_LEVEL = os.environ.get("ROK_SINGLE_BUILDING_LEVEL", "/Game/Maps/L_RokCitySingleBuilding")


def actor_label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def verify_single_building_scene():
    failures = []
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Missing map: {0}".format(TARGET_LEVEL))

    unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    building_actors = [actor for actor in actors if "RokBuilding" in list(actor.tags)]
    diagnostic_actors = [actor for actor in actors if "RokSingleBuildingDiagnostic" in list(actor.tags)]

    if len(building_actors) != 1:
        failures.append("expected exactly 1 RokBuilding actor, found {0}".format(len(building_actors)))
    if len(diagnostic_actors) != 1:
        failures.append("expected exactly 1 RokSingleBuildingDiagnostic actor, found {0}".format(len(diagnostic_actors)))

    sprite_actor = diagnostic_actors[0] if diagnostic_actors else None
    component_class = "missing"
    rotation = None
    location = None
    component_elements = None
    component_material = "missing"
    if sprite_actor:
        component = sprite_actor.get_component_by_class(unreal.MaterialBillboardComponent)
        if not component:
            failures.append("diagnostic building does not use MaterialBillboardComponent")
        else:
            component_class = component.get_class().get_name()
            try:
                elements = list(component.get_editor_property("elements"))
                component_elements = len(elements)
                if len(elements) != 1:
                    failures.append("diagnostic billboard should have exactly 1 material element, found {0}".format(len(elements)))
                elif elements[0].material:
                    component_material = elements[0].material.get_path_name()
            except Exception as exc:
                failures.append("failed to inspect billboard material elements: {0}".format(exc))
        rotation = sprite_actor.get_actor_rotation()
        location = sprite_actor.get_actor_location()
        if float(location.z) < 120.0:
            failures.append("diagnostic billboard anchor is too low for a bottom-anchored building: z={0:.1f}".format(float(location.z)))

    camera_actors = [actor for actor in actors if actor_label(actor) == "Camera_RokRuntimeCity"]
    if len(camera_actors) != 1:
        failures.append("expected exactly 1 Camera_RokRuntimeCity, found {0}".format(len(camera_actors)))
    elif not camera_actors[0].get_component_by_class(unreal.CameraComponent):
        failures.append("Camera_RokRuntimeCity missing CameraComponent")

    summary = "SingleBuildingVerify actors={0}, buildings={1}, diagnostics={2}, component={3}".format(
        len(actors),
        len(building_actors),
        len(diagnostic_actors),
        component_class,
    )
    if rotation:
        summary += ", actor_loc=({0:.1f},{1:.1f},{2:.1f}), rotation=(pitch={3:.2f}, yaw={4:.2f}, roll={5:.2f})".format(
            float(location.x),
            float(location.y),
            float(location.z),
            float(rotation.pitch),
            float(rotation.yaw),
            float(rotation.roll),
        )
    if component_elements is not None:
        summary += ", billboard_elements={0}, material={1}".format(component_elements, component_material)
    unreal.log(summary)

    if failures:
        for failure in failures:
            unreal.log_error(failure)
        raise RuntimeError("Rok single building scene verification failed")


if __name__ == "__main__":
    try:
        verify_single_building_scene()
    except Exception as exc:
        unreal.log_error(str(exc))
        sys.exit(1)
