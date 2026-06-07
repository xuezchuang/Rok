import os
import sys
import json

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

TARGET_LEVEL = os.environ.get("ROK_UNITY_GAMEAPP_CITY_LEVEL", "/Game/Maps/L_RokUnityGameAppCity")
VERIFY_OUTPUT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_gameapp_city_verify.json")


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


def has_component(actor, component_class):
    try:
        return actor.get_component_by_class(component_class) is not None
    except Exception:
        return False


def actor_tags(actor):
    return [str(tag) for tag in list(actor.tags)]


def verify_scene():
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Missing map: {0}".format(TARGET_LEVEL))

    unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    testunity_tagged = [actor for actor in actors if "RokTestUnityLayout" in actor_tags(actor)]
    if testunity_tagged:
        buildings = [actor for actor in testunity_tagged if "UnityDenseBuilding" in [str(tag) for tag in list(actor.tags)]]
        palms = [actor for actor in testunity_tagged if "UnityPalm" in [str(tag) for tag in list(actor.tags)]]
        walls = [actor for actor in testunity_tagged if "UnityWallSegment" in [str(tag) for tag in list(actor.tags)]]
        source_boundary_models = [
            actor for actor in walls
            if "RokUnityCityBoundaryPrefabSource" in [str(tag) for tag in list(actor.tags)]
        ]
        scene_boundary_models = [
            actor for actor in walls
            if "RokUnityTestUnitySceneBoundary" in [str(tag) for tag in list(actor.tags)]
        ]
        legacy_fbx_walls = [
            actor for actor in walls
            if "UnityWallFullSourceModel" in [str(tag) for tag in list(actor.tags)]
        ]
        gray_obj_walls = [
            actor for actor in walls
            if "UnityMesh_CityWall_1_1_Obj" in [str(tag) for tag in list(actor.tags)]
        ]
        fallback_walls = [
            actor for actor in walls
            if "FallbackCubeWall" in [str(tag) for tag in list(actor.tags)]
        ]
        units = [actor for actor in testunity_tagged if "UnityUnit" in [str(tag) for tag in list(actor.tags)]]
        flags = [actor for actor in testunity_tagged if "UnityFlag" in [str(tag) for tag in list(actor.tags)]]
        cameras = [
            actor for actor in testunity_tagged
            if actor.get_class().get_name() == "CameraActor"
            and actor_label(actor) == "Camera_RokUnityGameAppCity"
        ]
        player_starts = [
            actor for actor in testunity_tagged
            if actor.get_class().get_name() == "PlayerStart"
            and actor_label(actor) == "PlayerStart_RokUnityGameAppCity"
        ]
        terrain = [
            actor for actor in testunity_tagged
            if actor_folder(actor).startswith("RokUnityGameAppCity/TestUnity/Ground")
        ]

        failures = []
        if len(buildings) < 16:
            failures.append("expected at least 16 TestUnity dense buildings, found {0}".format(len(buildings)))
        if len(palms) < 80:
            failures.append("expected at least 80 TestUnity palm mesh parts, found {0}".format(len(palms)))
        if len(walls) < 1:
            failures.append("expected at least 1 TestUnity wall mesh, found {0}".format(len(walls)))
        if len(scene_boundary_models) == 0 and len(source_boundary_models) < 3:
            failures.append("expected Rok-unity prefab-driven wall/gate/tower source meshes, found {0}".format(len(source_boundary_models)))
        if len(scene_boundary_models) > 1:
            failures.append("expected a single TestUnity scene-driven city boundary mesh, found {0}".format(len(scene_boundary_models)))
        if legacy_fbx_walls:
            failures.append("legacy single CityWall_101_1 full-wall actor is not allowed for current Rok-unity wall preview")
        if gray_obj_walls:
            failures.append("gray BundleAssetsTest CityWall_1_1 OBJ is diagnostic-only and not allowed for current Rok-unity wall preview")
        if fallback_walls:
            failures.append("fallback cube walls are not allowed in TestUnity city wall preview")
        if len(units) < 4:
            failures.append("expected at least 4 TestUnity unit markers, found {0}".format(len(units)))
        if len(flags) < 8:
            failures.append("expected at least 8 TestUnity flags, found {0}".format(len(flags)))
        if len(terrain) < 2:
            failures.append("expected TestUnity outer and city ground, found {0}".format(len(terrain)))
        if len(cameras) != 1:
            failures.append("expected exactly 1 Camera_RokUnityGameAppCity, found {0}".format(len(cameras)))
        elif not has_component(cameras[0], unreal.CameraComponent):
            failures.append("Camera_RokUnityGameAppCity is missing a CameraComponent")
        if len(player_starts) != 1:
            failures.append("expected exactly 1 PlayerStart_RokUnityGameAppCity, found {0}".format(len(player_starts)))

        summary = (
            "UnityGameAppCityVerify(TestUnity) actors={0}, tagged={1}, buildings={2}, palms={3}, "
            "walls={4}, units={5}, flags={6}, terrain={7}, cameras={8}, player_starts={9}"
        ).format(
            len(actors),
            len(testunity_tagged),
            len(buildings),
            len(palms),
            len(walls),
            len(units),
            len(flags),
            len(terrain),
            len(cameras),
            len(player_starts),
        )
        print(summary)
        unreal.log(summary)
        os.makedirs(os.path.dirname(VERIFY_OUTPUT), exist_ok=True)
        with open(VERIFY_OUTPUT, "w", encoding="utf-8") as handle:
            json.dump({
                "target_level": TARGET_LEVEL,
                "mode": "TestUnity",
                "actors": len(actors),
                "tagged": len(testunity_tagged),
                "buildings": len(buildings),
                "palms": len(palms),
                "walls": len(walls),
                "units": len(units),
                "flags": len(flags),
                "terrain": len(terrain),
                "cameras": len(cameras),
                "player_starts": len(player_starts),
                "failures": failures,
            }, handle, ensure_ascii=False, indent=2)

        if failures:
            for failure in failures:
                unreal.log_error(failure)
            raise RuntimeError("Unity GameApp city TestUnity verification failed")
        return

    runtime_tagged = [actor for actor in actors if "RokUnityRuntimeCityAllBuildings" in actor_tags(actor)]
    if runtime_tagged:
        boundary = [actor for actor in runtime_tagged if "RokUnityRuntimeBoundarySource" in actor_tags(actor)]
        ground = [actor for actor in runtime_tagged if "RokUnityRuntimeGroundSource" in actor_tags(actor)]
        buildings = [actor for actor in runtime_tagged if "RokUnityRuntimeBuildingSource" in actor_tags(actor)]
        config_init_buildings = [
            actor for actor in buildings
            if "RokUnityConfigInitBuilding" in actor_tags(actor)
        ]
        cameras = [
            actor for actor in runtime_tagged
            if actor.get_class().get_name() == "CameraActor"
            and "RokUnityRuntimeCameraSource" in actor_tags(actor)
        ]
        player_starts = [
            actor for actor in runtime_tagged
            if actor.get_class().get_name() == "PlayerStart"
            and "RokUnityRuntimePlayerStart" in actor_tags(actor)
        ]

        failures = []
        if len(boundary) != 10:
            failures.append("expected exactly 10 runtime city boundary actors, found {0}".format(len(boundary)))
        if len(ground) != 2:
            failures.append("expected exactly 2 runtime city ground actors, found {0}".format(len(ground)))
        if len(buildings) < 24:
            failures.append("expected at least 24 runtime city building sprites, found {0}".format(len(buildings)))
        if len(config_init_buildings) != 24:
            failures.append("expected exactly 24 config-derived InitBuilding sprites, found {0}".format(len(config_init_buildings)))
        if len(cameras) != 1:
            failures.append("expected exactly 1 runtime city camera, found {0}".format(len(cameras)))
        elif not has_component(cameras[0], unreal.CameraComponent):
            failures.append("runtime city camera is missing a CameraComponent")
        if len(player_starts) != 1:
            failures.append("expected exactly 1 runtime city PlayerStart, found {0}".format(len(player_starts)))

        missing_visuals = [
            actor_label(actor)
            for actor in buildings + boundary + ground
            if not has_component(actor, unreal.StaticMeshComponent)
        ]
        if missing_visuals:
            failures.append("runtime city actors missing StaticMesh visual: {0}".format(", ".join(missing_visuals[:8])))

        summary = (
            "UnityGameAppCityVerify(RuntimeAllBuildings) actors={0}, tagged={1}, boundary={2}, "
            "ground={3}, buildings={4}, cameras={5}, player_starts={6}, config_init_buildings={7}"
        ).format(
            len(actors),
            len(runtime_tagged),
            len(boundary),
            len(ground),
            len(buildings),
            len(cameras),
            len(player_starts),
            len(config_init_buildings),
        )
        print(summary)
        unreal.log(summary)
        os.makedirs(os.path.dirname(VERIFY_OUTPUT), exist_ok=True)
        with open(VERIFY_OUTPUT, "w", encoding="utf-8") as handle:
            json.dump({
                "target_level": TARGET_LEVEL,
                "mode": "RuntimeAllBuildings",
                "actors": len(actors),
                "tagged": len(runtime_tagged),
                "boundary": len(boundary),
                "ground": len(ground),
                "buildings": len(buildings),
                "config_init_buildings": len(config_init_buildings),
                "cameras": len(cameras),
                "player_starts": len(player_starts),
                "failures": failures,
            }, handle, ensure_ascii=False, indent=2)

        if failures:
            for failure in failures:
                unreal.log_error(failure)
            raise RuntimeError("Unity GameApp city runtime-all-buildings verification failed")
        return

    tagged = [actor for actor in actors if "RokUnityGameAppCity" in actor_tags(actor)]
    buildings = [
        actor for actor in actors
        if actor.get_class().get_name() == "RokCitySpriteActor"
        and "CityBuilding" in [str(tag) for tag in list(actor.tags)]
    ]
    config_init_buildings = [
        actor for actor in buildings
        if "RokUnityConfigInitBuilding" in [str(tag) for tag in list(actor.tags)]
    ]
    trees = [actor for actor in tagged if "RokUnityGameAppTree" in [str(tag) for tag in list(actor.tags)]]
    walls = [actor for actor in tagged if "RokUnityGameAppWall" in [str(tag) for tag in list(actor.tags)]]
    cameras = [
        actor for actor in tagged
        if actor.get_class().get_name() == "CameraActor"
        and actor_label(actor) == "Camera_RokUnityGameAppCity"
    ]
    player_starts = [
        actor for actor in tagged
        if actor.get_class().get_name() == "PlayerStart"
        and actor_label(actor) == "PlayerStart_RokUnityGameAppCity"
    ]
    terrain = [actor for actor in actors if actor_folder(actor).startswith("RokUnityGameAppCity/Terrain")]
    roads = [actor for actor in actors if actor_folder(actor).startswith("RokUnityGameAppCity/Roads")]

    failures = []
    if len(buildings) < 20:
        failures.append("expected at least 20 Unity GameApp building sprites, found {0}".format(len(buildings)))
    if len(config_init_buildings) < 20:
        failures.append("expected at least 20 config-derived InitBuilding sprites, found {0}".format(len(config_init_buildings)))
    if len(trees) < 8:
        failures.append("expected at least 8 Unity GameApp tree meshes, found {0}".format(len(trees)))
    if len(walls) < 3:
        failures.append("expected at least 3 Unity GameApp wall meshes, found {0}".format(len(walls)))
    if len(terrain) < 1:
        failures.append("missing Unity GameApp ground terrain")
    if len(cameras) != 1:
        failures.append("expected exactly 1 Camera_RokUnityGameAppCity, found {0}".format(len(cameras)))
    elif not has_component(cameras[0], unreal.CameraComponent):
        failures.append("Camera_RokUnityGameAppCity is missing a CameraComponent")
    if len(player_starts) != 1:
        failures.append("expected exactly 1 PlayerStart_RokUnityGameAppCity, found {0}".format(len(player_starts)))

    missing_visuals = [
        actor_label(actor)
        for actor in buildings
        if not has_component(actor, unreal.StaticMeshComponent)
    ]
    if missing_visuals:
        failures.append("building actors missing StaticMesh visual: {0}".format(", ".join(missing_visuals[:8])))

    summary = (
        "UnityGameAppCityVerify actors={0}, tagged={1}, buildings={2}, trees={3}, "
        "walls={4}, terrain={5}, roads={6}, cameras={7}, player_starts={8}, config_init_buildings={9}"
    ).format(
        len(actors),
        len(tagged),
        len(buildings),
        len(trees),
        len(walls),
        len(terrain),
        len(roads),
        len(cameras),
        len(player_starts),
        len(config_init_buildings),
    )
    print(summary)
    unreal.log(summary)
    os.makedirs(os.path.dirname(VERIFY_OUTPUT), exist_ok=True)
    with open(VERIFY_OUTPUT, "w", encoding="utf-8") as handle:
        json.dump({
            "target_level": TARGET_LEVEL,
            "actors": len(actors),
            "tagged": len(tagged),
            "buildings": len(buildings),
            "config_init_buildings": len(config_init_buildings),
            "trees": len(trees),
            "walls": len(walls),
            "terrain": len(terrain),
            "roads": len(roads),
            "cameras": len(cameras),
            "player_starts": len(player_starts),
            "failures": failures,
        }, handle, ensure_ascii=False, indent=2)

    if failures:
        for failure in failures:
            unreal.log_error(failure)
        raise RuntimeError("Unity GameApp city verification failed")


if __name__ == "__main__":
    verify_scene()
