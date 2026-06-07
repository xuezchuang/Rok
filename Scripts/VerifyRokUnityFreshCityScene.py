import json
import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)


TARGET_LEVEL = os.environ.get("ROK_UNITY_FRESH_CITY_LEVEL", "/Game/Maps/L_RokUnityFreshCity")
VERIFY_OUTPUT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_fresh_city_verify.json")
SCENE_MANIFEST = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_fresh_city_scene_manifest.json")
CITY_TAG = "RokUnityFreshCity"


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


def has_component(actor, component_class):
    try:
        return actor.get_component_by_class(component_class) is not None
    except Exception:
        return False


def has_tag(actor, tag):
    return tag in actor_tags(actor)


def expected_ue_from_unity_position(position):
    return (
        -float(position[0]) * 500.0,
        float(position[2]) * 500.0,
        float(position[1]) * 500.0,
    )


def load_scene_manifest():
    if not os.path.exists(SCENE_MANIFEST):
        return {}
    with open(SCENE_MANIFEST, "r", encoding="utf-8") as handle:
        return json.load(handle)


def verify_scene():
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Missing map: {0}".format(TARGET_LEVEL))

    manifest = load_scene_manifest()
    unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    tagged = [actor for actor in actors if has_tag(actor, CITY_TAG)]
    buildings = [actor for actor in tagged if has_tag(actor, "RokUnityFreshInitBuilding")]
    boundary = [actor for actor in tagged if has_tag(actor, "RokUnityFreshBoundarySource")]
    ground = [actor for actor in tagged if has_tag(actor, "RokUnityFreshGround")]
    source_chain = [actor for actor in tagged if has_tag(actor, "UnityRuntimeChain_GameAppScene")]
    cameras = [
        actor for actor in tagged
        if actor.get_class().get_name() == "CameraActor"
        and actor_label(actor) == "Camera_RokUnityFreshCity"
    ]
    player_starts = [
        actor for actor in tagged
        if actor.get_class().get_name() == "PlayerStart"
        and actor_label(actor) == "PlayerStart_RokUnityFreshCity"
    ]

    boundary_tags = [tag for actor in boundary for tag in actor_tags(actor)]
    has_wall = any("UnityMesh_CityWall_101_1" in tag for tag in boundary_tags)
    has_gate = any("UnityMesh_CityGate_101_1" in tag for tag in boundary_tags)
    has_tower = any("UnityMesh_CityTower_101_1" in tag for tag in boundary_tags)
    has_citywall_prefab = any("UnitySourcePrefab_build_6_101_1" in tag for tag in boundary_tags)
    has_guardtower_prefab = any("UnitySourcePrefab_build_8_101_1" in tag for tag in boundary_tags)
    exact_boundary = [
        actor for actor in boundary
        if has_tag(actor, "RokUnityBoundaryExactPrefabTransform")
    ]
    runtime_scaled_boundary = [
        actor for actor in boundary
        if has_tag(actor, "UnityRuntimeGridScaleApplied")
    ]
    container_yaw_boundary = [
        actor for actor in boundary
        if has_tag(actor, "UnityCityContainerYawApplied")
    ]
    direct_png_buildings = [
        actor for actor in buildings
        if has_tag(actor, "UnityVisualResolution_direct_png")
    ]
    prefab_sprite_buildings = [
        actor for actor in buildings
        if has_tag(actor, "UnityVisualResolution_prefab_sprite_renderer")
    ]
    fallback_walls = [actor for actor in tagged if has_tag(actor, "FallbackCubeWall")]
    tower_asset_review = [
        actor for actor in tagged
        if has_tag(actor, "UnityTowerAssetReview")
    ]
    boundary_asset_review = [
        actor for actor in tagged
        if has_tag(actor, "UnityBoundaryAssetReview")
    ]

    failures = []
    if len(source_chain) != 1:
        failures.append("expected exactly 1 Unity runtime source-chain marker, found {0}".format(len(source_chain)))
    skipped = manifest.get("skipped_init_buildings", []) if manifest else []
    represented_by_boundary = [
        item for item in skipped
        if item.get("status") == "represented_by_boundary_prefab"
    ]
    unexpected_skips = [
        item for item in skipped
        if item.get("status") != "represented_by_boundary_prefab"
    ]
    expected_buildings = int(manifest.get("initial_city_building_count", 0) or 0) - len(represented_by_boundary)
    if expected_buildings > 0 and len(buildings) != expected_buildings:
        failures.append("expected {0} source InitBuilding visual actors, found {1}".format(expected_buildings, len(buildings)))
    elif len(buildings) < 6:
        failures.append("expected at least 6 source InitBuilding visual actors, found {0}".format(len(buildings)))
    if unexpected_skips:
        failures.append("unexpected skipped InitBuilding entries: {0}".format(
            ", ".join("init {0} {1}".format(item.get("init_id"), item.get("status")) for item in unexpected_skips[:8])
        ))
    if expected_buildings > 0 and len(direct_png_buildings) + len(prefab_sprite_buildings) != len(buildings):
        failures.append("every InitBuilding actor must record direct_png or prefab_sprite_renderer visual resolution")
    if expected_buildings > 0 and len(prefab_sprite_buildings) == 0:
        failures.append("expected prefab SpriteRenderer-resolved buildings for Rok-unity decoration prefabs")
    if len(boundary) < 3:
        failures.append("expected at least 3 Rok-unity prefab-driven boundary actors, found {0}".format(len(boundary)))
    if len(boundary) != len(exact_boundary):
        failures.append("all boundary actors must use exact Unity prefab transform; exact={0}, boundary={1}".format(
            len(exact_boundary),
            len(boundary),
        ))
    if len(boundary) != len(runtime_scaled_boundary):
        failures.append("all boundary actors must apply Unity GridCollideItem runtime scale; scaled={0}, boundary={1}".format(
            len(runtime_scaled_boundary),
            len(boundary),
        ))
    if len(boundary) != len(container_yaw_boundary):
        failures.append("all boundary actors must apply CityBuildingContainer Y=-45 runtime yaw; yaw_applied={0}, boundary={1}".format(
            len(container_yaw_boundary),
            len(boundary),
        ))
    boundary_by_label = {actor_label(actor): actor for actor in boundary}
    for record in manifest.get("boundary_actors", []):
        label = record.get("actor", "")
        actor = boundary_by_label.get(label)
        if not actor:
            failures.append("manifest boundary actor is missing from map: {0}".format(label))
            continue
        expected_x, expected_y, expected_bottom_z = expected_ue_from_unity_position(record.get("unity_world_position", [0, 0, 0]))
        tolerance = 0.5
        if record.get("ue_tower_bounds_normalized_to_pivot"):
            origin, extent = actor.get_actor_bounds(False)
            actual_bottom_z = float(origin.z - extent.z)
            if abs(float(origin.x) - expected_x) > tolerance or abs(float(origin.y) - expected_y) > tolerance:
                failures.append("tower visible center mismatch for {0}: expected ({1:.2f},{2:.2f}), got ({3:.2f},{4:.2f})".format(
                    label,
                    expected_x,
                    expected_y,
                    float(origin.x),
                    float(origin.y),
                ))
            if abs(actual_bottom_z - expected_bottom_z) > tolerance:
                failures.append("tower visible bottom mismatch for {0}: expected {1:.2f}, got {2:.2f}".format(
                    label,
                    expected_bottom_z,
                    actual_bottom_z,
                ))
        else:
            location = actor.get_actor_location()
            if (
                abs(float(location.x) - expected_x) > tolerance
                or abs(float(location.y) - expected_y) > tolerance
                or abs(float(location.z) - expected_bottom_z) > tolerance
            ):
                failures.append("boundary actor transform mismatch for {0}: expected ({1:.2f},{2:.2f},{3:.2f}), got ({4:.2f},{5:.2f},{6:.2f})".format(
                    label,
                    expected_x,
                    expected_y,
                    expected_bottom_z,
                    float(location.x),
                    float(location.y),
                    float(location.z),
                ))
    if not has_wall:
        failures.append("missing CityWall_101_1 source mesh actor")
    if not has_gate:
        failures.append("missing CityGate_101_1 source mesh actor")
    if not has_tower:
        failures.append("missing CityTower_101_1 source mesh actor")
    if not has_citywall_prefab:
        failures.append("missing source tag for build_6_101_1 city-wall prefab")
    if not has_guardtower_prefab:
        failures.append("missing source tag for build_8_101_1 guard-tower/gate prefab")
    if fallback_walls:
        failures.append("fallback cube walls are not allowed in fresh Rok-unity city scene")
    if len(ground) < 2:
        failures.append("expected at least 2 source ground/road actors, found {0}".format(len(ground)))
    if len(tower_asset_review) != 8:
        failures.append("expected all 8 CityTower_101_1_* assets in AssetReview, found {0}".format(len(tower_asset_review)))
    if len(boundary_asset_review) != 10:
        failures.append("expected all 10 boundary FBX assets in AssetReview, found {0}".format(len(boundary_asset_review)))
    if len(cameras) != 1:
        failures.append("expected exactly 1 Camera_RokUnityFreshCity, found {0}".format(len(cameras)))
    elif not has_component(cameras[0], unreal.CameraComponent):
        failures.append("Camera_RokUnityFreshCity is missing a CameraComponent")
    if len(player_starts) != 1:
        failures.append("expected exactly 1 PlayerStart_RokUnityFreshCity, found {0}".format(len(player_starts)))

    missing_mesh = [
        actor_label(actor)
        for actor in buildings + boundary + ground
        if not has_component(actor, unreal.StaticMeshComponent)
    ]
    if missing_mesh:
        failures.append("actors missing StaticMeshComponent: {0}".format(", ".join(missing_mesh[:10])))

    folders = {}
    for actor in tagged:
        folder = actor_folder(actor)
        folders[folder] = folders.get(folder, 0) + 1

    summary = {
        "target_level": TARGET_LEVEL,
        "actors": len(actors),
        "tagged": len(tagged),
        "source_chain": len(source_chain),
        "buildings": len(buildings),
        "direct_png_buildings": len(direct_png_buildings),
        "prefab_sprite_buildings": len(prefab_sprite_buildings),
        "boundary": len(boundary),
        "exact_boundary": len(exact_boundary),
        "runtime_scaled_boundary": len(runtime_scaled_boundary),
        "container_yaw_boundary": len(container_yaw_boundary),
        "ground": len(ground),
        "boundary_asset_review": len(boundary_asset_review),
        "tower_asset_review": len(tower_asset_review),
        "cameras": len(cameras),
        "player_starts": len(player_starts),
        "has_wall": has_wall,
        "has_gate": has_gate,
        "has_tower": has_tower,
        "has_citywall_prefab": has_citywall_prefab,
        "has_guardtower_prefab": has_guardtower_prefab,
        "folders": folders,
        "failures": failures,
    }
    os.makedirs(os.path.dirname(VERIFY_OUTPUT), exist_ok=True)
    with open(VERIFY_OUTPUT, "w", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)

    line = (
        "UnityFreshCityVerify actors={actors}, tagged={tagged}, buildings={buildings}, "
        "direct_png={direct_png_buildings}, prefab_sprite={prefab_sprite_buildings}, "
        "boundary={boundary}, exact_boundary={exact_boundary}, runtime_scaled={runtime_scaled_boundary}, container_yaw={container_yaw_boundary}, ground={ground}, boundary_review={boundary_asset_review}, tower_review={tower_asset_review}, "
        "cameras={cameras}, player_starts={player_starts}"
    ).format(**summary)
    print(line)
    unreal.log(line)

    if failures:
        for failure in failures:
            unreal.log_error(failure)
        raise RuntimeError("Rok Unity fresh city verification failed")


if __name__ == "__main__":
    verify_scene()
