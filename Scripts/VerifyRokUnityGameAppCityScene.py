import os
import sys
import json

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)

import VerifyRokUnityRuntimeGroundScene as ground_verify

TARGET_LEVEL = os.environ.get("ROK_UNITY_GAMEAPP_CITY_LEVEL", "/Game/Maps/L_RokUnityGameAppCity")
VERIFY_OUTPUT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_gameapp_city_verify.json")
RUNTIME_SCENE_MANIFEST = os.environ.get(
    "ROK_UNITY_RUNTIME_CITY_ALL_BUILDINGS_MANIFEST",
    os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_gameapp_city_runtime_scene_manifest.json"),
)


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


def package_path(object_path):
    return str(object_path).split(".", 1)[0].rstrip("/")


def verify_scene():
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Missing map: {0}".format(TARGET_LEVEL))

    current_world = unreal.EditorLevelLibrary.get_editor_world()
    current_level = package_path(current_world.get_path_name()) if current_world else ""
    if current_level != package_path(TARGET_LEVEL):
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
        support = [actor for actor in runtime_tagged if "RokUnityRuntimeCitySupportLayer" in actor_tags(actor)]
        gate_flags = [actor for actor in runtime_tagged if "RokUnityRuntimeCityGateFlag" in actor_tags(actor)]
        roads = [actor for actor in support if "UnitySupportRole_road" in actor_tags(actor)]
        authored_shadows = [
            actor for actor in support
            if "UnitySupportRole_authored_shadow" in actor_tags(actor)
        ]
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
        if not os.path.exists(RUNTIME_SCENE_MANIFEST):
            runtime_manifest = {}
            failures.append("missing runtime city scene manifest: {0}".format(RUNTIME_SCENE_MANIFEST))
        else:
            with open(RUNTIME_SCENE_MANIFEST, "r", encoding="utf-8-sig") as handle:
                runtime_manifest = json.load(handle)
        ground_source_manifest = ground_verify.load_ground_source_manifest()
        if os.path.normcase(os.path.abspath(runtime_manifest.get("unity_runtime_ground_obj_manifest", ""))) != os.path.normcase(os.path.abspath(ground_verify.GROUND_SOURCE_MANIFEST)):
            failures.append("runtime city scene manifest points at the wrong Unity ground source manifest")
        ground_contract = ground_verify.verify_ground_source_contract(
            ground_source_manifest,
            runtime_manifest.get("ground_actors", []),
            ground,
            failures,
        )
        if len(boundary) != 10:
            failures.append("expected exactly 10 runtime city boundary actors, found {0}".format(len(boundary)))
        if len(ground) != 2:
            failures.append("expected exactly 2 runtime city ground actors, found {0}".format(len(ground)))
        if len(support) != 2:
            failures.append("expected exactly 2 runtime city source support actors, found {0}".format(len(support)))
        if len(gate_flags) != 2:
            failures.append("expected exactly 2 Unity source CityGate flag SpriteRenderers, found {0}".format(len(gate_flags)))
        if len(roads) != 1:
            failures.append("expected exactly 1 Unity-authored runtime city road, found {0}".format(len(roads)))
        if len(authored_shadows) != 1:
            failures.append("expected exactly 1 Unity-authored runtime city wall shadow, found {0}".format(len(authored_shadows)))
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
        for actor in boundary:
            tags = actor_tags(actor)
            location = actor.get_actor_location()
            rotation = actor.get_actor_rotation()
            scale = actor.get_actor_scale3d()
            if any(abs(float(value)) > 0.05 for value in (
                location.x,
                location.y,
                location.z,
                rotation.pitch,
                rotation.yaw,
                rotation.roll,
            )):
                failures.append("runtime boundary world-baked OBJ must keep identity location/rotation: {0}".format(actor_label(actor)))
            if any(abs(float(value) - 1.0) > 0.001 for value in (scale.x, scale.y, scale.z)):
                failures.append("runtime boundary world-baked OBJ must keep identity scale: {0}".format(actor_label(actor)))
            if "UnityWorldBakedActorIdentity" not in tags:
                failures.append("runtime boundary actor is missing identity source evidence: {0}".format(actor_label(actor)))
            if "UnityRendererBoundsCenterAligned" in tags:
                failures.append("runtime boundary actor still uses obsolete Renderer.bounds alignment: {0}".format(actor_label(actor)))
        for actor in gate_flags:
            tags = actor_tags(actor)
            required = (
                "UnityImportedTightSpriteMesh",
                "UnityObjImportAxisCompensation_FbxFactoryInverseY",
                "UnityRuntimeColor_RSBuildGreen_62_125_195",
                "UnityRuntimeMask_MaskSpritePropertyBlock",
                "UnityBakedOrientationScaleIdentity",
                "UnityRootRotationOverride_Identity",
            )
            for tag in required:
                if tag not in tags:
                    failures.append("runtime CityGate flag {0} is missing source evidence tag {1}".format(actor_label(actor), tag))
            if actor.get_class().get_name() != "StaticMeshActor":
                failures.append("runtime CityGate flag must be an exact world-baked StaticMeshActor, found {0}".format(actor.get_class().get_name()))

        missing_visuals = [
            actor_label(actor)
            for actor in buildings + boundary + ground + support + gate_flags
            if not has_component(actor, unreal.StaticMeshComponent)
        ]
        if missing_visuals:
            failures.append("runtime city actors missing StaticMesh visual: {0}".format(", ".join(missing_visuals[:8])))

        summary = (
            "UnityGameAppCityVerify(RuntimeAllBuildings) actors={0}, tagged={1}, boundary={2}, "
            "ground={3}, support={4}, gate_flags={5}, roads={6}, authored_shadows={7}, buildings={8}, "
            "cameras={9}, player_starts={10}, config_init_buildings={11}"
        ).format(
            len(actors),
            len(runtime_tagged),
            len(boundary),
            len(ground),
            len(support),
            len(gate_flags),
            len(roads),
            len(authored_shadows),
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
                "support": len(support),
                "gate_flags": len(gate_flags),
                "roads": len(roads),
                "authored_shadows": len(authored_shadows),
                "buildings": len(buildings),
                "config_init_buildings": len(config_init_buildings),
                "cameras": len(cameras),
                "player_starts": len(player_starts),
                "ground_identity_count": ground_contract["identity_count"],
                "ground_axis_compensated_count": ground_contract["axis_compensated_count"],
                "ground_face_winding_compensated_count": ground_contract["face_winding_compensated_count"],
                "ground_one_sided_material_count": ground_contract["one_sided_material_count"],
                "ground_predicted_ue_up_facing_triangle_count": ground_contract["predicted_ue_up_facing_triangle_count"],
                "ground_predicted_ue_down_facing_triangle_count": ground_contract["predicted_ue_down_facing_triangle_count"],
                "ground_predicted_ue_degenerate_triangle_count": ground_contract["predicted_ue_degenerate_triangle_count"],
                "ground_imported_ue_up_facing_triangle_count": ground_contract["imported_ue_up_facing_triangle_count"],
                "ground_imported_ue_down_facing_triangle_count": ground_contract["imported_ue_down_facing_triangle_count"],
                "ground_imported_ue_degenerate_triangle_count": ground_contract["imported_ue_degenerate_triangle_count"],
                "ground_max_tight_bounds_center_error": ground_contract["max_bounds_center_error"],
                "ground_max_tight_bounds_extent_error": ground_contract["max_bounds_extent_error"],
                "ground_max_axis_probe_error": ground_contract["max_axis_probe_error"],
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
