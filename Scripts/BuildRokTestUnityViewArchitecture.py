import json
import os
import re
import struct
from collections import Counter, defaultdict
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TESTUNITY_ROOT = Path(r"D:\ueDemo\TestUnity")
TESTUNITY_ROOT = Path(os.environ.get("ROK_TESTUNITY_ROOT", str(DEFAULT_TESTUNITY_ROOT)))
SCENE_PATH = TESTUNITY_ROOT / "Assets" / "Scenes" / "RokCity2_5D_Demo.unity"
CAMERA_SCRIPT_PATH = TESTUNITY_ROOT / "Assets" / "Scripts" / "RokCitySimpleCamera.cs"
DEPENDENCY_MANIFEST_PATH = TESTUNITY_ROOT / "Assets" / "RokExtract" / "dependency-manifest.txt"
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
OUTPUT_JSON = OUTPUT_DIR / "testunity_city_view_architecture.json"
OUTPUT_MD = OUTPUT_DIR / "testunity_city_view_architecture.md"


def read_png_size(path):
    if not path.exists():
        return None
    with path.open("rb") as handle:
        header = handle.read(24)
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", header[16:24])


def parse_vector2(text):
    match = re.search(r"\{x: ([^,]+), y: ([^}]+)\}", text)
    return {"x": float(match.group(1)), "y": float(match.group(2))} if match else {}


def parse_vector3(text):
    match = re.search(r"\{x: ([^,]+), y: ([^,]+), z: ([^}]+)\}", text)
    return {"x": float(match.group(1)), "y": float(match.group(2)), "z": float(match.group(3))} if match else {}


