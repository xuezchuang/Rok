#include "RokMainHudWidget.h"
#include "RokPowerGainWidget.h"
#include "RokHudAnimation.h"
#include "RokMailRewardDetails.h"
#include "RokMainInterfaceModuleAnimation.h"
#include "RokUiAnimatedButton.h"
#include "RokUiWidgets.h"
#include "RokUiSpec.h"
#include "RokCityPlacementPreviewActor.h"
#include "RokCitySpriteActor.h"
#include "RokCityLifeSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokUiTextTable.h"
#include "RokUiFonts.h"
#include "RokGameNetSubsystem.h"
#include "RokSproto.h"
#include "RokUnityConfigSubsystem.h"
#include "RokServerCitySubsystem.h"
#include "RokServerTaskSubsystem.h"
#include "RokServerHeroSubsystem.h"
#include "RokServerArmySubsystem.h"
#include "RokTroopPortrait.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "RokPlayerHeadCatalog.h"
#include "RokPlayerTopBar.h"
#include "RokTrainResourceCost.h"
#include "EngineUtils.h"
#include "RokPlayerController.h"
#include "RokAudioRouterSubsystem.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Algo/Count.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
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
const TCHAR* PlacementShadowPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_button_shadow.rok_building_menu_button_shadow");
const TCHAR* PlacementAtlasPath = TEXT("/Game/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud.rok_placement_ui_hud");
const TCHAR* PlacementGrayMaterialPath = TEXT("/Game/RokPrototype/UI/PlacementAtlases/M_RokPlacementGrayUI.M_RokPlacementGrayUI");
constexpr float PlacementButtonTintSeconds = 0.1f;
const FVector2D PlacementCancelPosition(142.85f, 167.5f);
const FVector2D PlacementConfirmPosition(242.85f, 167.5f);
constexpr float PlacementMenuDesignHeight = 900.0f;
constexpr float PlacementMenuGroundOffsetY = 71.0f;
FAnchorData MakeDefaultPlacementControlsBounds()
{
	FAnchorData Bounds;
	Bounds.Anchors = FAnchors(0.5f, 1.0f);
	Bounds.Alignment = FVector2D(0.5f, 1.0f);
	Bounds.Offsets = FMargin(0.0f, -55.0f, 472.7f, 220.0f);
	return Bounds;
}
const FName ResourceCellNames[] = {TEXT("FoodResource"), TEXT("WoodResource"), TEXT("StoneResource"),
	TEXT("GoldResource"), TEXT("GemResource")};
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
// ui_mainmenu btn_mmu_1017_1 (world-map globe) and btn_mmu_1006 (magnifier).
const FIntRect GlobeRect = SpriteRect(1, 517, 127, 148);
const FIntRect SearchRect = SpriteRect(417, 812, 92, 92);
const FIntRect FeatureBackgroundRect = SpriteRect(317, 907, 147, 101);
const FIntRect FeatureLabelRect = SpriteRect(130, 553, 84, 27);
const FIntRect CampaignRect = SpriteRect(315, 717, 88, 88);
const FIntRect AllianceRect = SpriteRect(320, 627, 88, 88);
const FIntRect ItemsRect = SpriteRect(774, 833, 88, 88);
const FIntRect CommanderRect = SpriteRect(230, 627, 88, 88);
const FIntRect MoreRect = SpriteRect(774, 923, 100, 100);
const FIntRect MailRect = SpriteRect(864, 833, 88, 88);
const FIntRect ActivityRect = SpriteRect(656, 560, 80, 80);
const FIntRect FirstChargeRect = SpriteRect(902, 587, 80, 80);
const FIntRect RiseRoadRect = SpriteRect(738, 642, 80, 80);
const FIntRect SpecialOfferRect = SpriteRect(738, 724, 80, 80);

const FIntRect FoodRect = SpriteRect(865, 349, 80, 80);
const FIntRect WoodRect = SpriteRect(1085, 489, 80, 80);
const FIntRect StoneRect = SpriteRect(947, 431, 80, 80);
const FIntRect GoldRect = SpriteRect(865, 431, 80, 80);
constexpr int32 HospitalFeatureLayer = 110;
const FIntRect GemRect = SpriteRect(783, 435, 80, 80);
const FIntRect TaskRowRect = SpriteRect(436, 165, 63, 55);
// UI_Item_HotTask: the title uses Best Fit, and MainInterfaceMediator.BestFit
// shrinks the wrapped description until it fits the prefab's text rectangle.
const FVector2D TaskTitleSize(135.23f, 30.71f);
const FVector2D TaskDescriptionSize(176.0f, 44.0f);
constexpr int32 TaskTitlePixels = 20;
constexpr int32 TaskDescriptionPixels = 18;

}

FSlateFontInfo URokMainHudWidget::MakeUnitySourceFont(int32 Pixels, bool bBold)
{
	const FName Weight = bBold ? TEXT("Bold") : TEXT("Regular");
	const float PointSize = Pixels * (72.0f / 96.0f);
	// The composite lives in RokUiFonts so WBP text (font role UnitySource)
	// resolves to the very same object.
	const TSharedPtr<const FCompositeFont> SourceFont = RokUiFonts::GetUnitySourceComposite();
	if (SourceFont.IsValid()) return FSlateFontInfo(SourceFont, PointSize, Weight);
	return FCoreStyle::GetDefaultFontStyle(Weight, PointSize);
}

