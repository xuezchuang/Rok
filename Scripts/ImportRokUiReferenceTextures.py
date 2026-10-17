import os

import unreal


DESTINATION_PATH = "/Game/RokPrototype/UI/Processed"
PROJECT_ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE_ROOT = os.path.join(PROJECT_ROOT, "Saved", "RokUiProcessed")
MAIN_HUD_ATLAS_DESTINATION = "/Game/RokPrototype/UI/MainHudAtlases"
BUILDING_MENU_DESTINATION = "/Game/RokPrototype/UI/BuildingMenu"

TEXTURES = [
    ("rok_main_top.png", "rok_main_top"),
    ("rok_main_bottom.png", "rok_main_bottom"),
    ("rok_food_icon.png", "rok_food_icon"),
    ("rok_wood_icon.png", "rok_wood_icon"),
    ("rok_stone_icon.png", "rok_stone_icon"),
    ("rok_gold_icon.png", "rok_gold_icon"),
    ("rok_upgrade_icon.png", "rok_upgrade_icon"),
]

BUILDING_MENU_TEXTURES = [
    ("rok_building_menu_title.png", "rok_building_menu_title"),
    ("rok_building_menu_level_bg.png", "rok_building_menu_level_bg"),
    ("rok_building_menu_info.png", "rok_building_menu_info"),
    ("rok_building_menu_upgrade.png", "rok_building_menu_upgrade"),
    ("rok_building_menu_feature.png", "rok_building_menu_feature"),
    ("rok_building_menu_remove.png", "rok_building_menu_remove"),
    ("rok_building_menu_speedup.png", "rok_building_menu_speedup"),
    ("rok_building_menu_worker.png", "rok_building_menu_worker"),
    ("rok_building_menu_train_infantry.png", "rok_building_menu_train_infantry"),
    ("rok_building_menu_train_cavalry.png", "rok_building_menu_train_cavalry"),
    ("rok_building_menu_train_archer.png", "rok_building_menu_train_archer"),
    ("rok_building_menu_train_siege.png", "rok_building_menu_train_siege"),
    ("rok_building_menu_hospital.png", "rok_building_menu_hospital"),
    ("rok_building_menu_button_shadow.png", "rok_building_menu_button_shadow"),
    ("rok_building_select_arrow_down.png", "rok_building_select_arrow_down"),
    ("rok_building_select_arrow_left.png", "rok_building_select_arrow_left"),
    ("rok_building_select_arrow_right.png", "rok_building_select_arrow_right"),
    ("rok_building_select_arrow_up.png", "rok_building_select_arrow_up"),
]

MAIN_HUD_ATLASES = [
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_common", "ui_mainmenu.png"),
        "rok_hud_atlas_mainmenu",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_common", "ui_common.png"),
        "rok_hud_atlas_common",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_player", "player_head.png"),
        "rok_hud_atlas_player_head",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_player", "vip_icon.png"),
        "rok_hud_atlas_vip",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_map", "ui_map.png"),
        "rok_hud_atlas_map",
    ),
]

RESOURCE_SEARCH_ATLASES = [
    (
        os.path.join(
            PROJECT_ROOT,
            "ref",
            "client-unity",
            "Assets",
            "BundleAssets",
            "UI",
            "UITextureStatic",
            "ui_res_map",
            "map_icon.png",
        ),
        "rok_hud_atlas_map_icon",
    ),
]

MODAL_ATLAS_DESTINATION = "/Game/RokPrototype/UI/ModalAtlases"

FULL_SCREEN_ATLAS_DESTINATION = "/Game/RokPrototype/UI/FullScreenAtlases"
COMMANDER_ATLAS_DESTINATION = "/Game/RokPrototype/UI/CommanderAtlases"
BUILD_CITY_ATLAS_DESTINATION = "/Game/RokPrototype/UI/BuildCityAtlases"
BUILD_CITY_PREVIEW_DESTINATION = "/Game/RokPrototype/UI/BuildCityPreviews"

WORKER_QUEUE_ATLASES = [
    (
        os.path.join(
            PROJECT_ROOT,
            "ref",
            "client-unity",
            "Assets",
            "BundleAssets",
            "UI",
            "UITextureStatic",
            "ui_res_else",
            "matrix_icon.png",
        ),
        "rok_fullscreen_worker_matrix_icon",
    ),
]

