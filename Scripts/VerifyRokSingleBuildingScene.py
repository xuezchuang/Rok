import json
import math
import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
TARGET_LEVEL = "/Game/Maps/L_RokCitySingleBuilding"
PLACEMENT_JSON = os.path.join(PROJECT_ROOT, "Saved", "RokUnityPlacement", "unity_city_placement.json")
VERIFY_OUTPUT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityPlacement", "rok_single_building_scene_verify.json")
WORLD_SCALE = 170.0
ASPECT_RATIO = 16.0 / 9.0
BUILDING_LAYER_OFFSET = -38.0
EXPECTED_BUILDING_IDS = (681621973, 681621974, 681621975)
EXPECTED_SPRITE_FILE_ID = 21300000
EXPECTED_SPRITE_GUID = "e6124bd7f7a6dbd45a1fb2d9370d86a5"
EXPECTED_ATLAS_SIZE = [1024, 1024]
EXPECTED_ATLAS_RECT_BOTTOM_LEFT = [513, 402, 418, 331]
EXPECTED_ATLAS_RECT_TOP_LEFT = [513, 291, 418, 331]
EXPECTED_COLOR_ATLAS_PATH = "ref/resources/Texture2D/city_build_6_5_0.png"
EXPECTED_MASK_ATLAS_PATH = "ref/resources/Texture2D/city_build_mask_6_5_0.png"
EXPECTED_COLOR_PIXEL_SHA256 = "5188d1fef08a0b2b51e7198607e732a601af4a7e3cd1c60be2f4d9174b8e433e"
EXPECTED_MASK_PIXEL_SHA256 = "93c07f8a0e2208a0733812fdad14f5675d6d50973bd6a8fca9bee52eb31c32ce"
EXPECTED_CAMERA_IDS = (2027188707, 2027188708)
EXPECTED_GROUND_INSTANCE_ID = 961280323
EXPECTED_GROUND_PREFAB_GUID = "06deaa9b5051d13469db70c5c24ed512"
EXPECTED_GROUND_MATERIAL_GUID = "628148ec3bbaaf74488352d43cc39a63"


def actor_label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def actor_tags(actor):
    return [str(tag) for tag in list(actor.tags)]


def has_tag(actor, tag):
    return tag in actor_tags(actor)


def component_by_name(actor, component_class, component_name):
    try:
        components = actor.get_components_by_class(component_class)
    except Exception:
        components = []
    for component in components:
        if component.get_name() == component_name:
            return component
    return None


def package_path(object_path):
    return str(object_path).split(".", 1)[0].replace("UEDPIE_0_", "").rstrip("/")


def enum_text(value):
    return str(value).upper()


def vector_values(value):
    return [float(value.x), float(value.y), float(value.z)]


def distance(a, b):
    return math.sqrt(sum((float(a[index]) - float(b[index])) ** 2 for index in range(len(b))))


def unity_position_to_ue(position):
    return [-float(position[0]) * WORLD_SCALE, float(position[2]) * WORLD_SCALE, float(position[1]) * WORLD_SCALE]


def ue_horizontal_fov(vertical_fov):
    return math.degrees(2.0 * math.atan(math.tan(math.radians(vertical_fov) * 0.5) * ASPECT_RATIO))


def quaternion_multiply(a, b):
    ax, ay, az, aw = [float(item) for item in a]
    bx, by, bz, bw = [float(item) for item in b]
    return [
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    ]


def quaternion_rotate(rotation, vector):
    value = [float(vector[0]), float(vector[1]), float(vector[2]), 0.0]
    conjugate = [-float(rotation[0]), -float(rotation[1]), -float(rotation[2]), float(rotation[3])]
    return quaternion_multiply(quaternion_multiply(rotation, value), conjugate)[:3]


def unity_direction_to_ue(direction):
    converted = [-float(direction[0]), float(direction[2]), float(direction[1])]
    length = max(0.000001, math.sqrt(sum(item * item for item in converted)))
    return [item / length for item in converted]


