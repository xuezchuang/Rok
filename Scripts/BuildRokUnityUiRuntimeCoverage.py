import json
import re
from collections import Counter
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
FULL_CATALOG_PATH = OUTPUT_DIR / "unity_full_ui_catalog.json"
OUTPUT_JSON = OUTPUT_DIR / "unity_ui_runtime_coverage.json"
OUTPUT_MD = OUTPUT_DIR / "unity_ui_runtime_coverage.md"


MODAL_WIDGET = {
    "class": "URokUnityModalWidget",
    "header": "Source/Rok/RokUnityModalWidget.h",
    "cpp": "Source/Rok/RokUnityModalWidget.cpp",
}
FEATURE_WIDGET = {
    "class": "URokUnityFeatureScreenWidget",
    "header": "Source/Rok/RokUnityFeatureScreenWidget.h",
    "cpp": "Source/Rok/RokUnityFeatureScreenWidget.cpp",
}
CITY_HUD_WIDGET = {
    "class": "URokMainHudWidget",
    "header": "Source/Rok/RokMainHudWidget.h",
    "cpp": "Source/Rok/RokMainHudWidget.cpp",
}
WORLD_HUD_WIDGET = {
    "class": "URokWorldMapHudWidget",
    "header": "Source/Rok/RokWorldMapHudWidget.h",
    "cpp": "Source/Rok/RokWorldMapHudWidget.cpp",
}
BUILDING_PANEL_WIDGET = {
    "class": "URokBuildingPanelWidget",
    "header": "Source/Rok/RokBuildingPanelWidget.h",
    "cpp": "Source/Rok/RokBuildingPanelWidget.cpp",
}


def entry(
    surface,
    owner_class,
    header,
    cpp,
    control_name,
    handler,
    *,
    factory_seed=None,
    route_evidence=None,
):
    result = {
        "kind": "hud_entry"
        if surface in {"city_hud", "world_hud"}
        else (
            "building_menu_entry"
            if surface == "building_menu"
            else "widget_entry"
        ),
        "surface": surface,
        "owner_class": owner_class,
        "header": header,
        "cpp": cpp,
        "control_name": control_name,
        "handler": handler,
    }
    if factory_seed:
        result["control_factory"] = {
            "name": "MakeFeatureButton",
            "seed": factory_seed,
            "generated_suffix": "Button",
        }
    else:
        result["control_literal"] = control_name
    if route_evidence:
        result["route_evidence"] = {
            "path": route_evidence[0],
            "tokens": list(route_evidence[1]),
        }
    return result


def city_entry(control_name, handler, *, factory_seed=None):
    return entry(
        "city_hud",
        CITY_HUD_WIDGET["class"],
        CITY_HUD_WIDGET["header"],
        CITY_HUD_WIDGET["cpp"],
        control_name,
        handler,
        factory_seed=factory_seed,
    )


def world_entry(control_name, handler):
    return entry(
        "world_hud",
        WORLD_HUD_WIDGET["class"],
        WORLD_HUD_WIDGET["header"],
        WORLD_HUD_WIDGET["cpp"],
        control_name,
        handler,
    )


def building_entry(control_name, handler, *, route_evidence=None):
    return entry(
        "building_menu",
        "URokBuildingPanelWidget",
        "Source/Rok/RokBuildingPanelWidget.h",
        "Source/Rok/RokBuildingPanelWidget.cpp",
        control_name,
        handler,
        route_evidence=route_evidence,
    )


def behavior(
    *,
    preview_shell=False,
    real_local_loop=False,
    service_boundary=False,
    evidence=(),
):
    return {
        "preview_shell": preview_shell,
        "real_local_loop": real_local_loop,
        "service_boundary": service_boundary,
        "unimplemented": False,
        "evidence": [
            {"state": state, "path": path, "tokens": list(tokens)}
            for state, path, tokens in evidence
        ],
    }