# Unity UIBuildCity uses two dedicated atlases for the bottom drawer/card
# chrome and one full PNG Sprite per building preview.  Keep all previews in a
# stable UI-owned folder instead of referencing diagnostic city-map assets.
BUILD_CITY_ATLASES = [
    (
        os.path.join(
            PROJECT_ROOT,
            "ref",
            "client-unity",
            "Assets",
            "BundleAssets",
            "UI",
            "UITextureStatic",
            "ui_res_build",
            "ui_buildingimg.png",
        ),
        "rok_buildcity_atlas_buildingimg",
    ),
    (
        os.path.join(
            PROJECT_ROOT,
            "ref",
            "client-unity",
            "Assets",
            "BundleAssets",
            "UI",
            "UITextureStatic",
            "ui_res_loading",
            "ui_loginLoading.png",
        ),
        "rok_buildcity_atlas_login_loading",
    ),
]

BUILD_CITY_PREVIEW_SOURCES = {
    2: ("city_building_1", "build_2_101_1.png"),
    3: ("city_building_1", "build_3_101_1.png"),
    4: ("city_building_2", "build_4_101_2.png"),
    5: ("city_building_2", "build_5_101_2.png"),
    7: ("city_building_1", "build_7_101_1.png"),
    9: ("city_building_1", "build_9_101_1.png"),
    10: ("city_building_2", "build_10_101_2.png"),
    11: ("city_building_1", "build_11_101_1.png"),
    12: ("city_building_2", "build_12_101_2.png"),
    13: ("city_building_2", "build_13_101_2.png"),
    14: ("city_building_1", "build_14_101_1.png"),
    15: ("city_building_1", "build_15_101_1.png"),
    16: ("city_building_1", "build_16_101_1.png"),
    17: ("city_building_2", "build_17_101_2.png"),
    18: ("city_building_1", "build_18_101_1.png"),
    19: ("city_building_5", "build_19_101_5.png"),
    20: ("city_building_2", "build_20_101_2.png"),
    21: ("city_building_2", "build_21_101_2.png"),
    28: ("city_building_1", "build_28_101_1.png"),
    29: ("city_building_1", "build_29_101_1.png"),
    30: ("city_building_1", "build_30_101_1.png"),
    31: ("city_building_5", "build_31_101_5.png"),
    51: ("city_building_1", "build_51_101_1_1.png"),
    52: ("city_building_1", "build_51_101_1_2.png"),
    53: ("city_building_1", "build_51_101_1_3.png"),
}

# Full Unity source atlases used by the player profile, quick-use resources,
# building information, and building upgrade windows. The C++ widgets sample
# the original Sprite rects directly; no replacement artwork is generated.
MODAL_ATLASES = [
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_common", "ui_frame.png"),
        "rok_modal_atlas_frame",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_setting", "ui_setting.png"),
        "rok_modal_atlas_setting",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_civilization", "civilization_flag.png"),
        "rok_modal_atlas_civilization",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_civilization", "civilization_img.png"),
        "rok_modal_atlas_civilization_img",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_hero", "hero_background.png"),
        "rok_modal_atlas_hero_background",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_common", "ui_page.png"),
        "rok_modal_atlas_page",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_item", "item0.png"),
        "rok_modal_atlas_item0",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_item", "item1.png"),
        "rok_modal_atlas_item1",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_build", "ui_build.png"),
        "rok_modal_atlas_build",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "Effect", "UIsprite", "shared_uisprite", "star003.png"),
        "rok_modal_build_star",
    ),
]

# Original Unity atlases used by the activity calendar, mail, quest, bag,
# alliance, VIP, and chat screens. Widgets sample Sprite rects from the source
# metadata; no artwork is baked from the screenshots.
FULL_SCREEN_ATLASES = [
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_activity", "activity_img.png"),
        "rok_fullscreen_activity_img",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_activity", "activity_icon.png"),
        "rok_fullscreen_activity_icon",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_mail", "ui_mail.png"),
        "rok_fullscreen_mail",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_task", "ui_quest.png"),
        "rok_fullscreen_quest",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_task", "task_icon.png"),
        "rok_fullscreen_task_icon",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_item", "item2.png"),
        "rok_fullscreen_item2",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_item", "item3.png"),
        "rok_fullscreen_item3",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_item", "item4.png"),
        "rok_fullscreen_item4",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_item", "item5.png"),
        "rok_fullscreen_item5",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_troops", "troops_head.png"),
        "rok_fullscreen_troops_head",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_alliance", "ui_alliance.png"),
        "rok_fullscreen_alliance",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_alliance", "alliance_flag.png"),
        "rok_fullscreen_alliance_flag",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_player", "ui_player.png"),
        "rok_fullscreen_player",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_chat", "ui_chat.png"),
        "rok_fullscreen_chat",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_chat", "ui_emoji.png"),
        "rok_fullscreen_emoji",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UIAnimation", "UE_TreasureBox", "UE_TreasureBox.png"),
        "rok_fullscreen_treasure_box",
    ),
]

