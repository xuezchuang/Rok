import os

import unreal


LEVEL_PATH = os.environ.get("ROK_INSPECT_LEVEL", "/Game/Maps/L_RokPrototype_Lit")


def main():
    unreal.EditorLevelLibrary.load_level(LEVEL_PATH)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()

    real_reference = []
    basic_shapes = []
    other_meshes = []
    for actor in actors:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component:
            continue
        mesh = component.get_editor_property("static_mesh")
        mesh_path = mesh.get_path_name() if mesh else "<none>"
        row = "{0} | {1} | {2}".format(actor.get_actor_label(), actor.get_folder_path(), mesh_path)
        if mesh_path.startswith("/Game/RokPrototype/Meshes/"):
            real_reference.append(row)
        elif mesh_path.startswith("/Engine/BasicShapes/"):
            basic_shapes.append(row)
        else:
            other_meshes.append(row)

    unreal.log("Rok mesh inspect level: {0}".format(LEVEL_PATH))
    unreal.log("Rok mesh inspect real reference count: {0}".format(len(real_reference)))
    for row in sorted(real_reference):
        unreal.log("REAL_REF {0}".format(row))

    unreal.log("Rok mesh inspect BasicShapes count: {0}".format(len(basic_shapes)))
    for row in sorted(basic_shapes):
        unreal.log("BASIC_SHAPE {0}".format(row))

    unreal.log("Rok mesh inspect other mesh count: {0}".format(len(other_meshes)))
    for row in sorted(other_meshes):
        unreal.log("OTHER_MESH {0}".format(row))


if __name__ == "__main__":
    main()
