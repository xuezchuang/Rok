import csv
import hashlib
import json
import math
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_ROOT = PROJECT_ROOT / "Saved/RokUnityPlacement"
PLACEMENT_JSON = OUTPUT_ROOT / "unity_city_placement.json"
PLACEMENT_CSV = OUTPUT_ROOT / "unity_city_placement.csv"
GRID_CSV = OUTPUT_ROOT / "unity_init_building_grid.csv"
VERIFY_JSON = OUTPUT_ROOT / "unity_city_placement_verify.json"

EXPECTED_SCENE = "Map_TTS.unity"
EXPECTED_BUILDING = "Building_Castle_02"
EXPECTED_SPRITE_GUID = "e6124bd7f7a6dbd45a1fb2d9370d86a5"
EXPECTED_SPRITE_PATH = "ref/resources/Sprite/Castle_6_5.png"
EXPECTED_MASK_PATH = "ref/resources/Sprite/Castle_6_5_mask.png"
EXPECTED_COLOR_ATLAS_PATH = "ref/resources/Texture2D/city_build_6_5_0.png"
EXPECTED_MASK_ATLAS_PATH = "ref/resources/Texture2D/city_build_mask_6_5_0.png"
EXPECTED_ATLAS_SIZE = [1024, 1024]
EXPECTED_ATLAS_RECT_BOTTOM_LEFT = [513, 402, 418, 331]
EXPECTED_ATLAS_RECT_TOP_LEFT = [513, 291, 418, 331]
EXPECTED_COLOR_PIXEL_SHA256 = "5188d1fef08a0b2b51e7198607e732a601af4a7e3cd1c60be2f4d9174b8e433e"
EXPECTED_MASK_PIXEL_SHA256 = "93c07f8a0e2208a0733812fdad14f5675d6d50973bd6a8fca9bee52eb31c32ce"
EXPECTED_GROUND_NAME = "CityWall_Ground_1_1_0 (1)"
EXPECTED_GROUND_PREFAB_GUID = "06deaa9b5051d13469db70c5c24ed512"
EXPECTED_GROUND_MATERIAL_GUID = "628148ec3bbaaf74488352d43cc39a63"


def distance(a, b):
    return math.sqrt(sum((float(a[index]) - float(b[index])) ** 2 for index in range(len(b))))


def pixel_sha256(image):
    return hashlib.sha256(image.convert("RGBA").tobytes()).hexdigest()


def verify_atlas_pixels(failures):
    paths = {
        "color_source": PROJECT_ROOT / EXPECTED_SPRITE_PATH,
        "mask_source": PROJECT_ROOT / EXPECTED_MASK_PATH,
        "color_atlas": PROJECT_ROOT / EXPECTED_COLOR_ATLAS_PATH,
        "mask_atlas": PROJECT_ROOT / EXPECTED_MASK_ATLAS_PATH,
    }
    for label, path in paths.items():
        if not path.exists():
            failures.append("Missing Castle_6_5 runtime-atlas evidence file {0}: {1}".format(label, path))
    if any(not path.exists() for path in paths.values()):
        return {}

    x, y, width, height = EXPECTED_ATLAS_RECT_TOP_LEFT
    result = {}
    for key in ("color", "mask"):
        with Image.open(paths["{0}_source".format(key)]) as source_image, Image.open(paths["{0}_atlas".format(key)]) as atlas_image:
            source_rgba = source_image.convert("RGBA")
            atlas_rgba = atlas_image.convert("RGBA")
            crop = atlas_rgba.crop((x, y, x + width, y + height))
            source_hash = pixel_sha256(source_rgba)
            crop_hash = pixel_sha256(crop)
            expected_hash = EXPECTED_COLOR_PIXEL_SHA256 if key == "color" else EXPECTED_MASK_PIXEL_SHA256
            if list(source_rgba.size) != [width, height]:
                failures.append("Castle_6_5 {0} Sprite must be 418x331".format(key))
            if list(atlas_rgba.size) != EXPECTED_ATLAS_SIZE:
                failures.append("Castle_6_5 {0} runtime atlas must be 1024x1024".format(key))
            if source_rgba.tobytes() != crop.tobytes():
                failures.append("Castle_6_5 {0} Sprite is not the exact unrotated atlas rect".format(key))
            if source_hash != expected_hash or crop_hash != expected_hash:
                failures.append("Castle_6_5 {0} runtime-atlas pixel hash drifted".format(key))
            result[key] = {
                "source_size": list(source_rgba.size),
                "atlas_size": list(atlas_rgba.size),
                "source_pixel_sha256": source_hash,
                "crop_pixel_sha256": crop_hash,
                "exact_pixel_match": source_rgba.tobytes() == crop.tobytes(),
            }
    return result