# The full-screen screens also sample these existing shared atlases. Keeping
# them in the dedicated wrapper makes the manifest reproducible on a clean
# Content tree without reimporting unrelated HUD/modal textures.
FULL_SCREEN_SHARED_ATLASES = [
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_common", "ui_common.png"),
        "rok_hud_atlas_common",
        MAIN_HUD_ATLAS_DESTINATION,
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_common", "ui_frame.png"),
        "rok_modal_atlas_frame",
        MODAL_ATLAS_DESTINATION,
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_common", "ui_page.png"),
        "rok_modal_atlas_page",
        MODAL_ATLAS_DESTINATION,
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_item", "item0.png"),
        "rok_modal_atlas_item0",
        MODAL_ATLAS_DESTINATION,
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_item", "item1.png"),
        "rok_modal_atlas_item1",
        MODAL_ATLAS_DESTINATION,
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_player", "vip_icon.png"),
        "rok_hud_atlas_vip",
        MAIN_HUD_ATLAS_DESTINATION,
    ),
]

# Original Unity commander atlases plus the deterministic idle-frame render
# exported from Hero_2007_SD. The render is presentation evidence generated
# from the Spine skeleton/materials; it is not screenshot artwork.
COMMANDER_ATLASES = [
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_hero", "hero_background.png"),
        "rok_commander_background",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_hero", "hero_head.png"),
        "rok_commander_head",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_hero", "ui_hero.png"),
        "rok_commander_ui",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_hero", "hero_skill.png"),
        "rok_commander_skill",
    ),
    (
        os.path.join(PROJECT_ROOT, "ref", "client-unity", "Assets", "BundleAssets", "UI", "UITextureStatic", "ui_res_hero", "hero_skill2.png"),
        "rok_commander_skill2",
    ),
    (
        os.path.join(PROJECT_ROOT, "Saved", "RokUnityMigration", "Commander", "hero_2007_idle.png"),
        "rok_commander_hero_2007_idle",
    ),
]


def build_import_task(source_file, destination_name, destination_path=DESTINATION_PATH):
    task = unreal.AssetImportTask()
    task.filename = source_file
    task.destination_path = destination_path
    task.destination_name = destination_name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.factory = unreal.TextureFactory()
    return task