MAPPINGS = [
    {
        "id": "scout",
        "display_name": "斥候营地",
        "unity_prefab": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_troops/UIScout/UI_Win_Scout.prefab",
        "runtime_widget": {
            "class": "URokScoutScreenWidget",
            "header": "Source/Rok/RokScoutScreenWidget.h",
            "cpp": "Source/Rok/RokScoutScreenWidget.cpp",
            "open_method": "Open",
        },
        "entries": [building_entry(
            "BuildingFeatureButton", "HandleFeatureClicked",
            route_evidence=("Source/Rok/RokMainHudWidget.cpp", (
                "State.UnityBuildingTypeId == 28", "ScoutScreenWidget->Open(State.Level)",
            )),
        )],
        "behavior": behavior(
            preview_shell=True, real_local_loop=True, service_boundary=True,
            evidence=(
                ("preview_shell", "Source/Rok/RokScoutScreenWidget.cpp", ("ScoutMotionCanvas", "ScoutListScroll")),
                ("real_local_loop", "Source/Rok/RokScoutScreenWidget.cpp", ("ShowDescription", "ShowList", "ScoutDescriptionScroll")),
                ("service_boundary", "Source/Rok/RokScoutScreenWidget.cpp", ("ScoutQueueUnavailable",)),
            ),
        ),
    },
    {
        "id": "city_hud",
        "display_name": "主城 HUD",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_map/UIMainCity/UI_IF_MainInterface.prefab"
        ),
        "runtime_widget": {**CITY_HUD_WIDGET, "open_method": "RebuildWidget"},
        "runtime_activation": {
            "path": "Source/Rok/RokPlayerController.cpp",
            "tokens": [
                "CreateWidget<URokMainHudWidget>",
                "MainHudWidget->AddToViewport",
            ],
        },
        "entries": [],
        "behavior": behavior(
            real_local_loop=True,
            evidence=(
                (
                    "real_local_loop",
                    "Source/Rok/RokMainHudWidget.cpp",
                    ("HandleReturnToWorldMap", "RequestReturnToWorldMap"),
                ),
            ),
        ),
    },
    {
        "id": "world_hud",
        "display_name": "世界地图 HUD",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_map/UIMainCity/UI_IF_MainInterface.prefab"
        ),
        "runtime_widget": {
            **WORLD_HUD_WIDGET,
            "open_method": "RebuildWidget",
        },
        "runtime_activation": {
            "path": "Source/Rok/RokWorldMapPlayerController.cpp",
            "tokens": [
                "CreateWidget<URokWorldMapHudWidget>",
                "WorldMapHudWidget->AddToViewport",
            ],
        },
        "entries": [],
        "behavior": behavior(
            real_local_loop=True,
            evidence=(
                (
                    "real_local_loop",
                    "Source/Rok/RokWorldMapHudWidget.cpp",
                    (
                        "HandleEnterPlayerCityClicked",
                        "RequestEnterPlayerCity",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "server_list",
        "display_name": "王国列表",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_map/UIServer/UI_Win_Server.prefab"
        ),
        "runtime_widget": {
            **WORLD_HUD_WIDGET,
            "open_method": "HandleOpenServerListClicked",
        },
        "entries": [
            world_entry(
                "WorldMapGlobeButton",
                "HandleOpenServerListClicked",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    WORLD_HUD_WIDGET["cpp"],
                    ("BuildServerListOverlay", "WorldMapServerListPanel"),
                ),
                (
                    "service_boundary",
                    WORLD_HUD_WIDGET["cpp"],
                    (
                        "ServerSelectionUnavailable",
                        "HandleServerSelectionUnavailableClicked",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "player_profile",
        "display_name": "玩家资料",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_player/UIPlayerData/UI_Win_PlayerData.prefab"
        ),
        "runtime_widget": {**MODAL_WIDGET, "open_method": "OpenPlayerProfile"},
        "entries": [
            city_entry("PlayerProfileHitButton", "HandleOpenPlayerProfile"),
            world_entry(
                "WorldMapPlayerProfileHitButton",
                "HandleOpenPlayerProfileClicked",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    MODAL_WIDGET["cpp"],
                    ("BuildPlayerProfile", "MakePlayerDataButton"),
                ),
                (
                    "real_local_loop",
                    "Source/Rok/RokUnityRuntimeStateSubsystem.cpp",
                    (
                        "ParsePlayerProfileState",
                        "local_preview_score",
                    ),
                ),
                (
                    "service_boundary",
                    MODAL_WIDGET["cpp"],
                    (
                        "ShowProfileBoundaryFeedback",
                        "HandleProfileRenameClicked",
                        "HandleProfileActionAddClicked",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "player_head_selection",
        "display_name": "头像与头像框",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_player/UIPlayerHeadSet/UI_Win_PlayerHeadPic.prefab"
        ),
        "runtime_widget": {
            **MODAL_WIDGET,
            "open_method": "OpenPlayerHeadSelection",
        },
        "entries": [
            entry(
                "player_profile",
                MODAL_WIDGET["class"],
                MODAL_WIDGET["header"],
                MODAL_WIDGET["cpp"],
                "ProfilePictureButton",
                "HandleProfileAvatarClicked",
            ),
            entry(
                "player_profile",
                MODAL_WIDGET["class"],
                MODAL_WIDGET["header"],
                MODAL_WIDGET["cpp"],
                "ProfileModifyButton",
                "HandleProfileAvatarClicked",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    MODAL_WIDGET["cpp"],
                    (
                        "BuildPlayerHeadSelection",
                        "PlayerHeadGridPanel",
                        "PlayerHeadPreviewPanel",
                    ),
                ),
                (
                    "service_boundary",
                    MODAL_WIDGET["cpp"],
                    (
                        "PlayerHeadServerBoundaryState",
                        "Role_SetRoleHead",
                        "state_unchanged=1 authority=server",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "player_power_details",
        "display_name": "玩家战力详情",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_player/UIPlayerData/UI_PlayerPowerView.prefab"
        ),
        "runtime_widget": {
            **MODAL_WIDGET,
            "open_method": "OpenPlayerPowerDetails",
        },
        "entries": [
            entry(
                "player_profile",
                MODAL_WIDGET["class"],
                MODAL_WIDGET["header"],
                MODAL_WIDGET["cpp"],
                "ProfileMoreButton",
                "HandleProfileMoreClicked",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    MODAL_WIDGET["cpp"],
                    (
                        "BuildPlayerPowerDetails",
                        "PlayerPowerScroll",
                        "PlayerPowerComposition",
                    ),
                ),
                (
                    "real_local_loop",
                    MODAL_WIDGET["cpp"],
                    (
                        "ProfileState.LocalPreviewScore",
                        "ProfileState.LocalTroopPower",
                        "ProfileState.LocalHeroScore",
                    ),
                ),
                (
                    "service_boundary",
                    MODAL_WIDGET["cpp"],
                    (
                        "PlayerPowerServerBoundaryState",
                        "正式统计待同步",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "player_ranking",
        "display_name": "玩家排行榜",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_player/UIRanking/UI_Win_Ranking.prefab"
        ),
        "runtime_widget": {
            **MODAL_WIDGET,
            "open_method": "OpenPlayerRanking",
        },
        "entries": [
            entry(
                "player_profile",
                MODAL_WIDGET["class"],
                MODAL_WIDGET["header"],
                MODAL_WIDGET["cpp"],
                "ProfileRankingButton",
                "HandleProfileRankingClicked",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    MODAL_WIDGET["cpp"],
                    (
                        "BuildPlayerRanking",
                        "PlayerRankingCategoryButton_%d",
                        "PlayerRankingListHeader",
                    ),
                ),
                (
                    "service_boundary",
                    MODAL_WIDGET["cpp"],
                    (
                        "PlayerRankingServerBoundaryState",
                        "Rank_ShowRankFirst.response",
                        "配置预览 · 榜单服务尚未接入",
                        "state_unchanged=1",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "player_civilization",
        "display_name": "文明选择与预览",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_login/UILogin/CreateCharView.prefab"
        ),
        "runtime_widget": {
            **MODAL_WIDGET,
            "open_method": "OpenCivilizationSelection",
        },
        "entries": [
            entry(
                "player_profile",
                MODAL_WIDGET["class"],
                MODAL_WIDGET["header"],
                MODAL_WIDGET["cpp"],
                "ProfileCivilizationButton",
                "HandleProfileCivilizationClicked",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    MODAL_WIDGET["cpp"],
                    (
                        "BuildCivilizationSelection",
                        "CivilizationSelectionBackground",
                        "CivilizationListShade",
                    ),
                ),
                (
                    "real_local_loop",
                    MODAL_WIDGET["cpp"],
                    (
                        "Catalog->GetCivilizations()",
                        "SelectedCivilizationId",
                        "FormatCivilizationBonusDescription",
                    ),
                ),
                (
                    "service_boundary",
                    MODAL_WIDGET["cpp"],
                    (
                        "CivilizationSelectionServerBoundaryState",
                        "profile_state_unchanged=1 authority=server",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "player_civilization_flag_item",
        "display_name": "文明选择条目",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_login/UILogin/CreateCharFlagItem.prefab"
        ),
        "runtime_widget": {
            **MODAL_WIDGET,
            "open_method": "OpenCivilizationSelection",
        },
        "entries": [
            entry(
                "civilization_selection",
                MODAL_WIDGET["class"],
                MODAL_WIDGET["header"],
                MODAL_WIDGET["cpp"],
                "CivilizationChoiceButton_%d",
                "HandleProfileSubscreenAction",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    MODAL_WIDGET["cpp"],
                    (
                        "CivilizationFlag_%d",
                        "CivilizationSelectedFrame_%d",
                        "CivilizationLockedState_%d",
                    ),
                ),
                (
                    "real_local_loop",
                    MODAL_WIDGET["cpp"],
                    (
                        "Rok civilization selection changed",
                        "SelectedCivilizationId = Civilizations[Index].Id",
                    ),
                ),
                (
                    "service_boundary",
                    MODAL_WIDGET["cpp"],
                    (
                        "!Civilizations[Index].bLocked",
                        "state_unchanged=1 authority=server",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "player_civilization_confirmation",
        "display_name": "文明更换确认",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_login/UILogin/UI_Win_CivilizationChange.prefab"
        ),
        "runtime_widget": {
            **MODAL_WIDGET,
            "open_method": "OpenCivilizationChangeConfirmation",
        },
        "entries": [
            entry(
                "civilization_selection",
                MODAL_WIDGET["class"],
                MODAL_WIDGET["header"],
                MODAL_WIDGET["cpp"],
                "CivilizationContinueButton",
                "HandleCivilizationContinueClicked",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    MODAL_WIDGET["cpp"],
                    (
                        "BuildCivilizationChangeConfirmation",
                        "CivilizationConfirmationScroll",
                        "CivilizationSubmitButton",
                    ),
                ),
                (
                    "service_boundary",
                    MODAL_WIDGET["cpp"],
                    (
                        "Role_ChangeCivilization 未接入",
                        "ConfirmButton->SetIsEnabled(false)",
                        "state_unchanged=1",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "settings_hub",
        "display_name": "设置中心",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_player/UIPlayerData/UI_Win_Setting.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenSettingsHub",
        },
        "entries": [
            entry(
                "player_profile",
                MODAL_WIDGET["class"],
                MODAL_WIDGET["header"],
                MODAL_WIDGET["cpp"],
                "ProfileSettingsButton",
                "HandleProfileSettingsClicked",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildSettingsHub",
                        "SettingsHubGeneralButton",
                        "entries=7 active_routes=4",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "city_manager",
        "display_name": "城市管理",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_build/UICityManager/UI_Win_CityManager.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenCityManager",
        },
        "entries": [
            city_entry("PlayerArrowButton", "HandleOpenCityManager"),
            world_entry(
                "WorldMapPlayerArrowHitButton",
                "HandleOpenCityManagerClicked",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildCityManager",
                        "CityManagerListScroll",
                        "CityManagerGroupButton_",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuffUseBoundary",
                        "RelocateBoundary",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "army_show",
        "display_name": "行军队列",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_troops/UIArmyShow/UI_Win_ArmyShow.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenArmyShow",
        },
        "entries": [
            city_entry("ArmyQueueButton", "HandleOpenArmyShow"),
            entry(
                "world_hud", CITY_HUD_WIDGET["class"], CITY_HUD_WIDGET["header"], CITY_HUD_WIDGET["cpp"],
                "ArmyQueueButton", "HandleOpenArmyShow",
                route_evidence=(WORLD_HUD_WIDGET["cpp"], ("ConfigureWorldMapTacticalOverlay",)),
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildArmyShow",
                        "ArmyShowListScroll",
                        "ArmyShowRulesButton",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    (
                        "rendered_rows=0 placeholder_rows=0 authority=server",
                        "RulesDescription",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "general_settings",
        "display_name": "通用设置",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_player/UIPlayerData/"
            "UI_Win_GeneralSetting.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenGeneralSettings",
        },
        "entries": [
            entry(
                "settings_hub",
                FEATURE_WIDGET["class"],
                FEATURE_WIDGET["header"],
                FEATURE_WIDGET["cpp"],
                "SettingsHubGeneralButton",
                "HandleIndexedAction",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildGeneralSettings",
                        "audio=persisted_only_disabled",
                        "personality=unconsumed_disabled",
                    ),
                ),
                (
                    "real_local_loop",
                    "Source/Rok/RokClientSettingsSubsystem.cpp",
                    (
                        "SetGraphicsQualityTier",
                        "SetFrameRateOption",
                        "ApplyVideoSettings",
                        "SetOverallScalabilityLevel",
                        "SetFrameRateLimit",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "notification_settings",
        "display_name": "通知设置",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_player/UIPlayerData/UI_Win_SettingNotice.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenNotificationSettings",
        },
        "entries": [
            entry(
                "settings_hub",
                FEATURE_WIDGET["class"],
                FEATURE_WIDGET["header"],
                FEATURE_WIDGET["cpp"],
                "SettingsHubNotificationButton",
                "HandleIndexedAction",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildNotificationSettings",
                        "NotificationSettingsScroll",
                        "rows=10",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    (
                        "state=unknown authority=server",
                        "optimistic_update=0",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "language_settings",
        "display_name": "语言设置",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_setting/UILanguage/UI_Win_Language.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenLanguageSettings",
        },
        "entries": [
            entry(
                "settings_hub",
                FEATURE_WIDGET["class"],
                FEATURE_WIDGET["header"],
                FEATURE_WIDGET["cpp"],
                "SettingsHubLanguageButton",
                "HandleIndexedAction",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildLanguageSettings",
                        "LanguageOptionChineseButton",
                        "LanguageConfirmButton",
                    ),
                ),
                (
                    "real_local_loop",
                    FEATURE_WIDGET["cpp"],
                    (
                        "LanguageDraftId = RokLanguageSettingsLayout::AppliedLanguageId",
                        "LanguageDraftId = RokLanguageSettingsLayout::SourceId(ActionId-1460)",
                        "UpdateLanguageDraftSelection",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    (
                        "CanCloseWithoutReload(LanguageDraftId)",
                        "draft only, no locale or preference write",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "help",
        "display_name": "帮助目录",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_player/UIHelper/UI_Win_Helper.prefab"
        ),
        "runtime_widget": {**FEATURE_WIDGET, "open_method": "OpenHelp"},
        "entries": [
            entry("settings_hub", FEATURE_WIDGET["class"], FEATURE_WIDGET["header"],
                  FEATURE_WIDGET["cpp"], "SettingsHubHelpButton", "HandleIndexedAction"),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                ("preview_shell", FEATURE_WIDGET["cpp"], ("BuildHelp", "SettingsHelpWidget")),
                ("real_local_loop", "Source/Rok/RokHelpWidget.cpp",
                 ("RebuildList", "VisibleQuestionIds.Contains", "HelpBackButton")),
                ("service_boundary", "Source/Rok/RokHelpWidget.cpp",
                 ("HelpCustomerServiceBoundary", "客服平台尚未接入")),
            ),
        ),
    },
    {
        "id": "account_settings",
        "display_name": "账号管理",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_login/UILogin/UI_Win_AccountMgr.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenAccountSettings",
        },
        "entries": [
            entry(
                "settings_hub",
                FEATURE_WIDGET["class"],
                FEATURE_WIDGET["header"],
                FEATURE_WIDGET["cpp"],
                "SettingsHubAccountButton",
                "HandleIndexedAction",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildAccountSettings",
                        "AccountDeviceBindButton",
                        "AccountChangeButton",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    (
                        "igg_sdk=missing",
                        "bind=disabled",
                        "change_account=disabled",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "army_statistics",
        "display_name": "部队统计",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_troops/UIArmyShow/"
            "UI_Win_ArmyConst.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenArmyStatistics",
        },
        "entries": [
            entry(
                "player_profile",
                MODAL_WIDGET["class"],
                MODAL_WIDGET["header"],
                MODAL_WIDGET["cpp"],
                "ProfileArmyButton",
                "HandleProfileArmyClicked",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    ("BuildArmyStatistics", "GetStarterTroopStacks"),
                ),
                (
                    "real_local_loop",
                    FEATURE_WIDGET["cpp"],
                    ("BuildArmyStatistics", "GetStarterTroopStacks"),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    ("OutsideEmpty", "ArmyShow"),
                ),
            ),
        ),
    },
    {
        "id": "store",
        "display_name": "普通商店",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_item/UINormalStore/"
            "UI_Win_NormalStore.prefab"
        ),
        "runtime_widget": {**FEATURE_WIDGET, "open_method": "OpenStore"},
        "entries": [],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    ("BuildStore", "VisibleStoreItemIds"),
                ),
                (
                    "real_local_loop",
                    "Source/Rok/RokUnityEconomyCatalogSubsystem.cpp",
                    ("GetShopItemsByCategory", "FindItemById"),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    ("PurchaseBoundary", "Shop_BuyShopItem"),
                ),
            ),
        ),
    },
    {
        "id": "train_army",
        "display_name": "训练部队",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_troops/UIArmyTrain/UI_Win_TrainArmy.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenTrainArmy",
        },
        "entries": [
            building_entry(
                "BuildingFeatureButton",
                "HandleFeatureClicked",
                route_evidence=(
                    "Source/Rok/RokMainHudWidget.cpp",
                    (
                        "State.UnityBuildingTypeId >= 9",
                        "State.UnityBuildingTypeId <= 12",
                        "FeatureScreenWidget->OpenTrainArmy",
                    ),
                ),
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    ("BuildTrainArmy", "SelectedTrainTier"),
                ),
                (
                    "real_local_loop",
                    FEATURE_WIDGET["cpp"],
                    ("GetTrainingCapacity", "GetTroopsForArmyType"),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    ("Role_TrainArmy", "TrainingBoundary"),
                ),
            ),
        ),
    },
    {
        "id": "speed_up",
        "display_name": "队列加速",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_item/UIAddSpeed/UI_Win_AddSpeed.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenSpeedUp",
        },
        "entries": [
            building_entry(
                "BuildingFeatureButton",
                "HandleFeatureClicked",
                route_evidence=(
                    "Source/Rok/RokMainHudWidget.cpp",
                    (
                        "State.bUpgradeInProgress",
                        "FeatureScreenWidget->OpenSpeedUp",
                    ),
                ),
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    ("BuildSpeedUp", "VisibleSpeedUpItemIds"),
                ),
                (
                    "real_local_loop",
                    FEATURE_WIDGET["cpp"],
                    ("GetRemainingUpgradeSeconds", "SpeedUpQueueType"),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    ("Role_SpeedUp", "UseBoundary"),
                ),
            ),
        ),
    },
    {
        "id": "resources",
        "display_name": "资源快捷使用",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_item/UIQuickUseResources/"
            "UI_Win_QuickUseResources.prefab"
        ),
        "runtime_widget": {
            **MODAL_WIDGET,
            "open_method": "OpenResourceInventory",
        },
        "entries": [
            city_entry("FoodResourceHitButton", "HandleOpenFoodResources"),
            city_entry("WoodResourceHitButton", "HandleOpenWoodResources"),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    MODAL_WIDGET["cpp"],
                    (
                        "BuildResourceInventory",
                        "MakeResourceRow",
                        "ResourceInventoryEmptyState",
                        "ResourceInventoryReferenceTag",
                    ),
                ),
                (
                    "service_boundary",
                    MODAL_WIDGET["cpp"],
                    (
                        "HandlePreviewItemAction",
                        "inventory/server command boundary",
                        "Rok resource inventory: category=",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "building_info",
        "display_name": "建筑信息",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_build/UIBuildingDesc/UI_Win_BuildingDesc.prefab"
        ),
        "runtime_widget": {**MODAL_WIDGET, "open_method": "OpenBuildingInfo"},
        "entries": [
            building_entry("BuildingInfoButton", "HandleInfoClicked"),
        ],
        "behavior": behavior(
            real_local_loop=True,
            evidence=(
                (
                    "real_local_loop",
                    MODAL_WIDGET["cpp"],
                    ("BuildBuildingInfo", "GetBuildingStateForActor"),
                ),
            ),
        ),
    },
    {
        "id": "building_upgrade",
        "display_name": "建筑升级",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_build/UIBuildingDesc/UI_Win_BuildingUpdate.prefab"
        ),
        "runtime_widget": {
            **MODAL_WIDGET,
            "open_method": "OpenBuildingUpgrade",
        },
        "entries": [
            building_entry("BuildingUpgradeButton", "HandleUpgradeClicked"),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    MODAL_WIDGET["cpp"],
                    ("BuildBuildingUpgrade", "Preview"),
                ),
                (
                    "real_local_loop",
                    MODAL_WIDGET["cpp"],
                    ("HandleStartUpgrade", "Subsystem->StartUpgrade"),
                ),
                (
                    "service_boundary",
                    MODAL_WIDGET["cpp"],
                    ("GemStateNotMigrated", "HandleInstantUpgrade"),
                ),
            ),
        ),
    },
    {
        "id": "building_menu",
        "display_name": "建筑快捷菜单",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_map/UIMainCity/UI_Pop_BuildingMenu.prefab"
        ),
        "runtime_widget": {
            **BUILDING_PANEL_WIDGET,
            "open_method": "SetSelectedBuilding",
        },
        "runtime_activation": {
            "path": "Source/Rok/RokMainHudWidget.cpp",
            "tokens": [
                "ConstructWidget<URokBuildingPanelWidget>",
                "RootCanvas->AddChildToCanvas(BuildingPanelWidget)",
            ],
        },
        "entries": [],
        "behavior": behavior(
            real_local_loop=True,
            evidence=(
                (
                    "real_local_loop",
                    BUILDING_PANEL_WIDGET["cpp"],
                    ("SetSelectedBuilding", "GetBuildingStateForActor"),
                ),
            ),
        ),
    },
    {
        "id": "build_city",
        "display_name": "建造城市",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_build/UIBuildCity/UI_Win_BuildCity.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenBuildCity",
        },
        "entries": [
            city_entry("BuildCityButton", "HandleOpenBuildCity"),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildBuildCity",
                        "BuildCityBuildingScroll",
                        "BuildCityCardPreview_",
                        "BuildCityRightAlignmentSpacer",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildableBuildCityTypeIds",
                        "placement boundary type=%d",
                        "ShowBoundaryFeedback",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "worker_queue",
        "display_name": "工人队列",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_build/UIWorker/UI_Win_Worker.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenWorkerQueue",
        },
        "entries": [
            building_entry(
                "BuildingFeatureButton",
                "HandleFeatureClicked",
                route_evidence=(
                    "Source/Rok/RokMainHudWidget.cpp",
                    (
                        "State.UnityBuildingTypeId == 7",
                        "FeatureScreenWidget->OpenWorkerQueue",
                    ),
                ),
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    ("BuildWorkerQueue", "WorkerQueuePrimaryAction"),
                ),
                (
                    "real_local_loop",
                    FEATURE_WIDGET["cpp"],
                    (
                        "GetFirstActiveUpgradeActor",
                        "WorkerQueuePrimaryProgress",
                        "OpenSpeedUp",
                        "OpenBuildCity",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    (
                        "SecondQueueUnlockOrExtendBoundary",
                        "ShowBoundaryFeedback",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "hospital",
        "display_name": "医院",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_build/UIHospital/UI_Win_Hospital.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenHospital",
        },
        "entries": [
            building_entry(
                "BuildingFeatureButton",
                "HandleFeatureClicked",
                route_evidence=(
                    "Source/Rok/RokMainHudWidget.cpp",
                    (
                        "State.UnityBuildingTypeId == 14",
                        "FeatureScreenWidget->OpenHospital",
                    ),
                ),
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    ("BuildHospital", "HospitalCapacityValue"),
                ),
                (
                    "real_local_loop",
                    FEATURE_WIDGET["cpp"],
                    ("GetHospitalCapacity", "GetBuildingStateForActor"),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    ("TreatmentBoundary", "ShowBoundaryFeedback"),
                ),
            ),
        ),
    },
    {
        "id": "activity",
        "display_name": "活动中心",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_activity/UIEventDate/UI_Win_EventDate.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenActivityCenter",
        },
        "entries": [
            city_entry("ActivityButton", "HandleOpenActivityCenter"),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    ("BuildActivityCenter", "ActivityPreviewSecondsRemaining", "ActivityPreviewTag"),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    ("ActivityHistoryButton", "RankBoundary", "Rok activity screen: tab="),
                ),
            ),
        ),
    },
    {
        "id": "mail",
        "display_name": "邮件",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_mail/UIMail/UI_Win_Mail.prefab"
        ),
        "runtime_widget": {**FEATURE_WIDGET, "open_method": "OpenMail"},
        "entries": [
            city_entry("MailFeatureHitButton", "HandleOpenMail"),
            world_entry("WorldMapMailButton", "HandleOpenMailClicked"),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildMail",
                        "bReferenceMailCaptureState",
                        "MailReferencePreviewNotice",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    (
                        "MailCompose",
                        "MailBoundary",
                        "ShowBoundaryFeedback",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "tasks",
        "display_name": "任务",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_task/UIQuest/UI_Win_Quest.prefab"
        ),
        "runtime_widget": {**FEATURE_WIDGET, "open_method": "OpenTasks"},
        "entries": [
            city_entry("TaskEntryButton", "HandleOpenTasks"),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    ("BuildTasks", "TaskMainScroll", "bTaskTestFixtureState"),
                ),
                (
                    "real_local_loop",
                    FEATURE_WIDGET["cpp"],
                    (
                        "GetActiveTask",
                        "GetSideTasksBySortOrder",
                        "ComputeBootstrapSideTaskCandidates",
                        "TaskMainTag",
                        "TaskSideTag",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    (
                        "Rok task action: task=%d page=%s state=%s authority=%s intent=%s state_unchanged=1.",
                        "TaskClaimUnavailable",
                        "TaskGoUnavailable",
                        "ShowBoundaryFeedback",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "commander",
        "display_name": "统帅",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_hero/UICaptain/UI_IF_Captain.prefab"
        ),
        "runtime_widget": {**FEATURE_WIDGET, "open_method": "OpenCommander"},
        "entries": [
            city_entry(
                "CommanderFeatureButton",
                "HandleOpenCommander",
                factory_seed="CommanderFeature",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildCommander",
                        "CommanderHeroShowStage",
                        "CommanderPortraitPath",
                        "RokCommanderHero=",
                    ),
                ),
                (
                    "real_local_loop",
                    FEATURE_WIDGET["cpp"],
                    (
                        "GetHeroIds",
                        "GetHeroSkill",
                        "GetStarterHeroes",
                        "VisibleCommanderIds",
                        "CommanderRosterScroll",
                        "SelectedCommanderSortType",
                        "CommanderSortAction",
                        "CommanderSortOption_%d",
                        "CommanderOwnedPanel",
                        "CommanderSummonPanel",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    (
                        "Rok commander skill upgrade boundary:",
                        "Rok commander talent boundary:",
                        "Rok commander summon boundary:",
                        "ShowBoundaryFeedback",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "items",
        "display_name": "道具背包",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_item/UIBag/UI_Win_Bag.prefab"
        ),
        "runtime_widget": {**FEATURE_WIDGET, "open_method": "OpenItems"},
        "entries": [
            city_entry(
                "ItemsFeatureButton",
                "HandleOpenItems",
                factory_seed="ItemsFeature",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildItems",
                        "RokItemsPreviewFixture",
                        "bItemsTestFixtureState",
                        "ItemsInventoryScroll",
                    ),
                ),
                (
                    "real_local_loop",
                    FEATURE_WIDGET["cpp"],
                    (
                        "ItemsInventoryEmptyState",
                        "VisibleItemsStacks",
                        "SelectedItemsUseCount",
                        "ResolveEconomyItemDescription",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    (
                        "Rok items use boundary",
                        "state_unchanged=1",
                        "authority=server",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "alliance",
        "display_name": "联盟",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_guild/UIGuildMain/UI_Win_GuildMain.prefab"
        ),
        "runtime_widget": {**FEATURE_WIDGET, "open_method": "OpenAlliance"},
        "entries": [
            city_entry(
                "AllianceFeatureButton",
                "HandleOpenAlliance",
                factory_seed="AllianceFeature",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildAlliance",
                        "RokAlliancePreviewFixture",
                        "AllianceJoinEmptyState",
                        "AllianceJoinServiceState",
                        "AllianceSearchInput",
                        "AllianceCreateNameInput",
                        "AllianceJoinedFixtureTag",
                        "AllianceJoinFixtureTag",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    (
                        "Rok alliance search boundary:",
                        "Rok alliance join boundary:",
                        "Rok alliance create boundary:",
                        "Rok alliance screen: page=",
                        "state_unchanged=1",
                        "authority=server",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "vip",
        "display_name": "VIP",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_player/UIVIP/UI_Win_Vip.prefab"
        ),
        "runtime_widget": {**FEATURE_WIDGET, "open_method": "OpenVip"},
        "entries": [
            city_entry("VipHitButton", "HandleOpenVip"),
            world_entry("WorldMapVipHitButton", "HandleOpenVipClicked"),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    ("BuildVip", "SelectedVipPreviewLevel", "VipFixtureTag"),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    ("DailyPointBoundary", "PurchaseBoundary", "Rok VIP screen:"),
                ),
            ),
        ),
    },
    {
        "id": "chat",
        "display_name": "聊天",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_chat/UIChat/UI_Win_Chat.prefab"
        ),
        "runtime_widget": {**FEATURE_WIDGET, "open_method": "OpenChat"},
        "entries": [
            city_entry("ChatButton", "HandleOpenChat"),
            world_entry("WorldMapChatButton", "HandleOpenChatClicked"),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildChat",
                        "SelectedChatChannel",
                        "ChatMessagesEmptyState",
                        "ChatContactsEmptyState",
                        "ChatEmojiButton",
                        "ChatFixtureTag",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    (
                        "Rok chat send boundary:",
                        "Rok chat screen: channel=",
                        "state_unchanged=1",
                        "authority=server",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "world_position",
        "display_name": "世界坐标定位",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_map/UIMainCity/UI_Pop_WorldPosSearch.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenWorldPositionSearch",
        },
        "entries": [
            city_entry(
                "PositionSearchButton",
                "HandleOpenWorldPositionSearch",
            ),
            world_entry(
                "WorldMapCoordinateSearchButton",
                "HandleOpenPositionSearchClicked",
            ),
        ],
        "behavior": behavior(
            real_local_loop=True,
            evidence=(
                (
                    "real_local_loop",
                    FEATURE_WIDGET["cpp"],
                    (
                        "FocusDisplayCoordinate",
                        "RequestReturnToWorldMapAtDisplayCoordinate",
                    ),
                ),
            ),
        ),
    },
    {
        "id": "bookmarks",
        "display_name": "地图书签",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_map/UIBook/UI_Win_Book.prefab"
        ),
        "runtime_widget": {**FEATURE_WIDGET, "open_method": "OpenMapMarkers"},
        "entries": [
            city_entry("FavoriteButton", "HandleOpenMapMarkers"),
            world_entry(
                "WorldMapFavoriteButton",
                "HandleOpenMapMarkersClicked",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    ("BuildMapMarkers", "WorldMapPreviewMarkers"),
                ),
                (
                    "real_local_loop",
                    FEATURE_WIDGET["cpp"],
                    (
                        "FocusDisplayCoordinate",
                        "RequestReturnToWorldMapAtDisplayCoordinate",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    ("AddBoundary", "ShowBoundaryFeedback"),
                ),
            ),
        ),
    },
    {
        "id": "resource_search",
        "display_name": "资源搜索",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_map/UISearchRes/UI_IF_SearchRes.prefab"
        ),
        "runtime_widget": {
            **FEATURE_WIDGET,
            "open_method": "OpenResourceSearch",
        },
        "entries": [
            world_entry(
                "WorldMapSearchButton",
                "HandleOpenResourceSearchClicked",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    (
                        "BuildResourceSearch",
                        "ResourceSearchTouchCloseMask",
                        "ResourceSearchBarbarianLevel",
                        "ResourceSearchResourceLevel",
                        "ResourceSearchSlider",
                    ),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    ("SearchBoundary", "ShowBoundaryFeedback"),
                ),
            ),
        ),
    },
    {
        "id": "expedition",
        "display_name": "远征",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_battle/UIExpeditionFight/"
            "UI_IF_ExpeditionFight.prefab"
        ),
        "runtime_widget": {**FEATURE_WIDGET, "open_method": "OpenExpedition"},
        "entries": [
            city_entry(
                "CampaignFeatureButton",
                "HandleOpenExpedition",
                factory_seed="CampaignFeature",
            ),
        ],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    ("BuildExpedition", "SelectedExpeditionStage"),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    ("ChallengeBoundary", "ShowBoundaryFeedback"),
                ),
            ),
        ),
    },
    {
        "id": "war_warning",
        "display_name": "战争预警",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_warn/UIWarn/UI_Win_Warn.prefab"
        ),
        "runtime_widget": {**MODAL_WIDGET, "open_method": "OpenWarWarning"},
        "entries": [],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    MODAL_WIDGET["cpp"],
                    ("BuildWarWarning", "OpenWarWarning"),
                ),
                (
                    "service_boundary",
                    MODAL_WIDGET["cpp"],
                    ("WarWarningServiceBoundaryState",),
                ),
            ),
        ),
    },
    {
        "id": "charge",
        "display_name": "充值中心",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_charge/UICharge/UI_IF_Charge.prefab"
        ),
        "runtime_widget": {**FEATURE_WIDGET, "open_method": "OpenCharge"},
        "entries": [
            city_entry("GemResourceHitButton", "HandleOpenCharge"),
        ],
        "behavior": behavior(
            preview_shell=True,
            real_local_loop=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    ("BuildCharge", "ChargeServiceBoundaryState"),
                ),
                (
                    "real_local_loop",
                    FEATURE_WIDGET["cpp"],
                    ("BuildCharge", "SelectedChargePageType"),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    ("ChargeServiceBoundaryState",),
                ),
            ),
        ),
    },
    {
        "id": "buff_list",
        "display_name": "增益列表",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_build/UICityManager/UI_Pop_BuffList.prefab"
        ),
        "runtime_widget": {**MODAL_WIDGET, "open_method": "OpenBuffList"},
        "entries": [],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    MODAL_WIDGET["cpp"],
                    ("BuildBuffList", "OpenBuffList"),
                ),
                (
                    "service_boundary",
                    MODAL_WIDGET["cpp"],
                    ("BuffListServiceBoundaryState",),
                ),
            ),
        ),
    },
    {
        "id": "gem_shop",
        "display_name": "宝石商店",
        "unity_prefab": (
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/"
            "ui_prefabs_charge/UICharge/UI_Item_ChargeGemShop.prefab"
        ),
        "runtime_widget": {**FEATURE_WIDGET, "open_method": "OpenGemShop"},
        "entries": [],
        "behavior": behavior(
            preview_shell=True,
            service_boundary=True,
            evidence=(
                (
                    "preview_shell",
                    FEATURE_WIDGET["cpp"],
                    ("BuildGemShop", "OpenGemShop", "GemShopBalance", "GemShopPackageShade_"),
                ),
                (
                    "service_boundary",
                    FEATURE_WIDGET["cpp"],
                    ("RokGemShop", "PurchaseBoundary", "Rok gem shop screen:"),
                ),
            ),
        ),
    },
]


