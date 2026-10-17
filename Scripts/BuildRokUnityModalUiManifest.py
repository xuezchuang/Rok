import json
from pathlib import Path

from PrepareRokUiTextures import (
    PROJECT_ROOT,
    SOURCE_ROOT,
    sha256_file,
    unity_sprite_rect,
    unity_sprite_vector,
)


OUTPUT_PATH = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_modal_ui.json"
BUILD_STAR_PATH = (
    PROJECT_ROOT
    / "ref"
    / "client-unity"
    / "Assets"
    / "BundleAssets"
    / "Effect"
    / "UIsprite"
    / "shared_uisprite"
    / "star003.png"
)

ATLAS_TARGETS = {
    "UITextureStatic/ui_res_common/ui_common.png": {
        "size": [2048, 1024],
        "ue_target": "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common",
    },
    "UITextureStatic/ui_res_player/player_head.png": {
        "size": [512, 2048],
        "ue_target": "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_player_head",
    },
    "UITextureStatic/ui_res_common/ui_frame.png": {
        "size": [1024, 2048],
        "ue_target": "/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame",
    },
    "UITextureStatic/ui_res_setting/ui_setting.png": {
        "size": [1024, 1024],
        "ue_target": "/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_setting",
    },
    "UITextureStatic/ui_res_civilization/civilization_flag.png": {
        "size": [2048, 256],
        "ue_target": "/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_civilization",
    },
    "UITextureStatic/ui_res_civilization/civilization_img.png": {
        "size": [2048, 1024],
        "ue_target": "/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_civilization_img",
    },
    "UITextureStatic/ui_res_hero/hero_background.png": {
        "size": [1024, 512],
        "ue_target": "/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_hero_background",
    },
    "UITextureStatic/ui_res_common/ui_page.png": {
        "size": [128, 2048],
        "ue_target": "/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_page",
    },
    "UITextureStatic/ui_res_item/item0.png": {
        "size": [512, 512],
        "ue_target": "/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item0",
    },
    "UITextureStatic/ui_res_item/item1.png": {
        "size": [256, 1024],
        "ue_target": "/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item1",
    },
    "UITextureStatic/ui_res_build/ui_build.png": {
        "size": [1024, 1024],
        "ue_target": "/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_build",
    },
}

SPRITES = {
    "UITextureStatic/ui_res_common/ui_frame.png": (
        "pl_fra_1000",
        "pl_fra_1002",
    ),
    "UITextureStatic/ui_res_common/ui_common.png": (
        "btn_com_1000_1",
        "pl_com_1000",
        "pl_com_1001",
        "pl_com_1008",
        "img_com_itemTopBg",
        "btn_com_1007",
        "btn_com_1007_1",
        "pb_com_1000_1",
        "pb_com_1000_4",
        "btn_com_1006_2",
        "icon_com_1000",
        "img_com_1000",
        "img_com_cur1",
        "img_com_cur2",
        "img_com_cur3",
        "img_com_cur4",
        "img_com_cur5",
        "btn_com_min3",
        "btn_com_mid2_2",
        "btn_com_mid2_3",
        "btn_com_max2",
        "btn_com_max3",
    ),
    "UITextureStatic/ui_res_player/player_head.png": (
        "player_head_1000",
        "img_player_frame",
    ),
    "UITextureStatic/ui_res_setting/ui_setting.png": (
        "pl_set_1000",
        "img_set_playerFlag",
        "btn_set_pic",
        "icon_set_rename",
        "icon_set_change",
        "icon_set_1011",
        "icon_set_1012",
        "icon_set_1013",
        "icon_set_1015",
    ),
    "UITextureStatic/ui_res_civilization/civilization_flag.png": (
        "civilization_flag_10",
        "civilization_flag_11",
        "civilization_flag_12",
        "civilization_flag_13",
        "civilization_flag_14",
        "civilization_flag_15",
        "civilization_flag_17",
        "civilization_flag_101",
        "civilization_flag_111",
        "civilization_flag_121",
        "civilization_flag_131",
        "civilization_flag_141",
        "civilization_flag_151",
        "civilization_flag_171",
        "civilization_flag_select",
    ),
    "UITextureStatic/ui_res_civilization/civilization_img.png": (
        "civilization_img_10",
        "civilization_img_11",
        "civilization_img_14",
        "civilization_soldiers_10",
        "civilization_soldiers_11",
        "civilization_soldiers_14",
    ),
    "UITextureStatic/ui_res_hero/hero_background.png": (
        "hero_background_10",
        "hero_background_11",
        "hero_background_14",
        "hero_background_15",
    ),
    "UITextureStatic/ui_res_common/ui_page.png": (
        "paging_201",
        "paging_201_1",
        "paging_202",
        "paging_202_1",
        "paging_203",
        "paging_203_1",
        "paging_204",
        "paging_204_1",
    ),
    "UITextureStatic/ui_res_item/item0.png": (
        "img_item_bg1",
        "img_item_bg2",
        "img_item_bg3",
    ),
    "UITextureStatic/ui_res_item/item1.png": (
        "item_1_1001",
        "item_1_1002",
        "item_1_1003",
        "item_1_1004",
    ),
    "UITextureStatic/ui_res_build/ui_build.png": (
        "pl_build_1000",
    ),
}


