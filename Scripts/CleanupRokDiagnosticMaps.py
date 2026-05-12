import unreal


KEEP_ASSETS = {
    "/Game/Maps/L_RokCityRuntime",
    "/Game/Maps/L_RokPrototype_Lit",
}

DELETE_ASSETS = [
    "/Game/Maps/L_RokCityArcherySingle",
    "/Game/Maps/L_RokCityBarracksSingle",
    "/Game/Maps/L_RokCityCluster501Age5",
    "/Game/Maps/L_RokCityClusterNamedAge5",
    "/Game/Maps/L_RokCityClusterNamedCiv6Age5",
    "/Game/Maps/L_RokCityClusterNamedCiv6Layered",
    "/Game/Maps/L_RokCityClusterNamedCiv6Raw",
    "/Game/Maps/L_RokCityClusterPreview",
    "/Game/Maps/L_RokCityLocalBlockCiv1",
    "/Game/Maps/L_RokCityLocalBlockCiv6",
    "/Game/Maps/L_RokCityMainBuilding",
    "/Game/Maps/L_RokCityMilitaryShowcase",
    "/Game/Maps/L_RokCitySiegeSingle",
    "/Game/Maps/L_RokCitySingleBuilding",
    "/Game/Maps/L_RokCityStableSingle",
]


def cleanup():
    deleted = []
    skipped = []
    for asset_path in DELETE_ASSETS:
        if asset_path in KEEP_ASSETS:
            skipped.append("{0}: keep-listed".format(asset_path))
            continue
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            skipped.append("{0}: missing".format(asset_path))
            continue
        if unreal.EditorAssetLibrary.delete_asset(asset_path):
            deleted.append(asset_path)
        else:
            skipped.append("{0}: delete failed".format(asset_path))
    unreal.log("Rok diagnostic map cleanup deleted: {0}".format(", ".join(deleted) if deleted else "none"))
    unreal.log("Rok diagnostic map cleanup skipped: {0}".format(", ".join(skipped) if skipped else "none"))


if __name__ == "__main__":
    cleanup()
