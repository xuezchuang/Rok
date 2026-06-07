import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
MANIFEST_PATH = OUTPUT_DIR / "testunity_city_view_architecture.json"
VERIFY_PATH = OUTPUT_DIR / "testunity_city_view_architecture_verify.json"
CAMERA_PAWN_CPP = PROJECT_ROOT / "Source" / "Rok" / "RokStrategyCameraPawn.cpp"
VIEW_TYPES_H = PROJECT_ROOT / "Source" / "Rok" / "RokCityViewTypes.h"


def main():
    failures = []
    if not MANIFEST_PATH.exists():
        failures.append(f"Missing generated TestUnity view architecture manifest: {MANIFEST_PATH}")
        manifest = {}
    else:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    if manifest.get("schema") != "rok.testunity_city_view_architecture.v1":
        failures.append(f"Unexpected schema: {manifest.get('schema')}")

    scene = manifest.get("scene", {})
    if scene.get("object_count", 0) < 100:
        failures.append("TestUnity scene has too few objects to be the city view demo")
    if scene.get("sprite_renderer_count", 0) <= 0:
        failures.append("TestUnity scene has no SpriteRenderer records")
    if scene.get("mesh_renderer_count", 0) <= 0:
        failures.append("TestUnity scene has no MeshRenderer records")
    if scene.get("dense_layout_child_count", 0) < 40:
        failures.append("Dense_Layout_Screenshot does not contain the expected dense city view children")

    camera = scene.get("camera_behaviour", {})
    if not camera:
        failures.append("Missing RokCitySimpleCamera behaviour data")
    else:
        if abs(camera.get("camera_fov", 0.0) - 7.5) > 0.01:
            failures.append(f"Unexpected TestUnity camera FOV: {camera.get('camera_fov')}")
        if camera.get("current_dxf", 0.0) <= 0.0:
            failures.append("TestUnity camera dxf is not positive")
        if camera.get("camera_forward", {}).get("z", 0.0) <= 0.0:
            failures.append("TestUnity camera forward does not point toward the city plane")

    camera_script = manifest.get("camera_script", {})
    if len(camera_script.get("camera_keys", [])) < 4:
        failures.append("Missing TestUnity camera key curve")
    for flag in ["has_zoom_to_cursor", "has_dxf_solver", "has_drag_pan", "has_keyboard_pan"]:
        if not camera_script.get(flag):
            failures.append(f"Camera script is missing required view behaviour: {flag}")

    for screenshot in manifest.get("screenshots", []):
        if not screenshot.get("exists"):
            failures.append(f"Missing TestUnity screenshot: {screenshot.get('name')}")
        if screenshot.get("width", 0) < 1000 or screenshot.get("height", 0) < 700:
            failures.append(f"Unexpected TestUnity screenshot size: {screenshot.get('name')}")

    if not CAMERA_PAWN_CPP.exists():
        failures.append("Missing UE strategy camera pawn source")
        camera_source = ""
    else:
        camera_source = CAMERA_PAWN_CPP.read_text(encoding="utf-8")
    if "ECameraProjectionMode::Perspective" not in camera_source:
        failures.append("UE strategy camera is not configured as a perspective view")
    if "FRokCityViewMath::GetDistFovByDxf" not in camera_source:
        failures.append("UE strategy camera does not use the TestUnity dxf curve")
    if "ApplyDxfAtScreenPoint" not in camera_source:
        failures.append("UE strategy camera is missing zoom-to-cursor behaviour")

    if not VIEW_TYPES_H.exists():
        failures.append("Missing UE city view type definitions")
        view_types = ""
    else:
        view_types = VIEW_TYPES_H.read_text(encoding="utf-8")
    for layer_name in ["Terrain", "Road", "Wall", "BuildingBody", "Decoration", "Unit", "Hud"]:
        if layer_name not in view_types:
            failures.append(f"Missing city view layer type: {layer_name}")

    result = {
        "manifest": str(MANIFEST_PATH.relative_to(PROJECT_ROOT)),
        "failure_count": len(failures),
        "failures": failures,
        "object_count": scene.get("object_count", 0),
        "sprite_renderer_count": scene.get("sprite_renderer_count", 0),
        "mesh_renderer_count": scene.get("mesh_renderer_count", 0),
        "dense_layout_child_count": scene.get("dense_layout_child_count", 0),
        "camera_dxf": camera.get("current_dxf", 0),
        "camera_fov": camera.get("camera_fov", 0),
    }
    VERIFY_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")

    if failures:
        print("TestUnity city view architecture verification FAILED")
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)

    print(
        "TestUnity city view architecture verification passed: "
        f"objects={result['object_count']} sprites={result['sprite_renderer_count']} "
        f"meshes={result['mesh_renderer_count']} dxf={result['camera_dxf']}"
    )


if __name__ == "__main__":
    main()
