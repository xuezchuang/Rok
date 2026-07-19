import json
from pathlib import Path

import PrepareRokUnityRuntimeGroundVertexAlpha as prepare


def main():
    failures = []
    try:
        expected_manifest, expected_png = prepare.expected_manifest()
    except Exception as exception:
        expected_manifest = {"schema": prepare.SCHEMA, "records": []}
        expected_png = b""
        failures.append("could not rebuild expected vertex-alpha evidence: {0}".format(exception))

    if not prepare.OUTPUT_MANIFEST_PATH.exists():
        actual_manifest = {}
        failures.append("missing vertex-alpha manifest: {0}".format(prepare.OUTPUT_MANIFEST_PATH))
    else:
        actual_manifest = json.loads(prepare.OUTPUT_MANIFEST_PATH.read_text(encoding="utf-8-sig"))
        if actual_manifest != expected_manifest:
            failures.append("vertex-alpha manifest does not match the current FBX/OBJ/texture sources")

    expected_records = expected_manifest.get("records", [])
    if len(expected_records) == 1:
        output_path = Path(expected_records[0].get("output_texture_path", ""))
        if not output_path.exists():
            failures.append("missing generated vertex-alpha texture: {0}".format(output_path))
        elif output_path.read_bytes() != expected_png:
            failures.append("generated vertex-alpha PNG bytes do not match current source evidence")

    summary = {
        "schema": "rok.unity.runtime_ground_vertex_alpha_verify.v1",
        "source_manifest": str(prepare.OUTPUT_MANIFEST_PATH),
        "record_count": len(actual_manifest.get("records", [])) if isinstance(actual_manifest, dict) else 0,
        "mesh_stem": expected_records[0].get("mesh_stem") if expected_records else "",
        "fbx_polygon_vertex_count": expected_records[0].get("fbx_polygon_vertex_count") if expected_records else 0,
        "obj_triangle_count": expected_records[0].get("obj_triangle_count") if expected_records else 0,
        "output_alpha_extrema": expected_records[0].get("output_alpha_extrema") if expected_records else [],
        "failure_count": len(failures),
        "failures": failures,
    }
    prepare.OUTPUT_VERIFY_PATH.parent.mkdir(parents=True, exist_ok=True)
    prepare.OUTPUT_VERIFY_PATH.write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(
        "verify_runtime_ground_vertex_alpha records={0} corners={1} triangles={2} failures={3}".format(
            summary["record_count"],
            summary["fbx_polygon_vertex_count"],
            summary["obj_triangle_count"],
            summary["failure_count"],
        )
    )
    if failures:
        raise RuntimeError("; ".join(failures))


if __name__ == "__main__":
    main()