def read_text(path):
    return (PROJECT_ROOT / path).read_text(encoding="utf-8", errors="ignore")


def tokens_present(path, tokens):
    source_path = PROJECT_ROOT / path
    if not source_path.is_file():
        return False
    source = read_text(path)
    return all(token in source for token in tokens)


def inspect_widget(widget):
    header_path = PROJECT_ROOT / widget["header"]
    cpp_path = PROJECT_ROOT / widget["cpp"]
    header = read_text(widget["header"]) if header_path.is_file() else ""
    cpp = read_text(widget["cpp"]) if cpp_path.is_file() else ""
    class_name = widget["class"]
    method = widget["open_method"]
    inheritance_pattern = re.compile(
        rf"class\s+(?:\w+_API\s+)?{re.escape(class_name)}\s*"
        rf":\s*public\s+UUserWidget\b"
    )
    return {
        **widget,
        "header_exists": header_path.is_file(),
        "cpp_exists": cpp_path.is_file(),
        "class_declared": bool(re.search(rf"\bclass\b[^\n]*\b{class_name}\b", header)),
        "inherits_user_widget": bool(inheritance_pattern.search(header)),
        "method_declared": bool(
            re.search(rf"\b{re.escape(method)}\s*\(", header)
        ),
        "method_defined": bool(
            re.search(
                rf"\b{re.escape(class_name)}::{re.escape(method)}\s*\(",
                cpp,
            )
        ),
    }