def configure_texture(asset_path):
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not texture:
        raise RuntimeError("Rok UI import: failed to load imported texture {0}".format(asset_path))

    properties = (
        ("srgb", True),
        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
        ("never_stream", True),
        ("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON),
        ("address_x", unreal.TextureAddress.TA_CLAMP),
        ("address_y", unreal.TextureAddress.TA_CLAMP),
    )
    for property_name, value in properties:
        try:
            texture.set_editor_property(property_name, value)
        except Exception as exc:
            raise RuntimeError(
                "Rok UI import: failed to set {0} on {1}: {2}".format(
                    property_name,
                    asset_path,
                    exc,
                )
            )
        actual_value = texture.get_editor_property(property_name)
        if actual_value != value:
            raise RuntimeError(
                "Rok UI import: {0} verification failed on {1}; expected {2}, got {3}".format(
                    property_name,
                    asset_path,
                    value,
                    actual_value,
                )
            )
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError("Rok UI import: failed to save configured texture {0}".format(asset_path))


def import_texture_set(imports, label):
    tasks = []
    missing = []
    for relative_path, destination_name, destination_path in imports:
        source_file = relative_path if os.path.isabs(relative_path) else os.path.join(SOURCE_ROOT, *relative_path.split("/"))
        if not os.path.exists(source_file):
            missing.append(source_file)
            continue
        tasks.append(build_import_task(source_file, destination_name, destination_path))

    if missing:
        raise RuntimeError("Missing {0} source textures: {1}".format(label, ", ".join(missing)))
    if not tasks:
        raise RuntimeError("No {0} texture import tasks were created.".format(label))

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    failed_task_sources = [
        task.filename
        for task in tasks
        if not list(task.imported_object_paths)
    ]
    if failed_task_sources:
        raise RuntimeError(
            "Rok UI import tasks returned no imported objects: {0}".format(
                ", ".join(failed_task_sources)
            )
        )

    imported_assets = []
    for _, destination_name, destination_path in imports:
        asset_path = "{0}/{1}".format(destination_path, destination_name)
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            raise RuntimeError("Rok UI import failed for {0}".format(asset_path))
        configure_texture(asset_path)
        imported_assets.append(asset_path)

    unreal.log("{0} complete: {1}".format(label, ", ".join(imported_assets)))
    return imported_assets


def import_main_hud_atlases():
    unreal.EditorAssetLibrary.make_directory(MAIN_HUD_ATLAS_DESTINATION)
    imports = [
        (source_file, destination_name, MAIN_HUD_ATLAS_DESTINATION)
        for source_file, destination_name in MAIN_HUD_ATLASES
    ]
    return import_texture_set(imports, "Rok Unity main HUD atlas import")


def import_resource_search_atlases():
    unreal.EditorAssetLibrary.make_directory(MAIN_HUD_ATLAS_DESTINATION)
    imports = [
        (source_file, destination_name, MAIN_HUD_ATLAS_DESTINATION)
        for source_file, destination_name in RESOURCE_SEARCH_ATLASES
    ]
    return import_texture_set(imports, "Rok Unity resource-search atlas import")


def import_building_menu_textures():
    unreal.EditorAssetLibrary.make_directory(BUILDING_MENU_DESTINATION)
    imports = [
        (relative_path, destination_name, BUILDING_MENU_DESTINATION)
        for relative_path, destination_name in BUILDING_MENU_TEXTURES
    ]
    return import_texture_set(imports, "Rok Unity building-menu texture import")


def import_modal_atlases():
    unreal.EditorAssetLibrary.make_directory(MODAL_ATLAS_DESTINATION)
    imports = [
        (source_file, destination_name, MODAL_ATLAS_DESTINATION)
        for source_file, destination_name in MODAL_ATLASES
    ]
    return import_texture_set(imports, "Rok Unity modal atlas import")


def import_full_screen_atlases():
    unreal.EditorAssetLibrary.make_directory(FULL_SCREEN_ATLAS_DESTINATION)
    imports = [
        (source_file, destination_name, FULL_SCREEN_ATLAS_DESTINATION)
        for source_file, destination_name in FULL_SCREEN_ATLASES
    ]
    return import_texture_set(imports, "Rok Unity full-screen UI atlas import")


def import_worker_queue_atlases():
    unreal.EditorAssetLibrary.make_directory(FULL_SCREEN_ATLAS_DESTINATION)
    imports = [
        (source_file, destination_name, FULL_SCREEN_ATLAS_DESTINATION)
        for source_file, destination_name in WORKER_QUEUE_ATLASES
    ]
    return import_texture_set(imports, "Rok Unity worker-queue atlas import")


def import_build_city_textures():
    unreal.EditorAssetLibrary.make_directory(BUILD_CITY_ATLAS_DESTINATION)
    unreal.EditorAssetLibrary.make_directory(BUILD_CITY_PREVIEW_DESTINATION)
    atlas_imports = [
        (source_file, destination_name, BUILD_CITY_ATLAS_DESTINATION)
        for source_file, destination_name in BUILD_CITY_ATLASES
    ]
    imported_assets = import_texture_set(
        atlas_imports,
        "Rok Unity BuildCity atlas import",
    )

    building_root = os.path.join(
        PROJECT_ROOT,
        "ref",
        "client-unity",
        "Assets",
        "BundleAssets",
        "Map",
        "Building",
        "Building_use",
        "shared_building",
        "Building_101",
    )
    preview_imports = [
        (
            os.path.join(building_root, folder_name, source_name),
            "rok_buildcity_preview_t{0}_c101_a1".format(type_id),
            BUILD_CITY_PREVIEW_DESTINATION,
        )
        for type_id, (folder_name, source_name) in sorted(
            BUILD_CITY_PREVIEW_SOURCES.items()
        )
    ]
    imported_assets.extend(
        import_texture_set(
            preview_imports,
            "Rok Unity BuildCity preview import",
        )
    )
    return imported_assets


def import_full_screen_dependencies():
    unreal.EditorAssetLibrary.make_directory(MAIN_HUD_ATLAS_DESTINATION)
    unreal.EditorAssetLibrary.make_directory(MODAL_ATLAS_DESTINATION)
    return import_texture_set(
        FULL_SCREEN_SHARED_ATLASES,
        "Rok Unity full-screen shared atlas import",
    )


def import_commander_atlases():
    unreal.EditorAssetLibrary.make_directory(COMMANDER_ATLAS_DESTINATION)
    imports = [
        (source_file, destination_name, COMMANDER_ATLAS_DESTINATION)
        for source_file, destination_name in COMMANDER_ATLASES
    ]
    return import_texture_set(imports, "Rok Unity commander UI atlas import")


def main():
    unreal.EditorAssetLibrary.make_directory(DESTINATION_PATH)

    imports = [(relative_path, destination_name, DESTINATION_PATH) for relative_path, destination_name in TEXTURES]
    import_texture_set(imports, "Rok UI processed texture import")
    import_building_menu_textures()
    import_main_hud_atlases()
    import_modal_atlases()
    import_full_screen_atlases()
    import_worker_queue_atlases()
    import_build_city_textures()
    import_commander_atlases()


if __name__ == "__main__":
    main()
