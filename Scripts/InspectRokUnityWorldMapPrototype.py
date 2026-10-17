import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
TARGET_LEVEL = "/Game/Maps/L_RokUnityWorldMapPrototype"
OUTPUT_PATH = (
    PROJECT_ROOT
    / "Saved"
    / "RokUnityMigration"
    / "unity_world_map_actor_bounds.json"
)


def vector_record(vector):
    return {
        "x": float(vector.x),
        "y": float(vector.y),
        "z": float(vector.z),
    }


def safe_property(obj, property_name, default=None):
    try:
        return obj.get_editor_property(property_name)
    except Exception:
        return default


def safe_bool_call(obj, method_name, default=False):
    try:
        return bool(getattr(obj, method_name)())
    except Exception:
        return bool(default)


def material_record(material):
    if not material:
        return {}
    record = {
        "path": material.get_path_name(),
    }
    for property_name in (
        "blend_mode",
        "shading_model",
        "two_sided",
        "opacity_mask_clip_value",
    ):
        value = safe_property(material, property_name)
        if value is not None:
            record[property_name] = str(value)
    return record


def component_record(component):
    if not component:
        return {}
    flags = {}
    for property_name in (
        "visible",
        "hidden_in_game",
        "visible_in_scene_capture_only",
        "hidden_in_scene_capture",
        "render_in_main_pass",
        "render_in_depth_pass",
        "owner_no_see",
        "only_owner_see",
        "cast_hidden_shadow",
    ):
        value = safe_property(component, property_name)
        if value is not None:
            flags[property_name] = bool(value)
    return {
        "registered": safe_bool_call(component, "is_registered"),
        "active": safe_bool_call(component, "is_active"),
        "flags": flags,
        "translucency_sort_priority": int(
            safe_property(component, "translucency_sort_priority", 0)
        ),
    }


def inspect_world_map():
    unreal.EditorLoadingAndSavingUtils.load_map(TARGET_LEVEL)
    actors = []
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        tags = [str(tag) for tag in actor.tags]
        if not any(
            tag in tags
            for tag in (
                "RokWorldMapTerrain",
                "RokWorldMapSourceObject",
                "RokWorldMapPlayerCity",
                "RokWorldMapConfig",
            )
        ):
            continue
        bounds_origin, bounds_extent = actor.get_actor_bounds(False)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        mesh = (
            component.get_editor_property("static_mesh")
            if component
            else None
        )
        actors.append(
            {
                "label": actor.get_actor_label(),
                "tags": tags,
                "location": vector_record(actor.get_actor_location()),
                "rotation": {
                    "pitch": float(actor.get_actor_rotation().pitch),
                    "yaw": float(actor.get_actor_rotation().yaw),
                    "roll": float(actor.get_actor_rotation().roll),
                },
                "scale": vector_record(actor.get_actor_scale3d()),
                "bounds_origin": vector_record(bounds_origin),
                "bounds_extent": vector_record(bounds_extent),
                "mesh": mesh.get_path_name() if mesh else "",
                "materials": [
                    material_record(component.get_material(index))
                    for index in range(component.get_num_materials())
                ]
                if component
                else [],
                "actor_flags": {
                    "hidden": bool(safe_property(actor, "hidden", False)),
                    "is_editor_only_actor": bool(
                        safe_property(actor, "is_editor_only_actor", False)
                    ),
                    "hidden_in_game": safe_bool_call(actor, "is_hidden"),
                    "hidden_in_editor": safe_bool_call(
                        actor,
                        "is_hidden_ed",
                    ),
                },
                "component": component_record(component),
            }
        )
    actors.sort(
        key=lambda record: (
            record["bounds_origin"]["x"] ** 2
            + record["bounds_origin"]["y"] ** 2
            + record["bounds_origin"]["z"] ** 2
        )
    )
    payload = {
        "schema": "rok.unity_world_map_actor_bounds.v1",
        "target_level": TARGET_LEVEL,
        "actor_count": len(actors),
        "actors": actors,
    }
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(
        json.dumps(payload, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    unreal.log(
        f"Rok world-map actor bounds exported: {len(actors)} actors"
    )


if __name__ == "__main__":
    inspect_world_map()
