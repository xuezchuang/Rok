import hashlib
import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
UNITY_CLIENT_ROOT = Path(r"D:\ueDemo\Rok-unity\client-unity")
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
SOURCE_MANIFEST_PATH = MIGRATION_ROOT / "unity_runtime_ground_obj_manifest.json"
OUTPUT_ROOT = MIGRATION_ROOT / "UnityRuntimeGroundObjCompensated"
OUTPUT_MANIFEST_PATH = MIGRATION_ROOT / "unity_runtime_ground_obj_compensated_manifest.json"
OUTPUT_VERIFY_PATH = MIGRATION_ROOT / "unity_runtime_ground_obj_compensated_verify.json"
SCHEMA = "rok.unity.runtime_ground_obj_compensated.v3"
SUPPORTED_SOURCE_SCHEMAS = {
    "rok.unity.runtime_ground_obj.v1",
    "rok.unity.runtime_ground_obj.v2",
}


def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def parse_obj(path):
    vertices = []
    passthrough = []
    faces = []
    for raw_line in Path(path).read_text(encoding="utf-8-sig").splitlines():
        line = raw_line.strip()
        if line.startswith("v "):
            values = line.split()
            if len(values) < 4:
                raise RuntimeError("Malformed OBJ vertex in {0}: {1}".format(path, line))
            vertices.append([float(values[1]), float(values[2]), float(values[3])])
            passthrough.append(None)
        else:
            passthrough.append(raw_line)
            if line.startswith("f "):
                face = line.split()[1:]
                if len(face) < 3:
                    raise RuntimeError("Malformed OBJ face in {0}: {1}".format(path, line))
                faces.append(face)
    if not vertices:
        raise RuntimeError("OBJ has no vertices: {0}".format(path))
    return vertices, passthrough, faces


def obj_vertex_index(token, vertex_count):
    raw_index = token.split("/", 1)[0]
    if not raw_index:
        raise RuntimeError("OBJ face token is missing a vertex index: {0}".format(token))
    index = int(raw_index)
    resolved = index - 1 if index > 0 else vertex_count + index
    if resolved < 0 or resolved >= vertex_count:
        raise RuntimeError("OBJ face vertex index is out of range: {0}".format(token))
    return resolved


def predicted_ue_face_metrics(obj_input_vertices, faces):
    imported_vertices = [[vertex[0], -vertex[1], vertex[2]] for vertex in obj_input_vertices]
    signed_z_values = []
    for face in faces:
        indices = [obj_vertex_index(token, len(imported_vertices)) for token in face]
        for offset in range(1, len(indices) - 1):
            a = imported_vertices[indices[0]]
            b = imported_vertices[indices[offset]]
            c = imported_vertices[indices[offset + 1]]
            ab = [b[axis] - a[axis] for axis in range(3)]
            ac = [c[axis] - a[axis] for axis in range(3)]
            # UE's front-facing triangle normal uses the left-handed winding
            # convention exposed by GeometryScript (AC x AB), not AB x AC.
            signed_z_values.append(ac[0] * ab[1] - ac[1] * ab[0])
    epsilon = 1.0e-8
    return {
        "predicted_ue_up_facing_triangle_count": sum(value > epsilon for value in signed_z_values),
        "predicted_ue_down_facing_triangle_count": sum(value < -epsilon for value in signed_z_values),
        "predicted_ue_degenerate_triangle_count": sum(abs(value) <= epsilon for value in signed_z_values),
        "predicted_ue_min_signed_normal_z": min(signed_z_values) if signed_z_values else 0.0,
        "predicted_ue_max_signed_normal_z": max(signed_z_values) if signed_z_values else 0.0,
        "triangle_count": len(signed_z_values),
    }


def tight_bounds(vertices):
    mins = [min(vertex[axis] for vertex in vertices) for axis in range(3)]
    maxs = [max(vertex[axis] for vertex in vertices) for axis in range(3)]
    return {
        "min": mins,
        "max": maxs,
        "center": [(mins[axis] + maxs[axis]) * 0.5 for axis in range(3)],
        "extents": [(maxs[axis] - mins[axis]) * 0.5 for axis in range(3)],
    }


