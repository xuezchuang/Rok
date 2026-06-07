import json
import math
import os

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
TARGET_LEVEL = os.environ.get("ROK_UNITY_RUNTIME_CITY_ALL_BUILDINGS_LEVEL", "/Game/Maps/L_RokUnityRuntimeCityAllBuildings")
OUTPUT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_runtime_city_all_buildings_visibility_inspect.json")
UE_UNITS_PER_UNITY_UNIT = 500.0
CAMERA_ASPECT = 16.0 / 10.0
CAMERA_VERTICAL_FOV = 7.5
CAMERA_FORWARD_UNITY = (0.0, -0.7, 0.7)


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


def actor_tags(actor):
    return [str(tag) for tag in list(actor.tags)]


def has_tag(actor, tag):
    return tag in actor_tags(actor)


def dot_vector(a, b):
    return float(a.x) * float(b.x) + float(a.y) * float(b.y) + float(a.z) * float(b.z)


def normalize_vector(vector):
    length = math.sqrt(dot_vector(vector, vector))
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


def actor_visibility_sample(camera_loc, camera_forward, horizontal_fov, aspect_ratio, actors):
    horizontal_radians = math.radians(horizontal_fov)
    vertical_radians = 2.0 * math.atan(math.tan(horizontal_radians * 0.5) / max(aspect_ratio, 0.01))
    world_up = unreal.Vector(0.0, 0.0, 1.0)
    right = normalize_vector(unreal.Vector(
        world_up.y * camera_forward.z - world_up.z * camera_forward.y,
        world_up.z * camera_forward.x - world_up.x * camera_forward.z,
        world_up.x * camera_forward.y - world_up.y * camera_forward.x,
    ))
    camera_up = normalize_vector(unreal.Vector(
        camera_forward.y * right.z - camera_forward.z * right.y,
        camera_forward.z * right.x - camera_forward.x * right.z,
        camera_forward.x * right.y - camera_forward.y * right.x,
    ))
    visible = []
    samples = []
    for actor in actors:
        bounds_origin, bounds_extent = actor.get_actor_bounds(False)
        to_actor = bounds_origin - camera_loc
        depth = dot_vector(to_actor, camera_forward)
        horizontal_angle = math.degrees(math.atan2(dot_vector(to_actor, right), depth)) if abs(depth) > 0.001 else 180.0
        vertical_angle = math.degrees(math.atan2(dot_vector(to_actor, camera_up), depth)) if abs(depth) > 0.001 else 180.0
        in_view = (
            depth > 0.0
            and abs(math.radians(horizontal_angle)) <= horizontal_radians * 0.5
            and abs(math.radians(vertical_angle)) <= vertical_radians * 0.5
        )
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        record = {
            "label": actor_label(actor),
            "class": actor.get_class().get_name(),
            "folder": actor_folder(actor),
            "location": [float(actor.get_actor_location().x), float(actor.get_actor_location().y), float(actor.get_actor_location().z)],
            "rotation": [float(actor.get_actor_rotation().pitch), float(actor.get_actor_rotation().yaw), float(actor.get_actor_rotation().roll)],
            "scale": [float(actor.get_actor_scale3d().x), float(actor.get_actor_scale3d().y), float(actor.get_actor_scale3d().z)],
            "bounds_origin": [float(bounds_origin.x), float(bounds_origin.y), float(bounds_origin.z)],
            "bounds_extent": [float(bounds_extent.x), float(bounds_extent.y), float(bounds_extent.z)],
            "actor_hidden": None,
            "component_visible": None,
            "component_hidden_in_game": None,
            "depth": depth,
            "horizontal_angle": horizontal_angle,
            "vertical_angle": vertical_angle,
            "in_view": in_view,
            "tags": actor_tags(actor),
        }
        try:
            record["actor_hidden"] = bool(actor.get_editor_property("hidden"))
        except Exception:
            pass
        if component:
            try:
                record["component_visible"] = bool(component.is_visible())
            except Exception:
                pass
            try:
                record["component_hidden_in_game"] = bool(component.get_editor_property("hidden_in_game"))
            except Exception:
                pass
            try:
                record["translucency_sort_priority"] = int(component.get_editor_property("translucency_sort_priority"))
            except Exception:
                pass
            material = component.get_material(0)
            if material:
                record["material"] = material.get_path_name()
                try:
                    record["blend_mode"] = str(material.get_editor_property("blend_mode"))
                except Exception:
                    pass
                try:
                    record["two_sided"] = bool(material.get_editor_property("two_sided"))
                except Exception:
                    pass
        samples.append(record)
        if in_view:
            visible.append(record["label"])
    return visible, samples


def inspect_scene():
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Missing map: {0}".format(TARGET_LEVEL))

    unreal.EditorLoadingAndSavingUtils.load_map(TARGET_LEVEL)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    tagged = [actor for actor in actors if has_tag(actor, "RokUnityRuntimeCityAllBuildings")]
    boundary = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeBoundarySource")]
    ground = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeGroundSource")]
    buildings = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeBuildingSource")]
    cameras = [actor for actor in actors if actor.get_class().get_name() == "CameraActor"]

    forward = camera_forward_ue()
    horizontal_fov = vertical_to_horizontal_fov(CAMERA_VERTICAL_FOV, CAMERA_ASPECT)
    camera_loc = camera_location((0.0, 0.0), 20.0)
    visible_buildings, building_samples = actor_visibility_sample(camera_loc, forward, horizontal_fov, CAMERA_ASPECT, buildings)
    visible_ground, ground_samples = actor_visibility_sample(camera_loc, forward, horizontal_fov, CAMERA_ASPECT, ground)
    visible_boundary, boundary_samples = actor_visibility_sample(camera_loc, forward, horizontal_fov, CAMERA_ASPECT, boundary)

    summary = {
        "target_level": TARGET_LEVEL,
        "actors": len(actors),
        "tagged": len(tagged),
        "boundary": len(boundary),
        "ground": len(ground),
        "buildings": len(buildings),
        "cameras": [
            {
                "label": actor_label(actor),
                "location": [float(actor.get_actor_location().x), float(actor.get_actor_location().y), float(actor.get_actor_location().z)],
                "rotation": [float(actor.get_actor_rotation().pitch), float(actor.get_actor_rotation().yaw), float(actor.get_actor_rotation().roll)],
                "tags": actor_tags(actor),
            }
            for actor in cameras
        ],
        "runtime_camera": {
            "view_center": [0.0, 0.0],
            "view_distance": 20.0,
            "horizontal_fov": horizontal_fov,
            "location": [float(camera_loc.x), float(camera_loc.y), float(camera_loc.z)],
            "forward": [float(forward.x), float(forward.y), float(forward.z)],
        },
        "visible_buildings": len(visible_buildings),
        "visible_ground": len(visible_ground),
        "visible_boundary": len(visible_boundary),
        "visible_building_labels": visible_buildings,
        "building_samples": building_samples,
        "ground_samples": ground_samples,
        "boundary_samples": boundary_samples,
    }
    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    with open(OUTPUT, "w", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)
    unreal.log("RuntimeCityAllBuildingsVisibility buildings={0}, visible={1}, ground={2}, boundary={3}".format(
        len(buildings), len(visible_buildings), len(visible_ground), len(visible_boundary)
    ))


if __name__ == "__main__":
    inspect_scene()
