import json
import hashlib
import os
import sys

import unreal


PROJECT_ROOT = unreal.Paths.project_dir()
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "Scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.append(SCRIPTS_DIR)


TARGET_LEVEL = os.environ.get("ROK_UNITY_RUNTIME_GROUND_LEVEL", "/Game/Maps/L_RokUnityRuntimeGround")
VERIFY_OUTPUT = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_runtime_ground_verify.json")
SCENE_MANIFEST = os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_runtime_ground_scene_manifest.json")
GROUND_SOURCE_MANIFEST = os.path.join(
    PROJECT_ROOT, "Saved", "RokUnityMigration", "unity_runtime_ground_obj_compensated_manifest.json"
)
GROUND_SOURCE_SCHEMA = "rok.unity.runtime_ground_obj_compensated.v3"
CITY_TAG = "RokUnityRuntimeGround"
GROUND_BOUNDS_TOLERANCE = 0.1
GROUND_TRANSFORM_TOLERANCE = 0.05
GROUND_SCALE_TOLERANCE = 0.001


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


def has_tag(actor, tag):
    return tag in actor_tags(actor)


def vector_distance(a, b):
    return ((float(a.x) - float(b[0])) ** 2 + (float(a.y) - float(b[1])) ** 2 + (float(a.z) - float(b[2])) ** 2) ** 0.5


def vector_components(value):
    if hasattr(value, "x"):
        return [float(value.x), float(value.y), float(value.z)]
    return [float(value[0]), float(value[1]), float(value[2])]


def vector_max_error(actual, expected):
    actual_values = vector_components(actual)
    expected_values = vector_components(expected)
    return max(abs(actual_values[index] - expected_values[index]) for index in range(3))


def file_sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def load_ground_source_manifest():
    if not os.path.exists(GROUND_SOURCE_MANIFEST):
        raise RuntimeError("Missing Unity runtime ground source manifest: {0}".format(GROUND_SOURCE_MANIFEST))
    with open(GROUND_SOURCE_MANIFEST, "r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def read_obj_geometry(path):
    vertices = []
    faces = []
    with open(path, "r", encoding="utf-8-sig") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if line.startswith("v "):
                values = line.split()
                vertices.append([float(values[1]), float(values[2]), float(values[3])])
            elif line.startswith("f "):
                face = line.split()[1:]
                if len(face) < 3:
                    raise RuntimeError("Malformed ground OBJ face: {0}".format(line))
                faces.append(face)
    if not vertices:
        raise RuntimeError("Ground OBJ has no vertices: {0}".format(path))
    return vertices, faces


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
            # Match the UE left-handed front-normal convention exposed by
            # GeometryScript_MeshQueries.get_triangle_face_normal (AC x AB).
            signed_z_values.append(ac[0] * ab[1] - ac[1] * ab[0])
    epsilon = 1.0e-8
    return {
        "triangle_count": len(signed_z_values),
        "up": sum(value > epsilon for value in signed_z_values),
        "down": sum(value < -epsilon for value in signed_z_values),
        "degenerate": sum(abs(value) <= epsilon for value in signed_z_values),
        "min_signed_normal_z": min(signed_z_values) if signed_z_values else 0.0,
        "max_signed_normal_z": max(signed_z_values) if signed_z_values else 0.0,
    }


def tight_bounds(vertices):
    mins = [min(vertex[axis] for vertex in vertices) for axis in range(3)]
    maxs = [max(vertex[axis] for vertex in vertices) for axis in range(3)]
    return (
        [(mins[axis] + maxs[axis]) * 0.5 for axis in range(3)],
        [(maxs[axis] - mins[axis]) * 0.5 for axis in range(3)],
    )


def imported_static_mesh_face_metrics(static_mesh):
    dynamic_mesh = unreal.DynamicMesh()
    dynamic_mesh, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        static_mesh,
        dynamic_mesh,
        unreal.GeometryScriptCopyMeshFromAssetOptions(),
        unreal.GeometryScriptMeshReadLOD(),
    )
    _, triangle_ids, _ = unreal.GeometryScript_MeshQueries.get_all_triangle_i_ds(dynamic_mesh)
    normal_z_values = []
    for triangle_id in triangle_ids.convert_index_list_to_array():
        normal, is_valid = unreal.GeometryScript_MeshQueries.get_triangle_face_normal(dynamic_mesh, triangle_id)
        if is_valid:
            normal_z_values.append(float(normal.z))
    epsilon = 1.0e-6
    return {
        "triangle_count": len(normal_z_values),
        "up": sum(value > epsilon for value in normal_z_values),
        "down": sum(value < -epsilon for value in normal_z_values),
        "degenerate": sum(abs(value) <= epsilon for value in normal_z_values),
        "min_normal_z": min(normal_z_values) if normal_z_values else 0.0,
        "max_normal_z": max(normal_z_values) if normal_z_values else 0.0,
    }


