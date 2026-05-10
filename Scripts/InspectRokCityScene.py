import unreal


LEVEL_PATH = "/Game/Maps/L_RokPrototype_Lit"
EXPECTED_LABELS = [
    "OriginalTownCenter_CheckpointBig",
    "OriginalBarracks_01",
    "OriginalStable_01",
    "OriginalArcheryRange_01",
    "OriginalHospital_01",
    "OriginalAcademy_01",
    "OriginalTavern_01",
    "OriginalStorehouse_01",
    "OriginalQuarryYard_01",
    "OriginalGoldMine_01",
    "OriginalLumberMill_01",
    "OriginalFarmHouse_01",
    "OriginalScoutCamp_01",
    "OriginalMarket_01",
]


def actor_mesh_and_material(actor):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        return "", ""
    mesh = component.get_editor_property("static_mesh")
    material = component.get_material(0)
    mesh_path = mesh.get_path_name() if mesh else ""
    material_path = material.get_path_name() if material else ""
    return mesh_path, material_path


def main():
    unreal.EditorLevelLibrary.load_level(LEVEL_PATH)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    labels = {actor.get_actor_label(): actor for actor in actors}
    city_sprite_count = 0
    building_tag_count = 0
    for actor in actors:
        mesh_path, material_path = actor_mesh_and_material(actor)
        if "/Game/RokPrototype/Materials/CitySprites/" in material_path:
            city_sprite_count += 1
        if "RokBuilding" in [str(tag) for tag in actor.tags]:
            building_tag_count += 1

    missing = [label for label in EXPECTED_LABELS if label not in labels]
    unreal.log("Rok city inspect: actor_count={0}".format(len(actors)))
    unreal.log("Rok city inspect: city_sprite_actor_count={0}".format(city_sprite_count))
    unreal.log("Rok city inspect: building_tag_count={0}".format(building_tag_count))
    unreal.log("Rok city inspect: missing_expected={0}".format(",".join(missing) if missing else "none"))
    for label in EXPECTED_LABELS:
        actor = labels.get(label)
        if not actor:
            continue
        mesh_path, material_path = actor_mesh_and_material(actor)
        loc = actor.get_actor_location()
        scale = actor.get_actor_scale3d()
        unreal.log(
            "Rok city inspect: {0} loc=({1:.1f},{2:.1f},{3:.1f}) scale=({4:.2f},{5:.2f},{6:.2f}) mesh={7} material={8}".format(
                label,
                loc.x,
                loc.y,
                loc.z,
                scale.x,
                scale.y,
                scale.z,
                mesh_path,
                material_path,
            )
        )


if __name__ == "__main__":
    main()