def inspect_entry(declared):
    header_path = PROJECT_ROOT / declared["header"]
    cpp_path = PROJECT_ROOT / declared["cpp"]
    header = read_text(declared["header"]) if header_path.is_file() else ""
    cpp = read_text(declared["cpp"]) if cpp_path.is_file() else ""
    owner_class = declared["owner_class"]
    handler = declared["handler"]
    result = {
        **declared,
        "owner_header_exists": header_path.is_file(),
        "owner_cpp_exists": cpp_path.is_file(),
        "owner_class_declared": bool(
            re.search(rf"\bclass\b[^\n]*\b{owner_class}\b", header)
        ),
        "handler_declared": bool(
            re.search(rf"\b{re.escape(handler)}\s*\(", header)
        ),
        "handler_defined": bool(
            re.search(
                rf"\b{re.escape(owner_class)}::{re.escape(handler)}\s*\(",
                cpp,
            )
        ),
        "binding_present": f"&{owner_class}::{handler}" in cpp,
    }
    factory = declared.get("control_factory")
    if factory:
        result["control_present"] = (
            f'TEXT("{factory["seed"]}")' in cpp
            and 'TEXT("%sButton")' in cpp
            and "MakeFeatureButton" in cpp
        )
    else:
        result["control_present"] = (
            f'TEXT("{declared["control_literal"]}")' in cpp
        )
    route_evidence = declared.get("route_evidence")
    if route_evidence:
        result["route_evidence"] = {
            **route_evidence,
            "tokens_present": tokens_present(
                route_evidence["path"],
                route_evidence["tokens"],
            ),
        }
    return result


