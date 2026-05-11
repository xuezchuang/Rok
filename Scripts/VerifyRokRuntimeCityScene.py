import os
import sys

import unreal

PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import CreateRokRuntimeCityScene as runtime


TARGET_LEVEL = os.environ.get("ROK_RUNTIME_CITY_LEVEL", "/Game/Maps/L_RokCityRuntime")
EXPECTED_DEFAULT_MAP = "/Game/Maps/L_RokCityRuntime.L_RokCityRuntime"


def actor_label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def actor_folder(actor):
    try:
        return str(actor.get_folder_path())
    except Exception:
        return ""


def count_by_folder(actors, folder_prefix):
    return len([actor for actor in actors if actor_folder(actor).startswith(folder_prefix)])


def normalize(vector):
    size = (float(vector.x) ** 2 + float(vector.y) ** 2 + float(vector.z) ** 2) ** 0.5
    if size <= 0.0001:
        return unreal.Vector(0.0, 0.0, 0.0)
    return unreal.Vector(float(vector.x) / size, float(vector.y) / size, float(vector.z) / size)


def dot(a, b):
    return float(a.x) * float(b.x) + float(a.y) * float(b.y) + float(a.z) * float(b.z)


def sprite_actor_normal_dot_camera(actor):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    plane_normal = normalize(unreal.MathLibrary.get_up_vector(rotation))
    camera_direction = normalize(runtime.CAMERA_LOCATION - location)
    return dot(plane_normal, camera_direction)


def has_sprite_visual_component(actor):
    if actor.get_component_by_class(unreal.MaterialBillboardComponent):
        return True
    if actor.get_component_by_class(unreal.StaticMeshComponent):
        return True
    return False


def verify_camera(camera):
    failures = []
    component = camera.get_component_by_class(unreal.CameraComponent)
    if not component:
        return ["Camera_RokRuntimeCity is missing CameraComponent"]

    projection = component.get_editor_property("projection_mode")
    if projection != unreal.CameraProjectionMode.ORTHOGRAPHIC:
        failures.append("Camera_RokRuntimeCity is not orthographic")

    ortho_width = float(component.get_editor_property("ortho_width"))
    if ortho_width < 2600.0 or ortho_width > 3200.0:
        failures.append("Camera_RokRuntimeCity ortho width is outside 2600-3200: {0}".format(ortho_width))

    rotation = camera.get_actor_rotation()
    if abs(float(rotation.yaw) - 45.0) > 1.0:
        failures.append("Camera_RokRuntimeCity yaw is not approximately 45: {0}".format(float(rotation.yaw)))
    if abs(float(rotation.pitch) + 55.0) > 3.0:
        failures.append("Camera_RokRuntimeCity pitch is not approximately -55: {0}".format(float(rotation.pitch)))
    return failures


def verify_default_map():
    config_path = os.path.join(unreal.Paths.project_dir(), "Config", "DefaultEngine.ini")
    failures = []
    with open(config_path, "r", encoding="utf-8", errors="ignore") as config_file:
        config_text = config_file.read()
    if "EditorStartupMap={0}".format(EXPECTED_DEFAULT_MAP) not in config_text:
        failures.append("EditorStartupMap is not L_RokCityRuntime in DefaultEngine.ini")
    if "GameDefaultMap={0}".format(EXPECTED_DEFAULT_MAP) not in config_text:
        failures.append("GameDefaultMap is not L_RokCityRuntime in DefaultEngine.ini")
    return failures


def verify_runtime_city():
    unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()

    sprite_buildings = []
    mesh_building_cards = []
    cameras = []

    for actor in actors:
        label = actor_label(actor)
        folder = actor_folder(actor)
        actor_class = actor.get_class().get_name()
        if label == "Camera_RokRuntimeCity":
            cameras.append(actor)
        if actor_class == "RokCitySpriteActor" and actor.tags and "CityBuilding" in [str(tag) for tag in actor.tags]:
            sprite_buildings.append(actor)
        if actor_class != "RokCitySpriteActor" and folder.startswith("RokRuntimeCity/Buildings") and actor.get_component_by_class(unreal.StaticMeshComponent):
            mesh_building_cards.append(actor)

    terrain_count = count_by_folder(actors, "RokRuntimeCity/Terrain")
    road_count = count_by_folder(actors, "RokRuntimeCity/Roads")
    shadow_count = count_by_folder(actors, "RokRuntimeCity/Shadows")
    base_count = count_by_folder(actors, "RokRuntimeCity/Bases")
    resource_count = count_by_folder(actors, "RokRuntimeCity/Resources")
    decor_count = count_by_folder(actors, "RokRuntimeCity/Decor")

    scene_marker_count = count_by_folder(actors, "RokRuntimeCity/SceneMarkers")
    target_scene = any("RokTargetScene" in [str(tag) for tag in actor.tags] for actor in actors if actor.tags)

    failures = []
    if mesh_building_cards:
        failures.append("found static mesh building cards: {0}".format(
            ", ".join(actor_label(actor) for actor in mesh_building_cards[:8])
        ))
    expected_buildings = 15 if target_scene else 45
    if len(sprite_buildings) < expected_buildings:
        failures.append("expected at least {0} RokCitySpriteActor buildings, found {1}".format(expected_buildings, len(sprite_buildings)))
    bad_sprite_components = [
        actor_label(actor)
        for actor in sprite_buildings
        if not has_sprite_visual_component(actor)
    ]
    if bad_sprite_components:
        failures.append("RokCitySpriteActor buildings missing visual component: {0}".format(
            ", ".join(bad_sprite_components[:8])
        ))
    if terrain_count < 5:
        failures.append("expected at least 5 terrain actors, found {0}".format(terrain_count))
    if road_count < 12:
        failures.append("expected at least 12 road actors, found {0}".format(road_count))
    if not target_scene and shadow_count < 45:
        failures.append("expected at least 45 shadow actors, found {0}".format(shadow_count))
    if not target_scene and base_count < 45:
        failures.append("expected at least 45 base pad actors, found {0}".format(base_count))
    if resource_count < 10:
        failures.append("expected at least 10 resource actors, found {0}".format(resource_count))
    if not target_scene and decor_count < 10:
        failures.append("expected at least 10 decor actors, found {0}".format(decor_count))
    if target_scene and scene_marker_count < 20:
        failures.append("expected at least 20 scene marker actors, found {0}".format(scene_marker_count))
    if not cameras:
        failures.append("missing Camera_RokRuntimeCity")
    else:
        failures.extend(verify_camera(cameras[0]))
    failures.extend(verify_default_map())

    unreal.log("Runtime city verification: actors={0}, sprite_buildings={1}, mesh_building_cards={2}, terrain={3}, roads={4}, shadows={5}, bases={6}, resources={7}, decor={8}, markers={9}, cameras={10}, target_scene={11}".format(
        len(actors),
        len(sprite_buildings),
        len(mesh_building_cards),
        terrain_count,
        road_count,
        shadow_count,
        base_count,
        resource_count,
        decor_count,
        scene_marker_count,
        len(cameras),
        target_scene,
    ))

    if failures:
        raise RuntimeError("; ".join(failures))


if __name__ == "__main__":
    verify_runtime_city()