def main():
    failures = []
    for path in (PLACEMENT_JSON, PLACEMENT_CSV, GRID_CSV):
        if not path.exists():
            failures.append("Missing generated placement output: {0}".format(path))
    if failures:
        result = {"failure_count": len(failures), "failures": failures}
        VERIFY_JSON.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        raise RuntimeError("Unity city placement verification failed")

    manifest = json.loads(PLACEMENT_JSON.read_text(encoding="utf-8"))
    if manifest.get("schema") != "rok.unity_city_placement.v3":
        failures.append("Unexpected placement schema")

    direct_atlas_evidence = verify_atlas_pixels(failures)

    building_records = [
        item for item in manifest.get("placements", [])
        if item.get("scene") == EXPECTED_SCENE and item.get("name") == EXPECTED_BUILDING
    ]
    if len(building_records) != 1:
        failures.append("Expected exactly one Map_TTS Building_Castle_02 record, found {0}".format(len(building_records)))
        building = {}
    else:
        building = building_records[0]
        if not building.get("active"):
            failures.append("Building_Castle_02 should be active")
        if building.get("category") != "city_building":
            failures.append("Building_Castle_02 category should be city_building")
        if building.get("sprite_guid") != EXPECTED_SPRITE_GUID:
            failures.append("Building_Castle_02 sprite GUID mismatch")
        if not building.get("renderer_enabled"):
            failures.append("Building_Castle_02 SpriteRenderer should be enabled")
        if distance(building.get("renderer_color", []), [1.0, 1.0, 1.0, 1.0]) > 0.00001:
            failures.append("Building_Castle_02 SpriteRenderer color mismatch")
        if building.get("draw_mode") != 0 or building.get("mask_interaction") != 0 or building.get("sprite_sort_point") != 0:
            failures.append("Building_Castle_02 SpriteRenderer draw/mask/sort-point settings mismatch")
        if building.get("mapped_sprite_path") != EXPECTED_SPRITE_PATH:
            failures.append("Building_Castle_02 should map to Castle_6_5.png")
        if building.get("mapped_mask_path") != EXPECTED_MASK_PATH:
            failures.append("Building_Castle_02 should map to Castle_6_5_mask.png")
        if building.get("mapping_source") != "known_object_name_map":
            failures.append("Building_Castle_02 mapping must record the snapshot's explicit object-name source chain")
        atlas = building.get("runtime_atlas") or {}
        if atlas.get("color_atlas_path") != EXPECTED_COLOR_ATLAS_PATH or atlas.get("mask_atlas_path") != EXPECTED_MASK_ATLAS_PATH:
            failures.append("Building_Castle_02 runtime atlas paths mismatch")
        if atlas.get("atlas_size") != EXPECTED_ATLAS_SIZE:
            failures.append("Building_Castle_02 runtime atlas size mismatch")
        if atlas.get("texture_rect_unity_bottom_left") != EXPECTED_ATLAS_RECT_BOTTOM_LEFT:
            failures.append("Building_Castle_02 Unity bottom-left textureRect mismatch")
        if atlas.get("texture_rect_top_left") != EXPECTED_ATLAS_RECT_TOP_LEFT:
            failures.append("Building_Castle_02 top-left atlas crop rect mismatch")
        if (
            atlas.get("observed_crop_transform") != "identity"
            or atlas.get("observed_rotation_degrees") != 0
            or atlas.get("observed_flip_x") is not False
            or atlas.get("observed_flip_y") is not False
        ):
            failures.append("Building_Castle_02 exported Sprite should be an observed identity crop of the decoded atlas")
        if atlas.get("unity_packing_metadata_status") != "unresolved_in_snapshot":
            failures.append("Building_Castle_02 Unity packing metadata must remain explicitly unresolved")
        if not atlas.get("color_pixel_match") or not atlas.get("mask_pixel_match"):
            failures.append("Building_Castle_02 runtime atlas must record exact color/mask pixel matches")
        if atlas.get("color_pixel_sha256") != EXPECTED_COLOR_PIXEL_SHA256 or atlas.get("mask_pixel_sha256") != EXPECTED_MASK_PIXEL_SHA256:
            failures.append("Building_Castle_02 runtime atlas pixel hashes mismatch")
        if building.get("source_sprite_meta_status") != "unresolved_in_snapshot" or atlas.get("source_sprite_meta_status") != "unresolved_in_snapshot":
            failures.append("Building_Castle_02 must keep original Sprite pivot/PPU metadata explicitly unresolved")
        if distance(building.get("world_position", []), [-0.104, 0.334, 0.079]) > 0.00001:
            failures.append("Building_Castle_02 world position mismatch")
        if distance(building.get("world_scale", []), [0.24076737, 0.24076748, 0.24076748]) > 0.00001:
            failures.append("Building_Castle_02 world scale mismatch")
        if distance(building.get("local_euler", []), [45.0, 0.0, 0.0]) > 0.00001:
            failures.append("Building_Castle_02 rotation mismatch")
        if building.get("flip_x") or building.get("flip_y"):
            failures.append("Building_Castle_02 source flip should be X=0 Y=0")
        if distance(building.get("sprite_size", []), [8.62, 8.0]) > 0.00001:
            failures.append("Building_Castle_02 sprite size mismatch")

    camera_records = [
        item for item in manifest.get("cameras", [])
        if item.get("scene") == EXPECTED_SCENE and item.get("name") == "Main Camera"
    ]
    if len(camera_records) != 1:
        failures.append("Expected exactly one Map_TTS Main Camera record")
        camera = {}
    else:
        camera = camera_records[0]
        if camera.get("orthographic"):
            failures.append("Map_TTS Main Camera should be perspective")
        if camera.get("fov_axis_mode") != 0:
            failures.append("Map_TTS Main Camera should use vertical FOV axis mode")
        if abs(float(camera.get("vertical_fov", 0.0)) - 12.0) > 0.0001:
            failures.append("Map_TTS Main Camera vertical FOV mismatch")
        if distance(camera.get("world_position", []), [0.52, 15.82, -16.37]) > 0.00001:
            failures.append("Map_TTS Main Camera position mismatch")
        if distance(camera.get("local_euler", []), [45.0, 0.0, 0.0]) > 0.00001:
            failures.append("Map_TTS Main Camera rotation mismatch")
        if camera.get("clear_flags") != 2 or not camera.get("hdr") or not camera.get("msaa") or not camera.get("occlusion_culling"):
            failures.append("Map_TTS Main Camera render flags mismatch")
        if distance(camera.get("background_color", []), [0.745283, 0.6318618, 0.4324048, 0.0]) > 0.00001:
            failures.append("Map_TTS Main Camera background color mismatch")

    ground_records = [
        item for item in manifest.get("prefab_instances", [])
        if item.get("scene") == EXPECTED_SCENE
        and item.get("name", "").startswith("CityWall_Ground_1_1_0")
        and item.get("active")
    ]
    if len(ground_records) != 1:
        failures.append("Expected exactly one enabled CityWall_Ground_1_1_0 prefab record")
        ground = {}
    else:
        ground = ground_records[0]
        if not ground.get("active"):
            failures.append("CityWall_Ground_1_1_0 should be active")
        if ground.get("name") != EXPECTED_GROUND_NAME:
            failures.append("The enabled Map_TTS ground must come from the LV_02 '(1)' instance")
        if ground.get("parent_hierarchy_path") != "City_Use/LV_02":
            failures.append("The enabled Map_TTS ground must be parented below City_Use/LV_02")
        if not ground.get("parent_active"):
            failures.append("The accepted Map_TTS ground parent hierarchy must be active")
        if ground.get("source_prefab_guid") != EXPECTED_GROUND_PREFAB_GUID:
            failures.append("CityWall_Ground source prefab GUID mismatch")
        if ground.get("material_guid") != EXPECTED_GROUND_MATERIAL_GUID:
            failures.append("CityWall_Ground material GUID mismatch")
        if not str(ground.get("material_path", "")).endswith("build_ground_6_101_1.mat"):
            failures.append("CityWall_Ground should resolve build_ground_6_101_1.mat")
        if distance(ground.get("local_position", []), [0.01499939, 0.0, -0.072006226]) > 0.00001:
            failures.append("CityWall_Ground prefab instance position mismatch")
        if abs(float(ground.get("local_scale", [0.0, 0.0, 0.0])[0]) - 1.1) > 0.00001 or abs(float(ground.get("local_scale", [0.0, 0.0, 0.0])[2]) - 1.1) > 0.00001:
            failures.append("CityWall_Ground explicit X/Z scale overrides mismatch")
        if sorted(ground.get("explicit_scale_components", [])) != ["x", "z"] or ground.get("inferred_scale_components") != ["y"]:
            failures.append("CityWall_Ground scale provenance should distinguish explicit X/Z from inferred Y")
        if ground.get("source_prefab_geometry_resolved"):
            failures.append("Snapshot unexpectedly claims the missing CityWall_Ground prefab geometry is resolved")
        if ground.get("material_target_file_id") != 6686674215700230162:
            failures.append("CityWall_Ground renderer material target mismatch")
        for required_active_target in (5439141916090014720, 4969441578155328718):
            if required_active_target not in ground.get("active_target_file_ids", []):
                failures.append("CityWall_Ground missing active prefab target {0}".format(required_active_target))

    with PLACEMENT_CSV.open("r", encoding="utf-8-sig", newline="") as handle:
        csv_records = list(csv.DictReader(handle))
    csv_castle = [item for item in csv_records if item.get("scene") == EXPECTED_SCENE and item.get("name") == EXPECTED_BUILDING]
    if len(csv_castle) != 1:
        failures.append("Placement CSV is missing the unique Building_Castle_02 row")
    else:
        try:
            csv_atlas = json.loads(csv_castle[0].get("runtime_atlas") or "null") or {}
        except json.JSONDecodeError:
            csv_atlas = {}
            failures.append("Placement CSV runtime_atlas is not valid JSON")
        if csv_atlas.get("texture_rect_unity_bottom_left") != EXPECTED_ATLAS_RECT_BOTTOM_LEFT:
            failures.append("Placement CSV is missing the verified Castle_6_5 runtime atlas rect")
        if csv_atlas != (building.get("runtime_atlas") or {}):
            failures.append("Placement CSV runtime_atlas must exactly match the JSON source record")
        if csv_castle[0].get("source_sprite_meta_status") != "unresolved_in_snapshot":
            failures.append("Placement CSV must preserve unresolved Sprite meta provenance")

    with GRID_CSV.open("r", encoding="utf-8-sig", newline="") as handle:
        grid_records = list(csv.DictReader(handle))
    grid_ids = [int(item["ID"]) for item in grid_records if item.get("ID")]
    if len(grid_records) != 26 or sorted(grid_ids) != list(range(1, 27)):
        failures.append("InitBuilding grid CSV should contain stable IDs 1..26")

    result = {
        "schema": manifest.get("schema"),
        "placement_record_count": manifest.get("placement_record_count"),
        "mapped_placement_count": manifest.get("mapped_placement_count"),
        "camera_record_count": manifest.get("camera_record_count"),
        "prefab_instance_record_count": manifest.get("prefab_instance_record_count"),
        "init_grid_count": len(grid_records),
        "castle": building,
        "camera": camera,
        "ground": ground,
        "direct_runtime_atlas_evidence": direct_atlas_evidence,
        "failure_count": len(failures),
        "failures": failures,
    }
    VERIFY_JSON.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    if failures:
        raise RuntimeError("Unity city placement verification failed")


if __name__ == "__main__":
    main()