def inspect_behavior(declared):
    result = {
        key: bool(declared[key])
        for key in (
            "preview_shell",
            "real_local_loop",
            "service_boundary",
            "unimplemented",
        )
    }
    evidence = []
    for item in declared["evidence"]:
        evidence.append(
            {
                **item,
                "tokens_present": tokens_present(
                    item["path"],
                    item["tokens"],
                ),
            }
        )
    result["evidence"] = evidence
    return result


def build_coverage():
    if not FULL_CATALOG_PATH.is_file():
        raise FileNotFoundError(
            f"Missing full Unity UI catalog: {FULL_CATALOG_PATH}"
        )
    full_catalog = json.loads(FULL_CATALOG_PATH.read_text(encoding="utf-8"))
    prefab_records = {
        item["path"]: item for item in full_catalog.get("ui_prefabs", [])
    }

    mapping_records = []
    mapped_paths = set()
    for declared in MAPPINGS:
        prefab_path = declared["unity_prefab"]
        prefab_record = prefab_records.get(prefab_path)
        if prefab_record:
            mapped_paths.add(prefab_path)
        mapping_records.append(
            {
                "id": declared["id"],
                "display_name": declared["display_name"],
                "unity_source": {
                    "prefab": prefab_path,
                    "catalog_record_exists": prefab_record is not None,
                    "family": (
                        prefab_record.get("family")
                        if prefab_record
                        else None
                    ),
                    "basename": (
                        prefab_record.get("basename")
                        if prefab_record
                        else None
                    ),
                    "view_classes": (
                        prefab_record.get("view_classes", [])
                        if prefab_record
                        else []
                    ),
                },
                "runtime_widget": inspect_widget(
                    declared["runtime_widget"]
                ),
                "runtime_activation": (
                    {
                        **declared["runtime_activation"],
                        "tokens_present": tokens_present(
                            declared["runtime_activation"]["path"],
                            declared["runtime_activation"]["tokens"],
                        ),
                    }
                    if declared.get("runtime_activation")
                    else None
                ),
                "entries": [
                    inspect_entry(item) for item in declared["entries"]
                ],
                "behavior": inspect_behavior(declared["behavior"]),
            }
        )

    ui_prefabs = full_catalog.get("ui_prefabs", [])
    unmapped = [
        {
            "path": item["path"],
            "basename": item.get("basename"),
            "family": item.get("family"),
            "implementation_state": "unimplemented",
        }
        for item in ui_prefabs
        if item["path"] not in mapped_paths
    ]
    unmapped_family_counts = Counter(
        item["family"] for item in unmapped
    )
    entry_count = sum(len(item["entries"]) for item in mapping_records)
    unique_entry_control_count = len(
        {
            (
                declared_entry["owner_class"],
                declared_entry["control_name"],
            )
            for item in mapping_records
            for declared_entry in item["entries"]
        }
    )
    hud_entry_count = sum(
        1
        for item in mapping_records
        for declared_entry in item["entries"]
        if declared_entry["kind"] == "hud_entry"
    )
    behavior_counts = {
        state: sum(
            1
            for item in mapping_records
            if item["behavior"][state]
        )
        for state in (
            "preview_shell",
            "real_local_loop",
            "service_boundary",
            "unimplemented",
        )
    }
    runtime_widget_count = sum(
        1
        for item in mapping_records
        if item["runtime_widget"]["inherits_user_widget"]
        and item["runtime_widget"]["method_declared"]
        and item["runtime_widget"]["method_defined"]
    )
    runtime_widget_classes = {
        item["runtime_widget"]["class"]
        for item in mapping_records
        if item["runtime_widget"]["inherits_user_widget"]
        and item["runtime_widget"]["method_declared"]
        and item["runtime_widget"]["method_defined"]
    }
    coverage = {
        "schema": "rok.unity_ui_runtime_coverage.v1",
        "inputs": {
            "full_catalog": FULL_CATALOG_PATH.relative_to(
                PROJECT_ROOT
            ).as_posix(),
            "full_catalog_schema": full_catalog.get("schema"),
            "cpp_root": "Source/Rok",
        },
        "summary": {
            "full_catalog_ui_prefab_count": len(ui_prefabs),
            "declared_mapping_count": len(mapping_records),
            "mapped_unique_prefab_count": len(mapped_paths),
            "unimplemented_catalog_prefab_count": len(unmapped),
            "runtime_screen_mapping_count": runtime_widget_count,
            "runtime_widget_class_count": len(runtime_widget_classes),
            "entry_mapping_count": entry_count,
            "unique_entry_control_count": unique_entry_control_count,
            "runtime_screen_widget_count": runtime_widget_count,
            "entry_count": entry_count,
            "hud_entry_count": hud_entry_count,
            **{
                f"{state}_mapping_count": count
                for state, count in behavior_counts.items()
            },
        },
        "definitions": {
            "runtime_screen_mapping": (
                "An explicit mapping whose declared C++ class inherits "
                "UUserWidget and whose open/rebuild method exists."
            ),
            "runtime_widget_class": (
                "A unique C++ UUserWidget owner class used by one or more "
                "runtime mappings."
            ),
            "entry_mapping": (
                "One feature-to-control relation; several mappings may "
                "share a context-sensitive control."
            ),
            "unique_entry_control": (
                "A unique owner-class and control-name pair."
            ),
            "runtime_screen_widget": (
                "Legacy alias for runtime_screen_mapping."
            ),
            "hud_entry": (
                "A named UButton construction path with a bound handler "
                "on the city or world HUD."
            ),
            "preview_shell": (
                "Runtime presentation and local selection state exist, but "
                "the authoritative gameplay result is not claimed."
            ),
            "real_local_loop": (
                "The UI reaches an implemented local camera, scene, query, "
                "or building-state operation."
            ),
            "service_boundary": (
                "The UI explicitly refuses to fabricate a server-authoritative "
                "result or an unavailable platform/localization integration."
            ),
            "unimplemented": (
                "No explicit source-to-UE runtime mapping is declared."
            ),
        },
        "mappings": mapping_records,
        "unimplemented_by_family": [
            {"family": family, "prefab_count": count}
            for family, count in sorted(
                unmapped_family_counts.items(),
                key=lambda item: item[0].casefold(),
            )
        ],
        "unimplemented_catalog_prefabs": unmapped,
    }
    return coverage


