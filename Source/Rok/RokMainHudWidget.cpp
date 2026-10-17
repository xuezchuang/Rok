#include "RokMainHudWidget.h"
#include "RokPowerGainWidget.h"
#include "RokHudAnimation.h"
#include "RokMailRewardDetails.h"
#include "RokMainInterfaceModuleAnimation.h"
#include "RokUiAnimatedButton.h"
#include "RokCityPlacementPreviewActor.h"
#include "RokPlayerController.h"
#include "RokAudioRouterSubsystem.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RokBuildingPanelWidget.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokStrategyCameraPawn.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokScoutScreenWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Sound/SoundBase.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"

namespace
{
const TCHAR* MainMenuAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu");
const TCHAR* CommonAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");
const TCHAR* PlayerHeadAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_player_head.rok_hud_atlas_player_head");
const TCHAR* VipAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_vip.rok_hud_atlas_vip");
const TCHAR* MapAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_map.rok_hud_atlas_map");
const TCHAR* PlacementAtlasPath = TEXT("/Game/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud.rok_placement_ui_hud");
const TCHAR* PlacementGrayMaterialPath = TEXT("/Game/RokPrototype/UI/PlacementAtlases/M_RokPlacementGrayUI.M_RokPlacementGrayUI");
constexpr float PlacementButtonTintSeconds = 0.1f;
const FLinearColor PlacementButtonDisabledSourceTint(200.0f / 255.0f, 200.0f / 255.0f, 200.0f / 255.0f, 128.0f / 255.0f);
const FLinearColor PlacementButtonHoverSourceTint(245.0f / 255.0f, 245.0f / 255.0f, 245.0f / 255.0f, 1.0f);
const FLinearColor PlacementButtonPressedSourceTint(200.0f / 255.0f, 200.0f / 255.0f, 200.0f / 255.0f, 1.0f);

FLinearColor PlacementSourceTintToSlate(const FLinearColor& SourceTint)
{
	// The original Canvas is Gamma. UImage/Slate stores linear tint, so convert
	// each Color32 value for regular texture graphics. Gray material uses
	// the original Gamma value through its SourceTint parameter.
	return FLinearColor::FromSRGBColor(FColor(
		static_cast<uint8>(FMath::RoundToInt(SourceTint.R * 255.0f)),
		static_cast<uint8>(FMath::RoundToInt(SourceTint.G * 255.0f)),
		static_cast<uint8>(FMath::RoundToInt(SourceTint.B * 255.0f)),
		static_cast<uint8>(FMath::RoundToInt(SourceTint.A * 255.0f))));
}

const FIntPoint MainMenuAtlasSize(1024, 1024);
const FIntPoint CommonAtlasSize(2048, 1024);
const FIntPoint PlayerHeadAtlasSize(512, 2048);
const FIntPoint VipAtlasSize(512, 1024);
const FIntPoint MapAtlasSize(1024, 512);
const FIntPoint PlacementAtlasSize(1024, 1024);

FIntRect SpriteRect(int32 X, int32 Y, int32 Width, int32 Height)
{
	return FIntRect(X, Y, X + Width, Y + Height);
}

const FIntRect PlayerBackgroundRect = SpriteRect(1, 872, 314, 136);
const FIntRect PlayerArrowRect = SpriteRect(647, 516, 65, 42);
const FIntRect PowerIconRect = SpriteRect(396, 587, 17, 15);
const FIntRect PositionSearchRect = SpriteRect(796, 809, 22, 22);
const FIntRect FavoriteRect = SpriteRect(1, 475, 36, 36);
const FIntRect PanelBackgroundRect = SpriteRect(499, 532, 24, 38);
const FIntRect AddRect = SpriteRect(39, 475, 36, 36);
const FIntRect TaskEntryRect = SpriteRect(577, 564, 77, 76);
const FIntRect TaskArrowRect = SpriteRect(75, 181, 25, 34);
const FIntRect TaskRewardRect = SpriteRect(976, 899, 40, 40);
const FIntRect ChatBarRect = SpriteRect(1, 1010, 768, 13);
const FIntRect ChatRect = SpriteRect(976, 983, 43, 40);
const FIntRect BuildRect = SpriteRect(511, 812, 92, 92);
const FIntRect CityRect = SpriteRect(1, 667, 127, 148);
const FIntRect FeatureBackgroundRect = SpriteRect(317, 907, 147, 101);
const FIntRect FeatureLabelRect = SpriteRect(130, 553, 84, 27);
const FIntRect CampaignRect = SpriteRect(315, 717, 88, 88);
const FIntRect AllianceRect = SpriteRect(320, 627, 88, 88);
const FIntRect ItemsRect = SpriteRect(774, 833, 88, 88);
const FIntRect CommanderRect = SpriteRect(230, 627, 88, 88);
const FIntRect MoreRect = SpriteRect(774, 923, 100, 100);
const FIntRect MailRect = SpriteRect(864, 833, 88, 88);
const FIntRect ActivityRect = SpriteRect(656, 560, 80, 80);

const FIntRect FoodRect = SpriteRect(865, 349, 80, 80);
const FIntRect WoodRect = SpriteRect(1085, 489, 80, 80);
const FIntRect GemRect = SpriteRect(783, 435, 80, 80);
const FIntRect TaskRowRect = SpriteRect(436, 165, 63, 55);
const FIntRect RedPointRect = SpriteRect(2018, 994, 27, 29);
// BuildingMenuData IDs [2,1] -> ui_hud[btn_hud_1008], ui_hud[btn_hud_1004].
const FIntRect PlacementConfirmRect = SpriteRect(721, 575, 88, 88);
const FIntRect PlacementCancelRect = SpriteRect(1, 435, 88, 88);

const FIntRect PlayerFrameRect = SpriteRect(214, 984, 211, 211);
const FIntRect PlayerHead1000Rect = SpriteRect(1, 345, 211, 211);
const FIntRect VipIcon100Rect = SpriteRect(1, 325, 138, 138);

const FIntRect CityNameBackgroundRect = SpriteRect(431, 144, 69, 36);
const FIntRect CityLevelBackgroundRect = SpriteRect(269, 168, 60, 33);

constexpr float UnityReferenceWidth = 1440.0f;
constexpr float UnityReferenceHeight = 810.0f;

FText FormatCompactNumber(int64 Value)
{
	const double AbsoluteValue = FMath::Abs(static_cast<double>(Value));
	if (AbsoluteValue >= 1000000.0)
	{
		return FText::FromString(FString::Printf(TEXT("%.1fM"), static_cast<double>(Value) / 1000000.0));
	}
	if (AbsoluteValue >= 10000.0)
	{
		return FText::FromString(FString::Printf(TEXT("%.1fK"), static_cast<double>(Value) / 1000.0));
	}
	return FText::AsNumber(Value);
}
}

void URokMainHudWidget::ConfigureWorldMapTacticalOverlay(
	URokUnityModalWidget* InModalWidget,
	URokUnityFeatureScreenWidget* InFeatureScreenWidget)
{
	check(!ExpandedDesignCanvas);
	check(InModalWidget && InFeatureScreenWidget);
	bWorldMapTacticalOverlay = true;
	CurrentExpandedDesignSize = FVector2D(UnityReferenceWidth, UnityReferenceHeight);
	ModalWidget = InModalWidget;
	FeatureScreenWidget = InFeatureScreenWidget;
}

void URokMainHudWidget::SetSelectedBuilding(AActor* InBuildingActor)
{
	SelectedBuildingActor = InBuildingActor;
	if (BuildingPanelWidget)
	{
		BuildingPanelWidget->SetSelectedBuilding(InBuildingActor);
	}
	RefreshFromSubsystem();
}