def verify_ground_source_contract(source_manifest, scene_records, actors, failures):
    result = {
        "source_record_count": len(source_manifest.get("records", [])),
        "scene_record_count": len(scene_records),
        "actor_count": len(actors),
        "identity_count": 0,
        "axis_compensated_count": 0,
        "face_winding_compensated_count": 0,
        "one_sided_material_count": 0,
        "predicted_ue_up_facing_triangle_count": 0,
        "predicted_ue_down_facing_triangle_count": 0,
        "predicted_ue_degenerate_triangle_count": 0,
        "imported_ue_up_facing_triangle_count": 0,
        "imported_ue_down_facing_triangle_count": 0,
        "imported_ue_degenerate_triangle_count": 0,
        "max_bounds_center_error": 0.0,
        "max_bounds_extent_error": 0.0,
        "max_axis_probe_error": 0.0,
        "max_obj_input_bounds_error": 0.0,
    }
    if source_manifest.get("schema") != GROUND_SOURCE_SCHEMA:
        failures.append("unexpected Unity ground source schema: {0}".format(source_manifest.get("schema")))
    if source_manifest.get("ue_actor_transform_mode") != "world_baked_identity":
        failures.append("Unity ground source must require world-baked identity actors")
    if "inverse Y reflection" not in str(source_manifest.get("obj_import_axis_compensation", "")):
        failures.append("Unity ground source is missing FbxFactory inverse-Y compensation evidence")
    if "preserve exported faces" not in str(source_manifest.get("obj_face_winding_contract", "")):
        failures.append("Unity ground source is missing face-winding compensation evidence")

    source_records = source_manifest.get("records", [])
    source_by_id = {record.get("ground_id"): record for record in source_records}
    scene_by_id = {record.get("ground_id"): record for record in scene_records}
    actors_by_label = {actor_label(actor): actor for actor in actors}
    if len(source_records) != 2 or len(scene_records) != 2 or len(actors) != 2:
        failures.append("expected exactly two Unity ground records/actors: source={0}, scene={1}, actors={2}".format(
            len(source_records), len(scene_records), len(actors)
        ))
    if len(source_by_id) != len(source_records) or None in source_by_id:
        failures.append("Unity ground source contains missing or duplicate ground_id values")
    if len(scene_by_id) != len(scene_records) or None in scene_by_id:
        failures.append("runtime scene manifest contains missing or duplicate ground_id values")
    missing_ids = sorted(str(value) for value in set(source_by_id) - set(scene_by_id))
    orphan_ids = sorted(str(value) for value in set(scene_by_id) - set(source_by_id))
    if missing_ids:
        failures.append("runtime scene is missing Unity ground IDs: {0}".format(missing_ids))
    if orphan_ids:
        failures.append("runtime scene contains orphan ground IDs: {0}".format(orphan_ids))

    for ground_id, source in source_by_id.items():
        if ground_id is None:
            continue
        scene_record = scene_by_id.get(ground_id)
        if not scene_record:
            continue
        actor_name = scene_record.get("actor", "")
        actor = actors_by_label.get(actor_name)
        if not actor:
            failures.append("runtime ground actor is missing from map: {0}".format(actor_name))
            continue

        obj_path = source.get("obj_path", "")
        source_fbx = source.get("source_fbx_disk_path", "")
        for path, expected_hash, label in [
            (obj_path, source.get("source_obj_sha256", ""), "OBJ"),
            (source_fbx, source.get("source_fbx_sha256", ""), "FBX"),
        ]:
            if not path or not os.path.exists(path):
                failures.append("Unity ground {0} source is missing for {1}: {2}".format(label, ground_id, path))
            elif not expected_hash or file_sha256(path) != expected_hash:
                failures.append("Unity ground {0} hash drift for {1}: {2}".format(label, ground_id, path))

        try:
            obj_vertices, obj_faces = read_obj_geometry(obj_path)
            face_metrics = predicted_ue_face_metrics(obj_vertices, obj_faces)
            obj_triangle_count = face_metrics["triangle_count"]
            result["predicted_ue_up_facing_triangle_count"] += face_metrics["up"]
            result["predicted_ue_down_facing_triangle_count"] += face_metrics["down"]
            result["predicted_ue_degenerate_triangle_count"] += face_metrics["degenerate"]
            obj_center, obj_extent = tight_bounds(obj_vertices)
            obj_bounds_error = max(
                vector_max_error(obj_center, source.get("obj_input_vertex_bounds_center", [0.0, 0.0, 0.0])),
                vector_max_error(obj_extent, source.get("obj_input_vertex_bounds_extents", [0.0, 0.0, 0.0])),
            )
            result["max_obj_input_bounds_error"] = max(result["max_obj_input_bounds_error"], obj_bounds_error)
            if obj_bounds_error > GROUND_BOUNDS_TOLERANCE:
                failures.append("ground OBJ input tight bounds mismatch for {0}: max error {1:.6f} cm".format(
                    ground_id, obj_bounds_error
                ))
            if len(obj_vertices) != int(source.get("vertex_count", -1)):
                failures.append("ground OBJ vertex count mismatch for {0}: expected {1}, got {2}".format(
                    ground_id, source.get("vertex_count"), len(obj_vertices)
                ))
            if obj_triangle_count != int(source.get("triangle_count", -1)):
                failures.append("ground OBJ triangle count mismatch for {0}: expected {1}, got {2}".format(
                    ground_id, source.get("triangle_count"), obj_triangle_count
                ))
            if face_metrics["up"] != obj_triangle_count or face_metrics["down"] != 0 or face_metrics["degenerate"] != 0:
                failures.append("ground OBJ predicted UE winding must face +Z for {0}: up={1}, down={2}, degenerate={3}".format(
                    ground_id, face_metrics["up"], face_metrics["down"], face_metrics["degenerate"]
                ))
            for key, actual in (
                ("predicted_ue_up_facing_triangle_count", face_metrics["up"]),
                ("predicted_ue_down_facing_triangle_count", face_metrics["down"]),
                ("predicted_ue_degenerate_triangle_count", face_metrics["degenerate"]),
            ):
                if int(source.get(key, -1)) != actual:
                    failures.append("ground OBJ/source {0} mismatch for {1}".format(key, ground_id))
            probe_index = int(source.get("axis_probe_vertex_index", -1))
            if probe_index < 0 or probe_index >= len(obj_vertices):
                failures.append("ground OBJ axis probe index is invalid for {0}: {1}".format(ground_id, probe_index))
            else:
                obj_probe = obj_vertices[probe_index]
                expected_obj_probe = source.get("obj_input_axis_probe_vertex", [0.0, 0.0, 0.0])
                desired_probe = source.get("ue_axis_probe_vertex", [0.0, 0.0, 0.0])
                imported_probe = [obj_probe[0], -obj_probe[1], obj_probe[2]]
                probe_error = max(
                    vector_max_error(obj_probe, expected_obj_probe),
                    vector_max_error(imported_probe, desired_probe),
                )
                result["max_axis_probe_error"] = max(result["max_axis_probe_error"], probe_error)
                if probe_error > GROUND_BOUNDS_TOLERANCE:
                    failures.append("ground inverse-Y axis probe mismatch for {0}: max error {1:.6f} cm".format(
                        ground_id, probe_error
                    ))
        except Exception as exception:
            failures.append("could not verify ground OBJ geometry for {0}: {1}".format(ground_id, exception))

        for key in (
            "source_fbx_sha256",
            "source_obj_sha256",
            "vertex_count",
            "triangle_count",
            "obj_face_winding_compensation",
            "predicted_ue_up_facing_triangle_count",
            "predicted_ue_down_facing_triangle_count",
            "predicted_ue_degenerate_triangle_count",
        ):
            if scene_record.get(key) != source.get(key):
                failures.append("ground scene/source {0} mismatch for {1}".format(key, ground_id))
        for scene_key, source_key in [
            ("ue_expected_bounds_center", "ue_world_vertex_bounds_center"),
            ("ue_expected_bounds_extents", "ue_world_vertex_bounds_extents"),
            ("ue_axis_probe_vertex", "ue_axis_probe_vertex"),
            ("obj_input_axis_probe_vertex", "obj_input_axis_probe_vertex"),
        ]:
            if vector_max_error(scene_record.get(scene_key, [0.0, 0.0, 0.0]), source.get(source_key, [0.0, 0.0, 0.0])) > 1.0e-5:
                failures.append("ground scene/source {0} mismatch for {1}".format(scene_key, ground_id))

        tags = actor_tags(actor)
        location = actor.get_actor_location()
        rotation = actor.get_actor_rotation()
        scale = actor.get_actor_scale3d()
        identity = (
            max(abs(float(value)) for value in (location.x, location.y, location.z, rotation.pitch, rotation.yaw, rotation.roll)) <= GROUND_TRANSFORM_TOLERANCE
            and max(abs(float(value) - 1.0) for value in (scale.x, scale.y, scale.z)) <= GROUND_SCALE_TOLERANCE
        )
        if identity:
            result["identity_count"] += 1
        else:
            failures.append("ground world-baked actor must keep identity transform: {0}".format(actor_name))
        required_tags = (
            "UnityWorldBakedActorIdentity",
            "UnityBakedOrientationScaleIdentity",
            "UnityObjImportAxisCompensation_FbxFactoryInverseY",
            "UnityObjImportFaceWindingCompensation_Preserved",
            "UnityShaderCull_BackDefault",
            "UnityRendererBoundsEvidenceOnly",
        )
        missing_tags = [tag for tag in required_tags if tag not in tags]
        if missing_tags:
            failures.append("ground actor {0} is missing source evidence tags: {1}".format(actor_name, missing_tags))
        else:
            result["axis_compensated_count"] += 1
            result["face_winding_compensated_count"] += 1
        if "UnityRendererBoundsCenterAligned" in tags or "UnityRendererBoundsBaked" in tags:
            failures.append("ground actor still uses obsolete Renderer.bounds alignment evidence: {0}".format(actor_name))
        if actor.get_class().get_name() != "StaticMeshActor":
            failures.append("ground world-baked actor must be a StaticMeshActor: {0}".format(actor_name))
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component or not component.get_editor_property("static_mesh"):
            failures.append("ground actor is missing its imported StaticMesh: {0}".format(actor_name))
        elif component.get_material(0) is None:
            failures.append("ground actor is missing material slot 0: {0}".format(actor_name))
        else:
            material = component.get_material(0)
            try:
                if bool(material.get_editor_property("two_sided")):
                    failures.append("Unity ground material must use default back-face culling: {0}".format(actor_name))
                else:
                    result["one_sided_material_count"] += 1
            except Exception as exception:
                failures.append("could not verify Unity ground material culling for {0}: {1}".format(
                    actor_name, exception
                ))
        if component and component.get_editor_property("static_mesh"):
            try:
                imported_metrics = imported_static_mesh_face_metrics(component.get_editor_property("static_mesh"))
                result["imported_ue_up_facing_triangle_count"] += imported_metrics["up"]
                result["imported_ue_down_facing_triangle_count"] += imported_metrics["down"]
                result["imported_ue_degenerate_triangle_count"] += imported_metrics["degenerate"]
                expected_triangles = int(source.get("triangle_count", -1))
                if (
                    imported_metrics["triangle_count"] != expected_triangles
                    or imported_metrics["up"] != expected_triangles
                    or imported_metrics["down"] != 0
                    or imported_metrics["degenerate"] != 0
                ):
                    failures.append(
                        "imported UE ground mesh normals must face +Z for {0}: triangles={1}, up={2}, down={3}, degenerate={4}".format(
                            actor_name,
                            imported_metrics["triangle_count"],
                            imported_metrics["up"],
                            imported_metrics["down"],
                            imported_metrics["degenerate"],
                        )
                    )
            except Exception as exception:
                failures.append("could not verify imported UE ground mesh normals for {0}: {1}".format(
                    actor_name, exception
                ))

        expected_center = source.get("ue_world_vertex_bounds_center", [0.0, 0.0, 0.0])
        expected_extent = source.get("ue_world_vertex_bounds_extents", [0.0, 0.0, 0.0])
        bounds_origin, bounds_extent = actor.get_actor_bounds(False)
        center_error = vector_max_error(bounds_origin, expected_center)
        extent_error = vector_max_error(bounds_extent, expected_extent)
        result["max_bounds_center_error"] = max(result["max_bounds_center_error"], center_error)
        result["max_bounds_extent_error"] = max(result["max_bounds_extent_error"], extent_error)
        if center_error > GROUND_BOUNDS_TOLERANCE:
            failures.append("ground tight bounds center mismatch for {0}: max error {1:.6f} cm".format(
                actor_name, center_error
            ))
        if extent_error > GROUND_BOUNDS_TOLERANCE:
            failures.append("ground tight bounds extent mismatch for {0}: max error {1:.6f} cm".format(
                actor_name, extent_error
            ))
    return result