def write_markdown(coverage):
    summary = coverage["summary"]
    lines = [
        "# Rok Unity UI Runtime Coverage",
        "",
        "This is a source audit, not a claim that every Unity UI is migrated.",
        "",
        "## Summary",
        "",
        f"- Full Unity UIPrefab inventory: "
        f"{summary['full_catalog_ui_prefab_count']}",
        f"- Explicit source-to-UE mappings: "
        f"{summary['declared_mapping_count']}",
        f"- Unique mapped Unity prefabs: "
        f"{summary['mapped_unique_prefab_count']}",
        f"- Runtime screen mappings: "
        f"{summary['runtime_screen_mapping_count']}",
        f"- Unique C++ UUserWidget owner classes: "
        f"{summary['runtime_widget_class_count']}",
        f"- Verified feature-to-entry relations: "
        f"{summary['entry_mapping_count']} "
        f"(unique controls: {summary['unique_entry_control_count']}; "
        f"HUD relations: {summary['hud_entry_count']})",
        f"- Preview-shell mappings: "
        f"{summary['preview_shell_mapping_count']}",
        f"- Real local-loop mappings: "
        f"{summary['real_local_loop_mapping_count']}",
        f"- Explicit service-boundary mappings: "
        f"{summary['service_boundary_mapping_count']}",
        f"- Unimplemented catalog prefabs: "
        f"{summary['unimplemented_catalog_prefab_count']}",
        "",
        "A mapping can be both a preview shell and a service boundary, or "
        "contain a real local loop while still blocking server-authoritative "
        "operations.",
        "",
        "## Explicit mappings",
        "",
        "| Feature | Unity prefab | UE widget | Entries | Behavior |",
        "|---|---|---|---:|---|",
    ]
    for item in coverage["mappings"]:
        behavior = item["behavior"]
        states = [
            state
            for state in (
                "preview_shell",
                "real_local_loop",
                "service_boundary",
                "unimplemented",
            )
            if behavior[state]
        ]
        lines.append(
            f"| {item['display_name']} | "
            f"`{item['unity_source']['basename']}` | "
            f"`{item['runtime_widget']['class']}` | "
            f"{len(item['entries'])} | {', '.join(states)} |"
        )
    lines.extend(
        [
            "",
            "## Unimplemented Unity prefabs by family",
            "",
            "| Family | Prefabs without explicit runtime mapping |",
            "|---|---:|",
        ]
    )
    for item in coverage["unimplemented_by_family"]:
        lines.append(
            f"| `{item['family']}` | {item['prefab_count']} |"
        )
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    coverage = build_coverage()
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    OUTPUT_JSON.write_text(
        json.dumps(coverage, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    write_markdown(coverage)
    print(
        "Built Unity UI runtime coverage: "
        f"{coverage['summary']['declared_mapping_count']} mappings, "
        f"{coverage['summary']['runtime_widget_class_count']} runtime "
        "widget classes, "
        f"{coverage['summary']['unimplemented_catalog_prefab_count']} "
        "unimplemented prefabs."
    )


if __name__ == "__main__":
    main()