void URokMainHudWidget::OpenCommandLineModal()
{
	if (!ModalWidget && !FeatureScreenWidget)
	{
		return;
	}

	FString ModalName;
	if (!FParse::Value(FCommandLine::Get(), TEXT("RokOpenModal="), ModalName) || ModalName.IsEmpty())
	{
		return;
	}

	ModalName.TrimStartAndEndInline();
	ModalName.ToLowerInline();
	if (ModalName == TEXT("scout"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->CloseModal();
		if (ScoutScreenWidget) ScoutScreenWidget->Open(INDEX_NONE);
		UE_LOG(LogTemp, Display, TEXT("Rok scout command-line preview: camp level and server queue unknown."));
	}
	else if (ModalName == TEXT("resource_building_info_reference"))
	{
		int32 Type = 2;
		FParse::Value(FCommandLine::Get(), TEXT("RokReferenceBuildingType="), Type);
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenResourceBuildingInfoReferencePreview(Type);
		UE_LOG(LogTemp, Display, TEXT("Rok resource building config preview: type=%d, no city actor or current level supplied."), Type);
	}
	else if (ModalName == TEXT("profile") || ModalName == TEXT("player_profile"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenPlayerProfile();
	}
	else if (ModalName == TEXT("player_head")
		|| ModalName == TEXT("avatar")
		|| ModalName == TEXT("head_selection"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenPlayerHeadSelection();
	}
	else if (ModalName == TEXT("player_power")
		|| ModalName == TEXT("power_details")
		|| ModalName == TEXT("player_details"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenPlayerPowerDetails();
	}
	else if (ModalName == TEXT("ranking")
		|| ModalName == TEXT("rank")
		|| ModalName == TEXT("player_ranking"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenPlayerRanking();
	}
	else if (ModalName == TEXT("civilization")
		|| ModalName == TEXT("civilization_selection")
		|| ModalName == TEXT("change_civilization"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenCivilizationSelection();
	}
	else if (ModalName == TEXT("civilization_confirmation"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget)
		{
			ModalWidget->OpenCivilizationSelection();
			ModalWidget->OpenCivilizationChangeConfirmation();
		}
	}
	else if (ModalName == TEXT("rename")
		|| ModalName == TEXT("change_name")
		|| ModalName == TEXT("player_rename"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenChangeName();
	}
	else if (ModalName == TEXT("resources") || ModalName == TEXT("resource") || ModalName == TEXT("food"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenResourceInventory(0);
	}
	else if (ModalName == TEXT("resources_reference"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenResourceInventory(0, true);
	}
	else if (ModalName == TEXT("wood"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenResourceInventory(1);
	}
	else if (ModalName == TEXT("stone"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenResourceInventory(2);
	}
	else if (ModalName == TEXT("gold"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenResourceInventory(3);
	}
	else if (ModalName == TEXT("building_info") || ModalName == TEXT("buildinginfo"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenBuildingInfo(SelectedBuildingActor.Get());
	}
	else if (ModalName == TEXT("building_upgrade") || ModalName == TEXT("buildingupgrade"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenBuildingUpgrade(SelectedBuildingActor.Get());
	}
	else if (ModalName == TEXT("activity") || ModalName == TEXT("event"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenActivityCenter();
	}
	else if (ModalName == TEXT("mail") || ModalName == TEXT("email"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenMail(false);
	}
	else if (ModalName == TEXT("mail_local_unread"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenMailLocalUnreadFixture();
	}
	else if (ModalName == TEXT("mail_reference") || ModalName == TEXT("mail_preview"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenMail(true);
	}
	else if (ModalName == TEXT("mail_rewards_preview"))
	{
		FRokMailRewardPayload Mixed, Empty;
		FString Error;
		if (!RokMailRewardDetails::LoadFixture(ERokMailRewardFixture::MixedAudit, Mixed, Error)
			|| !RokMailRewardDetails::LoadFixture(ERokMailRewardFixture::EmptyAlternate, Empty, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("Rok read-only mail reward preview unavailable: %s"), *Error);
			return;
		}
		TMap<int32, FRokMailRewardPayload> FixtureByMailIndex;
		FixtureByMailIndex.Add(0, MoveTemp(Mixed));
		FixtureByMailIndex.Add(1, MoveTemp(Empty));
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenMailReadOnlyRewardFixture(FixtureByMailIndex);
		UE_LOG(LogTemp, Display, TEXT("Rok mail reward presentation fixture: synthetic read-only payload, no account mail or claim request."));
	}
	else if (ModalName == TEXT("tasks") || ModalName == TEXT("quest"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenTasks(false);
	}
	else if (ModalName == TEXT("tasks_reference") || ModalName == TEXT("task_reference"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenTasks(true);
	}
	else if (ModalName == TEXT("commander") || ModalName == TEXT("hero") || ModalName == TEXT("captain"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenCommander();
	}
	else if (ModalName == TEXT("items") || ModalName == TEXT("bag"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenItems();
	}
	else if (ModalName == TEXT("alliance") || ModalName == TEXT("alliance_welcome") || ModalName == TEXT("alliance_unjoined"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenAlliance(false);
	}
	else if (ModalName == TEXT("alliance_joined") || ModalName == TEXT("alliance_main"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenAlliance(true);
	}
	else if (ModalName == TEXT("vip") || ModalName == TEXT("vip0"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenVip();
	}
	else if (ModalName == TEXT("vip_reference"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenVip(true);
	}
	else if (ModalName == TEXT("chat"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenChat();
	}
	else if (ModalName == TEXT("chat_reference"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenChat(true);
	}
	else if (
		ModalName == TEXT("position")
		|| ModalName == TEXT("coordinate")
		|| ModalName == TEXT("world_position"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenWorldPositionSearch();
	}
	else if (
		ModalName == TEXT("markers")
		|| ModalName == TEXT("bookmarks")
		|| ModalName == TEXT("favorite"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenMapMarkers();
	}
	else if (
		ModalName == TEXT("resource_search")
		|| ModalName == TEXT("search_resource")
		|| ModalName == TEXT("search_res"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenResourceSearch();
	}
	else if (
		ModalName == TEXT("build_city")
		|| ModalName == TEXT("buildcity")
		|| ModalName == TEXT("build"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenBuildCity();
	}
	else if (
		ModalName == TEXT("worker_queue")
		|| ModalName == TEXT("workerqueue")
		|| ModalName == TEXT("builders"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenWorkerQueue(SelectedBuildingActor.Get());
	}
	else if (
		ModalName == TEXT("hospital")
		|| ModalName == TEXT("healing")
		|| ModalName == TEXT("heal"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenHospital(SelectedBuildingActor.Get());
	}
	else if (
		ModalName == TEXT("city_manager")
		|| ModalName == TEXT("citymanager")
		|| ModalName == TEXT("city_buffs"))
	{
		HandleOpenCityManager();
	}
	else if (
		ModalName == TEXT("army_show")
		|| ModalName == TEXT("armyshow")
		|| ModalName == TEXT("marches")
		|| ModalName == TEXT("troop_queue"))
	{
		HandleOpenArmyShow();
	}
	else if (ModalName == TEXT("settings"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenSettingsHub();
	}
	else if (
		ModalName == TEXT("general_settings")
		|| ModalName == TEXT("generalsetting"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenGeneralSettings();
	}
	else if (
		ModalName == TEXT("notification_settings")
		|| ModalName == TEXT("notifications"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenNotificationSettings();
	}
	else if (
		ModalName == TEXT("language_settings")
		|| ModalName == TEXT("language"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenLanguageSettings();
	}
	else if (
		ModalName == TEXT("account_settings")
		|| ModalName == TEXT("account"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenAccountSettings();
	}
	else if (
		ModalName == TEXT("army")
		|| ModalName == TEXT("army_statistics")
		|| ModalName == TEXT("armyconst"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenArmyStatistics();
	}
	else if (
		ModalName == TEXT("store")
		|| ModalName == TEXT("normal_store")
		|| ModalName == TEXT("shop"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenStore();
	}
	else if (
		ModalName == TEXT("train_army")
		|| ModalName == TEXT("trainarmy")
		|| ModalName == TEXT("training"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenTrainArmy(
				SelectedBuildingActor.Get(),
				1);
		}
	}
	else if (ModalName == TEXT("research_reference"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenResearch(nullptr);
		UE_LOG(LogTemp, Display, TEXT("Rok research reference entry: academy, technology levels and queue are not synchronized."));
	}
	else if (
		ModalName == TEXT("speed_up")
		|| ModalName == TEXT("speedup")
		|| ModalName == TEXT("building_speedup"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenSpeedUp(
				SelectedBuildingActor.Get(),
				20102);
		}
	}
	else if (
		ModalName == TEXT("expedition")
		|| ModalName == TEXT("campaign"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenExpedition();
	}
	else if (
		ModalName == TEXT("warn")
		|| ModalName == TEXT("war_warning"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenWarWarning(false);
	}
	else if (
		ModalName == TEXT("warn_reference")
		|| ModalName == TEXT("war_warning_reference"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenWarWarning(true);
	}
	else if (
		ModalName == TEXT("buff")
		|| ModalName == TEXT("buff_list")
		|| ModalName == TEXT("buffs"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenBuffList(false);
	}
	else if (
		ModalName == TEXT("buff_reference")
		|| ModalName == TEXT("buff_list_reference"))
	{
		if (FeatureScreenWidget) FeatureScreenWidget->CloseScreen();
		if (ModalWidget) ModalWidget->OpenBuffList(true);
	}
	else if (
		ModalName == TEXT("gem_shop")
		|| ModalName == TEXT("gemshop"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenGemShop();
	}
	else if (
		ModalName == TEXT("charge")
		|| ModalName == TEXT("recharge")
		|| ModalName == TEXT("ui_charge"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenCharge(107);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok modal UI: unknown -RokOpenModal value '%s'."), *ModalName);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Rok modal UI: opened command-line mode '%s'."), *ModalName);
}

void URokMainHudWidget::RefreshFromSubsystem()
{
	if (!bWorldMapTacticalOverlay && GetWorld()
		&& (!CityMarkerAnchorActor.IsValid() || CityMarkerAnchorActor->GetWorld() != GetWorld()))
	{
		TArray<AActor*> CityHallActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("UnityInitId_5"), CityHallActors);
		CityMarkerAnchorActor = CityHallActors.IsEmpty() ? nullptr : CityHallActors[0];
	}
	URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetUpgradeSubsystem();
	FRokBuildingState LocalCityHallState;
	bool bHasLocalCityHallState = false;
	if (UpgradeSubsystem)
	{
		const FRokResourceStock Resources = UpgradeSubsystem->GetCurrentResources();
		if (FoodText)
		{
			FoodText->SetText(FormatCompactNumber(Resources.Food));
		}
		if (WoodText)
		{
			WoodText->SetText(FormatCompactNumber(Resources.Wood));
		}
		if (GemText)
		{
			// Gem/Denar player state is not migrated. Preserve the source preview
			// value rather than presenting Gold as a different currency.
			GemText->SetText(FText::AsNumber(350));
		}

		if (CityMarkerAnchorActor.IsValid())
		{
			LocalCityHallState = UpgradeSubsystem->GetBuildingStateForActor(CityMarkerAnchorActor.Get());
			bHasLocalCityHallState = true;
			if (CityLevelText)
			{
				CityLevelText->SetText(FText::AsNumber(FMath::Max(1, LocalCityHallState.Level)));
			}
		}

		if (StatusText)
		{
			if (SelectedBuildingActor.IsValid())
			{
				const FRokBuildingState SelectedState = UpgradeSubsystem->GetBuildingStateForActor(SelectedBuildingActor.Get());
				StatusText->SetText(FText::Format(
					NSLOCTEXT("RokHud", "SelectedBuilding", "{0}  Lv.{1}"),
					SelectedState.DisplayName,
					FText::AsNumber(SelectedState.Level)));
			}
			else
			{
				StatusText->SetText(FText::GetEmpty());
			}
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		const URokUnityRuntimeStateSubsystem* RuntimeState = GameInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
		if (RuntimeState && RuntimeState->IsRuntimeStateLoaded())
		{
			const FRokUnityRuntimePlayerProfileState ProfileState =
				RuntimeState->GetPlayerProfileState();
			if (PowerText)
			{
				SetPowerPresentation(ProfileState.LocalPreviewScore);
				PowerText->SetToolTipText(NSLOCTEXT(
					"RokHud",
					"ProfilePowerTooltip",
					"基础预览评分＋当前城市升级增量，不是服务器正式战力"));
			}
			if (CityNameText)
			{
				CityNameText->SetText(ProfileState.bHasDisplayName
					? FText::FromString(ProfileState.DisplayName)
					: NSLOCTEXT("RokHud", "LocalCityName", "本地预览"));
			}

			const FRokUnityRuntimeTaskLocalProjection TaskProjection = RuntimeState->GetActiveTaskLocalProjection(
				bHasLocalCityHallState ? &LocalCityHallState : nullptr);
			const FRokUnityRuntimeTaskState& ActiveTask = TaskProjection.Task;
			bool bRewardComplete = ActiveTask.bComplete;
#if !UE_BUILD_SHIPPING
			if (FParse::Param(FCommandLine::Get(), TEXT("RokTaskRewardPreview")))
			{
				// Explicit visual fixture only; never modifies ActiveTask or rewards.
				bRewardComplete = true;
			}
#endif
			SetTaskRewardPresentation(ActiveTask.TaskId, bRewardComplete);
			for (int32 Index = 0; Index < TaskTitleTexts.Num() && Index < TaskProgressTexts.Num(); ++Index)
			{
				const FText DesiredTooltip = Index == 0 && ActiveTask.TaskId > 0 && TaskProjection.bProjected
					? (TaskProjection.bConditionMet
						? NSLOCTEXT("RokHud", "TaskLocalConditionTooltip", "本地条件已满足，服务器任务状态待同步")
						: NSLOCTEXT("RokHud", "TaskLocalProgressTooltip", "当前显示本地城市条件进度；服务器任务状态待同步"))
					: FText::GetEmpty();
				if (!TaskProgressTexts[Index]->GetToolTipText().EqualTo(DesiredTooltip))
				{
					TaskProgressTexts[Index]->SetToolTipText(DesiredTooltip);
				}
				if (Index == 0 && ActiveTask.TaskId > 0)
				{
					const FString TaskName = ActiveTask.Name.IsEmpty()
						? FString::Printf(TEXT("任务 %d"), ActiveTask.TaskId)
						: ActiveTask.Name;
					TaskTitleTexts[Index]->SetText(TaskProjection.bProjected
						? FText::Format(NSLOCTEXT("RokHud", "TaskLocalTitle", "{0}·本地"), FText::FromString(TaskName))
						: FText::FromString(TaskName));
					const bool bLocalProgressKnown =
						ActiveTask.ProgressRule.Equals(TEXT("building_level"), ESearchCase::IgnoreCase)
						|| ActiveTask.ProgressRule.Equals(TEXT("building_count"), ESearchCase::IgnoreCase);
					if (bLocalProgressKnown)
					{
						FString TaskDescription = ActiveTask.Desc.IsEmpty()
							? ActiveTask.ProgressRule
							: ActiveTask.Desc;
						TaskDescription.ReplaceInline(TEXT("{p1}"), *FString::FromInt(ActiveTask.Param1));
						TaskDescription.ReplaceInline(TEXT("{p2}"), *FString::FromInt(ActiveTask.Param2));
						TaskDescription.ReplaceInline(TEXT("{p3}"), *FString::FromInt(ActiveTask.Target));
						const FText ProgressText = FText::Format(
							NSLOCTEXT("RokHud", "TaskSnapshotProgress", "{0} ({1}/{2})"),
							FText::FromString(TaskDescription),
							FText::AsNumber(ActiveTask.Current),
							FText::AsNumber(ActiveTask.Target));
						TaskProgressTexts[Index]->SetText(ProgressText);
					}
					else
					{
						TaskProgressTexts[Index]->SetText(FText::Format(
							NSLOCTEXT("RokHud", "TaskRuntimeBoundary", "任务 #{0} · 进度待服务器"),
							FText::AsNumber(ActiveTask.TaskId)));
					}
				}
				else if (Index > 0)
				{
					// Side rows need server task state (Unity TaskProxy
					// GetTaskSideList); without it stay on the boundary text
					// instead of presenting config rows as account tasks.
					TaskTitleTexts[Index]->SetText(NSLOCTEXT("RokHud", "SideTaskPending", "支线任务"));
					TaskProgressTexts[Index]->SetText(NSLOCTEXT("RokHud", "SideTaskServerBoundary", "进度等待服务器状态"));
				}
				else
				{
					TaskTitleTexts[Index]->SetText(NSLOCTEXT("RokHud", "MainTaskPending", "主线任务"));
					TaskProgressTexts[Index]->SetText(NSLOCTEXT("RokHud", "TaskStateServerBoundary", "进度等待服务器状态"));
				}
			}
		}
	}

	if (VipText)
	{
		VipText->SetText(NSLOCTEXT("RokHud", "VipUnavailable", "VIP --"));
	}
	if (CoordinateText)
	{
		// Position/server player state is not migrated yet; preserve the Unity
		// preview values instead of inventing a client-side authoritative source.
		CoordinateText->SetText(NSLOCTEXT("RokHud", "PositionPreview", "#1    X:1146 Y:157"));
	}
	if (AgeText)
	{
		AgeText->SetText(NSLOCTEXT("RokHud", "AgePreview", "远古时代"));
	}
	if (RuntimeInfoText)
	{
		RuntimeInfoText->SetText(FText::FromString(
			FDateTime::Now().ToString(TEXT("AST %m/%d %H:%M  lod:-- lv:0 ping:--"))));
	}
}

TSharedRef<SWidget> URokMainHudWidget::RebuildWidget()
{
	BuildWidgetTree();
	UE_LOG(LogTemp, Log, TEXT("Rok UI: Unity MainInterface HUD widget tree rebuilt."));
	return Super::RebuildWidget();
}

void URokMainHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (PlacementCancelButton)
	{
		PlacementCancelButton->OnClicked.AddUniqueDynamic(this, &URokMainHudWidget::HandlePlacementCancelClicked);
	}
	if (PlacementConfirmButton)
	{
		PlacementConfirmButton->OnClicked.AddUniqueDynamic(this, &URokMainHudWidget::HandlePlacementConfirmClicked);
	}
	SetPlacementPresentation(bPlacementControlsVisible, bPlacementGridValid, bPlacementCanCommit, PlacementReason);
	if (!bWorldMapTacticalOverlay)
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (BuildingPanelWidget)
	{
		BuildingPanelWidget->SetSelectedBuilding(nullptr);
	}
	RefreshFromSubsystem();
}

void URokMainHudWidget::NativeDestruct()
{
	if (PlacementCancelButton)
	{
		PlacementCancelButton->OnClicked.RemoveDynamic(this, &URokMainHudWidget::HandlePlacementCancelClicked);
	}
	if (PlacementConfirmButton)
	{
		PlacementConfirmButton->OnClicked.RemoveDynamic(this, &URokMainHudWidget::HandlePlacementConfirmClicked);
	}
	Super::NativeDestruct();
}

bool URokMainHudWidget::IsPlacementBlockingModalOpen() const
{
	return ModalWidget && ModalWidget->IsModalOpen();
}

void URokMainHudWidget::SetPlacementPresentation(bool bVisible, bool bGridValid, bool bCanCommit, const FText& Reason)
{
	const bool bWasVisible = bPlacementControlsVisible;
	bPlacementControlsVisible = bVisible;
	bPlacementGridValid = bGridValid;
	bPlacementCanCommit = bCanCommit;
	PlacementReason = Reason;
	if (!PlacementControls)
	{
		return;
	}
	if (bVisible && !bWasVisible)
	{
		// A fresh source popup starts from its serialized Image color even if
		// the previous one was still in its Close animation.
		PlacementConfirmSourceTint = FLinearColor::White;
		PlacementConfirmTintStart = FLinearColor::White;
		PlacementConfirmTintTarget = FLinearColor::White;
		PlacementConfirmTintElapsed = PlacementButtonTintSeconds;
		PlacementCancelSourceTint = FLinearColor::White;
		PlacementCancelTintStart = FLinearColor::White;
		PlacementCancelTintTarget = FLinearColor::White;
		PlacementCancelTintElapsed = PlacementButtonTintSeconds;
		if (PlacementConfirmIcon) PlacementConfirmIcon->SetColorAndOpacity(FLinearColor::White);
		if (PlacementCancelIcon) PlacementCancelIcon->SetColorAndOpacity(FLinearColor::White);
		if (PlacementMenuAnimation.IsClosing())
		{
			// A new HUD registration creates a fresh source popup rather than
			// resuming the old instance's Close clip.
			PlacementMenuAnimation.Reset();
		}
		PlacementMenuAnimation.Open(ERokUiWindowAnimationStyle::BuildingMenu);
	}
	else if (!bVisible && bWasVisible)
	{
		PlacementMenuAnimation.Close();
	}
	if (bVisible || PlacementMenuAnimation.IsClosing())
	{
		PlacementControls->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		PlacementControls->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (PlacementConfirmButton)
	{
		PlacementConfirmButton->SetIsEnabled(bVisible ? bCanCommit
			: (PlacementMenuAnimation.IsClosing() && PlacementConfirmButton->GetIsEnabled()));
	}
	if (PlacementCancelButton)
	{
		// The source CanvasGroup retains raycasts during Off. The delegate
		// handlers above still reject a second action once bVisible is false.
		PlacementCancelButton->SetIsEnabled(bVisible || PlacementMenuAnimation.IsClosing());
	}
	if (bVisible && PlacementConfirmIcon && PlacementMenuAtlas)
	{
		UObject* DesiredResource = !bCanCommit && PlacementConfirmGrayMaterial
			? static_cast<UObject*>(PlacementConfirmGrayMaterial.Get())
			: static_cast<UObject*>(PlacementMenuAtlas.Get());
		if (PlacementConfirmIcon->GetBrush().GetResourceObject() != DesiredResource)
		{
			FSlateBrush Brush = MakeAtlasBrush(PlacementMenuAtlas, PlacementConfirmRect,
				PlacementAtlasSize, FVector2D(87.0f, 87.0f));
			Brush.SetResourceObject(DesiredResource);
			PlacementConfirmIcon->SetBrush(Brush);
		}
	}
	AdvancePlacementButtonTints(0.0f);
	AdvancePlacementMenuAnimation(0.0f);
	if (!bVisible)
	{
		return;
	}
	if (PlacementStatusText)
	{
		PlacementStatusText->SetText(!bGridValid
			? NSLOCTEXT("RokHud", "PlacementGridInvalid", "不可摆放")
			: (bCanCommit
				? NSLOCTEXT("RokHud", "PlacementCanBuild", "可开始建造")
				: NSLOCTEXT("RokHud", "PlacementGridValid", "可摆放（预览）")));
		PlacementStatusText->SetColorAndOpacity(FSlateColor(bGridValid
			? FLinearColor(0.48f, 0.92f, 0.56f, 1.0f)
			: FLinearColor(1.0f, 0.46f, 0.39f, 1.0f)));
	}
	if (PlacementReasonText)
	{
		PlacementReasonText->SetText(!Reason.IsEmpty() ? Reason
			: (bCanCommit
				? NSLOCTEXT("RokHud", "PlacementReadyReason", "确认后开始本地建造")
				: NSLOCTEXT("RokHud", "PlacementInvalidReason", "请选择可用位置")));
	}
}

void URokMainHudWidget::HandlePlacementCancelClicked()
{
	if (bPlacementControlsVisible)
	{
		OnPlacementCancelRequested.Broadcast();
	}
}

void URokMainHudWidget::HandlePlacementConfirmClicked()
{
	if (bPlacementControlsVisible && bPlacementCanCommit)
	{
		OnPlacementConfirmRequested.Broadcast();
	}
}

void URokMainHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AdvanceHudAnimations(InDeltaTime);
	CurrentRootSize = MyGeometry.GetLocalSize();
	UpdateExpandedDesignResolution(CurrentRootSize);
	UpdateCityMarkerProjection();
	UpdatePlacementMenuProjection();
	AdvancePlacementMenuAnimation(InDeltaTime);
	AdvancePlacementButtonTints(InDeltaTime);
	if (HotTaskGroup && !bWorldMapTacticalOverlay)
	{
		const ESlateVisibility DesiredTaskVisibility = FeatureScreenWidget && FeatureScreenWidget->IsScreenOpen()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible;
		if (HotTaskGroup->GetVisibility() != DesiredTaskVisibility)
		{
			if (DesiredTaskVisibility == ESlateVisibility::Collapsed)
			{
				URokUiAnimatedButton::ResetAnimationsInHierarchy(HotTaskGroup);
			}
			HotTaskGroup->SetVisibility(DesiredTaskVisibility);
		}
	}

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.25f)
	{
		RefreshAccumulator = 0.0f;
		RefreshFromSubsystem();
	}
}

void URokMainHudWidget::BuildWidgetTree()
{
	if (!WidgetTree || ExpandedDesignCanvas)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RokMainHudRoot"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	UScaleBox* ExpandScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("UnityExpandScaleBox"));
	ExpandScaleBox->SetStretch(EStretch::ScaleToFit);
	ExpandScaleBox->SetStretchDirection(EStretchDirection::Both);
	ExpandScaleBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UCanvasPanelSlot* ScaleSlot = RootCanvas->AddChildToCanvas(ExpandScaleBox);
	ScaleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	ScaleSlot->SetOffsets(FMargin(0.0f));

	ExpandedDesignSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("UnityExpandedDesignSize"));
	ExpandedDesignSizeBox->SetWidthOverride(CurrentExpandedDesignSize.X);
	ExpandedDesignSizeBox->SetHeightOverride(CurrentExpandedDesignSize.Y);
	ExpandedDesignSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ExpandScaleBox->SetContent(ExpandedDesignSizeBox);

	ExpandedDesignCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("UnityExpandedDesignCanvas"));
	ExpandedDesignCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ExpandedDesignSizeBox->SetContent(ExpandedDesignCanvas);

	if (!bWorldMapTacticalOverlay)
	{
		BuildPlayerPowerGroup();
		BuildPositionGroup();
	}
	// Group the source right-side modules under a motion-only parent. Their
	// own fold/hover animations remain on children and cannot overwrite zoom motion.
	UCanvasPanel* DesignCanvas = ExpandedDesignCanvas;
	if (bWorldMapTacticalOverlay)
	{
		WorldMapRightModules = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("WorldMapRightModules"));
		auto* MotionSlot = DesignCanvas->AddChildToCanvas(WorldMapRightModules);
		MotionSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		MotionSlot->SetOffsets(FMargin(0));
		ExpandedDesignCanvas = WorldMapRightModules;
	}
	BuildResourceGroup();
	BuildAgeGroup();
	ExpandedDesignCanvas = DesignCanvas;
	BuildTaskGroup();
	if (!bWorldMapTacticalOverlay)
	{
		BuildChatAndMapGroup();
	}
	if (bWorldMapTacticalOverlay) ExpandedDesignCanvas = WorldMapRightModules;
	BuildFeatureGroup();
	ExpandedDesignCanvas = DesignCanvas;
	if (bWorldMapTacticalOverlay)
	{
		AdvanceWorldMapModuleAnimation(0.0f);
		// The world HUD already owns city markers, selection input and modal
		// layers. Reuse only the MainInterface tactical controls here.
		return;
	}
	BuildCityMarker();

	StatusText = MakeHudText(TEXT("SelectionStatusText"), 14, FLinearColor(0.95f, 0.9f, 0.75f, 1.0f), true);
	StatusText->SetJustification(ETextJustify::Center);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		StatusText,
		FVector2D(0.0f, -22.0f),
		FVector2D(360.0f, 26.0f),
		FAnchors(0.5f, 1.0f),
		FVector2D(0.5f, 1.0f));

	BuildingPanelWidget = WidgetTree->ConstructWidget<URokBuildingPanelWidget>(URokBuildingPanelWidget::StaticClass(), TEXT("BuildingPanelWidget"));
	BuildingPanelWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BuildingPanelWidget->OnInfoRequested.AddUObject(this, &URokMainHudWidget::HandleBuildingInfoRequested);
	BuildingPanelWidget->OnUpgradeRequested.AddUObject(this, &URokMainHudWidget::HandleBuildingUpgradeRequested);
	BuildingPanelWidget->OnFeatureRequested.AddUObject(this, &URokMainHudWidget::HandleBuildingFeatureRequested);
	UCanvasPanelSlot* BuildingPanelSlot = RootCanvas->AddChildToCanvas(BuildingPanelWidget);
	BuildingPanelSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BuildingPanelSlot->SetOffsets(FMargin(0.0f));
	BuildingPanelSlot->SetZOrder(20);

	ModalWidget = WidgetTree->ConstructWidget<URokUnityModalWidget>(URokUnityModalWidget::StaticClass(), TEXT("UnityModalWidget"));
	ModalWidget->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* ModalSlot = RootCanvas->AddChildToCanvas(ModalWidget);
	ModalSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	ModalSlot->SetOffsets(FMargin(0.0f));
	ModalSlot->SetZOrder(100);

	FeatureScreenWidget = WidgetTree->ConstructWidget<URokUnityFeatureScreenWidget>(URokUnityFeatureScreenWidget::StaticClass(), TEXT("UnityFeatureScreenWidget"));
	FeatureScreenWidget->SetVisibility(ESlateVisibility::Collapsed);
	FeatureScreenWidget->OnTaskGoTownHallRequested.BindUObject(
		this, &URokMainHudWidget::HandleTaskGoTownHallRequested);
	UCanvasPanelSlot* FeatureScreenSlot = RootCanvas->AddChildToCanvas(FeatureScreenWidget);
	FeatureScreenSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	FeatureScreenSlot->SetOffsets(FMargin(0.0f));
	FeatureScreenSlot->SetZOrder(110);
	ModalWidget->SetFeatureScreenWidget(FeatureScreenWidget);

	ScoutScreenWidget = WidgetTree->ConstructWidget<URokScoutScreenWidget>(URokScoutScreenWidget::StaticClass(), TEXT("ScoutScreenWidget"));
	ScoutScreenWidget->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* ScoutSlot = RootCanvas->AddChildToCanvas(ScoutScreenWidget);
	ScoutSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	ScoutSlot->SetOffsets(FMargin(0));
	ScoutSlot->SetZOrder(120);

	// UI_Pop_BuildingMenu follows the temporary building's `button` transform.
	// Only its two UI_Model_CommandBtn images receive input; the canvas passes
	// world drags through. The source title is hidden for menu IDs [2,1].
	PlacementControls = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("CityPlacementControls"));
	PlacementControls->SetVisibility(ESlateVisibility::Collapsed);
	PlacementControls->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	PlacementControlsSlot = RootCanvas->AddChildToCanvas(PlacementControls);
	PlacementControlsSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	PlacementControlsSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	PlacementControlsSlot->SetPosition(FVector2D(0.0f, -55.0f));
	PlacementControlsSlot->SetSize(FVector2D(472.7f, 220.0f));
	PlacementControlsSlot->SetZOrder(130);

	UBorder* PlacementBackground = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("CityPlacementBackground"));
	PlacementBackground->SetPadding(FMargin(0.0f));
	PlacementBackground->SetBrushColor(FLinearColor::Transparent);
	PlacementBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(PlacementControls, PlacementBackground, FVector2D::ZeroVector, FVector2D(472.7f, 220.0f));

	// Retain the existing read-only feedback nodes for the no-preview failure
	// route. They are hidden for the source two-icon temporary building menu.
	PlacementStatusText = MakeHudText(TEXT("CityPlacementStatus"), 17, FLinearColor::White, true);
	PlacementStatusText->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(PlacementControls, PlacementStatusText, FVector2D(36.0f, 12.0f), FVector2D(400.0f, 26.0f));
	PlacementReasonText = MakeHudText(TEXT("CityPlacementReason"), 13, FLinearColor(0.92f, 0.88f, 0.75f, 1.0f));
	PlacementReasonText->SetAutoWrapText(true);
	PlacementReasonText->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(PlacementControls, PlacementReasonText, FVector2D(36.0f, 42.0f), FVector2D(400.0f, 56.0f));

	PlacementMenuAtlas = LoadUiTexture(PlacementAtlasPath);
	if (PlacementMenuAtlas)
	{
		if (UMaterialInterface* GrayMaterial = LoadObject<UMaterialInterface>(nullptr, PlacementGrayMaterialPath))
		{
			PlacementConfirmGrayMaterial = UMaterialInstanceDynamic::Create(GrayMaterial, this);
			PlacementConfirmGrayMaterial->SetTextureParameterValue(TEXT("SourceAtlas"), PlacementMenuAtlas);
			PlacementConfirmGrayMaterial->SetVectorParameterValue(TEXT("SourceTint"), FLinearColor::White);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Rok placement: dedicated Placement gray UI material is missing"));
		}
	}
	// Keep the graphic beside the disabled hit button. Slate paints descendants
	// of a disabled SButton with DisabledEffect; Unity GrayChildrens instead
	// applies ImageGrayShader to the image while GameButton stops interaction.
	PlacementConfirmIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CityPlacementConfirmImage"));
	PlacementConfirmIcon->SetVisibility(PlacementMenuAtlas ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (PlacementMenuAtlas)
	{
		PlacementConfirmIcon->SetBrush(MakeAtlasBrush(PlacementMenuAtlas, PlacementConfirmRect,
			PlacementAtlasSize, FVector2D(87.0f, 87.0f)));
	}
	PlaceOnCanvas(PlacementControls, PlacementConfirmIcon, FVector2D(279.2f, 167.5f), FVector2D(87.0f, 87.0f));
	UTextBlock* ConfirmLabel = MakeHudText(TEXT("CityPlacementConfirmLabel"), 17, FLinearColor::White, true);
	ConfirmLabel->SetText(NSLOCTEXT("RokHud", "PlacementConfirm", "建造"));
	ConfirmLabel->SetJustification(ETextJustify::Center);
	ConfirmLabel->SetVisibility(PlacementMenuAtlas ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(PlacementControls, ConfirmLabel, FVector2D(279.2f, 167.5f), FVector2D(87.0f, 87.0f));
	PlacementConfirmButton = MakeTransparentHitButton(TEXT("CityPlacementConfirmButton"));
	PlacementConfirmButton->SetToolTipText(NSLOCTEXT("RokHud", "PlacementConfirmTip", "确认本地建造"));
	PlaceOnCanvas(PlacementControls, PlacementConfirmButton, FVector2D(279.2f, 167.5f), FVector2D(87.0f, 87.0f));

	PlacementCancelIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CityPlacementCancelImage"));
	PlacementCancelIcon->SetVisibility(PlacementMenuAtlas ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (PlacementMenuAtlas)
	{
		PlacementCancelIcon->SetBrush(MakeAtlasBrush(PlacementMenuAtlas, PlacementCancelRect,
			PlacementAtlasSize, FVector2D(87.0f, 87.0f)));
	}
	PlaceOnCanvas(PlacementControls, PlacementCancelIcon, FVector2D(179.2f, 167.5f), FVector2D(87.0f, 87.0f));
	UTextBlock* CancelLabel = MakeHudText(TEXT("CityPlacementCancelLabel"), 17, FLinearColor::White, true);
	CancelLabel->SetText(NSLOCTEXT("RokHud", "PlacementCancel", "取消摆放"));
	CancelLabel->SetJustification(ETextJustify::Center);
	CancelLabel->SetVisibility(PlacementMenuAtlas ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(PlacementControls, CancelLabel, FVector2D(179.2f, 167.5f), FVector2D(87.0f, 87.0f));
	PlacementCancelButton = MakeTransparentHitButton(TEXT("CityPlacementCancelButton"));
	PlacementCancelButton->SetToolTipText(NSLOCTEXT("RokHud", "PlacementCancelTip", "取消本次临时摆放"));
	PlaceOnCanvas(PlacementControls, PlacementCancelButton, FVector2D(179.2f, 167.5f), FVector2D(87.0f, 87.0f));
}

void URokMainHudWidget::BuildPlayerPowerGroup()
{
	UCanvasPanel* Group = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerPowerGroup"));
	Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ExpandedDesignCanvas, Group, FVector2D(0.0f, 0.0f), FVector2D(302.0f, 125.0f));

	UImage* PlayerBackground = MakeAtlasImage(TEXT("PlayerPowerBackground"), MainMenuAtlasPath, PlayerBackgroundRect, MainMenuAtlasSize, FVector2D(302.0f, 125.0f));
	PlayerBackground->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	PlayerBackground->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(Group, PlayerBackground, FVector2D::ZeroVector, FVector2D(302.0f, 125.0f));
	PlaceOnCanvas(Group, MakeAtlasImage(TEXT("PlayerHead1000"), PlayerHeadAtlasPath, PlayerHead1000Rect, PlayerHeadAtlasSize, FVector2D(92.0f, 92.0f)), FVector2D(10.0f, 10.0f), FVector2D(92.0f, 92.0f));
	PlaceOnCanvas(Group, MakeAtlasImage(TEXT("PlayerHeadFrame"), PlayerHeadAtlasPath, PlayerFrameRect, PlayerHeadAtlasSize, FVector2D(104.0f, 104.0f)), FVector2D(4.0f, 4.0f), FVector2D(104.0f, 104.0f));
	PlaceOnCanvas(Group, MakeAtlasImage(TEXT("PowerIcon"), MainMenuAtlasPath, PowerIconRect, MainMenuAtlasSize, FVector2D(28.0f, 25.0f)), FVector2D(111.0f, 10.0f), FVector2D(28.0f, 25.0f));

	PowerText = MakeHudText(TEXT("PlayerPowerText"), 25, FLinearColor(0.9f, 0.96f, 1.0f, 1.0f), true);
	PowerText->SetText(NSLOCTEXT("RokHud", "InitialPowerUnavailable", "--"));
	PlaceOnCanvas(Group, PowerText, FVector2D(145.0f, 5.0f), FVector2D(135.0f, 36.0f));
	PowerGain = WidgetTree->ConstructWidget<URokPowerGainWidget>(URokPowerGainWidget::StaticClass(), TEXT("PlayerPowerGain"));
	// UI_Item_PlayerPowerInfo/Pl_Power/btn_powerShow, mirrored for the LTR HUD.
	// Pivot X = 98.19989 + 193.42/2 + 4.75; Y = 28.75 + 2.72.
	PlaceOnCanvas(Group, PowerGain, FVector2D(199.65989f - 320, 31.47f - 180), FVector2D(640, 360))->SetZOrder(51);

	PlaceOnCanvas(Group, MakeAtlasImage(TEXT("VipIcon"), VipAtlasPath, VipIcon100Rect, VipAtlasSize, FVector2D(34.0f, 34.0f)), FVector2D(112.0f, 49.0f), FVector2D(34.0f, 34.0f));
	VipText = MakeHudText(TEXT("VipText"), 20, FLinearColor(1.0f, 0.88f, 0.4f, 1.0f), true);
	PlaceOnCanvas(Group, VipText, FVector2D(151.0f, 51.0f), FVector2D(85.0f, 30.0f));
	UButton* VipHitButton = MakeTransparentHitButton(TEXT("VipHitButton"));
	VipHitButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenVip);
	if (UCanvasPanelSlot* VipHitSlot = PlaceOnCanvas(Group, VipHitButton, FVector2D(108.0f, 44.0f), FVector2D(129.0f, 44.0f)))
	{
		VipHitSlot->SetZOrder(10);
	}
	UButton* PlayerArrowButton = MakeAtlasButton(TEXT("PlayerArrowButton"), MainMenuAtlasPath, PlayerArrowRect, MainMenuAtlasSize, FVector2D(65.0f, 42.0f));
	PlayerArrowButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenCityManager);
	PlaceOnCanvas(Group, PlayerArrowButton, FVector2D(237.0f, 50.0f), FVector2D(65.0f, 42.0f));
	UButton* PlayerProfileHitButton = MakeTransparentHitButton(TEXT("PlayerProfileHitButton"));
	PlayerProfileHitButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenPlayerProfile);
	if (UCanvasPanelSlot* ProfileHitSlot = PlaceOnCanvas(Group, PlayerProfileHitButton, FVector2D(0.0f, 0.0f), FVector2D(108.0f, 108.0f)))
	{
		ProfileHitSlot->SetZOrder(10);
	}
}

void URokMainHudWidget::BuildPositionGroup()
{
	UCanvasPanel* Group = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PositionGroup"));
	Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ExpandedDesignCanvas, Group, FVector2D(298.0f, 6.0f), FVector2D(280.0f, 50.0f));

	// UI_IF_MainInterface/UI_Item_Position/img has one sliced pl_mmu_1001
	// image, not an additional opaque shade underneath its transparent corners.
	PlaceOnCanvas(Group, MakeHudSlicedAtlasPanel(TEXT("PositionBackground"), MainMenuAtlasPath,
		PanelBackgroundRect, MainMenuAtlasSize, FMargin(9, 13, 8, 12)),
		FVector2D(0.0f, 5.0f), FVector2D(235.0f, 40.0f));
	CoordinateText = MakeHudText(TEXT("CoordinateText"), 18, FLinearColor(0.95f, 0.95f, 0.92f, 1.0f), false);
	CoordinateText->SetJustification(ETextJustify::Center);
	PlaceOnCanvas(Group, CoordinateText, FVector2D(10.0f, 8.0f), FVector2D(190.0f, 32.0f));
	UButton* PositionSearchButton = MakeAtlasButton(
		TEXT("PositionSearchButton"),
		MainMenuAtlasPath,
		PositionSearchRect,
		MainMenuAtlasSize,
		FVector2D(22.0f, 22.0f));
	PositionSearchButton->OnClicked.AddDynamic(
		this,
		&URokMainHudWidget::HandleOpenWorldPositionSearch);
	PlaceOnCanvas(
		Group,
		PositionSearchButton,
		FVector2D(198.0f, 13.0f),
		FVector2D(22.0f, 22.0f));

	UButton* FavoriteButton = MakeAtlasButton(
		TEXT("FavoriteButton"),
		MainMenuAtlasPath,
		FavoriteRect,
		MainMenuAtlasSize,
		FVector2D(44.0f, 44.0f));
	FavoriteButton->OnClicked.AddDynamic(
		this,
		&URokMainHudWidget::HandleOpenMapMarkers);
	PlaceOnCanvas(
		Group,
		FavoriteButton,
		FVector2D(232.0f, 2.0f),
		FVector2D(44.0f, 44.0f));
}

void URokMainHudWidget::SetWorldMapTacticalVisible(bool bVisible)
{
	if (!bWorldMapTacticalOverlay || bWorldMapModulesVisible == bVisible) return;
	bWorldMapModulesVisible = bVisible;
	WorldMapModuleSeconds = 0.0f; // Unity OnShowModule plays the new clip from normalized time zero.
	if (!bVisible) URokUiAnimatedButton::ResetAnimationsInHierarchy(this);
	AdvanceWorldMapModuleAnimation(0.0f);
}

void URokMainHudWidget::AdvanceWorldMapModuleAnimation(float DeltaSeconds)
{
	if (!bWorldMapTacticalOverlay) return;
	if (FMath::IsFinite(DeltaSeconds))
		WorldMapModuleSeconds = FMath::Clamp(WorldMapModuleSeconds + FMath::Max(DeltaSeconds, 0.0f), 0.0f, 0.25f);
	const bool bExiting = !bWorldMapModulesVisible && WorldMapModuleSeconds < 0.25f;
	const ESlateVisibility State = bWorldMapModulesVisible ? ESlateVisibility::SelfHitTestInvisible
		: (bExiting ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetVisibility(State);
	if (WorldMapRightModules)
	{
		const FVector2D Pose = RokMainInterfaceModuleAnimation::EvaluateRight(bWorldMapModulesVisible, WorldMapModuleSeconds);
		WorldMapRightModules->SetRenderTranslation(FVector2D(Pose.X, 0));
		WorldMapRightModules->SetRenderOpacity(Pose.Y);
		WorldMapRightModules->SetVisibility(State);
	}
	if (HotTaskGroup)
	{
		const FVector2D Pose = RokMainInterfaceModuleAnimation::EvaluateLeft(bWorldMapModulesVisible, WorldMapModuleSeconds);
		HotTaskGroup->SetRenderTranslation(FVector2D(Pose.X, 0));
		HotTaskGroup->SetRenderOpacity(Pose.Y);
		HotTaskGroup->SetVisibility(FeatureScreenWidget && FeatureScreenWidget->IsScreenOpen()
			? ESlateVisibility::Collapsed : State);
	}
}

void URokMainHudWidget::BuildResourceGroup()
{
	UCanvasPanel* Group = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerResourcesGroup"));
	Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		Group,
		FVector2D(-20.0f, 10.0f),
		FVector2D(440.0f, 44.0f),
		FAnchors(1.0f, 0.0f),
		FVector2D(1.0f, 0.0f));

	UTextBlock* FoodTextRaw = nullptr;
	UTextBlock* WoodTextRaw = nullptr;
	UTextBlock* GemTextRaw = nullptr;
	PlaceOnCanvas(Group, MakeResourceCell(TEXT("FoodResource"), FoodRect, FoodTextRaw, false), FVector2D(0.0f, 0.0f), FVector2D(136.0f, 44.0f));
	PlaceOnCanvas(Group, MakeResourceCell(TEXT("WoodResource"), WoodRect, WoodTextRaw, false), FVector2D(136.0f, 0.0f), FVector2D(136.0f, 44.0f));
	PlaceOnCanvas(Group, MakeResourceCell(TEXT("GemResource"), GemRect, GemTextRaw, true), FVector2D(272.0f, 0.0f), FVector2D(164.0f, 44.0f));
	UButton* FoodHitButton = MakeTransparentHitButton(TEXT("FoodResourceHitButton"));
	FoodHitButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenFoodResources);
	PlaceOnCanvas(Group, FoodHitButton, FVector2D(0.0f, 0.0f), FVector2D(136.0f, 44.0f))->SetZOrder(10);
	UButton* WoodHitButton = MakeTransparentHitButton(TEXT("WoodResourceHitButton"));
	WoodHitButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenWoodResources);
	PlaceOnCanvas(Group, WoodHitButton, FVector2D(136.0f, 0.0f), FVector2D(136.0f, 44.0f))->SetZOrder(10);
	UButton* GemHitButton = MakeTransparentHitButton(TEXT("GemResourceHitButton"));
	GemHitButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenCharge);
	PlaceOnCanvas(Group, GemHitButton, FVector2D(272.0f, 0.0f), FVector2D(164.0f, 44.0f))->SetZOrder(10);
	FoodText = FoodTextRaw;
	WoodText = WoodTextRaw;
	GemText = GemTextRaw;
}

void URokMainHudWidget::BuildAgeGroup()
{
	UCanvasPanel* Group = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AgeAndRuntimeGroup"));
	Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		Group,
		FVector2D(-20.0f, 62.0f),
		FVector2D(380.0f, 62.0f),
		FAnchors(1.0f, 0.0f),
		FVector2D(1.0f, 0.0f));

	RuntimeInfoText = MakeHudText(TEXT("RuntimeInfoText"), 14, FLinearColor(0.1f, 0.09f, 0.07f, 1.0f));
	RuntimeInfoText->SetJustification(ETextJustify::Right);
	PlaceOnCanvas(Group, RuntimeInfoText, FVector2D(0.0f, 0.0f), FVector2D(380.0f, 28.0f));
	AgeText = MakeHudText(TEXT("AgeText"), 18, FLinearColor(0.08f, 0.07f, 0.05f, 1.0f), true);
	AgeText->SetJustification(ETextJustify::Right);
	PlaceOnCanvas(Group, AgeText, FVector2D(190.0f, 28.0f), FVector2D(190.0f, 28.0f));
}

void URokMainHudWidget::BuildTaskGroup()
{
	TaskFoldSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/RokPrototype/UI/Audio/Sound_Ui_CommonClickButton1.Sound_Ui_CommonClickButton1"));
	if (!TaskFoldSound) UE_LOG(LogTemp, Warning, TEXT("Missing source task-fold audio: Sound_Ui_CommonClickButton1"));
	UCanvasPanel* Group = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HotTaskGroup"));
	HotTaskGroup = Group;
	Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ExpandedDesignCanvas, Group, FVector2D(0.0f, 136.0f), FVector2D(232.0f, 381.0f));

	UButton* TaskEntryButton = MakeAtlasButton(TEXT("TaskEntryButton"), MainMenuAtlasPath, TaskEntryRect, MainMenuAtlasSize, FVector2D(77.0f, 76.0f));
	TaskEntryButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenTasks);
	PlaceOnCanvas(Group, TaskEntryButton, FVector2D(10.0f, 0.0f), FVector2D(77.0f, 76.0f))->SetZOrder(5);
	TaskListBackground = MakeHudSlicedAtlasPanel(TEXT("TaskListBackground"), MainMenuAtlasPath,
		PanelBackgroundRect, MainMenuAtlasSize, FMargin(9, 13, 8, 12));
	// ArabLayoutCompment mirrors the serialized Arabic layout for Chinese:
	// img_bg becomes left/top anchored at (+5.63, -44.52) in Unity UI space.
	PlaceOnCanvas(Group, TaskListBackground, FVector2D(5.63f, 44.52f), FVector2D(230.0f, 322.0f));
	TaskFoldButton = MakeAtlasButton(TEXT("TaskFoldButton"), MainMenuAtlasPath, TaskArrowRect, MainMenuAtlasSize, FVector2D(25.0f, 34.0f));
	TaskFoldButton->SetRenderTransformPivot(FVector2D(0.5f));
	if (UImage* Arrow = Cast<UImage>(TaskFoldButton->GetContent()))
	{
		Arrow->SetRenderTransformPivot(FVector2D(0.5f));
		Arrow->SetRenderScale(FVector2D(-1.0f, 1.0f));
	}
	TaskFoldButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleToggleTaskPanel);
	PlaceOnCanvas(Group, TaskFoldButton, FVector2D(202.13f, 49.52f), FVector2D(25.0f, 34.0f));
	TaskListClip = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TaskListClip"));
	TaskListClip->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TaskListClip->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(Group, TaskListClip, FVector2D(15.63f, 81.8f), FVector2D(210.0f, 280.0f));

	UVerticalBox* TaskList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TaskList"));
	TaskList->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	// Animate the viewport, not row height: source sv_list_tasks clips its content.
	PlaceOnCanvas(TaskListClip, TaskList, FVector2D::ZeroVector, FVector2D(210.0f, 320.0f));
	// Only the active task has account/local-simulation state. Do not invent
	// three side-task rows from config while TaskProxy data is unavailable.
	for (int32 Index = 0; Index < 1; ++Index)
	{
		UCanvasPanel* Row = MakeTaskRow(Index);
		USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TaskRowSize"));
		RowSize->SetHeightOverride(80.0f);
		RowSize->SetWidthOverride(196.0f);
		RowSize->SetContent(Row);
		UVerticalBoxSlot* RowSlot = TaskList->AddChildToVerticalBox(RowSize);
		RowSlot->SetHorizontalAlignment(HAlign_Left);
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	ApplyTaskFoldPresentation();

	UCanvasPanel* ArmyQueueEntry = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("ArmyQueueEntry"));
	// MainInterfaceMediator.RefreshTroopsDispatchNum shows this group when there
	// are active marches. The world prototype has no march snapshot yet;
	// keep the shared ArmyShow route, not the old diagnostic --/-- row.
	ArmyQueueEntry->SetVisibility(bWorldMapTacticalOverlay
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		ArmyQueueEntry,
		FVector2D(20.0f, 525.0f),
		FVector2D(180.0f, 48.0f));
	PlaceOnCanvas(
		ArmyQueueEntry,
		MakeAtlasBorder(
			TEXT("ArmyQueueEntryBackground"),
			MainMenuAtlasPath,
			PanelBackgroundRect,
			MainMenuAtlasSize,
			FVector2D(180.0f, 48.0f),
			FMargin(0.375f, 0.342f, 0.333f, 0.316f)),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 48.0f));
	UTextBlock* ArmyQueueLabel = MakeHudText(
		TEXT("ArmyQueueEntryLabel"),
		17,
		FLinearColor(0.94f, 0.93f, 0.88f, 1.0f),
		true);
	ArmyQueueLabel->SetText(NSLOCTEXT("RokHud", "ArmyQueue", "行军队列"));
	PlaceOnCanvas(
		ArmyQueueEntry,
		ArmyQueueLabel,
		FVector2D(14.0f, 8.0f),
		FVector2D(96.0f, 30.0f),
		2);
	UTextBlock* ArmyQueueState = MakeHudText(
		TEXT("ArmyQueueEntryState"),
		15,
		FLinearColor(0.72f, 0.74f, 0.72f, 1.0f),
		true);
	ArmyQueueState->SetText(FText::FromString(TEXT("--/--")));
	ArmyQueueState->SetJustification(ETextJustify::Center);
	PlaceOnCanvas(
		ArmyQueueEntry,
		ArmyQueueState,
		FVector2D(106.0f, 8.0f),
		FVector2D(48.0f, 30.0f),
		2);
	PlaceOnCanvas(
		ArmyQueueEntry,
		MakeAtlasImage(
			TEXT("ArmyQueueEntryArrow"),
			MainMenuAtlasPath,
			TaskArrowRect,
			MainMenuAtlasSize,
			FVector2D(18.0f, 28.0f)),
		FVector2D(156.0f, 9.0f),
		FVector2D(18.0f, 28.0f),
		3);
	UButton* ArmyQueueButton = MakeTransparentHitButton(TEXT("ArmyQueueButton"));
	ArmyQueueButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenArmyShow);
	PlaceOnCanvas(
		ArmyQueueEntry,
		ArmyQueueButton,
		FVector2D::ZeroVector,
		FVector2D(180.0f, 48.0f))->SetZOrder(10);
}

void URokMainHudWidget::SetChatChannelPresentation(bool bInAlliance, bool bAllianceChannel, int32 AllianceUnread)
{
	bChatAllianceVisible = bInAlliance;
	bChatAllianceSelected = bInAlliance && bAllianceChannel;
	const bool bFlash = bInAlliance && !bChatAllianceSelected && AllianceUnread > 0;
	if (bFlash != bChatUnreadFlash) ChatUnreadTime = 0;
	bChatUnreadFlash = bFlash;
	ApplyChatChannelPresentation();
}

void URokMainHudWidget::ApplyChatChannelPresentation()
{
	if (ChatWorldMarker) ChatWorldMarker->SetRenderOpacity(bChatAllianceSelected ? 0 : 1);
	const ESlateVisibility AllianceVisibility = bChatAllianceVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (ChatAllianceMarkerBase) ChatAllianceMarkerBase->SetVisibility(AllianceVisibility);
	if (ChatAllianceMarker)
	{
		ChatAllianceMarker->SetVisibility(AllianceVisibility);
		ChatAllianceMarker->SetRenderOpacity(bChatAllianceSelected ? 1 :
			(bChatUnreadFlash ? RokHudAnimation::ChatUnreadAlpha(ChatUnreadTime) : 0));
	}
}

void URokMainHudWidget::BuildChatAndMapGroup()
{
	// UI_IF_MainInterface chat background: pl_mmu_1000, white tint,
	// source top/bottom borders 6/3 pixels (no horizontal borders).
	UCanvasPanel* ChatBackground = MakeHudSlicedAtlasPanel(TEXT("ChatBackground"),
		MainMenuAtlasPath, ChatBarRect, MainMenuAtlasSize, FMargin(0, 6, 0, 3));
	PlaceOnCanvas(ExpandedDesignCanvas, ChatBackground, FVector2D::ZeroVector, FVector2D(768.0f, 91.0f), FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f));
	// UI_Item_ChatSearchBuild: Chinese mirrored positions from center x=384,
	// local x=166.4/130, y=38.8. Both markers share the original 30x4 sprites.
	ChatWorldMarker = MakeAtlasImage(TEXT("ChatWorldMarker"), MainMenuAtlasPath,
		SpriteRect(275, 817, 30, 4), MainMenuAtlasSize, FVector2D(30, 4));
	ChatAllianceMarkerBase = MakeAtlasImage(TEXT("ChatAllianceMarkerBase"), MainMenuAtlasPath,
		SpriteRect(275, 823, 30, 4), MainMenuAtlasSize, FVector2D(30, 4));
	ChatAllianceMarker = MakeAtlasImage(TEXT("ChatAllianceMarker"), MainMenuAtlasPath,
		SpriteRect(275, 817, 30, 4), MainMenuAtlasSize, FVector2D(30, 4));
	PlaceOnCanvas(ExpandedDesignCanvas, ChatWorldMarker, FVector2D(202.6f, -84.3f), FVector2D(30, 4), FAnchors(0, 1), FVector2D(0, 0.5));
	PlaceOnCanvas(ExpandedDesignCanvas, ChatAllianceMarkerBase, FVector2D(239, -84.3f), FVector2D(30, 4), FAnchors(0, 1), FVector2D(0, 0.5));
	PlaceOnCanvas(ExpandedDesignCanvas, ChatAllianceMarker, FVector2D(239, -84.3f), FVector2D(30, 4), FAnchors(0, 1), FVector2D(0, 0.5));
	ApplyChatChannelPresentation();

	UButton* ChatButton = MakeAtlasButton(
		TEXT("ChatButton"),
		MainMenuAtlasPath,
		ChatRect,
		MainMenuAtlasSize,
		FVector2D(50.0f, 47.0f));
	ChatButton->OnClicked.AddDynamic(
		this,
		&URokMainHudWidget::HandleOpenChat);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		ChatButton,
		FVector2D(134.0f, -18.0f),
		FVector2D(50.0f, 47.0f),
		FAnchors(0.0f, 1.0f),
		FVector2D(0.0f, 1.0f));
	UButton* BuildCityButton = MakeAtlasButton(
		TEXT("BuildCityButton"),
		MainMenuAtlasPath,
		BuildRect,
		MainMenuAtlasSize,
		FVector2D(92.0f, 92.0f));
	BuildCityButton->OnClicked.AddDynamic(
		this,
		&URokMainHudWidget::HandleOpenBuildCity);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		BuildCityButton,
		FVector2D(18.0f, -158.0f),
		FVector2D(92.0f, 92.0f),
		FAnchors(0.0f, 1.0f),
		FVector2D(0.0f, 1.0f));
	UButton* CityWorldButton = MakeAtlasButton(
		TEXT("CityWorldButton"),
		MainMenuAtlasPath,
		CityRect,
		MainMenuAtlasSize,
		FVector2D(127.0f, 148.0f));
	CityWorldButton->OnClicked.AddDynamic(
		this,
		&URokMainHudWidget::HandleReturnToWorldMap);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		CityWorldButton,
		FVector2D::ZeroVector,
		FVector2D(127.0f, 148.0f),
		FAnchors(0.0f, 1.0f),
		FVector2D(0.0f, 1.0f));
}

void URokMainHudWidget::BuildFeatureGroup()
{
	FeatureFoldSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/RokPrototype/UI/Audio/Sound_Ui_PickUp.Sound_Ui_PickUp"));
	if (!FeatureFoldSound) UE_LOG(LogTemp, Warning, TEXT("Missing source feature-fold audio: Sound_Ui_PickUp"));
	CollapsibleFeatureWidgets.Reset();
	// UI_IF_MainInterface/pl_menu/UI_Item_Features/pl_rect, Chinese runtime
	// layout after ArabLayoutCompment: keep the background separate from mask.
	// ui_mainmenu/pl_mmu_1019: Unity border L/B/R/T = 59/46/53/50.
	FeatureBackground = MakeHudSlicedAtlasPanel(TEXT("FeatureBackground"), MainMenuAtlasPath,
		FeatureBackgroundRect, MainMenuAtlasSize, FMargin(59, 50, 53, 46));
	PlaceOnCanvas(ExpandedDesignCanvas, FeatureBackground, FVector2D(-12.445f, -9.465f), FVector2D(539.06f, 98.0f), FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f));
	CollapsibleFeatureWidgets.Add(FeatureBackground);
	UCanvasPanel* FeatureClip = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FeatureButtonsClip"));
	FeatureClip->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	FeatureClip->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(ExpandedDesignCanvas, FeatureClip, FVector2D(-120.975f, 13.935f), FVector2D(430.0f, 134.8f), FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f));

	UButton* CampaignCoreButton = nullptr;
	UCanvasPanel* CampaignButton = MakeFeatureButton(
		TEXT("CampaignFeature"),
		NSLOCTEXT("RokHud", "Campaign", "战役"),
		CampaignRect,
		false,
		&CampaignCoreButton);
	PlaceOnCanvas(FeatureClip, CampaignButton, FVector2D(0.0f, 10.8f), FVector2D(100.0f, 100.0f));
	CollapsibleFeatureWidgets.Add(CampaignButton);
	if (CampaignCoreButton)
	{
		CampaignCoreButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenExpedition);
	}
	UButton* AllianceCoreButton = nullptr;
	UCanvasPanel* AllianceButton = MakeFeatureButton(TEXT("AllianceFeature"), NSLOCTEXT("RokHud", "Alliance", "联盟"), AllianceRect, false, &AllianceCoreButton);
	PlaceOnCanvas(FeatureClip, AllianceButton, FVector2D(110.0f, 10.8f), FVector2D(100.0f, 100.0f));
	CollapsibleFeatureWidgets.Add(AllianceButton);
	if (AllianceCoreButton)
	{
		AllianceCoreButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenAlliance);
	}
	UButton* ItemsCoreButton = nullptr;
	UCanvasPanel* ItemsButton = MakeFeatureButton(TEXT("ItemsFeature"), NSLOCTEXT("RokHud", "Items", "道具"), ItemsRect, false, &ItemsCoreButton);
	PlaceOnCanvas(FeatureClip, ItemsButton, FVector2D(220.0f, 10.8f), FVector2D(100.0f, 100.0f));
	CollapsibleFeatureWidgets.Add(ItemsButton);
	if (ItemsCoreButton)
	{
		ItemsCoreButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenItems);
	}
	UButton* CommanderCoreButton = nullptr;
	UCanvasPanel* CommanderButton = MakeFeatureButton(TEXT("CommanderFeature"), NSLOCTEXT("RokHud", "Commander", "统帅"), CommanderRect, true, &CommanderCoreButton);
	PlaceOnCanvas(FeatureClip, CommanderButton, FVector2D(330.0f, 10.8f), FVector2D(100.0f, 100.0f));
	CollapsibleFeatureWidgets.Add(CommanderButton);
	if (CommanderCoreButton)
	{
		CommanderCoreButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenCommander);
	}
	UButton* MoreButton = MakeAtlasButton(TEXT("MoreFeatureButton"), MainMenuAtlasPath, MoreRect, MainMenuAtlasSize, FVector2D(87.0f, 88.0f));
	// UI_Model_more overrides both btn_btn and its img_hide child with the
	// same sprite. MainInterfaceMediator switches img_hide immediately on/off.
	UWidget* MoreBaseImage = MoreButton->GetContent();
	MoreButton->SetContent(nullptr);
	UCanvasPanel* MoreLayers = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("MoreFeatureLayers"));
	MoreLayers->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(MoreLayers, MoreBaseImage, FVector2D::ZeroVector, FVector2D(87, 88));
	MoreFeatureOverlay = MakeAtlasImage(TEXT("MoreFeatureSourceHideImage"), MainMenuAtlasPath,
		MoreRect, MainMenuAtlasSize, FVector2D(87, 88));
	PlaceOnCanvas(MoreLayers, MoreFeatureOverlay, FVector2D::ZeroVector, FVector2D(87, 88));
	MoreButton->SetContent(MoreLayers);
	MoreFeatureButton = MoreButton;
	MoreButton->SetRenderTransformPivot(FVector2D(0.5f));
	MoreButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleToggleFeatureBar);
	PlaceOnCanvas(ExpandedDesignCanvas, MoreButton, FVector2D(-20.045f, -14.865f), FVector2D(87.0f, 88.0f), FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f));
	ApplyFeatureBarVisibility();

	UCanvasPanel* MailButton = MakeFeatureButton(TEXT("MailFeature"), FText::GetEmpty(), MailRect, false);
	PlaceOnCanvas(ExpandedDesignCanvas, MailButton, FVector2D(-16.0f, -116.0f), FVector2D(100.0f, 100.0f), FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f));
	UButton* MailHitButton = MakeTransparentHitButton(TEXT("MailFeatureHitButton"));
	MailHitButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenMail);
	PlaceOnCanvas(ExpandedDesignCanvas, MailHitButton, FVector2D(-16.0f, -116.0f), FVector2D(100.0f, 100.0f), FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f));
	UButton* ActivityButton = MakeAtlasButton(TEXT("ActivityButton"), MainMenuAtlasPath, ActivityRect, MainMenuAtlasSize, FVector2D(80.0f, 80.0f));
	ActivityButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenActivityCenter);
	PlaceOnCanvas(ExpandedDesignCanvas, ActivityButton, FVector2D(-12.0f, 112.0f), FVector2D(80.0f, 80.0f), FAnchors(1.0f, 0.0f), FVector2D(1.0f, 0.0f));
	PlaceOnCanvas(ExpandedDesignCanvas, MakeAtlasImage(TEXT("AllianceGuideRedPoint"), CommonAtlasPath, RedPointRect, CommonAtlasSize, FVector2D(27.0f, 29.0f)), FVector2D(-21.0f, 104.0f), FVector2D(27.0f, 29.0f), FAnchors(1.0f, 0.0f), FVector2D(1.0f, 0.0f));
}

void URokMainHudWidget::ApplyFeatureBarVisibility()
{
	if (MoreFeatureOverlay)
	{
		MoreFeatureOverlay->SetVisibility(bFeatureBarExpanded
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	const bool bMoving = FeatureAnimationTime < RokHudAnimation::FeatureDuration;
	const ESlateVisibility FeatureVisibility = bMoving
		? ESlateVisibility::HitTestInvisible
		: (bFeatureBarExpanded ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	for (UWidget* Widget : CollapsibleFeatureWidgets)
	{
		if (Widget)
		{
			Widget->SetVisibility(FeatureVisibility);
			if (Widget != FeatureBackground)
			{
				Widget->SetRenderTranslation(FVector2D(FeatureButtonsOffset, 0.0f));
			}
		}
	}
	if (FeatureBackground)
	{
		// Unity's centered size delta -437.96 + center X218.98 keeps the
		// right edge fixed; this UMG slot is already right-anchored.
		const float Width = 539.06f - 437.96f * FeatureFold;
		CastChecked<UCanvasPanelSlot>(FeatureBackground->Slot)->SetSize(FVector2D(Width, 98.0f));
		// Unity Image.GetAdjustedBorders proportionally compresses borders when
		// the animated rect is narrower than their sum (collapsed width 101.1).
		const float BorderScale = FMath::Min(1.0f, Width / 112.0f);
		const float Left = 59.0f * BorderScale;
		const float Right = 53.0f * BorderScale;
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Column = 0; Column < 3; ++Column)
			{
				UCanvasPanelSlot* PatchSlot = CastChecked<UCanvasPanelSlot>(FeatureBackground->GetChildAt(Row * 3 + Column)->Slot);
				FMargin Offsets = PatchSlot->GetOffsets();
				Offsets.Left = Column == 0 ? 0.0f : (Column == 1 ? Left : -Right);
				Offsets.Right = Column == 0 ? Left : Right;
				PatchSlot->SetOffsets(Offsets);
			}
		}
		FeatureBackground->SetRenderOpacity(FeatureAlpha);
	}
	if (MoreFeatureButton)
	{
		// Unity Z rotation is opposite UMG's screen-down rotation.
		MoreFeatureButton->SetRenderTransformAngle(90.0f * FeatureFold);
	}
}

void URokMainHudWidget::ApplyTaskFoldPresentation()
{
	if (!TaskListBackground || !TaskListClip || !TaskFoldButton) return;
	CastChecked<UCanvasPanelSlot>(TaskListBackground->Slot)->SetSize(
		FVector2D(FMath::Lerp(230.0f, 166.0f, TaskFold), FMath::Lerp(322.0f, 43.0f, TaskFold)));
	CastChecked<UCanvasPanelSlot>(TaskListClip->Slot)->SetSize(
		FVector2D(FMath::Lerp(210.0f, 146.0f, TaskFold), FMath::Lerp(280.0f, 0.0f, TaskFold)));
	// Source list pivot/anchor is centered with anchored Y=-16.28.
	CastChecked<UCanvasPanelSlot>(TaskListClip->Slot)->SetPosition(
		FVector2D(15.63f, 81.8f + 0.5f * TaskFold));
	TaskListClip->SetVisibility(TaskFold >= 1.0f
		? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	CastChecked<UCanvasPanelSlot>(TaskFoldButton->Slot)->SetPosition(
		FVector2D(202.13f - 64.0f * TaskFold, 49.52f));
	TaskFoldButton->SetRenderTransformAngle(-180.0f * TaskFold);
}

void URokMainHudWidget::SetTaskRewardPresentation(int32 TaskId, bool bComplete)
{
	const bool bVisible = TaskId > 0 && bComplete;
	if (TaskId != RewardTaskId || bVisible != bTaskRewardVisible)
	{
		TaskRewardTime = 0.0f;
		if (TaskRewardImage)
		{
			TaskRewardImage->SetRenderScale(FVector2D(1.0f));
			TaskRewardImage->SetRenderTransformAngle(0.0f);
		}
	}
	RewardTaskId = TaskId;
	bTaskRewardVisible = bVisible;
	if (TaskRewardImage)
	{
		TaskRewardImage->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void URokMainHudWidget::SetPowerPresentation(int32 Value)
{
	if (PowerGain) PowerGain->ObservePower(PowerCounter.Target, Value, IsVisible() && GetRenderOpacity() > 0);
	PowerCounter.SetTarget(Value);
	if (PowerText) PowerText->SetText(PowerCounter.Target > 0
		? FText::AsNumber(PowerCounter.Value) : NSLOCTEXT("RokHud", "ProfilePowerUnavailable", "--"));
}

void URokMainHudWidget::AdvanceHudAnimations(float DeltaSeconds)
{
	if (PowerGain) PowerGain->Advance(DeltaSeconds);
	const int32 PreviousPower = PowerCounter.Value;
	PowerCounter.Advance(DeltaSeconds);
	if (PreviousPower != PowerCounter.Value) SetPowerPresentation(PowerCounter.Target);
	if (bChatUnreadFlash)
	{
		ChatUnreadTime = FMath::Fmod(ChatUnreadTime + FMath::Max(0.0f, DeltaSeconds), 1.0f);
		ApplyChatChannelPresentation();
	}
	if (bTaskRewardVisible && TaskRewardImage)
	{
		TaskRewardTime = FMath::Fmod(TaskRewardTime + FMath::Max(0.0f, DeltaSeconds), RokHudAnimation::RewardDuration);
		const float Pulse = RokHudAnimation::RewardPulse(TaskRewardTime);
		TaskRewardImage->SetRenderScale(FVector2D(1.0f + 0.2f * Pulse));
		TaskRewardImage->SetRenderTransformAngle(-2.0f * Pulse);
	}
	if (TaskAnimationTime < RokHudAnimation::TaskDuration)
	{
		TaskAnimationTime = FMath::Min(RokHudAnimation::TaskDuration, TaskAnimationTime + FMath::Max(0.0f, DeltaSeconds));
		TaskFold = FMath::Lerp(TaskFoldStart, bTaskCollapsed ? 1.0f : 0.0f,
			RokHudAnimation::Smooth(TaskAnimationTime, RokHudAnimation::TaskDuration));
		ApplyTaskFoldPresentation();
	}
	if (FeatureAnimationTime < RokHudAnimation::FeatureDuration)
	{
		FeatureAnimationTime = FMath::Min(RokHudAnimation::FeatureDuration, FeatureAnimationTime + FMath::Max(0.0f, DeltaSeconds));
		const float Smooth = RokHudAnimation::Smooth(FeatureAnimationTime, RokHudAnimation::FeatureDuration);
		FeatureFold = FMath::Lerp(FeatureFoldStart, bFeatureBarExpanded ? 0.0f : 1.0f, Smooth);
		const float ButtonsProgress = bFeatureBarExpanded
			? 1.0f - RokHudAnimation::FeatureOpenOffset(FeatureAnimationTime) / RokHudAnimation::FeatureTravel
			: Smooth;
		FeatureButtonsOffset = FMath::Lerp(FeatureButtonsStart,
			bFeatureBarExpanded ? 0.0f : RokHudAnimation::FeatureTravel, ButtonsProgress);
		FeatureAlpha = bFeatureBarExpanded
			? FMath::Lerp(FeatureAlphaStart, 1.0f, RokHudAnimation::Smooth(FeatureAnimationTime, 1.0f / 60.0f))
			: FMath::Lerp(FeatureAlphaStart, 0.0f, RokHudAnimation::Smooth(FeatureAnimationTime - 14.0f / 60.0f, 1.0f / 60.0f));
		ApplyFeatureBarVisibility();
	}
}

void URokMainHudWidget::BuildCityMarker()
{
	CityMarkerRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerCityWorldMarker"));
	CityMarkerRoot->SetVisibility(ESlateVisibility::Collapsed);
	CityMarkerCanvasSlot = PlaceOnCanvas(ExpandedDesignCanvas, CityMarkerRoot, FVector2D(640.0f, 710.0f), FVector2D(160.0f, 110.0f));

	PlaceOnCanvas(CityMarkerRoot, MakeAtlasBorder(TEXT("CityNameBackground"), MapAtlasPath, CityNameBackgroundRect, MapAtlasSize, FVector2D(140.0f, 42.0f), FMargin(0.2f)), FVector2D(10.0f, 14.0f), FVector2D(140.0f, 42.0f));
	CityNameText = MakeHudText(TEXT("CityNameText"), 20, FLinearColor(0.12f, 0.95f, 0.48f, 1.0f), true);
	CityNameText->SetText(NSLOCTEXT("RokHud", "InitialLocalCityName", "本地预览"));
	CityNameText->SetJustification(ETextJustify::Center);
	PlaceOnCanvas(CityMarkerRoot, CityNameText, FVector2D(20.0f, 19.0f), FVector2D(120.0f, 30.0f));
	PlaceOnCanvas(CityMarkerRoot, MakeAtlasImage(TEXT("CityLevelBackground"), MapAtlasPath, CityLevelBackgroundRect, MapAtlasSize, FVector2D(60.0f, 33.0f)), FVector2D(50.0f, 52.0f), FVector2D(60.0f, 33.0f));
	CityLevelText = MakeHudText(TEXT("CityLevelText"), 18, FLinearColor(0.16f, 0.14f, 0.11f, 1.0f), true);
	CityLevelText->SetText(FText::AsNumber(1));
	CityLevelText->SetJustification(ETextJustify::Center);
	PlaceOnCanvas(CityMarkerRoot, CityLevelText, FVector2D(50.0f, 54.0f), FVector2D(60.0f, 29.0f));
}

void URokMainHudWidget::UpdateExpandedDesignResolution(const FVector2D& RootSize)
{
	if (!ExpandedDesignSizeBox || RootSize.X <= 1.0f || RootSize.Y <= 1.0f)
	{
		return;
	}

	const float ViewAspect = RootSize.X / RootSize.Y;
	const float ReferenceAspect = UnityReferenceWidth / UnityReferenceHeight;
	FVector2D DesiredDesignSize;
	if (ViewAspect >= ReferenceAspect)
	{
		DesiredDesignSize = FVector2D(UnityReferenceHeight * ViewAspect, UnityReferenceHeight);
	}
	else
	{
		DesiredDesignSize = FVector2D(UnityReferenceWidth, UnityReferenceWidth / ViewAspect);
	}

	if (!DesiredDesignSize.Equals(CurrentExpandedDesignSize, 0.5f))
	{
		CurrentExpandedDesignSize = DesiredDesignSize;
		ExpandedDesignSizeBox->SetWidthOverride(CurrentExpandedDesignSize.X);
		ExpandedDesignSizeBox->SetHeightOverride(CurrentExpandedDesignSize.Y);
	}
}

void URokMainHudWidget::UpdateCityMarkerProjection()
{
	if (!CityMarkerCanvasSlot.IsValid() || !GetWorld() || CurrentRootSize.X <= 1.0f || CurrentRootSize.Y <= 1.0f)
	{
		if (CityMarkerRoot)
		{
			CityMarkerRoot->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (!CityMarkerAnchorActor.IsValid())
	{
		TArray<AActor*> CityHallActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("UnityInitId_5"), CityHallActors);
		if (!CityHallActors.IsEmpty())
		{
			CityMarkerAnchorActor = CityHallActors[0];
		}
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController || !CityMarkerAnchorActor.IsValid())
	{
		CityMarkerRoot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FVector2D ScreenPosition;
	if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PlayerController,
		CityMarkerAnchorActor->GetActorLocation(),
		ScreenPosition,
		true))
	{
		const FVector2D DesignPosition(
			ScreenPosition.X * CurrentExpandedDesignSize.X / CurrentRootSize.X,
			ScreenPosition.Y * CurrentExpandedDesignSize.Y / CurrentRootSize.Y);
		CityMarkerCanvasSlot->SetPosition(DesignPosition + FVector2D(-80.0f, 145.0f));
		CityMarkerRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		CityMarkerRoot->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URokMainHudWidget::AdvancePlacementButtonTints(float DeltaSeconds)
{
	const auto AdvanceIcon = [DeltaSeconds](UImage* Icon, UMaterialInstanceDynamic* GrayMaterial,
		const FLinearColor& DesiredSourceTint, FLinearColor& Current,
		FLinearColor& Start, FLinearColor& Target, float& Elapsed)
	{
		if (!Icon)
		{
			return;
		}
		if (!Target.Equals(DesiredSourceTint, KINDA_SMALL_NUMBER))
		{
			Start = Current;
			Target = DesiredSourceTint;
			Elapsed = 0.0f;
		}
		Elapsed = FMath::Min(PlacementButtonTintSeconds,
			Elapsed + FMath::Max(0.0f, DeltaSeconds));
		Current = FMath::Lerp(Start, Target, Elapsed / PlacementButtonTintSeconds);
		if (GrayMaterial && Icon->GetBrush().GetResourceObject() == GrayMaterial)
		{
			// The dedicated Placement material applies Unity Gamma ColorTint
			// inside SourceTint; Slate then multiplies its white vertex once.
			GrayMaterial->SetVectorParameterValue(TEXT("SourceTint"), Current);
			Icon->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			Icon->SetColorAndOpacity(PlacementSourceTintToSlate(Current));
		}
	};
	const auto ResetIcon = [](UImage* Icon, FLinearColor& Current,
		FLinearColor& Start, FLinearColor& Target, float& Elapsed)
	{
		Current = FLinearColor::White;
		Start = FLinearColor::White;
		Target = FLinearColor::White;
		Elapsed = PlacementButtonTintSeconds;
		if (Icon)
		{
			Icon->SetColorAndOpacity(FLinearColor::White);
		}
	};
	if (!bPlacementControlsVisible && PlacementMenuAnimation.IsClosing())
	{
		// HUDUI.Close leaves the existing graphics alive until the Off clip's
		// callback destroys the source popup at 0.25 s.
		AdvanceIcon(PlacementConfirmIcon.Get(), PlacementConfirmGrayMaterial.Get(), PlacementConfirmTintTarget,
			PlacementConfirmSourceTint, PlacementConfirmTintStart,
			PlacementConfirmTintTarget, PlacementConfirmTintElapsed);
		AdvanceIcon(PlacementCancelIcon.Get(), nullptr, PlacementCancelTintTarget,
			PlacementCancelSourceTint, PlacementCancelTintStart,
			PlacementCancelTintTarget, PlacementCancelTintElapsed);
		return;
	}
	if (!bPlacementControlsVisible)
	{
		// Closing destroys both source GameButtons. Reopening starts with their
		// serialized white Image colors, without a stale hover/disabled tween.
		ResetIcon(PlacementConfirmIcon.Get(), PlacementConfirmSourceTint,
			PlacementConfirmTintStart, PlacementConfirmTintTarget, PlacementConfirmTintElapsed);
		ResetIcon(PlacementCancelIcon.Get(), PlacementCancelSourceTint,
			PlacementCancelTintStart, PlacementCancelTintTarget, PlacementCancelTintElapsed);
		return;
	}

	FLinearColor ConfirmDesired = PlacementButtonDisabledSourceTint;
	if (bPlacementCanCommit && PlacementConfirmButton)
	{
		ConfirmDesired = PlacementConfirmButton->IsPressed()
			? PlacementButtonPressedSourceTint
			: (PlacementConfirmButton->IsHovered() ? PlacementButtonHoverSourceTint : FLinearColor::White);
	}
	const FLinearColor CancelDesired = PlacementCancelButton && PlacementCancelButton->IsPressed()
		? PlacementButtonPressedSourceTint
		: (PlacementCancelButton && PlacementCancelButton->IsHovered()
			? PlacementButtonHoverSourceTint : FLinearColor::White);
	AdvanceIcon(PlacementConfirmIcon.Get(), PlacementConfirmGrayMaterial.Get(), ConfirmDesired, PlacementConfirmSourceTint,
		PlacementConfirmTintStart, PlacementConfirmTintTarget, PlacementConfirmTintElapsed);
	AdvanceIcon(PlacementCancelIcon.Get(), nullptr, CancelDesired, PlacementCancelSourceTint,
		PlacementCancelTintStart, PlacementCancelTintTarget, PlacementCancelTintElapsed);
}

void URokMainHudWidget::AdvancePlacementMenuAnimation(float DeltaSeconds)
{
	if (!PlacementControls || PlacementControls->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}
	const bool bClosing = PlacementMenuAnimation.IsClosing();
	const bool bCloseFinished = PlacementMenuAnimation.Advance(DeltaSeconds);
	// Source HUDUI.Close calls Animator.Play("Close") and starts the Off clip at
	// alpha 1 even if the On clip was interrupted. The shared animation helper
	// otherwise preserves the interrupted outgoing opacity for other windows.
	const FRokUiWindowAnimationSample Sample = bClosing
		? FRokUiWindowAnimation::Evaluate(ERokUiWindowAnimationStyle::BuildingMenu,
			true, PlacementMenuAnimation.GetElapsedSeconds())
		: PlacementMenuAnimation.GetSample();
	PlacementControls->SetRenderOpacity(Sample.Opacity);
	if (bCloseFinished)
	{
		// UA_BuildingMenu_Off calls CloseAnimatorCallback at 0.25 s; the HUD
		// manager then destroys that popup instance.
		PlacementControls->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URokMainHudWidget::UpdatePlacementMenuProjection()
{
	if (!bPlacementControlsVisible || !PlacementControls || !PlacementControlsSlot.IsValid())
	{
		return;
	}
	ARokPlayerController* Controller = Cast<ARokPlayerController>(GetOwningPlayer());
	const FRokCityPlacementRuntimeSnapshot Snapshot = Controller
		? Controller->GetCityPlacementPreviewSnapshot() : FRokCityPlacementRuntimeSnapshot();
	ARokCityPlacementPreviewActor* Preview = Cast<ARokCityPlacementPreviewActor>(Snapshot.PreviewActor.Get());
	UStaticMeshComponent* Bottom = Preview ? Preview->GetBottomMeshComponent() : nullptr;
	ARokStrategyCameraPawn* Camera = Controller
		? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
	UCanvasPanelSlot* MenuSlot = PlacementControlsSlot.Get();
	if (!Snapshot.bActive || !Preview || !Bottom || !Camera)
	{
		// The explicit no-preview feedback route has no Unity world target.
		// Keep cancellation and its existing reason visible without pretending
		// that a source building menu has a valid screen projection.
		MenuSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		MenuSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		MenuSlot->SetPosition(FVector2D(0.0f, -55.0f));
		PlacementControls->SetRenderScale(FVector2D(1.0f));
		PlacementControls->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		PlacementStatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlacementReasonText->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	const float Dxf = Camera->GetCurrentCityDxf();
	if (!FMath::IsFinite(Dxf) || Dxf <= 0.0f || Dxf > 270.0f
		|| CurrentRootSize.X <= 1.0f || CurrentRootSize.Y <= 1.0f)
	{
		PlacementControls->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	// CityObjData.SIZE_BUTTON[width-1] is (0,+0.25,0) for a 5x5 farm/wood.
	// The preview bottom component follows its candidate root at source Y=.01.
	const FVector SourceButtonWorld = Bottom->GetComponentLocation()
		+ Preview->GetActorTransform().TransformVector(FVector(0.0, 0.0, 0.24));
	FVector2D ScreenPosition;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		Controller, SourceButtonWorld, ScreenPosition, false))
	{
		PlacementControls->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// HUDManager.AutoAnchorUI/SetScale use InitCameraDxf 172 for this popup.
	// The source CanvasScaler matches height against the 1440x810 reference.
	const float MenuScale = (172.0f / Dxf) * (CurrentRootSize.Y / UnityReferenceHeight);
	MenuSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	MenuSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	// HUDManager replaces the prefab root's serialized (0,+35) position with
	// this projected target every update; there is no additional root offset.
	MenuSlot->SetPosition(ScreenPosition);
	PlacementControls->SetRenderScale(FVector2D(MenuScale));
	PlacementControls->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlacementStatusText->SetVisibility(ESlateVisibility::Collapsed);
	PlacementReasonText->SetVisibility(ESlateVisibility::Collapsed);
}

UCanvasPanelSlot* URokMainHudWidget::PlaceOnCanvas(
	UCanvasPanel* Parent,
	UWidget* Child,
	const FVector2D& Position,
	const FVector2D& Size,
	const FAnchors& Anchors,
	const FVector2D& Alignment)
{
	check(Parent);
	check(Child);
	UCanvasPanelSlot* CanvasSlot = Parent->AddChildToCanvas(Child);
	CanvasSlot->SetAnchors(Anchors);
	CanvasSlot->SetAlignment(Alignment);
	CanvasSlot->SetPosition(Position);
	CanvasSlot->SetSize(Size);
	return CanvasSlot;
}

UTextBlock* URokMainHudWidget::MakeHudText(const FName& Name, int32 FontSize, const FLinearColor& Color, bool bBold)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	TextBlock->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), FontSize));
	TextBlock->SetShadowOffset(FVector2D(1.0f, 1.0f));
	TextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
	TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
	return TextBlock;
}

UImage* URokMainHudWidget::MakeAtlasImage(
	const FName& Name,
	const TCHAR* TexturePath,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize)
{
	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	if (UTexture2D* Texture = LoadUiTexture(TexturePath))
	{
		Image->SetBrush(MakeAtlasBrush(Texture, UnityRect, AtlasSize, DesiredSize));
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
	}
	return Image;
}

UBorder* URokMainHudWidget::MakeAtlasBorder(
	const FName& Name,
	const TCHAR* TexturePath,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize,
	const FMargin& BrushMargin)
{
	UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	Border->SetPadding(FMargin(0.0f));
	Border->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UTexture2D* Texture = LoadUiTexture(TexturePath))
	{
		Border->SetBrush(MakeAtlasBrush(Texture, UnityRect, AtlasSize, DesiredSize, ESlateBrushDrawType::Box, BrushMargin));
	}
	else
	{
		Border->SetBrushColor(FLinearColor(0.05f, 0.04f, 0.03f, 0.75f));
	}
	return Border;
}

UCanvasPanel* URokMainHudWidget::MakeHudSlicedAtlasPanel(
	const FName& Name, const TCHAR* TexturePath, const FIntRect& UnityRect,
	const FIntPoint& AtlasSize, const FMargin& BorderPixels)
{
	// UI_IF_MainInterface backgrounds and UI_Item_HotTask/img_bg are Unity Sliced
	// images. Slate Box margins use the entire atlas size, not its UV subrect.
	// Image patches keep the source pixel borders; anchors resize only the center
	// and edge strips as the task-fold animation changes the parent dimensions.
	UCanvasPanel* Panel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name);
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	const int32 Left = FMath::RoundToInt(BorderPixels.Left);
	const int32 Top = FMath::RoundToInt(BorderPixels.Top);
	const int32 Right = FMath::RoundToInt(BorderPixels.Right);
	const int32 Bottom = FMath::RoundToInt(BorderPixels.Bottom);
	const int32 X[] = {UnityRect.Min.X, UnityRect.Min.X + Left, UnityRect.Max.X - Right, UnityRect.Max.X};
	// Unity atlas coordinates start at the bottom; Canvas starts at the top.
	const int32 Y[] = {UnityRect.Max.Y, UnityRect.Max.Y - Top, UnityRect.Min.Y + Bottom, UnityRect.Min.Y};
	const float PositionX[] = {0.0f, static_cast<float>(Left), static_cast<float>(-Right)};
	const float PositionY[] = {0.0f, static_cast<float>(Top), static_cast<float>(-Bottom)};
	const float ExtentX[] = {static_cast<float>(Left), static_cast<float>(Right), static_cast<float>(Right)};
	const float ExtentY[] = {static_cast<float>(Top), static_cast<float>(Bottom), static_cast<float>(Bottom)};
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Column = 0; Column < 3; ++Column)
		{
			const FName PatchName(*FString::Printf(TEXT("%s_%d_%d"), *Name.ToString(), Row, Column));
			if (X[Column + 1] == X[Column] || Y[Row] == Y[Row + 1]) continue;
			UImage* Patch = MakeAtlasImage(PatchName, TexturePath,
				FIntRect(X[Column], Y[Row + 1], X[Column + 1], Y[Row]), AtlasSize,
				FVector2D(X[Column + 1] - X[Column], Y[Row] - Y[Row + 1]));
			UCanvasPanelSlot* PatchSlot = Panel->AddChildToCanvas(Patch);
			PatchSlot->SetAnchors(FAnchors(Column == 2 ? 1.0f : 0.0f, Row == 2 ? 1.0f : 0.0f,
				Column == 0 ? 0.0f : 1.0f, Row == 0 ? 0.0f : 1.0f));
			PatchSlot->SetOffsets(FMargin(PositionX[Column], PositionY[Row], ExtentX[Column], ExtentY[Row]));
		}
	}
	return Panel;
}

UButton* URokMainHudWidget::MakeAtlasButton(
	const FName& Name,
	const TCHAR* TexturePath,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize)
{
	UButton* Button = WidgetTree->ConstructWidget<URokUiAnimatedButton>(URokUiAnimatedButton::StaticClass(), Name);
	FSlateBrush NoBrush;
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle ButtonStyle;
	ButtonStyle.SetNormal(NoBrush);
	ButtonStyle.SetHovered(NoBrush);
	ButtonStyle.SetPressed(NoBrush);
	ButtonStyle.SetDisabled(NoBrush);
	ButtonStyle.SetNormalPadding(FMargin(0.0f));
	ButtonStyle.SetPressedPadding(FMargin(0.0f));
	Button->SetStyle(ButtonStyle);
	Button->SetBackgroundColor(FLinearColor::Transparent);
	Button->SetColorAndOpacity(FLinearColor::White);
	Button->SetContent(MakeAtlasImage(*FString::Printf(TEXT("%sImage"), *Name.ToString()), TexturePath, UnityRect, AtlasSize, DesiredSize));
	return Button;
}

UCanvasPanel* URokMainHudWidget::MakeTaskRow(int32 Index)
{
	UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("TaskRow_%d"), Index));
	Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Row->SetToolTipText(NSLOCTEXT("RokHud", "TaskSnapshotAuthority", "本地任务快照；奖励领取以服务器确认为准"));
	PlaceOnCanvas(Row, MakeHudSlicedAtlasPanel(*FString::Printf(TEXT("TaskRowBackground_%d"), Index),
		CommonAtlasPath, TaskRowRect, CommonAtlasSize, FMargin(16, 13, 15, 13)),
		FVector2D::ZeroVector, FVector2D(196.0f, 80.0f));

	UTextBlock* Title = MakeHudText(*FString::Printf(TEXT("TaskTitle_%d"), Index), 20, Index == 0 ? FLinearColor(1.0f, 0.68f, 0.25f, 1.0f) : FLinearColor(0.96f, 0.94f, 0.9f, 1.0f), true);
	Title->SetText(NSLOCTEXT("RokHud", "TaskLoading", "任务加载中"));
	PlaceOnCanvas(Row, Title, FVector2D(9.685f, 1.1f), FVector2D(135.23f, 30.71f));
	TaskTitleTexts.Add(Title);

	UTextBlock* Progress = MakeHudText(*FString::Printf(TEXT("TaskProgress_%d"), Index), 18, FLinearColor(0.94f, 0.93f, 0.9f, 1.0f));
	Progress->SetAutoWrapText(true);
	Progress->SetWrapTextAt(176.0f);
	Progress->SetLineHeightPercentage(0.8f);
	Progress->SetClipping(EWidgetClipping::ClipToBounds);
	if (Index == 0) Progress->SetVisibility(ESlateVisibility::Visible);
	PlaceOnCanvas(Row, Progress, FVector2D(10.0f, 33.0f), FVector2D(176.0f, 44.0f));
	TaskProgressTexts.Add(Progress);

	TaskRewardImage = MakeAtlasImage(*FString::Printf(TEXT("TaskReward_%d"), Index), MainMenuAtlasPath, TaskRewardRect, MainMenuAtlasSize, FVector2D(30.0f));
	TaskRewardImage->SetRenderTransformPivot(FVector2D(0.5f));
	TaskRewardImage->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(Row, TaskRewardImage, FVector2D(161.8f, 6.9f), FVector2D(30.0f));
	return Row;
}

UCanvasPanel* URokMainHudWidget::MakeResourceCell(
	const FName& Name,
	const FIntRect& IconRect,
	UTextBlock*& OutTextBlock,
	bool bShowAddButton)
{
	const float CellWidth = bShowAddButton ? 164.0f : 136.0f;
	UCanvasPanel* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name);
	Cell->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	// UI_Item_PlayerResources/img_bg uses the same single sliced source image.
	PlaceOnCanvas(Cell, MakeHudSlicedAtlasPanel(*FString::Printf(TEXT("%sBackground"), *Name.ToString()),
		MainMenuAtlasPath, PanelBackgroundRect, MainMenuAtlasSize, FMargin(9, 13, 8, 12)),
		FVector2D(0.0f, 2.0f), FVector2D(CellWidth - (bShowAddButton ? 28.0f : 0.0f), 40.0f));
	PlaceOnCanvas(Cell, MakeAtlasImage(*FString::Printf(TEXT("%sIcon"), *Name.ToString()), CommonAtlasPath, IconRect, CommonAtlasSize, FVector2D(54.0f, 54.0f)), FVector2D(-4.0f, -5.0f), FVector2D(54.0f, 54.0f));
	OutTextBlock = MakeHudText(*FString::Printf(TEXT("%sValue"), *Name.ToString()), 20, FLinearColor(0.96f, 0.95f, 0.9f, 1.0f));
	OutTextBlock->SetJustification(ETextJustify::Center);
	PlaceOnCanvas(Cell, OutTextBlock, FVector2D(43.0f, 7.0f), FVector2D(88.0f, 30.0f));
	if (bShowAddButton)
	{
		PlaceOnCanvas(Cell, MakeAtlasButton(*FString::Printf(TEXT("%sAddButton"), *Name.ToString()), MainMenuAtlasPath, AddRect, MainMenuAtlasSize, FVector2D(36.0f, 36.0f)), FVector2D(128.0f, 4.0f), FVector2D(36.0f, 36.0f));
	}
	return Cell;
}

UCanvasPanel* URokMainHudWidget::MakeFeatureButton(
	const FName& Name,
	const FText& Label,
	const FIntRect& IconRect,
	bool bShowRedPoint,
	UButton** OutButton)
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name);
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UButton* IconButton = MakeAtlasButton(*FString::Printf(TEXT("%sButton"), *Name.ToString()), MainMenuAtlasPath, IconRect, MainMenuAtlasSize, FVector2D(88.0f, 88.0f));
	if (OutButton)
	{
		*OutButton = IconButton;
	}
	PlaceOnCanvas(Root, IconButton, FVector2D(6.0f, 0.0f), FVector2D(88.0f, 88.0f));
	if (!Label.IsEmpty())
	{
		PlaceOnCanvas(Root, MakeAtlasImage(*FString::Printf(TEXT("%sLabelBackground"), *Name.ToString()), MainMenuAtlasPath, FeatureLabelRect, MainMenuAtlasSize, FVector2D(84.0f, 27.0f)), FVector2D(8.0f, 71.0f), FVector2D(84.0f, 27.0f));
		UTextBlock* LabelText = MakeHudText(*FString::Printf(TEXT("%sLabel"), *Name.ToString()), 18, FLinearColor(0.95f, 0.93f, 0.88f, 1.0f));
		LabelText->SetText(Label);
		LabelText->SetJustification(ETextJustify::Center);
		PlaceOnCanvas(Root, LabelText, FVector2D(8.0f, 72.0f), FVector2D(84.0f, 25.0f));
	}
	if (bShowRedPoint)
	{
		PlaceOnCanvas(Root, MakeAtlasImage(*FString::Printf(TEXT("%sRedPoint"), *Name.ToString()), CommonAtlasPath, RedPointRect, CommonAtlasSize, FVector2D(27.0f, 29.0f)), FVector2D(72.0f, 0.0f), FVector2D(27.0f, 29.0f));
	}
	return Root;
}

UButton* URokMainHudWidget::MakeTransparentHitButton(const FName& Name)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	FSlateBrush NoBrush;
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle Style;
	Style.SetNormal(NoBrush);
	Style.SetHovered(NoBrush);
	Style.SetPressed(NoBrush);
	Style.SetDisabled(NoBrush);
	Style.SetNormalPadding(FMargin(0.0f));
	Style.SetPressedPadding(FMargin(0.0f));
	Button->SetStyle(Style);
	return Button;
}

UTexture2D* URokMainHudWidget::LoadUiTexture(const TCHAR* TexturePath) const
{
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TexturePath);
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok UI: Missing Unity HUD atlas %s"), TexturePath);
	}
	return Texture;
}

FSlateBrush URokMainHudWidget::MakeAtlasBrush(
	UTexture2D* Texture,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize,
	ESlateBrushDrawType::Type DrawAs,
	const FMargin& BrushMargin) const
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.ImageSize = DesiredSize;
	Brush.DrawAs = DrawAs;
	Brush.Margin = BrushMargin;
	Brush.Tiling = ESlateBrushTileType::NoTile;
	Brush.ImageType = ESlateBrushImageType::FullColor;

	const float Top = static_cast<float>(AtlasSize.Y - UnityRect.Max.Y);
	const FVector2f MinUv(
		static_cast<float>(UnityRect.Min.X) / static_cast<float>(AtlasSize.X),
		Top / static_cast<float>(AtlasSize.Y));
	const FVector2f MaxUv(
		static_cast<float>(UnityRect.Max.X) / static_cast<float>(AtlasSize.X),
		(Top + static_cast<float>(UnityRect.Height())) / static_cast<float>(AtlasSize.Y));
	Brush.SetUVRegion(FBox2f(MinUv, MaxUv));
	return Brush;
}

URokBuildingUpgradeSubsystem* URokMainHudWidget::GetUpgradeSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
}

void URokMainHudWidget::HandleReturnToWorldMap()
{
	APlayerController* PlayerController = GetOwningPlayer();
	ARokStrategyCameraPawn* CameraPawn =
		PlayerController
			? Cast<ARokStrategyCameraPawn>(PlayerController->GetPawn())
			: nullptr;
	if (!CameraPawn || !CameraPawn->RequestReturnToWorldMap())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok city/world button could not request the world map."));
	}
}

void URokMainHudWidget::HandleOpenPlayerProfile()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok city HUD click: player_profile."));
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->CloseScreen();
	}
	if (ModalWidget)
	{
		ModalWidget->OpenPlayerProfile();
	}
}