def load_scene_manifest():
    if not os.path.exists(SCENE_MANIFEST):
        return {}
    with open(SCENE_MANIFEST, "r", encoding="utf-8") as handle:
        return json.load(handle)


def tags_with_prefix(actors, prefix):
    return [tag for actor in actors for tag in actor_tags(actor) if tag.startswith(prefix)]


def count_boundary_materials(boundary):
    material_counts = {}
    for tag in tags_with_prefix(boundary, "UnityMaterial_"):
        key = tag[len("UnityMaterial_"):]
        material_counts[key] = material_counts.get(key, 0) + 1
    return material_counts


def verify_manifest_bounds(records, actors, failures, label):
    by_label = {actor_label(actor): actor for actor in actors}
    max_error = 0.0
    for record in records:
        actor_name = record.get("actor", "")
        actor = by_label.get(actor_name)
        if not actor:
            failures.append("manifest {0} actor is missing from map: {1}".format(label, actor_name))
            continue
        expected = record.get("ue_expected_bounds_center")
        if not expected:
            continue
        bounds_origin, _bounds_extent = actor.get_actor_bounds(False)
        distance = vector_distance(bounds_origin, expected)
        max_error = max(max_error, distance)
        if distance > 0.5:
            failures.append("{0} visible bounds center mismatch for {1}: expected ({2:.2f},{3:.2f},{4:.2f}), got ({5:.2f},{6:.2f},{7:.2f})".format(
                label,
                actor_name,
                float(expected[0]),
                float(expected[1]),
                float(expected[2]),
                float(bounds_origin.x),
                float(bounds_origin.y),
                float(bounds_origin.z),
            ))
    return max_error