def load_source_records():
    if not os.path.exists(PLACEMENT_JSON):
        raise RuntimeError("Missing Unity placement manifest: {0}".format(PLACEMENT_JSON))
    with open(PLACEMENT_JSON, "r", encoding="utf-8-sig") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != "rok.unity_city_placement.v3":
        raise RuntimeError("Unexpected placement manifest schema: {0}".format(manifest.get("schema")))

    buildings = [
        item for item in manifest.get("placements", [])
        if item.get("scene") == "Map_TTS.unity" and item.get("name") == "Building_Castle_02" and item.get("active")
    ]
    cameras = [
        item for item in manifest.get("cameras", [])
        if item.get("scene") == "Map_TTS.unity" and item.get("name") == "Main Camera" and item.get("active")
    ]
    grounds = [
        item for item in manifest.get("prefab_instances", [])
        if item.get("scene") == "Map_TTS.unity"
        and item.get("name") == "CityWall_Ground_1_1_0 (1)"
        and item.get("active")
    ]
    if len(buildings) != 1 or len(cameras) != 1 or len(grounds) != 1:
        raise RuntimeError("Map_TTS source identity is ambiguous: buildings={0} cameras={1} grounds={2}".format(
            len(buildings), len(cameras), len(grounds)
        ))
    atlas = buildings[0].get("runtime_atlas") or {}
    if (
        atlas.get("atlas_size") != EXPECTED_ATLAS_SIZE
        or atlas.get("color_atlas_path") != EXPECTED_COLOR_ATLAS_PATH
        or atlas.get("mask_atlas_path") != EXPECTED_MASK_ATLAS_PATH
        or atlas.get("texture_rect_unity_bottom_left") != EXPECTED_ATLAS_RECT_BOTTOM_LEFT
        or atlas.get("texture_rect_top_left") != EXPECTED_ATLAS_RECT_TOP_LEFT
        or atlas.get("observed_crop_transform") != "identity"
        or atlas.get("observed_rotation_degrees") != 0
        or atlas.get("observed_flip_x") is not False
        or atlas.get("observed_flip_y") is not False
        or atlas.get("unity_packing_metadata_status") != "unresolved_in_snapshot"
        or not atlas.get("color_pixel_match")
        or not atlas.get("mask_pixel_match")
        or atlas.get("color_pixel_sha256") != EXPECTED_COLOR_PIXEL_SHA256
        or atlas.get("mask_pixel_sha256") != EXPECTED_MASK_PIXEL_SHA256
        or buildings[0].get("source_sprite_meta_status") != "unresolved_in_snapshot"
    ):
        raise RuntimeError("Map_TTS Castle_6_5 runtime-atlas evidence is incomplete or inconsistent")
    return manifest, buildings[0], cameras[0], grounds[0]


def material_texture_samples(material):
    if not isinstance(material, unreal.Material):
        return []
    return [
        expression.get_editor_property("texture")
        for expression in unreal.MaterialEditingLibrary.get_material_expressions(material)
        if isinstance(expression, unreal.MaterialExpressionTextureSample)
        and expression.get_editor_property("texture")
    ]


def texture_record(texture):
    if not texture:
        return {"path": "", "size": [0, 0], "srgb": None, "has_alpha": None}
    width = int(texture.blueprint_get_size_x()) if hasattr(texture, "blueprint_get_size_x") else 0
    height = int(texture.blueprint_get_size_y()) if hasattr(texture, "blueprint_get_size_y") else 0
    has_alpha = None
    try:
        has_alpha = bool(texture.get_editor_property("has_alpha_channel"))
    except Exception:
        pass
    return {
        "path": texture.get_path_name(),
        "size": [width, height],
        "srgb": bool(texture.get_editor_property("srgb")),
        "has_alpha": has_alpha,
    }


def verify_building_material(card, failures):
    material = card.get_material(0) if card else None
    result = {"path": material.get_path_name() if material else "", "textures": []}
    if not material:
        failures.append("CardMeshComponent has no material")
        return result
    blend_mode = material.get_editor_property("blend_mode")
    shading_model = material.get_editor_property("shading_model")
    result.update({
        "blend_mode": str(blend_mode),
        "shading_model": str(shading_model),
        "two_sided": bool(material.get_editor_property("two_sided")),
        "opacity_mask_clip": float(material.get_editor_property("opacity_mask_clip_value")),
    })
    if "MASKED" not in enum_text(blend_mode):
        failures.append("building card material must be Masked")
    if "UNLIT" not in enum_text(shading_model):
        failures.append("building card material must match Unity SpriteRenderer's unlit presentation")
    if not result["two_sided"]:
        failures.append("building card material must be TwoSided for the UE plane representation")
    if result["opacity_mask_clip"] < 0.3:
        failures.append("building card opacity clip is too low for the cleaned mask")
    textures = [texture_record(texture) for texture in material_texture_samples(material)]
    result["textures"] = textures
    clean = [item for item in textures if "CLEANFULLCANVAS_MAPTTS_SOURCEV2" in item["path"].upper()]
    if len(clean) != 1:
        failures.append("building material must use the unique full-canvas Map_TTS clean texture")
    elif clean[0]["size"] != [418, 331] or clean[0]["has_alpha"] is False:
        failures.append("building clean texture must be 418x331 with alpha")
    return result


