import os

import unreal


LEVEL_PATH = os.environ.get("ROK_VERIFY_LEVEL", "/Game/Maps/L_RokPrototype_Lit")
LEVEL_ASSET_NAME = LEVEL_PATH.rsplit("/", 1)[-1]
UNITY_RECONSTRUCTION_MODE = os.environ.get("ROK_UNITY_RECONSTRUCTION_MODE", "1") == "1"
REQUIRED_ASSETS = [
    "{0}.{1}".format(LEVEL_PATH, LEVEL_ASSET_NAME),
    "/Game/RokPrototype/Materials/M_Rok_Grass_Blockout.M_Rok_Grass_Blockout",
    "/Game/RokPrototype/Materials/M_Rok_Stone_Blockout.M_Rok_Stone_Blockout",
    "/Game/RokPrototype/Materials/M_Rok_Terracotta_Blockout.M_Rok_Terracotta_Blockout",
    "/Game/RokPrototype/Meshes/City/SM_Rok_CityWall.SM_Rok_CityWall",
    "/Game/RokPrototype/Meshes/City/SM_Rok_CityGate.SM_Rok_CityGate",
    "/Game/RokPrototype/Meshes/City/SM_Rok_CityTower.SM_Rok_CityTower",
    "/Game/RokPrototype/Meshes/City/SM_Rok_Checkpoint.SM_Rok_Checkpoint",
    "/Game/RokPrototype/Meshes/City/SM_Rok_AllianceFlag.SM_Rok_AllianceFlag",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_TreeA.SM_Rok_TreeA",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_TreeB.SM_Rok_TreeB",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_MountainA.SM_Rok_MountainA",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_MountainB.SM_Rok_MountainB",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_Bridge.SM_Rok_Bridge",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_Ground.SM_Rok_Ground",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_Plane180.SM_Rok_Plane180",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_Plane60.SM_Rok_Plane60",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_Plane30.SM_Rok_Plane30",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_RiverA.SM_Rok_RiverA",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_RiverB.SM_Rok_RiverB",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_StoneResource.SM_Rok_StoneResource",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_MarchUnit.SM_Rok_MarchUnit",
    "/Game/RokPrototype/Meshes/Environment/SM_Rok_MarchFlag.SM_Rok_MarchFlag",
]
REQUIRED_ACTORS = [
    "Ground_StrategyMap_Clean",
    "WorldBackdrop_FarGround",
    "City_Plaza_Round",
    "OriginalCityWall_101_1",
    "OriginalCityGate_101_1",
    "OriginalCityTower_101_1_01",
    "OriginalTownCenter_CheckpointBig",
    "OriginalWoodGrove_01",
    "OriginalFarmField_01",
    "OriginalStoneNode_01",
    "OriginalGoldNode_01",
    "KeySun_RokPrototype",
    "SkyLight_RokPrototype",
    "SkyAtmosphere_RokPrototype",
    "SoftMapFog_RokPrototype",
    "PostProcess_RTS_Exposure",
]