namespace
{
void SetFittedTaskText(UTextBlock& Label, const FText& Text, int32 MaxPixels, const FVector2D& BoxSize)
{
	if (Label.GetWrapTextAt() == BoxSize.X && Label.GetText().EqualTo(Text)) return;

	Label.SetWrapTextAt(BoxSize.X);
	Label.SetText(Text);
	const TSharedRef<SWidget> SlateText = Label.TakeWidget();
	FSlateFontInfo Font = Label.GetFont();
	for (int32 Pixels = MaxPixels; Pixels >= 2; --Pixels)
	{
		// Unity specifies pixels. Slate fonts use points at 96 DPI.
		Font.Size = Pixels * (72.0f / 96.0f);
		Label.SetFont(Font);
		SlateText->SlatePrepass(1.0f);
		const FVector2D DesiredSize = SlateText->GetDesiredSize();
		if (DesiredSize.X <= BoxSize.X && DesiredSize.Y <= BoxSize.Y) break;
	}
}

const FIntRect RedPointRect = SpriteRect(2018, 994, 27, 29);
// BuildingMenuData IDs [2,1] -> ui_hud[btn_hud_1008], ui_hud[btn_hud_1004].
const FIntRect PlacementConfirmRect = SpriteRect(721, 575, 88, 88);
const FIntRect PlacementCancelRect = SpriteRect(1, 435, 88, 88);

const FIntRect PlayerFrameRect = SpriteRect(214, 984, 211, 211);
const FIntRect PlayerHead1000Rect = SpriteRect(1, 345, 211, 211);
const FIntRect VipIcon100Rect = SpriteRect(1, 325, 138, 138);

const FIntRect CityNameBackgroundRect = SpriteRect(431, 144, 69, 36);
const FIntRect CityLevelBackgroundRect = SpriteRect(269, 168, 60, 33);

// UI_Tip_BuildingBar sprites (ui_mainmenu.png.meta / ui_research.png.meta).
const TCHAR* ResearchUiAtlasPath = TEXT("/Game/RokPrototype/UI/ResearchAtlases/rok_research_ui_research.rok_research_ui_research");
const FIntPoint ResearchUiAtlasSize(1024, 2048);
const FIntRect BuildBarBackgroundRect = SpriteRect(215, 682, 7, 18); // icon_mmu_pbbg, border L2 R1
const FIntRect BuildBarFillRect = SpriteRect(221, 565, 7, 15); // icon_mmu_pbbar, border L3 R2
const FIntRect BuildBarBuildingIconRect = SpriteRect(525, 520, 50, 50); // RS.MMU_building
// RS.MMU_levelup names "img_mmu_levelp", which the atlas lacks, so upgrades keep
// the prefab's default img_icon sprite.
const FIntRect BuildBarLevelUpIconRect = SpriteRect(394, 535, 50, 50); // img_mmu_levelup
const FIntRect BuildBarIconBackgroundRect = SpriteRect(471, 1824, 223, 223); // img_research_iconBg

constexpr float UnityReferenceWidth = 1440.0f;
constexpr float UnityReferenceHeight = 810.0f;

// UI_Item_PlayerResources_SubView: ClientUtils.CurrencyFormat for every cell.
FText FormatResource(int64 Value)
{
	return FText::FromString(RokTrainResourceCost::Format(Value));
}

// Live top bar from the game net subsystem, when a role is logged in.
bool GetServerTopBar(const UUserWidget& Widget, FRokPlayerTopBar& Out)
{
	const UGameInstance* GameInstance = Widget.GetGameInstance();
	const URokGameNetSubsystem* Net = GameInstance ? GameInstance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net || Net->GetState() != ERokGameNetState::RoleReady)
	{
		return false;
	}
	FString Error;
	return RokPlayerTopBar::FromRoleInfo(Net->GetRoleInfo(), Out, Error);
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

void URokMainHudWidget::SampleModalAnimationForCapture(float Seconds)
{
	if (ModalWidget && ModalWidget->IsModalOpen())
	{
		ModalWidget->SampleWindowAnimationForCapture(Seconds);
	}
}

void URokMainHudWidget::OpenUiGalleryHotkey()
{
	if (!ModalWidget)
	{
		return;
	}
	ModalWidget->CloseModal();
	ModalWidget->OpenUiGallery();
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
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenMail(false);
#if !UE_BUILD_SHIPPING
			const TCHAR* CommandLine = FCommandLine::Get();
			if (FParse::Param(CommandLine, TEXT("RokLocalOnly"))
				&& FParse::Param(CommandLine, TEXT("RokNoNet"))
				&& FParse::Param(CommandLine, TEXT("RokMailReportPreview")))
			{
				auto* Tab = Cast<UButton>(RokUiFind::Find(FeatureScreenWidget, TEXT("MailTab_1")));
				if (Tab && Tab->GetIsEnabled())
				{
					Tab->OnClicked.Broadcast();
					UE_LOG(LogTemp, Display, TEXT("Rok local mail report preview: clicked MailTab_1."));
				}
			}
#endif
		}
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
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenCommander();
#if !UE_BUILD_SHIPPING
			const TCHAR* CommandLine = FCommandLine::Get();
			if (FParse::Param(CommandLine, TEXT("RokLocalOnly")))
			{
				int32 Index = INDEX_NONE;
				FName PreviewEntry;
				if (FParse::Value(CommandLine, TEXT("RokCommanderPreviewSkill="), Index) && Index >= 0 && Index <= 4)
				{
					PreviewEntry = *FString::Printf(TEXT("CommanderSkillAction_%d"), Index);
				}
				else if (FParse::Value(CommandLine, TEXT("RokCommanderPreviewTalent="), Index) && Index >= 0 && Index <= 2)
				{
					PreviewEntry = *FString::Printf(TEXT("CommanderTalentLabel_%d"), Index);
				}
				else if (FParse::Param(CommandLine, TEXT("RokCommanderPreviewBiography")))
				{
					PreviewEntry = TEXT("CommanderBiographyAction");
				}
				else if (FParse::Param(CommandLine, TEXT("RokCommanderPreviewSort")))
				{
					PreviewEntry = TEXT("CommanderSortAction");
				}
				if (!PreviewEntry.IsNone())
				{
					auto* Entry = Cast<UButton>(RokUiFind::Find(FeatureScreenWidget, PreviewEntry));
					if (Entry && Entry->GetIsEnabled())
					{
						Entry->OnClicked.Broadcast();
					}
				}
			}
#endif
		}
	}
	else if (ModalName == TEXT("items") || ModalName == TEXT("bag"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget) FeatureScreenWidget->OpenItems();
	}
	else if (ModalName == TEXT("alliance") || ModalName == TEXT("alliance_welcome") || ModalName == TEXT("alliance_unjoined"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenAlliance(false);
			FName PreviewEntry;
			if (FParse::Param(FCommandLine::Get(), TEXT("RokAllianceOfficePreview")))
			{
				PreviewEntry = TEXT("AllianceOfficeReferenceOpen");
			}
			else if (FParse::Param(FCommandLine::Get(), TEXT("RokAllianceJoinPreview")))
			{
				PreviewEntry = TEXT("AllianceWelcomeJoin");
			}
			else if (FParse::Param(FCommandLine::Get(), TEXT("RokAllianceCreatePreview")))
			{
				PreviewEntry = TEXT("AllianceWelcomeCreate");
			}
			if (!PreviewEntry.IsNone())
			{
				if (auto* Entry = Cast<UButton>(RokUiFind::Find(FeatureScreenWidget, PreviewEntry)); Entry && Entry->GetIsEnabled())
				{
					Entry->OnClicked.Broadcast();
				}
			}
		}
	}
	else if (ModalName == TEXT("alliance_joined") || ModalName == TEXT("alliance_main"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenAlliance(true);
			int32 JoinedPage = 0;
			if (FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
				&& FParse::Value(FCommandLine::Get(), TEXT("RokAllianceJoinedPage="), JoinedPage)
				&& (JoinedPage == 1 || JoinedPage == 2))
			{
				auto* Tab = Cast<UButton>(RokUiFind::Find(FeatureScreenWidget,
					*FString::Printf(TEXT("AlliancePageHit_%d"), JoinedPage)));
				if (Tab && Tab->GetIsEnabled()) Tab->OnClicked.Broadcast();
			}
		}
	}
	else if (ModalName == TEXT("vip") || ModalName == TEXT("vip0") || ModalName == TEXT("vip_reference"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenVip(ModalName == TEXT("vip_reference"));
			if (FParse::Param(FCommandLine::Get(), TEXT("RokVipHelpPreview")))
			{
				auto* Help = Cast<UButton>(RokUiFind::Find(FeatureScreenWidget, TEXT("VipHelp")));
				if (Help && Help->GetIsEnabled()) Help->OnClicked.Broadcast();
			}
		}
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
	else if (ModalName == TEXT("vip_privilege")
		|| ModalName == TEXT("vip"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenVipPrivilege();
	}	else if (ModalName == TEXT("ui_gallery")
		|| ModalName == TEXT("gallery"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenUiGallery();
	}	else if (ModalName == TEXT("item_exchange")
		|| ModalName == TEXT("reward_choice"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenItemExchange();
	}	else if (ModalName == TEXT("guild_gift")
		|| ModalName == TEXT("alliance_gift"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenGuildGift();
	}	else if (ModalName == TEXT("func_guide")
		|| ModalName == TEXT("guide"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenFuncGuide();
	}	else if (ModalName == TEXT("tavern_reward")
		|| ModalName == TEXT("tavern_pool"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenTavernReward();
	}	else if (ModalName == TEXT("expedition_store")
		|| ModalName == TEXT("expedition_shop"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenExpeditionStore();
	}	else if (ModalName == TEXT("charge_pop")
		|| ModalName == TEXT("recharge_sale"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenChargePop();
	}	else if (ModalName == TEXT("gift_limited")
		|| ModalName == TEXT("limit_time_bag"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenGiftLimitedTime();
	}	else if (ModalName == TEXT("guild_store")
		|| ModalName == TEXT("alliance_store"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenGuildStore();
	}	else if (ModalName == TEXT("guard_tower")
		|| ModalName == TEXT("the_tower"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenTheTower();
	}	else if (ModalName == TEXT("city_wall")
		|| ModalName == TEXT("the_wall"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenTheWall();
	}	else if (ModalName == TEXT("tavern_rate")
		|| ModalName == TEXT("tavern_box"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenTavernBoxDesc();
	}	else if (ModalName == TEXT("vip_store")
		|| ModalName == TEXT("vipstore"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenVipStore();
	}	else if (ModalName == TEXT("item_collection")
		|| ModalName == TEXT("item_compendium"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenItemCollection();
	}	else if (ModalName == TEXT("guild_fortress")
		|| ModalName == TEXT("alliance_build"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenGuildFortress();
	}	else if (ModalName == TEXT("chapter_dialog")
		|| ModalName == TEXT("story_chapter"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		int32 ChapterGroup = 0;
		FParse::Value(FCommandLine::Get(), TEXT("RokChapterDialogGroup="), ChapterGroup);
		if (ModalWidget) ModalWidget->OpenChapterDialog(ChapterGroup);
	}	else if (ModalName == TEXT("guild_purview")
		|| ModalName == TEXT("alliance_purview"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenGuildPurview();
	}	else if (ModalName == TEXT("event_date")
		|| ModalName == TEXT("activity_calendar"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenEventDate();
	}	else if (ModalName == TEXT("bp_rewards")
		|| ModalName == TEXT("battlepass_rewards"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenBattlePassRewards();
	}
	else if (
		ModalName == TEXT("gain_effect")
		|| ModalName == TEXT("gain_effect_economy"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenGainEffect(0);
	}
	else if (ModalName == TEXT("gain_effect_military"))
	{
		if (ModalWidget) ModalWidget->CloseModal();
		if (ModalWidget) ModalWidget->OpenGainEffect(1);
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
		if (FeatureScreenWidget && FParse::Param(FCommandLine::Get(), TEXT("RokArmyShowRulesPreview")))
		{
			auto* Rules = Cast<UButton>(RokUiFind::Find(FeatureScreenWidget, TEXT("ArmyShowRulesButton")));
			if (Rules && Rules->GetIsEnabled()) Rules->OnClicked.Broadcast();
		}
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
		int32 ChargePage = 107;
		FParse::Value(FCommandLine::Get(), TEXT("RokChargePage="), ChargePage);
		if (FeatureScreenWidget) FeatureScreenWidget->OpenCharge(ChargePage >= 101 && ChargePage <= 107 ? ChargePage : 107);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok modal UI: unknown -RokOpenModal value '%s'."), *ModalName);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Rok modal UI: opened command-line mode '%s'."), *ModalName);

	// Captures of states reached by clicks (a ranking category, the frame tab, a
	// level page): -RokModalClick=A+B clicks those modal buttons in order, as a player would.
	FString Clicks;
	if (ModalWidget && FParse::Value(FCommandLine::Get(), TEXT("RokModalClick="), Clicks))
	{
		TArray<FString> Names;
		Clicks.ParseIntoArray(Names, TEXT("+"));
		for (const FString& Name : Names)
		{
			if (UButton* Button = Cast<UButton>(RokUiFind::Find(ModalWidget, FName(*Name))))
			{
				Button->OnClicked.Broadcast();
				UE_LOG(LogTemp, Display, TEXT("Rok modal UI: clicked %s"), *Name);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Rok modal UI: no button %s to click"), *Name);
			}
		}
	}
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
	FRokPlayerTopBar ServerTopBar;
	const bool bServerTopBar = GetServerTopBar(*this, ServerTopBar);
	const bool bServerTasks = ApplyServerTasks();
	RefreshNotificationBadges();
	UpdateServerArmyHud();
	URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetUpgradeSubsystem();
	FRokBuildingState LocalCityHallState;
	bool bHasLocalCityHallState = false;
	if (UpgradeSubsystem)
	{
		const FRokResourceStock Resources = UpgradeSubsystem->GetCurrentResources();
		if (FoodText && !bServerTopBar)
		{
			FoodText->SetText(FormatResource(Resources.Food));
		}
		if (WoodText && !bServerTopBar)
		{
			WoodText->SetText(FormatResource(Resources.Wood));
		}
		if (GemText && !bServerTopBar)
		{
			GemText->SetText(FormatResource(Resources.Denar));
		}

		if (CityMarkerAnchorActor.IsValid())
		{
			LocalCityHallState = UpgradeSubsystem->GetBuildingStateForActor(CityMarkerAnchorActor.Get());
			bHasLocalCityHallState = true;
			if (CityLevelText && !bServerTopBar)
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
			if (PowerText && !bServerTopBar)
			{
				SetPowerPresentation(ProfileState.LocalPreviewScore);
				PowerText->SetToolTipText(NSLOCTEXT(
					"RokHud",
					"ProfilePowerTooltip",
					"基础预览评分＋当前城市升级增量，不是服务器正式战力"));
			}
			if (CityNameText && !bServerTopBar)
			{
				CityNameText->SetText(ProfileState.bHasDisplayName
					? FText::FromString(ProfileState.DisplayName)
					: NSLOCTEXT("RokHud", "LocalCityName", "本地预览"));
			}

			if (!bServerTasks)
			{
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
					FText TitleText;
					FText DescriptionText;
					if (Index == 0 && ActiveTask.TaskId > 0)
					{
						const FString TaskName = ActiveTask.Name.IsEmpty()
							? FString::Printf(TEXT("任务 %d"), ActiveTask.TaskId)
							: ActiveTask.Name;
						TitleText = TaskProjection.bProjected
							? FText::Format(NSLOCTEXT("RokHud", "TaskLocalTitle", "{0}·本地"), FText::FromString(TaskName))
							: FText::FromString(TaskName);
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
							DescriptionText = ProgressText;
						}
						else
						{
							DescriptionText = FText::Format(
								NSLOCTEXT("RokHud", "TaskRuntimeBoundary", "任务 #{0} · 进度待服务器"),
								FText::AsNumber(ActiveTask.TaskId));
						}
					}
					else if (Index > 0)
					{
						// Side rows need server task state (Unity TaskProxy
						// GetTaskSideList); without it stay on the boundary text
						// instead of presenting config rows as account tasks.
						TitleText = NSLOCTEXT("RokHud", "SideTaskPending", "支线任务");
						DescriptionText = NSLOCTEXT("RokHud", "SideTaskServerBoundary", "进度等待服务器状态");
					}
					else
					{
						TitleText = NSLOCTEXT("RokHud", "MainTaskPending", "主线任务");
						DescriptionText = NSLOCTEXT("RokHud", "TaskStateServerBoundary", "进度等待服务器状态");
					}
					SetFittedTaskText(*TaskTitleTexts[Index], TitleText, TaskTitlePixels, TaskTitleSize);
					SetFittedTaskText(*TaskProgressTexts[Index], DescriptionText, TaskDescriptionPixels, TaskDescriptionSize);
				}
			}
		}
	}

	if (bServerTopBar)
	{
		ApplyServerTopBar(ServerTopBar);
	}
	else
	{
		if (VipText)
		{
			VipText->SetText(NSLOCTEXT("RokHud", "VipUnavailable", "VIP --"));
		}
		if (CoordinateText)
		{
			// Offline: the Unity preview account's city position (Role_RoleInfo.pos
			// 687939, 94263 when -RokNet is logged in).
			CoordinateText->SetText(NSLOCTEXT("RokHud", "PositionPreview", "#1    X:1146 Y:157"));
		}
		if (AgeText)
		{
			AgeText->SetText(NSLOCTEXT("RokHud", "AgePreview", "远古时代"));
		}
	}
	if (RuntimeInfoText)
	{
		RuntimeInfoText->SetText(FText::FromString(
			FDateTime::Now().ToString(TEXT("AST %m/%d %H:%M  lod:-- lv:0 ping:--"))));
	}
}

TSubclassOf<URokMainHudWidget> URokMainHudWidget::ResolveHudClass(bool bTactical)
{
	static const TSoftClassPtr<URokMainHudWidget> CityClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokCityHud.WBP_RokCityHud_C")));
	static const TSoftClassPtr<URokMainHudWidget> TacticalClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokTacticalHud.WBP_RokTacticalHud_C")));
	UClass* HudClass = URokUiSettings::Resolve(bTactical ? TacticalClass : CityClass);
	return HudClass ? HudClass : StaticClass();
}

TSharedRef<SWidget> URokMainHudWidget::RebuildWidget()
{
	if (!IsDesignTime())
	{
		BuildWidgetTree();
	}
	UE_LOG(LogTemp, Log, TEXT("Rok UI: Unity MainInterface HUD widget tree rebuilt."));
	return Super::RebuildWidget();
}

void URokMainHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bWorldMapTacticalOverlay && GetWorld())
	{
		const int32 BuildType = FCString::Atoi(GetWorld()->URL.GetOption(TEXT("RokTaskBuildType="), TEXT("0")));
		const int32 UpgradeType = FCString::Atoi(GetWorld()->URL.GetOption(TEXT("RokTaskUpgradeType="), TEXT("0")));
		bPendingTaskBuildCity = BuildType > 0;
		bPendingTaskUpgradeBuilding = UpgradeType > 0;
		PendingTaskBuildingType = bPendingTaskBuildCity ? BuildType : bPendingTaskUpgradeBuilding ? UpgradeType
			: FCString::Atoi(GetWorld()->URL.GetOption(TEXT("RokTaskBuildingType="), TEXT("0")));
	}
	if (PlacementCancelButton)
	{
		PlacementCancelButton->OnClicked.AddUniqueDynamic(this, &URokMainHudWidget::HandlePlacementCancelClicked);
	}
	if (PlacementConfirmButton)
	{
		PlacementConfirmButton->OnClicked.AddUniqueDynamic(this, &URokMainHudWidget::HandlePlacementConfirmClicked);
	}
	SetPlacementPresentation(bPlacementControlsVisible, bPlacementGridValid, bPlacementCanCommit,
		PlacementReason, bPlacementCanOpenWorkerQueue);
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

void URokMainHudWidget::SetPlacementPresentation(bool bVisible, bool bGridValid, bool bCanCommit,
	const FText& Reason, bool bCanOpenWorkerQueue)
{
	const bool bWasVisible = bPlacementControlsVisible;
	bPlacementControlsVisible = bVisible;
	bPlacementGridValid = bGridValid;
	bPlacementCanCommit = bCanCommit;
	bPlacementCanOpenWorkerQueue = bCanOpenWorkerQueue;
	const bool bConfirmEnabled = bCanCommit || bCanOpenWorkerQueue;
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
		PlacementConfirmButton->SetIsEnabled(bVisible ? bConfirmEnabled
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
		UObject* DesiredResource = !bConfirmEnabled && PlacementConfirmGrayMaterial
			? static_cast<UObject*>(PlacementConfirmGrayMaterial.Get())
			: static_cast<UObject*>(PlacementMenuAtlas.Get());
		if (PlacementConfirmIcon->GetBrush().GetResourceObject() != DesiredResource)
		{
			FSlateBrush Brush = PlacementConfirmIcon->GetBrush();
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
	if (bPlacementControlsVisible && (bPlacementCanCommit || bPlacementCanOpenWorkerQueue))
	{
		OnPlacementConfirmRequested.Broadcast();
	}
}

void URokMainHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (PendingTaskBuildingType > 0 && FeatureScreenWidget && GetWorld())
	{
		const auto* City = GetWorld()->GetSubsystem<URokServerCitySubsystem>();
		if (City && City->HasAppliedServerCity())
		{
			const int32 Type = PendingTaskBuildingType;
			PendingTaskBuildingType = 0;
			FeatureScreenWidget->OpenTaskCityTarget(Type, bPendingTaskBuildCity, bPendingTaskUpgradeBuilding);
		}
	}
	const bool bHideCityHud = FeatureScreenWidget && FeatureScreenWidget->IsExpeditionStaging();
	const ESlateVisibility HudVisibility = bHideCityHud ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible;
	if (ExpandedDesignCanvas && ExpandedDesignCanvas->GetVisibility() != HudVisibility)
		ExpandedDesignCanvas->SetVisibility(HudVisibility);
	if (CityTopBarLayer)
	{
		const bool bHospitalOpen = FeatureScreenWidget && FeatureScreenWidget->IsHospitalOpen();
		const ESlateVisibility TopBarVisibility = bHospitalOpen ? ESlateVisibility::HitTestInvisible : HudVisibility;
		if (CityTopBarLayer->GetVisibility() != TopBarVisibility)
			CityTopBarLayer->SetVisibility(TopBarVisibility);
		if (auto* LayerSlot = Cast<UCanvasPanelSlot>(CityTopBarLayer->Slot))
		{
			const int32 ZOrder = bHospitalOpen ? HospitalFeatureLayer + 1 : 1;
			if (LayerSlot->GetZOrder() != ZOrder) LayerSlot->SetZOrder(ZOrder);
		}
	}
	if (ExpeditionUnlockTip && ExpeditionUnlockTipSeconds > 0.0f)
	{
		ExpeditionUnlockTipSeconds -= InDeltaTime;
		if (ExpeditionUnlockTipSeconds <= 0.0f)
			ExpeditionUnlockTip->SetVisibility(ESlateVisibility::Collapsed);
	}
	AdvanceHudAnimations(InDeltaTime);
	if (!bWorldMapTacticalOverlay)
	{
		const float CityDxf = GetCityCameraDxf();
		if (CityDxf >= 0.0f)
		{
			const bool bInsideBand = IsCityHudInsideBand(CityDxf);
			if (bInsideBand != bCityHudInsideBand)
			{
				ApplyCityHudBand(bInsideBand);
			}
			const bool bButtonsOutside = IsCityButtonOutsideBand(CityDxf, IsCityInViewport());
			if (bButtonsOutside != bCityHudButtonsOutside)
			{
				ApplyCityHudButtons(bButtonsOutside);
			}
		}
	}
	CurrentRootSize = MyGeometry.GetLocalSize();
	UpdateExpandedDesignResolution(CurrentRootSize);
	UpdateCityMarkerProjection();
	UpdateConstructionBars();
	UpdateServerTrainingPops();
	UpdateCityHarvestPops(InDeltaTime);
	UpdateBarracksIdleMarker(InDeltaTime);
	UpdateCityStatusIndicators(InDeltaTime);
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

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	const bool bDesignedLayout = RootCanvas != nullptr;
	if (bDesignedLayout)
	{
		RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		BindHudLayout();
		if (!ExpandedDesignCanvas)
		{
			return;
		}
	}
	else
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RokMainHudRoot"));
		RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetTree->RootWidget = RootCanvas;
		BuildHudLayout(RootCanvas);
	}

	CacheResourceGroupLayout();
	BuildTaskGroup();
	if (!bDesignedLayout && !bWorldMapTacticalOverlay)
	{
		BuildChatAndMapGroup();
	}
	UCanvasPanel* DesignCanvas = ExpandedDesignCanvas;
	if (bWorldMapTacticalOverlay)
	{
		ExpandedDesignCanvas = WorldMapNonResourceModules;
	}
	BuildFeatureGroup();
	ExpandedDesignCanvas = DesignCanvas;
#if WITH_EDITOR
	if (URokUiSettings::IsLegacyForced())
	{
		FString Filename;
		if (FParse::Value(FCommandLine::Get(), TEXT("RokUiExportMainHud="), Filename))
		{
			FString Error;
			if (RokUiSpec::ExportChildren(RootCanvas, Filename, bWorldMapTacticalOverlay ? TEXT("tactical_hud") : TEXT("city_hud"), Error))
			{
				UE_LOG(LogTemp, Display, TEXT("Rok UI: HUD widget spec saved to %s"), *Filename);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Rok UI: HUD widget spec export failed: %s"), *Error);
			}
		}
	}
#endif

	if (bWorldMapTacticalOverlay)
	{
		AdvanceWorldMapModuleAnimation(0.0f);
		// The world HUD owns the city markers, selection input and modal layers.
		return;
	}
	BuildCityMarker();
	BuildConstructionBarPool();
	BuildServerTrainingPopPool();
	BuildCityHarvestPops();
	BuildBarracksIdleMarker();
	BuildCityStatusIndicators();

	static const TSoftClassPtr<URokUiPageWidget> SelectionStatusPage(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokCitySelectionStatus.WBP_RokCitySelectionStatus_C")));
	if (auto* Page = AddHudPage(ExpandedDesignCanvas, SelectionStatusPage, TEXT("CitySelectionStatusPage")))
	{
		StatusText = Page->Get<UTextBlock>(TEXT("SelectionStatusText"));
		if (!StatusText) Page->RemoveFromParent();
	}
	if (!StatusText)
	{
		StatusText = MakeHudText(TEXT("SelectionStatusText"), 14, FLinearColor(0.95f, 0.9f, 0.75f, 1.0f), true);
		StatusText->SetJustification(ETextJustify::Center);
		PlaceOnCanvas(ExpandedDesignCanvas, StatusText, {0.0f, -22.0f}, {360.0f, 26.0f},
			FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f));
	}

	BuildingPanelWidget = WidgetTree->ConstructWidget<URokBuildingPanelWidget>(URokBuildingPanelWidget::ResolvePanelClass(), TEXT("BuildingPanelWidget"));
	BuildingPanelWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BuildingPanelWidget->OnInfoRequested.AddUObject(this, &URokMainHudWidget::HandleBuildingInfoRequested);
	BuildingPanelWidget->OnUpgradeRequested.AddUObject(this, &URokMainHudWidget::HandleBuildingUpgradeRequested);
	BuildingPanelWidget->OnFeatureRequested.AddUObject(this, &URokMainHudWidget::HandleBuildingFeatureRequested);
	UCanvasPanelSlot* BuildingPanelSlot = RootCanvas->AddChildToCanvas(BuildingPanelWidget);
	BuildingPanelSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BuildingPanelSlot->SetOffsets(FMargin(0.0f));
	BuildingPanelSlot->SetZOrder(20);

	ModalWidget = WidgetTree->ConstructWidget<URokUnityModalWidget>(URokUnityModalWidget::ResolveShellClass(), TEXT("UnityModalWidget"));
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
	FeatureScreenSlot->SetZOrder(HospitalFeatureLayer);
	ModalWidget->SetFeatureScreenWidget(FeatureScreenWidget);
	FeatureScreenWidget->OnExpeditionStoreRequested.BindUObject(ModalWidget, &URokUnityModalWidget::OpenExpeditionStore);

	ScoutScreenWidget = WidgetTree->ConstructWidget<URokScoutScreenWidget>(URokScoutScreenWidget::StaticClass(), TEXT("ScoutScreenWidget"));
	ScoutScreenWidget->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* ScoutSlot = RootCanvas->AddChildToCanvas(ScoutScreenWidget);
	ScoutSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	ScoutSlot->SetOffsets(FMargin(0));
	ScoutSlot->SetZOrder(120);

	BuildPlacementControls(RootCanvas);
#if WITH_EDITOR
	if (URokUiSettings::IsLegacyForced())
	{
		FString Filename;
		if (FParse::Value(FCommandLine::Get(), TEXT("RokUiExportCityProjected="), Filename))
		{
			FString Error;
			if (!RokUiSpec::ExportChildren(RootCanvas, Filename, TEXT("city_projected_hud"), Error))
			{
				UE_LOG(LogTemp, Error, TEXT("Rok UI: projected city HUD spec export failed: %s"), *Error);
			}
		}
	}
#endif
}

void URokMainHudWidget::BuildPlacementControls(UCanvasPanel* RootCanvas)
{
	// UI_Pop_BuildingMenu follows the temporary building's `button` transform.
	// Only its two UI_Model_CommandBtn images receive input; the canvas passes
	// world drags through. The source title is hidden for menu IDs [2,1].
	PlacementControls = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("CityPlacementControls"));
	PlacementControls->SetVisibility(ESlateVisibility::Collapsed);
	PlacementControls->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	PlacementControlsSlot = RootCanvas->AddChildToCanvas(PlacementControls);
	PlacementControlsSlot->SetLayout(MakeDefaultPlacementControlsBounds());
	PlacementControlsSlot->SetZOrder(130);

	ConfigurePlacementMaterial();
	static const TSoftClassPtr<URokUiPageWidget> PlacementPage(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokCityPlacementControls.WBP_RokCityPlacementControls_C")));
	if (auto* Page = AddHudPage(PlacementControls, PlacementPage, TEXT("CityPlacementPage")))
	{
		const UWidget* Bounds = Page->GetWidgetFromName(TEXT("CityPlacementDefaultBounds"));
		auto* BoundsSlot = Bounds ? Cast<UCanvasPanelSlot>(Bounds->Slot) : nullptr;
		if (BoundsSlot && BoundsSlot->GetAnchors().Minimum == BoundsSlot->GetAnchors().Maximum
			&& BoundsSlot->GetSize().X > 0 && BoundsSlot->GetSize().Y > 0)
		{
			PlacementControlsDesignSlot = BoundsSlot;
			PlacementControlsSlot->SetLayout(BoundsSlot->GetLayout());
		}
		PlacementButtonShadow = Cast<UImage>(Page->GetWidgetFromName(TEXT("CityPlacementButtonShadow")));
		PlacementStatusText = Page->Get<UTextBlock>(TEXT("CityPlacementStatus"));
		PlacementReasonText = Page->Get<UTextBlock>(TEXT("CityPlacementReason"));
		PlacementConfirmIcon = Page->Get<UImage>(TEXT("CityPlacementConfirmImage"));
		PlacementCancelIcon = Page->Get<UImage>(TEXT("CityPlacementCancelImage"));
		PlacementConfirmButton = Page->Get<UButton>(TEXT("CityPlacementConfirmButton"));
		PlacementCancelButton = Page->Get<UButton>(TEXT("CityPlacementCancelButton"));
		return;
	}

	PlacementButtonShadow = nullptr;
	if (UTexture2D* ShadowTexture = LoadUiTexture(PlacementShadowPath))
	{
		// UI_Item_CMDBtns/bgbig uses pl_hud_1000 at 374x156. Its top is
		// 96 units above the button centers (75 + 50 - 29 in the prefab).
		const FVector2D ShadowSize(374.0f, 156.0f);
		const FVector2D ShadowPosition((472.7f - ShadowSize.X) * 0.5f, PlacementCancelPosition.Y + 43.5f - 96.0f);
		PlacementButtonShadow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CityPlacementButtonShadow"));
		PlacementButtonShadow->SetBrushFromTexture(ShadowTexture);
		PlacementButtonShadow->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(PlacementControls, PlacementButtonShadow, ShadowPosition, ShadowSize);
	}

	// Retain the existing read-only feedback nodes for the no-preview failure
	// route. They are hidden for the source two-icon temporary building menu.
	PlacementStatusText = MakeHudText(TEXT("CityPlacementStatus"), 17, FLinearColor::White, true);
	PlacementStatusText->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(PlacementControls, PlacementStatusText, FVector2D(36.0f, 12.0f), FVector2D(400.0f, 26.0f));
	PlacementReasonText = MakeHudText(TEXT("CityPlacementReason"), 13, FLinearColor(0.92f, 0.88f, 0.75f, 1.0f));
	PlacementReasonText->SetAutoWrapText(true);
	PlacementReasonText->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(PlacementControls, PlacementReasonText, FVector2D(36.0f, 42.0f), FVector2D(400.0f, 56.0f));

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
	PlaceOnCanvas(PlacementControls, PlacementConfirmIcon, PlacementConfirmPosition, FVector2D(87.0f, 87.0f));
	UTextBlock* ConfirmLabel = MakeHudText(TEXT("CityPlacementConfirmLabel"), 17, FLinearColor::White, true);
	ConfirmLabel->SetText(NSLOCTEXT("RokHud", "PlacementConfirm", "建造"));
	ConfirmLabel->SetJustification(ETextJustify::Center);
	ConfirmLabel->SetVisibility(PlacementMenuAtlas ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(PlacementControls, ConfirmLabel, PlacementConfirmPosition, FVector2D(87.0f, 87.0f));
	PlacementConfirmButton = MakeTransparentHitButton(TEXT("CityPlacementConfirmButton"));
	PlaceOnCanvas(PlacementControls, PlacementConfirmButton, PlacementConfirmPosition, FVector2D(87.0f, 87.0f));

	PlacementCancelIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CityPlacementCancelImage"));
	PlacementCancelIcon->SetVisibility(PlacementMenuAtlas ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (PlacementMenuAtlas)
	{
		PlacementCancelIcon->SetBrush(MakeAtlasBrush(PlacementMenuAtlas, PlacementCancelRect,
			PlacementAtlasSize, FVector2D(87.0f, 87.0f)));
	}
	PlaceOnCanvas(PlacementControls, PlacementCancelIcon, PlacementCancelPosition, FVector2D(87.0f, 87.0f));
	UTextBlock* CancelLabel = MakeHudText(TEXT("CityPlacementCancelLabel"), 17, FLinearColor::White, true);
	CancelLabel->SetText(NSLOCTEXT("RokHud", "PlacementCancel", "取消摆放"));
	CancelLabel->SetJustification(ETextJustify::Center);
	CancelLabel->SetVisibility(PlacementMenuAtlas ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(PlacementControls, CancelLabel, PlacementCancelPosition, FVector2D(87.0f, 87.0f));
	PlacementCancelButton = MakeTransparentHitButton(TEXT("CityPlacementCancelButton"));
	PlacementCancelButton->SetToolTipText(NSLOCTEXT("RokHud", "PlacementCancelTip", "取消本次临时摆放"));
	PlaceOnCanvas(PlacementControls, PlacementCancelButton, PlacementCancelPosition, FVector2D(87.0f, 87.0f));
}

void URokMainHudWidget::ConfigurePlacementMaterial()
{
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
}

void URokMainHudWidget::BuildHudLayout(UCanvasPanel* RootCanvas)
{
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
		BuildCityTopBar(RootCanvas);
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
		BuildResourceGroup(WorldMapRightModules);
	}
	if (WorldMapRightModules)
	{
		WorldMapNonResourceModules = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("WorldMapNonResourceModules"));
		WorldMapNonResourceModules->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		auto* ModuleSlot = WorldMapRightModules->AddChildToCanvas(WorldMapNonResourceModules);
		ModuleSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		ModuleSlot->SetOffsets(FMargin(0));
		ExpandedDesignCanvas = WorldMapNonResourceModules;
	}
	BuildAgeGroup();
	ExpandedDesignCanvas = DesignCanvas;
}

void URokMainHudWidget::BuildCityTopBar(UCanvasPanel* RootCanvas)
{
	// Unity keeps the city identity and resource HUD above the treatment window's mask.
	// Keep the existing controls in one layer so live values and sizing have one owner.
	CityTopBarLayer = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("CityTopBarLayer"));
	CityTopBarLayer->SetStretch(EStretch::ScaleToFit);
	CityTopBarLayer->SetStretchDirection(EStretchDirection::Both);
	CityTopBarLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	auto* LayerSlot = RootCanvas->AddChildToCanvas(CityTopBarLayer);
	LayerSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	LayerSlot->SetOffsets(FMargin(0));
	LayerSlot->SetZOrder(1);

	CityTopBarSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CityTopBarDesignSize"));
	CityTopBarSizeBox->SetWidthOverride(CurrentExpandedDesignSize.X);
	CityTopBarSizeBox->SetHeightOverride(CurrentExpandedDesignSize.Y);
	CityTopBarSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	CityTopBarLayer->SetContent(CityTopBarSizeBox);
	auto* TopBarCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CityTopBarCanvas"));
	TopBarCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	CityTopBarSizeBox->SetContent(TopBarCanvas);
	BuildPlayerPowerGroup(TopBarCanvas);
	BuildResourceGroup(TopBarCanvas);
}

void URokMainHudWidget::BuildPlayerPowerGroup(UCanvasPanel* Parent)
{
	UCanvasPanel* Group = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerPowerGroup"));
	Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(Parent, Group, FVector2D(0.0f, 0.0f), FVector2D(302.0f, 125.0f));

	UImage* PlayerBackground = MakeAtlasImage(TEXT("PlayerPowerBackground"), MainMenuAtlasPath, PlayerBackgroundRect, MainMenuAtlasSize, FVector2D(302.0f, 125.0f));
	PlayerBackground->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	PlayerBackground->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(Group, PlayerBackground, FVector2D::ZeroVector, FVector2D(302.0f, 125.0f));
	PlayerHeadImage = MakeAtlasImage(TEXT("PlayerHead1000"), PlayerHeadAtlasPath, PlayerHead1000Rect, PlayerHeadAtlasSize, FVector2D(92.0f, 92.0f));
	PlaceOnCanvas(Group, PlayerHeadImage, FVector2D(10.0f, 10.0f), FVector2D(92.0f, 92.0f));
	PlayerHeadFrameImage = MakeAtlasImage(TEXT("PlayerHeadFrame"), PlayerHeadAtlasPath, PlayerFrameRect, PlayerHeadAtlasSize, FVector2D(104.0f, 104.0f));
	PlaceOnCanvas(Group, PlayerHeadFrameImage, FVector2D(4.0f, 4.0f), FVector2D(104.0f, 104.0f));
	PlaceOnCanvas(Group, MakeAtlasImage(TEXT("PowerIcon"), MainMenuAtlasPath, PowerIconRect, MainMenuAtlasSize, FVector2D(28.0f, 25.0f)), FVector2D(111.0f, 10.0f), FVector2D(28.0f, 25.0f));

	PowerText = MakeHudText(TEXT("PlayerPowerText"), 25, FLinearColor(0.9f, 0.96f, 1.0f, 1.0f), true);
	PowerText->SetText(NSLOCTEXT("RokHud", "InitialPowerUnavailable", "--"));
	PlaceOnCanvas(Group, PowerText, FVector2D(145.0f, 5.0f), FVector2D(135.0f, 36.0f));
	AddPowerGain(Group);

	PlaceOnCanvas(Group, MakeAtlasImage(TEXT("VipIcon"), VipAtlasPath, VipIcon100Rect, VipAtlasSize, FVector2D(34.0f, 34.0f)), FVector2D(112.0f, 49.0f), FVector2D(34.0f, 34.0f));
	VipText = MakeHudText(TEXT("VipText"), 20, FLinearColor(1.0f, 0.88f, 0.4f, 1.0f), true);
	PlaceOnCanvas(Group, VipText, FVector2D(151.0f, 51.0f), FVector2D(85.0f, 30.0f));
	// Pl_Vip/img_redpoint (18x18), measured on the Unity capture at centre (219.5, 61).
	// Server-driven only, so it stays hidden offline.
	VipRedPointImage = MakeAtlasImage(TEXT("VipRedPoint"), CommonAtlasPath, RedPointRect, CommonAtlasSize, FVector2D(18.0f, 18.0f));
	VipRedPointImage->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(Group, VipRedPointImage, FVector2D(210.5f, 52.0f), FVector2D(18.0f, 18.0f))->SetZOrder(5);
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

void URokMainHudWidget::AddPowerGain(UCanvasPanel* Group)
{
	static const TSoftClassPtr<URokUiPageWidget> Class(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokHudPowerGain.WBP_RokHudPowerGain_C")));
	if (auto* Page = AddHudPage(Group, Class, TEXT("HudPowerGainPage"), 51))
	{
		PowerGain = Page->Get<URokPowerGainWidget>(TEXT("PlayerPowerGain"));
		if (PowerGain)
		{
			return;
		}
		Page->RemoveFromParent();
	}
	PowerGain = WidgetTree->ConstructWidget<URokPowerGainWidget>(URokPowerGainWidget::StaticClass(), TEXT("PlayerPowerGain"));
	// UI_Item_PlayerPowerInfo/Pl_Power/btn_powerShow, mirrored for the LTR HUD.
	// Pivot X = 98.19989 + 193.42/2 + 4.75; Y = 28.75 + 2.72.
	PlaceOnCanvas(Group, PowerGain, FVector2D(199.65989f - 320, 31.47f - 180), FVector2D(640, 360))->SetZOrder(51);
}

void URokMainHudWidget::BindHudLayout()
{
	ExpandedDesignCanvas = WidgetTree->FindWidget<UCanvasPanel>(TEXT("UnityExpandedDesignCanvas"));
	ExpandedDesignSizeBox = WidgetTree->FindWidget<USizeBox>(TEXT("UnityExpandedDesignSize"));
	WorldMapRightModules = WidgetTree->FindWidget<UCanvasPanel>(TEXT("WorldMapRightModules"));
	WorldMapNonResourceModules = WidgetTree->FindWidget<UCanvasPanel>(TEXT("WorldMapNonResourceModules"));
	if (!ExpandedDesignCanvas || !ExpandedDesignSizeBox
		|| (bWorldMapTacticalOverlay && (!WorldMapRightModules || !WorldMapNonResourceModules)))
	{
		UE_LOG(LogTemp, Error, TEXT("Rok UI: required HUD layout containers are missing from %s"), *GetClass()->GetName());
		ExpandedDesignCanvas = nullptr;
		return;
	}

	FoodText = WidgetTree->FindWidget<UTextBlock>(TEXT("FoodResourceValue"));
	WoodText = WidgetTree->FindWidget<UTextBlock>(TEXT("WoodResourceValue"));
	GemText = WidgetTree->FindWidget<UTextBlock>(TEXT("GemResourceValue"));
	AgeText = WidgetTree->FindWidget<UTextBlock>(TEXT("AgeText"));
	RuntimeInfoText = WidgetTree->FindWidget<UTextBlock>(TEXT("RuntimeInfoText"));
	BindHudButton(*this, TEXT("FoodResourceHitButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenFoodResources));
	BindHudButton(*this, TEXT("WoodResourceHitButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenWoodResources));
	BindHudButton(*this, TEXT("GemResourceHitButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenCharge));

	if (bWorldMapTacticalOverlay)
	{
		return;
	}

	CityTopBarLayer = WidgetTree->FindWidget<UScaleBox>(TEXT("CityTopBarLayer"));
	CityTopBarSizeBox = WidgetTree->FindWidget<USizeBox>(TEXT("CityTopBarDesignSize"));
	PowerText = WidgetTree->FindWidget<UTextBlock>(TEXT("PlayerPowerText"));
	VipText = WidgetTree->FindWidget<UTextBlock>(TEXT("VipText"));
	VipRedPointImage = WidgetTree->FindWidget<UImage>(TEXT("VipRedPoint"));
	PlayerHeadImage = WidgetTree->FindWidget<UImage>(TEXT("PlayerHead1000"));
	PlayerHeadFrameImage = WidgetTree->FindWidget<UImage>(TEXT("PlayerHeadFrame"));
	if (UCanvasPanel* PowerGroup = WidgetTree->FindWidget<UCanvasPanel>(TEXT("PlayerPowerGroup")))
	{
		AddPowerGain(PowerGroup);
	}

	PositionGroup = WidgetTree->FindWidget<UCanvasPanel>(TEXT("PositionGroup"));
	CoordinateText = WidgetTree->FindWidget<UTextBlock>(TEXT("CoordinateText"));
	ChatWorldMarker = WidgetTree->FindWidget<UImage>(TEXT("ChatWorldMarker"));
	ChatAllianceMarkerBase = WidgetTree->FindWidget<UImage>(TEXT("ChatAllianceMarkerBase"));
	ChatAllianceMarker = WidgetTree->FindWidget<UImage>(TEXT("ChatAllianceMarker"));
	BuildCityButton = WidgetTree->FindWidget<UButton>(TEXT("BuildCityButton"));
	WorldSearchButton = WidgetTree->FindWidget<UButton>(TEXT("WorldSearchButton"));
	CityWorldButton = WidgetTree->FindWidget<UButton>(TEXT("CityWorldButton"));
	WorldGlobeButton = WidgetTree->FindWidget<UButton>(TEXT("WorldGlobeButton"));
	BindHudButton(*this, TEXT("VipHitButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenVip));
	BindHudButton(*this, TEXT("PlayerArrowButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenCityManager));
	BindHudButton(*this, TEXT("PlayerProfileHitButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenPlayerProfile));
	BindHudButton(*this, TEXT("PositionSearchButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenWorldPositionSearch));
	BindHudButton(*this, TEXT("FavoriteButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenMapMarkers));
	BindHudButton(*this, TEXT("ChatButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenChat));
	BindHudButton(*this, TEXT("BuildCityButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenBuildCity));
	BindHudButton(*this, TEXT("WorldSearchButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenWorldPositionSearch));
	BindHudButton(*this, TEXT("CityWorldButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleFocusCityCenter));
	BindHudButton(*this, TEXT("WorldGlobeButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleReturnToWorldMap));
	ApplyChatChannelPresentation();
	ApplyCityHudBand(true);
	ApplyCityHudButtons(false);
}

void URokMainHudWidget::BuildPositionGroup()
{
	UCanvasPanel* Group = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PositionGroup"));
	Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ExpandedDesignCanvas, Group, FVector2D(298.0f, 6.0f), FVector2D(280.0f, 50.0f));
	PositionGroup = Group;

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

void URokMainHudWidget::SetWorldMapSearchVisible(bool bVisible)
{
	if (WorldMapNonResourceModules)
		WorldMapNonResourceModules->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
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

void URokMainHudWidget::BuildResourceGroup(UCanvasPanel* Parent)
{
	auto* Group = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerResourcesGroup"));
	Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(Parent, Group, FVector2D(-20, 10), FVector2D(440, 44), FAnchors(1, 0), FVector2D(1, 0));

	const FIntRect Icons[] = {FoodRect, WoodRect, StoneRect, GoldRect, GemRect};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ResourceCellNames); ++Index)
	{
		const bool bGem = Index == 4;
		const float Width = bGem ? 164.0f : 136.0f;
		UTextBlock* Text = nullptr;
		auto* Cell = MakeResourceCell(ResourceCellNames[Index], Icons[Index], Text, bGem);
		const bool bUnlockedInLocalPreview = Index < 2 || bGem;
		Cell->SetVisibility(bUnlockedInLocalPreview ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		const float DefaultX = bGem ? 272.0f : Index * 136.0f;
		PlaceOnCanvas(Group, Cell, FVector2D(DefaultX, 0), FVector2D(Width, 44));
		if (Index == 0 || Index == 1 || bGem)
		{
			auto* Hit = MakeTransparentHitButton(*(ResourceCellNames[Index].ToString() + TEXT("HitButton")));
			if (Index == 0)
			{
				FoodText = Text;
				Hit->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenFoodResources);
			}
			else if (Index == 1)
			{
				WoodText = Text;
				Hit->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenWoodResources);
			}
			else
			{
				GemText = Text;
				Hit->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenCharge);
			}
			PlaceOnCanvas(Cell, Hit, FVector2D::ZeroVector, FVector2D(Width, 44))->SetZOrder(10);
		}
	}
}

void URokMainHudWidget::CacheResourceGroupLayout()
{
	ResourceCellLayouts.Reset(UE_ARRAY_COUNT(ResourceCellNames));
	double PreviousRight = 0.0;
	double VisibleRight = 0.0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ResourceCellNames); ++Index)
	{
		const auto* Cell = GetWidgetFromName(ResourceCellNames[Index]);
		const auto* CellSlot = Cell ? Cast<UCanvasPanelSlot>(Cell->Slot) : nullptr;
		const bool bGem = Index == UE_ARRAY_COUNT(ResourceCellNames) - 1;
		const FVector2D Position = CellSlot ? CellSlot->GetPosition() : FVector2D(bGem ? 272.0 : Index * 136.0, 0.0);
		const double Width = CellSlot ? CellSlot->GetSize().X : bGem ? 164.0 : 136.0;
		if (Index == 0)
		{
			ResourceGroupLeftPadding = Position.X;
			PreviousRight = Position.X;
		}
		// The default layout places gems immediately after wood while stone and gold are hidden.
		const double Gap = Position.X - (bGem && Position.X < PreviousRight ? VisibleRight : PreviousRight);
		ResourceCellLayouts.Add({Width, Gap, Position.Y});
		PreviousRight = Position.X + Width;
		const ESlateVisibility CellVisibility = Cell ? Cell->GetVisibility() : ESlateVisibility::Collapsed;
		if (CellVisibility != ESlateVisibility::Collapsed && CellVisibility != ESlateVisibility::Hidden)
		{
			VisibleRight = FMath::Max(VisibleRight, PreviousRight);
		}
	}
	const auto* Group = GetWidgetFromName(TEXT("PlayerResourcesGroup"));
	const auto* GroupSlot = Group ? Cast<UCanvasPanelSlot>(Group->Slot) : nullptr;
	ResourceGroupRightPadding = GroupSlot ? GroupSlot->GetSize().X - VisibleRight : 4.0;
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

	// pl_age/UI_Item_CurAge: lbl_age on top, lbl_time below it (16.1 in the prefab; 20
	// matches the line gap on the Unity capture with the UE fonts).
	AgeText = MakeHudText(TEXT("AgeText"), 18, FLinearColor(0.08f, 0.07f, 0.05f, 1.0f), true);
	AgeText->SetJustification(ETextJustify::Right);
	PlaceOnCanvas(Group, AgeText, FVector2D(190.0f, 0.0f), FVector2D(190.0f, 28.0f));
	RuntimeInfoText = MakeHudText(TEXT("RuntimeInfoText"), 14, FLinearColor(0.1f, 0.09f, 0.07f, 1.0f));
	RuntimeInfoText->SetJustification(ETextJustify::Right);
	PlaceOnCanvas(Group, RuntimeInfoText, FVector2D(0.0f, 20.0f), FVector2D(380.0f, 28.0f));
}

void URokMainHudWidget::BindHudButton(const UUserWidget& Owner, FName Name, FName Function)
{
	if (UButton* Button = Cast<UButton>(Owner.GetWidgetFromName(Name)))
	{
		FScriptDelegate Handler;
		Handler.BindUFunction(this, Function);
		Button->OnClicked.AddUnique(Handler);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Rok UI: %s has no HUD button named %s"), *Owner.GetClass()->GetName(), *Name.ToString());
	}
}

URokUiPageWidget* URokMainHudWidget::CreateHudPage(const TSoftClassPtr<URokUiPageWidget>& Class, FName Name)
{
	UClass* PageClass = GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint)
		? URokUiSettings::Resolve(Class) : nullptr;
	if (!PageClass)
	{
		return nullptr;
	}
	auto* Page = WidgetTree->ConstructWidget<URokUiPageWidget>(PageClass, Name);
	Page->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UWidget* Root = Page->GetRootWidget())
	{
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	return Page;
}

URokUiPageWidget* URokMainHudWidget::AddHudPage(UCanvasPanel* Parent,
	const TSoftClassPtr<URokUiPageWidget>& Class, FName Name, int32 ZOrder)
{
	auto* Page = CreateHudPage(Class, Name);
	if (!Page)
	{
		return nullptr;
	}
	UCanvasPanelSlot* PageSlot = Parent->AddChildToCanvas(Page);
	PageSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	PageSlot->SetOffsets(FMargin(0));
	PageSlot->SetZOrder(ZOrder);
	return Page;
}

void URokMainHudWidget::BindTaskGroup(URokUiPageWidget& Page)
{
	HotTaskGroup = Page.Get<UCanvasPanel>(TEXT("HotTaskGroup"));
	TaskListBackground = Page.Get<UCanvasPanel>(TEXT("TaskListBackground"));
	TaskListClip = Page.Get<UCanvasPanel>(TEXT("TaskListClip"));
	TaskFoldButton = Page.Get<UButton>(TEXT("TaskFoldButton"));
	TaskScroll = Page.Get<UScrollBox>(TEXT("HudTaskScroll"));
	TaskListBox = Page.Get<UVerticalBox>(TEXT("TaskList"));
	static const TSoftClassPtr<URokUiPageWidget> ListLayoutClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokHudListLayout.WBP_RokHudListLayout_C")));
	if (auto* Layout = CreateHudPage(ListLayoutClass, NAME_None))
	{
		if (const auto* Row = Layout->Get<UCanvasPanel>(TEXT("TaskRow")))
		{
			if (const auto* RowLayoutSlot = Cast<UCanvasPanelSlot>(Row->Slot))
			{
				TaskRowLayoutSize = RowLayoutSlot->GetSize();
			}
		}
		if (const auto* Gap = Layout->Get<UCanvasPanel>(TEXT("TaskRowGap")))
		{
			if (const auto* GapLayoutSlot = Cast<UCanvasPanelSlot>(Gap->Slot))
			{
				TaskRowBottomPadding = GapLayoutSlot->GetSize().Y;
			}
		}
	}
	ArmyQueueGroup = Page.Get<UCanvasPanel>(TEXT("ArmyQueueEntry"));
	ArmyQueueCountText = Page.Get<UTextBlock>(TEXT("ArmyQueueEntryState"));
	if (UTextBlock* Badge = Page.Get<UTextBlock>(TEXT("TaskEntryBadgeCount")))
	{
		NotificationTexts.Add(TEXT("TaskEntry"), Badge);
	}
	BindHudButton(Page, TEXT("TaskEntryButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenTasks));
	BindHudButton(Page, TEXT("TaskFoldButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleToggleTaskPanel));
	BindHudButton(Page, TEXT("ArmyQueueButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenArmyShow));
	RebuildTaskRows(1, false);
	CacheTaskFoldLayout();
	ApplyTaskFoldPresentation();
}

void URokMainHudWidget::CacheTaskFoldLayout()
{
	if (const auto* BackgroundSlot = TaskListBackground ? Cast<UCanvasPanelSlot>(TaskListBackground->Slot) : nullptr)
	{
		TaskBackgroundOpenSize = BackgroundSlot->GetSize();
	}
	if (const auto* ClipSlot = TaskListClip ? Cast<UCanvasPanelSlot>(TaskListClip->Slot) : nullptr)
	{
		TaskClipOpenSize = ClipSlot->GetSize();
		TaskClipOpenPosition = ClipSlot->GetPosition();
	}
	if (const auto* FoldSlot = TaskFoldButton ? Cast<UCanvasPanelSlot>(TaskFoldButton->Slot) : nullptr)
	{
		TaskFoldOpenPosition = FoldSlot->GetPosition();
		TaskFoldOpenAngle = TaskFoldButton->GetRenderTransformAngle();
	}
}

void URokMainHudWidget::BuildTaskGroup()
{
	TaskFoldSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/RokPrototype/UI/Audio/Sound_Ui_CommonClickButton1.Sound_Ui_CommonClickButton1"));
	if (!TaskFoldSound) UE_LOG(LogTemp, Warning, TEXT("Missing source task-fold audio: Sound_Ui_CommonClickButton1"));
	static const TSoftClassPtr<URokUiPageWidget> TaskPage(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokHudTasks.WBP_RokHudTasks_C")));
	if (URokUiPageWidget* Page = AddHudPage(ExpandedDesignCanvas, TaskPage, TEXT("HudTasksPage")))
	{
		BindTaskGroup(*Page);
		return;
	}

	UCanvasPanel* Group = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HotTaskGroup"));
	HotTaskGroup = Group;
	Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ExpandedDesignCanvas, Group, FVector2D(0.0f, 136.0f), FVector2D(232.0f, 381.0f));

	UButton* TaskEntryButton = MakeAtlasButton(TEXT("TaskEntryButton"), MainMenuAtlasPath, TaskEntryRect, MainMenuAtlasSize, FVector2D(77.0f, 76.0f));
	TaskEntryButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenTasks);
	PlaceOnCanvas(Group, TaskEntryButton, FVector2D(10.0f, 0.0f), FVector2D(77.0f, 76.0f))->SetZOrder(5);
	PlaceOnCanvas(Group, MakeNotificationBadge(TEXT("TaskEntry")), FVector2D(65, -3), FVector2D(27, 29))->SetZOrder(6);
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

	// Unity's sv_list_tasks is a ScrollRect with source-ordered, variable rows.
	TaskScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("HudTaskScroll"));
	TaskScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	TaskScroll->SetAllowOverscroll(false);
	PlaceOnCanvas(TaskListClip, TaskScroll, FVector2D::ZeroVector, FVector2D(210.0f, 280.0f));
	TaskListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TaskList"));
	TaskListBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TaskScroll->AddChild(TaskListBox);
	RebuildTaskRows(1, false);
	CacheTaskFoldLayout();
	ApplyTaskFoldPresentation();

	UCanvasPanel* ArmyQueueEntry = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("ArmyQueueEntry"));
	ArmyQueueGroup = ArmyQueueEntry;
	// MainInterfaceMediator.RefreshTroopsDispatchNum shows this group only when
	// active troop/scout/transport rows exist. Stay collapsed until the
	// authoritative snapshot is available (Unity also hides an empty list).
	ArmyQueueEntry->SetVisibility(ESlateVisibility::Collapsed);
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
	ArmyQueueCountText = ArmyQueueState;
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
	BuildCityButton = MakeAtlasButton(
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

	// Unity UI_IF_MainInterface pl_BuildAndSearch swaps btn_build for the
	// btn_search magnifier at city_bound (MainInterfaceMediator SetCityButton).
	WorldSearchButton = MakeAtlasButton(
		TEXT("WorldSearchButton"),
		MainMenuAtlasPath,
		SearchRect,
		MainMenuAtlasSize,
		FVector2D(92.0f, 92.0f));
	WorldSearchButton->OnClicked.AddDynamic(
		this,
		&URokMainHudWidget::HandleOpenWorldPositionSearch);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		WorldSearchButton,
		FVector2D(18.0f, -158.0f),
		FVector2D(92.0f, 92.0f),
		FAnchors(0.0f, 1.0f),
		FVector2D(0.0f, 1.0f));

	// pl_MapAndCity swaps btn_map (globe, returns to the world map) for btn_city
	// (castle, returns to the city's default framing) on the same band change.
	CityWorldButton = MakeAtlasButton(
		TEXT("CityWorldButton"),
		MainMenuAtlasPath,
		CityRect,
		MainMenuAtlasSize,
		FVector2D(127.0f, 148.0f));
	CityWorldButton->OnClicked.AddDynamic(
		this,
		&URokMainHudWidget::HandleFocusCityCenter);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		CityWorldButton,
		FVector2D::ZeroVector,
		FVector2D(127.0f, 148.0f),
		FAnchors(0.0f, 1.0f),
		FVector2D(0.0f, 1.0f));
	WorldGlobeButton = MakeAtlasButton(
		TEXT("WorldGlobeButton"),
		MainMenuAtlasPath,
		GlobeRect,
		MainMenuAtlasSize,
		FVector2D(127.0f, 148.0f));
	WorldGlobeButton->OnClicked.AddDynamic(
		this,
		&URokMainHudWidget::HandleReturnToWorldMap);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		WorldGlobeButton,
		FVector2D::ZeroVector,
		FVector2D(127.0f, 148.0f),
		FAnchors(0.0f, 1.0f),
		FVector2D(0.0f, 1.0f));

	// The city opens at city_default (dxf 150); NativeTick re-applies the band
	// and the button pair from the live camera once the pawn exists.
	ApplyCityHudBand(true);
	ApplyCityHudButtons(false);
}

void URokMainHudWidget::BindFeatureGroup(URokUiPageWidget& Page)
{
	FeatureBackground = Page.Get<UCanvasPanel>(TEXT("FeatureBackground"));
	MoreFeatureButton = Page.Get<UButton>(TEXT("MoreFeatureButton"));
	MoreFeatureOverlay = Page.Get<UImage>(TEXT("MoreFeatureSourceHideImage"));
	ChargeEventsButton = Page.Get<UButton>(TEXT("ChargeEventsButton"));

	CollapsibleFeatureWidgets.Reset();
	CollapsibleFeatureWidgets.Add(FeatureBackground);
	for (const FName Name : {TEXT("CampaignFeature"), TEXT("AllianceFeature"), TEXT("ItemsFeature"), TEXT("CommanderFeature")})
	{
		CollapsibleFeatureWidgets.Add(Page.Get<UCanvasPanel>(Name));
	}
	for (const FName Name : {TEXT("CampaignFeature"), TEXT("CommanderFeature"), TEXT("MailFeature"), TEXT("Activity"), TEXT("ChargeEvents")})
	{
		const FName CountName(*(Name.ToString() + TEXT("BadgeCount")));
		if (UTextBlock* Count = Page.Get<UTextBlock>(CountName))
		{
			NotificationTexts.Add(Name, Count);
		}
	}

	BindHudButton(Page, TEXT("CampaignFeatureButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenExpedition));
	BindHudButton(Page, TEXT("AllianceFeatureButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenAlliance));
	BindHudButton(Page, TEXT("ItemsFeatureButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenItems));
	BindHudButton(Page, TEXT("CommanderFeatureButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenCommander));
	BindHudButton(Page, TEXT("MoreFeatureButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleToggleFeatureBar));
	BindHudButton(Page, TEXT("MailFeatureHitButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenMail));
	BindHudButton(Page, TEXT("ActivityButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenActivityCenter));
	BindHudButton(Page, TEXT("ChargeEventsButton"), GET_FUNCTION_NAME_CHECKED(URokMainHudWidget, HandleOpenChargeEvents));
	CacheFeatureFoldLayout();
	ApplyFeatureBarVisibility();
}

void URokMainHudWidget::CacheFeatureFoldLayout()
{
	if (const auto* BackgroundSlot = FeatureBackground ? Cast<UCanvasPanelSlot>(FeatureBackground->Slot) : nullptr)
	{
		FeatureBackgroundOpenSize = BackgroundSlot->GetSize();
		const UWidget* Left = FeatureBackground->GetChildAt(0);
		const UWidget* Right = FeatureBackground->GetChildAt(2);
		const auto* LeftSlot = Left ? Cast<UCanvasPanelSlot>(Left->Slot) : nullptr;
		const auto* RightSlot = Right ? Cast<UCanvasPanelSlot>(Right->Slot) : nullptr;
		if (LeftSlot && RightSlot)
		{
			FeatureBackgroundBorders = FVector2D(LeftSlot->GetOffsets().Right, RightSlot->GetOffsets().Right);
		}
	}
	MoreFeatureOpenAngle = MoreFeatureButton ? MoreFeatureButton->GetRenderTransformAngle() : 0.0f;
	CollapsibleFeatureOpenTranslations.Reset();
	for (const UWidget* Widget : CollapsibleFeatureWidgets)
	{
		CollapsibleFeatureOpenTranslations.Add(Widget ? Widget->GetRenderTransform().Translation : FVector2D::ZeroVector);
	}
}

void URokMainHudWidget::BuildExpeditionUnlockTip()
{
	// Keep the tip above projected city widgets in the same parent canvas.
	static const TSoftClassPtr<URokUiPageWidget> TipPage(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokHudExpeditionTip.WBP_RokHudExpeditionTip_C")));
	if (URokUiPageWidget* Page = AddHudPage(ExpandedDesignCanvas, TipPage, TEXT("HudExpeditionTipPage"), 50))
	{
		ExpeditionUnlockTip = Page->Get<UTextBlock>(TEXT("ExpeditionUnlockTip"));
		return;
	}

	ExpeditionUnlockTip = MakeHudText(TEXT("ExpeditionUnlockTip"), 21, FLinearColor::White, true);
	ExpeditionUnlockTip->SetJustification(ETextJustify::Center);
	ExpeditionUnlockTip->SetShadowOffset(FVector2D(1.5));
	ExpeditionUnlockTip->SetShadowColorAndOpacity(FLinearColor::Black);
	ExpeditionUnlockTip->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(ExpandedDesignCanvas, ExpeditionUnlockTip, FVector2D::ZeroVector, FVector2D(620, 45),
		FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f))->SetZOrder(50);
}

void URokMainHudWidget::BuildFeatureGroup()
{
	BuildExpeditionUnlockTip();
	FeatureFoldSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/RokPrototype/UI/Audio/Sound_Ui_PickUp.Sound_Ui_PickUp"));
	if (!FeatureFoldSound) UE_LOG(LogTemp, Warning, TEXT("Missing source feature-fold audio: Sound_Ui_PickUp"));
	static const TSoftClassPtr<URokUiPageWidget> FeaturePage(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokHudFeatures.WBP_RokHudFeatures_C")));
	if (URokUiPageWidget* Page = AddHudPage(ExpandedDesignCanvas, FeaturePage, TEXT("HudFeaturesPage")))
	{
		BindFeatureGroup(*Page);
		return;
	}

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
		true,
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
	CacheFeatureFoldLayout();
	ApplyFeatureBarVisibility();

	UCanvasPanel* MailButton = MakeFeatureButton(TEXT("MailFeature"), FText::GetEmpty(), MailRect, true);
	PlaceOnCanvas(ExpandedDesignCanvas, MailButton, FVector2D(-16.0f, -116.0f), FVector2D(100.0f, 100.0f), FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f));
	UButton* MailHitButton = MakeTransparentHitButton(TEXT("MailFeatureHitButton"));
	MailHitButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenMail);
	PlaceOnCanvas(ExpandedDesignCanvas, MailHitButton, FVector2D(-16.0f, -116.0f), FVector2D(100.0f, 100.0f), FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f));
	UButton* ActivityButton = MakeAtlasButton(TEXT("ActivityButton"), MainMenuAtlasPath, ActivityRect, MainMenuAtlasSize, FVector2D(80.0f, 80.0f));
	ActivityButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenActivityCenter);
	PlaceOnCanvas(ExpandedDesignCanvas, ActivityButton, FVector2D(-92, 112), FVector2D(80),
		FAnchors(1, 0), FVector2D(1, 0));
	PlaceOnCanvas(ExpandedDesignCanvas, MakeNotificationBadge(TEXT("Activity")), FVector2D(-101, 104),
		FVector2D(27, 29), FAnchors(1, 0), FVector2D(1, 0));
	ChargeEventsButton = MakeAtlasButton(TEXT("ChargeEventsButton"), MainMenuAtlasPath,
		FirstChargeRect, MainMenuAtlasSize, FVector2D(80));
	ChargeEventsButton->OnClicked.AddDynamic(this, &URokMainHudWidget::HandleOpenChargeEvents);
	ChargeEventsButton->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(ExpandedDesignCanvas, ChargeEventsButton, FVector2D(-12, 112), FVector2D(80),
		FAnchors(1, 0), FVector2D(1, 0));
	PlaceOnCanvas(ExpandedDesignCanvas, MakeNotificationBadge(TEXT("ChargeEvents")), FVector2D(-21, 104),
		FVector2D(27, 29), FAnchors(1, 0), FVector2D(1, 0));
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
	for (int32 Index = 0; Index < CollapsibleFeatureWidgets.Num(); ++Index)
	{
		if (UWidget* Widget = CollapsibleFeatureWidgets[Index])
		{
			Widget->SetVisibility(FeatureVisibility);
			if (Widget != FeatureBackground)
			{
				Widget->SetRenderTranslation(CollapsibleFeatureOpenTranslations[Index]
					+ FVector2D(FeatureButtonsOffset, 0.0f));
			}
		}
	}
	if (FeatureBackground)
	{
		// Unity's centered size delta -437.96 + center X218.98 keeps the
		// right edge fixed; this UMG slot is already right-anchored.
		const double Width = FMath::Max(1.0, FeatureBackgroundOpenSize.X - 437.96f * FeatureFold);
		if (auto* BackgroundSlot = Cast<UCanvasPanelSlot>(FeatureBackground->Slot))
		{
			BackgroundSlot->SetSize(FVector2D(Width, FeatureBackgroundOpenSize.Y));
		}
		// Unity Image.GetAdjustedBorders proportionally compresses the borders.
		const double BorderScale = FMath::Min(1.0, Width / FMath::Max(1.0, FeatureBackgroundBorders.X + FeatureBackgroundBorders.Y));
		const double Left = FeatureBackgroundBorders.X * BorderScale;
		const double Right = FeatureBackgroundBorders.Y * BorderScale;
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Column = 0; Column < 3; ++Column)
			{
				UWidget* Patch = FeatureBackground->GetChildAt(Row * 3 + Column);
				if (auto* PatchSlot = Patch ? Cast<UCanvasPanelSlot>(Patch->Slot) : nullptr)
				{
					FMargin Offsets = PatchSlot->GetOffsets();
					Offsets.Left = Column == 0 ? 0.0f : (Column == 1 ? Left : -Right);
					Offsets.Right = Column == 0 ? Left : Right;
					PatchSlot->SetOffsets(Offsets);
				}
			}
		}
		FeatureBackground->SetRenderOpacity(FeatureAlpha);
	}
	if (MoreFeatureButton)
	{
		// Unity Z rotation is opposite UMG's screen-down rotation.
		MoreFeatureButton->SetRenderTransformAngle(MoreFeatureOpenAngle + 90.0f * FeatureFold);
	}
}

void URokMainHudWidget::ApplyTaskFoldPresentation()
{
	const auto Resize = [this](UWidget* Widget, const FVector2D& OpenSize, const FVector2D& SizeDelta)
	{
		if (auto* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
		{
			const FVector2D Size = OpenSize + SizeDelta * TaskFold;
			CanvasSlot->SetSize(FVector2D(FMath::Max(0.0, Size.X), FMath::Max(0.0, Size.Y)));
		}
	};
	Resize(TaskListBackground, TaskBackgroundOpenSize, FVector2D(-64, -279));
	Resize(TaskListClip, TaskClipOpenSize, FVector2D(-64, -TaskClipOpenSize.Y));
	if (auto* ClipSlot = TaskListClip ? Cast<UCanvasPanelSlot>(TaskListClip->Slot) : nullptr)
	{
		ClipSlot->SetPosition(TaskClipOpenPosition + FVector2D(0.0, 0.5 * TaskFold));
		TaskListClip->SetVisibility(TaskFold >= 1.0f
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (auto* FoldSlot = TaskFoldButton ? Cast<UCanvasPanelSlot>(TaskFoldButton->Slot) : nullptr)
	{
		FoldSlot->SetPosition(TaskFoldOpenPosition + FVector2D(-64.0 * TaskFold, 0.0));
		TaskFoldButton->SetRenderTransformAngle(TaskFoldOpenAngle - 180.0f * TaskFold);
	}
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

void URokMainHudWidget::RebuildTaskRows(int32 Count, bool bServer)
{
	if (!TaskListBox) return;
	TaskListBox->ClearChildren();
	TaskTitleTexts.Reset();
	TaskProgressTexts.Reset();
	ServerTaskRewardImages.Reset();
	ServerHudTaskIds.Reset();
	ServerHudTaskPages.Reset();
	TaskRewardImage = nullptr;
	RewardTaskId = 0;
	bTaskRewardVisible = false;
	TaskRewardTime = 0.0f;
	bServerTaskRows = bServer;
	static const TSoftClassPtr<URokUiPageWidget> TaskRowClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokHudTaskRow.WBP_RokHudTaskRow_C")));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		auto* Page = CreateHudPage(TaskRowClass, NAME_None);
		auto* Row = Page ? Cast<UCanvasPanel>(Page->GetRootWidget()) : nullptr;
		if (!Row || !BindTaskRow(*Page, Index))
		{
			Page = nullptr;
			Row = MakeTaskRow(Index);
		}
		if (bServer)
		{
			URokUnityIndexedButton* Hit = WidgetTree->ConstructWidget<URokUnityIndexedButton>(
				URokUnityIndexedButton::StaticClass(), *FString::Printf(TEXT("HudTaskAction_%d"), Index));
			FSlateBrush None;
			None.DrawAs = ESlateBrushDrawType::NoDrawType;
			FButtonStyle Style;
			Style.SetNormal(None).SetHovered(None).SetPressed(None).SetDisabled(None);
			Hit->SetStyle(Style);
			Hit->SetBackgroundColor(FLinearColor::Transparent);
			Hit->ConfigureAction(Index);
			Hit->OnIndexedClicked.AddUObject(this, &URokMainHudWidget::HandleServerTaskClicked);
			PlaceOnCanvas(Row, Hit, FVector2D::ZeroVector, TaskRowLayoutSize)->SetZOrder(10);
		}
		const FName SizeName = Index == 0 ? FName(TEXT("TaskRowSize"))
			: FName(*FString::Printf(TEXT("TaskRowSize_%d"), Index));
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), SizeName);
		Size->SetHeightOverride(TaskRowLayoutSize.Y);
		Size->SetWidthOverride(TaskRowLayoutSize.X);
		Size->SetContent(Page ? static_cast<UWidget*>(Page) : Row);
		UVerticalBoxSlot* RowSlot = TaskListBox->AddChildToVerticalBox(Size);
		RowSlot->SetHorizontalAlignment(HAlign_Left);
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TaskRowBottomPadding));
		RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
}

bool URokMainHudWidget::BindTaskRow(URokUiPageWidget& Page, int32 Index)
{
	Page.LegacySuffix = FString::Printf(TEXT("_%d"), Index);
	Page.LegacyNames.Add(Page.GetRootWidget()->GetFName(), TEXT("TaskRow{Suffix}"));
	auto* Title = Cast<UTextBlock>(Page.GetWidgetFromName(TEXT("TaskTitle")));
	auto* Progress = Cast<UTextBlock>(Page.GetWidgetFromName(TEXT("TaskProgress")));
	auto* Reward = Cast<UImage>(Page.GetWidgetFromName(TEXT("TaskReward")));
	if (!Title || !Progress || !Reward)
	{
		UE_LOG(LogTemp, Error, TEXT("Rok UI: %s needs TaskTitle, TaskProgress and TaskReward"), *Page.GetClass()->GetName());
		return false;
	}

	if (Index > 0)
	{
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.94f, 0.9f, 1.0f)));
		Progress->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	TaskTitleTexts.Add(Title);
	TaskProgressTexts.Add(Progress);
	ServerTaskRewardImages.Add(Reward);
	if (Index == 0) TaskRewardImage = Reward;
	return true;
}

bool URokMainHudWidget::ApplyServerTasks()
{
	const UGameInstance* Instance = GetGameInstance();
	const URokServerTaskSubsystem* Tasks = Instance ? Instance->GetSubsystem<URokServerTaskSubsystem>() : nullptr;
	const FRokServerTaskSnapshot Snapshot = Tasks ? Tasks->GetSnapshot() : FRokServerTaskSnapshot();
	const auto IsFinished = [](const auto& Row) { return Row.State == ERokServerTaskState::Finished; };
	int32 CountReady = 0;
	if (Snapshot.bAvailable && Snapshot.bLoaded)
	{
		CountReady = Snapshot.bChapterClaimable ? 1 : Algo::CountIf(Snapshot.ChapterTasks, IsFinished);
		int32 IntroChapter = 1;
		if (auto* Config = Instance->GetSubsystem<URokUnityConfigSubsystem>())
		{
			const auto* Rows = Config->GetTableRows(TEXT("Config"));
			if (Rows) for (const auto& Row : *Rows)
			{
				FString Key, Value;
				if (Row && Row->TryGetStringField(TEXT("Scouts"), Key) && Key == TEXT("preChapter")
					&& Row->TryGetStringField(TEXT("Column5"), Value)) LexTryParseString(IntroChapter, *Value);
			}
		}
		if (Snapshot.ChapterId > IntroChapter || Snapshot.ChapterId == -1)
		{
			CountReady += Algo::CountIf(Snapshot.MainTasks, IsFinished)
				+ Algo::CountIf(Snapshot.SideTasks, IsFinished)
				+ Algo::CountIf(Snapshot.DailyTasks, IsFinished)
				+ Algo::CountIf(Snapshot.DailyMilestones, IsFinished);
		}
	}
	SetNotificationCount(TEXT("TaskEntry"), CountReady);
	if (!Snapshot.bAvailable)
	{
		if (bServerTaskRows) RebuildTaskRows(1, false);
		LastServerHudTaskSignature.Reset();
		return false;
	}
	if (Snapshot.Signature != LastServerHudTaskSignature)
	{
		const FRokServerTaskView* Main = Snapshot.MainTasks.IsEmpty() ? nullptr : &Snapshot.MainTasks[0];
		UE_LOG(LogTemp, Display,
			TEXT("Rok server task list: main=%d progress=%lld/%lld state=%d side=%d chapter=%d progress=%lld/%lld daily=%d loaded=%d"),
			Snapshot.MainTaskId, Main ? Main->Current : 0LL, Main ? Main->Target : 0LL,
			Main ? int32(Main->State) : 0, Snapshot.SideTasks.Num(), Snapshot.ChapterId,
			Snapshot.ChapterCurrent, Snapshot.ChapterTarget, Snapshot.DailyTasks.Num(), Snapshot.bLoaded);
		LastServerHudTaskSignature = Snapshot.Signature;
	}
	const int32 Count = Snapshot.bLoaded ? Snapshot.HudTasks.Num() : 1;
	if (!bServerTaskRows || TaskTitleTexts.Num() != Count) RebuildTaskRows(Count, true);
	ServerHudTaskIds.Reset();
	ServerHudTaskPages.Reset();
	for (int32 Index = 0; Index < Count && Index < TaskTitleTexts.Num(); ++Index)
	{
		UTextBlock* Title = TaskTitleTexts[Index];
		UTextBlock* Progress = TaskProgressTexts[Index];
		UImage* Reward = ServerTaskRewardImages.IsValidIndex(Index) ? ServerTaskRewardImages[Index].Get() : nullptr;
		if (!Snapshot.bLoaded)
		{
			SetFittedTaskText(*Title, NSLOCTEXT("RokHud", "ServerTaskLoading", "任务同步中"), TaskTitlePixels, TaskTitleSize);
			SetFittedTaskText(*Progress, NSLOCTEXT("RokHud", "ServerTaskLoadingProgress", "正在获取任务状态…"), TaskDescriptionPixels, TaskDescriptionSize);
			if (Reward) Reward->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const FRokServerTaskView& Task = Snapshot.HudTasks[Index];
		ServerHudTaskIds.Add(Task.TaskId);
		ServerHudTaskPages.Add(int32(Task.Page));
		SetFittedTaskText(*Title, FText::FromString(Task.Name), TaskTitlePixels, TaskTitleSize);
		Title->SetColorAndOpacity(Task.Page == ERokServerTaskPage::Side
			? FLinearColor(0.96f, 0.94f, 0.9f, 1.0f) : FLinearColor(1.0f, 0.68f, 0.25f, 1.0f));
		const FText DescriptionText = Task.bProgressKnown
			? FText::Format(NSLOCTEXT("RokHud", "ServerTaskProgress", "{0} ({1}/{2})"),
				FText::FromString(Task.Description), FText::AsNumber(Task.Current), FText::AsNumber(Task.Target))
			: FText::Format(NSLOCTEXT("RokHud", "ServerTaskProgressPending", "{0} · 进度同步中"),
				FText::FromString(Task.Description));
		SetFittedTaskText(*Progress, DescriptionText, TaskDescriptionPixels, TaskDescriptionSize);
		Progress->SetToolTipText(FText::GetEmpty());
		if (Reward) Reward->SetVisibility(Task.State == ERokServerTaskState::Finished
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	return true;
}

namespace
{
int32 CountActivityTargetRewards(const FJsonObject& Schedule, int32 ActivityId, URokUnityConfigSubsystem& Config)
{
	const TArray<TSharedPtr<FJsonValue>>* ProgressRows = nullptr;
	if (!Schedule.TryGetArrayField(TEXT("scheduleInfo"), ProgressRows) || ProgressRows->IsEmpty()
		|| ActivityId > (MAX_int32 - 10000) / 10000) return 0;
	TMap<FIntPoint, int64> Progress;
	for (const auto& Value : *ProgressRows)
	{
		const TSharedPtr<FJsonObject>* Behavior = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Conditions = nullptr;
		int32 Type = 0;
		if (!Value || !Value->TryGetObject(Behavior) || !(*Behavior)->TryGetNumberField(TEXT("type"), Type)
			|| !(*Behavior)->TryGetArrayField(TEXT("data"), Conditions)) continue;
		for (const auto& ConditionValue : *Conditions)
		{
			const TSharedPtr<FJsonObject>* Condition = nullptr;
			int32 Key = 0;
			int64 Count = 0;
			if (ConditionValue && ConditionValue->TryGetObject(Condition)
				&& (*Condition)->TryGetNumberField(TEXT("condition"), Key)
				&& RokSprotoJson::TryGetInt64(*Condition, TEXT("count"), Count)) Progress.Add(FIntPoint(Type, Key), Count);
		}
	}
	TSet<int32> Claimed;
	const TArray<TSharedPtr<FJsonValue>>* RewardIds = nullptr;
	if (Schedule.TryGetArrayField(TEXT("rewardId"), RewardIds))
	{
		for (const auto& Value : *RewardIds)
		{
			int32 Id = 0;
			if (Value && Value->TryGetNumber(Id)) Claimed.Add(Id);
		}
	}
	const auto CountRows = [&](int32 Base)
	{
		int32 Count = 0;
		for (int32 Offset = 1; Offset < 500; ++Offset)
		{
			const int32 Id = Base + Offset;
			int32 Goal = 0, Behavior = 0, Condition = 0;
			if (!Config.GetIntFieldByKey(TEXT("ActivityTargetType"), TEXT("ID"), Id, TEXT("data0"), Goal)) break;
			if (!Claimed.Contains(Id)
				&& Config.GetIntFieldByKey(TEXT("ActivityTargetType"), TEXT("ID"), Id, TEXT("playerBehavior"), Behavior)
				&& Config.GetIntFieldByKey(TEXT("ActivityTargetType"), TEXT("ID"), Id, TEXT("data3"), Condition))
			{
				const int64* Value = Progress.Find(FIntPoint(Behavior, Condition));
				if (Value && *Value >= Goal) ++Count;
			}
		}
		return Count;
	};
	int32 Level = 0;
	Schedule.TryGetNumberField(TEXT("level"), Level);
	int32 Count = CountRows(ActivityId * 10000 - 100);
	for (Level = FMath::Min(Level, 99); Level > 0; --Level)
	{
		const int32 Base = ActivityId * 10000 + Level * 100;
		int32 Goal = 0;
		if (Config.GetIntFieldByKey(TEXT("ActivityTargetType"), TEXT("ID"), Base + 1, TEXT("data0"), Goal))
		{
			Count += CountRows(Base);
			break;
		}
	}
	return Count;
}

bool HasClaimableChargeRow(const FJsonObject& Role, URokUnityConfigSubsystem& Config, FName Table,
	const TCHAR* ClaimedField, const TCHAR* GoalField, int64 Progress, bool bRiseRoad)
{
	const auto* Rows = Config.GetTableRows(Table);
	const TArray<TSharedPtr<FJsonValue>>* Claimed = nullptr;
	if (!Rows || !Role.TryGetArrayField(ClaimedField, Claimed)) return false;
	for (const auto& Row : *Rows)
	{
		int32 Id = 0, Goal = 0, Type = 0;
		if (!Row || !Row->TryGetNumberField(TEXT("ID"), Id) || !Row->TryGetNumberField(GoalField, Goal)
			|| (bRiseRoad && (!Row->TryGetNumberField(TEXT("type"), Type) || Type != 2))) continue;
		if (Claimed->ContainsByPredicate([Id](const auto& Value)
		{
			int32 ValueId = 0;
			return Value && Value->TryGetNumber(ValueId) && ValueId == Id;
		})) continue;
		if (Progress >= Goal) return true;
		if (bRiseRoad) return false;
	}
	return false;
}
}

void URokMainHudWidget::RefreshNotificationBadges()
{
	const UGameInstance* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	const auto* Heroes = Instance ? Instance->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
	auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	const auto Role = Net && Net->GetState() == ERokGameNetState::RoleReady ? Net->GetRoleInfo() : nullptr;
	SetNotificationCount(TEXT("MailFeature"), Role ? Net->GetUnreadMailCount() : 0);
	SetNotificationCount(TEXT("CommanderFeature"), Role && Heroes ? Heroes->GetNotificationCount() : 0);

	int32 Campaign = 0;
	int32 UnlockLevel = 0;
	int64 Level = 0;
	if (Config) Config->GetIntFieldByKey(TEXT("SystemOpen"), TEXT("ID"), 10005, TEXT("openLv"), UnlockLevel);
	if (RokSprotoJson::TryGetInt64(Role, TEXT("level"), Level) && Level >= (UnlockLevel > 0 ? UnlockLevel : 5))
	{
		const TArray<TSharedPtr<FJsonValue>>* Stages = nullptr;
		if (!Role->TryGetArrayField(TEXT("expeditionInfo"), Stages) || Stages->IsEmpty()) Campaign = 1;
		else for (const auto& Value : *Stages)
		{
			const TSharedPtr<FJsonObject>* Stage = nullptr;
			int64 Stars = 0;
			bool bRewarded = false;
			if (Value && Value->TryGetObject(Stage) && RokSprotoJson::TryGetInt64(*Stage, TEXT("star"), Stars)
				&& Stars == 3 && (*Stage)->TryGetBoolField(TEXT("reward"), bRewarded) && !bRewarded) ++Campaign;
		}
	}
	SetNotificationCount(TEXT("CampaignFeature"), Campaign);

	double ServerTime = 0.0;
	const bool bHasServerTime = Role && Net->TryGetServerTimeSeconds(ServerTime);
	int32 Activity = 0;
	const TArray<TSharedPtr<FJsonValue>>* Schedules = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* ActivityTimes = nullptr;
	if (bHasServerTime && Config && Role->TryGetArrayField(TEXT("activity"), Schedules)
		&& Role->TryGetArrayField(TEXT("activityTimeInfo"), ActivityTimes))
	{
		TMap<int32, TSharedPtr<FJsonObject>> ById;
		for (const auto& Value : *Schedules)
		{
			const TSharedPtr<FJsonObject>* Schedule = nullptr;
			int64 Id = 0;
			if (Value && Value->TryGetObject(Schedule) && RokSprotoJson::TryGetInt64(*Schedule, TEXT("activityId"), Id)
				&& Id > 0 && Id <= MAX_int32) ById.Add(static_cast<int32>(Id), *Schedule);
		}
		const FDateTime Today = FDateTime::FromUnixTimestamp(FMath::FloorToInt64(ServerTime)).GetDate();
		const int64 WeekStart = (Today - FTimespan::FromDays((int32(Today.GetDayOfWeek()) + 1) % 7)).ToUnixTimestamp();
		const int64 WeekEnd = WeekStart + 7 * 86400;
		for (const auto& Value : *ActivityTimes)
		{
			const TSharedPtr<FJsonObject>* Time = nullptr;
			int64 RawId = 0, Start = 0, End = 0;
			if (!Value || !Value->TryGetObject(Time) || !RokSprotoJson::TryGetInt64(*Time, TEXT("activityId"), RawId)
				|| RawId <= 0 || RawId > MAX_int32 || RawId == 100001
				|| !RokSprotoJson::TryGetInt64(*Time, TEXT("startTime"), Start)
				|| !RokSprotoJson::TryGetInt64(*Time, TEXT("endTime"), End)) continue;
			const int32 Id = static_cast<int32>(RawId);
			const auto* Schedule = ById.Find(Id);
			if (!Schedule) continue;
			int32 Calendar = 0, TimeType = 0, ActivityType = 0, Previous = 0;
			if (!Config->GetIntFieldByKey(TEXT("ActivityCalendar"), TEXT("ID"), Id, TEXT("ifShow"), Calendar)
				|| !Config->GetIntFieldByKey(TEXT("ActivityCalendar"), TEXT("ID"), Id, TEXT("timeType"), TimeType)) continue;
			const bool bActive = Start < 0 || (ServerTime >= Start && ServerTime <= End);
			const bool bInCalendar = Start >= 0 && Start <= End
				&& ((Start >= WeekStart && Start < WeekEnd) || (End >= WeekStart && End < WeekEnd));
			bool bNew = false;
			if ((*Schedule)->TryGetBoolField(TEXT("isNew"), bNew) && bNew
				&& (Calendar == 1 ? bInCalendar : (TimeType == 4 || bActive))) ++Activity;
			if (bActive && Config->GetIntFieldByKey(TEXT("ActivityCalendar"), TEXT("ID"), Id, TEXT("activityType"), ActivityType)
				&& ActivityType >= 400 && ActivityType <= 403
				&& Config->GetIntFieldByKey(TEXT("ActivityCalendar"), TEXT("ID"), Id, TEXT("prepositionID"), Previous)
				&& Previous == 0)
			{
				Activity += CountActivityTargetRewards(**Schedule, Id, *Config);
			}
		}
	}
	SetNotificationCount(TEXT("Activity"), Activity);

	int32 ChargeLevel = 0;
	if (Config) Config->GetIntFieldByKey(TEXT("SystemOpen"), TEXT("ID"), 30001, TEXT("openLv"), ChargeLevel);
	const bool bChargeOpen = Role && Level >= (ChargeLevel > 0 ? ChargeLevel : 3);
	int32 ChargeCount = 0;
	if (ChargeEventsButton)
	{
		ChargeEventsButton->SetVisibility(bChargeOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (bChargeOpen && Config)
	{
		bool bDailyClaimed = false, bGrowthFund = false, bFirstCharge = false;
		if (Role->TryGetBoolField(TEXT("freeDaily"), bDailyClaimed) && !bDailyClaimed) ++ChargeCount;
		if (Role->TryGetBoolField(TEXT("growthFund"), bGrowthFund)
			&& (!bGrowthFund || HasClaimableChargeRow(*Role, *Config, TEXT("RechargeFund"),
				TEXT("growthFundReward"), TEXT("needLv"), Level, false))) ++ChargeCount;
		int64 RiseProgress = 0;
		RokSprotoJson::TryGetInt64(Role, TEXT("riseRoad"), RiseProgress);
		const bool bRiseHasPage = HasClaimableChargeRow(*Role, *Config, TEXT("RechargeFirst"),
			TEXT("riseRoadPackage"), TEXT("needDenar"), MAX_int64, true);
		if (HasClaimableChargeRow(*Role, *Config, TEXT("RechargeFirst"),
			TEXT("riseRoadPackage"), TEXT("needDenar"), RiseProgress, true)) ++ChargeCount;
		const TArray<TSharedPtr<FJsonValue>>* Supplies = nullptr;
		if (bHasServerTime && Role->TryGetArrayField(TEXT("supply"), Supplies))
		{
			for (const auto& Value : *Supplies)
			{
				const TSharedPtr<FJsonObject>* Row = nullptr;
				int64 Expires = 0;
				bool bAwarded = false;
				if (Value && Value->TryGetObject(Row) && RokSprotoJson::TryGetInt64(*Row, TEXT("expiredTime"), Expires)
					&& Expires > ServerTime && (*Row)->TryGetBoolField(TEXT("award"), bAwarded) && !bAwarded) ++ChargeCount;
			}
		}
		Role->TryGetBoolField(TEXT("rechargeFirst"), bFirstCharge);
		const int32 Page = !bFirstCharge ? 101 : (bRiseHasPage ? 102 : 103);
		if (ChargeEventPage != Page && ChargeEventsButton)
		{
			ChargeEventPage = Page;
			if (auto* Icon = Cast<UImage>(ChargeEventsButton->GetContent()))
			{
				Icon->SetBrush(MakeAtlasBrush(LoadUiTexture(MainMenuAtlasPath),
					Page == 101 ? FirstChargeRect : (Page == 102 ? RiseRoadRect : SpecialOfferRect),
					MainMenuAtlasSize, FVector2D(80)));
			}
		}
	}
	SetNotificationCount(TEXT("ChargeEvents"), ChargeCount);
}

void URokMainHudWidget::HandleServerTaskClicked(int32 RowIndex)
{
	if (!FeatureScreenWidget || !ServerHudTaskIds.IsValidIndex(RowIndex)) return;
	const UGameInstance* Instance = GetGameInstance();
	const URokServerTaskSubsystem* Tasks = Instance ? Instance->GetSubsystem<URokServerTaskSubsystem>() : nullptr;
	const FRokServerTaskSnapshot Snapshot = Tasks ? Tasks->GetSnapshot() : FRokServerTaskSnapshot();
	if (!Snapshot.bAvailable || !Snapshot.HudTasks.IsValidIndex(RowIndex)
		|| Snapshot.HudTasks[RowIndex].TaskId != ServerHudTaskIds[RowIndex]
		|| int32(Snapshot.HudTasks[RowIndex].Page) != ServerHudTaskPages[RowIndex]) return;
	if (ModalWidget) ModalWidget->CloseModal();
	const FRokServerTaskView& Task = Snapshot.HudTasks[RowIndex];
	if (Task.Type == 30 && Task.State == ERokServerTaskState::Unfinished)
	{
		FeatureScreenWidget->OpenTaskCityTarget(14);
		return;
	}
	const ERokServerTaskPage Page = Task.Page;
	FeatureScreenWidget->OpenServerTasksPage(Page == ERokServerTaskPage::Chapter ? 0
		: Page == ERokServerTaskPage::Daily ? 2 : 1);
}

void URokMainHudWidget::SetPowerPresentation(int32 Value)
{
	if (PowerGain) PowerGain->ObservePower(PowerCounter.Target, Value, IsVisible() && GetRenderOpacity() > 0);
	PowerCounter.SetTarget(Value);
	if (PowerText) PowerText->SetText(PowerCounter.Target > 0
		? FText::AsNumber(PowerCounter.Value) : NSLOCTEXT("RokHud", "ProfilePowerUnavailable", "--"));
}

void URokMainHudWidget::ApplyServerTopBar(const FRokPlayerTopBar& TopBar)
{
	if (PowerText)
	{
		SetPowerPresentation(static_cast<int32>(FMath::Clamp<int64>(TopBar.Power, 0, MAX_int32)));
		PowerText->SetToolTipText(FText::GetEmpty());
	}
	const struct
	{
		int64 Value;
		bool bVisible;
	} Cells[] = {
		{TopBar.Food, TopBar.bShowFood}, {TopBar.Wood, TopBar.bShowWood},
		{TopBar.Stone, TopBar.bShowStone}, {TopBar.Gold, TopBar.bShowGold}, {TopBar.Denar, TopBar.bShowGem}};
	double Width = ResourceGroupLeftPadding;
	for (int32 Index = 0; Index < ResourceCellLayouts.Num(); ++Index)
	{
		const auto& Cell = Cells[Index];
		const auto& Layout = ResourceCellLayouts[Index];
		if (auto* Widget = GetWidgetFromName(ResourceCellNames[Index]))
		{
			Widget->SetVisibility(Cell.bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
			if (!Cell.bVisible) continue;
			Width += Layout.Gap;
			if (auto* CellSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
			{
				CellSlot->SetPosition(FVector2D(Width, Layout.Y));
			}
			const FName ValueName(*(ResourceCellNames[Index].ToString() + TEXT("Value")));
			if (auto* Value = Cast<UTextBlock>(GetWidgetFromName(ValueName)))
			{
				Value->SetText(FormatResource(Cell.Value));
			}
			Width += Layout.Width;
		}
	}
	if (auto* Group = GetWidgetFromName(TEXT("PlayerResourcesGroup")))
	{
		if (auto* GroupSlot = Cast<UCanvasPanelSlot>(Group->Slot))
		{
			GroupSlot->SetSize(FVector2D(FMath::Max(0.0, Width + ResourceGroupRightPadding), GroupSlot->GetSize().Y));
		}
	}
	if (VipText) VipText->SetText(FText::FromString(TopBar.VipText));
	if (VipRedPointImage)
	{
		VipRedPointImage->SetVisibility(TopBar.bVipRedPoint ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CoordinateText) CoordinateText->SetText(FText::FromString(RokPlayerTopBar::FormatCoordinate(TopBar.PosX, TopBar.PosY)));
	if (AgeText) AgeText->SetText(FText::FromString(TopBar.AgeText));
	if (CityNameText) CityNameText->SetText(FText::FromString(TopBar.Name));
	if (CityLevelText) CityLevelText->SetText(FText::AsNumber(FMath::Max(1, TopBar.Level)));
	if (TopBar.HeadId > 0 && TopBar.HeadId != ShownHeadId)
	{
		ShownHeadId = TopBar.HeadId;
		ApplyPlayerHeadSprite(PlayerHeadImage, ShownHeadId, FVector2D(92.0f, 92.0f));
	}
	if (TopBar.HeadFrameId > 0 && TopBar.HeadFrameId != ShownHeadFrameId)
	{
		ShownHeadFrameId = TopBar.HeadFrameId;
		ApplyPlayerHeadSprite(PlayerHeadFrameImage, ShownHeadFrameId, FVector2D(104.0f, 104.0f));
	}
}

void URokMainHudWidget::ApplyPlayerHeadSprite(UImage* Image, int32 HeadId, const FVector2D& DesiredSize)
{
	// UI_Model_PlayerHead_SubView.LoadPlayerIcon: PlayerHead.csv icon, frames included.
	static TArray<FRokPlayerHeadRow> Catalog;
	static bool bCatalogLoaded = false;
	if (!bCatalogLoaded)
	{
		bCatalogLoaded = true;
		FString Error;
		if (!RokPlayerHeadCatalog::Load(Catalog, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("Rok top bar: player head catalog unavailable (%s)."), *Error);
		}
	}
	const FRokPlayerHeadRow* Row = Catalog.FindByPredicate([HeadId](const FRokPlayerHeadRow& Candidate)
	{
		return Candidate.Id == HeadId;
	});
	UTexture2D* Texture = Row && Row->bArtAvailable ? LoadUiTexture(*Row->Texture) : nullptr;
	if (!Image || !Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok top bar: no head sprite for PlayerHead %d."), HeadId);
		return;
	}
	Image->SetBrush(MakeAtlasBrush(Texture, Row->Rect, Row->AtlasSize, DesiredSize));
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
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
	if (bServerTaskRows)
	{
		TaskRewardTime = FMath::Fmod(TaskRewardTime + FMath::Max(0.0f, DeltaSeconds), RokHudAnimation::RewardDuration);
		const float Pulse = RokHudAnimation::RewardPulse(TaskRewardTime);
		for (UImage* Image : ServerTaskRewardImages)
		{
			if (Image && Image->GetVisibility() != ESlateVisibility::Collapsed)
			{
				Image->SetRenderScale(FVector2D(1.0f + 0.2f * Pulse));
				Image->SetRenderTransformAngle(-2.0f * Pulse);
			}
		}
	}
	else if (bTaskRewardVisible && TaskRewardImage)
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
	static const TSoftClassPtr<URokUiPageWidget> MarkerPage(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokCityNameplate.WBP_RokCityNameplate_C")));
	if (auto* Page = AddHudPage(CityMarkerRoot, MarkerPage, TEXT("CityNameplatePage")))
	{
		CityNameText = Page->Get<UTextBlock>(TEXT("CityNameText"));
		CityLevelText = Page->Get<UTextBlock>(TEXT("CityLevelText"));
		return;
	}

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
		if (CityTopBarSizeBox)
		{
			CityTopBarSizeBox->SetWidthOverride(CurrentExpandedDesignSize.X);
			CityTopBarSizeBox->SetHeightOverride(CurrentExpandedDesignSize.Y);
		}
	}
}

void URokMainHudWidget::UpdateCityMarkerProjection()
{
	// The city view has no floating nameplate in Unity: the player-city label
	// belongs to the world map (GlobalViewLevelMediator.SetCityHud). Keep the
	// local-preview marker as an explicit diagnostic only.
	bool bShowCityMarker = false;
#if !UE_BUILD_SHIPPING
	bShowCityMarker = FParse::Param(
		FCommandLine::Get(),
		TEXT("RokShowCityMarker"));
#endif
	if (!bShowCityMarker || !CityMarkerCanvasSlot.IsValid() || !GetWorld() || CurrentRootSize.X <= 1.0f || CurrentRootSize.Y <= 1.0f)
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
	if ((bPlacementCanCommit || bPlacementCanOpenWorkerQueue) && PlacementConfirmButton)
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
	if (FeatureScreenWidget && FeatureScreenWidget->IsWorkerQueueOpen())
	{
		PlacementControls->SetVisibility(ESlateVisibility::Collapsed);
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
	const auto* BoundsSlot = PlacementControlsDesignSlot.Get();
	const FAnchorData Bounds = BoundsSlot ? BoundsSlot->GetLayout() : MakeDefaultPlacementControlsBounds();
	if (!Snapshot.bActive || !Preview || !Bottom || !Camera)
	{
		// The explicit no-preview feedback route has no Unity world target.
		// Keep cancellation and its existing reason visible without pretending
		// that a source building menu has a valid screen projection.
		MenuSlot->SetLayout(Bounds);
		PlacementControls->SetRenderScale(FVector2D(1.0f));
		PlacementControls->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (PlacementButtonShadow) PlacementButtonShadow->SetVisibility(ESlateVisibility::Collapsed);
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
	const FVector PlacementCenter = Bottom->GetComponentLocation();
	FVector2D ScreenPosition;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		Controller, PlacementCenter, ScreenPosition, false))
	{
		PlacementControls->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// HUDManager.AutoAnchorUI/SetScale use InitCameraDxf 172 for this popup.
	// Measured against the 16:10 source Game View, with the prefab's 100-unit button spacing.
	const float MenuScale = (172.0f / Dxf) * (CurrentRootSize.Y / PlacementMenuDesignHeight);
	MenuSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	MenuSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	MenuSlot->SetSize(FVector2D(Bounds.Offsets.Right, Bounds.Offsets.Bottom));
	// Anchor the two-button row below the footprint as in the Unity 16:10 placement capture.
	MenuSlot->SetPosition(ScreenPosition + FVector2D(0.0f, PlacementMenuGroundOffsetY * MenuScale));
	PlacementControls->SetRenderScale(FVector2D(MenuScale));
	PlacementControls->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (PlacementButtonShadow) PlacementButtonShadow->SetVisibility(ESlateVisibility::HitTestInvisible);
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

	UTextBlock* Title = MakeHudText(*FString::Printf(TEXT("TaskTitle_%d"), Index), TaskTitlePixels,
		Index == 0 ? FLinearColor(1.0f, 0.68f, 0.25f, 1.0f) : FLinearColor(0.96f, 0.94f, 0.9f, 1.0f), true);
	Title->SetFont(MakeUnitySourceFont(TaskTitlePixels, true));
	Title->SetShadowOffset(FVector2D::ZeroVector);
	SetFittedTaskText(*Title, NSLOCTEXT("RokHud", "TaskLoading", "任务加载中"), TaskTitlePixels, TaskTitleSize);
	UBorder* TitleBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("TaskTitleBox_%d"), Index));
	TitleBox->SetBrushColor(FLinearColor::Transparent);
	TitleBox->SetPadding(FMargin(0));
	TitleBox->SetVerticalAlignment(VAlign_Bottom);
	TitleBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TitleBox->SetContent(Title);
	PlaceOnCanvas(Row, TitleBox, FVector2D(9.685f, 1.1f), TaskTitleSize);
	TaskTitleTexts.Add(Title);

	UTextBlock* Progress = MakeHudText(*FString::Printf(TEXT("TaskProgress_%d"), Index), TaskDescriptionPixels,
		FLinearColor(0.94f, 0.93f, 0.9f, 1.0f));
	Progress->SetFont(MakeUnitySourceFont(TaskDescriptionPixels, false));
	Progress->SetShadowOffset(FVector2D::ZeroVector);
	Progress->SetLineHeightPercentage(0.8f);
	Progress->SetClipping(EWidgetClipping::ClipToBounds);
	SetFittedTaskText(*Progress, FText::GetEmpty(), TaskDescriptionPixels, TaskDescriptionSize);
	if (Index == 0) Progress->SetVisibility(ESlateVisibility::Visible);
	PlaceOnCanvas(Row, Progress, FVector2D(10.0f, 33.0f), TaskDescriptionSize);
	TaskProgressTexts.Add(Progress);

	UImage* Reward = MakeAtlasImage(*FString::Printf(TEXT("TaskReward_%d"), Index), MainMenuAtlasPath, TaskRewardRect, MainMenuAtlasSize, FVector2D(30.0f));
	Reward->SetRenderTransformPivot(FVector2D(0.5f));
	Reward->SetVisibility(ESlateVisibility::Collapsed);
	ServerTaskRewardImages.Add(Reward);
	if (Index == 0) TaskRewardImage = Reward;
	PlaceOnCanvas(Row, Reward, FVector2D(161.8f, 6.9f), FVector2D(30.0f));
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
	// UI_Model_Resources/lbl_val: Arial 22 px, #F4F4F4, with a downward shadow.
	OutTextBlock = MakeHudText(*FString::Printf(TEXT("%sValue"), *Name.ToString()), 22,
		FLinearColor(0.95686275f, 0.95686275f, 0.95686275f, 1.0f));
	OutTextBlock->SetFont(MakeUnitySourceFont(22));
	OutTextBlock->SetShadowOffset(FVector2D(0.0f, 2.0f));
	OutTextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7058824f));
	OutTextBlock->SetJustification(ETextJustify::Center);
	PlaceOnCanvas(Cell, OutTextBlock, FVector2D(37.54f, 7.0f), FVector2D(92.32f, 30.0f));
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
	bool bHasNotificationCount,
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
	if (bHasNotificationCount)
	{
		PlaceOnCanvas(Root, MakeNotificationBadge(Name), FVector2D(72, 0), FVector2D(27, 29))->SetZOrder(2);
	}
	return Root;
}

UCanvasPanel* URokMainHudWidget::MakeNotificationBadge(const FName& Name)
{
	const FString Prefix = Name.ToString();
	auto* Badge = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *(Prefix + TEXT("Badge")));
	Badge->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(Badge, MakeAtlasImage(*(Prefix + TEXT("RedPoint")), CommonAtlasPath, RedPointRect, CommonAtlasSize,
		FVector2D(27, 29)), FVector2D::ZeroVector, FVector2D(27, 29));
	auto* Count = MakeHudText(*(Prefix + TEXT("BadgeCount")), 14, FLinearColor::White, true);
	Count->SetJustification(ETextJustify::Center);
	Count->SetShadowOffset(FVector2D::ZeroVector);
	PlaceOnCanvas(Badge, Count, FVector2D(0, 5), FVector2D(27, 22))->SetZOrder(1);
	NotificationTexts.Add(Name, Count);
	return Badge;
}

void URokMainHudWidget::SetNotificationCount(const FName& Name, int32 Count)
{
	const auto* Found = NotificationTexts.Find(Name);
	if (!Found || !*Found) return;
	UTextBlock* Label = Found->Get();
	Label->GetParent()->SetVisibility(Count > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Label->SetText(Count > 99 ? FText::FromString(TEXT("99+")) : (Count > 0 ? FText::AsNumber(Count) : FText::GetEmpty()));
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

bool URokMainHudWidget::IsCityHudInsideBand(float CityDxf)
{
	// MainInterfaceMediator.SetCityButton: limitDxf is the camera dxf of
	// city_bound (250). At or below it the view is still "in city".
	if (!FMath::IsFinite(CityDxf))
	{
		// ARokStrategyCameraPawn::GetCurrentCityDxf already normalizes a broken
		// camera to city_default; keep the same fallback here.
		return true;
	}
	return CityDxf <= ARokStrategyCameraPawn::GetCityMaxDxf();
}

float URokMainHudWidget::GetCityCameraDxf() const
{
	const APlayerController* PlayerController = GetOwningPlayer();
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	const ARokStrategyCameraPawn* CameraPawn = Cast<ARokStrategyCameraPawn>(Pawn);
	return CameraPawn ? CameraPawn->GetCurrentCityDxf() : -1.0f;
}

void URokMainHudWidget::ApplyCityHudBand(bool bInsideCity)
{
	bCityHudInsideBand = bInsideCity;
	if (PositionGroup)
	{
		// OpenLodUi.csv Position row: City = 0, Tactical = 1. Only the camera
		// band drives the coordinate box; the viewport test never does.
		PositionGroup->SetVisibility(
			bInsideCity
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
	}
}

bool URokMainHudWidget::IsCityButtonOutsideBand(float CityDxf, bool bCityInViewport)
{
	// MainInterfaceMediator.OnCityButtonChange:
	//   outSide = dxf > limitDxf || !Common.IsInViewPort2D(WorldCamera, RolePos, ...)
	// So the hammer/globe pair also flips when the player pans the city out of
	// view while still inside the camera band.
	return !IsCityHudInsideBand(CityDxf) || !bCityInViewport;
}

bool URokMainHudWidget::IsCityInViewport()
{
	const APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController || !GetWorld() || CurrentRootSize.X <= 1.0f || CurrentRootSize.Y <= 1.0f)
	{
		return true;
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
	if (!CityMarkerAnchorActor.IsValid())
	{
		return true;
	}

	FVector2D ScreenPosition;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PlayerController,
		CityMarkerAnchorActor->GetActorLocation(),
		ScreenPosition,
		true))
	{
		// Behind the camera: the city is not on screen.
		return false;
	}
	constexpr float ViewportTolerance = 16.0f;
	return ScreenPosition.X >= -ViewportTolerance
		&& ScreenPosition.Y >= -ViewportTolerance
		&& ScreenPosition.X <= CurrentRootSize.X + ViewportTolerance
		&& ScreenPosition.Y <= CurrentRootSize.Y + ViewportTolerance;
}

void URokMainHudWidget::ApplyCityHudButtons(bool bOutside)
{
	bCityHudButtonsOutside = bOutside;
	const ESlateVisibility InCityVisibility =
		bOutside ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	const ESlateVisibility OutsideCityVisibility =
		bOutside ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (BuildCityButton)
	{
		BuildCityButton->SetVisibility(InCityVisibility);
	}
	if (WorldGlobeButton)
	{
		WorldGlobeButton->SetVisibility(InCityVisibility);
	}
	if (WorldSearchButton)
	{
		WorldSearchButton->SetVisibility(OutsideCityVisibility);
	}
	if (CityWorldButton)
	{
		CityWorldButton->SetVisibility(OutsideCityVisibility);
	}
}

FString URokMainHudWidget::FormatRemainingClock(float Seconds)
{
	const int32 TotalSeconds = FMath::Max(0, FMath::FloorToInt(Seconds));
	const int32 Days = TotalSeconds / 86400;
	const FString Clock = FString::Printf(TEXT("%02d:%02d:%02d"),
		(TotalSeconds / 3600) % 24, (TotalSeconds / 60) % 60, TotalSeconds % 60);
	if (Days <= 0)
	{
		return Clock;
	}
	return FString::Printf(TEXT("%s %s"),
		*FRokUiTextTable::Resolve(128004).Replace(TEXT("{0}"), *FString::FromInt(Days)), *Clock);
}

float URokMainHudWidget::GetBuildBarProgress(float TotalSeconds, float RemainingSeconds)
{
	if (RemainingSeconds <= 0.0f || TotalSeconds <= 0.0f)
	{
		return 1.0f;
	}
	return FMath::Clamp((TotalSeconds - RemainingSeconds) / TotalSeconds, 0.0f, 1.0f);
}

void URokMainHudWidget::BuildConstructionBarPool()
{
	// UI_Tip_BuildingBar, laid out in its 100x100 prefab root (Unity units, y down here):
	// pb_rogressBar 150x20 at (0,+7.4) with Background, Fill Area (-4,-4) and lbl_time,
	// then img_researchBg 32x32 and img_icon 28x28 at (+86.7,+7.4).
	constexpr int32 BarCount = 4;
	const FVector2D BarTopLeft(50.0f - 75.0f, 50.0f - 7.4f - 10.0f);
	const FVector2D IconCenter(50.0f + 86.7f, 50.0f - 7.4f);
	ConstructionBars.Reset();
	ConstructionBarLabels.Reset();
	ConstructionBarFills.Reset();
	ConstructionBarIcons.Reset();
	ConstructionBarActors.Reset();
	ConstructionBarActors.SetNum(BarCount);
	if (UTexture2D* MainMenuAtlas = LoadUiTexture(MainMenuAtlasPath))
	{
		ConstructionBarBuildingBrush = MakeAtlasBrush(MainMenuAtlas, BuildBarBuildingIconRect,
			MainMenuAtlasSize, FVector2D(28.0f, 28.0f));
		ConstructionBarLevelUpBrush = MakeAtlasBrush(MainMenuAtlas, BuildBarLevelUpIconRect,
			MainMenuAtlasSize, FVector2D(28.0f, 28.0f));
	}
	for (int32 Index = 0; Index < BarCount; ++Index)
	{
		UCanvasPanel* Bar = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("ConstructionBar_%d"), Index));
		Bar->SetVisibility(ESlateVisibility::Collapsed);
		Bar->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		UCanvasPanelSlot* BarSlot = PlaceOnCanvas(ExpandedDesignCanvas, Bar,
			FVector2D::ZeroVector, FVector2D(100.0f, 100.0f), FAnchors(0.0f, 0.0f), FVector2D(0.5f, 0.5f));
		BarSlot->SetZOrder(6);
		static const TSoftClassPtr<URokUiPageWidget> BarClass(FSoftObjectPath(
			TEXT("/Game/RokUI/Hud/WBP_RokCityConstructionBar.WBP_RokCityConstructionBar_C")));
		if (auto* Page = AddHudPage(Bar, BarClass, NAME_None))
		{
			auto* Label = Page->Get<UTextBlock>(TEXT("ConstructionBarTime"));
			auto* Fill = Page->Get<UCanvasPanel>(TEXT("ConstructionBarFill"));
			auto* Icon = Page->Get<UImage>(TEXT("ConstructionBarIcon"));
			if (Label && Fill && Icon)
			{
				Page->LegacySuffix = FString::Printf(TEXT("_%d"), Index);
				ConstructionBars.Add(Bar);
				ConstructionBarLabels.Add(Label);
				ConstructionBarFills.Add(Fill);
				ConstructionBarIcons.Add(Icon);
				continue;
			}
			Page->RemoveFromParent();
		}

		PlaceOnCanvas(Bar,
			MakeHudSlicedAtlasPanel(*FString::Printf(TEXT("ConstructionBarBackground_%d"), Index),
				MainMenuAtlasPath, BuildBarBackgroundRect, MainMenuAtlasSize, FMargin(2, 0, 1, 0)),
			BarTopLeft, FVector2D(150.0f, 20.0f));
		UCanvasPanel* FillArea = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("ConstructionBarFillArea_%d"), Index));
		FillArea->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(Bar, FillArea, BarTopLeft + FVector2D(2.0f, 2.0f), FVector2D(146.0f, 16.0f));
		UCanvasPanel* Fill = MakeHudSlicedAtlasPanel(*FString::Printf(TEXT("ConstructionBarFill_%d"), Index),
			MainMenuAtlasPath, BuildBarFillRect, MainMenuAtlasSize, FMargin(3, 0, 2, 0));
		UCanvasPanelSlot* FillSlot = FillArea->AddChildToCanvas(Fill);
		FillSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 1.0f));
		FillSlot->SetOffsets(FMargin(0.0f));
		// lbl_time: white bold 16 px (12 pt), centred, stretched with sizeDelta (-2,0) at x -1.
		UTextBlock* Label = MakeHudText(
			*FString::Printf(TEXT("ConstructionBarTime_%d"), Index), 12, FLinearColor::White, true);
		Label->SetJustification(ETextJustify::Center);
		Label->SetShadowOffset(FVector2D::ZeroVector);
		UCanvasPanelSlot* LabelSlot = PlaceOnCanvas(Bar, Label, BarTopLeft + FVector2D(0.0f, 10.0f),
			FVector2D(148.0f, 20.0f), FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.5f));
		LabelSlot->SetZOrder(1);
		PlaceOnCanvas(Bar,
			MakeAtlasImage(*FString::Printf(TEXT("ConstructionBarIconBackground_%d"), Index),
				ResearchUiAtlasPath, BuildBarIconBackgroundRect, ResearchUiAtlasSize, FVector2D(32.0f, 32.0f)),
			IconCenter, FVector2D(32.0f, 32.0f), FAnchors(0.0f, 0.0f), FVector2D(0.5f, 0.5f))->SetZOrder(2);
		UImage* Icon = MakeAtlasImage(*FString::Printf(TEXT("ConstructionBarIcon_%d"), Index),
			MainMenuAtlasPath, BuildBarLevelUpIconRect, MainMenuAtlasSize, FVector2D(28.0f, 28.0f));
		PlaceOnCanvas(Bar, Icon, IconCenter, FVector2D(28.0f, 28.0f),
			FAnchors(0.0f, 0.0f), FVector2D(0.5f, 0.5f))->SetZOrder(3);
		ConstructionBars.Add(Bar);
		ConstructionBarLabels.Add(Label);
		ConstructionBarFills.Add(Fill);
		ConstructionBarIcons.Add(Icon);
	}
}

void URokMainHudWidget::UpdateConstructionBars()
{
	if (ConstructionBars.Num() == 0)
	{
		return;
	}
	URokBuildingUpgradeSubsystem* Subsystem = GetUpgradeSubsystem();
	APlayerController* PlayerController = GetOwningPlayer();
	// HUDUI.SetCameraLodDist(0, 270): the bar shows only up to dxf 270.
	const float Dxf = GetCityCameraDxf();
	int32 Used = 0;
	if (PlayerController && GetWorld()
		&& FMath::IsFinite(Dxf) && Dxf > 0.0f && Dxf <= 270.0f
		&& CurrentRootSize.X > 1.0f && CurrentRootSize.Y > 1.0f)
	{
		// HUDManager.SetScale/AutoAnchorUI: scale InitCameraDxf 172 / dxf, applied to the
		// prefab and to SetPosOffset(0,-50); the source canvas matches 810 px height.
		const float UnityToDesign = CurrentExpandedDesignSize.Y / UnityReferenceHeight;
		const float BarScale = 172.0f / Dxf;
		struct FBarSource
		{
			AActor* Anchor = nullptr;
			float Remaining = 0.0f;
			float Progress = 0.0f;
			bool bNewBuild = false;
		};
		TArray<FBarSource> Sources;
		const URokServerCitySubsystem* ServerCity = GetWorld()->GetSubsystem<URokServerCitySubsystem>();
		const FRokServerBuildQueueSnapshot ServerQueues = ServerCity
			? ServerCity->GetBuildQueueSnapshot() : FRokServerBuildQueueSnapshot();
		if (ServerQueues.bAvailable)
		{
			for (const FRokServerBuildQueueView& Queue : ServerQueues.Queues)
			{
				if (!Queue.bCountdownKnown) continue;
				if (AActor* Anchor = ServerCity->FindBuildingBarAnchor(Queue.BuildingIndex))
				{
					Sources.Add({Anchor, float(Queue.RemainingSeconds),
						Queue.bProgressKnown ? Queue.Progress : 0.0f, Queue.bNewBuilding});
				}
			}
		}
		else if (Subsystem)
		{
			for (TActorIterator<ARokCitySpriteActor> It(GetWorld()); It; ++It)
			{
				ARokCitySpriteActor* Building = *It;
				if (!Building || Building->IsHidden()) continue;
				float RemainingSeconds = 0.0f, TotalSeconds = 0.0f;
				const bool bNewBuild = Building->Tags.Contains(TEXT("RokLocalConstructionPending"));
				if (bNewBuild)
				{
					// Pending local builds must not register a second upgrade state.
					if (!Subsystem->GetPendingLocalConstructionClock(Building, RemainingSeconds, TotalSeconds)) continue;
				}
				else
				{
					const FRokBuildingState State = Subsystem->GetBuildingStateForActor(Building);
					if (!State.bUpgradeInProgress) continue;
					RemainingSeconds = Subsystem->GetRemainingUpgradeSeconds(Building);
					TotalSeconds = State.UpgradeFinishesAt - State.UpgradeStartedAt;
				}
				Sources.Add({Building, RemainingSeconds,
					GetBuildBarProgress(TotalSeconds, RemainingSeconds), bNewBuild});
			}
		}
		for (const FBarSource& Source : Sources)
		{
			if (Used >= ConstructionBars.Num()) break;
			// CityObjData.transform_tip is the first BoxCollider, at the building root.
			const ARokCitySpriteActor* Sprite = Cast<ARokCitySpriteActor>(Source.Anchor);
			const FVector AnchorPosition = Sprite ? Sprite->GetVisualAnchorWorldPosition() : Source.Anchor->GetActorLocation();
			FVector2D ScreenPosition;
			if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
				PlayerController, AnchorPosition, ScreenPosition, true))
			{
				continue;
			}
			UCanvasPanel* Bar = ConstructionBars[Used];
			UTextBlock* Label = ConstructionBarLabels[Used];
			UCanvasPanel* Fill = ConstructionBarFills[Used];
			UImage* Icon = ConstructionBarIcons[Used];
			if (!Bar || !Label || !Fill || !Icon)
			{
				continue;
			}
			ConstructionBarActors[Used] = Source.Anchor;
			const FVector2D DesignPosition(
				ScreenPosition.X * CurrentExpandedDesignSize.X / CurrentRootSize.X,
				ScreenPosition.Y * CurrentExpandedDesignSize.Y / CurrentRootSize.Y);
			CastChecked<UCanvasPanelSlot>(Bar->Slot)->SetPosition(
				DesignPosition + FVector2D(0.0f, 50.0f * BarScale * UnityToDesign));
			Bar->SetRenderScale(FVector2D(BarScale * UnityToDesign));
			// GameSlider m_Direction 1 (RightToLeft): the fill grows from the icon end.
			const float Progress = Source.Progress;
			UCanvasPanelSlot* FillSlot = CastChecked<UCanvasPanelSlot>(Fill->Slot);
			FillSlot->SetAnchors(FAnchors(1.0f - Progress, 0.0f, 1.0f, 1.0f));
			FillSlot->SetOffsets(FMargin(0.0f));
			Fill->SetVisibility(Progress > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			// finishTime and server time are whole seconds, so the source leftTime is the
			// real remainder rounded up: an active queue never reads 00:00:00.
			// Server clocks may reach zero while the authoritative completion push is
			// pending. Keep that bar at zero; no local completion or level mutation.
			Label->SetText(FText::FromString(FormatRemainingClock(FMath::CeilToFloat(Source.Remaining))));
			Icon->SetBrush(Source.bNewBuild ? ConstructionBarBuildingBrush : ConstructionBarLevelUpBrush);
			Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
			++Used;
		}
	}
	for (int32 Index = Used; Index < ConstructionBars.Num(); ++Index)
	{
		if (ConstructionBars[Index])
		{
			ConstructionBars[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
		ConstructionBarActors[Index].Reset();
	}
}

void URokMainHudWidget::UpdateServerArmyHud()
{
	const UGameInstance* Instance = GetGameInstance();
	const URokServerArmySubsystem* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const FRokServerArmySnapshot Snapshot = Army ? Army->GetSnapshot() : FRokServerArmySnapshot();
	if (ArmyQueueGroup)
	{
		// MainInterfaceMediator.RefreshTroopsDispatchNum: scouts contribute rows but
		// dispatch capacity counts only armies and transports (TroopProxy).
		// The world-map HUD owns the portrait queue when field armies exist.
		const bool bWorldPortraitQueue = bWorldMapTacticalOverlay && !Snapshot.Marches.IsEmpty();
		ArmyQueueGroup->SetVisibility(Snapshot.bAvailable && Snapshot.bLoaded && Snapshot.HudQueueRowCount > 0 && !bWorldPortraitQueue
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ArmyQueueCountText)
	{
		ArmyQueueCountText->SetText(FText::FromString(FString::Printf(TEXT("%s/%s"),
			Snapshot.bDispatchUsedKnown ? *FString::FromInt(Snapshot.DispatchSlotsUsed) : TEXT("--"),
			Snapshot.bDispatchCapacityKnown ? *FString::FromInt(Snapshot.DispatchSlotsMax) : TEXT("--"))));
	}
	if (Snapshot.bAvailable && Snapshot.bLoaded && Snapshot.Signature != LastServerArmySignature)
	{
		LastServerArmySignature = Snapshot.Signature;
		UE_LOG(LogTemp, Log, TEXT("Rok server army HUD: city=%lld field=%lld total=%lld marches=%d trainingQueues=%d dispatch=%d/%d."),
			Snapshot.CityCount, Snapshot.FieldCount, Snapshot.TotalCount, Snapshot.Marches.Num(),
			Snapshot.TrainingQueues.Num(), Snapshot.DispatchSlotsUsed, Snapshot.DispatchSlotsMax);
	}
}

void URokMainHudWidget::BuildBarracksIdleMarker()
{
	static const TSoftClassPtr<URokUiPageWidget> BarracksIdleClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokCityBarracksIdle.WBP_RokCityBarracksIdle_C")));
	if (auto* Page = AddHudPage(ExpandedDesignCanvas, BarracksIdleClass, TEXT("CityBarracksIdlePage"), 5))
	{
		auto* Large = Page->Get<UTextBlock>(TEXT("BarracksIdleZ"));
		auto* Small = Page->Get<UTextBlock>(TEXT("BarracksIdleSmallZ"));
		if (Large && Small)
		{
			BarracksIdleGlyphs.Add(Large);
			BarracksIdleGlyphs.Add(Small);
			return;
		}
		Page->RemoveFromParent();
	}

	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto* Glyph = MakeHudText(Index == 0 ? TEXT("BarracksIdleZ") : TEXT("BarracksIdleSmallZ"),
			28, FLinearColor(0.68f, 0.82f, 0.86f), true);
		Glyph->SetText(FText::FromString(TEXT("Z")));
		Glyph->SetJustification(ETextJustify::Center);
		Glyph->SetVisibility(ESlateVisibility::Collapsed);
		PlaceOnCanvas(ExpandedDesignCanvas, Glyph, FVector2D::ZeroVector, FVector2D(34.0f),
			FAnchors(0.0f), FVector2D(0.5f))->SetZOrder(5);
		BarracksIdleGlyphs.Add(Glyph);
	}
}

void URokMainHudWidget::UpdateBarracksIdleMarker(float DeltaTime)
{
	if (BarracksIdleGlyphs.IsEmpty()) return;
	if (const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr; City && City->HasAppliedServerCity())
	{
		for (UTextBlock* Glyph : BarracksIdleGlyphs)
		{
			Glyph->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	const auto* Life = GetWorld() ? GetWorld()->GetSubsystem<URokCityLifeSubsystem>() : nullptr;
	FVector Anchor;
	auto* Controller = GetOwningPlayer();
	const float Dxf = GetCityCameraDxf();
	FVector2D Screen;
	if (!Life || !Life->TryGetBarracksIdleMarkerLocation(Anchor) || !Controller || Dxf <= 0.0f || Dxf > 270.0f
		|| CurrentRootSize.X <= 1 || CurrentRootSize.Y <= 1
		|| !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, Anchor, Screen, true))
	{
		for (UTextBlock* Glyph : BarracksIdleGlyphs) Glyph->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	BarracksIdleMarkerElapsed = FMath::Fmod(BarracksIdleMarkerElapsed + DeltaTime, 2.6f);
	const FVector2D Feet(Screen.X * CurrentExpandedDesignSize.X / CurrentRootSize.X,
		Screen.Y * CurrentExpandedDesignSize.Y / CurrentRootSize.Y);
	const float Scale = (150.0f / Dxf) * CurrentExpandedDesignSize.Y / UnityReferenceHeight;
	for (int32 Index = 0; Index < BarracksIdleGlyphs.Num(); ++Index)
	{
		const float Rise = FMath::Fmod(BarracksIdleMarkerElapsed / 2.6f + Index * 0.5f, 1.0f);
		auto* Glyph = BarracksIdleGlyphs[Index].Get();
		CastChecked<UCanvasPanelSlot>(Glyph->Slot)->SetPosition(Feet + FVector2D(4.0f + 14.0f * Rise, -49.0f - 65.0f * Rise) * Scale);
		Glyph->SetRenderScale(FVector2D(Scale * FMath::Lerp(0.58f, 1.0f, Rise)));
		Glyph->SetRenderOpacity(FMath::SmoothStep(0.0f, 0.22f, Rise) * (1.0f - FMath::SmoothStep(0.64f, 1.0f, Rise)) * 0.78f);
		Glyph->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void URokMainHudWidget::BuildCityHarvestPops()
{
	// Unity UI_Pop_IconOnBuilding, img_bg0 / img_bg1 and m_img_icon.
	constexpr int32 MaxResourceBuildings = 16;
	const FVector2D BubbleSize(80.0f, 98.0f);
	static const TSoftClassPtr<URokUiPageWidget> FeedbackClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokCityHarvestFeedback.WBP_RokCityHarvestFeedback_C")));
	if (auto* Page = AddHudPage(ExpandedDesignCanvas, FeedbackClass, TEXT("CityHarvestFeedbackPage"), 12))
	{
		CityHarvestFeedback = Page->Get<UTextBlock>(TEXT("CityHarvestFeedback"));
		if (!CityHarvestFeedback) Page->RemoveFromParent();
	}
	if (!CityHarvestFeedback)
	{
		CityHarvestFeedback = MakeHudText(TEXT("CityHarvestFeedback"), 19, FLinearColor(1.0f, 0.88f, 0.58f), true);
		CityHarvestFeedback->SetJustification(ETextJustify::Center);
		CityHarvestFeedback->SetVisibility(ESlateVisibility::Collapsed);
		PlaceOnCanvas(ExpandedDesignCanvas, CityHarvestFeedback, FVector2D(0, -86), FVector2D(570, 30),
			FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f))->SetZOrder(12);
	}

	static const TSoftClassPtr<URokUiPageWidget> HarvestClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokCityHarvestBubble.WBP_RokCityHarvestBubble_C")));
	CityHarvestPopTypes.SetNumZeroed(MaxResourceBuildings);
	for (int32 Index = 0; Index < MaxResourceBuildings; ++Index)
	{
		const auto Name = [Index](const TCHAR* Stem) { return FName(*FString::Printf(TEXT("%s_%d"), Stem, Index)); };
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name(TEXT("CityHarvestBubble")));
		Root->SetVisibility(ESlateVisibility::Collapsed);
		Root->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
		PlaceOnCanvas(ExpandedDesignCanvas, Root, FVector2D::ZeroVector, BubbleSize, FAnchors(0), FVector2D(0.5f, 1.0f))->SetZOrder(6);
		if (auto* Page = AddHudPage(Root, HarvestClass, NAME_None))
		{
			auto* Normal = Page->Get<UImage>(TEXT("CityHarvestNormal"));
			auto* Full = Page->Get<UImage>(TEXT("CityHarvestFull"));
			auto* Icon = Page->Get<UImage>(TEXT("CityHarvestIcon"));
			auto* Button = Page->Get<URokUnityIndexedButton>(TEXT("CityHarvestAction"));
			if (Normal && Full && Icon && Button)
			{
				Page->LegacySuffix = FString::Printf(TEXT("_%d"), Index);
				Button->ConfigureAction(Index);
				Button->OnIndexedClicked.AddUObject(this, &URokMainHudWidget::HandleCityHarvestClicked);
				CityHarvestRoots.Add(Root);
				CityHarvestNormalBackgrounds.Add(Normal);
				CityHarvestFullBackgrounds.Add(Full);
				CityHarvestIcons.Add(Icon);
				continue;
			}
			Page->RemoveFromParent();
		}

		UImage* Normal = MakeAtlasImage(Name(TEXT("CityHarvestNormal")), MainMenuAtlasPath,
			SpriteRect(689, 806, 82, 100), MainMenuAtlasSize, BubbleSize);
		UImage* Full = MakeAtlasImage(Name(TEXT("CityHarvestFull")), MainMenuAtlasPath,
			SpriteRect(605, 806, 82, 100), MainMenuAtlasSize, BubbleSize);
		PlaceOnCanvas(Root, Normal, FVector2D::ZeroVector, BubbleSize);
		PlaceOnCanvas(Root, Full, FVector2D::ZeroVector, BubbleSize);
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name(TEXT("CityHarvestIcon")));
		Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(Root, Icon, FVector2D(10.0f, 10.0f), FVector2D(60.0f));
		URokUnityIndexedButton* Button = WidgetTree->ConstructWidget<URokUnityIndexedButton>(URokUnityIndexedButton::StaticClass(), Name(TEXT("CityHarvestAction")));
		Button->ConfigureAction(Index);
		Button->OnIndexedClicked.AddUObject(this, &URokMainHudWidget::HandleCityHarvestClicked);
		FButtonStyle Style;
		Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
		Button->SetStyle(Style);
		Button->SetToolTipText(NSLOCTEXT("RokHud", "CityHarvest", "领取建筑产出"));
		PlaceOnCanvas(Root, Button, FVector2D::ZeroVector, FVector2D(80.0f));
		CityHarvestRoots.Add(Root);
		CityHarvestNormalBackgrounds.Add(Normal);
		CityHarvestFullBackgrounds.Add(Full);
		CityHarvestIcons.Add(Icon);
	}
}

void URokMainHudWidget::UpdateCityHarvestPops(float DeltaTime)
{
	if (CityHarvestRoots.IsEmpty()) return;
	if (CityHarvestFeedbackSeconds > 0.0f)
	{
		CityHarvestFeedbackSeconds -= DeltaTime;
		if (CityHarvestFeedbackSeconds <= 0.0f)
		{
			CityHarvestFeedback->SetVisibility(ESlateVisibility::Collapsed);
			CityHarvestFeedbackAnchor.Reset();
		}
		else if (auto* FeedbackSlot = Cast<UCanvasPanelSlot>(CityHarvestFeedback->Slot))
		{
			FVector2D Screen;
			const auto* Anchor = CityHarvestFeedbackAnchor.Get();
			if (Anchor && CurrentRootSize.X > 1.0f && CurrentRootSize.Y > 1.0f
				&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),
					Anchor->GetVisualCenterWorldPosition(), Screen, true)
				&& Screen.X >= 0.0f && Screen.Y >= 0.0f && Screen.X <= CurrentRootSize.X && Screen.Y <= CurrentRootSize.Y)
			{
				const FVector2D DesignPosition = Screen * CurrentExpandedDesignSize / CurrentRootSize;
				const float Rise = FMath::Clamp((3.0f - CityHarvestFeedbackSeconds) * 32.0f, 0.0f, 64.0f);
				FeedbackSlot->SetAnchors(FAnchors(0.0f));
				FeedbackSlot->SetPosition(DesignPosition - FVector2D(0.0f, 26.0f + Rise));
			}
			else
			{
				FeedbackSlot->SetAnchors(FAnchors(0.5f, 1.0f));
				FeedbackSlot->SetPosition(FVector2D(0.0f, -86.0f));
			}
		}
	}
	const UGameInstance* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	const auto* BuildingStates = GetUpgradeSubsystem();
	const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
	auto* Controller = GetOwningPlayer();
	const float Dxf = GetCityCameraDxf();
	const bool bShow = Net && BuildingStates && City && Controller
		&& Net->GetState() == ERokGameNetState::RoleReady
		&& Dxf > 0.0f && Dxf <= 270.0f && CurrentRootSize.X > 1.0f && CurrentRootSize.Y > 1.0f
		&& !bPlacementControlsVisible && (!FeatureScreenWidget || !FeatureScreenWidget->IsScreenOpen());
	CityHarvestRefreshDelay -= DeltaTime;
	if (!bShow)
	{
		CityHarvestEntries.Reset();
		CityHarvestRefreshDelay = 0.0f;
	}
	else if (CityHarvestRefreshDelay <= 0.0f)
	{
		CityHarvestRefreshDelay = 0.25f;
		CityHarvestEntries.Reset();
		double ServerTime = 0.0;
		const auto Collection = Net->GetCollection(TEXT("Build_BuildingInfo"), TEXT("buildingInfo"));
		const auto Role = Net->GetRoleInfo();
		const TCHAR* BonusFields[] = {TEXT("foodCapacityMulti"), TEXT("woodCapacityMulti"), TEXT("stoneCapacityMulti"), TEXT("glodCapacityMulti")};
		if (Collection && Net->TryGetServerTimeSeconds(ServerTime))
		{
			for (const FRokServerBuilding& Building : City->GetAppliedBuildings())
			{
				if (Building.Type < 2 || Building.Type > 5 || Building.Level <= 0 || Building.FinishTime != -1) continue;
				const auto* Config = BuildingStates->GetBuildingProduceConfig(Building.Type, Building.Level);
				const TSharedPtr<FJsonObject>* Row = nullptr;
				const TSharedPtr<FJsonObject>* Gain = nullptr;
				int64 Num = 0;
				int64 Time = 0;
				if (!Config || Config->GatherMin <= 0 || !Collection->TryGetObjectField(FString::FromInt(Building.Index), Row)
					|| !(*Row)->TryGetObjectField(TEXT("buildingGainInfo"), Gain)
					|| !RokSprotoJson::TryGetInt64(*Gain, TEXT("num"), Num)
					|| !RokSprotoJson::TryGetInt64(*Gain, TEXT("changeTime"), Time) || Time <= 0
					|| Net->IsCityHarvestWaiting(Building.Index, Time)) continue;
				int64 Bonus = 0;
				RokSprotoJson::TryGetInt64(Role, BonusFields[Building.Type - 2], Bonus);
				const double Rate = FMath::Max(0.0, 1.0 + Bonus / 1000.0);
				const double Elapsed = FMath::Max(0.0, ServerTime - Time);
				const double Amount = FMath::Min(Config->GatherMax * Rate, Num + Elapsed * Config->ProducePerHour / 3600.0 * Rate);
				if (Amount < Config->GatherMin) continue;
				if (auto* Anchor = Cast<ARokCitySpriteActor>(City->FindBuildingBarAnchor(Building.Index)))
					CityHarvestEntries.Add({Building.Index, Building.Type, Time, Amount >= Config->GatherMax, Anchor});
			}
		}
	}
	int32 Used = 0;
	const FIntRect IconRects[] = {FoodRect, WoodRect, StoneRect, GoldRect};
	for (const FCityHarvestEntry& Entry : CityHarvestEntries)
	{
		if (Used == CityHarvestRoots.Num()) break;
		if (!Entry.Anchor.IsValid() || Entry.Anchor->IsHidden()) continue;
		FVector2D Screen;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller,
			Entry.Anchor->GetVisualCenterWorldPosition(), Screen, true)) continue;
		const float Scale = (105.0f / Dxf) * CurrentExpandedDesignSize.Y / UnityReferenceHeight;
		auto* Root = CityHarvestRoots[Used].Get();
		CastChecked<UCanvasPanelSlot>(Root->Slot)->SetPosition(FVector2D(
			Screen.X * CurrentExpandedDesignSize.X / CurrentRootSize.X,
			Screen.Y * CurrentExpandedDesignSize.Y / CurrentRootSize.Y - 62.0f * Scale));
		Root->SetRenderScale(FVector2D(Scale));
		CityHarvestNormalBackgrounds[Used]->SetVisibility(Entry.bFull ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		CityHarvestFullBackgrounds[Used]->SetVisibility(Entry.bFull ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (CityHarvestPopTypes[Used] != Entry.Type)
		{
			CityHarvestIcons[Used]->SetBrush(MakeAtlasBrush(LoadUiTexture(CommonAtlasPath),
				IconRects[Entry.Type - 2], CommonAtlasSize, FVector2D(60.0f)));
			CityHarvestPopTypes[Used] = Entry.Type;
		}
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		++Used;
	}
	for (int32 Index = Used; Index < CityHarvestRoots.Num(); ++Index)
		CityHarvestRoots[Index]->SetVisibility(ESlateVisibility::Collapsed);
}

void URokMainHudWidget::HandleCityHarvestClicked(int32 Index)
{
	if (!CityHarvestPopTypes.IsValidIndex(Index) || CityHarvestRoots[Index]->GetVisibility() == ESlateVisibility::Collapsed) return;
	auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net) return;
	TMap<int32, int64> Buildings;
	for (const FCityHarvestEntry& Entry : CityHarvestEntries)
	{
		if (Entry.Type == CityHarvestPopTypes[Index]) Buildings.Add(Entry.Index, Entry.GainTime);
	}
	SubmitCityHarvest(Buildings);
}

void URokMainHudWidget::RequestSelectedBuildingHarvest(AActor* BuildingActor)
{
	const auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
	const FRokServerBuilding* Building = City ? City->FindBuildingForActor(BuildingActor) : nullptr;
	const auto Collection = Net ? Net->GetCollection(TEXT("Build_BuildingInfo"), TEXT("buildingInfo")) : nullptr;
	const TSharedPtr<FJsonObject>* Row = nullptr;
	const TSharedPtr<FJsonObject>* Gain = nullptr;
	int64 GainTime = 0;
	if (!Building || Building->Type < 2 || Building->Type > 5 || !Collection
		|| !Collection->TryGetObjectField(FString::FromInt(Building->Index), Row)
		|| !(*Row)->TryGetObjectField(TEXT("buildingGainInfo"), Gain)
		|| !RokSprotoJson::TryGetInt64(*Gain, TEXT("changeTime"), GainTime) || GainTime <= 0)
	{
		ShowCityHarvestFeedback(NSLOCTEXT("RokHud", "HarvestDataPending", "建筑产出数据尚未就绪。"));
		return;
	}
	TMap<int32, int64> Buildings;
	Buildings.Add(Building->Index, GainTime);
	SubmitCityHarvest(Buildings, BuildingActor);
}

void URokMainHudWidget::SubmitCityHarvest(const TMap<int32, int64>& Buildings, AActor* FeedbackAnchor)
{
	auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net) return;

	const TWeakObjectPtr<URokMainHudWidget> WeakThis(this);
	const TWeakObjectPtr<AActor> Anchor(FeedbackAnchor);
	const auto Feedback = [WeakThis, Anchor](const FString& Error)
	{
		if (auto* Widget = WeakThis.Get())
		{
			Widget->CityHarvestRefreshDelay = 0.0f;
			// The server returns a result flag and updates balances separately.
			if (Error.IsEmpty())
			{
				Widget->ShowCityHarvestFeedback(NSLOCTEXT("RokHud", "HarvestConfirmed", "收获完成"), Anchor.Get());
			}
			else Widget->ShowCityHarvestFeedback(FText::FromString(Error));
		}
	};
	FString Error;
	if (Net->RequestCityHarvest(Buildings, Feedback, Error))
	{
		CityHarvestRefreshDelay = 0.0f;
		ShowCityHarvestFeedback(NSLOCTEXT("RokHud", "HarvestSubmitting", "正在收获…"));
	}
	else Feedback(Error);
}

void URokMainHudWidget::ShowCityHarvestFeedback(const FText& Message, AActor* SuccessAnchor)
{
	if (!CityHarvestFeedback) return;
	CityHarvestFeedbackAnchor = Cast<ARokCitySpriteActor>(SuccessAnchor);
	CityHarvestFeedbackSeconds = 3.0f;
	CityHarvestFeedback->SetText(Message);
	CityHarvestFeedback->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URokMainHudWidget::BuildServerTrainingPopPool()
{
	// Unity UI_Pop_TrainOnBuilding is separate from UI_Tip_BuildingBar:
	// pl_train (0,-47), 150x20 progress, troop icon (84.3,1.5) at .3 scale.
	// pl_get (0,108), .9 scale -> plArrow -> plSize -> soldier icon.
	constexpr int32 Count = 4;
	TrainingTroopAtlas = LoadUiTexture(TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_troops_head.rok_fullscreen_troops_head"));
	TrainingPopAnchors.SetNum(Count);
	TrainingPopFamilies.SetNumZeroed(Count);
	static const TSoftClassPtr<URokUiPageWidget> TrainingClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokCityTrainingIndicator.WBP_RokCityTrainingIndicator_C")));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const auto Name = [Index](const TCHAR* Stem) { return FName(*FString::Printf(TEXT("%s_%d"), Stem, Index)); };
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name(TEXT("ServerTrainingPop")));
		Root->SetVisibility(ESlateVisibility::Collapsed);
		Root->SetRenderTransformPivot(FVector2D(.5f));
		PlaceOnCanvas(ExpandedDesignCanvas, Root, FVector2D::ZeroVector, FVector2D(100), FAnchors(0), FVector2D(.5f))->SetZOrder(7);
		if (auto* Page = AddHudPage(Root, TrainingClass, NAME_None))
		{
			auto* Progress = Page->Get<UCanvasPanel>(TEXT("TrainingPopProgress"));
			auto* Ready = Page->Get<UCanvasPanel>(TEXT("TrainingPopReady"));
			auto* Fill = Page->Get<UCanvasPanel>(TEXT("TrainingPopFill"));
			auto* Label = Page->Get<UTextBlock>(TEXT("TrainingPopTime"));
			auto* Icon = Page->Get<UImage>(TEXT("TrainingPopIcon"));
			auto* ReadyIcon = Page->Get<UImage>(TEXT("TrainingPopReadyIcon"));
			auto* Hit = Page->Get<URokUnityIndexedButton>(TEXT("TrainingPopReadyAction"));
			if (Progress && Ready && Fill && Label && Icon && ReadyIcon && Hit)
			{
				Page->LegacySuffix = FString::Printf(TEXT("_%d"), Index);
				Hit->ConfigureAction(Index);
				Hit->OnIndexedClicked.AddUObject(this, &URokMainHudWidget::HandleTrainingPopClicked);
				TrainingPopRoots.Add(Root);
				TrainingPopProgressAreas.Add(Progress);
				TrainingPopReadyAreas.Add(Ready);
				TrainingPopFills.Add(Fill);
				TrainingPopLabels.Add(Label);
				TrainingPopIcons.Add(Icon);
				TrainingPopReadyIcons.Add(ReadyIcon);
				continue;
			}
			Page->RemoveFromParent();
		}

		UCanvasPanel* Progress = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name(TEXT("TrainingPopProgress")));
		PlaceOnCanvas(Root, Progress, FVector2D::ZeroVector, FVector2D(100));
		const FVector2D BarTopLeft(-25, 87);
		PlaceOnCanvas(Progress, MakeHudSlicedAtlasPanel(Name(TEXT("TrainingPopBackground")), MainMenuAtlasPath,
			BuildBarBackgroundRect, MainMenuAtlasSize, FMargin(2, 0, 1, 0)), BarTopLeft, FVector2D(150,20));
		UCanvasPanel* FillArea = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name(TEXT("TrainingPopFillArea")));
		PlaceOnCanvas(Progress, FillArea, BarTopLeft + FVector2D(2), FVector2D(146,16));
		UCanvasPanel* Fill = MakeHudSlicedAtlasPanel(Name(TEXT("TrainingPopFill")), MainMenuAtlasPath,
			BuildBarFillRect, MainMenuAtlasSize, FMargin(3,0,2,0));
		UCanvasPanelSlot* FillSlot = FillArea->AddChildToCanvas(Fill);
		FillSlot->SetAnchors(FAnchors(1,0,1,1)); FillSlot->SetOffsets(FMargin(0));
		UTextBlock* Label = MakeHudText(Name(TEXT("TrainingPopTime")), 12, FLinearColor::White, true);
		Label->SetJustification(ETextJustify::Center); Label->SetShadowOffset(FVector2D::ZeroVector);
		PlaceOnCanvas(Progress, Label, BarTopLeft, FVector2D(148,20))->SetZOrder(1);
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name(TEXT("TrainingPopIcon")));
		PlaceOnCanvas(Progress, Icon, FVector2D(134.3f,95.5f), FVector2D(37.8f), FAnchors(0), FVector2D(.5f));
		UCanvasPanel* Ready = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name(TEXT("TrainingPopReady")));
		PlaceOnCanvas(Root, Ready, FVector2D::ZeroVector, FVector2D(100));
		PlaceOnCanvas(Ready, MakeHudSlicedAtlasPanel(Name(TEXT("TrainingPopReadyBackground")), MainMenuAtlasPath,
			FIntRect(75,217,109,251), MainMenuAtlasSize, FMargin(16,17,9,12)),
			FVector2D(50,-86.71f), FVector2D(70.2f,45.9f), FAnchors(0), FVector2D(.5f));
		UImage* ReadyIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name(TEXT("TrainingPopReadyIcon")));
		PlaceOnCanvas(Ready, ReadyIcon, FVector2D(50,-93.91f), FVector2D(56.7f), FAnchors(0), FVector2D(.5f));
		// Open the owning building's training screen with its ready-to-claim queue.
		URokUnityIndexedButton* Hit = WidgetTree->ConstructWidget<URokUnityIndexedButton>(URokUnityIndexedButton::StaticClass(), Name(TEXT("TrainingPopReadyAction")));
		Hit->ConfigureAction(Index); Hit->OnIndexedClicked.AddUObject(this, &URokMainHudWidget::HandleTrainingPopClicked);
		FButtonStyle Style; Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType; Style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType; Hit->SetStyle(Style);
		Hit->SetToolTipText(NSLOCTEXT("RokHud", "TrainingReadyInfo", "训练完成，前往领取"));
		PlaceOnCanvas(Ready, Hit, FVector2D(50.54f,-95.845f), FVector2D(126.63f,75.699f), FAnchors(0), FVector2D(.5f))->SetZOrder(5);
		TrainingPopRoots.Add(Root); TrainingPopProgressAreas.Add(Progress); TrainingPopReadyAreas.Add(Ready);
		TrainingPopFills.Add(Fill); TrainingPopLabels.Add(Label); TrainingPopIcons.Add(Icon); TrainingPopReadyIcons.Add(ReadyIcon);
	}
}

void URokMainHudWidget::UpdateServerTrainingPops()
{
	const UGameInstance* Instance = GetGameInstance();
	const URokServerArmySubsystem* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const FRokServerArmySnapshot Snapshot = Army ? Army->GetSnapshot() : FRokServerArmySnapshot();
	APlayerController* Controller = GetOwningPlayer();
	const URokServerCitySubsystem* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
	const URokUnityGameplayCatalogSubsystem* Catalog = Instance ? Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
	const float Dxf = GetCityCameraDxf();
	int32 Used = 0;
	if (Snapshot.bAvailable && Snapshot.bLoaded && Snapshot.bTrainingKnown && City && Controller
		&& FMath::IsFinite(Dxf) && Dxf > 0 && Dxf <= 270 && CurrentRootSize.X > 1 && CurrentRootSize.Y > 1)
	{
		for (const FRokServerTrainingQueue& Queue : Snapshot.TrainingQueues)
		{
			if (Used >= TrainingPopRoots.Num()) break;
			if (!Queue.bBuildingKnown || !Queue.bTimeKnown || (Queue.State != ERokServerTrainingState::Training
				&& Queue.State != ERokServerTrainingState::Upgrading && Queue.State != ERokServerTrainingState::AwaitingReceive)) continue;
			AActor* Anchor = City->FindBuildingBarAnchor(Queue.BuildingIndex);
			if (!Anchor) continue;
			const ARokCitySpriteActor* Sprite = Cast<ARokCitySpriteActor>(Anchor);
			FVector2D Screen;
			if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller,
				Sprite ? Sprite->GetVisualAnchorWorldPosition() : Anchor->GetActorLocation(), Screen, true)) continue;
			UCanvasPanel* Root = TrainingPopRoots[Used];
			CastChecked<UCanvasPanelSlot>(Root->Slot)->SetPosition(FVector2D(
				Screen.X * CurrentExpandedDesignSize.X / CurrentRootSize.X, Screen.Y * CurrentExpandedDesignSize.Y / CurrentRootSize.Y));
			Root->SetRenderScale(FVector2D(172.0f / Dxf * CurrentExpandedDesignSize.Y / UnityReferenceHeight));
			const bool bReady = Queue.State == ERokServerTrainingState::AwaitingReceive;
			TrainingPopReadyAreas[Used]->SetVisibility(bReady ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
			TrainingPopProgressAreas[Used]->SetVisibility(bReady ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
			TrainingPopLabels[Used]->SetText(FText::FromString(FormatRemainingClock(FMath::CeilToFloat(Queue.RemainingSeconds))));
			const float Progress = FMath::Clamp(float(Queue.Progress01), 0.0f, 1.0f);
			UCanvasPanelSlot* FillSlot = CastChecked<UCanvasPanelSlot>(TrainingPopFills[Used]->Slot);
			FillSlot->SetAnchors(FAnchors(1.0f - Progress,0,1,1)); FillSlot->SetOffsets(FMargin(0));
			TrainingPopFills[Used]->SetVisibility(Progress > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			FRokUnityCatalogTroop Troop; FIntRect Rect;
			const bool bIconKnown = Catalog && Catalog->GetTroop(Queue.TargetTroopId, Troop)
				&& TrainingTroopAtlas && RokTroopPortrait::Resolve(Troop.Icon, Rect);
			for (UImage* Image : {TrainingPopIcons[Used].Get(), TrainingPopReadyIcons[Used].Get()})
			{
				Image->SetVisibility(bIconKnown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
				if (bIconKnown) Image->SetBrush(MakeAtlasBrush(TrainingTroopAtlas, Rect, FIntPoint(1024,1024), FVector2D(126)));
			}
			TrainingPopAnchors[Used] = Anchor; TrainingPopFamilies[Used] = Queue.ArmyType;
			Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			++Used;
		}
	}
	for (int32 Index = Used; Index < TrainingPopRoots.Num(); ++Index)
	{
		TrainingPopRoots[Index]->SetVisibility(ESlateVisibility::Collapsed);
		TrainingPopAnchors[Index].Reset(); TrainingPopFamilies[Index] = 0;
	}
}

void URokMainHudWidget::HandleTrainingPopClicked(int32 Index)
{
	if (FeatureScreenWidget && TrainingPopAnchors.IsValidIndex(Index) && TrainingPopAnchors[Index].IsValid())
	{
		if (ModalWidget) ModalWidget->CloseModal();
		FeatureScreenWidget->OpenTrainArmy(TrainingPopAnchors[Index].Get(), TrainingPopFamilies[Index]);
	}
}

void URokMainHudWidget::HandleFocusCityCenter(){
	APlayerController* PlayerController = GetOwningPlayer();
	ARokStrategyCameraPawn* CameraPawn =
		PlayerController
			? Cast<ARokStrategyCameraPawn>(PlayerController->GetPawn())
			: nullptr;
	if (!CameraPawn)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok city focus button found no city camera pawn."));
		return;
	}
	CameraPawn->FrameCityCenter();
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
	if (State.UnityBuildingTypeId >= 2 && State.UnityBuildingTypeId <= 5)
	{
		RequestSelectedBuildingHarvest(BuildingActor);
		return;
	}
	if (State.UnityBuildingTypeId == 1)
	{
		// BuildingMenuData[129] (btn_hud_1036) opens UI 1001, which OpenUICMD
		// maps to UI.s_GainEffect (市政厅-增益效果（经济）).
		UE_LOG(LogTemp, Display, TEXT("Rok building feature route: gain_effect (town hall)."));
		ModalWidget->OpenGainEffect(0);
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
		const auto* Tasks = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerTaskSubsystem>() : nullptr;
		const auto Snapshot = Tasks ? Tasks->GetSnapshot() : FRokServerTaskSnapshot();
		FeatureScreenWidget->OpenTasks(false, Snapshot.bAvailable && (!Snapshot.bLoaded || Snapshot.ChapterId > 0) ? 0 : INDEX_NONE);
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
	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	int32 ConfiguredLevel = 0;
	if (Config) Config->GetIntFieldByKey(TEXT("SystemOpen"), TEXT("ID"), 10005, TEXT("openLv"), ConfiguredLevel);
	const int32 RequiredLevel = ConfiguredLevel > 0 ? ConfiguredLevel : 5;
	int64 CityLevel = 0;
	const bool bUsesServerCity = Net && Net->GetState() != ERokGameNetState::Offline;
	if (bUsesServerCity)
	{
		if (Net->GetState() == ERokGameNetState::RoleReady)
			RokSprotoJson::TryGetInt64(Net->GetRoleInfo(), TEXT("level"), CityLevel);
	}
	else if (auto* Upgrade = GetUpgradeSubsystem())
	{
		AActor* TownHall = CityMarkerAnchorActor.Get();
		if (!TownHall)
		{
			TArray<AActor*> Halls;
			UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("UnityInitId_5"), Halls);
			TownHall = Halls.IsEmpty() ? nullptr : Halls[0];
		}
		if (TownHall && TownHall->ActorHasTag(TEXT("UnityInitId_5")))
			CityLevel = Upgrade->GetBuildingStateForActor(TownHall).Level;
	}
	if (CityLevel < RequiredLevel)
	{
		if (ExpeditionUnlockTip)
		{
			ExpeditionUnlockTip->SetText(CityLevel <= 0 && bUsesServerCity
				? NSLOCTEXT("RokExpedition", "TownHallPending", "市政厅等级尚未同步")
				: FText::Format(NSLOCTEXT("RokExpedition", "TownHallLock", "将市政厅升至等级{0}后解锁远征模式"),
					FText::AsNumber(RequiredLevel)));
			ExpeditionUnlockTip->SetVisibility(ESlateVisibility::HitTestInvisible);
			ExpeditionUnlockTipSeconds = 3.0f;
		}
		return;
	}
	if (ExpeditionUnlockTip) ExpeditionUnlockTip->SetVisibility(ESlateVisibility::Collapsed);
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

void URokMainHudWidget::HandleOpenChargeEvents()
{
	if (ModalWidget) ModalWidget->CloseModal();
	if (FeatureScreenWidget) FeatureScreenWidget->OpenCharge(ChargeEventPage);
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

#include "RokCityStatusIndicators.inl"