void URokMainHudWidget::HandleOpenCityManager()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok city HUD click: city_manager."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenCityManager();
	}
}

void URokMainHudWidget::HandleOpenArmyShow()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok city HUD click: army_show."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenArmyShow();
	}
}

void URokMainHudWidget::HandleOpenFoodResources()
{
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->CloseScreen();
	}
	if (ModalWidget)
	{
		ModalWidget->OpenResourceInventory(0);
	}
}

void URokMainHudWidget::HandleOpenWoodResources()
{
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->CloseScreen();
	}
	if (ModalWidget)
	{
		ModalWidget->OpenResourceInventory(1);
	}
}

void URokMainHudWidget::HandleBuildingInfoRequested(AActor* BuildingActor)
{
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->CloseScreen();
	}
	if (ModalWidget)
	{
		ModalWidget->OpenBuildingInfo(BuildingActor);
	}
}

void URokMainHudWidget::HandleBuildingUpgradeRequested(AActor* BuildingActor)
{
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->CloseScreen();
	}
	if (ModalWidget)
	{
		ModalWidget->OpenBuildingUpgrade(BuildingActor);
	}
}

void URokMainHudWidget::HandleBuildingFeatureRequested(AActor* BuildingActor)
{
	if (!BuildingActor || !FeatureScreenWidget)
	{
		return;
	}
	URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetUpgradeSubsystem();
	if (!UpgradeSubsystem)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok building feature: upgrade subsystem unavailable."));
		return;
	}

	const FRokBuildingState State = UpgradeSubsystem
		->GetBuildingStateForActor(BuildingActor);
	if (State.UnityBuildingTypeId >= 51
		&& State.UnityBuildingTypeId <= 55)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok building feature: decoration removal is not connected; actor=%s."),
			*GetNameSafe(BuildingActor));
		return;
	}

	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (State.bUpgradeInProgress)
	{
		FeatureScreenWidget->OpenSpeedUp(BuildingActor, 20102);
		return;
	}
	if (State.UnityBuildingTypeId == 7)
	{
		UE_LOG(LogTemp, Display, TEXT("Rok building feature route: worker_queue."));
		FeatureScreenWidget->OpenWorkerQueue(BuildingActor);
		return;
	}
	if (State.UnityBuildingTypeId >= 9
		&& State.UnityBuildingTypeId <= 12)
	{
		FeatureScreenWidget->OpenTrainArmy(
			BuildingActor,
			State.UnityBuildingTypeId - 8);
		return;
	}
	if (State.UnityBuildingTypeId == 13)
	{
		// BuildingTypeConfig[13] -> BuildingMenuData[110], study, levels 1..25.
		UE_LOG(LogTemp, Display, TEXT("Rok building feature route: research; local academy level=%d, account research state unknown."), State.Level);
		FeatureScreenWidget->OpenResearch(BuildingActor);
		return;
	}
	if (State.UnityBuildingTypeId == 14)
	{
		UE_LOG(LogTemp, Display, TEXT("Rok building feature route: hospital."));
		FeatureScreenWidget->OpenHospital(BuildingActor);
		return;
	}
	if (State.UnityBuildingTypeId == 28 && ScoutScreenWidget)
	{
		// EnumCityBuildingType.ScoutCamp; BuildingTypeConfig menu action 137.
		FeatureScreenWidget->CloseScreen();
		ScoutScreenWidget->Open(State.Level);
		UE_LOG(LogTemp, Display, TEXT("Rok building feature route: scout camp, local level=%d; server queue unknown."), State.Level);
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Rok building feature: no migrated feature route for Unity building type %d."),
		State.UnityBuildingTypeId);
}