def normalized_uv(rect, atlas_size):
    x, unity_y, width, height = rect
    atlas_width, atlas_height = atlas_size
    top = atlas_height - unity_y - height
    return {
        "min": [x / atlas_width, top / atlas_height],
        "max": [(x + width) / atlas_width, (top + height) / atlas_height],
    }


def main():
    sprite_rows = []
    for atlas_key, sprite_names in SPRITES.items():
        atlas_config = ATLAS_TARGETS[atlas_key]
        atlas_path = SOURCE_ROOT / atlas_key
        meta_path = Path(str(atlas_path) + ".meta")
        if not atlas_path.exists() or not meta_path.exists():
            raise FileNotFoundError("Missing Unity modal UI atlas source: {0}".format(atlas_path))

        meta_text = meta_path.read_text(encoding="utf-8-sig")
        atlas_hash = sha256_file(atlas_path)
        meta_hash = sha256_file(meta_path)
        for sprite_name in sprite_names:
            rect = unity_sprite_rect(meta_text, sprite_name)
            sprite_rows.append(
                {
                    "source_atlas": atlas_path.relative_to(PROJECT_ROOT).as_posix(),
                    "source_meta": meta_path.relative_to(PROJECT_ROOT).as_posix(),
                    "source_atlas_sha256": atlas_hash,
                    "source_meta_sha256": meta_hash,
                    "sprite_name": sprite_name,
                    "rect": {
                        "x": rect[0],
                        "y": rect[1],
                        "width": rect[2],
                        "height": rect[3],
                    },
                    "border": unity_sprite_vector(
                        meta_text,
                        sprite_name,
                        "border",
                        ("x", "y", "z", "w"),
                    ),
                    "atlas_size": atlas_config["size"],
                    "normalized_uv": normalized_uv(rect, atlas_config["size"]),
                    "ue_atlas_target": atlas_config["ue_target"],
                }
            )

    manifest = {
        "version": 1,
        "reference_resolution": [1440, 810],
        "screen_match_mode": "Expand",
        "reference_capture_expanded_resolution": [1440, 900],
        "source_prefabs": {
            "player_profile": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_player/UIPlayerData/UI_Win_PlayerData.prefab",
            "quick_use_resources": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_item/UIQuickUseResources/UI_Win_QuickUseResources.prefab",
            "quick_use_resource_row": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_item/UIAddSpeed/UI_Item_StandardUseItem.prefab",
            "building_info": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Win_BuildingDesc.prefab",
            "building_upgrade": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Win_BuildingUpdate.prefab",
            "window_type_1": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_Window_Type1.prefab",
            "window_type_2": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_Window_Type2.prefab",
            "civilization_selection": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_login/UILogin/CreateCharView.prefab",
            "civilization_selection_item": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_login/UILogin/CreateCharFlagItem.prefab",
            "civilization_change_confirmation": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_login/UILogin/UI_Win_CivilizationChange.prefab",
        },
        "source_runtime": {
            "player_profile": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/PlayerInfo/PlayerDataMediator.cs",
            "quick_use_resources": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/QuickUseResources/QuickUseResourcesMediator.cs",
            "building_info": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/BuildingDescMediator.cs",
            "building_upgrade": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateMediator.cs",
            "civilization_selection": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Login/CreateCharMediator.cs",
            "civilization_change_confirmation": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Login/UI_Win_CivilizationChangeMediator.cs",
        },
        "ue_import_script": "Scripts/ImportRokUnityModalAtlases.py",
        "ue_widget": "Source/Rok/RokUnityModalWidget.cpp",
        "texture_settings": {
            "srgb": True,
            "mip_gen_settings": "TMGS_NoMipmaps",
            "lod_group": "TEXTUREGROUP_UI",
            "never_stream": True,
            "compression_settings": "TC_EditorIcon",
            "address_x": "TA_Clamp",
            "address_y": "TA_Clamp",
        },
        "layouts": {
            "player_profile_window": [133.5, 85.5, 1173.0, 729.0],
            "type_2_window": [160.0, 100.0, 1120.0, 700.0],
            "resource_content": [207.0, 168.3, 1026.0, 609.4],
            "building_content": [208.0, 162.0, 1024.0, 622.0],
            "civilization_selection_root": [0.0, 0.0, 1440.0, 810.0],
            "civilization_list_view": [0.0, 640.7, 1440.0, 168.3],
            "civilization_flag_item": [199.0, 180.0],
            "civilization_change_confirmation": [160.0, 55.0, 1120.0, 700.0],
        },
        "preview_only_state": {
            "player_server_fields": [
                "encrypted_role_id",
                "head_id",
                "head_frame_id",
                "civilization",
                "alliance",
                "kill_count",
                "action_force",
            ],
            "resource_item_inventory": True,
            "denar_balance": True,
        },
        "atlases": ATLAS_TARGETS,
        "independent_textures": [
            {
                "source": BUILD_STAR_PATH.relative_to(PROJECT_ROOT).as_posix(),
                "source_sha256": sha256_file(BUILD_STAR_PATH),
                "size": [256, 256],
                "ue_target": "/Game/RokPrototype/UI/ModalAtlases/rok_modal_build_star",
            }
        ],
        "sprites": sprite_rows,
    }
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Rok Unity modal UI manifest:", OUTPUT_PATH)
    print("Rok Unity modal UI sprites:", len(sprite_rows))


if __name__ == "__main__":
    main()
