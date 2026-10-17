"""Export the verified Unity Academy Sprite.mesh as a local UE OBJ.

The source prefab pose is deliberately not baked here. This local mesh is
usable by an isolated visual comparison and by the eventual runtime pose.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
from rok_data_paths import resolve

from BuildRokAcademyVisualSource import OUT as CATALOG_PATH, ROOT, build


RUNS = ROOT / "Saved/UnityAcademyVisualProbe/OutputRuns"
DEST_ROOT = ROOT / "Saved/RokUnityMigration/AcademyPresentationMesh"
MESH_NAME = "SM_build_13_101_2_SourceSpriteMesh"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical_sha(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()


def load_source(run):
    run = run.resolve()
    if run.parent != RUNS.resolve() or not run.is_dir():
        raise RuntimeError("Academy native run must be an existing direct OutputRuns child")
    catalog = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    if catalog != build(write=False):
        raise RuntimeError("Academy source catalog differs from current immutable sources")
    for relative, expected in catalog["sources"].items():
        if sha(resolve(relative)) != expected:
            raise RuntimeError(f"Academy source hash differs: {relative}")
    verify_path = run / "native_verify.json"
    render_path = run / "native_render.json"
    verified = json.loads(verify_path.read_text(encoding="utf-8"))
    rendered = json.loads(render_path.read_text(encoding="utf-8"))
    if (verified["schema"] != "rok.academy_visual_native_verify.v2" or not verified["passed"] or
            Path(verified["run_directory"]).resolve() != run or
            verified["catalog_sha256"] != sha(CATALOG_PATH) or
            verified["native_report_sha256"] != sha(render_path) or
            rendered["schema"] != "rok.academy_visual_native_render.v2" or
            rendered["colorSpace"] != "Gamma" or
            rendered["unityVersion"] != "2019.4.28f1c1"):
        raise RuntimeError("Academy native source rendering is not the accepted verified run")
    geometry = rendered["geometry"]
    vertices = geometry["vertices"]
    uvs = geometry["uvs"]
    indices = geometry["triangles"]
    if (len(vertices) != verified["sprite_vertex_count"] or len(vertices) != 49 or
            len(uvs) != len(vertices) or len(indices) != verified["sprite_triangle_count"] * 3 or
            len(indices) != 47 * 3 or
            geometry["rect"]["width"] != 512 or geometry["rect"]["height"] != 426 or
            geometry["pivot"] != {"x": 256.0, "y": 213.0} or
            abs(geometry["pixelsPerUnit"] - 100.0) > 1e-6 or
            verified["sprite_pivot_pixels"] != geometry["pivot"] or
            verified["sprite_rect"] != geometry["rect"] or
            rendered["camera"]["source_row"] != "city_default"):
        raise RuntimeError("Academy imported tight SpriteMesh dimensions differ")
    if any(not math.isfinite(float(vertex[key])) for vertex in vertices for key in ("x", "y")) or \
       any(not math.isfinite(float(uv[key])) or not -1e-6 <= float(uv[key]) <= 1.0 + 1e-6
           for uv in uvs for key in ("x", "y")) or \
       any(not isinstance(index, int) or not 0 <= index < len(vertices) for index in indices):
        raise RuntimeError("Academy SpriteMesh has invalid vertex, UV, or triangle data")
    winding = []
    for offset in range(0, len(indices), 3):
        a, b, c = (vertices[indices[offset + i]] for i in range(3))
        signed_area = ((b["x"] - a["x"]) * (c["y"] - a["y"]) -
                       (b["y"] - a["y"]) * (c["x"] - a["x"]))
        if abs(signed_area) < 1e-10:
            raise RuntimeError(f"Academy SpriteMesh triangle {offset // 3} is degenerate")
        winding.append(math.copysign(1, signed_area))
    if len(set(winding)) != 1:
        raise RuntimeError("Academy SpriteMesh source triangles have mixed winding")
    return catalog, verified, rendered


def expected_obj(run, geometry):
    vertices = geometry["vertices"]
    uvs = geometry["uvs"]
    indices = geometry["triangles"]
    # Unity local sprite XY (metres) -> UE local XZ (centimetres), preserving
    # its imported tight-mesh vertex/UV identity and pivot. No prefab pose.
    ue_vertices = [[-float(v["x"]) * 100.0, 0.0, float(v["y"]) * 100.0]
                   for v in vertices]
    # The repo's UE 5.8 legacy FbxFactory OBJ route reflects the imported Y
    # axis. Supply the inverse Y reflection, as with the city flag pipeline.
    obj_vertices = [[v[0], -v[1], v[2]] for v in ue_vertices]
    # Match the established SpriteMesh OBJ export order (c,b,a). UVs remain
    # the exact Unity Sprite.uv values; image parity will validate import V.
    faces = [tuple(indices[offset + i] + 1 for i in (2, 1, 0))
             for offset in range(0, len(indices), 3)]
    lines = ["# Academy Unity 2019 imported tight Sprite.mesh; local only",
             "# Source run: " + run.name,
             "# 1 Unity unit = 100 UE cm; UE=(-UnityX,UnityZ,UnityY)",
             "o " + MESH_NAME]
    for x, y, z in obj_vertices:
        lines.append(f"v {x:.9f} {y:.9f} {z:.9f}")
    for uv in uvs:
        lines.append(f"vt {float(uv['x']):.9f} {float(uv['y']):.9f}")
    # Reversed source winding gives positive local UE Y before OBJ import.
    lines.append("vn 0.000000000 1.000000000 0.000000000")
    for a, b, c in faces:
        lines.append(f"f {a}/{a}/1 {b}/{b}/1 {c}/{c}/1")
    return "\n".join(lines) + "\n", ue_vertices, faces


def prepare(run):
    run = run.resolve()
    catalog, verified, rendered = load_source(run)
    geometry = rendered["geometry"]
    vertices = geometry["vertices"]
    uvs = geometry["uvs"]
    obj_text, ue_vertices, faces = expected_obj(run, geometry)
    out = DEST_ROOT / run.name
    out.mkdir(parents=True, exist_ok=True)
    obj_path = out / f"{MESH_NAME}.obj"
    manifest_path = out / "source_mesh_manifest.json"
    obj_path.write_text(obj_text, encoding="utf-8", newline="\n")
    mins = [min(v[axis] for v in ue_vertices) for axis in range(3)]
    maxs = [max(v[axis] for v in ue_vertices) for axis in range(3)]
    child_position = geometry["childLocalPosition"]
    child_rotation = geometry["childLocalRotation"]
    child_scale = geometry["childLocalScale"]
    if max(abs(child_scale[key] - child_scale["x"]) for key in ("y", "z")) > 1e-6:
        raise RuntimeError("Academy source child sprite scale is no longer uniform")
    runtime_scale_factor = 500.0 / 100.0
    manifest = {
        "schema": "rok.academy_source_sprite_mesh.v1",
        "scope": "Local source Sprite.mesh only; no independent probe prefab pose or runtime city pose baked",
        "source_run": str(run),
        "source_catalog_sha256": sha(CATALOG_PATH),
        "native_render_sha256": sha(run / "native_render.json"),
        "native_verify_sha256": sha(run / "native_verify.json"),
        "source_sprite_png_sha256": catalog["sprite"]["sha256"],
        "source_geometry_sha256": canonical_sha({key: geometry[key] for key in ("vertices", "uvs", "triangles", "rect", "pivot", "pixelsPerUnit")}),
        "obj_file": obj_path.name,
        "obj_sha256": sha(obj_path),
        "vertex_count": len(vertices), "uv_count": len(uvs), "triangle_count": len(faces),
        "unity_sprite_rect": geometry["rect"],
        "unity_sprite_pivot_pixels": geometry["pivot"],
        "unity_sprite_pixels_per_unit": geometry["pixelsPerUnit"],
        "coordinate_mapping": "Unity local (x,y,z) metres -> UE local (-x,z,y)*100 cm; FbxFactory OBJ input Y pre-reflected",
        "face_order": "Unity triangle (a,b,c) -> OBJ (c,b,a), matching established city SpriteMesh pipeline",
        "uv_mapping": "Unity Sprite.uv (u,v) written as OBJ vt (u,v); imported V orientation requires visual verification",
        "ue_local_bounds_min_cm": mins, "ue_local_bounds_max_cm": maxs,
        "project_ue_units_per_unity_unit": 500.0,
        "obj_ue_units_per_unity_unit": 100.0,
        "component_relative_transform_example": {
            "translation_cm": [-child_position["x"] * 500.0,
                               child_position["z"] * 500.0,
                               child_position["y"] * 500.0],
            "rotation_quaternion_xyzw": [-child_rotation["x"], child_rotation["z"],
                                         child_rotation["y"], child_rotation["w"]],
            "uniform_scale": runtime_scale_factor * child_scale["x"],
            "scope": "Source prefab child-local transform relative to its root; root/world city pose supplied separately",
        },
        "material_asset": "/Game/RokUnity/City/Academy/build_13_101_2/M_build_13_101_2_SourceRG",
        "mesh_asset": "/Game/RokUnity/City/Academy/build_13_101_2/" + MESH_NAME,
        "runtime_pose_status": "pending source runtime CreateCityBuilding transform reconciliation",
        "visual_acceptance_status": "pending isolated UE render versus Unity source day frame",
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest_path, obj_path


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=Path, required=True)
    args = parser.parse_args()
    manifest_file, obj_file = prepare(args.run)
    print("ACADEMY_SOURCE_MESH_PREPARED", manifest_file, obj_file)