void URokMainHudWidget::HandleOpenActivityCenter()
{
	UE_LOG(LogTemp, Log, TEXT("Rok HUD: activity button clicked."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenActivityCenter();
	}
}

void URokMainHudWidget::HandleOpenMail()
{
	UE_LOG(LogTemp, Display, TEXT("Rok city HUD click: mail."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FString ExplicitMailFixture;
		if (FParse::Value(FCommandLine::Get(), TEXT("RokOpenModal="), ExplicitMailFixture)
			&& ExplicitMailFixture.TrimStartAndEnd().Equals(TEXT("mail_local_unread"), ESearchCase::IgnoreCase))
		{
			FeatureScreenWidget->OpenMailLocalUnreadFixture();
		}
		else
		{
			FeatureScreenWidget->OpenMail();
		}
	}
}

bool URokMainHudWidget::HandleTaskGoTownHallRequested(AActor* BuildingActor)
{
	if (!IsValid(BuildingActor) || BuildingActor->GetWorld() != GetWorld()
		|| !BuildingActor->ActorHasTag(TEXT("UnityInitId_5"))
		|| !BuildingActor->ActorHasTag(TEXT("UnityBuildingType_1"))
		|| !FeatureScreenWidget || !FeatureScreenWidget->IsScreenOpen()
		|| !ModalWidget || !ModalWidget->GetParent() || ModalWidget->IsModalOpen()) return false;
	URokBuildingUpgradeSubsystem* Buildings = GetUpgradeSubsystem();
	FRokBuildingState CachedHall;
	if (!Buildings || !Buildings->TryPeekExistingBuildingStateForActor(BuildingActor, CachedHall)
		|| CachedHall.BuildingKey != FName(TEXT("UnityInitId_5"))
		|| CachedHall.UnityInitId != 5 || CachedHall.UnityBuildingTypeId != 1
		|| CachedHall.Type != ERokBuildingType::TownCenter || CachedHall.Level != 1
		|| CachedHall.bUpgradeInProgress
		|| !FMath::IsNearlyZero(CachedHall.UpgradeStartedAt)
		|| !FMath::IsNearlyZero(CachedHall.UpgradeFinishesAt)) return false;
	// Modal and panel still use the older refreshing getter. This exact
	// cached idle state cannot settle during the synchronous handoff.
	ModalWidget->OpenBuildingInfo(BuildingActor);
	if (!ModalWidget->IsBuildingInfoOpenForActor(BuildingActor))
	{
		ModalWidget->CloseModal();
		return false;
	}
	SetSelectedBuilding(BuildingActor);
	FeatureScreenWidget->CloseScreen();
	return true;
}

void URokMainHudWidget::HandleOpenTasks()
{
	UE_LOG(LogTemp, Display, TEXT("Rok city HUD click: task_center."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenTasks(false);
	}
}

void URokMainHudWidget::HandleOpenCommander()
{
	UE_LOG(LogTemp, Log, TEXT("Rok HUD: commander feature button clicked."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenCommander();
	}
}

void URokMainHudWidget::HandleOpenItems()
{
	UE_LOG(LogTemp, Log, TEXT("Rok HUD: items feature button clicked."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenItems();
	}
}

void URokMainHudWidget::HandleOpenAlliance()
{
	UE_LOG(LogTemp, Log, TEXT("Rok HUD: alliance feature button clicked."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenAlliance(false);
	}
}

void URokMainHudWidget::HandleOpenVip()
{
	UE_LOG(LogTemp, Log, TEXT("Rok HUD: VIP button clicked."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenVip();
	}
}

void URokMainHudWidget::HandleOpenChat()
{
	UE_LOG(LogTemp, Log, TEXT("Rok HUD: chat button clicked."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenChat();
	}
}

void URokMainHudWidget::HandleOpenWorldPositionSearch()
{
	UE_LOG(LogTemp, Log, TEXT("Rok HUD: world-position button clicked."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenWorldPositionSearch();
	}
}

void URokMainHudWidget::HandleOpenMapMarkers()
{
	UE_LOG(LogTemp, Log, TEXT("Rok HUD: map-markers button clicked."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenMapMarkers();
	}
}

void URokMainHudWidget::HandleOpenBuildCity()
{
	UE_LOG(LogTemp, Log, TEXT("Rok HUD: build-city button clicked."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenBuildCity();
	}
}

void URokMainHudWidget::HandleOpenExpedition()
{
	UE_LOG(LogTemp, Log, TEXT("Rok HUD: expedition button clicked."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenExpedition();
	}
}

void URokMainHudWidget::HandleOpenGemShop()
{
	UE_LOG(LogTemp, Log, TEXT("Rok HUD: gem-shop button clicked."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenGemShop();
	}
}

void URokMainHudWidget::HandleOpenCharge()
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Rok HUD: charge button clicked; opening Unity page 107."));
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenCharge(107);
	}
}

void URokMainHudWidget::HandleToggleFeatureBar()
{
	for (UWidget* Widget : CollapsibleFeatureWidgets)
	{
		URokUiAnimatedButton::ResetAnimationsInHierarchy(Widget);
	}
	bFeatureBarExpanded = !bFeatureBarExpanded;
	FeatureFoldStart = FeatureFold;
	FeatureButtonsStart = FeatureButtonsOffset;
	FeatureAlphaStart = FeatureAlpha;
	FeatureAnimationTime = 0.0f;
	ApplyFeatureBarVisibility();
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Rok HUD: feature bar expanded=%s."),
		bFeatureBarExpanded ? TEXT("true") : TEXT("false"));
	PlayFoldSound(FeatureFoldSound);
}

void URokMainHudWidget::HandleToggleTaskPanel()
{
	bTaskCollapsed = !bTaskCollapsed;
	TaskFoldStart = TaskFold;
	TaskAnimationTime = 0.0f;
	UE_LOG(LogTemp, Log, TEXT("Rok HUD: task panel collapsed=%s."),
		bTaskCollapsed ? TEXT("true") : TEXT("false"));
	PlayFoldSound(TaskFoldSound);
}

void URokMainHudWidget::PlayFoldSound(USoundBase* Sound)
{
	// GameButton.OnPointerClick plays after the onClick callback. Press,
	// release and animation tick must not enqueue additional sound requests.
	if (!Sound) return;
#if WITH_DEV_AUTOMATION_TESTS
	++FoldSoundRequests;
#endif
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URokAudioRouterSubsystem* Router = GameInstance->GetSubsystem<URokAudioRouterSubsystem>())
		{
			Router->PlaySfx(Sound);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Rok HUD fold audio requested: %s"), *Sound->GetName());
}