def parse_scene(scene_text):
    objects = {}
    transforms = {}
    children = defaultdict(list)
    sprites = []
    mesh_renderers = []
    cameras = []
    mono_behaviours = []

    for block in scene_text.split("--- !u!"):
        if not block.strip():
            continue
        first_line = block.splitlines()[0].strip()
        object_match = re.match(r"(\d+) &(-?\d+)", first_line)
        if not object_match:
            continue

        type_id = int(object_match.group(1))
        file_id = object_match.group(2)
        if type_id == 1:
            name = re.search(r"\n  m_Name: (.*)", block)
            active = re.search(r"\n  m_IsActive: (\d+)", block)
            components = re.findall(r"- component: \{fileID: (-?\d+)\}", block)
            objects[file_id] = {
                "name": name.group(1) if name else "",
                "active": active.group(1) == "1" if active else False,
                "components": components,
            }
        elif type_id == 4:
            game_object = re.search(r"\n  m_GameObject: \{fileID: (-?\d+)\}", block)
            father = re.search(r"\n  m_Father: \{fileID: (-?\d+)\}", block)
            position = re.search(r"\n  m_LocalPosition: (\{x: [^}]+\})", block)
            scale = re.search(r"\n  m_LocalScale: (\{x: [^}]+\})", block)
            if game_object:
                transforms[file_id] = {
                    "game_object": game_object.group(1),
                    "father": father.group(1) if father else "0",
                    "position": parse_vector3(position.group(1)) if position else {},
                    "scale": parse_vector3(scale.group(1)) if scale else {},
                }
        elif type_id == 20:
            game_object = re.search(r"\n  m_GameObject: \{fileID: (-?\d+)\}", block)
            fov = re.search(r"\n  field of view: ([^\n]+)", block)
            orthographic = re.search(r"\n  orthographic: ([^\n]+)", block)
            background = re.search(r"\n  m_BackGroundColor: (\{r: [^}]+\})", block)
            if game_object:
                cameras.append({
                    "game_object": game_object.group(1),
                    "vertical_fov": float(fov.group(1)) if fov else 0.0,
                    "orthographic": orthographic.group(1) == "1" if orthographic else False,
                    "background": background.group(1) if background else "",
                })
        elif type_id == 23:
            game_object = re.search(r"\n  m_GameObject: \{fileID: (-?\d+)\}", block)
            sorting_order = re.search(r"\n  m_SortingOrder: (-?\d+)", block)
            if game_object:
                mesh_renderers.append({
                    "game_object": game_object.group(1),
                    "sorting_order": int(sorting_order.group(1)) if sorting_order else 0,
                })
        elif type_id == 114:
            script = re.search(r"\n  m_Script: \{fileID: 11500000, guid: ([^,]+), type: 3\}", block)
            view_center = re.search(r"\n  viewCenter: (\{x: [^}]+\})", block)
            world_x = re.search(r"\n  worldX: (\{x: [^}]+\})", block)
            world_z = re.search(r"\n  worldZ: (\{x: [^}]+\})", block)
            camera_forward = re.search(r"\n  cameraForward: (\{x: [^}]+\})", block)
            current_dxf = re.search(r"\n  currentDxf: ([^\n]+)", block)
            camera_view_dist = re.search(r"\n  cameraViewDist: ([^\n]+)", block)
            camera_fov = re.search(r"\n  cameraFov: ([^\n]+)", block)
            if script:
                mono_behaviours.append({
                    "script_guid": script.group(1),
                    "view_center": parse_vector2(view_center.group(1)) if view_center else {},
                    "world_x": parse_vector2(world_x.group(1)) if world_x else {},
                    "world_z": parse_vector2(world_z.group(1)) if world_z else {},
                    "camera_forward": parse_vector3(camera_forward.group(1)) if camera_forward else {},
                    "current_dxf": float(current_dxf.group(1)) if current_dxf else 0.0,
                    "camera_view_dist": float(camera_view_dist.group(1)) if camera_view_dist else 0.0,
                    "camera_fov": float(camera_fov.group(1)) if camera_fov else 0.0,
                })
        elif type_id == 212:
            game_object = re.search(r"\n  m_GameObject: \{fileID: (-?\d+)\}", block)
            sorting_order = re.search(r"\n  m_SortingOrder: (-?\d+)", block)
            sprite = re.search(r"\n  m_Sprite: \{fileID: [^,]+, guid: ([^,\s}]+)", block)
            flip_x = re.search(r"\n  m_FlipX: (\d+)", block)
            flip_y = re.search(r"\n  m_FlipY: (\d+)", block)
            if game_object:
                sprites.append({
                    "game_object": game_object.group(1),
                    "sorting_order": int(sorting_order.group(1)) if sorting_order else 0,
                    "sprite_guid": sprite.group(1) if sprite else "",
                    "flip_x": flip_x.group(1) == "1" if flip_x else False,
                    "flip_y": flip_y.group(1) == "1" if flip_y else False,
                })

    for transform_id, transform in transforms.items():
        children[transform["father"]].append(transform_id)

    def object_name_from_transform(transform_id):
        transform = transforms.get(transform_id, {})
        return objects.get(transform.get("game_object", ""), {}).get("name", "")

    def object_active_from_transform(transform_id):
        transform = transforms.get(transform_id, {})
        return objects.get(transform.get("game_object", ""), {}).get("active", False)

    roots = []
    for transform_id, transform in transforms.items():
        if transform["father"] == "0":
            roots.append({
                "name": object_name_from_transform(transform_id),
                "active": object_active_from_transform(transform_id),
                "position": transform.get("position", {}),
                "scale": transform.get("scale", {}),
                "child_count": len(children[transform_id]),
            })

    dense_children = []
    for transform_id in transforms:
        if object_name_from_transform(transform_id) == "Dense_Layout_Screenshot":
            for child_id in children[transform_id]:
                dense_children.append({
                    "name": object_name_from_transform(child_id),
                    "active": object_active_from_transform(child_id),
                    "position": transforms[child_id].get("position", {}),
                    "scale": transforms[child_id].get("scale", {}),
                    "child_count": len(children[child_id]),
                })

    categories = Counter()
    for obj in objects.values():
        name = obj["name"]
        if name.startswith("Building_") or "_build_" in name:
            categories["building"] += 1
        elif name.startswith("CityWall") or name.startswith("CityTower") or name.startswith("CityGate") or "wall" in name.lower():
            categories["wall_gate_tower"] += 1
        elif name.lower() in {"ground", "city_interior_ground", "outer_desert_ground", "groundcollider"}:
            categories["ground"] += 1
        elif "palm" in name.lower() or "tree" in name.lower() or "grass" in name.lower():
            categories["vegetation"] += 1
        elif name.startswith("unit_") or name == "Arms":
            categories["unit"] += 1
        elif name == "Main Camera":
            categories["camera"] += 1
        else:
            categories["other"] += 1

    sprite_orders = [sprite["sorting_order"] for sprite in sprites] or [0]
    mesh_orders = [mesh["sorting_order"] for mesh in mesh_renderers] or [0]
    camera_records = []
    for camera in cameras:
        camera_record = dict(camera)
        camera_record["name"] = objects.get(camera["game_object"], {}).get("name", "")
        camera_records.append(camera_record)

    return {
        "object_count": len(objects),
        "transform_count": len(transforms),
        "sprite_renderer_count": len(sprites),
        "mesh_renderer_count": len(mesh_renderers),
        "camera_count": len(cameras),
        "roots": roots,
        "dense_layout_child_count": len(dense_children),
        "dense_layout_sample": dense_children[:24],
        "category_counts": dict(categories),
        "sprite_sorting_order": {"min": min(sprite_orders), "max": max(sprite_orders)},
        "mesh_sorting_order": {"min": min(mesh_orders), "max": max(mesh_orders)},
        "cameras": camera_records,
        "camera_behaviour": mono_behaviours[0] if mono_behaviours else {},
    }


