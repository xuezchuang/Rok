import unreal


GENERATED_DIRECTORIES = [
    "/Game/RokPrototype/Textures/RuntimeCity",
    "/Game/RokPrototype/Materials/RuntimeCity",
]

GENERATED_ASSETS = [
    "/Game/Maps/L_RokCityRuntime",
]


def delete_asset(asset_path):
    if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return "missing"
    return "deleted" if unreal.EditorAssetLibrary.delete_asset(asset_path) else "delete failed"


def delete_directory(directory_path):
    if not unreal.EditorAssetLibrary.does_directory_exist(directory_path):
        return "missing"
    return "deleted" if unreal.EditorAssetLibrary.delete_directory(directory_path) else "delete failed"


def cleanup():
    asset_results = ["{0}: {1}".format(path, delete_asset(path)) for path in GENERATED_ASSETS]
    directory_results = ["{0}: {1}".format(path, delete_directory(path)) for path in GENERATED_DIRECTORIES]
    unreal.log("Rok runtime city generated asset cleanup assets: {0}".format("; ".join(asset_results)))
    unreal.log("Rok runtime city generated asset cleanup directories: {0}".format("; ".join(directory_results)))


if __name__ == "__main__":
    cleanup()