def verify_ground_material(component, failures):
    material = component.get_material(0) if component else None
    result = {"path": material.get_path_name() if material else "", "textures": []}
    if not material:
        failures.append("Map_TTS ground has no material")
        return result
    blend_mode = material.get_editor_property("blend_mode")
    shading_model = material.get_editor_property("shading_model")
    result.update({
        "blend_mode": str(blend_mode),
        "shading_model": str(shading_model),
        "two_sided": bool(material.get_editor_property("two_sided")),
    })
    if "OPAQUE" not in enum_text(blend_mode):
        failures.append("Map_TTS ground must preserve Unity Standard Opaque semantics")
    if "DEFAULT_LIT" not in enum_text(shading_model):
        failures.append("Map_TTS ground must use DefaultLit rather than the old Unlit helper")
    if result["two_sided"]:
        failures.append("Map_TTS ground material should keep Standard-style backface culling")
    textures = [texture_record(texture) for texture in material_texture_samples(material)]
    result["textures"] = textures
    source = [item for item in textures if "UNITYCITYGROUND_BUILD_GROUND_6_101_1_MAPTTS_SOURCEV2" in item["path"].upper()]
    if len(source) != 1:
        failures.append("Map_TTS ground material must use the unique Unity sand-ground texture")
    elif source[0]["size"] != [1024, 1024]:
        failures.append("Map_TTS sand-ground texture must be 1024x1024")
    return result


def write_result(result):
    os.makedirs(os.path.dirname(VERIFY_OUTPUT), exist_ok=True)
    with open(VERIFY_OUTPUT, "w", encoding="utf-8") as handle:
        json.dump(result, handle, ensure_ascii=False, indent=2)


