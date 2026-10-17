import pathlib

import unreal


EXPECTED_PROJECT_ROOT = pathlib.Path(r"D:\ueDemo\Rok").resolve()
PRODUCTION_MAPS = {
    "/Game/Maps/L_RokUnityGameAppCity",
    "/Game/Maps/L_RokUnityWorldMapPrototype",
}
DIAGNOSTIC_ROOT = "/Game/RokDiagnostics/Maps"


def package_path(asset_path):
    return str(asset_path).split(".", 1)[0]


def main():
    project_root = pathlib.Path(unreal.Paths.project_dir()).resolve()
    if project_root != EXPECTED_PROJECT_ROOT:
        raise RuntimeError(
            "Wrong Unreal project: expected {0}, got {1}".format(
                EXPECTED_PROJECT_ROOT,
                project_root,
            )
        )

    world = unreal.EditorLevelLibrary.get_editor_world()
    world_name = world.get_name() if world else "<none>"
    unreal.log(
        "Rok map organization identity: project={0} world={1}".format(
            project_root,
            world_name,
        )
    )

    source_assets = sorted(
        {
            package_path(asset_path)
            for asset_path in unreal.EditorAssetLibrary.list_assets(
                "/Game/Maps",
                recursive=False,
                include_folder=False,
            )
        }
    )
    source_maps = [
        asset_path
        for asset_path in source_assets
        if unreal.EditorAssetLibrary.find_asset_data(asset_path).asset_class_path.asset_name
        == "World"
    ]

    missing_production = sorted(PRODUCTION_MAPS.difference(source_maps))
    if missing_production:
        raise RuntimeError(
            "Missing production maps: {0}".format(", ".join(missing_production))
        )

    moved = []
    for source_path in source_maps:
        if source_path in PRODUCTION_MAPS:
            continue
        destination_path = "{0}/{1}".format(
            DIAGNOSTIC_ROOT,
            source_path.rsplit("/", 1)[-1],
        )
        if unreal.EditorAssetLibrary.does_asset_exist(destination_path):
            raise RuntimeError(
                "Refusing to overwrite existing diagnostic map: {0}".format(
                    destination_path
                )
            )
        if not unreal.EditorAssetLibrary.rename_asset(
            source_path,
            destination_path,
        ):
            raise RuntimeError(
                "Failed to move {0} to {1}".format(
                    source_path,
                    destination_path,
                )
            )
        moved.append((source_path, destination_path))

    if not unreal.EditorAssetLibrary.save_directory(
        DIAGNOSTIC_ROOT,
        only_if_is_dirty=True,
        recursive=True,
    ):
        raise RuntimeError("Failed to save diagnostic map directory")

    unreal.log(
        "Rok map organization complete: production={0} diagnostics_moved={1}".format(
            len(PRODUCTION_MAPS),
            len(moved),
        )
    )
    for source_path, destination_path in moved:
        unreal.log("  {0} -> {1}".format(source_path, destination_path))


if __name__ == "__main__":
    main()
