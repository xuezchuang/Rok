import json

from PrepareRokUiTextures import (
    PROJECT_ROOT,
    SOURCE_ROOT,
    sha256_file,
    unity_sprite_rect,
    unity_sprite_vector,
)


OUTPUT_PATH = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_full_screen_ui.json"

ATLAS_TARGETS = {
    "UITextureStatic/ui_res_common/ui_common.png": {
        "size": [2048, 1024],
        "ue_target": "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common",
    },
    "UITextureStatic/ui_res_common/ui_frame.png": {
        "size": [1024, 2048],
        "ue_target": "/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame",
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
    "UITextureStatic/ui_res_activity/activity_img.png": {
        "size": [2048, 2048],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_activity_img",
    },
    "UITextureStatic/ui_res_activity/activity_icon.png": {
        "size": [1024, 256],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_activity_icon",
    },
    "UITextureStatic/ui_res_mail/ui_mail.png": {
        "size": [2048, 1024],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_mail",
    },
    "UITextureStatic/ui_res_task/ui_quest.png": {
        "size": [512, 128],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_quest",
    },
    "UITextureStatic/ui_res_task/task_icon.png": {
        "size": [256, 1024],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_task_icon",
    },
    "UITextureStatic/ui_res_item/item2.png": {
        "size": [256, 512],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item2",
    },
    "UITextureStatic/ui_res_item/item3.png": {
        "size": [256, 1024],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item3",
    },
    "UITextureStatic/ui_res_item/item4.png": {
        "size": [2048, 2048],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item4",
    },
    "UITextureStatic/ui_res_item/item5.png": {
        "size": [1024, 2048],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item5",
    },
    "UITextureStatic/ui_res_alliance/ui_alliance.png": {
        "size": [1024, 2048],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance",
    },
    "UITextureStatic/ui_res_alliance/alliance_flag.png": {
        "size": [1024, 2048],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance_flag",
    },
    "UITextureStatic/ui_res_player/ui_player.png": {
        "size": [512, 1024],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_player",
    },
    "UITextureStatic/ui_res_player/vip_icon.png": {
        "size": [512, 1024],
        "ue_target": "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_vip",
    },
    "UITextureStatic/ui_res_chat/ui_chat.png": {
        "size": [1024, 128],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_chat",
    },
    "UITextureStatic/ui_res_chat/ui_emoji.png": {
        "size": [256, 1024],
        "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_emoji",
    },
}

SPRITES = {
    "UITextureStatic/ui_res_common/ui_common.png": (
        "btn_com_1000_1",
        "pl_com_1000",
        "pl_com_1001",
        "pl_com_1005",
        "pl_com_1008",
        "btn_com_max3",
        "btn_com_mid2_3",
        "btn_com_min3",
        "btn_com_min4",
        "btn_com_1006_2",
        "btn_com_1006_3",
        "btn_com_1007_1",
        "btn_com_bagMax",
        "btn_com_page1",
        "btn_com_page2",
        "btn_com_side1",
        "btn_com_side2",
        "btn_com_1008_1",
        "btn_com_1008_2",
        "img_com_new",
        "img_com_page",
        "img_com_redpoint",
        "img_com_redpot",
        "img_com_itemTopBg",
        "img_com_cur1",
        "img_com_cur2",
        "img_com_cur5",
        "icon_com_1001",
        "icon_com_vipcur",
        "pb_com_1000_1",
        "pb_com_1000_4",
    ),
    "UITextureStatic/ui_res_common/ui_frame.png": (
        "pl_fra_1000",
        "pl_fra_1002",
        "pl_fra_1003",
    ),
    "UITextureStatic/ui_res_common/ui_page.png": (
        "paging_301",
        "paging_301_1",
        "paging_302",
        "paging_302_1",
        "paging_303",
        "paging_303_1",
        "paging_401",
        "paging_401_1",
        "paging_402",
        "paging_402_1",
        "paging_503",
        "paging_503_1",
    ),
    "UITextureStatic/ui_res_item/item0.png": (
        "img_item_bg1",
        "img_item_bg2",
        "img_item_bg3",
        "img_item_bg4",
        "img_item_bg5",
    ),
    "UITextureStatic/ui_res_item/item1.png": (
        "item_1_1001",
        "item_1_1002",
        "item_1_1003",
        "item_1_1004",
        "item_1_1005",
        "item_1_1009",
    ),
    "UITextureStatic/ui_res_activity/activity_img.png": ("activity_img_2000",),
    "UITextureStatic/ui_res_activity/activity_icon.png": (
        "activity_icon_1000",
        "activity_icon_1003",
        "activity_icon_1006",
    ),
    "UITextureStatic/ui_res_mail/ui_mail.png": (
        "icon_mail_1004",
        "img_mail_title_1",
        "btn_mail_1000",
        "btn_mail_1001",
        "btn_mail_1002",
        "btn_mail_1003",
        "btn_mail_1006",
        "btn_mail_1008",
    ),
    "UITextureStatic/ui_res_task/ui_quest.png": (
        "img_quest_activeBg",
        "img_quest_line",
    ),
    "UITextureStatic/ui_res_task/task_icon.png": (
        "task_icon_1003",
        "task_icon_1005",
        "task_icon_1009",
        "task_icon_1012",
    ),
    "UITextureStatic/ui_res_item/item2.png": (
        "item_2_1000",
        "item_2_1001",
        "item_2_1002",
        "item_2_1003",
    ),
    "UITextureStatic/ui_res_item/item3.png": (
        "item_3_1003",
        "item_3_1004",
        "item_3_1005",
        "item_3_1007",
    ),
    "UITextureStatic/ui_res_item/item4.png": (
        "item_equip0_100",
        "item_equip0_101",
        "item_equip0_102",
        "item_equip0_103",
    ),
    "UITextureStatic/ui_res_item/item5.png": (
        "item_5_1018",
        "item_5_2006",
        "item_5_1031",
    ),
    "UITextureStatic/ui_res_alliance/ui_alliance.png": (
        "pl_ali_join",
        "icon_ali_star",
        "img_ali_aliFlag",
        "icon_ali_1000",
        "icon_ali_1001",
        "icon_ali_1002",
        "icon_ali_1003",
        "icon_ali_1004",
        "icon_ali_1005",
        "icon_ali_1006",
        "icon_ali_1007",
        "icon_ali_1008",
        "icon_ali_1009",
        "icon_ali_1010",
        "icon_ali_1011",
        "icon_ali_1012",
        "icon_ali_1013",
        "img_ali_r5",
    ),
    "UITextureStatic/ui_res_alliance/alliance_flag.png": (
        "alliance_flag_2_101",
        "alliance_flag_3_100",
    ),
    "UITextureStatic/ui_res_player/ui_player.png": (
        "pl_player_1003",
        "pl_player_1004",
    ),
    "UITextureStatic/ui_res_player/vip_icon.png": tuple(
        "vip_icon_{0}".format(index) for index in range(100, 118)
    ),
    "UITextureStatic/ui_res_chat/ui_chat.png": (
        "btn_chat_close",
        "btn_chat_delete",
        "btn_chat_look",
        "btn_chat_send",
        "btn_chat_upset",
        "btn_chat_upsetun",
        "icon_chat_1000",
        "icon_chat_1001",
        "icon_chat_1002",
        "icon_chat_1002_1",
        "icon_chat_1003",
        "icon_chat_1004",
        "icon_chat_1004_1",
        "icon_chat_1005",
        "icon_chat_1006",
        "icon_chat_1007",
        "icon_chat_1008",
        "icon_chat_1009",
        "icon_chat_1010",
        "icon_chat_ali",
        "icon_chat_share_1",
        "icon_chat_share_2",
        "icon_chat_share_3",
        "icon_chat_world",
        "img_chat_1000",
        "img_chat_1001",
        "img_chat_choose",
        "pl_chat_1000",
        "pl_chat_1001",
        "pl_chat_1002",
        "pl_chat_1003",
        "pl_chat_1004",
    ),
    "UITextureStatic/ui_res_chat/ui_emoji.png": (
        "btn_emoji_1000",
        "btn_emoji_1001",
        "img_emoji_1000",
        "img_emoji_pop1",
        "img_emoji_pop2",
        "img_emoji_pop3",
        "img_emoji_popfre1",
        "img_emoji_popfre2",
        "img_emoji_popfre3",
        "pl_emoji_fra",
        "pl_emoji_pop",
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
        meta_path = atlas_path.with_name(atlas_path.name + ".meta")
        if not atlas_path.exists() or not meta_path.exists():
            raise FileNotFoundError("Missing Unity full-screen UI atlas source: {0}".format(atlas_path))

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
            "activity": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_activity/UIEventDate/UI_Win_EventDate.prefab",
            "activity_lords_war": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_activity/UIEventDate/UI_Item_EventStrongerPlayer1.prefab",
            "mail": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_mail/UIMail/UI_Win_Mail.prefab",
            "mail_row": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_mail/UIMail/UI_Item_MailBtn.prefab",
            "mail_system": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_mail/UIMail/UI_Item_MailTypeSystem.prefab",
            "tasks": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_task/UIQuest/UI_Win_Quest.prefab",
            "task_main_row": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_task/UIQuest/UI_Item_QuestMain.prefab",
            "task_side_row": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_task/UIQuest/UI_Item_Quest.prefab",
            "items": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_item/UIBag/UI_Win_Bag.prefab",
            "alliance_welcome": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildCreate/UI_Win_GuildWelcome.prefab",
            "alliance_join": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildJoin/UI_Win_GuildJoin.prefab",
            "alliance_join_row": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildJoin/UI_Item_GuildJoin.prefab",
            "alliance_create": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildCreate/UI_Win_GuildSetting.prefab",
            "alliance_main": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildMain/UI_Win_GuildMain.prefab",
            "vip": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_player/UIVIP/UI_Win_Vip.prefab",
            "vip_page": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_player/UIVIP/UI_Item_VipListItem.prefab",
            "vip_buff_row": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_player/UIVIP/UI_Item_VipBuffListItem.prefab",
            "vip_daily_points": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_player/UIVIP/UI_IF_VipPointDayReward.prefab",
            "chat": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_chat/UIChat/UI_Win_Chat.prefab",
            "chat_contact_row": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_chat/UIChat/UI_Item_Contact.prefab",
            "chat_self_row": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_chat/UIChat/UI_Item_ChatSelf.prefab",
            "chat_other_row": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_chat/UIChat/UI_Item_ChatOther.prefab",
            "chat_self_emoji_row": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_chat/UIChat/UI_Item_ChatSelfEmoji.prefab",
            "chat_other_emoji_row": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_chat/UIChat/UI_Item_ChatOtherEmoji.prefab",
        },
        "source_runtime": {
            "activity": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Activity/EventDateMediator.cs",
            "mail": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Mail/MailMediator.cs",
            "tasks": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestMediator.cs",
            "items": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Bag/BagMediator.cs",
            "alliance_welcome": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Win_GuildWelcomeMediator.cs",
            "alliance_join": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Win_GuildJoinMediator.cs",
            "alliance_create": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Win_GuildSettingMediator.cs",
            "alliance_main": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Win_GuildMainMediator.cs",
            "vip": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/VIP/UI_Win_VipMediator.cs",
            "chat": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Chat/UI_Win_ChatMediator.cs",
            "chat_view": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Chat/UI_Win_ChatView.cs",
            "chat_proxy": "ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/ChatProxy.cs",
        },
        "ue_import_script": "Scripts/ImportRokUnityFullScreenAtlases.py",
        "ue_widget": "Source/Rok/RokUnityFeatureScreenWidget.cpp",
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
            "activity_outer": [168.0, 84.0, 1104.0, 732.0],
            "activity_left": [182.0, 101.0, 310.0, 688.0],
            "activity_detail": [504.5, 121.0, 735.0, 658.0],
            "mail_outer": [36.5, 60.0, 1367.0, 780.0],
            "mail_list": [55.0, 174.0, 478.5, 590.0],
            "mail_detail": [547.0, 174.0, 830.0, 590.0],
            "task_outer": [146.0, 73.0, 1148.0, 754.0],
            "task_content": [176.0, 151.0, 1088.0, 664.0],
            "items_reference_outer": [170.0, 73.0, 1100.0, 754.0],
            "items_reference_grid": [184.0, 199.0, 700.0, 618.0],
            "items_reference_detail": [891.0, 211.0, 350.0, 590.0],
            "items_source_prefab_panel": [176.0, 93.1, 1088.0, 683.8],
            "alliance_welcome_outer": [170.0, 73.0, 1100.0, 754.0],
            "alliance_join_outer": [146.0, 73.0, 1148.0, 754.0],
            "alliance_main_outer": [146.0, 73.0, 1148.0, 754.0],
            "vip_outer": [160.0, 100.0, 1120.0, 700.0],
            "vip_content": [208.0, 160.0, 1024.0, 612.0],
            "vip_page": [254.0, 320.0, 932.0, 400.0],
            # Chat is bottom-anchored in the 1440x810 Unity design rather than
            # centered in the 1440x900 expanded capture used above. These are
            # exact top-left design-space rectangles derived from the prefab's
            # RectTransforms, including the source's slight overscan.
            "chat_design_background": [-5.2, 0.0, 1028.8, 817.2],
            "chat_design_contacts": [0.83, 0.0, 330.0, 794.2],
            "chat_design_conversation": [326.3, 0.0, 694.7, 810.0],
            "chat_design_messages": [342.65, 82.6, 652.0, 611.4],
            "chat_design_composer": [326.65, 711.27, 694.0, 98.73],
            "chat_design_input": [339.95, 736.64, 520.0, 48.0],
            "chat_design_emoji_button": [868.65, 730.64, 60.0, 60.0],
            "chat_design_send_button": [927.65, 730.64, 60.0, 60.0],
        },
        "preview_only_state": {
            "activity_calendar_and_countdown": True,
            "mail_server_state_and_commands": True,
            "task_side_state_and_reward_commands": True,
            "item_owned_counts_and_use_commands": True,
            "alliance_membership_join_create_and_commands": True,
            "vip_progress_rewards_and_purchases": True,
            "chat_contacts_messages_and_send_commands": True,
        },
        "runtime_bindings": {
            "tasks": "URokUnityRuntimeStateSubsystem.GetActiveTask",
            "task_rewards": "TaskMain.reward -> ItemPackageShow.group (compiled CSV)",
            "activity": None,
            "mail": None,
            "items": None,
            "alliance": None,
            "vip": None,
            "chat": None,
        },
        "reference_vip_state": {
            "state_kind": "unity_first_login_test_fixture",
            "account_state_authority": "server",
            "normal_offline_behavior": "unsynchronized_disabled",
            "current_level": 0,
            "current_points": 0,
            "next_level_points": 200,
            "today_points": 40,
            "continuous_login_days": 1,
            "daily_points_claimed": False,
            "free_chest_claimed": False,
            "special_chest_purchased": False,
            "special_chest_price_usd": 0.99,
            "vip_config": "ref/config-tables/编译客户端配置/CSV/Vip.csv",
            "vip_day_point_config": "ref/config-tables/编译客户端配置/CSV/VipDayPoint.csv",
            "vip_attribute_config": "ref/config-tables/编译客户端配置/CSV/VipAtt.csv",
            "unity_mediator": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/VIP/UI_Win_VipMediator.cs",
            "unity_player_proxy": "ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/PlayerProxy.cs",
            "server_role_logic": "ref/server/data/work/server/game_server/logic/lualib/RoleLogic.lua",
            "server_role_definition": "ref/server/data/work/common/lualib/define/RoleDef.lua",
        },
        "reference_rewards": {
            "task_200900_runtime": [
                {"currency": "denar", "count": 50, "sprite": "img_com_cur5"},
                {"currency": "food", "count": 2000, "sprite": "img_com_cur1"},
                {"currency": "wood", "count": 2000, "sprite": "img_com_cur2"},
            ],
            "activity_30001": [
                {"item_id": 101090090, "count": 1, "sprite": "item_1_1005", "top_label": "2,000"},
                {"item_id": 509010007, "count": 180, "sprite": "item_5_2006"},
                {"item_id": 201010030, "count": 35, "sprite": "item_2_1000", "top_label": "60分钟"},
                {"item_id": 101100040, "count": 100, "sprite": "item_1_1009"},
            ],
            "mail_100030": [{"item_id": 502090001, "count": 3, "sprite": "item_5_1031"}],
            "task_200903": [{"item_id": 201010013, "count": 2, "sprite": "item_2_1000"}],
            "task_304001": [
                {"currency": "food", "count": 5000, "sprite": "img_com_cur1"},
                {"currency": "wood", "count": 5000, "sprite": "img_com_cur2"},
                {"item_id": 201010013, "count": 2, "sprite": "item_2_1000"},
            ],
            "task_306163": [{"item_id": 1, "count": 1, "sprite": "item_5_1018"}],
            "task_306164": [{"item_id": 1, "count": 1, "sprite": "item_3_1003"}],
        },
        "atlases": ATLAS_TARGETS,
        "spine_regions": [
            {
                "source": "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UE_TreasureBox/UE_TreasureBox.png",
                "source_sha256": sha256_file(PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UE_TreasureBox/UE_TreasureBox.png"),
                "atlas_size": [1263, 380],
                "region": "gift1_stay",
                "top_left_rect": [1090, 67, 171, 136],
                "display_size": [171, 136],
                "rotated": False,
                "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_treasure_box",
            },
            {
                "source": "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UE_TreasureBox/UE_TreasureBox.png",
                "source_sha256": sha256_file(PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UE_TreasureBox/UE_TreasureBox.png"),
                "atlas_size": [1263, 380],
                "region": "gift3_stay",
                "top_left_rect": [940, 12, 148, 182],
                "display_size": [182, 148],
                "rotated": True,
                "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_treasure_box",
            },
            {
                "source": "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UE_TreasureBox/UE_TreasureBox.png",
                "source_sha256": sha256_file(PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UE_TreasureBox/UE_TreasureBox.png"),
                "atlas_size": [1263, 380],
                "region": "gift6_stay",
                "top_left_rect": [972, 205, 146, 173],
                "display_size": [173, 146],
                "rotated": True,
                "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_treasure_box",
            },
            {
                "source": "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UE_TreasureBox/UE_TreasureBox.png",
                "source_sha256": sha256_file(PROJECT_ROOT / "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UE_TreasureBox/UE_TreasureBox.png"),
                "atlas_size": [1263, 380],
                "region": "gift5_stay",
                "top_left_rect": [610, 3, 161, 187],
                "display_size": [187, 161],
                "rotated": True,
                "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_treasure_box",
            }
        ],
        "sprites": sprite_rows,
    }
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Rok Unity full-screen UI manifest:", OUTPUT_PATH)
    print("Rok Unity full-screen UI sprites:", len(sprite_rows))


if __name__ == "__main__":
    main()
