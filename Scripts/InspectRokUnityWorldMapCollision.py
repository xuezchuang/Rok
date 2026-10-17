import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
TARGET_LEVEL = "/Game/Maps/L_RokUnityWorldMapPrototype"
OUTPUT_PATH = (
    PROJECT_ROOT
    / "Saved"
    / "RokUnityMigration"
    / "unity_world_map_collision_inspect.json"
)


def enum_text(value):
    return str(value)


def inspect_component(component):
    record = {
        "class": component.get_class().get_path_name(),
        "collision_enabled": enum_text(component.get_collision_enabled()),
        "collision_profile_name": str(
            component.get_collision_profile_name()
        ),
        "visibility_response": enum_text(
            component.get_collision_response_to_channel(
                unreal.CollisionChannel.ECC_VISIBILITY
            )
        ),
        "camera_response": enum_text(
            component.get_collision_response_to_channel(
                unreal.CollisionChannel.ECC_CAMERA
            )
        ),
        "visible": bool(component.is_visible()),
        "hidden_in_game": bool(
            component.get_editor_property("hidden_in_game")
        ),
        "generate_overlap_events": bool(
            component.get_editor_property("generate_overlap_events")
        ),
    }
    try:
        body_instance = component.get_editor_property("body_instance")
        record["body_instance_collision_enabled"] = enum_text(
            body_instance.get_editor_property("collision_enabled")
        )
        record["body_instance_profile_name"] = str(
            body_instance.get_editor_property("collision_profile_name")
        )
    except Exception as error:
        record["body_instance_read_error"] = str(error)
    return record


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(TARGET_LEVEL)
    world = unreal.EditorLevelLibrary.get_editor_world()
    if str(world.get_outermost().get_name()) != TARGET_LEVEL:
        raise RuntimeError(
            "Loaded "
            f"{world.get_outermost().get_name()}, expected {TARGET_LEVEL}"
        )

    categories = {
        "terrain": [],
        "source_object": [],
        "player_city": [],
    }
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        tags = {str(tag) for tag in actor.tags}
        category = None
        if "RokWorldMapTerrain" in tags:
            category = "terrain"
        elif "RokWorldMapSourceObject" in tags:
            category = "source_object"
        elif "RokWorldMapSelectable" in tags:
            category = "player_city"
        if category is None:
            continue
        for component in actor.get_components_by_class(
            unreal.PrimitiveComponent
        ):
            categories[category].append(
                {
                    "actor": actor.get_actor_label(),
                    "component": component.get_name(),
                    "component_tags": [
                        str(tag)
                        for tag in component.get_editor_property(
                            "component_tags"
                        )
                    ],
                    **inspect_component(component),
                }
            )

    summary = {}
    for category, records in categories.items():
        signatures = {}
        for record in records:
            key = "|".join(
                (
                    record["collision_enabled"],
                    record["collision_profile_name"],
                    record["visibility_response"],
                    record["camera_response"],
                    str(record.get("body_instance_collision_enabled", "")),
                    str(record.get("body_instance_profile_name", "")),
                )
            )
            signatures[key] = signatures.get(key, 0) + 1
        summary[category] = {
            "count": len(records),
            "signatures": signatures,
            "samples": records[:3],
        }

    payload = {
        "schema": "rok.unity_world_map_collision_inspect.v1",
        "target_level": TARGET_LEVEL,
        "summary": summary,
    }
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(
        json.dumps(payload, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    print(json.dumps(summary, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