def verify_scene():
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_LEVEL):
        raise RuntimeError("Missing map: {0}".format(TARGET_LEVEL))

    manifest = load_scene_manifest()
    source_manifest = load_ground_source_manifest()
    unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    tagged = [actor for actor in actors if has_tag(actor, CITY_TAG)]
    boundary = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeBoundarySource")]
    ground = [actor for actor in tagged if has_tag(actor, "RokUnityRuntimeGroundSource")]
    buildings = [actor for actor in tagged if has_tag(actor, "RokUnityFreshInitBuilding") or "Building" in actor_folder(actor)]
    roads = [actor for actor in tagged if has_tag(actor, "RokUnityRoad") or "Road" in actor_folder(actor)]
    asset_review = [actor for actor in tagged if has_tag(actor, "RokUnityFreshAssetReview") or has_tag(actor, "UnityBoundaryAssetReview")]
    cameras = [actor for actor in actors if actor.get_class().get_name() == "CameraActor"]
    player_starts = [actor for actor in actors if actor.get_class().get_name() == "PlayerStart"]

    boundary_tags = [tag for actor in boundary for tag in actor_tags(actor)]
    ground_tags = [tag for actor in ground for tag in actor_tags(actor)]
    has_wall = any("UnityMesh_CityWall_101_1" in tag for tag in boundary_tags)
    has_gate = any("UnityMesh_CityGate_101_1" in tag for tag in boundary_tags)
    tower_actors = [
        actor for actor in boundary
        if any(tag.startswith("UnityMesh_CityTower_101_1_") for tag in actor_tags(actor))
    ]
    tower_mesh_tags = sorted(set(tag for tag in boundary_tags if tag.startswith("UnityMesh_CityTower_101_1_")))
    has_city_surface = "UnityMesh_CitySurface_101_1" in ground_tags
    has_city_translucent = "UnityMesh_CityTranslucent_101_1" in ground_tags
    has_city_surface_material = "UnityMaterial_CitySurface_101_1_mat" in ground_tags
    has_city_translucent_material = "UnityMaterial_CityTranslucent_101_1_mat" in ground_tags
    has_baked_texture = any("UnityTextureSource_baked_texture_path" in tag for tag in ground_tags)
    city_translucent_actors = [
        actor for actor in ground if "UnityMesh_CityTranslucent_101_1" in actor_tags(actor)
    ]
    has_vertex_alpha_texture = any(
        "UnityTextureSource_vertex_alpha_baked_texture_path" in tag for tag in ground_tags
    )
    has_vertex_alpha_evidence = any("UnityGroundVertexAlphaBaked" in tag for tag in ground_tags)
    has_vertex_alpha_mapping = any(
        "UnityGroundVertexAlphaMapping_ByPolygonVertex_IndexToDirect" in tag for tag in ground_tags
    )
    city_translucent_sort_priority = None
    if len(city_translucent_actors) == 1:
        component = city_translucent_actors[0].get_component_by_class(unreal.StaticMeshComponent)
        if component:
            city_translucent_sort_priority = int(component.get_editor_property("translucency_sort_priority"))
    material_counts = count_boundary_materials(boundary)

    failures = []
    if len(boundary) != 10:
        failures.append("expected exactly 10 runtime boundary actors, found {0}".format(len(boundary)))
    if len(ground) != 2:
        failures.append("expected exactly 2 runtime ground actors, found {0}".format(len(ground)))
    if len(tagged) != len(boundary) + len(ground):
        failures.append("runtime ground map should have only boundary and ground tagged actors: tagged={0}, boundary={1}, ground={2}".format(
            len(tagged),
            len(boundary),
            len(ground),
        ))
    if buildings:
        failures.append("building actors are not allowed in runtime ground map: {0}".format(len(buildings)))
    if roads:
        failures.append("road actors are not allowed in runtime ground map: {0}".format(len(roads)))
    if asset_review:
        failures.append("asset-review actors are not allowed in runtime ground map: {0}".format(len(asset_review)))
    if cameras:
        failures.append("camera actors are not allowed in runtime ground map: {0}".format(len(cameras)))
    if player_starts:
        failures.append("player starts are not allowed in runtime ground map: {0}".format(len(player_starts)))
    if not has_wall:
        failures.append("missing CityWall_101_1 runtime actor")
    if not has_gate:
        failures.append("missing CityGate_101_1 runtime actor")
    if len(tower_actors) != 8:
        failures.append("expected 8 Unity runtime tower actors from build_8_101_1, found {0}".format(len(tower_actors)))
    for material_key, expected_count in {"wall": 1, "gate": 1, "tower": 5, "tower_2": 3}.items():
        actual_count = material_counts.get(material_key, 0)
        if actual_count != expected_count:
            failures.append("expected {0} actors with UnityMaterial_{1}, found {2}".format(expected_count, material_key, actual_count))
    if not has_city_surface:
        failures.append("missing CitySurface_101_1 runtime ground actor")
    if not has_city_translucent:
        failures.append("missing CityTranslucent_101_1 runtime ground actor")
    if not has_city_surface_material:
        failures.append("missing CitySurface_101_1_mat material tag")
    if not has_city_translucent_material:
        failures.append("missing CityTranslucent_101_1_mat material tag")
    if not has_baked_texture:
        failures.append("runtime ground actors should use Unity shader-baked texture output")
    if len(city_translucent_actors) != 1:
        failures.append("expected exactly one CityTranslucent_101_1 actor, found {0}".format(
            len(city_translucent_actors)
        ))
    if not has_vertex_alpha_texture:
        failures.append("CityTranslucent_101_1 must use the FBX vertex-alpha baked texture")
    if not has_vertex_alpha_evidence or not has_vertex_alpha_mapping:
        failures.append("CityTranslucent_101_1 is missing strict FBX ByPolygonVertex alpha evidence tags")
    if city_translucent_sort_priority != 2991:
        failures.append("CityTranslucent_101_1 translucency sort priority must be Unity queue 2991, found {0}".format(
            city_translucent_sort_priority
        ))

    if manifest.get("schema") != "rok.unity_runtime_ground_scene.v2":
        failures.append("unexpected runtime ground scene manifest schema: {0}".format(manifest.get("schema")))
    if os.path.normcase(os.path.abspath(manifest.get("unity_runtime_ground_obj_manifest", ""))) != os.path.normcase(os.path.abspath(GROUND_SOURCE_MANIFEST)):
        failures.append("runtime ground scene manifest references the wrong source manifest")
    ground_contract = verify_ground_source_contract(
        source_manifest,
        manifest.get("ground_actors", []),
        ground,
        failures,
    )

    max_boundary_bounds_error = verify_manifest_bounds(manifest.get("boundary_actors", []), boundary, failures, "boundary")
    max_ground_bounds_error = verify_manifest_bounds(manifest.get("ground_actors", []), ground, failures, "ground")

    folders = {}
    for actor in tagged:
        folder = actor_folder(actor)
        folders[folder] = folders.get(folder, 0) + 1

    summary = {
        "target_level": TARGET_LEVEL,
        "actors": len(actors),
        "tagged": len(tagged),
        "boundary": len(boundary),
        "ground": len(ground),
        "buildings": len(buildings),
        "roads": len(roads),
        "asset_review": len(asset_review),
        "cameras": len(cameras),
        "player_starts": len(player_starts),
        "has_wall": has_wall,
        "has_gate": has_gate,
        "tower_actor_count": len(tower_actors),
        "tower_unique_mesh_count": len(tower_mesh_tags),
        "has_city_surface": has_city_surface,
        "has_city_translucent": has_city_translucent,
        "has_city_surface_material": has_city_surface_material,
        "has_city_translucent_material": has_city_translucent_material,
        "has_baked_texture": has_baked_texture,
        "has_vertex_alpha_texture": has_vertex_alpha_texture,
        "has_vertex_alpha_evidence": has_vertex_alpha_evidence,
        "has_vertex_alpha_mapping": has_vertex_alpha_mapping,
        "city_translucent_sort_priority": city_translucent_sort_priority,
        "material_counts": material_counts,
        "max_boundary_bounds_center_error": max_boundary_bounds_error,
        "max_ground_bounds_center_error": max_ground_bounds_error,
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
        "ground_max_obj_input_bounds_error": ground_contract["max_obj_input_bounds_error"],
        "folders": folders,
        "failures": failures,
    }
    os.makedirs(os.path.dirname(VERIFY_OUTPUT), exist_ok=True)
    with open(VERIFY_OUTPUT, "w", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)

    line = "UnityRuntimeGroundVerify actors={actors}, tagged={tagged}, boundary={boundary}, ground={ground}, towers={tower_actor_count}, failures={failure_count}".format(
        failure_count=len(failures),
        **summary
    )
    print(line)
    unreal.log(line)
    if failures:
        raise RuntimeError("; ".join(failures))


if __name__ == "__main__":
    verify_scene()
