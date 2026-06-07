import json
import math
import os

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
TARGET_LEVEL = os.environ.get("ROK_UNITY_GAMEAPP_CITY_LEVEL", "/Game/Maps/L_RokUnityGameAppCity")
OUTPUT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_gameapp_city_visibility_inspect.json")
UE_UNITS_PER_UNITY_UNIT = 500.0
CAMERA_ASPECT = 16.0 / 10.0
CAMERA_VERTICAL_FOV = 7.5
CAMERA_FORWARD_UNITY = (0.0, -0.70710678, 0.70710678)


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


def camera_forward_ue():
    x, y, z = CAMERA_FORWARD_UNITY
    length = math.sqrt(x * x + y * y + z * z)
    if length < 0.0001:
        return unreal.Vector(0.0, 0.70710678, -0.70710678)
    return unreal.Vector(x / length, z / length, y / length)


def unity_planar_to_ue(x, z, y=0.0):
    return unreal.Vector(-x * UE_UNITS_PER_UNITY_UNIT, z * UE_UNITS_PER_UNITY_UNIT, y * UE_UNITS_PER_UNITY_UNIT)


def camera_location(view_center, view_distance):
    return unity_planar_to_ue(view_center[0], view_center[1], 0.0) - camera_forward_ue() * (view_distance * UE_UNITS_PER_UNITY_UNIT)


def vertical_to_horizontal_fov(vertical_fov, aspect_ratio):
    return 2.0 * math.degrees(math.atan(math.tan(math.radians(vertical_fov) * 0.5) * aspect_ratio))


def dot_vector(a, b):
    return float(a.x) * float(b.x) + float(a.y) * float(b.y) + float(a.z) * float(b.z)


def normalize_vector(vector):
    length = math.sqrt(dot_vector(vector, vector))
    if length < 0.0001:
        return unreal.Vector(0.0, 0.0, 0.0)
    return unreal.Vector(float(vector.x) / length, float(vector.y) / length, float(vector.z) / length)


def actor_visibility_sample(camera_loc, camera_forward, horizontal_fov, aspect_ratio, actors):
    horizontal_radians = math.radians(horizontal_fov)
    vertical_radians = 2.0 * math.atan(math.tan(horizontal_radians * 0.5) / max(aspect_ratio, 0.01))
    up = unreal.Vector(0.0, 0.0, 1.0)
    right = normalize_vector(unreal.Vector(
        up.y * camera_forward.z - up.z * camera_forward.y,
        up.z * camera_forward.x - up.x * camera_forward.z,
        up.x * camera_forward.y - up.y * camera_forward.x,
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
        hidden_in_game = None
        try:
            hidden_in_game = bool(actor.get_editor_property("hidden"))
        except Exception:
            pass
        record = {
            "label": actor_label(actor),
            "class": actor.get_class().get_name(),
            "folder": actor_folder(actor),
            "location": [float(actor.get_actor_location().x), float(actor.get_actor_location().y), float(actor.get_actor_location().z)],
            "bounds_origin": [float(bounds_origin.x), float(bounds_origin.y), float(bounds_origin.z)],
            "bounds_extent": [float(bounds_extent.x), float(bounds_extent.y), float(bounds_extent.z)],
            "hidden_in_game": hidden_in_game,
            "component_visible": None,
            "component_hidden_in_game": None,
            "depth": depth,
            "horizontal_angle": horizontal_angle,
            "vertical_angle": vertical_angle,
            "in_view": in_view,
            "tags": actor_tags(actor),
        }
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component:
            try:
                record["component_visible"] = bool(component.is_visible())
            except Exception:
                pass
            try:
                record["component_hidden_in_game"] = bool(component.b_hidden_in_game)
            except Exception:
                pass
            material = component.get_material(0)
            if material:
                record["material"] = material.get_path_name()
                try:
                    record["blend_mode"] = str(material.get_editor_property("blend_mode"))
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
    testunity = [actor for actor in actors if has_tag(actor, "RokTestUnityLayout")]
    buildings = [actor for actor in testunity if has_tag(actor, "UnityDenseBuilding")]
    palms = [actor for actor in testunity if has_tag(actor, "UnityPalm")]
    flags = [actor for actor in testunity if has_tag(actor, "UnityFlag")]
    units = [actor for actor in testunity if has_tag(actor, "UnityUnit")]
    walls = [actor for actor in testunity if has_tag(actor, "UnityWallSegment")]
    ground = [actor for actor in testunity if actor_folder(actor).endswith("/Ground") or "/Ground" in actor_folder(actor)]
    cameras = [actor for actor in actors if actor.get_class().get_name() == "CameraActor"]

    horizontal_fov = vertical_to_horizontal_fov(CAMERA_VERTICAL_FOV, CAMERA_ASPECT)
    forward = camera_forward_ue()
    camera_modes = {
        "testunity_map_source": {
            "view_center": [-0.38, -0.48],
            "view_distance": 24.25,
            "location": camera_location((-0.38, -0.48), 24.25),
            "horizontal_fov": horizontal_fov,
        },
        "runtime_strategy_pawn_default": {
            "view_center": [0.0, 0.0],
            "view_distance": 20.0,
            "location": camera_location((0.0, 0.0), 20.0),
            "horizontal_fov": horizontal_fov,
        },
    }

    mode_results = {}
    for mode, config in camera_modes.items():
        visible_buildings, building_samples = actor_visibility_sample(
            config["location"], forward, config["horizontal_fov"], CAMERA_ASPECT, buildings
        )
        visible_palms, _ = actor_visibility_sample(config["location"], forward, config["horizontal_fov"], CAMERA_ASPECT, palms)
        visible_flags, _ = actor_visibility_sample(config["location"], forward, config["horizontal_fov"], CAMERA_ASPECT, flags)
        visible_units, _ = actor_visibility_sample(config["location"], forward, config["horizontal_fov"], CAMERA_ASPECT, units)
        mode_results[mode] = {
            "view_center": config["view_center"],
            "view_distance": config["view_distance"],
            "horizontal_fov": config["horizontal_fov"],
            "location": [float(config["location"].x), float(config["location"].y), float(config["location"].z)],
            "visible_buildings": len(visible_buildings),
            "visible_palms": len(visible_palms),
            "visible_flags": len(visible_flags),
            "visible_units": len(visible_units),
            "visible_building_labels": visible_buildings,
            "building_samples": building_samples[:32],
        }

    summary = {
        "target_level": TARGET_LEVEL,
        "actors": len(actors),
        "testunity": len(testunity),
        "buildings": len(buildings),
        "palms": len(palms),
        "flags": len(flags),
        "units": len(units),
        "walls": len(walls),
        "ground": len(ground),
        "cameras": [
            {
                "label": actor_label(actor),
                "location": [float(actor.get_actor_location().x), float(actor.get_actor_location().y), float(actor.get_actor_location().z)],
                "rotation": [float(actor.get_actor_rotation().pitch), float(actor.get_actor_rotation().yaw), float(actor.get_actor_rotation().roll)],
                "tags": actor_tags(actor),
            }
            for actor in cameras
        ],
        "modes": mode_results,
    }
    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    with open(OUTPUT, "w", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)
    unreal.log("RokUnityGameAppCityVisibility actors={actors}, buildings={buildings}, runtime_visible_buildings={visible}".format(
        actors=len(actors),
        buildings=len(buildings),
        visible=mode_results["runtime_strategy_pawn_default"]["visible_buildings"],
    ))


if __name__ == "__main__":
    inspect_scene()
