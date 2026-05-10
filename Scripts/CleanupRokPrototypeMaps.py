import unreal


KEEP_MAP = "/Game/Maps/L_RokPrototype_Lit"
DELETE_MAPS = (
    "/Game/Maps/L_RokPrototype",
    "/Game/Maps/L_RokPrototype_RefMeshes",
    "/Game/Maps/L_RokPrototype_RefMeshes_Baseline",
)


def main():
    project_dir = unreal.Paths.project_dir()
    unreal.log("Rok map cleanup: project_dir={0}".format(project_dir))
    if not project_dir.replace("\\", "/").rstrip("/").endswith("/Rok"):
        raise RuntimeError("Refusing to cleanup maps outside Rok project: {0}".format(project_dir))

    if not unreal.EditorAssetLibrary.does_asset_exist(KEEP_MAP):
        raise RuntimeError("Keep map is missing: {0}".format(KEEP_MAP))

    deleted = []
    skipped = []
    for map_path in DELETE_MAPS:
        if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
            skipped.append(map_path)
            continue
        if not unreal.EditorAssetLibrary.delete_asset(map_path):
            raise RuntimeError("Failed to delete duplicate map: {0}".format(map_path))
        deleted.append(map_path)

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log("Rok map cleanup complete: kept={0}, deleted={1}, skipped={2}".format(
        KEEP_MAP,
        ", ".join(deleted) if deleted else "<none>",
        ", ".join(skipped) if skipped else "<none>",
    ))


if __name__ == "__main__":
    main()