def write_compensated_obj(source_path, output_path, source_schema):
    source_vertices, passthrough, source_faces = parse_obj(source_path)
    if source_schema == "rok.unity.runtime_ground_obj.v1":
        desired_ue_vertices = source_vertices
    else:
        desired_ue_vertices = [[vertex[0], -vertex[1], vertex[2]] for vertex in source_vertices]
    face_winding_compensation = "preserve_unity_exported_face_order"
    obj_input_vertices = [[vertex[0], -vertex[1], vertex[2]] for vertex in desired_ue_vertices]

    lines = [
        "# Unity runtime ground geometry world-baked in UE centimeters",
        "# UE 5.8 FbxFactory inverse-Y import compensation: OBJ input=(UE.x,-UE.y,UE.z)",
        "# Source Unity export: {0}".format(Path(source_path)),
    ]
    vertex_index = 0
    face_index = 0
    output_faces = []
    for original in passthrough:
        if original is None:
            vertex = obj_input_vertices[vertex_index]
            vertex_index += 1
            lines.append("v {0:.9f} {1:.9f} {2:.9f}".format(*vertex))
        elif not original.lstrip().startswith("#"):
            if original.strip().startswith("f "):
                face = source_faces[face_index]
                face_index += 1
                output_faces.append(face)
                lines.append("f " + " ".join(face))
            else:
                lines.append(original)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    metrics = predicted_ue_face_metrics(obj_input_vertices, output_faces)
    return {
        "desired_ue_vertices": desired_ue_vertices,
        "obj_input_vertices": obj_input_vertices,
        "desired_bounds": tight_bounds(desired_ue_vertices),
        "obj_input_bounds": tight_bounds(obj_input_vertices),
        "vertex_count": len(desired_ue_vertices),
        "triangle_count": metrics["triangle_count"],
        "obj_face_winding_compensation": face_winding_compensation,
        "predicted_ue_up_facing_triangle_count": metrics["predicted_ue_up_facing_triangle_count"],
        "predicted_ue_down_facing_triangle_count": metrics["predicted_ue_down_facing_triangle_count"],
        "predicted_ue_degenerate_triangle_count": metrics["predicted_ue_degenerate_triangle_count"],
        "predicted_ue_min_signed_normal_z": metrics["predicted_ue_min_signed_normal_z"],
        "predicted_ue_max_signed_normal_z": metrics["predicted_ue_max_signed_normal_z"],
    }


def source_fbx_path(record):
    explicit = record.get("source_fbx_disk_path", "")
    if explicit:
        return Path(explicit)
    asset_path = str(record.get("mesh_asset_path", "")).replace("/", "\\")
    return UNITY_CLIENT_ROOT / asset_path


def renderer_evidence(record, new_key, legacy_key):
    value = record.get(new_key)
    if value is not None:
        return value
    return record.get(legacy_key)


