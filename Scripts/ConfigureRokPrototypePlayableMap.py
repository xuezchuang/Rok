import unreal


MAP_PATH = "/Game/Maps/L_RokPrototype_Lit"
GAME_MODE_CLASS = "/Script/Rok.RokGameModeBase"
BUILDING_TAG = "RokBuilding"
CITY_BUILDING_TAG = "CityBuilding"


BUILDING_LABEL_PREFIXES = (
    "RokLayout_",
    "OriginalTownCenter",
    "OriginalCity",
    "AllianceCenter",
    "Barracks",
    "Stable",
    "Archery",
    "Hospital",
    "Farm",
    "Lumber",
    "Quarry",
    "Gold",
    "Storehouse",
    "Tavern",
)

BUILDING_FOLDER_PREFIXES = (
    "RokPrototype/City",
    "RokPrototype/Resources",
    "RokPrototype/ReferenceModels/City",
    "RokPrototype/UnityReferenceScene/Volumetric",
)

NON_BUILDING_LABEL_TOKENS = (
    "Road",
    "Terrain",
    "Label_",
    "Camera_",
    "Sky",
    "Fog",
    "Light",
    "Sun",
    "Mountain",
    "WorldTree",
    "Bridge",
    "MarchUnit",
)


def actor_label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def is_city_building(actor):
    label = actor_label(actor)
    if any(token in label for token in NON_BUILDING_LABEL_TOKENS):
        return False
    if label.startswith(BUILDING_LABEL_PREFIXES):
        return True

    folder = str(actor.get_folder_path())
    return folder.startswith(BUILDING_FOLDER_PREFIXES)


def add_tags(actor, tags):
    current_tags = list(actor.tags)
    changed = False
    for tag in tags:
        name = unreal.Name(tag)
        if name not in current_tags:
            current_tags.append(name)
            changed = True
    if changed:
        actor.set_editor_property("tags", current_tags)
    return changed


def configure_visibility_trace(actor):
    changed = 0
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        if not component:
            continue
        component.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
        component.set_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY, unreal.CollisionResponseType.ECR_BLOCK)
        component.set_editor_property("render_custom_depth", False)
        changed += 1
    return changed


def configure_world_settings():
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_settings = world.get_world_settings()
    game_mode_class = unreal.load_class(None, GAME_MODE_CLASS)
    if not game_mode_class:
        raise RuntimeError("Could not load Rok game mode class: {0}".format(GAME_MODE_CLASS))
    world_settings.set_editor_property("default_game_mode", game_mode_class)
    return world.get_name()


def main():
    project_dir = unreal.Paths.project_dir()
    unreal.log("Rok map configure: project_dir={0}".format(project_dir))
    if not project_dir.replace("\\", "/").rstrip("/").endswith("/Rok"):
        raise RuntimeError("Refusing to configure non-Rok project: {0}".format(project_dir))

    if not unreal.EditorLevelLibrary.load_level(MAP_PATH):
        raise RuntimeError("Failed to load map: {0}".format(MAP_PATH))

    world_name = configure_world_settings()
    tagged_actors = 0
    configured_components = 0
    actor_samples = []
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if not is_city_building(actor):
            continue
        if add_tags(actor, (BUILDING_TAG, CITY_BUILDING_TAG)):
            tagged_actors += 1
        configured_components += configure_visibility_trace(actor)
        if len(actor_samples) < 12:
            actor_samples.append(actor_label(actor))

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log(
        "Rok map configure complete: world={0}, tagged_actors={1}, configured_static_mesh_components={2}, samples={3}".format(
            world_name,
            tagged_actors,
            configured_components,
            ", ".join(actor_samples),
        )
    )


if __name__ == "__main__":
    main()