def parse_camera_script(script_text):
    camera_keys = []
    for match in re.finditer(r'new CameraInfo\("([^"]+)",\s*([0-9.]+)f,\s*([0-9.]+)f,\s*([0-9.]+)f\)', script_text):
        camera_keys.append({
            "id": match.group(1),
            "dist": float(match.group(2)),
            "vertical_fov": float(match.group(3)),
            "dxf": float(match.group(4)),
        })
    return {
        "camera_keys": camera_keys,
        "has_zoom_to_cursor": "ApplyDxfAtScreenPoint" in script_text,
        "has_dxf_solver": "GetDistFovByDxf" in script_text and "GetDistFovWithTwoCameraInfo" in script_text,
        "has_drag_pan": "BeginDrag" in script_text and "MoveDrag" in script_text,
        "has_keyboard_pan": "HandleKeyboard" in script_text,
    }


def build_manifest():
    scene_text = SCENE_PATH.read_text(encoding="utf-8")
    script_text = CAMERA_SCRIPT_PATH.read_text(encoding="utf-8")
    scene = parse_scene(scene_text)
    camera_script = parse_camera_script(script_text)

    screenshots = []
    for name in ["RokCity2_5D_Demo.png", "RokCity2_5D_Dense.png"]:
        path = TESTUNITY_ROOT / name
        size = read_png_size(path)
        screenshots.append({
            "name": name,
            "path": str(path),
            "exists": path.exists(),
            "width": size[0] if size else 0,
            "height": size[1] if size else 0,
        })

    return {
        "schema": "rok.testunity_city_view_architecture.v1",
        "source_root": str(TESTUNITY_ROOT),
        "source_scene": str(SCENE_PATH),
        "source_camera_script": str(CAMERA_SCRIPT_PATH),
        "source_dependency_manifest": str(DEPENDENCY_MANIFEST_PATH),
        "screenshots": screenshots,
        "scene": scene,
        "camera_script": camera_script,
        "ue_target": {
            "camera_pawn": "Source/Rok/RokStrategyCameraPawn",
            "view_types": "Source/Rok/RokCityViewTypes",
            "target_level": "/Game/Maps/L_RokUnityGameAppCity",
            "unity_to_ue_planar_scale": 100.0,
            "planar_mapping": "Unity (x,z) -> UE (x,y); Unity up y -> UE z",
        },
        "view_architecture_rules": [
            "Camera is perspective 2.5D and uses the Unity dxf curve, not orthographic top-down zoom.",
            "The city view is separated into camera, terrain/ground, road, wall, building body, decoration, unit, and HUD layers.",
            "View state is source-derived from TestUnity before gameplay/data systems drive presentation.",
            "Generated UE maps should keep view/source tags so visual slices can be verified without gameplay assumptions.",
        ],
    }


def write_markdown(manifest):
    scene = manifest["scene"]
    camera = scene.get("camera_behaviour", {})
    lines = [
        "# TestUnity City View Architecture",
        "",
        f"- Schema: `{manifest['schema']}`",
        f"- Source root: `{manifest['source_root']}`",
        f"- Source scene: `{manifest['source_scene']}`",
        f"- Source camera script: `{manifest['source_camera_script']}`",
        f"- Objects/transforms: {scene['object_count']} / {scene['transform_count']}",
        f"- Sprite renderers / mesh renderers: {scene['sprite_renderer_count']} / {scene['mesh_renderer_count']}",
        f"- Dense view children: {scene['dense_layout_child_count']}",
        f"- Camera view center: `{camera.get('view_center', {})}`",
        f"- Camera forward: `{camera.get('camera_forward', {})}`",
        f"- Camera dxf/dist/fov: {camera.get('current_dxf', 0)} / {camera.get('camera_view_dist', 0)} / {camera.get('camera_fov', 0)}",
        "",
        "## Camera Keys",
        "",
        "| Id | Distance | Vertical FOV | Dxf |",
        "| --- | ---: | ---: | ---: |",
    ]
    for key in manifest["camera_script"]["camera_keys"]:
        lines.append(f"| {key['id']} | {key['dist']} | {key['vertical_fov']} | {key['dxf']} |")

    lines.extend(["", "## Scene Categories", ""])
    for name, count in sorted(scene["category_counts"].items()):
        lines.append(f"- {name}: {count}")

    lines.extend(["", "## Screenshots", ""])
    for screenshot in manifest["screenshots"]:
        status = "ok" if screenshot["exists"] else "missing"
        lines.append(f"- {status}: `{screenshot['name']}` {screenshot['width']}x{screenshot['height']}")

    lines.extend(["", "## Rules", ""])
    for rule in manifest["view_architecture_rules"]:
        lines.append(f"- {rule}")

    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    manifest = build_manifest()
    OUTPUT_JSON.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    write_markdown(manifest)
    scene = manifest["scene"]
    print(
        "TestUnity city view architecture generated: "
        f"objects={scene['object_count']} sprites={scene['sprite_renderer_count']} "
        f"meshes={scene['mesh_renderer_count']} dense_children={scene['dense_layout_child_count']}"
    )


if __name__ == "__main__":
    main()