def verify_single_building_scene():
    failures = []
    manifest, source_building, source_camera, source_ground = load_source_records()
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        result = {"target_level": TARGET_LEVEL, "failure_count": 1, "failures": ["Missing map: " + TARGET_LEVEL]}
        write_result(result)
        raise RuntimeError(result["failures"][0])

    current_world = unreal.EditorLevelLibrary.get_editor_world()
    if not current_world or package_path(current_world.get_path_name()) != TARGET_LEVEL:
        unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    world = unreal.EditorLevelLibrary.get_editor_world()
    loaded_level = package_path(world.get_path_name()) if world else ""
    if loaded_level != TARGET_LEVEL:
        failures.append("loaded world package mismatch: {0}".format(loaded_level))

    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    buildings = [actor for actor in actors if actor.get_class().get_name() == "RokBuildingActor"]
    source_buildings = [actor for actor in buildings if has_tag(actor, "RokSingleBuildingSourceAcceptance")]
    grounds = [actor for actor in actors if has_tag(actor, "RokSingleBuildingUnityCityGround")]
    cameras = [actor for actor in actors if actor.get_class().get_name() == "CameraActor" and has_tag(actor, "RokUnityPerspectiveCamera")]
    lighting_proxies = [actor for actor in actors if has_tag(actor, "RokSingleBuildingLightingProxy")]

    forbidden_tags = {
        "RokSingleBuildingDiagnostic",
        "RokSingleBuildingCitySurface",
        "RokSingleBuildingCityRoad",
        "RokSingleBuildingGroundPatch",
        "RokSingleBuildingTerrainPlate",
        "RokSingleBuilding_GroundPreview",
    }
    forbidden = [
        actor_label(actor)
        for actor in actors
        if forbidden_tags.intersection(actor_tags(actor))
        or "RuntimeShadow" in actor_label(actor)
        or "Legacy" in actor_label(actor)
    ]
    if len(buildings) != 1 or len(source_buildings) != 1:
        failures.append("expected exactly one source-acceptance RokBuildingActor")
    if len(grounds) != 1:
        failures.append("expected exactly one active LV_02 Unity ground actor")
    if len(cameras) != 1:
        failures.append("expected exactly one Map_TTS perspective CameraActor")
    if len(lighting_proxies) != 2:
        failures.append("expected the two tagged Map_TTS ambient-lighting proxies")
    if forbidden:
        failures.append("diagnostic/legacy visual layers are forbidden: {0}".format(", ".join(forbidden)))

    unexpected_environment = [
        actor_label(actor)
        for actor in actors
        if actor.get_class().get_name() in ("PointLight", "SpotLight", "RectLight", "ExponentialHeightFog", "SkyAtmosphere")
    ]
    if unexpected_environment:
        failures.append("prototype lighting/fog actors leaked into Map_TTS baseline: {0}".format(", ".join(unexpected_environment)))

    building_result = {}
    if source_buildings:
        actor = source_buildings[0]
        tags = actor_tags(actor)
        expected_tags = [
            "UnityScene_Map_TTS_unity",
            "UnityObject_Building_Castle_02",
            "UnityGameObjectFileId_{0}".format(EXPECTED_BUILDING_IDS[0]),
            "UnityTransformFileId_{0}".format(EXPECTED_BUILDING_IDS[1]),
            "UnityRendererFileId_{0}".format(EXPECTED_BUILDING_IDS[2]),
            "UnitySpriteFileId_{0}".format(EXPECTED_SPRITE_FILE_ID),
            "UnitySpriteGuid_{0}".format(EXPECTED_SPRITE_GUID),
            "UnityMapping_known_object_name_map",
            "UnityHierarchy_City_Use_LV_02_Building_Castle_02",
            "UnityFlipX_0",
            "UnityFlipY_0",
            "UnityAtlas_city_build_6_5_0",
            "UnityAtlasMask_city_build_mask_6_5_0",
            "UnityAtlasRectBL_513_402_418_331",
            "UnityAtlasRectTL_513_291_418_331",
            "UnityAtlasObservedCropTransform_Identity",
            "UnityAtlasPixelMatch_ColorMask",
            "UELayerOffset_-38",
            "UnitySpriteMeta_Unresolved",
            "UnitySpritePackingMeta_Unresolved",
            "UnitySpritePivotMeta_Unresolved",
        ]
        missing_tags = [tag for tag in expected_tags if tag not in tags]
        if missing_tags:
            failures.append("building is missing Map_TTS source tags: {0}".format(", ".join(missing_tags)))

        expected_root = unity_position_to_ue(source_building["world_position"])
        expected_root[2] = 0.0
        root_error = distance(vector_values(actor.get_actor_location()), expected_root)
        if root_error > 0.05:
            failures.append("building gameplay root XY mismatch, error={0:.4f}".format(root_error))

        card = component_by_name(actor, unreal.StaticMeshComponent, "CardMeshComponent")
        visual_root = component_by_name(actor, unreal.SceneComponent, "VisualRoot")
        footprint_collision = component_by_name(actor, unreal.BoxComponent, "FootprintCollision")
        footprint_highlight = component_by_name(actor, unreal.StaticMeshComponent, "FootprintHighlightComponent")
        if not card or not visual_root or not footprint_collision or not footprint_highlight:
            failures.append("RokBuildingActor source presentation components are incomplete")
        card_material = verify_building_material(card, failures) if card else {}

        sprite_size = source_building["sprite_size"]
        source_scale = source_building["world_scale"]
        expected_size = [
            float(sprite_size[0]) * float(source_scale[0]) * WORLD_SCALE,
            float(sprite_size[1]) * float(source_scale[1]) * WORLD_SCALE,
        ]
        expected_card_scale = [expected_size[0] / 100.0, -expected_size[1] / 100.0, 1.0]
        card_scale_error = distance(vector_values(card.get_editor_property("relative_scale3d")), expected_card_scale) if card else 999999.0
        if card_scale_error > 0.02:
            failures.append("building card size/UE plane Y-flip mismatch, error={0:.4f}".format(card_scale_error))

        source_forward = quaternion_rotate(source_building["world_rotation"], [0.0, 0.0, 1.0])
        expected_forward = unity_direction_to_ue(source_forward)
        expected_pivot = [
            expected_forward[0] * BUILDING_LAYER_OFFSET,
            expected_forward[1] * BUILDING_LAYER_OFFSET,
            float(source_building["world_position"][1]) * WORLD_SCALE + expected_forward[2] * BUILDING_LAYER_OFFSET,
        ]
        pivot_error = distance(vector_values(visual_root.get_editor_property("relative_location")), expected_pivot) if visual_root else 999999.0
        if pivot_error > 0.05:
            failures.append("building source center/layer-offset mismatch, error={0:.4f}".format(pivot_error))
        if card:
            try:
                if "NO_COLLISION" not in enum_text(card.get_editor_property("collision_enabled")):
                    failures.append("building card must not participate in selection collision")
            except Exception:
                pass
            if bool(card.get_editor_property("render_custom_depth")):
                failures.append("building card must not use CustomDepth outline")
        if footprint_collision:
            try:
                if "QUERY_ONLY" not in enum_text(footprint_collision.get_editor_property("collision_enabled")):
                    failures.append("building footprint collision must remain QueryOnly")
            except Exception:
                pass

        building_result = {
            "actor": actor_label(actor),
            "root_location": vector_values(actor.get_actor_location()),
            "expected_root_location": expected_root,
            "root_error": root_error,
            "visual_root_relative_location": vector_values(visual_root.get_editor_property("relative_location")) if visual_root else [],
            "expected_visual_root_relative_location": expected_pivot,
            "visual_root_error": pivot_error,
            "expected_visual_size": expected_size,
            "card_relative_scale": vector_values(card.get_editor_property("relative_scale3d")) if card else [],
            "expected_card_relative_scale": expected_card_scale,
            "card_scale_error": card_scale_error,
            "source_tags_missing": missing_tags,
            "material": card_material,
            "runtime_atlas": source_building.get("runtime_atlas"),
        }

    ground_result = {}
    if grounds:
        actor = grounds[0]
        tags = actor_tags(actor)
        expected_tags = [
            "UnityPrefabInstanceFileId_{0}".format(EXPECTED_GROUND_INSTANCE_ID),
            "UnityPrefabGuid_{0}".format(EXPECTED_GROUND_PREFAB_GUID),
            "UnityMaterialGuid_{0}".format(EXPECTED_GROUND_MATERIAL_GUID),
            "UnityParent_City_Use_LV_02",
            "UnityGroundMaterial_StandardOpaqueZWrite",
            "UnityGroundGeometry_InferredPlaneProxy41x41",
            "UnityGroundPrefabGeometry_Unresolved",
        ]
        missing_tags = [tag for tag in expected_tags if tag not in tags]
        if missing_tags:
            failures.append("ground is missing Map_TTS source/proxy tags: {0}".format(", ".join(missing_tags)))
        expected_location = unity_position_to_ue(source_ground["local_position"])
        location_error = distance(vector_values(actor.get_actor_location()), expected_location)
        rotation = actor.get_actor_rotation()
        rotation_error = abs(float(rotation.pitch)) + abs(float(rotation.roll)) + abs(float(rotation.yaw) - 45.0)
        expected_scale_xy = 41.0 * 1.1 * WORLD_SCALE / 100.0
        scale = vector_values(actor.get_actor_scale3d())
        scale_error = distance(scale, [expected_scale_xy, expected_scale_xy, 1.0])
        if location_error > 0.05:
            failures.append("active LV_02 ground location mismatch, error={0:.4f}".format(location_error))
        if rotation_error > 0.05:
            failures.append("active LV_02 ground yaw should be +45 degrees")
        if scale_error > 0.05:
            failures.append("inferred 41x41 ground plane proxy scale mismatch")
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        ground_material = verify_ground_material(component, failures)
        ground_result = {
            "actor": actor_label(actor),
            "location": vector_values(actor.get_actor_location()),
            "expected_location": expected_location,
            "location_error": location_error,
            "rotation": [float(rotation.pitch), float(rotation.yaw), float(rotation.roll)],
            "scale": scale,
            "scale_error": scale_error,
            "source_tags_missing": missing_tags,
            "material": ground_material,
            "geometry_status": "inferred 41x41 plane proxy; source prefab mesh/UV/pivot unavailable",
        }

    camera_result = {}
    if cameras:
        actor = cameras[0]
        tags = actor_tags(actor)
        expected_tags = [
            "UnityCameraFileId_{0}".format(EXPECTED_CAMERA_IDS[0]),
            "UnityTransformFileId_{0}".format(EXPECTED_CAMERA_IDS[1]),
            "UnityClearFlags_2",
            "UnityHDR_1",
            "UnityMSAA_1",
            "UnityOcclusionCulling_1",
            "UnityNearClip_0.3",
            "UnityFarClip_1000",
        ]
        missing_tags = [tag for tag in expected_tags if tag not in tags]
        if missing_tags:
            failures.append("camera is missing Map_TTS source tags: {0}".format(", ".join(missing_tags)))
        expected_location = unity_position_to_ue(source_camera["world_position"])
        location_error = distance(vector_values(actor.get_actor_location()), expected_location)
        rotation = actor.get_actor_rotation()
        rotation_error = distance([float(rotation.pitch), float(rotation.yaw), float(rotation.roll)], [-45.0, 90.0, 0.0])
        component = actor.get_component_by_class(unreal.CameraComponent)
        if not component:
            failures.append("Map_TTS camera has no CameraComponent")
            actual_fov = 0.0
            actual_aspect = 0.0
        else:
            if "PERSPECTIVE" not in enum_text(component.get_editor_property("projection_mode")):
                failures.append("Map_TTS camera must be perspective")
            actual_fov = float(component.get_editor_property("field_of_view"))
            actual_aspect = float(component.get_editor_property("aspect_ratio"))
            if not bool(component.get_editor_property("constrain_aspect_ratio")):
                failures.append("Map_TTS camera must constrain the 16:9 comparison aspect")
        expected_fov = ue_horizontal_fov(float(source_camera["vertical_fov"]))
        if location_error > 0.05:
            failures.append("Map_TTS camera position mismatch, error={0:.4f}".format(location_error))
        if rotation_error > 0.05:
            failures.append("Map_TTS camera rotation/handedness mismatch, error={0:.4f}".format(rotation_error))
        if abs(actual_fov - expected_fov) > 0.05 or abs(actual_aspect - ASPECT_RATIO) > 0.0001:
            failures.append("Map_TTS camera FOV/aspect mismatch")
        camera_result = {
            "actor": actor_label(actor),
            "location": vector_values(actor.get_actor_location()),
            "expected_location": expected_location,
            "location_error": location_error,
            "rotation": [float(rotation.pitch), float(rotation.yaw), float(rotation.roll)],
            "expected_rotation": [-45.0, 90.0, 0.0],
            "rotation_error": rotation_error,
            "horizontal_fov": actual_fov,
            "expected_horizontal_fov": expected_fov,
            "aspect_ratio": actual_aspect,
            "source_background_color": source_camera.get("background_color"),
            "source_tags_missing": missing_tags,
        }

    result = {
        "schema": "rok.single_building.source_baseline_verify.v3",
        "target_level": TARGET_LEVEL,
        "loaded_level": loaded_level,
        "source_scene": "ref/client-unity/Assets/BundleAssets/Map/Scenes/Map_TTS.unity",
        "actor_count": len(actors),
        "source_building_count": len(source_buildings),
        "source_ground_count": len(grounds),
        "source_camera_count": len(cameras),
        "lighting_proxy_count": len(lighting_proxies),
        "forbidden_actor_count": len(forbidden),
        "building": building_result,
        "ground": ground_result,
        "camera": camera_result,
        "known_source_gaps": {
            "ground_prefab_geometry": "unresolved GUID 06deaa9b5051d13469db70c5c24ed512; UE plane is an explicit proxy",
            "sprite_meta": "runtime atlas rect and unrotated color/mask crops are verified; original sprite meta/pivot/PPU remains absent",
            "lighting": "Unity default-skybox ambient is represented by two tagged UE lighting proxies",
            "building_layer_offset_cm": BUILDING_LAYER_OFFSET,
        },
        "failure_count": len(failures),
        "failures": failures,
    }
    write_result(result)
    line = "SingleBuildingSourceVerify actors={0} building={1} ground={2} camera={3} failures={4}".format(
        len(actors), len(source_buildings), len(grounds), len(cameras), len(failures)
    )
    print(line)
    unreal.log(line)
    if failures:
        raise RuntimeError("; ".join(failures))
    return result


if __name__ == "__main__":
    try:
        verify_single_building_scene()
    except Exception as exc:
        unreal.log_error(str(exc))
        sys.exit(1)