def main():
    missing = [asset for asset in REQUIRED_ASSETS if not unreal.EditorAssetLibrary.does_asset_exist(asset)]
    if missing:
        raise RuntimeError("Missing prototype assets: {0}".format(", ".join(missing)))

    unreal.EditorLevelLibrary.load_level(LEVEL_PATH)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    static_mesh_actors = [
        actor for actor in actors
        if actor.get_component_by_class(unreal.StaticMeshComponent) is not None
    ]
    labels = sorted(actor.get_actor_label() for actor in actors)
    if UNITY_RECONSTRUCTION_MODE:
        required_visual_actors = [
            "Ground_StrategyMap_Clean",
            "WorldBackdrop_FarGround",
            "Camera_Rok2D_Prototype",
            "KeySun_RokPrototype",
            "SkyLight_RokPrototype",
        ]
        missing_visual = [label for label in required_visual_actors if label not in labels]
        if missing_visual:
            raise RuntimeError("Missing Unity visual reconstruction actors: {0}".format(", ".join(missing_visual)))
        clean_build_count = len([label for label in labels if label.startswith("RokCityBuild_")])
        bad_legacy_labels = [
            label for label in labels
            if label.startswith("UnityMapTTS_")
            or "GuardTowerUI" in label
            or label.startswith("World_")
            or label.startswith("zhedang")
            or label == "City_Main"
        ]
        if clean_build_count < 12:
            raise RuntimeError("Clean city build reconstruction has too few sprite actors: {0}".format(clean_build_count))
        if bad_legacy_labels:
            raise RuntimeError("Clean city build reconstruction still contains legacy/wrong sprite actors: {0}".format(
                ", ".join(bad_legacy_labels[:20])
            ))
        unreal.log("Rok prototype verify passed: {0} actors, {1} static mesh actors, {2} clean city build sprites".format(
            len(actors), len(static_mesh_actors), clean_build_count
        ))
        return

    unity_sprite_labels = [label for label in labels if label.startswith("UnitySprite_")]
    layout_building_labels = [label for label in labels if label.startswith("RokLayout_") and not label.endswith("_Roof")]
    missing_actors = [label for label in REQUIRED_ACTORS if label not in labels]
    if missing_actors:
        raise RuntimeError("Missing prototype actors: {0}".format(", ".join(missing_actors)))

    bad_placeholder_labels = [
        label for label in labels
        if "Spire" in label or label == "TownCenter_BannerRed"
    ]
    bad_tree_placeholders = [
        label for label in labels
        if label.startswith("WorldTree_") or label.endswith("_Canopy")
    ]
    bad_city_fallback_labels = [
        label for label in labels
        if label.startswith("TownCenter_")
        or label in ["Barracks", "Stable", "ArcheryRange", "Hospital", "Academy", "Tavern", "Storehouse", "AllianceCenter"]
    ]
    if bad_placeholder_labels:
        raise RuntimeError("Prototype still contains fake spire/banner placeholders: {0}".format(
            ", ".join(bad_placeholder_labels[:20])
        ))
    if bad_tree_placeholders:
        raise RuntimeError("Prototype still contains fake cone/tree placeholders: {0}".format(
            ", ".join(bad_tree_placeholders[:20])
        ))
    if bad_city_fallback_labels:
        raise RuntimeError("Prototype still contains city building fallback placeholders: {0}".format(
            ", ".join(bad_city_fallback_labels[:20])
        ))

    floating_fragments = []
    cone_placeholders = []
    tree_cone_placeholders = []
    city_blockout_meshes = []
    basic_shape_meshes = []
    for actor in static_mesh_actors:
        label = actor.get_actor_label()
        if label.startswith("Camera_"):
            continue
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        mesh = component.get_editor_property("static_mesh") if component else None
        folder = str(actor.get_folder_path())
        mesh_path = mesh.get_path_name() if mesh else ""
        if mesh_path.startswith("/Engine/BasicShapes/"):
            basic_shape_meshes.append("{0} -> {1}".format(label, mesh_path))
        if mesh and mesh.get_path_name() == "/Engine/BasicShapes/Cone.Cone":
            if folder.startswith("RokPrototype/City") or folder.startswith("RokPrototype/UnityReferenceScene"):
                cone_placeholders.append(label)
            if folder.startswith("RokPrototype/Terrain/Trees") or folder.startswith("RokPrototype/Resources/Wood"):
                tree_cone_placeholders.append(label)
        if mesh_path.startswith("/Engine/BasicShapes/") and (
            folder.startswith("RokPrototype/City/Buildings")
            or folder.startswith("RokPrototype/UnityReferenceScene")
        ):
            city_blockout_meshes.append(label)
        is_reference_mesh = mesh_path.startswith("/Game/RokPrototype/Meshes/")
        if "_Ref" in label:
            floating_fragments.append(label)
            continue
        location = actor.get_actor_location()
        if not is_reference_mesh and (abs(location.x) > 4000.0 or abs(location.y) > 4000.0 or location.z > 900.0):
            floating_fragments.append(label)

    if floating_fragments:
        raise RuntimeError("Prototype still contains suspect scattered actors: {0}".format(
            ", ".join(floating_fragments[:20])
        ))
    if cone_placeholders:
        raise RuntimeError("Prototype still contains cone placeholders in city layers: {0}".format(
            ", ".join(cone_placeholders[:20])
        ))
    if tree_cone_placeholders:
        raise RuntimeError("Prototype still contains cone placeholders in tree layers: {0}".format(
            ", ".join(tree_cone_placeholders[:20])
        ))
    if city_blockout_meshes:
        raise RuntimeError("Prototype still contains BasicShapes city/layout blockouts: {0}".format(
            ", ".join(city_blockout_meshes[:20])
        ))
    if basic_shape_meshes:
        raise RuntimeError("Prototype still contains BasicShapes static mesh actors: {0}".format(
            ", ".join(basic_shape_meshes[:20])
        ))

    if len(static_mesh_actors) < 55:
        raise RuntimeError(
            "Prototype scene has too few static mesh actors for the RoK blockout: {0}".format(
                len(static_mesh_actors)
            )
        )
    if unity_sprite_labels:
        raise RuntimeError(
            "Prototype scene still contains direct plane sprite actors: {0}".format(
                ", ".join(unity_sprite_labels[:12])
            )
        )
    if layout_building_labels:
        raise RuntimeError(
            "Prototype scene still contains Unity layout BasicShapes buildings: {0}".format(
                ", ".join(layout_building_labels[:20])
            )
        )

    unreal.log("Rok prototype verify passed: {0} actors, {1} static mesh actors".format(
        len(actors), len(static_mesh_actors)
    ))
    unreal.log("Rok prototype Unity layout volumetric buildings removed: {0}".format(len(layout_building_labels)))
    unreal.log("Rok prototype key actors: {0}".format(", ".join(REQUIRED_ACTORS)))


if __name__ == "__main__":
    main()
