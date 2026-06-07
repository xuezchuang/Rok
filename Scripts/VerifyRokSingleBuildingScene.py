import os
import sys

import unreal

PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = PROJECT_ROOT + "/Scripts"
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

TARGET_LEVEL = os.environ.get("ROK_SINGLE_BUILDING_LEVEL", "/Game/Maps/L_RokCitySingleBuilding")
SINGLE_CAMERA_MODE = os.environ.get("ROK_SINGLE_BUILDING_CAMERA_MODE", "unity_perspective").lower()
UNITY_CAMERA_VERTICAL_FOV = float(os.environ.get("ROK_SINGLE_BUILDING_UNITY_CAMERA_FOV", "12.0"))
UNITY_CAMERA_ASPECT_RATIO = float(os.environ.get("ROK_SINGLE_BUILDING_CAMERA_ASPECT", str(16.0 / 9.0)))


def actor_label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def component_by_name(actor, component_class, component_name):
    try:
        components = actor.get_components_by_class(component_class)
    except Exception:
        components = []
    for component in components:
        if component.get_name() == component_name:
            return component
    return None


def enum_text(value):
    return str(value).upper()


def ue_horizontal_fov_from_unity_vertical_fov(vertical_fov, aspect_ratio):
    import math

    return math.degrees(2.0 * math.atan(math.tan(math.radians(vertical_fov) * 0.5) * aspect_ratio))


def verify_card_material(card_component, failures):
    material = card_component.get_material(0) if card_component else None
    if not material:
        failures.append("CardMeshComponent has no material")
        return "missing"

    blend_mode = material.get_editor_property("blend_mode")
    two_sided = material.get_editor_property("two_sided")
    shading_model = material.get_editor_property("shading_model")
    clip_value = material.get_editor_property("opacity_mask_clip_value")
    if "MASKED" not in enum_text(blend_mode):
        failures.append("building card material must be Masked, got {0}".format(blend_mode))
    if not two_sided:
        failures.append("building card material must be TwoSided")
    if "UNLIT" not in enum_text(shading_model):
        failures.append("building card material must be Unlit, got {0}".format(shading_model))
    if float(clip_value) < 0.3:
        failures.append("building card opacity mask clip should be diagnostic-clean >= 0.3, got {0}".format(clip_value))
    return "{0} blend={1} two_sided={2} shading={3} clip={4}".format(
        material.get_path_name(),
        blend_mode,
        two_sided,
        shading_model,
        clip_value,
    )


def verify_single_building_scene():
    failures = []
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Missing map: {0}".format(TARGET_LEVEL))

    unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    building_actors = [actor for actor in actors if actor.get_class().get_name() == "RokBuildingActor"]
    diagnostic_actors = [actor for actor in actors if "RokSingleBuildingDiagnostic" in list(actor.tags)]

    if len(building_actors) != 1:
        failures.append("expected exactly 1 ARokBuildingActor, found {0}".format(len(building_actors)))
    if len(diagnostic_actors) != 1:
        failures.append("expected exactly 1 RokSingleBuildingDiagnostic actor, found {0}".format(len(diagnostic_actors)))

    building_actor = building_actors[0] if building_actors else None
    card_material_summary = "missing"
    if building_actor:
        if abs(float(building_actor.get_actor_location().z)) > 0.01:
            failures.append("building root must stay at ground-grid z=0, got {0:.2f}".format(float(building_actor.get_actor_location().z)))

        card = component_by_name(building_actor, unreal.StaticMeshComponent, "CardMeshComponent")
        footprint_collision = component_by_name(building_actor, unreal.BoxComponent, "FootprintCollision")
        footprint_highlight = component_by_name(building_actor, unreal.StaticMeshComponent, "FootprintHighlightComponent")
        if not card:
            failures.append("missing CardMeshComponent")
        else:
            try:
                card_collision = card.get_editor_property("collision_enabled")
                if "NO_COLLISION" not in enum_text(card_collision):
                    failures.append("CardMeshComponent must not participate in click/selection collision, got {0}".format(card_collision))
            except Exception:
                pass
            if card.get_editor_property("render_custom_depth"):
                failures.append("CardMeshComponent must not use CustomDepth outline")
            card_material_summary = verify_card_material(card, failures)

        if not footprint_collision:
            failures.append("missing FootprintCollision")
        else:
            try:
                footprint_collision_mode = footprint_collision.get_editor_property("collision_enabled")
                if "QUERY_ONLY" not in enum_text(footprint_collision_mode):
                    failures.append("FootprintCollision must be QueryOnly, got {0}".format(footprint_collision_mode))
            except Exception:
                pass
        if not footprint_highlight:
            failures.append("missing FootprintHighlightComponent")

    if SINGLE_CAMERA_MODE == "orthographic":
        camera_actors = [actor for actor in actors if actor.get_class().get_name() == "RokCameraPawn"]
    else:
        camera_actors = [
            actor
            for actor in actors
            if actor.get_class().get_name() == "CameraActor"
            and "RokUnityPerspectiveCamera" in [str(tag) for tag in list(actor.tags)]
        ]
    if len(camera_actors) != 1:
        failures.append("expected exactly 1 {0} camera, found {1}".format(SINGLE_CAMERA_MODE, len(camera_actors)))
    else:
        camera_component = camera_actors[0].get_component_by_class(unreal.CameraComponent)
        if not camera_component:
            failures.append("single-building camera missing CameraComponent")
        else:
            projection_mode = camera_component.get_editor_property("projection_mode")
            if SINGLE_CAMERA_MODE == "orthographic":
                if "ORTHOGRAPHIC" not in enum_text(projection_mode):
                    failures.append("diagnostic camera must be Orthographic, got {0}".format(projection_mode))
            else:
                if "PERSPECTIVE" not in enum_text(projection_mode):
                    failures.append("Unity camera must be Perspective, got {0}".format(projection_mode))
                expected_fov = ue_horizontal_fov_from_unity_vertical_fov(
                    UNITY_CAMERA_VERTICAL_FOV,
                    UNITY_CAMERA_ASPECT_RATIO,
                )
                actual_fov = float(camera_component.get_editor_property("field_of_view"))
                if abs(actual_fov - expected_fov) > 0.25:
                    failures.append("Unity camera horizontal FOV should be {0:.2f}, got {1:.2f}".format(expected_fov, actual_fov))
            settings = camera_component.get_editor_property("post_process_settings")
            try:
                if not settings.get_editor_property("override_auto_exposure_method"):
                    failures.append("diagnostic camera must override auto exposure method")
            except Exception:
                pass

    summary = "SingleBuildingVerify actors={0}, buildings={1}, diagnostics={2}, material={3}".format(
        len(actors),
        len(building_actors),
        len(diagnostic_actors),
        card_material_summary,
    )
    print(summary)
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