def build_manifest():
    if not SOURCE_MANIFEST_PATH.exists():
        raise RuntimeError("Missing Unity runtime ground OBJ manifest: {0}".format(SOURCE_MANIFEST_PATH))
    source_manifest = json.loads(SOURCE_MANIFEST_PATH.read_text(encoding="utf-8-sig"))
    source_schema = source_manifest.get("schema")
    if source_schema not in SUPPORTED_SOURCE_SCHEMAS:
        raise RuntimeError("Unsupported Unity runtime ground source schema: {0}".format(source_schema))
    source_records = source_manifest.get("records", [])
    if len(source_records) != 2:
        raise RuntimeError("Expected exactly two Unity runtime ground records, found {0}".format(len(source_records)))

    records = []
    for record in source_records:
        source_obj = Path(record.get("obj_path", ""))
        if not source_obj.exists():
            raise RuntimeError("Missing Unity-exported ground OBJ: {0}".format(source_obj))
        fbx_path = source_fbx_path(record)
        if not fbx_path.exists():
            raise RuntimeError("Missing Unity ground FBX: {0}".format(fbx_path))
        output_obj = OUTPUT_ROOT / source_obj.name
        geometry = write_compensated_obj(source_obj, output_obj, source_schema)
        desired_bounds = geometry["desired_bounds"]
        obj_input_bounds = geometry["obj_input_bounds"]
        desired_probe = geometry["desired_ue_vertices"][0]
        obj_input_probe = geometry["obj_input_vertices"][0]
        ground_id = record.get("ground_id") or "{0}|{1}".format(
            record.get("object_name", ""), record.get("mesh_stem", "")
        )

        output_record = dict(record)
        for obsolete_key in (
            "unity_bounds_center",
            "unity_bounds_extents",
            "ue_bounds_center",
            "ue_bounds_extents",
        ):
            output_record.pop(obsolete_key, None)
        output_record.update({
            "ground_id": ground_id,
            "source_unity_export_obj_path": str(source_obj),
            "source_unity_export_obj_sha256": sha256_file(source_obj),
            "obj_path": str(output_obj),
            "source_fbx_disk_path": str(fbx_path),
            "source_fbx_sha256": sha256_file(fbx_path),
            "source_obj_sha256": sha256_file(output_obj),
            "vertex_count": geometry["vertex_count"],
            "triangle_count": geometry["triangle_count"],
            "obj_face_winding_compensation": geometry["obj_face_winding_compensation"],
            "predicted_ue_up_facing_triangle_count": geometry["predicted_ue_up_facing_triangle_count"],
            "predicted_ue_down_facing_triangle_count": geometry["predicted_ue_down_facing_triangle_count"],
            "predicted_ue_degenerate_triangle_count": geometry["predicted_ue_degenerate_triangle_count"],
            "predicted_ue_min_signed_normal_z": geometry["predicted_ue_min_signed_normal_z"],
            "predicted_ue_max_signed_normal_z": geometry["predicted_ue_max_signed_normal_z"],
            "ue_world_vertex_bounds_center": desired_bounds["center"],
            "ue_world_vertex_bounds_extents": desired_bounds["extents"],
            "obj_input_vertex_bounds_center": obj_input_bounds["center"],
            "obj_input_vertex_bounds_extents": obj_input_bounds["extents"],
            "axis_probe_vertex_index": 0,
            "ue_axis_probe_vertex": desired_probe,
            "obj_input_axis_probe_vertex": obj_input_probe,
            "unity_renderer_bounds_center_evidence": renderer_evidence(
                record, "unity_renderer_bounds_center_evidence", "unity_bounds_center"
            ),
            "unity_renderer_bounds_extents_evidence": renderer_evidence(
                record, "unity_renderer_bounds_extents_evidence", "unity_bounds_extents"
            ),
            "ue_renderer_bounds_center_evidence": renderer_evidence(
                record, "ue_renderer_bounds_center_evidence", "ue_bounds_center"
            ),
            "ue_renderer_bounds_extents_evidence": renderer_evidence(
                record, "ue_renderer_bounds_extents_evidence", "ue_bounds_extents"
            ),
        })
        records.append(output_record)

    return {
        "schema": SCHEMA,
        "source_unity_export_manifest_path": str(SOURCE_MANIFEST_PATH),
        "source_unity_export_manifest_schema": source_schema,
        "source_unity_export_manifest_sha256": sha256_file(SOURCE_MANIFEST_PATH),
        "container_yaw": source_manifest.get("container_yaw"),
        "runtime_lod_scale": source_manifest.get("runtime_lod_scale"),
        "ue_coordinate_rule": "(-unity.x, unity.z, unity.y) * 500",
        "obj_import_axis_compensation": "FbxFactory OBJ inverse Y reflection: input=(UE.x,-UE.y,UE.z), imported=(UE.x,UE.y,UE.z)",
        "obj_face_winding_contract": "preserve exported faces for v1 and v2; Unity exporter already emits c,b,a source order and FbxFactory inverse-Y restores UE +Z front normals",
        "ue_actor_transform_mode": "world_baked_identity",
        "scene": source_manifest.get("scene"),
        "obj_root": str(OUTPUT_ROOT),
        "records": records,
    }


def main():
    manifest = build_manifest()
    OUTPUT_MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_MANIFEST_PATH.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    failures = []
    if len(manifest.get("records", [])) != 2:
        failures.append("expected exactly two compensated ground records")
    if any(record.get("vertex_count", 0) <= 0 for record in manifest.get("records", [])):
        failures.append("compensated ground records require non-empty geometry")
    for record in manifest.get("records", []):
        triangle_count = int(record.get("triangle_count", 0))
        if record.get("predicted_ue_up_facing_triangle_count") != triangle_count:
            failures.append("all imported triangles must face UE +Z for {0}".format(record.get("ground_id")))
        if record.get("predicted_ue_down_facing_triangle_count") != 0:
            failures.append("imported triangles face UE -Z for {0}".format(record.get("ground_id")))
        if record.get("predicted_ue_degenerate_triangle_count") != 0:
            failures.append("degenerate imported triangles found for {0}".format(record.get("ground_id")))
    verify = {
        "schema": "rok.unity.runtime_ground_obj_compensated_verify.v3",
        "manifest": str(OUTPUT_MANIFEST_PATH),
        "source_schema": manifest.get("source_unity_export_manifest_schema"),
        "record_count": len(manifest.get("records", [])),
        "actor_transform_mode": manifest.get("ue_actor_transform_mode"),
        "axis_compensation": manifest.get("obj_import_axis_compensation"),
        "face_winding_contract": manifest.get("obj_face_winding_contract"),
        "predicted_ue_up_facing_triangle_count": sum(
            int(record.get("predicted_ue_up_facing_triangle_count", 0)) for record in manifest.get("records", [])
        ),
        "predicted_ue_down_facing_triangle_count": sum(
            int(record.get("predicted_ue_down_facing_triangle_count", 0)) for record in manifest.get("records", [])
        ),
        "failures": failures,
    }
    OUTPUT_VERIFY_PATH.write_text(
        json.dumps(verify, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print("build_runtime_ground_obj_compensation records={0} failures={1}".format(
        verify["record_count"], len(failures)
    ))
    if failures:
        raise RuntimeError("; ".join(failures))


if __name__ == "__main__":
    main()
