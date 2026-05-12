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
    if ortho_width < 1750.0 or ortho_width > 1900.0:
        failures.append("Camera_RokRuntimeCity ortho width is outside 1750-1900: {0}".format(ortho_width))

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
    wall_mesh_anchors = []
    wall_sprite_anchors = []
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
        if folder.startswith("RokRuntimeCity/Wall"):
            if actor_class == "RokCitySpriteActor":
                wall_sprite_anchors.append(actor)
            elif actor.get_component_by_class(unreal.StaticMeshComponent):
                wall_mesh_anchors.append(actor)

    terrain_count = count_by_folder(actors, "RokRuntimeCity/Terrain")
    terrain_sprite_count = count_by_folder(actors, "RokRuntimeCity/TerrainSprites")
    visual_composite_count = count_by_folder(actors, "RokRuntimeCity/VisualComposite")
    road_count = count_by_folder(actors, "RokRuntimeCity/Roads")
    shadow_count = count_by_folder(actors, "RokRuntimeCity/Shadows")
    base_count = count_by_folder(actors, "RokRuntimeCity/Bases")
    resource_count = count_by_folder(actors, "RokRuntimeCity/Resources")
    decor_count = count_by_folder(actors, "RokRuntimeCity/Decor")
    wall_count = count_by_folder(actors, "RokRuntimeCity/Wall")

    scene_marker_count = count_by_folder(actors, "RokRuntimeCity/SceneMarkers")
    target_scene = any("RokTargetScene" in [str(tag) for tag in actor.tags] for actor in actors if actor.tags)
    layered_scene = any("RokLayeredScene" in [str(tag) for tag in actor.tags] for actor in actors if actor.tags)
    layered_ground_plate = any("RokLayeredGroundPlate" in [str(tag) for tag in actor.tags] for actor in actors if actor.tags)
    target_terrain_composite = any("RokTargetTerrainComposite" in [str(tag) for tag in actor.tags] for actor in actors if actor.tags)
    target_city_composite = any("RokTargetCityComposite" in [str(tag) for tag in actor.tags] for actor in actors if actor.tags)

    failures = []
    if layered_scene and visual_composite_count:
        failures.append("layered scene should not include full visual composite actors, found {0}".format(visual_composite_count))
    if layered_scene and target_city_composite:
        failures.append("layered scene still has RokTargetCityComposite tag")
    if layered_scene and target_terrain_composite:
        failures.append("layered scene still has RokTargetTerrainComposite tag")
    if mesh_building_cards:
        failures.append("found static mesh building cards: {0}".format(
            ", ".join(actor_label(actor) for actor in mesh_building_cards[:8])
        ))
    expected_buildings = 5 if layered_scene else (15 if target_scene else 45)
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
    if target_city_composite and terrain_count:
        failures.append("target city composite should not include legacy terrain actors, found {0}".format(terrain_count))
    if layered_scene and terrain_count < 2:
        failures.append("expected at least 2 layered ground actors, found {0}".format(terrain_count))
    if layered_scene and terrain_sprite_count < 1:
        failures.append("expected at least 1 layered ground sprite actor, found {0}".format(terrain_sprite_count))
    if layered_scene and not layered_ground_plate:
        failures.append("layered scene is missing the generated ground/road/base plate")
    if terrain_count < 5 and not (target_city_composite or layered_scene):
        failures.append("expected at least 5 terrain actors, found {0}".format(terrain_count))
    if road_count < 12 and not (target_scene and (target_terrain_composite or target_city_composite)) and not layered_scene:
        failures.append("expected at least 12 road actors, found {0}".format(road_count))
    if layered_scene and shadow_count:
        failures.append("layered scene should use source sprite shadows instead of shadow actors, found {0}".format(shadow_count))
    if not target_scene and not layered_scene and shadow_count < 45:
        failures.append("expected at least 45 shadow actors, found {0}".format(shadow_count))
    if layered_scene and base_count:
        failures.append("layered scene should use sprite floor pads instead of base pad actors, found {0}".format(base_count))
    if not target_scene and not layered_scene and base_count < 45:
        failures.append("expected at least 45 base pad actors, found {0}".format(base_count))
    if resource_count < 10 and not (target_city_composite or layered_scene):
        failures.append("expected at least 10 resource actors, found {0}".format(resource_count))
    if layered_scene and decor_count < 5:
        failures.append("expected at least 5 layered decor actors, found {0}".format(decor_count))
    if layered_scene and wall_mesh_anchors:
        failures.append("layered scene should use 2.5D wall sprites instead of static mesh wall anchors: {0}".format(
            ", ".join(actor_label(actor) for actor in wall_mesh_anchors[:4])
        ))
    if layered_scene and len(wall_sprite_anchors) < 1:
        failures.append("expected at least 1 layered 2.5D wall sprite actor, found {0}".format(len(wall_sprite_anchors)))
    if not target_scene and not layered_scene and decor_count < 10:
        failures.append("expected at least 10 decor actors, found {0}".format(decor_count))
    if target_city_composite and scene_marker_count:
        failures.append("target city composite should not include scene marker actors, found {0}".format(scene_marker_count))
    if layered_scene and scene_marker_count:
        failures.append("layered scene should not include scene marker actors, found {0}".format(scene_marker_count))
    if target_scene and not target_city_composite and scene_marker_count < 20:
        failures.append("expected at least 20 scene marker actors, found {0}".format(scene_marker_count))
    if not cameras:
        failures.append("missing Camera_RokRuntimeCity")
    else:
        failures.extend(verify_camera(cameras[0]))
    failures.extend(verify_default_map())

    unreal.log("Runtime city verification: actors={0}, sprite_buildings={1}, mesh_building_cards={2}, terrain={3}, terrain_sprites={4}, roads={5}, shadows={6}, bases={7}, resources={8}, decor={9}, wall={10}, wall_sprites={11}, wall_meshes={12}, markers={13}, cameras={14}, target_scene={15}, layered_scene={16}, layered_ground_plate={17}, target_terrain_composite={18}, target_city_composite={19}".format(
        len(actors),
        len(sprite_buildings),
        len(mesh_building_cards),
        terrain_count,
        terrain_sprite_count,
        road_count,
        shadow_count,
        base_count,
        resource_count,
        decor_count,
        wall_count,
        len(wall_sprite_anchors),
        len(wall_mesh_anchors),
        scene_marker_count,
        len(cameras),
        target_scene,
        layered_scene,
        layered_ground_plate,
        target_terrain_composite,
        target_city_composite,
    ))

    if failures:
        raise RuntimeError("; ".join(failures))


if __name__ == "__main__":
    verify_runtime_city()
